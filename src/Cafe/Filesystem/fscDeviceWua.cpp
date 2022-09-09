#include "Cafe/Filesystem/fsc.h"
#include <zarchive/zarchivereader.h>

class FSCDeviceWuaFileCtx : public FSCVirtualFile
{
	friend class fscDeviceWUAC;

protected:
	FSCDeviceWuaFileCtx(ZArchiveReader* archive, ZArchiveNodeHandle fstFileHandle, uint32 fscType)
	{
		this->m_archive = archive;
		this->m_fscType = fscType;
		this->m_nodeHandle = fstFileHandle;
		this->m_seek = 0;
	};

public:
	sint32 fscGetType() override
	{
		return m_fscType;
	}

	uint32 fscDeviceWuaFile_getFileSize()
	{
		return (uint32)m_archive->GetFileSize(m_nodeHandle);
	}

	uint64 fscQueryValueU64(uint32 id) override
	{
		if (m_fscType == FSC_TYPE_FILE)
		{
			if (id == FSC_QUERY_SIZE)
				return fscDeviceWuaFile_getFileSize();
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
		cemu_assert_error();
		return 0;
	}

	uint32 fscReadData(void* buffer, uint32 size) override
	{
		if (m_fscType != FSC_TYPE_FILE)
			return 0;
		cemu_assert(size < (2ULL * 1024 * 1024 * 1024)); // single read operation larger than 2GiB not supported
		uint32 bytesLeft = fscDeviceWuaFile_getFileSize() - m_seek;
		uint32 bytesToRead = (std::min)(bytesLeft, (uint32)size);
		uint32 bytesSuccessfullyRead = (uint32)m_archive->ReadFromFile(m_nodeHandle, m_seek, bytesToRead, buffer);
		m_seek += bytesSuccessfullyRead;
		return bytesSuccessfullyRead;
	}

	void fscSetSeek(uint64 seek) override
	{
		if (m_fscType != FSC_TYPE_FILE)
			return;
		cemu_assert_debug(seek <= 0xFFFFFFFFULL);
		this->m_seek = (uint32)seek;
	}

	uint64 fscGetSeek() override
	{
		if (m_fscType != FSC_TYPE_FILE)
			return 0;
		return m_seek;
	}

	bool fscDirNext(FSCDirEntry* dirEntry) override
	{
		if (m_fscType != FSC_TYPE_DIRECTORY)
			return false;

		ZArchiveReader::DirEntry zarDirEntry;
		if (!m_archive->GetDirEntry(m_nodeHandle, m_iteratorIndex, zarDirEntry))
			return false;
		m_iteratorIndex++;

		if (zarDirEntry.isDirectory)
		{
			dirEntry->isDirectory = true;
			dirEntry->isFile = false;
			dirEntry->fileSize = 0;
		}
		else if(zarDirEntry.isFile)
		{
			dirEntry->isDirectory = false;
			dirEntry->isFile = true;
			dirEntry->fileSize = (uint32)zarDirEntry.size;
		}
		else
		{
			cemu_assert_suspicious();
		}
		std::memset(dirEntry->path, 0, sizeof(dirEntry->path));
		std::strncpy(dirEntry->path, zarDirEntry.name.data(), std::min(sizeof(dirEntry->path) - 1, zarDirEntry.name.size()));
		return true;
	}

private:
	ZArchiveReader* m_archive{nullptr};
	sint32 m_fscType;
	ZArchiveNodeHandle m_nodeHandle;
	// file
	uint32 m_seek{0};
	// directory
	uint32 m_iteratorIndex{0};
};

class fscDeviceWUAC : public fscDeviceC
{
	FSCVirtualFile* fscDeviceOpenByPath(std::string_view path, FSC_ACCESS_FLAG accessFlags, void* ctx, sint32* fscStatus) override
	{
		ZArchiveReader* archive = (ZArchiveReader*)ctx;
		cemu_assert_debug(!HAS_FLAG(accessFlags, FSC_ACCESS_FLAG::WRITE_PERMISSION)); // writing to WUA is not supported

		ZArchiveNodeHandle fileHandle = archive->LookUp(path, true, true);
		if (fileHandle == ZARCHIVE_INVALID_NODE)
		{
			*fscStatus = FSC_STATUS_FILE_NOT_FOUND;
			return nullptr;
		}
		if (archive->IsFile(fileHandle))
		{
			if (!HAS_FLAG(accessFlags, FSC_ACCESS_FLAG::OPEN_FILE))
			{
				*fscStatus = FSC_STATUS_FILE_NOT_FOUND;
				return nullptr;
			}
			*fscStatus = FSC_STATUS_OK;
			return new FSCDeviceWuaFileCtx(archive, fileHandle, FSC_TYPE_FILE);
		}
		else if (archive->IsDirectory(fileHandle))
		{
			if (!HAS_FLAG(accessFlags, FSC_ACCESS_FLAG::OPEN_DIR))
			{
				*fscStatus = FSC_STATUS_FILE_NOT_FOUND;
				return nullptr;
			}
			*fscStatus = FSC_STATUS_OK;
			return new FSCDeviceWuaFileCtx(archive, fileHandle, FSC_TYPE_DIRECTORY);
		}
		else
		{
			cemu_assert_suspicious();
		}
		*fscStatus = FSC_STATUS_FILE_NOT_FOUND;
		return nullptr;
	}

	// singleton
public:
	static fscDeviceWUAC& instance()
	{
		static fscDeviceWUAC _instance;
		return _instance;
	}
};

bool FSCDeviceWUA_Mount(std::string_view mountPath, std::string_view destinationBaseDir, ZArchiveReader* archive, sint32 priority)
{
	return fsc_mount(mountPath, destinationBaseDir, &fscDeviceWUAC::instance(), archive, priority) == FSC_STATUS_OK;
}