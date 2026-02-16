#!/usr/bin/env python3
"""
CrossIDE command-line builder (no Qt, no GUI).

Examples:
  python3 crosside_cli.py build module raylib web android
  python3 crosside_cli.py build app bugame android --run
  python3 crosside_cli.py build bugame web --run
  python3 crosside_cli.py list all
  python3 crosside_cli.py clean bugame web --dry-run
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import webbrowser
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Tuple
from urllib.parse import quote

from ide_utils.android_native_build import android_build, android_compile
from ide_utils.android_pipeline import android_compile_java_native, build_native_manifest
from ide_utils.fs_utils import createFolderTree
from ide_utils.native_build import desktop_build, desktop_compile, web_build, web_compile
from ide_utils.toolchain import ToolchainPaths, resolve_toolchain_paths


NATIVE_MANIFEST_TEMPLATE = """<?xml version="1.0" encoding="utf-8"?>
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

</manifest>"""


COMPILE_EXTS = {".c", ".cc", ".cpp", ".cxx", ".xpp", ".m", ".mm"}
DEFAULT_EMCC = "/media/projectos/projects/emsdk/upstream/emscripten/emcc"
DEFAULT_EMCPP = "/media/projectos/projects/emsdk/upstream/emscripten/em++"
DEFAULT_EMAR = "/media/projectos/projects/emsdk/upstream/emscripten/emar"

TARGET_ALIASES = {
    "desktop": "desktop",
    "linux": "desktop",
    "native": "desktop",
    "android": "android",
    "web": "web",
    "emscripten": "web",
}


@dataclass
class PlatformBlock:
    src: List[str] = field(default_factory=list)
    include: List[str] = field(default_factory=list)
    cpp_args: List[str] = field(default_factory=list)
    cc_args: List[str] = field(default_factory=list)
    ld_args: List[str] = field(default_factory=list)
    template: str = ""


@dataclass
class ModuleSpec:
    name: str
    directory: Path
    static: bool
    depends: List[str]
    system: List[str]
    main_src: List[str]
    main_include: List[str]
    main_cpp_args: List[str]
    main_cc_args: List[str]
    main_ld_args: List[str]
    desktop: PlatformBlock
    android: PlatformBlock
    web: PlatformBlock


@dataclass
class ProjectSpec:
    name: str
    root: Path
    file_path: Path
    modules: List[str]
    src: List[str]
    include: List[str]
    main_cpp: List[str]
    main_cc: List[str]
    main_ld: List[str]
    desktop_cpp: List[str]
    desktop_cc: List[str]
    desktop_ld: List[str]
    android_package: str
    android_activity: str
    android_cpp: List[str]
    android_cc: List[str]
    android_ld: List[str]
    web_shell: str
    web_cpp: List[str]
    web_cc: List[str]
    web_ld: List[str]


class CliContext:
    def __init__(self) -> None:
        self.IsDone = False

    def trace(self, *args: object) -> None:
        print("".join(str(item) for item in args), flush=True)

    def setProgress(self, value: int, code: str) -> None:
        print(f"Compile  {code}  [{value}] %", flush=True)


def append_unique(items: List[str], value: str) -> None:
    if value and value not in items:
        items.append(value)


def split_flags(value: object) -> List[str]:
    if isinstance(value, list):
        return [str(part).strip() for part in value if str(part).strip()]
    if isinstance(value, str):
        return [part.strip() for part in value.split(" ") if part.strip()]
    return []


def apply_desktop_mode_to_flags(
    cc_args: List[str],
    cpp_args: List[str],
    ld_args: List[str],
    mode: str,
) -> Tuple[List[str], List[str], List[str]]:
    def filter_compile_flags(flags: List[str]) -> List[str]:
        out: List[str] = []
        for flag in flags:
            value = (flag or "").strip()
            if not value:
                continue
            if value in {"-DDEBUG", "-DNDEBUG", "-s"}:
                continue
            if value.startswith("-O"):
                continue
            if value.startswith("-g"):
                continue
            out.append(value)
        return out

    def filter_link_flags(flags: List[str]) -> List[str]:
        out: List[str] = []
        for flag in flags:
            value = (flag or "").strip()
            if not value:
                continue
            if value in {"-s", "-Wl,-s"} and mode == "debug":
                continue
            out.append(value)
        return out

    cc = filter_compile_flags(cc_args)
    cpp = filter_compile_flags(cpp_args)
    ld = filter_link_flags(ld_args)

    if mode == "debug":
        debug_flags = ["-O0", "-g3", "-DDEBUG", "-fno-omit-frame-pointer"]
        cc.extend(debug_flags)
        cpp.extend(debug_flags)
    else:
        release_flags = ["-O2", "-DNDEBUG"]
        cc.extend(release_flags)
        cpp.extend(release_flags)

    return cc, cpp, ld


def list_text(values: object) -> List[str]:
    if not isinstance(values, list):
        return []
    out: List[str] = []
    for value in values:
        text = str(value).strip()
        if text:
            out.append(text)
    return out


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if not isinstance(data, dict):
        raise ValueError(f"Invalid JSON object in {path}")
    return data


def normalize_target(token: str) -> str:
    key = token.strip().lower()
    if key not in TARGET_ALIASES:
        raise ValueError(f"Unknown target '{token}' (use: desktop, android, web)")
    return TARGET_ALIASES[key]


def normalize_targets(tokens: Sequence[str], default_target: str) -> List[str]:
    if not tokens:
        return [default_target]
    out: List[str] = []
    for token in tokens:
        target = normalize_target(token)
        if target not in out:
            out.append(target)
    return out


def normalize_abis(raw: str) -> List[int]:
    aliases = {
        "arm7": 0,
        "armeabi-v7a": 0,
        "armeabi": 0,
        "arm64": 1,
        "arm64-v8a": 1,
        "aarch64": 1,
    }
    requested = [token.strip().lower() for token in raw.split(",") if token.strip()]
    if not requested:
        return [0, 1]
    abis: List[int] = []
    for token in requested:
        if token not in aliases:
            raise ValueError(f"Unknown Android ABI '{token}' (use arm7, arm64)")
        abi = aliases[token]
        if abi not in abis:
            abis.append(abi)
    return abis


def resolve_default_target(config_path: Path) -> str:
    if not config_path.exists():
        return "desktop"
    try:
        data = load_json(config_path)
    except Exception:
        return "desktop"

    root = data.get("Configuration", data)
    if not isinstance(root, dict):
        return "desktop"
    session = root.get("Session")
    if not isinstance(session, dict):
        return "desktop"

    value = int(session.get("CurrentPlatform", 0))
    if value == 1:
        return "android"
    if value == 2:
        return "web"
    return "desktop"


def resolve_toolchain(repo_root: Path) -> ToolchainPaths:
    return resolve_toolchain_paths(
        config_path=str(repo_root / "config.json"),
        default_sdk="/home/djoker/android/android-sdk",
        default_ndk="/home/djoker/android/android-ndk-r27d",
        default_java="/usr/lib/jvm/java-11-openjdk-amd64",
        preferred_build_tools="30.0.2",
        preferred_platform="android-31",
    )


def pick_tool(env_keys: Sequence[str], default_path: str, fallback_cmd: str) -> str:
    for key in env_keys:
        value = os.environ.get(key, "").strip()
        if value:
            return value
    if default_path and os.path.exists(default_path):
        return default_path
    found = shutil.which(fallback_cmd)
    return found or default_path


def resolve_emscripten_tools() -> Tuple[str, str, str]:
    emcc = pick_tool(("EMCC",), DEFAULT_EMCC, "emcc")
    emcpp = pick_tool(("EMCPP", "EMXX"), DEFAULT_EMCPP, "em++")
    emar = pick_tool(("EMAR",), DEFAULT_EMAR, "emar")
    return emcc, emcpp, emar


def parse_platform_block(data: dict, key: str) -> PlatformBlock:
    platforms = data.get("plataforms", {})
    block_data = {}
    if isinstance(platforms, dict):
        raw = platforms.get(key, {})
        if isinstance(raw, dict):
            block_data = raw

    return PlatformBlock(
        src=list_text(block_data.get("src", [])),
        include=list_text(block_data.get("include", [])),
        cpp_args=split_flags(block_data.get("CPP_ARGS", "")),
        cc_args=split_flags(block_data.get("CC_ARGS", "")),
        ld_args=split_flags(block_data.get("LD_ARGS", "")),
        template=str(block_data.get("template", "") or "").strip(),
    )


def load_module_file(module_file: Path) -> ModuleSpec:
    data = load_json(module_file)
    directory = module_file.parent.resolve()
    name = str(data.get("module", directory.name)).strip() or directory.name

    return ModuleSpec(
        name=name,
        directory=directory,
        static=bool(data.get("static", True)),
        depends=list_text(data.get("depends", [])),
        system=[entry.lower() for entry in list_text(data.get("system", []))],
        main_src=list_text(data.get("src", [])),
        main_include=list_text(data.get("include", [])),
        main_cpp_args=split_flags(data.get("CPP_ARGS", "")),
        main_cc_args=split_flags(data.get("CC_ARGS", "")),
        main_ld_args=split_flags(data.get("LD_ARGS", "")),
        desktop=parse_platform_block(data, "windows" if os.name == "nt" else "linux"),
        android=parse_platform_block(data, "android"),
        web=parse_platform_block(data, "emscripten"),
    )


def discover_modules(modules_root: Path) -> Dict[str, ModuleSpec]:
    modules: Dict[str, ModuleSpec] = {}
    if not modules_root.exists():
        return modules
    for module_file in sorted(modules_root.glob("*/module.json")):
        try:
            module = load_module_file(module_file)
            modules[module.name] = module
        except Exception as exc:
            print(f"Skip invalid module file {module_file}: {exc}", flush=True)
    return modules


def resolve_module_file(repo_root: Path, module_name: str, explicit_file: str = "") -> Path:
    if explicit_file:
        path = Path(explicit_file).expanduser()
        if not path.is_absolute():
            path = (repo_root / path).resolve()
        return path
    return (repo_root / "modules" / module_name / "module.json").resolve()


def parse_project_file(project_file: Path) -> ProjectSpec:
    data = load_json(project_file)
    root = project_file.parent.resolve()

    path_raw = str(data.get("Path", root)).strip()
    path_value = Path(path_raw).expanduser()
    project_root = path_value.resolve() if path_value.is_absolute() else (root / path_value).resolve()

    def to_abs(path_text: str) -> str:
        path = Path(path_text)
        if path.is_absolute():
            return str(path)
        return str((project_root / path).resolve())

    modules = list_text(data.get("Modules", []))
    src = [to_abs(item) for item in list_text(data.get("Src", []))]
    include = [to_abs(item) for item in list_text(data.get("Include", []))]

    main = data.get("Main", {}) if isinstance(data.get("Main"), dict) else {}
    desktop = data.get("Desktop", {}) if isinstance(data.get("Desktop"), dict) else {}
    android = data.get("Android", {}) if isinstance(data.get("Android"), dict) else {}
    web = data.get("Web", {}) if isinstance(data.get("Web"), dict) else {}

    return ProjectSpec(
        name=str(data.get("Name", project_file.stem)).strip() or project_file.stem,
        root=project_root,
        file_path=project_file.resolve(),
        modules=modules,
        src=src,
        include=include,
        main_cpp=split_flags(main.get("CPP", [])),
        main_cc=split_flags(main.get("CC", [])),
        main_ld=split_flags(main.get("LD", [])),
        desktop_cpp=split_flags(desktop.get("CPP", [])),
        desktop_cc=split_flags(desktop.get("CC", [])),
        desktop_ld=split_flags(desktop.get("LD", [])),
        android_package=str(android.get("PACKAGE", "") or "").strip(),
        android_activity=str(android.get("ACTIVITY", "") or "").strip(),
        android_cpp=split_flags(android.get("CPP", [])),
        android_cc=split_flags(android.get("CC", [])),
        android_ld=split_flags(android.get("LD", [])),
        web_shell=str(web.get("SHELL", "") or "").strip(),
        web_cpp=split_flags(web.get("CPP", [])),
        web_cc=split_flags(web.get("CC", [])),
        web_ld=split_flags(web.get("LD", [])),
    )


def resolve_project_file(repo_root: Path, project_hint: str, explicit_file: str = "") -> Path:
    if explicit_file:
        path = Path(explicit_file).expanduser()
        if not path.is_absolute():
            path = (repo_root / path).resolve()
        return path

    candidate = Path(project_hint).expanduser()
    if candidate.is_absolute() and candidate.exists():
        if candidate.is_dir():
            return (candidate / "main.mk").resolve()
        return candidate.resolve()

    from_repo = (repo_root / candidate).resolve()
    if from_repo.exists():
        if from_repo.is_dir():
            return (from_repo / "main.mk").resolve()
        return from_repo

    return (repo_root / "projects" / project_hint / "main.mk").resolve()


def is_cpp_source(path: str) -> bool:
    return Path(path).suffix.lower() in {".cc", ".cpp", ".cxx", ".xpp", ".mm"}


def normalize_source_list(ctx: CliContext, source_paths: Iterable[str]) -> List[str]:
    out: List[str] = []
    for source in source_paths:
        src_path = Path(source)
        ext = src_path.suffix.lower()
        if ext not in COMPILE_EXTS:
            ctx.trace("Skip non-compilable file: ", source)
            continue
        out.append(str(src_path))
    return out


def module_platform_block(module: ModuleSpec, target: str) -> PlatformBlock:
    if target == "desktop":
        return module.desktop
    if target == "android":
        return module.android
    return module.web


def module_system_name(target: str) -> str:
    if target == "desktop":
        return "windows" if os.name == "nt" else "linux"
    if target == "android":
        return "android"
    return "emscripten"


def include_suffix_for_target(target: str) -> str:
    if target == "desktop":
        return "windows" if os.name == "nt" else "linux"
    if target == "android":
        return "android"
    return "web"


def library_dir_for_module(module: ModuleSpec, target: str, abi: int = 0) -> Path:
    if target == "desktop":
        return module.directory / ("Windows" if os.name == "nt" else "Linux")
    if target == "web":
        return module.directory / "Web"
    abi_name = "arm64-v8a" if abi == 1 else "armeabi-v7a"
    return module.directory / "Android" / abi_name


def library_file_for_module(module: ModuleSpec, target: str, abi: int = 0) -> Path:
    extension = ".a" if module.static or target == "web" else ".so"
    return library_dir_for_module(module, target, abi) / f"lib{module.name}{extension}"


def topological_modules(
    module_name: str,
    all_modules: Dict[str, ModuleSpec],
    ctx: CliContext,
) -> List[str]:
    order: List[str] = []
    visited: set[str] = set()
    stack: set[str] = set()

    def visit(name: str) -> None:
        if name in visited:
            return
        if name in stack:
            ctx.trace("Warn: circular module dependency detected on ", name)
            return
        module = all_modules.get(name)
        if not module:
            ctx.trace("Warn: module dependency not found: ", name)
            return
        stack.add(name)
        for dep in module.depends:
            dep_name = dep.strip()
            if not dep_name or dep_name == name:
                continue
            visit(dep_name)
        stack.remove(name)
        visited.add(name)
        order.append(name)

    visit(module_name)
    return order


def closure_for_modules(module_names: Sequence[str], all_modules: Dict[str, ModuleSpec], ctx: CliContext) -> List[str]:
    ordered: List[str] = []
    for name in module_names:
        for item in topological_modules(name, all_modules, ctx):
            if item not in ordered:
                ordered.append(item)
    return ordered


def add_module_include_flags(
    module: ModuleSpec,
    block: PlatformBlock,
    target: str,
    cc_args: List[str],
    cpp_args: List[str],
) -> None:
    suffix = include_suffix_for_target(target)

    include_candidates = [
        module.directory / "src",
        module.directory / "include",
        module.directory / "include" / suffix,
    ]
    include_candidates.extend(module.directory / rel for rel in module.main_include)
    include_candidates.extend(module.directory / rel for rel in block.include)

    for include_path in include_candidates:
        flag = "-I" + str(include_path)
        append_unique(cc_args, flag)
        append_unique(cpp_args, flag)


def add_module_link_flags(
    module: ModuleSpec,
    block: PlatformBlock,
    target: str,
    abi: int,
    ld_args: List[str],
) -> None:
    lib_dir = library_dir_for_module(module, target, abi)
    append_unique(ld_args, "-L" + str(lib_dir))

    lib_file = library_file_for_module(module, target, abi)
    if lib_file.exists():
        ld_args.append("-l" + module.name)

    for flag in module.main_ld_args:
        if flag:
            ld_args.append(flag)
    for flag in block.ld_args:
        if flag:
            ld_args.append(flag)


def collect_module_build_flags(
    module: ModuleSpec,
    target: str,
    abi: int,
    all_modules: Dict[str, ModuleSpec],
    ctx: CliContext,
) -> Tuple[List[str], List[str], List[str]]:
    block = module_platform_block(module, target)
    cc_args: List[str] = []
    cpp_args: List[str] = []
    ld_args: List[str] = []

    add_module_include_flags(module, block, target, cc_args, cpp_args)

    for flag in module.main_cc_args:
        append_unique(cc_args, flag)
    for flag in block.cc_args:
        append_unique(cc_args, flag)

    for flag in module.main_cpp_args:
        append_unique(cpp_args, flag)
    for flag in block.cpp_args:
        append_unique(cpp_args, flag)

    for dep_name in module.depends:
        name = dep_name.strip()
        if not name or name == module.name:
            continue
        dep = all_modules.get(name)
        if not dep:
            ctx.trace("Warn: dependency module not found for ", module.name, ": ", name)
            continue
        dep_block = module_platform_block(dep, target)
        add_module_include_flags(dep, dep_block, target, cc_args, cpp_args)
        add_module_link_flags(dep, dep_block, target, abi, ld_args)

    for flag in module.main_ld_args:
        append_unique(ld_args, flag)
    for flag in block.ld_args:
        append_unique(ld_args, flag)

    return cc_args, cpp_args, ld_args


def toolchain_ready_for_target(
    ctx: CliContext,
    target: str,
    toolchain: ToolchainPaths,
    emcc: str,
    emcpp: str,
    emar: str,
) -> bool:
    if target == "web":
        missing = [path for path in (emcc, emcpp, emar) if not path or not os.path.exists(path)]
        if missing:
            ctx.trace("Missing emscripten tools:")
            for path in missing:
                ctx.trace("  ", path)
            return False
        return True

    if target == "android":
        checks = [
            toolchain.ANDROID_NDK,
            toolchain.ANDROID_SDK,
            toolchain.AAPT,
            toolchain.DX,
            toolchain.APKSIGNER,
            toolchain.PLATFORM,
        ]
        missing = [path for path in checks if not path or not os.path.exists(path)]
        if missing:
            ctx.trace("Missing Android toolchain paths:")
            for path in missing:
                ctx.trace("  ", path)
            return False
    return True


def build_module_target(
    ctx: CliContext,
    module: ModuleSpec,
    target: str,
    abi_list: Sequence[int],
    full_build: bool,
    all_modules: Dict[str, ModuleSpec],
    emcc: str,
    emcpp: str,
    emar: str,
    toolchain: ToolchainPaths,
    desktop_mode: str = "release",
) -> bool:
    system_name = module_system_name(target)
    if module.system and system_name not in module.system:
        ctx.trace("Skip module ", module.name, " for ", target, " (unsupported by module.json)")
        return True

    block = module_platform_block(module, target)
    all_src = [str((module.directory / rel).resolve()) for rel in (module.main_src + block.src) if rel.strip()]
    srcs = normalize_source_list(ctx, all_src)
    if not srcs:
        ctx.trace("No source files to compile for module ", module.name, " target ", target)
        return False

    use_cpp = any(is_cpp_source(src) for src in srcs)
    build_type = 2 if module.static else 1

    if target == "desktop":
        cc_args, cpp_args, ld_args = collect_module_build_flags(module, target, 0, all_modules, ctx)
        cc_args, cpp_args, ld_args = apply_desktop_mode_to_flags(cc_args, cpp_args, ld_args, desktop_mode)
        if not desktop_compile(ctx, str(module.directory), module.name, srcs, cc_args, cpp_args, full_build):
            return False
        return bool(desktop_build(ctx, str(module.directory), module.name, use_cpp, ld_args, build_type))

    if target == "web":
        cc_args, cpp_args, ld_args = collect_module_build_flags(module, target, 0, all_modules, ctx)
        if not web_compile(ctx, str(module.directory), module.name, srcs, cc_args, cpp_args, emcc, emcpp, full_build):
            return False
        return bool(web_build(ctx, str(module.directory), module.name, use_cpp, ld_args, emcc, emcpp, emar, module.static))

    if target == "android":
        ok = True
        for abi in abi_list:
            cc_args, cpp_args, ld_args = collect_module_build_flags(module, target, abi, all_modules, ctx)
            if not android_compile(
                ctx,
                str(module.directory),
                module.name,
                srcs,
                cc_args,
                cpp_args,
                abi,
                full_build,
                android_ndk=toolchain.ANDROID_NDK,
            ):
                ok = False
                continue
            if not android_build(
                ctx,
                str(module.directory),
                module.name,
                use_cpp,
                ld_args,
                build_type,
                abi,
                android_ndk=toolchain.ANDROID_NDK,
            ):
                ok = False
        return ok

    return False


def load_global_modules(config_path: Path) -> List[str]:
    if not config_path.exists():
        return []
    try:
        data = load_json(config_path)
    except Exception:
        return []

    root = data.get("Configuration", data)
    if not isinstance(root, dict):
        return []
    modules = root.get("Modules", [])
    return [str(item).strip() for item in modules if str(item).strip()]


def collect_project_module_flags(
    ctx: CliContext,
    project: ProjectSpec,
    target: str,
    abi: int,
    active_modules: Sequence[str],
    all_modules: Dict[str, ModuleSpec],
    modules_root: Path,
    cc_args: List[str],
    cpp_args: List[str],
    ld_args: List[str],
) -> None:
    expanded_modules = closure_for_modules(active_modules, all_modules, ctx)
    for module_name in expanded_modules:
        module = all_modules.get(module_name)
        if module:
            block = module_platform_block(module, target)
            add_module_include_flags(module, block, target, cc_args, cpp_args)
            add_module_link_flags(module, block, target, abi, ld_args)
            continue

        fallback_dir = modules_root / module_name
        include_dir = fallback_dir / "include"
        suffix = include_suffix_for_target(target)
        append_unique(cc_args, "-I" + str(include_dir))
        append_unique(cpp_args, "-I" + str(include_dir))
        append_unique(cc_args, "-I" + str(include_dir / suffix))
        append_unique(cpp_args, "-I" + str(include_dir / suffix))

        if target == "android":
            abi_name = "arm64-v8a" if abi == 1 else "armeabi-v7a"
            lib_dir = fallback_dir / "Android" / abi_name
        elif target == "web":
            lib_dir = fallback_dir / "Web"
        else:
            lib_dir = fallback_dir / ("Windows" if os.name == "nt" else "Linux")
        append_unique(ld_args, "-L" + str(lib_dir))
        append_unique(ld_args, "-l" + module_name)


def select_project_modules(project: ProjectSpec, config_path: Path) -> List[str]:
    if project.modules:
        return project.modules
    return load_global_modules(config_path)


def sanitize_android_package_name(package_name: str, fallback: str = "com.djokersoft.game") -> str:
    value = (package_name or "").strip().replace("/", ".")
    value = re.sub(r"[^A-Za-z0-9_.]", "", value)
    value = re.sub(r"\.+", ".", value).strip(".")
    parts = [part for part in value.split(".") if part]
    clean_parts = []
    for part in parts:
        cleaned = re.sub(r"[^A-Za-z0-9_]", "", part)
        if not cleaned:
            continue
        if cleaned[0].isdigit():
            cleaned = "p" + cleaned
        clean_parts.append(cleaned)
    if len(clean_parts) < 2:
        return fallback
    return ".".join(clean_parts)


def ensure_android_native_template(
    ctx: CliContext,
    project_root: Path,
    templates_root: Path,
    app_name: str,
    package_name: str,
    label: str,
    activity: str,
) -> Tuple[str, str, str]:
    out_folder = project_root / "Android" / app_name
    res_root = out_folder / "res"
    createFolderTree(str(out_folder))
    createFolderTree(str(res_root))

    icon_src = templates_root / "Android" / "Res" / "mipmap-hdpi" / "ic_launcher.png"
    for bucket in ("mipmap-hdpi", "mipmap-mdpi", "mipmap-xhdpi", "mipmap-xxhdpi"):
        bucket_dir = res_root / bucket
        createFolderTree(str(bucket_dir))
        icon_dst = bucket_dir / "ic_launcher.png"
        if icon_src.exists() and not icon_dst.exists():
            try:
                shutil.copy(str(icon_src), str(icon_dst))
            except Exception as exc:
                ctx.trace("Warn copy android icon: ", type(exc).__name__, " ", exc)

    package_name = sanitize_android_package_name(package_name)
    if not label:
        label = app_name
    if not activity:
        activity = "android.app.NativeActivity"

    if activity.startswith("."):
        activity = package_name + activity
    elif "." not in activity:
        activity = package_name + "." + activity

    manifest_file = out_folder / "AndroidManifest.xml"
    if not manifest_file.exists():
        manifest_data = build_native_manifest(
            NATIVE_MANIFEST_TEMPLATE,
            package_name,
            label,
            activity,
            app_name,
        )
        manifest_file.write_text(manifest_data, encoding="utf-8")

    return package_name, label, activity


def append_web_template_and_assets(
    ctx: CliContext,
    project: ProjectSpec,
    active_modules: Sequence[str],
    all_modules: Dict[str, ModuleSpec],
    ld_args: List[str],
) -> None:
    template: Optional[Path] = None
    if project.web_shell:
        shell_path = Path(project.web_shell)
        if not shell_path.is_absolute():
            shell_path = (project.root / shell_path).resolve()
        if shell_path.exists():
            template = shell_path
        else:
            ctx.trace("Web shell not found: ", shell_path)

    if template is None:
        for module_name in active_modules:
            module = all_modules.get(module_name)
            if not module:
                continue
            shell = module.web.template.strip()
            if not shell:
                continue
            shell_path = (module.directory / shell).resolve()
            if shell_path.exists():
                template = shell_path
                break

    if template is not None:
        append_unique(ld_args, "--shell-file")
        append_unique(ld_args, str(template))

    preload_entries = [
        ("scripts", "scripts"),
        ("assets", "assets"),
        ("resources", "resources"),
        ("data", "data"),
        ("media", "media"),
    ]
    for folder_name, mount_name in preload_entries:
        host_path = project.root / folder_name
        if host_path.exists():
            append_unique(ld_args, "--preload-file")
            append_unique(ld_args, str(host_path) + f"@/{mount_name}")


def resolve_web_export(project_root: Path, app_name: str) -> Optional[Path]:
    base = project_root / "Web"
    candidates = [base / f"{app_name}.html", base / app_name / f"{app_name}.html"]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return None


def resolve_web_url(export_path: Path, repo_root: Path, port: int) -> Tuple[Path, str]:
    repo_root = repo_root.resolve()
    export_path = export_path.resolve()
    try:
        relative = export_path.relative_to(repo_root)
        serve_root = repo_root
    except ValueError:
        relative = export_path.name
        serve_root = export_path.parent
    url_path = quote(str(relative).replace(os.path.sep, "/"), safe="/")
    return serve_root, f"http://127.0.0.1:{port}/{url_path}"


def run_desktop_binary(ctx: CliContext, binary_path: Path) -> bool:
    if not binary_path.exists():
        ctx.trace("Desktop binary not found: ", binary_path)
        return False
    ctx.trace("Running desktop app: ", binary_path)
    code = subprocess.call([str(binary_path)], cwd=str(binary_path.parent))
    return code == 0


def run_web_output(ctx: CliContext, export_path: Path, repo_root: Path, port: int) -> bool:
    if not export_path.exists():
        ctx.trace("Web output not found: ", export_path)
        return False
    serve_root, url = resolve_web_url(export_path, repo_root, port)
    ctx.trace("Serving Web from ", serve_root)
    ctx.trace("Open ", url)
    webbrowser.open_new_tab(url)
    try:
        subprocess.run(
            [sys.executable, "-m", "http.server", str(port), "--directory", str(serve_root)],
            check=False,
        )
    except KeyboardInterrupt:
        ctx.trace("Web server stopped")
    return True


def build_project_target(
    ctx: CliContext,
    repo_root: Path,
    project: ProjectSpec,
    target: str,
    abi_list: Sequence[int],
    full_build: bool,
    run_after: bool,
    auto_build_modules: bool,
    modules: Dict[str, ModuleSpec],
    toolchain: ToolchainPaths,
    emcc: str,
    emcpp: str,
    emar: str,
    web_port: int,
    desktop_mode: str = "release",
) -> bool:
    source_files = normalize_source_list(ctx, project.src)
    if not source_files:
        ctx.trace("No compilable project source files in ", project.file_path)
        return False

    active_modules = select_project_modules(project, repo_root / "config.json")

    if auto_build_modules and active_modules:
        ordered_modules = closure_for_modules(active_modules, modules, ctx)
        for module_name in ordered_modules:
            module = modules.get(module_name)
            if not module:
                ctx.trace("Warn: module not found for auto-build: ", module_name)
                continue
            ctx.trace("Auto-build module ", module.name, " for ", target)
            if not build_module_target(
                ctx,
                module,
                target,
                abi_list,
                full_build,
                modules,
                emcc,
                emcpp,
                emar,
                toolchain,
                desktop_mode=desktop_mode,
            ):
                ctx.trace("Failed auto-build module ", module.name, " for ", target)
                return False

    cc_args = list(project.main_cc)
    cpp_args = list(project.main_cpp)
    ld_args = list(project.main_ld)

    if target == "desktop":
        cc_args.extend(project.desktop_cc)
        cpp_args.extend(project.desktop_cpp)
        ld_args.extend(project.desktop_ld)
    elif target == "android":
        cc_args.extend(project.android_cc)
        cpp_args.extend(project.android_cpp)
        ld_args.extend(project.android_ld)
    else:
        cc_args.extend(project.web_cc)
        cpp_args.extend(project.web_cpp)
        ld_args.extend(project.web_ld)

    for include in project.include:
        append_unique(cc_args, "-I" + include)
        append_unique(cpp_args, "-I" + include)

    use_cpp = any(is_cpp_source(src) for src in source_files)

    if target == "desktop":
        cc_args, cpp_args, ld_args = apply_desktop_mode_to_flags(cc_args, cpp_args, ld_args, desktop_mode)
        collect_project_module_flags(
            ctx,
            project,
            target,
            0,
            active_modules,
            modules,
            repo_root / "modules",
            cc_args,
            cpp_args,
            ld_args,
        )
        if not desktop_compile(ctx, str(project.root), project.name, source_files, cc_args, cpp_args, full_build):
            return False
        if not desktop_build(ctx, str(project.root), project.name, use_cpp, ld_args, 0):
            return False
        if run_after:
            return run_desktop_binary(ctx, project.root / project.name)
        return True

    if target == "web":
        collect_project_module_flags(
            ctx,
            project,
            target,
            0,
            active_modules,
            modules,
            repo_root / "modules",
            cc_args,
            cpp_args,
            ld_args,
        )
        append_web_template_and_assets(ctx, project, active_modules, modules, ld_args)
        if not web_compile(ctx, str(project.root), project.name, source_files, cc_args, cpp_args, emcc, emcpp, full_build):
            return False
        if not web_build(ctx, str(project.root), project.name, use_cpp, ld_args, emcc, emcpp, emar, False):
            return False
        if run_after:
            export = resolve_web_export(project.root, project.name)
            if not export:
                ctx.trace("Web output not found for ", project.name)
                return False
            return run_web_output(ctx, export, repo_root, web_port)
        return True

    if target == "android":
        ok = True
        for abi in abi_list:
            abi_cc = list(cc_args)
            abi_cpp = list(cpp_args)
            abi_ld = list(ld_args)
            collect_project_module_flags(
                ctx,
                project,
                target,
                abi,
                active_modules,
                modules,
                repo_root / "modules",
                abi_cc,
                abi_cpp,
                abi_ld,
            )
            if not android_compile(
                ctx,
                str(project.root),
                project.name,
                source_files,
                abi_cc,
                abi_cpp,
                abi,
                full_build,
                android_ndk=toolchain.ANDROID_NDK,
            ):
                ok = False
                continue
            if not android_build(
                ctx,
                str(project.root),
                project.name,
                use_cpp,
                abi_ld,
                0,
                abi,
                android_ndk=toolchain.ANDROID_NDK,
            ):
                ok = False
        if not ok:
            return False

        package_name, label, activity = ensure_android_native_template(
            ctx,
            project.root,
            repo_root / "Templates",
            project.name,
            project.android_package,
            project.name,
            project.android_activity,
        )

        keytool = str((Path(toolchain.JAVA_SDK) / "bin" / "keytool").resolve())
        return bool(
            android_compile_java_native(
                ctx,
                str(project.root),
                0,
                project.name,
                package_name,
                label,
                activity,
                run_after,
                android_sdk=toolchain.ANDROID_SDK,
                aapt=toolchain.AAPT,
                dx=toolchain.DX,
                dx8=toolchain.DX8,
                apksigner=toolchain.APKSIGNER,
                platform=toolchain.PLATFORM,
                keytool=keytool,
                native_manifest_template=NATIVE_MANIFEST_TEMPLATE,
            )
        )

    return False


def parse_build_subject(subject: str, name_or_target: Optional[str], trailing_targets: Sequence[str]) -> Tuple[str, str, List[str]]:
    kind = subject.strip().lower()
    if kind in {"module", "mod"}:
        if not name_or_target:
            raise ValueError("Missing module name. Usage: build module <name> [targets...]")
        return "module", name_or_target, list(trailing_targets)

    if kind in {"app", "project", "proj"}:
        if not name_or_target:
            raise ValueError("Missing project name. Usage: build app <name> [targets...]")
        return "app", name_or_target, list(trailing_targets)

    targets = []
    if name_or_target:
        targets.append(name_or_target)
    targets.extend(trailing_targets)
    return "app", subject, targets


def iter_project_files(projects_root: Path) -> List[Path]:
    if not projects_root.exists():
        return []
    return sorted(projects_root.rglob("main.mk"))


def remove_path(ctx: CliContext, path: Path, dry_run: bool) -> int:
    if not path.exists():
        return 0
    ctx.trace(("Would remove: " if dry_run else "Remove: "), path)
    if dry_run:
        return 1
    try:
        if path.is_dir():
            shutil.rmtree(path)
        else:
            path.unlink()
        return 1
    except Exception as exc:
        ctx.trace("Failed remove ", path, " : ", type(exc).__name__, " ", exc)
        return 0


def clean_module_target(ctx: CliContext, module: ModuleSpec, target: str, abi_list: Sequence[int], dry_run: bool) -> int:
    removed = 0
    os_name = "Windows" if os.name == "nt" else "Linux"

    if target == "desktop":
        removed += remove_path(ctx, module.directory / "obj" / os_name / module.name, dry_run)
        removed += remove_path(ctx, module.directory / os_name / f"lib{module.name}.a", dry_run)
        removed += remove_path(ctx, module.directory / os_name / f"lib{module.name}.so", dry_run)
        removed += remove_path(ctx, module.directory / os_name / f"lib{module.name}.dll", dry_run)
        return removed

    if target == "web":
        removed += remove_path(ctx, module.directory / "obj" / "Web" / module.name, dry_run)
        web_root = module.directory / "Web"
        for candidate in (
            web_root / f"lib{module.name}.a",
            web_root / f"lib{module.name}.so",
            web_root / f"{module.name}.html",
            web_root / f"{module.name}.js",
            web_root / f"{module.name}.wasm",
            web_root / f"{module.name}.data",
            web_root / f"{module.name}.worker.js",
        ):
            removed += remove_path(ctx, candidate, dry_run)
        return removed

    if target == "android":
        removed += remove_path(ctx, module.directory / "obj" / "Android" / module.name, dry_run)
        for abi in abi_list:
            abi_name = "arm64-v8a" if abi == 1 else "armeabi-v7a"
            abi_root = module.directory / "Android" / abi_name
            removed += remove_path(ctx, abi_root / f"lib{module.name}.a", dry_run)
            removed += remove_path(ctx, abi_root / f"lib{module.name}.so", dry_run)
        return removed

    return removed


def clean_project_target(ctx: CliContext, project: ProjectSpec, target: str, abi_list: Sequence[int], dry_run: bool) -> int:
    removed = 0
    os_name = "Windows" if os.name == "nt" else "Linux"

    if target == "desktop":
        removed += remove_path(ctx, project.root / "obj" / os_name / project.name, dry_run)
        removed += remove_path(ctx, project.root / project.name, dry_run)
        removed += remove_path(ctx, project.root / f"{project.name}.exe", dry_run)
        return removed

    if target == "web":
        removed += remove_path(ctx, project.root / "obj" / "Web" / project.name, dry_run)
        web_root = project.root / "Web"
        for candidate in (
            web_root / f"{project.name}.html",
            web_root / f"{project.name}.js",
            web_root / f"{project.name}.wasm",
            web_root / f"{project.name}.data",
            web_root / f"{project.name}.worker.js",
            web_root / project.name,
        ):
            removed += remove_path(ctx, candidate, dry_run)
        return removed

    if target == "android":
        removed += remove_path(ctx, project.root / "obj" / "Android" / project.name, dry_run)
        android_root = project.root / "Android"
        for abi in abi_list:
            abi_name = "arm64-v8a" if abi == 1 else "armeabi-v7a"
            removed += remove_path(ctx, android_root / abi_name / f"lib{project.name}.so", dry_run)
            removed += remove_path(ctx, android_root / abi_name / f"lib{project.name}.a", dry_run)
        removed += remove_path(ctx, android_root / project.name, dry_run)
        return removed

    return removed


def command_build(args: argparse.Namespace, repo_root: Path) -> int:
    ctx = CliContext()

    kind, subject_name, raw_targets = parse_build_subject(args.subject, args.name_or_target, args.targets)
    default_target = resolve_default_target(repo_root / "config.json")
    targets = normalize_targets(raw_targets, default_target)
    abis = normalize_abis(args.abis)

    toolchain = resolve_toolchain(repo_root)
    emcc, emcpp, emar = resolve_emscripten_tools()

    ctx.trace("Build type: ", kind)
    ctx.trace("Name: ", subject_name)
    ctx.trace("Targets: ", ", ".join(targets))
    ctx.trace("Desktop mode: ", args.mode)
    ctx.trace("Android ABIs: ", ", ".join("arm64-v8a" if abi == 1 else "armeabi-v7a" for abi in abis))

    modules = discover_modules(repo_root / "modules")

    for target in targets:
        effective_mode = args.mode if target == "desktop" else "release"
        if target != "desktop" and args.mode != "release":
            ctx.trace("Target ", target, " uses release mode (desktop mode ignored)")

        if not toolchain_ready_for_target(ctx, target, toolchain, emcc, emcpp, emar):
            return 1

        if kind == "module":
            module_file = resolve_module_file(repo_root, subject_name, args.module_file)
            if not module_file.exists():
                ctx.trace("Module file not found: ", module_file)
                return 1

            module = load_module_file(module_file)
            modules[module.name] = module

            build_order = closure_for_modules([module.name], modules, ctx) if not args.no_deps else [module.name]
            for module_name in build_order:
                mod = modules.get(module_name)
                if not mod:
                    ctx.trace("Module not found in registry: ", module_name)
                    return 1
                ctx.trace("Build module ", mod.name, " -> ", target)
                if args.dry_run:
                    continue
                if not build_module_target(
                    ctx,
                    mod,
                    target,
                    abis,
                    args.full,
                    modules,
                    emcc,
                    emcpp,
                    emar,
                    toolchain,
                    desktop_mode=effective_mode,
                ):
                    return 1

            if args.run:
                ctx.trace("--run is ignored for module builds")
            continue

        project_file = resolve_project_file(repo_root, subject_name, args.project_file)
        if not project_file.exists():
            ctx.trace("Project file not found: ", project_file)
            return 1

        project = parse_project_file(project_file)
        ctx.trace("Build app ", project.name, " from ", project.file_path)
        if args.dry_run:
            continue

        ok = build_project_target(
            ctx,
            repo_root,
            project,
            target,
            abis,
            args.full,
            args.run,
            not args.skip_modules,
            modules,
            toolchain,
            emcc,
            emcpp,
            emar,
            args.port,
            desktop_mode=effective_mode,
        )
        if not ok:
            return 1

    return 0


def command_list(args: argparse.Namespace, repo_root: Path) -> int:
    ctx = CliContext()
    what = args.what.lower()

    if what in {"modules", "all"}:
        modules = discover_modules(repo_root / "modules")
        ctx.trace("Modules:")
        for name in sorted(modules):
            module = modules[name]
            systems = ",".join(module.system) if module.system else "-"
            ctx.trace("  ", name, "  [", systems, "]  ", module.directory)
        if not modules:
            ctx.trace("  <none>")

    if what in {"apps", "projects", "all"}:
        ctx.trace("Projects:")
        files = iter_project_files(repo_root / "projects")
        if not files:
            ctx.trace("  <none>")
            return 0
        for file_path in files:
            try:
                project = parse_project_file(file_path)
                ctx.trace("  ", project.root.name, " (name=", project.name, ")  ", project.file_path)
            except Exception as exc:
                ctx.trace("  ", file_path, "  [invalid: ", type(exc).__name__, "]")
    return 0


def command_clean(args: argparse.Namespace, repo_root: Path) -> int:
    ctx = CliContext()
    kind, subject_name, raw_targets = parse_build_subject(args.subject, args.name_or_target, args.targets)
    default_target = resolve_default_target(repo_root / "config.json")
    targets = normalize_targets(raw_targets, default_target)
    abis = normalize_abis(args.abis)

    ctx.trace("Clean type: ", kind)
    ctx.trace("Name: ", subject_name)
    ctx.trace("Targets: ", ", ".join(targets))
    ctx.trace("Android ABIs: ", ", ".join("arm64-v8a" if abi == 1 else "armeabi-v7a" for abi in abis))

    removed = 0
    modules = discover_modules(repo_root / "modules")

    for target in targets:
        if kind == "module":
            module_file = resolve_module_file(repo_root, subject_name, args.module_file)
            if not module_file.exists():
                ctx.trace("Module file not found: ", module_file)
                return 1
            module = load_module_file(module_file)
            modules[module.name] = module
            clean_order = closure_for_modules([module.name], modules, ctx) if args.with_deps else [module.name]
            for module_name in clean_order:
                mod = modules.get(module_name)
                if not mod:
                    continue
                ctx.trace("Clean module ", mod.name, " -> ", target)
                removed += clean_module_target(ctx, mod, target, abis, args.dry_run)
            continue

        project_file = resolve_project_file(repo_root, subject_name, args.project_file)
        if not project_file.exists():
            ctx.trace("Project file not found: ", project_file)
            return 1
        project = parse_project_file(project_file)
        ctx.trace("Clean app ", project.name, " -> ", target)
        removed += clean_project_target(ctx, project, target, abis, args.dry_run)

    if args.dry_run:
        ctx.trace("Dry-run done. Candidates: ", removed)
    else:
        ctx.trace("Removed entries: ", removed)
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="CrossIDE CLI builder (without Qt/editor GUI).",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    build = subparsers.add_parser("build", help="Build module or app/project")
    build.add_argument(
        "subject",
        help="module|app|project or direct project name (e.g. bugame)",
    )
    build.add_argument(
        "name_or_target",
        nargs="?",
        help="When subject is module/app: object name. Otherwise: first target.",
    )
    build.add_argument(
        "targets",
        nargs="*",
        help="Targets: desktop android web",
    )
    build.add_argument("--run", action="store_true", help="Run after successful build")
    build.add_argument("--full", action="store_true", help="Force full rebuild")
    build.add_argument(
        "--mode",
        default="release",
        choices=["release", "debug"],
        help="Desktop build mode (Android/Web always release)",
    )
    build.add_argument(
        "--abis",
        default="arm7,arm64",
        help="Android ABIs list (default: arm7,arm64)",
    )
    build.add_argument("--no-deps", action="store_true", help="Do not build dependencies for module builds")
    build.add_argument(
        "--skip-modules",
        action="store_true",
        help="Do not auto-build referenced modules for app builds",
    )
    build.add_argument(
        "--project-file",
        default="",
        help="Explicit path to project main.mk",
    )
    build.add_argument(
        "--module-file",
        default="",
        help="Explicit path to module.json",
    )
    build.add_argument(
        "--port",
        type=int,
        default=8080,
        help="HTTP port for --run on web target",
    )
    build.add_argument("--dry-run", action="store_true", help="Show plan and skip build commands")

    listing = subparsers.add_parser("list", help="List available modules and projects")
    listing.add_argument(
        "what",
        nargs="?",
        default="all",
        choices=["all", "modules", "apps", "projects"],
        help="What to list (default: all)",
    )

    clean = subparsers.add_parser("clean", help="Clean build outputs for module/app")
    clean.add_argument(
        "subject",
        help="module|app|project or direct project name (e.g. bugame)",
    )
    clean.add_argument(
        "name_or_target",
        nargs="?",
        help="When subject is module/app: object name. Otherwise: first target.",
    )
    clean.add_argument(
        "targets",
        nargs="*",
        help="Targets: desktop android web",
    )
    clean.add_argument(
        "--abis",
        default="arm7,arm64",
        help="Android ABIs list (default: arm7,arm64)",
    )
    clean.add_argument(
        "--with-deps",
        action="store_true",
        help="Also clean module dependencies (module clean)",
    )
    clean.add_argument(
        "--project-file",
        default="",
        help="Explicit path to project main.mk",
    )
    clean.add_argument(
        "--module-file",
        default="",
        help="Explicit path to module.json",
    )
    clean.add_argument("--dry-run", action="store_true", help="Show files/dirs to clean without removing")

    return parser


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    repo_root = Path(__file__).resolve().parent

    if args.command == "build":
        try:
            return command_build(args, repo_root)
        except KeyboardInterrupt:
            print("Interrupted", flush=True)
            return 130
        except Exception as exc:
            print(f"Build failed: {type(exc).__name__}: {exc}", flush=True)
            return 1

    if args.command == "list":
        try:
            return command_list(args, repo_root)
        except Exception as exc:
            print(f"List failed: {type(exc).__name__}: {exc}", flush=True)
            return 1

    if args.command == "clean":
        try:
            return command_clean(args, repo_root)
        except KeyboardInterrupt:
            print("Interrupted", flush=True)
            return 130
        except Exception as exc:
            print(f"Clean failed: {type(exc).__name__}: {exc}", flush=True)
            return 1

    parser.print_help()
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
