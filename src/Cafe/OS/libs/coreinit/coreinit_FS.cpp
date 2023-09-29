#include <OS/RPL/rpl.h>
#include "config/ActiveSettings.h"
#include "Cafe/OS/libs/coreinit/coreinit_SystemInfo.h"
#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"
#include "Cafe/OS/libs/coreinit/coreinit_FS.h"
#include "Cafe/OS/libs/coreinit/coreinit_MessageQueue.h"
#include "util/helpers/Semaphore.h"
#include "Cafe/HW/Espresso/PPCCallback.h"
#include "Cafe/IOSU/iosu_ipc_common.h"
#include "coreinit_IPC.h"
#include "Cafe/Filesystem/fsc.h"
#include "coreinit_IPCBuf.h"

#define FS_CB_PLACEHOLDER_FINISHCMD (MPTR)(0xF122330E)

// return false if src+'\0' does not fit into dst
template<std::size_t Size>
bool strcpy_whole(char (&dst)[Size], const char* src)
{
	size_t inputLength = strlen(src);
	if ((inputLength + 1) > Size)
	{
		dst[0] = '\0';
		return false;
	}
	strcpy(dst, src);
	return true;
}

bool strcpy_whole(char* dst, size_t dstLength, const char* src)
{
	size_t inputLength = strlen(src);
	if ((inputLength + 1) > dstLength)
	{
		dst[0] = '\0';
		return false;
	}
	strcpy(dst, src);
	return true;
}

namespace coreinit
{
	SysAllocator<OSMutex> s_fsGlobalMutex;

	inline void FSLockMutex()
	{
		OSLockMutex(&s_fsGlobalMutex);
	}

	inline void FSUnlockMutex()
	{
		OSUnlockMutex(&s_fsGlobalMutex);
	}

	void _debugVerifyCommand(const char* stage, FSCmdBlockBody_t* fsCmdBlockBody);

	bool sFSInitialized = true; // this should be false but it seems like some games rely on FSInit being called before main()? Twilight Princess for example reads files before it calls FSInit
	bool sFSShutdown = false;

	enum class MOUNT_TYPE : uint32
	{
		SD = 0,
		// 1 = usb?
	};

	struct FS_MOUNT_SOURCE
	{
		uint32be sourceType; // ukn values
		char path[128];		 // todo - determine correct length
	};

	FS_RESULT FSGetMountSourceNext(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, MOUNT_TYPE mountSourceType, FS_MOUNT_SOURCE* mountSourceInfo, FS_ERROR_MASK errMask)
	{
		if (mountSourceType == MOUNT_TYPE::SD)
		{
			// This function is supposed to be called after an initial FSGetMountSource call => always returns FS_RESULT::END_ITERATION because we only have one SD Card
			// It *might* causes issues if this function is called for getting the first MountSource (instead of "FSGetMountSource")
			cemu_assert_suspicious();
			return FS_RESULT::END_ITERATION;
		}
		else
		{
			cemu_assert_unimplemented();
		}

		return FS_RESULT::END_ITERATION;
	}

	FS_RESULT FSGetMountSource(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, MOUNT_TYPE mountSourceType, FS_MOUNT_SOURCE* mountSourceInfo, FS_ERROR_MASK errMask)
	{
		// This implementation is simplified A LOT compared to what the Wii U is actually doing. On Cemu we expect to only have one mountable source (SD Card) anyway,
		// so we can just hard code it. Other mount types are not (yet) supported.
		if (mountSourceType == MOUNT_TYPE::SD)
		{
			mountSourceInfo->sourceType = 0;
			strcpy(mountSourceInfo->path, "/sd");
			return FS_RESULT::SUCCESS;
		}
		else
		{
			cemu_assert_unimplemented();
		}

		return FS_RESULT::END_ITERATION;
	}

	bool _sdCard01Mounted = false;
	bool _mlc01Mounted = false;

	void mountSDCard()
	{
		if (_sdCard01Mounted)
			return;

		std::error_code ec;
		const auto path = ActiveSettings::GetUserDataPath("sdcard/");
		fs::create_directories(path, ec);
		FSCDeviceHostFS_Mount("/vol/external01", _pathToUtf8(path), FSC_PRIORITY_BASE);

		_sdCard01Mounted = true;
	}

	FS_RESULT FSMount(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, FS_MOUNT_SOURCE* mountSourceInfo, char* mountPathOut, sint32 mountPathMaxLength, FS_ERROR_MASK errMask)
	{
		if (mountSourceInfo->sourceType != 0)
		{
			return FS_RESULT::ERR_PLACEHOLDER;
		}

		if (!strcmp(mountSourceInfo->path, "/sd"))
		{
			const char* sdMountPath = "/vol/external01";
			// note: mount path should not end with a slash
			if (!strcpy_whole(mountPathOut, mountPathMaxLength, sdMountPath))
				return FS_RESULT::ERR_PLACEHOLDER; // string does not fit
			mountSDCard();
		}

		return FS_RESULT::SUCCESS;
	}

	FS_RESULT FSBindMount(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* mountPathSrc, char* mountPathOut, FS_ERROR_MASK errMask)
	{
		if (strcmp(mountPathSrc, "/dev/sdcard01") == 0)
		{
			if (_sdCard01Mounted)
				return FS_RESULT::ERR_PLACEHOLDER;

			std::error_code ec;
			const auto path = ActiveSettings::GetUserDataPath("sdcard/");
			fs::create_directories(path, ec);
			if (!FSCDeviceHostFS_Mount(mountPathOut, _pathToUtf8(path), FSC_PRIORITY_BASE))
				return FS_RESULT::ERR_PLACEHOLDER;
			_sdCard01Mounted = true;
		}
		else if (strcmp(mountPathSrc, "/dev/mlc01") == 0)
		{
			if (_mlc01Mounted)
				return FS_RESULT::ERR_PLACEHOLDER;

			if (!FSCDeviceHostFS_Mount(mountPathOut, _pathToUtf8(ActiveSettings::GetMlcPath()), FSC_PRIORITY_BASE))
				return FS_RESULT::ERR_PLACEHOLDER;
			_mlc01Mounted = true;
		}
		else
		{
			return FS_RESULT::ERR_PLACEHOLDER;
		}

		return FS_RESULT::SUCCESS;
	}

	// return the aligned FSClientBody struct inside a FSClient struct
	FSClientBody_t* __FSGetClientBody(FSClient_t* fsClient)
	{
		// align pointer to 64 bytes
		if (fsClient == nullptr)
			return nullptr;
		FSClientBody_t* fsClientBody = (FSClientBody_t*)(((uintptr_t)fsClient + 0x3F) & ~0x3F);
		fsClientBody->selfClient = fsClient;
		return fsClientBody;
	}

	// return the aligned FSClientBody struct inside a FSClient struct
	FSCmdBlockBody_t* __FSGetCmdBlockBody(FSCmdBlock_t* fsCmdBlock)
	{
		// align pointer to 64 bytes
		if (fsCmdBlock == nullptr)
			return nullptr;
		FSCmdBlockBody_t* fsCmdBlockBody = (FSCmdBlockBody_t*)(((uintptr_t)fsCmdBlock + 0x3F) & ~0x3F);
		fsCmdBlockBody->selfCmdBlock = fsCmdBlock;
		return fsCmdBlockBody;
	}

	void __FSErrorAndBlock(std::string_view msg)
	{
		cemuLog_log(LogType::Force, "Critical error in FS: {}", msg);
		while (true)
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}

	/* FS client management */
	FSClientBody_t* g_fsRegisteredClientBodies = nullptr;

	sint32 FSGetClientNum()
	{
		sint32 clientNum = 0;
		FSClientBody_t* fsBodyItr = g_fsRegisteredClientBodies;
		while (fsBodyItr)
		{
			clientNum++;
			fsBodyItr = fsBodyItr->fsClientBodyNext.GetPtr();
		}
		return clientNum;
	}

	bool __FSIsClientRegistered(FSClientBody_t* fsClientBody)
	{
		FSLockMutex();
		FSClientBody_t* fsClientBodyFirst = g_fsRegisteredClientBodies;
		FSClientBody_t* fsClientBodyItr = fsClientBodyFirst;
		if (fsClientBodyItr == NULL)
		{
			FSUnlockMutex();
			return false;
		}
		while (true)
		{
			if (fsClientBody == fsClientBodyItr)
			{
				FSUnlockMutex();
				return true;
			}
			// next
			fsClientBodyItr = fsClientBodyItr->fsClientBodyNext.GetPtr();
			if (fsClientBodyItr == MPTR_NULL)
				break;
			if (fsClientBodyItr == fsClientBodyFirst)
				break;
		}
		FSUnlockMutex();
		return false;
	}

	void __FSInitCmdQueue(FSCmdQueue* fsCmdQueueBE, MPTR dequeueHandlerFuncMPTR, uint32 numMaxCommandsInFlight)
	{
		cemu_assert(numMaxCommandsInFlight > 0);
		fsCmdQueueBE->dequeueHandlerFuncMPTR = _swapEndianU32(dequeueHandlerFuncMPTR);
		fsCmdQueueBE->numCommandsInFlight = 0;
		fsCmdQueueBE->numMaxCommandsInFlight = numMaxCommandsInFlight;
		coreinit::OSFastMutex_Init(&fsCmdQueueBE->fastMutex, nullptr);
		fsCmdQueueBE->firstMPTR = _swapEndianU32(0);
		fsCmdQueueBE->lastMPTR = _swapEndianU32(0);
	}

	void FSInit()
	{
		sFSInitialized = true;
	}

	void FSShutdown()
	{
		cemuLog_log(LogType::Force, "FSShutdown called");
	}

	FS_RESULT FSAddClientEx(FSClient_t* fsClient, uint32 uknR4, uint32 errHandling)
	{
		if (!sFSInitialized || sFSShutdown || !fsClient)
		{
			__FSErrorAndBlock("Called FSAddClient(Ex) with invalid parameters or while FS is not initialized");
			return FS_RESULT::FATAL_ERROR;
		}
		FSLockMutex();
		if (uknR4 != 0)
		{
			uint32 uknValue = memory_readU32(uknR4 + 0x00);
			if (uknValue == 0)
			{
				FSUnlockMutex();
				__FSErrorAndBlock("FSAddClientEx - unknown error");
				return FS_RESULT::FATAL_ERROR;
			}
		}
		if (__FSIsClientRegistered(__FSGetClientBody(fsClient)))
		{
			FSUnlockMutex();
			__FSErrorAndBlock("Called FSAddClient(Ex) on client that was already added");
			return FS_RESULT::FATAL_ERROR;
		}
		FSClientBody_t* fsClientBody = __FSGetClientBody(fsClient);
		__FSInitCmdQueue(&fsClientBody->fsCmdQueue, MPTR_NULL, 1);

		IOSDevHandle devHandle = IOS_Open("/dev/fsa", 0);
		if (IOS_ResultIsError((IOS_ERROR)devHandle))
		{
			cemuLog_log(LogType::Force, "FSAddClientEx(): Exhausted device handles");
			cemu_assert(IOS_ResultIsError((IOS_ERROR)devHandle));
			FSUnlockMutex();
			return FS_RESULT::FATAL_ERROR;
		}
		fsClientBody->iosuFSAHandle = devHandle;

		// add to list of registered clients
		if (g_fsRegisteredClientBodies != MPTR_NULL)
		{
			fsClientBody->fsClientBodyNext = g_fsRegisteredClientBodies;
			g_fsRegisteredClientBodies = fsClientBody;
		}
		else
		{
			g_fsRegisteredClientBodies = fsClientBody;
			fsClientBody->fsClientBodyNext = nullptr;
		}
		FSUnlockMutex();
		return FS_RESULT::SUCCESS;
	}

	FS_RESULT FSAddClient(FSClient_t* fsClient, uint32 errHandling)
	{
		return FSAddClientEx(fsClient, 0, errHandling);
	}

	FS_RESULT FSDelClient(FSClient_t* fsClient, uint32 errHandling)
	{
		if (sFSInitialized == false || sFSShutdown == true || !fsClient)
		{
			__FSErrorAndBlock("Called FSDelClient with invalid parameters or while FS is not initialized");
			return FS_RESULT::FATAL_ERROR;
		}
		FSClientBody_t* fsClientBody = __FSGetClientBody(fsClient);
		FSLockMutex();
		IOS_Close(fsClientBody->iosuFSAHandle);
		// todo: wait till in-flight transactions are done
		// remove from list
		if (g_fsRegisteredClientBodies == fsClientBody)
		{
			// first entry in list
			g_fsRegisteredClientBodies = fsClientBody->fsClientBodyNext.GetPtr();
		}
		else
		{
			// scan for entry in list
			FSClientBody_t* fsBodyItr = g_fsRegisteredClientBodies;
			FSClientBody_t* fsBodyPrev = g_fsRegisteredClientBodies;
			bool entryFound = false;
			while (fsBodyItr)
			{
				if (fsBodyItr == fsClientBody)
				{
					fsBodyPrev->fsClientBodyNext = fsClientBody->fsClientBodyNext;
					entryFound = true;
					break;
				}
				// next
				fsBodyPrev = fsBodyItr;
				fsBodyItr = fsBodyItr->fsClientBodyNext.GetPtr();
			}
			if (entryFound == false)
			{
#ifdef CEMU_DEBUG_ASSERT
				assert_dbg();
#endif
			}
		}
		FSUnlockMutex();
		return FS_RESULT::SUCCESS;
	}

	/* FS command queue */

	Semaphore g_semaphoreQueuedCmds;

	void __FSQueueCmdByPriority(FSCmdQueue* fsCmdQueueBE, FSCmdBlockBody_t* fsCmdBlockBody, bool stopAtEqualPriority)
	{
		MPTR fsCmdBlockBodyMPTR = memory_getVirtualOffsetFromPointer(fsCmdBlockBody);
		if (_swapEndianU32(fsCmdQueueBE->firstMPTR) == MPTR_NULL)
		{
			// queue is currently empty
			cemu_assert(fsCmdQueueBE->lastMPTR == MPTR_NULL);
			fsCmdQueueBE->firstMPTR = _swapEndianU32(fsCmdBlockBodyMPTR);
			fsCmdQueueBE->lastMPTR = _swapEndianU32(fsCmdBlockBodyMPTR);
			fsCmdBlockBody->nextMPTR = _swapEndianU32(MPTR_NULL);
			fsCmdBlockBody->previousMPTR = _swapEndianU32(MPTR_NULL);
			return;
		}
		// iterate from last to first element as long as iterated priority is lower
		FSCmdBlockBody_t* fsCmdBlockBodyItrPrev = NULL;
		FSCmdBlockBody_t* fsCmdBlockBodyItr = (FSCmdBlockBody_t*)memory_getPointerFromVirtualOffsetAllowNull(_swapEndianU32(fsCmdQueueBE->lastMPTR));
		while (true)
		{
			if (fsCmdBlockBodyItr == NULL)
			{
				// insert at the head of the list
				fsCmdQueueBE->firstMPTR = _swapEndianU32(fsCmdBlockBodyMPTR);
				fsCmdBlockBody->nextMPTR = _swapEndianU32(memory_getVirtualOffsetFromPointer(fsCmdBlockBodyItrPrev));
				fsCmdBlockBody->previousMPTR = _swapEndianU32(MPTR_NULL);
				fsCmdBlockBodyItrPrev->previousMPTR = _swapEndianU32(memory_getVirtualOffsetFromPointer(fsCmdBlockBody));
				return;
			}
			// compare priority
			if ((stopAtEqualPriority && fsCmdBlockBodyItr->priority >= fsCmdBlockBody->priority) || (stopAtEqualPriority == false && fsCmdBlockBodyItr->priority > fsCmdBlockBody->priority))
			{
				// insert cmd here
				if (fsCmdBlockBodyItrPrev != NULL)
				{
					fsCmdBlockBody->nextMPTR = _swapEndianU32(memory_getVirtualOffsetFromPointer(fsCmdBlockBodyItrPrev));
				}
				else
				{
					fsCmdBlockBody->nextMPTR = _swapEndianU32(MPTR_NULL);
					fsCmdQueueBE->lastMPTR = _swapEndianU32(fsCmdBlockBodyMPTR);
				}
				fsCmdBlockBody->previousMPTR = _swapEndianU32(memory_getVirtualOffsetFromPointer(fsCmdBlockBodyItr));
				if (fsCmdBlockBodyItrPrev)
					fsCmdBlockBodyItrPrev->previousMPTR = _swapEndianU32(memory_getVirtualOffsetFromPointer(fsCmdBlockBody));
				fsCmdBlockBodyItr->nextMPTR = _swapEndianU32(memory_getVirtualOffsetFromPointer(fsCmdBlockBody));
				return;
			}
			// next
			fsCmdBlockBodyItrPrev = fsCmdBlockBodyItr;
			fsCmdBlockBodyItr = (FSCmdBlockBody_t*)memory_getPointerFromVirtualOffsetAllowNull(_swapEndianU32(fsCmdBlockBodyItr->previousMPTR));
		}
	}

