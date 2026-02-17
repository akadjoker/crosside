#!/usr/bin/env python3
"""
Fetch third-party source archives from upstream releases/tags without git clone.
"""

from __future__ import annotations

import argparse
import datetime
import json
import os
import re
import shutil
import tarfile
import urllib.error
import urllib.parse
import urllib.request
import zipfile
from pathlib import Path
from typing import Any


SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_REPO_ROOT = SCRIPT_DIR.parent
DEFAULT_MANIFEST = SCRIPT_DIR / "third_party_releases.json"
DEFAULT_OUTPUT_DIR = Path("third_party_src")
DEFAULT_BACKUP_DIR = Path("third_party_backup")
DEFAULT_TIMEOUT_SECONDS = 30
USER_AGENT = "crosside-third-party-fetch/1.0"


def load_manifest(path: Path) -> dict[str, Any]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict) or "libraries" not in data:
        raise ValueError(f"Invalid manifest structure: {path}")
    libraries = data["libraries"]
    if not isinstance(libraries, dict):
        raise ValueError(f"'libraries' must be an object in: {path}")
    return data


def select_libraries(manifest: dict[str, Any], names: list[str]) -> dict[str, dict[str, Any]]:
    libraries: dict[str, dict[str, Any]] = manifest["libraries"]
    if not names or "all" in names:
        return dict(sorted(libraries.items()))

    selected: dict[str, dict[str, Any]] = {}
    missing: list[str] = []
    for name in names:
        if name in libraries:
            selected[name] = libraries[name]
        else:
            missing.append(name)

    if missing:
        raise ValueError(f"Unknown libraries: {', '.join(missing)}")
    return dict(sorted(selected.items()))


def normalize_name(raw: str) -> str:
    return re.sub(r"[^A-Za-z0-9._-]+", "-", raw).strip("-")


def now_stamp() -> str:
    return datetime.datetime.now().strftime("%Y%m%d-%H%M%S")


def unique_in_order(values: list[str]) -> list[str]:
    seen: set[str] = set()
    out: list[str] = []
    for value in values:
        if value in seen:
            continue
        seen.add(value)
        out.append(value)
    return out


def copy_path(src: Path, dst: Path) -> None:
    if src.is_dir():
        if dst.exists() and dst.is_file():
            dst.unlink()
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(src, dst, dirs_exist_ok=True)
        return
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)


def remove_path(path: Path) -> None:
    if not path.exists():
        return
    if path.is_dir():
        shutil.rmtree(path)
        return
    path.unlink()


def backup_then_remove(target: Path, backup_target: Path | None) -> None:
    if not target.exists():
        return
    if backup_target is not None:
        backup_target.parent.mkdir(parents=True, exist_ok=True)
        copy_path(target, backup_target)
    remove_path(target)


def detect_extracted_source_root(extract_dir: Path) -> Path:
    entries = [p for p in extract_dir.iterdir() if p.name != "__MACOSX"]
    if len(entries) == 1 and entries[0].is_dir():
        return entries[0]
    return extract_dir


def sync_config_for_library(lib_name: str, info: dict[str, Any], source_root: Path) -> dict[str, Any]:
    explicit = info.get("sync")
    if isinstance(explicit, dict):
        return explicit

    if (source_root / "src").exists() and (source_root / "include").exists():
        return {
            "clean": ["src", "include"],
            "copy": [
                {"from": "src", "to": "src"},
                {"from": "include", "to": "include"},
            ],
        }

    raise RuntimeError(
        f"No sync rule for '{lib_name}' and archive does not expose src+include. "
        "Add a 'sync' section in tools/third_party_releases.json."
    )


def describe_error(exc: Exception) -> str:
    if isinstance(exc, urllib.error.HTTPError):
        if exc.code == 403:
            return "HTTP 403 (GitHub API rate limit). Use --github-token or set GITHUB_TOKEN."
        if exc.code == 404:
            return "HTTP 404 (not found). Check repo/tag/channel in manifest."
        return f"HTTP {exc.code}: {exc.reason}"
    return str(exc)


