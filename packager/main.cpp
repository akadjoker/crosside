#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <sstream>
#include <algorithm>


#include "json.hpp"
#include "process.hpp"
#include "context.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

// =============================================================================
// UTILITÁRIOS
// =============================================================================

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
        return json::object();
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

void mergeJson(json &base, const json &overlay)
{
    for (auto &[key, val] : overlay.items())
    {
        if (val.is_object() && base.contains(key) && base[key].is_object())
        {
            mergeJson(base[key], val);
        }
        else
        {
            base[key] = val;
        }
    }
}

fs::path findTool(const std::string &name, const std::vector<fs::path> &searchPaths)
{
    for (const auto &path : searchPaths)
    {
        if (path.empty())
            continue;
        fs::path tool = path / name;
#ifdef _WIN32
        if (fs::exists(tool.replace_extension(".exe")))
            return tool;
        if (fs::exists(tool.replace_extension(".bat")))
            return tool;
#else
        if (fs::exists(tool))
            return tool;
#endif
    }
    return "";
}

// =============================================================================
// ANDROID PACKAGER
// =============================================================================

class AndroidPackager
{
    fs::path repoRoot;
    fs::path projectDir;
    json projectSpec;
    std::string name;
    std::string releaseName;

    fs::path sdkRoot;
    fs::path aapt, apksigner, zipalign, platformJar;

    fs::path outDir, resDir, assetsDir, libDir, tmpDir;
    const crosside::Context &ctx;

public:
    AndroidPackager(fs::path root, fs::path proj, json releaseConfig, std::string relName, const crosside::Context &context)
        : repoRoot(root), projectDir(proj), releaseName(relName), ctx(context)
    {

        projectSpec = loadJson(projectDir / "main.mk");
        if (!releaseConfig.is_null())
        {
            std::cout << "[INFO] Applying release configuration..." << std::endl;
            mergeJson(projectSpec, releaseConfig);
        }

        name = projectSpec.value("Name", projectDir.filename().string());

        // Toolchain config
        json config = loadJson(repoRoot / "config.json");
        json tc = config["Configuration"]["Toolchain"];

        std::string sdkEnv = std::getenv("ANDROID_SDK_ROOT") ? std::getenv("ANDROID_SDK_ROOT") : "";
        sdkRoot = sdkEnv.empty() ? fs::path(tc.value("AndroidSdk", "")) : fs::path(sdkEnv);

        // Find tools
        fs::path buildTools = sdkRoot / "build-tools";
        fs::path latestBuildTools;
        if (fs::exists(buildTools))
        {
            for (const auto &entry : fs::directory_iterator(buildTools))
            {
                if (entry.is_directory())
                    latestBuildTools = entry.path(); 
            }
        }

        if (tc.contains("BuildTools"))
        {
            latestBuildTools = buildTools / tc["BuildTools"].get<std::string>();
        }

        aapt = findTool("aapt", {latestBuildTools});
        apksigner = findTool("apksigner", {latestBuildTools});
        zipalign = findTool("zipalign", {latestBuildTools});

        // Platform JAR
        std::string platformVer = tc.value("Platform", "android-31");
        platformJar = sdkRoot / "platforms" / platformVer / "android.jar";
        if (!fs::exists(platformJar))
        {
            // Fallback
            fs::path platformsDir = sdkRoot / "platforms";
            if (fs::exists(platformsDir))
            {
                for (const auto &entry : fs::directory_iterator(platformsDir))
                {
                    if (entry.is_directory())
                        platformJar = entry.path() / "android.jar";
                }
            }
        }

        // Output dirs
        std::string outFolder = releaseName.empty() ? "Package" : releaseName;
        outDir = projectDir / "Android" / outFolder;
        resDir = outDir / "res";
        assetsDir = outDir / "assets";
        libDir = outDir / "lib";
        tmpDir = outDir / "tmp";
    }

