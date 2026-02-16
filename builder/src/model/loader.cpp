#include "model/loader.hpp"

#include <algorithm>
#include <functional>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include "io/fs_utils.hpp"
#include "io/json_reader.hpp"

namespace fs = std::filesystem;
using nlohmann::json;

namespace crosside::model
{

    namespace
    {

        std::vector<std::string> toStringList(const json &node)
        {
            std::vector<std::string> out;
            if (!node.is_array())
            {
                return out;
            }

            for (const auto &item : node)
            {
                if (!item.is_string())
                {
                    continue;
                }
                std::string value = item.get<std::string>();
                if (!value.empty())
                {
                    out.push_back(value);
                }
            }
            return out;
        }

        std::unordered_map<std::string, std::string> toStringMap(const json &node)
        {
            std::unordered_map<std::string, std::string> out;
            if (!node.is_object())
            {
                return out;
            }

            for (auto it = node.begin(); it != node.end(); ++it)
            {
                if (!it.value().is_string())
                {
                    continue;
                }
                out[it.key()] = it.value().get<std::string>();
            }

            return out;
        }

        BuildArgs parseBuildArgs(const json &node)
        {
            BuildArgs out;
            if (!node.is_object())
            {
                return out;
            }

            if (node.contains("CPP"))
            {
                if (node["CPP"].is_string())
                {
                    out.cpp = io::splitFlags(node["CPP"].get<std::string>());
                }
                else
                {
                    out.cpp = toStringList(node["CPP"]);
                }
            }
            if (node.contains("CC"))
            {
                if (node["CC"].is_string())
                {
                    out.cc = io::splitFlags(node["CC"].get<std::string>());
                }
                else
                {
                    out.cc = toStringList(node["CC"]);
                }
            }
            if (node.contains("LD"))
            {
                if (node["LD"].is_string())
                {
                    out.ld = io::splitFlags(node["LD"].get<std::string>());
                }
                else
                {
                    out.ld = toStringList(node["LD"]);
                }
            }
            return out;
        }

        PlatformBlock parsePlatformBlock(const json &node)
        {
            PlatformBlock out;
            if (!node.is_object())
            {
                return out;
            }

            if (node.contains("src"))
            {
                out.src = toStringList(node["src"]);
            }
            if (node.contains("include"))
            {
                out.include = toStringList(node["include"]);
            }

            if (node.contains("CPP_ARGS"))
            {
                if (node["CPP_ARGS"].is_string())
                {
                    out.cppArgs = io::splitFlags(node["CPP_ARGS"].get<std::string>());
                }
                else
                {
                    out.cppArgs = toStringList(node["CPP_ARGS"]);
                }
            }
            if (node.contains("CC_ARGS"))
            {
                if (node["CC_ARGS"].is_string())
                {
                    out.ccArgs = io::splitFlags(node["CC_ARGS"].get<std::string>());
                }
                else
                {
                    out.ccArgs = toStringList(node["CC_ARGS"]);
                }
            }
            if (node.contains("LD_ARGS"))
            {
                if (node["LD_ARGS"].is_string())
                {
                    out.ldArgs = io::splitFlags(node["LD_ARGS"].get<std::string>());
                }
                else
                {
                    out.ldArgs = toStringList(node["LD_ARGS"]);
                }
            }

            if (node.contains("template") && node["template"].is_string())
            {
                out.shellTemplate = node["template"].get<std::string>();
            }

            return out;
        }

        fs::path toAbsolute(const fs::path &base, const std::string &value)
        {
            fs::path path(value);
            if (path.is_absolute())
            {
                return path;
            }
            return fs::absolute(base / path);
        }

    } // namespace

    std::string hostDesktopKey()
    {
#ifdef _WIN32
        return "windows";
#else
        return "linux";
#endif
    }

    std::string defaultTargetFromConfig(const fs::path &repoRoot)
    {
        fs::path configPath = repoRoot / "config.json";
        if (!fs::exists(configPath))
        {
            return "desktop";
        }

        try
        {
            json data = io::loadJsonFile(configPath);
            json root = data;
            if (data.contains("Configuration") && data["Configuration"].is_object())
            {
                root = data["Configuration"];
            }
            if (!root.contains("Session") || !root["Session"].is_object())
            {
                return "desktop";
            }
            int value = root["Session"].value("CurrentPlatform", 0);
            if (value == 1)
            {
                return "android";
            }
            if (value == 2)
            {
                return "web";
            }
        }
        catch (...)
        {
            return "desktop";
        }
        return "desktop";
    }

