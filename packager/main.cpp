#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "context.hpp"
#include "json.hpp"
#include "process.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace
{
const std::vector<std::string> kContentDirs = {"assets", "resources", "res", "data", "media"};

bool check(const crosside::io::ProcessResult &res)
{
    if (res.code != 0)
    {
        std::cerr << "[ERROR] Command failed (" << res.code << "): " << res.commandLine << std::endl;
        return false;
    }
    return true;
}

json loadJson(const fs::path &path)
{
    if (!fs::exists(path))
    {
        return json::object();
    }
    std::ifstream f(path);
    json j;
    try
    {
        f >> j;
    }
    catch (const std::exception &e)
    {
        std::cerr << "[ERROR] Failed to parse JSON " << path << ": " << e.what() << std::endl;
        return json::object();
    }
    return j;
}

bool isStringField(const json &obj, const std::string &key)
{
    return obj.contains(key) && obj[key].is_string() && !obj[key].get<std::string>().empty();
}

std::string readTextFile(const fs::path &path)
{
    std::ifstream f(path, std::ios::in | std::ios::binary);
    if (!f.good())
    {
        return "";
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool writeTextFile(const fs::path &path, const std::string &text)
{
    std::ofstream f(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!f.good())
    {
        return false;
    }
    f << text;
    return f.good();
}

void replaceAll(std::string &text, const std::string &from, const std::string &to)
{
    if (from.empty())
    {
        return;
    }
    std::size_t start = 0;
    while (true)
    {
        const std::size_t pos = text.find(from, start);
        if (pos == std::string::npos)
        {
            break;
        }
        text.replace(pos, from.size(), to);
        start = pos + to.size();
    }
}

bool hasBytecodeExt(const fs::path &path)
{
    const std::string ext = path.extension().string();
    return (ext == ".buc" || ext == ".bubc" || ext == ".bytecode");
}

bool cleanDir(const fs::path &path)
{
    std::error_code ec;
    if (fs::exists(path, ec))
    {
        fs::remove_all(path, ec);
        if (ec)
        {
            std::cerr << "[ERROR] Failed cleaning folder: " << path << " (" << ec.message() << ")" << std::endl;
            return false;
        }
    }
    fs::create_directories(path, ec);
    if (ec)
    {
        std::cerr << "[ERROR] Failed creating folder: " << path << " (" << ec.message() << ")" << std::endl;
        return false;
    }
    return true;
}

bool copyFileSafe(const fs::path &src, const fs::path &dst)
{
    if (!fs::exists(src))
    {
        std::cerr << "[ERROR] Missing file: " << src << std::endl;
        return false;
    }

    std::error_code ec;
    fs::create_directories(dst.parent_path(), ec);
    if (ec)
    {
        std::cerr << "[ERROR] Failed creating dir for " << dst << " (" << ec.message() << ")" << std::endl;
        return false;
    }

    fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
    if (ec)
    {
        std::cerr << "[ERROR] Failed copying " << src << " -> " << dst << " (" << ec.message() << ")" << std::endl;
        return false;
    }
    return true;
}

bool copyTreeSafe(const fs::path &src, const fs::path &dst)
{
    if (!fs::exists(src))
    {
        return true;
    }
    std::error_code ec;
    fs::create_directories(dst, ec);
    if (ec)
    {
        std::cerr << "[ERROR] Failed creating dir " << dst << " (" << ec.message() << ")" << std::endl;
        return false;
    }
    fs::copy(src, dst, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    if (ec)
    {
        std::cerr << "[ERROR] Failed copying folder " << src << " -> " << dst << " (" << ec.message() << ")" << std::endl;
        return false;
    }
    return true;
}

bool startsWith(const std::string &value, const std::string &prefix)
{
    if (prefix.size() > value.size())
    {
        return false;
    }
    return std::equal(prefix.begin(), prefix.end(), value.begin());
}

bool looksLikeAndroidResRoot(const fs::path &dir)
{
    if (!fs::exists(dir) || !fs::is_directory(dir))
    {
        return false;
    }

    for (const auto &entry : fs::directory_iterator(dir))
    {
        if (!entry.is_directory())
        {
            continue;
        }
        const std::string name = entry.path().filename().string();
        if (startsWith(name, "mipmap-") ||
            startsWith(name, "drawable") ||
            startsWith(name, "values") ||
            startsWith(name, "xml") ||
            startsWith(name, "layout") ||
            startsWith(name, "raw") ||
            startsWith(name, "color") ||
            startsWith(name, "font") ||
            startsWith(name, "menu") ||
            startsWith(name, "anim") ||
            startsWith(name, "animator") ||
            startsWith(name, "navigation") ||
            startsWith(name, "interpolator"))
        {
            return true;
        }
    }
    return false;
}

fs::path findTool(const std::string &name, const std::vector<fs::path> &searchPaths)
{
    for (const auto &path : searchPaths)
    {
        if (path.empty())
        {
            continue;
        }
        fs::path tool = path / name;
#ifdef _WIN32
        fs::path exe = tool;
        exe.replace_extension(".exe");
        if (fs::exists(exe))
        {
            return exe;
        }
        fs::path bat = tool;
        bat.replace_extension(".bat");
        if (fs::exists(bat))
        {
            return bat;
        }
#else
        if (fs::exists(tool))
        {
            return tool;
        }
#endif
    }
    return fs::path();
}

struct ReleaseInfo
{
    std::string name;
    fs::path jsonPath;
    fs::path contentRoot;
    json raw;
};

bool resolveReleaseInfo(const fs::path &bugameRoot, const std::string &releaseArg, ReleaseInfo &outInfo)
{
    fs::path relPath = releaseArg;

    if (relPath.extension() != ".json")
    {
        relPath = bugameRoot / "releases" / (releaseArg + ".json");
    }
    else if (!relPath.is_absolute())
    {
        fs::path cwdTry = fs::absolute(relPath);
        if (fs::exists(cwdTry))
        {
            relPath = cwdTry;
        }
        else
        {
            relPath = bugameRoot / relPath;
        }
    }
    relPath = fs::absolute(relPath);

    if (!fs::exists(relPath))
    {
        std::cerr << "[ERROR] Release JSON not found: " << relPath << std::endl;
        return false;
    }

    json data = loadJson(relPath);
    if (!data.is_object())
    {
        std::cerr << "[ERROR] Invalid release JSON: " << relPath << std::endl;
        return false;
    }

    json web = data.contains("Web") && data["Web"].is_object() ? data["Web"] : json::object();
    json android = data.contains("Android") && data["Android"].is_object() ? data["Android"] : json::object();

    std::string contentRootRel = "releases/" + relPath.stem().string();
    if (isStringField(web, "CONTENT_ROOT"))
    {
        contentRootRel = web["CONTENT_ROOT"].get<std::string>();
    }
    else if (isStringField(android, "CONTENT_ROOT"))
    {
        contentRootRel = android["CONTENT_ROOT"].get<std::string>();
    }

    fs::path contentRoot = bugameRoot / contentRootRel;
    if (!contentRoot.is_absolute())
    {
        contentRoot = fs::absolute(contentRoot);
    }

    if (!fs::exists(contentRoot))
    {
        std::cerr << "[ERROR] CONTENT_ROOT not found: " << contentRoot << std::endl;
        return false;
    }

    outInfo.name = relPath.stem().string();
    outInfo.jsonPath = relPath;
    outInfo.contentRoot = contentRoot;
    outInfo.raw = data;
    return true;
}

fs::path resolveMainBytecodeSource(const fs::path &contentRoot)
{
    fs::path fromAssets = contentRoot / "assets" / "main.buc";
    if (fs::exists(fromAssets))
    {
        return fromAssets;
    }
    fs::path fromScripts = contentRoot / "scripts" / "main.buc";
    if (fs::exists(fromScripts))
    {
        return fromScripts;
    }
    fs::path fromTypoScripts = contentRoot / "scrips" / "main.buc";
    if (fs::exists(fromTypoScripts))
    {
        return fromTypoScripts;
    }
    return fs::path();
}

fs::path resolveMainBytecodeSourceCode(const fs::path &contentRoot)
{
    fs::path fromScripts = contentRoot / "scripts" / "main.bu";
    if (fs::exists(fromScripts))
    {
        return fromScripts;
    }
    fs::path fromTypoScripts = contentRoot / "scrips" / "main.bu";
    if (fs::exists(fromTypoScripts))
    {
        return fromTypoScripts;
    }
    return fs::path();
}

fs::path resolveCompilerTool(const fs::path &repoRoot, const fs::path &bugameRoot)
{
    if (const char *envTool = std::getenv("BUGAME_COMPILER"))
    {
        fs::path p = envTool;
        if (fs::exists(p))
        {
            return fs::absolute(p);
        }
    }

    std::vector<fs::path> candidates = {
        bugameRoot / "main",
        bugameRoot / "main.exe",
        bugameRoot / "build" / "main",
        bugameRoot / "build" / "main.exe",
        bugameRoot / "projects" / "bugame" / "main",
        bugameRoot / "projects" / "bugame" / "main.exe",
        repoRoot / "projects" / "bugame" / "main",
        repoRoot / "projects" / "bugame" / "main.exe"};

    for (const auto &p : candidates)
    {
        if (fs::exists(p))
        {
            return p;
        }
    }

    return fs::path();
}

bool ensureMainBytecode(const fs::path &repoRoot,
                        const fs::path &bugameRoot,
                        const ReleaseInfo &release,
                        const crosside::Context &ctx,
                        bool forceCompile)
{
    const fs::path existing = resolveMainBytecodeSource(release.contentRoot);
    if (!forceCompile && !existing.empty())
    {
        return true;
    }

    const fs::path srcMain = resolveMainBytecodeSourceCode(release.contentRoot);
    if (srcMain.empty())
    {
        if (!existing.empty())
        {
            return true;
        }
        std::cerr << "[ERROR] Missing bytecode and source script for release '" << release.name << "'." << std::endl;
        std::cerr << "Expected one of:" << std::endl;
        std::cerr << "  - " << (release.contentRoot / "assets" / "main.buc") << std::endl;
        std::cerr << "  - " << (release.contentRoot / "scripts" / "main.buc") << std::endl;
        std::cerr << "  - " << (release.contentRoot / "scripts" / "main.bu") << std::endl;
        return false;
    }

    const fs::path compilerTool = resolveCompilerTool(repoRoot, bugameRoot);
    if (compilerTool.empty())
    {
        std::cerr << "[ERROR] Could not find bugame compiler executable." << std::endl;
        std::cerr << "Looked in common paths under " << bugameRoot << " and " << repoRoot << std::endl;
        std::cerr << "Tip: set BUGAME_COMPILER=/full/path/to/main" << std::endl;
        return false;
    }

    fs::path outBytecode = release.contentRoot / "assets" / "main.buc";
    std::error_code ec;
    fs::create_directories(outBytecode.parent_path(), ec);
    if (ec)
    {
        std::cerr << "[ERROR] Failed creating bytecode output folder: " << outBytecode.parent_path()
                  << " (" << ec.message() << ")" << std::endl;
        return false;
    }

    std::cout << "[pack] compile : " << srcMain << " -> " << outBytecode << std::endl;
    if (!check(crosside::io::runCommand(
            compilerTool.string(),
            {"--compile-bc", srcMain.string(), outBytecode.string()},
            compilerTool.parent_path(),
            ctx)))
    {
        return false;
    }

    if (!fs::exists(outBytecode))
    {
        std::cerr << "[ERROR] Bytecode compilation finished but output was not created: " << outBytecode << std::endl;
        return false;
    }
    return true;
}

bool copyReleaseContentForRuntime(const ReleaseInfo &release, const fs::path &dstRoot)
{
    for (const auto &folder : kContentDirs)
    {
        fs::path src = release.contentRoot / folder;
        if (fs::exists(src))
        {
            if (!copyTreeSafe(src, dstRoot / folder))
            {
                return false;
            }
        }
    }

    fs::path scriptsRoot = release.contentRoot / "scripts";
    if (fs::exists(scriptsRoot))
    {
        for (const auto &entry : fs::recursive_directory_iterator(scriptsRoot))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }
            if (!hasBytecodeExt(entry.path()))
            {
                continue;
            }
            fs::path rel = fs::relative(entry.path(), scriptsRoot);
            fs::path out = dstRoot / "assets" / rel;
            if (!copyFileSafe(entry.path(), out))
            {
                return false;
            }
        }
    }

    fs::path mainBytecode = resolveMainBytecodeSource(release.contentRoot);
    if (mainBytecode.empty())
    {
        std::cerr << "[ERROR] Missing bytecode. Expected one of:" << std::endl;
        std::cerr << "  - " << (release.contentRoot / "assets" / "main.buc") << std::endl;
        std::cerr << "  - " << (release.contentRoot / "scripts" / "main.buc") << std::endl;
        return false;
    }

    if (!copyFileSafe(mainBytecode, dstRoot / "assets" / "main.buc"))
    {
        return false;
    }
    return true;
}

struct AndroidTools
{
    fs::path aapt;
    fs::path aapt2;
    fs::path zipalign;
    fs::path apksigner;
    fs::path jarsigner;
    fs::path java;
    fs::path bundletoolJar;
    fs::path platformJar;
};

bool resolveAndroidTools(const fs::path &repoRoot, AndroidTools &outTools)
{
    json config = loadJson(repoRoot / "config.json");
    json tc = config.contains("Configuration") && config["Configuration"].contains("Toolchain")
                  ? config["Configuration"]["Toolchain"]
                  : json::object();

    std::string sdkEnv = std::getenv("ANDROID_SDK_ROOT") ? std::getenv("ANDROID_SDK_ROOT") : "";
    if (sdkEnv.empty())
    {
        sdkEnv = std::getenv("ANDROID_HOME") ? std::getenv("ANDROID_HOME") : "";
    }

    fs::path sdkRoot = sdkEnv.empty() ? fs::path(tc.value("AndroidSdk", "")) : fs::path(sdkEnv);
    if (!fs::exists(sdkRoot))
    {
        std::cerr << "[ERROR] Android SDK not found. Set ANDROID_SDK_ROOT/ANDROID_HOME." << std::endl;
        return false;
    }

    fs::path buildToolsRoot = sdkRoot / "build-tools";
    if (!fs::exists(buildToolsRoot))
    {
        std::cerr << "[ERROR] build-tools folder missing: " << buildToolsRoot << std::endl;
        return false;
    }

    fs::path buildToolsPath;
    if (isStringField(tc, "BuildTools"))
    {
        buildToolsPath = buildToolsRoot / tc["BuildTools"].get<std::string>();
    }
    else
    {
        std::vector<fs::path> versions;
        for (const auto &entry : fs::directory_iterator(buildToolsRoot))
        {
            if (entry.is_directory())
            {
                versions.push_back(entry.path());
            }
        }
        std::sort(versions.begin(), versions.end());
        if (!versions.empty())
        {
            buildToolsPath = versions.back();
        }
    }

    outTools.aapt = findTool("aapt", {buildToolsPath});
    outTools.aapt2 = findTool("aapt2", {buildToolsPath});
    outTools.zipalign = findTool("zipalign", {buildToolsPath});
    outTools.apksigner = findTool("apksigner", {buildToolsPath});

    if (outTools.aapt.empty() || outTools.aapt2.empty() || outTools.zipalign.empty() || outTools.apksigner.empty())
    {
        std::cerr << "[ERROR] Missing Android tools in: " << buildToolsPath << std::endl;
        return false;
    }

    std::string platformVer = tc.value("Platform", "android-35");
    outTools.platformJar = sdkRoot / "platforms" / platformVer / "android.jar";
    if (!fs::exists(outTools.platformJar))
    {
        fs::path platforms = sdkRoot / "platforms";
        std::vector<fs::path> platformCandidates;
        if (fs::exists(platforms))
        {
            for (const auto &entry : fs::directory_iterator(platforms))
            {
                fs::path jar = entry.path() / "android.jar";
                if (entry.is_directory() && fs::exists(jar))
                {
                    platformCandidates.push_back(jar);
                }
            }
        }
        std::sort(platformCandidates.begin(), platformCandidates.end());
        if (!platformCandidates.empty())
        {
            outTools.platformJar = platformCandidates.back();
        }
    }

    if (!fs::exists(outTools.platformJar))
    {
        std::cerr << "[ERROR] android.jar not found." << std::endl;
        return false;
    }

    fs::path javaHome = std::getenv("JAVA_HOME") ? fs::path(std::getenv("JAVA_HOME")) : fs::path();
    outTools.java = findTool("java", {javaHome / "bin"});
    outTools.jarsigner = findTool("jarsigner", {javaHome / "bin"});
    if (outTools.java.empty())
    {
        outTools.java = fs::path("java");
    }
    if (outTools.jarsigner.empty())
    {
        outTools.jarsigner = fs::path("jarsigner");
    }

    if (const char *bundleEnv = std::getenv("BUNDLETOOL_JAR"))
    {
        fs::path p = bundleEnv;
        if (fs::exists(p))
        {
            outTools.bundletoolJar = fs::absolute(p);
        }
    }
    if (outTools.bundletoolJar.empty())
    {
        const fs::path p = repoRoot / "tools" / "android" / "bundletool.jar";
        if (fs::exists(p))
        {
            outTools.bundletoolJar = p;
        }
    }
    if (outTools.bundletoolJar.empty())
    {
        const fs::path p = sdkRoot / "cmdline-tools" / "latest" / "lib" / "bundletool.jar";
        if (fs::exists(p))
        {
            outTools.bundletoolJar = p;
        }
    }
    if (outTools.bundletoolJar.empty())
    {
        fs::path cmdlineRoot = sdkRoot / "cmdline-tools";
        if (fs::exists(cmdlineRoot))
        {
            for (const auto &entry : fs::directory_iterator(cmdlineRoot))
            {
                if (!entry.is_directory()) continue;
                fs::path p = entry.path() / "lib" / "bundletool.jar";
                if (fs::exists(p))
                {
                    outTools.bundletoolJar = p;
                    break;
                }
            }
        }
    }

    return true;
}

struct RunnerAndroidInputs
{
    fs::path resRoot;
    fs::path dexFile;
    fs::path keyFile;
    std::vector<std::pair<std::string, fs::path>> libs;
};

struct AndroidSigningConfig
{
    fs::path keystore;
    std::string alias;
    std::string ksPassArg;
    std::string ksPassValue;
    bool hasKeyPass = false;
    std::string keyPassArg;
    std::string keyPassValue;
};

fs::path resolveReleaseLocalPath(const ReleaseInfo &release, const fs::path &bugameRoot, const std::string &pathValue)
{
    fs::path p = pathValue;
    if (p.is_absolute())
    {
        return p;
    }

    const std::vector<fs::path> candidates = {
        release.contentRoot / p,
        release.jsonPath.parent_path() / p,
        bugameRoot / p};

    for (const auto &c : candidates)
    {
        if (fs::exists(c))
        {
            return fs::absolute(c);
        }
    }

    // Fallback for clear error message later.
    return fs::absolute(release.jsonPath.parent_path() / p);
}

bool resolveAndroidSigning(const ReleaseInfo &release,
                           const fs::path &bugameRoot,
                           const RunnerAndroidInputs &runnerInputs,
                           AndroidSigningConfig &outSigning)
{
    json android = release.raw.contains("Android") && release.raw["Android"].is_object()
                       ? release.raw["Android"]
                       : json::object();
    json signing = android.contains("SIGNING") && android["SIGNING"].is_object()
                       ? android["SIGNING"]
                       : json::object();

    outSigning.keystore = runnerInputs.keyFile;
    outSigning.alias = "djokersoft";
    outSigning.ksPassArg = "pass:14781478";
    outSigning.ksPassValue = "14781478";

    if (isStringField(android, "KEYSTORE"))
    {
        outSigning.keystore = resolveReleaseLocalPath(release, bugameRoot, android["KEYSTORE"].get<std::string>());
    }
    else if (isStringField(signing, "KEYSTORE"))
    {
        outSigning.keystore = resolveReleaseLocalPath(release, bugameRoot, signing["KEYSTORE"].get<std::string>());
    }

    if (!fs::exists(outSigning.keystore))
    {
        std::cerr << "[ERROR] Android keystore not found: " << outSigning.keystore << std::endl;
        return false;
    }

    if (isStringField(android, "KEY_ALIAS"))
    {
        outSigning.alias = android["KEY_ALIAS"].get<std::string>();
    }
    else if (isStringField(signing, "KEY_ALIAS"))
    {
        outSigning.alias = signing["KEY_ALIAS"].get<std::string>();
    }

    if (isStringField(android, "KEYSTORE_PASS_ENV"))
    {
        const std::string envName = android["KEYSTORE_PASS_ENV"].get<std::string>();
        const char *envValue = std::getenv(envName.c_str());
        if (!envValue || !envValue[0])
        {
            std::cerr << "[ERROR] Android KEYSTORE_PASS_ENV is set but env var is missing: " << envName << std::endl;
            return false;
        }
        outSigning.ksPassArg = "env:" + envName;
        outSigning.ksPassValue = envValue;
    }
    else if (isStringField(signing, "KEYSTORE_PASS_ENV"))
    {
        const std::string envName = signing["KEYSTORE_PASS_ENV"].get<std::string>();
        const char *envValue = std::getenv(envName.c_str());
        if (!envValue || !envValue[0])
        {
            std::cerr << "[ERROR] Android SIGNING.KEYSTORE_PASS_ENV is set but env var is missing: " << envName << std::endl;
            return false;
        }
        outSigning.ksPassArg = "env:" + envName;
        outSigning.ksPassValue = envValue;
    }
    else if (isStringField(android, "KEYSTORE_PASS"))
    {
        outSigning.ksPassValue = android["KEYSTORE_PASS"].get<std::string>();
        outSigning.ksPassArg = "pass:" + outSigning.ksPassValue;
    }
    else if (isStringField(signing, "KEYSTORE_PASS"))
    {
        outSigning.ksPassValue = signing["KEYSTORE_PASS"].get<std::string>();
        outSigning.ksPassArg = "pass:" + outSigning.ksPassValue;
    }

    if (isStringField(android, "KEY_PASS_ENV"))
    {
        const std::string envName = android["KEY_PASS_ENV"].get<std::string>();
        const char *envValue = std::getenv(envName.c_str());
        if (!envValue || !envValue[0])
        {
            std::cerr << "[ERROR] Android KEY_PASS_ENV is set but env var is missing: " << envName << std::endl;
            return false;
        }
        outSigning.hasKeyPass = true;
        outSigning.keyPassArg = "env:" + envName;
        outSigning.keyPassValue = envValue;
    }
    else if (isStringField(signing, "KEY_PASS_ENV"))
    {
        const std::string envName = signing["KEY_PASS_ENV"].get<std::string>();
        const char *envValue = std::getenv(envName.c_str());
        if (!envValue || !envValue[0])
        {
            std::cerr << "[ERROR] Android SIGNING.KEY_PASS_ENV is set but env var is missing: " << envName << std::endl;
            return false;
        }
        outSigning.hasKeyPass = true;
        outSigning.keyPassArg = "env:" + envName;
        outSigning.keyPassValue = envValue;
    }
    else if (isStringField(android, "KEY_PASS"))
    {
        outSigning.hasKeyPass = true;
        outSigning.keyPassValue = android["KEY_PASS"].get<std::string>();
        outSigning.keyPassArg = "pass:" + outSigning.keyPassValue;
    }
    else if (isStringField(signing, "KEY_PASS"))
    {
        outSigning.hasKeyPass = true;
        outSigning.keyPassValue = signing["KEY_PASS"].get<std::string>();
        outSigning.keyPassArg = "pass:" + outSigning.keyPassValue;
    }

    return true;
}

bool moveOrCopyFile(const fs::path &src, const fs::path &dst)
{
    std::error_code ec;
    fs::create_directories(dst.parent_path(), ec);
    if (ec)
    {
        std::cerr << "[ERROR] Failed creating folder for move: " << dst.parent_path() << " (" << ec.message() << ")" << std::endl;
        return false;
    }

    fs::rename(src, dst, ec);
    if (!ec)
    {
        return true;
    }

    if (!copyFileSafe(src, dst))
    {
        return false;
    }
    fs::remove(src, ec);
    return true;
}

bool resolveRunnerAndroidInputs(const fs::path &runnerRoot, RunnerAndroidInputs &outInputs)
{
    outInputs.resRoot = runnerRoot / "Android" / "runner" / "res";
    outInputs.dexFile = runnerRoot / "Android" / "runner" / "dex" / "classes.dex";
    outInputs.keyFile = runnerRoot / "Android" / "runner" / "runner.key";

    if (!fs::exists(outInputs.resRoot))
    {
        std::cerr << "[ERROR] Runner Android resources not found: " << outInputs.resRoot << std::endl;
        return false;
    }
    if (!fs::exists(outInputs.keyFile))
    {
        std::cerr << "[ERROR] Runner keystore not found: " << outInputs.keyFile << std::endl;
        return false;
    }

    const std::vector<std::string> abis = {"armeabi-v7a", "arm64-v8a", "x86", "x86_64"};
    for (const auto &abi : abis)
    {
        fs::path lib = runnerRoot / "Android" / abi / "librunner.so";
        if (fs::exists(lib))
        {
            outInputs.libs.push_back({abi, lib});
        }
    }

    if (outInputs.libs.empty())
    {
        std::cerr << "[ERROR] Runner libs not found in " << (runnerRoot / "Android") << std::endl;
        return false;
    }

    return true;
}

bool overlayReleaseAndroidRes(const ReleaseInfo &release, const fs::path &bugameRoot, const fs::path &resRoot)
{
    json android = release.raw.contains("Android") && release.raw["Android"].is_object()
                       ? release.raw["Android"]
                       : json::object();

    std::vector<fs::path> candidates;

    // Automatic conventional folders inside release CONTENT_ROOT.
    candidates.push_back(release.contentRoot / "res");
    candidates.push_back(release.contentRoot / "resources");

    // Optional explicit roots in release json.
    if (isStringField(android, "RES"))
    {
        fs::path p = android["RES"].get<std::string>();
        if (!p.is_absolute())
        {
            p = bugameRoot / p;
        }
        candidates.push_back(fs::absolute(p));
    }
    if (isStringField(android, "RES_ROOT"))
    {
        fs::path p = android["RES_ROOT"].get<std::string>();
        if (!p.is_absolute())
        {
            p = bugameRoot / p;
        }
        candidates.push_back(fs::absolute(p));
    }
    if (isStringField(android, "RESOURCES"))
    {
        fs::path p = android["RESOURCES"].get<std::string>();
        if (!p.is_absolute())
        {
            p = bugameRoot / p;
        }
        candidates.push_back(fs::absolute(p));
    }

    // Deduplicate and overlay.
    std::vector<std::string> seen;
    for (const auto &src : candidates)
    {
        const std::string key = src.lexically_normal().string();
        if (std::find(seen.begin(), seen.end(), key) != seen.end())
        {
            continue;
        }
        seen.push_back(key);

        if (!fs::exists(src) || !fs::is_directory(src))
        {
            continue;
        }
        if (!looksLikeAndroidResRoot(src))
        {
            continue;
        }

        std::cout << "[COPY] Android res overlay: " << src << " -> " << resRoot << std::endl;
        if (!copyTreeSafe(src, resRoot))
        {
            return false;
        }
    }
    return true;
}

std::vector<std::pair<fs::path, std::string>> collectReleaseFilesForApk(const ReleaseInfo &release)
{
    std::map<std::string, fs::path> mapped;
    const std::map<std::string, std::string> mountMap = {
        {"assets", "assets/assets"},
        {"resources", "assets/resources"},
        {"res", "assets/res"},
        {"data", "assets/data"},
        {"media", "assets/media"}};

    for (const auto &folder : kContentDirs)
    {
        fs::path srcRoot = release.contentRoot / folder;
        if (!fs::exists(srcRoot))
        {
            continue;
        }
        const std::string mountRoot = mountMap.at(folder);
        for (const auto &entry : fs::recursive_directory_iterator(srcRoot))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }
            fs::path relPath = fs::relative(entry.path(), srcRoot);
            const std::string rel = relPath.generic_string();
            std::string apkPath;

            // Runtime bytecode must be directly in assets/main.buc.
            if (folder == "assets" && hasBytecodeExt(entry.path()))
            {
                apkPath = "assets/" + rel;
            }
            else
            {
                apkPath = mountRoot + "/" + rel;
            }
            mapped[apkPath] = entry.path();
        }
    }

    fs::path scriptsRoot = release.contentRoot / "scripts";
    if (fs::exists(scriptsRoot))
    {
        for (const auto &entry : fs::recursive_directory_iterator(scriptsRoot))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }
            if (!hasBytecodeExt(entry.path()))
            {
                continue;
            }
            fs::path relPath = fs::relative(entry.path(), scriptsRoot);
            mapped["assets/" + relPath.generic_string()] = entry.path();
        }
    }

    fs::path mainBytecode = resolveMainBytecodeSource(release.contentRoot);
    if (!mainBytecode.empty())
    {
        mapped["assets/main.buc"] = mainBytecode;
    }

    std::vector<std::pair<fs::path, std::string>> out;
    for (const auto &it : mapped)
    {
        out.push_back({it.second, it.first});
    }
    return out;
}