    void prepareLayout()
    {
        if (fs::exists(outDir))
            fs::remove_all(outDir);
        fs::create_directories(resDir);
        fs::create_directories(assetsDir);
        fs::create_directories(libDir);
        fs::create_directories(tmpDir);

        json androidSpec = projectSpec["Android"];

        // Icon
        if (androidSpec.contains("ICON"))
        {
            fs::path src = projectDir / androidSpec["ICON"].get<std::string>();
            if (fs::exists(src))
            {
                fs::create_directories(resDir / "mipmap-hdpi");
                fs::copy_file(src, resDir / "mipmap-hdpi" / "ic_launcher.png");
            }
        }

        // Assets
        fs::path contentRoot = projectDir;
        if (projectSpec.contains("CONTENT_ROOT"))
            contentRoot = projectDir / projectSpec["CONTENT_ROOT"].get<std::string>();
        else if (androidSpec.contains("CONTENT_ROOT"))
            contentRoot = projectDir / androidSpec["CONTENT_ROOT"].get<std::string>();

        std::vector<std::string> folders = {"scripts", "assets", "resources", "data", "media"};
        for (const auto &folder : folders)
        {
            fs::path src = contentRoot / folder;
            if (fs::exists(src))
            {
                std::cout << "[COPY] " << folder << " -> assets/" << folder << std::endl;
                fs::copy(src, assetsDir / folder, fs::copy_options::recursive);
            }
        }

        // Native Libs
        std::vector<std::string> abis = {"armeabi-v7a", "arm64-v8a", "x86", "x86_64"};
        bool foundLibs = false;
        std::string libName = "lib" + name + ".so";

        for (const auto &abi : abis)
        {
            std::vector<fs::path> candidates = {
                projectDir / "Android" / abi / libName,
                projectDir / "bin" / "Android" / abi / libName,
                projectDir / "libs" / abi / libName};

            for (const auto &src : candidates)
            {
                if (fs::exists(src))
                {
                    fs::path dst = libDir / abi;
                    fs::create_directories(dst);
                    fs::copy_file(src, dst / libName);
                    std::cout << "[LIB] Found " << abi << ": " << src << std::endl;
                    foundLibs = true;

                    // Copy dependencies
                    for (const auto &entry : fs::directory_iterator(src.parent_path()))
                    {
                        if (entry.path().extension() == ".so" && entry.path().filename() != libName)
                        {
                            fs::copy_file(entry.path(), dst / entry.path().filename());
                        }
                    }
                    break;
                }
            }
        }
        if (!foundLibs)
            std::cerr << "[WARNING] No native libraries found!" << std::endl;
    }

    fs::path generateManifest()
    {
        json androidSpec = projectSpec["Android"];
        std::string package = androidSpec.value("PACKAGE", "com.example.game");
        std::string activity = androidSpec.value("ACTIVITY", "android.app.NativeActivity");
        std::string label = androidSpec.value("LABEL", name);

        std::string minSdk = "21";
        std::string targetSdk = "30";
        if (androidSpec.contains("MANIFEST_VARS"))
        {
            minSdk = androidSpec["MANIFEST_VARS"].value("MIN_SDK", "21");
            targetSdk = androidSpec["MANIFEST_VARS"].value("TARGET_SDK", "30");
        }

        std::string iconAttr = "";
        if (fs::exists(resDir / "mipmap-hdpi" / "ic_launcher.png"))
        {
            iconAttr = "android:icon=\"@mipmap/ic_launcher\"";
        }

        std::stringstream ss;
        ss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
           << "<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\"\n"
           << "          package=\"" << package << "\"\n"
           << "          android:versionCode=\"1\"\n"
           << "          android:versionName=\"1.0\">\n"
           << "    <uses-sdk android:minSdkVersion=\"" << minSdk << "\" android:targetSdkVersion=\"" << targetSdk << "\" />\n"
           << "    <uses-feature android:glEsVersion=\"0x00020000\" android:required=\"true\" />\n"
           << "    <application android:label=\"" << label << "\" " << iconAttr << " android:hasCode=\"false\">\n"
           << "        <activity android:name=\"" << activity << "\"\n"
           << "                  android:label=\"" << label << "\"\n"
           << "                  android:configChanges=\"orientation|keyboardHidden|screenSize\"\n"
           << "                  android:screenOrientation=\"landscape\"\n"
           << "                  android:exported=\"true\">\n"
           << "            <meta-data android:name=\"android.app.lib_name\" android:value=\"" << name << "\" />\n"
           << "            <intent-filter>\n"
           << "                <action android:name=\"android.intent.action.MAIN\" />\n"
           << "                <category android:name=\"android.intent.category.LAUNCHER\" />\n"
           << "            </intent-filter>\n"
           << "        </activity>\n"
           << "    </application>\n"
           << "</manifest>";

        fs::path manifestPath = outDir / "AndroidManifest.xml";
        std::ofstream f(manifestPath);
        f << ss.str();
        return manifestPath;
    }

