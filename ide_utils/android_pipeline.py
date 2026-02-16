import os
import re
import time
import zipfile

from ide_utils.fs_utils import createFolderTree
from ide_utils.process_utils import cmd_args_to_str, runProcess


def _sanitize_android_package(package_name, fallback="com.djokersoft.game"):
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


def _has_android_resource(res_root, resource_ref):
    if not resource_ref or not resource_ref.startswith("@"):
        return True

    if resource_ref.startswith("@android:"):
        return True

    value = resource_ref[1:]
    if "/" not in value:
        return False

    res_type, res_name = value.split("/", 1)
    if not res_type or not res_name:
        return False

    if not os.path.isdir(res_root):
        return False

    for entry in os.listdir(res_root):
        entry_path = os.path.join(res_root, entry)
        if not os.path.isdir(entry_path):
            continue
        if entry != res_type and not entry.startswith(res_type + "-"):
            continue
        for filename in os.listdir(entry_path):
            if os.path.splitext(filename)[0] == res_name:
                return True
    return False


def _ensure_manifest_icon_fallback(parent, manifest_path, res_root):
    try:
        with open(manifest_path, "r", encoding="utf-8") as text_file:
            manifest_data = text_file.read()

        icon_match = re.search(r'android:icon="(@[^"]+)"', manifest_data)
        if not icon_match:
            return

        icon_ref = icon_match.group(1)
        if _has_android_resource(res_root, icon_ref):
            return

        fallback = "@android:drawable/sym_def_app_icon"
        patched = (
            manifest_data[: icon_match.start(1)]
            + fallback
            + manifest_data[icon_match.end(1) :]
        )
        with open(manifest_path, "w", encoding="utf-8") as text_file:
            text_file.write(patched)
        parent.trace("Missing icon resource ", icon_ref, ", using ", fallback)
    except Exception as e:
        parent.trace("Warn: failed to verify Android icon resource: ", type(e).__name__, " ", e)


def _zip_add_tree(zip_file, src_root, dst_root):
    if not os.path.isdir(src_root):
        return 0

    count = 0
    base = dst_root.strip("/").replace("\\", "/")
    for root, _, files in os.walk(src_root):
        for name in files:
            src_path = os.path.join(root, name)
            rel = os.path.relpath(src_path, src_root).replace(os.path.sep, "/")
            arcname = base + "/" + rel if base else rel
            zip_file.write(src_path, arcname)
            count += 1
    return count


def build_native_manifest(template, package_name, label, activity, lib_name):
    data = template
    data = data.replace("@apppkg@", package_name)
    data = data.replace("@applbl@", label)
    data = data.replace("@appact@", activity)
    data = data.replace("@appLIBNAME@", lib_name)
    return data


def android_remove(parent, android_sdk, package_name):
    parent.trace("Try remove app ...")
    args = ["uninstall", package_name]
    code, out, err = runProcess(android_sdk + "/platform-tools/adb", args)
    if code != 0:
        parent.trace("Error  uninstall  apk :" + err.decode("utf-8"))
        return False
    parent.trace(out.decode("utf-8"))
    return True


def android_install_app(parent, android_sdk, app_signed):
    parent.trace("Try install app ...")
    args = ["install", "-r", app_signed]
    code, out, err = runProcess(android_sdk + "/platform-tools/adb", args)
    if code != 0:
        parent.trace("Error  installing  apk :" + err.decode("utf-8"))
        return False
    return True


