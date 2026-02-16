import os
import re
import sys
import time

from ide_utils.fs_utils import createFolderTree
from ide_utils.process_utils import runProcess


def desktop_compile(parent, folder_root, name, srcs, cargs, cppargs, full_build=False):
    use_cpp = False

    os_name = "Linux"
    if sys.platform.startswith("windows"):
        os_name = "Windows"
    elif sys.platform.startswith("linux"):
        os_name = "Linux"

    out_folder = folder_root + os.path.sep + "obj" + os.path.sep + os_name + os.path.sep + name

    print("Obj ", out_folder)

    parent.trace("Compile ", out_folder)
    createFolderTree(out_folder)
    objs_list = []
    index = 0
    num_src = len(srcs)
    for src in srcs:
        if not os.path.isfile(src):
            parent.trace("File not exists")
            continue

        if parent.IsDone:
            return False

        args = []

        src_folder = os.path.dirname(os.path.abspath(src))
        obj_folder = out_folder + src_folder.replace(folder_root, "")

        createFolderTree(obj_folder)
        filename, file_extension = os.path.splitext(src)
        basename = os.path.basename(src)
        basename_without_ext = os.path.splitext(os.path.basename(src))[0]
        obj_name = obj_folder + os.path.sep + basename_without_ext + ".o"

        objs_list.append(obj_name)
        src_modified_time = os.path.getmtime(src)
        src_convert_time = time.ctime(src_modified_time)

        index += 1

        ctype = "gcc"
        if len(file_extension) >= 3:
            ctype = "g++"
            use_cpp = True

        if not full_build:
            if os.path.exists(obj_name):
                obj_modified_time = os.path.getmtime(obj_name)
                obj_convert_time = time.ctime(obj_modified_time)

                if src_convert_time < obj_convert_time:
                    parent.trace("Skip  file" + src)
                    parent.setProgress(int((index / num_src) * 100), "Skip" + os.path.basename(src))
                    continue

        parent.setProgress(int((index / num_src) * 100), os.path.basename(src))

        args.append("-c")
        args.append(src)
        args.append("-o")
        args.append(obj_name)
        print("Compile ", obj_name)

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

        args.append("-fPIC")

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
            except Exception as e:
                print("Error on line {}".format(sys.exc_info()[-1].tb_lineno), type(e).__name__, e)
            return False

        parent.trace(out.decode("utf-8"))

        if parent.IsDone:
            return False
    parent.trace("Compiling completed")
    return True


