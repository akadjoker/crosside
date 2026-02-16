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
- Single-file app build is supported (`.c/.cpp/...`) without a `main.mk` project file.
- You can set default modules for single-file builds in `config.json` with `Configuration.SingleFileModules`.
- You can set a default Web shell template in `config.json` with `Configuration.Web.SHELL`
  (example: `Templates/Web/shell.html`).
- Android project config supports launcher icons via `Android.ICON` / `Android.ICONS`,
  round icons via `Android.ROUND_ICON` / `Android.ROUND_ICONS`,
  adaptive icons via `Android.ADAPTIVE_ICON`,
  plus manifest modes (`Android.MANIFEST_MODE`) and Java source copy (`Android.JAVA_SOURCES`/`JAVA`).
- Legacy Python tools are still available while migration to C++ continues.

## Module authoring

- Module creation guide: `builder/docs/creating_modules.md`

## Tests

From `builder/`:

```bash
make test
```

Current test suite validates process execution, path resolution, and web helper behavior.

adb pair 192.168.1.92:34011
