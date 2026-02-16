#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

#include "commands/module_command.hpp"
#include "core/context.hpp"

namespace fs = std::filesystem;

namespace
{

    fs::path makeTempRepoRoot(const std::string &name)
    {
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        return fs::temp_directory_path() / ("builder_module_test_" + name + "_" + std::to_string(now));
    }

    std::string loadTextFile(const fs::path &file)
    {
        std::ifstream in(file, std::ios::binary);
        std::ostringstream out;
        out << in.rdbuf();
        return out.str();
    }

    void cleanupTemp(const fs::path &root)
    {
        std::error_code ec;
        fs::remove_all(root, ec);
    }

} // namespace

TEST(ModuleCommand, InitCreatesScaffoldFiles)
{
    const fs::path repoRoot = makeTempRepoRoot("create");
    cleanupTemp(repoRoot);
    const crosside::Context ctx(false);

    const int code = crosside::commands::runModuleCommand(ctx, repoRoot, {"init", "mymodule"});
    EXPECT_EQ(code, 0);

    const fs::path moduleRoot = repoRoot / "modules" / "mymodule";
    EXPECT_TRUE(fs::exists(moduleRoot / "module.json"));
    EXPECT_TRUE(fs::exists(moduleRoot / "src" / "mymodule.c"));
    EXPECT_TRUE(fs::exists(moduleRoot / "include" / "mymodule.h"));

    const std::string moduleJson = loadTextFile(moduleRoot / "module.json");
    EXPECT_NE(moduleJson.find("\"module\": \"mymodule\""), std::string::npos);
    EXPECT_NE(moduleJson.find("\"plataforms\""), std::string::npos);

    cleanupTemp(repoRoot);
}

TEST(ModuleCommand, InitFailsIfModuleAlreadyExistsWithoutForce)
{
    const fs::path repoRoot = makeTempRepoRoot("exists");
    cleanupTemp(repoRoot);
    const crosside::Context ctx(false);

    EXPECT_EQ(crosside::commands::runModuleCommand(ctx, repoRoot, {"init", "mymodule"}), 0);
    EXPECT_NE(crosside::commands::runModuleCommand(ctx, repoRoot, {"init", "mymodule"}), 0);

    cleanupTemp(repoRoot);
}

TEST(ModuleCommand, InitForceOverwritesScaffold)
{
    const fs::path repoRoot = makeTempRepoRoot("force");
    cleanupTemp(repoRoot);
    const crosside::Context ctx(false);

    EXPECT_EQ(crosside::commands::runModuleCommand(ctx, repoRoot, {"init", "mymodule"}), 0);

    const fs::path moduleJsonPath = repoRoot / "modules" / "mymodule" / "module.json";
    {
        std::ofstream out(moduleJsonPath, std::ios::trunc);
        out << "{ \"module\": \"broken\" }\n";
    }

    EXPECT_EQ(crosside::commands::runModuleCommand(ctx, repoRoot, {"init", "mymodule", "--force"}), 0);
    const std::string moduleJson = loadTextFile(moduleJsonPath);
    EXPECT_NE(moduleJson.find("\"module\": \"mymodule\""), std::string::npos);

    cleanupTemp(repoRoot);
}
