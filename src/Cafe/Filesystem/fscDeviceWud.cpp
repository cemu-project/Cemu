#include "Cafe/Filesystem/fsc.h"
#include "Cafe/Filesystem/FST/FST.h"

class FSCDeviceWudFileCtx : public FSCVirtualFile
{
	friend class fscDeviceWUDC;

protected:
	FSCDeviceWudFileCtx(FSTVolume* _volume, FSTFileHandle _fstFileHandle)
	{
		this->m_volume = _volume;
		this->m_fscType = FSC_TYPE_FILE;
		this->m_fstFileHandle = _fstFileHandle;
		this->m_seek = 0;
	};

	FSCDeviceWudFileCtx(FSTVolume* _volume, FSTDirectoryIterator _dirIterator)
	{
		this->m_volume = _volume;
		this->m_fscType = FSC_TYPE_DIRECTORY;
		this->m_dirIterator = _dirIterator;
	}

public:
	sint32 fscGetType() override
	{
		return m_fscType;
	}

	uint32 fscDeviceWudFile_getFileSize()
	{
		return m_volume->GetFileSize(m_fstFileHandle);
	}

	uint64 fscQueryValueU64(uint32 id) override
	{
		if (m_fscType == FSC_TYPE_FILE)
		{
			if (id == FSC_QUERY_SIZE)
				return fscDeviceWudFile_getFileSize();
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
		uint32 bytesLeft = fscDeviceWudFile_getFileSize() - m_seek;
		uint32 bytesToRead = (std::min)(bytesLeft, (uint32)size);
		uint32 bytesSuccessfullyRead = m_volume->ReadFile(m_fstFileHandle, m_seek, bytesToRead, buffer);
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
		FSTFileHandle entryItr;
		if (!m_volume->Next(m_dirIterator, entryItr))
			return false;
		if (m_volume->IsDirectory(entryItr))
		{
			dirEntry->isDirectory = true;
			dirEntry->isFile = false;
			dirEntry->fileSize = 0;
		}
		else
		{
			dirEntry->isDirectory = false;
			dirEntry->isFile = true;
			dirEntry->fileSize = m_volume->GetFileSize(entryItr);
		}
		auto path = m_volume->GetName(entryItr);
		std::memset(dirEntry->path, 0, sizeof(dirEntry->path));
		std::strncpy(dirEntry->path, path.data(), std::min(sizeof(dirEntry->path) - 1, path.size()));
		return true;
	}

private:
	FSTVolume* m_volume{nullptr};
	sint32 m_fscType;
	FSTFileHandle m_fstFileHandle;
	// file
	uint32 m_seek{0};
	// directory
	FSTDirectoryIterator m_dirIterator{};
};

class fscDeviceWUDC : public fscDeviceC
{
	FSCVirtualFile* fscDeviceOpenByPath(std::string_view path, FSC_ACCESS_FLAG accessFlags, void* ctx, sint32* fscStatus) override
	{
		FSTVolume* mountedVolume = (FSTVolume*)ctx;
		cemu_assert_debug(!HAS_FLAG(accessFlags, FSC_ACCESS_FLAG::WRITE_PERMISSION)); // writing to FST is never allowed

		if (HAS_FLAG(accessFlags, FSC_ACCESS_FLAG::OPEN_FILE))
		{
			FSTFileHandle fstFileHandle;
			if (mountedVolume->OpenFile(path, fstFileHandle, true) && !mountedVolume->HasLinkFlag(fstFileHandle))
			{
				*fscStatus = FSC_STATUS_OK;
				return new FSCDeviceWudFileCtx(mountedVolume, fstFileHandle);
			}
		}
		if (HAS_FLAG(accessFlags, FSC_ACCESS_FLAG::OPEN_DIR))
		{
			FSTDirectoryIterator dirIterator;
			if (mountedVolume->OpenDirectoryIterator(path, dirIterator) && !mountedVolume->HasLinkFlag(dirIterator.GetDirHandle()))
			{
				*fscStatus = FSC_STATUS_OK;
				return new FSCDeviceWudFileCtx(mountedVolume, dirIterator);
			}
		}
		*fscStatus = FSC_STATUS_FILE_NOT_FOUND;
		return nullptr;
	}

	// singleton
public:
	static fscDeviceWUDC& instance()
	{
		static fscDeviceWUDC _instance;
		return _instance;
	}
};

bool FSCDeviceWUD_Mount(std::string_view mountPath, std::string_view destinationBaseDir, FSTVolume* mountedVolume, sint32 priority)
{
	return fsc_mount(mountPath, destinationBaseDir, &fscDeviceWUDC::instance(), mountedVolume, priority) == FSC_STATUS_OK;
}