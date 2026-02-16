import json
import os
import re
from dataclasses import dataclass


@dataclass(frozen=True)
class ToolchainPaths:
    ANDROID_SDK: str
    ANDROID_NDK: str
    JAVA_SDK: str
    JAVA_LIB_RT: str
    JAVAFX: str
    AAPT: str
    DX: str
    DX8: str
    ZIPALIGN: str
    APKSIGNER: str
    PLATFORM: str


def _version_key(value):
    parts = re.findall(r"\d+", value)
    if not parts:
        return (0,)
    return tuple(int(part) for part in parts)


def _read_config(config_path):
    if not config_path or not os.path.exists(config_path):
        return {}
    try:
        with open(config_path, "r", encoding="utf-8") as handle:
            return json.load(handle)
    except Exception:
        return {}


def _get_toolchain_config(data):
    if not isinstance(data, dict):
        return {}

    if isinstance(data.get("Toolchain"), dict):
        return data["Toolchain"]

    configuration = data.get("Configuration")
    if isinstance(configuration, dict) and isinstance(configuration.get("Toolchain"), dict):
        return configuration["Toolchain"]

    return {}


def _pick_latest_subdir(root_dir):
    if not os.path.isdir(root_dir):
        return None

    candidates = [
        name
        for name in os.listdir(root_dir)
        if os.path.isdir(os.path.join(root_dir, name))
    ]
    if not candidates:
        return None
    return max(candidates, key=_version_key)


def _select_build_tools_version(sdk_root, preferred):
    build_tools_root = os.path.join(sdk_root, "build-tools")
    if preferred and os.path.isdir(os.path.join(build_tools_root, preferred)):
        return preferred

    latest = _pick_latest_subdir(build_tools_root)
    return latest or preferred


def _select_platform_version(sdk_root, preferred):
    platforms_root = os.path.join(sdk_root, "platforms")
    preferred_jar = os.path.join(platforms_root, preferred or "", "android.jar")
    if preferred and os.path.isfile(preferred_jar):
        return preferred

    if not os.path.isdir(platforms_root):
        return preferred

    candidates = []
    for name in os.listdir(platforms_root):
        platform_dir = os.path.join(platforms_root, name)
        android_jar = os.path.join(platform_dir, "android.jar")
        if os.path.isdir(platform_dir) and os.path.isfile(android_jar):
            candidates.append(name)

    if not candidates:
        return preferred
    return max(candidates, key=_version_key)


def _select_ndk_path(sdk_root, preferred_ndk):
    if preferred_ndk and os.path.isdir(preferred_ndk):
        return preferred_ndk

    ndk_root = os.path.join(sdk_root, "ndk")
    latest = _pick_latest_subdir(ndk_root)
    if latest:
        return os.path.join(ndk_root, latest)

    return preferred_ndk


def resolve_toolchain_paths(
    config_path,
    default_sdk,
    default_ndk,
    default_java,
    preferred_build_tools,
    preferred_platform,
):
    config = _get_toolchain_config(_read_config(config_path))

    android_sdk = (
        os.environ.get("ANDROID_SDK_ROOT")
        or os.environ.get("ANDROID_HOME")
        or config.get("AndroidSdk")
        or default_sdk
    )

    android_ndk = (
        os.environ.get("ANDROID_NDK_ROOT")
        or config.get("AndroidNdk")
        or default_ndk
    )
    android_ndk = _select_ndk_path(android_sdk, android_ndk)

    java_sdk = os.environ.get("JAVA_HOME") or config.get("JavaSdk") or default_java

    build_tools_version = (
        os.environ.get("CROSSIDE_BUILD_TOOLS")
        or config.get("BuildTools")
        or preferred_build_tools
    )
    build_tools_version = _select_build_tools_version(android_sdk, build_tools_version)

    platform_version = (
        os.environ.get("CROSSIDE_PLATFORM")
        or config.get("Platform")
        or preferred_platform
    )
    platform_version = _select_platform_version(android_sdk, platform_version)

    build_tools_root = os.path.join(android_sdk, "build-tools", build_tools_version)
    platform_jar = os.path.join(android_sdk, "platforms", platform_version, "android.jar")

    return ToolchainPaths(
        ANDROID_SDK=android_sdk,
        ANDROID_NDK=android_ndk,
        JAVA_SDK=java_sdk,
        JAVA_LIB_RT=os.path.join(java_sdk, "jre", "lib", "rt.jar"),
        JAVAFX=os.path.join(java_sdk, "lib", "javafx-mx.jar"),
        AAPT=os.path.join(build_tools_root, "aapt"),
        DX=os.path.join(build_tools_root, "dx"),
        DX8=os.path.join(build_tools_root, "d8"),
        ZIPALIGN=os.path.join(build_tools_root, "zipalign"),
        APKSIGNER=os.path.join(build_tools_root, "apksigner"),
        PLATFORM=platform_jar,
    )