	FSCmdBlockBody_t* __FSTakeCommandFromQueue(FSCmdQueue* cmdQueue)
	{
		FSCmdBlockBody_t* dequeuedCmd = nullptr;
		if (_swapEndianU32(cmdQueue->firstMPTR) != MPTR_NULL)
		{
			dequeuedCmd = (FSCmdBlockBody_t*)memory_getPointerFromVirtualOffset(_swapEndianU32(cmdQueue->firstMPTR));
			// dequeue cmd
			if (cmdQueue->firstMPTR == cmdQueue->lastMPTR)
				cmdQueue->lastMPTR = _swapEndianU32(MPTR_NULL);
			cmdQueue->firstMPTR = dequeuedCmd->nextMPTR;
			dequeuedCmd->nextMPTR = _swapEndianU32(MPTR_NULL);
			if (_swapEndianU32(dequeuedCmd->nextMPTR) != MPTR_NULL)
			{
				FSCmdBlockBody_t* fsCmdBodyNext = (FSCmdBlockBody_t*)memory_getPointerFromVirtualOffset(_swapEndianU32(dequeuedCmd->nextMPTR));
				fsCmdBodyNext->previousMPTR = _swapEndianU32(MPTR_NULL);
			}
		}
		return dequeuedCmd;
	}

	void __FSAIoctlResponseCallback(PPCInterpreter_t* hCPU);

	void __FSAIPCSubmitCommandAsync(iosu::fsa::FSAShimBuffer* shimBuffer, const MEMPTR<void>& callback, void* context)
	{
		if (shimBuffer->ipcReqType == 0)
		{
			IOS_ERROR r = IOS_IoctlAsync(shimBuffer->fsaDevHandle, shimBuffer->operationType, &shimBuffer->request, sizeof(shimBuffer->request), &shimBuffer->response, sizeof(shimBuffer->response), callback, context);
			cemu_assert(!IOS_ResultIsError(r));
		}
		else if (shimBuffer->ipcReqType == 1)
		{
			IOS_ERROR r = IOS_IoctlvAsync(shimBuffer->fsaDevHandle, shimBuffer->operationType, shimBuffer->ioctlvVecIn, shimBuffer->ioctlvVecOut, shimBuffer->ioctlvVec, callback, context);
			cemu_assert(!IOS_ResultIsError(r));
		}
		else
		{
			cemu_assert_error();
		}
	}

	FSA_RESULT __FSADecodeIOSErrorToFSA(IOS_ERROR result)
	{
		return (FSA_RESULT)result;
	}

	FSA_RESULT __FSAIPCSubmitCommand(iosu::fsa::FSAShimBuffer* shimBuffer)
	{
		if (shimBuffer->ipcReqType == 0)
		{
			IOS_ERROR result = IOS_Ioctl(shimBuffer->fsaDevHandle, shimBuffer->operationType, &shimBuffer->request, sizeof(shimBuffer->request), &shimBuffer->response, sizeof(shimBuffer->response));
			return __FSADecodeIOSErrorToFSA(result);
		}
		else if (shimBuffer->ipcReqType == 1)
		{
			IOS_ERROR result = IOS_Ioctlv(shimBuffer->fsaDevHandle, shimBuffer->operationType, shimBuffer->ioctlvVecIn, shimBuffer->ioctlvVecOut, shimBuffer->ioctlvVec);
			return __FSADecodeIOSErrorToFSA(result);
		}
		return FSA_RESULT::FATAL_ERROR;
	}

	void __FSUpdateQueue(FSCmdQueue* cmdQueue)
	{
		FSLockMutex();
		if (cmdQueue->numCommandsInFlight < cmdQueue->numMaxCommandsInFlight)
		{
			FSCmdBlockBody_t* dequeuedCommand = __FSTakeCommandFromQueue(cmdQueue);
			if (dequeuedCommand)
			{
				cmdQueue->numCommandsInFlight += 1;
				if (cmdQueue->numCommandsInFlight >= cmdQueue->numMaxCommandsInFlight)
					cmdQueue->queueFlags = cmdQueue->queueFlags | FSCmdQueue::QUEUE_FLAG::IS_FULL;
				cemu_assert_debug(cmdQueue->dequeueHandlerFuncMPTR == 0); // not supported. We HLE call the handler here
				__FSAIPCSubmitCommandAsync(&dequeuedCommand->fsaShimBuffer, MEMPTR<void>(PPCInterpreter_makeCallableExportDepr(__FSAIoctlResponseCallback)), &dequeuedCommand->fsaShimBuffer);
			}
		}
		FSUnlockMutex();
	}

	void __FSQueueDefaultFinishFunc(FSCmdBlockBody_t* fsCmdBlockBody, FS_RESULT result)
	{
		switch ((FSA_CMD_OPERATION_TYPE)fsCmdBlockBody->fsaShimBuffer.operationType.value())
		{
		case FSA_CMD_OPERATION_TYPE::OPENFILE:
		{
			*fsCmdBlockBody->returnValues.cmdOpenFile.handlePtr = fsCmdBlockBody->fsaShimBuffer.response.cmdOpenFile.fileHandleOutput;
			break;
		}

		case FSA_CMD_OPERATION_TYPE::GETCWD:
		{
			auto transferSize = fsCmdBlockBody->returnValues.cmdGetCwd.transferSize;
			if (transferSize < 0 && transferSize > sizeof(fsCmdBlockBody->fsaShimBuffer.response.cmdGetCWD.path))
			{
				cemu_assert_error();
			}
			memcpy(fsCmdBlockBody->returnValues.cmdGetCwd.pathPtr, fsCmdBlockBody->fsaShimBuffer.response.cmdGetCWD.path, transferSize);
			break;
		}

		case FSA_CMD_OPERATION_TYPE::OPENDIR:
		{
			*fsCmdBlockBody->returnValues.cmdOpenDir.handlePtr = fsCmdBlockBody->fsaShimBuffer.response.cmdOpenDir.dirHandleOutput;
			break;
		}
		case FSA_CMD_OPERATION_TYPE::READDIR:
		{
			*fsCmdBlockBody->returnValues.cmdReadDir.dirEntryPtr = fsCmdBlockBody->fsaShimBuffer.response.cmdReadDir.dirEntry;
			break;
		}
		case FSA_CMD_OPERATION_TYPE::GETPOS:
		{
			*fsCmdBlockBody->returnValues.cmdGetPosFile.filePosPtr = fsCmdBlockBody->fsaShimBuffer.response.cmdGetPosFile.filePos;
			break;
		}
		case FSA_CMD_OPERATION_TYPE::GETSTATFILE:
		{
			*((FSStat_t*)fsCmdBlockBody->returnValues.cmdStatFile.resultPtr.GetPtr()) = fsCmdBlockBody->fsaShimBuffer.response.cmdStatFile.statOut;
			break;
		}
		case FSA_CMD_OPERATION_TYPE::QUERYINFO:
		{
			if (fsCmdBlockBody->fsaShimBuffer.request.cmdQueryInfo.queryType == FSA_QUERY_TYPE_FREESPACE)
			{
				*((uint64be*)fsCmdBlockBody->returnValues.cmdQueryInfo.queryResultPtr.GetPtr()) = fsCmdBlockBody->fsaShimBuffer.response.cmdQueryInfo.queryFreeSpace.freespace;
			}
			else if (fsCmdBlockBody->fsaShimBuffer.request.cmdQueryInfo.queryType == FSA_QUERY_TYPE_STAT)
			{
				*((FSStat_t*)fsCmdBlockBody->returnValues.cmdQueryInfo.queryResultPtr.GetPtr()) = fsCmdBlockBody->fsaShimBuffer.response.cmdQueryInfo.queryStat.stat;
			}
			else
			{
				cemu_assert_unimplemented();
			}
			break;
		}
		case FSA_CMD_OPERATION_TYPE::CHANGEDIR:
		case FSA_CMD_OPERATION_TYPE::MAKEDIR:
		case FSA_CMD_OPERATION_TYPE::REMOVE:
		case FSA_CMD_OPERATION_TYPE::RENAME:
		case FSA_CMD_OPERATION_TYPE::CLOSEDIR:
		case FSA_CMD_OPERATION_TYPE::READ:
		case FSA_CMD_OPERATION_TYPE::WRITE:
		case FSA_CMD_OPERATION_TYPE::SETPOS:
		case FSA_CMD_OPERATION_TYPE::ISEOF:
		case FSA_CMD_OPERATION_TYPE::CLOSEFILE:
		case FSA_CMD_OPERATION_TYPE::APPENDFILE:
		case FSA_CMD_OPERATION_TYPE::TRUNCATEFILE:
		case FSA_CMD_OPERATION_TYPE::FLUSHQUOTA:
		{
			break;
		}
		default:
		{
			cemu_assert_unimplemented();
		}
		}
	}

	void export___FSQueueDefaultFinishFunc(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamPtr(cmd, FSCmdBlockBody_t, 0);
		FS_RESULT result = (FS_RESULT)PPCInterpreter_getCallParamU32(hCPU, 1);
		__FSQueueDefaultFinishFunc(cmd, static_cast<FS_RESULT>(result));
		osLib_returnFromFunction(hCPU, 0);
	}

	void __FSQueueCmd(FSCmdQueue* cmdQueue, FSCmdBlockBody_t* fsCmdBlockBody, MPTR finishCmdFunc)
	{
		fsCmdBlockBody->cmdFinishFuncMPTR = finishCmdFunc;
		FSLockMutex();
		fsCmdBlockBody->statusCode = _swapEndianU32(FSA_CMD_STATUS_CODE_D900A22);
		__FSQueueCmdByPriority(cmdQueue, fsCmdBlockBody, true);
		FSUnlockMutex();
		__FSUpdateQueue(cmdQueue);
	}

	FSA_RESULT _FSIosErrorToFSAStatus(IOS_ERROR iosError)
	{
		return (FSA_RESULT)iosError;
	}

	FS_RESULT _FSAStatusToFSStatus(FSA_RESULT err)
	{
		if ((int)err > 0)
		{
			return (FS_RESULT)err;
		}
		switch (err)
		{
		case FSA_RESULT::OK:
		{
			return FS_RESULT::SUCCESS;
		}
		case FSA_RESULT::END_OF_DIRECTORY:
		case FSA_RESULT::END_OF_FILE:
		{
			return FS_RESULT::END_ITERATION;
		}
		case FSA_RESULT::ALREADY_EXISTS:
		{
			return FS_RESULT::ALREADY_EXISTS;
		}
		case FSA_RESULT::NOT_FOUND:
		{
			return FS_RESULT::NOT_FOUND;
		}
		case FSA_RESULT::PERMISSION_ERROR:
		{
			return FS_RESULT::PERMISSION_ERROR;
		}
		case FSA_RESULT::NOT_FILE:
		{
			return FS_RESULT::NOT_FILE;
		}
		case FSA_RESULT::NOT_DIR:
		{
			return FS_RESULT::NOT_DIR;
		}
		case FSA_RESULT::MAX_FILES:
		case FSA_RESULT::MAX_DIRS:
		{
			return FS_RESULT::MAX_HANDLES;
		}
		case FSA_RESULT::INVALID_CLIENT_HANDLE:
		case FSA_RESULT::INVALID_FILE_HANDLE:
		case FSA_RESULT::INVALID_DIR_HANDLE:
		case FSA_RESULT::INVALID_PARAM:
		case FSA_RESULT::INVALID_PATH:
		case FSA_RESULT::INVALID_BUFFER:
		case FSA_RESULT::INVALID_ALIGNMENT:
		case FSA_RESULT::NOT_INIT:
		case FSA_RESULT::MAX_CLIENTS:
		case FSA_RESULT::OUT_OF_RESOURCES:
		case FSA_RESULT::FATAL_ERROR:
		{
			return FS_RESULT::FATAL_ERROR;
		}
		}
		cemu_assert_unimplemented();
		return FS_RESULT::FATAL_ERROR;
	}

