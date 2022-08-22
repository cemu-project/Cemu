#pragma once
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"
#include "Cafe/IOSU/iosu_ipc_common.h"
#include "Cafe/IOSU/fsa/fsa_types.h"
#include "Cafe/IOSU/fsa/iosu_fsa.h"
#include "coreinit_MessageQueue.h"

typedef struct
{
	uint32be fileHandle;
}FSFileHandleDepr_t;

typedef MEMPTR<betype<FSDirHandle2>> FSDirHandlePtr;

typedef struct
{
	MEMPTR<void>				userCallback;
	MEMPTR<void>				userContext;
	MEMPTR<coreinit::OSMessageQueue>	ioMsgQueue;
}FSAsyncParamsNew_t;

static_assert(sizeof(FSAsyncParamsNew_t) == 0xC);

typedef struct
{
	MPTR					userCallback; // 0x96C
	MPTR					userContext;
	MPTR					ioMsgQueue;
}FSAsyncParams_t; // legacy struct. Replace with FSAsyncParamsNew_t

namespace coreinit
{
	struct FSCmdQueue
	{
		enum class QUEUE_FLAG : uint32
		{
			IS_FULL = (1 << 0), // waiting for Ioctl(v) result
			CANCEL_ALL = (1 << 4),
		};

		/* +0x00 */ MPTR firstMPTR;
		/* +0x04 */ MPTR lastMPTR;
		/* +0x08 */ OSMutex mutex;
		/* +0x34 */ MPTR dequeueHandlerFuncMPTR;
		/* +0x38 */ uint32be numCommandsInFlight;
		/* +0x3C */ uint32 numMaxCommandsInFlight;
		/* +0x40 */ betype<QUEUE_FLAG> queueFlags;
	};
	DEFINE_ENUM_FLAG_OPERATORS(FSCmdQueue::QUEUE_FLAG);

	#define FS_CLIENT_BUFFER_SIZE           (5888)
	#define FS_CMD_BLOCK_SIZE               (2688)

	struct FSClient_t
	{
		uint8 buffer[FS_CLIENT_BUFFER_SIZE];
	};

	struct FSCmdBlock_t
	{
		union
		{
			uint8 buffer[FS_CMD_BLOCK_SIZE];
			struct
			{
				uint32 mount_it;
			}data;
		};
	};

	static_assert(sizeof(FSCmdBlock_t) == FS_CMD_BLOCK_SIZE);

	struct FSClientBody_t
	{
		uint8 ukn0000[0x100];
		uint8 ukn0100[0x100];
		uint8 ukn0200[0x100];
		uint8 ukn0300[0x100];
		uint8 ukn0400[0x100];
		uint8 ukn0500[0x100];
		uint8 ukn0600[0x100];
		uint8 ukn0700[0x100];
		uint8 ukn0800[0x100];
		uint8 ukn0900[0x100];
		uint8 ukn0A00[0x100];
		uint8 ukn0B00[0x100];
		uint8 ukn0C00[0x100];
		uint8 ukn0D00[0x100];
		uint8 ukn0E00[0x100];
		uint8 ukn0F00[0x100];
		uint8 ukn1000[0x100];
		uint8 ukn1100[0x100];
		uint8 ukn1200[0x100];
		uint8 ukn1300[0x100];
		uint8 ukn1400[0x10];
		uint8 ukn1410[0x10];
		uint8 ukn1420[0x10];
		uint8 ukn1430[0x10];
		uint32 ukn1440;
		betype<IOSDevHandle> iosuFSAHandle;
		uint32 ukn1448;
		uint32 ukn144C;
		uint8 ukn1450[0x10];
		uint8 ukn1460[0x10];
		uint8 ukn1470[0x10];
		FSCmdQueue fsCmdQueue;
		/* +0x14C4 */ MEMPTR<struct FSCmdBlockBody_t> currentCmdBlockBody; // set to currently active cmd
		uint32 ukn14C8;
		uint32 ukn14CC;
		uint8 ukn14D0[0x10];
		uint8 ukn14E0[0x10];
		uint8 ukn14F0[0x10];
		uint8 ukn1500[0x100];
		uint32 ukn1600;
		uint32 ukn1604;
		uint32 ukn1608;
		uint32 ukn160C;
		uint32 ukn1610;
		MEMPTR<FSClientBody_t> fsClientBodyNext; // next FSClientBody_t* in list of registered clients (list is circular, the last element points to the first element)
		uint32 ukn1618;
		/* +0x161C */ MEMPTR<FSClient_t> selfClient; // pointer to FSClient struct which holds this FSClientBody
		uint32 ukn1620;
	};