def android_compile_java_native(
    parent,
    main_root,
    app_type,
    app_name,
    android_pack,
    android_label,
    android_activity,
    run_app,
    *,
    android_sdk,
    aapt,
    dx,
    dx8=None,
    apksigner,
    platform,
    keytool,
    native_manifest_template,
):
    osp = os.path.sep
    original_pack = android_pack
    android_pack = _sanitize_android_package(android_pack)
    if original_pack != android_pack:
        parent.trace("Fix package name:", original_pack, "->", android_pack)

    if not android_label or not android_label.strip():
        android_label = app_name

    bin_folder = main_root + osp + "Android" + osp + app_name + osp

    if not os.path.exists(bin_folder):
        os.mkdir(bin_folder)

    print(main_root)
    print(app_type)
    print(app_name)
    print(bin_folder)

    res = bin_folder + "res"
    if not os.path.exists(res):
        parent.trace("*** Create :" + res)
        createFolderTree(res)

    java = bin_folder + "java"
    if not os.path.exists(java):
        parent.trace("** Create :" + java)
        os.mkdir(java)

    tmp = bin_folder + "tmp"
    if not os.path.exists(tmp):
        parent.trace("** Create :" + tmp)
        createFolderTree(tmp)

    java_out = bin_folder + "out"
    if not os.path.exists(java_out):
        parent.trace("** Create :" + java_out)
        os.mkdir(java_out)

    dex_files = bin_folder + "dex"
    if not os.path.exists(dex_files):
        parent.trace("*** Create :" + dex_files)
        os.mkdir(dex_files)

    java_file_dirs = java + osp + android_pack.replace(".", osp)
    createFolderTree(java_file_dirs)

    if app_type == 0:
        manif_file = bin_folder + "AndroidManifest.xml"
        should_write_manifest = True
        if os.path.exists(manif_file):
            try:
                with open(manif_file, "r", encoding="utf-8") as text_file:
                    existing_manifest = text_file.read()

                package_match = re.search(r'package="([^"]+)"', existing_manifest)
                activity_match = re.search(r'<activity android:name="([^"]+)"', existing_manifest)
                lib_match = re.search(
                    r'android:name="android\.app\.lib_name"[\s\S]*?android:value="([^"]+)"',
                    existing_manifest,
                )

                existing_package = package_match.group(1) if package_match else ""
                existing_activity = activity_match.group(1) if activity_match else ""
                existing_lib = lib_match.group(1) if lib_match else ""

                if (
                    existing_package == android_pack
                    and existing_activity == android_activity
                    and existing_lib == app_name
                ):
                    should_write_manifest = False
            except Exception:
                should_write_manifest = True

        if should_write_manifest:
            manifest_data = build_native_manifest(
                native_manifest_template,
                android_pack,
                android_label,
                android_activity,
                app_name,
            )
            with open(manif_file, "w", encoding="utf-8") as text_file:
                text_file.write(manifest_data)
    else:
        manif_file = bin_folder + "AndroidManifest.xml"

    _ensure_manifest_icon_fallback(parent, manif_file, res)

    debug_key = bin_folder + app_name + ".key"
    if not os.path.exists(debug_key):
        parent.trace(" Generate " + debug_key + " keystor")
        args = [
            "-genkeypair",
            "-validity",
            "1000",
            "-dname",
            "CN=djokersoft,O=Android,C=PT",
            "-keystore",
            debug_key,
            "-storepass",
            "14781478",
            "-keypass",
            "14781478",
            "-alias",
            "djokersoft",
            "-keyalg",
            "RSA",
        ]

        final_command = cmd_args_to_str(args)
        parent.trace(keytool + final_command)
        code, out, err = runProcess(keytool, args)
        if code != 0:
            parent.trace("Error on generate keystore:" + err.decode("utf-8"))
            return False
        parent.trace(out.decode("utf-8"))

    args = [
        "package",
        "-f",
        "-m",
        "-J",
        java,
        "-M",
        manif_file,
        "-S",
        res,
        "-I",
        platform,
    ]

    # Remove stale generated R.java from previous package names.
    for root, _, files in os.walk(java):
        for file in files:
            if file == "R.java" or file.startswith("R$"):
                try:
                    os.remove(os.path.join(root, file))
                except OSError:
                    pass

    parent.trace("Generate resources .")
    code, out, err = runProcess(aapt, args)
    if code != 0:
        parent.trace("Error on generate resources:" + err.decode("utf-8"))
        return False
    parent.trace(out.decode("utf-8"))
    parent.trace("Search java files ")

    java_src_files = []
    for root, _, files in os.walk(java):
        for file in files:
            if file.endswith(".java"):
                print(os.path.join(root, file))
                java_src_files.append(os.path.join(root, file))

    java_src_files.sort(reverse=True)

    has_java_sources = len(java_src_files) > 0

    for src in java_src_files:
        parent.trace("Compile " + src.strip())
        args = [
            "-nowarn",
            "-Xlint:none",
            "-J-Xmx2048m",
            "-Xlint:unchecked",
            "-source",
            "1.8",
            "-target",
            "1.8",
            "-d",
            java_out,
            "-classpath",
            platform + os.pathsep + java_out,
            "-sourcepath",
            java + os.pathsep + java + "/org" + os.pathsep + java_out,
            src,
        ]

        src_modified_time = os.path.getmtime(src)

        basename_without_ext = os.path.splitext(os.path.basename(src))[0]
        maindir = os.path.dirname(os.path.abspath(src))
        maindir = maindir.replace("java", "out")
        obj_name = maindir + os.path.sep + basename_without_ext + ".class"

        if os.path.exists(obj_name):
            obj_modified_time = os.path.getmtime(obj_name)
            if src_modified_time <= obj_modified_time:
                parent.trace("Skip " + src)
                continue

        code, out, err = runProcess("javac", args)
        if code != 0:
            parent.trace("Error  compiling :" + err.decode("utf-8"))
            return False
        parent.trace(out.decode("utf-8"))

    parent.trace("Java is compiled ...")

    # Remove stale dex files from previous builds.
    for root, _, files in os.walk(dex_files):
        for file in files:
            if file.endswith(".dex"):
                try:
                    os.remove(os.path.join(root, file))
                except OSError:
                    pass

    class_files = []
    for root, _, files in os.walk(java_out):
        for file in files:
            if file.endswith(".class"):
                class_files.append(os.path.join(root, file))

    parent.trace("Translating in Dalvik bytecode...")
    if not class_files:
        if has_java_sources:
            parent.trace("Warning: Java sources found but no .class generated, skipping dex.")
        else:
            parent.trace("No Java sources for this app, skipping dex (native-only APK).")
    else:
        code = 1
        out = b""
        err = b""

        # Prefer d8 when available, fallback to legacy dx.
        if dx8 and os.path.exists(dx8):
            args = ["--release", "--output", dex_files, "--lib", platform] + class_files
            code, out, err = runProcess(dx8, args)
            if code != 0:
                parent.trace("d8 failed, fallback to dx: " + err.decode("utf-8"))
            else:
                parent.trace(out.decode("utf-8"))

        if code != 0:
            args = ["--dex", "--output=" + dex_files + os.path.sep + "classes.dex"] + class_files
            code, out, err = runProcess(dx, args)
            if code != 0:
                parent.trace("Error  Translating java do dex :" + err.decode("utf-8"))
                return False
            parent.trace(out.decode("utf-8"))

    parent.trace("Making APK...")
    args = [
        "package",
        "-f",
        "-m",
        "-F",
        tmp + os.path.sep + app_name + ".unaligned.apk",
        "-M",
        manif_file,
        "-S",
        res,
        "-I",
        platform,
    ]

    code, out, err = runProcess(aapt, args)
    if code != 0:
        parent.trace("Error  packing apk :" + err.decode("utf-8"))
        return False
    parent.trace(out.decode("utf-8"))

    parent.trace("File is created in " + tmp + os.path.sep + app_name + ".unaligned.apk")

    zip_file = zipfile.ZipFile(tmp + os.path.sep + app_name + ".unaligned.apk", "a")
    build_output_arm = "armeabi-v7a"
    app_bin = main_root + osp + "Android" + osp + build_output_arm + os.path.sep + "lib" + app_name + ".so"

    if os.path.exists(app_bin):
        parent.trace("insert ", app_bin)
        zip_file.write(app_bin, "lib/armeabi-v7a/" + "lib" + app_name + ".so")
    else:
        parent.trace("missing ", app_bin)

    build_output_arm = "arm64-v8a"
    app_bin = main_root + osp + "Android" + osp + build_output_arm + os.path.sep + "lib" + app_name + ".so"

    if os.path.exists(app_bin):
        parent.trace("insert ", app_bin)
        zip_file.write(app_bin, "lib/arm64-v8a/" + "lib" + app_name + ".so")
    else:
        parent.trace("missing ", app_bin)

    asset_roots = [
        ("scripts", "assets/scripts"),
        ("assets", "assets/assets"),
        ("resources", "assets/resources"),
        ("data", "assets/data"),
        ("media", "assets/media"),
    ]
    for host_name, apk_name in asset_roots:
        host_dir = os.path.join(main_root, host_name)
        added = _zip_add_tree(zip_file, host_dir, apk_name)
        if added > 0:
            parent.trace("pack ", host_name, " -> ", apk_name, " (", added, " files)")

    dex_list_files = []
    parent.trace("look for dex files on ", dex_files)
    for root, _, files in os.walk(dex_files):
        for file in files:
            if file.endswith(".dex"):
                print("DEX: ", os.path.join(root, file), " filename :", os.path.basename(file))
                dex_list_files.append(os.path.join(root, file))

    for dex in dex_list_files:
        parent.trace("Insert ", dex, " to " + os.path.basename(dex))
        zip_file.write(dex, os.path.basename(dex))

    zip_file.close()

    app_signed = bin_folder + app_name + ".signed.apk"
    parent.trace("Sign app ")
    args = [
        "sign",
        "--ks",
        debug_key,
        "--ks-key-alias",
        "djokersoft",
        "--ks-pass",
        "pass:14781478",
        "--in",
        tmp + os.path.sep + app_name + ".unaligned.apk",
        "--out",
        app_signed,
    ]

    parent.trace(cmd_args_to_str(args))
    code, out, err = runProcess(apksigner, args)
    if code != 0:
        parent.trace("Error  packing apk :" + err.decode("utf-8"))
        return False
    parent.trace(out.decode("utf-8"))
    parent.trace("Build competed ;D ")

    stop_app = True
    if stop_app:
        parent.trace("Try stop  " + android_pack + "...")
        args = ["shell", "am", "force-stop", android_pack + "/." + android_activity]
        code, out, err = runProcess(android_sdk + "/platform-tools/adb", args)
        if code != 0:
            parent.trace("Error  stoping  apk :" + err.decode("utf-8"))
        parent.trace(out.decode("utf-8"))

    remove_app = False
    if remove_app:
        android_remove(parent, android_sdk, android_pack)

    install_app = True
    if install_app:
        if not android_install_app(parent, android_sdk, app_signed):
            if android_remove(parent, android_sdk, android_pack):
                android_install_app(parent, android_sdk, app_signed)

    if run_app:
        parent.trace("Try run app ...")
        args = ["shell", "am", "start", "-n", android_pack + "/" + android_activity]
        parent.trace(str(args))
        code, out, err = runProcess(android_sdk + "/platform-tools/adb", args)
        if code != 0:
            parent.trace("Error  running  apk :" + err.decode("utf-8"))
            return False
        parent.trace(out.decode("utf-8"))

    return True