    void package()
    {
        std::cout << "Packaging Android APK for " << name << "..." << std::endl;
        prepareLayout();
        fs::path manifest = generateManifest();

        fs::path unsignedApk = tmpDir / (name + ".unsigned.apk");
        fs::path alignedApk = tmpDir / (name + ".aligned.apk");
        fs::path finalApk = outDir / (name + ".apk");

        // 1. AAPT
        std::vector<std::string> aaptArgs = {
            "package", "-f",
            "-M", manifest.string(),
            "-S", resDir.string(),
            "-A", assetsDir.string(),
            "-I", platformJar.string(),
            "-F", unsignedApk.string()
        };
        
        if (!check(crosside::io::runCommand(aapt.string(), aaptArgs, projectDir, ctx)))
            return;

        // 2. Add Libs
        // Usamos outDir como CWD para que o aapt adicione os ficheiros com caminhos relativos corretos
        for (const auto &entry : fs::recursive_directory_iterator("lib"))
        {
            if (entry.is_regular_file())
            {
                // entry.path() é relativo a outDir porque recursive_directory_iterator começou lá?
                // Não, recursive_directory_iterator usa caminhos completos ou relativos ao inicio.
                // Vamos garantir caminhos relativos para o 'aapt add'.
                fs::path relPath = fs::relative(entry.path(), outDir);
                check(crosside::io::runCommand(aapt.string(), {"add", fs::relative(unsignedApk, outDir).string(), relPath.string()}, outDir, ctx));
            }
        }

        // 3. Zipalign
        fs::path targetApk = unsignedApk;
        if (!zipalign.empty())
        {
            check(crosside::io::runCommand(zipalign.string(), {"-f", "-p", "4", unsignedApk.string(), alignedApk.string()}, projectDir, ctx));
            targetApk = alignedApk;
        }

        // 4. Sign
        fs::path keystore = outDir / "debug.keystore";
        if (!fs::exists(keystore))
        {
            std::string keytool = "keytool"; // Assume in PATH or JAVA_HOME
            if (const char *javaHome = std::getenv("JAVA_HOME"))
            {
                keytool = (fs::path(javaHome) / "bin" / "keytool").string();
            }
            check(crosside::io::runCommand(keytool, {
                "-genkeypair", "-keystore", keystore.string(),
                "-storepass", "android", "-alias", "androiddebugkey", "-keypass", "android",
                "-dname", "CN=Android Debug,O=Android,C=US", "-validity", "10000"
            }, projectDir, ctx));
        }

        check(crosside::io::runCommand(apksigner.string(), {
            "sign", "--ks", keystore.string(),
            "--ks-pass", "pass:android", "--out", finalApk.string(), targetApk.string()
        }, projectDir, ctx));

        std::cout << "[SUCCESS] APK created: " << finalApk << std::endl;
    }
};

// =============================================================================
// WEB PACKAGER
// =============================================================================

class WebPackager
{
    fs::path repoRoot;
    fs::path projectDir;
    json projectSpec;
    std::string name;
    std::string releaseName;
    fs::path outDir;
    fs::path srcWebDir;
    const crosside::Context &ctx;

public:
    WebPackager(fs::path root, fs::path proj, json releaseConfig, std::string relName, const crosside::Context &context)
        : repoRoot(root), projectDir(proj), releaseName(relName), ctx(context)
    {

        projectSpec = loadJson(projectDir / "main.mk");
        if (!releaseConfig.is_null())
        {
            mergeJson(projectSpec, releaseConfig);
        }
        name = projectSpec.value("Name", projectDir.filename().string());

        std::string outFolder = releaseName.empty() ? "Deploy" : releaseName;
        outDir = projectDir / "Web" / outFolder;
        srcWebDir = projectDir / "Web";
    }

