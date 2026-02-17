#include "io/fs_utils.hpp"

#include <algorithm>
#include <system_error>

namespace fs = std::filesystem;

namespace crosside::io
{

    bool ensureDir(const fs::path &path)
    {
        std::error_code ec;
        if (fs::exists(path, ec))
        {
            return fs::is_directory(path, ec);
        }
        return fs::create_directories(path, ec) && !ec;
    }

    std::vector<fs::path> listModuleJsonFiles(const fs::path &modulesRoot)
    {
        std::vector<fs::path> out;
        std::error_code ec;
        if (!fs::exists(modulesRoot, ec))
        {
            return out;
        }

        for (const auto &entry : fs::directory_iterator(modulesRoot, ec))
        {
            if (ec)
            {
                break;
            }
            if (!entry.is_directory(ec))
            {
                continue;
            }
            fs::path file = entry.path() / "module.json";
            if (fs::exists(file, ec))
            {
                out.push_back(file);
            }
        }

        std::sort(out.begin(), out.end());
        return out;
    }

    std::vector<fs::path> listProjectFiles(const fs::path &projectsRoot)
    {
        std::vector<fs::path> out;
        std::error_code ec;
        if (!fs::exists(projectsRoot, ec))
        {
            return out;
        }

        for (const auto &entry : fs::recursive_directory_iterator(projectsRoot, ec))
        {
            if (ec)
            {
                break;
            }
            if (!entry.is_regular_file(ec))
            {
                continue;
            }
            const auto name = entry.path().filename().string();
            if (name == "main.mk" || name == "project.mk")
            {
                out.push_back(entry.path());
            }
        }

        std::sort(out.begin(), out.end());
        return out;
    }

    bool removePath(const fs::path &path, bool dryRun, const crosside::Context &ctx)
    {
        std::error_code ec;
        if (!fs::exists(path, ec))
        {
            return false;
        }

        if (dryRun)
        {
            ctx.log("Would remove: ", path.string());
            return true;
        }

        ctx.log("Remove: ", path.string());
        if (fs::is_directory(path, ec))
        {
            fs::remove_all(path, ec);
            if (ec)
            {
                ctx.error("Failed remove ", path.string(), " : ", ec.message());
                return false;
            }
            return true;
        }

        bool ok = fs::remove(path, ec);
        if (!ok || ec)
        {
            ctx.error("Failed remove ", path.string(), " : ", ec.message());
            return false;
        }
        return true;
    }

} // namespace crosside::io
