#pragma once

#include <string>
#include <vector>

#include "core/context.hpp"
#include "model/specs.hpp"

namespace crosside::build {

bool buildModuleDesktop(
    const crosside::Context &ctx,
    const crosside::model::ModuleSpec &module,
    const crosside::model::ModuleMap &modules,
    bool full,
    const std::string &mode
);

bool buildProjectDesktop(
    const crosside::Context &ctx,
    const crosside::model::ProjectSpec &project,
    const crosside::model::ModuleMap &modules,
    const std::vector<std::string> &activeModules,
    bool full,
    const std::string &mode,
    bool runAfter,
    bool detachRun,
    bool autoBuildModules
);

} // namespace crosside::build
