#include "Cafe/OS/common/OSCommon.h"
#include "gui/wxgui.h"
#include "nn_save.h"

#include "Cafe/OS/libs/nn_acp/nn_acp.h"
#include "Cafe/OS/libs/nn_act/nn_act.h"

#include <filesystem>
#include <sstream>
#include "config/ActiveSettings.h"
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"
#include "Cafe/OS/libs/coreinit/coreinit_FS.h"
#include "Cafe/CafeSystem.h"
#include "Cafe/Filesystem/fsc.h"

#define SAVE_STATUS_OK ((FSStatus)FS_RESULT::SUCCESS)
#define SAVE_MAX_PATH_SIZE (FSA_CMD_PATH_MAX_LENGTH)

#define SAVE_ACCOUNT_ID_MIN (1)
#define SAVE_ACCOUNT_ID_MAX (0xC)

#define SAVE_UNIQUE_TO_TITLE_ID(_unique_) (((((uint64)_unique_ >> 24ULL) | 0x50000) << 32ULL) | ((_unique_ << 8) | 0x10000000))
#define SAVE_UNIQUE_TO_TITLE_ID_VARIATION(_unique_,_variation_) (((((uint64)_unique_ >> 24ULL) | 0x50000 ) << 32) | ((_unique_ << 8) | 0x10000000 | _variation_))
#define SAVE_UNIQUE_DEMO_TO_TITLE_ID(_unique_) (((((uint64)_unique_ >> 24ULL) | 0x50002) << 32ULL) | ((_unique_ << 8) | 0x10000000))
#define SAVE_UNIQUE_DEMO_TO_TITLE_ID_VARIATION(_unique_,_variation_) (((((uint64)_unique_ >> 24ULL) | 0x50002 ) << 32ULL) | ((_unique_ << 8) | 0x10000000 | _variation_))

namespace nn
{
namespace save
{
	typedef FSStatus SAVEStatus;

	typedef struct
	{
		bool initialized;
		coreinit::OSMutex mutex;
		coreinit::FSClient_t fsClient;
		coreinit::FSCmdBlock_t fsCmdBlock;
		uint32 persistentIdCache[0xC];
	}nn_save_t;

	SysAllocator<nn_save_t> g_nn_save;

	uint32 GetPersistentIdFromLocalCache(uint8 accountSlot)
	{
		accountSlot--;
		if (accountSlot >= 0xC)
			return 0;

		return g_nn_save->persistentIdCache[accountSlot];
	}

	void SetPersistentIdToLocalCache(uint8 accountSlot, uint32 persistentId)
	{
		accountSlot--;
		if (accountSlot >= 0xC)
			return;

		g_nn_save->persistentIdCache[accountSlot] = persistentId;
	}

	bool GetPersistentIdEx(uint8 accountSlot, uint32* persistentId)
	{
		if (accountSlot == 0xFF)
		{
			*persistentId = 0;
			return true;
		}

		const uint32 result = GetPersistentIdFromLocalCache(accountSlot);
		*persistentId = result;
		return result != 0;
	}

	bool GetCurrentTitleApplicationBox(acp::ACPDeviceType* deviceType)
	{
		if (deviceType)
		{
			*deviceType = acp::InternalDeviceType;
			return true;
		}
		return false;
	}

	void UpdateSaveTimeStamp(uint32 persistentId)
	{
		acp::ACPDeviceType deviceType;
		if (GetCurrentTitleApplicationBox(&deviceType))
			ACPUpdateSaveTimeStamp(persistentId, CafeSystem::GetForegroundTitleId(), deviceType);
	}

	SAVEStatus ConvertACPToSaveStatus(acp::ACPStatus status)
	{
		cemu_assert_debug(status == 0); // todo
		return 0;
	}

	bool GetAbsoluteFullPath(uint32 persistentId, const char* subDir, char* outPath)
	{
		int size;
		if (persistentId != 0)
		{
			if (subDir)
				size = snprintf(outPath, SAVE_MAX_PATH_SIZE - 1, "/vol/save/%08x/%s", persistentId, subDir);
			else
				size = snprintf(outPath, SAVE_MAX_PATH_SIZE - 1, "/vol/save/%08x/", persistentId);
		}
		else
		{
			if (subDir)
				size = snprintf(outPath, SAVE_MAX_PATH_SIZE - 1, "/vol/save/common/%s", subDir);
			else
				size = snprintf(outPath, SAVE_MAX_PATH_SIZE - 1, "/vol/save/common/");
		}

		if (size < SAVE_MAX_PATH_SIZE - 1)
			return true;
		return false;
	}

	SAVEStatus GetAbsoluteFullPathOtherApplication(uint32 persistentId, uint64 titleId, const char* subDir, char* outPath)
	{
		uint32be applicationBox;
		if(acp::ACPGetApplicationBox(&applicationBox, titleId) != acp::ACPStatus::SUCCESS)
			return (FSStatus)FS_RESULT::NOT_FOUND;

		sint32 written = 0;
		if(applicationBox == 3)	
		{
			if(persistentId != 0)
			{
				if (subDir)
					written = snprintf(outPath, SAVE_MAX_PATH_SIZE - 1, "/vol/storage_mlc01/usr/save/%08x/%08x/user/%08x/%s",
						GetTitleIdHigh(titleId), GetTitleIdLow(titleId), persistentId, subDir);
				else
					written = snprintf(outPath, SAVE_MAX_PATH_SIZE - 1, "/vol/storage_mlc01/usr/save/%08x/%08x/user/%08x/",
						GetTitleIdHigh(titleId), GetTitleIdLow(titleId), persistentId);
			}	
			else
			{
				if (subDir)
					written = snprintf(outPath, SAVE_MAX_PATH_SIZE - 1, "/vol/storage_mlc01/usr/save/%08x/%08x/user/common/%s",
						GetTitleIdHigh(titleId), GetTitleIdLow(titleId), subDir);
				else
					written = snprintf(outPath, SAVE_MAX_PATH_SIZE - 1, "/vol/storage_mlc01/usr/save/%08x/%08x/user/common/",
						GetTitleIdHigh(titleId), GetTitleIdLow(titleId));
			}
		}
		else if(applicationBox == 4)
		{
			cemu_assert_unimplemented();
		}
		else 
			return (FSStatus)FS_RESULT::NOT_FOUND;

		if (written < SAVE_MAX_PATH_SIZE - 1)
			return (FSStatus)FS_RESULT::SUCCESS;

		cemu_assert_suspicious();
		return (FSStatus)(FS_RESULT::FATAL_ERROR);
	}

	typedef struct
	{
		coreinit::OSEvent* event;
		SAVEStatus returnStatus;

		MEMPTR<OSThread_t> thread; // own stuff until cond + event rewritten
	} AsyncCallbackParam_t;

	void AsyncCallback(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint32 returnStatus, void* p)
	{
		cemu_assert_debug(p && ((AsyncCallbackParam_t*)p)->event);

		AsyncCallbackParam_t* param = (AsyncCallbackParam_t*)p;
		param->returnStatus = returnStatus;
		coreinit::OSSignalEvent(param->event);
	}