    void packageAssets()
    {
        json config = loadJson(repoRoot / "config.json");
        std::string emsdkEnv = std::getenv("EMSDK") ? std::getenv("EMSDK") : "";
        fs::path emsdk = emsdkEnv.empty() ? fs::path(config["Configuration"]["Toolchain"].value("Emsdk", "")) : fs::path(emsdkEnv);

        if (emsdk.empty())
        {
            std::cerr << "[ERROR] EMSDK not found." << std::endl;
            return;
        }

        fs::path filePackager = emsdk / "upstream" / "emscripten" / "tools" / "file_packager.py";
        if (!fs::exists(filePackager))
            filePackager = emsdk / "emscripten" / "tools" / "file_packager.py";

        if (!fs::exists(filePackager))
        {
            std::cerr << "[ERROR] file_packager.py not found." << std::endl;
            return;
        }

        fs::path dataFile = outDir / (name + ".data");
        fs::path jsFile = outDir / (name + ".data.js");

        fs::path contentRoot = projectDir;
        if (projectSpec.contains("CONTENT_ROOT"))
            contentRoot = projectDir / projectSpec["CONTENT_ROOT"].get<std::string>();
        else if (projectSpec.contains("Web") && projectSpec["Web"].contains("CONTENT_ROOT"))
            contentRoot = projectDir / projectSpec["Web"]["CONTENT_ROOT"].get<std::string>();

        std::vector<std::string> args = {filePackager.string(), dataFile.string()};

        bool hasAssets = false;
        std::vector<std::string> folders = {"scripts", "assets", "resources", "data", "media"};
        for (const auto &folder : folders)
        {
            fs::path src = contentRoot / folder;
            if (fs::exists(src))
            {
                args.push_back("--preload");
                args.push_back(src.string() + "@" + folder);
                hasAssets = true;
            }
        }

        if (hasAssets)
        {
            args.push_back("--js-output=" + jsFile.string());
            args.push_back("--no-heap-copy");
            std::cout << "[PACK] Running file_packager..." << std::endl;
            check(crosside::io::runCommand("python3", args, projectDir, ctx));
            std::cout << "[PACK] Generated .data and .js" << std::endl;
        }
        else
        {
            std::cout << "[INFO] No assets to package." << std::endl;
        }
    }

    void package()
    {
        std::cout << "Packaging Web build for " << name << "..." << std::endl;
        if (fs::exists(outDir))
            fs::remove_all(outDir);
        fs::create_directories(outDir);

        // Copy binaries
        std::vector<std::string> exts = {".html", ".js", ".wasm"};
        bool found = false;
        for (const auto &ext : exts)
        {
            std::vector<fs::path> candidates = {
                srcWebDir / (name + ext),
                srcWebDir / ("index" + ext),
                srcWebDir / ("main" + ext)};
            for (const auto &c : candidates)
            {
                if (fs::exists(c))
                {
                    fs::copy_file(c, outDir / c.filename());
                    std::cout << "[COPY] " << c.filename() << std::endl;
                    found = true;
                }
            }
        }

        if (!found)
            std::cerr << "[WARNING] No Web binaries found!" << std::endl;

        packageAssets();
        std::cout << "[SUCCESS] Web deploy created: " << outDir << std::endl;
    }
};

// =============================================================================
// MAIN
// =============================================================================

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        std::cout << "Usage: packager <project_path> <target> [--release <release.json>]" << std::endl;
        return 1;
    }

    fs::path projectPath = fs::absolute(argv[1]);
    std::string target = argv[2];
    fs::path repoRoot = fs::absolute(fs::path(argv[0]).parent_path().parent_path()); // Assume bin/packager -> root

    // Ajuste do repoRoot se estivermos a correr de outro sitio
    if (!fs::exists(repoRoot / "config.json"))
    {
        // Tenta current path
        repoRoot = fs::current_path();
    }

    json releaseConfig;
    std::string releaseName = "";

    for (int i = 3; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--release" && i + 1 < argc)
        {
            fs::path relPath = argv[++i];
            if (!fs::exists(relPath))
                relPath = projectPath / relPath;

            if (fs::exists(relPath))
            {
                releaseConfig = loadJson(relPath);
                releaseName = relPath.stem().string();
            }
            else
            {
                std::cerr << "[ERROR] Release file not found: " << relPath << std::endl;
                return 1;
            }
        }
    }

    crosside::Context ctx;

    if (target == "android")
    {
        AndroidPackager pkg(repoRoot, projectPath, releaseConfig, releaseName, ctx);
        pkg.package();
    }
    else if (target == "web")
    {
        WebPackager pkg(repoRoot, projectPath, releaseConfig, releaseName, ctx);
        pkg.package();
    }
    else
    {
        std::cerr << "Unknown target: " << target << std::endl;
        return 1;
    }

    return 0;
}
