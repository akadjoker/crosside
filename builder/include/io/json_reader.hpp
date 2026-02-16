#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

namespace crosside::io {

nlohmann::json loadJsonFile(const std::filesystem::path &path);
std::vector<std::string> splitFlags(const std::string &text);

} // namespace crosside::io
