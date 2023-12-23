#include "Common/FileStream.h"

#if BOOST_OS_UNIX

#if __ANDROID__
#include "Common/unix/FileStream_contentUri.h"
#include "Common/unix/FilesystemAndroid.h"
#endif  // __ANDROID__

#include "Common/unix/FileStream_unix.h"

FileStream* FileStream::openFile(std::string_view path)
{
    return openFile2(path, false);
}

FileStream* FileStream::openFile(const wchar_t* path, bool allowWrite)
{
    return openFile2(path, allowWrite);
}

FileStream* FileStream::openFile2(const fs::path& path, bool allowWrite)
{
#if __ANDROID__
    if (FilesystemAndroid::isContentUri(path))
    {
        if (allowWrite || FilesystemAndroid::isDirectory(path))
        {
            return nullptr;
        }
        FileStreamContentUri* fs = new FileStreamContentUri(path);
        if (fs->m_isValid)
            return fs;
        delete fs;
        return nullptr;
    }
#endif  // __ANDROID__
    FileStreamUnix* fs = new FileStreamUnix(path, true, allowWrite);
    if (fs->m_isValid)
        return fs;
    delete fs;
    return nullptr;
}

FileStream* FileStream::createFile(const wchar_t* path)
{
    return createFile2(path);
}

FileStream* FileStream::createFile(std::string_view path)
{
    return createFile2(path);
}

FileStream* FileStream::createFile2(const fs::path& path)
{
    FileStreamUnix* fs = new FileStreamUnix(path, false, false);
    if (fs->m_isValid)
        return fs;
    delete fs;
    return nullptr;
}

std::optional<std::vector<uint8>> FileStream::LoadIntoMemory(const fs::path& path)
{
    FileStream* fs = openFile2(path);
    if (!fs)
        return std::nullopt;
    uint64 fileSize = fs->GetSize();
    if (fileSize > 0xFFFFFFFFull)
    {
        delete fs;
        return std::nullopt;
    }
    std::optional<std::vector<uint8>> v(fileSize);
    if (fs->readData(v->data(), (uint32)fileSize) != (uint32)fileSize)
    {
        delete fs;
        return std::nullopt;
    }
    delete fs;
    return v;
}

#endif  // BOOST_OS_UNIX

#ifdef _WIN32

#include "Common/windows/FileStream_win32.h"

FileStream* FileStream::openFile(std::string_view path)
{
    HANDLE hFile = CreateFileW(boost::nowide::widen(path.data(), path.size()).c_str(), FILE_GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
    if (hFile == INVALID_HANDLE_VALUE)
        return nullptr;
    return new FileStreamWin32(hFile);
}

FileStream* FileStream::openFile(const wchar_t* path, bool allowWrite)
{
    HANDLE hFile = CreateFileW(path, allowWrite ? (FILE_GENERIC_READ | FILE_GENERIC_WRITE) : FILE_GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
    if (hFile == INVALID_HANDLE_VALUE)
        return nullptr;
    return new FileStreamWin32(hFile);
}

FileStream* FileStream::openFile2(const fs::path& path, bool allowWrite)
{
    return openFile(path.generic_wstring().c_str(), allowWrite);
}

FileStream* FileStream::createFile(const wchar_t* path)
{
    HANDLE hFile = CreateFileW(path, FILE_GENERIC_READ | FILE_GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, 0);
    if (hFile == INVALID_HANDLE_VALUE)
        return nullptr;
    return new FileStreamWin32(hFile);
}

FileStream* FileStream::createFile(std::string_view path)
{
    auto w = boost::nowide::widen(path.data(), path.size());
    HANDLE hFile = CreateFileW(w.c_str(), FILE_GENERIC_READ | FILE_GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, 0);
    if (hFile == INVALID_HANDLE_VALUE)
        return nullptr;
    return new FileStreamWin32(hFile);
}

FileStream* FileStream::createFile2(const fs::path& path)
{
    return createFile(path.generic_wstring().c_str());
}

std::optional<std::vector<uint8>> FileStream::LoadIntoMemory(const fs::path& path)
{
    FileStream* fs = openFile2(path);
    if (!fs)
        return std::nullopt;
    uint64 fileSize = fs->GetSize();
    if (fileSize > 0xFFFFFFFFull)
    {
        delete fs;
        return std::nullopt;
    }
    std::optional<std::vector<uint8>> v(fileSize);
    if (fs->readData(v->data(), (uint32)fileSize) != (uint32)fileSize)
    {
        delete fs;
        return std::nullopt;
    }
    delete fs;
    return v;
}

#endif  // _WIN32