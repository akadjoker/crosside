#pragma once

#include <filesystem>
#include <vector>

#include "core/context.hpp"
#include "model/specs.hpp"

namespace crosside::build {

bool buildModuleWeb(
    const crosside::Context &ctx,
    const std::filesystem::path &repoRoot,
    const crosside::model::ModuleSpec &module,
    const crosside::model::ModuleMap &modules,
    bool fullBuild
);

bool buildProjectWeb(
    const crosside::Context &ctx,
    const std::filesystem::path &repoRoot,
    const crosside::model::ProjectSpec &project,
    const crosside::model::ModuleMap &modules,
    const std::vector<std::string> &activeModules,
    bool fullBuild,
    bool runAfter,
    bool detachRun,
    bool autoBuildModules,
    int port
);

} // namespace crosside::build
