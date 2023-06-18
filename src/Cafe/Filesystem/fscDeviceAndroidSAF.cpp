#include "Cafe/Filesystem/fscDeviceAndroidSAF.h"

#include <memory>
#include <stdexcept>

#include "Cafe/Filesystem/fsc.h"
#include "Common/FileStream.h"
#include "Common/unix/FilesystemAndroid.h"

FSCVirtualFile_AndroidSAF::~FSCVirtualFile_AndroidSAF()
{
    if (m_type == FSC_TYPE_FILE)
        delete m_fs;
}

sint32 FSCVirtualFile_AndroidSAF::fscGetType()
{
    return m_type;
}

uint32 FSCVirtualFile_AndroidSAF::fscDeviceAndroidSAFFSFile_getFileSize()
{
    if (m_type == FSC_TYPE_FILE)
    {
        if (m_fileSize > 0xFFFFFFFFULL)
            cemu_assert_suspicious();  // files larger than 4GB are not supported by Wii U filesystem
        return (uint32)m_fileSize;
    }
    return 0;
}

uint64 FSCVirtualFile_AndroidSAF::fscQueryValueU64(uint32 id)
{
    if (m_type == FSC_TYPE_FILE)
    {
        if (id == FSC_QUERY_SIZE)
            return fscDeviceAndroidSAFFSFile_getFileSize();
        else if (id == FSC_QUERY_WRITEABLE)
            return m_isWritable;
        else
            cemu_assert_unimplemented();
    }
    else if (m_type == FSC_TYPE_DIRECTORY)
    {
        if (id == FSC_QUERY_SIZE)
            return fscDeviceAndroidSAFFSFile_getFileSize();
        else
            cemu_assert_unimplemented();
    }
    cemu_assert_unimplemented();
    return 0;
}

uint32 FSCVirtualFile_AndroidSAF::fscWriteData(void* buffer, uint32 size)
{
    throw std::logic_error("write not supported with SAF");
    return 0;
}

uint32 FSCVirtualFile_AndroidSAF::fscReadData(void* buffer, uint32 size)
{
    if (m_type != FSC_TYPE_FILE)
        return 0;
    if (size >= (2UL * 1024UL * 1024UL * 1024UL))
    {
        cemu_assert_suspicious();
        return 0;
    }
    uint32 bytesLeft = (uint32)(m_fileSize - m_seek);
    bytesLeft = std::min(bytesLeft, 0x7FFFFFFFu);
    sint32 bytesToRead = std::min(bytesLeft, size);
    uint32 bytesRead = m_fs->readData(buffer, bytesToRead);
    m_seek += bytesRead;
    return bytesRead;
}

void FSCVirtualFile_AndroidSAF::fscSetSeek(uint64 seek)
{
    if (m_type != FSC_TYPE_FILE)
        return;
    this->m_seek = seek;
    cemu_assert_debug(seek <= m_fileSize);
    m_fs->SetPosition(seek);
}

uint64 FSCVirtualFile_AndroidSAF::fscGetSeek()
{
    if (m_type != FSC_TYPE_FILE)
        return 0;
    return m_seek;
}

void FSCVirtualFile_AndroidSAF::fscSetFileLength(uint64 endOffset)
{
    if (m_type != FSC_TYPE_FILE)
        return;
    m_fs->SetPosition(endOffset);
    bool r = m_fs->SetEndOfFile();
    m_seek = std::min(m_seek, endOffset);
    m_fileSize = m_seek;
    m_fs->SetPosition(m_seek);
    if (!r)
        cemuLog_log(LogType::Force, "fscSetFileLength: Failed to set size to 0x{:x}", endOffset);
}

bool FSCVirtualFile_AndroidSAF::fscDirNext(FSCDirEntry* dirEntry)
{
    if (m_type != FSC_TYPE_DIRECTORY)
        return false;

    if (!m_files)
    {
        // init iterator on first iteration attempt
        m_files = std::make_unique<std::vector<fs::path>>(FilesystemAndroid::listFiles(*m_path));
        m_filesIterator = m_files->begin();
        if (!m_files)
        {
            cemuLog_log(LogType::Force, "Failed to iterate directory: {}", _pathToUtf8(*m_path));
            return false;
        }
    }
    if (m_filesIterator == m_files->end())
        return false;

    const fs::path& file = *m_filesIterator;

    std::string fileName = file.filename().generic_string();
    if (fileName.size() >= sizeof(dirEntry->path) - 1)
        fileName.resize(sizeof(dirEntry->path) - 1);
    strncpy(dirEntry->path, fileName.data(), sizeof(dirEntry->path));
    if (FilesystemAndroid::isDirectory(file))
    {
        dirEntry->isDirectory = true;
        dirEntry->isFile = false;
        dirEntry->fileSize = 0;
    }
    else
    {
        dirEntry->isDirectory = false;
        dirEntry->isFile = true;
        dirEntry->fileSize = 0;
        auto fs = FileStream::openFile2(file);
        if (fs)
        {
            dirEntry->fileSize = fs->GetSize();
            delete fs;
        }
    }
    m_filesIterator++;
    return true;
}

