#include "build/android_builder.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "io/fs_utils.hpp"
#include "io/json_reader.hpp"
#include "io/process.hpp"
#include "model/loader.hpp"

namespace fs = std::filesystem;
using nlohmann::json;

namespace crosside::build
{
    namespace
    {

        constexpr const char *kTemplateManifest = R"(<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="@apppkg@"
          android:versionCode="1"
          android:versionName="1.0">

           <uses-sdk  android:compileSdkVersion="30"     android:minSdkVersion="16"  android:targetSdkVersion="23" />

  <application
      android:allowBackup="false"
      android:fullBackupContent="false"
      android:icon="@mipmap/ic_launcher"
      android:label="@applbl@"
      android:hasCode="false">


    <activity android:name="@appact@"
              android:label="@applbl@"
              android:configChanges="orientation|keyboardHidden|screenSize"
             android:screenOrientation="landscape" android:launchMode="singleTask"
             android:clearTaskOnLaunch="true">

      <meta-data android:name="android.app.lib_name"
                 android:value="@appLIBNAME@" />
      <intent-filter>
        <action android:name="android.intent.action.MAIN" />
        <category android:name="android.intent.category.LAUNCHER" />
      </intent-filter>
    </activity>
  </application>

</manifest>)";

        constexpr const char *kTemplateManifestJava = R"(<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="@apppkg@"
          android:versionCode="1"
          android:versionName="1.0">

    <uses-sdk
        android:compileSdkVersion="30"
        android:minSdkVersion="16"
        android:targetSdkVersion="23" />

    <application
        android:allowBackup="false"
        android:fullBackupContent="false"
        android:icon="@mipmap/ic_launcher"
        android:label="@applbl@"
        android:hasCode="true">