def desktop_build(parent, folder_root, app_name, use_cpp, ldargs, build_type=0):
    args = []
    ctype = "gcc"
    if use_cpp:
        ctype = "g++"

    os_name = "Linux"
    if sys.platform.startswith("windows"):
        os_name = "Windows"
    elif sys.platform.startswith("linux"):
        os_name = "Linux"
    out_folder = folder_root + os.path.sep + os_name
    list_objs = []
    desktop_obj_root = folder_root + os.path.sep + "obj" + os.path.sep + os_name + os.path.sep + app_name

    for root, directories, files in os.walk(desktop_obj_root, topdown=True):
        for name in files:
            ext = os.path.splitext(name)[1]
            if ext == ".o":
                if name not in list_objs:
                    list_objs.append(root + os.path.sep + name)
                    print(root + os.path.sep + name)

    if build_type == 0:
        parent.trace("Build ", os_name, " ( ", app_name, ") aplication")
        args.append("-o")
        export = folder_root + os.path.sep + app_name
        args.append(export)
        objs = ""
        for obj in list_objs:
            objs += obj + " "
            args.append(obj)
        for arg in ldargs:
            value = arg.strip()
            if len(value) > 1:
                args.append(value)

        final_cmd = str(args)
        final_cmd = final_cmd.replace("'", " ")
        final_cmd = final_cmd.replace(",", " ")
        final_cmd = final_cmd.replace("[", " ")
        final_cmd = final_cmd.replace("]", " ")
        parent.trace(ctype, " ", final_cmd)
        code, out, err = runProcess(ctype, args)

        if code != 0:
            parent.trace(err.decode("utf-8"))
            parent.trace("Operation Fail  .. ")
            return False
    if build_type == 1:
        args.append("-shared")
        args.append("-fPIC")
        args.append("-o")

        export = folder_root + os.path.sep + os_name + os.path.sep + app_name + ".so"
        parent.trace("Build ", os_name, " ( ", app_name, ") shared lib")
        args.append(export)
        objs = ""
        for obj in list_objs:
            objs += obj + " "
            args.append(obj)

        for arg in ldargs:
            value = arg.strip()
            if len(value) > 1:
                args.append(value)

        root_folder = os.getcwd() + os.path.sep + "libs" + os.path.sep + os_name
        args.append("-L" + root_folder)

        final_cmd = str(args)
        final_cmd = final_cmd.replace("'", " ")
        final_cmd = final_cmd.replace(",", " ")
        final_cmd = final_cmd.replace("[", " ")
        final_cmd = final_cmd.replace("]", " ")
        parent.trace(ctype, " ", final_cmd)

        code, out, err = runProcess(ctype, args)
        if code != 0:
            parent.trace(err.decode("utf-8"))
            parent.trace("Operation Fail  .. ")
            return False

    if build_type == 2:
        parent.trace("Build ", os_name, " ( ", app_name, ") static lib")
        createFolderTree(folder_root + os.path.sep + os_name + os.path.sep)
        export = folder_root + os.path.sep + os_name + os.path.sep + "lib" + app_name + ".a"
        parent.trace("Build ", os_name, " ( ", app_name, ") static lib")
        code, out, err = runProcess("rm", ["-f", export])
        parent.trace("rm -f ", export)

        if code != 0:
            parent.trace(err.decode("utf-8"))
            parent.trace("Operation Fail  .. ")
            return False
        parent.trace(out.decode("utf-8"))
        args = []
        args.append("-r")
        args.append("-s")
        args.append(export)
        for obj in list_objs:
            args.append(obj)

        final_cmd = str(args)
        final_cmd = final_cmd.replace("'", " ")
        final_cmd = final_cmd.replace(",", " ")
        final_cmd = final_cmd.replace("[", " ")
        final_cmd = final_cmd.replace("]", " ")
        parent.trace(ctype, " ", final_cmd)
        code, out, err = runProcess("ar", args)
        if code != 0:
            parent.trace(err.decode("utf-8"))
            parent.trace("Operation Fail  .. ")
            return False
        print(out.decode("utf-8"))

    parent.trace("Done :) ")
    return True


def web_compile(parent, folder_root, name, srcs, cargs, cppargs, emcc, emcpp, full_build=False):
    use_cpp = False
    out_folder = folder_root + os.path.sep + "obj" + os.path.sep + "Web" + os.path.sep + name
    createFolderTree(out_folder)
    objs_list = []

    index = 0
    num_src = len(srcs)
    for src in srcs:
        if not os.path.isfile(src):
            parent.trace("File not exists")
            continue

        if parent.IsDone:
            return None
        args = []
        src_folder = os.path.dirname(os.path.abspath(src))
        obj_folder = out_folder + src_folder.replace(folder_root, "")

        createFolderTree(obj_folder)
        filename, file_extension = os.path.splitext(src)
        basename = os.path.basename(src)
        basename_without_ext = os.path.splitext(os.path.basename(src))[0]
        obj_name = obj_folder + os.path.sep + basename_without_ext + ".o"
        objs_list.append(obj_name)
        src_modified_time = os.path.getmtime(src)
        src_convert_time = time.ctime(src_modified_time)

        index += 1
        parent.setProgress(int((index / num_src) * 100), os.path.basename(src))

        ctype = emcc
        if len(file_extension) >= 3:
            ctype = emcpp
            use_cpp = True

        if not full_build:
            if os.path.exists(obj_name):
                obj_modified_time = os.path.getmtime(obj_name)
                obj_convert_time = time.ctime(obj_modified_time)
                if src_convert_time < obj_convert_time:
                    parent.trace("Skip  file" + src)
                    continue

        args.append("-c")
        args.append(src)
        args.append("-o")
        args.append(obj_name)

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
            except Exception as e:
                print("Error on line {}".format(sys.exc_info()[-1].tb_lineno), type(e).__name__, e)
            return False
        parent.trace(out.decode("utf-8"))
        if parent.IsDone:
            return False
    parent.trace("Compiling completed")
    return True


