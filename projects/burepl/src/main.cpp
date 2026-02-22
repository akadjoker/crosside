#include "interpreter.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <iterator>

struct FileLoaderContext
{
    const char *searchPaths[16];
    int pathCount;
    char fullPath[1024];
    std::vector<unsigned char> data;
};

static bool hasSuffix(const std::string &s, const char *suffix)
{
    const size_t suffixLen = std::strlen(suffix);
    if (s.size() < suffixLen) return false;
    return std::memcmp(s.data() + (s.size() - suffixLen), suffix, suffixLen) == 0;
}

static bool isAbsolutePath(const char *path)
{
    if (!path || !*path) return false;
    if (path[0] == '/' || path[0] == '\\') return true;
#if defined(_WIN32)
    if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
        path[1] == ':')
    {
        return true;
    }
#endif
    return false;
}

static void pathDirname(const char *path, char *out, size_t outSize)
{
    if (!out || outSize == 0) return;
    out[0] = '\0';

    if (!path || !*path)
    {
        std::snprintf(out, outSize, ".");
        return;
    }

    const char *slash1 = std::strrchr(path, '/');
    const char *slash2 = std::strrchr(path, '\\');
    const char *slash = slash1;
    if (slash2 && (!slash1 || slash2 > slash1)) slash = slash2;

    if (!slash)
    {
        std::snprintf(out, outSize, ".");
        return;
    }

    size_t len = (size_t)(slash - path);
    if (len == 0)
    {
        std::snprintf(out, outSize, "/");
        return;
    }

    if (len >= outSize) len = outSize - 1;
    std::memcpy(out, path, len);
    out[len] = '\0';
}

static bool readFileBytes(const char *path, std::vector<unsigned char> &out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    f.seekg(0, std::ios::end);
    const std::streamoff size = f.tellg();
    if (size < 0) return false;
    f.seekg(0, std::ios::beg);

    out.resize((size_t)size);
    if (size > 0)
    {
        f.read(reinterpret_cast<char *>(out.data()), size);
        if (!f) return false;
    }
    return true;
}

