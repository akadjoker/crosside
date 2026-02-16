#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "commands/build_command.hpp"
#include "commands/clean_command.hpp"
#include "commands/list_command.hpp"
#include "commands/module_command.hpp"
#include "commands/serve_command.hpp"
#include "core/context.hpp"

namespace fs = std::filesystem;

namespace
{

    constexpr const char *kAppName = "builder";
    constexpr const char *kVersionLine = "by Luis Santos AKA Djoker";

    void printHelp()
    {
        std::cout << kAppName << " (C++ edition)\n"
                  << kVersionLine << "\n"
                  << "\n"
                  << "Usage:\n"
                  << "  " << kAppName << " build <subject> [name_or_target] [targets...] [options]\n"
                  << "  " << kAppName << " clean <subject> [name_or_target] [targets...] [options]\n"
                  << "  " << kAppName << " list [all|modules|projects]\n"
                  << "  " << kAppName << " module init <name> [--author NAME] [--shared|--static] [--force]\n"
                  << "  " << kAppName << " serve <path_or_file> [--port N] [--host 127.0.0.1] [--index file] [--no-open] [--detach]\n"
                  << "\n"
                  << "Examples:\n"
                  << "  " << kAppName << " build module raylib desktop --mode debug\n"
                  << "  " << kAppName << " build projects/sdl/tutorial_2.c desktop\n"
                  << "  " << kAppName << " module init mymodule --author \"Luis Santos\"\n"
                  << "  " << kAppName << " build bugame desktop --run\n"
                  << "  " << kAppName << " build bugame web --run --detach --port 8080\n"
                  << "  " << kAppName << " clean bugame web --dry-run\n"
                  << "  " << kAppName << " serve projects/bugame/Web/main.html --port 8080 --detach\n"
                  << "  " << kAppName << " list all\n";
    }

    fs::path detectRepoRoot(const char *argv0)
    {
        auto hasWorkspaceLayout = [](const fs::path &path) -> bool
        {
            std::error_code ec1, ec2;
            return fs::exists(path / "modules", ec1) && fs::exists(path / "projects", ec2);
        };

        std::error_code ec;
        fs::path cwd = fs::current_path(ec);
        if (!ec)
        {
            fs::path probe = cwd;
            for (int i = 0; i < 8; ++i)
            {
                if (hasWorkspaceLayout(probe))
                {
                    return probe;
                }
                if (!probe.has_parent_path())
                {
                    break;
                }
                probe = probe.parent_path();
            }
        }

        fs::path exe = fs::absolute(argv0, ec);
        if (ec)
        {
            return cwd;
        }

        fs::path probe = exe.parent_path();
        for (int i = 0; i < 8; ++i)
        {
            if (hasWorkspaceLayout(probe))
            {
                return probe;
            }
            if (!probe.has_parent_path())
            {
                break;
            }
            probe = probe.parent_path();
        }
        return cwd;
    }

    std::vector<std::string> collectArgs(int argc, char **argv, int startIndex)
    {
        std::vector<std::string> out;
        for (int i = startIndex; i < argc; ++i)
        {
            out.emplace_back(argv[i]);
        }
        return out;
    }

} // namespace

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printHelp();
        return 1;
    }

    const std::string command = argv[1];
    const crosside::Context ctx(true);
    const fs::path repoRoot = detectRepoRoot(argv[0]);

    if (command == "help" || command == "--help" || command == "-h")
    {
        printHelp();
        return 0;
    }
    if (command == "version" || command == "--version" || command == "-v")
    {
        std::cout << kAppName << " - " << kVersionLine << '\n';
        return 0;
    }

    if (command == "build")
    {
        return crosside::commands::runBuildCommand(ctx, repoRoot, collectArgs(argc, argv, 2));
    }
    if (command == "list")
    {
        return crosside::commands::runListCommand(ctx, repoRoot, collectArgs(argc, argv, 2));
    }
    if (command == "clean")
    {
        return crosside::commands::runCleanCommand(ctx, repoRoot, collectArgs(argc, argv, 2));
    }
    if (command == "serve")
    {
        return crosside::commands::runServeCommand(ctx, repoRoot, collectArgs(argc, argv, 2));
    }
    if (command == "module")
    {
        return crosside::commands::runModuleCommand(ctx, repoRoot, collectArgs(argc, argv, 2));
    }

    std::cerr << "Unknown command: " << command << '\n';
    printHelp();
    return 1;
}
