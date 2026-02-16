#include "commands/module_command.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "io/fs_utils.hpp"

namespace fs = std::filesystem;

namespace crosside::commands
{
    namespace
    {

        struct ModuleInitOptions
        {
            std::string name;
            std::string author = "djokersoft";
            bool staticLib = true;
            bool force = false;
        };

        std::string lower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
                           { return static_cast<char>(std::tolower(c)); });
            return value;
        }

        bool isValidModuleName(const std::string &name)
        {
            if (name.empty())
            {
                return false;
            }
            for (unsigned char ch : name)
            {
                if (std::isalnum(ch) != 0)
                {
                    continue;
                }
                if (ch == '_' || ch == '-' || ch == '.')
                {
                    continue;
                }
                return false;
            }
            return true;
        }

        std::string toIdentifier(const std::string &value)
        {
            std::string out;
            out.reserve(value.size() + 4);
            for (unsigned char ch : value)
            {
                if (std::isalnum(ch) != 0)
                {
                    out.push_back(static_cast<char>(std::tolower(ch)));
                }
                else
                {
                    out.push_back('_');
                }
            }
            if (out.empty())
            {
                out = "module";
            }
            if (std::isdigit(static_cast<unsigned char>(out.front())) != 0)
            {
                out.insert(out.begin(), '_');
            }
            return out;
        }

        std::string toHeaderGuard(const std::string &moduleName)
        {
            std::string out = "MODULE_";
            for (unsigned char ch : moduleName)
            {
                if (std::isalnum(ch) != 0)
                {
                    out.push_back(static_cast<char>(std::toupper(ch)));
                }
                else
                {
                    out.push_back('_');
                }
            }
            out += "_H";
            return out;
        }

        bool writeTextFile(const fs::path &file, const std::string &content, bool force, const crosside::Context &ctx)
        {
            std::error_code ec;
            if (fs::exists(file, ec) && !force)
            {
                ctx.error("File already exists: ", file.string(), " (use --force)");
                return false;
            }

            if (!crosside::io::ensureDir(file.parent_path()))
            {
                ctx.error("Failed create directory: ", file.parent_path().string());
                return false;
            }

            std::ofstream out(file, std::ios::binary | std::ios::trunc);
            if (!out.is_open())
            {
                ctx.error("Failed write file: ", file.string());
                return false;
            }
            out << content;
            out.close();
            if (!out.good())
            {
                ctx.error("Failed flush file: ", file.string());
                return false;
            }
            return true;
        }

        std::string buildModuleJson(const ModuleInitOptions &opt)
        {
            std::ostringstream out;
            out << "{\n"
                << "  \"module\": \"" << opt.name << "\",\n"
                << "  \"about\": \"new module\",\n"
                << "  \"author\": \"" << opt.author << "\",\n"
                << "  \"version\": \"1.0.0\",\n"
                << "  \"depends\": [],\n"
                << "  \"static\": " << (opt.staticLib ? "true" : "false") << ",\n"
                << "  \"priority\": 0,\n"
                << "  \"system\": [\"linux\", \"windows\", \"android\", \"emscripten\"],\n"
                << "  \"src\": [\n"
                << "    \"src/" << opt.name << ".c\"\n"
                << "  ],\n"
                << "  \"include\": [\n"
                << "    \"include\"\n"
                << "  ],\n"
                << "  \"CPP_ARGS\": \"\",\n"
                << "  \"CC_ARGS\": \"\",\n"
                << "  \"LD_ARGS\": \"\",\n"
                << "  \"plataforms\": {\n"
                << "    \"linux\": {\n"
                << "      \"CPP_ARGS\": \"\",\n"
                << "      \"CC_ARGS\": \"\",\n"
                << "      \"LD_ARGS\": \"\",\n"
                << "      \"src\": [],\n"
                << "      \"include\": []\n"
                << "    },\n"
                << "    \"windows\": {\n"
                << "      \"CPP_ARGS\": \"\",\n"
                << "      \"CC_ARGS\": \"\",\n"
                << "      \"LD_ARGS\": \"\",\n"
                << "      \"src\": [],\n"
                << "      \"include\": []\n"
                << "    },\n"
                << "    \"android\": {\n"
                << "      \"CPP_ARGS\": \"\",\n"
                << "      \"CC_ARGS\": \"\",\n"
                << "      \"LD_ARGS\": \"\",\n"
                << "      \"src\": [],\n"
                << "      \"include\": []\n"
                << "    },\n"
                << "    \"emscripten\": {\n"
                << "      \"template\": \"\",\n"
                << "      \"CPP_ARGS\": \"\",\n"
                << "      \"CC_ARGS\": \"\",\n"
                << "      \"LD_ARGS\": \"\",\n"
                << "      \"src\": [],\n"
                << "      \"include\": []\n"
                << "    }\n"
                << "  }\n"
                << "}\n";
            return out.str();
        }

        std::string buildHeaderFile(const ModuleInitOptions &opt)
        {
            const std::string guard = toHeaderGuard(opt.name);
            const std::string symbol = toIdentifier(opt.name);
            std::ostringstream out;
            out << "#ifndef " << guard << "\n"
                << "#define " << guard << "\n"
                << "\n"
                << "#ifdef __cplusplus\n"
                << "extern \"C\" {\n"
                << "#endif\n"
                << "\n"
                << "int " << symbol << "_ping(void);\n"
                << "\n"
                << "#ifdef __cplusplus\n"
                << "}\n"
                << "#endif\n"
                << "\n"
                << "#endif\n";
            return out.str();
        }

        std::string buildSourceFile(const ModuleInitOptions &opt)
        {
            const std::string symbol = toIdentifier(opt.name);
            std::ostringstream out;
            out << "#include \"" << opt.name << ".h\"\n"
                << "\n"
                << "int " << symbol << "_ping(void)\n"
                << "{\n"
                << "    return 0;\n"
                << "}\n";
            return out.str();
        }

        bool parseModuleInitOptions(const std::vector<std::string> &args, ModuleInitOptions &opt, const crosside::Context &ctx)
        {
            std::vector<std::string> positionals;
            for (std::size_t i = 0; i < args.size(); ++i)
            {
                const std::string &arg = args[i];
                if (arg == "--force")
                {
                    opt.force = true;
                    continue;
                }
                if (arg == "--shared")
                {
                    opt.staticLib = false;
                    continue;
                }
                if (arg == "--static")
                {
                    opt.staticLib = true;
                    continue;
                }
                if (arg == "--author")
                {
                    if (i + 1 >= args.size())
                    {
                        ctx.error("--author requires value");
                        return false;
                    }
                    opt.author = args[++i];
                    continue;
                }
                if (arg.rfind("--", 0) == 0)
                {
                    ctx.error("Unknown module init option: ", arg);
                    return false;
                }
                positionals.push_back(arg);
            }

            if (positionals.empty())
            {
                ctx.error("module init: missing module name");
                return false;
            }
            if (positionals.size() > 1)
            {
                ctx.error("module init: too many positional arguments");
                return false;
            }

            opt.name = positionals.front();
            if (!isValidModuleName(opt.name))
            {
                ctx.error("Invalid module name: ", opt.name, " (allowed: letters, numbers, _, -, .)");
                return false;
            }
            return true;
        }

        int runModuleInitCommand(const crosside::Context &ctx, const fs::path &repoRoot, const std::vector<std::string> &args)
        {
            ModuleInitOptions opt;
            if (!parseModuleInitOptions(args, opt, ctx))
            {
                return 1;
            }

            const fs::path modulesRoot = fs::absolute(repoRoot / "modules");
            const fs::path moduleRoot = modulesRoot / opt.name;
            const fs::path moduleJson = moduleRoot / "module.json";
            const fs::path srcDir = moduleRoot / "src";
            const fs::path includeDir = moduleRoot / "include";
            const fs::path sourceFile = srcDir / (opt.name + ".c");
            const fs::path headerFile = includeDir / (opt.name + ".h");

            std::error_code ec;
            if (fs::exists(moduleRoot, ec) && !opt.force)
            {
                ctx.error("Module folder already exists: ", moduleRoot.string(), " (use --force)");
                return 1;
            }

            if (!crosside::io::ensureDir(srcDir) || !crosside::io::ensureDir(includeDir))
            {
                ctx.error("Failed create module folders under: ", moduleRoot.string());
                return 1;
            }

            if (!writeTextFile(moduleJson, buildModuleJson(opt), opt.force, ctx))
            {
                return 1;
            }
            if (!writeTextFile(headerFile, buildHeaderFile(opt), opt.force, ctx))
            {
                return 1;
            }
            if (!writeTextFile(sourceFile, buildSourceFile(opt), opt.force, ctx))
            {
                return 1;
            }

            ctx.log("Module scaffold created: ", moduleRoot.string());
            ctx.log("Next steps:");
            ctx.log("  ./bin/builder build module ", opt.name, " desktop --mode debug");
            return 0;
        }

    } // namespace

    int runModuleCommand(const crosside::Context &ctx, const fs::path &repoRoot, const std::vector<std::string> &args)
    {
        if (args.empty())
        {
            ctx.error("module: missing subcommand (use: init)");
            return 1;
        }

        const std::string subcommand = lower(args.front());
        std::vector<std::string> rest(args.begin() + 1, args.end());

        if (subcommand == "init")
        {
            return runModuleInitCommand(ctx, repoRoot, rest);
        }

        ctx.error("Unknown module subcommand: ", subcommand, " (use: init)");
        return 1;
    }

} // namespace crosside::commands