	struct FSAsyncResult
	{
		/* +0x00 */ FSAsyncParamsNew_t fsAsyncParamsNew;

		// fs message storage
		struct FSMessage 
		{
			/* +0x0C / 0x0978 */ MEMPTR<FSAsyncResult>	fsAsyncResult;
			/* +0x10 */ MPTR	fsClientMPTR2;		// 0x097C 
			/* +0x14 */ MPTR	fsCmdBlockMPTR;		// 0x0980
			/* +0x18 */ MPTR	commandType;		// 0x0984
		};

		union
		{
			OSMessage osMsg;
			FSMessage fsMsg;
		}msgUnion;

		/* +0x1C */ MEMPTR<FSClient_t> fsClient; // 0x0988
		/* +0x20 */ MEMPTR<FSCmdBlock_t> fsCmdBlock; // 0x98C
		/* +0x24 */ uint32be fsStatusNew; // 0x990
	};

	static_assert(sizeof(FSAsyncResult) == 0x28);

	struct FSCmdBlockBody_t
	{
		iosu::fsa::FSAIpcCommand ipcData;
		uint8 ukn0820[0x10];
		uint8 ukn0830[0x10];
		uint8 ukn0840[0x10];
		uint8 ukn0850[0x10];
		uint8 ukn0860[0x10];
		uint8 ukn0870[0x10];
		MPTR fsCmdBlockBodyMPTR;
		uint32 ukn0884;
		uint32 ukn0888;
		uint32 destBuffer88CMPTR;
		uint32 ukn0890;
		uint32 ukn0894;
		uint32 ukn0898;
		uint32 ukn089C;
		uint32 ukn08A0;
		uint32 ukn08A4;
		uint32 ukn08A8;
		uint32 ukn08AC;
		uint8 ukn08B0[0x10];
		uint8 ukn08C0[0x10];
		uint8 ukn08D0[0x10];
		uint8 ukn08E0[0x10];
		uint8 ukn08F0[0x10];
		/* +0x0900 */ uint32be operationType;
		betype<IOSDevHandle> fsaDevHandle;
		/* +0x0908 */ uint16be ipcReqType; // 0 -> IoctlAsync, 1 -> IoctlvAsync
		uint8 ukn090A;
		uint8 ukn090B;
		uint32 ukn090C;
		uint32 ukn0910;
		uint32 ukn0914;
		uint32 ukn0918;
		uint32 ukn091C;
		uint32 ukn0920;
		uint32 ukn0924;
		uint32 ukn0928;
		uint32 ukn092C;
		uint32 ukn0930;
		uint32 ukn0934;
		/* +0x0938 */ MEMPTR<FSClientBody_t> fsClientBody;
		/* +0x093C */ uint32 statusCode; // not a status code but rather the state? Uses weird values for some reason
		/* +0x0940 */ uint32be cancelState; // bitmask. Bit 0 -> If set command has been canceled
		// return values
		/* +0x0944 */ uint32 returnValueMPTR; // returnedFilePos (used to store pointer to variable that holds return value?), also used by QUERYINFO to store pointer for result. Also used for GetCwd() to hold the pointer for the returned dir path. Also used by OPENFILE to hold returned fileHandle
		/* +0x0948 */ uint32 transferSize; // number of bytes to transfer
		// transfer control?
		uint32 uknVal094C;
		uint32 transferElemSize; // number of bytes of a single transferred element (count of elements can be calculated via count = transferSize/transferElemSize)
		uint32 uknVal0954; // this is set to max(0x10, transferSize) for reads and to min(0x40000, transferSize) for writes?
		// link for cmd queue
		MPTR nextMPTR;  // points towards FSCmdQueue->first
		MPTR previousMPTR; // points towards FSCmdQueue->last

