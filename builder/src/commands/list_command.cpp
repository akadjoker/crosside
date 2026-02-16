#include "commands/list_command.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

#include "io/fs_utils.hpp"
#include "model/loader.hpp"

namespace fs = std::filesystem;

namespace crosside::commands
{
    namespace
    {

        std::string lower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
                           { return static_cast<char>(std::tolower(c)); });
            return value;
        }

    } // namespace

    int runListCommand(const crosside::Context &ctx, const fs::path &repoRoot, const std::vector<std::string> &args)
    {
        std::string what = args.empty() ? "all" : lower(args.front());
        if (what != "all" && what != "modules" && what != "apps" && what != "projects")
        {
            ctx.error("Invalid list target: ", what, " (use all|modules|apps|projects)");
            return 1;
        }

        if (what == "all" || what == "modules")
        {
            auto modules = crosside::model::discoverModules(repoRoot / "modules", ctx);
            std::vector<std::string> names;
            names.reserve(modules.size());
            for (const auto &item : modules)
            {
                names.push_back(item.first);
            }
            std::sort(names.begin(), names.end());

            ctx.log("Modules:");
            if (names.empty())
            {
                ctx.log("  <none>");
            }
            for (const auto &name : names)
            {
                const auto &module = modules.at(name);
                std::string systems;
                for (size_t i = 0; i < module.systems.size(); ++i)
                {
                    if (i > 0)
                    {
                        systems += ",";
                    }
                    systems += module.systems[i];
                }
                if (systems.empty())
                {
                    systems = "-";
                }
                ctx.log("  ", module.name, "  [", systems, "]  ", module.dir.string());
            }
        }

        if (what == "all" || what == "apps" || what == "projects")
        {
            ctx.log("Projects:");
            auto files = crosside::io::listProjectFiles(repoRoot / "projects");
            if (files.empty())
            {
                ctx.log("  <none>");
                return 0;
            }

            for (const auto &file : files)
            {
                auto project = crosside::model::loadProjectFile(file, ctx);
                if (!project.has_value())
                {
                    ctx.log("  ", file.string(), "  [invalid]");
                    continue;
                }
                ctx.log("  ", project->root.filename().string(), " (name=", project->name, ")  ", file.string());
            }
        }

        return 0;
    }

} // namespace crosside::commands
