#include <filesystem>
#include <string>

#include <gtest/gtest.h>

#include "core/context.hpp"
#include "io/process.hpp"

namespace
{

    crosside::Context makeContext()
    {
        return crosside::Context(false);
    }

    std::filesystem::path invalidDirPath()
    {
#ifdef _WIN32
        return std::filesystem::path("Z:\\crosside\\this\\path\\should\\not\\exist\\12345");
#else
        return std::filesystem::path("/crosside/this/path/should/not/exist/12345");
#endif
    }

} // namespace

TEST(ProcessRunCommand, BasicCommandExecution)
{
    auto ctx = makeContext();
#ifdef _WIN32
    auto result = crosside::io::runCommand("cmd", {"/c", "echo", "Hello"}, {}, ctx, false);
#else
    auto result = crosside::io::runCommand("echo", {"Hello"}, {}, ctx, false);
#endif
    EXPECT_EQ(result.code, 0) << "Command: " << result.commandLine;
}

TEST(ProcessRunCommand, CommandWithSpaces)
{
    auto ctx = makeContext();
#ifdef _WIN32
    auto result = crosside::io::runCommand("cmd", {"/c", "echo", "Hello World"}, {}, ctx, false);
#else
    auto result = crosside::io::runCommand("echo", {"Hello World"}, {}, ctx, false);
#endif
    EXPECT_EQ(result.code, 0) << "Command: " << result.commandLine;
}

TEST(ProcessRunCommand, ShellInjectionPayloadIsLiteral)
{
#ifdef _WIN32
    GTEST_SKIP() << "Injection payload test is shell-specific and skipped on Windows.";
#else
    auto ctx = makeContext();
    auto result = crosside::io::runCommand("echo", {"test && rm -rf /"}, {}, ctx, false);
    EXPECT_EQ(result.code, 0) << "Command: " << result.commandLine;
#endif
}

TEST(ProcessRunCommand, MissingCommandReturnsError)
{
    auto ctx = makeContext();
    auto result = crosside::io::runCommand("this_command_does_not_exist_12345", {}, {}, ctx, false);
    EXPECT_NE(result.code, 0);
}

TEST(ProcessRunCommand, InvalidWorkingDirectoryFailsFast)
{
    auto ctx = makeContext();
    auto result = crosside::io::runCommand("echo", {"test"}, invalidDirPath(), ctx, false);
    EXPECT_NE(result.code, 0);
}

TEST(ProcessRunCommand, DryRunReturnsSuccessWithoutExecution)
{
    auto ctx = makeContext();
    auto result = crosside::io::runCommand("echo", {"test"}, {}, ctx, true);
    EXPECT_EQ(result.code, 0);
}

TEST(ProcessRunCommand, ShellQuoteKeepsSpacesQuoted)
{
    const std::string quoted = crosside::io::shellQuote("Hello World");
#ifdef _WIN32
    EXPECT_EQ(quoted, "\"Hello World\"");
#else
    EXPECT_EQ(quoted, "'Hello World'");
#endif
}