def web_build(parent, folder_root, app_name, use_cpp, ldargs, emcc, emcpp, emar, is_static=True):
    args = []
    is_static = bool(is_static)

    out_folder = folder_root + os.path.sep + "Web" + os.path.sep
    createFolderTree(out_folder)

    list_objs = []
    web_obj_root = folder_root + os.path.sep + "obj" + os.path.sep + "Web" + os.path.sep + app_name

    for root, directories, files in os.walk(web_obj_root, topdown=False):
        for name in files:
            ext = os.path.splitext(name)[1]
            if ext == ".o":
                if name not in list_objs:
                    list_objs.append(root + os.path.sep + name)

    if len(list_objs) <= 0:
        parent.trace("No Web objects found for ", app_name, " at ", web_obj_root)
        parent.trace("Operation Fail  .. ")
        return False

    ctype = emcc
    if use_cpp:
        ctype = emcpp

    if not is_static:
        normalized_ldargs = []
        index = 0
        while index < len(ldargs):
            value = str(ldargs[index]).strip()
            if value == "-s":
                if (index + 1) < len(ldargs):
                    setting = str(ldargs[index + 1]).strip()
                    if len(setting) > 0:
                        normalized_ldargs.append("-s" + setting)
                    index += 2
                    continue
            if len(value) > 1:
                normalized_ldargs.append(value)
            index += 1

        has_asyncify = False
        has_exported_runtime_methods = False
        for arg in normalized_ldargs:
            if arg.startswith("-sASYNCIFY"):
                has_asyncify = True
            if arg.startswith("-sEXPORTED_RUNTIME_METHODS="):
                has_exported_runtime_methods = True

        if not has_asyncify:
            normalized_ldargs.append("-sASYNCIFY")

        if not has_exported_runtime_methods:
            normalized_ldargs.append(
                "-sEXPORTED_RUNTIME_METHODS=['HEAP8','HEAPU8','HEAP16','HEAPU16','HEAP32','HEAPU32','HEAPF32','HEAPF64','ccall','cwrap']"
            )

        parent.trace("Build EMSDK aplication")
        args.append("-o")
        export = out_folder + os.path.sep + app_name + ".html"
        try:
            base = os.path.splitext(export)[0]
            stale_outputs = [
                export,
                base + ".js",
                base + ".wasm",
                base + ".data",
                base + ".worker.js",
            ]
            for stale in stale_outputs:
                if os.path.exists(stale):
                    os.remove(stale)
        except Exception:
            pass
        args.append(export)
        objs = ""
        for obj in list_objs:
            objs += obj + " "
            args.append(obj)

        for arg in normalized_ldargs:
            args.append(arg)
        root_folder = os.getcwd() + os.path.sep + "libs" + os.path.sep + "Web"
        args.append("-L" + root_folder)
        parent.trace("Add folder lib:", root_folder)

        parent.trace("Build to " + ctype + " " + app_name)

        final_cmd = str(args)
        final_cmd = final_cmd.replace("'", " ")
        final_cmd = final_cmd.replace(",", " ")
        final_cmd = final_cmd.replace("[", " ")
        final_cmd = final_cmd.replace("]", " ")
        parent.trace(ctype, " ", final_cmd)
        code, out, err = runProcess(ctype, args)
        if code != 0:
            parent.trace(err.decode("utf-8"))
            parent.trace("Operation Fail  .. ")
            return False

    if is_static:
        parent.trace("Build EMSDK static lib")
        export = folder_root + os.path.sep + "Web" + os.path.sep + "lib" + app_name + ".a"
        if os.path.exists(export):
            try:
                os.remove(export)
            except Exception:
                pass
        args.append("rcs")
        args.append(export)
        parent.trace("Save to ", export)
        for obj in list_objs:
            args.append(obj)

        final_cmd = str(args)
        final_cmd = final_cmd.replace("'", " ")
        final_cmd = final_cmd.replace(",", " ")
        final_cmd = final_cmd.replace("[", " ")
        final_cmd = final_cmd.replace("]", " ")
        parent.trace(ctype, " ", final_cmd)
        code, out, err = runProcess(emar, args)
        if code != 0:
            parent.trace(err.decode("utf-8"))
            parent.trace("Operation Fail  .. ")
            return False
        if not os.path.exists(export) or os.path.getsize(export) <= 8:
            parent.trace("Generated Web archive is empty: ", export)
            parent.trace("Operation Fail  .. ")
            return False
    parent.trace("Done :) ")
    return True