	void __FSCmdSubmitResult(FSCmdBlockBody_t* fsCmdBlockBody, FS_RESULT result)
	{
		_debugVerifyCommand("FSCmdSubmitResult", fsCmdBlockBody);

		FSClientBody_t* fsClientBody = fsCmdBlockBody->fsClientBody.GetPtr();
		OSFastMutex_Lock(&fsClientBody->fsCmdQueue.fastMutex);
		fsCmdBlockBody->cancelState &= ~(1 << 0); // clear cancel bit
		if (fsClientBody->currentCmdBlockBody.GetPtr() == fsCmdBlockBody)
			fsClientBody->currentCmdBlockBody = nullptr;
		fsCmdBlockBody->statusCode = _swapEndianU32(FSA_CMD_STATUS_CODE_D900A24);
		OSFastMutex_Unlock(&fsClientBody->fsCmdQueue.fastMutex);
		// send result via msg queue or callback
		cemu_assert_debug(!fsCmdBlockBody->asyncResult.fsAsyncParamsNew.ioMsgQueue != !fsCmdBlockBody->asyncResult.fsAsyncParamsNew.userCallback); // either must be set
		fsCmdBlockBody->ukn09EA = 0;
		fsCmdBlockBody->ukn9F4_lastErrorRelated = 0;
		if (fsCmdBlockBody->asyncResult.fsAsyncParamsNew.ioMsgQueue)
		{
			// msg queue
			_debugVerifyCommand("SubmitResultQueue", fsCmdBlockBody);
			coreinit::OSMessageQueue* ioMsgQueue = fsCmdBlockBody->asyncResult.fsAsyncParamsNew.ioMsgQueue.GetPtr();
			fsCmdBlockBody->asyncResult.fsCmdBlock = fsCmdBlockBody->selfCmdBlock;
			fsCmdBlockBody->asyncResult.fsStatusNew = (uint32)result;
			while (OSSendMessage(ioMsgQueue, &fsCmdBlockBody->asyncResult.msgUnion.osMsg, 0) == 0)
			{
				cemuLog_log(LogType::Force, "FS driver: Failed to add message to result queue. Retrying...");
				if (PPCInterpreter_getCurrentInstance())
					PPCCore_switchToScheduler();
				else
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		}
		else
		{
			// callback
			_debugVerifyCommand("SubmitResultCallback", fsCmdBlockBody);
			FSClient_t* fsClient = fsCmdBlockBody->asyncResult.fsClient.GetPtr();
			FSCmdBlock_t* fsCmdBlock = fsCmdBlockBody->asyncResult.fsCmdBlock.GetPtr();
			PPCCoreCallback(fsCmdBlockBody->asyncResult.fsAsyncParamsNew.userCallback, fsClient, fsCmdBlock, (sint32)result, fsCmdBlockBody->asyncResult.fsAsyncParamsNew.userContext);
		}
	}

	void __FSAIoctlResponseCallback(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(iosResult, 0);
		ppcDefineParamPtr(cmd, FSCmdBlockBody_t, 1);

		FSA_RESULT fsaStatus = _FSIosErrorToFSAStatus((IOS_ERROR)iosResult);

		cmd->statusCode = _swapEndianU32(FSA_CMD_STATUS_CODE_D900A26);
		cmd->lastFSAStatus = fsaStatus;

		// translate error code to FSStatus
		FS_RESULT fsStatus = _FSAStatusToFSStatus(fsaStatus);

		// On actual hardware this delegates the processing to the AppIO threads, but for now we just run it directly from the IPC thread
		FSClientBody_t* client = cmd->fsClientBody;
		FSCmdQueue& cmdQueue = client->fsCmdQueue;
		FSLockMutex();
		cmdQueue.numCommandsInFlight -= 1;
		cmdQueue.queueFlags = cmdQueue.queueFlags & ~FSCmdQueue::QUEUE_FLAG::IS_FULL;
		FSUnlockMutex();

		if (cmd->cmdFinishFuncMPTR)
		{
			if (cmd->cmdFinishFuncMPTR != FS_CB_PLACEHOLDER_FINISHCMD)
				PPCCoreCallback(MEMPTR<void>(cmd->cmdFinishFuncMPTR), cmd, fsStatus);
		}

		__FSCmdSubmitResult(cmd, fsStatus);
		__FSUpdateQueue(&cmd->fsClientBody->fsCmdQueue);
		osLib_returnFromFunction(hCPU, 0);
	}

	/* FS commands */

	void FSInitCmdBlock(FSCmdBlock_t* fsCmdBlock)
	{
		memset(fsCmdBlock, 0x00, sizeof(FSCmdBlock_t));
		FSCmdBlockBody_t* fsCmdBlockBody = __FSGetCmdBlockBody(fsCmdBlock);
		fsCmdBlockBody->statusCode = _swapEndianU32(FSA_CMD_STATUS_CODE_D900A21);
		fsCmdBlockBody->priority = 0x10;
	}

	void __FSAsyncToSyncInit(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, FSAsyncParamsNew_t* asyncParams)
	{
		if (fsClient == nullptr || fsCmdBlock == nullptr || asyncParams == nullptr)
			assert_dbg();
		FSCmdBlockBody_t* fsCmdBlockBody = __FSGetCmdBlockBody(fsCmdBlock);
		coreinit::OSInitMessageQueue(&fsCmdBlockBody->syncTaskMsgQueue, fsCmdBlockBody->_syncTaskMsg, 1);
		asyncParams->userCallback = nullptr;
		asyncParams->userContext = nullptr;
		asyncParams->ioMsgQueue = &fsCmdBlockBody->syncTaskMsgQueue;
	}

	void __FSPrepareCmdAsyncResult(FSClientBody_t* fsClientBody, FSCmdBlockBody_t* fsCmdBlockBody, FSAsyncResult* fsCmdBlockAsyncResult, FSAsyncParamsNew_t* fsAsyncParams)
	{
		memcpy(&fsCmdBlockAsyncResult->fsAsyncParamsNew, fsAsyncParams, sizeof(FSAsyncParamsNew_t));

		fsCmdBlockAsyncResult->fsClient = fsClientBody->selfClient;
		fsCmdBlockAsyncResult->fsCmdBlock = fsCmdBlockBody->selfCmdBlock;

		fsCmdBlockAsyncResult->msgUnion.fsMsg.fsAsyncResult = fsCmdBlockAsyncResult;
		fsCmdBlockAsyncResult->msgUnion.fsMsg.commandType = _swapEndianU32(8);
	}

	sint32 __FSPrepareCmd(FSClientBody_t* fsClientBody, FSCmdBlockBody_t* fsCmdBlockBody, uint32 errHandling, FSAsyncParamsNew_t* fsAsyncParams)
	{
		if (sFSInitialized == false || sFSShutdown == true)
			return -0x400;
		if (!fsClientBody || !fsCmdBlockBody)
		{
			cemu_assert(false);
			return -0x400;
		}
		if (_swapEndianU32(fsCmdBlockBody->statusCode) != FSA_CMD_STATUS_CODE_D900A21 && _swapEndianU32(fsCmdBlockBody->statusCode) != FSA_CMD_STATUS_CODE_D900A24)
		{
			cemu_assert(false);
			return -0x400;
		}
		if ((fsAsyncParams->userCallback == nullptr && fsAsyncParams->ioMsgQueue == nullptr) ||
			(fsAsyncParams->userCallback != nullptr && fsAsyncParams->ioMsgQueue != nullptr))
		{
			// userCallback and ioMsgQueue both set or none set
			cemu_assert(false);
			return -0x400;
		}
		fsCmdBlockBody->fsClientBody = fsClientBody;
		fsCmdBlockBody->errHandling = _swapEndianU32(errHandling);
		fsCmdBlockBody->uknStatusGuessed09E9 = 0;
		fsCmdBlockBody->cancelState &= ~(1 << 0); // clear cancel bit
		fsCmdBlockBody->fsaShimBuffer.fsaDevHandle = fsClientBody->iosuFSAHandle;
		__FSPrepareCmdAsyncResult(fsClientBody, fsCmdBlockBody, &fsCmdBlockBody->asyncResult, fsAsyncParams);
		return 0;
	}

#define _FSCmdIntro()                                                                        \
	FSClientBody_t* fsClientBody = __FSGetClientBody(fsClient);                              \
	FSCmdBlockBody_t* fsCmdBlockBody = __FSGetCmdBlockBody(fsCmdBlock);                      \
	sint32 fsError = __FSPrepareCmd(fsClientBody, fsCmdBlockBody, errorMask, fsAsyncParams); \
	if (fsError != 0)                                                                        \
		return fsError;

	void _debugVerifyCommand(const char* stage, FSCmdBlockBody_t* fsCmdBlockBody)
	{
		if (fsCmdBlockBody->asyncResult.msgUnion.fsMsg.commandType != _swapEndianU32(8))
		{
			cemuLog_log(LogType::Force, "Corrupted FS command detected in stage {}", stage);
			cemuLog_log(LogType::Force, "Printing CMD block: ");
			for (uint32 i = 0; i < (sizeof(FSCmdBlockBody_t) + 31) / 32; i++)
			{
				uint8* p = ((uint8*)fsCmdBlockBody) + i * 32;
				cemuLog_log(LogType::Force, "{:04x}: {:02x} {:02x} {:02x} {:02x} - {:02x} {:02x} {:02x} {:02x} - {:02x} {:02x} {:02x} {:02x} - {:02x} {:02x} {:02x} {:02x} | {:02x} {:02x} {:02x} {:02x} - {:02x} {:02x} {:02x} {:02x} - {:02x} {:02x} {:02x} {:02x} - {:02x} {:02x} {:02x} {:02x}",
							i * 32,
							p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15],
							p[16], p[17], p[18], p[19], p[20], p[21], p[22], p[23], p[24], p[25], p[26], p[27], p[28], p[29], p[30], p[31]);
			}
		}
	}

	FSAsyncResult* FSGetAsyncResult(OSMessage* msg)
	{
		return (FSAsyncResult*)memory_getPointerFromVirtualOffset(_swapEndianU32(msg->message));
	}

	sint32 __FSProcessAsyncResult(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, sint32 fsStatus, uint32 errHandling)
	{
		// a positive result (or zero) means success. Most operations return zero in case of success. Read and write operations return the number of transferred units
		if (fsStatus >= 0)
		{
			FSCmdBlockBody_t* fsCmdBlockBody = __FSGetCmdBlockBody(fsCmdBlock);
			OSMessage msg;
			OSReceiveMessage(&fsCmdBlockBody->syncTaskMsgQueue, &msg, OS_MESSAGE_BLOCK);
			_debugVerifyCommand("handleAsyncResult", fsCmdBlockBody);
			FSAsyncResult* asyncResult = FSGetAsyncResult(&msg);
			return asyncResult->fsStatusNew;
		}
		else
		{
			// todo - error handling
			cemuLog_log(LogType::Force, "FS handleAsyncResult(): unexpected error {:08x}", errHandling);
			cemu_assert_debug(false);
			return 0;
		}
	}

	FSA_RESULT __FSPrepareCmd_OpenFile(iosu::fsa::FSAShimBuffer* fsaShimBuffer, IOSDevHandle fsaHandle, char* path, char* mode, uint32 createMode, uint32 openFlags, uint32 preallocSize)
	{
		if (fsaShimBuffer == NULL)
			return FSA_RESULT::INVALID_BUFFER;
		if (path == NULL)
			return FSA_RESULT::INVALID_PATH;
		if (mode == NULL)
			return FSA_RESULT::INVALID_PARAM;
		fsaShimBuffer->fsaDevHandle = fsaHandle;
		fsaShimBuffer->ipcReqType = 0;
		fsaShimBuffer->operationType = (uint32)FSA_CMD_OPERATION_TYPE::OPENFILE;
		// path
		size_t pathLen = strlen((char*)path);
		if (pathLen >= FSA_CMD_PATH_MAX_LENGTH)
		{
			cemu_assert_debug(false);
			pathLen = FSA_CMD_PATH_MAX_LENGTH - 1;
		}
		for (sint32 i = 0; i < pathLen; i++)
			fsaShimBuffer->request.cmdOpenFile.path[i] = path[i];
		for (size_t i = pathLen; i < FSA_CMD_PATH_MAX_LENGTH; i++)
			fsaShimBuffer->request.cmdOpenFile.path[i] = '\0';
		// mode
		size_t modeLen = strlen((char*)mode);
		if (modeLen >= 12)
		{
			cemu_assert_debug(false);
			modeLen = 12 - 1;
		}
		for (sint32 i = 0; i < modeLen; i++)
			fsaShimBuffer->request.cmdOpenFile.mode[i] = mode[i];
		for (size_t i = modeLen; i < 12; i++)
			fsaShimBuffer->request.cmdOpenFile.mode[i] = '\0';
		// createMode
		fsaShimBuffer->request.cmdOpenFile.createMode = createMode;
		// openFlags
		fsaShimBuffer->request.cmdOpenFile.openFlags = openFlags;
		// preallocSize
		fsaShimBuffer->request.cmdOpenFile.preallocSize = preallocSize;

		fsaShimBuffer->response.cmdOpenFile.fileHandleOutput = 0xFFFFFFFF;

		return FSA_RESULT::OK;
	}

