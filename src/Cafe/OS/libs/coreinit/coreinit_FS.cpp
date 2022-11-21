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

#define FS_CB_PLACEHOLDER_FINISHCMD	(MPTR)(0xF122330E)

// return false if src+'\0' does not fit into dst
template<std::size_t Size>
bool strcpy_whole(char(&dst)[Size], const char* src)
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
	std::mutex sFSClientLock;
	std::recursive_mutex sFSGlobalMutex;

	inline void FSLockMutex()
	{
		sFSGlobalMutex.lock();
	}

	inline void FSUnlockMutex()
	{
		sFSGlobalMutex.unlock();
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
		char path[128]; // todo - determine correct length
	};

	FS_RESULT FSGetMountSourceNext(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, MOUNT_TYPE mountSourceType, FS_MOUNT_SOURCE* mountSourceInfo, FS_ERROR_MASK errMask)
	{
		// hacky
		static FS_MOUNT_SOURCE* s_last_source = nullptr;
		if(s_last_source != mountSourceInfo)
		{
			s_last_source = mountSourceInfo;
			fsCmdBlock->data.mount_it = 0;
		}

		fsCmdBlock->data.mount_it++;
		
		// SD
		if (mountSourceType == MOUNT_TYPE::SD && fsCmdBlock->data.mount_it == 1)
		{
			mountSourceInfo->sourceType = 0;
			strcpy(mountSourceInfo->path, "/sd");
			return FS_RESULT::SUCCESS;
		}

		return FS_RESULT::END_ITERATION;
	}

	FS_RESULT FSGetMountSource(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, MOUNT_TYPE mountSourceType, FS_MOUNT_SOURCE* mountSourceInfo, FS_ERROR_MASK errMask)
	{
		return FSGetMountSourceNext(fsClient, fsCmdBlock, mountSourceType, mountSourceInfo, errMask);
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
		if (strcmp(mountPathSrc, "/dev/sdcard01") == 0) {
			if (_sdCard01Mounted)
				return FS_RESULT::ERR_PLACEHOLDER;

			std::error_code ec;
			const auto path = ActiveSettings::GetUserDataPath("sdcard/");
			fs::create_directories(path, ec);
			if (!FSCDeviceHostFS_Mount(mountPathOut, _pathToUtf8(path), FSC_PRIORITY_BASE))
				return FS_RESULT::ERR_PLACEHOLDER;
			_sdCard01Mounted = true;
		}
		else if (strcmp(mountPathSrc, "/dev/mlc01") == 0) {
			if (_mlc01Mounted)
				return FS_RESULT::ERR_PLACEHOLDER;

			if (!FSCDeviceHostFS_Mount(mountPathOut, _pathToUtf8(ActiveSettings::GetMlcPath()), FSC_PRIORITY_BASE))
				return FS_RESULT::ERR_PLACEHOLDER;
			_mlc01Mounted = true;
		}
		else {
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
		forceLog_printf("Critical error in FS: %s", msg.data());
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
		coreinit::OSInitMutexEx(&fsCmdQueueBE->mutex, nullptr);
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

	void __FSAIPCSubmitCommand(FSCmdBlockBody_t* cmd)
	{
		if (cmd->ipcReqType == 0)
		{
			IOS_ERROR r = IOS_IoctlAsync(cmd->fsaDevHandle, cmd->operationType, cmd, 0x520, (uint8*)cmd + 0x580, 0x293, MEMPTR<void>(PPCInterpreter_makeCallableExportDepr(__FSAIoctlResponseCallback)), cmd);
			cemu_assert(!IOS_ResultIsError(r));
		}
		else
		{
			cemu_assert_unimplemented(); // IOS_IoctlvAsync
		}
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
				__FSAIPCSubmitCommand(dequeuedCommand);
			}
		}
		FSUnlockMutex();
	}

	void __FSQueueCmd(FSCmdQueue* cmdQueue, FSCmdBlockBody_t* fsCmdBlockBody, MPTR finishCmdFunc)
	{
		fsCmdBlockBody->cmdFinishFuncMPTR = _swapEndianU32(finishCmdFunc);
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
		// todo
		// currently /dev/fsa uses FS status codes internally. We should refactor everything to use FSA error codes (which are compatible with IOS_ERROR) and then translate them here to FS status
		return (FS_RESULT)err;
	}

	void __FSCmdSubmitResult(FSCmdBlockBody_t* fsCmdBlockBody, FS_RESULT result)
	{
		_debugVerifyCommand("FSCmdSubmitResult", fsCmdBlockBody);

		FSClientBody_t* fsClientBody = fsCmdBlockBody->fsClientBody.GetPtr();
		sFSClientLock.lock(); // OSFastMutex_Lock(&fsClientBody->fsCmdQueue.mutex)
		fsCmdBlockBody->cancelState &= ~(1 << 0); // clear cancel bit
		if (fsClientBody->currentCmdBlockBody.GetPtr() == fsCmdBlockBody)
			fsClientBody->currentCmdBlockBody = nullptr;
		fsCmdBlockBody->statusCode = _swapEndianU32(FSA_CMD_STATUS_CODE_D900A24);
		sFSClientLock.unlock();
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
				forceLog_printf("FS driver: Failed to add message to result queue. Retrying...");
				if (ppcInterpreterCurrentInstance)
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
		fsCmdBlockBody->fsaDevHandle = fsClientBody->iosuFSAHandle;
		__FSPrepareCmdAsyncResult(fsClientBody, fsCmdBlockBody , &fsCmdBlockBody->asyncResult, fsAsyncParams);
		return 0;
	}

