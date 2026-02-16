import os
import re
import time
from glob import glob

from ide_utils.fs_utils import createFolderTree
from ide_utils.process_utils import runProcess


def _ndk_prebuilt_root(android_ndk):
    return android_ndk + "/toolchains/llvm/prebuilt/linux-x86_64"


def _android_triple(arm):
    if arm == 1:
        return "aarch64-linux-android", "aarch64"
    return "arm-linux-androideabi", "arm"


def _ndk_cpp_include_dir(android_ndk):
    return _ndk_prebuilt_root(android_ndk) + "/sysroot/usr/include/c++/v1"


def _ndk_cpp_runtime_libs(android_ndk, arm):
    prebuilt = _ndk_prebuilt_root(android_ndk)
    triple, unwind_arch = _android_triple(arm)
    libcxx = prebuilt + "/sysroot/usr/lib/" + triple + "/libc++_static.a"
    libcxxabi = prebuilt + "/sysroot/usr/lib/" + triple + "/libc++abi.a"

    unwind = None
    unwind_candidates = sorted(glob(prebuilt + "/lib/clang/*/lib/linux/" + unwind_arch + "/libunwind.a"))
    if unwind_candidates:
        unwind = unwind_candidates[-1]

    return libcxx, libcxxabi, unwind


def android_builder(
    parent,
    folder_root,
    name,
    srcs,
    cargs,
    cppargs,
    ldargs,
    build_type=0,
    arm=0,
    full_build=False,
    *,
    android_ndk,
):
    use_cpp = False
    link_cpp = False

    osp = os.path.sep
    build_plataform = "23"
    build_output_arm = "armeabi-v7a"
    build_arch = "armv7a"

    arm_target = "armv7a-linux-androideabi21"

    if arm == 1:
        build_output_arm = "arm64-v8a"
        build_arch = "aarch64"
        arm_target = "aarch64-linux-android21"

    prebuilt_root = _ndk_prebuilt_root(android_ndk)
    cc = prebuilt_root + "/bin/clang"
    cpp = prebuilt_root + "/bin/clang++"
    ar = prebuilt_root + "/bin/llvm-ar"
    strip = prebuilt_root + "/bin/llvm-strip"

    out_folder = folder_root + osp + "obj" + osp + "Android" + osp + name + osp + build_output_arm
    bin_folder = folder_root + osp + "Android" + osp + name + osp + name + osp + build_output_arm
    createFolderTree(out_folder)
    createFolderTree(bin_folder)
    objs_list = []
    args = []

    ctype = cc

    c_extencions = [".c", ".cc"]
    cpp_extencions = [".cpp", "xpp"]
    for src in srcs:
        ctype = cc
        use_cpp = False
        if not os.path.isfile(src):
            parent.trace("File not exists")
            continue

        src_folder = os.path.dirname(os.path.abspath(src))
        obj_folder = out_folder + src_folder.replace(folder_root, "")

        createFolderTree(obj_folder)
        filename, file_extension = os.path.splitext(src)
        basename = os.path.basename(src)
        basename_without_ext = os.path.splitext(os.path.basename(src))[0]
        obj_name = obj_folder + os.path.sep + basename_without_ext + ".o"
        objs_list.append(obj_name)
        src_modified_time = os.path.getmtime(src)

        args.clear()
        if not full_build:
            if os.path.exists(obj_name):
                obj_modified_time = os.path.getmtime(obj_name)
                if src_modified_time <= obj_modified_time:
                    parent.trace("Skip  file" + src)
                    continue

        if file_extension in cpp_extencions:
            ctype = cpp
            use_cpp = True
            link_cpp = True

        parent.trace(ctype, " ", os.path.basename(src), ">", os.path.basename(obj_name))

        args.append("-target")
        args.append(arm_target)
        args.append("-fdata-sections")
        args.append("-ffunction-sections")
        args.append("-fstack-protector-strong")
        args.append("-funwind-tables")
        args.append("-no-canonical-prefixes")
        args.append("--sysroot")
        args.append(android_ndk + "/toolchains/llvm/prebuilt/linux-x86_64/sysroot")
        args.append("-g")
        args.append("-Wno-invalid-command-line-argument")
        args.append("-Wno-unused-command-line-argument")
        args.append("-D_FORTIFY_SOURCE=2")
        args.append("-fno-exceptions")
        args.append("-fno-rtti")

        args.append("-fpic")

        if arm == 0:
            args.append("-march=armv7-a")
            args.append("-mthumb")
            args.append("-Oz")
        elif arm == 1:
            args.append("-O2")
        args.append("-DNDEBUG")

        if arm == 0:
            args.append(
                "-I"
                + android_ndk
                + "/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/arm-linux-androideabi"
            )
        elif arm == 1:
            args.append(
                "-I"
                + android_ndk
                + "/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/aarch64-linux-android"
            )

        args.append("-I" + android_ndk + "/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include")

        args.append("-I" + folder_root)
        args.append("-I" + src_folder)

        args.append("-DANDROID")
        if use_cpp:
            args.append("-nostdinc++")
            args.append("-I" + _ndk_cpp_include_dir(android_ndk))
        args.append("-Wformat")
        args.append("-Werror=format-security")
        args.append("-fno-strict-aliasing")
        args.append("-DPLATFORM_ANDROID")

        if use_cpp:
            for arg in cppargs:
                value = arg.strip()
                if len(value) >= 2:
                    args.append(arg)
        else:
            for arg in cargs:
                value = arg.strip()
                if len(value) >= 2:
                    args.append(arg)

        args.append("-c")
        args.append(src)
        args.append("-o")
        args.append(obj_name)

        code, out, err = runProcess(ctype, args)
        if code != 0:
            parent.trace(err.decode("utf-8"))
            rexp = ":(.*?):(.*?): error: "
            erro = re.search(rexp, err.decode("utf-8"))
            try:
                line_y = 0
                line_x = 0
                if erro:
                    linhas = erro.group().split(":")
                    line_x = int(linhas[2])
                    line_y = int(linhas[1])
            except Exception:
                print("Erro unknow .. ")
            return False
        parent.trace(out.decode("utf-8"))
    parent.trace("Compiling completed")

    if build_type == 0 or build_type == 1:
        args = []
        export = bin_folder + osp + "lib" + name + ".so"
        parent.trace("Build app ", build_arch, " ", export)

        args.append("-Wl,-soname," + "lib" + name + ".so")
        args.append("-shared")

        objs = ""
        for obj in objs_list:
            objs += obj + " "
            args.append(obj)

        root_folder = os.getcwd() + "/libs/android/" + build_output_arm
        args.append("-L" + root_folder)

        if link_cpp:
            args.append("-L" + prebuilt_root + "/sysroot/usr/lib")

        for arg in ldargs:
            value = arg.strip()
            if len(value) > 1:
                args.append(value)

        args.append("-Wl,--no-whole-archive")

        if link_cpp:
            libcxx, libcxxabi, libunwind = _ndk_cpp_runtime_libs(android_ndk, arm)
            if os.path.exists(libcxx):
                args.append(libcxx)
            if os.path.exists(libcxxabi):
                args.append(libcxxabi)
            if libunwind and os.path.exists(libunwind):
                args.append(libunwind)

        args.append("-target")
        if arm == 0:
            args.append("armv7a-linux-androideabi21")

        elif arm == 1:
            args.append("aarch64-linux-android21")
        args.append("--sysroot")
        args.append(prebuilt_root + "/sysroot")

        args.append("-no-canonical-prefixes")
        args.append("-Wl,--build-id")

        if link_cpp:
            args.append("-nostdlib++")
        args.append("-Wl,--no-undefined")
        args.append("-Wl,--fatal-warnings")

        args.append("-o")
        args.append(export)

        if link_cpp:
            ctype = cpp

        final_cmd = str(args)
        final_cmd = final_cmd.replace("'", " ")
        final_cmd = final_cmd.replace(",", " ")
        final_cmd = final_cmd.replace("[", " ")
        final_cmd = final_cmd.replace("]", " ")
        print(ctype, " ", final_cmd)
        code, out, err = runProcess(ctype, args)
        if code != 0:
            parent.trace(err.decode("utf-8"))
            rexp = ":(.*?):(.*?): error:"
            return False
        parent.trace(out.decode("utf-8"))

        parent.trace("Strip library ")
        code, out, err = runProcess(strip, ["--strip-unneeded", export])
        if code != 0:
            parent.trace(err.decode("utf-8"))
            rexp = ":(.*?):(.*?): error:"
            return False
        parent.trace(out.decode("utf-8"))
        parent.trace("Native Done :) ")
        return True

    if build_type == 2:
        parent.trace("Build static lib")
        args = []
        export = bin_folder + osp + "lib" + name + ".a"
        args.append("rcs")
        args.append(export)
        objs = ""
        for obj in objs_list:
            objs += obj + " "
            args.append(obj)

        final_cmd = str(args)
        final_cmd = final_cmd.replace("'", " ")
        final_cmd = final_cmd.replace(",", " ")
        final_cmd = final_cmd.replace("[", " ")
        final_cmd = final_cmd.replace("]", " ")
        parent.trace(ctype, " ", final_cmd)
        code, out, err = runProcess(ar, args)
        if code != 0:
            parent.trace(err.decode("utf-8"))
            rexp = ":(.*?):(.*?): error:"
            return False
        parent.trace(out.decode("utf-8"))
        parent.trace("Static build completed :) ")
        return True

    return False