		/* +0x960 */ betype<FSA_RESULT> lastFSAStatus;
		uint32 ukn0964;
		/* +0x0968 */ uint8 errHandling; // return error flag mask
		/* +0x096C */ FSAsyncResult asyncResult;
		/* +0x0994 */ MEMPTR<void> userData;
		/* +0x0998 */ OSMessageQueue syncTaskMsgQueue; // this message queue is used when mapping asynchronous tasks to synchronous API
		/* +0x09D4 */ OSMessage _syncTaskMsg[1];
		/* +0x09E4 */ MPTR cmdFinishFuncMPTR;
		/* +0x09E8 */ uint8 priority;
		uint8 uknStatusGuessed09E9;
		uint8 ukn09EA;
		uint8 ukn09EB;
		uint32 ukn09EC;
		uint32 ukn9F0;
		uint32be ukn9F4_lastErrorRelated;
		/* +0x9F8 */ MEMPTR<FSCmdBlock_t> selfCmdBlock;
		uint32 ukn9FC;
	};

	static_assert(sizeof(FSAsyncParams_t) == 0xC);
	static_assert(sizeof(FSCmdBlock_t) == 0xA80);

	#define FSA_CMD_FLAG_SET_POS					(1<<0)

	#define FSA_CMD_OPERATION_TYPE_CHANGEDIR		(0x5)
	#define FSA_CMD_OPERATION_TYPE_GETCWD			(0x6)
	#define FSA_CMD_OPERATION_TYPE_MAKEDIR			(0x7)
	#define FSA_CMD_OPERATION_TYPE_REMOVE			(0x8)
	#define FSA_CMD_OPERATION_TYPE_RENAME			(0x9)
	#define FSA_CMD_OPERATION_TYPE_OPENDIR			(0xA)
	#define FSA_CMD_OPERATION_TYPE_READDIR			(0xB)
	#define FSA_CMD_OPERATION_TYPE_CLOSEDIR			(0xD)
	#define FSA_CMD_OPERATION_TYPE_OPENFILE			(0xE)
	#define FSA_CMD_OPERATION_TYPE_READ				(0xF)
	#define FSA_CMD_OPERATION_TYPE_WRITE			(0x10)
	#define FSA_CMD_OPERATION_TYPE_GETPOS			(0x11)
	#define FSA_CMD_OPERATION_TYPE_SETPOS			(0x12)
	#define FSA_CMD_OPERATION_TYPE_ISEOF			(0x13)
	#define FSA_CMD_OPERATION_TYPE_GETSTATFILE		(0x14)
	#define FSA_CMD_OPERATION_TYPE_CLOSEFILE		(0x15)
	#define FSA_CMD_OPERATION_TYPE_QUERYINFO	(0x18)
	#define FSA_CMD_OPERATION_TYPE_APPENDFILE		(0x19)
	#define FSA_CMD_OPERATION_TYPE_TRUNCATEFILE		(0x1A)
	#define FSA_CMD_OPERATION_TYPE_FLUSHQUOTA		(0x1E)


	#define FSA_CMD_STATUS_CODE_D900A21				0xD900A21	// cmd block is initialized
	#define FSA_CMD_STATUS_CODE_D900A22				0xD900A22	// cmd block is queued
	#define FSA_CMD_STATUS_CODE_D900A24				0xD900A24	// cmd block was processed and is available again
	#define FSA_CMD_STATUS_CODE_D900A26				0xD900A26	// cmd block result is being processed

	enum FS_VOLSTATE : sint32
	{
		FS_VOLSTATE_READY = 1,
	};

