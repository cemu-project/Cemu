#pragma once
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"
#include "Cafe/IOSU/iosu_ipc_common.h"
#include "Cafe/IOSU/fsa/fsa_types.h"
#include "Cafe/IOSU/fsa/iosu_fsa.h"
#include "coreinit_MessageQueue.h"

typedef MEMPTR<betype<FSFileHandle2>> FSFileHandlePtr;
typedef MEMPTR<betype<FSDirHandle2>> FSDirHandlePtr;

typedef uint32 FSAClientHandle;

struct FSAsyncParams
{
	MEMPTR<void> userCallback;
	MEMPTR<void> userContext;
	MEMPTR<coreinit::OSMessageQueue> ioMsgQueue;
};
static_assert(sizeof(FSAsyncParams) == 0xC);

namespace coreinit
{
	struct FSCmdBlockBody;

	struct FSCmdQueue
	{
		enum class QUEUE_FLAG : uint32
		{
			IS_FULL = (1 << 0), // waiting for Ioctl(v) result
			CANCEL_ALL = (1 << 4),
		};

		/* +0x00 */ MEMPTR<FSCmdBlockBody> first;
		/* +0x04 */ MEMPTR<FSCmdBlockBody> last;
		/* +0x08 */ OSFastMutex fastMutex;
		/* +0x34 */ MPTR dequeueHandlerFuncMPTR;
		/* +0x38 */ uint32be numCommandsInFlight;
		/* +0x3C */ uint32 numMaxCommandsInFlight;
		/* +0x40 */ betype<QUEUE_FLAG> queueFlags;
	};
	DEFINE_ENUM_FLAG_OPERATORS(FSCmdQueue::QUEUE_FLAG);