	void AsyncCallback(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(client, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(block, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU32(returnStatus, 2);
		ppcDefineParamMEMPTR(userContext, void, 3);

		MEMPTR<AsyncCallbackParam_t> param{ userContext };

		// wait till thread is actually suspended
		OSThread_t* thread = param->thread.GetPtr();
		while (thread->suspendCounter == 0 || thread->state == OSThread_t::THREAD_STATE::STATE_RUNNING)
			coreinit::OSYieldThread();

		param->returnStatus = returnStatus;
		coreinit_resumeThread(param->thread.GetPtr(), 1000);

		osLib_returnFromFunction(hCPU, 0);
	}

	SAVEStatus SAVEMountSaveDir()
	{
		acp::ACPStatus status = acp::ACPMountSaveDir();
		return ConvertACPToSaveStatus(status);
	}

    SAVEStatus SAVEUnmountSaveDir()
    {
        return ConvertACPToSaveStatus(acp::ACPUnmountSaveDir());
    }

	void _CheckAndMoveLegacySaves()
	{
		const uint64 titleId = CafeSystem::GetForegroundTitleId();

		fs::path targetPath, sourcePath;
		try
		{
			bool copiedUser = false, copiedCommon = false;

			const auto sourceSavePath = ActiveSettings::GetMlcPath("emulatorSave/{:08x}", CafeSystem::GetRPXHashBase());
			sourcePath = sourceSavePath;

			if (fs::exists(sourceSavePath) && is_directory(sourceSavePath))
			{
				targetPath = ActiveSettings::GetMlcPath("usr/save/{:08x}/{:08x}/user/{:08x}", GetTitleIdHigh(titleId), GetTitleIdLow(titleId), 0x80000001);
				fs::create_directories(targetPath);
				copy(sourceSavePath, targetPath, fs::copy_options::overwrite_existing | fs::copy_options::recursive);
				copiedUser = true;
			}

			const auto sourceCommonPath = ActiveSettings::GetMlcPath("emulatorSave/{:08x}_255", CafeSystem::GetRPXHashBase());
			sourcePath = sourceCommonPath;

			if (fs::exists(sourceCommonPath) && is_directory(sourceCommonPath))
			{
				targetPath = ActiveSettings::GetMlcPath("usr/save/{:08x}/{:08x}/user/common", GetTitleIdHigh(titleId), GetTitleIdLow(titleId));
				fs::create_directories(targetPath);
				copy(sourceCommonPath, targetPath, fs::copy_options::overwrite_existing | fs::copy_options::recursive);
				copiedCommon = true;
			}

			if (copiedUser)
				fs::remove_all(sourceSavePath);

			if (copiedCommon)
				fs::remove_all(sourceCommonPath);
		}
		catch (const std::exception& ex)
		{
#if BOOST_OS_WINDOWS
			std::wstringstream errorMsg;
			errorMsg << L"Couldn't move your save files!" << std::endl << std::endl;
			errorMsg << L"Error: " << ex.what() << std::endl << std::endl;
			errorMsg << L"From:" << std::endl << sourcePath << std::endl << std::endl << "To:" << std::endl << targetPath;

			const DWORD lastError = GetLastError();
			if (lastError != ERROR_SUCCESS)
			{
				LPTSTR lpMsgBuf = nullptr;
				FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, 0, (LPTSTR)&lpMsgBuf, 0, nullptr);
				if (lpMsgBuf)
				{
					errorMsg << std::endl << std::endl << L"Details: " << lpMsgBuf;
					LocalFree(lpMsgBuf);
				}
				else
				{
					errorMsg << std::endl << std::endl << L"Error Code: 0x" << std::hex << lastError;
				}
			}

			errorMsg << std::endl << std::endl << "Continuing will create a new save at the target location." << std::endl << "Do you want to continue?";

			int result = wxMessageBox(errorMsg.str(), "Save Migration - Error", wxCENTRE | wxYES_NO | wxICON_ERROR);
			if (result != wxYES)
			{
				exit(0);
				return;
			}
#endif
		}
	}

	SAVEStatus SAVEInit()
	{
		const uint64 titleId = CafeSystem::GetForegroundTitleId();

		if (!g_nn_save->initialized)
		{
			OSInitMutexEx(&g_nn_save->mutex, nullptr);
			act::Initialize();
			coreinit::FSAddClientEx(&g_nn_save->fsClient, 0, 0);
			coreinit::FSInitCmdBlock(&g_nn_save->fsCmdBlock);
			for(uint8 accountId = SAVE_ACCOUNT_ID_MIN; accountId <= SAVE_ACCOUNT_ID_MAX; ++accountId)
			{
				uint32 persistentId = act::GetPersistentIdEx(accountId);
				SetPersistentIdToLocalCache(accountId, persistentId);
			}
			
			SAVEMountSaveDir();
			g_nn_save->initialized = true;

			_CheckAndMoveLegacySaves();

			uint32 high = GetTitleIdHigh(titleId) & (~0xC);
			uint32 low = GetTitleIdLow(titleId);

			sint32 fscStatus = FSC_STATUS_FILE_NOT_FOUND;
			char path[256];
			sprintf(path, "%susr/save/%08x/", "/vol/storage_mlc01/", high);
			fsc_createDir(path, &fscStatus);
			sprintf(path, "%susr/save/%08x/%08x/", "/vol/storage_mlc01/", high, low);
			fsc_createDir(path, &fscStatus);
			sprintf(path, "%susr/save/%08x/%08x/meta/", "/vol/storage_mlc01/", high, low);
			fsc_createDir(path, &fscStatus);

			acp::CreateSaveMetaFiles(ActiveSettings::GetPersistentId(), titleId);
		}

		return SAVE_STATUS_OK;
	}

	SAVEStatus SAVERemoveAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, const char* path, FS_ERROR_MASK errHandling, const FSAsyncParams_t* asyncParams)
	{
		SAVEStatus result = (FSStatus)(FS_RESULT::FATAL_ERROR);

		OSLockMutex(&g_nn_save->mutex);

		uint32 persistentId;
		if (GetPersistentIdEx(accountSlot, &persistentId))
		{
			char fullPath[SAVE_MAX_PATH_SIZE];
			if (GetAbsoluteFullPath(persistentId, path, fullPath))
				result = coreinit::FSRemoveAsync(client, block, (uint8*)fullPath, errHandling, (FSAsyncParamsNew_t*)asyncParams);
		}
		else
			result = (FSStatus)FS_RESULT::NOT_FOUND;

		OSUnlockMutex(&g_nn_save->mutex);
		return result;
	}

	SAVEStatus SAVEMakeDirAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, const char* path, FS_ERROR_MASK errHandling, const FSAsyncParams_t* asyncParams)
	{
		SAVEStatus result = (FSStatus)(FS_RESULT::FATAL_ERROR);

		OSLockMutex(&g_nn_save->mutex);

		uint32 persistentId;
		if (GetPersistentIdEx(accountSlot, &persistentId))
		{
			char fullPath[SAVE_MAX_PATH_SIZE];
			if (GetAbsoluteFullPath(persistentId, path, fullPath))
				result = coreinit::FSMakeDirAsync(client, block, fullPath, errHandling, (FSAsyncParamsNew_t*)asyncParams);
		}
		else
			result = (FSStatus)FS_RESULT::NOT_FOUND;

		OSUnlockMutex(&g_nn_save->mutex);

		return result;
	}

