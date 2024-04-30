#include "Filesystem/WUHB/WUHBReader.h"
#include "Cafe/Filesystem/fsc.h"
#include "Cafe/Filesystem/FST/FST.h"

class FSCDeviceWuhbFileCtx : public FSCVirtualFile {
  public:
	FSCDeviceWuhbFileCtx(WUHBReader* reader, uint32_t entryOffset, uint32 fscType) : m_wuhbReader(reader), m_entryOffset(entryOffset), m_fscType(fscType)
	{ }
	sint32 fscGetType() override
	{
		return m_fscType;
	}
	uint64 fscQueryValueU64(uint32 id) override
	{
		if (m_fscType == FSC_TYPE_FILE)
		{
			if (id == FSC_QUERY_SIZE)
				return m_wuhbReader->GetFileSize(m_entryOffset);
			else if (id == FSC_QUERY_WRITEABLE)
				return 0; // WUHB images are read-only
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
		cemu_assert_error();
		return 0;
	}
	uint32 fscReadData(void* buffer, uint32 size) override
	{
		if (m_fscType != FSC_TYPE_FILE)
			return 0;
		return m_wuhbReader->ReadFromFile(m_entryOffset, m_seek, size, buffer);
	}
	void fscSetSeek(uint64 seek) override
	{
		m_seek = seek;
	}
	uint64 fscGetSeek() override
	{
		if (m_fscType != FSC_TYPE_FILE)
			return 0;
		return m_seek;
	}
	void fscSetFileLength(uint64 endOffset) override
	{
		cemu_assert_error();
	}
	bool fscDirNext(FSCDirEntry* dirEntry) override
	{
		return FSCVirtualFile::fscDirNext(dirEntry);
	}

  private:
	WUHBReader* m_wuhbReader{};
	uint32_t m_fscType;
	uint32_t m_entryOffset;
	uint64_t m_seek = 0;
};

class fscDeviceWUHB : public fscDeviceC {
	FSCVirtualFile* fscDeviceOpenByPath(std::string_view path, FSC_ACCESS_FLAG accessFlags, void* ctx, sint32* fscStatus) override
	{
		WUHBReader* reader = (WUHBReader*)ctx;
		cemu_assert_debug(!HAS_FLAG(accessFlags, FSC_ACCESS_FLAG::WRITE_PERMISSION)); // writing to WUHB is not supported

		if(!(HAS_FLAG(accessFlags, FSC_ACCESS_FLAG::OPEN_DIR) ^ HAS_FLAG(accessFlags, FSC_ACCESS_FLAG::OPEN_FILE)))
		{
			*fscStatus = FSC_STATUS_FILE_NOT_FOUND;
			return nullptr;
		}

		uint32_t table_offset = reader->Lookup(path, HAS_FLAG(accessFlags, FSC_ACCESS_FLAG::OPEN_FILE));
		if(table_offset == ROMFS_ENTRY_EMPTY)
		{
			*fscStatus = FSC_STATUS_FILE_NOT_FOUND;
			return nullptr;
		}

		*fscStatus = FSC_STATUS_OK;
		return new FSCDeviceWuhbFileCtx(reader, table_offset, FSC_TYPE_FILE);
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