	static_assert(sizeof(FSCmdQueue) == 0x44);

#define FS_CLIENT_BUFFER_SIZE (5888)
#define FS_CMD_BLOCK_SIZE (2688)

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
			} data;
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
		/* +0x14C4 */ MEMPTR<struct FSCmdBlockBody> currentCmdBlockBody; // set to currently active cmd
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
		MEMPTR<FSClientBody_t> fsClientBodyNext;	 // next FSClientBody_t* in list of registered clients (list is circular, the last element points to the first element)
		uint32 ukn1618;
		/* +0x161C */ MEMPTR<FSClient_t> selfClient; // pointer to FSClient struct which holds this FSClientBody
		uint32 ukn1620;
	};

	struct FSAsyncResult
	{
		/* +0x00 */ FSAsyncParams fsAsyncParamsNew;

		// fs message storage
		struct FSMessage
		{
			/* +0x0C / 0x0978 */ MEMPTR<FSAsyncResult> fsAsyncResult;
			/* +0x10 */ MPTR fsClientMPTR2;	 // 0x097C
			/* +0x14 */ MPTR fsCmdBlockMPTR; // 0x0980
			/* +0x18 */ MPTR commandType;	 // 0x0984
		};

		union
		{
			OSMessage osMsg;
			FSMessage fsMsg;
		} msgUnion;

		/* +0x1C */ MEMPTR<FSClient_t> fsClient;	 // 0x0988
		/* +0x20 */ MEMPTR<FSCmdBlock_t> fsCmdBlock; // 0x98C
		/* +0x24 */ uint32be fsStatusNew;			 // 0x990
	};

	static_assert(sizeof(FSAsyncResult) == 0x28);

	struct FSCmdBlockReturnValues_t
	{
		union
		{
			uint8 ukn0[0x14];
			struct
			{
				MEMPTR<betype<FSResHandle>> handlePtr;
			} cmdOpenFile;
			struct
			{
				MEMPTR<uint32be> filePosPtr;
			} cmdGetPosFile;
			struct
			{
				uint32be transferSize;
				uint32be uknVal094C;
				uint32be transferElemSize;
				uint32be uknVal0954;
			} cmdReadFile;
			struct
			{
				uint32be transferSize;
				uint32be uknVal094C;
				uint32be transferElemSize;
				uint32be uknVal0954;
			} cmdWriteFile;
			struct
			{
				MEMPTR<uint32be> handlePtr;
			} cmdOpenDir;
			struct
			{
				MEMPTR<FSDirEntry_t> dirEntryPtr;
			} cmdReadDir;
			struct
			{
				MEMPTR<char> pathPtr;
				uint32be transferSize;
			} cmdGetCwd;
			struct
			{
				MEMPTR<void> queryResultPtr;
			} cmdQueryInfo;
			struct
			{
				MEMPTR<void> resultPtr;
			} cmdStatFile;
		};
	};

	static_assert(sizeof(FSCmdBlockReturnValues_t) == 0x14);

	struct FSCmdBlockBody
	{
		iosu::fsa::FSAShimBuffer fsaShimBuffer;
		/* +0x0938 */ MEMPTR<FSClientBody_t> fsClientBody;
		/* +0x093C */ uint32 statusCode;	// not a status code but rather the state? Uses weird values for some reason
		/* +0x0940 */ uint32be cancelState; // bitmask. Bit 0 -> If set command has been canceled
		FSCmdBlockReturnValues_t returnValues;
		// link for cmd queue
		MEMPTR<FSCmdBlockBody> next;
		MEMPTR<FSCmdBlockBody> previous;
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

	static_assert(sizeof(FSCmdBlock_t) == 0xA80);

#define FSA_CMD_FLAG_SET_POS (1 << 0)

#define FSA_CMD_STATUS_CODE_D900A21 0xD900A21 // cmd block is initialized
#define FSA_CMD_STATUS_CODE_D900A22 0xD900A22 // cmd block is queued
#define FSA_CMD_STATUS_CODE_D900A24 0xD900A24 // cmd block was processed and is available again
#define FSA_CMD_STATUS_CODE_D900A26 0xD900A26 // cmd block result is being processed

	enum FS_VOLSTATE : sint32
	{
		FS_VOLSTATE_READY = 1,
	};

	// internal interface
	sint32 __FSQueryInfoAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint8* queryString, uint32 queryType, void* queryResult, uint32 errHandling, FSAsyncParams* fsAsyncParams);

	// coreinit exports
	FS_RESULT FSAddClientEx(FSClient_t* fsClient, uint32 uknR4, uint32 errHandling);
	FS_RESULT FSAddClient(FSClient_t* fsClient, uint32 errHandling);
	FS_RESULT FSDelClient(FSClient_t* fsClient, uint32 errHandling);

	void FSInitCmdBlock(FSCmdBlock_t* fsCmdBlock);

	sint32 FSOpenFileAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, char* mode, FSFileHandlePtr outFileHandle, uint32 errHandling, FSAsyncParams* asyncParams);
	sint32 FSOpenFile(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, char* mode, FSFileHandlePtr outFileHandle, uint32 errHandling);

	sint32 FSReadFileAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, void* dst, uint32 size, uint32 count, uint32 fileHandle, uint32 flag, uint32 errorMask, FSAsyncParams* fsAsyncParams);
	sint32 FSReadFile(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, void* dst, uint32 size, uint32 count, uint32 fileHandle, uint32 flag, uint32 errorMask);
	sint32 FSReadFileWithPosAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, void* dst, uint32 size, uint32 count, uint32 filePos, uint32 fileHandle, uint32 flag, uint32 errorMask, FSAsyncParams* fsAsyncParams);
	sint32 FSReadFileWithPos(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, void* dst, uint32 size, uint32 count, uint32 filePos, uint32 fileHandle, uint32 flag, uint32 errorMask);

	sint32 FSWriteFileAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, void* src, uint32 size, uint32 count, uint32 fileHandle, uint32 flag, uint32 errorMask, FSAsyncParams* fsAsyncParams);
	sint32 FSWriteFile(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, void* src, uint32 size, uint32 count, uint32 fileHandle, uint32 flag, uint32 errorMask);
	sint32 FSWriteFileWithPosAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, void* src, uint32 size, uint32 count, uint32 filePos, uint32 fileHandle, uint32 flag, uint32 errorMask, FSAsyncParams* fsAsyncParams);
	sint32 FSWriteFileWithPos(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, void* src, uint32 size, uint32 count, uint32 filePos, uint32 fileHandle, uint32 flag, uint32 errorMask);

	sint32 FSSetPosFileAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32 filePos, uint32 errorMask, FSAsyncParams* fsAsyncParams);
	sint32 FSSetPosFile(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32 filePos, uint32 errorMask);
	sint32 FSGetPosFileAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32be* returnedFilePos, uint32 errorMask, FSAsyncParams* fsAsyncParams);
	sint32 FSGetPosFile(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32be* returnedFilePos, uint32 errorMask);

	sint32 FSAppendFileAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32 size, uint32 count, uint32 errorMask, FSAsyncParams* fsAsyncParams);
	sint32 FSAppendFile(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32 size, uint32 count, uint32 errorMask);

	sint32 FSIsEofAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32 errorMask, FSAsyncParams* fsAsyncParams);
	sint32 FSIsEof(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32 errorMask);

	sint32 FSRenameAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* srcPath, char* dstPath, uint32 errorMask, FSAsyncParams* fsAsyncParams);
	sint32 FSRename(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* srcPath, char* dstPath, uint32 errorMask);
	sint32 FSRemoveAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint8* filePath, uint32 errorMask, FSAsyncParams* fsAsyncParams);
	sint32 FSRemove(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint8* filePath, uint32 errorMask);
	sint32 FSMakeDirAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, const char* dirPath, uint32 errorMask, FSAsyncParams* fsAsyncParams);
	sint32 FSMakeDir(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, const char* path, uint32 errorMask);
	sint32 FSChangeDirAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, uint32 errorMask, FSAsyncParams* fsAsyncParams);
	sint32 FSChangeDir(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, uint32 errorMask);
	sint32 FSGetCwdAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* dirPathOut, sint32 dirPathMaxLen, uint32 errorMask, FSAsyncParams* fsAsyncParams);
	sint32 FSGetCwd(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* dirPathOut, sint32 dirPathMaxLen, uint32 errorMask);

	sint32 FSGetFreeSpaceSizeAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, const char* path, FSLargeSize* returnedFreeSize, uint32 errorMask, FSAsyncParams* fsAsyncParams);
	sint32 FSGetFreeSpaceSize(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, const char* path, FSLargeSize* returnedFreeSize, uint32 errorMask);

	sint32 FSOpenDirAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, FSDirHandlePtr dirHandleOut, uint32 errorMask, FSAsyncParams* fsAsyncParams);
	sint32 FSOpenDir(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, FSDirHandlePtr dirHandleOut, uint32 errorMask);
	sint32 FSReadDirAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, FSDirHandle2 dirHandle, FSDirEntry_t* dirEntryOut, uint32 errorMask, FSAsyncParams* fsAsyncParams);
	sint32 FSReadDir(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, FSDirHandle2 dirHandle, FSDirEntry_t* dirEntryOut, uint32 errorMask, FSAsyncParams* fsAsyncParams);
	sint32 FSCloseDirAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, FSDirHandle2 dirHandle, uint32 errorMask, FSAsyncParams* fsAsyncParams);
	sint32 FSCloseDir(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, FSDirHandle2 dirHandle, uint32 errorMask);

	sint32 FSFlushQuotaAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, uint32 errorMask, FSAsyncParams* fsAsyncParams);
	sint32 FSFlushQuota(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, uint32 errorMask);

	FS_VOLSTATE FSGetVolumeState(FSClient_t* fsClient);

	void InitializeFS();
}; // namespace coreinit
