#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

#include "core/context.hpp"
#include "model/loader.hpp"

namespace fs = std::filesystem;

namespace
{

    crosside::Context makeContext()
    {
        return crosside::Context(false);
    }

    fs::path makeTempRepoRoot(const std::string &name)
    {
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        return fs::temp_directory_path() / ("builder_path_test_" + name + "_" + std::to_string(now));
    }

    void cleanupTemp(const fs::path &root)
    {
        std::error_code ec;
        fs::remove_all(root, ec);
    }

} // namespace

TEST(PathResolve, ResolveModuleFileDefaultsToModulesFolder)
{
    const fs::path repoRoot = "/tmp/crosside_test_repo";
    const fs::path out = crosside::model::resolveModuleFile(repoRoot, "raylib", "");
    EXPECT_EQ(out, fs::absolute(repoRoot / "modules" / "raylib" / "module.json"));
}

TEST(PathResolve, ResolveModuleFileUsesExplicitRelativePath)
{
    const fs::path repoRoot = "/tmp/crosside_test_repo";
    const fs::path out = crosside::model::resolveModuleFile(repoRoot, "ignored", "custom/module.json");
    EXPECT_EQ(out, fs::absolute(repoRoot / "custom" / "module.json"));
}

TEST(PathResolve, ResolveProjectFileFallsBackToProjectsFolder)
{
    const fs::path repoRoot = "/tmp/crosside_test_repo";
    const fs::path out = crosside::model::resolveProjectFile(repoRoot, "bugame", "");
    EXPECT_EQ(out, fs::absolute(repoRoot / "projects" / "bugame" / "main.mk"));
}

TEST(PathResolve, ResolveProjectFileUsesExplicitRelativePath)
{
    const fs::path repoRoot = "/tmp/crosside_test_repo";
    const fs::path out = crosside::model::resolveProjectFile(repoRoot, "ignored", "projects/bugame/main.mk");
    EXPECT_EQ(out, fs::absolute(repoRoot / "projects" / "bugame" / "main.mk"));
}

TEST(PathResolve, ModuleClosureOrdersDependenciesBeforeRoot)
{
    crosside::model::ModuleSpec miniz;
    miniz.name = "miniz";

    crosside::model::ModuleSpec bu;
    bu.name = "bu";
    bu.depends = {"miniz"};

    crosside::model::ModuleMap modules;
    modules["miniz"] = miniz;
    modules["bu"] = bu;

    const auto out = crosside::model::moduleClosure({"bu"}, modules, makeContext());
    ASSERT_EQ(out.size(), 2U);
    EXPECT_EQ(out[0], "miniz");
    EXPECT_EQ(out[1], "bu");
}

TEST(PathResolve, LoadSingleFileModulesUsesSingleList)
{
    const fs::path repoRoot = makeTempRepoRoot("single_list");
    cleanupTemp(repoRoot);
    fs::create_directories(repoRoot / "projects");

    const fs::path config = repoRoot / "config.json";
    {
        std::ofstream out(config);
        out << R"({
  "Configuration": {
    "Modules": ["graphics"],
    "SingleFileModules": ["raylib"]
  }
}
)";
    }

    const auto modules = crosside::model::loadSingleFileModules(repoRoot, makeContext());
    ASSERT_EQ(modules.size(), 1U);
    EXPECT_EQ(modules[0], "raylib");

    cleanupTemp(repoRoot);
}

TEST(PathResolve, LoadSingleFileModulesFallsBackToGlobal)
{
    const fs::path repoRoot = makeTempRepoRoot("single_global");
    cleanupTemp(repoRoot);
    fs::create_directories(repoRoot / "projects");

    const fs::path config = repoRoot / "config.json";
    {
        std::ofstream out(config);
        out << R"({
  "Configuration": {
    "Modules": ["bu", "graphics"]
  }
}
)";
    }

    const auto modules = crosside::model::loadSingleFileModules(repoRoot, makeContext());
    ASSERT_EQ(modules.size(), 2U);
    EXPECT_EQ(modules[0], "bu");
    EXPECT_EQ(modules[1], "graphics");

    cleanupTemp(repoRoot);
}

TEST(PathResolve, LoadDefaultWebShellFromConfigurationWebShell)
{
    const fs::path repoRoot = makeTempRepoRoot("web_shell");
    cleanupTemp(repoRoot);
    fs::create_directories(repoRoot / "Templates" / "Web");

    const fs::path config = repoRoot / "config.json";
    {
        std::ofstream out(config);
        out << R"({
  "Configuration": {
    "Web": {
      "SHELL": "Templates/Web/shell.html"
    }
  }
}
)";
    }

    const auto shell = crosside::model::loadDefaultWebShell(repoRoot);
    ASSERT_TRUE(shell.has_value());
    EXPECT_EQ(shell.value(), fs::absolute(repoRoot / "Templates" / "Web" / "shell.html"));

    cleanupTemp(repoRoot);
}

TEST(PathResolve, LoadDefaultWebShellReturnsEmptyWhenUnset)
{
    const fs::path repoRoot = makeTempRepoRoot("web_shell_unset");
    cleanupTemp(repoRoot);
    fs::create_directories(repoRoot);

    const fs::path config = repoRoot / "config.json";
    {
        std::ofstream out(config);
        out << R"({
  "Configuration": {
    "Modules": ["raylib"]
  }
}
)";
    }

    const auto shell = crosside::model::loadDefaultWebShell(repoRoot);
    EXPECT_FALSE(shell.has_value());

    cleanupTemp(repoRoot);
}

TEST(PathResolve, LoadModuleFileSupportsPlatformStaticOverrides)
{
    const fs::path repoRoot = makeTempRepoRoot("module_static_override");
    cleanupTemp(repoRoot);
    const fs::path moduleRoot = repoRoot / "modules" / "codec";
    fs::create_directories(moduleRoot / "src");
    fs::create_directories(moduleRoot / "include");

    {
        std::ofstream src(moduleRoot / "src" / "codec.c");
        src << "int codec_ping(void) { return 1; }\n";
    }

    std::ostringstream json;
    json << "{\n"
         << "  \"module\": \"codec\",\n"
         << "  \"static\": true,\n"
         << "  \"src\": [\"src/codec.c\"],\n"
         << "  \"plataforms\": {\n"
         << "    \"" << crosside::model::hostDesktopKey() << "\": { \"static\": false },\n"
         << "    \"android\": { \"shared\": true },\n"
         << "    \"emscripten\": { \"static\": true }\n"
         << "  }\n"
         << "}\n";

    {
        std::ofstream mod(moduleRoot / "module.json");
        mod << json.str();
    }

    const auto spec = crosside::model::loadModuleFile(moduleRoot / "module.json", makeContext());
    ASSERT_TRUE(spec.has_value());
    EXPECT_TRUE(spec->staticLib);
    ASSERT_TRUE(spec->desktop.staticLib.has_value());
    EXPECT_FALSE(spec->desktop.staticLib.value());
    ASSERT_TRUE(spec->android.staticLib.has_value());
    EXPECT_FALSE(spec->android.staticLib.value());
    ASSERT_TRUE(spec->web.staticLib.has_value());
    EXPECT_TRUE(spec->web.staticLib.value());

    EXPECT_FALSE(crosside::model::moduleStaticForDesktop(spec.value()));
    EXPECT_FALSE(crosside::model::moduleStaticForAndroid(spec.value()));
    EXPECT_TRUE(crosside::model::moduleStaticForWeb(spec.value()));

    cleanupTemp(repoRoot);
}
