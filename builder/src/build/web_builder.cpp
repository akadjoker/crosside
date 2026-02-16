#include "build/web_builder.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "io/fs_utils.hpp"
#include "io/http_server.hpp"
#include "io/process.hpp"
#include "model/loader.hpp"

namespace fs = std::filesystem;

namespace crosside::build {
namespace {

constexpr const char *kDefaultEmcc = "/media/projectos/projects/emsdk/upstream/emscripten/emcc";
constexpr const char *kDefaultEmcpp = "/media/projectos/projects/emsdk/upstream/emscripten/em++";
constexpr const char *kDefaultEmar = "/media/projectos/projects/emsdk/upstream/emscripten/emar";

struct WebToolchain {
    fs::path emcc;
    fs::path emcpp;
    fs::path emar;
};

struct CompileResult {
    std::vector<fs::path> objects;
    bool hasCpp = false;
};

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool startsWith(const std::string &text, const std::string &prefix) {
    return text.rfind(prefix, 0) == 0;
}

std::string trim(const std::string &value) {
    std::size_t begin = 0;
    std::size_t end = value.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }
    return value.substr(begin, end - begin);
}

void appendUnique(std::vector<std::string> &items, const std::string &value) {
    if (value.empty()) {
        return;
    }
    if (std::find(items.begin(), items.end(), value) == items.end()) {
        items.push_back(value);
    }
}

void appendAll(std::vector<std::string> &dst, const std::vector<std::string> &src) {
    for (const auto &item : src) {
        if (!item.empty()) {
            dst.push_back(item);
        }
    }
}

bool isCppSource(const fs::path &path) {
    const std::string ext = lower(path.extension().string());
    return ext == ".cc" || ext == ".cpp" || ext == ".cxx" || ext == ".mm" || ext == ".xpp";
}

bool isCompilable(const fs::path &path) {
    const std::string ext = lower(path.extension().string());
    return ext == ".c" || ext == ".cc" || ext == ".cpp" || ext == ".cxx" || ext == ".mm" || ext == ".xpp";
}

std::string pathString(const fs::path &path) {
    return path.lexically_normal().string();
}

std::string envValue(const char *name) {
    const char *value = std::getenv(name);
    if (!value) {
        return "";
    }
    return std::string(value);
}

fs::path resolveTool(const std::vector<std::string> &envKeys, const fs::path &defaultPath, const std::string &fallbackCmd) {
    for (const auto &key : envKeys) {
        const std::string value = trim(envValue(key.c_str()));
        if (!value.empty()) {
            return fs::path(value);
        }
    }

    std::error_code ec;
    if (!defaultPath.empty() && fs::exists(defaultPath, ec)) {
        return defaultPath;
    }

    return fs::path(fallbackCmd);
}

WebToolchain resolveToolchain() {
    WebToolchain out;
    out.emcc = resolveTool({"EMCC"}, fs::path(kDefaultEmcc), "emcc");
    out.emcpp = resolveTool({"EMCPP", "EMXX"}, fs::path(kDefaultEmcpp), "em++");
    out.emar = resolveTool({"EMAR"}, fs::path(kDefaultEmar), "emar");
    return out;
}

bool validateTool(const crosside::Context &ctx, const fs::path &toolPath, const std::string &label) {
    if (toolPath.empty()) {
        ctx.error("Missing web tool: ", label);
        return false;
    }

    if (toolPath.has_parent_path()) {
        std::error_code ec;
        if (!fs::exists(toolPath, ec)) {
            ctx.error("Missing web tool path: ", pathString(toolPath));
            return false;
        }
    }
    return true;
}

bool validateToolchain(const crosside::Context &ctx, const WebToolchain &tc) {
    return validateTool(ctx, tc.emcc, "emcc")
        && validateTool(ctx, tc.emcpp, "em++")
        && validateTool(ctx, tc.emar, "emar");
}

bool moduleSupportsWeb(const crosside::model::ModuleSpec &module) {
    if (module.systems.empty()) {
        return true;
    }
    for (const auto &value : module.systems) {
        const std::string key = lower(value);
        if (key == "emscripten" || key == "web") {
            return true;
        }
    }
    return false;
}

void addIncludeFlag(std::vector<std::string> &cc, std::vector<std::string> &cpp, const fs::path &path) {
    const std::string flag = "-I" + pathString(path);
    appendUnique(cc, flag);
    appendUnique(cpp, flag);
}

void collectModuleIncludesWeb(
    const crosside::model::ModuleSpec &module,
    const crosside::model::PlatformBlock &block,
    std::vector<std::string> &cc,
    std::vector<std::string> &cpp
) {
    addIncludeFlag(cc, cpp, module.dir / "src");
    addIncludeFlag(cc, cpp, module.dir / "include");
    addIncludeFlag(cc, cpp, module.dir / "include" / "web");

    for (const auto &inc : module.main.include) {
        addIncludeFlag(cc, cpp, module.dir / inc);
    }
    for (const auto &inc : block.include) {
        addIncludeFlag(cc, cpp, module.dir / inc);
    }
}

std::vector<fs::path> collectModuleSourcesWeb(const crosside::model::ModuleSpec &module, const crosside::Context &ctx) {
    std::vector<fs::path> out;
    std::set<std::string> seen;

    auto appendSource = [&](const std::string &rel) {
        if (rel.empty()) {
            return;
        }
        const fs::path path = fs::absolute(module.dir / rel);
        if (!fs::exists(path) || !isCompilable(path)) {
            return;
        }
        const std::string key = pathString(path);
        if (seen.insert(key).second) {
            out.push_back(path);
        }
    };

    for (const auto &src : module.main.src) {
        appendSource(src);
    }
    for (const auto &src : module.web.src) {
        appendSource(src);
    }

    if (out.empty()) {
        ctx.warn("No web sources for module ", module.name);
    }

    return out;
}

std::vector<fs::path> collectProjectSourcesWeb(const crosside::model::ProjectSpec &project, const crosside::Context &ctx) {
    std::vector<fs::path> out;
    std::set<std::string> seen;

    for (const auto &src : project.src) {
        if (!fs::exists(src) || !isCompilable(src)) {
            continue;
        }
        const fs::path full = fs::absolute(src);
        const std::string key = pathString(full);
        if (seen.insert(key).second) {
            out.push_back(full);
        }
    }

    if (out.empty()) {
        ctx.error("No compilable web sources for project ", project.name);
    }

    return out;
}

void appendModuleDependencyFlagsWeb(
    const crosside::model::ModuleSpec &module,
    const crosside::model::ModuleMap &modules,
    std::vector<std::string> &cc,
    std::vector<std::string> &cpp,
    std::vector<std::string> &ld,
    const crosside::Context &ctx
) {
    const std::vector<std::string> deps = crosside::model::moduleClosure(module.depends, modules, ctx);
    for (const auto &depName : deps) {
        auto it = modules.find(depName);
        if (it == modules.end()) {
            continue;
        }

        const auto &dep = it->second;
        collectModuleIncludesWeb(dep, dep.web, cc, cpp);

        const fs::path depLibDir = dep.dir / "Web";
        appendUnique(ld, "-L" + pathString(depLibDir));
        if (fs::exists(depLibDir / ("lib" + dep.name + ".a"))) {
            appendUnique(ld, "-l" + dep.name);
        }

        appendAll(ld, dep.main.ldArgs);
        appendAll(ld, dep.web.ldArgs);
    }
}

void collectProjectModuleFlagsWeb(
    const fs::path &repoRoot,
    const crosside::model::ModuleMap &modules,
    const std::vector<std::string> &activeModules,
    std::vector<std::string> &cc,
    std::vector<std::string> &cpp,
    std::vector<std::string> &ld,
    const crosside::Context &ctx
) {
    const std::vector<std::string> allModules = crosside::model::moduleClosure(activeModules, modules, ctx);

    for (const auto &moduleName : allModules) {
        auto it = modules.find(moduleName);
        if (it != modules.end()) {
            const auto &module = it->second;
            collectModuleIncludesWeb(module, module.web, cc, cpp);

            const fs::path libDir = module.dir / "Web";
            appendUnique(ld, "-L" + pathString(libDir));
            if (fs::exists(libDir / ("lib" + module.name + ".a"))) {
                appendUnique(ld, "-l" + module.name);
            }

            appendAll(ld, module.main.ldArgs);
            appendAll(ld, module.web.ldArgs);
            continue;
        }

        const fs::path fallbackDir = repoRoot / "modules" / moduleName;
        addIncludeFlag(cc, cpp, fallbackDir / "include");
        addIncludeFlag(cc, cpp, fallbackDir / "include" / "web");

        const fs::path libDir = fallbackDir / "Web";
        appendUnique(ld, "-L" + pathString(libDir));
        appendUnique(ld, "-l" + moduleName);
    }
}

std::vector<std::string> normalizeWebLdArgs(const std::vector<std::string> &raw, bool ensureRuntime) {
    std::vector<std::string> out;
    for (std::size_t i = 0; i < raw.size(); ++i) {
        std::string value = trim(raw[i]);
        if (value.empty()) {
            continue;
        }

        if (value == "-s") {
            if (i + 1 < raw.size()) {
                std::string setting = trim(raw[i + 1]);
                if (!setting.empty()) {
                    out.push_back("-s" + setting);
                }
                ++i;
            }
            continue;
        }

        if (value.size() > 1) {
            out.push_back(value);
        }
    }

    if (!ensureRuntime) {
        return out;
    }

    bool hasAsyncify = false;
    bool hasRuntimeExport = false;
    for (const auto &arg : out) {
        if (startsWith(arg, "-sASYNCIFY")) {
            hasAsyncify = true;
        }
        if (startsWith(arg, "-sEXPORTED_RUNTIME_METHODS=")) {
            hasRuntimeExport = true;
        }
    }

    if (!hasAsyncify) {
        out.push_back("-sASYNCIFY");
    }
    if (!hasRuntimeExport) {
        out.push_back("-sEXPORTED_RUNTIME_METHODS=['HEAP8','HEAPU8','HEAP16','HEAPU16','HEAP32','HEAPU32','HEAPF32','HEAPF64','ccall','cwrap','requestFullscreen']");
    }

    return out;
}

bool compileWebSources(
    const crosside::Context &ctx,
    const WebToolchain &tc,
    const fs::path &baseRoot,
    const fs::path &objRoot,
    const std::vector<fs::path> &sources,
    const std::vector<std::string> &ccFlags,
    const std::vector<std::string> &cppFlags,
    bool fullBuild,
    CompileResult &result
) {
    result.objects.clear();
    result.hasCpp = false;

    if (!crosside::io::ensureDir(objRoot)) {
        ctx.error("Failed create object directory: ", objRoot.string());
        return false;
    }

    for (const auto &src : sources) {
        const bool cppSource = isCppSource(src);
        if (cppSource) {
            result.hasCpp = true;
        }

        fs::path relParent;
        try {
            relParent = fs::relative(src.parent_path(), baseRoot);
        } catch (...) {
            relParent = src.parent_path().filename();
        }

        const fs::path objDir = objRoot / relParent;
        if (!crosside::io::ensureDir(objDir)) {
            ctx.error("Failed create object subdir: ", objDir.string());
            return false;
        }

        const fs::path obj = objDir / (src.stem().string() + ".o");

        if (!fullBuild && fs::exists(obj)) {
            std::error_code ec;
            const auto srcTime = fs::last_write_time(src, ec);
            if (!ec) {
                const auto objTime = fs::last_write_time(obj, ec);
                if (!ec && objTime >= srcTime) {
                    ctx.log("Skip ", src.string());
                    result.objects.push_back(obj);
                    continue;
                }
            }
        }

        std::vector<std::string> args;
        args.push_back("-c");
        args.push_back(pathString(src));
        args.push_back("-o");
        args.push_back(pathString(obj));

        if (cppSource) {
            appendAll(args, cppFlags);
        } else {
            appendAll(args, ccFlags);
        }

        const std::string compiler = cppSource ? pathString(tc.emcpp) : pathString(tc.emcc);
        auto command = crosside::io::runCommand(compiler, args, {}, ctx, false);
        if (command.code != 0) {
            ctx.error("Compile failed for ", src.string());
            return false;
        }

        result.objects.push_back(obj);
    }

    return !result.objects.empty();
}

bool archiveWebStatic(
    const crosside::Context &ctx,
    const WebToolchain &tc,
    const fs::path &output,
    const std::vector<fs::path> &objects
) {
    if (objects.empty()) {
        ctx.error("No objects to archive for ", output.string());
        return false;
    }

    if (!crosside::io::ensureDir(output.parent_path())) {
        ctx.error("Failed create output folder: ", output.parent_path().string());
        return false;
    }

    std::error_code ec;
    fs::remove(output, ec);

    std::vector<std::string> args;
    args.push_back("rcs");
    args.push_back(pathString(output));
    for (const auto &obj : objects) {
        args.push_back(pathString(obj));
    }

    auto command = crosside::io::runCommand(pathString(tc.emar), args, {}, ctx, false);
    if (command.code != 0) {
        ctx.error("Static web archive failed: ", output.string());
        return false;
    }

    if (!fs::exists(output) || fs::file_size(output, ec) <= 8) {
        ctx.error("Generated web archive is empty: ", output.string());
        return false;
    }

    return true;
}

bool linkWebApp(
    const crosside::Context &ctx,
    const fs::path &repoRoot,
    const WebToolchain &tc,
    const std::string &name,
    const std::vector<fs::path> &objects,
    const std::vector<std::string> &ldFlags,
    bool hasCpp,
    const fs::path &outputHtml,
    bool ensureRuntime
) {
    if (objects.empty()) {
        ctx.error("No objects to link for web target ", name);
        return false;
    }

    if (!crosside::io::ensureDir(outputHtml.parent_path())) {
        ctx.error("Failed create web output folder: ", outputHtml.parent_path().string());
        return false;
    }

    {
        std::error_code ec;
        const fs::path base = outputHtml.parent_path() / outputHtml.stem();
        fs::remove(outputHtml, ec);
        fs::remove(base.string() + ".js", ec);
        fs::remove(base.string() + ".wasm", ec);
        fs::remove(base.string() + ".data", ec);
        fs::remove(base.string() + ".worker.js", ec);
    }

    std::vector<std::string> args;
    args.push_back("-o");
    args.push_back(pathString(outputHtml));

    for (const auto &obj : objects) {
        args.push_back(pathString(obj));
    }

    const auto normalizedLd = normalizeWebLdArgs(ldFlags, ensureRuntime);
    appendAll(args, normalizedLd);

    const fs::path libsRoot = repoRoot / "libs" / "Web";
    if (fs::exists(libsRoot)) {
        appendUnique(args, "-L" + pathString(libsRoot));
    }

    auto command = crosside::io::runCommand(hasCpp ? pathString(tc.emcpp) : pathString(tc.emcc), args, {}, ctx, false);
    if (command.code != 0) {
        ctx.error("Web link failed for ", outputHtml.string());
        return false;
    }

    return true;
}

bool ensureWebOutputExists(const crosside::Context &ctx, const fs::path &outputHtml, const std::string &name) {
    std::error_code ec;
    if (fs::exists(outputHtml, ec) && fs::is_regular_file(outputHtml, ec)) {
        ctx.log("Web output: ", outputHtml.string());
        return true;
    }

    ctx.error("Web output not found for ", name);
    ctx.error("Expected: ", outputHtml.string());
    return false;
}

void appendWebTemplateAndAssets(
    const crosside::Context &ctx,
    const crosside::model::ProjectSpec &project,
    const std::vector<std::string> &activeModules,
    const crosside::model::ModuleMap &modules,
    std::vector<std::string> &ld
) {
    fs::path templateFile;

    if (!project.webShell.empty()) {
        fs::path shell = fs::path(project.webShell);
        if (!shell.is_absolute()) {
            shell = fs::absolute(project.root / shell);
        }
        if (fs::exists(shell)) {
            templateFile = shell;
        } else {
            ctx.warn("Web shell not found: ", shell.string());
        }
    }

    if (templateFile.empty()) {
        for (const auto &moduleName : activeModules) {
            auto it = modules.find(moduleName);
            if (it == modules.end()) {
                continue;
            }
            const std::string shell = trim(it->second.web.shellTemplate);
            if (shell.empty()) {
                continue;
            }
            const fs::path candidate = fs::absolute(it->second.dir / shell);
            if (fs::exists(candidate)) {
                templateFile = candidate;
                break;
            }
        }
    }

    if (!templateFile.empty()) {
        appendUnique(ld, "--shell-file");
        appendUnique(ld, pathString(templateFile));
    }

    const std::vector<std::pair<std::string, std::string>> preload = {
        {"scripts", "scripts"},
        {"assets", "assets"},
        {"resources", "resources"},
        {"data", "data"},
        {"media", "media"},
    };

    for (const auto &[folder, mount] : preload) {
        const fs::path host = project.root / folder;
        if (fs::exists(host) && fs::is_directory(host)) {
            ld.push_back("--preload-file");
            ld.push_back(pathString(host) + "@/" + mount);
        }
    }
}

std::optional<fs::path> resolveWebExport(const fs::path &projectRoot, const std::string &name) {
    const fs::path base = projectRoot / "Web";
    const std::vector<fs::path> candidates = {
        base / (name + ".html"),
        base / name / (name + ".html"),
    };

    for (const auto &candidate : candidates) {
        if (fs::exists(candidate)) {
            return candidate;
        }
    }
    return std::nullopt;
}

struct WebRunInfo {
    fs::path serveRoot;
    std::string url;
};

int resolveAvailableRunPort(const crosside::Context &ctx, int preferredPort) {
    constexpr const char *kHost = "127.0.0.1";
    constexpr int kMaxOffset = 64;

    if (crosside::io::isHttpPortAvailable(ctx, kHost, preferredPort)) {
        return preferredPort;
    }

    for (int offset = 1; offset <= kMaxOffset; ++offset) {
        const int candidate = preferredPort + offset;
        if (candidate > 65535) {
            break;
        }
        if (crosside::io::isHttpPortAvailable(ctx, kHost, candidate)) {
            ctx.warn(
                "Web port ", preferredPort, " is busy (likely previous detached server). Using ",
                candidate, " for this run.");
            return candidate;
        }
    }

    ctx.error("No free web port found from ", preferredPort, " to ", (preferredPort + kMaxOffset));
    return -1;
}

WebRunInfo resolveWebRunInfo(const fs::path &exportFile, const fs::path &repoRoot, int port) {
    WebRunInfo info;
    (void)repoRoot;

    std::error_code ec;
    const fs::path absExport = fs::absolute(exportFile, ec);

    info.serveRoot = absExport.parent_path();
    info.url = "http://127.0.0.1:" + std::to_string(port) + "/";
    return info;
}

void tryOpenBrowser(const crosside::Context &ctx, const std::string &url) {
#ifdef _WIN32
    crosside::io::runCommand("cmd", {"/c", "start", "", url}, {}, ctx, false);
#elif __APPLE__
    crosside::io::runCommand("open", {url}, {}, ctx, false);
#else
    crosside::io::runCommand("xdg-open", {url}, {}, ctx, false);
#endif
}

bool runWebOutput(
    const crosside::Context &ctx,
    const fs::path &repoRoot,
    const crosside::model::ProjectSpec &project,
    bool detachRun,
    int port
) {
    auto exportFile = resolveWebExport(project.root, project.name);
    if (!exportFile.has_value()) {
        ctx.error("Web output not found for ", project.name);
        return false;
    }

    const int runPort = resolveAvailableRunPort(ctx, port);
    if (runPort <= 0) {
        return false;
    }

    const WebRunInfo runInfo = resolveWebRunInfo(exportFile.value(), repoRoot, runPort);
    ctx.log("Serving Web from ", runInfo.serveRoot.string());
    ctx.log("Open ", runInfo.url);

    if (detachRun) {
        const auto exePath = crosside::io::currentExecutablePath();
        if (!exePath.has_value()) {
            ctx.error("Could not resolve builder executable path for detached web serve");
            return false;
        }

        std::vector<std::string> serveArgs = {
            "serve",
            exportFile.value().string(),
            "--host",
            "127.0.0.1",
            "--port",
            std::to_string(runPort),
            "--no-open",
        };
        auto detached = crosside::io::runCommandDetached(exePath->string(), serveArgs, {}, ctx, false);
        if (detached.code != 0) {
            ctx.error("Failed to start detached web server");
            return false;
        }

        if (detached.processId > 0) {
            ctx.log("Detached web server launcher PID: ", detached.processId);
        }

        tryOpenBrowser(ctx, runInfo.url);
        return true;
    }

    tryOpenBrowser(ctx, runInfo.url);
    crosside::io::StaticHttpServerOptions options;
    options.root = runInfo.serveRoot;
    options.host = "127.0.0.1";
    options.port = runPort;
    options.indexFile = exportFile.value().filename().string();
    return crosside::io::serveStaticHttp(ctx, options);
}

} // namespace

