#pragma once

#include "Cafe/Filesystem/fsc.h"
#include "Common/FileStream.h"

class FSCVirtualFile_AndroidSAF : public FSCVirtualFile
{
   public:
    static FSCVirtualFile* OpenFile(const fs::path& path, FSC_ACCESS_FLAG accessFlags, sint32& fscStatus);
    ~FSCVirtualFile_AndroidSAF() override;

    sint32 fscGetType() override;

    uint32 fscDeviceAndroidSAFFSFile_getFileSize();

    uint64 fscQueryValueU64(uint32 id) override;
    uint32 fscWriteData(void* buffer, uint32 size) override;
    uint32 fscReadData(void* buffer, uint32 size) override;
    void fscSetSeek(uint64 seek) override;
    uint64 fscGetSeek() override;
    void fscSetFileLength(uint64 endOffset) override;
    bool fscDirNext(FSCDirEntry* dirEntry) override;

   private:
    FSCVirtualFile_AndroidSAF(uint32 type) : m_type(type){};

    uint32 m_type;  // FSC_TYPE_*
    FileStream* m_fs{};
    // file
    uint64 m_seek{0};
    uint64 m_fileSize{0};
    bool m_isWritable{false};
    // directory
    std::unique_ptr<fs::path> m_path{};
    std::unique_ptr<std::vector<fs::path>> m_files{};
    std::vector<fs::path>::iterator m_filesIterator;
};