std::string buildDefaultManifest(const std::string &packageName,
                                 const std::string &label,
                                 const std::string &activity,
                                 const std::string &libName)
{
    std::stringstream ss;
    ss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
       << "<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\"\n"
       << "          package=\"" << packageName << "\"\n"
       << "          android:versionCode=\"1\"\n"
       << "          android:versionName=\"1.0\">\n"
       << "    <uses-sdk android:minSdkVersion=\"24\" android:targetSdkVersion=\"35\" />\n"
       << "    <application android:label=\"" << label << "\" android:icon=\"@mipmap/ic_launcher\" android:hasCode=\"false\">\n"
       << "        <activity android:name=\"" << activity << "\"\n"
       << "                  android:label=\"" << label << "\"\n"
       << "                  android:configChanges=\"orientation|keyboardHidden|screenSize\"\n"
       << "                  android:screenOrientation=\"landscape\"\n"
       << "                  android:launchMode=\"singleTask\"\n"
       << "                  android:clearTaskOnLaunch=\"true\"\n"
       << "                  android:exported=\"true\">\n"
       << "            <meta-data android:name=\"android.app.lib_name\" android:value=\"" << libName << "\" />\n"
       << "            <intent-filter>\n"
       << "                <action android:name=\"android.intent.action.MAIN\" />\n"
       << "                <category android:name=\"android.intent.category.LAUNCHER\" />\n"
       << "            </intent-filter>\n"
       << "        </activity>\n"
       << "    </application>\n"
       << "</manifest>\n";
    return ss.str();
}