        <activity
            android:name="@appact@"
            android:label="@applbl@"
            android:configChanges="orientation|keyboardHidden|screenSize"
            android:screenOrientation="landscape"
            android:launchMode="singleTask"
            android:clearTaskOnLaunch="true">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>
    </application>

</manifest>)";

        struct AbiInfo
        {
            int value = 0;
            std::string name;
            std::string clangTarget;
            std::string includeTriple;
            std::string runtimeTriple;
            std::string unwindArch;
        };

        struct AndroidToolchain
        {
            fs::path androidSdk;
            fs::path androidNdk;
            fs::path javaHome;

            fs::path buildToolsRoot;
            fs::path platformJar;

            fs::path prebuiltRoot;
            fs::path sysroot;
            fs::path cppInclude;

            fs::path clang;
            fs::path clangxx;
            fs::path llvmAr;
            fs::path llvmStrip;

            fs::path aapt;
            fs::path dx;
            fs::path d8;
            fs::path apksigner;
            fs::path adb;
            fs::path keytool;
            fs::path javac;
        };

        struct CompileResult
        {
            std::vector<fs::path> objects;
            bool hasCpp = false;
        };

        std::string lower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
                           { return static_cast<char>(std::tolower(c)); });
            return value;
        }

        bool startsWith(const std::string &text, const std::string &prefix)
        {
            return text.rfind(prefix, 0) == 0;
        }

        void appendUnique(std::vector<std::string> &items, const std::string &value)
        {
            if (value.empty())
            {
                return;
            }
            if (std::find(items.begin(), items.end(), value) == items.end())
            {
                items.push_back(value);
            }
        }

        void appendAll(std::vector<std::string> &dst, const std::vector<std::string> &src)
        {
            for (const auto &value : src)
            {
                if (!value.empty())
                {
                    dst.push_back(value);
                }
            }
        }

        bool isCppSource(const fs::path &path)
        {
            const std::string ext = lower(path.extension().string());
            return ext == ".cc" || ext == ".cpp" || ext == ".cxx" || ext == ".mm" || ext == ".xpp";
        }

        bool isCompilable(const fs::path &path)
        {
            const std::string ext = lower(path.extension().string());
            return ext == ".c" || ext == ".cc" || ext == ".cpp" || ext == ".cxx" || ext == ".mm" || ext == ".xpp";
        }

        std::string pathString(const fs::path &path)
        {
            return path.lexically_normal().string();
        }

        std::string normalizeIconBucketKey(const std::string &value)
        {
            std::string key;
            key.reserve(value.size());
            for (unsigned char ch : value)
            {
                if (std::isspace(ch) == 0)
                {
                    key.push_back(static_cast<char>(std::tolower(ch)));
                }
            }

            if (key == "mdpi" || key == "mipmap-mdpi")
            {
                return "mipmap-mdpi";
            }
            if (key == "hdpi" || key == "mipmap-hdpi")
            {
                return "mipmap-hdpi";
            }
            if (key == "xhdpi" || key == "mipmap-xhdpi")
            {
                return "mipmap-xhdpi";
            }
            if (key == "xxhdpi" || key == "mipmap-xxhdpi")
            {
                return "mipmap-xxhdpi";
            }
            if (key == "xxxhdpi" || key == "mipmap-xxxhdpi")
            {
                return "mipmap-xxxhdpi";
            }

            return "";
        }

        std::optional<AbiInfo> abiInfoFromValue(int abi)
        {
            if (abi == 1)
            {
                return AbiInfo{1, "arm64-v8a", "aarch64-linux-android21", "aarch64-linux-android", "aarch64-linux-android", "aarch64"};
            }
            if (abi == 0)
            {
                return AbiInfo{0, "armeabi-v7a", "armv7a-linux-androideabi21", "arm-linux-androideabi", "arm-linux-androideabi", "arm"};
            }
            return std::nullopt;
        }

        std::vector<int> normalizeAbis(const std::vector<int> &abis)
        {
            std::vector<int> out;
            for (int abi : abis)
            {
                if ((abi == 0 || abi == 1) && std::find(out.begin(), out.end(), abi) == out.end())
                {
                    out.push_back(abi);
                }
            }
            if (out.empty())
            {
                out = {0, 1};
            }
            return out;
        }

        std::vector<int> numericKey(const std::string &value)
        {
            std::vector<int> out;
            std::string token;
            for (char ch : value)
            {
                if (std::isdigit(static_cast<unsigned char>(ch)) != 0)
                {
                    token.push_back(ch);
                }
                else if (!token.empty())
                {
                    out.push_back(std::stoi(token));
                    token.clear();
                }
            }
            if (!token.empty())
            {
                out.push_back(std::stoi(token));
            }
            if (out.empty())
            {
                out.push_back(0);
            }
            return out;
        }

        std::optional<std::string> latestSubdirName(const fs::path &root)
        {
            std::error_code ec;
            if (!fs::exists(root, ec) || !fs::is_directory(root, ec))
            {
                return std::nullopt;
            }

            std::vector<std::string> names;
            for (const auto &entry : fs::directory_iterator(root, ec))
            {
                if (ec)
                {
                    break;
                }
                if (!entry.is_directory(ec))
                {
                    continue;
                }
                names.push_back(entry.path().filename().string());
            }

            if (names.empty())
            {
                return std::nullopt;
            }

            std::sort(names.begin(), names.end(), [](const std::string &a, const std::string &b)
                      { return numericKey(a) < numericKey(b); });
            return names.back();
        }

        fs::path pickPath(const std::vector<fs::path> &candidates)
        {
            std::error_code ec;
            for (const auto &path : candidates)
            {
                if (!path.empty() && fs::exists(path, ec))
                {
                    return path;
                }
            }
            return {};
        }

        fs::path resolveToolInDir(const fs::path &root, const std::string &name)
        {
#ifdef _WIN32
            const std::vector<std::string> suffixes = {".exe", ".bat", ".cmd", ""};
#else
            const std::vector<std::string> suffixes = {""};
#endif

            std::vector<fs::path> candidates;
            for (const auto &suffix : suffixes)
            {
                candidates.push_back(root / (name + suffix));
            }
            return pickPath(candidates);
        }

        std::string envValue(const char *name)
        {
            const char *value = std::getenv(name);
            if (!value)
            {
                return "";
            }
            return std::string(value);
        }

        json readToolchainConfig(const fs::path &repoRoot, const crosside::Context &ctx)
        {
            const fs::path configPath = repoRoot / "config.json";
            if (!fs::exists(configPath))
            {
                return json::object();
            }

            try
            {
                json data = crosside::io::loadJsonFile(configPath);
                json root = data;
                if (data.contains("Configuration") && data["Configuration"].is_object())
                {
                    root = data["Configuration"];
                }
                if (root.contains("Toolchain") && root["Toolchain"].is_object())
                {
                    return root["Toolchain"];
                }
            }
            catch (const std::exception &e)
            {
                ctx.warn("Failed parse config.json toolchain: ", e.what());
            }
            return json::object();
        }

        std::string configString(const json &obj, const std::string &key)
        {
            if (!obj.is_object() || !obj.contains(key) || !obj[key].is_string())
            {
                return "";
            }
            return obj[key].get<std::string>();
        }

        fs::path pickNdk(const fs::path &androidSdk, const fs::path &preferredNdk)
        {
            std::error_code ec;
            if (!preferredNdk.empty() && fs::exists(preferredNdk, ec) && fs::is_directory(preferredNdk, ec))
            {
                return preferredNdk;
            }

            const fs::path ndkRoot = androidSdk / "ndk";
            auto latest = latestSubdirName(ndkRoot);
            if (latest.has_value())
            {
                return ndkRoot / latest.value();
            }
            return preferredNdk;
        }

        std::string pickBuildToolsVersion(const fs::path &androidSdk, const std::string &preferred)
        {
            const fs::path root = androidSdk / "build-tools";
            if (!preferred.empty() && fs::exists(root / preferred))
            {
                return preferred;
            }
            auto latest = latestSubdirName(root);
            if (latest.has_value())
            {
                return latest.value();
            }
            return preferred;
        }

        std::string pickPlatformVersion(const fs::path &androidSdk, const std::string &preferred)
        {
            const fs::path root = androidSdk / "platforms";
            if (!preferred.empty() && fs::exists(root / preferred / "android.jar"))
            {
                return preferred;
            }

            std::error_code ec;
            if (!fs::exists(root, ec) || !fs::is_directory(root, ec))
            {
                return preferred;
            }

            std::vector<std::string> candidates;
            for (const auto &entry : fs::directory_iterator(root, ec))
            {
                if (ec || !entry.is_directory(ec))
                {
                    continue;
                }
                if (fs::exists(entry.path() / "android.jar", ec))
                {
                    candidates.push_back(entry.path().filename().string());
                }
            }

            if (candidates.empty())
            {
                return preferred;
            }

            std::sort(candidates.begin(), candidates.end(), [](const std::string &a, const std::string &b)
                      { return numericKey(a) < numericKey(b); });
            return candidates.back();
        }

        fs::path pickPrebuiltRoot(const fs::path &androidNdk)
        {
            const fs::path root = androidNdk / "toolchains" / "llvm" / "prebuilt";

#ifdef _WIN32
            const fs::path host = root / "windows-x86_64";
#elif __APPLE__
            const fs::path host = root / "darwin-x86_64";
#else
            const fs::path host = root / "linux-x86_64";
#endif

            std::error_code ec;
            if (fs::exists(host, ec))
            {
                return host;
            }

            if (!fs::exists(root, ec) || !fs::is_directory(root, ec))
            {
                return {};
            }

            for (const auto &entry : fs::directory_iterator(root, ec))
            {
                if (ec)
                {
                    break;
                }
                if (entry.is_directory(ec))
                {
                    return entry.path();
                }
            }
            return {};
        }

        AndroidToolchain resolveToolchain(const fs::path &repoRoot, const crosside::Context &ctx)
        {
            const json config = readToolchainConfig(repoRoot, ctx);

            std::string androidSdkText = envValue("ANDROID_SDK_ROOT");
            if (androidSdkText.empty())
            {
                androidSdkText = envValue("ANDROID_HOME");
            }
            if (androidSdkText.empty())
            {
                androidSdkText = configString(config, "AndroidSdk");
            }
            if (androidSdkText.empty())
            {
                androidSdkText = "/home/djoker/android/android-sdk";
            }

            std::string androidNdkText = envValue("ANDROID_NDK_ROOT");
            if (androidNdkText.empty())
            {
                androidNdkText = configString(config, "AndroidNdk");
            }
            if (androidNdkText.empty())
            {
                androidNdkText = "/home/djoker/android/android-ndk-r27d";
            }

            std::string javaHomeText = envValue("JAVA_HOME");
            if (javaHomeText.empty())
            {
                javaHomeText = configString(config, "JavaSdk");
            }
            if (javaHomeText.empty())
            {
                javaHomeText = "/usr/lib/jvm/java-11-openjdk-amd64";
            }

            std::string buildToolsVersion = envValue("CROSSIDE_BUILD_TOOLS");
            if (buildToolsVersion.empty())
            {
                buildToolsVersion = configString(config, "BuildTools");
            }
            if (buildToolsVersion.empty())
            {
                buildToolsVersion = "30.0.2";
            }

            std::string platformVersion = envValue("CROSSIDE_PLATFORM");
            if (platformVersion.empty())
            {
                platformVersion = configString(config, "Platform");
            }
            if (platformVersion.empty())
            {
                platformVersion = "android-31";
            }

            AndroidToolchain out;
            out.androidSdk = fs::path(androidSdkText);
            out.androidNdk = pickNdk(out.androidSdk, fs::path(androidNdkText));
            out.javaHome = fs::path(javaHomeText);

            buildToolsVersion = pickBuildToolsVersion(out.androidSdk, buildToolsVersion);
            platformVersion = pickPlatformVersion(out.androidSdk, platformVersion);

            out.buildToolsRoot = out.androidSdk / "build-tools" / buildToolsVersion;
            out.platformJar = out.androidSdk / "platforms" / platformVersion / "android.jar";

            out.prebuiltRoot = pickPrebuiltRoot(out.androidNdk);
            out.sysroot = out.prebuiltRoot / "sysroot";
            out.cppInclude = out.sysroot / "usr" / "include" / "c++" / "v1";

            const fs::path prebuiltBin = out.prebuiltRoot / "bin";
            out.clang = resolveToolInDir(prebuiltBin, "clang");
            out.clangxx = resolveToolInDir(prebuiltBin, "clang++");
            out.llvmAr = resolveToolInDir(prebuiltBin, "llvm-ar");
            out.llvmStrip = resolveToolInDir(prebuiltBin, "llvm-strip");

            out.aapt = resolveToolInDir(out.buildToolsRoot, "aapt");
            out.dx = resolveToolInDir(out.buildToolsRoot, "dx");
            out.d8 = resolveToolInDir(out.buildToolsRoot, "d8");
            out.apksigner = resolveToolInDir(out.buildToolsRoot, "apksigner");

            out.adb = resolveToolInDir(out.androidSdk / "platform-tools", "adb");
            out.keytool = resolveToolInDir(out.javaHome / "bin", "keytool");
            out.javac = resolveToolInDir(out.javaHome / "bin", "javac");

            if (out.keytool.empty())
            {
                out.keytool = "keytool";
            }
            if (out.javac.empty())
            {
                out.javac = "javac";
            }
            if (out.dx.empty())
            {
                out.dx = "dx";
            }
            if (out.d8.empty())
            {
                out.d8 = "d8";
            }

            return out;
        }

        bool validateToolchainCompile(const crosside::Context &ctx, const AndroidToolchain &tc)
        {
            std::vector<fs::path> required = {
                tc.androidSdk,
                tc.androidNdk,
                tc.prebuiltRoot,
                tc.sysroot,
                tc.clang,
                tc.clangxx,
                tc.llvmAr,
            };
            for (const auto &path : required)
            {
                if (path.empty() || !fs::exists(path))
                {
                    ctx.error("Missing Android compile toolchain path: ", path.string());
                    return false;
                }
            }
            return true;
        }

        bool validateToolchainPackage(const crosside::Context &ctx, const AndroidToolchain &tc)
        {
            std::vector<fs::path> required = {
                tc.aapt,
                tc.apksigner,
                tc.platformJar,
                tc.adb,
            };
            for (const auto &path : required)
            {
                if (path.empty() || !fs::exists(path))
                {
                    ctx.error("Missing Android packaging path: ", path.string());
                    return false;
                }
            }
            return true;
        }

        std::optional<fs::path> findLatestLibUnwind(const AndroidToolchain &tc, const AbiInfo &abi)
        {
            const fs::path clangRoot = tc.prebuiltRoot / "lib" / "clang";
            std::error_code ec;
            if (!fs::exists(clangRoot, ec))
            {
                return std::nullopt;
            }

            std::vector<fs::path> versions;
            for (const auto &entry : fs::directory_iterator(clangRoot, ec))
            {
                if (ec || !entry.is_directory(ec))
                {
                    continue;
                }
                versions.push_back(entry.path());
            }
            if (versions.empty())
            {
                return std::nullopt;
            }

            std::sort(versions.begin(), versions.end(), [](const fs::path &a, const fs::path &b)
                      { return numericKey(a.filename().string()) < numericKey(b.filename().string()); });

            for (auto it = versions.rbegin(); it != versions.rend(); ++it)
            {
                const fs::path candidate = *it / "lib" / "linux" / abi.unwindArch / "libunwind.a";
                if (fs::exists(candidate, ec))
                {
                    return candidate;
                }
            }
            return std::nullopt;
        }

        void addIncludeFlag(std::vector<std::string> &cc, std::vector<std::string> &cpp, const fs::path &path)
        {
            const std::string flag = "-I" + pathString(path);
            appendUnique(cc, flag);
            appendUnique(cpp, flag);
        }

        void collectModuleIncludeFlagsAndroid(
            const crosside::model::ModuleSpec &module,
            const crosside::model::PlatformBlock &block,
            std::vector<std::string> &cc,
            std::vector<std::string> &cpp)
        {
            addIncludeFlag(cc, cpp, module.dir / "src");
            addIncludeFlag(cc, cpp, module.dir / "include");
            addIncludeFlag(cc, cpp, module.dir / "include" / "android");

            for (const auto &item : module.main.include)
            {
                addIncludeFlag(cc, cpp, module.dir / item);
            }
            for (const auto &item : block.include)
            {
                addIncludeFlag(cc, cpp, module.dir / item);
            }
        }

        bool moduleSupportsAndroid(const crosside::model::ModuleSpec &module)
        {
            if (module.systems.empty())
            {
                return true;
            }
            for (const auto &system : module.systems)
            {
                if (lower(system) == "android")
                {
                    return true;
                }
            }
            return false;
        }

        std::vector<fs::path> collectModuleSourcesAndroid(const crosside::model::ModuleSpec &module, const crosside::Context &ctx)
        {
            (void)ctx;
            std::vector<fs::path> out;
            std::set<std::string> seen;

            auto appendSource = [&](const std::string &rel)
            {
                if (rel.empty())
                {
                    return;
                }
                const fs::path path = fs::absolute(module.dir / rel);
                if (!fs::exists(path) || !isCompilable(path))
                {
                    return;
                }
                const std::string key = pathString(path);
                if (seen.insert(key).second)
                {
                    out.push_back(path);
                }
            };

            for (const auto &src : module.main.src)
            {
                appendSource(src);
            }
            for (const auto &src : module.android.src)
            {
                appendSource(src);
            }

            return out;
        }

        std::optional<fs::path> findPrebuiltModuleOutputAndroid(
            const fs::path &outDir,
            const std::string &moduleName,
            bool staticLib)
        {
            std::error_code ec;
            if (!fs::exists(outDir, ec) || !fs::is_directory(outDir, ec))
            {
                return std::nullopt;
            }

            const std::string expectedExt = staticLib ? ".a" : ".so";
            const std::string expectedNameLower = lower(moduleName);

            for (const auto &entry : fs::directory_iterator(outDir, ec))
            {
                if (ec || !entry.is_regular_file(ec))
                {
                    continue;
                }

                const fs::path candidate = entry.path();
                if (candidate.extension() != expectedExt)
                {
                    continue;
                }

                std::string stem = candidate.stem().string(); // libxxx
                if (stem.rfind("lib", 0) == 0)
                {
                    stem = stem.substr(3);
                }

                if (lower(stem) == expectedNameLower)
                {
                    return candidate;
                }
            }

            return std::nullopt;
        }

        fs::path resolveNdkBuildTool(const AndroidToolchain &tc)
        {
#ifdef _WIN32
            const fs::path cmd = tc.androidNdk / "ndk-build.cmd";
            if (fs::exists(cmd))
            {
                return cmd;
            }
            const fs::path bat = tc.androidNdk / "ndk-build.bat";
            if (fs::exists(bat))
            {
                return bat;
            }
#endif
            const fs::path bin = tc.androidNdk / "ndk-build";
            if (fs::exists(bin))
            {
                return bin;
            }
            return {};
        }

        void copyLibraryArtifacts(
            const fs::path &srcDir,
            const fs::path &dstDir,
            const std::string &skipModuleNameLower,
            const crosside::Context &ctx)
        {
            std::error_code ec;
            if (!fs::exists(srcDir, ec) || !fs::is_directory(srcDir, ec))
            {
                return;
            }
            if (!crosside::io::ensureDir(dstDir))
            {
                return;
            }

            for (const auto &entry : fs::directory_iterator(srcDir, ec))
            {
                if (ec || !entry.is_regular_file(ec))
                {
                    continue;
                }

                const fs::path file = entry.path();
                const std::string ext = lower(file.extension().string());
                if (ext != ".a" && ext != ".so")
                {
                    continue;
                }

                if (!skipModuleNameLower.empty())
                {
                    std::string stem = lower(file.stem().string());
                    if (stem.rfind("lib", 0) == 0)
                    {
                        stem = stem.substr(3);
                    }
                    if (stem == skipModuleNameLower)
                    {
                        continue;
                    }
                }

                const fs::path dst = dstDir / file.filename();
                fs::copy_file(file, dst, fs::copy_options::overwrite_existing, ec);
                if (ec)
                {
                    ctx.warn("Failed copy artifact ", file.string(), " -> ", dst.string(), " : ", ec.message());
                    ec.clear();
                }
            }
        }

        void removeDuplicateModuleArtifacts(
            const fs::path &outDir,
            const fs::path &keepLib,
            const std::string &moduleNameLower,
            const crosside::Context &ctx)
        {
            std::error_code ec;
            if (!fs::exists(outDir, ec) || !fs::is_directory(outDir, ec))
            {
                return;
            }

            for (const auto &entry : fs::directory_iterator(outDir, ec))
            {
                if (ec || !entry.is_regular_file(ec))
                {
                    continue;
                }

                const fs::path file = entry.path();
                if (pathString(file) == pathString(keepLib))
                {
                    continue;
                }

                const std::string ext = lower(file.extension().string());
                if (ext != ".a" && ext != ".so")
                {
                    continue;
                }

                std::string stem = lower(file.stem().string());
                if (stem.rfind("lib", 0) == 0)
                {
                    stem = stem.substr(3);
                }
                if (stem != moduleNameLower)
                {
                    continue;
                }

                fs::remove(file, ec);
                if (ec)
                {
                    ctx.warn("Failed remove duplicate module artifact ", file.string(), " : ", ec.message());
                    ec.clear();
                }
            }
        }

        bool tryBuildModuleWithNdkBuild(
            const crosside::Context &ctx,
            const AndroidToolchain &tc,
            const crosside::model::ModuleSpec &module,
            const AbiInfo &abi,
            const fs::path &outDir,
            const fs::path &outLib)
        {
            const fs::path androidMk = module.dir / "Android.mk";
            if (!fs::exists(androidMk))
            {
                return false;
            }

            const fs::path ndkBuild = resolveNdkBuildTool(tc);
            if (ndkBuild.empty())
            {
                ctx.warn("ndk-build not found for module ", module.name, " (expected under ", tc.androidNdk.string(), ")");
                return false;
            }

            const fs::path ndkOut = module.dir / "obj" / "ndk";
            std::vector<std::string> args = {
                "-C",
                pathString(module.dir),
                "APP_BUILD_SCRIPT=Android.mk",
                "NDK_PROJECT_PATH=" + pathString(module.dir),
                "NDK_OUT=" + pathString(ndkOut),
                "NDK_LIBS_OUT=" + pathString(module.dir / "Android"),
                "APP_PLATFORM=android-21",
                "APP_ABI=" + abi.name,
                "APP_STL=c++_static",
                "-j8",
            };

            auto ndk = crosside::io::runCommand(pathString(ndkBuild), args, {}, ctx, false);
            if (ndk.code != 0)
            {
                ctx.warn("ndk-build failed for module ", module.name, " [", abi.name, "]");
                return false;
            }

            if (!crosside::io::ensureDir(outDir))
            {
                ctx.error("Failed create module Android output dir: ", outDir.string());
                return false;
            }

            const fs::path localOut = ndkOut / "local" / abi.name;
            const std::string moduleNameLower = lower(module.name);
            copyLibraryArtifacts(localOut, outDir, moduleNameLower, ctx);
            copyLibraryArtifacts(module.dir / "obj" / "local" / abi.name, outDir, moduleNameLower, ctx);

            auto built = findPrebuiltModuleOutputAndroid(outDir, module.name, module.staticLib);
            if (!built.has_value())
            {
                built = findPrebuiltModuleOutputAndroid(localOut, module.name, module.staticLib);
            }
            if (!built.has_value())
            {
                built = findPrebuiltModuleOutputAndroid(module.dir / "obj" / "local" / abi.name, module.name, module.staticLib);
            }
            if (!built.has_value())
            {
                ctx.warn("ndk-build finished but output for module ", module.name, " [", abi.name, "] was not found");
                return false;
            }

            std::error_code ec;
            if (pathString(built.value()) != pathString(outLib))
            {
                fs::copy_file(built.value(), outLib, fs::copy_options::overwrite_existing, ec);
                if (ec)
                {
                    ctx.error("Failed stage module output ", built->string(), " -> ", outLib.string(), " : ", ec.message());
                    return false;
                }
            }

            removeDuplicateModuleArtifacts(outDir, outLib, moduleNameLower, ctx);

            ctx.log("Build module ", module.name, " via ndk-build for ", abi.name, " -> ", outLib.string());
            return true;
        }

        std::vector<fs::path> collectProjectSourcesAndroid(const crosside::model::ProjectSpec &project, const crosside::Context &ctx)
        {
            std::vector<fs::path> out;
            std::set<std::string> seen;

            for (const auto &src : project.src)
            {
                if (!fs::exists(src) || !isCompilable(src))
                {
                    continue;
                }
                const fs::path full = fs::absolute(src);
                const std::string key = pathString(full);
                if (seen.insert(key).second)
                {
                    out.push_back(full);
                }
            }

            if (out.empty())
            {
                ctx.error("No compilable Android sources for project ", project.name);
            }

            return out;
        }

        bool compileAndroidSources(
            const crosside::Context &ctx,
            const AndroidToolchain &tc,
            const fs::path &baseRoot,
            const fs::path &objRoot,
            const std::vector<fs::path> &sources,
            const std::vector<std::string> &ccFlags,
            const std::vector<std::string> &cppFlags,
            const AbiInfo &abi,
            bool fullBuild,
            CompileResult &result)
        {
            result.objects.clear();
            result.hasCpp = false;

            if (!crosside::io::ensureDir(objRoot))
            {
                ctx.error("Failed create object dir: ", objRoot.string());
                return false;
            }

            for (const auto &src : sources)
            {
                const bool cppSource = isCppSource(src);
                if (cppSource)
                {
                    result.hasCpp = true;
                }

                fs::path relParent;
                try
                {
                    relParent = fs::relative(src.parent_path(), baseRoot);
                }
                catch (...)
                {
                    relParent = src.parent_path().filename();
                }

                const fs::path objDir = objRoot / relParent;
                if (!crosside::io::ensureDir(objDir))
                {
                    ctx.error("Failed create object subdir: ", objDir.string());
                    return false;
                }

                const fs::path obj = objDir / (src.stem().string() + ".o");

                if (!fullBuild && fs::exists(obj))
                {
                    std::error_code ec;
                    const auto srcTime = fs::last_write_time(src, ec);
                    if (ec)
                    {
                        ctx.warn("Failed to read source timestamp: ", src.string());
                    }
                    else
                    {
                        const auto objTime = fs::last_write_time(obj, ec);
                        if (!ec && objTime >= srcTime)
                        {
                            ctx.log("Skip ", src.string());
                            result.objects.push_back(obj);
                            continue;
                        }
                    }
                }

                std::vector<std::string> args;
                args.push_back("-target");
                args.push_back(abi.clangTarget);
                args.push_back("--sysroot");
                args.push_back(pathString(tc.sysroot));

                args.push_back("-fdata-sections");
                args.push_back("-ffunction-sections");
                args.push_back("-fstack-protector-strong");
                args.push_back("-funwind-tables");
                args.push_back("-no-canonical-prefixes");

                args.push_back("-D_FORTIFY_SOURCE=2");
                args.push_back("-fpic");
                args.push_back("-Wformat");
                args.push_back("-Werror=format-security");
                args.push_back("-fno-strict-aliasing");
                args.push_back("-DNDEBUG");
                args.push_back("-DANDROID");
                args.push_back("-DPLATFORM_ANDROID");

                if (abi.value == 0)
                {
                    args.push_back("-march=armv7-a");
                    args.push_back("-mthumb");
                    args.push_back("-Oz");
                }
                else
                {
                    args.push_back("-O2");
                }

                args.push_back("-I" + pathString(tc.sysroot / "usr" / "include" / abi.includeTriple));
                args.push_back("-I" + pathString(tc.sysroot / "usr" / "include"));
                args.push_back("-I" + pathString(baseRoot));
                args.push_back("-I" + pathString(src.parent_path()));

                if (cppSource)
                {
                    args.push_back("-nostdinc++");
                    args.push_back("-I" + pathString(tc.cppInclude));
                    appendAll(args, cppFlags);
                }
                else
                {
                    appendAll(args, ccFlags);
                }

                args.push_back("-c");
                args.push_back(pathString(src));
                args.push_back("-o");
                args.push_back(pathString(obj));

                const std::string compiler = cppSource ? pathString(tc.clangxx) : pathString(tc.clang);
                auto command = crosside::io::runCommand(compiler, args, {}, ctx, false);
                if (command.code != 0)
                {
                    ctx.error("Compile failed for ", src.string());
                    return false;
                }

                result.objects.push_back(obj);
            }

            return !result.objects.empty();
        }

        bool archiveAndroidStatic(
            const crosside::Context &ctx,
            const AndroidToolchain &tc,
            const fs::path &output,
            const std::vector<fs::path> &objects)
        {
            if (objects.empty())
            {
                ctx.error("No objects to archive for ", output.string());
                return false;
            }

            if (!crosside::io::ensureDir(output.parent_path()))
            {
                ctx.error("Failed create output directory: ", output.parent_path().string());
                return false;
            }

            std::error_code ec;
            fs::remove(output, ec);

            std::vector<std::string> args;
            args.push_back("rcs");
            args.push_back(pathString(output));
            for (const auto &obj : objects)
            {
                args.push_back(pathString(obj));
            }

            auto command = crosside::io::runCommand(pathString(tc.llvmAr), args, {}, ctx, false);
            if (command.code != 0)
            {
                ctx.error("Static archive failed: ", output.string());
                return false;
            }
            return true;
        }

        void appendCppRuntimeLibraries(std::vector<std::string> &args, const AndroidToolchain &tc, const AbiInfo &abi)
        {
            const fs::path runtimeDir = tc.sysroot / "usr" / "lib" / abi.runtimeTriple;
            const fs::path libcxx = runtimeDir / "libc++_static.a";
            const fs::path libcxxabi = runtimeDir / "libc++abi.a";

            if (fs::exists(libcxx))
            {
                args.push_back(pathString(libcxx));
            }
            if (fs::exists(libcxxabi))
            {
                args.push_back(pathString(libcxxabi));
            }

            auto unwind = findLatestLibUnwind(tc, abi);
            if (unwind.has_value() && fs::exists(unwind.value()))
            {
                args.push_back(pathString(unwind.value()));
            }
        }

        bool linkAndroidShared(
            const crosside::Context &ctx,
            const fs::path &repoRoot,
            const AndroidToolchain &tc,
            const AbiInfo &abi,
            const std::string &name,
            const std::vector<fs::path> &objects,
            const std::vector<std::string> &ldFlags,
            bool hasCpp,
            const fs::path &output)
        {
            if (objects.empty())
            {
                ctx.error("No objects to link for ", name);
                return false;
            }

            if (!crosside::io::ensureDir(output.parent_path()))
            {
                ctx.error("Failed create output directory: ", output.parent_path().string());
                return false;
            }

            std::vector<std::string> args;
            args.push_back("-Wl,-soname,lib" + name + ".so");
            args.push_back("-shared");

            for (const auto &obj : objects)
            {
                args.push_back(pathString(obj));
            }

            const fs::path projectLibRoot = repoRoot / "libs" / "android" / abi.name;
            if (fs::exists(projectLibRoot))
            {
                appendUnique(args, "-L" + pathString(projectLibRoot));
            }

            appendUnique(args, "-Wl,--no-whole-archive");
            if (hasCpp)
            {
                appendCppRuntimeLibraries(args, tc, abi);
            }

            args.push_back("-target");
            args.push_back(abi.clangTarget);
            args.push_back("--sysroot");
            args.push_back(pathString(tc.sysroot));
            args.push_back("-no-canonical-prefixes");
            args.push_back("-Wl,--build-id");
            if (hasCpp)
            {
                args.push_back("-nostdlib++");
            }
            args.push_back("-Wl,--no-undefined");
            args.push_back("-Wl,--fatal-warnings");

            appendAll(args, ldFlags);

            args.push_back("-o");
            args.push_back(pathString(output));

            auto link = crosside::io::runCommand(hasCpp ? pathString(tc.clangxx) : pathString(tc.clang), args, {}, ctx, false);
            if (link.code != 0)
            {
                ctx.error("Link failed for ", output.string());
                return false;
            }

            if (!tc.llvmStrip.empty() && fs::exists(tc.llvmStrip))
            {
                auto strip = crosside::io::runCommand(pathString(tc.llvmStrip), {"--strip-unneeded", pathString(output)}, {}, ctx, false);
                if (strip.code != 0)
                {
                    ctx.warn("Strip failed for ", output.string());
                }
            }

            return true;
        }

        void appendModuleDependencyFlags(
            const crosside::model::ModuleSpec &module,
            const crosside::model::ModuleMap &modules,
            const AbiInfo &abi,
            std::vector<std::string> &cc,
            std::vector<std::string> &cpp,
            std::vector<std::string> &ld,
            const crosside::Context &ctx)
        {
            auto appendModuleLibLinkArgs = [&](const fs::path &libDir, const std::string &moduleName)
            {
                const fs::path staticLib = libDir / ("lib" + moduleName + ".a");
                const fs::path sharedLib = libDir / ("lib" + moduleName + ".so");
                const bool hasCanonical = fs::exists(staticLib) || fs::exists(sharedLib);
                if (hasCanonical)
                {
                    appendUnique(ld, "-l" + moduleName);
                }

                std::error_code ec;
                if (!fs::exists(libDir, ec) || !fs::is_directory(libDir, ec))
                {
                    return;
                }

                const std::string moduleLower = lower(moduleName);
                for (const auto &entry : fs::directory_iterator(libDir, ec))
                {
                    if (ec || !entry.is_regular_file(ec))
                    {
                        continue;
                    }

                    const fs::path path = entry.path();
                    const std::string ext = lower(path.extension().string());
                    if (ext != ".a" && ext != ".so")
                    {
                        continue;
                    }

                    if (hasCanonical)
                    {
                        continue;
                    }

                    const std::string stem = path.stem().string();
                    const std::string stemLower = lower(stem);
                    if (!startsWith(stemLower, "lib") || stem.size() <= 3)
                    {
                        continue;
                    }

                    const std::string altName = stem.substr(3);
                    if (altName.empty() || altName == moduleName)
                    {
                        continue;
                    }
                    if (lower(altName) != moduleLower)
                    {
                        continue;
                    }

                    appendUnique(ld, "-l" + altName);
                }
            };

            const std::vector<std::string> deps = crosside::model::moduleClosure(module.depends, modules, ctx);
            for (const auto &depName : deps)
            {
                auto it = modules.find(depName);
                if (it == modules.end())
                {
                    continue;
                }
                const auto &dep = it->second;
                collectModuleIncludeFlagsAndroid(dep, dep.android, cc, cpp);

                const fs::path depLibDir = dep.dir / "Android" / abi.name;
                appendUnique(ld, "-L" + pathString(depLibDir));
                appendModuleLibLinkArgs(depLibDir, dep.name);

                appendAll(ld, dep.main.ldArgs);
                appendAll(ld, dep.android.ldArgs);
            }
        }

        void collectProjectModuleFlags(
            const fs::path &repoRoot,
            const crosside::model::ModuleMap &modules,
            const std::vector<std::string> &activeModules,
            const AbiInfo &abi,
            std::vector<std::string> &cc,
            std::vector<std::string> &cpp,
            std::vector<std::string> &ld,
            const crosside::Context &ctx)
        {
            auto appendModuleLibLinkArgs = [&](const fs::path &libDir, const std::string &moduleName)
            {
                const fs::path staticLib = libDir / ("lib" + moduleName + ".a");
                const fs::path sharedLib = libDir / ("lib" + moduleName + ".so");
                const bool hasCanonical = fs::exists(staticLib) || fs::exists(sharedLib);
                if (hasCanonical)
                {
                    appendUnique(ld, "-l" + moduleName);
                }

                std::error_code ec;
                if (!fs::exists(libDir, ec) || !fs::is_directory(libDir, ec))
                {
                    return;
                }

                const std::string moduleLower = lower(moduleName);
                for (const auto &entry : fs::directory_iterator(libDir, ec))
                {
                    if (ec || !entry.is_regular_file(ec))
                    {
                        continue;
                    }

                    const fs::path path = entry.path();
                    const std::string ext = lower(path.extension().string());
                    if (ext != ".a" && ext != ".so")
                    {
                        continue;
                    }

                    if (hasCanonical)
                    {
                        continue;
                    }

                    const std::string stem = path.stem().string();
                    const std::string stemLower = lower(stem);
                    if (!startsWith(stemLower, "lib") || stem.size() <= 3)
                    {
                        continue;
                    }

                    const std::string altName = stem.substr(3);
                    if (altName.empty() || altName == moduleName)
                    {
                        continue;
                    }
                    if (lower(altName) != moduleLower)
                    {
                        continue;
                    }

                    appendUnique(ld, "-l" + altName);
                }
            };

            const std::vector<std::string> allModules = crosside::model::moduleClosure(activeModules, modules, ctx);

            for (const auto &moduleName : allModules)
            {
                auto it = modules.find(moduleName);
                if (it != modules.end())
                {
                    const auto &module = it->second;
                    collectModuleIncludeFlagsAndroid(module, module.android, cc, cpp);

                    const fs::path libDir = module.dir / "Android" / abi.name;
                    appendUnique(ld, "-L" + pathString(libDir));
                    appendModuleLibLinkArgs(libDir, module.name);

                    appendAll(ld, module.main.ldArgs);
                    appendAll(ld, module.android.ldArgs);
                    continue;
                }

                const fs::path fallbackDir = repoRoot / "modules" / moduleName;
                addIncludeFlag(cc, cpp, fallbackDir / "include");
                addIncludeFlag(cc, cpp, fallbackDir / "include" / "android");

                const fs::path libDir = fallbackDir / "Android" / abi.name;
                appendUnique(ld, "-L" + pathString(libDir));
                appendModuleLibLinkArgs(libDir, moduleName);
            }
        }

        std::string replaceAll(std::string text, const std::string &from, const std::string &to)
        {
            if (from.empty())
            {
                return text;
            }
            std::size_t pos = 0;
            while ((pos = text.find(from, pos)) != std::string::npos)
            {
                text.replace(pos, from.size(), to);
                pos += to.size();
            }
            return text;
        }

        std::string sanitizeAndroidPackage(const std::string &packageName, const std::string &fallback = "com.djokersoft.game")
        {
            std::string value = packageName;
            for (char &ch : value)
            {
                if (ch == '/')
                {
                    ch = '.';
                }
            }

            value = std::regex_replace(value, std::regex("[^A-Za-z0-9_.]"), "");
            value = std::regex_replace(value, std::regex("\\.+"), ".");

            while (!value.empty() && value.front() == '.')
            {
                value.erase(value.begin());
            }
            while (!value.empty() && value.back() == '.')
            {
                value.pop_back();
            }

            std::vector<std::string> parts;
            std::string token;
            for (std::size_t i = 0; i <= value.size(); ++i)
            {
                if (i == value.size() || value[i] == '.')
                {
                    if (!token.empty())
                    {
                        token = std::regex_replace(token, std::regex("[^A-Za-z0-9_]"), "");
                        if (!token.empty() && std::isdigit(static_cast<unsigned char>(token.front())) != 0)
                        {
                            token = "p" + token;
                        }
                        if (!token.empty())
                        {
                            parts.push_back(token);
                        }
                    }
                    token.clear();
                    continue;
                }
                token.push_back(value[i]);
            }

            if (parts.size() < 2)
            {
                return fallback;
            }

            std::string out;
            for (std::size_t i = 0; i < parts.size(); ++i)
            {
                if (i > 0)
                {
                    out.push_back('.');
                }
                out += parts[i];
            }
            return out;
        }

        std::string normalizeActivity(const std::string &packageName, const std::string &activity)
        {
            std::string out = activity;
            if (out.empty())
            {
                out = "android.app.NativeActivity";
            }

            if (!out.empty() && out.front() == '.')
            {
                return packageName + out;
            }
            if (out.find('.') == std::string::npos)
            {
                return packageName + "." + out;
            }
            return out;
        }

        bool useNativeManifestTemplate(const crosside::model::ProjectSpec &project, const std::string &activity)
        {
            const std::string mode = lower(project.androidManifestMode);
            if (mode == "native")
            {
                return true;
            }
            if (mode == "java" || mode == "sdl" || mode == "sdl2")
            {
                return false;
            }

            return lower(activity).find("nativeactivity") != std::string::npos;
        }

        std::string buildManifest(
            const std::string &manifestTemplate,
            const std::string &packageName,
            const std::string &label,
            const std::string &activity,
            const std::string &libName,
            const std::unordered_map<std::string, std::string> &customVars)
        {
            std::string out = manifestTemplate.empty() ? std::string(kTemplateManifest) : manifestTemplate;

            out = replaceAll(out, "@apppkg@", packageName);
            out = replaceAll(out, "@applbl@", label);
            out = replaceAll(out, "@appact@", activity);
            out = replaceAll(out, "@appactv@", activity);
            out = replaceAll(out, "@appLIBNAME@", libName);
            out = replaceAll(out, "@APP_PACKAGE@", packageName);
            out = replaceAll(out, "@APP_LABEL@", label);
            out = replaceAll(out, "@APP_ACTIVITY@", activity);
            out = replaceAll(out, "@APP_LIB_NAME@", libName);

            for (const auto &[key, value] : customVars)
            {
                if (key.empty())
                {
                    continue;
                }
                if (key.find('@') != std::string::npos)
                {
                    out = replaceAll(out, key, value);
                    continue;
                }

                out = replaceAll(out, "@" + key + "@", value);
                out = replaceAll(out, "${" + key + "}", value);
            }
            return out;
        }

        bool readTextFile(const fs::path &filePath, std::string &outText)
        {
            std::ifstream in(filePath, std::ios::binary);
            if (!in.is_open())
            {
                return false;
            }
            outText.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
            return true;
        }

        std::optional<std::string> loadManifestTemplate(
            const crosside::Context &ctx,
            const fs::path &repoRoot,
            const crosside::model::ProjectSpec &project,
            const std::string &activity)
        {
            if (!project.androidManifestTemplate.empty())
            {
                fs::path templatePath = project.androidManifestTemplate;
                if (!templatePath.is_absolute())
                {
                    templatePath = fs::absolute(project.root / templatePath);
                }

                std::string text;
                if (!readTextFile(templatePath, text))
                {
                    ctx.error("Failed read Android manifest template: ", templatePath.string());
                    return std::nullopt;
                }
                return text;
            }

            const bool nativeTemplate = useNativeManifestTemplate(project, activity);
            const fs::path templatePath = nativeTemplate
                                              ? pickPath({
                                                    repoRoot / "Templates" / "Android" / "AndroidManifest.xml",
                                                    repoRoot / "Templates" / "Android" / "AndroidManifest.template.xml",
                                                })
                                              : pickPath({
                                                    repoRoot / "Templates" / "Android" / "AndroidManifest.java.xml",
                                                    repoRoot / "Templates" / "Android" / "AndroidManifest_java.xml",
                                                    repoRoot / "Templates" / "Android" / "AndroidManifest.sdl2.xml",
                                                    repoRoot / "Templates" / "Android" / "AndroidManifest_sdl2.xml",
                                                });

            if (templatePath.empty())
            {
                return nativeTemplate ? std::string(kTemplateManifest) : std::string(kTemplateManifestJava);
            }

            std::string text;
            if (!readTextFile(templatePath, text))
            {
                ctx.warn("Failed read default Android manifest template, using embedded fallback: ", templatePath.string());
                return nativeTemplate ? std::string(kTemplateManifest) : std::string(kTemplateManifestJava);
            }
            return text;
        }

        bool resourceExistsForRef(const fs::path &resRoot, const std::string &resourceRef)
        {
            if (resourceRef.empty() || resourceRef[0] != '@')
            {
                return true;
            }
            if (startsWith(resourceRef, "@android:"))
            {
                return true;
            }

            const std::string body = resourceRef.substr(1);
            const std::size_t slash = body.find('/');
            if (slash == std::string::npos || slash == 0 || slash + 1 >= body.size())
            {
                return false;
            }

            const std::string type = body.substr(0, slash);
            const std::string name = body.substr(slash + 1);

            std::error_code ec;
            if (!fs::exists(resRoot, ec) || !fs::is_directory(resRoot, ec))
            {
                return false;
            }

            for (const auto &entry : fs::directory_iterator(resRoot, ec))
            {
                if (ec || !entry.is_directory(ec))
                {
                    continue;
                }

                const std::string folder = entry.path().filename().string();
                if (!(folder == type || startsWith(folder, type + "-")))
                {
                    continue;
                }

                for (const auto &file : fs::directory_iterator(entry.path(), ec))
                {
                    if (ec || !file.is_regular_file(ec))
                    {
                        continue;
                    }
                    if (file.path().stem().string() == name)
                    {
                        return true;
                    }
                }
            }

            return false;
        }

        void ensureManifestIconFallback(const crosside::Context &ctx, const fs::path &manifestPath, const fs::path &resRoot)
        {
            std::ifstream in(manifestPath);
            if (!in.is_open())
            {
                return;
            }

            const std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            const std::regex iconRegex(R"REGEX(android:icon="(@[^"]+)")REGEX");
            std::smatch match;
            if (!std::regex_search(content, match, iconRegex))
            {
                return;
            }

            if (match.size() < 2)
            {
                return;
            }

            const std::string iconRef = match[1].str();
            if (resourceExistsForRef(resRoot, iconRef))
            {
                return;
            }

            const std::string fallback = "@android:drawable/sym_def_app_icon";
            const std::string patched = content.substr(0, static_cast<std::size_t>(match.position(1))) + fallback + content.substr(static_cast<std::size_t>(match.position(1) + match.length(1)));

            std::ofstream out(manifestPath, std::ios::trunc);
            if (out.is_open())
            {
                out << patched;
                ctx.warn("Missing icon resource ", iconRef, ", using ", fallback);
            }
        }

        void ensureManifestRoundIcon(const crosside::Context &ctx, const fs::path &manifestPath, const fs::path &resRoot)
        {
            const std::string desiredRef = "@mipmap/ic_launcher_round";
            if (!resourceExistsForRef(resRoot, desiredRef))
            {
                return;
            }

            std::ifstream in(manifestPath);
            if (!in.is_open())
            {
                return;
            }

            const std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            const std::regex roundRegex(R"REGEX(android:roundIcon="(@[^"]+)")REGEX");
            std::smatch roundMatch;
            if (std::regex_search(content, roundMatch, roundRegex))
            {
                if (roundMatch.size() < 2)
                {
                    return;
                }

                const std::string currentRef = roundMatch[1].str();
                if (resourceExistsForRef(resRoot, currentRef))
                {
                    return;
                }

                const std::string fallbackRef = resourceExistsForRef(resRoot, "@mipmap/ic_launcher")
                                                    ? "@mipmap/ic_launcher"
                                                    : desiredRef;
                const std::string patched =
                    content.substr(0, static_cast<std::size_t>(roundMatch.position(1))) +
                    fallbackRef +
                    content.substr(static_cast<std::size_t>(roundMatch.position(1) + roundMatch.length(1)));

                std::ofstream out(manifestPath, std::ios::trunc);
                if (out.is_open())
                {
                    out << patched;
                    ctx.warn("Missing round icon resource ", currentRef, ", using ", fallbackRef);
                }
                return;
            }

            const std::regex appTagRegex(R"REGEX(<application\b[^>]*>)REGEX");
            std::smatch appMatch;
            if (!std::regex_search(content, appMatch, appTagRegex))
            {
                return;
            }

            const std::string appTag = appMatch[0].str();
            std::string patchedTag;
            if (appTag.size() >= 2 && appTag[appTag.size() - 2] == '/')
            {
                patchedTag = appTag.substr(0, appTag.size() - 2) +
                             "\n      android:roundIcon=\"" + desiredRef + "\"/>";
            }
            else
            {
                patchedTag = appTag.substr(0, appTag.size() - 1) +
                             "\n      android:roundIcon=\"" + desiredRef + "\">";
            }

            const std::string patched =
                content.substr(0, static_cast<std::size_t>(appMatch.position(0))) +
                patchedTag +
                content.substr(static_cast<std::size_t>(appMatch.position(0) + appMatch.length(0)));

            std::ofstream out(manifestPath, std::ios::trunc);
            if (out.is_open())
            {
                out << patched;
            }
        }

        bool maybeWriteManifest(
            const crosside::Context &ctx,
            const fs::path &manifestPath,
            const std::string &manifestText)
        {
            if (fs::exists(manifestPath))
            {
                std::ifstream in(manifestPath);
                if (in.is_open())
                {
                    const std::string existing((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                    if (existing == manifestText)
                    {
                        return true;
                    }
                }
            }

            std::ofstream out(manifestPath, std::ios::trunc);
            if (!out.is_open())
            {
                ctx.error("Failed write manifest: ", manifestPath.string());
                return false;
            }

            out << manifestText;
            return true;
        }

        bool ensureDebugKeystore(const crosside::Context &ctx, const AndroidToolchain &tc, const fs::path &keystorePath)
        {
            if (fs::exists(keystorePath))
            {
                return true;
            }

            const std::vector<std::string> args = {
                "-genkeypair",
                "-validity",
                "1000",
                "-dname",
                "CN=djokersoft,O=Android,C=PT",
                "-keystore",
                pathString(keystorePath),
                "-storepass",
                "14781478",
                "-keypass",
                "14781478",
                "-alias",
                "djokersoft",
                "-keyalg",
                "RSA",
            };

            auto command = crosside::io::runCommand(pathString(tc.keytool), args, {}, ctx, false);
            if (command.code != 0)
            {
                ctx.error("Failed to generate debug keystore: ", keystorePath.string());
                return false;
            }
            return true;
        }

        void removeGeneratedJavaResources(const fs::path &javaRoot)
        {
            std::error_code ec;
            if (!fs::exists(javaRoot, ec))
            {
                return;
            }

            for (const auto &entry : fs::recursive_directory_iterator(javaRoot, ec))
            {
                if (ec || !entry.is_regular_file(ec))
                {
                    continue;
                }
                const std::string fileName = entry.path().filename().string();
                if (fileName == "R.java" || startsWith(fileName, "R$"))
                {
                    fs::remove(entry.path(), ec);
                }
            }
        }

        std::vector<fs::path> collectFilesByExtension(const fs::path &root, const std::string &ext)
        {
            std::vector<fs::path> out;
            std::error_code ec;
            if (!fs::exists(root, ec))
            {
                return out;
            }

            const std::string wanted = lower(ext);
            for (const auto &entry : fs::recursive_directory_iterator(root, ec))
            {
                if (ec || !entry.is_regular_file(ec))
                {
                    continue;
                }
                if (lower(entry.path().extension().string()) == wanted)
                {
                    out.push_back(entry.path());
                }
            }

            std::sort(out.begin(), out.end());
            return out;
        }

        bool compileJavaSources(
            const crosside::Context &ctx,
            const AndroidToolchain &tc,
            const fs::path &javaRoot,
            const fs::path &javaOut,
            const fs::path &platformJar)
        {
            const auto javaFiles = collectFilesByExtension(javaRoot, ".java");
            if (javaFiles.empty())
            {
                ctx.log("No Java sources found, skipping javac");
                return true;
            }

            std::error_code ec;
            fs::remove_all(javaOut, ec);
            if (!crosside::io::ensureDir(javaOut))
            {
                ctx.error("Failed create java output dir: ", javaOut.string());
                return false;
            }

#ifdef _WIN32
            constexpr char kPathSep = ';';
#else
            constexpr char kPathSep = ':';
#endif

            const std::string classpath = pathString(platformJar) + kPathSep + pathString(javaOut);
            const std::string sourcepath = pathString(javaRoot) + kPathSep + pathString(javaRoot / "org") + kPathSep + pathString(javaOut);

            std::vector<std::string> args = {
                "-nowarn",
                "-Xlint:none",
                "-J-Xmx2048m",
                "-Xlint:unchecked",
                "-source",
                "1.8",
                "-target",
                "1.8",
                "-d",
                pathString(javaOut),
                "-classpath",
                classpath,
                "-sourcepath",
                sourcepath,
            };

            for (const auto &file : javaFiles)
            {
                args.push_back(pathString(file));
            }

            auto command = crosside::io::runCommand(pathString(tc.javac), args, {}, ctx, false);
            if (command.code != 0)
            {
                ctx.error("Java compilation failed");
                return false;
            }

            return true;
        }

        bool buildDex(
            const crosside::Context &ctx,
            const AndroidToolchain &tc,
            const fs::path &javaOut,
            const fs::path &dexDir,
            const fs::path &platformJar)
        {
            std::error_code ec;
            fs::remove_all(dexDir, ec);
            if (!crosside::io::ensureDir(dexDir))
            {
                ctx.error("Failed create dex dir: ", dexDir.string());
                return false;
            }

            const auto classes = collectFilesByExtension(javaOut, ".class");
            if (classes.empty())
            {
                ctx.log("No .class files found, skipping dex");
                return true;
            }

            bool d8Ok = false;
            if (!tc.d8.empty() && fs::exists(tc.d8))
            {
                std::vector<std::string> args = {
                    "--release",
                    "--output",
                    pathString(dexDir),
                    "--lib",
                    pathString(platformJar),
                };
                for (const auto &cls : classes)
                {
                    args.push_back(pathString(cls));
                }

                auto d8 = crosside::io::runCommand(pathString(tc.d8), args, {}, ctx, false);
                d8Ok = d8.code == 0;
                if (!d8Ok)
                {
                    ctx.warn("d8 failed, trying dx fallback");
                }
            }

            if (d8Ok)
            {
                return true;
            }

            if (tc.dx.empty() || !fs::exists(tc.dx))
            {
                ctx.error("dx fallback not found and d8 failed");
                return false;
            }

            std::vector<std::string> dxArgs = {
                "--dex",
                "--output=" + pathString(dexDir / "classes.dex"),
            };
            for (const auto &cls : classes)
            {
                dxArgs.push_back(pathString(cls));
            }

            auto dx = crosside::io::runCommand(pathString(tc.dx), dxArgs, {}, ctx, false);
            if (dx.code != 0)
            {
                ctx.error("dx failed while creating classes.dex");
                return false;
            }
            return true;
        }

        std::size_t copyDirectoryTree(const fs::path &src, const fs::path &dst)
        {
            std::error_code ec;
            if (!fs::exists(src, ec) || !fs::is_directory(src, ec))
            {
                return 0;
            }

            std::size_t copied = 0;
            for (const auto &entry : fs::recursive_directory_iterator(src, ec))
            {
                if (ec)
                {
                    break;
                }

                const fs::path rel = fs::relative(entry.path(), src, ec);
                if (ec)
                {
                    continue;
                }

                const fs::path outPath = dst / rel;
                if (entry.is_directory(ec))
                {
                    crosside::io::ensureDir(outPath);
                    continue;
                }
                if (!entry.is_regular_file(ec))
                {
                    continue;
                }

                crosside::io::ensureDir(outPath.parent_path());
                fs::copy_file(entry.path(), outPath, fs::copy_options::overwrite_existing, ec);
                if (!ec)
                {
                    ++copied;
                }
            }

            return copied;
        }

        bool copyProjectJavaSources(
            const crosside::Context &ctx,
            const crosside::model::ProjectSpec &project,
            const fs::path &javaRoot)
        {
            if (project.androidJavaSources.empty())
            {
                return true;
            }

            for (const fs::path &input : project.androidJavaSources)
            {
                std::error_code ec;
                if (input.empty() || !fs::exists(input, ec))
                {
                    ctx.warn("Android Java source path not found: ", input.string());
                    continue;
                }

                if (fs::is_directory(input, ec))
                {
                    const std::size_t count = copyDirectoryTree(input, javaRoot);
                    if (count > 0)
                    {
                        ctx.log("copy java dir ", input.string(), " -> ", javaRoot.string(), " (", count, " files)");
                    }
                    continue;
                }

                if (!fs::is_regular_file(input, ec))
                {
                    continue;
                }

                fs::path target = javaRoot / input.filename();
                ec.clear();
                const fs::path rel = fs::relative(input, project.root, ec);
                if (!ec && !rel.empty() && *rel.begin() != "..")
                {
                    target = javaRoot / rel;
                }

                if (!crosside::io::ensureDir(target.parent_path()))
                {
                    ctx.error("Failed create Java target dir: ", target.parent_path().string());
                    return false;
                }

                fs::copy_file(input, target, fs::copy_options::overwrite_existing, ec);
                if (ec)
                {
                    ctx.error("Failed copy Java file ", input.string(), " -> ", target.string(), " : ", ec.message());
                    return false;
                }
                ctx.log("copy java file ", input.string(), " -> ", target.string());
            }

            return true;
        }

        bool copyLauncherIconSet(
            const crosside::Context &ctx,
            const fs::path &resRoot,
            const std::string &outputFileName,
            const std::string &label,
            const fs::path &singleIcon,
            const std::unordered_map<std::string, fs::path> &iconMapRaw,
            const fs::path &fallbackIcon,
            bool &copiedAny)
        {
            const std::vector<std::string> buckets = {
                "mipmap-mdpi",
                "mipmap-hdpi",
                "mipmap-xhdpi",
                "mipmap-xxhdpi",
                "mipmap-xxxhdpi",
            };

            std::unordered_map<std::string, fs::path> iconByBucket;
            for (const auto &[rawKey, rawPath] : iconMapRaw)
            {
                const std::string bucket = normalizeIconBucketKey(rawKey);
                if (bucket.empty())
                {
                    ctx.warn("Unknown Android icon bucket key: ", rawKey);
                    continue;
                }
                if (rawPath.empty() || !fs::exists(rawPath))
                {
                    ctx.warn(label, " file not found for ", rawKey, ": ", rawPath.string());
                    continue;
                }
                iconByBucket[bucket] = rawPath;
            }

            const bool hasSingleIcon = !singleIcon.empty() && fs::exists(singleIcon);
            if (!singleIcon.empty() && !hasSingleIcon)
            {
                ctx.warn(label, " file not found: ", singleIcon.string());
            }

            const bool hasFallbackIcon = !fallbackIcon.empty() && fs::exists(fallbackIcon);
            for (const auto &bucket : buckets)
            {
                fs::path source;
                auto it = iconByBucket.find(bucket);
                if (it != iconByBucket.end())
                {
                    source = it->second;
                }
                else if (hasSingleIcon)
                {
                    source = singleIcon;
                }
                else if (hasFallbackIcon)
                {
                    source = fallbackIcon;
                }

                if (source.empty())
                {
                    continue;
                }

                const fs::path bucketDir = resRoot / bucket;
                if (!crosside::io::ensureDir(bucketDir))
                {
                    ctx.error("Failed create Android icon folder: ", bucketDir.string());
                    return false;
                }

                const fs::path iconDest = bucketDir / outputFileName;
                std::error_code ec;
                fs::copy_file(source, iconDest, fs::copy_options::overwrite_existing, ec);
                if (ec)
                {
                    ctx.error("Failed copy Android icon ", source.string(), " -> ", iconDest.string(), " : ", ec.message());
                    return false;
                }
                copiedAny = true;
            }

            return true;
        }

        bool writeSmallTextFile(const fs::path &path, const std::string &text)
        {
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            if (!out.is_open())
            {
                return false;
            }
            out << text;
            return out.good();
        }

        std::string buildAdaptiveIconXml(const std::string &backgroundRef, const std::string &foregroundRef, const std::string &monochromeRef)
        {
            std::string xml;
            xml += "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
            xml += "<adaptive-icon xmlns:android=\"http://schemas.android.com/apk/res/android\">\n";
            xml += "    <background android:drawable=\"" + backgroundRef + "\"/>\n";
            xml += "    <foreground android:drawable=\"" + foregroundRef + "\"/>\n";
            if (!monochromeRef.empty())
            {
                xml += "    <monochrome android:drawable=\"" + monochromeRef + "\"/>\n";
            }
            xml += "</adaptive-icon>\n";
            return xml;
        }

        bool ensureAdaptiveLauncherIcons(
            const crosside::Context &ctx,
            const crosside::model::ProjectSpec &project,
            const fs::path &resRoot,
            bool &createdRoundResource)
        {
            if (project.androidAdaptiveForeground.empty())
            {
                return true;
            }

            if (!fs::exists(project.androidAdaptiveForeground))
            {
                ctx.error("Android adaptive icon foreground not found: ", project.androidAdaptiveForeground.string());
                return false;
            }

            const fs::path drawableRoot = resRoot / "drawable";
            const fs::path adaptiveRoot = resRoot / "mipmap-anydpi-v26";
            if (!crosside::io::ensureDir(drawableRoot) || !crosside::io::ensureDir(adaptiveRoot))
            {
                ctx.error("Failed create Android adaptive icon folders under: ", resRoot.string());
                return false;
            }

            std::error_code ec;
            fs::copy_file(
                project.androidAdaptiveForeground,
                drawableRoot / "ic_launcher_foreground.png",
                fs::copy_options::overwrite_existing,
                ec);
            if (ec)
            {
                ctx.error("Failed copy adaptive foreground icon: ", project.androidAdaptiveForeground.string());
                return false;
            }

            std::string monochromeRef;
            if (!project.androidAdaptiveMonochrome.empty())
            {
                if (!fs::exists(project.androidAdaptiveMonochrome))
                {
                    ctx.error("Android adaptive monochrome icon not found: ", project.androidAdaptiveMonochrome.string());
                    return false;
                }
                ec.clear();
                fs::copy_file(
                    project.androidAdaptiveMonochrome,
                    drawableRoot / "ic_launcher_monochrome.png",
                    fs::copy_options::overwrite_existing,
                    ec);
                if (ec)
                {
                    ctx.error("Failed copy adaptive monochrome icon: ", project.androidAdaptiveMonochrome.string());
                    return false;
                }
                monochromeRef = "@drawable/ic_launcher_monochrome";
            }

            std::string backgroundRef = "@drawable/ic_launcher_background";
            if (!project.androidAdaptiveBackgroundImage.empty())
            {
                if (!fs::exists(project.androidAdaptiveBackgroundImage))
                {
                    ctx.error("Android adaptive background image not found: ", project.androidAdaptiveBackgroundImage.string());
                    return false;
                }
                ec.clear();
                fs::copy_file(
                    project.androidAdaptiveBackgroundImage,
                    drawableRoot / "ic_launcher_background.png",
                    fs::copy_options::overwrite_existing,
                    ec);
                if (ec)
                {
                    ctx.error("Failed copy adaptive background image: ", project.androidAdaptiveBackgroundImage.string());
                    return false;
                }
            }
            else
            {
                const std::string color = project.androidAdaptiveBackgroundColor.empty()
                                              ? "#FFFFFF"
                                              : project.androidAdaptiveBackgroundColor;
                const std::string backgroundXml =
                    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                    "<shape xmlns:android=\"http://schemas.android.com/apk/res/android\" android:shape=\"rectangle\">\n"
                    "    <solid android:color=\"" +
                    color +
                    "\"/>\n"
                    "</shape>\n";
                if (!writeSmallTextFile(drawableRoot / "ic_launcher_background.xml", backgroundXml))
                {
                    ctx.error("Failed write adaptive background xml");
                    return false;
                }
            }

            const std::string adaptiveXml = buildAdaptiveIconXml(
                backgroundRef,
                "@drawable/ic_launcher_foreground",
                monochromeRef);
            if (!writeSmallTextFile(adaptiveRoot / "ic_launcher.xml", adaptiveXml))
            {
                ctx.error("Failed write adaptive launcher xml");
                return false;
            }

            if (project.androidAdaptiveRound)
            {
                if (!writeSmallTextFile(adaptiveRoot / "ic_launcher_round.xml", adaptiveXml))
                {
                    ctx.error("Failed write adaptive round launcher xml");
                    return false;
                }
                createdRoundResource = true;
            }

            return true;
        }

        bool ensureProjectLauncherIcons(
            const crosside::Context &ctx,
            const fs::path &repoRoot,
            const crosside::model::ProjectSpec &project,
            const fs::path &resRoot)
        {
            const fs::path fallbackIcon = repoRoot / "Templates" / "Android" / "Res" / "mipmap-hdpi" / "ic_launcher.png";

            bool copiedMain = false;
            if (!copyLauncherIconSet(
                    ctx,
                    resRoot,
                    "ic_launcher.png",
                    "Android ICON",
                    project.androidIcon,
                    project.androidIcons,
                    fallbackIcon,
                    copiedMain))
            {
                return false;
            }

            fs::path roundSingle = project.androidRoundIcon;
            if (roundSingle.empty())
            {
                roundSingle = project.androidIcon;
            }
            std::unordered_map<std::string, fs::path> roundMap = project.androidRoundIcons;
            if (roundMap.empty())
            {
                roundMap = project.androidIcons;
            }

            bool copiedRound = false;
            if (!copyLauncherIconSet(
                    ctx,
                    resRoot,
                    "ic_launcher_round.png",
                    "Android ROUND_ICON",
                    roundSingle,
                    roundMap,
                    fallbackIcon,
                    copiedRound))
            {
                return false;
            }

            bool adaptiveRoundCreated = false;
            if (!ensureAdaptiveLauncherIcons(ctx, project, resRoot, adaptiveRoundCreated))
            {
                return false;
            }

            if (!copiedMain)
            {
                ctx.warn("No launcher icon copied. Configure Android.ICON or Android.ICONS in project main.mk");
            }
            if (!copiedRound && !adaptiveRoundCreated)
            {
                ctx.warn("No round launcher icon copied. Configure Android.ROUND_ICON/ROUND_ICONS or ADAPTIVE_ICON");
            }

            return true;
        }

        std::vector<std::string> collectRelativeFiles(const fs::path &root)
        {
            std::vector<std::string> out;
            std::error_code ec;
            if (!fs::exists(root, ec))
            {
                return out;
            }

            for (const auto &entry : fs::recursive_directory_iterator(root, ec))
            {
                if (ec || !entry.is_regular_file(ec))
                {
                    continue;
                }

                fs::path rel = fs::relative(entry.path(), root, ec);
                if (ec)
                {
                    continue;
                }

                out.push_back(rel.generic_string());
            }

            std::sort(out.begin(), out.end());
            return out;
        }

        bool addFilesToApk(
            const crosside::Context &ctx,
            const AndroidToolchain &tc,
            const fs::path &apkPath,
            const fs::path &stageRoot,
            const std::vector<std::string> &files)
        {
            if (files.empty())
            {
                return true;
            }

            constexpr std::size_t kChunkSize = 180;
            for (std::size_t i = 0; i < files.size(); i += kChunkSize)
            {
                const std::size_t end = std::min(files.size(), i + kChunkSize);

                std::vector<std::string> args;
                args.push_back("add");
                args.push_back(pathString(apkPath));
                for (std::size_t j = i; j < end; ++j)
                {
                    args.push_back(files[j]);
                }

                auto command = crosside::io::runCommand(pathString(tc.aapt), args, stageRoot, ctx, false);
                if (command.code != 0)
                {
                    ctx.error("aapt add failed while adding staged files to apk");
                    return false;
                }
            }
            return true;
        }

        bool ensureAndroidProjectLayout(
            const crosside::Context &ctx,
            const fs::path &repoRoot,
            const crosside::model::ProjectSpec &project,
            const std::string &packageName,
            const std::string &activity,
            fs::path &appRoot,
            fs::path &resRoot,
            fs::path &javaRoot,
            fs::path &tmpRoot,
            fs::path &javaOut,
            fs::path &dexRoot,
            fs::path &manifestPath)
        {
            appRoot = project.root / "Android" / project.name;
            resRoot = appRoot / "res";
            javaRoot = appRoot / "java";
            tmpRoot = appRoot / "tmp";
            javaOut = appRoot / "out";
            dexRoot = appRoot / "dex";
            manifestPath = appRoot / "AndroidManifest.xml";

            if (!crosside::io::ensureDir(appRoot) || !crosside::io::ensureDir(resRoot) || !crosside::io::ensureDir(javaRoot) || !crosside::io::ensureDir(tmpRoot) || !crosside::io::ensureDir(javaOut) || !crosside::io::ensureDir(dexRoot))
            {
                ctx.error("Failed create Android project output folders for ", project.name);
                return false;
            }

            if (!copyProjectJavaSources(ctx, project, javaRoot))
            {
                return false;
            }

            if (!ensureProjectLauncherIcons(ctx, repoRoot, project, resRoot))
            {
                return false;
            }

            const std::string label = project.androidLabel.empty()
                                          ? (project.name.empty() ? std::string("app") : project.name)
                                          : project.androidLabel;
            const auto manifestTemplate = loadManifestTemplate(ctx, repoRoot, project, activity);
            if (!manifestTemplate.has_value())
            {
                return false;
            }

            const std::string manifestText = buildManifest(
                manifestTemplate.value(),
                packageName,
                label,
                activity,
                project.name,
                project.androidManifestVars);

            if (!maybeWriteManifest(ctx, manifestPath, manifestText))
            {
                return false;
            }

            ensureManifestIconFallback(ctx, manifestPath, resRoot);
            ensureManifestRoundIcon(ctx, manifestPath, resRoot);
            return true;
        }

        bool runAaptGenerateResources(
            const crosside::Context &ctx,
            const AndroidToolchain &tc,
            const fs::path &manifestPath,
            const fs::path &resRoot,
            const fs::path &javaRoot)
        {
            removeGeneratedJavaResources(javaRoot);

            std::vector<std::string> args = {
                "package",
                "-f",
                "-m",
                "-J",
                pathString(javaRoot),
                "-M",
                pathString(manifestPath),
                "-S",
                pathString(resRoot),
                "-I",
                pathString(tc.platformJar),
            };

            auto command = crosside::io::runCommand(pathString(tc.aapt), args, {}, ctx, false);
            if (command.code != 0)
            {
                ctx.error("aapt resource generation failed");
                return false;
            }

            return true;
        }

        bool createBaseApk(
            const crosside::Context &ctx,
            const AndroidToolchain &tc,
            const fs::path &manifestPath,
            const fs::path &resRoot,
            const fs::path &apkPath)
        {
            std::vector<std::string> args = {
                "package",
                "-f",
                "-m",
                "-F",
                pathString(apkPath),
                "-M",
                pathString(manifestPath),
                "-S",
                pathString(resRoot),
                "-I",
                pathString(tc.platformJar),
            };

            auto command = crosside::io::runCommand(pathString(tc.aapt), args, {}, ctx, false);
            if (command.code != 0)
            {
                ctx.error("aapt base apk packaging failed");
                return false;
            }
            return true;
        }

        bool stageNativeLibsAndAssets(
            const crosside::Context &ctx,
            const crosside::model::ProjectSpec &project,
            const fs::path &stageRoot,
            const fs::path &dexRoot)
        {
            std::error_code ec;
            fs::remove_all(stageRoot, ec);
            if (!crosside::io::ensureDir(stageRoot))
            {
                ctx.error("Failed create APK stage dir: ", stageRoot.string());
                return false;
            }

            for (const std::string &abiName : {std::string("armeabi-v7a"), std::string("arm64-v8a")})
            {
                const fs::path libFile = project.root / "Android" / abiName / ("lib" + project.name + ".so");
                if (!fs::exists(libFile))
                {
                    continue;
                }

                const fs::path dst = stageRoot / "lib" / abiName / ("lib" + project.name + ".so");
                crosside::io::ensureDir(dst.parent_path());
                fs::copy_file(libFile, dst, fs::copy_options::overwrite_existing, ec);
                if (ec)
                {
                    ctx.error("Failed stage native library: ", libFile.string());
                    return false;
                }
            }

            const std::vector<std::pair<std::string, std::string>> assets = {
                {"scripts", "assets/scripts"},
                {"assets", "assets/assets"},
                {"resources", "assets/resources"},
                {"data", "assets/data"},
                {"media", "assets/media"},
            };

            for (const auto &[hostName, apkName] : assets)
            {
                const fs::path src = project.root / hostName;
                const fs::path dst = stageRoot / apkName;
                const std::size_t count = copyDirectoryTree(src, dst);
                if (count > 0)
                {
                    ctx.log("pack ", hostName, " -> ", apkName, " (", count, " files)");
                }
            }

            for (const auto &dex : collectFilesByExtension(dexRoot, ".dex"))
            {
                const fs::path dst = stageRoot / dex.filename();
                fs::copy_file(dex, dst, fs::copy_options::overwrite_existing, ec);
                if (ec)
                {
                    ctx.error("Failed stage dex file: ", dex.string());
                    return false;
                }
            }

            return true;
        }

        bool signApk(
            const crosside::Context &ctx,
            const AndroidToolchain &tc,
            const fs::path &unsignedApk,
            const fs::path &signedApk,
            const fs::path &keystore)
        {
            std::vector<std::string> args = {
                "sign",
                "--ks",
                pathString(keystore),
                "--ks-key-alias",
                "djokersoft",
                "--ks-pass",
                "pass:14781478",
                "--in",
                pathString(unsignedApk),
                "--out",
                pathString(signedApk),
            };

            auto sign = crosside::io::runCommand(pathString(tc.apksigner), args, {}, ctx, false);
            if (sign.code != 0)
            {
                ctx.error("apksigner failed: ", signedApk.string());
                return false;
            }
            return true;
        }

        bool adbInstall(
            const crosside::Context &ctx,
            const AndroidToolchain &tc,
            const fs::path &signedApk,
            const std::string &packageName)
        {
            auto install = crosside::io::runCommand(pathString(tc.adb), {"install", "-r", pathString(signedApk)}, {}, ctx, false);
            if (install.code == 0)
            {
                return true;
            }

            crosside::io::runCommand(pathString(tc.adb), {"uninstall", packageName}, {}, ctx, false);
            install = crosside::io::runCommand(pathString(tc.adb), {"install", "-r", pathString(signedApk)}, {}, ctx, false);
            return install.code == 0;
        }

        bool adbRun(
            const crosside::Context &ctx,
            const AndroidToolchain &tc,
            const std::string &packageName,
            const std::string &activity)
        {
            crosside::io::runCommand(pathString(tc.adb), {"shell", "am", "force-stop", packageName}, {}, ctx, false);

            const std::string component = packageName + "/" + activity;
            auto run = crosside::io::runCommand(pathString(tc.adb), {"shell", "am", "start", "-n", component}, {}, ctx, false);
            return run.code == 0;
        }

        bool buildAndroidProjectApk(
            const crosside::Context &ctx,
            const fs::path &repoRoot,
            const crosside::model::ProjectSpec &project,
            const AndroidToolchain &tc,
            bool runAfter)
        {
            std::string packageName = sanitizeAndroidPackage(project.androidPackage);
            std::string activity = normalizeActivity(packageName, project.androidActivity);

            fs::path appRoot;
            fs::path resRoot;
            fs::path javaRoot;
            fs::path tmpRoot;
            fs::path javaOut;
            fs::path dexRoot;
            fs::path manifestPath;

            if (!ensureAndroidProjectLayout(ctx, repoRoot, project, packageName, activity, appRoot, resRoot, javaRoot, tmpRoot, javaOut, dexRoot, manifestPath))
            {
                return false;
            }

            if (!runAaptGenerateResources(ctx, tc, manifestPath, resRoot, javaRoot))
            {
                return false;
            }
            if (!compileJavaSources(ctx, tc, javaRoot, javaOut, tc.platformJar))
            {
                return false;
            }
            if (!buildDex(ctx, tc, javaOut, dexRoot, tc.platformJar))
            {
                return false;
            }

            const fs::path unalignedApk = tmpRoot / (project.name + ".unaligned.apk");
            if (!createBaseApk(ctx, tc, manifestPath, resRoot, unalignedApk))
            {
                return false;
            }

            const fs::path stageRoot = tmpRoot / "apk_stage";
            if (!stageNativeLibsAndAssets(ctx, project, stageRoot, dexRoot))
            {
                return false;
            }

            const std::vector<std::string> stagedFiles = collectRelativeFiles(stageRoot);
            if (!addFilesToApk(ctx, tc, unalignedApk, stageRoot, stagedFiles))
            {
                return false;
            }

            const fs::path debugKey = appRoot / (project.name + ".key");
            if (!ensureDebugKeystore(ctx, tc, debugKey))
            {
                return false;
            }

            const fs::path signedApk = appRoot / (project.name + ".signed.apk");
            if (!signApk(ctx, tc, unalignedApk, signedApk, debugKey))
            {
                return false;
            }

            if (runAfter)
            {
                if (!adbInstall(ctx, tc, signedApk, packageName))
                {
                    ctx.error("adb install failed: ", signedApk.string());
                    return false;
                }

                if (!adbRun(ctx, tc, packageName, activity))
                {
                    ctx.error("adb run failed for component: ", packageName, "/", activity);
                    return false;
                }
            }

            return true;
        }

        bool buildModuleForAbi(
            const crosside::Context &ctx,
            const fs::path &repoRoot,
            const AndroidToolchain &tc,
            const crosside::model::ModuleSpec &module,
            const crosside::model::ModuleMap &modules,
            const AbiInfo &abi,
            bool fullBuild)
        {
            const fs::path outDir = module.dir / "Android" / abi.name;
            const fs::path outLib = outDir / ("lib" + module.name + (module.staticLib ? ".a" : ".so"));

            const auto sources = collectModuleSourcesAndroid(module, ctx);
            if (sources.empty())
            {
                std::error_code ec;
                const bool hasOutLib = fs::exists(outLib, ec) && fs::is_regular_file(outLib, ec);

                if (fullBuild || !hasOutLib)
                {
                    if (tryBuildModuleWithNdkBuild(ctx, tc, module, abi, outDir, outLib))
                    {
                        return true;
                    }
                }

                if (fs::exists(outLib, ec) && fs::is_regular_file(outLib, ec))
                {
                    if (fullBuild)
                    {
                        ctx.warn("Full build requested but module ", module.name, " has no Android sources; using prebuilt ", outLib.string());
                    }
                    else
                    {
                        ctx.log("Use prebuilt Android module ", module.name, ": ", outLib.string());
                    }
                    return true;
                }

                const auto prebuilt = findPrebuiltModuleOutputAndroid(outDir, module.name, module.staticLib);
                if (!prebuilt.has_value())
                {
                    ctx.warn("No Android sources for module ", module.name, " and no prebuilt output at ", outLib.string());
                    return false;
                }

                if (!crosside::io::ensureDir(outDir))
                {
                    ctx.error("Failed create module Android output dir: ", outDir.string());
                    return false;
                }

                fs::copy_file(prebuilt.value(), outLib, fs::copy_options::overwrite_existing, ec);
                if (ec)
                {
                    ctx.error("Failed alias prebuilt module output ", prebuilt->string(), " -> ", outLib.string(), " : ", ec.message());
                    return false;
                }

                ctx.log("Use prebuilt Android module ", module.name, ": ", prebuilt->string(), " -> ", outLib.string());
                return true;
            }

            std::vector<std::string> ccFlags = module.main.ccArgs;
            std::vector<std::string> cppFlags = module.main.cppArgs;
            std::vector<std::string> ldFlags = module.main.ldArgs;

            appendAll(ccFlags, module.android.ccArgs);
            appendAll(cppFlags, module.android.cppArgs);
            appendAll(ldFlags, module.android.ldArgs);

            collectModuleIncludeFlagsAndroid(module, module.android, ccFlags, cppFlags);
            appendModuleDependencyFlags(module, modules, abi, ccFlags, cppFlags, ldFlags, ctx);

            const fs::path objRoot = module.dir / "obj" / "Android" / module.name / abi.name;
            CompileResult compiled;
            if (!compileAndroidSources(ctx, tc, module.dir, objRoot, sources, ccFlags, cppFlags, abi, fullBuild, compiled))
            {
                return false;
            }

            if (!crosside::io::ensureDir(outDir))
            {
                ctx.error("Failed create module Android output dir: ", outDir.string());
                return false;
            }

            if (module.staticLib)
            {
                return archiveAndroidStatic(ctx, tc, outLib, compiled.objects);
            }

            return linkAndroidShared(ctx, repoRoot, tc, abi, module.name, compiled.objects, ldFlags, compiled.hasCpp, outLib);
        }

        bool buildProjectForAbi(
            const crosside::Context &ctx,
            const fs::path &repoRoot,
            const AndroidToolchain &tc,
            const crosside::model::ProjectSpec &project,
            const crosside::model::ModuleMap &modules,
            const std::vector<std::string> &activeModules,
            const AbiInfo &abi,
            bool fullBuild)
        {
            const auto sources = collectProjectSourcesAndroid(project, ctx);
            if (sources.empty())
            {
                return false;
            }

            std::vector<std::string> ccFlags = project.main.cc;
            std::vector<std::string> cppFlags = project.main.cpp;
            std::vector<std::string> ldFlags = project.main.ld;

            appendAll(ccFlags, project.android.cc);
            appendAll(cppFlags, project.android.cpp);
            appendAll(ldFlags, project.android.ld);

            for (const auto &inc : project.include)
            {
                addIncludeFlag(ccFlags, cppFlags, inc);
            }

            collectProjectModuleFlags(repoRoot, modules, activeModules, abi, ccFlags, cppFlags, ldFlags, ctx);

            appendUnique(ldFlags, "-u");
            appendUnique(ldFlags, "ANativeActivity_onCreate");

            const fs::path objRoot = project.root / "obj" / "Android" / project.name / abi.name;
            CompileResult compiled;
            if (!compileAndroidSources(ctx, tc, project.root, objRoot, sources, ccFlags, cppFlags, abi, fullBuild, compiled))
            {
                return false;
            }

            const fs::path outDir = project.root / "Android" / abi.name;
            if (!crosside::io::ensureDir(outDir))
            {
                ctx.error("Failed create project Android output dir: ", outDir.string());
                return false;
            }

            const fs::path outLib = outDir / ("lib" + project.name + ".so");
            const bool needsCppRuntime = compiled.hasCpp || !activeModules.empty();
            return linkAndroidShared(ctx, repoRoot, tc, abi, project.name, compiled.objects, ldFlags, needsCppRuntime, outLib);
        }

    } // namespace

    bool buildModuleAndroid(
        const crosside::Context &ctx,
        const fs::path &repoRoot,
        const crosside::model::ModuleSpec &module,
        const crosside::model::ModuleMap &modules,
        bool fullBuild,
        const std::vector<int> &abis)
    {
        if (!moduleSupportsAndroid(module))
        {
            ctx.log("Skip module ", module.name, " for android (unsupported by module.json)");
            return true;
        }

        const AndroidToolchain tc = resolveToolchain(repoRoot, ctx);
        if (!validateToolchainCompile(ctx, tc))
        {
            return false;
        }

        for (int abiValue : normalizeAbis(abis))
        {
            auto abi = abiInfoFromValue(abiValue);
            if (!abi.has_value())
            {
                continue;
            }
            ctx.log("Build module ", module.name, " for ", abi->name);
            if (!buildModuleForAbi(ctx, repoRoot, tc, module, modules, abi.value(), fullBuild))
            {
                return false;
            }
        }

        return true;
    }

    bool buildProjectAndroid(
        const crosside::Context &ctx,
        const fs::path &repoRoot,
        const crosside::model::ProjectSpec &project,
        const crosside::model::ModuleMap &modules,
        const std::vector<std::string> &activeModules,
        bool fullBuild,
        bool runAfter,
        bool autoBuildModules,
        const std::vector<int> &abis)
    {
        const AndroidToolchain tc = resolveToolchain(repoRoot, ctx);
        if (!validateToolchainCompile(ctx, tc) || !validateToolchainPackage(ctx, tc))
        {
            return false;
        }

        if (autoBuildModules)
        {
            const std::vector<std::string> allModules = crosside::model::moduleClosure(activeModules, modules, ctx);
            for (const auto &name : allModules)
            {
                auto it = modules.find(name);
                if (it == modules.end())
                {
                    ctx.warn("Missing module for auto-build: ", name);
                    continue;
                }
                if (!buildModuleAndroid(ctx, repoRoot, it->second, modules, fullBuild, abis))
                {
                    ctx.error("Failed auto-build module ", name, " for android");
                    return false;
                }
            }
        }

        for (int abiValue : normalizeAbis(abis))
        {
            auto abi = abiInfoFromValue(abiValue);
            if (!abi.has_value())
            {
                continue;
            }
            ctx.log("Build app ", project.name, " native lib for ", abi->name);
            if (!buildProjectForAbi(ctx, repoRoot, tc, project, modules, activeModules, abi.value(), fullBuild))
            {
                return false;
            }
        }

        return buildAndroidProjectApk(ctx, repoRoot, project, tc, runAfter);
    }

} // namespace crosside::build
