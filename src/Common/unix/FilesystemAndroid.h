#pragma once

namespace FilesystemAndroid
{
class FilesystemCallbacks
{
   public:
    virtual int openContentUri(const std::filesystem::path &uri) = 0;
    virtual std::vector<std::filesystem::path> listFiles(const std::filesystem::path &uri) = 0;
    virtual bool isDirectory(const std::filesystem::path &uri) = 0;
    virtual bool isFile(const std::filesystem::path &uri) = 0;
    virtual bool exists(const std::filesystem::path &uri) = 0;
};

void setFilesystemCallbacks(const std::shared_ptr<FilesystemCallbacks> &filesystemCallbacks);

int openContentUri(const std::filesystem::path &uri);

std::vector<std::filesystem::path> listFiles(const std::filesystem::path &uri);

bool isDirectory(const std::filesystem::path &uri);

bool isFile(const std::filesystem::path &uri);

bool exists(const std::filesystem::path& uri);

bool isContentUri(const std::string &uri);

}  // namespace FilesystemAndroid
