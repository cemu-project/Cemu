#pragma once

struct FSCVirtualFile;

#define FSC_TYPE_INVALID				(0)
#define FSC_TYPE_FILE					(1)
#define FSC_TYPE_DIRECTORY				(2)

#define FSC_QUERY_SIZE					(1) // file size, 0 for directories
#define FSC_QUERY_WRITEABLE				(2) // non-zero if file is writeable, else 0

enum class FSC_ACCESS_FLAG : uint8
{
	NONE = 0,

	// file permissions
	READ_PERMISSION = (1<<0),
	WRITE_PERMISSION = (1<<1),

	// file open mode (incompatible with OPEN_DIR flag)
	FILE_ALLOW_CREATE = (1 << 2), // create file if it does not exist
	FILE_ALWAYS_CREATE = (1 << 3), // overwrite any existing file

	// which types can be opened
	// invalid operation if neither is set
	OPEN_DIR = (1 << 4), 
	OPEN_FILE = (1 << 5),

	// Writing seeks to the end of the file if set
	IS_APPEND = (1 << 6)
};
DEFINE_ENUM_FLAG_OPERATORS(FSC_ACCESS_FLAG);

#define FSC_STATUS_UNDEFINED			(-1)
#define FSC_STATUS_OK					(0)
#define FSC_STATUS_INVALID_PATH			(1)
#define FSC_STATUS_FILE_NOT_FOUND		(2)
#define FSC_STATUS_ALREADY_EXISTS		(3)
// note: Unlike the native Wii U filesystem, FSC does not provide separate error codes for NOT_A_FILE and NOT_A_DIRECTORY
// to determine them manually, open with both modes (file and dir) and check the type

#define FSC_MAX_DIR_NAME_LENGTH			(256)
#define FSC_MAX_DEVICE_PATH_LENGTH		((std::max)(260,FSA_PATH_SIZE_MAX))	// max length for FSC device paths (should be at least equal or greater than supported by host filesystem)

struct FSCDirEntry
{
	char path[FSC_MAX_DIR_NAME_LENGTH];
	// stats
	bool isDirectory;
	bool isFile;
	uint32 fileSize;

	std::string_view GetPath()
	{
		size_t len = strnlen(path, FSC_MAX_DIR_NAME_LENGTH);
		return std::basic_string_view<char>(path, len);
	}
};

class fscDeviceC
{
public:
	virtual FSCVirtualFile* fscDeviceOpenByPath(std::string_view path, FSC_ACCESS_FLAG accessFlags, void* ctx, sint32* fscStatus)
	{
		cemu_assert_unimplemented();
		return nullptr;
	}

	virtual bool fscDeviceCreateDir(std::string_view path, void* ctx, sint32* fscStatus)
	{
		cemu_assert_unimplemented();
		return false;
	}

	virtual bool fscDeviceRemoveFileOrDir(std::string_view path, void* ctx, sint32* fscStatus)
	{
		cemu_assert_unimplemented();
		return false;
	}

	virtual bool fscDeviceRename(std::string_view srcPath, std::string_view dstPath, void* ctx, sint32* fscStatus)
	{
		cemu_assert_unimplemented();
		return false;
	}

};


struct FSCVirtualFile
{
	struct FSCDirIteratorState
	{
		sint32 index;
		std::vector<FSCDirEntry> dirEntries;
	};

	FSCVirtualFile()
	{

	}

	virtual ~FSCVirtualFile()
	{
		if (dirIterator)
			delete dirIterator;
	}

	virtual sint32 fscGetType()
	{
		cemu_assert_unimplemented();
		return 0;
	}

	virtual uint64 fscQueryValueU64(uint32 id)
	{
		cemu_assert_unimplemented();
		return 0;
	}

	virtual uint32 fscWriteData(void* buffer, uint32 size)
	{
		cemu_assert_unimplemented();
		return 0;
	}

