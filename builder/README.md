# builder (C++ edition)

CLI build tool reimplementation of `crosside_cli.py` in modern C++.

## Goals
- Single native binary
- No Qt / no Python runtime needed for CLI
- Modular code by domain/platform

## Build
```bash
cd builder
make debug
# or
make release
```

Binary output:
- `builder/bin/builder`

## Commands
```bash
./bin/builder list all
./bin/builder clean bugame web --dry-run
./bin/builder build module miniz desktop --mode debug
./bin/builder build module miniz desktop --with-deps
./bin/builder build projects/sdl/tutorial_2.c desktop
./bin/builder module init mymodule --author "Luis Santos"
./bin/builder build bugame desktop
./bin/builder build bugame android --build-modules --abis arm7,arm64
./bin/builder build bugame web --run --detach --port 8080
./bin/builder serve projects/bugame/Web/main.html --port 8080
./bin/builder serve projects/bugame/Web/main.html --port 8080 --detach
make test
```

`--detach` behavior:
- `build ... desktop --run --detach`: starts app without blocking CLI
- `build ... web --run --detach`: starts background HTTP server + opens browser
- `serve ... --detach`: starts background HTTP server and returns immediately

## Creating modules

Step-by-step guide:
- `docs/creating_modules.md`

Quick scaffold command:
- `./bin/builder module init mymodule`
- `./bin/builder module init mymodule --shared`

## Tests (gtest)
- Test sources: `tests/`
- Build + run: `make test`
- Vendored source: `third_party/googletest`
- If missing, clone:
  - `git clone --depth 1 https://github.com/google/googletest.git third_party/googletest`

## Current status
- `list`: implemented
- `clean`: implemented
- `build desktop`: implemented for modules and apps
- `build android`: implemented in C++ (module/app, APK packaging, signing, install/run)
- `build web`: implemented in C++ (module/app, shell template, preload assets, built-in multithread socket server for `--run`)

## Module build behavior for apps
- App build no longer auto-builds module dependencies by default.
- Default behavior expects prebuilt module libraries (`lib<name>.a` or `lib<name>.so`) in each target output folder.
- If a module binary is missing, builder stops early with a clear error showing expected paths.
- Use `--build-modules` to restore auto-build dependencies before linking app.
- `--skip-modules` remains available and is now effectively the default behavior.

## Module build behavior for modules
- `build module ...` now builds only the requested module by default.
- Use `--with-deps` when you want to rebuild the module dependency closure.

## Single-file build mode
- You can build a single C/C++ source file without a `main.mk` project file.
- Example: `./bin/builder build projects/sdl/tutorial_2.c desktop`
- Builder creates an in-memory project using:
- `root`: source file folder
- `name`: source filename stem
- `modules`: `Configuration.SingleFileModules` from `config.json` (fallback: `Configuration.Modules`)
- You can also point app build directly to a project file/folder:
- `./bin/builder build projects/bugame/releases/chaos web`
- If the folder has `project.mk`, builder uses it; otherwise it falls back to `main.mk`.

Single-file config in `config.json`:

```json
{
  "Configuration": {
    "Modules": ["raylib", "graphics"],
    "SingleFileModules": ["raylib"],
    "Web": {
      "SHELL": "Templates/Web/shell.html"
    }
  }
}
```

`Configuration.Web.SHELL` is used as default for app builds when `main.mk` does not define `Web.SHELL`.

## Android manifest template
`main.mk` can customize manifest generation in `Android` block:

