#include "build/desktop_builder.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "io/fs_utils.hpp"
#include "io/process.hpp"
#include "model/loader.hpp"

namespace fs = std::filesystem;

namespace crosside::build
{
    namespace
    {

        constexpr const char *kDesktopFolder =
#ifdef _WIN32
            "Windows";
#else
            "Linux";
#endif

        constexpr const char *kDesktopIncludeFolder =
#ifdef _WIN32
            "windows";
#else
            "linux";
#endif

        bool hasPrefix(const std::string &value, const std::string &prefix)
        {
            return value.rfind(prefix, 0) == 0;
        }

        bool isCppSource(const fs::path &path)
        {
            const std::string ext = path.extension().string();
            return ext == ".cc" || ext == ".cpp" || ext == ".cxx" || ext == ".mm" || ext == ".xpp";
        }

        bool isCompilable(const fs::path &path)
        {
            const std::string ext = path.extension().string();
            return ext == ".c" || ext == ".cc" || ext == ".cpp" || ext == ".cxx" || ext == ".mm" || ext == ".xpp";
        }

        void appendUnique(std::vector<std::string> &list, const std::string &value)
        {
            if (value.empty())
            {
                return;
            }
            if (std::find(list.begin(), list.end(), value) == list.end())
            {
                list.push_back(value);
            }
        }

        void addIncludeFlag(std::vector<std::string> &cc, std::vector<std::string> &cpp, const fs::path &path)
        {
            const std::string flag = "-I" + path.string();
            appendUnique(cc, flag);
            appendUnique(cpp, flag);
        }

        void appendLdFlags(std::vector<std::string> &dst, const std::vector<std::string> &src)
        {
            for (const auto &flag : src)
            {
                if (!flag.empty())
                {
                    dst.push_back(flag);
                }
            }
        }

        std::optional<fs::path> resolveDesktopRunScript(const crosside::model::ProjectSpec &project)
        {
            std::vector<fs::path> contentRoots;
            auto addContentRoot = [&](const fs::path &root)
            {
                if (root.empty())
                {
                    return;
                }
                if (std::find(contentRoots.begin(), contentRoots.end(), root) == contentRoots.end())
                {
                    contentRoots.push_back(root);
                }
            };

            addContentRoot(project.desktopContentRoot);
            addContentRoot(project.webContentRoot);
            addContentRoot(project.androidContentRoot);

            for (const auto &contentRoot : contentRoots)
            {
                const fs::path primary = contentRoot / "scripts" / "main.bu";
                if (fs::exists(primary) && fs::is_regular_file(primary))
                {
                    return fs::absolute(primary);
                }

                const fs::path fallback = contentRoot / "main.bu";
                if (fs::exists(fallback) && fs::is_regular_file(fallback))
                {
                    return fs::absolute(fallback);
                }
            }

            const fs::path rootPrimary = project.root / "scripts" / "main.bu";
            if (fs::exists(rootPrimary) && fs::is_regular_file(rootPrimary))
            {
                return fs::absolute(rootPrimary);
            }

            const fs::path rootFallback = project.root / "main.bu";
            if (fs::exists(rootFallback) && fs::is_regular_file(rootFallback))
            {
                return fs::absolute(rootFallback);
            }

            return std::nullopt;
        }

        std::vector<std::string> resolveDesktopRunArgs(
            const crosside::Context &ctx,
            const crosside::model::ProjectSpec &project)
        {
            std::vector<std::string> args;
            const auto script = resolveDesktopRunScript(project);
            if (!script.has_value())
            {
                return args;
            }

            std::error_code ec;
            fs::path displayPath = script.value();
            const fs::path relativePath = fs::relative(script.value(), project.root, ec);
            if (!ec)
            {
                displayPath = relativePath;
            }

            args.push_back(displayPath.string());
            ctx.log("Desktop run script: ", args.front());
            return args;
        }

        void normalizeModeFlags(std::vector<std::string> &flags)
        {
            std::vector<std::string> out;
            out.reserve(flags.size());
            for (const auto &flag : flags)
            {
                if (flag.empty())
                {
                    continue;
                }
                if (flag == "-DDEBUG" || flag == "-DNDEBUG" || flag == "-s")
                {
                    continue;
                }
                if (hasPrefix(flag, "-O") || hasPrefix(flag, "-g"))
                {
                    continue;
                }
                out.push_back(flag);
            }
            flags.swap(out);
        }

