import os


def getFoldersTree(root, max_depth=0):
    path = os.path.normpath(root)
    result = []

    for current_root, dirs, _ in os.walk(path, topdown=True):
        depth = current_root[len(path) + len(os.path.sep) :].count(os.path.sep)
        if depth == max_depth:
            result += [os.path.join(current_root, directory) for directory in dirs]
            dirs[:] = []

    return result


def getParentDir(path, level=1):
    return os.path.normpath(os.path.join(path, *([".."] * level)))


def cleanString(string):
    return "-".join(string.split())


def createPath(root, sub):
    path = os.path.join(os.path.dirname(os.path.abspath(root)), sub)
    print("Create path ", path)
    if not os.path.exists(path):
        os.mkdir(path)


def createFolderTree(maindir):
    if not os.path.exists(maindir):
        try:
            os.makedirs(maindir)
        except OSError as error:
            print("Something else happened" + str(error))

