#pragma once

enum class FS_RESULT : sint32 // aka FSStatus
{
	SUCCESS = 0,
	END_ITERATION = -2, // used by FSGetMountSource / FSGetMountSourceNext to indicate when last element was reached
	FATAL_ERROR = -0x400,

	ALREADY_EXISTS = -5,
	NOT_FOUND = -6,
	NOT_FILE = -7,
	NOT_DIR = -8,
	PERMISSION_ERROR = -10,

	INVALID_CLIENT_HANDLE = -0x30000 - 37,

	ERR_PLACEHOLDER = -9999, // used when exact error code has yet to be determined
};

enum class FSA_RESULT : sint32 // aka FSError/FSAStatus
{
	SUCCESS = 0,
	INVALID_CLIENT_HANDLE = -0x30000 - 37,
	INVALID_HANDLE_UKN38 = -0x30000 - 38,
	FATAL_ERROR = -0x30000 - 0x400,
};
// todo - error handling in the IOSU part is pretty hacky right now and we use FS_RESULT in most places which we shouldn't be doing. Rework it

using FSResHandle = sint32;
using FSFileHandle2 = FSResHandle;
using FSDirHandle2 = FSResHandle;
#define FS_INVALID_HANDLE_VALUE         -1

#define FSA_FILENAME_SIZE_MAX			128
#define FSA_PATH_SIZE_MAX				(512 + FSA_FILENAME_SIZE_MAX)
#define FSA_CMD_PATH_MAX_LENGTH			FSA_PATH_SIZE_MAX
#define FSA_MAX_CLIENTS					32

typedef sint32                 FSStatus; // DEPR - replaced by FS_RESULT
typedef uint32				   FS_ERROR_MASK; // replace with enum bitmask
typedef uint32                 FSFileSize;
typedef uint64                 FSLargeSize;
typedef uint64                 FSTime;

enum class FSFlag : uint32
{
	NONE		= 0,
	IS_DIR		= 0x80000000,
};
DEFINE_ENUM_FLAG_OPERATORS(FSFlag);

#pragma pack(1)

struct FSStat_t
{
	/* +0x000 */ betype<FSFlag>			flag;
	/* +0x004 */ uint32be				permissions;
	/* +0x008 */ uint32be				ownerId;
	/* +0x00C */ uint32be				groupId;
	/* +0x010 */ betype<FSFileSize>		size;
	/* +0x014 */ betype<FSFileSize>		allocatedSize;
	/* +0x018 */ betype<FSLargeSize>	quotaSize;
	/* +0x020 */ uint32be				entryId;
	/* +0x024 */ betype<FSTime>			createdTime;
	/* +0x02C */ betype<FSTime>			modifiedTime;
	/* +0x034 */ uint8					attributes[0x30];
};

static_assert(sizeof(FSStat_t) == 0x64);

struct FSDirEntry_t
{
	/* +0x00 */ FSStat_t    stat;
	/* +0x64 */ char        name[FSA_FILENAME_SIZE_MAX];
};

static_assert(sizeof(FSDirEntry_t) == 0xE4);

#pragma pack()

// query types for QueryInfo
#define FSA_QUERY_TYPE_FREESPACE		0
#define FSA_QUERY_TYPE_STAT				5
