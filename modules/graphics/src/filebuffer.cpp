#include "filebuffer.hpp"
#include <raylib.h>


bool FileBuffer::load(const char *path)

{
    data.clear();

    int fileSize = 0;
    unsigned char *fileData = LoadFileData(path, &fileSize);
    if (!fileData || fileSize <= 0)
    {
        if (fileData)
            UnloadFileData(fileData);
        return false;
    }

    data.assign(fileData, fileData + fileSize);
    data.push_back(0); // Keep NUL terminator for C-style loaders.
    UnloadFileData(fileData);
    return true;
}