def android_compile(
    parent,
    folder_root,
    name,
    srcs,
    cargs,
    cppargs,
    arm=0,
    full_build=False,
    *,
    android_ndk,
):
    use_cpp = False
    link_cpp = False

    osp = os.path.sep
    build_plataform = "23"
    build_output_arm = "armeabi-v7a"
    build_arch = "armv7a"

    arm_target = "armv7a-linux-androideabi21"

    if arm == 1:
        build_output_arm = "arm64-v8a"
        build_arch = "aarch64"
        arm_target = "aarch64-linux-android21"

    prebuilt_root = _ndk_prebuilt_root(android_ndk)
    cc = prebuilt_root + "/bin/clang"
    cpp = prebuilt_root + "/bin/clang++"
    ar = prebuilt_root + "/bin/llvm-ar"
    strip = prebuilt_root + "/bin/llvm-strip"

    out_folder = folder_root + osp + "obj" + osp + "Android" + osp + name + osp + build_output_arm
    createFolderTree(out_folder)
    objs_list = []
    args = []
    num_src = len(srcs)

    index = 0
    c_extencions = [".c", ".cc"]
    cpp_extencions = [".cpp", "xpp"]
    for src in srcs:
        ctype = cc
        use_cpp = False

        if not os.path.isfile(src):
            parent.trace("File not exists")
            continue

        index += 1
        src_folder = os.path.dirname(os.path.abspath(src))
        obj_folder = out_folder

        createFolderTree(obj_folder)
        filename, file_extension = os.path.splitext(src)
        basename = os.path.basename(src)
        basename_without_ext = os.path.splitext(os.path.basename(src))[0]
        obj_name = obj_folder + os.path.sep + basename_without_ext + ".o"
        objs_list.append(obj_name)
        src_modified_time = os.path.getmtime(src)

        args.clear()
        if not full_build:
            if os.path.exists(obj_name):
                obj_modified_time = os.path.getmtime(obj_name)
                if src_modified_time <= obj_modified_time:
                    parent.trace("Skip  file" + src)
                    parent.setProgress(int((index / num_src) * 100), "Skip" + os.path.basename(src))
                    continue

        parent.setProgress(int((index / num_src) * 100), os.path.basename(src))
        if file_extension in cpp_extencions:
            ctype = cpp
            use_cpp = True
            link_cpp = True
        else:
            use_cpp = False

        parent.trace(ctype, " ", os.path.basename(src), ">", os.path.basename(obj_name))

        final_cmd = str(args)
        final_cmd = final_cmd.replace("'", " ")
        final_cmd = final_cmd.replace(",", " ")
        final_cmd = final_cmd.replace("[", " ")
        final_cmd = final_cmd.replace("]", " ")
        print(ctype, " ", final_cmd)

        args.append("-target")
        args.append(arm_target)
        args.append("-fdata-sections")
        args.append("-ffunction-sections")
        args.append("-fstack-protector-strong")
        args.append("-funwind-tables")
        args.append("-no-canonical-prefixes")
        args.append("--sysroot")
        args.append(android_ndk + "/toolchains/llvm/prebuilt/linux-x86_64/sysroot")
        args.append("-g")
        args.append("-Wno-invalid-command-line-argument")
        args.append("-Wno-unused-command-line-argument")
        args.append("-D_FORTIFY_SOURCE=2")
        args.append("-fno-exceptions")
        args.append("-fno-rtti")

        args.append("-fpic")

        if arm == 0:
            args.append("-march=armv7-a")
            args.append("-mthumb")
            args.append("-Oz")
        elif arm == 1:
            args.append("-O2")
        args.append("-DNDEBUG")

        if arm == 0:
            args.append(
                "-I"
                + android_ndk
                + "/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/arm-linux-androideabi"
            )
        elif arm == 1:
            args.append(
                "-I"
                + android_ndk
                + "/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/aarch64-linux-android"
            )

        args.append("-I" + android_ndk + "/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include")

        args.append("-I" + folder_root)
        args.append("-I" + src_folder)

        args.append("-DANDROID")
        if use_cpp:
            args.append("-nostdinc++")
            args.append("-I" + _ndk_cpp_include_dir(android_ndk))
        args.append("-Wformat")
        args.append("-Werror=format-security")
        args.append("-fno-strict-aliasing")
        args.append("-DPLATFORM_ANDROID")

        if use_cpp:
            for arg in cppargs:
                value = arg.strip()
                if len(value) >= 2:
                    args.append(arg)

        else:
            for arg in cargs:
                value = arg.strip()
                if len(value) >= 2:
                    args.append(arg)

        args.append("-c")
        args.append(src)
        args.append("-o")
        args.append(obj_name)

        final_cmd = str(args)
        final_cmd = final_cmd.replace("'", " ")
        final_cmd = final_cmd.replace(",", " ")
        final_cmd = final_cmd.replace("[", " ")
        final_cmd = final_cmd.replace("]", " ")
        parent.trace(ctype, " ", final_cmd)

        code, out, err = runProcess(ctype, args)
        if code != 0:
            parent.trace(err.decode("utf-8"))
            rexp = ":(.*?):(.*?): error: "
            erro = re.search(rexp, err.decode("utf-8"))
            try:
                line_y = 0
                line_x = 0
                if erro:
                    linhas = erro.group().split(":")
                    line_x = int(linhas[2])
                    line_y = int(linhas[1])
            except Exception:
                print("Erro unknow .. ")
            return False
        parent.trace(out.decode("utf-8"))
    parent.trace("Compiling completed")
    return True