    std::optional<ModuleSpec> loadModuleFile(const fs::path &moduleFile, const crosside::Context &ctx)
    {
        try
        {
            json data = io::loadJsonFile(moduleFile);

            ModuleSpec module;
            module.dir = fs::absolute(moduleFile.parent_path());
            module.name = data.value("module", module.dir.filename().string());
            module.staticLib = data.value("static", true);

            module.depends = toStringList(data.value("depends", json::array()));
            module.systems = toStringList(data.value("system", json::array()));

            module.main.src = toStringList(data.value("src", json::array()));
            module.main.include = toStringList(data.value("include", json::array()));

            if (data.contains("CPP_ARGS") && data["CPP_ARGS"].is_string())
            {
                module.main.cppArgs = io::splitFlags(data["CPP_ARGS"].get<std::string>());
            }
            if (data.contains("CC_ARGS") && data["CC_ARGS"].is_string())
            {
                module.main.ccArgs = io::splitFlags(data["CC_ARGS"].get<std::string>());
            }
            if (data.contains("LD_ARGS") && data["LD_ARGS"].is_string())
            {
                module.main.ldArgs = io::splitFlags(data["LD_ARGS"].get<std::string>());
            }

            if (data.contains("plataforms") && data["plataforms"].is_object())
            {
                const auto &platforms = data["plataforms"];

                std::string desktopKey = hostDesktopKey();
                if (platforms.contains(desktopKey))
                {
                    module.desktop = parsePlatformBlock(platforms[desktopKey]);
                }
                if (platforms.contains("android"))
                {
                    module.android = parsePlatformBlock(platforms["android"]);
                }
                if (platforms.contains("emscripten"))
                {
                    module.web = parsePlatformBlock(platforms["emscripten"]);
                }
            }

            return module;
        }
        catch (const std::exception &e)
        {
            ctx.error("Failed parse module ", moduleFile.string(), " : ", e.what());
            return std::nullopt;
        }
    }

