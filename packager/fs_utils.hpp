#pragma once

#include <filesystem>
#include <vector>

#include "context.hpp"

namespace crosside::io {

bool ensureDir(const std::filesystem::path &path);
std::vector<std::filesystem::path> listModuleJsonFiles(const std::filesystem::path &modulesRoot);
std::vector<std::filesystem::path> listProjectFiles(const std::filesystem::path &projectsRoot);
bool removePath(const std::filesystem::path &path, bool dryRun, const crosside::Context &ctx);

} // namespace crosside::io