	// internal interface
	sint32 __FSQueryInfoAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint8* queryString, uint32 queryType, void* queryResult, uint32 errHandling, FSAsyncParamsNew_t* fsAsyncParams);

	// coreinit exports
	FS_RESULT FSAddClientEx(FSClient_t* fsClient, uint32 uknR4, uint32 errHandling);
	FS_RESULT FSAddClient(FSClient_t* fsClient, uint32 errHandling);
	FS_RESULT FSDelClient(FSClient_t* fsClient, uint32 errHandling);

	void FSInitCmdBlock(FSCmdBlock_t* fsCmdBlock);

	sint32 FSOpenFileAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, char* mode, FSFileHandleDepr_t* fileHandle, uint32 errHandling, FSAsyncParamsNew_t* asyncParams);
	sint32 FSOpenFile(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, char* mode, FSFileHandleDepr_t* fileHandle, uint32 errHandling);
	
	sint32 FSReadFileAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, void* dst, uint32 size, uint32 count, uint32 fileHandle, uint32 flag, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams);
	sint32 FSReadFile(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, void* dst, uint32 size, uint32 count, uint32 fileHandle, uint32 flag, uint32 errorMask);
	sint32 FSReadFileWithPosAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, void* dst, uint32 size, uint32 count, uint32 filePos, uint32 fileHandle, uint32 flag, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams);
	sint32 FSReadFileWithPos(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, void* dst, uint32 size, uint32 count, uint32 filePos, uint32 fileHandle, uint32 flag, uint32 errorMask);
	
	sint32 FSWriteFileAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, void* src, uint32 size, uint32 count, uint32 fileHandle, uint32 flag, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams);
	sint32 FSWriteFile(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, void* src, uint32 size, uint32 count, uint32 fileHandle, uint32 flag, uint32 errorMask);
	sint32 FSWriteFileWithPosAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, void* src, uint32 size, uint32 count, uint32 filePos, uint32 fileHandle, uint32 flag, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams);
	sint32 FSWriteFileWithPos(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, void* src, uint32 size, uint32 count, uint32 filePos, uint32 fileHandle, uint32 flag, uint32 errorMask);

	sint32 FSSetPosFileAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32 filePos, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams);
	sint32 FSSetPosFile(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32 filePos, uint32 errorMask);
	sint32 FSGetPosFileAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32be* returnedFilePos, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams);
	sint32 FSGetPosFile(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32be* returnedFilePos, uint32 errorMask);

	sint32 FSAppendFileAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32 size, uint32 count, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams);
	sint32 FSAppendFile(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32 size, uint32 count, uint32 errorMask);

	sint32 FSIsEofAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams);
	sint32 FSIsEof(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32 errorMask);

	sint32 FSRenameAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* srcPath, char* dstPath, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams);
	sint32 FSRename(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* srcPath, char* dstPath, uint32 errorMask);
	sint32 FSRemoveAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint8* filePath, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams);
	sint32 FSRemove(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint8* filePath, uint32 errorMask);
	sint32 FSMakeDirAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, const uint8* dirPath, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams);
	sint32 FSMakeDir(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, const uint8* path, uint32 errorMask);
	sint32 FSChangeDirAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams);
	sint32 FSChangeDir(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, uint32 errorMask);
	sint32 FSGetCwdAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* dirPathOut, sint32 dirPathMaxLen, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams);
	sint32 FSGetCwd(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* dirPathOut, sint32 dirPathMaxLen, uint32 errorMask);

	sint32 FSGetFreeSpaceSizeAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, const char* path, FSLargeSize* returnedFreeSize, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams);
	sint32 FSGetFreeSpaceSize(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, const char* path, FSLargeSize* returnedFreeSize, uint32 errorMask);

	sint32 FSOpenDirAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, FSDirHandlePtr dirHandleOut, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams);
	sint32 FSOpenDir(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, FSDirHandlePtr dirHandleOut, uint32 errorMask);
	sint32 FSReadDirAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, FSDirHandle2 dirHandle, FSDirEntry_t* dirEntryOut, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams);
	sint32 FSReadDir(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, FSDirHandle2 dirHandle, FSDirEntry_t* dirEntryOut, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams);
	sint32 FSCloseDirAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, FSDirHandle2 dirHandle, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams);
	sint32 FSCloseDir(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, FSDirHandle2 dirHandle, uint32 errorMask);

	sint32 FSFlushQuotaAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams);
	sint32 FSFlushQuota(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, uint32 errorMask);

	FS_VOLSTATE FSGetVolumeState(FSClient_t* fsClient);

	void InitializeFS();
};

