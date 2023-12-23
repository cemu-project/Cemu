#include "FilesystemAndroid.h"

namespace FilesystemAndroid
{
std::shared_ptr<FilesystemCallbacks> g_filesystemCallbacks = nullptr;

void setFilesystemCallbacks(const std::shared_ptr<FilesystemCallbacks> &filesystemCallbacks)
{
    g_filesystemCallbacks = filesystemCallbacks;
}

int openContentUri(const fs::path &uri)
{
    if (g_filesystemCallbacks)
        return g_filesystemCallbacks->openContentUri(uri);
    return -1;
}

std::vector<fs::path> listFiles(const fs::path &uri)
{
    if (g_filesystemCallbacks)
        return g_filesystemCallbacks->listFiles(uri);
    return {};
}
bool isDirectory(const fs::path &uri)
{
    if (g_filesystemCallbacks)
        return g_filesystemCallbacks->isDirectory(uri);
    return false;
}
bool isFile(const fs::path &uri)
{
    if (g_filesystemCallbacks)
        return g_filesystemCallbacks->isFile(uri);
    return false;
}
bool exists(const fs::path &uri)
{
    if (g_filesystemCallbacks)
        return g_filesystemCallbacks->exists(uri);
    return false;
}
bool isContentUri(const std::string &uri)
{
    static constexpr auto content = "content://";
    return uri.starts_with(content);
}
}  // namespace FilesystemAndroid