bool writeManifest(const ReleaseInfo &release,
                   const fs::path &bugameRoot,
                   const fs::path &manifestDst,
                   std::string &packageName,
                   std::string &label,
                   std::string &activity)
{
    json android = release.raw.contains("Android") && release.raw["Android"].is_object()
                       ? release.raw["Android"]
                       : json::object();

    packageName = isStringField(android, "PACKAGE") ? android["PACKAGE"].get<std::string>()
                                                     : ("com.djokersoft." + release.name);
    label = isStringField(android, "LABEL") ? android["LABEL"].get<std::string>()
                                             : release.name;
    activity = isStringField(android, "ACTIVITY") ? android["ACTIVITY"].get<std::string>()
                                                   : "android.app.NativeActivity";

    const std::string libName = "runner";

    fs::path manifestTemplate;
    if (isStringField(android, "MANIFEST_TEMPLATE"))
    {
        manifestTemplate = android["MANIFEST_TEMPLATE"].get<std::string>();
        if (!manifestTemplate.is_absolute())
        {
            manifestTemplate = bugameRoot / manifestTemplate;
        }
    }
    else
    {
        fs::path releaseManifest = release.contentRoot / "AndroidManifest.xml";
        if (fs::exists(releaseManifest))
        {
            manifestTemplate = releaseManifest;
        }
    }

    std::string text;
    if (!manifestTemplate.empty() && fs::exists(manifestTemplate))
    {
        text = readTextFile(manifestTemplate);
        if (text.empty())
        {
            std::cerr << "[ERROR] Failed reading manifest template: " << manifestTemplate << std::endl;
            return false;
        }

        replaceAll(text, "@apppkg@", packageName);
        replaceAll(text, "@applbl@", label);
        replaceAll(text, "@appact@", activity);
        replaceAll(text, "@appLIBNAME@", libName);
        replaceAll(text, "@appname@", release.name);
    }
    else
    {
        text = buildDefaultManifest(packageName, label, activity, libName);
    }

    return writeTextFile(manifestDst, text);
}