    std::optional<ProjectSpec> loadProjectFile(const fs::path &projectFile, const crosside::Context &ctx)
    {
        try
        {
            json data = io::loadJsonFile(projectFile);

            ProjectSpec project;
            project.filePath = fs::absolute(projectFile);
            project.name = data.value("Name", projectFile.stem().string());

            fs::path rootBase = fs::absolute(projectFile.parent_path());
            if (data.contains("Path") && data["Path"].is_string())
            {
                fs::path fromJson(data["Path"].get<std::string>());
                project.root = fromJson.is_absolute() ? fromJson : fs::absolute(rootBase / fromJson);
            }
            else
            {
                project.root = rootBase;
            }

            project.modules = toStringList(data.value("Modules", json::array()));

            for (const auto &item : toStringList(data.value("Src", json::array())))
            {
                project.src.push_back(toAbsolute(project.root, item));
            }
            for (const auto &item : toStringList(data.value("Include", json::array())))
            {
                project.include.push_back(toAbsolute(project.root, item));
            }

            project.main = parseBuildArgs(data.value("Main", json::object()));
            project.desktop = parseBuildArgs(data.value("Desktop", json::object()));
            project.android = parseBuildArgs(data.value("Android", json::object()));
            project.web = parseBuildArgs(data.value("Web", json::object()));

            if (data.contains("Android") && data["Android"].is_object())
            {
                const auto &android = data["Android"];
                project.androidPackage = android.value("PACKAGE", "");
                project.androidActivity = android.value("ACTIVITY", "");
                project.androidLabel = android.value("LABEL", "");

                auto parsePathField = [&](const json &node, const std::string &key, fs::path &outPath)
                {
                    if (!node.contains(key) || !node[key].is_string())
                    {
                        return;
                    }
                    const std::string rel = node[key].get<std::string>();
                    if (rel.empty())
                    {
                        return;
                    }
                    outPath = toAbsolute(project.root, rel);
                };

                auto parsePathMapField = [&](const json &node, const std::string &key, std::unordered_map<std::string, fs::path> &outMap)
                {
                    if (!node.contains(key) || !node[key].is_object())
                    {
                        return;
                    }
                    for (auto it = node[key].begin(); it != node[key].end(); ++it)
                    {
                        if (!it.value().is_string())
                        {
                            continue;
                        }
                        const std::string rel = it.value().get<std::string>();
                        if (rel.empty())
                        {
                            continue;
                        }
                        outMap[it.key()] = toAbsolute(project.root, rel);
                    }
                };

                auto parsePathListField = [&](const json &node, const std::string &key, std::vector<fs::path> &outList)
                {
                    if (!node.contains(key))
                    {
                        return;
                    }

                    if (node[key].is_string())
                    {
                        const std::string rel = node[key].get<std::string>();
                        if (!rel.empty())
                        {
                            outList.push_back(toAbsolute(project.root, rel));
                        }
                        return;
                    }

                    if (!node[key].is_array())
                    {
                        return;
                    }

                    for (const auto &item : node[key])
                    {
                        if (!item.is_string())
                        {
                            continue;
                        }
                        const std::string rel = item.get<std::string>();
                        if (!rel.empty())
                        {
                            outList.push_back(toAbsolute(project.root, rel));
                        }
                    }
                };

                parsePathField(android, "ICON", project.androidIcon);
                parsePathMapField(android, "ICONS", project.androidIcons);
                parsePathField(android, "ROUND_ICON", project.androidRoundIcon);
                parsePathMapField(android, "ROUND_ICONS", project.androidRoundIcons);
                project.androidManifestMode = android.value("MANIFEST_MODE", "");
                if (project.androidManifestMode.empty())
                {
                    project.androidManifestMode = android.value("MANIFEST_TYPE", "");
                }

                parsePathListField(android, "JAVA_SOURCES", project.androidJavaSources);
                parsePathListField(android, "JAVA", project.androidJavaSources);
                parsePathListField(android, "JAVA_DIRS", project.androidJavaSources);

                if (android.contains("ADAPTIVE_ICON") && android["ADAPTIVE_ICON"].is_object())
                {
                    const auto &adaptive = android["ADAPTIVE_ICON"];
                    parsePathField(adaptive, "FOREGROUND", project.androidAdaptiveForeground);
                    parsePathField(adaptive, "MONOCHROME", project.androidAdaptiveMonochrome);
                    if (adaptive.contains("BACKGROUND") && adaptive["BACKGROUND"].is_string())
                    {
                        const std::string value = adaptive["BACKGROUND"].get<std::string>();
                        if (!value.empty())
                        {
                            if (value.front() == '#')
                            {
                                project.androidAdaptiveBackgroundColor = value;
                            }
                            else
                            {
                                project.androidAdaptiveBackgroundImage = toAbsolute(project.root, value);
                            }
                        }
                    }
                    project.androidAdaptiveRound = adaptive.value("ROUND", true);
                }

                parsePathField(android, "ADAPTIVE_FOREGROUND", project.androidAdaptiveForeground);
                parsePathField(android, "ADAPTIVE_MONOCHROME", project.androidAdaptiveMonochrome);
                if (android.contains("ADAPTIVE_BACKGROUND") && android["ADAPTIVE_BACKGROUND"].is_string())
                {
                    const std::string value = android["ADAPTIVE_BACKGROUND"].get<std::string>();
                    if (!value.empty())
                    {
                        if (value.front() == '#')
                        {
                            project.androidAdaptiveBackgroundColor = value;
                            project.androidAdaptiveBackgroundImage.clear();
                        }
                        else
                        {
                            project.androidAdaptiveBackgroundImage = toAbsolute(project.root, value);
                            project.androidAdaptiveBackgroundColor.clear();
                        }
                    }
                }
                if (android.contains("ADAPTIVE_ROUND") && android["ADAPTIVE_ROUND"].is_boolean())
                {
                    project.androidAdaptiveRound = android["ADAPTIVE_ROUND"].get<bool>();
                }

                std::string manifestTemplate = android.value("MANIFEST_TEMPLATE", "");
                if (manifestTemplate.empty())
                {
                    manifestTemplate = android.value("MANIFEST", "");
                }
                if (!manifestTemplate.empty())
                {
                    project.androidManifestTemplate = toAbsolute(project.root, manifestTemplate);
                }

                if (android.contains("MANIFEST_VARS"))
                {
                    project.androidManifestVars = toStringMap(android["MANIFEST_VARS"]);
                }
            }
            if (data.contains("Web") && data["Web"].is_object())
            {
                project.webShell = data["Web"].value("SHELL", "");
            }

            return project;
        }
        catch (const std::exception &e)
        {
            ctx.error("Failed parse project ", projectFile.string(), " : ", e.what());
            return std::nullopt;
        }
    }

    ModuleMap discoverModules(const fs::path &modulesRoot, const crosside::Context &ctx)
    {
        ModuleMap modules;
        for (const auto &file : io::listModuleJsonFiles(modulesRoot))
        {
            auto spec = loadModuleFile(file, ctx);
            if (spec.has_value())
            {
                modules[spec->name] = spec.value();
            }
        }
        return modules;
    }

    fs::path resolveModuleFile(
        const fs::path &repoRoot,
        const std::string &moduleName,
        const std::string &explicitFile)
    {
        if (!explicitFile.empty())
        {
            fs::path path(explicitFile);
            if (!path.is_absolute())
            {
                path = fs::absolute(repoRoot / path);
            }
            return path;
        }
        return fs::absolute(repoRoot / "modules" / moduleName / "module.json");
    }