def android_build(
    parent,
    folder_root,
    name,
    use_cpp,
    ldargs,
    build_type=0,
    arm=0,
    *,
    android_ndk,
):
    link_cpp = bool(use_cpp)
    osp = os.path.sep
    build_plataform = "23"
    build_output_arm = "armeabi-v7a"
    build_arch = "armv7a"

    arm_target = "armv7a-linux-androideabi21"
    if arm == 1:
        build_output_arm = "arm64-v8a"
        build_arch = "aarch64"
        arm_target = "aarch64-linux-android21"

    prebuilt_root = _ndk_prebuilt_root(android_ndk)
    cc = prebuilt_root + "/bin/clang"
    cpp = prebuilt_root + "/bin/clang++"
    ar = prebuilt_root + "/bin/llvm-ar"
    strip = prebuilt_root + "/bin/llvm-strip"

    out_folder = folder_root + osp + "obj" + osp + "Android" + osp + name + osp + build_output_arm
    bin_folder = folder_root + osp + "Android" + osp + build_output_arm
    createFolderTree(bin_folder)

    list_objs = []

    for root, directories, files in os.walk(out_folder, topdown=False):
        for oname in files:
            ext = os.path.splitext(oname)[1]
            if ext == ".o":
                obj_name = root + osp + oname
                if obj_name not in list_objs:
                    if os.path.exists(obj_name):
                        list_objs.append(obj_name)
                    else:
                        parent.trace(" File ", obj_name, " dont exits")

    args = []

    ctype = cc

    if build_type == 0 or build_type == 1:
        args = []
        export = bin_folder + osp + "lib" + name + ".so"
        parent.trace("Build app ", build_arch, " ", export)
        args.append("-Wl,-soname," + "lib" + name + ".so")
        args.append("-shared")

        objs = ""
        for obj in list_objs:
            objs += obj + " "
            args.append(obj)

        root_folder = os.getcwd() + "/libs/android/" + build_output_arm
        args.append("-L" + root_folder)

        if link_cpp:
            args.append("-L" + prebuilt_root + "/sysroot/usr/lib")

        args.append("-Wl,--no-whole-archive")

        if link_cpp:
            libcxx, libcxxabi, libunwind = _ndk_cpp_runtime_libs(android_ndk, arm)
            if os.path.exists(libcxx):
                args.append(libcxx)
            if os.path.exists(libcxxabi):
                args.append(libcxxabi)
            if libunwind and os.path.exists(libunwind):
                args.append(libunwind)

        args.append("-target")
        if arm == 0:
            args.append("armv7a-linux-androideabi21")

        elif arm == 1:
            args.append("aarch64-linux-android21")
        args.append("--sysroot")
        args.append(prebuilt_root + "/sysroot")

        args.append("-no-canonical-prefixes")
        args.append("-Wl,--build-id")

        if link_cpp:
            args.append("-nostdlib++")
        args.append("-Wl,--no-undefined")
        args.append("-Wl,--fatal-warnings")

        for arg in ldargs:
            value = arg.strip()
            if len(value) > 1:
                args.append(value)

        args.append("-o")
        args.append(export)

        if link_cpp:
            ctype = cpp

        final_cmd = str(args)
        final_cmd = final_cmd.replace("'", " ")
        final_cmd = final_cmd.replace(",", " ")
        final_cmd = final_cmd.replace("[", " ")
        final_cmd = final_cmd.replace("]", " ")
        parent.trace(ctype, " ", final_cmd)
        code, out, err = runProcess(ctype, args)
        if code != 0:
            parent.trace(err.decode("utf-8"))
            rexp = ":(.*?):(.*?): error:"
            return False
        parent.trace(out.decode("utf-8"))

        parent.trace("Strip library ")
        code, out, err = runProcess(strip, ["--strip-unneeded", export])
        if code != 0:
            parent.trace(err.decode("utf-8"))
            rexp = ":(.*?):(.*?): error:"
            return False
        parent.trace(out.decode("utf-8"))
        parent.trace("Native Done :) ")
        return True

    if build_type == 2:
        parent.trace("Build static lib")
        print(list_objs)
        args = []
        export = bin_folder + osp + "lib" + name + ".a"
        args.append("rcs")
        args.append(export)
        objs = ""
        for obj in list_objs:
            objs += obj + " "
            args.append(obj)

        final_cmd = str(args)
        final_cmd = final_cmd.replace("'", " ")
        final_cmd = final_cmd.replace(",", " ")
        final_cmd = final_cmd.replace("[", " ")
        final_cmd = final_cmd.replace("]", " ")
        print(ctype, " ", final_cmd)
        code, out, err = runProcess(ar, args)
        if code != 0:
            parent.trace(err.decode("utf-8"))
            rexp = ":(.*?):(.*?): error:"
            return False
        parent.trace(out.decode("utf-8"))
        parent.trace("Static build completed :) ")
        return True

    return False
