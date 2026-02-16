#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/context.hpp"

namespace crosside::commands {

int runServeCommand(
    const crosside::Context &ctx,
    const std::filesystem::path &repoRoot,
    const std::vector<std::string> &args
);

} // namespace crosside::commands