bool buildModuleWeb(
    const crosside::Context &ctx,
    const fs::path &repoRoot,
    const crosside::model::ModuleSpec &module,
    const crosside::model::ModuleMap &modules,
    bool fullBuild
) {
    if (!moduleSupportsWeb(module)) {
        ctx.log("Skip module ", module.name, " for web (unsupported by module.json)");
        return true;
    }

    const WebToolchain tc = resolveToolchain();
    if (!validateToolchain(ctx, tc)) {
        return false;
    }

    const auto sources = collectModuleSourcesWeb(module, ctx);
    if (sources.empty()) {
        return false;
    }

    std::vector<std::string> ccFlags = module.main.ccArgs;
    std::vector<std::string> cppFlags = module.main.cppArgs;
    std::vector<std::string> ldFlags = module.main.ldArgs;

    appendAll(ccFlags, module.web.ccArgs);
    appendAll(cppFlags, module.web.cppArgs);
    appendAll(ldFlags, module.web.ldArgs);

    collectModuleIncludesWeb(module, module.web, ccFlags, cppFlags);
    appendModuleDependencyFlagsWeb(module, modules, ccFlags, cppFlags, ldFlags, ctx);

    const fs::path objRoot = module.dir / "obj" / "Web" / module.name;
    CompileResult compiled;
    if (!compileWebSources(ctx, tc, module.dir, objRoot, sources, ccFlags, cppFlags, fullBuild, compiled)) {
        return false;
    }

    const fs::path webRoot = module.dir / "Web";
    if (!module.staticLib) {
        const fs::path outHtml = webRoot / (module.name + ".html");
        if (!linkWebApp(ctx, repoRoot, tc, module.name, compiled.objects, ldFlags, compiled.hasCpp, outHtml, true)) {
            return false;
        }
        return ensureWebOutputExists(ctx, outHtml, module.name);
    }

    const fs::path outLib = webRoot / ("lib" + module.name + ".a");
    return archiveWebStatic(ctx, tc, outLib, compiled.objects);
}

