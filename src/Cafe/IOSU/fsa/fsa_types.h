#pragma once

enum class FS_RESULT : sint32 // aka FSStatus
{
	SUCCESS = 0,
	END_ITERATION = -2, // used by FSGetMountSource / FSGetMountSourceNext to indicate when last element was reached
	MAX_HANDLES = -3,
	ALREADY_EXISTS = -5,
	NOT_FOUND = -6,
	NOT_FILE = -7,
	NOT_DIR = -8,
	PERMISSION_ERROR = -10,

	FATAL_ERROR = -0x400,
	ERR_PLACEHOLDER = -9999, // used when exact error code has yet to be determined
};

enum class FSA_RESULT : sint32 // aka FSError/FSAStatus
{
	OK = 0,
	NOT_INIT = -0x30000 - 0x01,
	END_OF_DIRECTORY = -0x30000 - 0x04,
	END_OF_FILE = -0x30000 - 0x05,
	MAX_CLIENTS = -0x30000 - 0x12,
	MAX_FILES = -0x30000 - 0x13,
	MAX_DIRS = -0x30000 - 0x14,
	ALREADY_EXISTS = -0x30000 - 0x16,
	NOT_FOUND = -0x30000 - 0x17,
	PERMISSION_ERROR = -0x30000 - 0x1A,
	INVALID_PARAM = -0x30000 - 0x21,
	INVALID_PATH = -0x30000 - 0x22,
	INVALID_BUFFER = -0x30000 - 0x23,
	INVALID_ALIGNMENT = -0x30000 - 0x24,
	INVALID_CLIENT_HANDLE = -0x30000 - 0x25,
	INVALID_FILE_HANDLE = -0x30000 - 0x26,
	INVALID_DIR_HANDLE = -0x30000 - 0x27,
	NOT_FILE = -0x30000 - 0x28,
	NOT_DIR = -0x30000 - 0x29,
	OUT_OF_RESOURCES = -0x30000 - 0x2C,
	FATAL_ERROR = -0x30000 - 0x400,
};

enum class FSA_CMD_OPERATION_TYPE : uint32
{
	CHANGEDIR = 0x5,
	GETCWD = 0x6,
	MAKEDIR = 0x7,
	REMOVE = 0x8,
	RENAME = 0x9,
	OPENDIR = 0xA,
	READDIR = 0xB,
	REWINDDIR = 0xC,
	CLOSEDIR = 0xD,
	OPENFILE = 0xE,
	READ = 0xF,
	WRITE = 0x10,
	GETPOS = 0x11,
	SETPOS = 0x12,
	ISEOF = 0x13,
	GETSTATFILE = 0x14,
	CLOSEFILE = 0x15,
	FLUSHFILE = 0x17,
	QUERYINFO = 0x18,
	APPENDFILE = 0x19,
	TRUNCATEFILE = 0x1A,
	FLUSHQUOTA = 0x1E,
};

using FSResHandle = sint32;
using FSFileHandle2 = FSResHandle;
using FSDirHandle2 = FSResHandle;
#define FS_INVALID_HANDLE_VALUE -1

#define FSA_FILENAME_SIZE_MAX 128
#define FSA_PATH_SIZE_MAX (512 + FSA_FILENAME_SIZE_MAX)
#define FSA_CMD_PATH_MAX_LENGTH FSA_PATH_SIZE_MAX
#define FSA_MAX_CLIENTS 32

typedef sint32 FSStatus;	  // DEPR - replaced by FS_RESULT
typedef uint32 FS_ERROR_MASK; // replace with enum bitmask
typedef uint32 FSFileSize;
typedef uint64 FSLargeSize;
typedef uint64 FSTime;

enum class FSFlag : uint32
{
	NONE = 0,
	IS_DIR = 0x80000000,
	IS_FILE = 0x01000000,
};
DEFINE_ENUM_FLAG_OPERATORS(FSFlag);

#pragma pack(1)

struct FSStat_t
{
	/* +0x000 */ betype<FSFlag> flag;
	/* +0x004 */ uint32be permissions;
	/* +0x008 */ uint32be ownerId;
	/* +0x00C */ uint32be groupId;
	/* +0x010 */ betype<FSFileSize> size;
	/* +0x014 */ betype<FSFileSize> allocatedSize;
	/* +0x018 */ betype<FSLargeSize> quotaSize;
	/* +0x020 */ uint32be entryId;
	/* +0x024 */ betype<FSTime> createdTime;
	/* +0x02C */ betype<FSTime> modifiedTime;
	/* +0x034 */ uint8 attributes[0x30];
};

static_assert(sizeof(FSStat_t) == 0x64);

struct FSDirEntry_t
{
	/* +0x00 */ FSStat_t stat;
	/* +0x64 */ char name[FSA_FILENAME_SIZE_MAX];
};

static_assert(sizeof(FSDirEntry_t) == 0xE4);

struct FSADeviceInfo_t
{
	uint8 ukn0[0x8];
	uint64be deviceSizeInSectors;
	uint32be deviceSectorSize;
	uint8 ukn014[0x14];
};
static_assert(sizeof(FSADeviceInfo_t) == 0x28);

#pragma pack()

// query types for QueryInfo
#define FSA_QUERY_TYPE_FREESPACE 0
#define FSA_QUERY_TYPE_DEVICE_INFO 4
#define FSA_QUERY_TYPE_STAT 5