bool packageWeb(const fs::path &runnerRoot,
                const fs::path &outRoot,
                const ReleaseInfo &release)
{
    fs::path outDir = outRoot / release.name / "web";
    if (!cleanDir(outDir))
    {
        return false;
    }

    const fs::path webRoot = runnerRoot / "Web";
    auto hasRuntimeSet = [&](const std::string &base) -> bool
    {
        return fs::exists(webRoot / (base + ".js")) &&
               fs::exists(webRoot / (base + ".wasm")) &&
               fs::exists(webRoot / (base + ".data")) &&
               fs::exists(webRoot / (base + ".html"));
    };

    // Prefer release-specific runtime bundle (<release>.js/.wasm/.data/.html).
    // Fallback to runner.* for backward compatibility.
    std::string runtimeBase = release.name;
    if (!hasRuntimeSet(runtimeBase))
    {
        runtimeBase = "runner";
    }
    if (!hasRuntimeSet(runtimeBase))
    {
        std::cerr << "[ERROR] Missing web runtime files in: " << webRoot << std::endl;
        std::cerr << "Expected either:" << std::endl;
        std::cerr << "  - " << (webRoot / (release.name + ".js")) << std::endl;
        std::cerr << "  - " << (webRoot / (release.name + ".wasm")) << std::endl;
        std::cerr << "  - " << (webRoot / (release.name + ".data")) << std::endl;
        std::cerr << "  - " << (webRoot / (release.name + ".html")) << std::endl;
        std::cerr << "or:" << std::endl;
        std::cerr << "  - " << (webRoot / "runner.js") << std::endl;
        std::cerr << "  - " << (webRoot / "runner.wasm") << std::endl;
        std::cerr << "  - " << (webRoot / "runner.data") << std::endl;
        std::cerr << "  - " << (webRoot / "runner.html") << std::endl;
        return false;
    }

    if (runtimeBase != "runner")
    {
        std::cout << "[pack] web runtime: " << runtimeBase << ".*" << std::endl;
    }

    const std::vector<std::string> runtimeFiles = {".js", ".wasm", ".data"};
    for (const auto &ext : runtimeFiles)
    {
        const std::string file = runtimeBase + ext;
        if (!copyFileSafe(webRoot / file, outDir / file))
        {
            return false;
        }
    }
    if (!copyFileSafe(webRoot / (runtimeBase + ".html"), outDir / "index.html"))
    {
        return false;
    }

    if (!copyReleaseContentForRuntime(release, outDir))
    {
        return false;
    }
    if (!copyFileSafe(release.jsonPath, outDir / "release.json"))
    {
        return false;
    }

    std::cout << "[SUCCESS] Web package: " << outDir << std::endl;
    return true;
}