        void applyDesktopMode(std::vector<std::string> &cc, std::vector<std::string> &cpp, const std::string &mode)
        {
            normalizeModeFlags(cc);
            normalizeModeFlags(cpp);

            if (mode == "debug")
            {
                cc.push_back("-O0");
                cc.push_back("-g3");
                cc.push_back("-DDEBUG");
                cc.push_back("-fno-omit-frame-pointer");

                cpp.push_back("-O0");
                cpp.push_back("-g3");
                cpp.push_back("-DDEBUG");
                cpp.push_back("-fno-omit-frame-pointer");
            }
            else
            {
                cc.push_back("-O2");
                cc.push_back("-DNDEBUG");

                cpp.push_back("-O2");
                cpp.push_back("-DNDEBUG");
            }
        }

        void collectModuleIncludes(
            const crosside::model::ModuleSpec &module,
            const crosside::model::PlatformBlock &block,
            std::vector<std::string> &cc,
            std::vector<std::string> &cpp)
        {
            addIncludeFlag(cc, cpp, module.dir / "src");
            addIncludeFlag(cc, cpp, module.dir / "include");
            addIncludeFlag(cc, cpp, module.dir / "include" / kDesktopIncludeFolder);

            for (const auto &inc : module.main.include)
            {
                addIncludeFlag(cc, cpp, module.dir / inc);
            }
            for (const auto &inc : block.include)
            {
                addIncludeFlag(cc, cpp, module.dir / inc);
            }
        }

        void collectModuleSources(
            const crosside::Context &ctx,
            const crosside::model::ModuleSpec &module,
            const crosside::model::PlatformBlock &block,
            std::vector<fs::path> &sources)
        {
            for (const auto &src : module.main.src)
            {
                fs::path file = fs::absolute(module.dir / src);
                if (!fs::exists(file) || !isCompilable(file))
                {
                    continue;
                }
                sources.push_back(file);
            }
            for (const auto &src : block.src)
            {
                fs::path file = fs::absolute(module.dir / src);
                if (!fs::exists(file) || !isCompilable(file))
                {
                    continue;
                }
                sources.push_back(file);
            }
            if (sources.empty())
            {
                ctx.warn("No desktop sources for module ", module.name);
            }
        }

        bool compileSources(
            const crosside::Context &ctx,
            const fs::path &baseRoot,
            const fs::path &objRoot,
            const std::vector<fs::path> &sources,
            const std::vector<std::string> &ccArgs,
            const std::vector<std::string> &cppArgs,
            bool full,
            std::vector<fs::path> &objects)
        {
            objects.clear();

            for (const auto &src : sources)
            {
                fs::path relParent;
                try
                {
                    relParent = fs::relative(src.parent_path(), baseRoot);
                }
                catch (...)
                {
                    relParent = src.parent_path().filename();
                }

                fs::path objDir = objRoot / relParent;
                io::ensureDir(objDir);
                fs::path obj = objDir / (src.stem().string() + ".o");

                if (!full && fs::exists(obj))
                {
                    std::error_code ec;
                    auto srcTime = fs::last_write_time(src, ec);
                    if (ec)
                    {
                        srcTime = fs::file_time_type::min();
                    }
                    auto objTime = fs::last_write_time(obj, ec);
                    if (!ec && objTime >= srcTime)
                    {
                        ctx.log("Skip ", src.string());
                        objects.push_back(obj);
                        continue;
                    }
                }

                std::vector<std::string> args;
                args.push_back("-c");
                args.push_back(src.string());
                args.push_back("-o");
                args.push_back(obj.string());

                const bool cpp = isCppSource(src);
                const auto &flags = cpp ? cppArgs : ccArgs;
                args.insert(args.end(), flags.begin(), flags.end());
                args.push_back("-fPIC");

                const std::string compiler = cpp ? "g++" : "gcc";
                auto result = io::runCommand(compiler, args, {}, ctx, false);
                if (result.code != 0)
                {
                    return false;
                }

                objects.push_back(obj);
            }

            return !objects.empty();
        }