bool buildProjectWeb(
    const crosside::Context &ctx,
    const fs::path &repoRoot,
    const crosside::model::ProjectSpec &project,
    const crosside::model::ModuleMap &modules,
    const std::vector<std::string> &activeModules,
    bool fullBuild,
    bool runAfter,
    bool detachRun,
    bool autoBuildModules,
    int port
) {
    const WebToolchain tc = resolveToolchain();
    if (!validateToolchain(ctx, tc)) {
        return false;
    }

    if (autoBuildModules) {
        const std::vector<std::string> allModules = crosside::model::moduleClosure(activeModules, modules, ctx);
        for (const auto &moduleName : allModules) {
            auto it = modules.find(moduleName);
            if (it == modules.end()) {
                ctx.warn("Missing module for auto-build: ", moduleName);
                continue;
            }
            if (!buildModuleWeb(ctx, repoRoot, it->second, modules, fullBuild)) {
                ctx.error("Failed auto-build module ", moduleName, " for web");
                return false;
            }
        }
    }

    const auto sources = collectProjectSourcesWeb(project, ctx);
    if (sources.empty()) {
        return false;
    }

    std::vector<std::string> ccFlags = project.main.cc;
    std::vector<std::string> cppFlags = project.main.cpp;
    std::vector<std::string> ldFlags = project.main.ld;

    appendAll(ccFlags, project.web.cc);
    appendAll(cppFlags, project.web.cpp);
    appendAll(ldFlags, project.web.ld);

    for (const auto &inc : project.include) {
        addIncludeFlag(ccFlags, cppFlags, inc);
    }

    collectProjectModuleFlagsWeb(repoRoot, modules, activeModules, ccFlags, cppFlags, ldFlags, ctx);
    appendWebTemplateAndAssets(ctx, project, activeModules, modules, ldFlags);

    const fs::path objRoot = project.root / "obj" / "Web" / project.name;
    CompileResult compiled;
    if (!compileWebSources(ctx, tc, project.root, objRoot, sources, ccFlags, cppFlags, fullBuild, compiled)) {
        return false;
    }

    const fs::path outHtml = project.root / "Web" / (project.name + ".html");
    if (!linkWebApp(ctx, repoRoot, tc, project.name, compiled.objects, ldFlags, compiled.hasCpp, outHtml, true)) {
        return false;
    }
    if (!ensureWebOutputExists(ctx, outHtml, project.name)) {
        return false;
    }

    if (!runAfter) {
        return true;
    }

    return runWebOutput(ctx, repoRoot, project, detachRun, port);
}

} // namespace crosside::build
