# Creating New Modules

This guide explains how to add a new module for the C++ `builder`.

## Fast start (scaffold command)

Generate a ready-to-edit module with one command:

```bash
./bin/builder module init mymodule
```

Useful options:
- `--author "Your Name"`
- `--shared` (generate module with `"static": false`)
- `--force` (overwrite scaffold files if they already exist)

## Folder layout

Create a folder inside `modules/`:

```text
modules/
  mymodule/
    module.json
    src/
      mymodule.c
    include/
      mymodule.h
```

Default module file path:
- `modules/<module-name>/module.json`

## Minimal `module.json`

Copy this as a starting point:

```json
{
  "module": "mymodule",
  "about": "short description",
  "author": "your-name",
  "version": "1.0.0",
  "depends": [],
  "static": true,
  "system": ["linux", "windows", "android", "emscripten"],
  "src": [
    "src/mymodule.c"
  ],
  "include": [
    "include"
  ],
  "CPP_ARGS": "",
  "CC_ARGS": "",
  "LD_ARGS": "",
  "plataforms": {
    "linux": {
      "CPP_ARGS": "",
      "CC_ARGS": "",
      "LD_ARGS": "",
      "src": [],
      "include": []
    },
    "windows": {
      "CPP_ARGS": "",
      "CC_ARGS": "",
      "LD_ARGS": "",
      "src": [],
      "include": []
    },
    "android": {
      "CPP_ARGS": "",
      "CC_ARGS": "",
      "LD_ARGS": "",
      "src": [],
      "include": []
    },
    "emscripten": {
      "template": "",
      "CPP_ARGS": "",
      "CC_ARGS": "",
      "LD_ARGS": "",
      "src": [],
      "include": []
    }
  }
}
```

## Important field notes

- Use `plataforms` (this exact key). The loader currently expects this spelling.
- Platform keys inside `plataforms`: `linux`, `windows`, `android`, `emscripten`.
- `system` controls target support checks for Android/Web. Use `android` and `emscripten` there.
- `depends` must use module names (the value of each dependency module `module` field).
- `src` and `include` paths are relative to the module folder.
- `static: true` produces `lib<name>.a`; `static: false` produces shared output (`.so` for current pipelines).
- `CPP_ARGS`, `CC_ARGS`, and `LD_ARGS` can be a single string or an array of strings.

## How includes are resolved

The builder automatically adds:
- `<module>/src`
- `<module>/include`
- platform include folder (`<module>/include/linux`, `<module>/include/android`, `<module>/include/web`)

And then also includes what you define in:
- top-level `include`
- per-platform `plataforms.<target>.include`

## How sources are resolved

Build source list is:
- top-level `src`
- plus per-platform `plataforms.<target>.src`

Only compilable extensions are used:
- `.c`, `.cc`, `.cpp`, `.cxx`, `.mm`, `.xpp`

## Build examples

From `builder/`:

```bash
# Desktop (host platform)
./bin/builder build module mymodule desktop --mode debug

# Android (both ABIs)
./bin/builder build module mymodule android --abis arm7,arm64

# Web (Emscripten)
./bin/builder build module mymodule web
```

Use a custom module file path:

```bash
./bin/builder build module mymodule web --module-file modules/mymodule/module.json
```

## Dependency linking behavior

For each dependency in `depends`, builder:
- builds in dependency order (unless `--no-deps` is used)
- injects dependency include paths
- adds dependency output library (`-l<dependency-name>`)
- appends dependency linker flags

## Common pitfalls

- Typo in `plataforms` key.
- Using `web` instead of `emscripten` inside `plataforms`.
- Forgetting `-fexceptions` when C++ code uses `try/catch`.
- Missing dependency module in `depends`.
- Header path not listed in top-level or platform `include`.