bool packageAndroid(const fs::path &repoRoot,
                    const fs::path &bugameRoot,
                    const fs::path &runnerRoot,
                    const fs::path &outRoot,
                    const crosside::Context &ctx,
                    const ReleaseInfo &release,
                    fs::path &outSignedApk,
                    std::string &outPackageName,
                    std::string &outActivityName)
{
    AndroidTools tools;
    if (!resolveAndroidTools(repoRoot, tools))
    {
        return false;
    }

    RunnerAndroidInputs runnerInputs;
    if (!resolveRunnerAndroidInputs(runnerRoot, runnerInputs))
    {
        return false;
    }
    AndroidSigningConfig signing;
    if (!resolveAndroidSigning(release, bugameRoot, runnerInputs, signing))
    {
        return false;
    }
    std::cout << "[pack] signing  : keystore=" << signing.keystore
              << " alias=" << signing.alias
              << " ks-pass=" << (startsWith(signing.ksPassArg, "env:") ? "env" : "inline")
              << " key-pass=";
    if (!signing.hasKeyPass)
    {
        std::cout << "same/default";
    }
    else
    {
        std::cout << (startsWith(signing.keyPassArg, "env:") ? "env" : "inline");
    }
    std::cout << std::endl;

    fs::path outDir = outRoot / release.name / "android";
    fs::path buildRoot = outDir / "_build";
    fs::path resRoot = buildRoot / "res";
    fs::path stageRoot = buildRoot / "stage";
    fs::path tmpRoot = buildRoot / "tmp";

    if (!cleanDir(outDir))
    {
        return false;
    }
    fs::create_directories(resRoot);
    fs::create_directories(stageRoot);
    fs::create_directories(tmpRoot);

    if (!copyTreeSafe(runnerInputs.resRoot, resRoot))
    {
        return false;
    }
    if (!overlayReleaseAndroidRes(release, bugameRoot, resRoot))
    {
        return false;
    }

    fs::path manifest = buildRoot / "AndroidManifest.xml";
    std::string packageName;
    std::string label;
    std::string activity;
    if (!writeManifest(release, bugameRoot, manifest, packageName, label, activity))
    {
        std::cerr << "[ERROR] Failed creating AndroidManifest.xml" << std::endl;
        return false;
    }

    fs::path unsignedApk = tmpRoot / (release.name + ".unsigned.apk");
    fs::path alignedApk = tmpRoot / (release.name + ".aligned.apk");
    fs::path signedApk = outDir / (release.name + ".signed.apk");

    if (!check(crosside::io::runCommand(
            tools.aapt.string(),
            {"package", "-f", "-m", "-0", "arsc",
             "-F", unsignedApk.string(),
             "-M", manifest.string(),
             "-S", resRoot.string(),
             "-I", tools.platformJar.string()},
            repoRoot,
            ctx)))
    {
        return false;
    }

    std::vector<std::string> stagedFiles;

    if (fs::exists(runnerInputs.dexFile))
    {
        if (!copyFileSafe(runnerInputs.dexFile, stageRoot / "classes.dex"))
        {
            return false;
        }
        stagedFiles.push_back("classes.dex");
    }

    for (const auto &lib : runnerInputs.libs)
    {
        const std::string rel = "lib/" + lib.first + "/librunner.so";
        if (!copyFileSafe(lib.second, stageRoot / rel))
        {
            return false;
        }
        stagedFiles.push_back(rel);
    }

    for (const auto &entry : collectReleaseFilesForApk(release))
    {
        if (!copyFileSafe(entry.first, stageRoot / entry.second))
        {
            return false;
        }
        stagedFiles.push_back(entry.second);
    }

    for (const auto &rel : stagedFiles)
    {
        if (!check(crosside::io::runCommand(
                tools.aapt.string(),
                {"add", unsignedApk.string(), rel},
                stageRoot,
                ctx)))
        {
            return false;
        }
    }

    if (!check(crosside::io::runCommand(
            tools.zipalign.string(),
            {"-f", "-p", "4", unsignedApk.string(), alignedApk.string()},
            repoRoot,
            ctx)))
    {
        return false;
    }

    if (!check(crosside::io::runCommand(
            tools.apksigner.string(),
            [&]()
            {
                std::vector<std::string> args = {
                    "sign",
                    "--ks", signing.keystore.string(),
                    "--ks-key-alias", signing.alias,
                    "--ks-pass", signing.ksPassArg};
                if (signing.hasKeyPass)
                {
                    args.push_back("--key-pass");
                    args.push_back(signing.keyPassArg);
                }
                args.push_back("--in");
                args.push_back(alignedApk.string());
                args.push_back("--out");
                args.push_back(signedApk.string());
                return args;
            }(),
            repoRoot,
            ctx)))
    {
        return false;
    }

    if (!copyFileSafe(release.jsonPath, outDir / "release.json"))
    {
        return false;
    }

    outSignedApk = signedApk;
    outPackageName = packageName;
    outActivityName = activity;

    std::cout << "[SUCCESS] Android package: " << signedApk << std::endl;
    return true;
}

