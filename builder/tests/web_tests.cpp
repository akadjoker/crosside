#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "io/http_server.hpp"

namespace fs = std::filesystem;

TEST(WebHttp, DetectMimeTypeKnownTypes)
{
    EXPECT_EQ(crosside::io::detectHttpMimeType("index.html"), "text/html; charset=utf-8");
    EXPECT_EQ(crosside::io::detectHttpMimeType("game.wasm"), "application/wasm");
    EXPECT_EQ(crosside::io::detectHttpMimeType("data.bin"), "application/octet-stream");
}

TEST(WebHttp, DetectMimeTypeUnknownDefaultsToOctetStream)
{
    EXPECT_EQ(crosside::io::detectHttpMimeType("file.unknownext"), "application/octet-stream");
}

TEST(WebHttp, SanitizeRelativePathUsesIndexForRoot)
{
    fs::path rel;
    ASSERT_TRUE(crosside::io::sanitizeHttpRelativePath("/", "main.html", rel));
    EXPECT_EQ(rel.generic_string(), "main.html");
}

TEST(WebHttp, SanitizeRelativePathStripsQueryAndFragment)
{
    fs::path rel;
    ASSERT_TRUE(crosside::io::sanitizeHttpRelativePath("/assets/game.png?v=1#frag", "index.html", rel));
    EXPECT_EQ(rel.generic_string(), "assets/game.png");
}

TEST(WebHttp, SanitizeRelativePathRejectsTraversal)
{
    fs::path rel;
    EXPECT_FALSE(crosside::io::sanitizeHttpRelativePath("/../etc/passwd", "index.html", rel));
}

TEST(WebHttp, IsPathSafeAcceptsFileInsideRootAndRejectsOutside)
{
    fs::path root = fs::temp_directory_path() / "builder_http_test_root";
    fs::path outside = fs::temp_directory_path() / "builder_http_test_outside";
    std::error_code ec;

    fs::remove_all(root, ec);
    fs::remove_all(outside, ec);
    fs::create_directories(root / "assets", ec);
    fs::create_directories(outside, ec);

    fs::path inFile = root / "assets" / "ok.txt";
    fs::path outFile = outside / "bad.txt";
    {
        std::ofstream f(inFile);
        f << "ok";
    }
    {
        std::ofstream f(outFile);
        f << "bad";
    }

    EXPECT_TRUE(crosside::io::isHttpPathSafe(inFile, root));
    EXPECT_FALSE(crosside::io::isHttpPathSafe(outFile, root));

    fs::remove_all(root, ec);
    fs::remove_all(outside, ec);
}
