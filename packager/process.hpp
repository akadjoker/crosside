#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "context.hpp"

namespace crosside::io {

struct ProcessResult {
    int code = -1;
    std::string commandLine;
    long long processId = -1;
};

std::string shellQuote(const std::string &value);
ProcessResult runCommand(
    const std::string &command,
    const std::vector<std::string> &args,
    const std::filesystem::path &cwd,
    const crosside::Context &ctx,
    bool dryRun = false
);
ProcessResult runCommandDetached(
    const std::string &command,
    const std::vector<std::string> &args,
    const std::filesystem::path &cwd,
    const crosside::Context &ctx,
    bool dryRun = false
);
std::optional<std::filesystem::path> currentExecutablePath();

} // namespace crosside::io