	SAVEStatus SAVEOpenDirAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, const char* path, FSDirHandlePtr hDir, FS_ERROR_MASK errHandling, const FSAsyncParams_t* asyncParams)
	{
		SAVEStatus result = (FSStatus)(FS_RESULT::FATAL_ERROR);

		OSLockMutex(&g_nn_save->mutex);

		uint32 persistentId;
		if (GetPersistentIdEx(accountSlot, &persistentId))
		{
			char fullPath[SAVE_MAX_PATH_SIZE];
			if (GetAbsoluteFullPath(persistentId, path, fullPath))
				result = coreinit::FSOpenDirAsync(client, block, fullPath, hDir, errHandling, (FSAsyncParamsNew_t*)asyncParams);
			
		}
		else
			result = (FSStatus)FS_RESULT::NOT_FOUND;

		OSUnlockMutex(&g_nn_save->mutex);

		return result;
	}

	SAVEStatus SAVEOpenFileAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, const char* path,  const char* mode, FSFileHandleDepr_t* hFile, FS_ERROR_MASK errHandling, const FSAsyncParamsNew_t* asyncParams)
	{
		SAVEStatus result = (FSStatus)(FS_RESULT::FATAL_ERROR);

		OSLockMutex(&g_nn_save->mutex);

		uint32 persistentId;
		if (GetPersistentIdEx(accountSlot, &persistentId))
		{
			char fullPath[SAVE_MAX_PATH_SIZE];
			if (GetAbsoluteFullPath(persistentId, path, fullPath))
				result = coreinit::FSOpenFileAsync(client, block, fullPath, (char*)mode, hFile, errHandling, (FSAsyncParamsNew_t*)asyncParams);
		}
		else
			result = (FSStatus)FS_RESULT::NOT_FOUND;

		OSUnlockMutex(&g_nn_save->mutex);

		return result;
	}

	SAVEStatus SAVEOpenFileOtherApplicationAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint64 titleId, uint8 accountSlot, const char* path, const char* mode, FSFileHandleDepr_t* hFile, FS_ERROR_MASK errHandling, const FSAsyncParamsNew_t* asyncParams)
	{
		if (strcmp(mode, "r") != 0)
			return (SAVEStatus)(FS_RESULT::PERMISSION_ERROR);

		SAVEStatus result = (FSStatus)(FS_RESULT::FATAL_ERROR);

		OSLockMutex(&g_nn_save->mutex);

		uint32 persistentId;
		if (GetPersistentIdEx(accountSlot, &persistentId))
		{
			char fullPath[SAVE_MAX_PATH_SIZE];
			if (GetAbsoluteFullPathOtherApplication(persistentId, titleId, path, fullPath))
				result = coreinit::FSOpenFileAsync(client, block, fullPath, (char*)mode, hFile, errHandling, (FSAsyncParamsNew_t*)asyncParams);
		}
		else
			result = (FSStatus)FS_RESULT::NOT_FOUND;

		OSUnlockMutex(&g_nn_save->mutex);

		return result;
	}

	void export_SAVEOpenFileOtherApplicationAsync(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU64(titleId, 2);
		ppcDefineParamU8(accountSlot, 4);
		ppcDefineParamMEMPTR(path, const char, 5);
		ppcDefineParamMEMPTR(mode, const char, 6);
		ppcDefineParamMEMPTR(hFile, FSFileHandleDepr_t, 7);
		ppcDefineParamU32(errHandling, 8);
		ppcDefineParamMEMPTR(asyncParams, FSAsyncParamsNew_t, 9);

		const SAVEStatus result = SAVEOpenFileOtherApplicationAsync(fsClient.GetPtr(), fsCmdBlock.GetPtr(), titleId, accountSlot, path.GetPtr(), mode.GetPtr(), hFile.GetPtr(), errHandling, asyncParams.GetPtr());
		osLib_returnFromFunction(hCPU, result);
	}

	SAVEStatus SAVEOpenFileOtherApplication(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint64 titleId, uint8 accountSlot, const char* path, const char* mode, FSFileHandleDepr_t* hFile, FS_ERROR_MASK errHandling)
	{
		FSAsyncParamsNew_t asyncParams;
		asyncParams.ioMsgQueue = nullptr;
		asyncParams.userCallback = PPCInterpreter_makeCallableExportDepr(AsyncCallback);

		StackAllocator<AsyncCallbackParam_t> param;
		param->thread = coreinitThread_getCurrentThreadMPTRDepr(ppcInterpreterCurrentInstance);
		param->returnStatus = (FSStatus)FS_RESULT::SUCCESS;
		asyncParams.userContext = param.GetPointer();

		SAVEStatus status = SAVEOpenFileOtherApplicationAsync(client, block, titleId, accountSlot, path, mode, hFile, errHandling, &asyncParams);
		if (status == (FSStatus)FS_RESULT::SUCCESS)
		{
			coreinit_suspendThread(coreinitThread_getCurrentThreadDepr(ppcInterpreterCurrentInstance), 1000);
			PPCCore_switchToScheduler();
			return param->returnStatus;
		}

		return status;
	}

	void export_SAVEOpenFileOtherApplication(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU64(titleId, 2);
		ppcDefineParamU8(accountSlot, 4);
		ppcDefineParamMEMPTR(path, const char, 5);
		ppcDefineParamMEMPTR(mode, const char, 6);
		ppcDefineParamMEMPTR(hFile, FSFileHandleDepr_t, 7);
		ppcDefineParamU32(errHandling, 8);

		const SAVEStatus result = SAVEOpenFileOtherApplication(fsClient.GetPtr(), fsCmdBlock.GetPtr(), titleId, accountSlot, path.GetPtr(), mode.GetPtr(), hFile.GetPtr(), errHandling);
		osLib_returnFromFunction(hCPU, result);
	}

	SAVEStatus SAVEOpenFileOtherNormalApplicationAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint32 uniqueId, uint8 accountSlot, const char* path, const char* mode, FSFileHandleDepr_t* hFile, FS_ERROR_MASK errHandling, const FSAsyncParamsNew_t* asyncParams)
	{
		uint64 titleId = SAVE_UNIQUE_TO_TITLE_ID(uniqueId);
		return SAVEOpenFileOtherApplicationAsync(client, block, titleId, accountSlot, path, mode, hFile, errHandling, asyncParams);
	}

	void export_SAVEOpenFileOtherNormalApplicationAsync(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU32(uniqueId, 2);
		ppcDefineParamU8(accountSlot, 3);
		ppcDefineParamMEMPTR(path, const char, 4);
		ppcDefineParamMEMPTR(mode, const char, 5);
		ppcDefineParamMEMPTR(hFile, FSFileHandleDepr_t, 6);
		ppcDefineParamU32(errHandling, 7);
		ppcDefineParamMEMPTR(asyncParams, FSAsyncParamsNew_t, 8);

		const SAVEStatus result = SAVEOpenFileOtherNormalApplicationAsync(fsClient.GetPtr(), fsCmdBlock.GetPtr(), uniqueId, accountSlot, path.GetPtr(), mode.GetPtr(), hFile.GetPtr(), errHandling, asyncParams.GetPtr());
		osLib_returnFromFunction(hCPU, result);
	}
	SAVEStatus SAVEOpenFileOtherNormalApplication(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint32 uniqueId, uint8 accountSlot, const char* path, const char* mode, FSFileHandleDepr_t* hFile, FS_ERROR_MASK errHandling)
	{
		//peterBreak();

		uint64 titleId = SAVE_UNIQUE_TO_TITLE_ID(uniqueId);
		return SAVEOpenFileOtherApplication(client, block, titleId, accountSlot, path, mode, hFile, errHandling);
	}

	void export_SAVEOpenFileOtherNormalApplication(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU32(uniqueId, 2);
		ppcDefineParamU8(accountSlot, 3);
		ppcDefineParamMEMPTR(path, const char, 4);
		ppcDefineParamMEMPTR(mode, const char, 5);
		ppcDefineParamMEMPTR(hFile, FSFileHandleDepr_t, 6);
		ppcDefineParamU32(errHandling, 7);

		const SAVEStatus result = SAVEOpenFileOtherNormalApplication(fsClient.GetPtr(), fsCmdBlock.GetPtr(), uniqueId, accountSlot, path.GetPtr(), mode.GetPtr(), hFile.GetPtr(), errHandling);
		osLib_returnFromFunction(hCPU, result);
	}

	SAVEStatus SAVEOpenFileOtherNormalApplicationVariationAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint32 uniqueId, uint8 variation, uint8 accountSlot, const char* path, const char* mode, FSFileHandleDepr_t* hFile, FS_ERROR_MASK errHandling, const FSAsyncParamsNew_t* asyncParams)
	{
		//peterBreak();

		uint64 titleId = SAVE_UNIQUE_TO_TITLE_ID_VARIATION(uniqueId, variation);
		return SAVEOpenFileOtherApplicationAsync(client, block, titleId, accountSlot, path, mode, hFile, errHandling, asyncParams);
	}

	void export_SAVEOpenFileOtherNormalApplicationVariationAsync(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU32(uniqueId, 2);
		ppcDefineParamU8(variation, 3);
		ppcDefineParamU8(accountSlot, 4);
		ppcDefineParamMEMPTR(path, const char, 5);
		ppcDefineParamMEMPTR(mode, const char, 6);
		ppcDefineParamMEMPTR(hFile, FSFileHandleDepr_t, 7);
		ppcDefineParamU32(errHandling, 8);
		ppcDefineParamMEMPTR(asyncParams, FSAsyncParamsNew_t, 9);

		const SAVEStatus result = SAVEOpenFileOtherNormalApplicationVariationAsync(fsClient.GetPtr(), fsCmdBlock.GetPtr(), uniqueId, variation, accountSlot, path.GetPtr(), mode.GetPtr(), hFile.GetPtr(), errHandling, asyncParams.GetPtr());
		osLib_returnFromFunction(hCPU, result);
	}

	SAVEStatus SAVEOpenFileOtherNormalApplicationVariation(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint32 uniqueId, uint8 variation, uint8 accountSlot, const char* path, const char* mode, FSFileHandleDepr_t* hFile, FS_ERROR_MASK errHandling)
	{
		uint64 titleId = SAVE_UNIQUE_TO_TITLE_ID_VARIATION(uniqueId, variation);
		return SAVEOpenFileOtherApplication(client, block, titleId, accountSlot, path, mode, hFile, errHandling);
	}

	void export_SAVEOpenFileOtherNormalApplicationVariation(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU32(uniqueId, 2);
		ppcDefineParamU8(variation, 3);
		ppcDefineParamU8(accountSlot, 4);
		ppcDefineParamMEMPTR(path, const char, 5);
		ppcDefineParamMEMPTR(mode, const char, 6);
		ppcDefineParamMEMPTR(hFile, FSFileHandleDepr_t, 7);
		ppcDefineParamU32(errHandling, 8);

		const SAVEStatus result = SAVEOpenFileOtherNormalApplicationVariation(fsClient.GetPtr(), fsCmdBlock.GetPtr(), uniqueId, variation, accountSlot, path.GetPtr(), mode.GetPtr(), hFile.GetPtr(), errHandling);
		osLib_returnFromFunction(hCPU, result);
	}

	SAVEStatus SAVEGetFreeSpaceSizeAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, FSLargeSize* freeSize, FS_ERROR_MASK errHandling, const FSAsyncParams_t* asyncParams)
	{
		SAVEStatus result = (FSStatus)(FS_RESULT::FATAL_ERROR);

		OSLockMutex(&g_nn_save->mutex);

		uint32 persistentId;
		if (GetPersistentIdEx(accountSlot, &persistentId))
		{
			char fullPath[SAVE_MAX_PATH_SIZE];
			// usually a pointer with '\0' instead of nullptr, but it's basically the same
			if (GetAbsoluteFullPath(persistentId, nullptr, fullPath))
				result = coreinit::FSGetFreeSpaceSizeAsync(client, block, fullPath, freeSize, errHandling, (FSAsyncParamsNew_t*)asyncParams);
		}
		else
			result = (FSStatus)FS_RESULT::NOT_FOUND;

		OSUnlockMutex(&g_nn_save->mutex);

		return result;
	}

	SAVEStatus SAVEGetStatAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, const char* path, FSStat_t* stat, FS_ERROR_MASK errHandling, const FSAsyncParams_t* asyncParams)
	{
		SAVEStatus result = (FSStatus)(FS_RESULT::FATAL_ERROR);

		OSLockMutex(&g_nn_save->mutex);

		uint32 persistentId;
		if (GetPersistentIdEx(accountSlot, &persistentId))
		{
			char fullPath[SAVE_MAX_PATH_SIZE];
			if (GetAbsoluteFullPath(persistentId, path, fullPath))
				result = coreinit::__FSQueryInfoAsync(client, block, (uint8*)fullPath, FSA_QUERY_TYPE_STAT, stat, errHandling, (FSAsyncParamsNew_t*)asyncParams); // FSGetStatAsync(...)
		}
		else
			result = (FSStatus)FS_RESULT::NOT_FOUND;

		OSUnlockMutex(&g_nn_save->mutex);

		return result;
	}

	SAVEStatus SAVEGetStatOtherApplicationAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint64 titleId, uint8 accountSlot, const char* path, FSStat_t* stat, FS_ERROR_MASK errHandling, const FSAsyncParams_t* asyncParams)
	{
		SAVEStatus result = (FSStatus)(FS_RESULT::FATAL_ERROR);

		OSLockMutex(&g_nn_save->mutex);

		uint32 persistentId;
		if (GetPersistentIdEx(accountSlot, &persistentId))
		{
			char fullPath[SAVE_MAX_PATH_SIZE];
			if (GetAbsoluteFullPathOtherApplication(persistentId, titleId, path, fullPath) == (FSStatus)FS_RESULT::SUCCESS)
				result = coreinit::__FSQueryInfoAsync(client, block, (uint8*)fullPath, FSA_QUERY_TYPE_STAT, stat, errHandling, (FSAsyncParamsNew_t*)asyncParams); // FSGetStatAsync(...)
		}
		else
			result = (FSStatus)FS_RESULT::NOT_FOUND;

		OSUnlockMutex(&g_nn_save->mutex);

		return result;
	}

	SAVEStatus SAVEGetStatOtherNormalApplicationAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint32 uniqueId, uint8 accountSlot, const char* path, FSStat_t* stat, FS_ERROR_MASK errHandling, const FSAsyncParams_t* asyncParams)
	{
		uint64 titleId = SAVE_UNIQUE_TO_TITLE_ID(uniqueId);
		return SAVEGetStatOtherApplicationAsync(client, block, titleId, accountSlot, path, stat, errHandling, asyncParams);
	}

	SAVEStatus SAVEGetStatOtherDemoApplicationAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint32 uniqueId, uint8 accountSlot, const char* path, FSStat_t* stat, FS_ERROR_MASK errHandling, const FSAsyncParams_t* asyncParams)
	{
		uint64 titleId = SAVE_UNIQUE_DEMO_TO_TITLE_ID(uniqueId);
		return SAVEGetStatOtherApplicationAsync(client, block, titleId, accountSlot, path, stat, errHandling, asyncParams);
	}

	SAVEStatus SAVEGetStatOtherNormalApplicationVariationAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint32 uniqueId, uint8 variation, uint8 accountSlot, const char* path, FSStat_t* stat, FS_ERROR_MASK errHandling, const FSAsyncParams_t* asyncParams)
	{
		uint64 titleId = SAVE_UNIQUE_TO_TITLE_ID_VARIATION(uniqueId, variation);
		return SAVEGetStatOtherApplicationAsync(client, block, titleId, accountSlot, path, stat, errHandling, asyncParams);
	}

	SAVEStatus SAVEGetStatOtherDemoApplicationVariationAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint32 uniqueId, uint8 variation, uint8 accountSlot, const char* path, FSStat_t* stat, FS_ERROR_MASK errHandling, const FSAsyncParams_t* asyncParams)
	{
		uint64 titleId = SAVE_UNIQUE_DEMO_TO_TITLE_ID_VARIATION(uniqueId, variation);
		return SAVEGetStatOtherApplicationAsync(client, block, titleId, accountSlot, path, stat, errHandling, asyncParams);
	}

	SAVEStatus SAVEInitSaveDir(uint8 accountSlot)
	{
		SAVEStatus result = (FSStatus)(FS_RESULT::FATAL_ERROR);
		OSLockMutex(&g_nn_save->mutex);

		uint32 persistentId;
		if (GetPersistentIdEx(accountSlot, &persistentId))
		{
			acp::ACPStatus status = ACPCreateSaveDir(persistentId, acp::InternalDeviceType);
			result = ConvertACPToSaveStatus(status);
		}
		else
			result = (FSStatus)FS_RESULT::NOT_FOUND;

		OSUnlockMutex(&g_nn_save->mutex);
		return result;
	}

	SAVEStatus SAVEGetFreeSpaceSize(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, FSLargeSize* freeSize, FS_ERROR_MASK errHandling)
	{
		FSAsyncParams_t asyncParams;
		asyncParams.ioMsgQueue = MPTR_NULL;
		asyncParams.userCallback = _swapEndianU32(PPCInterpreter_makeCallableExportDepr(AsyncCallback));

		StackAllocator<AsyncCallbackParam_t> param;
		param->thread = coreinitThread_getCurrentThreadMPTRDepr(ppcInterpreterCurrentInstance);
		param->returnStatus = (FSStatus)FS_RESULT::SUCCESS;
		asyncParams.userContext = param.GetMPTRBE();

		SAVEStatus status = SAVEGetFreeSpaceSizeAsync(client, block, accountSlot, freeSize, errHandling, &asyncParams);
		if (status == (FSStatus)FS_RESULT::SUCCESS)
		{
			coreinit_suspendThread(coreinitThread_getCurrentThreadDepr(ppcInterpreterCurrentInstance), 1000);
			PPCCore_switchToScheduler();
			return param->returnStatus;
		}

		return status;
	}

	void export_SAVEGetFreeSpaceSize(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU8(accountSlot, 2);
		ppcDefineParamMEMPTR(returnedFreeSize, FSLargeSize, 3);
		ppcDefineParamU32(errHandling, 4);

		const SAVEStatus result = SAVEGetFreeSpaceSize(fsClient.GetPtr(), fsCmdBlock.GetPtr(), accountSlot, returnedFreeSize.GetPtr(), errHandling);
		cemuLog_log(LogType::Save, "SAVEGetFreeSpaceSize(0x{:08x}, 0x{:08x}, {:x}, {:x}) -> {:x}", fsClient.GetMPTR(), fsCmdBlock.GetMPTR(), accountSlot, errHandling, result);
		osLib_returnFromFunction(hCPU, result);
	}

	void export_SAVEGetFreeSpaceSizeAsync(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU8(accountSlot, 2);
		ppcDefineParamMEMPTR(returnedFreeSize, FSLargeSize, 3);
		ppcDefineParamU32(errHandling, 4);
		ppcDefineParamMEMPTR(asyncParams, FSAsyncParams_t, 5);

		const SAVEStatus result = SAVEGetFreeSpaceSizeAsync(fsClient.GetPtr(), fsCmdBlock.GetPtr(), accountSlot, returnedFreeSize.GetPtr(), errHandling, asyncParams.GetPtr());
		cemuLog_log(LogType::Save, "SAVEGetFreeSpaceSizeAsync(0x{:08x}, 0x{:08x}, {:x}, {:x}) -> {:x}", fsClient.GetMPTR(), fsCmdBlock.GetMPTR(), accountSlot, errHandling, result);
		osLib_returnFromFunction(hCPU, result);
	}

	void export_SAVEInit(PPCInterpreter_t* hCPU)
	{
		const SAVEStatus result = SAVEInit();
		cemuLog_log(LogType::Save, "SAVEInit() -> {:x}", result);
		osLib_returnFromFunction(hCPU, result);
	}

	void export_SAVERemoveAsync(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU8(accountSlot, 2);
		ppcDefineParamMEMPTR(path, const char, 3);
		ppcDefineParamU32(errHandling, 4);
		ppcDefineParamMEMPTR(asyncParams, FSAsyncParams_t, 5);

		const SAVEStatus result = SAVERemoveAsync(fsClient.GetPtr(), fsCmdBlock.GetPtr(), accountSlot, path.GetPtr(), errHandling, asyncParams.GetPtr());
		osLib_returnFromFunction(hCPU, result);
	}

	SAVEStatus SAVERemove(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, const char* path, FS_ERROR_MASK errHandling)
	{
		FSAsyncParams_t asyncParams;
		asyncParams.ioMsgQueue = MPTR_NULL;
		asyncParams.userCallback = _swapEndianU32(PPCInterpreter_makeCallableExportDepr(AsyncCallback));

		StackAllocator<AsyncCallbackParam_t> param;
		param->thread = coreinitThread_getCurrentThreadMPTRDepr(ppcInterpreterCurrentInstance);
		param->returnStatus = (FSStatus)FS_RESULT::SUCCESS;
		asyncParams.userContext = param.GetMPTRBE();

		SAVEStatus status = SAVERemoveAsync(client, block, accountSlot, path, errHandling, &asyncParams);
		if (status == (FSStatus)FS_RESULT::SUCCESS)
		{
			coreinit_suspendThread(coreinitThread_getCurrentThreadDepr(ppcInterpreterCurrentInstance), 1000);
			PPCCore_switchToScheduler();
			return param->returnStatus;
		}

		return status;
	}

	void export_SAVERemove(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU8(accountSlot, 2);
		ppcDefineParamMEMPTR(path, const char, 3);
		ppcDefineParamU32(errHandling, 4);

		const SAVEStatus result = SAVERemove(fsClient.GetPtr(), fsCmdBlock.GetPtr(), accountSlot, path.GetPtr(), errHandling);
		osLib_returnFromFunction(hCPU, result);
	}

	SAVEStatus SAVERenameAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, const char* oldPath, const char* newPath, FS_ERROR_MASK errHandling, const FSAsyncParams_t* asyncParams)
	{
		SAVEStatus result = (FSStatus)(FS_RESULT::FATAL_ERROR);

		OSLockMutex(&g_nn_save->mutex);

		uint32 persistentId;
		if (GetPersistentIdEx(accountSlot, &persistentId))
		{
			char fullOldPath[SAVE_MAX_PATH_SIZE];
			if (GetAbsoluteFullPath(persistentId, oldPath, fullOldPath))
			{
				char fullNewPath[SAVE_MAX_PATH_SIZE];
				if (GetAbsoluteFullPath(persistentId, newPath, fullNewPath))
					result = coreinit::FSRenameAsync(client, block, fullOldPath, fullNewPath, errHandling, (FSAsyncParamsNew_t*)asyncParams);
			}
		}
		else
			result = (FSStatus)FS_RESULT::NOT_FOUND;

		OSUnlockMutex(&g_nn_save->mutex);

		return result;
	}

	void export_SAVERenameAsync(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU8(accountSlot, 2);
		ppcDefineParamMEMPTR(oldPath, const char, 3);
		ppcDefineParamMEMPTR(newPath, const char, 4);
		ppcDefineParamU32(errHandling, 5);
		ppcDefineParamMEMPTR(asyncParams, FSAsyncParams_t, 6);

		const SAVEStatus result = SAVERenameAsync(fsClient.GetPtr(), fsCmdBlock.GetPtr(), accountSlot, oldPath.GetPtr(), newPath.GetPtr(), errHandling, asyncParams.GetPtr());
		cemuLog_log(LogType::Save, "SAVERenameAsync(0x{:08x}, 0x{:08x}, {:x}, {}, {}, {:x}) -> {:x}", fsClient.GetMPTR(), fsCmdBlock.GetMPTR(), accountSlot, oldPath.GetPtr(), newPath.GetPtr(), errHandling, result);
		osLib_returnFromFunction(hCPU, result);
	}

	SAVEStatus SAVERename(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, const char* oldPath, const char* newPath, FS_ERROR_MASK errHandling)
	{
		SAVEStatus result = (FSStatus)(FS_RESULT::FATAL_ERROR);

		OSLockMutex(&g_nn_save->mutex);
		uint32 persistentId;
		if (GetPersistentIdEx(accountSlot, &persistentId))
		{
			char fullOldPath[SAVE_MAX_PATH_SIZE];
			if (GetAbsoluteFullPath(persistentId, oldPath, fullOldPath))
			{
				char fullNewPath[SAVE_MAX_PATH_SIZE];
				if (GetAbsoluteFullPath(persistentId, newPath, fullNewPath))
					result = coreinit::FSRename(client, block, fullOldPath, fullNewPath, errHandling);
			}
		}
		else
			result = (FSStatus)FS_RESULT::NOT_FOUND;
		OSUnlockMutex(&g_nn_save->mutex);

		return result;
	}

	void export_SAVEOpenDirAsync(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU8(accountSlot, 2);
		ppcDefineParamMEMPTR(path, const char, 3);
		ppcDefineParamMEMPTR(hDir, betype<FSDirHandle2>, 4);
		ppcDefineParamU32(errHandling, 5);
		ppcDefineParamMEMPTR(asyncParams, FSAsyncParams_t, 6);

		const SAVEStatus result = SAVEOpenDirAsync(fsClient.GetPtr(), fsCmdBlock.GetPtr(), accountSlot, path.GetPtr(), hDir, errHandling, asyncParams.GetPtr());
		cemuLog_log(LogType::Save, "SAVEOpenDirAsync(0x{:08x}, 0x{:08x}, {:x}, {}, 0x{:08x} ({:x}), {:x}) -> {:x}", fsClient.GetMPTR(), fsCmdBlock.GetMPTR(), accountSlot, path.GetPtr(), hDir.GetMPTR(),
			(hDir.GetPtr() == nullptr ? 0 : (uint32)*hDir.GetPtr()), errHandling, result);
		osLib_returnFromFunction(hCPU, result);
	}

	SAVEStatus SAVEOpenDir(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, const char* path, FSDirHandlePtr hDir, FS_ERROR_MASK errHandling)
	{
		FSAsyncParams_t asyncParams;
		asyncParams.ioMsgQueue = MPTR_NULL;
		asyncParams.userCallback = _swapEndianU32(PPCInterpreter_makeCallableExportDepr(AsyncCallback));

		StackAllocator<AsyncCallbackParam_t> param;
		param->thread = coreinitThread_getCurrentThreadMPTRDepr(ppcInterpreterCurrentInstance);
		param->returnStatus = (FSStatus)FS_RESULT::SUCCESS;
		asyncParams.userContext = param.GetMPTRBE();

		SAVEStatus status = SAVEOpenDirAsync(client, block, accountSlot, path, hDir, errHandling, &asyncParams);
		if (status == (FSStatus)FS_RESULT::SUCCESS)
		{
			coreinit_suspendThread(coreinitThread_getCurrentThreadDepr(ppcInterpreterCurrentInstance), 1000);
			PPCCore_switchToScheduler();
			return param->returnStatus;
		}

		return status;
	}

	void export_SAVEOpenDir(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU8(accountSlot, 2);
		ppcDefineParamMEMPTR(path, const char, 3);
		ppcDefineParamMEMPTR(hDir, betype<FSDirHandle2>, 4);
		ppcDefineParamU32(errHandling, 5);

		const SAVEStatus result = SAVEOpenDir(fsClient.GetPtr(), fsCmdBlock.GetPtr(), accountSlot, path.GetPtr(), hDir, errHandling);
		cemuLog_log(LogType::Save, "SAVEOpenDir(0x{:08x}, 0x{:08x}, {:x}, {}, 0x{:08x} ({:x}), {:x}) -> {:x}", fsClient.GetMPTR(), fsCmdBlock.GetMPTR(), accountSlot, path.GetPtr(), hDir.GetMPTR(),
			(hDir.GetPtr() == nullptr ? 0 : (uint32)*hDir.GetPtr()), errHandling, result);
		osLib_returnFromFunction(hCPU, result);
	}

	SAVEStatus SAVEOpenDirOtherApplicationAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint64 titleId, uint8 accountSlot, const char* path, FSDirHandlePtr hDir, FS_ERROR_MASK errHandling, const FSAsyncParams_t* asyncParams)
	{
		SAVEStatus result = (FSStatus)(FS_RESULT::FATAL_ERROR);

		OSLockMutex(&g_nn_save->mutex);
		uint32 persistentId;
		if (GetPersistentIdEx(accountSlot, &persistentId))
		{
			char fullPath[SAVE_MAX_PATH_SIZE];
			if (GetAbsoluteFullPathOtherApplication(persistentId, titleId, path, fullPath))
				result = coreinit::FSOpenDirAsync(client, block, fullPath, hDir, errHandling, (FSAsyncParamsNew_t*)asyncParams);
		}
		else
			result = (FSStatus)FS_RESULT::NOT_FOUND;
		OSUnlockMutex(&g_nn_save->mutex);

		return result;
	}

	void export_SAVEOpenDirOtherApplicationAsync(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU64(titleId, 2);
		ppcDefineParamU8(accountSlot, 3);
		ppcDefineParamMEMPTR(path, const char, 4);
		ppcDefineParamMEMPTR(hDir, betype<FSDirHandle2>, 5);
		ppcDefineParamU32(errHandling, 6);
		ppcDefineParamMEMPTR(asyncParams, FSAsyncParams_t, 7);

		const SAVEStatus result = SAVEOpenDirOtherApplicationAsync(fsClient.GetPtr(), fsCmdBlock.GetPtr(), titleId, accountSlot, path.GetPtr(), hDir, errHandling, asyncParams.GetPtr());
		cemuLog_log(LogType::Save, "SAVEOpenDirOtherApplicationAsync(0x{:08x}, 0x{:08x}, {:x}, {:x}, {}, 0x{:08x} ({:x}), {:x}) -> {:x}", fsClient.GetMPTR(), fsCmdBlock.GetMPTR(), titleId, accountSlot, path.GetPtr(), hDir.GetMPTR(),
			(hDir.GetPtr() == nullptr ? 0 : (uint32)*hDir.GetPtr()), errHandling, result);
		osLib_returnFromFunction(hCPU, result);
	}

	SAVEStatus SAVEOpenDirOtherApplication(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint64 titleId, uint8 accountSlot, const char* path, FSDirHandlePtr hDir, FS_ERROR_MASK errHandling)
	{
		FSAsyncParams_t asyncParams;
		asyncParams.ioMsgQueue = MPTR_NULL;
		asyncParams.userCallback = _swapEndianU32(PPCInterpreter_makeCallableExportDepr(AsyncCallback));

		StackAllocator<AsyncCallbackParam_t> param;
		param->thread = coreinitThread_getCurrentThreadMPTRDepr(ppcInterpreterCurrentInstance);
		param->returnStatus = (FSStatus)FS_RESULT::SUCCESS;
		asyncParams.userContext = param.GetMPTRBE();

		SAVEStatus status = SAVEOpenDirOtherApplicationAsync(client, block, titleId, accountSlot, path, hDir, errHandling, &asyncParams);
		if (status == (FSStatus)FS_RESULT::SUCCESS)
		{
			coreinit_suspendThread(coreinitThread_getCurrentThreadDepr(ppcInterpreterCurrentInstance), 1000);
			PPCCore_switchToScheduler();
			return param->returnStatus;
		}

		return status;
	}

	void export_SAVEOpenDirOtherApplication(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU64(titleId, 2);
		ppcDefineParamU8(accountSlot, 3);
		ppcDefineParamMEMPTR(path, const char, 4);
		ppcDefineParamMEMPTR(hDir, betype<FSDirHandle2>, 5);
		ppcDefineParamU32(errHandling, 6);

		const SAVEStatus result = SAVEOpenDirOtherApplication(fsClient.GetPtr(), fsCmdBlock.GetPtr(), titleId, accountSlot, path.GetPtr(), hDir, errHandling);
		cemuLog_log(LogType::Save, "SAVEOpenDirOtherApplication(0x{:08x}, 0x{:08x}, {:x}, {:x}, {}, 0x{:08x} ({:x}), {:x}) -> {:x}", fsClient.GetMPTR(), fsCmdBlock.GetMPTR(), titleId, accountSlot, path.GetPtr(), hDir.GetMPTR(),
			(hDir.GetPtr() == nullptr ? 0 : (uint32)*hDir.GetPtr()), errHandling, result);
		osLib_returnFromFunction(hCPU, result);
	}

	SAVEStatus SAVEOpenDirOtherNormalApplicationAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint32 uniqueId, uint8 accountSlot, const char* path, FSDirHandlePtr hDir, FS_ERROR_MASK errHandling, const FSAsyncParams_t* asyncParams)
	{
		uint64 titleId = SAVE_UNIQUE_TO_TITLE_ID(uniqueId);
		return SAVEOpenDirOtherApplicationAsync(client, block, titleId, accountSlot, path, hDir, errHandling, asyncParams);
	}

	void export_SAVEOpenDirOtherNormalApplicationAsync(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU32(uniqueId, 2);
		ppcDefineParamU8(accountSlot, 3);
		ppcDefineParamMEMPTR(path, const char, 4);
		ppcDefineParamMEMPTR(hDir, betype<FSDirHandle2>, 5);
		ppcDefineParamU32(errHandling, 6);
		ppcDefineParamMEMPTR(asyncParams, FSAsyncParams_t, 7);

		const SAVEStatus result = SAVEOpenDirOtherNormalApplicationAsync(fsClient.GetPtr(), fsCmdBlock.GetPtr(), uniqueId, accountSlot, path.GetPtr(), hDir, errHandling, asyncParams.GetPtr());
		osLib_returnFromFunction(hCPU, result);
	}

	SAVEStatus SAVEOpenDirOtherNormalApplication(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint32 uniqueId, uint8 accountSlot, const char* path, FSDirHandlePtr hDir, FS_ERROR_MASK errHandling)
	{
		uint64 titleId = SAVE_UNIQUE_TO_TITLE_ID(uniqueId);
		return SAVEOpenDirOtherApplication(client, block, titleId, accountSlot, path, hDir, errHandling);
	}

	void export_SAVEOpenDirOtherNormalApplication(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU32(uniqueId, 2);
		ppcDefineParamU8(accountSlot, 3);
		ppcDefineParamMEMPTR(path, const char, 4);
		ppcDefineParamMEMPTR(hDir, betype<FSDirHandle2>, 5);
		ppcDefineParamU32(errHandling, 6);

		const SAVEStatus result = SAVEOpenDirOtherNormalApplication(fsClient.GetPtr(), fsCmdBlock.GetPtr(), uniqueId, accountSlot, path.GetPtr(), hDir, errHandling);
		osLib_returnFromFunction(hCPU, result);
	}

	SAVEStatus SAVEOpenDirOtherNormalApplicationVariationAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint32 uniqueId, uint8 variation, uint8 accountSlot, const char* path, FSDirHandlePtr hDir, FS_ERROR_MASK errHandling, const FSAsyncParams_t* asyncParams)
	{
		uint64 titleId = SAVE_UNIQUE_TO_TITLE_ID_VARIATION(uniqueId, variation);
		return SAVEOpenDirOtherApplicationAsync(client, block, titleId, accountSlot, path, hDir, errHandling, asyncParams);
	}

	void export_SAVEOpenDirOtherNormalApplicationVariationAsync(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU32(uniqueId, 2);
		ppcDefineParamU8(variation, 3);
		ppcDefineParamU8(accountSlot, 4);
		ppcDefineParamMEMPTR(path, const char, 5);
		ppcDefineParamMEMPTR(hDir, betype<FSDirHandle2>, 6);
		ppcDefineParamU32(errHandling, 7);
		ppcDefineParamMEMPTR(asyncParams, FSAsyncParams_t, 8);

		const SAVEStatus result = SAVEOpenDirOtherNormalApplicationVariationAsync(fsClient.GetPtr(), fsCmdBlock.GetPtr(), uniqueId, variation, accountSlot, path.GetPtr(), hDir, errHandling, asyncParams.GetPtr());
		osLib_returnFromFunction(hCPU, result);
	}

	SAVEStatus SAVEOpenDirOtherNormalApplicationVariation(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint32 uniqueId, uint8 variation, uint8 accountSlot, const char* path, FSDirHandlePtr hDir, FS_ERROR_MASK errHandling)
	{
		uint64 titleId = SAVE_UNIQUE_TO_TITLE_ID_VARIATION(uniqueId, variation);
		return SAVEOpenDirOtherApplication(client, block, titleId, accountSlot, path, hDir, errHandling);
	}

	void export_SAVEOpenDirOtherNormalApplicationVariation(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU32(uniqueId, 2);
		ppcDefineParamU8(variation, 3);
		ppcDefineParamU8(accountSlot, 4);
		ppcDefineParamMEMPTR(path, const char, 5);
		ppcDefineParamMEMPTR(hDir, betype<FSDirHandle2>, 6);
		ppcDefineParamU32(errHandling, 7);

		const SAVEStatus result = SAVEOpenDirOtherNormalApplicationVariation(fsClient.GetPtr(), fsCmdBlock.GetPtr(), uniqueId, variation, accountSlot, path.GetPtr(), hDir, errHandling);
		osLib_returnFromFunction(hCPU, result);
	}

	void export_SAVEMakeDirAsync(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU8(accountSlot, 2);
		ppcDefineParamMEMPTR(path, const char, 3);
		ppcDefineParamU32(errHandling, 4);
		ppcDefineParamMEMPTR(asyncParams, FSAsyncParams_t, 5);

		const SAVEStatus result = SAVEMakeDirAsync(fsClient.GetPtr(), fsCmdBlock.GetPtr(), accountSlot, path.GetPtr(), errHandling, asyncParams.GetPtr());
		cemuLog_log(LogType::Save, "SAVEMakeDirAsync(0x{:08x}, 0x{:08x}, {:x}, {},  {:x}) -> {:x}", fsClient.GetMPTR(), fsCmdBlock.GetMPTR(), accountSlot, path.GetPtr(), errHandling, result);
		osLib_returnFromFunction(hCPU, result);
	}

	SAVEStatus SAVEMakeDir(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, const char* path, FS_ERROR_MASK errHandling)
	{
		FSAsyncParams_t asyncParams;
		asyncParams.ioMsgQueue = MPTR_NULL;
		asyncParams.userCallback = _swapEndianU32(PPCInterpreter_makeCallableExportDepr(AsyncCallback));

		StackAllocator<AsyncCallbackParam_t> param;
		param->thread = coreinitThread_getCurrentThreadMPTRDepr(ppcInterpreterCurrentInstance);
		param->returnStatus = (FSStatus)FS_RESULT::SUCCESS;
		asyncParams.userContext = param.GetMPTRBE();

		SAVEStatus status = SAVEMakeDirAsync(client, block, accountSlot, path, errHandling, &asyncParams);
		if (status == (FSStatus)FS_RESULT::SUCCESS)
		{
			coreinit_suspendThread(coreinitThread_getCurrentThreadDepr(ppcInterpreterCurrentInstance), 1000);
			PPCCore_switchToScheduler();
			return param->returnStatus;
		}

		return status;
	}

	void export_SAVEMakeDir(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU8(accountSlot, 2);
		ppcDefineParamMEMPTR(path, const char, 3);
		ppcDefineParamU32(errHandling, 4);

		const SAVEStatus result = SAVEMakeDir(fsClient.GetPtr(), fsCmdBlock.GetPtr(), accountSlot, path.GetPtr(), errHandling);
		cemuLog_log(LogType::Save, "SAVEMakeDir(0x{:08x}, 0x{:08x}, {:x}, {},  {:x}) -> {:x}", fsClient.GetMPTR(), fsCmdBlock.GetMPTR(), accountSlot, path.GetPtr(), errHandling, result);
		osLib_returnFromFunction(hCPU, result);
	}

	void export_SAVEOpenFileAsync(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU8(accountSlot, 2);
		ppcDefineParamMEMPTR(path, const char, 3);
		ppcDefineParamMEMPTR(mode, const char, 4);
		ppcDefineParamMEMPTR(hFile, FSFileHandleDepr_t, 5);
		ppcDefineParamU32(errHandling, 6);
		ppcDefineParamMEMPTR(asyncParams, FSAsyncParamsNew_t, 7);

		const SAVEStatus result = SAVEOpenFileAsync(fsClient.GetPtr(), fsCmdBlock.GetPtr(), accountSlot, path.GetPtr(), mode.GetPtr(), hFile.GetPtr(), errHandling, asyncParams.GetPtr());
		cemuLog_log(LogType::Save, "SAVEOpenFileAsync(0x{:08x}, 0x{:08x}, {:x}, {}, {}, 0x{:08x}, {:x}) -> {:x}", fsClient.GetMPTR(), fsCmdBlock.GetMPTR(), accountSlot, path.GetPtr(), mode.GetPtr(), hFile.GetMPTR(), errHandling, result);
		osLib_returnFromFunction(hCPU, result);
	}

	SAVEStatus SAVEOpenFile(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, const char* path, const char* mode, FSFileHandleDepr_t* hFile, FS_ERROR_MASK errHandling)
	{
		FSAsyncParamsNew_t asyncParams;
		asyncParams.ioMsgQueue = nullptr;
		asyncParams.userCallback = PPCInterpreter_makeCallableExportDepr(AsyncCallback);

		StackAllocator<AsyncCallbackParam_t> param;
		param->thread = coreinitThread_getCurrentThreadMPTRDepr(ppcInterpreterCurrentInstance);
		param->returnStatus = (FSStatus)FS_RESULT::SUCCESS;
		asyncParams.userContext = param.GetPointer();

		SAVEStatus status = SAVEOpenFileAsync(client, block, accountSlot, path, mode, hFile, errHandling, &asyncParams);
		if (status == (FSStatus)FS_RESULT::SUCCESS)
		{
			coreinit_suspendThread(coreinitThread_getCurrentThreadDepr(ppcInterpreterCurrentInstance), 1000);
			PPCCore_switchToScheduler();
			return param->returnStatus;
		}

		return status;
	}

	void export_SAVEOpenFile(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU8(accountSlot, 2);
		ppcDefineParamMEMPTR(path, const char, 3);
		ppcDefineParamMEMPTR(mode, const char, 4);
		ppcDefineParamMEMPTR(hFile, FSFileHandleDepr_t, 5);
		ppcDefineParamU32(errHandling, 6);

		const SAVEStatus result = SAVEOpenFile(fsClient.GetPtr(), fsCmdBlock.GetPtr(), accountSlot, path.GetPtr(), mode.GetPtr(), hFile.GetPtr(), errHandling);
		cemuLog_log(LogType::Save, "SAVEOpenFile(0x{:08x}, 0x{:08x}, {:x}, {}, {}, 0x{:08x}, {:x}) -> {:x}", fsClient.GetMPTR(), fsCmdBlock.GetMPTR(), accountSlot, path.GetPtr(), mode.GetPtr(), hFile.GetMPTR(), errHandling, result);
		osLib_returnFromFunction(hCPU, result);
	}

	void export_SAVEInitSaveDir(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU8(accountSlot, 0);
		const SAVEStatus result = SAVEInitSaveDir(accountSlot);
		cemuLog_log(LogType::Save, "SAVEInitSaveDir({:x}) -> {:x}", accountSlot, result);
		osLib_returnFromFunction(hCPU, result);
	}

	void export_SAVEGetStatAsync(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU8(accountSlot, 2);
		ppcDefineParamMEMPTR(path, const char, 3);
		ppcDefineParamMEMPTR(stat, FSStat_t, 4);
		ppcDefineParamU32(errHandling, 5);
		ppcDefineParamMEMPTR(asyncParams, FSAsyncParams_t, 6);

		const SAVEStatus result = SAVEGetStatAsync(fsClient.GetPtr(), fsCmdBlock.GetPtr(), accountSlot, path.GetPtr(), stat.GetPtr(), errHandling, asyncParams.GetPtr());
		cemuLog_log(LogType::Save, "SAVEGetStatAsync(0x{:08x}, 0x{:08x}, {:x}, {}, 0x{:08x}, {:x}) -> {:x}", fsClient.GetMPTR(), fsCmdBlock.GetMPTR(), accountSlot, path.GetPtr(), stat.GetMPTR(), errHandling, result);
		osLib_returnFromFunction(hCPU, result);
	}

	SAVEStatus SAVEGetStat(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, const char* path, FSStat_t* stat, FS_ERROR_MASK errHandling)
	{
		FSAsyncParams_t asyncParams;
		asyncParams.ioMsgQueue = MPTR_NULL;
		asyncParams.userCallback = _swapEndianU32(PPCInterpreter_makeCallableExportDepr(AsyncCallback));

		StackAllocator<AsyncCallbackParam_t> param;
		param->thread = coreinitThread_getCurrentThreadMPTRDepr(ppcInterpreterCurrentInstance);
		param->returnStatus = (FSStatus)FS_RESULT::SUCCESS;
		asyncParams.userContext = param.GetMPTRBE();

		SAVEStatus status = SAVEGetStatAsync(client, block, accountSlot, path, stat, errHandling, &asyncParams);
		if (status == (FSStatus)FS_RESULT::SUCCESS)
		{
			coreinit_suspendThread(coreinitThread_getCurrentThreadDepr(ppcInterpreterCurrentInstance), 1000);
			PPCCore_switchToScheduler();
			return param->returnStatus;
		}

		return status;
	}

	void export_SAVEGetStat(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU8(accountSlot, 2);
		ppcDefineParamMEMPTR(path, const char, 3);
		ppcDefineParamMEMPTR(stat, FSStat_t, 4);
		ppcDefineParamU32(errHandling, 5);

		const SAVEStatus result = SAVEGetStat(fsClient.GetPtr(), fsCmdBlock.GetPtr(), accountSlot, path.GetPtr(), stat.GetPtr(), errHandling);
		cemuLog_log(LogType::Save, "SAVEGetStat(0x{:08x}, 0x{:08x}, {:x}, {}, 0x{:08x}, {:x}) -> {:x}", fsClient.GetMPTR(), fsCmdBlock.GetMPTR(), accountSlot, path.GetPtr(), stat.GetMPTR(), errHandling, result);
		osLib_returnFromFunction(hCPU, result);
	}

	void export_SAVEGetStatOtherApplicationAsync(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU64(titleId, 2);
		ppcDefineParamU8(accountSlot, 4);
		ppcDefineParamMEMPTR(path, const char, 5);
		ppcDefineParamMEMPTR(stat, FSStat_t, 6);
		ppcDefineParamU32(errHandling, 7);
		ppcDefineParamMEMPTR(asyncParams, FSAsyncParams_t, 8);

		const SAVEStatus result = SAVEGetStatOtherApplicationAsync(fsClient.GetPtr(), fsCmdBlock.GetPtr(), titleId, accountSlot, path.GetPtr(), stat.GetPtr(), errHandling, asyncParams.GetPtr());
		osLib_returnFromFunction(hCPU, result);
	}

	SAVEStatus SAVEGetStatOtherApplication(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint64 titleId, uint8 accountSlot, const char* path, FSStat_t* stat, FS_ERROR_MASK errHandling)
	{
		FSAsyncParams_t asyncParams;
		asyncParams.ioMsgQueue = MPTR_NULL;
		asyncParams.userCallback = _swapEndianU32(PPCInterpreter_makeCallableExportDepr(AsyncCallback));

		StackAllocator<AsyncCallbackParam_t> param;
		param->thread = coreinitThread_getCurrentThreadMPTRDepr(ppcInterpreterCurrentInstance);
		param->returnStatus = (FSStatus)FS_RESULT::SUCCESS;
		asyncParams.userContext = param.GetMPTRBE();

		SAVEStatus status = SAVEGetStatOtherApplicationAsync(client, block, titleId, accountSlot, path, stat, errHandling, &asyncParams);
		if (status == (FSStatus)FS_RESULT::SUCCESS)
		{
			coreinit_suspendThread(coreinitThread_getCurrentThreadDepr(ppcInterpreterCurrentInstance), 1000);
			PPCCore_switchToScheduler();
			return param->returnStatus;
		}

		return status;
	}

	void export_SAVEGetStatOtherApplication(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU64(titleId, 2);
		ppcDefineParamU8(accountSlot, 4);
		ppcDefineParamMEMPTR(path, const char, 5);
		ppcDefineParamMEMPTR(stat, FSStat_t, 6);
		ppcDefineParamU32(errHandling, 7);

		const SAVEStatus result = SAVEGetStatOtherApplication(fsClient.GetPtr(), fsCmdBlock.GetPtr(), titleId, accountSlot, path.GetPtr(), stat.GetPtr(), errHandling);
		osLib_returnFromFunction(hCPU, result);
	}
	

	void export_SAVEGetStatOtherNormalApplicationAsync(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU32(uniqueId, 2);
		ppcDefineParamU8(accountSlot, 3);
		ppcDefineParamMEMPTR(path, const char, 4);
		ppcDefineParamMEMPTR(stat, FSStat_t, 5);
		ppcDefineParamU32(errHandling, 6);
		ppcDefineParamMEMPTR(asyncParams, FSAsyncParams_t, 8);

		const SAVEStatus result = SAVEGetStatOtherNormalApplicationAsync(fsClient.GetPtr(), fsCmdBlock.GetPtr(), uniqueId, accountSlot, path.GetPtr(), stat.GetPtr(), errHandling, asyncParams.GetPtr());
		osLib_returnFromFunction(hCPU, result);
	}

	SAVEStatus SAVEGetStatOtherNormalApplication(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint32 uniqueId, uint8 accountSlot, const char* path, FSStat_t* stat, FS_ERROR_MASK errHandling)
	{
		//peterBreak();

		uint64 titleId = SAVE_UNIQUE_TO_TITLE_ID(uniqueId);
		return SAVEGetStatOtherApplication(client, block, titleId, accountSlot, path, stat, errHandling);
	}

	void export_SAVEGetStatOtherNormalApplication(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU32(uniqueId, 2);
		ppcDefineParamU8(accountSlot, 3);
		ppcDefineParamMEMPTR(path, const char, 4);
		ppcDefineParamMEMPTR(stat, FSStat_t, 5);
		ppcDefineParamU32(errHandling, 6);

		const SAVEStatus result = SAVEGetStatOtherNormalApplication(fsClient.GetPtr(), fsCmdBlock.GetPtr(), uniqueId, accountSlot, path.GetPtr(), stat.GetPtr(), errHandling);
		osLib_returnFromFunction(hCPU, result);
	}



	void export_SAVEGetStatOtherNormalApplicationVariationAsync(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU32(uniqueId, 2);
		ppcDefineParamU8(variation, 3);
		ppcDefineParamU8(accountSlot, 4);
		ppcDefineParamMEMPTR(path, const char, 5);
		ppcDefineParamMEMPTR(stat, FSStat_t, 6);
		ppcDefineParamU32(errHandling, 7);
		ppcDefineParamMEMPTR(asyncParams, FSAsyncParams_t, 8);

		const SAVEStatus result = SAVEGetStatOtherNormalApplicationVariationAsync(fsClient.GetPtr(), fsCmdBlock.GetPtr(), uniqueId, variation, accountSlot, path.GetPtr(), stat.GetPtr(), errHandling, asyncParams.GetPtr());
		osLib_returnFromFunction(hCPU, result);
	}

	SAVEStatus SAVEGetStatOtherNormalApplicationVariation(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint32 uniqueId, uint8 variation, uint8 accountSlot, const char* path, FSStat_t* stat, FS_ERROR_MASK errHandling)
	{
		//peterBreak();

		uint64 titleId = SAVE_UNIQUE_TO_TITLE_ID_VARIATION(uniqueId, variation);
		return SAVEGetStatOtherApplication(client, block, titleId, accountSlot, path, stat, errHandling);
	}

	void export_SAVEGetStatOtherNormalApplicationVariation(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU32(uniqueId, 2);
		ppcDefineParamU8(variation, 3);
		ppcDefineParamU8(accountSlot, 4);
		ppcDefineParamMEMPTR(path, const char, 5);
		ppcDefineParamMEMPTR(stat, FSStat_t, 6);
		ppcDefineParamU32(errHandling, 7);

		const SAVEStatus result = SAVEGetStatOtherNormalApplicationVariation(fsClient.GetPtr(), fsCmdBlock.GetPtr(), uniqueId, variation, accountSlot, path.GetPtr(), stat.GetPtr(), errHandling);
		osLib_returnFromFunction(hCPU, result);
	}


	SAVEStatus SAVEGetSharedDataTitlePath(uint64 titleId, const char* dataFileName, char* output, sint32 outputLength)
	{
		SAVEStatus result = (FSStatus)(FS_RESULT::FATAL_ERROR);
		sint32 written = snprintf(output, outputLength, "/vol/storage_mlc01/sys/title/%08x/%08x/content/%s", GetTitleIdHigh(titleId), GetTitleIdLow(titleId), dataFileName);
		if (written >= 0 && written < outputLength)
			result = (FSStatus)FS_RESULT::SUCCESS;
		cemu_assert_debug(result != (FSStatus)(FS_RESULT::FATAL_ERROR));
		return result;
	}

	void export_SAVEGetSharedDataTitlePath(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU64(titleId, 0);
		ppcDefineParamMEMPTR(dataFileName, const char, 2);
		ppcDefineParamMEMPTR(output, char, 3);
		ppcDefineParamS32(outputLength, 4);
		const SAVEStatus result = SAVEGetSharedDataTitlePath(titleId, dataFileName.GetPtr(), output.GetPtr(), outputLength);
		cemuLog_log(LogType::Save, "SAVEGetSharedDataTitlePath(0x{:x}, {}, {}, 0x{:x}) -> {:x}", titleId, dataFileName.GetPtr(), output.GetPtr(), outputLength, result);
		osLib_returnFromFunction(hCPU, result);
	}

	SAVEStatus SAVEGetSharedSaveDataPath(uint64 titleId, const char* dataFileName, char* output, uint32 outputLength)
	{
		SAVEStatus result = (FSStatus)(FS_RESULT::FATAL_ERROR);
		int written = snprintf(output, outputLength, "/vol/storage_mlc01/usr/save/%08x/%08x/user/common/%s", GetTitleIdHigh(titleId), GetTitleIdLow(titleId), dataFileName);
		if (written >= 0 && written < (sint32)outputLength)
			result = (FSStatus)FS_RESULT::SUCCESS;
		cemu_assert_debug(result != (FSStatus)(FS_RESULT::FATAL_ERROR));
		return result;
	}

	void export_SAVEGetSharedSaveDataPath(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU64(titleId, 0);
		ppcDefineParamMEMPTR(dataFileName, const char, 2);
		ppcDefineParamMEMPTR(output, char, 3);
		ppcDefineParamU32(outputLength, 4);
		const SAVEStatus result = SAVEGetSharedSaveDataPath(titleId, dataFileName.GetPtr(), output.GetPtr(), outputLength);
		osLib_returnFromFunction(hCPU, result);
	}

	SAVEStatus SAVEChangeDirAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, const char* path, FS_ERROR_MASK errHandling, const FSAsyncParams_t* asyncParams)
	{
		SAVEStatus result = (FSStatus)(FS_RESULT::FATAL_ERROR);

		OSLockMutex(&g_nn_save->mutex);
		uint32 persistentId;
		if (GetPersistentIdEx(accountSlot, &persistentId))
		{
			char fullPath[SAVE_MAX_PATH_SIZE];
			if (GetAbsoluteFullPath(persistentId, path, fullPath))
				result = coreinit::FSChangeDirAsync(client, block, fullPath, errHandling, (FSAsyncParamsNew_t*)asyncParams);
		}
		else
			result = (FSStatus)FS_RESULT::NOT_FOUND;
		OSUnlockMutex(&g_nn_save->mutex);

		return result;
	}

	void export_SAVEChangeDirAsync(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU8(accountSlot, 2);
		ppcDefineParamMEMPTR(path, const char, 3);
		ppcDefineParamU32(errHandling, 4);
		ppcDefineParamMEMPTR(asyncParams, FSAsyncParams_t, 5);
		const SAVEStatus result = SAVEChangeDirAsync(fsClient.GetPtr(), fsCmdBlock.GetPtr(), accountSlot, path.GetPtr(), errHandling, asyncParams.GetPtr());
		cemuLog_log(LogType::Save, "SAVEChangeDirAsync(0x{:08x}, 0x{:08x}, {:x}, {}, {:x}) -> {:x}", fsClient.GetMPTR(), fsCmdBlock.GetMPTR(), accountSlot, path.GetPtr(), errHandling, result);
		osLib_returnFromFunction(hCPU, result);
	}

	SAVEStatus SAVEChangeDir(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, const char* path, FS_ERROR_MASK errHandling)
	{
		FSAsyncParams_t asyncParams;
		asyncParams.ioMsgQueue = MPTR_NULL;
		asyncParams.userCallback = _swapEndianU32(PPCInterpreter_makeCallableExportDepr(AsyncCallback));

		StackAllocator<AsyncCallbackParam_t> param;
		param->thread = coreinitThread_getCurrentThreadMPTRDepr(ppcInterpreterCurrentInstance);
		param->returnStatus = (FSStatus)FS_RESULT::SUCCESS;
		asyncParams.userContext = param.GetMPTRBE();

		SAVEStatus status = SAVEChangeDirAsync(client, block, accountSlot, path, errHandling, &asyncParams);
		if (status == (FSStatus)FS_RESULT::SUCCESS)
		{
			coreinit_suspendThread(coreinitThread_getCurrentThreadDepr(ppcInterpreterCurrentInstance), 1000);
			PPCCore_switchToScheduler();
			return param->returnStatus;
		}

		return status;
	}

	void export_SAVEChangeDir(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU8(accountSlot, 2);
		ppcDefineParamMEMPTR(path, const char, 3);
		ppcDefineParamU32(errHandling, 4);
		const SAVEStatus result = SAVEChangeDir(fsClient.GetPtr(), fsCmdBlock.GetPtr(), accountSlot, path.GetPtr(), errHandling);
		cemuLog_log(LogType::Save, "SAVEChangeDir(0x{:08x}, 0x{:08x}, {:x}, {}, {:x}) -> {:x}", fsClient.GetMPTR(), fsCmdBlock.GetMPTR(), accountSlot, path.GetPtr(), errHandling, result);
		osLib_returnFromFunction(hCPU, result);
	}

	SAVEStatus SAVEFlushQuotaAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, FS_ERROR_MASK errHandling, const FSAsyncParams_t* asyncParams)
	{
		SAVEStatus result = (FSStatus)(FS_RESULT::FATAL_ERROR);

		OSLockMutex(&g_nn_save->mutex);
		uint32 persistentId;
		if (GetPersistentIdEx(accountSlot, &persistentId))
		{
			char fullPath[SAVE_MAX_PATH_SIZE];
			if (GetAbsoluteFullPath(persistentId, nullptr, fullPath))
			{
				result = coreinit::FSFlushQuotaAsync(client, block, fullPath, errHandling, (FSAsyncParamsNew_t*)asyncParams);
				// if(OSGetUPID != 0xF)
				UpdateSaveTimeStamp(persistentId);
			}
		}
		else
			result = (FSStatus)FS_RESULT::NOT_FOUND;
		OSUnlockMutex(&g_nn_save->mutex);

		return result;
	}

	void export_SAVEFlushQuotaAsync(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU8(accountSlot, 2);
		ppcDefineParamU32(errHandling, 3);
		ppcDefineParamMEMPTR(asyncParams, FSAsyncParams_t, 4);
		const SAVEStatus result = SAVEFlushQuotaAsync(fsClient.GetPtr(), fsCmdBlock.GetPtr(), accountSlot, errHandling, asyncParams.GetPtr());
		cemuLog_log(LogType::Save, "SAVEFlushQuotaAsync(0x{:08x}, 0x{:08x}, {:x}, {:x}) -> {:x}", fsClient.GetMPTR(), fsCmdBlock.GetMPTR(), accountSlot, errHandling, result);
		osLib_returnFromFunction(hCPU, result);
	}

	SAVEStatus SAVEFlushQuota(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot,  FS_ERROR_MASK errHandling)
	{
		FSAsyncParams_t asyncParams;
		asyncParams.ioMsgQueue = MPTR_NULL;
		asyncParams.userCallback = _swapEndianU32(PPCInterpreter_makeCallableExportDepr(AsyncCallback));

		StackAllocator<AsyncCallbackParam_t> param;
		param->thread = coreinitThread_getCurrentThreadMPTRDepr(ppcInterpreterCurrentInstance);
		param->returnStatus = (FSStatus)FS_RESULT::SUCCESS;
		asyncParams.userContext = param.GetMPTRBE();

		SAVEStatus status = SAVEFlushQuotaAsync(client, block, accountSlot, errHandling, &asyncParams);
		if (status == (FSStatus)FS_RESULT::SUCCESS)
		{
			coreinit_suspendThread(coreinitThread_getCurrentThreadDepr(ppcInterpreterCurrentInstance), 1000);
			PPCCore_switchToScheduler();
			return param->returnStatus;
		}
		return status;
	}

	void export_SAVEFlushQuota(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(fsCmdBlock, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU8(accountSlot, 2);
		ppcDefineParamU32(errHandling, 3);
		const SAVEStatus result = SAVEFlushQuota(fsClient.GetPtr(), fsCmdBlock.GetPtr(), accountSlot, errHandling);
		cemuLog_log(LogType::Save, "SAVEFlushQuota(0x{:08x}, 0x{:08x}, {:x}, {:x}) -> {:x}", fsClient.GetMPTR(), fsCmdBlock.GetMPTR(), accountSlot, errHandling, result);
		osLib_returnFromFunction(hCPU, result);
	}

	void load()
	{


		osLib_addFunction("nn_save", "SAVEInit", export_SAVEInit);
		osLib_addFunction("nn_save", "SAVEInitSaveDir", export_SAVEInitSaveDir);
		osLib_addFunction("nn_save", "SAVEGetSharedDataTitlePath", export_SAVEGetSharedDataTitlePath);
		osLib_addFunction("nn_save", "SAVEGetSharedSaveDataPath", export_SAVEGetSharedSaveDataPath);

		// sync functions
		osLib_addFunction("nn_save", "SAVEGetFreeSpaceSize", export_SAVEGetFreeSpaceSize);
		osLib_addFunction("nn_save", "SAVEMakeDir", export_SAVEMakeDir);
		osLib_addFunction("nn_save", "SAVERemove", export_SAVERemove);
		osLib_addFunction("nn_save", "SAVEChangeDir", export_SAVEChangeDir);
		osLib_addFunction("nn_save", "SAVEFlushQuota", export_SAVEFlushQuota);

		osLib_addFunction("nn_save", "SAVEGetStat", export_SAVEGetStat);
		osLib_addFunction("nn_save", "SAVEGetStatOtherApplication", export_SAVEGetStatOtherApplication);
		osLib_addFunction("nn_save", "SAVEGetStatOtherNormalApplication", export_SAVEGetStatOtherNormalApplication);
		osLib_addFunction("nn_save", "SAVEGetStatOtherNormalApplicationVariation", export_SAVEGetStatOtherNormalApplicationVariation);

		osLib_addFunction("nn_save", "SAVEOpenFile", export_SAVEOpenFile);
		osLib_addFunction("nn_save", "SAVEOpenFileOtherApplication", export_SAVEOpenFileOtherApplication);
		osLib_addFunction("nn_save", "SAVEOpenFileOtherNormalApplication", export_SAVEOpenFileOtherNormalApplication);
		osLib_addFunction("nn_save", "SAVEOpenFileOtherNormalApplicationVariation", export_SAVEOpenFileOtherNormalApplicationVariation);

		osLib_addFunction("nn_save", "SAVEOpenDir", export_SAVEOpenDir);
		osLib_addFunction("nn_save", "SAVEOpenDirOtherApplication", export_SAVEOpenDirOtherApplication);
		osLib_addFunction("nn_save", "SAVEOpenDirOtherNormalApplication", export_SAVEOpenDirOtherNormalApplication);
		osLib_addFunction("nn_save", "SAVEOpenDirOtherNormalApplicationVariation", export_SAVEOpenDirOtherNormalApplicationVariation);

		// async functions
		osLib_addFunction("nn_save", "SAVEGetFreeSpaceSizeAsync", export_SAVEGetFreeSpaceSizeAsync);
		osLib_addFunction("nn_save", "SAVEMakeDirAsync", export_SAVEMakeDirAsync);
		osLib_addFunction("nn_save", "SAVERemoveAsync", export_SAVERemoveAsync);
		osLib_addFunction("nn_save", "SAVERenameAsync", export_SAVERenameAsync);
		cafeExportRegister("nn_save", SAVERename, LogType::Save);		

		osLib_addFunction("nn_save", "SAVEChangeDirAsync", export_SAVEChangeDirAsync);
		osLib_addFunction("nn_save", "SAVEFlushQuotaAsync", export_SAVEFlushQuotaAsync);

		osLib_addFunction("nn_save", "SAVEGetStatAsync", export_SAVEGetStatAsync);
		osLib_addFunction("nn_save", "SAVEGetStatOtherApplicationAsync", export_SAVEGetStatOtherApplicationAsync);
		osLib_addFunction("nn_save", "SAVEGetStatOtherNormalApplicationAsync", export_SAVEGetStatOtherNormalApplicationAsync);
		osLib_addFunction("nn_save", "SAVEGetStatOtherNormalApplicationVariationAsync", export_SAVEGetStatOtherNormalApplicationVariationAsync);

		osLib_addFunction("nn_save", "SAVEOpenFileAsync", export_SAVEOpenFileAsync);
		osLib_addFunction("nn_save", "SAVEOpenFileOtherApplicationAsync", export_SAVEOpenFileOtherApplicationAsync);
		osLib_addFunction("nn_save", "SAVEOpenFileOtherNormalApplicationAsync", export_SAVEOpenFileOtherNormalApplicationAsync);
		osLib_addFunction("nn_save", "SAVEOpenFileOtherNormalApplicationVariationAsync", export_SAVEOpenFileOtherNormalApplicationVariationAsync);
		
		osLib_addFunction("nn_save", "SAVEOpenDirAsync", export_SAVEOpenDirAsync);
		osLib_addFunction("nn_save", "SAVEOpenDirOtherApplicationAsync", export_SAVEOpenDirOtherApplicationAsync);
		osLib_addFunction("nn_save", "SAVEOpenDirOtherNormalApplicationAsync", export_SAVEOpenDirOtherNormalApplicationAsync);
		osLib_addFunction("nn_save", "SAVEOpenDirOtherNormalApplicationVariationAsync", export_SAVEOpenDirOtherNormalApplicationVariationAsync);
	}

    void ResetToDefaultState()
    {
        if(g_nn_save->initialized)
        {
            SAVEUnmountSaveDir();
            g_nn_save->initialized = false;
        }
    }

}
}
