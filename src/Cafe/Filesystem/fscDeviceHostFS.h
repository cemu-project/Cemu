#include "Cafe/Filesystem/fsc.h"

class FSCVirtualFile_Host : public FSCVirtualFile
{
public:
	void Save(MemStreamWriter& writer) override;
	static FSCVirtualFile* OpenFile(const fs::path& path, FSC_ACCESS_FLAG accessFlags, sint32& fscStatus);
	~FSCVirtualFile_Host() override;

	sint32 fscGetType() override;

	uint32 fscDeviceHostFSFile_getFileSize();

	uint64 fscQueryValueU64(uint32 id) override;
	uint32 fscWriteData(void* buffer, uint32 size) override;
	uint32 fscReadData(void* buffer, uint32 size) override;
	void fscSetSeek(uint64 seek) override;
	uint64 fscGetSeek() override;
	void fscSetFileLength(uint64 endOffset) override;
	bool fscDirNext(FSCDirEntry* dirEntry) override;

private:
	FSCVirtualFile_Host(uint32 type) : m_type(type) {};

private:
	uint32 m_type; // FSC_TYPE_*
	class FileStream* m_fs{};
	// file
	uint64 m_seek{ 0 };
	uint64 m_fileSize{ 0 };
	bool m_isWritable{ false };
	// directory
	std::unique_ptr<std::filesystem::path> m_path{};
	std::unique_ptr<std::filesystem::directory_iterator> m_dirIterator{};
	// serialization
	FSC_ACCESS_FLAG m_accessFlags;
};