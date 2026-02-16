#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "core/context.hpp"
#include "model/specs.hpp"

namespace crosside::model {

std::string hostDesktopKey();
std::string defaultTargetFromConfig(const std::filesystem::path &repoRoot);

ModuleMap discoverModules(const std::filesystem::path &modulesRoot, const crosside::Context &ctx);
std::optional<ModuleSpec> loadModuleFile(const std::filesystem::path &moduleFile, const crosside::Context &ctx);
std::optional<ProjectSpec> loadProjectFile(const std::filesystem::path &projectFile, const crosside::Context &ctx);

std::filesystem::path resolveModuleFile(
    const std::filesystem::path &repoRoot,
    const std::string &moduleName,
    const std::string &explicitFile
);

std::filesystem::path resolveProjectFile(
    const std::filesystem::path &repoRoot,
    const std::string &projectHint,
    const std::string &explicitFile
);

std::vector<std::string> moduleClosure(
    const std::vector<std::string> &seedModules,
    const ModuleMap &modules,
    const crosside::Context &ctx
);

std::vector<std::string> loadGlobalModules(const std::filesystem::path &repoRoot, const crosside::Context &ctx);
std::vector<std::string> loadSingleFileModules(
    const std::filesystem::path &repoRoot,
    const crosside::Context &ctx
);
std::optional<std::filesystem::path> loadDefaultWebShell(const std::filesystem::path &repoRoot);

} // namespace crosside::model