        std::vector<std::string> moduleActiveDependencies(
            const crosside::model::ModuleSpec &module,
            const crosside::model::ModuleMap &modules,
            const crosside::Context &ctx)
        {
            return crosside::model::moduleClosure(module.depends, modules, ctx);
        }

    } // namespace

    bool buildModuleDesktop(
        const crosside::Context &ctx,
        const crosside::model::ModuleSpec &module,
        const crosside::model::ModuleMap &modules,
        bool full,
        const std::string &mode)
    {
        std::vector<fs::path> sources;
        collectModuleSources(ctx, module, module.desktop, sources);
        if (sources.empty())
        {
            return false;
        }

        std::vector<std::string> cc = module.main.ccArgs;
        std::vector<std::string> cpp = module.main.cppArgs;
        std::vector<std::string> ld = module.main.ldArgs;

        collectModuleIncludes(module, module.desktop, cc, cpp);

        for (const auto &flag : module.desktop.ccArgs)
        {
            appendUnique(cc, flag);
        }
        for (const auto &flag : module.desktop.cppArgs)
        {
            appendUnique(cpp, flag);
        }
        for (const auto &flag : module.desktop.ldArgs)
        {
            if (!flag.empty())
            {
                ld.push_back(flag);
            }
        }

        const auto depOrder = moduleActiveDependencies(module, modules, ctx);
        for (const auto &depName : depOrder)
        {
            auto it = modules.find(depName);
            if (it == modules.end())
            {
                continue;
            }
            const auto &dep = it->second;
            collectModuleIncludes(dep, dep.desktop, cc, cpp);

            const fs::path libDir = dep.dir / kDesktopFolder;
            ld.push_back("-L" + libDir.string());
            ld.push_back("-l" + dep.name);

            for (const auto &flag : dep.main.ldArgs)
            {
                if (!flag.empty())
                {
                    ld.push_back(flag);
                }
            }
            for (const auto &flag : dep.desktop.ldArgs)
            {
                if (!flag.empty())
                {
                    ld.push_back(flag);
                }
            }
        }

        applyDesktopMode(cc, cpp, mode);

        const fs::path objRoot = module.dir / "obj" / kDesktopFolder / module.name;
        io::ensureDir(objRoot);

        std::vector<fs::path> objects;
        if (!compileSources(ctx, module.dir, objRoot, sources, cc, cpp, full, objects))
        {
            return false;
        }

        const fs::path outDir = module.dir / kDesktopFolder;
        io::ensureDir(outDir);

        const bool moduleStaticLib = crosside::model::moduleStaticForDesktop(module);
        if (moduleStaticLib)
        {
            const fs::path outLib = outDir / ("lib" + module.name + ".a");
            std::error_code ec;
            fs::remove(outLib, ec);

            std::vector<std::string> args;
            args.push_back("rcs");
            args.push_back(outLib.string());
            for (const auto &obj : objects)
            {
                args.push_back(obj.string());
            }

            auto result = io::runCommand("ar", args, {}, ctx, false);
            return result.code == 0;
        }

        bool hasCpp = false;
        for (const auto &src : sources)
        {
            if (isCppSource(src))
            {
                hasCpp = true;
                break;
            }
        }

        const fs::path outLib = outDir / ("lib" + module.name + ".so");
        std::vector<std::string> args;
        args.push_back("-shared");
        args.push_back("-fPIC");
        args.push_back("-Wl,--no-undefined");
        args.push_back("-o");
        args.push_back(outLib.string());
        for (const auto &obj : objects)
        {
            args.push_back(obj.string());
        }
        args.insert(args.end(), ld.begin(), ld.end());

        auto result = io::runCommand(hasCpp ? "g++" : "gcc", args, {}, ctx, false);
        return result.code == 0;
    }