def headers_with_optional_token(token: str | None) -> dict[str, str]:
    headers = {"User-Agent": USER_AGENT, "Accept": "application/vnd.github+json"}
    if token:
        headers["Authorization"] = f"Bearer {token}"
    return headers


def http_get_json(url: str, token: str | None) -> Any:
    req = urllib.request.Request(url, headers=headers_with_optional_token(token))
    with urllib.request.urlopen(req, timeout=DEFAULT_TIMEOUT_SECONDS) as response:
        return json.loads(response.read().decode("utf-8"))


def is_prerelease_tag(tag: str) -> bool:
    return bool(re.search(r"(alpha|beta|rc|preview|pre)", tag, flags=re.IGNORECASE))


def looks_like_version_tag(tag: str) -> bool:
    return bool(re.match(r"(?i)^(v?\d+(?:\.\d+)*|ver-\d+(?:-\d+)*)$", tag))


def github_latest_release(repo: str, token: str | None) -> dict[str, str] | None:
    url = f"https://api.github.com/repos/{repo}/releases/latest"
    try:
        data = http_get_json(url, token)
    except urllib.error.HTTPError as exc:
        if exc.code == 404:
            return None
        raise
    return {
        "tag": data["tag_name"],
        "tar_url": data["tarball_url"],
        "zip_url": data["zipball_url"],
        "html_url": data["html_url"],
        "source": "release",
    }


def github_latest_tag(repo: str, token: str | None, stable_only: bool = True) -> dict[str, str]:
    url = f"https://api.github.com/repos/{repo}/tags?per_page=100"
    data = http_get_json(url, token)
    if not isinstance(data, list) or not data:
        raise RuntimeError(f"No tags found for {repo}")
    tag = ""
    for item in data:
        candidate = item["name"]
        if stable_only and is_prerelease_tag(candidate):
            continue
        if stable_only and not looks_like_version_tag(candidate):
            continue
        tag = candidate
        break
    if not tag:
        tag = data[0]["name"]
    return {
        "tag": tag,
        "tar_url": f"https://api.github.com/repos/{repo}/tarball/{urllib.parse.quote(tag)}",
        "zip_url": f"https://api.github.com/repos/{repo}/zipball/{urllib.parse.quote(tag)}",
        "html_url": f"https://github.com/{repo}/tree/{urllib.parse.quote(tag)}",
        "source": "tag",
    }


def resolve_latest(repo: str, channel: str, token: str | None, allow_prerelease: bool = False) -> dict[str, str]:
    if channel == "release":
        rel = github_latest_release(repo, token)
        if rel:
            return rel
        return github_latest_tag(repo, token, stable_only=not allow_prerelease)
    if channel == "tag":
        return github_latest_tag(repo, token, stable_only=not allow_prerelease)
    raise ValueError(f"Unsupported channel '{channel}' for repo {repo}")


def archive_url_for_tag(repo: str, tag: str, archive_kind: str) -> str:
    base = f"https://github.com/{repo}/archive/refs/tags/{urllib.parse.quote(tag)}"
    if archive_kind == "tar.gz":
        return f"{base}.tar.gz"
    if archive_kind == "zip":
        return f"{base}.zip"
    raise ValueError(f"Unsupported archive kind: {archive_kind}")


def download_file(url: str, output_path: Path, token: str | None) -> int:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    req = urllib.request.Request(url, headers=headers_with_optional_token(token))
    total = 0
    with urllib.request.urlopen(req, timeout=DEFAULT_TIMEOUT_SECONDS) as response, output_path.open("wb") as dst:
        while True:
            chunk = response.read(1024 * 64)
            if not chunk:
                break
            dst.write(chunk)
            total += len(chunk)
    return total