static bool readFileText(const char *path, std::string &out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

const char *multiPathFileLoader(const char *filename, size_t *outSize, void *userdata)
{
    if (!filename || !outSize || !userdata) return nullptr;

    FileLoaderContext *ctx = reinterpret_cast<FileLoaderContext *>(userdata);
    *outSize = 0;

    auto tryLoad = [&](const char *path) -> const char *
    {
        if (!path || !*path) return nullptr;
        if (!readFileBytes(path, ctx->data)) return nullptr;

        const size_t payloadSize = ctx->data.size();
        ctx->data.push_back(0); // c-string safe for text paths
        *outSize = payloadSize;
        return reinterpret_cast<const char *>(ctx->data.data());
    };

    const char *relative = filename;
    while (*relative == '/' || *relative == '\\') relative++;

    if (isAbsolutePath(filename))
    {
        return tryLoad(filename);
    }

    const char *namePart = (relative != filename) ? relative : filename;
    for (int i = 0; i < ctx->pathCount; i++)
    {
        std::snprintf(ctx->fullPath, sizeof(ctx->fullPath), "%s/%s", ctx->searchPaths[i], namePart);
        const char *loaded = tryLoad(ctx->fullPath);
        if (loaded) return loaded;
    }

    const char *loaded = tryLoad(filename);
    if (loaded) return loaded;
    if (relative != filename) return tryLoad(relative);

    return nullptr;
}

static void configureLoader(FileLoaderContext &ctx, const char *entryPath)
{
    static char rootDir[1024];
    pathDirname(entryPath, rootDir, sizeof(rootDir));

    ctx.pathCount = 0;
    ctx.searchPaths[ctx.pathCount++] = rootDir;
    ctx.searchPaths[ctx.pathCount++] = ".";
}

static std::string defaultBytecodePath(const std::string &sourcePath)
{
    if (hasSuffix(sourcePath, ".bu"))
    {
        return sourcePath.substr(0, sourcePath.size() - 3) + ".buc";
    }
    return sourcePath + ".buc";
}

static bool loadPlugins(Interpreter &vm, const std::vector<std::string> &plugins)
{
    for (size_t i = 0; i < plugins.size(); i++)
    {
        const std::string &name = plugins[i];
        const bool pathLike =
            name.find('/') != std::string::npos ||
            name.find('\\') != std::string::npos ||
            hasSuffix(name, ".so") || hasSuffix(name, ".dll") || hasSuffix(name, ".dylib");

        bool ok = pathLike ? vm.loadPlugin(name.c_str()) : vm.loadPluginByName(name.c_str());
        if (!ok)
        {
            const char *err = vm.getLastPluginError();
            std::fprintf(stderr, "Plugin load failed: %s%s%s\n",
                         name.c_str(),
                         err ? " -> " : "",
                         err ? err : "");
            return false;
        }
    }
    return true;
}

static bool runSource(Interpreter &vm, FileLoaderContext &loader, const std::string &path, bool dump)
{
    std::string source;
    if (!readFileText(path.c_str(), source))
    {
        std::fprintf(stderr, "Cannot read source file: %s\n", path.c_str());
        return false;
    }

    configureLoader(loader, path.c_str());
    vm.setFileLoader(multiPathFileLoader, &loader);
    return vm.run(source.c_str(), dump);
}

static bool runBytecode(Interpreter &vm, FileLoaderContext &loader, const std::string &path)
{
    configureLoader(loader, path.c_str());
    vm.setFileLoader(multiPathFileLoader, &loader);
    return vm.loadBytecode(path.c_str());
}

static bool compileBytecode(Interpreter &vm, FileLoaderContext &loader, const std::string &srcPath, const std::string &outPath, bool dump)
{
    std::string source;
    if (!readFileText(srcPath.c_str(), source))
    {
        std::fprintf(stderr, "Cannot read source file: %s\n", srcPath.c_str());
        return false;
    }

    configureLoader(loader, srcPath.c_str());
    vm.setFileLoader(multiPathFileLoader, &loader);
    return vm.compileToBytecode(source.c_str(), outPath.c_str(), dump);
}

static bool copyBytecode(const std::string &srcPath, const std::string &dstPath)
{
    std::ifstream in(srcPath.c_str(), std::ios::binary);
    if (!in)
    {
        std::fprintf(stderr, "Cannot read bytecode file: %s\n", srcPath.c_str());
        return false;
    }

    std::ofstream out(dstPath.c_str(), std::ios::binary | std::ios::trunc);
    if (!out)
    {
        std::fprintf(stderr, "Cannot write bytecode file: %s\n", dstPath.c_str());
        return false;
    }

    out << in.rdbuf();
    if (!out.good())
    {
        std::fprintf(stderr, "Failed writing bytecode file: %s\n", dstPath.c_str());
        return false;
    }
    return true;
}

static void printUsage()
{
    std::puts("Usage:");
    std::puts("  burepl                            # interactive REPL");
    std::puts("  burepl <file.bu>                  # run source script");
    std::puts("  burepl <file.buc>                 # run bytecode");
    std::puts("  burepl --run-bc <file.buc>        # run bytecode");
    std::puts("  burepl --compile-bc <in.bu> [out.buc]");
    std::puts("  burepl --copy-bc <in.buc> <out.buc>");
    std::puts("  burepl --plugin <name|path> [--plugin ...]");
    std::puts("  burepl --dump ...                 # disassemble on compile/run");
}

int main(int argc, char **argv)
{
    enum Mode
    {
        MODE_REPL,
        MODE_RUN_SOURCE,
        MODE_RUN_BC,
        MODE_COMPILE_BC,
        MODE_COPY_BC
    };

    Mode mode = MODE_REPL;
    bool dump = false;
    std::vector<std::string> plugins;
    std::string inputPath;
    std::string outputPath;

    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h")
        {
            printUsage();
            return 0;
        }
        else if (arg == "--dump")
        {
            dump = true;
        }
        else if (arg == "--plugin")
        {
            if (i + 1 >= argc)
            {
                std::fprintf(stderr, "--plugin requires a value\n");
                return 1;
            }
            plugins.push_back(argv[++i]);
        }
        else if (arg == "--repl")
        {
            mode = MODE_REPL;
        }
        else if (arg == "--run-bc" || arg == "--run-bytecode")
        {
            if (i + 1 >= argc)
            {
                std::fprintf(stderr, "%s requires <file.buc>\n", arg.c_str());
                return 1;
            }
            mode = MODE_RUN_BC;
            inputPath = argv[++i];
        }
        else if (arg == "--compile-bc")
        {
            if (i + 1 >= argc)
            {
                std::fprintf(stderr, "--compile-bc requires <in.bu> [out.buc]\n");
                return 1;
            }
            mode = MODE_COMPILE_BC;
            inputPath = argv[++i];
            if (i + 1 < argc)
            {
                std::string maybeOut = argv[i + 1];
                if (!maybeOut.empty() && maybeOut[0] != '-')
                {
                    outputPath = maybeOut;
                    i++;
                }
            }
        }
        else if (arg == "--copy-bc")
        {
            if (i + 2 >= argc)
            {
                std::fprintf(stderr, "--copy-bc requires <in.buc> <out.buc>\n");
                return 1;
            }
            mode = MODE_COPY_BC;
            inputPath = argv[++i];
            outputPath = argv[++i];
        }
        else if (!arg.empty() && arg[0] == '-')
        {
            std::fprintf(stderr, "Unknown option: %s\n", arg.c_str());
            printUsage();
            return 1;
        }
        else
        {
            if (hasSuffix(arg, ".buc") || hasSuffix(arg, ".bubc") || hasSuffix(arg, ".bytecode"))
            {
                mode = MODE_RUN_BC;
            }
            else
            {
                mode = MODE_RUN_SOURCE;
            }
            inputPath = arg;
        }
    }

    if (mode == MODE_COPY_BC)
    {
        return copyBytecode(inputPath, outputPath) ? 0 : 1;
    }

    Interpreter vm;
    vm.registerAll();
    vm.addPluginSearchPath(".");
    vm.addPluginSearchPath("./plugins");

    if (!loadPlugins(vm, plugins))
    {
        return 1;
    }

    FileLoaderContext loader = {};

    if (mode == MODE_RUN_SOURCE)
    {
        return runSource(vm, loader, inputPath, dump) ? 0 : 1;
    }

    if (mode == MODE_RUN_BC)
    {
        return runBytecode(vm, loader, inputPath) ? 0 : 1;
    }

    if (mode == MODE_COMPILE_BC)
    {
        if (outputPath.empty()) outputPath = defaultBytecodePath(inputPath);
        const bool ok = compileBytecode(vm, loader, inputPath, outputPath, dump);
        if (ok) std::printf("Bytecode saved: %s\n", outputPath.c_str());
        return ok ? 0 : 1;
    }

    loader.pathCount = 1;
    loader.searchPaths[0] = ".";
    vm.setFileLoader(multiPathFileLoader, &loader);

    std::puts("BuLang REPL (headless)");
    std::puts("Commands: .help  .quit  .reset  .load <file.bu>");

    std::string line;
    while (true)
    {
        std::cout << "bu> " << std::flush;
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;

        if (line == ".quit" || line == ".exit")
        {
            break;
        }
        if (line == ".help")
        {
            std::puts("Type BuLang code and press enter.");
            std::puts(".load <file.bu> : run source file");
            std::puts(".reset          : reset VM internals");
            std::puts(".quit           : exit");
            continue;
        }
        if (line == ".reset")
        {
            vm.reset();
            std::puts("VM reset.");
            continue;
        }
        if (line.size() > 6 && line.substr(0, 6) == ".load ")
        {
            std::string path = line.substr(6);
            std::string src;
            if (!readFileText(path.c_str(), src))
            {
                std::fprintf(stderr, "Cannot read source file: %s\n", path.c_str());
                continue;
            }
            configureLoader(loader, path.c_str());
            vm.setFileLoader(multiPathFileLoader, &loader);
            vm.run(src.c_str(), false);
            loader.pathCount = 1;
            loader.searchPaths[0] = ".";
            vm.setFileLoader(multiPathFileLoader, &loader);
            continue;
        }

        vm.run(line.c_str(), false);
    }

    return 0;
}
