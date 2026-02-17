#include "commands/clean_command.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

#include "io/fs_utils.hpp"
#include "model/loader.hpp"

namespace fs = std::filesystem;

namespace crosside::commands
{
    namespace
    {

        constexpr const char *kDesktopFolder =
#ifdef _WIN32
            "Windows";
#else
            "Linux";
#endif

        struct CleanOptions
        {
            std::string kind;
            std::string name;
            std::vector<std::string> targets;
            std::string projectFile;
            std::string moduleFile;
            bool withDeps = false;
            bool dryRun = false;
            std::vector<int> abis = {0, 1};
        };

        std::string lower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
                           { return static_cast<char>(std::tolower(c)); });
            return value;
        }

        std::vector<int> parseAbis(const std::string &value)
        {
            std::vector<int> out;
            std::string token;
            for (size_t i = 0; i <= value.size(); ++i)
            {
                if (i == value.size() || value[i] == ',')
                {
                    std::string key = lower(token);
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

        std::vector<std::string> normalizeTargets(const std::vector<std::string> &input, const std::string &fallback)
        {
            std::vector<std::string> out;
            if (input.empty())
            {
                out.push_back(fallback);
                return out;
            }
            for (const auto &item : input)
            {
                std::string normalized = normalizeTarget(item);
                if (normalized.empty())
                {
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
                ctx.error("clean: missing subject");
                return false;
            }

            const std::string first = lower(positionals[0]);
            if (first == "module" || first == "mod")
            {
                if (positionals.size() < 2)
                {
                    ctx.error("clean module: missing module name");
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
                    ctx.error("clean app: missing project name");
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

        bool isAllKeyword(const std::string &value)
        {
            const std::string key = lower(value);
            return key == "all" || key == "*";
        }

        bool parseOptions(const std::vector<std::string> &args, CleanOptions &opt, const crosside::Context &ctx)
        {
            std::vector<std::string> positionals;

            for (size_t i = 0; i < args.size(); ++i)
            {
                const std::string &arg = args[i];
                if (arg == "--with-deps")
                {
                    opt.withDeps = true;
                    continue;
                }
                if (arg == "--dry-run")
                {
                    opt.dryRun = true;
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
                if (arg.rfind("--", 0) == 0)
                {
                    ctx.error("Unknown clean option: ", arg);
                    return false;
                }
                positionals.push_back(arg);
            }

            std::vector<std::string> rawTargets;
            if (!parseSubject(positionals, opt.kind, opt.name, rawTargets, ctx))
            {
                return false;
            }

            const std::string fallback = crosside::model::defaultTargetFromConfig(std::filesystem::current_path());
            opt.targets = normalizeTargets(rawTargets, fallback);
            return true;
        }

        int cleanModuleTarget(
            const crosside::Context &ctx,
            const crosside::model::ModuleSpec &module,
            const std::string &target,
            const std::vector<int> &abis,
            bool dryRun)
        {
            int removed = 0;

            if (target == "desktop")
            {
                removed += crosside::io::removePath(module.dir / "obj" / kDesktopFolder / module.name, dryRun, ctx) ? 1 : 0;
                removed += crosside::io::removePath(module.dir / kDesktopFolder / ("lib" + module.name + ".a"), dryRun, ctx) ? 1 : 0;
                removed += crosside::io::removePath(module.dir / kDesktopFolder / ("lib" + module.name + ".so"), dryRun, ctx) ? 1 : 0;
                removed += crosside::io::removePath(module.dir / kDesktopFolder / ("lib" + module.name + ".dll"), dryRun, ctx) ? 1 : 0;
                return removed;
            }

            if (target == "web")
            {
                removed += crosside::io::removePath(module.dir / "obj" / "Web" / module.name, dryRun, ctx) ? 1 : 0;
                removed += crosside::io::removePath(module.dir / "Web" / ("lib" + module.name + ".a"), dryRun, ctx) ? 1 : 0;
                removed += crosside::io::removePath(module.dir / "Web" / (module.name + ".html"), dryRun, ctx) ? 1 : 0;
                removed += crosside::io::removePath(module.dir / "Web" / (module.name + ".js"), dryRun, ctx) ? 1 : 0;
                removed += crosside::io::removePath(module.dir / "Web" / (module.name + ".wasm"), dryRun, ctx) ? 1 : 0;
                removed += crosside::io::removePath(module.dir / "Web" / (module.name + ".data"), dryRun, ctx) ? 1 : 0;
                return removed;
            }

            if (target == "android")
            {
                removed += crosside::io::removePath(module.dir / "obj" / "Android" / module.name, dryRun, ctx) ? 1 : 0;
                for (int abi : abis)
                {
                    const std::string abiName = abi == 1 ? "arm64-v8a" : "armeabi-v7a";
                    removed += crosside::io::removePath(module.dir / "Android" / abiName / ("lib" + module.name + ".a"), dryRun, ctx) ? 1 : 0;
                    removed += crosside::io::removePath(module.dir / "Android" / abiName / ("lib" + module.name + ".so"), dryRun, ctx) ? 1 : 0;
                }
                return removed;
            }

            return removed;
        }

        int cleanProjectTarget(
            const crosside::Context &ctx,
            const crosside::model::ProjectSpec &project,
            const std::string &target,
            const std::vector<int> &abis,
            bool dryRun)
        {
            int removed = 0;
            const std::string buildCacheKey = crosside::model::projectBuildCacheKey(project);

            if (target == "desktop")
            {
                removed += crosside::io::removePath(project.root / "obj" / kDesktopFolder / project.name, dryRun, ctx) ? 1 : 0;
                if (buildCacheKey != project.name)
                {
                    removed += crosside::io::removePath(project.root / "obj" / kDesktopFolder / buildCacheKey, dryRun, ctx) ? 1 : 0;
                }
                removed += crosside::io::removePath(project.root / project.name, dryRun, ctx) ? 1 : 0;
                removed += crosside::io::removePath(project.root / (project.name + ".exe"), dryRun, ctx) ? 1 : 0;
                return removed;
            }

            if (target == "web")
            {
                removed += crosside::io::removePath(project.root / "obj" / "Web" / project.name, dryRun, ctx) ? 1 : 0;
                if (buildCacheKey != project.name)
                {
                    removed += crosside::io::removePath(project.root / "obj" / "Web" / buildCacheKey, dryRun, ctx) ? 1 : 0;
                }
                removed += crosside::io::removePath(project.root / "Web" / (project.name + ".html"), dryRun, ctx) ? 1 : 0;
                removed += crosside::io::removePath(project.root / "Web" / (project.name + ".js"), dryRun, ctx) ? 1 : 0;
                removed += crosside::io::removePath(project.root / "Web" / (project.name + ".wasm"), dryRun, ctx) ? 1 : 0;
                removed += crosside::io::removePath(project.root / "Web" / (project.name + ".data"), dryRun, ctx) ? 1 : 0;
                return removed;
            }

            if (target == "android")
            {
                removed += crosside::io::removePath(project.root / "obj" / "Android" / project.name, dryRun, ctx) ? 1 : 0;
                if (buildCacheKey != project.name)
                {
                    removed += crosside::io::removePath(project.root / "obj" / "Android" / buildCacheKey, dryRun, ctx) ? 1 : 0;
                }
                for (int abi : abis)
                {
                    const std::string abiName = abi == 1 ? "arm64-v8a" : "armeabi-v7a";
                    removed += crosside::io::removePath(project.root / "Android" / abiName / ("lib" + project.name + ".a"), dryRun, ctx) ? 1 : 0;
                    removed += crosside::io::removePath(project.root / "Android" / abiName / ("lib" + project.name + ".so"), dryRun, ctx) ? 1 : 0;
                }
                removed += crosside::io::removePath(project.root / "Android" / project.name, dryRun, ctx) ? 1 : 0;
                return removed;
            }

            return removed;
        }

    } // namespace

    int runCleanCommand(const crosside::Context &ctx, const fs::path &repoRoot, const std::vector<std::string> &args)
    {
        CleanOptions opt;
        if (!parseOptions(args, opt, ctx))
        {
            return 1;
        }

        ctx.log("Clean type: ", opt.kind);
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

        int removed = 0;

        if (opt.kind == "module")
        {
            auto modules = crosside::model::discoverModules(repoRoot / "modules", ctx);
            std::vector<std::string> order;

            if (isAllKeyword(opt.name))
            {
                if (!opt.moduleFile.empty())
                {
                    ctx.warn("clean module all: ignoring --module-file");
                }
                if (opt.withDeps)
                {
                    ctx.warn("clean module all: --with-deps has no effect");
                }

                for (const auto &[moduleName, _] : modules)
                {
                    (void)_;
                    order.push_back(moduleName);
                }
                std::sort(order.begin(), order.end());
            }
            else
            {
                const fs::path moduleFile = crosside::model::resolveModuleFile(repoRoot, opt.name, opt.moduleFile);
                auto module = crosside::model::loadModuleFile(moduleFile, ctx);
                if (!module.has_value())
                {
                    ctx.error("Module not found: ", moduleFile.string());
                    return 1;
                }
                modules[module->name] = module.value();

                order = opt.withDeps
                            ? crosside::model::moduleClosure({module->name}, modules, ctx)
                            : std::vector<std::string>{module->name};
            }

            if (order.empty())
            {
                ctx.warn("No modules to clean");
                return 0;
            }

            for (const auto &target : opt.targets)
            {
                for (const auto &name : order)
                {
                    auto it = modules.find(name);
                    if (it == modules.end())
                    {
                        continue;
                    }
                    ctx.log("Clean module ", it->second.name, " -> ", target);
                    removed += cleanModuleTarget(ctx, it->second, target, opt.abis, opt.dryRun);
                }
            }
        }
        else
        {
            const fs::path projectFile = crosside::model::resolveProjectFile(repoRoot, opt.name, opt.projectFile);
            auto project = crosside::model::loadProjectFile(projectFile, ctx);
            if (!project.has_value())
            {
                ctx.error("Project not found: ", projectFile.string());
                return 1;
            }

            for (const auto &target : opt.targets)
            {
                ctx.log("Clean app ", project->name, " -> ", target);
                removed += cleanProjectTarget(ctx, project.value(), target, opt.abis, opt.dryRun);
            }
        }

        if (opt.dryRun)
        {
            ctx.log("Dry-run done. Candidates: ", removed);
        }
        else
        {
            ctx.log("Removed entries: ", removed);
        }

        return 0;
    }

} // namespace crosside::commands
