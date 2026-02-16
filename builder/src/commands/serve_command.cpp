#include "commands/serve_command.hpp"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "io/http_server.hpp"
#include "io/process.hpp"

namespace fs = std::filesystem;

namespace crosside::commands
{
    namespace
    {

        struct ServeOptions
        {
            fs::path path;
            std::string host = "127.0.0.1";
            int port = 8080;
            std::string indexFile = "index.html";
            bool openBrowser = true;
            bool detach = false;
        };

        void tryOpenBrowser(const crosside::Context &ctx, const std::string &url)
        {
#ifdef _WIN32
            crosside::io::runCommand("cmd", {"/c", "start", "", url}, {}, ctx, false);
#elif __APPLE__
            crosside::io::runCommand("open", {url}, {}, ctx, false);
#else
            crosside::io::runCommand("xdg-open", {url}, {}, ctx, false);
#endif
        }

        bool parseServeOptions(const std::vector<std::string> &args, ServeOptions &opt, const crosside::Context &ctx)
        {
            std::vector<std::string> positionals;
            for (std::size_t i = 0; i < args.size(); ++i)
            {
                const std::string &arg = args[i];
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
                    continue;
                }
                if (arg == "--host")
                {
                    if (i + 1 >= args.size())
                    {
                        ctx.error("--host requires value");
                        return false;
                    }
                    opt.host = args[++i];
                    continue;
                }
                if (arg == "--index")
                {
                    if (i + 1 >= args.size())
                    {
                        ctx.error("--index requires value");
                        return false;
                    }
                    opt.indexFile = args[++i];
                    continue;
                }
                if (arg == "--no-open")
                {
                    opt.openBrowser = false;
                    continue;
                }
                if (arg == "--detach")
                {
                    opt.detach = true;
                    continue;
                }
                if (arg == "--open")
                {
                    opt.openBrowser = true;
                    continue;
                }

                if (arg.rfind("--", 0) == 0)
                {
                    ctx.error("Unknown serve option: ", arg);
                    return false;
                }
                positionals.push_back(arg);
            }

            if (positionals.empty())
            {
                ctx.error("serve: missing path argument");
                return false;
            }
            opt.path = positionals[0];
            return true;
        }

    } // namespace

    int runServeCommand(const crosside::Context &ctx, const fs::path &repoRoot, const std::vector<std::string> &args)
    {
        ServeOptions opt;
        if (!parseServeOptions(args, opt, ctx))
        {
            return 1;
        }

        fs::path input = opt.path;
        if (!input.is_absolute())
        {
            input = fs::absolute(repoRoot / input);
        }

        std::error_code ec;
        if (!fs::exists(input, ec))
        {
            ctx.error("serve path not found: ", input.string());
            return 1;
        }

        crosside::io::StaticHttpServerOptions serverOpt;
        serverOpt.host = opt.host;
        serverOpt.port = opt.port;
        serverOpt.indexFile = opt.indexFile;

        std::string startPath = "/";
        if (fs::is_regular_file(input, ec))
        {
            serverOpt.root = input.parent_path();
            serverOpt.indexFile = input.filename().string();
            startPath += serverOpt.indexFile;
        }
        else
        {
            serverOpt.root = input;
        }

        const std::string url = "http://" + serverOpt.host + ":" + std::to_string(serverOpt.port) + startPath;
        ctx.log("Serve URL: ", url);

        if (opt.detach)
        {
            auto exePath = crosside::io::currentExecutablePath();
            if (!exePath.has_value())
            {
                ctx.error("Could not resolve builder executable path for --detach");
                return 1;
            }

            std::vector<std::string> detachedArgs = {
                "serve",
                input.string(),
                "--host",
                serverOpt.host,
                "--port",
                std::to_string(serverOpt.port),
                "--index",
                serverOpt.indexFile,
                "--no-open",
            };

            auto detached = crosside::io::runCommandDetached(exePath->string(), detachedArgs, {}, ctx, false);
            if (detached.code != 0)
            {
                ctx.error("Failed to start detached server");
                return 1;
            }
            if (detached.processId > 0)
            {
                ctx.log("Detached server launcher PID: ", detached.processId);
            }
            if (opt.openBrowser)
            {
                tryOpenBrowser(ctx, url);
            }
            return 0;
        }

        if (opt.openBrowser)
        {
            tryOpenBrowser(ctx, url);
        }

        if (!crosside::io::serveStaticHttp(ctx, serverOpt))
        {
            return 1;
        }
        return 0;
    }

} // namespace crosside::commands
