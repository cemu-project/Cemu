#include "Filesystem/WUHB/WUHBReader.h"
#include "Cafe/Filesystem/fsc.h"
#include "Cafe/Filesystem/FST/FST.h"

class FSCDeviceWuhbFileCtx : public FSCVirtualFile
{
  public:
	FSCDeviceWuhbFileCtx(WUHBReader* reader, uint32 entryOffset, uint32 fscType)
		: m_wuhbReader(reader), m_entryOffset(entryOffset), m_fscType(fscType)
	{
		cemu_assert(entryOffset != ROMFS_ENTRY_EMPTY);
		if (fscType == FSC_TYPE_DIRECTORY)
		{
			romfs_direntry_t entry = reader->GetDirEntry(entryOffset);
			m_dirIterOffset = entry.dirListHead;
			m_fileIterOffset = entry.fileListHead;
		}
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
		auto read = m_wuhbReader->ReadFromFile(m_entryOffset, m_seek, size, buffer);
		m_seek += read;
		return read;
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
		if (m_dirIterOffset != ROMFS_ENTRY_EMPTY)
		{
			romfs_direntry_t entry = m_wuhbReader->GetDirEntry(m_dirIterOffset);
			m_dirIterOffset = entry.listNext;
			if(entry.name_size > 0)
			{
				dirEntry->isDirectory = true;
				dirEntry->isFile = false;
				dirEntry->fileSize = 0;
				std::strncpy(dirEntry->path, entry.name.c_str(), FSC_MAX_DIR_NAME_LENGTH);
				return true;
			}
		}
		if (m_fileIterOffset != ROMFS_ENTRY_EMPTY)
		{
			romfs_fentry_t entry = m_wuhbReader->GetFileEntry(m_fileIterOffset);
			m_fileIterOffset = entry.listNext;
			if(entry.name_size > 0)
			{
				dirEntry->isDirectory = false;
				dirEntry->isFile = true;
				dirEntry->fileSize = entry.size;
				std::strncpy(dirEntry->path, entry.name.c_str(), FSC_MAX_DIR_NAME_LENGTH);
				return true;
			}
		}

		return false;
	}

  private:
	WUHBReader* m_wuhbReader{};
	uint32 m_fscType;
	uint32 m_entryOffset = ROMFS_ENTRY_EMPTY;
	uint32 m_dirIterOffset = ROMFS_ENTRY_EMPTY;
	uint32 m_fileIterOffset = ROMFS_ENTRY_EMPTY;
	uint64 m_seek = 0;
};

class fscDeviceWUHB : public fscDeviceC
{
	FSCVirtualFile* fscDeviceOpenByPath(std::string_view path, FSC_ACCESS_FLAG accessFlags, void* ctx, sint32* fscStatus) override
	{
		WUHBReader* reader = (WUHBReader*)ctx;
		cemu_assert_debug(!HAS_FLAG(accessFlags, FSC_ACCESS_FLAG::WRITE_PERMISSION)); // writing to WUHB is not supported

		bool isFile;
		uint32 table_offset = ROMFS_ENTRY_EMPTY;

		if (table_offset == ROMFS_ENTRY_EMPTY && HAS_FLAG(accessFlags, FSC_ACCESS_FLAG::OPEN_DIR))
		{
			table_offset = reader->Lookup(path, false);
			isFile = false;
		}
		if (table_offset == ROMFS_ENTRY_EMPTY && HAS_FLAG(accessFlags, FSC_ACCESS_FLAG::OPEN_FILE))
		{
			table_offset = reader->Lookup(path, true);
			isFile = true;
		}

		if (table_offset == ROMFS_ENTRY_EMPTY)
		{
			*fscStatus = FSC_STATUS_FILE_NOT_FOUND;
			return nullptr;
		}

		*fscStatus = FSC_STATUS_OK;
		return new FSCDeviceWuhbFileCtx(reader, table_offset, isFile ? FSC_TYPE_FILE : FSC_TYPE_DIRECTORY);
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
