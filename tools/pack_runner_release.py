#!/usr/bin/env python3
"""
Pack a bugame release using already-built runner artifacts.

This script does NOT compile sources and does NOT call builder.
It only copies:
1) prebuilt runner outputs (desktop/web)
2) release content folders (scripts/assets/resources/data/media)
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import zipfile
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_REPO_ROOT = SCRIPT_DIR.parent
DEFAULT_BUGAME_ROOT = DEFAULT_REPO_ROOT / "projects" / "bugame"
DEFAULT_RUNNER_ROOT = DEFAULT_REPO_ROOT / "projects" / "runner"
DEFAULT_OUTPUT_ROOT = DEFAULT_REPO_ROOT / "projects" / "bugame" / "exports"
CONTENT_DIRS = ("assets", "resources", "data", "media")
BYTECODE_EXTS = (".buc", ".bubc", ".bytecode")


class PackError(RuntimeError):
    pass


@dataclass
class ReleaseInfo:
    name: str
    release_json_path: Path
    content_root: Path
    raw_json: dict


def parse_targets(raw: str) -> list[str]:
    out: list[str] = []
    for item in raw.split(","):
        key = item.strip().lower()
        if not key:
            continue
        if key not in ("desktop", "web", "android"):
            raise PackError(f"Unsupported target '{key}'. Use desktop, web and/or android.")
        if key not in out:
            out.append(key)
    if not out:
        raise PackError("No target selected.")
    return out


def load_json(path: Path) -> dict:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise PackError(f"JSON file not found: {path}") from exc
    except json.JSONDecodeError as exc:
        raise PackError(f"Invalid JSON in {path}: {exc}") from exc
    if not isinstance(data, dict):
        raise PackError(f"JSON root must be object: {path}")
    return data


def resolve_release_json_path(bugame_root: Path, release_arg: str) -> Path:
    candidate = Path(release_arg)
    if candidate.suffix.lower() == ".json":
        if not candidate.is_absolute():
            candidate = (bugame_root / candidate).resolve()
        return candidate
    return (bugame_root / "releases" / f"{release_arg}.json").resolve()


def resolve_release_info(bugame_root: Path, release_arg: str) -> ReleaseInfo:
    release_json_path = resolve_release_json_path(bugame_root, release_arg)
    data = load_json(release_json_path)

    web = data.get("Web") if isinstance(data.get("Web"), dict) else {}
    android = data.get("Android") if isinstance(data.get("Android"), dict) else {}

    content_root_rel = (
        web.get("CONTENT_ROOT")
        or android.get("CONTENT_ROOT")
        or f"releases/{release_json_path.stem}"
    )
    if not isinstance(content_root_rel, str):
        raise PackError("CONTENT_ROOT must be a string in release json.")

    content_root = (bugame_root / content_root_rel).resolve()
    if not content_root.exists() or not content_root.is_dir():
        raise PackError(f"CONTENT_ROOT folder not found: {content_root}")

    return ReleaseInfo(
        name=release_json_path.stem,
        release_json_path=release_json_path,
        content_root=content_root,
        raw_json=data,
    )


def ensure_dir(path: Path, dry_run: bool) -> None:
    if dry_run:
        return
    path.mkdir(parents=True, exist_ok=True)


def clean_dir(path: Path, dry_run: bool) -> None:
    if dry_run:
        return
    if path.exists():
        shutil.rmtree(path)
    path.mkdir(parents=True, exist_ok=True)


def copy_file(src: Path, dst: Path, dry_run: bool) -> None:
    if not src.exists():
        raise PackError(f"Missing file: {src}")
    if dry_run:
        return
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)


def copy_tree(src: Path, dst: Path, dry_run: bool) -> None:
    if not src.exists():
        return
    if dry_run:
        return
    shutil.copytree(src, dst, dirs_exist_ok=True)


def find_desktop_runner_binary(runner_root: Path) -> Path:
    candidates = [
        runner_root / "runner",
        runner_root / "runner.exe",
    ]
    for c in candidates:
        if c.exists() and c.is_file():
            return c
    raise PackError(
        "Desktop runner binary not found. Expected one of:\n"
        f"  - {candidates[0]}\n"
        f"  - {candidates[1]}"
    )


def find_web_runner_files(runner_root: Path) -> list[Path]:
    web_root = runner_root / "Web"
    required = [
        web_root / "runner.html",
        web_root / "runner.js",
        web_root / "runner.wasm",
        web_root / "runner.data",
    ]
    missing = [p for p in required if not p.exists()]
    if missing:
        lines = "\n".join(f"  - {m}" for m in missing)
        raise PackError(f"Web runner artifacts missing:\n{lines}")
    return required


def copy_release_content(content_root: Path, out_root: Path, dry_run: bool) -> None:
    for folder in CONTENT_DIRS:
        src = content_root / folder
        if src.exists() and src.is_dir():
            copy_tree(src, out_root / folder, dry_run)

    scripts_root = content_root / "scripts"
    if scripts_root.exists() and scripts_root.is_dir():
        for file in sorted(scripts_root.rglob("*")):
            if not file.is_file():
                continue
            if file.suffix.lower() not in BYTECODE_EXTS:
                continue
            rel = file.relative_to(scripts_root)
            copy_file(file, out_root / "assets" / rel, dry_run)


def collect_release_files_for_apk(content_root: Path) -> list[tuple[Path, str]]:
    mapped: dict[str, Path] = {}
    mount_map = {
        "assets": "assets/assets",
        "resources": "assets/resources",
        "data": "assets/data",
        "media": "assets/media",
    }
    for folder in CONTENT_DIRS:
        src_root = content_root / folder
        if not src_root.exists() or not src_root.is_dir():
            continue
        mount_root = mount_map[folder]
        for file in sorted(src_root.rglob("*")):
            if not file.is_file():
                continue
            rel = file.relative_to(src_root).as_posix()
            # Runner bytecode should live in /assets, not /assets/scripts or /assets/assets.
            if folder == "assets" and file.suffix.lower() in BYTECODE_EXTS:
                apk_path = f"assets/{rel}"
            else:
                apk_path = f"{mount_root}/{rel}"
            mapped[apk_path] = file

    scripts_root = content_root / "scripts"
    if scripts_root.exists() and scripts_root.is_dir():
        for file in sorted(scripts_root.rglob("*")):
            if not file.is_file():
                continue
            if file.suffix.lower() not in BYTECODE_EXTS:
                continue
            rel = file.relative_to(scripts_root).as_posix()
            mapped[f"assets/{rel}"] = file

    out: list[tuple[Path, str]] = []
    for apk_path in sorted(mapped.keys()):
        out.append((mapped[apk_path], apk_path))
    return out


def ensure_main_buc(content_root: Path, no_check: bool) -> None:
    if no_check:
        return
    main_buc_scripts = content_root / "scripts" / "main.buc"
    main_buc_assets = content_root / "assets" / "main.buc"
    if not main_buc_scripts.exists() and not main_buc_assets.exists():
        raise PackError(
            "Missing compiled bytecode file:\n"
            f"  {main_buc_scripts}\n"
            f"  or {main_buc_assets}\n"
            "Compile it first, then run pack again."
        )


def find_android_tools() -> tuple[Path, Path, Path, Path]:
    sdk_candidates: list[Path] = []
    env_home = os.environ.get("ANDROID_HOME") or os.environ.get("ANDROID_SDK_ROOT")
    if env_home:
        sdk_candidates.append(Path(env_home))
    sdk_candidates.append(Path("/home/djoker/android/android-sdk"))

    sdk_root = next((p for p in sdk_candidates if p.exists()), None)
    if not sdk_root:
        raise PackError("Android SDK not found (set ANDROID_HOME or ANDROID_SDK_ROOT).")

    build_tools_root = sdk_root / "build-tools"
    if not build_tools_root.exists():
        raise PackError(f"Android build-tools folder missing: {build_tools_root}")

    versions = sorted([p for p in build_tools_root.iterdir() if p.is_dir()], key=lambda p: p.name)
    if not versions:
        raise PackError(f"No build-tools versions in: {build_tools_root}")
    bt = versions[-1]

    aapt = bt / "aapt"
    zipalign = bt / "zipalign"
    apksigner = bt / "apksigner"
    for tool in (aapt, zipalign, apksigner):
        if not tool.exists():
            raise PackError(f"Missing Android tool: {tool}")

    platform_jar = sdk_root / "platforms" / "android-31" / "android.jar"
    if not platform_jar.exists():
        raise PackError(f"Missing android.jar: {platform_jar}")

    return aapt, zipalign, apksigner, platform_jar


def run_cmd(args: list[str], cwd: Path | None = None, dry_run: bool = False) -> None:
    pretty = " ".join(f"'{a}'" for a in args)
    print(f"[pack][cmd] {pretty}")
    if dry_run:
        return
    result = subprocess.run(args, cwd=str(cwd) if cwd else None, check=False)
    if result.returncode != 0:
        raise PackError(f"Command failed ({result.returncode}): {pretty}")


def find_runner_android_inputs(runner_root: Path) -> tuple[Path, Path, list[tuple[str, Path]], Path]:
    app_root = runner_root / "Android" / "runner"
    res_root = app_root / "res"
    dex_file = app_root / "dex" / "classes.dex"
    key_file = app_root / "runner.key"

    if not res_root.exists():
        raise PackError(f"Runner Android resources not found: {res_root}")
    if not key_file.exists():
        raise PackError(f"Runner Android keystore not found: {key_file}")

    libs: list[tuple[str, Path]] = []
    for abi in ("armeabi-v7a", "arm64-v8a"):
        lib = runner_root / "Android" / abi / "librunner.so"
        if lib.exists():
            libs.append((abi, lib))
    if not libs:
        raise PackError("Runner Android libs not found. Build runner android first.")

    return app_root, res_root, libs, dex_file


def get_android_release_meta(info: ReleaseInfo) -> tuple[str, str, str]:
    android = info.raw_json.get("Android") if isinstance(info.raw_json.get("Android"), dict) else {}
    package_name = android.get("PACKAGE") if isinstance(android.get("PACKAGE"), str) else ""
    label = android.get("LABEL") if isinstance(android.get("LABEL"), str) else ""
    activity = android.get("ACTIVITY") if isinstance(android.get("ACTIVITY"), str) else ""

    if not package_name:
        package_name = f"com.djokersoft.{info.name}"
    if not label:
        label = info.name
    if not activity:
        activity = "android.app.NativeActivity"
    return package_name, label, activity


def build_manifest_text(package_name: str, label: str, activity: str) -> str:
    return f"""<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="{package_name}"
          android:versionCode="1"
          android:versionName="1.0">

    <uses-sdk
        android:compileSdkVersion="34"
        android:minSdkVersion="24"
        android:targetSdkVersion="34" />

    <application
        android:allowBackup="false"
        android:fullBackupContent="false"
        android:icon="@mipmap/ic_launcher"
        android:label="{label}"
        android:hasCode="false"
        android:roundIcon="@mipmap/ic_launcher_round">

        <activity
            android:name="{activity}"
            android:label="{label}"
            android:configChanges="orientation|keyboardHidden|screenSize"
            android:screenOrientation="landscape"
            android:launchMode="singleTask"
            android:clearTaskOnLaunch="true"
            android:exported="true">

            <meta-data
                android:name="android.app.lib_name"
                android:value="runner" />

            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>
    </application>