    bool buildProjectDesktop(
        const crosside::Context &ctx,
        const crosside::model::ProjectSpec &project,
        const crosside::model::ModuleMap &modules,
        const std::vector<std::string> &activeModules,
        bool full,
        const std::string &mode,
        bool runAfter,
        bool detachRun,
        bool autoBuildModules)
    {
        const auto allModules = crosside::model::moduleClosure(activeModules, modules, ctx);
        if (autoBuildModules)
        {
            for (const auto &name : allModules)
            {
                auto it = modules.find(name);
                if (it == modules.end())
                {
                    continue;
                }
                if (!buildModuleDesktop(ctx, it->second, modules, full, mode))
                {
                    ctx.error("Failed auto-build module ", name);
                    return false;
                }
            }
        }

        std::vector<fs::path> sources;
        for (const auto &src : project.src)
        {
            if (fs::exists(src) && isCompilable(src))
            {
                sources.push_back(src);
            }
        }
        if (sources.empty())
        {
            ctx.error("No compilable sources for project ", project.name);
            return false;
        }

        std::vector<std::string> cc = project.main.cc;
        std::vector<std::string> cpp = project.main.cpp;
        std::vector<std::string> ld = project.main.ld;

        for (const auto &flag : project.desktop.cc)
        {
            appendUnique(cc, flag);
        }
        for (const auto &flag : project.desktop.cpp)
        {
            appendUnique(cpp, flag);
        }
        for (const auto &flag : project.desktop.ld)
        {
            if (!flag.empty())
            {
                ld.push_back(flag);
            }
        }

        for (const auto &inc : project.include)
        {
            addIncludeFlag(cc, cpp, inc);
        }

        std::vector<std::string> moduleLinkArgs;
        std::vector<std::string> moduleSysLdArgs;

        auto appendModuleLink = [&](const crosside::model::ModuleSpec &spec)
        {
            appendUnique(moduleLinkArgs, "-L" + (spec.dir / kDesktopFolder).string());
            appendUnique(moduleLinkArgs, "-l" + spec.name);
        };

        auto appendModuleSysLd = [&](const crosside::model::ModuleSpec &spec)
        {
            appendLdFlags(moduleSysLdArgs, spec.main.ldArgs);
            appendLdFlags(moduleSysLdArgs, spec.desktop.ldArgs);
        };

        for (const auto &moduleName : allModules)
        {
            auto it = modules.find(moduleName);
            if (it == modules.end())
            {
                ctx.warn("Missing module: ", moduleName);
                continue;
            }

            const auto &module = it->second;

            for (const auto &depName : module.depends)
            {
                auto depIt = modules.find(depName);
                if (depIt == modules.end())
                {
                    continue;
                }
                const auto &dep = depIt->second;
                collectModuleIncludes(dep, dep.desktop, cc, cpp);
                appendModuleLink(dep);
                appendModuleSysLd(dep);
            }

            collectModuleIncludes(module, module.desktop, cc, cpp);
            appendModuleLink(module);
            appendModuleSysLd(module);
        }

        if (!moduleLinkArgs.empty())
        {
            ld.push_back("-Wl,--start-group");
            ld.insert(ld.end(), moduleLinkArgs.begin(), moduleLinkArgs.end());
            ld.push_back("-Wl,--end-group");
            ld.insert(ld.end(), moduleSysLdArgs.begin(), moduleSysLdArgs.end());
        }

        applyDesktopMode(cc, cpp, mode);

        const std::string buildCacheKey = crosside::model::projectBuildCacheKey(project);
        const fs::path objRoot = project.root / "obj" / kDesktopFolder / buildCacheKey;
        io::ensureDir(objRoot);

        std::vector<fs::path> objects;
        if (!compileSources(ctx, project.root, objRoot, sources, cc, cpp, full, objects))
        {
            return false;
        }

        bool hasCpp = false;
        for (const auto &src : sources)
        {
            if (isCppSource(src))
            {
                hasCpp = true;
                break;
            }
        }

        const fs::path output = project.root / project.name;

        std::vector<std::string> args;
        args.push_back("-o");
        args.push_back(output.string());
        for (const auto &obj : objects)
        {
            args.push_back(obj.string());
        }
        args.insert(args.end(), ld.begin(), ld.end());

        auto result = io::runCommand(hasCpp ? "g++" : "gcc", args, {}, ctx, false);
        if (result.code != 0)
        {
            return false;
        }

        if (runAfter)
        {
            const auto runArgs = resolveDesktopRunArgs(ctx, project);
            auto run = detachRun
                           ? io::runCommandDetached(output.string(), runArgs, project.root, ctx, false)
                           : io::runCommand(output.string(), runArgs, project.root, ctx, false);
            return run.code == 0;
        }

        return true;
    }

} // namespace crosside::build