#define _FSCmdIntro()		FSClientBody_t* fsClientBody = __FSGetClientBody(fsClient); \
							FSCmdBlockBody_t* fsCmdBlockBody = __FSGetCmdBlockBody(fsCmdBlock); \
							sint32 fsError = __FSPrepareCmd(fsClientBody, fsCmdBlockBody, errorMask, fsAsyncParams); \
							if (fsError != 0) return fsError;

	void _debugVerifyCommand(const char* stage, FSCmdBlockBody_t* fsCmdBlockBody)
	{
		if (fsCmdBlockBody->asyncResult.msgUnion.fsMsg.commandType != _swapEndianU32(8))
		{
			forceLog_printf("Corrupted FS command detected in stage %s", stage);
			forceLog_printf("Printing CMD block: ");
			for (uint32 i = 0; i < (sizeof(FSCmdBlockBody_t) + 31) / 32; i++)
			{
				uint8* p = ((uint8*)fsCmdBlockBody) + i * 32;
				forceLog_printf("%04x: %02x %02x %02x %02x - %02x %02x %02x %02x - %02x %02x %02x %02x - %02x %02x %02x %02x | %02x %02x %02x %02x - %02x %02x %02x %02x - %02x %02x %02x %02x - %02x %02x %02x %02x",
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
			forceLog_printf("FS handleAsyncResult(): unexpected error %08x", errHandling);
			cemu_assert_debug(false);
			return 0;
		}
	}

	uint32 __FSPrepareCmd_OpenFile(FSCmdBlockBody_t* fsCmdBlockBody, char* path, char* mode, uint32 createMode, uint32 openFlags, uint32 preallocSize)
	{
		fsCmdBlockBody->fsCmdBlockBodyMPTR = _swapEndianU32(memory_getVirtualOffsetFromPointer(fsCmdBlockBody));
		if (fsCmdBlockBody == NULL)
			return 0xFFFCFFDD;
		if (path == NULL)
			return 0xFFFCFFDD + 1;
		if (mode == NULL)
			return 0xFFFCFFDD + 2;
		fsCmdBlockBody->fsCmdBlockBodyMPTR = _swapEndianU32(memory_getVirtualOffsetFromPointer(fsCmdBlockBody));
		fsCmdBlockBody->ipcData.cmdOpenFile.fileHandleOutput = _swapEndianU32(0xFFFFFFFF);
		fsCmdBlockBody->operationType = FSA_CMD_OPERATION_TYPE_OPENFILE;
		// path
		size_t pathLen = strlen((char*)path);
		if (pathLen >= FSA_CMD_PATH_MAX_LENGTH)
		{
			cemu_assert_debug(false);
			pathLen = FSA_CMD_PATH_MAX_LENGTH - 1;
		}
		for (sint32 i = 0; i < pathLen; i++)
			fsCmdBlockBody->ipcData.cmdOpenFile.path[i] = path[i];
		for (size_t i = pathLen; i < FSA_CMD_PATH_MAX_LENGTH; i++)
			fsCmdBlockBody->ipcData.cmdOpenFile.path[i] = '\0';
		// mode
		size_t modeLen = strlen((char*)mode);
		if (modeLen >= 12)
		{
			cemu_assert_debug(false);
			modeLen = 12 - 1;
		}
		for (sint32 i = 0; i < modeLen; i++)
			fsCmdBlockBody->ipcData.cmdOpenFile.mode[i] = mode[i];
		for (size_t i = modeLen; i < 12; i++)
			fsCmdBlockBody->ipcData.cmdOpenFile.mode[i] = '\0';
		// createMode
		fsCmdBlockBody->ipcData.cmdOpenFile.createMode = createMode;
		// openFlags
		fsCmdBlockBody->ipcData.cmdOpenFile.openFlags = openFlags;
		// preallocSize
		fsCmdBlockBody->ipcData.cmdOpenFile.preallocSize = preallocSize;
		return 0;
	}

	sint32 FSOpenFileAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, char* mode, FSFileHandleDepr_t* fileHandle, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		_FSCmdIntro();
		if (fileHandle == nullptr || path == nullptr || mode == nullptr)
			return -0x400;
		fsCmdBlockBody->returnValueMPTR = _swapEndianU32(memory_getVirtualOffsetFromPointer(fileHandle));
		fsError = __FSPrepareCmd_OpenFile(fsCmdBlockBody, path, mode, 0x660, 0, 0);
		if (fsError != (FSStatus)FS_RESULT::SUCCESS)
			return fsError;
		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, FS_CB_PLACEHOLDER_FINISHCMD);
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSOpenFile(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, char* mode, FSFileHandleDepr_t* fileHandle, uint32 errHandling)
	{
		StackAllocator<FSAsyncParamsNew_t, 1> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSOpenFileAsync(fsClient, fsCmdBlock, path, mode, fileHandle, errHandling, asyncParams);
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errHandling);
	}

	sint32 FSOpenFileExAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, char* mode, uint32 createMode, uint32 openFlag, uint32 preallocSize, FSFileHandleDepr_t* fileHandle, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		if (openFlag != 0)
		{
			cemuLog_log(LogType::Force, "FSOpenFileEx called with unsupported flags!");
			cemu_assert_debug(false);
		}

		_FSCmdIntro();
		if (fileHandle == nullptr || path == nullptr || mode == nullptr)
			return -0x400;
		fsCmdBlockBody->returnValueMPTR = _swapEndianU32(memory_getVirtualOffsetFromPointer(fileHandle));
		fsError = __FSPrepareCmd_OpenFile(fsCmdBlockBody, path, mode, createMode, openFlag, preallocSize);
		if (fsError != (FSStatus)FS_RESULT::SUCCESS)
			return fsError;
		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, FS_CB_PLACEHOLDER_FINISHCMD);
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSOpenFileEx(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, char* mode, uint32 createMode, uint32 openFlag, uint32 preallocSize, FSFileHandleDepr_t* fileHandle, uint32 errHandling)
	{
		StackAllocator<FSAsyncParamsNew_t, 1> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSOpenFileExAsync(fsClient, fsCmdBlock, path, mode, createMode, openFlag, preallocSize, fileHandle, errHandling, asyncParams);
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errHandling);
	}

	void __FSPrepareCmd_CloseFile(FSCmdBlockBody_t* fsCmdBlockBody, uint32 fileHandle)
	{
		fsCmdBlockBody->operationType = FSA_CMD_OPERATION_TYPE_CLOSEFILE;
		fsCmdBlockBody->ipcData.cmdCloseFile.fileHandle = fileHandle;
	}

	sint32 FSCloseFileAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		_FSCmdIntro();
		__FSPrepareCmd_CloseFile(fsCmdBlockBody, fileHandle);
		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, FS_CB_PLACEHOLDER_FINISHCMD);
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSCloseFile(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32 errHandling)
	{
		StackAllocator<FSAsyncParamsNew_t, 1> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSCloseFileAsync(fsClient, fsCmdBlock, fileHandle, errHandling, asyncParams);
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errHandling);
	}

	uint32 __FSPrepareCmd_ReadFile(FSCmdBlockBody_t* fsCmdBlockBody, void* dest, uint32 uknR6, uint32 transferSizeUknAligned, uint32 filePos, uint32 fileHandle, uint32 flag)
	{
		fsCmdBlockBody->fsCmdBlockBodyMPTR = _swapEndianU32(memory_getVirtualOffsetFromPointer(fsCmdBlockBody));
		if (fsCmdBlockBody == NULL || dest == NULL)
			return 0xFFFCFFDD;
		MPTR destMPTR = memory_getVirtualOffsetFromPointer(dest);
		if ((destMPTR & 0x3F) != 0)
			return 0xFFFCFFDC;
		fsCmdBlockBody->fsCmdBlockBodyMPTR = _swapEndianU32(memory_getVirtualOffsetFromPointer(fsCmdBlockBody));
		fsCmdBlockBody->ipcData.cmdDefault.fileHandle = _swapEndianU32(fileHandle);
		fsCmdBlockBody->ukn0898 = _swapEndianU32(memory_getVirtualOffsetFromPointer(fsCmdBlockBody) + 0x580);
		fsCmdBlockBody->ipcData.cmdDefault.ukn0008 = _swapEndianU32(uknR6);
		fsCmdBlockBody->ipcData.cmdDefault.ukn000C = _swapEndianU32(transferSizeUknAligned);
		uint32 fullTransferSize = transferSizeUknAligned * uknR6;
		fsCmdBlockBody->ukn090B = 2; // byte
		fsCmdBlockBody->ukn089C = _swapEndianU32(0x293);
		fsCmdBlockBody->ukn0890 = _swapEndianU32(fullTransferSize);
		fsCmdBlockBody->ukn0884 = _swapEndianU32(0x520);
		fsCmdBlockBody->destBuffer88CMPTR = _swapEndianU32(destMPTR);
		fsCmdBlockBody->ukn090A = 1;
		fsCmdBlockBody->operationType = FSA_CMD_OPERATION_TYPE_READ;
		fsCmdBlockBody->ipcData.cmdDefault.destBufferMPTR = _swapEndianU32(destMPTR);
		fsCmdBlockBody->ipcData.cmdDefault.transferFilePos = _swapEndianU32(filePos);
		fsCmdBlockBody->ipcData.cmdDefault.cmdFlag = _swapEndianU32(flag);
		return 0;
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
		uint32 transferSize = (uint32)transferSizeS64;
		fsCmdBlockBody->transferSize = _swapEndianU32(transferSize);
		fsCmdBlockBody->transferElemSize = _swapEndianU32(size);
		fsCmdBlockBody->uknVal094C = _swapEndianU32(0);
		if (transferSize < 0x10)
			transferSize = 0x10;
		fsCmdBlockBody->uknVal0954 = _swapEndianU32(transferSize);
		if(usePos)
			flag |= FSA_CMD_FLAG_SET_POS;
		else
			flag &= ~FSA_CMD_FLAG_SET_POS;
		fsError = __FSPrepareCmd_ReadFile(fsCmdBlockBody, dest, 1, transferSize, filePos, fileHandle, flag);
		if (fsError != (FSStatus)FS_RESULT::SUCCESS)
			return fsError;
		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, FS_CB_PLACEHOLDER_FINISHCMD);
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

	uint32 __FSPrepareCmd_WriteFile(FSCmdBlockBody_t* fsCmdBlockBody, void* dest, uint32 uknR6, uint32 transferSizeUknAligned, uint32 filePos, uint32 fileHandle, uint32 flag)
	{
		fsCmdBlockBody->fsCmdBlockBodyMPTR = _swapEndianU32(memory_getVirtualOffsetFromPointer(fsCmdBlockBody));
		if (fsCmdBlockBody == NULL && dest == NULL)
			return 0xFFFCFFDD;
		MPTR destMPTR = memory_getVirtualOffsetFromPointer(dest);
		if ((destMPTR & 0x3F) != 0)
			return 0xFFFCFFDC;
		cemu_assert_debug((uknR6 * transferSizeUknAligned) != 0); // todo: do zero-sized writes need special treatment?
		fsCmdBlockBody->fsCmdBlockBodyMPTR = _swapEndianU32(memory_getVirtualOffsetFromPointer(fsCmdBlockBody));
		fsCmdBlockBody->ipcData.cmdDefault.fileHandle = _swapEndianU32(fileHandle);
		fsCmdBlockBody->ukn0898 = _swapEndianU32(memory_getVirtualOffsetFromPointer(fsCmdBlockBody) + 0x580); // verified
		fsCmdBlockBody->ipcData.cmdDefault.ukn0008 = _swapEndianU32(uknR6);
		fsCmdBlockBody->ipcData.cmdDefault.ukn000C = _swapEndianU32(transferSizeUknAligned);
		uint32 fullTransferSize = transferSizeUknAligned * uknR6;
		fsCmdBlockBody->ukn090B = 1; // byte - verified (note: This member holds 2 for read operations)
		fsCmdBlockBody->ukn089C = _swapEndianU32(0x293);
		fsCmdBlockBody->ukn0890 = _swapEndianU32(fullTransferSize);
		fsCmdBlockBody->ukn0884 = _swapEndianU32(0x520); // verified
		fsCmdBlockBody->destBuffer88CMPTR = _swapEndianU32(destMPTR);
		fsCmdBlockBody->ukn090A = 1;
		fsCmdBlockBody->operationType = FSA_CMD_OPERATION_TYPE_WRITE;
		fsCmdBlockBody->ipcData.cmdDefault.destBufferMPTR = _swapEndianU32(destMPTR);
		fsCmdBlockBody->ipcData.cmdDefault.transferFilePos = _swapEndianU32(filePos);
		fsCmdBlockBody->ipcData.cmdDefault.cmdFlag = _swapEndianU32(flag);
		return (FSStatus)FS_RESULT::SUCCESS;
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
		uint32 transferSize = (uint32)transferSizeS64;
		fsCmdBlockBody->transferSize = _swapEndianU32(transferSize);
		fsCmdBlockBody->transferElemSize = _swapEndianU32(size);
		fsCmdBlockBody->uknVal094C = _swapEndianU32(0);
		fsCmdBlockBody->uknVal0954 = _swapEndianU32(transferSize);
		if (useFilePos)
			flag |= FSA_CMD_FLAG_SET_POS;
		else
			flag &= ~FSA_CMD_FLAG_SET_POS;
		fsError = __FSPrepareCmd_WriteFile(fsCmdBlockBody, dest, 1, transferSize, filePos, fileHandle, flag);
		if (fsError != (FSStatus)FS_RESULT::SUCCESS)
			return fsError;
		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, FS_CB_PLACEHOLDER_FINISHCMD);
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

	uint32 __FSPrepareCmd_SetPosFile(FSCmdBlockBody_t* fsCmdBlockBody, uint32 fileHandle, uint32 filePos)
	{
		if (fsCmdBlockBody == NULL)
			return 0xFFFCFFDD;
		fsCmdBlockBody->ipcData.cmdDefault.destBufferMPTR = _swapEndianU32(fileHandle);
		fsCmdBlockBody->ipcData.cmdDefault.ukn0008 = _swapEndianU32(filePos);
		fsCmdBlockBody->operationType = FSA_CMD_OPERATION_TYPE_SETPOS;
		return 0;
	}

	sint32 FSSetPosFileAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32 filePos, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		_FSCmdIntro();
		fsError = __FSPrepareCmd_SetPosFile(fsCmdBlockBody, fileHandle, filePos);
		if (fsError != (FSStatus)FS_RESULT::SUCCESS)
			return fsError;
		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, FS_CB_PLACEHOLDER_FINISHCMD);
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

	uint32 __FSPrepareCmd_GetPosFile(FSCmdBlockBody_t* fsCmdBlockBody, uint32 fileHandle)
	{
		if (fsCmdBlockBody == NULL)
			return 0xFFFCFFDD;
		fsCmdBlockBody->ipcData.cmdDefault.destBufferMPTR = _swapEndianU32(fileHandle);
		fsCmdBlockBody->operationType = FSA_CMD_OPERATION_TYPE_GETPOS;
		return 0;
	}

	sint32 FSGetPosFileAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32be* returnedFilePos, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		// games using this: Darksiders Warmastered Edition
		_FSCmdIntro();
		fsCmdBlockBody->returnValueMPTR = _swapEndianU32(memory_getVirtualOffsetFromPointer(returnedFilePos));
		fsError = __FSPrepareCmd_GetPosFile(fsCmdBlockBody, fileHandle);
		if (fsError != (FSStatus)FS_RESULT::SUCCESS)
			return fsError;
		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, FS_CB_PLACEHOLDER_FINISHCMD);
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSGetPosFile(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32be* returnedFilePos, uint32 errorMask)
	{
		StackAllocator<FSAsyncParamsNew_t, 1> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSGetPosFileAsync(fsClient, fsCmdBlock, fileHandle, returnedFilePos, errorMask, asyncParams.GetPointer());
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
	}

	sint32 __FSPrepareCmd_OpenDir(FSCmdBlockBody_t* fsCmdBlockBody, char* path)
	{
		fsCmdBlockBody->fsCmdBlockBodyMPTR = _swapEndianU32(memory_getVirtualOffsetFromPointer(fsCmdBlockBody));
		if (fsCmdBlockBody == nullptr)
			return 0xFFFCFFDD;
		if (path == nullptr)
			return 0xFFFCFFDD + 1;

		fsCmdBlockBody->fsCmdBlockBodyMPTR = _swapEndianU32(memory_getVirtualOffsetFromPointer(fsCmdBlockBody));
		fsCmdBlockBody->ipcData.cmdOpenDir.dirHandleOutput = -1;
		fsCmdBlockBody->operationType = FSA_CMD_OPERATION_TYPE_OPENDIR;
		// path
		sint32 pathLen = (sint32)strlen((char*)path);
		if (pathLen >= FSA_CMD_PATH_MAX_LENGTH)
		{
			cemu_assert_debug(false);
			pathLen = FSA_CMD_PATH_MAX_LENGTH - 1;
		}
		for (sint32 i = 0; i < pathLen; i++)
			fsCmdBlockBody->ipcData.cmdOpenDir.path[i] = path[i];
		for (sint32 i = pathLen; i < FSA_CMD_PATH_MAX_LENGTH; i++)
			fsCmdBlockBody->ipcData.cmdOpenDir.path[i] = '\0';
		return 0;
	}

	sint32 FSOpenDirAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, FSDirHandlePtr dirHandleOut, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		_FSCmdIntro();
		cemu_assert(dirHandleOut && path);
		fsCmdBlockBody->returnValueMPTR = _swapEndianU32(dirHandleOut.GetMPTR());
		fsError = __FSPrepareCmd_OpenDir(fsCmdBlockBody, path);
		if (fsError != (FSStatus)FS_RESULT::SUCCESS)
			return fsError;
		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, FS_CB_PLACEHOLDER_FINISHCMD);
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSOpenDir(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, FSDirHandlePtr dirHandleOut, uint32 errorMask)
	{
		StackAllocator<FSAsyncParamsNew_t, 1> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSOpenDirAsync(fsClient, fsCmdBlock, path, dirHandleOut, errorMask, asyncParams);
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
	}

	void __FSPrepareCmd_ReadDir(FSCmdBlockBody_t* fsCmdBlockBody, FSDirHandle2 dirHandle, FSDirEntry_t* dirEntryOut)
	{
		fsCmdBlockBody->returnValueMPTR = _swapEndianU32(memory_getVirtualOffsetFromPointer(dirEntryOut));
		fsCmdBlockBody->operationType = FSA_CMD_OPERATION_TYPE_READDIR;
		fsCmdBlockBody->ipcData.cmdReadDir.dirHandle = dirHandle;
	}

	sint32 FSReadDirAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, FSDirHandle2 dirHandle, FSDirEntry_t* dirEntryOut, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		_FSCmdIntro();
		__FSPrepareCmd_ReadDir(fsCmdBlockBody, dirHandle, dirEntryOut);
		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, FS_CB_PLACEHOLDER_FINISHCMD);
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSReadDir(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, FSDirHandle2 dirHandle, FSDirEntry_t* dirEntryOut, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		StackAllocator<FSAsyncParamsNew_t, 1> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSReadDirAsync(fsClient, fsCmdBlock, dirHandle, dirEntryOut, errorMask, asyncParams);
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
	}

	void __FSPrepareCmd_CloseDir(FSCmdBlockBody_t* fsCmdBlockBody, FSDirHandle2 dirHandle)
	{
		fsCmdBlockBody->operationType = FSA_CMD_OPERATION_TYPE_CLOSEDIR;
		fsCmdBlockBody->ipcData.cmdCloseDir.dirHandle = dirHandle;
	}

	sint32 FSCloseDirAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, FSDirHandle2 dirHandle, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		_FSCmdIntro();
		__FSPrepareCmd_CloseDir(fsCmdBlockBody, dirHandle);
		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, FS_CB_PLACEHOLDER_FINISHCMD);
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSCloseDir(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, FSDirHandle2 dirHandle, uint32 errorMask)
	{
		StackAllocator<FSAsyncParamsNew_t, 1> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSCloseDirAsync(fsClient, fsCmdBlock, dirHandle, errorMask, asyncParams);
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
	}

	void __FSPrepareCmd_AppendFile(FSCmdBlockBody_t* fsCmdBlockBody, uint32 fileHandle, uint32 size, uint32 count, uint32 uknParam)
	{
		fsCmdBlockBody->ipcData.cmdAppendFile.fileHandle = _swapEndianU32(fileHandle);
		fsCmdBlockBody->ipcData.cmdAppendFile.count = _swapEndianU32(size);
		fsCmdBlockBody->ipcData.cmdAppendFile.size = _swapEndianU32(count);
		fsCmdBlockBody->ipcData.cmdAppendFile.uknParam = _swapEndianU32(uknParam);
		fsCmdBlockBody->operationType = FSA_CMD_OPERATION_TYPE_APPENDFILE;
	}

	sint32 FSAppendFileAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32 size, uint32 count, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		_FSCmdIntro();
		__FSPrepareCmd_AppendFile(fsCmdBlockBody, fileHandle, size, count, 0);
		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, FS_CB_PLACEHOLDER_FINISHCMD);
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSAppendFile(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32 size, uint32 count, uint32 errorMask)
	{
		StackAllocator<FSAsyncParamsNew_t> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSAppendFileAsync(fsClient, fsCmdBlock, fileHandle, size, count, errorMask, asyncParams.GetPointer());
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
	}

	void __FSPrepareCmd_TruncateFile(FSCmdBlockBody_t* fsCmdBlockBody, FSFileHandle2 fileHandle)
	{
		fsCmdBlockBody->ipcData.cmdTruncateFile.fileHandle = fileHandle;
		fsCmdBlockBody->ipcData.cmdTruncateFile.ukn0008 = 0;
		fsCmdBlockBody->operationType = FSA_CMD_OPERATION_TYPE_TRUNCATEFILE;
	}

	sint32 FSTruncateFileAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, FSFileHandle2 fileHandle, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		_FSCmdIntro();
		__FSPrepareCmd_TruncateFile(fsCmdBlockBody, fileHandle);
		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, FS_CB_PLACEHOLDER_FINISHCMD);
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSTruncateFile(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, FSFileHandle2 fileHandle, uint32 errorMask)
	{
		StackAllocator<FSAsyncParamsNew_t> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSTruncateFileAsync(fsClient, fsCmdBlock, fileHandle, errorMask, asyncParams.GetPointer());
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
	}

	sint32 __FSPrepareCmd_Rename(FSCmdBlockBody_t* fsCmdBlockBody, char* srcPath, char* dstPath)
	{
		if (fsCmdBlockBody == NULL)
			return 0xFFFCFFDD;
		if (srcPath == NULL || dstPath == NULL)
			return 0xFFFCFFDE;
		// source path
		size_t stringLen = strlen((char*)srcPath);
		if (stringLen >= FSA_CMD_PATH_MAX_LENGTH)
		{
			cemu_assert_debug(false);
			stringLen = FSA_CMD_PATH_MAX_LENGTH - 1;
		}
		for (sint32 i = 0; i < stringLen; i++)
		{
			fsCmdBlockBody->ipcData.cmdRename.srcPath[i] = srcPath[i];
		}
		fsCmdBlockBody->ipcData.cmdRename.srcPath[stringLen] = '\0';
		// destination path
		stringLen = strlen((char*)dstPath);
		if (stringLen >= FSA_CMD_PATH_MAX_LENGTH)
		{
			cemu_assert_debug(false);
			stringLen = FSA_CMD_PATH_MAX_LENGTH - 1;
		}
		for (sint32 i = 0; i < stringLen; i++)
		{
			fsCmdBlockBody->ipcData.cmdRename.dstPath[i] = dstPath[i];
		}
		fsCmdBlockBody->ipcData.cmdRename.dstPath[stringLen] = '\0';
		fsCmdBlockBody->operationType = FSA_CMD_OPERATION_TYPE_RENAME;
		return 0;
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
		fsError = __FSPrepareCmd_Rename(fsCmdBlockBody, srcPath, dstPath);
		if (fsError != (FSStatus)FS_RESULT::SUCCESS)
			return fsError;
		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, FS_CB_PLACEHOLDER_FINISHCMD);
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSRename(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* srcPath, char* dstPath, uint32 errorMask)
	{
		StackAllocator<FSAsyncParamsNew_t> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSRenameAsync(fsClient, fsCmdBlock, srcPath, dstPath, errorMask, asyncParams.GetPointer());
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
	}

	sint32 __FSPrepareCmd_Remove(FSCmdBlockBody_t* fsCmdBlockBody, uint8* path)
	{
		if (fsCmdBlockBody == NULL)
			return 0xFFFCFFDD;
		if (path == NULL)
			return 0xFFFCFFDE;
		size_t pathLen = strlen((char*)path);
		if (pathLen >= FSA_CMD_PATH_MAX_LENGTH)
		{
			cemu_assert_debug(false);
			pathLen = FSA_CMD_PATH_MAX_LENGTH - 1;
		}
		for (sint32 i = 0; i < pathLen; i++)
		{
			fsCmdBlockBody->ipcData.cmdRemove.path[i] = path[i];
		}
		fsCmdBlockBody->ipcData.cmdRemove.path[pathLen] = '\0';
		fsCmdBlockBody->operationType = FSA_CMD_OPERATION_TYPE_REMOVE;
		return 0;
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
		uint32 ukn1444 = _swapEndianU32(fsClientBody->iosuFSAHandle);
		fsError = __FSPrepareCmd_Remove(fsCmdBlockBody, filePath);
		if (fsError != (FSStatus)FS_RESULT::SUCCESS)
		{
			cemu_assert_debug(false);
			return fsError;
		}
		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, FS_CB_PLACEHOLDER_FINISHCMD);
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSRemove(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint8* filePath, uint32 errorMask)
	{
		StackAllocator<FSAsyncParamsNew_t> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSRemoveAsync(fsClient, fsCmdBlock, filePath, errorMask, asyncParams);
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
	}

	uint32 __FSPrepareCmd_MakeDir(FSCmdBlockBody_t* fsCmdBlockBody, const uint8* path, uint32 uknVal660)
	{
		if (fsCmdBlockBody == NULL)
			return 0xFFFCFFDD;
		if (path == NULL)
			return 0xFFFCFFDE;
		size_t pathLen = strlen((char*)path);
		if (pathLen >= FSA_CMD_PATH_MAX_LENGTH)
		{
			cemu_assert_debug(false);
			pathLen = FSA_CMD_PATH_MAX_LENGTH - 1;
		}
		for (sint32 i = 0; i < pathLen; i++)
		{
			fsCmdBlockBody->ipcData.cmdMakeDir.path[i] = path[i];
		}
		fsCmdBlockBody->ipcData.cmdMakeDir.path[pathLen] = '\0';
		fsCmdBlockBody->ipcData.cmdMakeDir.uknParam = _swapEndianU32(uknVal660);
		fsCmdBlockBody->operationType = FSA_CMD_OPERATION_TYPE_MAKEDIR;
		return 0;
	}

	sint32 FSMakeDirAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, const uint8* dirPath, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		// used by titles: XCX (via SAVEMakeDirAsync)
		_FSCmdIntro();
		if (dirPath == NULL)
		{
			cemu_assert_debug(false); // path must not be NULL
			return -0x400;
		}
		uint32 ukn1444 = _swapEndianU32(fsClientBody->iosuFSAHandle);
		fsError = __FSPrepareCmd_MakeDir(fsCmdBlockBody, dirPath, 0x660);
		if (fsError != (FSStatus)FS_RESULT::SUCCESS)
		{
			cemu_assert_debug(false);
			return fsError;
		}
		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, FS_CB_PLACEHOLDER_FINISHCMD);
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSMakeDir(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, const uint8* path, uint32 errorMask)
	{
		StackAllocator<FSAsyncParamsNew_t> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSMakeDirAsync(fsClient, fsCmdBlock, path, errorMask, asyncParams);
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
	}

	sint32 __FSPrepareCmd_ChangeDir(FSCmdBlockBody_t* fsCmdBlockBody, uint8* path)
	{
		if (fsCmdBlockBody == NULL)
			return 0xFFFCFFDD;
		if (path == NULL)
			return 0xFFFCFFDE;
		size_t pathLen = strlen((char*)path);
		if (pathLen >= FSA_CMD_PATH_MAX_LENGTH)
		{
			cemu_assert_debug(false);
			pathLen = FSA_CMD_PATH_MAX_LENGTH - 1;
		}
		for (sint32 i = 0; i < pathLen; i++)
			fsCmdBlockBody->ipcData.cmdChangeDir.path[i] = path[i];
		fsCmdBlockBody->ipcData.cmdChangeDir.path[pathLen] = '\0';
		fsCmdBlockBody->operationType = FSA_CMD_OPERATION_TYPE_CHANGEDIR;
		return 0;
	}

	sint32 FSChangeDirAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		_FSCmdIntro();
		if (path == NULL)
		{
			cemu_assert_debug(false); // path must not be NULL
			return -0x400;
		}
		fsError = __FSPrepareCmd_ChangeDir(fsCmdBlockBody, (uint8*)path);
		if (fsError != (FSStatus)FS_RESULT::SUCCESS)
			return fsError;
		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, FS_CB_PLACEHOLDER_FINISHCMD);
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSChangeDir(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, uint32 errorMask)
	{
		StackAllocator<FSAsyncParamsNew_t> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSChangeDirAsync(fsClient, fsCmdBlock, path, errorMask, asyncParams);
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
	}

	sint32 __FSPrepareCmd_GetCwd(FSCmdBlockBody_t* fsCmdBlockBody)
	{
		if (fsCmdBlockBody == NULL)
			return 0xFFFCFFDD;
		fsCmdBlockBody->operationType = FSA_CMD_OPERATION_TYPE_GETCWD;
		return 0;
	}

	sint32 FSGetCwdAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* dirPathOut, sint32 dirPathMaxLen, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		// used by titles: Super Mario Maker
		_FSCmdIntro();
		fsCmdBlockBody->returnValueMPTR = _swapEndianU32(memory_getVirtualOffsetFromPointer(dirPathOut));
		fsCmdBlockBody->transferSize = _swapEndianU32(dirPathMaxLen);
		fsError = __FSPrepareCmd_GetCwd(fsCmdBlockBody);
		if (fsError != (FSStatus)FS_RESULT::SUCCESS)
			return fsError;
		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, FS_CB_PLACEHOLDER_FINISHCMD);
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

	void __FSPrepareCmd_FlushQuota(FSCmdBlockBody_t* fsCmdBlockBody, char* path)
	{
		size_t pathLen = strlen((char*)path);
		if (pathLen >= FSA_CMD_PATH_MAX_LENGTH)
		{
			cemu_assert_debug(false);
			pathLen = FSA_CMD_PATH_MAX_LENGTH - 1;
		}
		for (sint32 i = 0; i < pathLen; i++)
			fsCmdBlockBody->ipcData.cmdFlushQuota.path[i] = path[i];
		fsCmdBlockBody->ipcData.cmdFlushQuota.path[pathLen] = '\0';
		fsCmdBlockBody->operationType = FSA_CMD_OPERATION_TYPE_FLUSHQUOTA;
	}

	sint32 FSFlushQuotaAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		_FSCmdIntro();
		fsCmdBlockBody->returnValueMPTR = 0;
		__FSPrepareCmd_FlushQuota(fsCmdBlockBody, path);
		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, FS_CB_PLACEHOLDER_FINISHCMD);
		return (FSStatus)FS_RESULT::SUCCESS;
	}

	sint32 FSFlushQuota(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, char* path, uint32 errorMask)
	{
		StackAllocator<FSAsyncParamsNew_t> asyncParams;
		__FSAsyncToSyncInit(fsClient, fsCmdBlock, asyncParams);
		sint32 fsAsyncRet = FSFlushQuotaAsync(fsClient, fsCmdBlock, path, errorMask, asyncParams);
		return __FSProcessAsyncResult(fsClient, fsCmdBlock, fsAsyncRet, errorMask);
	}

	uint32 __FSPrepareCmd_QueryInfo(FSCmdBlockBody_t* fsCmdBlockBody, uint8* queryString, uint32 queryType)
	{
		// note: The output result is stored to fsCmdBlockBody+0x944
		size_t stringLen = strlen((char*)queryString);
		if (stringLen >= FSA_CMD_PATH_MAX_LENGTH)
		{
			cemu_assert_debug(false);
			stringLen = FSA_CMD_PATH_MAX_LENGTH - 1;
		}
		for (sint32 i = 0; i < stringLen; i++)
		{
			fsCmdBlockBody->ipcData.cmdQueryInfo.query[i] = queryString[i];
		}
		fsCmdBlockBody->ipcData.cmdQueryInfo.query[stringLen] = '\0';
		fsCmdBlockBody->ipcData.cmdQueryInfo.queryType = _swapEndianU32(queryType);
		fsCmdBlockBody->operationType = FSA_CMD_OPERATION_TYPE_QUERYINFO;
		return 0;
	}

	sint32 __FSQueryInfoAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint8* queryString, uint32 queryType, void* queryResult, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		_FSCmdIntro();
		cemu_assert(queryString && queryResult); // query string and result must not be null
		fsCmdBlockBody->returnValueMPTR = _swapEndianU32(memory_getVirtualOffsetFromPointer(queryResult));
		fsError = __FSPrepareCmd_QueryInfo(fsCmdBlockBody, queryString, queryType);
		if (fsError != (FSStatus)FS_RESULT::SUCCESS)
			return fsError;
		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, FS_CB_PLACEHOLDER_FINISHCMD);
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

	void __FSPrepareCmd_GetStatFile(FSCmdBlockBody_t* fsCmdBlockBody, FSFileHandle2 fileHandle, FSStat_t* statOut)
	{
		fsCmdBlockBody->ipcData.cmdGetStatFile.fileHandle = fileHandle;
		fsCmdBlockBody->operationType = FSA_CMD_OPERATION_TYPE_GETSTATFILE;
		fsCmdBlockBody->returnValueMPTR = _swapEndianU32(memory_getVirtualOffsetFromPointer(statOut));
	}

	sint32 FSGetStatFileAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, FSFileHandle2 fileHandle, FSStat_t* statOut, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		_FSCmdIntro();
		cemu_assert(statOut); // statOut must not be null
		__FSPrepareCmd_GetStatFile(fsCmdBlockBody, fileHandle, statOut);
		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, FS_CB_PLACEHOLDER_FINISHCMD);
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

	uint32 __FSPrepareCmd_IsEof(FSCmdBlockBody_t* fsCmdBlockBody, uint32 fileHandle)
	{
		fsCmdBlockBody->ipcData.cmdDefault.destBufferMPTR = _swapEndianU32(fileHandle);
		fsCmdBlockBody->operationType = FSA_CMD_OPERATION_TYPE_ISEOF;
		return 0;
	}

	sint32 FSIsEofAsync(FSClient_t* fsClient, FSCmdBlock_t* fsCmdBlock, uint32 fileHandle, uint32 errorMask, FSAsyncParamsNew_t* fsAsyncParams)
	{
		// used by Paper Monsters Recut
		_FSCmdIntro();
		__FSPrepareCmd_IsEof(fsCmdBlockBody, fileHandle);
		__FSQueueCmd(&fsClientBody->fsCmdQueue, fsCmdBlockBody, FS_CB_PLACEHOLDER_FINISHCMD);
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

	void InitializeFS()
	{
		cafeExportRegister("coreinit", FSInit, LogType::File);
		cafeExportRegister("coreinit", FSShutdown, LogType::File);

		cafeExportRegister("coreinit", FSGetMountSource, LogType::File);
		cafeExportRegister("coreinit", FSGetMountSourceNext, LogType::File);

		cafeExportRegister("coreinit", FSMount, LogType::File);
		cafeExportRegister("coreinit", FSBindMount, LogType::File);

		// client management
		cafeExportRegister("coreinit", FSAddClientEx, LogType::File);
		cafeExportRegister("coreinit", FSAddClient, LogType::File);
		cafeExportRegister("coreinit", FSDelClient, LogType::File);
		cafeExportRegister("coreinit", FSGetClientNum, LogType::File);

		// cmd
		cafeExportRegister("coreinit", FSInitCmdBlock, LogType::File);
		cafeExportRegister("coreinit", FSGetAsyncResult, LogType::File);

		// file operations
		cafeExportRegister("coreinit", FSOpenFileAsync, LogType::File);
		cafeExportRegister("coreinit", FSOpenFile, LogType::File);
		cafeExportRegister("coreinit", FSOpenFileExAsync, LogType::File);
		cafeExportRegister("coreinit", FSOpenFileEx, LogType::File);
		cafeExportRegister("coreinit", FSCloseFileAsync, LogType::File);
		cafeExportRegister("coreinit", FSCloseFile, LogType::File);

		cafeExportRegister("coreinit", FSReadFileAsync, LogType::File);
		cafeExportRegister("coreinit", FSReadFile, LogType::File);
		cafeExportRegister("coreinit", FSReadFileWithPosAsync, LogType::File);
		cafeExportRegister("coreinit", FSReadFileWithPos, LogType::File);

		cafeExportRegister("coreinit", FSWriteFileAsync, LogType::File);
		cafeExportRegister("coreinit", FSWriteFile, LogType::File);
		cafeExportRegister("coreinit", FSWriteFileWithPosAsync, LogType::File);
		cafeExportRegister("coreinit", FSWriteFileWithPos, LogType::File);

		cafeExportRegister("coreinit", FSSetPosFileAsync, LogType::File);
		cafeExportRegister("coreinit", FSSetPosFile, LogType::File);
		cafeExportRegister("coreinit", FSGetPosFileAsync, LogType::File);
		cafeExportRegister("coreinit", FSGetPosFile, LogType::File);

		cafeExportRegister("coreinit", FSAppendFileAsync, LogType::File);
		cafeExportRegister("coreinit", FSAppendFile, LogType::File);

		cafeExportRegister("coreinit", FSTruncateFileAsync, LogType::File);
		cafeExportRegister("coreinit", FSTruncateFile, LogType::File);

		cafeExportRegister("coreinit", FSRenameAsync, LogType::File);
		cafeExportRegister("coreinit", FSRename, LogType::File);
		cafeExportRegister("coreinit", FSRemoveAsync, LogType::File);
		cafeExportRegister("coreinit", FSRemove, LogType::File);
		cafeExportRegister("coreinit", FSMakeDirAsync, LogType::File);
		cafeExportRegister("coreinit", FSMakeDir, LogType::File);
		cafeExportRegister("coreinit", FSChangeDirAsync, LogType::File);
		cafeExportRegister("coreinit", FSChangeDir, LogType::File);
		cafeExportRegister("coreinit", FSGetCwdAsync, LogType::File);
		cafeExportRegister("coreinit", FSGetCwd, LogType::File);		

		cafeExportRegister("coreinit", FSIsEofAsync, LogType::File);
		cafeExportRegister("coreinit", FSIsEof, LogType::File);

		// directory operations
		cafeExportRegister("coreinit", FSOpenDirAsync, LogType::File);
		cafeExportRegister("coreinit", FSOpenDir, LogType::File);
		cafeExportRegister("coreinit", FSReadDirAsync, LogType::File);
		cafeExportRegister("coreinit", FSReadDir, LogType::File);
		cafeExportRegister("coreinit", FSCloseDirAsync, LogType::File);
		cafeExportRegister("coreinit", FSCloseDir, LogType::File);
		
		// stat
		cafeExportRegister("coreinit", FSGetFreeSpaceSizeAsync, LogType::File);
		cafeExportRegister("coreinit", FSGetFreeSpaceSize, LogType::File);

		cafeExportRegister("coreinit", FSGetStatAsync, LogType::File);
		cafeExportRegister("coreinit", FSGetStat, LogType::File);

		cafeExportRegister("coreinit", FSGetStatFileAsync, LogType::File);
		cafeExportRegister("coreinit", FSGetStatFile, LogType::File);

		// misc
		cafeExportRegister("coreinit", FSFlushQuotaAsync, LogType::File);
		cafeExportRegister("coreinit", FSFlushQuota, LogType::File);

		cafeExportRegister("coreinit", FSSetUserData, LogType::File);
		cafeExportRegister("coreinit", FSGetUserData, LogType::File);

		cafeExportRegister("coreinit", FSGetCurrentCmdBlock, LogType::File);

		cafeExportRegister("coreinit", FSGetVolumeState, LogType::File);
		cafeExportRegister("coreinit", FSGetErrorCodeForViewer, LogType::Placeholder);
		cafeExportRegister("coreinit", FSGetLastErrorCodeForViewer, LogType::Placeholder);

		g_fsRegisteredClientBodies = nullptr;
	}
}