```json
"Android": {
  "PACKAGE": "com.djokersoft.game",
  "ACTIVITY": "android.app.NativeActivity",
  "MANIFEST_MODE": "native",
  "LABEL": "Bugame",
  "JAVA_SOURCES": [
    "android/java"
  ],
  "ICON": "android/icon.png",
  "ICONS": {
    "mdpi": "android/icons/ic_launcher_mdpi.png",
    "hdpi": "android/icons/ic_launcher_hdpi.png",
    "xhdpi": "android/icons/ic_launcher_xhdpi.png",
    "xxhdpi": "android/icons/ic_launcher_xxhdpi.png",
    "xxxhdpi": "android/icons/ic_launcher_xxxhdpi.png"
  },
  "ROUND_ICON": "android/icon_round.png",
  "ROUND_ICONS": {
    "mdpi": "android/icons/ic_launcher_round_mdpi.png",
    "hdpi": "android/icons/ic_launcher_round_hdpi.png"
  },
  "ADAPTIVE_ICON": {
    "FOREGROUND": "android/icons/ic_launcher_foreground.png",
    "BACKGROUND": "#1F2937",
    "MONOCHROME": "android/icons/ic_launcher_monochrome.png",
    "ROUND": true
  },
  "MANIFEST_TEMPLATE": "android/AndroidManifest.custom.xml",
  "MANIFEST_VARS": {
    "MIN_SDK": "21",
    "TARGET_SDK": "35"
  }
}
```

Supported placeholders:
- Legacy: `@apppkg@`, `@applbl@`, `@appact@`, `@appactv@`, `@appLIBNAME@`
- Extended: `@APP_PACKAGE@`, `@APP_LABEL@`, `@APP_ACTIVITY@`, `@APP_LIB_NAME@`
- Custom vars from `MANIFEST_VARS`: `@KEY@` and `${KEY}`

Android icon options:
- `ICON`: single PNG copied to all `mipmap-*` buckets as `ic_launcher.png`
- `ICONS`: optional per-density override keys (`mdpi`, `hdpi`, `xhdpi`, `xxhdpi`, `xxxhdpi`)
- `ROUND_ICON`: single PNG copied to all `mipmap-*` buckets as `ic_launcher_round.png`
- `ROUND_ICONS`: optional per-density override for round icon
- `ADAPTIVE_ICON`: optional adaptive icon block:
- `FOREGROUND`: required PNG when using adaptive icon
- `BACKGROUND`: optional `#RRGGBB` color or image path
- `MONOCHROME`: optional PNG
- `ROUND`: optional bool (write `ic_launcher_round.xml`, default `true`)

Manifest/runtime options:
- `MANIFEST_MODE`: `native` or `java` (auto-detected if omitted)
- Auto mode uses `NativeActivity` template when activity contains `NativeActivity`, otherwise Java template
- Java template default file: `Templates/Android/AndroidManifest.java.xml`
- Native template default file: `Templates/Android/AndroidManifest.xml`
- `JAVA_SOURCES` / `JAVA` / `JAVA_DIRS`: Java files/folders copied into build java root before `javac` (useful for SDL2 activities)
- `CONTENT_ROOT`: optional folder used for Android asset packaging.
  If set, builder packs from `<CONTENT_ROOT>/scripts`, `assets`, `resources`, `data`, `media`.

Release/cache options:
- `BuildCache`: optional cache key for project object files.
  Projects/releases with the same `BuildCache` reuse compile artifacts (`obj/<target>/<BuildCache>`).
- `Release`: optional JSON overlay file (relative to project root) to override fields like `Name`, `Android`, `Web`, `CONTENT_ROOT`.
- Runtime override from CLI: `--release <profile>`.
  If `<profile>` has no path/ext (for example `chaos`), builder resolves it as `releases/<profile>.json`.

Web release content:
- `Web.CONTENT_ROOT`: optional folder used for web `--preload-file`.
  If set, builder preloads from `<CONTENT_ROOT>/scripts`, `assets`, `resources`, `data`, `media`.

Default template path:
- `Templates/Android/AndroidManifest.xml`

Platform separation files:
- `src/build/desktop_builder.cpp`
- `src/build/android_builder.cpp`
- `src/build/web_builder.cpp`

Web server implementation:
- `src/io/http_server.cpp`

## JSON library
Using vendored single-header:
- `third_party/nlohmann/json.hpp`
