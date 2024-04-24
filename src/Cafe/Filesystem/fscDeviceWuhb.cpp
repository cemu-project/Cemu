#include "Filesystem/WUHB/WUHBReader.h"
#include "Cafe/Filesystem/fsc.h"
#include "Cafe/Filesystem/FST/FST.h"

class FSCDeviceWuhbFileCtx : public FSCVirtualFile {
  public:
	FSCDeviceWuhbFileCtx(WUHBReader* reader, uint32_t fstFileHandle, uint32 fscType)
	{
		m_wuhbReader = reader;
		m_fscType = fscType;
		m_nodeHandle = fstFileHandle;
		m_seek = 0;
	}
	sint32 fscGetType() override
	{
		return m_fscType;
	}
	uint64 fscQueryValueU64(uint32 id) override
	{
		if (m_fscType == FSC_TYPE_FILE)
		{
			if (id == FSC_QUERY_SIZE)
				return m_wuhbReader->GetFileEntry(m_nodeHandle).size;
			else if (id == FSC_QUERY_WRITEABLE)
				return 0; // WUD images are read-only
			else
				cemu_assert_error();
		}
		else
		{
			cemu_assert_unimplemented();
		}
		return 0;
	}
	uint32 fscWriteData(void* buffer, uint32 size) override
	{
		return FSCVirtualFile::fscWriteData(buffer, size);
	}
	uint32 fscReadData(void* buffer, uint32 size) override
	{
		return FSCVirtualFile::fscReadData(buffer, size);
	}
	void fscSetSeek(uint64 seek) override
	{
		FSCVirtualFile::fscSetSeek(seek);
	}
	uint64 fscGetSeek() override
	{
		return FSCVirtualFile::fscGetSeek();
	}
	void fscSetFileLength(uint64 endOffset) override
	{
		FSCVirtualFile::fscSetFileLength(endOffset);
	}
	bool fscDirNext(FSCDirEntry* dirEntry) override
	{
		return FSCVirtualFile::fscDirNext(dirEntry);
	}
	bool fscRewindDir() override
	{
		return FSCVirtualFile::fscRewindDir();
	}

  private:
	WUHBReader* m_wuhbReader{};
	uint32_t m_fscType;
	uint32_t m_nodeHandle;
	uint64_t m_seek;
};

class fscDeviceWUHB : public fscDeviceC {
	FSCVirtualFile* fscDeviceOpenByPath(std::string_view path, FSC_ACCESS_FLAG accessFlags, void* ctx, sint32* fscStatus) override
	{
		WUHBReader* reader = (WUHBReader*)ctx;
		cemu_assert_debug(!HAS_FLAG(accessFlags, FSC_ACCESS_FLAG::WRITE_PERMISSION)); // writing to WUHB is not supported

		reader->Lookup(path);

		return nullptr;
	}

	// singleton
  public:
	static fscDeviceWUHB& instance()
	{
		static fscDeviceWUHB _instance;
		return _instance;
	}
};

bool FSCDeviceWUHB_Mount(std::string_view mountPath, std::string_view destinationBaseDir, WUHBReader* wuhbReader, sint32 priority)
{
	return fsc_mount(mountPath, destinationBaseDir, &fscDeviceWUHB::instance(), wuhbReader, priority) == FSC_STATUS_OK;
}