bool packageAndroidAab(const fs::path &repoRoot,
                       const fs::path &bugameRoot,
                       const fs::path &runnerRoot,
                       const fs::path &outRoot,
                       const crosside::Context &ctx,
                       const ReleaseInfo &release,
                       fs::path &outSignedAab)
{
    AndroidTools tools;
    if (!resolveAndroidTools(repoRoot, tools))
    {
        return false;
    }
    if (tools.bundletoolJar.empty() || !fs::exists(tools.bundletoolJar))
    {
        std::cerr << "[ERROR] bundletool.jar not found." << std::endl;
        std::cerr << "Place it at " << (repoRoot / "tools" / "android" / "bundletool.jar")
                  << " or set BUNDLETOOL_JAR=/full/path/bundletool.jar" << std::endl;
        return false;
    }

    RunnerAndroidInputs runnerInputs;
    if (!resolveRunnerAndroidInputs(runnerRoot, runnerInputs))
    {
        return false;
    }
    AndroidSigningConfig signing;
    if (!resolveAndroidSigning(release, bugameRoot, runnerInputs, signing))
    {
        return false;
    }

    fs::path outDir = outRoot / release.name / "android";
    fs::path buildRoot = outDir / "_build";
    fs::path resRoot = buildRoot / "res";
    fs::path stageRoot = buildRoot / "stage";
    fs::path tmpRoot = buildRoot / "tmp";
    fs::path moduleExtract = buildRoot / "module";

    if (!cleanDir(outDir))
    {
        return false;
    }
    fs::create_directories(resRoot);
    fs::create_directories(stageRoot);
    fs::create_directories(tmpRoot);
    fs::create_directories(moduleExtract);

    if (!copyTreeSafe(runnerInputs.resRoot, resRoot))
    {
        return false;
    }
    if (!overlayReleaseAndroidRes(release, bugameRoot, resRoot))
    {
        return false;
    }

    fs::path manifest = buildRoot / "AndroidManifest.xml";
    std::string packageName;
    std::string label;
    std::string activity;
    if (!writeManifest(release, bugameRoot, manifest, packageName, label, activity))
    {
        std::cerr << "[ERROR] Failed creating AndroidManifest.xml" << std::endl;
        return false;
    }

    fs::path unsignedApk = tmpRoot / (release.name + ".unsigned.apk");
    fs::path protoApk = tmpRoot / (release.name + ".proto.apk");
    fs::path moduleZip = tmpRoot / "base.module.zip";
    fs::path unsignedAab = tmpRoot / (release.name + ".unsigned.aab");
    fs::path signedAab = outDir / (release.name + ".signed.aab");

    std::cout << "[pack] signing  : keystore=" << signing.keystore
              << " alias=" << signing.alias
              << " ks-pass=" << (startsWith(signing.ksPassArg, "env:") ? "env" : "inline")
              << " key-pass=";
    if (!signing.hasKeyPass)
    {
        std::cout << "same/default";
    }
    else
    {
        std::cout << (startsWith(signing.keyPassArg, "env:") ? "env" : "inline");
    }
    std::cout << std::endl;

    if (!check(crosside::io::runCommand(
            tools.aapt.string(),
            {"package", "-f", "-m", "-0", "arsc",
             "-F", unsignedApk.string(),
             "-M", manifest.string(),
             "-S", resRoot.string(),
             "-I", tools.platformJar.string()},
            repoRoot,
            ctx)))
    {
        return false;
    }

    std::vector<std::string> stagedFiles;
    if (fs::exists(runnerInputs.dexFile))
    {
        if (!copyFileSafe(runnerInputs.dexFile, stageRoot / "classes.dex"))
        {
            return false;
        }
        stagedFiles.push_back("classes.dex");
    }

    for (const auto &lib : runnerInputs.libs)
    {
        const std::string rel = "lib/" + lib.first + "/librunner.so";
        if (!copyFileSafe(lib.second, stageRoot / rel))
        {
            return false;
        }
        stagedFiles.push_back(rel);
    }

    for (const auto &entry : collectReleaseFilesForApk(release))
    {
        if (!copyFileSafe(entry.first, stageRoot / entry.second))
        {
            return false;
        }
        stagedFiles.push_back(entry.second);
    }

    for (const auto &rel : stagedFiles)
    {
        if (!check(crosside::io::runCommand(
                tools.aapt.string(),
                {"add", unsignedApk.string(), rel},
                stageRoot,
                ctx)))
        {
            return false;
        }
    }

    if (!check(crosside::io::runCommand(
            tools.aapt2.string(),
            {"convert", "--output-format", "proto",
             "-o", protoApk.string(),
             unsignedApk.string()},
            repoRoot,
            ctx)))
    {
        return false;
    }

    if (!cleanDir(moduleExtract))
    {
        return false;
    }

    if (!check(crosside::io::runCommand(
            "jar",
            {"xf", protoApk.string()},
            moduleExtract,
            ctx)))
    {
        return false;
    }

    fs::path extractedManifest = moduleExtract / "AndroidManifest.xml";
    if (!fs::exists(extractedManifest))
    {
        std::cerr << "[ERROR] Missing AndroidManifest.xml in proto APK module stage." << std::endl;
        return false;
    }
    if (!moveOrCopyFile(extractedManifest, moduleExtract / "manifest" / "AndroidManifest.xml"))
    {
        return false;
    }

    fs::path extractedDex = moduleExtract / "classes.dex";
    if (fs::exists(extractedDex))
    {
        if (!moveOrCopyFile(extractedDex, moduleExtract / "dex" / "classes.dex"))
        {
            return false;
        }
    }

    if (fs::exists(moduleZip))
    {
        std::error_code ec;
        fs::remove(moduleZip, ec);
    }
    if (!check(crosside::io::runCommand(
            "jar",
            {"--create", "--file", moduleZip.string(), "--no-manifest", "-C", moduleExtract.string(), "."},
            repoRoot,
            ctx)))
    {
        return false;
    }

    if (!check(crosside::io::runCommand(
            tools.java.string(),
            {"-jar", tools.bundletoolJar.string(),
             "build-bundle",
             "--modules=" + moduleZip.string(),
             "--output=" + unsignedAab.string(),
             "--overwrite"},
            repoRoot,
            ctx)))
    {
        return false;
    }

    std::vector<std::string> signArgs = {
        "-keystore", signing.keystore.string(),
        "-storepass", signing.ksPassValue,
        "-signedjar", signedAab.string(),
        unsignedAab.string(),
        signing.alias};
    if (signing.hasKeyPass)
    {
        signArgs.insert(signArgs.begin() + 4, {"-keypass", signing.keyPassValue});
    }

    if (!check(crosside::io::runCommand(
            tools.jarsigner.string(),
            signArgs,
            repoRoot,
            ctx)))
    {
        return false;
    }

    if (!copyFileSafe(release.jsonPath, outDir / "release.json"))
    {
        return false;
    }

    outSignedAab = signedAab;
    std::cout << "[SUCCESS] Android AAB: " << signedAab << std::endl;
    return true;
}

