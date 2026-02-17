# crosside

Cross-platform C/C++ project builder and editor workspace focused on game/runtime projects.

## What this repository contains

- `builder/`: modern C++ CLI build system (`builder`) with no Python/Qt runtime dependency
- `editor.py`: legacy GUI editor workflow
- `crosside_cli.py`: legacy Python CLI workflow
- `modules/`: reusable modules (`module.json`, source, platform outputs)
- `projects/`: app/game projects (`main.mk`, sources, scripts, assets)
- `Templates/`: platform templates (Android, Web shell, etc.)

## Main features

- Multi-target builds: `desktop`, `android`, `web`
- Build by project (`app`) or module (`module`)
- Module dependency resolution from `module.json`
- Per-platform flags and include/link paths
- Android APK packaging/sign/install/run pipeline
- Web output generation with shell template and asset preload support
- Built-in static HTTP server (`serve`) for local Web runs
- Non-blocking run mode via `--detach` (IDE-friendly)
- Desktop build modes: `--mode debug|release`

## Quick start (C++ builder)

```bash
cd builder
make release

# Discover content
./bin/builder list all

# Build a module
./bin/builder build module raylib desktop --mode debug
# Build module + dependency closure (optional)
./bin/builder build module raylib desktop --with-deps

# Build a single source file (no main.mk)
./bin/builder build projects/sdl/tutorial_2.c desktop

# Create a new module scaffold
./bin/builder module init mymodule --author "Luis Santos"

# Build a desktop app and run without blocking terminal
./bin/builder build bugame desktop --run --detach

# Build Android app for both ABIs and run
./bin/builder build bugame android --run --abis arm7,arm64

# Auto-build project modules before linking (optional)
./bin/builder build bugame android --build-modules --abis arm7,arm64

# Build Web app, start server in background, open browser
./bin/builder build bugame web --run --detach --port 8080

# Clean all modules for desktop/android/web
./bin/builder clean module all desktop android web
```

## Command summary

- `builder build ...`: build module or app for one or more targets
- `builder module init ...`: create a new module scaffold
- `builder clean ...`: clean outputs
- `builder list ...`: list modules/projects
- `builder serve ...`: serve a folder or HTML file locally

## Notes

- Desktop supports `debug` and `release`.
- Android and Web builds are release-oriented by default.
- App builds expect module binaries to already exist by default.
  Use `--build-modules` when you want builder to compile module dependencies automatically.
- Module builds compile only the requested module by default.
  Use `--with-deps` to also compile its dependency closure.
- Single-file app build is supported (`.c/.cpp/...`) without a `main.mk` project file.
- App build accepts explicit project files/dirs, e.g.
  `builder build projects/bugame/releases/chaos web`
  (directory can contain `project.mk`; fallback is `main.mk`).
- Module/project `Src` supports `@` source patterns, e.g. `src/@.c` and `src/@.cpp`
  to auto-include files recursively.
- Modules can override `static/shared` per platform inside `plataforms`:
  use `"static": true|false` (or `"shared": true|false`) in `linux/windows/android/emscripten`.
  When omitted, it falls back to top-level `"static"`.
- You can set default modules for single-file builds in `config.json` with `Configuration.SingleFileModules`.
- You can set a default Web shell template in `config.json` with `Configuration.Web.SHELL`
  (example: `Templates/Web/shell.html`).
- Android project config supports launcher icons via `Android.ICON` / `Android.ICONS`,
  round icons via `Android.ROUND_ICON` / `Android.ROUND_ICONS`,
  adaptive icons via `Android.ADAPTIVE_ICON`,
  plus manifest modes (`Android.MANIFEST_MODE`) and Java source copy (`Android.JAVA_SOURCES`/`JAVA`).
- You can isolate release-only content for packaging:
  set `Android.CONTENT_ROOT` and/or `Web.CONTENT_ROOT` in `main.mk`.
  Builder then packs/preloads only `<CONTENT_ROOT>/scripts`, `<CONTENT_ROOT>/assets`, `<CONTENT_ROOT>/resources`, `<CONTENT_ROOT>/data`, `<CONTENT_ROOT>/media`.
- To avoid recompiling the same C/C++ core for each release variant, set `BuildCache` in project JSON.
  Releases that share the same `BuildCache` reuse `obj/<target>/<BuildCache>` artifacts.
- You can drive release variants from one main project file with `Release: "path/to/release.json"`.
  The release JSON overrides project fields (for example `Name`, `Android`, `Web`, `CONTENT_ROOT`, package/label).
- CLI override is supported: `builder build bugame android --release chaos`
  or `builder build bugame web --release releases/candycrash.json`.
- Legacy Python tools are still available while migration to C++ continues.

## Module authoring

- Module creation guide: `builder/docs/creating_modules.md`

## Third-party source updates (no git clone)

Use the manifest + fetch script to track and download upstream library sources as release archives (`.tar.gz`/`.zip`) without `.git` folders:

```bash
# Show tracked libraries and upstream repos
python3 tools/fetch_third_party_release.py list

# Check latest release/tag for all tracked libraries
python3 tools/fetch_third_party_release.py check

# Optional: avoid GitHub API limit
GITHUB_TOKEN=ghp_xxx python3 tools/fetch_third_party_release.py sync all

# Download latest archives for selected libs
python3 tools/fetch_third_party_release.py fetch png zlib glfw

# Download and extract latest for all tracked libs
python3 tools/fetch_third_party_release.py fetch all --extract

# Sync upstream src/include into modules/<lib> (clean old files + backup first)
python3 tools/fetch_third_party_release.py sync glfw sfml

# Sync all tracked module libs in one run
python3 tools/fetch_third_party_release.py sync all

# Dry-run sync (no file changes)
python3 tools/fetch_third_party_release.py sync glfw --dry-run


# ver primeiro
./builder/bin/builder clean module all desktop android web --dry-run

# limpar mesmo
./builder/bin/builder clean module all desktop android web

# só android arm64
./builder/bin/builder clean module all android --abis arm64

```

Manifest file: `tools/third_party_releases.json`

## Tests

From `builder/`:

```bash
make test
```

Current test suite validates process execution, path resolution, and web helper behavior.

Dependency smoke test for codec modules (`zlib/jpeg/png`, plus `webp` when available):

```bash
# desktop build + run
./tools/test_codecs.sh desktop

# android build/package check
./tools/test_codecs.sh android
```

adb pair 192.168.1.92:34011
