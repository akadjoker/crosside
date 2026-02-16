#pragma once

#include <filesystem>
#include <string>

#include "core/context.hpp"

namespace crosside::io {

struct StaticHttpServerOptions {
    std::filesystem::path root;
    std::string host = "127.0.0.1";
    int port = 8080;
    std::string indexFile = "index.html";
};

bool serveStaticHttp(const crosside::Context &ctx, const StaticHttpServerOptions &options);
void stopHttpServer();
bool isHttpPortAvailable(const crosside::Context &ctx, const std::string &host, int port);

// Test-friendly helpers for web/path validation logic
std::string detectHttpMimeType(const std::filesystem::path &path);
bool sanitizeHttpRelativePath(
    const std::string &rawTarget,
    const std::string &indexFile,
    std::filesystem::path &relativeOut
);
bool isHttpPathSafe(const std::filesystem::path &filePath, const std::filesystem::path &serveRoot);

} // namespace crosside::io