	sint32 FSOpenFileAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, char* mode, FSFileHandleDepr_t* outFileHandle, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		_FSCmdIntro();
		if (outFileHandle == nullptr || path == nullptr || mode == nullptr)
			return -0x400;
		fsCmdBlockBody->returnValues.cmdOpenFile.handlePtr = &outFileHandle->fileHandle;
		fsError = (FSStatus)__FSPrepareCmd_OpenFile(&fsCmdBlockBody->fsaShimBuffer, fsClientBody->iosuFSAHandle, path, mode, 0x660, 0, 0);
		if (fsError != (FSStatus)FS_RESULT::SUCCESS)
			return fsError;
		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, RPLLoader_MakePPCCallable(export___FSQueueDefaultFinishFunc));
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSOpenFile(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, char* mode, FSFileHandleDepr_t* fileHandle, uint32 errHandling)
	{
		StackAllocator<FSAsyncParamsNew_t, 1> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSOpenFileAsync(fsClient, fsCmdBlock, path, mode, fileHandle, errHandling, asyncParams);
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errHandling);
	}

	sint32 FSOpenFileExAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, char* mode, uint32 createMode, uint32 openFlag, uint32 preallocSize, FSFileHandleDepr_t* outFileHandle, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		if (openFlag != 0)
		{
			cemuLog_log(LogType::Force, "FSOpenFileEx called with unsupported flags!");
			cemu_assert_debug(false);
		}

		_FSCmdIntro();
		if (outFileHandle == nullptr || path == nullptr || mode == nullptr)
			return -0x400;
		fsCmdBlockBody->returnValues.cmdOpenFile.handlePtr = &outFileHandle->fileHandle;

		FSA_RESULT prepareResult = __FSPrepareCmd_OpenFile(&fsCmdBlockBody->fsaShimBuffer, fsClientBody->iosuFSAHandle, path, mode, createMode, openFlag, preallocSize);
		if (prepareResult != FSA_RESULT::OK)
			return (FSStatus)_FSAStatusToFSStatus(prepareResult);

		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, RPLLoader_MakePPCCallable(export___FSQueueDefaultFinishFunc));
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSOpenFileEx(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, char* mode, uint32 createMode, uint32 openFlag, uint32 preallocSize, FSFileHandleDepr_t* fileHandle, uint32 errHandling)
	{
		StackAllocator<FSAsyncParamsNew_t, 1> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSOpenFileExAsync(fsClient, fsCmdBlock, path, mode, createMode, openFlag, preallocSize, fileHandle, errHandling, asyncParams);
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errHandling);
	}

	FSA_RESULT __FSPrepareCmd_CloseFile(iosu::fsa::FSAShimBuffer* fsaShimBuffer, IOSDevHandle fsaHandle, uint32 fileHandle)
	{
		if (fsaShimBuffer == nullptr)
			return FSA_RESULT::INVALID_BUFFER;

		fsaShimBuffer->fsaDevHandle = fsaHandle;
		fsaShimBuffer->ipcReqType = 0;
		fsaShimBuffer->operationType = (uint32)FSA_CMD_OPERATION_TYPE::CLOSEFILE;

		fsaShimBuffer->request.cmdCloseFile.fileHandle = fileHandle;

		return FSA_RESULT::OK;
	}

	sint32 FSCloseFileAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		_FSCmdIntro();

		FSA_RESULT prepareResult = __FSPrepareCmd_CloseFile(&fsCmdBlockBody->fsaShimBuffer, fsClientBody->iosuFSAHandle, fileHandle);
		if (prepareResult != FSA_RESULT::OK)
			return (FSStatus)_FSAStatusToFSStatus(prepareResult);

		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, RPLLoader_MakePPCCallable(export___FSQueueDefaultFinishFunc));
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSCloseFile(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32 errHandling)
	{
		StackAllocator<FSAsyncParamsNew_t, 1> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSCloseFileAsync(fsClient, fsCmdBlock, fileHandle, errHandling, asyncParams);
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errHandling);
	}

	FSA_RESULT __FSPrepareCmd_FlushFile(iosu::fsa::FSAShimBuffer* fsaShimBuffer, IOSDevHandle fsaHandle, uint32 fileHandle)
	{
		if (fsaShimBuffer == nullptr)
			return FSA_RESULT::INVALID_BUFFER;

		fsaShimBuffer->fsaDevHandle = fsaHandle;
		fsaShimBuffer->ipcReqType = 0;
		fsaShimBuffer->operationType = (uint32)FSA_CMD_OPERATION_TYPE::FLUSHFILE;

		fsaShimBuffer->request.cmdFlushFile.fileHandle = fileHandle;

		return FSA_RESULT::OK;
	}

	sint32 FSFlushFileAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		_FSCmdIntro();

		FSA_RESULT prepareResult = __FSPrepareCmd_FlushFile(&fsCmdBlockBody->fsaShimBuffer, fsClientBody->iosuFSAHandle, fileHandle);
		if (prepareResult != FSA_RESULT::OK)
			return (FSStatus)_FSAStatusToFSStatus(prepareResult);

		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, RPLLoader_MakePPCCallable(export___FSQueueDefaultFinishFunc));
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSFlushFile(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32 errHandling)
	{
		StackAllocator<FSAsyncParamsNew_t, 1> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSFlushFileAsync(fsClient, fsCmdBlock, fileHandle, errHandling, asyncParams);
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errHandling);
	}

	FSA_RESULT __FSPrepareCmd_ReadFile(iosu::fsa::FSAShimBuffer* fsaShimBuffer, IOSDevHandle fsaHandle, void* dest, uint32 size, uint32 count, uint32 filePos, uint32 fileHandle, uint32 flag)
	{
		if (fsaShimBuffer == NULL || dest == NULL)
			return FSA_RESULT::INVALID_BUFFER;
		MPTR destMPTR = memory_getVirtualOffsetFromPointer(dest);
		if ((destMPTR & 0x3F) != 0)
			return FSA_RESULT::INVALID_ALIGNMENT;

		fsaShimBuffer->fsaDevHandle = fsaHandle;
		fsaShimBuffer->ipcReqType = 1;
		fsaShimBuffer->operationType = (uint32)FSA_CMD_OPERATION_TYPE::READ;

		fsaShimBuffer->ioctlvVecIn = 1;
		fsaShimBuffer->ioctlvVecOut = 2;

		fsaShimBuffer->ioctlvVec[0].baseVirt = fsaShimBuffer;
		fsaShimBuffer->ioctlvVec[0].size = sizeof(iosu::fsa::FSARequest);

		fsaShimBuffer->ioctlvVec[1].baseVirt = destMPTR;
		fsaShimBuffer->ioctlvVec[1].size = size * count;

		fsaShimBuffer->ioctlvVec[2].baseVirt = &fsaShimBuffer->response;
		fsaShimBuffer->ioctlvVec[2].size = sizeof(iosu::fsa::FSAResponse);

		fsaShimBuffer->request.cmdReadFile.dest = dest;
		fsaShimBuffer->request.cmdReadFile.size = size;
		fsaShimBuffer->request.cmdReadFile.count = count;
		fsaShimBuffer->request.cmdReadFile.filePos = filePos;
		fsaShimBuffer->request.cmdReadFile.fileHandle = fileHandle;
		fsaShimBuffer->request.cmdReadFile.flag = flag;

		return FSA_RESULT::OK;
	}

	SysAllocator<uint8, 128, 64> _tempFSSpace;

	sint32 __FSReadFileAsyncEx(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, void* dest, uint32 size, uint32 count, bool usePos, uint32 filePos, uint32 fileHandle, uint32 flag, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		_FSCmdIntro();
		if (size == 0 || count == 0 || dest == NULL)
			dest = _tempFSSpace.GetPtr();
		sint64 transferSizeS64 = (sint64)size * (sint64)count;
		if (transferSizeS64 < 0 || (transferSizeS64 >= 2LL * 1024LL * 1024LL * 1024LL))
		{
			// size below 0 or over 2GB
			cemu_assert(false);
			return -0x400;
		}

		// coreinit.rpl splits up each read into smaller chunks (probably to support canceling big writes). This is handled by a specific
		// callback for the __FSQueueCmd functions. Whenever a chunk is read, it's getting re-queued until the reading has been completed.
		// For this it writes values into the fsCmdBlockBody->returnValues struct. At the moment we go the lazy route of just reading everything
		// at once, so we can skip the initialization of these values.

		if (usePos)
			flag |= FSA_CMD_FLAG_SET_POS;
		else
			flag &= ~FSA_CMD_FLAG_SET_POS;

		FSA_RESULT prepareResult = __FSPrepareCmd_ReadFile(&fsCmdBlockBody->fsaShimBuffer, fsClientBody->iosuFSAHandle, dest, size, count, filePos, fileHandle, flag);
		if (prepareResult != FSA_RESULT::OK)
			return (FSStatus)_FSAStatusToFSStatus(prepareResult);

		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, RPLLoader_MakePPCCallable(export___FSQueueDefaultFinishFunc));
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSReadFileAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, void* dst, uint32 size, uint32 count, uint32 fileHandle, uint32 flag, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		cemu_assert_debug(flag == 0); // todo
		return __FSReadFileAsyncEx(fsClient, fsCmdBlock, dst, size, count, false, 0, fileHandle, flag, errorMask, fsAsyncParams);
	}

	sint32 FSReadFile(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, void* dst, uint32 size, uint32 count, uint32 fileHandle, uint32 flag, uint32 errorMask)
	{
		StackAllocator<FSAsyncParamsNew_t, 1> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSReadFileAsync(fsClient, fsCmdBlock, dst, size, count, fileHandle, flag, errorMask, asyncParams.GetPointer());
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
	}

	sint32 FSReadFileWithPosAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, void* dst, uint32 size, uint32 count, uint32 filePos, uint32 fileHandle, uint32 flag, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		cemu_assert_debug(flag == 0); // todo
		sint32 fsStatus = __FSReadFileAsyncEx(fsClient, fsCmdBlock, dst, size, count, true, filePos, fileHandle, flag, errorMask, fsAsyncParams);
		return fsStatus;
	}

	sint32 FSReadFileWithPos(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, void* dst, uint32 size, uint32 count, uint32 filePos, uint32 fileHandle, uint32 flag, uint32 errorMask)
	{
		StackAllocator<FSAsyncParamsNew_t, 1> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSReadFileWithPosAsync(fsClient, fsCmdBlock, dst, size, count, filePos, fileHandle, flag, errorMask, asyncParams.GetPointer());
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
	}

	FSA_RESULT __FSPrepareCmd_WriteFile(iosu::fsa::FSAShimBuffer* fsaShimBuffer, IOSDevHandle fsaHandle, void* dest, uint32 size, uint32 count, uint32 filePos, uint32 fileHandle, uint32 flag)
	{
		if (fsaShimBuffer == NULL || dest == NULL)
			return FSA_RESULT::INVALID_BUFFER;
		MPTR destMPTR = memory_getVirtualOffsetFromPointer(dest);
		if ((destMPTR & 0x3F) != 0)
			return FSA_RESULT::INVALID_ALIGNMENT;

		fsaShimBuffer->fsaDevHandle = fsaHandle;
		fsaShimBuffer->ipcReqType = 1;
		fsaShimBuffer->operationType = (uint32)FSA_CMD_OPERATION_TYPE::WRITE;

		fsaShimBuffer->ioctlvVecIn = 2;
		fsaShimBuffer->ioctlvVecOut = 1;

		fsaShimBuffer->ioctlvVec[0].baseVirt = fsaShimBuffer;
		fsaShimBuffer->ioctlvVec[0].size = sizeof(iosu::fsa::FSARequest);

		fsaShimBuffer->ioctlvVec[1].baseVirt = destMPTR;
		fsaShimBuffer->ioctlvVec[1].size = size * count;

		fsaShimBuffer->ioctlvVec[2].baseVirt = &fsaShimBuffer->response;
		fsaShimBuffer->ioctlvVec[2].size = sizeof(iosu::fsa::FSAResponse);

		fsaShimBuffer->request.cmdWriteFile.dest = dest;
		fsaShimBuffer->request.cmdWriteFile.size = size;
		fsaShimBuffer->request.cmdWriteFile.count = count;
		fsaShimBuffer->request.cmdWriteFile.filePos = filePos;
		fsaShimBuffer->request.cmdWriteFile.fileHandle = fileHandle;
		fsaShimBuffer->request.cmdWriteFile.flag = flag;

		return FSA_RESULT::OK;
	}

	sint32 __FSWriteFileWithPosAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, void* dest, uint32 size, uint32 count, bool useFilePos, uint32 filePos, uint32 fileHandle, uint32 flag, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		_FSCmdIntro();
		if (size == 0 || count == 0 || dest == nullptr)
			dest = _tempFSSpace.GetPtr();
		sint64 transferSizeS64 = (sint64)size * (sint64)count;
		if (transferSizeS64 < 0 || (transferSizeS64 >= 2LL * 1024LL * 1024LL * 1024LL))
		{
			// size below 0 or over 2GB
			cemu_assert(false);
			return -0x400;
		}

		// coreinit.rpl splits up each write into smaller chunks (probably to support canceling big writes). This is handled by a specific
		// callback for the __FSQueueCmd functions. Whenever a chunk is written, it's getting re-queued until the writing has been completed.
		// For this it writes values into the fsCmdBlockBody->returnValues struct. At the moment we go the lazy route of just writing everything
		// at once, so we can skip the initialization of these values.

		if (useFilePos)
			flag |= FSA_CMD_FLAG_SET_POS;
		else
			flag &= ~FSA_CMD_FLAG_SET_POS;

		FSA_RESULT prepareResult = __FSPrepareCmd_WriteFile(&fsCmdBlockBody->fsaShimBuffer, fsClientBody->iosuFSAHandle, dest, size, count, filePos, fileHandle, flag);
		if (prepareResult != FSA_RESULT::OK)
			return (FSStatus)_FSAStatusToFSStatus(prepareResult);

		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, RPLLoader_MakePPCCallable(export___FSQueueDefaultFinishFunc));
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSWriteFileAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, void* src, uint32 size, uint32 count, uint32 fileHandle, uint32 flag, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		return __FSWriteFileWithPosAsync(fsClient, fsCmdBlock, src, size, count, false, 0, fileHandle, flag, errorMask, fsAsyncParams);
	}

	sint32 FSWriteFile(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, void* src, uint32 size, uint32 count, uint32 fileHandle, uint32 flag, uint32 errorMask)
	{
		StackAllocator<FSAsyncParamsNew_t, 1> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSWriteFileAsync(fsClient, fsCmdBlock, src, size, count, fileHandle, flag, errorMask, asyncParams.GetPointer());
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
	}

	sint32 FSWriteFileWithPosAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, void* src, uint32 size, uint32 count, uint32 filePos, uint32 fileHandle, uint32 flag, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		return __FSWriteFileWithPosAsync(fsClient, fsCmdBlock, src, size, count, true, filePos, fileHandle, flag, errorMask, fsAsyncParams);
	}

	sint32 FSWriteFileWithPos(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, void* src, uint32 size, uint32 count, uint32 filePos, uint32 fileHandle, uint32 flag, uint32 errorMask)
	{
		StackAllocator<FSAsyncParamsNew_t, 1> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSWriteFileWithPosAsync(fsClient, fsCmdBlock, src, size, count, filePos, fileHandle, flag, errorMask, asyncParams.GetPointer());
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
	}

	FSA_RESULT __FSPrepareCmd_SetPosFile(iosu::fsa::FSAShimBuffer* fsaShimBuffer, IOSDevHandle fsaHandle, uint32 fileHandle, uint32 filePos)
	{
		if (fsaShimBuffer == NULL)
			return FSA_RESULT::INVALID_BUFFER;

		fsaShimBuffer->fsaDevHandle = fsaHandle;
		fsaShimBuffer->ipcReqType = 0;
		fsaShimBuffer->request.cmdSetPosFile.fileHandle = fileHandle;
		fsaShimBuffer->request.cmdSetPosFile.filePos = filePos;
		fsaShimBuffer->operationType = (uint32)FSA_CMD_OPERATION_TYPE::SETPOS;
		return FSA_RESULT::OK;
	}

	sint32 FSSetPosFileAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32 filePos, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		_FSCmdIntro();
		FSA_RESULT prepareResult = __FSPrepareCmd_SetPosFile(&fsCmdBlockBody->fsaShimBuffer, fsClientBody->iosuFSAHandle, fileHandle, filePos);
		if (prepareResult != FSA_RESULT::OK)
			return (FSStatus)_FSAStatusToFSStatus(prepareResult);
		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, RPLLoader_MakePPCCallable(export___FSQueueDefaultFinishFunc));
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSSetPosFile(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32 filePos, uint32 errorMask)
	{
		// used by games: Mario Kart 8
		StackAllocator<FSAsyncParamsNew_t, 1> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSSetPosFileAsync(fsClient, fsCmdBlock, fileHandle, filePos, errorMask, asyncParams.GetPointer());
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
	}

	FSA_RESULT __FSPrepareCmd_GetPosFile(iosu::fsa::FSAShimBuffer* fsaShimBuffer, IOSDevHandle fsaHandle, uint32 fileHandle)
	{
		if (fsaShimBuffer == NULL)
			return FSA_RESULT::INVALID_BUFFER;
		fsaShimBuffer->fsaDevHandle = fsaHandle;
		fsaShimBuffer->ipcReqType = 0;
		fsaShimBuffer->request.cmdGetPosFile.fileHandle = fileHandle;
		fsaShimBuffer->operationType = (uint32)FSA_CMD_OPERATION_TYPE::GETPOS;
		return FSA_RESULT::OK;
	}

	sint32 FSGetPosFileAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32be* returnedFilePos, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		// games using this: Darksiders Warmastered Edition
		_FSCmdIntro();
		fsCmdBlockBody->returnValues.cmdGetPosFile.filePosPtr = returnedFilePos;
		FSA_RESULT prepareResult = __FSPrepareCmd_GetPosFile(&fsCmdBlockBody->fsaShimBuffer, fsClientBody->iosuFSAHandle, fileHandle);
		if (prepareResult != FSA_RESULT::OK)
			return (FSStatus)_FSAStatusToFSStatus(prepareResult);
		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, RPLLoader_MakePPCCallable(export___FSQueueDefaultFinishFunc));
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSGetPosFile(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32be* returnedFilePos, uint32 errorMask)
	{
		StackAllocator<FSAsyncParamsNew_t, 1> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSGetPosFileAsync(fsClient, fsCmdBlock, fileHandle, returnedFilePos, errorMask, asyncParams.GetPointer());
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
	}

	FSA_RESULT __FSPrepareCmd_OpenDir(iosu::fsa::FSAShimBuffer* fsaShimBuffer, IOSDevHandle fsaHandle, char* path)
	{
		if (fsaShimBuffer == nullptr)
			return FSA_RESULT::INVALID_BUFFER;
		if (path == nullptr)
			return FSA_RESULT::INVALID_PATH;

		fsaShimBuffer->fsaDevHandle = fsaHandle;
		fsaShimBuffer->ipcReqType = 0;
		fsaShimBuffer->operationType = (uint32)FSA_CMD_OPERATION_TYPE::OPENDIR;

		// path
		sint32 pathLen = (sint32)strlen((char*)path);
		if (pathLen >= FSA_CMD_PATH_MAX_LENGTH)
		{
			cemu_assert_debug(false);
			pathLen = FSA_CMD_PATH_MAX_LENGTH - 1;
		}
		for (sint32 i = 0; i < pathLen; i++)
			fsaShimBuffer->request.cmdOpenDir.path[i] = path[i];
		for (sint32 i = pathLen; i < FSA_CMD_PATH_MAX_LENGTH; i++)
			fsaShimBuffer->request.cmdOpenDir.path[i] = '\0';

		fsaShimBuffer->response.cmdOpenDir.dirHandleOutput = -1;

		return FSA_RESULT::OK;
	}

	sint32 FSOpenDirAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, FSDirHandlePtr dirHandleOut, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		_FSCmdIntro();
		cemu_assert(dirHandleOut && path);
		fsCmdBlockBody->returnValues.cmdOpenDir.handlePtr = dirHandleOut.GetMPTR();
		FSA_RESULT prepareResult = __FSPrepareCmd_OpenDir(&fsCmdBlockBody->fsaShimBuffer, fsClientBody->iosuFSAHandle, path);
		if (prepareResult != FSA_RESULT::OK)
			return (FSStatus)_FSAStatusToFSStatus(prepareResult);
		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, RPLLoader_MakePPCCallable(export___FSQueueDefaultFinishFunc));
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSOpenDir(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, FSDirHandlePtr dirHandleOut, uint32 errorMask)
	{
		StackAllocator<FSAsyncParamsNew_t, 1> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSOpenDirAsync(fsClient, fsCmdBlock, path, dirHandleOut, errorMask, asyncParams);
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
	}

	FSA_RESULT __FSPrepareCmd_ReadDir(iosu::fsa::FSAShimBuffer* fsaShimBuffer, IOSDevHandle fsaHandle, FSDirHandle2 dirHandle)
	{
		if (fsaShimBuffer == nullptr)
			return FSA_RESULT::INVALID_BUFFER;
		fsaShimBuffer->fsaDevHandle = fsaHandle;
		fsaShimBuffer->ipcReqType = 0;
		fsaShimBuffer->operationType = (uint32)FSA_CMD_OPERATION_TYPE::READDIR;
		fsaShimBuffer->request.cmdReadDir.dirHandle = dirHandle;
		return FSA_RESULT::OK;
	}

	sint32 FSReadDirAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, FSDirHandle2 dirHandle, FSDirEntry_t* dirEntryOut, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		_FSCmdIntro();
		FSA_RESULT prepareResult = __FSPrepareCmd_ReadDir(&fsCmdBlockBody->fsaShimBuffer, fsClientBody->iosuFSAHandle, dirHandle);
		if (prepareResult != FSA_RESULT::OK)
			return (FSStatus)_FSAStatusToFSStatus(prepareResult);
		fsCmdBlockBody->returnValues.cmdReadDir.dirEntryPtr = dirEntryOut;
		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, RPLLoader_MakePPCCallable(export___FSQueueDefaultFinishFunc));
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSReadDir(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, FSDirHandle2 dirHandle, FSDirEntry_t* dirEntryOut, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		StackAllocator<FSAsyncParamsNew_t, 1> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSReadDirAsync(fsClient, fsCmdBlock, dirHandle, dirEntryOut, errorMask, asyncParams);
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
	}

	FSA_RESULT __FSPrepareCmd_CloseDir(iosu::fsa::FSAShimBuffer* fsaShimBuffer, IOSDevHandle fsaHandle, FSDirHandle2 dirHandle)
	{
		if (fsaShimBuffer == nullptr)
			return FSA_RESULT::INVALID_BUFFER;
		fsaShimBuffer->fsaDevHandle = fsaHandle;
		fsaShimBuffer->ipcReqType = 0;
		fsaShimBuffer->operationType = (uint32)FSA_CMD_OPERATION_TYPE::CLOSEDIR;
		fsaShimBuffer->request.cmdCloseDir.dirHandle = dirHandle;
		return FSA_RESULT::OK;
	}

	sint32 FSCloseDirAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, FSDirHandle2 dirHandle, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		_FSCmdIntro();
		FSA_RESULT prepareResult = __FSPrepareCmd_CloseDir(&fsCmdBlockBody->fsaShimBuffer, fsClientBody->iosuFSAHandle, dirHandle);
		if (prepareResult != FSA_RESULT::OK)
			return (FSStatus)_FSAStatusToFSStatus(prepareResult);

		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, RPLLoader_MakePPCCallable(export___FSQueueDefaultFinishFunc));
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSCloseDir(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, FSDirHandle2 dirHandle, uint32 errorMask)
	{
		StackAllocator<FSAsyncParamsNew_t, 1> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSCloseDirAsync(fsClient, fsCmdBlock, dirHandle, errorMask, asyncParams);
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
	}

	FSA_RESULT __FSPrepareCmd_RewindDir(iosu::fsa::FSAShimBuffer* fsaShimBuffer, IOSDevHandle fsaHandle, FSDirHandle2 dirHandle)
	{
		if (fsaShimBuffer == nullptr)
			return FSA_RESULT::INVALID_BUFFER;

		fsaShimBuffer->fsaDevHandle = fsaHandle;
		fsaShimBuffer->ipcReqType = 0;
		fsaShimBuffer->operationType = (uint32)FSA_CMD_OPERATION_TYPE::REWINDDIR;

		fsaShimBuffer->request.cmdRewindDir.dirHandle = dirHandle;

		return FSA_RESULT::OK;
	}

	sint32 FSRewindDirAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, FSDirHandle2 dirHandle, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		_FSCmdIntro();
		FSA_RESULT prepareResult = __FSPrepareCmd_RewindDir(&fsCmdBlockBody->fsaShimBuffer, fsClientBody->iosuFSAHandle, dirHandle);
		if (prepareResult != FSA_RESULT::OK)
			return (FSStatus)_FSAStatusToFSStatus(prepareResult);

		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, RPLLoader_MakePPCCallable(export___FSQueueDefaultFinishFunc));
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSRewindDir(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, FSDirHandle2 dirHandle, uint32 errorMask)
	{
		StackAllocator<FSAsyncParamsNew_t, 1> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSRewindDirAsync(fsClient, fsCmdBlock, dirHandle, errorMask, asyncParams);
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
	}

	FSA_RESULT __FSPrepareCmd_AppendFile(iosu::fsa::FSAShimBuffer* fsaShimBuffer, IOSDevHandle fsaHandle, uint32 size, uint32 count, uint32 fileHandle, uint32 uknParam)
	{
		if (fsaShimBuffer == nullptr)
			return FSA_RESULT::INVALID_BUFFER;
		fsaShimBuffer->fsaDevHandle = fsaHandle;
		fsaShimBuffer->ipcReqType = 0;
		fsaShimBuffer->operationType = (uint32)FSA_CMD_OPERATION_TYPE::APPENDFILE;

		fsaShimBuffer->request.cmdAppendFile.fileHandle = fileHandle;
		fsaShimBuffer->request.cmdAppendFile.count = count;
		fsaShimBuffer->request.cmdAppendFile.size = size;
		fsaShimBuffer->request.cmdAppendFile.uknParam = uknParam;

		return FSA_RESULT::OK;
	}

	sint32 FSAppendFileAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 size, uint32 count, uint32 fileHandle, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		_FSCmdIntro();
		FSA_RESULT prepareResult = __FSPrepareCmd_AppendFile(&fsCmdBlockBody->fsaShimBuffer, fsClientBody->iosuFSAHandle, size, count, fileHandle, 0);
		if (prepareResult != FSA_RESULT::OK)
			return (FSStatus)_FSAStatusToFSStatus(prepareResult);

		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, RPLLoader_MakePPCCallable(export___FSQueueDefaultFinishFunc));
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSAppendFile(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 size, uint32 count, uint32 fileHandle, uint32 errorMask)
	{
		StackAllocator<FSAsyncParamsNew_t> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSAppendFileAsync(fsClient, fsCmdBlock, size, count, fileHandle, errorMask, asyncParams.GetPointer());
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
	}

	FSA_RESULT __FSPrepareCmd_TruncateFile(iosu::fsa::FSAShimBuffer* fsaShimBuffer, IOSDevHandle fsaHandle, FSFileHandle2 fileHandle)
	{
		if (fsaShimBuffer == nullptr)
			return FSA_RESULT::INVALID_BUFFER;
		fsaShimBuffer->fsaDevHandle = fsaHandle;
		fsaShimBuffer->ipcReqType = 0;
		fsaShimBuffer->operationType = (uint32)FSA_CMD_OPERATION_TYPE::TRUNCATEFILE;

		fsaShimBuffer->request.cmdTruncateFile.fileHandle = fileHandle;

		return FSA_RESULT::OK;
	}

	sint32 FSTruncateFileAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, FSFileHandle2 fileHandle, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		_FSCmdIntro();
		FSA_RESULT prepareResult = __FSPrepareCmd_TruncateFile(&fsCmdBlockBody->fsaShimBuffer, fsClientBody->iosuFSAHandle, fileHandle);
		if (prepareResult != FSA_RESULT::OK)
			return (FSStatus)_FSAStatusToFSStatus(prepareResult);

		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, RPLLoader_MakePPCCallable(export___FSQueueDefaultFinishFunc));
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSTruncateFile(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, FSFileHandle2 fileHandle, uint32 errorMask)
	{
		StackAllocator<FSAsyncParamsNew_t> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSTruncateFileAsync(fsClient, fsCmdBlock, fileHandle, errorMask, asyncParams.GetPointer());
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
	}

	FSA_RESULT __FSPrepareCmd_Rename(iosu::fsa::FSAShimBuffer* fsaShimBuffer, IOSDevHandle fsaHandle, char* srcPath, char* dstPath)
	{
		if (fsaShimBuffer == nullptr)
			return FSA_RESULT::INVALID_BUFFER;
		if (srcPath == NULL || dstPath == NULL)
			return FSA_RESULT::INVALID_PATH;

		fsaShimBuffer->fsaDevHandle = fsaHandle;
		fsaShimBuffer->ipcReqType = 0;
		fsaShimBuffer->operationType = (uint32)FSA_CMD_OPERATION_TYPE::RENAME;

		// source path
		size_t stringLen = strlen((char*)srcPath);
		if (stringLen >= FSA_CMD_PATH_MAX_LENGTH)
		{
			cemu_assert_debug(false);
			stringLen = FSA_CMD_PATH_MAX_LENGTH - 1;
		}
		for (sint32 i = 0; i < stringLen; i++)
		{
			fsaShimBuffer->request.cmdRename.srcPath[i] = srcPath[i];
		}
		fsaShimBuffer->request.cmdRename.srcPath[stringLen] = '\0';
		// destination path
		stringLen = strlen((char*)dstPath);
		if (stringLen >= FSA_CMD_PATH_MAX_LENGTH)
		{
			cemu_assert_debug(false);
			stringLen = FSA_CMD_PATH_MAX_LENGTH - 1;
		}
		for (sint32 i = 0; i < stringLen; i++)
		{
			fsaShimBuffer->request.cmdRename.dstPath[i] = dstPath[i];
		}
		fsaShimBuffer->request.cmdRename.dstPath[stringLen] = '\0';

		return FSA_RESULT::OK;
	}

	sint32 FSRenameAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* srcPath, char* dstPath, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		// used by titles: XCX (via SAVERenameAsync)
		_FSCmdIntro();
		if (srcPath == NULL || dstPath == NULL)
		{
			cemu_assert_debug(false); // path must not be NULL
			return -0x400;
		}
		FSA_RESULT prepareResult = __FSPrepareCmd_Rename(&fsCmdBlockBody->fsaShimBuffer, fsClientBody->iosuFSAHandle, srcPath, dstPath);
		if (prepareResult != FSA_RESULT::OK)
			return (FSStatus)_FSAStatusToFSStatus(prepareResult);

		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, RPLLoader_MakePPCCallable(export___FSQueueDefaultFinishFunc));
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSRename(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* srcPath, char* dstPath, uint32 errorMask)
	{
		StackAllocator<FSAsyncParamsNew_t> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSRenameAsync(fsClient, fsCmdBlock, srcPath, dstPath, errorMask, asyncParams.GetPointer());
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
	}

	FSA_RESULT __FSPrepareCmd_Remove(iosu::fsa::FSAShimBuffer* fsaShimBuffer, IOSDevHandle devHandle, uint8* path)
	{
		if (fsaShimBuffer == NULL)
			return FSA_RESULT::INVALID_BUFFER;
		if (path == NULL)
			return FSA_RESULT::INVALID_PATH;

		fsaShimBuffer->fsaDevHandle = devHandle;
		fsaShimBuffer->ipcReqType = 0;
		fsaShimBuffer->operationType = (uint32)FSA_CMD_OPERATION_TYPE::REMOVE;

		size_t pathLen = strlen((char*)path);
		if (pathLen >= FSA_CMD_PATH_MAX_LENGTH)
		{
			cemu_assert_debug(false);
			pathLen = FSA_CMD_PATH_MAX_LENGTH - 1;
		}
		for (sint32 i = 0; i < pathLen; i++)
		{
			fsaShimBuffer->request.cmdRemove.path[i] = path[i];
		}
		fsaShimBuffer->request.cmdRemove.path[pathLen] = '\0';

		return FSA_RESULT::OK;
	}

	sint32 FSRemoveAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint8* filePath, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		// used by titles: XCX (via SAVERemoveAsync)
		_FSCmdIntro();
		if (filePath == NULL)
		{
			cemu_assert_debug(false); // path must not be NULL
			return -0x400;
		}
		FSA_RESULT prepareResult = __FSPrepareCmd_Remove(&fsCmdBlockBody->fsaShimBuffer, fsClientBody->iosuFSAHandle, filePath);
		if (prepareResult != FSA_RESULT::OK)
			return (FSStatus)_FSAStatusToFSStatus(prepareResult);

		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, RPLLoader_MakePPCCallable(export___FSQueueDefaultFinishFunc));
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSRemove(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint8* filePath, uint32 errorMask)
	{
		StackAllocator<FSAsyncParamsNew_t> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSRemoveAsync(fsClient, fsCmdBlock, filePath, errorMask, asyncParams);
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
	}

	FSA_RESULT __FSPrepareCmd_MakeDir(iosu::fsa::FSAShimBuffer* fsaShimBuffer, IOSDevHandle devHandle, const char* path, uint32 uknVal660)
	{
		if (fsaShimBuffer == NULL)
			return FSA_RESULT::INVALID_BUFFER;
		if (path == NULL)
			return FSA_RESULT::INVALID_PATH;

		fsaShimBuffer->fsaDevHandle = devHandle;
		fsaShimBuffer->ipcReqType = 0;
		fsaShimBuffer->operationType = (uint32)FSA_CMD_OPERATION_TYPE::MAKEDIR;

		size_t pathLen = strlen((char*)path);
		if (pathLen >= FSA_CMD_PATH_MAX_LENGTH)
		{
			cemu_assert_debug(false);
			pathLen = FSA_CMD_PATH_MAX_LENGTH - 1;
		}
		for (sint32 i = 0; i < pathLen; i++)
		{
			fsaShimBuffer->request.cmdMakeDir.path[i] = path[i];
		}
		fsaShimBuffer->request.cmdMakeDir.path[pathLen] = '\0';
		fsaShimBuffer->request.cmdMakeDir.uknParam = uknVal660;

		return FSA_RESULT::OK;
	}

	sint32 FSMakeDirAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, const char* dirPath, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		// used by titles: XCX (via SAVEMakeDirAsync)
		_FSCmdIntro();
		if (dirPath == NULL)
		{
			cemu_assert_debug(false); // path must not be NULL
			return -0x400;
		}
		FSA_RESULT prepareResult = __FSPrepareCmd_MakeDir(&fsCmdBlockBody->fsaShimBuffer, fsClientBody->iosuFSAHandle, dirPath, 0x660);
		if (prepareResult != FSA_RESULT::OK)
			return (FSStatus)_FSAStatusToFSStatus(prepareResult);

		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, RPLLoader_MakePPCCallable(export___FSQueueDefaultFinishFunc));
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSMakeDir(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, const char* path, uint32 errorMask)
	{
		StackAllocator<FSAsyncParamsNew_t> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSMakeDirAsync(fsClient, fsCmdBlock, path, errorMask, asyncParams);
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
	}

	FSA_RESULT __FSPrepareCmd_ChangeDir(iosu::fsa::FSAShimBuffer* fsaShimBuffer, IOSDevHandle devHandle, uint8* path)
	{
		if (fsaShimBuffer == NULL)
			return FSA_RESULT::INVALID_BUFFER;
		if (path == NULL)
			return FSA_RESULT::INVALID_PATH;

		fsaShimBuffer->fsaDevHandle = devHandle;
		fsaShimBuffer->ipcReqType = 0;
		fsaShimBuffer->operationType = (uint32)FSA_CMD_OPERATION_TYPE::CHANGEDIR;

		size_t pathLen = strlen((char*)path);
		if (pathLen >= FSA_CMD_PATH_MAX_LENGTH)
		{
			cemu_assert_debug(false);
			pathLen = FSA_CMD_PATH_MAX_LENGTH - 1;
		}
		for (sint32 i = 0; i < pathLen; i++)
			fsaShimBuffer->request.cmdChangeDir.path[i] = path[i];

		fsaShimBuffer->request.cmdChangeDir.path[pathLen] = '\0';

		return FSA_RESULT::OK;
	}

	sint32 FSChangeDirAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		_FSCmdIntro();
		if (path == NULL)
		{
			cemu_assert_debug(false); // path must not be NULL
			return -0x400;
		}
		FSA_RESULT prepareResult = __FSPrepareCmd_ChangeDir(&fsCmdBlockBody->fsaShimBuffer, fsClientBody->iosuFSAHandle, (uint8*)path);
		if (prepareResult != FSA_RESULT::OK)
			return (FSStatus)_FSAStatusToFSStatus(prepareResult);

		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, RPLLoader_MakePPCCallable(export___FSQueueDefaultFinishFunc));
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSChangeDir(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, uint32 errorMask)
	{
		StackAllocator<FSAsyncParamsNew_t> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSChangeDirAsync(fsClient, fsCmdBlock, path, errorMask, asyncParams);
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
	}

	FSA_RESULT __FSPrepareCmd_GetCwd(iosu::fsa::FSAShimBuffer* fsaShimBuffer, IOSDevHandle devHandle)
	{
		if (fsaShimBuffer == NULL)
			return FSA_RESULT::INVALID_BUFFER;

		fsaShimBuffer->fsaDevHandle = devHandle;
		fsaShimBuffer->ipcReqType = 0;
		fsaShimBuffer->operationType = (uint32)FSA_CMD_OPERATION_TYPE::GETCWD;

		return FSA_RESULT::OK;
	}

	sint32 FSGetCwdAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* dirPathOut, sint32 dirPathMaxLen, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		// used by titles: Super Mario Maker
		_FSCmdIntro();
		fsCmdBlockBody->returnValues.cmdGetCwd.pathPtr = dirPathOut;
		fsCmdBlockBody->returnValues.cmdGetCwd.transferSize = dirPathMaxLen;

		FSA_RESULT prepareResult = __FSPrepareCmd_GetCwd(&fsCmdBlockBody->fsaShimBuffer, fsClientBody->iosuFSAHandle);
		if (prepareResult != FSA_RESULT::OK)
			return (FSStatus)_FSAStatusToFSStatus(prepareResult);

		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, RPLLoader_MakePPCCallable(export___FSQueueDefaultFinishFunc));
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSGetCwd(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* dirPathOut, sint32 dirPathMaxLen, uint32 errorMask)
	{
		StackAllocator<FSAsyncParamsNew_t> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSGetCwdAsync(fsClient, fsCmdBlock, dirPathOut, dirPathMaxLen, errorMask, asyncParams);
		auto r = __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
		return r;
	}

	FSA_RESULT __FSPrepareCmd_FlushQuota(iosu::fsa::FSAShimBuffer* fsaShimBuffer, IOSDevHandle devHandle, char* path)
	{
		if (fsaShimBuffer == NULL)
			return FSA_RESULT::INVALID_BUFFER;
		if (path == NULL)
			return FSA_RESULT::INVALID_PATH;

		fsaShimBuffer->fsaDevHandle = devHandle;
		fsaShimBuffer->ipcReqType = 0;
		fsaShimBuffer->operationType = (uint32)FSA_CMD_OPERATION_TYPE::FLUSHQUOTA;

		size_t pathLen = strlen((char*)path);
		if (pathLen >= FSA_CMD_PATH_MAX_LENGTH)
		{
			cemu_assert_debug(false);
			pathLen = FSA_CMD_PATH_MAX_LENGTH - 1;
		}
		for (sint32 i = 0; i < pathLen; i++)
			fsaShimBuffer->request.cmdFlushQuota.path[i] = path[i];
		fsaShimBuffer->request.cmdFlushQuota.path[pathLen] = '\0';

		return FSA_RESULT::OK;
	}

	sint32 FSFlushQuotaAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		_FSCmdIntro();

		FSA_RESULT prepareResult = __FSPrepareCmd_FlushQuota(&fsCmdBlockBody->fsaShimBuffer, fsClientBody->iosuFSAHandle, path);
		if (prepareResult != FSA_RESULT::OK)
			return (FSStatus)_FSAStatusToFSStatus(prepareResult);

		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, RPLLoader_MakePPCCallable(export___FSQueueDefaultFinishFunc));
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSFlushQuota(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, uint32 errorMask)
	{
		StackAllocator<FSAsyncParamsNew_t> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSFlushQuotaAsync(fsClient, fsCmdBlock, path, errorMask, asyncParams);
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
	}

	FSA_RESULT __FSPrepareCmd_QueryInfo(iosu::fsa::FSAShimBuffer* fsaShimBuffer, IOSDevHandle devHandle, uint8* queryString, uint32 queryType)
	{
		if (fsaShimBuffer == NULL)
			return FSA_RESULT::INVALID_BUFFER;
		if (queryString == NULL)
			return FSA_RESULT::INVALID_PATH;
		if (queryType > 8)
			return FSA_RESULT::INVALID_PARAM;

		fsaShimBuffer->fsaDevHandle = devHandle;
		fsaShimBuffer->ipcReqType = 0;
		fsaShimBuffer->operationType = (uint32)FSA_CMD_OPERATION_TYPE::QUERYINFO;

		size_t stringLen = strlen((char*)queryString);
		if (stringLen >= FSA_CMD_PATH_MAX_LENGTH)
		{
			cemu_assert_debug(false);
			stringLen = FSA_CMD_PATH_MAX_LENGTH - 1;
		}
		for (sint32 i = 0; i < stringLen; i++)
		{
			fsaShimBuffer->request.cmdQueryInfo.query[i] = queryString[i];
		}

		fsaShimBuffer->request.cmdQueryInfo.query[stringLen] = '\0';
		fsaShimBuffer->request.cmdQueryInfo.queryType = queryType;

		return FSA_RESULT::OK;
	}

	sint32 __FSQueryInfoAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint8* queryString, uint32 queryType, void* queryResult, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		_FSCmdIntro();
		cemu_assert(queryString && queryResult); // query string and result must not be null
		fsCmdBlockBody->returnValues.cmdQueryInfo.queryResultPtr = queryResult;

		FSA_RESULT prepareResult = __FSPrepareCmd_QueryInfo(&fsCmdBlockBody->fsaShimBuffer, fsClientBody->iosuFSAHandle, queryString, queryType);
		if (prepareResult != FSA_RESULT::OK)
			return (FSStatus)_FSAStatusToFSStatus(prepareResult);

		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, RPLLoader_MakePPCCallable(export___FSQueueDefaultFinishFunc));
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSGetStatAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, const char* path, FSStat_t* statOut, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		sint32 fsStatus = __FSQueryInfoAsync(fsClient, fsCmdBlock, (uint8*)path, FSA_QUERY_TYPE_STAT, statOut, errorMask, fsAsyncParams);
		return fsStatus;
	}

	sint32 FSGetStat(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, const char* path, FSStat_t* statOut, uint32 errorMask)
	{
		StackAllocator<FSAsyncParamsNew_t, 1> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSGetStatAsync(fsClient, fsCmdBlock, path, statOut, errorMask, asyncParams);
		sint32 ret = __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
		return ret;
	}

	FSA_RESULT __FSPrepareCmd_GetStatFile(iosu::fsa::FSAShimBuffer* fsaShimBuffer, IOSDevHandle devHandle, FSFileHandle2 fileHandle)
	{
		if (fsaShimBuffer == NULL)
			return FSA_RESULT::INVALID_BUFFER;

		fsaShimBuffer->fsaDevHandle = devHandle;
		fsaShimBuffer->ipcReqType = 0;
		fsaShimBuffer->operationType = (uint32)FSA_CMD_OPERATION_TYPE::GETSTATFILE;

		fsaShimBuffer->request.cmdGetStatFile.fileHandle = fileHandle;

		return FSA_RESULT::OK;
	}

	sint32 FSGetStatFileAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, FSFileHandle2 fileHandle, FSStat_t* statOut, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		_FSCmdIntro();
		cemu_assert(statOut); // statOut must not be null
		fsCmdBlockBody->returnValues.cmdStatFile.resultPtr = statOut;

		FSA_RESULT prepareResult = __FSPrepareCmd_GetStatFile(&fsCmdBlockBody->fsaShimBuffer, fsClientBody->iosuFSAHandle, fileHandle);
		if (prepareResult != FSA_RESULT::OK)
			return (FSStatus)_FSAStatusToFSStatus(prepareResult);

		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, RPLLoader_MakePPCCallable(export___FSQueueDefaultFinishFunc));
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSGetStatFile(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, FSFileHandle2 fileHandle, FSStat_t* statOut, uint32 errorMask)
	{
		StackAllocator<FSAsyncParamsNew_t, 1> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSGetStatFileAsync(fsClient, fsCmdBlock, fileHandle, statOut, errorMask, asyncParams);
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
	}

	sint32 FSGetFreeSpaceSizeAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, const char* path, FSLargeSize* returnedFreeSize, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		// used by: Wii U system settings app, Art Academy, Unity (e.g. Snoopy's Grand Adventure), Super Smash Bros
		sint32 fsStatus = __FSQueryInfoAsync(fsClient, fsCmdBlock, (uint8*)path, FSA_QUERY_TYPE_FREESPACE, returnedFreeSize, errorMask, fsAsyncParams);
		return fsStatus;
	}

	sint32 FSGetFreeSpaceSize(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, const char* path, FSLargeSize* returnedFreeSize, uint32 errorMask)
	{
		StackAllocator<FSAsyncParamsNew_t, 1> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSGetFreeSpaceSizeAsync(fsClient, fsCmdBlock, path, returnedFreeSize, errorMask, asyncParams);
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
	}

	FSA_RESULT __FSPrepareCmd_IsEof(iosu::fsa::FSAShimBuffer* fsaShimBuffer, IOSDevHandle devHandle, uint32 fileHandle)
	{
		if (fsaShimBuffer == NULL)
			return FSA_RESULT::INVALID_BUFFER;

		fsaShimBuffer->fsaDevHandle = devHandle;
		fsaShimBuffer->ipcReqType = 0;
		fsaShimBuffer->operationType = (uint32)FSA_CMD_OPERATION_TYPE::ISEOF;

		fsaShimBuffer->request.cmdIsEof.fileHandle = fileHandle;

		return FSA_RESULT::OK;
	}

	sint32 FSIsEofAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		// used by Paper Monsters Recut
		_FSCmdIntro();

		FSA_RESULT prepareResult = __FSPrepareCmd_IsEof(&fsCmdBlockBody->fsaShimBuffer, fsClientBody->iosuFSAHandle, fileHandle);
		if (prepareResult != FSA_RESULT::OK)
			return (FSStatus)_FSAStatusToFSStatus(prepareResult);

		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, RPLLoader_MakePPCCallable(export___FSQueueDefaultFinishFunc));
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSIsEof(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32 errorMask)
	{
		StackAllocator<FSAsyncParamsNew_t, 1> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSIsEofAsync(fsClient, fsCmdBlock, fileHandle, errorMask, asyncParams);
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
	}

	void FSSetUserData(FSCmdBlock_t* fsCmdBlock, void* userData)
	{
		FSCmdBlockBody_t* fsCmdBlockBody = __FSGetCmdBlockBody(fsCmdBlock);
		if (fsCmdBlockBody)
			fsCmdBlockBody->userData = userData;
	}

	void* FSGetUserData(FSCmdBlock_t* fsCmdBlock)
	{
		FSCmdBlockBody_t* fsCmdBlockBody = __FSGetCmdBlockBody(fsCmdBlock);
		void* userData = nullptr;
		if (fsCmdBlockBody)
			userData = fsCmdBlockBody->userData.GetPtr();
		return userData;
	}

	FS_VOLSTATE FSGetVolumeState(FSClient_t* fsClient)
	{
		// todo
		return FS_VOLSTATE_READY;
	}

	sint32 FSGetErrorCodeForViewer(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock)
	{
		// todo
		return 0; // no error
	}

	FSCmdBlock_t* FSGetCurrentCmdBlock(FSClient_t* fsClient)
	{
		FSClientBody_t* fsClientBody = __FSGetClientBody(fsClient);
		if (!fsClientBody)
			return nullptr;
		FSCmdBlockBody_t* cmdBlockBody = fsClientBody->currentCmdBlockBody;
		if (!cmdBlockBody)
			return nullptr;
		return cmdBlockBody->selfCmdBlock;
	}

	sint32 FSGetLastErrorCodeForViewer(FSClient_t* fsClient)
	{
		FSCmdBlock_t* cmdBlock = FSGetCurrentCmdBlock(fsClient);
		if (cmdBlock)
		{
			cemu_assert_unimplemented();
		}
		return 0; // no error
	}

	std::vector<FSAClientHandle> s_fsa_activeClients;
	std::mutex s_fsa_activeClientsMutex;

	FSAClientHandle FSAAddClientEx(void* data)
	{
		if (data != NULL)
		{
			// TODO
			cemu_assert_unimplemented();
		}

		IOSDevHandle handle = IOS_Open("/dev/fsa", 0);
		if (handle < IOS_ERROR::IOS_ERROR_OK)
		{
			return (FSAClientHandle)FSA_RESULT::PERMISSION_ERROR;
		}

		s_fsa_activeClientsMutex.lock();
		s_fsa_activeClients.push_back((FSAClientHandle)handle);
		s_fsa_activeClientsMutex.unlock();

		return (FSAClientHandle)handle;
	}

	FSAClientHandle FSAAddClient(void* data)
	{
		return FSAAddClientEx(data);
	}

	FSA_RESULT FSADelClient(FSAClientHandle clientHandle)
	{
		if (clientHandle == 0)
		{
			return FSA_RESULT::INVALID_CLIENT_HANDLE;
		}
		s_fsa_activeClientsMutex.lock();

		auto it = std::find(s_fsa_activeClients.begin(), s_fsa_activeClients.end(), clientHandle);
		if (it != s_fsa_activeClients.end())
		{
			IOS_Close(clientHandle);
			s_fsa_activeClients.erase(it);
		}

		s_fsa_activeClientsMutex.unlock();

		return FSA_RESULT::OK;
	}

	SysAllocator<coreinit::IPCBufPool_t*> s_fsaIpcPool;
	SysAllocator<uint8, 0x37500> s_fsaIpcPoolBuffer;
	SysAllocator<uint32be> s_fsaIpcPoolBufferNumItems;

	std::mutex sFSAIPCBufferLock;
	bool s_fsaInitDone = false;

	void FSAInit()
	{
		if (!s_fsaInitDone)
		{
			s_fsaIpcPool = IPCBufPoolCreate(s_fsaIpcPoolBuffer.GetPtr(), s_fsaIpcPoolBuffer.GetByteSize(), sizeof(iosu::fsa::FSAShimBuffer), &s_fsaIpcPoolBufferNumItems, 0);
			s_fsaInitDone = true;
		}
	}

	bool FSAShimCheckClientHandle(FSAClientHandle clientHandle)
	{
		std::scoped_lock lock(s_fsa_activeClientsMutex);
		if (std::find(s_fsa_activeClients.begin(), s_fsa_activeClients.end(), clientHandle) != s_fsa_activeClients.end())
		{
			return true;
		}
		return false;
	}

	FSA_RESULT FSAShimAllocateBuffer(MEMPTR<MEMPTR<iosu::fsa::FSAShimBuffer>> outBuffer)
	{
		if (!s_fsaInitDone)
			return FSA_RESULT::NOT_INIT;

		sFSAIPCBufferLock.lock();
		auto ptr = IPCBufPoolAllocate(s_fsaIpcPool, sizeof(iosu::fsa::FSAShimBuffer));
		sFSAIPCBufferLock.unlock();

		if (!ptr)
			return FSA_RESULT::OUT_OF_RESOURCES;

		std::memset(ptr, 0, sizeof(iosu::fsa::FSAShimBuffer));
		outBuffer[0] = reinterpret_cast<iosu::fsa::FSAShimBuffer*>(ptr);
		return FSA_RESULT::OK;
	}

	FSA_RESULT FSAShimFreeBuffer(iosu::fsa::FSAShimBuffer* buffer)
	{
		sFSAIPCBufferLock.lock();
		IPCBufPoolFree(s_fsaIpcPool, (uint8_t*)buffer);
		sFSAIPCBufferLock.unlock();
		return FSA_RESULT::OK;
	}

	FSA_RESULT FSACloseFile(FSAClientHandle client, uint32 fileHandle)
	{
		if (!FSAShimCheckClientHandle(client))
			return FSA_RESULT::INVALID_CLIENT_HANDLE;

		StackAllocator<MEMPTR<iosu::fsa::FSAShimBuffer>, 1> shimBuffer;
		FSA_RESULT result = FSAShimAllocateBuffer(shimBuffer.GetPointer());
		if (result != FSA_RESULT::OK)
			return result;

		result = __FSPrepareCmd_CloseFile(shimBuffer->GetPtr(), client, fileHandle);
		if (result == FSA_RESULT::OK)
			result = __FSAIPCSubmitCommand(shimBuffer->GetPtr());

		FSAShimFreeBuffer(shimBuffer->GetPtr());
		return result;
	}

	FSA_RESULT FSAFlushFile(FSAClientHandle client, uint32_t fileHandle)
	{
		if (!FSAShimCheckClientHandle(client))
			return FSA_RESULT::INVALID_CLIENT_HANDLE;

		StackAllocator<MEMPTR<iosu::fsa::FSAShimBuffer>, 1> shimBuffer;
		FSA_RESULT result = FSAShimAllocateBuffer(shimBuffer.GetPointer());
		if (result != FSA_RESULT::OK)
			return result;

		result = __FSPrepareCmd_FlushFile(shimBuffer->GetPtr(), client, fileHandle);
		if (result == FSA_RESULT::OK)
			result = __FSAIPCSubmitCommand(shimBuffer->GetPtr());

		FSAShimFreeBuffer(shimBuffer->GetPtr());
		return result;
	}

	FSA_RESULT FSAMakeDir(FSAClientHandle client, const char* path, uint32 uknVal660)
	{
		if (!FSAShimCheckClientHandle(client))
			return FSA_RESULT::INVALID_CLIENT_HANDLE;

		StackAllocator<MEMPTR<iosu::fsa::FSAShimBuffer>, 1> shimBuffer;
		FSA_RESULT result = FSAShimAllocateBuffer(shimBuffer.GetPointer());
		if (result != FSA_RESULT::OK)
			return result;

		result = __FSPrepareCmd_MakeDir(shimBuffer->GetPtr(), client, path, uknVal660);
		if (result == FSA_RESULT::OK)
			result = __FSAIPCSubmitCommand(shimBuffer->GetPtr());

		FSAShimFreeBuffer(shimBuffer->GetPtr());
		return result;
	}

	FSA_RESULT FSARename(FSAClientHandle client, char* oldPath, char* newPath)
	{
		if (!FSAShimCheckClientHandle(client))
			return FSA_RESULT::INVALID_CLIENT_HANDLE;

		StackAllocator<MEMPTR<iosu::fsa::FSAShimBuffer>, 1> shimBuffer;
		FSA_RESULT result = FSAShimAllocateBuffer(shimBuffer.GetPointer());
		if (result != FSA_RESULT::OK)
			return result;

		result = __FSPrepareCmd_Rename(shimBuffer->GetPtr(), client, oldPath, newPath);
		if (result == FSA_RESULT::OK)
			result = __FSAIPCSubmitCommand(shimBuffer->GetPtr());

		FSAShimFreeBuffer(shimBuffer->GetPtr());
		return result;
	}

	FSA_RESULT FSAChangeDir(FSAClientHandle client, char* path)
	{
		if (!FSAShimCheckClientHandle(client))
			return FSA_RESULT::INVALID_CLIENT_HANDLE;

		StackAllocator<MEMPTR<iosu::fsa::FSAShimBuffer>, 1> shimBuffer;
		FSA_RESULT result = FSAShimAllocateBuffer(shimBuffer.GetPointer());
		if (result != FSA_RESULT::OK)
			return result;

		result = __FSPrepareCmd_ChangeDir(shimBuffer->GetPtr(), client, (uint8_t*)path);
		if (result == FSA_RESULT::OK)
			result = __FSAIPCSubmitCommand(shimBuffer->GetPtr());

		FSAShimFreeBuffer(shimBuffer->GetPtr());
		return result;
	}

	FSA_RESULT FSAReadDir(FSAClientHandle client, FSDirHandle2 dirHandle, MEMPTR<FSDirEntry_t> directoryEntry)
	{
		if (directoryEntry.IsNull())
			return FSA_RESULT::INVALID_BUFFER;

		if (!FSAShimCheckClientHandle(client))
			return FSA_RESULT::INVALID_CLIENT_HANDLE;

		StackAllocator<MEMPTR<iosu::fsa::FSAShimBuffer>, 1> shimBuffer;
		FSA_RESULT result = FSAShimAllocateBuffer(shimBuffer.GetPointer());
		if (result != FSA_RESULT::OK)
			return result;

		result = __FSPrepareCmd_ReadDir(shimBuffer->GetPtr(), client, dirHandle);
		if (result == FSA_RESULT::OK)
		{
			result = __FSAIPCSubmitCommand(shimBuffer->GetPtr());
			if (result == FSA_RESULT::OK)
			{
				*directoryEntry = shimBuffer->GetPtr()->response.cmdReadDir.dirEntry;
			}
		}

		FSAShimFreeBuffer(shimBuffer->GetPtr());
		return result;
	}

	FSA_RESULT FSAOpenDir(FSAClientHandle client, char* path, MEMPTR<uint32be> dirHandle)
	{
		if (dirHandle.IsNull())
			return FSA_RESULT::INVALID_BUFFER;

		if (!FSAShimCheckClientHandle(client))
			return FSA_RESULT::INVALID_CLIENT_HANDLE;

		StackAllocator<MEMPTR<iosu::fsa::FSAShimBuffer>, 1> shimBuffer;
		FSA_RESULT result = FSAShimAllocateBuffer(shimBuffer.GetPointer());
		if (result != FSA_RESULT::OK)
			return result;

		result = __FSPrepareCmd_OpenDir(shimBuffer->GetPtr(), client, path);
		if (result == FSA_RESULT::OK)
		{
			result = __FSAIPCSubmitCommand(shimBuffer->GetPtr());
			if (result == FSA_RESULT::OK)
			{
				*dirHandle = shimBuffer->GetPtr()->response.cmdOpenDir.dirHandleOutput;
			}
		}

		FSAShimFreeBuffer(shimBuffer->GetPtr());
		return result;
	}

	FSA_RESULT FSACloseDir(FSAClientHandle client, FSDirHandle2 dirHandle)
	{
		if (!FSAShimCheckClientHandle(client))
			return FSA_RESULT::INVALID_CLIENT_HANDLE;

		StackAllocator<MEMPTR<iosu::fsa::FSAShimBuffer>, 1> shimBuffer;
		FSA_RESULT result = FSAShimAllocateBuffer(shimBuffer.GetPointer());
		if (result != FSA_RESULT::OK)
			return result;

		result = __FSPrepareCmd_CloseDir(shimBuffer->GetPtr(), client, dirHandle);
		if (result == FSA_RESULT::OK)
		{
			result = __FSAIPCSubmitCommand(shimBuffer->GetPtr());
		}

		FSAShimFreeBuffer(shimBuffer->GetPtr());
		return result;
	}

	FSA_RESULT FSARewindDir(FSAClientHandle client, FSDirHandle2 dirHandle)
	{
		if (!FSAShimCheckClientHandle(client))
			return FSA_RESULT::INVALID_CLIENT_HANDLE;

		StackAllocator<MEMPTR<iosu::fsa::FSAShimBuffer>, 1> shimBuffer;
		FSA_RESULT result = FSAShimAllocateBuffer(shimBuffer.GetPointer());
		if (result != FSA_RESULT::OK)
			return result;

		result = __FSPrepareCmd_RewindDir(shimBuffer->GetPtr(), client, dirHandle);
		if (result == FSA_RESULT::OK)
		{
			result = __FSAIPCSubmitCommand(shimBuffer->GetPtr());
		}

		FSAShimFreeBuffer(shimBuffer->GetPtr());
		return result;
	}

	FSA_RESULT FSAOpenFileEx(FSAClientHandle client, char* path, char* mode, uint32 createMode, uint32 openFlag, uint32_t preallocSize, MEMPTR<uint32be> outFileHandle)
	{
		if (outFileHandle.IsNull())
			return FSA_RESULT::INVALID_BUFFER;

		if (!FSAShimCheckClientHandle(client))
			return FSA_RESULT::INVALID_CLIENT_HANDLE;

		StackAllocator<MEMPTR<iosu::fsa::FSAShimBuffer>, 1> shimBuffer;
		FSA_RESULT result = FSAShimAllocateBuffer(shimBuffer.GetPointer());
		if (result != FSA_RESULT::OK)
			return result;

		result = __FSPrepareCmd_OpenFile(shimBuffer->GetPtr(), client, path, mode, createMode, openFlag, preallocSize);
		if (result == FSA_RESULT::OK)
		{
			result = __FSAIPCSubmitCommand(shimBuffer->GetPtr());
			if (result == FSA_RESULT::OK)
			{
				*outFileHandle = shimBuffer->GetPtr()->response.cmdOpenFile.fileHandleOutput;
			}
		}

		FSAShimFreeBuffer(shimBuffer->GetPtr());
		return result;
	}

	FSA_RESULT FSAGetStatFile(FSAClientHandle client, FSFileHandle2 fileHandle, MEMPTR<FSStat_t> outStat)
	{
		if (outStat.IsNull())
			return FSA_RESULT::INVALID_BUFFER;

		if (!FSAShimCheckClientHandle(client))
			return FSA_RESULT::INVALID_CLIENT_HANDLE;

		StackAllocator<MEMPTR<iosu::fsa::FSAShimBuffer>, 1> shimBuffer;
		FSA_RESULT result = FSAShimAllocateBuffer(shimBuffer.GetPointer());
		if (result != FSA_RESULT::OK)
			return result;

		result = __FSPrepareCmd_GetStatFile(shimBuffer->GetPtr(), client, fileHandle);
		if (result == FSA_RESULT::OK)
		{
			result = __FSAIPCSubmitCommand(shimBuffer->GetPtr());
			if (result == FSA_RESULT::OK)
			{
				*outStat = shimBuffer->GetPtr()->response.cmdStatFile.statOut;
			}
		}

		FSAShimFreeBuffer(shimBuffer->GetPtr());
		return result;
	}

	FSA_RESULT FSASetPosFile(FSAClientHandle client, FSFileHandle2 fileHandle, uint32_t pos)
	{
		if (!FSAShimCheckClientHandle(client))
			return FSA_RESULT::INVALID_CLIENT_HANDLE;

		StackAllocator<MEMPTR<iosu::fsa::FSAShimBuffer>, 1> shimBuffer;
		FSA_RESULT result = FSAShimAllocateBuffer(shimBuffer.GetPointer());
		if (result != FSA_RESULT::OK)
			return result;

		result = __FSPrepareCmd_SetPosFile(shimBuffer->GetPtr(), client, fileHandle, pos);
		if (result == FSA_RESULT::OK)
		{
			result = __FSAIPCSubmitCommand(shimBuffer->GetPtr());
		}

		FSAShimFreeBuffer(shimBuffer->GetPtr());
		return result;
	}

	FSA_RESULT FSATruncateFile(FSAClientHandle client, FSFileHandle2 fileHandle)
	{
		if (!FSAShimCheckClientHandle(client))
			return FSA_RESULT::INVALID_CLIENT_HANDLE;

		StackAllocator<MEMPTR<iosu::fsa::FSAShimBuffer>, 1> shimBuffer;
		FSA_RESULT result = FSAShimAllocateBuffer(shimBuffer.GetPointer());
		if (result != FSA_RESULT::OK)
			return result;

		result = __FSPrepareCmd_TruncateFile(shimBuffer->GetPtr(), client, fileHandle);
		if (result == FSA_RESULT::OK)
		{
			result = __FSAIPCSubmitCommand(shimBuffer->GetPtr());
		}

		FSAShimFreeBuffer(shimBuffer->GetPtr());
		return result;
	}

	FSA_RESULT FSARemove(FSAClientHandle client, char* path)
	{
		if (!FSAShimCheckClientHandle(client))
			return FSA_RESULT::INVALID_CLIENT_HANDLE;

		StackAllocator<MEMPTR<iosu::fsa::FSAShimBuffer>, 1> shimBuffer;
		FSA_RESULT result = FSAShimAllocateBuffer(shimBuffer.GetPointer());
		if (result != FSA_RESULT::OK)
			return result;

		result = __FSPrepareCmd_Remove(shimBuffer->GetPtr(), client, (uint8_t*)path);
		if (result == FSA_RESULT::OK)
		{
			result = __FSAIPCSubmitCommand(shimBuffer->GetPtr());
		}

		FSAShimFreeBuffer(shimBuffer->GetPtr());
		return result;
	}

	FSA_RESULT __FSPrepareCmd_ChangeMode(iosu::fsa::FSAShimBuffer* fsaShimBuffer, IOSDevHandle fsaHandle, uint8_t* path, uint32 permission, uint32 permissionMask)
	{
		if (fsaShimBuffer == nullptr)
			return FSA_RESULT::INVALID_BUFFER;
		if (path == nullptr)
			return FSA_RESULT::INVALID_PATH;

		fsaShimBuffer->fsaDevHandle = fsaHandle;
		fsaShimBuffer->ipcReqType = 0;
		fsaShimBuffer->operationType = (uint32)FSA_CMD_OPERATION_TYPE::REWINDDIR;

		size_t pathLen = strlen((char*)path);

		for (sint32 i = 0; i < pathLen; i++)
			fsaShimBuffer->request.cmdChangeMode.path[i] = path[i];
		for (size_t i = pathLen; i < FSA_CMD_PATH_MAX_LENGTH; i++)
			fsaShimBuffer->request.cmdChangeMode.path[i] = '\0';

		fsaShimBuffer->request.cmdChangeMode.mode1 = permission;
		fsaShimBuffer->request.cmdChangeMode.mode2 = permissionMask;

		return FSA_RESULT::OK;
	}

	FSA_RESULT FSAChangeMode(FSAClientHandle client, const char* path, uint32 permission)
	{
		if (!FSAShimCheckClientHandle(client))
			return FSA_RESULT::INVALID_CLIENT_HANDLE;

		StackAllocator<MEMPTR<iosu::fsa::FSAShimBuffer>, 1> shimBuffer;
		FSA_RESULT result = FSAShimAllocateBuffer(shimBuffer.GetPointer());
		if (result != FSA_RESULT::OK)
			return result;

		result = __FSPrepareCmd_ChangeMode(shimBuffer->GetPtr(), client, (uint8_t*)path, permission, 0x666);
		if (result == FSA_RESULT::OK)
		{
			result = __FSAIPCSubmitCommand(shimBuffer->GetPtr());
		}

		FSAShimFreeBuffer(shimBuffer->GetPtr());
		return result;
	}

	FSA_RESULT FSAReadFile(FSAClientHandle client, void* buffer, uint32_t size, uint32_t count, FSFileHandle2 handle, uint32_t flags)
	{
		if (!FSAShimCheckClientHandle(client))
			return FSA_RESULT::INVALID_CLIENT_HANDLE;

		StackAllocator<MEMPTR<iosu::fsa::FSAShimBuffer>, 1> shimBuffer;
		FSA_RESULT result = FSAShimAllocateBuffer(shimBuffer.GetPointer());
		if (result != FSA_RESULT::OK)
			return result;

		result = __FSPrepareCmd_ReadFile(shimBuffer->GetPtr(), client, buffer, size, count, 0, handle, flags & ~0x2);
		if (result == FSA_RESULT::OK)
		{
			result = __FSAIPCSubmitCommand(shimBuffer->GetPtr());
		}

		FSAShimFreeBuffer(shimBuffer->GetPtr());
		return result;
	}

	FSA_RESULT FSAWriteFile(FSAClientHandle client, void* buffer, uint32_t size, uint32_t count, FSFileHandle2 handle, uint32_t flags)
	{
		if (!FSAShimCheckClientHandle(client))
			return FSA_RESULT::INVALID_CLIENT_HANDLE;

		StackAllocator<MEMPTR<iosu::fsa::FSAShimBuffer>, 1> shimBuffer;
		FSA_RESULT result = FSAShimAllocateBuffer(shimBuffer.GetPointer());
		if (result != FSA_RESULT::OK)
			return result;

		result = __FSPrepareCmd_WriteFile(shimBuffer->GetPtr(), client, buffer, size, count, 0, handle, flags & ~0x2);
		if (result == FSA_RESULT::OK)
		{
			result = __FSAIPCSubmitCommand(shimBuffer->GetPtr());
		}

		FSAShimFreeBuffer(shimBuffer->GetPtr());
		return result;
	}

	FSA_RESULT FSAGetInfoByQuery(FSAClientHandle client, char* path, uint32_t queryType, MEMPTR<void> outData)
	{
		if (outData.IsNull())
			return FSA_RESULT::INVALID_BUFFER;

		StackAllocator<MEMPTR<iosu::fsa::FSAShimBuffer>, 1> shimBuffer;
		FSA_RESULT result = FSAShimAllocateBuffer(shimBuffer.GetPointer());
		if (result != FSA_RESULT::OK)
			return result;

		result = __FSPrepareCmd_QueryInfo(shimBuffer->GetPtr(), client, (uint8_t*)path, queryType);
		if (result == FSA_RESULT::OK)
		{
			result = __FSAIPCSubmitCommand(shimBuffer->GetPtr());
			if (result == FSA_RESULT::OK)
			{
				if (queryType == FSA_QUERY_TYPE_FREESPACE)
				{
					*MEMPTR<uint64be>(outData.GetMPTR()) = shimBuffer->GetPtr()->response.cmdQueryInfo.queryFreeSpace.freespace;
				}
				else if (queryType == FSA_QUERY_TYPE_DEVICE_INFO)
				{
					*MEMPTR<FSADeviceInfo_t>(outData.GetMPTR()) = shimBuffer->GetPtr()->response.cmdQueryInfo.queryDeviceInfo.info;
				}
				else if (queryType == FSA_QUERY_TYPE_STAT)
				{
					*MEMPTR<FSStat_t>(outData.GetMPTR()) = shimBuffer->GetPtr()->response.cmdQueryInfo.queryStat.stat;
				}
				else
				{
					// TODO: implement other query types
					cemu_assert_unimplemented();
					result = FSA_RESULT::FATAL_ERROR;
				}
			}
		}

		FSAShimFreeBuffer(shimBuffer->GetPtr());
		return result;
	}

	FSA_RESULT FSAGetStat(FSAClientHandle client, char* path, FSStat_t* outStat)
	{
		return FSAGetInfoByQuery(client, path, FSA_QUERY_TYPE_STAT, outStat);
	}

	FSA_RESULT FSAGetFreeSpaceSize(FSAClientHandle client, char* path, uint64* outSize)
	{
		return FSAGetInfoByQuery(client, path, FSA_QUERY_TYPE_DEVICE_INFO, outSize);
	}

	FSA_RESULT FSAGetDeviceInfo(FSAClientHandle client, char* path, void* outSize)
	{
		return FSAGetInfoByQuery(client, path, FSA_QUERY_TYPE_FREESPACE, outSize);
	}

	SysAllocator s_fsaStr_OK("FSA_STATUS_OK");
	SysAllocator s_fsaStr_NOT_INIT("FSA_STATUS_NOT_INIT");
	SysAllocator s_fsaStr_END_OF_DIRECTORY("FSA_STATUS_END_OF_DIRECTORY");
	SysAllocator s_fsaStr_END_OF_FILE("FSA_STATUS_END_OF_FILE");
	SysAllocator s_fsaStr_MAX_CLIENTS("FSA_STATUS_MAX_CLIENTS");
	SysAllocator s_fsaStr_MAX_FILES("FSA_STATUS_MAX_FILES");
	SysAllocator s_fsaStr_MAX_DIRS("FSA_STATUS_MAX_DIRS");
	SysAllocator s_fsaStr_ALREADY_EXISTS("FSA_STATUS_ALREADY_EXISTS");
	SysAllocator s_fsaStr_NOT_FOUND("FSA_STATUS_NOT_FOUND");
	SysAllocator s_fsaStr_PERMISSION_ERROR("FSA_STATUS_PERMISSION_ERROR");
	SysAllocator s_fsaStr_INVALID_PARAM("FSA_STATUS_INVALID_PARAM");
	SysAllocator s_fsaStr_INVALID_PATH("FSA_STATUS_INVALID_PATH");
	SysAllocator s_fsaStr_INVALID_BUFFER("FSA_STATUS_INVALID_BUFFER");
	SysAllocator s_fsaStr_INVALID_ALIGNMENT("FSA_STATUS_INVALID_ALIGNMENT");
	SysAllocator s_fsaStr_INVALID_CLIENT_HANDLE("FSA_STATUS_INVALID_CLIENT_HANDLE");
	SysAllocator s_fsaStr_INVALID_FILE_HANDLE("FSA_STATUS_INVALID_FILE_HANDLE");
	SysAllocator s_fsaStr_INVALID_DIR_HANDLE("FSA_STATUS_INVALID_DIR_HANDLE");
	SysAllocator s_fsaStr_NOT_FILE("FSA_STATUS_NOT_FILE");
	SysAllocator s_fsaStr_NOT_DIR("FSA_STATUS_NOT_DIR");
	SysAllocator s_fsaStr_OUT_OF_RESOURCES("FSA_STATUS_OUT_OF_RESOURCES");
	SysAllocator s_fsaStr_UNKNOWN("FSA_STATUS_???");

	const char* FSAGetStatusStr(FSA_RESULT status)
	{
		switch (status)
		{
		case FSA_RESULT::OK:
		{
			return s_fsaStr_OK.GetPtr();
		}
		case FSA_RESULT::NOT_INIT:
		{
			return s_fsaStr_NOT_INIT.GetPtr();
		}
		case FSA_RESULT::END_OF_DIRECTORY:
		{
			return s_fsaStr_END_OF_DIRECTORY.GetPtr();
		}
		case FSA_RESULT::END_OF_FILE:
		{
			return s_fsaStr_END_OF_FILE.GetPtr();
		}
		case FSA_RESULT::MAX_CLIENTS:
		{
			return s_fsaStr_MAX_CLIENTS.GetPtr();
		}
		case FSA_RESULT::MAX_FILES:
		{
			return s_fsaStr_MAX_FILES.GetPtr();
		}
		case FSA_RESULT::MAX_DIRS:
		{
			return s_fsaStr_MAX_DIRS.GetPtr();
		}
		case FSA_RESULT::ALREADY_EXISTS:
		{
			return s_fsaStr_ALREADY_EXISTS.GetPtr();
		}
		case FSA_RESULT::NOT_FOUND:
		{
			return s_fsaStr_NOT_FOUND.GetPtr();
		}
		case FSA_RESULT::PERMISSION_ERROR:
		{
			return s_fsaStr_PERMISSION_ERROR.GetPtr();
		}
		case FSA_RESULT::INVALID_PARAM:
		{
			return s_fsaStr_INVALID_PARAM.GetPtr();
		}
		case FSA_RESULT::INVALID_PATH:
		{
			return s_fsaStr_INVALID_PATH.GetPtr();
		}
		case FSA_RESULT::INVALID_BUFFER:
		{
			return s_fsaStr_INVALID_BUFFER.GetPtr();
		}
		case FSA_RESULT::INVALID_ALIGNMENT:
		{
			return s_fsaStr_INVALID_ALIGNMENT.GetPtr();
		}
		case FSA_RESULT::INVALID_CLIENT_HANDLE:
		{
			return s_fsaStr_INVALID_CLIENT_HANDLE.GetPtr();
		}
		case FSA_RESULT::INVALID_FILE_HANDLE:
		{
			return s_fsaStr_INVALID_FILE_HANDLE.GetPtr();
		}
		case FSA_RESULT::INVALID_DIR_HANDLE:
		{
			return s_fsaStr_INVALID_DIR_HANDLE.GetPtr();
		}
		case FSA_RESULT::NOT_FILE:
		{
			return s_fsaStr_NOT_FILE.GetPtr();
		}
		case FSA_RESULT::NOT_DIR:
		{
			return s_fsaStr_NOT_DIR.GetPtr();
		}
		case FSA_RESULT::OUT_OF_RESOURCES:
		{
			return s_fsaStr_OUT_OF_RESOURCES.GetPtr();
		}
		case FSA_RESULT::FATAL_ERROR:
		{
			return s_fsaStr_UNKNOWN.GetPtr();
		}
		}
		cemu_assert_unimplemented();
		return s_fsaStr_UNKNOWN.GetPtr();
	}

	FSA_RESULT FSAMount(FSAClientHandle client, const char* source, const char* target, uint32 flags, void* arg_buf, uint32_t arg_len)
	{
		if ("/dev/sdcard01" == std::string_view(source) && "/vol/external01" == std::string_view(target) && flags == 0 && arg_buf == nullptr && arg_len == 0)
		{
			mountSDCard();
			return FSA_RESULT::OK;
		}
		else
		{
			cemu_assert_unimplemented();
		}

		return FSA_RESULT::FATAL_ERROR;
	}

	FSA_RESULT FSAUnmount(FSAClientHandle client,
						  const char* mountedTarget,
						  uint32 flags)
	{
		return FSA_RESULT::OK;
	}

	void InitializeFS()
	{
		OSInitMutex(&s_fsGlobalMutex);

		cafeExportRegister("coreinit", FSInit, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSShutdown, LogType::CoreinitFile);

		cafeExportRegister("coreinit", FSGetMountSource, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSGetMountSourceNext, LogType::CoreinitFile);

		cafeExportRegister("coreinit", FSMount, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSBindMount, LogType::CoreinitFile);

		// client management
		cafeExportRegister("coreinit", FSAddClientEx, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSAddClient, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSDelClient, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSGetClientNum, LogType::CoreinitFile);

		// cmd
		cafeExportRegister("coreinit", FSInitCmdBlock, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSGetAsyncResult, LogType::CoreinitFile);

		// file operations
		cafeExportRegister("coreinit", FSOpenFileAsync, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSOpenFile, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSOpenFileExAsync, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSOpenFileEx, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSCloseFileAsync, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSCloseFile, LogType::CoreinitFile);

		cafeExportRegister("coreinit", FSReadFileAsync, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSReadFile, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSReadFileWithPosAsync, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSReadFileWithPos, LogType::CoreinitFile);

		cafeExportRegister("coreinit", FSWriteFileAsync, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSWriteFile, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSWriteFileWithPosAsync, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSWriteFileWithPos, LogType::CoreinitFile);

		cafeExportRegister("coreinit", FSSetPosFileAsync, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSSetPosFile, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSGetPosFileAsync, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSGetPosFile, LogType::CoreinitFile);

		cafeExportRegister("coreinit", FSAppendFileAsync, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSAppendFile, LogType::CoreinitFile);

		cafeExportRegister("coreinit", FSTruncateFileAsync, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSTruncateFile, LogType::CoreinitFile);

		cafeExportRegister("coreinit", FSRenameAsync, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSRename, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSRemoveAsync, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSRemove, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSMakeDirAsync, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSMakeDir, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSChangeDirAsync, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSChangeDir, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSGetCwdAsync, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSGetCwd, LogType::CoreinitFile);

		cafeExportRegister("coreinit", FSIsEofAsync, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSIsEof, LogType::CoreinitFile);

		// directory operations
		cafeExportRegister("coreinit", FSOpenDirAsync, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSOpenDir, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSReadDirAsync, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSReadDir, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSCloseDirAsync, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSCloseDir, LogType::CoreinitFile);

		// stat
		cafeExportRegister("coreinit", FSGetFreeSpaceSizeAsync, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSGetFreeSpaceSize, LogType::CoreinitFile);

		cafeExportRegister("coreinit", FSGetStatAsync, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSGetStat, LogType::CoreinitFile);

		cafeExportRegister("coreinit", FSGetStatFileAsync, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSGetStatFile, LogType::CoreinitFile);

		// misc
		cafeExportRegister("coreinit", FSFlushQuotaAsync, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSFlushQuota, LogType::CoreinitFile);

		cafeExportRegister("coreinit", FSSetUserData, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSGetUserData, LogType::CoreinitFile);

		cafeExportRegister("coreinit", FSGetCurrentCmdBlock, LogType::CoreinitFile);

		cafeExportRegister("coreinit", FSGetVolumeState, LogType::CoreinitFile);
		cafeExportRegister("coreinit", FSGetErrorCodeForViewer, LogType::Placeholder);
		cafeExportRegister("coreinit", FSGetLastErrorCodeForViewer, LogType::Placeholder);

		cafeExportRegister("coreinit", FSAMakeDir, LogType::Placeholder);
		cafeExportRegister("coreinit", FSAInit, LogType::Placeholder);
		cafeExportRegister("coreinit", FSAAddClient, LogType::Placeholder);
		cafeExportRegister("coreinit", FSADelClient, LogType::Placeholder);
		cafeExportRegister("coreinit", FSARewindDir, LogType::Placeholder);
		cafeExportRegister("coreinit", FSAGetDeviceInfo, LogType::Placeholder);
		cafeExportRegister("coreinit", FSARename, LogType::Placeholder);

		cafeExportRegister("coreinit", FSAChangeDir, LogType::Placeholder);

		cafeExportRegister("coreinit", FSAMount, LogType::Placeholder);
		cafeExportRegister("coreinit", FSAUnmount, LogType::Placeholder);

		cafeExportRegister("coreinit", FSAChangeMode, LogType::Placeholder);
		cafeExportRegister("coreinit", FSAReadDir, LogType::Placeholder);
		cafeExportRegister("coreinit", FSAOpenDir, LogType::Placeholder);
		cafeExportRegister("coreinit", FSACloseDir, LogType::Placeholder);
		cafeExportRegister("coreinit", FSACloseFile, LogType::Placeholder);
		cafeExportRegister("coreinit", FSAFlushFile, LogType::Placeholder);
		cafeExportRegister("coreinit", FSAOpenFileEx, LogType::Placeholder);
		cafeExportRegister("coreinit", FSAGetStatFile, LogType::Placeholder);
		cafeExportRegister("coreinit", FSAGetFreeSpaceSize, LogType::Placeholder);
		cafeExportRegister("coreinit", FSASetPosFile, LogType::Placeholder);
		cafeExportRegister("coreinit", FSATruncateFile, LogType::Placeholder);
		cafeExportRegister("coreinit", FSARemove, LogType::Placeholder);
		cafeExportRegister("coreinit", FSAReadFile, LogType::Placeholder);
		cafeExportRegister("coreinit", FSAWriteFile, LogType::Placeholder);
		cafeExportRegister("coreinit", FSAGetStat, LogType::Placeholder);
		cafeExportRegister("coreinit", FSAGetStatusStr, LogType::Placeholder);

		g_fsRegisteredClientBodies = nullptr;
	}
} // namespace coreinit