fs::path resolveAdbTool(const fs::path &repoRoot)
{
    if (const char *sdk = std::getenv("ANDROID_SDK_ROOT"))
    {
        fs::path adb = fs::path(sdk) / "platform-tools" / "adb";
        if (fs::exists(adb))
        {
            return adb;
        }
    }
    if (const char *home = std::getenv("ANDROID_HOME"))
    {
        fs::path adb = fs::path(home) / "platform-tools" / "adb";
        if (fs::exists(adb))
        {
            return adb;
        }
    }

    json config = loadJson(repoRoot / "config.json");
    json tc = config.contains("Configuration") && config["Configuration"].contains("Toolchain")
                  ? config["Configuration"]["Toolchain"]
                  : json::object();
    if (isStringField(tc, "AndroidSdk"))
    {
        fs::path adb = fs::path(tc["AndroidSdk"].get<std::string>()) / "platform-tools" / "adb";
        if (fs::exists(adb))
        {
            return adb;
        }
    }

    const fs::path fallback = "/home/djoker/android/android-sdk/platform-tools/adb";
    if (fs::exists(fallback))
    {
        return fallback;
    }

    return fs::path("adb");
}

bool installAndRunAndroid(const crosside::Context &ctx,
                          const fs::path &adbPath,
                          const fs::path &apkPath,
                          const std::string &packageName,
                          const std::string &activityName,
                          const std::string &deviceSerial,
                          bool doInstall,
                          bool doRun)
{
    std::vector<std::string> adbPrefix;
    if (!deviceSerial.empty())
    {
        adbPrefix.push_back("-s");
        adbPrefix.push_back(deviceSerial);
    }

    auto adbArgs = [&](const std::vector<std::string> &tail)
    {
        std::vector<std::string> out = adbPrefix;
        out.insert(out.end(), tail.begin(), tail.end());
        return out;
    };

    if (!check(crosside::io::runCommand(adbPath.string(), adbArgs({"devices"}), fs::current_path(), ctx)))
    {
        return false;
    }

    if (doInstall)
    {
        if (!check(crosside::io::runCommand(adbPath.string(), adbArgs({"install", "-r", apkPath.string()}), fs::current_path(), ctx)))
        {
            return false;
        }
    }

    if (doRun)
    {
        std::string component;
        if (activityName.find('/') != std::string::npos)
        {
            component = activityName;
        }
        else
        {
            component = packageName + "/" + activityName;
        }

        if (!check(crosside::io::runCommand(adbPath.string(), adbArgs({"shell", "am", "start", "-n", component}), fs::current_path(), ctx)))
        {
            return false;
        }
    }

    return true;
}