</manifest>
"""


def zip_folder(src_dir: Path, zip_path: Path, dry_run: bool) -> None:
    if dry_run:
        return
    if zip_path.exists():
        zip_path.unlink()
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for file in sorted(src_dir.rglob("*")):
            if file.is_file():
                zf.write(file, file.relative_to(src_dir))


def write_pack_info(target_dir: Path, info: ReleaseInfo, target: str, dry_run: bool) -> None:
    payload = {
        "release": info.name,
        "target": target,
        "release_json": str(info.release_json_path),
        "content_root": str(info.content_root),
        "note": "Pack-only export. Runner/web artifacts were reused as-is.",
    }
    if dry_run:
        return
    (target_dir / "pack_info.json").write_text(
        json.dumps(payload, indent=2, ensure_ascii=True) + "\n",
        encoding="utf-8",
    )


def package_desktop(
    runner_root: Path,
    info: ReleaseInfo,
    out_dir: Path,
    dry_run: bool,
) -> None:
    binary = find_desktop_runner_binary(runner_root)
    clean_dir(out_dir, dry_run)
    copy_file(binary, out_dir / binary.name, dry_run)
    copy_release_content(info.content_root, out_dir, dry_run)
    copy_file(info.release_json_path, out_dir / "release.json", dry_run)
    write_pack_info(out_dir, info, "desktop", dry_run)


def package_web(
    runner_root: Path,
    info: ReleaseInfo,
    out_dir: Path,
    dry_run: bool,
) -> None:
    files = find_web_runner_files(runner_root)
    clean_dir(out_dir, dry_run)
    for src in files:
        copy_file(src, out_dir / src.name, dry_run)
    copy_release_content(info.content_root, out_dir, dry_run)
    copy_file(info.release_json_path, out_dir / "release.json", dry_run)
    write_pack_info(out_dir, info, "web", dry_run)


def package_android(
    runner_root: Path,
    info: ReleaseInfo,
    out_dir: Path,
    dry_run: bool,
) -> None:
    aapt, zipalign, apksigner, platform_jar = find_android_tools()
    _runner_app_root, res_root, libs, dex_file = find_runner_android_inputs(runner_root)
    package_name, label, activity = get_android_release_meta(info)

    clean_dir(out_dir, dry_run)
    build_root = out_dir / "_build"
    ensure_dir(build_root, dry_run)
    ensure_dir(build_root / "res", dry_run)
    ensure_dir(build_root / "stage", dry_run)

    manifest = build_root / "AndroidManifest.xml"
    if not dry_run:
        (build_root / "res").mkdir(parents=True, exist_ok=True)
        shutil.copytree(res_root, build_root / "res", dirs_exist_ok=True)
        manifest.write_text(build_manifest_text(package_name, label, activity), encoding="utf-8")

    unaligned_apk = build_root / f"{info.name}.unaligned.apk"
    aligned_apk = build_root / f"{info.name}.aligned.apk"
    signed_apk = out_dir / f"{info.name}.signed.apk"

    run_cmd(
        [
            str(aapt),
            "package",
            "-f",
            "-m",
            "-0",
            "arsc",
            "-F",
            str(unaligned_apk),
            "-M",
            str(manifest),
            "-S",
            str(build_root / "res"),
            "-I",
            str(platform_jar),
        ],
        dry_run=dry_run,
    )

    # Stage classes.dex (if present), runner libs and release content.
    staged_rel_files: list[str] = []
    if dex_file.exists():
        dst = build_root / "stage" / "classes.dex"
        copy_file(dex_file, dst, dry_run)
        staged_rel_files.append("classes.dex")

    for abi, lib_path in libs:
        rel = Path("lib") / abi / "librunner.so"
        dst = build_root / "stage" / rel
        copy_file(lib_path, dst, dry_run)
        staged_rel_files.append(rel.as_posix())

    for src, apk_rel in collect_release_files_for_apk(info.content_root):
        dst = build_root / "stage" / Path(apk_rel)
        copy_file(src, dst, dry_run)
        staged_rel_files.append(apk_rel)

    if staged_rel_files:
        run_cmd(
            [str(aapt), "add", str(unaligned_apk), *staged_rel_files],
            cwd=build_root / "stage",
            dry_run=dry_run,
        )

    run_cmd([str(zipalign), "-f", "-p", "4", str(unaligned_apk), str(aligned_apk)], dry_run=dry_run)
    run_cmd(
        [
            str(apksigner),
            "sign",
            "--ks",
            str(runner_root / "Android" / "runner" / "runner.key"),
            "--ks-key-alias",
            "djokersoft",
            "--ks-pass",
            "pass:14781478",
            "--in",
            str(aligned_apk),
            "--out",
            str(signed_apk),
        ],
        dry_run=dry_run,
    )

    copy_file(info.release_json_path, out_dir / "release.json", dry_run)
    write_pack_info(out_dir, info, "android", dry_run)


def parse_args(argv: Iterable[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Pack bugame release with prebuilt runner outputs (desktop/web/android)."
    )
    parser.add_argument(
        "--release",
        required=True,
        help="Release name (e.g. piano) or json path (e.g. releases/piano.json).",
    )
    parser.add_argument(
        "--targets",
        default="desktop,web",
        help="Comma-separated targets: desktop,web,android (default: desktop,web).",
    )
    parser.add_argument(
        "--bugame-root",
        default=str(DEFAULT_BUGAME_ROOT),
        help=f"Bugame project root (default: {DEFAULT_BUGAME_ROOT})",
    )
    parser.add_argument(
        "--runner-root",
        default=str(DEFAULT_RUNNER_ROOT),
        help=f"Runner project root (default: {DEFAULT_RUNNER_ROOT})",
    )
    parser.add_argument(
        "--out",
        default=str(DEFAULT_OUTPUT_ROOT),
        help=f"Export root (default: {DEFAULT_OUTPUT_ROOT})",
    )
    parser.add_argument(
        "--zip",
        action="store_true",
        help="Also create zip file per target.",
    )
    parser.add_argument(
        "--no-bytecode-check",
        action="store_true",
        help="Skip validation of scripts/main.buc (packed as assets/main.buc).",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Only validate and print plan. Do not copy files.",
    )
    return parser.parse_args(list(argv))


def main(argv: Iterable[str]) -> int:
    args = parse_args(argv)

    bugame_root = Path(args.bugame_root).resolve()
    runner_root = Path(args.runner_root).resolve()
    out_root = Path(args.out).resolve()
    targets = parse_targets(args.targets)

    if not bugame_root.exists():
        raise PackError(f"bugame root not found: {bugame_root}")
    if not runner_root.exists():
        raise PackError(f"runner root not found: {runner_root}")

    info = resolve_release_info(bugame_root, args.release)
    ensure_main_buc(info.content_root, args.no_bytecode_check)

    release_out_root = out_root / info.name
    ensure_dir(release_out_root, args.dry_run)

    print(f"[pack] release   : {info.name}")
    print(f"[pack] json      : {info.release_json_path}")
    print(f"[pack] content   : {info.content_root}")
    print(f"[pack] targets   : {', '.join(targets)}")
    print(f"[pack] out       : {release_out_root}")
    if args.dry_run:
        print("[pack] mode      : dry-run")

    for target in targets:
        target_dir = release_out_root / target
        print(f"[pack] -> {target}: {target_dir}")
        if target == "desktop":
            package_desktop(runner_root, info, target_dir, args.dry_run)
        elif target == "web":
            package_web(runner_root, info, target_dir, args.dry_run)
        elif target == "android":
            package_android(runner_root, info, target_dir, args.dry_run)
        else:
            raise PackError(f"Unknown target: {target}")

        if args.zip:
            zip_path = release_out_root / f"{info.name}-{target}.zip"
            print(f"[pack] zip      : {zip_path}")
            zip_folder(target_dir, zip_path, args.dry_run)

    print("[pack] done")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main(sys.argv[1:]))
    except PackError as exc:
        print(f"[pack][error] {exc}", file=sys.stderr)
        raise SystemExit(1)