FSCVirtualFile* FSCVirtualFile_AndroidSAF::OpenFile(const fs::path& path, FSC_ACCESS_FLAG accessFlags, sint32& fscStatus)
{
    if (!HAS_FLAG(accessFlags, FSC_ACCESS_FLAG::OPEN_FILE) && !HAS_FLAG(accessFlags, FSC_ACCESS_FLAG::OPEN_DIR))
        cemu_assert_debug(false);  // not allowed. At least one of both flags must be set
    if (HAS_FLAG(accessFlags, FSC_ACCESS_FLAG::WRITE_PERMISSION) ||
        HAS_FLAG(accessFlags, FSC_ACCESS_FLAG::FILE_ALLOW_CREATE) ||
        HAS_FLAG(accessFlags, FSC_ACCESS_FLAG::FILE_ALWAYS_CREATE))
        throw std::logic_error("writing and creating a file is not supported with SAF");
    // attempt to open as file
    if (HAS_FLAG(accessFlags, FSC_ACCESS_FLAG::OPEN_FILE))
    {
        FileStream* fs = FileStream::openFile2(path);
        if (fs)
        {
            FSCVirtualFile_AndroidSAF* vf = new FSCVirtualFile_AndroidSAF(FSC_TYPE_FILE);
            vf->m_fs = fs;
            vf->m_isWritable = false;
            vf->m_fileSize = fs->GetSize();
            fscStatus = FSC_STATUS_OK;
            return vf;
        }
    }

    // attempt to open as directory
    if (HAS_FLAG(accessFlags, FSC_ACCESS_FLAG::OPEN_DIR))
    {
        bool isExistingDir = FilesystemAndroid::exists(path);
        if (isExistingDir)
        {
            FSCVirtualFile_AndroidSAF* vf = new FSCVirtualFile_AndroidSAF(FSC_TYPE_DIRECTORY);
            vf->m_path.reset(new std::filesystem::path(path));
            fscStatus = FSC_STATUS_OK;
            return vf;
        }
    }
    fscStatus = FSC_STATUS_FILE_NOT_FOUND;
    return nullptr;
}

/* Device implementation */

class fscDeviceAndroidSAFFSC : public fscDeviceC
{
   public:
    FSCVirtualFile* fscDeviceOpenByPath(std::string_view path, FSC_ACCESS_FLAG accessFlags, void* ctx, sint32* fscStatus) override
    {
        *fscStatus = FSC_STATUS_OK;
        FSCVirtualFile* vf = FSCVirtualFile_AndroidSAF::OpenFile(_utf8ToPath(path), accessFlags, *fscStatus);
        cemu_assert_debug((bool)vf == (*fscStatus == FSC_STATUS_OK));
        return vf;
    }

    bool fscDeviceCreateDir(std::string_view path, void* ctx, sint32* fscStatus) override
    {
        throw std::logic_error("creating a directory is not supported with SAF");
        return false;
    }

    bool fscDeviceRemoveFileOrDir(std::string_view path, void* ctx, sint32* fscStatus) override
    {
        throw std::logic_error("removing a file or dir is not supported with SAF");
        return false;
    }

    bool fscDeviceRename(std::string_view srcPath, std::string_view dstPath, void* ctx, sint32* fscStatus) override
    {
        throw std::logic_error("renaming not supported with SAF");
        return false;
    }

    // singleton
   public:
    static fscDeviceAndroidSAFFSC& instance()
    {
        static fscDeviceAndroidSAFFSC _instance;
        return _instance;
    }
};

bool FSCDeviceAndroidSAF_Mount(std::string_view mountPath, std::string_view hostTargetPath, sint32 priority)
{
    return fsc_mount(mountPath, hostTargetPath, &fscDeviceAndroidSAFFSC::instance(), nullptr, priority) == FSC_STATUS_OK;
}