	virtual uint32 fscReadData(void* buffer, uint32 size)
	{
		cemu_assert_unimplemented();
		return 0;
	}

	virtual void fscSetSeek(uint64 seek)
	{
		cemu_assert_unimplemented();
	}

	virtual uint64 fscGetSeek()
	{
		cemu_assert_unimplemented();
		return 0;
	}

	virtual void fscSetFileLength(uint64 endOffset)
	{
		cemu_assert_unimplemented();
	}

	virtual bool fscDirNext(FSCDirEntry* dirEntry)
	{
		cemu_assert_unimplemented();
		return false;
	}

	virtual bool fscRewindDir()
	{
		cemu_assert_unimplemented();
		return false;
	}

	FSCDirIteratorState* dirIterator{};

	bool m_isAppend{ false };
};

#define FSC_PRIORITY_BASE				(0)
#define FSC_PRIORITY_AOC				(1)
#define FSC_PRIORITY_PATCH				(2)
#define FSC_PRIORITY_REDIRECT			(3)
#define FSC_PRIORITY_MAX				(3)

#define FSC_PRIORITY_COUNT				(4)

void fsc_init();
sint32 fsc_mount(std::string_view mountPath, std::string_view targetPath, fscDeviceC* fscDevice, void* ctx, sint32 priority=0);
bool fsc_unmount(std::string_view mountPath, sint32 priority);
void fsc_unmountAll();

FSCVirtualFile* fsc_open(const char* path, FSC_ACCESS_FLAG accessFlags, sint32* fscStatus, sint32 maxPriority=FSC_PRIORITY_MAX);
FSCVirtualFile* fsc_openDirIterator(const char* path, sint32* fscStatus);
bool fsc_createDir(const char* path, sint32* fscStatus);
bool fsc_rename(const char* srcPath, const char* dstPath, sint32* fscStatus);
bool fsc_remove(const char* path, sint32* fscStatus);
bool fsc_nextDir(FSCVirtualFile* fscFile, FSCDirEntry* dirEntry);
void fsc_close(FSCVirtualFile* fscFile);
uint32 fsc_getFileSize(FSCVirtualFile* fscFile);
uint32 fsc_getFileSeek(FSCVirtualFile* fscFile);
void fsc_setFileSeek(FSCVirtualFile* fscFile, uint32 newSeek);
void fsc_setFileLength(FSCVirtualFile* fscFile, uint32 newEndOffset);
bool fsc_isDirectory(FSCVirtualFile* fscFile);
bool fsc_isFile(FSCVirtualFile* fscFile);
bool fsc_isWritable(FSCVirtualFile* fscFile);
uint32 fsc_readFile(FSCVirtualFile* fscFile, void* buffer, uint32 size);
uint32 fsc_writeFile(FSCVirtualFile* fscFile, void* buffer, uint32 size);

uint8* fsc_extractFile(const char* path, uint32* fileSize, sint32 maxPriority = FSC_PRIORITY_MAX);
std::optional<std::vector<uint8>> fsc_extractFile(const char* path, sint32 maxPriority = FSC_PRIORITY_MAX);
bool fsc_doesFileExist(const char* path, sint32 maxPriority = FSC_PRIORITY_MAX);
bool fsc_doesDirectoryExist(const char* path, sint32 maxPriority = FSC_PRIORITY_MAX);

// wud device
bool FSCDeviceWUD_Mount(std::string_view mountPath, std::string_view destinationBaseDir, class FSTVolume* mountedVolume, sint32 priority);

// wua device
bool FSCDeviceWUA_Mount(std::string_view mountPath, std::string_view destinationBaseDir, class ZArchiveReader* archive, sint32 priority);

// hostFS device
bool FSCDeviceHostFS_Mount(std::string_view mountPath, std::string_view hostTargetPath, sint32 priority);

// redirect device
void fscDeviceRedirect_map();
void fscDeviceRedirect_add(std::string_view virtualSourcePath, const fs::path& targetFilePath, sint32 priority);