def extract_archive(archive_path: Path, output_dir: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    suffixes = "".join(archive_path.suffixes).lower()
    if suffixes.endswith(".tar.gz") or suffixes.endswith(".tgz"):
        with tarfile.open(archive_path, "r:gz") as tar:
            for member in tar.getmembers():
                target = (output_dir / member.name).resolve()
                if output_dir.resolve() not in target.parents and target != output_dir.resolve():
                    raise ValueError(f"Blocked unsafe tar entry: {member.name}")
            tar.extractall(path=output_dir)
        return
    if suffixes.endswith(".zip"):
        with zipfile.ZipFile(archive_path, "r") as zf:
            for member in zf.namelist():
                target = (output_dir / member).resolve()
                if output_dir.resolve() not in target.parents and target != output_dir.resolve():
                    raise ValueError(f"Blocked unsafe zip entry: {member}")
            zf.extractall(path=output_dir)
        return
    raise ValueError(f"Unsupported archive format for extraction: {archive_path.name}")


def command_list(manifest: dict[str, Any]) -> int:
    libs = dict(sorted(manifest["libraries"].items()))
    print("Tracked third-party sources:")
    for name, info in libs.items():
        module = info.get("module") or "-"
        repo = info["repo"]
        channel = info.get("channel", "release")
        archive = info.get("default_archive", "tar.gz")
        print(f"  {name:16} module={module:12} repo={repo:32} channel={channel:7} archive={archive}")
    return 0


def command_check(manifest: dict[str, Any], names: list[str], token: str | None) -> int:
    libs = select_libraries(manifest, names)
    exit_code = 0
    for name, info in libs.items():
        repo = info["repo"]
        channel = info.get("channel", "release")
        allow_prerelease = bool(info.get("allow_prerelease", False))
        try:
            latest = resolve_latest(repo, channel, token, allow_prerelease=allow_prerelease)
            print(
                f"{name:16} latest={latest['tag']:20} source={latest['source']:7} "
                f"repo={repo} url={latest['html_url']}"
            )
        except Exception as exc:  # noqa: BLE001 - show all check errors but keep iterating
            print(f"{name:16} [error] {describe_error(exc)}")
            exit_code = 1
    return exit_code


def command_fetch(
    manifest: dict[str, Any],
    names: list[str],
    token: str | None,
    output_dir: Path,
    archive_kind: str | None,
    extract: bool,
    tag: str | None,
) -> int:
    libs = select_libraries(manifest, names)
    exit_code = 0

    for name, info in libs.items():
        repo = info["repo"]
        default_archive = info.get("default_archive", "tar.gz")
        chosen_archive = archive_kind or default_archive
        safe_name = normalize_name(name)

        try:
            if tag:
                resolved_tag = tag
                url = archive_url_for_tag(repo, resolved_tag, chosen_archive)
                source_kind = "manual-tag"
            else:
                latest = resolve_latest(
                    repo,
                    info.get("channel", "release"),
                    token,
                    allow_prerelease=bool(info.get("allow_prerelease", False)),
                )
                resolved_tag = latest["tag"]
                url = latest["tar_url"] if chosen_archive == "tar.gz" else latest["zip_url"]
                source_kind = latest["source"]

            ext = ".tar.gz" if chosen_archive == "tar.gz" else ".zip"
            archive_name = normalize_name(f"{safe_name}-{resolved_tag}") + ext
            archive_path = output_dir / archive_name

            size = download_file(url, archive_path, token)
            print(
                f"{name:16} downloaded {archive_path} ({size} bytes) "
                f"tag={resolved_tag} source={source_kind}"
            )

            if extract:
                extract_dir = output_dir / normalize_name(f"{safe_name}-{resolved_tag}")
                extract_archive(archive_path, extract_dir)
                print(f"{name:16} extracted  {extract_dir}")
        except Exception as exc:  # noqa: BLE001 - keep batch behavior
            print(f"{name:16} [error] {describe_error(exc)}")
            exit_code = 1

    return exit_code


def command_sync(
    manifest: dict[str, Any],
    names: list[str],
    token: str | None,
    repo_root: Path,
    output_dir: Path,
    archive_kind: str | None,
    clean: bool,
    backup_dir: Path | None,
    tag: str | None,
    dry_run: bool,
) -> int:
    libs = select_libraries(manifest, names)
    exit_code = 0

    for name, info in libs.items():
        module = (info.get("module") or "").strip()
        if not module:
            print(f"{name:16} [skip] no module mapping (metadata-only entry).")
            continue

        module_dir = (repo_root / "modules" / module).resolve()
        if not module_dir.is_dir():
            print(f"{name:16} [error] Module directory not found: {module_dir}")
            exit_code = 1
            continue

        repo = info["repo"]
        chosen_archive = archive_kind or info.get("default_archive", "tar.gz")

        try:
            if tag:
                resolved_tag = tag
                url = archive_url_for_tag(repo, resolved_tag, chosen_archive)
                source_kind = "manual-tag"
            else:
                latest = resolve_latest(
                    repo,
                    info.get("channel", "release"),
                    token,
                    allow_prerelease=bool(info.get("allow_prerelease", False)),
                )
                resolved_tag = latest["tag"]
                url = latest["tar_url"] if chosen_archive == "tar.gz" else latest["zip_url"]
                source_kind = latest["source"]

            safe_name = normalize_name(name)
            ext = ".tar.gz" if chosen_archive == "tar.gz" else ".zip"
            archive_name = normalize_name(f"{safe_name}-{resolved_tag}") + ext
            archive_path = output_dir / archive_name
            extract_dir = output_dir / "_extract" / normalize_name(f"{safe_name}-{resolved_tag}")
            stamp = now_stamp()
            lib_backup_root = backup_dir / module / stamp if backup_dir else None

            if dry_run:
                print(
                    f"{name:16} [dry-run] module={module} tag={resolved_tag} source={source_kind} "
                    f"archive={archive_path}"
                )
                continue

            size = download_file(url, archive_path, token)
            print(
                f"{name:16} downloaded {archive_path} ({size} bytes) "
                f"tag={resolved_tag} source={source_kind}"
            )

            if extract_dir.exists():
                remove_path(extract_dir)
            extract_archive(archive_path, extract_dir)
            source_root = detect_extracted_source_root(extract_dir)

            sync_cfg = sync_config_for_library(name, info, source_root)
            copy_rules = sync_cfg.get("copy", [])
            if not isinstance(copy_rules, list) or not copy_rules:
                raise RuntimeError(f"Invalid sync.copy rules for '{name}'")

            clean_targets = sync_cfg.get("clean", [])
            if not clean_targets:
                clean_targets = [str(rule.get("to", "")).strip() for rule in copy_rules]
                clean_targets = [x for x in clean_targets if x]
            if not isinstance(clean_targets, list):
                raise RuntimeError(f"Invalid sync.clean rules for '{name}'")
            clean_targets = unique_in_order([str(x).strip() for x in clean_targets if str(x).strip()])

            if clean:
                for rel in clean_targets:
                    target = (module_dir / rel).resolve()
                    if target == module_dir:
                        raise RuntimeError(f"Refusing to clean module root for '{name}'")
                    if module_dir not in target.parents:
                        raise RuntimeError(f"Refusing to clean outside module dir: {target}")
                    backup_target = (lib_backup_root / rel) if lib_backup_root else None
                    backup_then_remove(target, backup_target)
                    if target.exists():
                        raise RuntimeError(f"Failed to clean target: {target}")

            for rule in copy_rules:
                if not isinstance(rule, dict):
                    raise RuntimeError(f"Invalid sync.copy entry for '{name}': {rule!r}")
                src_rel = str(rule.get("from", "")).strip()
                dst_rel = str(rule.get("to", "")).strip()
                if not src_rel or not dst_rel:
                    raise RuntimeError(f"Invalid sync.copy mapping for '{name}': {rule!r}")

                src_path = (source_root / src_rel).resolve()
                dst_path = (module_dir / dst_rel).resolve()
                if source_root not in src_path.parents and src_path != source_root:
                    raise RuntimeError(f"Invalid source path for '{name}': {src_path}")
                if module_dir not in dst_path.parents and dst_path != module_dir:
                    raise RuntimeError(f"Invalid target path for '{name}': {dst_path}")
                if not src_path.exists():
                    raise RuntimeError(f"Missing source path in archive: {src_rel}")

                copy_path(src_path, dst_path)

            print(f"{name:16} synced -> {module_dir}")
        except Exception as exc:  # noqa: BLE001 - keep batch behavior
            print(f"{name:16} [error] {describe_error(exc)}")
            exit_code = 1

    return exit_code


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Download third-party release archives without git clone.")
    parser.add_argument(
        "--manifest",
        default=str(DEFAULT_MANIFEST),
        help=f"Path to manifest JSON (default: {DEFAULT_MANIFEST})",
    )
    parser.add_argument(
        "--github-token",
        default="",
        help="Optional GitHub token (or set GITHUB_TOKEN env var) to increase rate limits.",
    )

    sub = parser.add_subparsers(dest="command", required=True)

    sub.add_parser("list", help="List tracked libraries from the manifest.")

    check = sub.add_parser("check", help="Check latest release/tag for selected libraries.")
    check.add_argument("libraries", nargs="*", help="Library names or 'all' (default: all)")

    fetch = sub.add_parser("fetch", help="Download source archives for selected libraries.")
    fetch.add_argument("libraries", nargs="*", help="Library names or 'all' (default: all)")
    fetch.add_argument(
        "--out-dir",
        default=str(DEFAULT_OUTPUT_DIR),
        help=f"Output directory for downloaded archives (default: {DEFAULT_OUTPUT_DIR})",
    )
    fetch.add_argument(
        "--archive",
        choices=("tar.gz", "zip"),
        default=None,
        help="Force archive format; defaults to library preference in manifest.",
    )
    fetch.add_argument(
        "--tag",
        default=None,
        help="Download this exact tag instead of resolving latest for each library.",
    )
    fetch.add_argument(
        "--extract",
        action="store_true",
        help="Extract downloaded archives into subfolders.",
    )

    sync = sub.add_parser(
        "sync",
        help="Download/extract and sync upstream src/include into modules/<name> with optional clean+backup.",
    )
    sync.add_argument("libraries", nargs="*", help="Library names or 'all' (default: all)")
    sync.add_argument(
        "--repo-root",
        default=str(DEFAULT_REPO_ROOT),
        help=f"Repository root containing modules/ (default: {DEFAULT_REPO_ROOT})",
    )
    sync.add_argument(
        "--out-dir",
        default=str(DEFAULT_OUTPUT_DIR),
        help=f"Archive/cache output directory (default: {DEFAULT_OUTPUT_DIR})",
    )
    sync.add_argument(
        "--archive",
        choices=("tar.gz", "zip"),
        default=None,
        help="Force archive format; defaults to library preference in manifest.",
    )
    sync.add_argument(
        "--tag",
        default=None,
        help="Use an exact tag for all selected libraries.",
    )
    sync.add_argument(
        "--no-clean",
        action="store_true",
        help="Do not delete existing target paths before copying (can cause mixed trees).",
    )
    sync.add_argument(
        "--backup-dir",
        default=str(DEFAULT_BACKUP_DIR),
        help=f"Backup directory used before clean (default: {DEFAULT_BACKUP_DIR}).",
    )
    sync.add_argument(
        "--no-backup",
        action="store_true",
        help="Disable backup before clean.",
    )
    sync.add_argument(
        "--dry-run",
        action="store_true",
        help="Print planned sync actions without modifying files.",
    )
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    manifest_path = Path(args.manifest).resolve()
    manifest = load_manifest(manifest_path)

    token = args.github_token or os.environ.get("GITHUB_TOKEN", "")
    token = token.strip() or None

    if args.command == "list":
        return command_list(manifest)
    if args.command == "check":
        return command_check(manifest, args.libraries, token)
    if args.command == "fetch":
        return command_fetch(
            manifest=manifest,
            names=args.libraries,
            token=token,
            output_dir=Path(args.out_dir).resolve(),
            archive_kind=args.archive,
            extract=args.extract,
            tag=args.tag,
        )
    if args.command == "sync":
        clean = not args.no_clean
        backup_dir = None if args.no_backup else Path(args.backup_dir).resolve()
        return command_sync(
            manifest=manifest,
            names=args.libraries,
            token=token,
            repo_root=Path(args.repo_root).resolve(),
            output_dir=Path(args.out_dir).resolve(),
            archive_kind=args.archive,
            clean=clean,
            backup_dir=backup_dir,
            tag=args.tag,
            dry_run=args.dry_run,
        )

    parser.error(f"Unsupported command: {args.command}")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
