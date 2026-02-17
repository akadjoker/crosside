#include "commands/build_command.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "build/android_builder.hpp"
#include "build/desktop_builder.hpp"
#include "build/web_builder.hpp"
#include "model/loader.hpp"

namespace fs = std::filesystem;

namespace crosside::commands
{
    namespace
    {

        struct BuildOptions
        {
            std::string kind;
            std::string name;
            std::vector<std::string> targets;

            std::string mode = "release";
            std::string projectFile;
            std::string moduleFile;
            std::string release;

            bool full = false;
            bool run = false;
            bool detach = false;
            bool skipModules = true;
            bool noDeps = true;
            bool dryRun = false;
            std::vector<int> abis = {0, 1};
            int port = 8080;
        };

        constexpr const char *kDesktopOutputFolder =
#ifdef _WIN32
            "Windows";
#else
            "Linux";
#endif

        std::string lower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
                           { return static_cast<char>(std::tolower(c)); });
            return value;
        }

        std::string normalizeTarget(const std::string &value)
        {
            const std::string key = lower(value);
            if (key == "desktop" || key == "linux" || key == "windows" || key == "native")
            {
                return "desktop";
            }
            if (key == "android")
            {
                return "android";
            }
            if (key == "web" || key == "emscripten")
            {
                return "web";
            }
            return "";
        }

        bool isCompilableSourcePath(const fs::path &path)
        {
            const std::string ext = lower(path.extension().string());
            return ext == ".c" || ext == ".cc" || ext == ".cpp" || ext == ".cxx" || ext == ".mm" || ext == ".xpp";
        }

        std::vector<int> parseAbis(const std::string &value)
        {
            std::vector<int> out;
            std::string token;
            for (size_t i = 0; i <= value.size(); ++i)
            {
                if (i == value.size() || value[i] == ',')
                {
                    const std::string key = lower(token);
                    if (!key.empty())
                    {
                        if (key == "arm7" || key == "armeabi" || key == "armeabi-v7a")
                        {
                            if (std::find(out.begin(), out.end(), 0) == out.end())
                            {
                                out.push_back(0);
                            }
                        }
                        else if (key == "arm64" || key == "arm64-v8a" || key == "aarch64")
                        {
                            if (std::find(out.begin(), out.end(), 1) == out.end())
                            {
                                out.push_back(1);
                            }
                        }
                    }
                    token.clear();
                    continue;
                }
                token.push_back(value[i]);
            }
            if (out.empty())
            {
                out = {0, 1};
            }
            return out;
        }

        std::optional<fs::path> resolveSingleSourceFile(const fs::path &repoRoot, const std::string &hint)
        {
            if (hint.empty())
            {
                return std::nullopt;
            }

            fs::path raw(hint);
            std::vector<fs::path> candidates;
            if (raw.is_absolute())
            {
                candidates.push_back(raw);
            }
            else
            {
                candidates.push_back(fs::current_path() / raw);
                candidates.push_back(repoRoot / raw);
                candidates.push_back(repoRoot / "projects" / raw);
            }

            for (const auto &candidate : candidates)
            {
                std::error_code ec;
                const fs::path abs = fs::absolute(candidate, ec);
                if (ec || !isCompilableSourcePath(abs))
                {
                    continue;
                }

                if (fs::exists(abs, ec) && fs::is_regular_file(abs, ec))
                {
                    return abs;
                }
            }

            return std::nullopt;
        }

        std::optional<crosside::model::ProjectSpec> tryCreateSingleFileProject(
            const crosside::Context &ctx,
            const fs::path &repoRoot,
            const BuildOptions &opt)
        {
            if (opt.kind != "app" || !opt.projectFile.empty())
            {
                return std::nullopt;
            }

            const auto sourceFile = resolveSingleSourceFile(repoRoot, opt.name);
            if (!sourceFile.has_value())
            {
                return std::nullopt;
            }

            crosside::model::ProjectSpec project;
            project.name = sourceFile->stem().string();
            if (project.name.empty())
            {
                project.name = "app";
            }
            project.root = sourceFile->parent_path();
            project.filePath = sourceFile.value();
            project.src.push_back(sourceFile.value());
            project.modules = crosside::model::loadSingleFileModules(repoRoot, ctx);
            return project;
        }

        std::vector<std::string> normalizeAbiNames(const std::vector<int> &abis)
        {
            std::vector<std::string> out;
            for (int abi : abis)
            {
                const std::string name = abi == 1 ? "arm64-v8a" : "armeabi-v7a";
                if (std::find(out.begin(), out.end(), name) == out.end())
                {
                    out.push_back(name);
                }
            }
            if (out.empty())
            {
                out = {"armeabi-v7a", "arm64-v8a"};
            }
            return out;
        }

        bool hasModuleBinaryInDir(const crosside::model::ModuleSpec &module, const fs::path &dir)
        {
            const fs::path staticLib = dir / ("lib" + module.name + ".a");
            const fs::path sharedLib = dir / ("lib" + module.name + ".so");
            return fs::exists(staticLib) || fs::exists(sharedLib);
        }

        bool validateProjectModuleArtifacts(
            const crosside::Context &ctx,
            const crosside::model::ModuleMap &modules,
            const std::vector<std::string> &activeModules,
            const std::string &target,
            const std::vector<int> &abis)
        {
            const std::vector<std::string> allModules = crosside::model::moduleClosure(activeModules, modules, ctx);
            bool ok = true;

            for (const auto &moduleName : allModules)
            {
                auto it = modules.find(moduleName);
                if (it == modules.end())
                {
                    ctx.error("Missing module definition: ", moduleName);
                    ok = false;
                    continue;
                }

                const auto &module = it->second;
                if (target == "desktop")
                {
                    const fs::path outDir = module.dir / kDesktopOutputFolder;
                    if (!hasModuleBinaryInDir(module, outDir))
                    {
                        ctx.error(
                            "Missing desktop module binary for ", module.name,
                            " (expected ", (outDir / ("lib" + module.name + ".a")).string(),
                            " or ", (outDir / ("lib" + module.name + ".so")).string(), ")");
                        ok = false;
                    }
                    continue;
                }

                if (target == "web")
                {
                    // For web, modules may be provided by Emscripten flags
                    // (e.g. SDL2 via -s USE_SDL=2) without local static/shared libs.
                    continue;
                }

                if (target == "android")
                {
                    for (const auto &abiName : normalizeAbiNames(abis))
                    {
                        const fs::path outDir = module.dir / "Android" / abiName;
                        if (!hasModuleBinaryInDir(module, outDir))
                        {
                            ctx.error(
                                "Missing android module binary for ", module.name, " [", abiName, "]",
                                " (expected ", (outDir / ("lib" + module.name + ".a")).string(),
                                " or ", (outDir / ("lib" + module.name + ".so")).string(), ")");
                            ok = false;
                        }
                    }
                    continue;
                }
            }

            return ok;
        }

        std::vector<std::string> normalizeTargets(
            const std::vector<std::string> &input,
            const std::string &fallback,
            const crosside::Context &ctx)
        {
            std::vector<std::string> out;
            if (input.empty())
            {
                out.push_back(fallback);
                return out;
            }

            for (const auto &value : input)
            {
                std::string normalized = normalizeTarget(value);
                if (normalized.empty())
                {
                    ctx.error("Unknown target: ", value);
                    continue;
                }
                if (std::find(out.begin(), out.end(), normalized) == out.end())
                {
                    out.push_back(normalized);
                }
            }

            if (out.empty())
            {
                out.push_back(fallback);
            }
            return out;
        }

        bool parseSubject(
            const std::vector<std::string> &positionals,
            std::string &kind,
            std::string &name,
            std::vector<std::string> &targets,
            const crosside::Context &ctx)
        {
            if (positionals.empty())
            {
                ctx.error("build: missing subject");
                return false;
            }

            const std::string first = lower(positionals[0]);
            if (first == "module" || first == "mod")
            {
                if (positionals.size() < 2)
                {
                    ctx.error("build module: missing module name");
                    return false;
                }
                kind = "module";
                name = positionals[1];
                targets.assign(positionals.begin() + 2, positionals.end());
                return true;
            }

            if (first == "app" || first == "project" || first == "proj")
            {
                if (positionals.size() < 2)
                {
                    ctx.error("build app: missing project name");
                    return false;
                }
                kind = "app";
                name = positionals[1];
                targets.assign(positionals.begin() + 2, positionals.end());
                return true;
            }

            kind = "app";
            name = positionals[0];
            targets.assign(positionals.begin() + 1, positionals.end());
            return true;
        }

        bool parseOptions(const std::vector<std::string> &args, BuildOptions &opt, const crosside::Context &ctx)
        {
            std::vector<std::string> positionals;

            for (size_t i = 0; i < args.size(); ++i)
            {
                const std::string &arg = args[i];
                if (arg == "--full")
                {
                    opt.full = true;
                    continue;
                }
                if (arg == "--run")
                {
                    opt.run = true;
                    continue;
                }
                if (arg == "--detach")
                {
                    opt.detach = true;
                    continue;
                }
                if (arg == "--skip-modules")
                {
                    opt.skipModules = true;
                    continue;
                }
                if (arg == "--build-modules")
                {
                    opt.skipModules = false;
                    continue;
                }
                if (arg == "--no-deps")
                {
                    opt.noDeps = true;
                    continue;
                }
                if (arg == "--with-deps")
                {
                    opt.noDeps = false;
                    continue;
                }
                if (arg == "--dry-run")
                {
                    opt.dryRun = true;
                    continue;
                }
                if (arg == "--mode")
                {
                    if (i + 1 >= args.size())
                    {
                        ctx.error("--mode requires value");
                        return false;
                    }
                    opt.mode = lower(args[++i]);
                    if (opt.mode != "release" && opt.mode != "debug")
                    {
                        ctx.error("Invalid --mode: ", opt.mode, " (use release|debug)");
                        return false;
                    }
                    continue;
                }
                if (arg == "--abis")
                {
                    if (i + 1 >= args.size())
                    {
                        ctx.error("--abis requires value");
                        return false;
                    }
                    opt.abis = parseAbis(args[++i]);
                    continue;
                }
                if (arg == "--project-file")
                {
                    if (i + 1 >= args.size())
                    {
                        ctx.error("--project-file requires value");
                        return false;
                    }
                    opt.projectFile = args[++i];
                    continue;
                }
                if (arg == "--release")
                {
                    if (i + 1 >= args.size())
                    {
                        ctx.error("--release requires value");
                        return false;
                    }
                    opt.release = args[++i];
                    continue;
                }
                if (arg == "--module-file")
                {
                    if (i + 1 >= args.size())
                    {
                        ctx.error("--module-file requires value");
                        return false;
                    }
                    opt.moduleFile = args[++i];
                    continue;
                }
                if (arg == "--port")
                {
                    if (i + 1 >= args.size())
                    {
                        ctx.error("--port requires value");
                        return false;
                    }
                    try
                    {
                        opt.port = std::stoi(args[++i]);
                    }
                    catch (...)
                    {
                        ctx.error("Invalid --port value");
                        return false;
                    }
                    if (opt.port <= 0 || opt.port > 65535)
                    {
                        ctx.error("Invalid --port: ", opt.port);
                        return false;
                    }
                    continue;
                }

                if (arg.rfind("--", 0) == 0)
                {
                    ctx.error("Unknown build option: ", arg);
                    return false;
                }
                positionals.push_back(arg);
            }

            std::vector<std::string> rawTargets;
            if (!parseSubject(positionals, opt.kind, opt.name, rawTargets, ctx))
            {
                return false;
            }

            opt.targets = normalizeTargets(rawTargets, crosside::model::defaultTargetFromConfig(fs::current_path()), ctx);
            return true;
        }

        bool buildModuleForTarget(
            const crosside::Context &ctx,
            const fs::path &repoRoot,
            const crosside::model::ModuleSpec &module,
            const crosside::model::ModuleMap &modules,
            const std::string &target,
            const BuildOptions &opt,
            const std::string &effectiveMode)
        {
            if (target == "desktop")
            {
                return crosside::build::buildModuleDesktop(ctx, module, modules, opt.full, effectiveMode);
            }
            if (target == "android")
            {
                return crosside::build::buildModuleAndroid(ctx, repoRoot, module, modules, opt.full, opt.abis);
            }
            if (target == "web")
            {
                return crosside::build::buildModuleWeb(ctx, repoRoot, module, modules, opt.full);
            }
            ctx.error("Unsupported target: ", target);
            return false;
        }

        bool buildProjectForTarget(
            const crosside::Context &ctx,
            const fs::path &repoRoot,
            const crosside::model::ProjectSpec &project,
            const crosside::model::ModuleMap &modules,
            const std::vector<std::string> &activeModules,
            const std::string &target,
            const BuildOptions &opt,
            const std::string &effectiveMode)
        {
            if (target == "desktop")
            {
                return crosside::build::buildProjectDesktop(
                    ctx,
                    project,
                    modules,
                    activeModules,
                    opt.full,
                    effectiveMode,
                    opt.run,
                    opt.detach,
                    !opt.skipModules);
            }
            if (target == "android")
            {
                return crosside::build::buildProjectAndroid(
                    ctx,
                    repoRoot,
                    project,
                    modules,
                    activeModules,
                    opt.full,
                    opt.run,
                    !opt.skipModules,
                    opt.abis);
            }
            if (target == "web")
            {
                return crosside::build::buildProjectWeb(
                    ctx,
                    repoRoot,
                    project,
                    modules,
                    activeModules,
                    opt.full,
                    opt.run,
                    opt.detach,
                    !opt.skipModules,
                    opt.port);
            }
            ctx.error("Unsupported target: ", target);
            return false;
        }

    } // namespace

    int runBuildCommand(const crosside::Context &ctx, const fs::path &repoRoot, const std::vector<std::string> &args)
    {
        BuildOptions opt;
        if (!parseOptions(args, opt, ctx))
        {
            return 1;
        }

        ctx.log("Build type: ", opt.kind);
        ctx.log("Name: ", opt.name);

        std::string targetText;
        for (size_t i = 0; i < opt.targets.size(); ++i)
        {
            if (i > 0)
            {
                targetText += ", ";
            }
            targetText += opt.targets[i];
        }
        ctx.log("Targets: ", targetText);
        ctx.log("Desktop mode: ", opt.mode);
        if (!opt.release.empty())
        {
            ctx.log("Release profile: ", opt.release);
        }
        if (opt.detach && !opt.run)
        {
            ctx.warn("--detach has no effect without --run");
        }
        std::string abiText;
        for (size_t i = 0; i < opt.abis.size(); ++i)
        {
            if (i > 0)
            {
                abiText += ", ";
            }
            abiText += opt.abis[i] == 1 ? "arm64-v8a" : "armeabi-v7a";
        }
        ctx.log("Android ABIs: ", abiText);

        auto modules = crosside::model::discoverModules(repoRoot / "modules", ctx);
        const auto defaultWebShell = crosside::model::loadDefaultWebShell(repoRoot);

        for (const auto &target : opt.targets)
        {
            const std::string effectiveMode = target == "desktop" ? opt.mode : "release";
            if (target != "desktop" && opt.mode != "release")
            {
                ctx.log("Target ", target, " uses release mode (desktop mode ignored)");
            }
            if (target == "android" && opt.detach && opt.run)
            {
                ctx.warn("--detach ignored for android --run");
            }

            if (opt.kind == "module")
            {
                if (!opt.release.empty())
                {
                    ctx.warn("--release ignored for module builds");
                }
                const fs::path moduleFile = crosside::model::resolveModuleFile(repoRoot, opt.name, opt.moduleFile);
                auto rootModule = crosside::model::loadModuleFile(moduleFile, ctx);
                if (!rootModule.has_value())
                {
                    ctx.error("Module not found: ", moduleFile.string());
                    return 1;
                }

                modules[rootModule->name] = rootModule.value();
                const std::vector<std::string> order = opt.noDeps
                                                           ? std::vector<std::string>{rootModule->name}
                                                           : crosside::model::moduleClosure({rootModule->name}, modules, ctx);

                for (const auto &name : order)
                {
                    auto it = modules.find(name);
                    if (it == modules.end())
                    {
                        continue;
                    }
                    ctx.log("Build module ", it->second.name, " -> ", target);
                    if (opt.dryRun)
                    {
                        continue;
                    }
                    if (!buildModuleForTarget(ctx, repoRoot, it->second, modules, target, opt, effectiveMode))
                    {
                        return 1;
                    }
                }

                if (opt.run)
                {
                    ctx.warn("--run ignored for module builds");
                }
                if (opt.detach)
                {
                    ctx.warn("--detach ignored for module builds");
                }
                continue;
            }

            auto project = tryCreateSingleFileProject(ctx, repoRoot, opt);
            const bool sourceHint = (opt.kind == "app" && opt.projectFile.empty() && isCompilableSourcePath(fs::path(opt.name)));
            if (sourceHint && !project.has_value())
            {
                ctx.error("Single file source not found: ", opt.name);
                return 1;
            }
            if (!project.has_value())
            {
                const fs::path projectFile = crosside::model::resolveProjectFile(repoRoot, opt.name, opt.projectFile);
                const bool useProjectDefaultRelease = !(target == "desktop" && opt.release.empty());
                if (!useProjectDefaultRelease)
                {
                    ctx.log("Desktop build without --release: using base project content");
                }
                project = crosside::model::loadProjectFile(projectFile, ctx, opt.release, useProjectDefaultRelease);
                if (!project.has_value())
                {
                    ctx.error("Project not found: ", projectFile.string());
                    return 1;
                }
            }
            else
            {
                if (!opt.release.empty())
                {
                    ctx.warn("--release ignored in single-file mode");
                }
                ctx.log("Single file mode: ", project->filePath.string(), " (no main.mk)");
                std::string mods;
                for (std::size_t i = 0; i < project->modules.size(); ++i)
                {
                    if (i > 0)
                    {
                        mods += ", ";
                    }
                    mods += project->modules[i];
                }
                if (mods.empty())
                {
                    mods = "(none)";
                }
                ctx.log("Single file modules: ", mods);
            }

            if (project->webShell.empty() && defaultWebShell.has_value())
            {
                project->webShell = defaultWebShell->string();
            }

            std::vector<std::string> activeModules = project->modules.empty()
                                                         ? crosside::model::loadGlobalModules(repoRoot, ctx)
                                                         : project->modules;

            ctx.log("Build app ", project->name, " from ", project->filePath.string());
            {
                const std::string buildCacheKey = crosside::model::projectBuildCacheKey(project.value());
                if (!buildCacheKey.empty() && buildCacheKey != project->name)
                {
                    ctx.log("Build cache key: ", buildCacheKey);
                }
            }
            ctx.log("Auto-build modules: ", opt.skipModules ? "off" : "on");
            if (opt.dryRun)
            {
                continue;
            }

            if (opt.skipModules && !validateProjectModuleArtifacts(ctx, modules, activeModules, target, opt.abis))
            {
                return 1;
            }

            if (!buildProjectForTarget(ctx, repoRoot, project.value(), modules, activeModules, target, opt, effectiveMode))
            {
                return 1;
            }
        }

        return 0;
    }

} // namespace crosside::commands