    fs::path resolveProjectFile(
        const fs::path &repoRoot,
        const std::string &projectHint,
        const std::string &explicitFile)
    {
        if (!explicitFile.empty())
        {
            fs::path path(explicitFile);
            if (!path.is_absolute())
            {
                path = fs::absolute(repoRoot / path);
            }
            return path;
        }

        fs::path hint(projectHint);
        if (hint.is_absolute())
        {
            if (fs::is_directory(hint))
            {
                return fs::absolute(hint / "main.mk");
            }
            return fs::absolute(hint);
        }

        fs::path fromRepo = fs::absolute(repoRoot / hint);
        if (fs::exists(fromRepo))
        {
            if (fs::is_directory(fromRepo))
            {
                return fs::absolute(fromRepo / "main.mk");
            }
            return fromRepo;
        }

        return fs::absolute(repoRoot / "projects" / projectHint / "main.mk");
    }

    std::vector<std::string> moduleClosure(
        const std::vector<std::string> &seedModules,
        const ModuleMap &modules,
        const crosside::Context &ctx)
    {
        std::vector<std::string> ordered;
        std::unordered_set<std::string> visited;
        std::unordered_set<std::string> active;

        std::function<void(const std::string &)> visit = [&](const std::string &name)
        {
            if (name.empty())
            {
                return;
            }
            if (visited.count(name) != 0U)
            {
                return;
            }
            if (active.count(name) != 0U)
            {
                ctx.warn("Circular dependency at ", name);
                return;
            }

            auto it = modules.find(name);
            if (it == modules.end())
            {
                ctx.warn("Missing module dependency: ", name);
                return;
            }

            active.insert(name);
            for (const auto &dep : it->second.depends)
            {
                if (!dep.empty() && dep != name)
                {
                    visit(dep);
                }
            }
            active.erase(name);

            visited.insert(name);
            ordered.push_back(name);
        };

        for (const auto &seed : seedModules)
        {
            visit(seed);
        }

        return ordered;
    }

    std::vector<std::string> loadGlobalModules(const fs::path &repoRoot, const crosside::Context &)
    {
        fs::path configPath = repoRoot / "config.json";
        if (!fs::exists(configPath))
        {
            return {};
        }

        try
        {
            json data = io::loadJsonFile(configPath);
            json root = data;
            if (data.contains("Configuration") && data["Configuration"].is_object())
            {
                root = data["Configuration"];
            }
            return toStringList(root.value("Modules", json::array()));
        }
        catch (...)
        {
            return {};
        }
    }

    std::vector<std::string> loadSingleFileModules(
        const fs::path &repoRoot,
        const crosside::Context &ctx)
    {
        fs::path configPath = repoRoot / "config.json";
        if (!fs::exists(configPath))
        {
            return loadGlobalModules(repoRoot, ctx);
        }

        try
        {
            json data = io::loadJsonFile(configPath);
            json root = data;
            if (data.contains("Configuration") && data["Configuration"].is_object())
            {
                root = data["Configuration"];
            }

            if (root.contains("SingleFileModules"))
            {
                const std::vector<std::string> singleModules =
                    root["SingleFileModules"].is_array() ? toStringList(root["SingleFileModules"]) : std::vector<std::string>{};
                if (!singleModules.empty())
                {
                    return singleModules;
                }
            }
        }
        catch (...)
        {
            return loadGlobalModules(repoRoot, ctx);
        }

        return loadGlobalModules(repoRoot, ctx);
    }

    std::optional<fs::path> loadDefaultWebShell(const fs::path &repoRoot)
    {
        fs::path configPath = repoRoot / "config.json";
        if (!fs::exists(configPath))
        {
            return std::nullopt;
        }

        try
        {
            json data = io::loadJsonFile(configPath);
            json root = data;
            if (data.contains("Configuration") && data["Configuration"].is_object())
            {
                root = data["Configuration"];
            }

            std::string shellPath;
            if (root.contains("Web") && root["Web"].is_object())
            {
                const auto &web = root["Web"];
                auto readString = [&](const char *key)
                {
                    if (shellPath.empty() && web.contains(key) && web[key].is_string())
                    {
                        shellPath = web[key].get<std::string>();
                    }
                };

                readString("SHELL");
                readString("Shell");
                readString("ShellTemplate");
                readString("Template");
            }

            if (shellPath.empty() && root.contains("WebShell") && root["WebShell"].is_string())
            {
                shellPath = root["WebShell"].get<std::string>();
            }

            if (shellPath.empty())
            {
                return std::nullopt;
            }

            fs::path shell = fs::path(shellPath);
            if (!shell.is_absolute())
            {
                shell = fs::absolute(repoRoot / shell);
            }

            return shell;
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

} // namespace crosside::model