void printUsage()
{
    std::cout << "Usage: packager <release_name_or_json> <target> [options]\n";
    std::cout << "Targets: web | android | aab | all\n";
    std::cout << "Options:\n";
    std::cout << "  --repo <path>    Repository root (default: inferred)\n";
    std::cout << "  --bugame <path>  Bugame root (default: <repo>/projects/bugame)\n";
    std::cout << "  --runner <path>  Runner root (default: <repo>/projects/runner)\n";
    std::cout << "  --out <path>     Output root (default: <repo>/export)\n";
    std::cout << "  --compile-bc     Force compile scripts/main.bu -> assets/main.buc before packaging\n";
    std::cout << "  --install        Install generated Android APK via adb\n";
    std::cout << "  --run            Launch app after Android package/install\n";
    std::cout << "  --adb <path>     Explicit adb binary path\n";
    std::cout << "  --device <id>    adb device serial (optional)\n";
    std::cout << "Android signing from release JSON (Android.KEYSTORE/KEY_ALIAS/KEYSTORE_PASS/KEY_PASS or Android.SIGNING.*)\n";
    std::cout << "AAB needs bundletool.jar (set BUNDLETOOL_JAR or place at tools/android/bundletool.jar)\n";
}
} // namespace

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printUsage();
        return 1;
    }

    const std::string releaseArg = argv[1];
    const std::string target = argv[2];

    fs::path repoRoot = fs::absolute(fs::path(argv[0]).parent_path().parent_path());
    if (!fs::exists(repoRoot / "config.json"))
    {
        repoRoot = fs::current_path();
    }

    fs::path bugameRoot = repoRoot / "projects" / "bugame";
    fs::path runnerRoot = repoRoot / "projects" / "runner";
    fs::path outRoot = repoRoot / "export";
    bool forceCompileBc = false;
    bool installAfterPack = false;
    bool runAfterPack = false;
    fs::path adbPath;
    std::string deviceSerial;

    for (int i = 3; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--repo" && i + 1 < argc)
        {
            repoRoot = fs::absolute(argv[++i]);
        }
        else if (arg == "--bugame" && i + 1 < argc)
        {
            bugameRoot = fs::absolute(argv[++i]);
        }
        else if (arg == "--runner" && i + 1 < argc)
        {
            runnerRoot = fs::absolute(argv[++i]);
        }
        else if (arg == "--out" && i + 1 < argc)
        {
            outRoot = fs::absolute(argv[++i]);
        }
        else if (arg == "--compile-bc")
        {
            forceCompileBc = true;
        }
        else if (arg == "--install")
        {
            installAfterPack = true;
        }
        else if (arg == "--run")
        {
            runAfterPack = true;
            installAfterPack = true;
        }
        else if (arg == "--adb" && i + 1 < argc)
        {
            adbPath = fs::absolute(argv[++i]);
        }
        else if (arg == "--device" && i + 1 < argc)
        {
            deviceSerial = argv[++i];
        }
        else
        {
            std::cerr << "[ERROR] Unknown/invalid option: " << arg << std::endl;
            printUsage();
            return 1;
        }
    }

    if (!fs::exists(bugameRoot))
    {
        std::cerr << "[ERROR] bugame root not found: " << bugameRoot << std::endl;
        return 1;
    }
    if (!fs::exists(runnerRoot))
    {
        std::cerr << "[ERROR] runner root not found: " << runnerRoot << std::endl;
        return 1;
    }

    ReleaseInfo release;
    if (!resolveReleaseInfo(bugameRoot, releaseArg, release))
    {
        return 1;
    }

    std::cout << "[pack] release : " << release.name << std::endl;
    std::cout << "[pack] json    : " << release.jsonPath << std::endl;
    std::cout << "[pack] content : " << release.contentRoot << std::endl;
    std::cout << "[pack] target  : " << target << std::endl;
    std::cout << "[pack] out     : " << outRoot << std::endl;

    crosside::Context ctx;

    if (!ensureMainBytecode(repoRoot, bugameRoot, release, ctx, forceCompileBc))
    {
        return 1;
    }

    if (target == "web")
    {
        return packageWeb(runnerRoot, outRoot, release) ? 0 : 1;
    }
    if (target == "android")
    {
        fs::path signedApkPath;
        std::string packageName;
        std::string activityName;
        if (!packageAndroid(repoRoot, bugameRoot, runnerRoot, outRoot, ctx, release, signedApkPath, packageName, activityName))
        {
            return 1;
        }

        if (installAfterPack || runAfterPack)
        {
            if (adbPath.empty())
            {
                adbPath = resolveAdbTool(repoRoot);
            }
            std::cout << "[pack] adb      : " << adbPath << std::endl;
            if (!installAndRunAndroid(ctx, adbPath, signedApkPath, packageName, activityName, deviceSerial, installAfterPack, runAfterPack))
            {
                return 1;
            }
        }
        return 0;
    }
    if (target == "aab")
    {
        if (installAfterPack || runAfterPack)
        {
            std::cerr << "[WARN] --install/--run ignored for target 'aab'." << std::endl;
        }
        fs::path signedAabPath;
        if (!packageAndroidAab(repoRoot, bugameRoot, runnerRoot, outRoot, ctx, release, signedAabPath))
        {
            return 1;
        }
        return 0;
    }
    if (target == "all")
    {
        if (!packageWeb(runnerRoot, outRoot, release))
        {
            return 1;
        }

        fs::path signedApkPath;
        std::string packageName;
        std::string activityName;
        if (!packageAndroid(repoRoot, bugameRoot, runnerRoot, outRoot, ctx, release, signedApkPath, packageName, activityName))
        {
            return 1;
        }

        if (installAfterPack || runAfterPack)
        {
            if (adbPath.empty())
            {
                adbPath = resolveAdbTool(repoRoot);
            }
            std::cout << "[pack] adb      : " << adbPath << std::endl;
            if (!installAndRunAndroid(ctx, adbPath, signedApkPath, packageName, activityName, deviceSerial, installAfterPack, runAfterPack))
            {
                return 1;
            }
        }
        return 0;
    }

    std::cerr << "[ERROR] Unknown target: " << target << std::endl;
    printUsage();
    return 1;
}
