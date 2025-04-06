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

	bool GetCurrentTitleApplicationBox(nn::acp::ACPDeviceType* deviceType)
	{
		if (deviceType)
		{
			*deviceType = nn::acp::ACPDeviceType::InternalDeviceType;
			return true;
		}
		return false;
	}

	void UpdateSaveTimeStamp(uint32 persistentId)
	{
		nn::acp::ACPDeviceType deviceType;
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

	FS_RESULT GetAbsoluteFullPathOtherApplication(uint32 persistentId, uint64 titleId, const char* subDir, char* outPath)
	{
		uint32be applicationBox;
		if(acp::ACPGetApplicationBox(&applicationBox, titleId) != acp::ACPStatus::SUCCESS)
			return FS_RESULT::NOT_FOUND;

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
			return FS_RESULT::NOT_FOUND;

		if (written < SAVE_MAX_PATH_SIZE - 1)
			return FS_RESULT::SUCCESS;

		cemu_assert_suspicious();
		return FS_RESULT::FATAL_ERROR;
	}

	struct AsyncResultData
	{
		MEMPTR<coreinit::OSEvent> event;
		betype<SAVEStatus> returnStatus;
	};

	void SaveAsyncFinishCallback(PPCInterpreter_t* hCPU);

	struct AsyncToSyncWrapper : public FSAsyncParams
	{
		AsyncToSyncWrapper()
		{
			coreinit::OSInitEvent(&_event, coreinit::OSEvent::EVENT_STATE::STATE_NOT_SIGNALED, coreinit::OSEvent::EVENT_MODE::MODE_AUTO);
			ioMsgQueue = nullptr;
			userContext = &_result;
			userCallback = RPLLoader_MakePPCCallable(SaveAsyncFinishCallback);
			_result.returnStatus = 0;
			_result.event = &_event;
		}

		~AsyncToSyncWrapper()
		{

		}

		FSAsyncParams* GetAsyncParams()
		{
			return this;
		}

		SAVEStatus GetResult()
		{
			return _result.returnStatus;
		}

		void WaitForEvent()
		{
			coreinit::OSWaitEvent(&_event);
		}
	  private:
		coreinit::OSEvent _event;
		AsyncResultData _result;
	};

	void SaveAsyncFinishCallback(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(client, coreinit::FSClient_t, 0);
		ppcDefineParamMEMPTR(block, coreinit::FSCmdBlock_t, 1);
		ppcDefineParamU32(returnStatus, 2);
		ppcDefineParamMEMPTR(userContext, void, 3);

		MEMPTR<AsyncResultData> resultPtr{ userContext };
		resultPtr->returnStatus = returnStatus;
		coreinit::OSSignalEvent(resultPtr->event);

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

			iosu::acp::CreateSaveMetaFiles(ActiveSettings::GetPersistentId(), titleId);
		}

		return SAVE_STATUS_OK;
	}

	SAVEStatus SAVEInitSaveDir(uint8 accountSlot)
	{
		SAVEStatus result = (FSStatus)(FS_RESULT::FATAL_ERROR);
		OSLockMutex(&g_nn_save->mutex);

		uint32 persistentId;
		if (GetPersistentIdEx(accountSlot, &persistentId))
		{
			acp::ACPStatus status = nn::acp::ACPCreateSaveDir(persistentId, iosu::acp::ACPDeviceType::InternalDeviceType);
			result = ConvertACPToSaveStatus(status);
		}
		else
			result = (FSStatus)FS_RESULT::NOT_FOUND;

		OSUnlockMutex(&g_nn_save->mutex);
		return result;
	}

	SAVEStatus SAVEOpenDirAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, const char* path, FSDirHandlePtr hDir, FS_ERROR_MASK errHandling, const FSAsyncParams* asyncParams)
	{
		SAVEStatus result = (FSStatus)(FS_RESULT::FATAL_ERROR);

		OSLockMutex(&g_nn_save->mutex);

		uint32 persistentId;
		if (GetPersistentIdEx(accountSlot, &persistentId))
		{
			char fullPath[SAVE_MAX_PATH_SIZE];
			if (GetAbsoluteFullPath(persistentId, path, fullPath))
				result = coreinit::FSOpenDirAsync(client, block, fullPath, hDir, errHandling, (FSAsyncParams*)asyncParams);
			
		}
		else
			result = (FSStatus)FS_RESULT::NOT_FOUND;

		OSUnlockMutex(&g_nn_save->mutex);

		return result;
	}

	SAVEStatus SAVEOpenDir(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, const char* path, FSDirHandlePtr hDir, FS_ERROR_MASK errHandling)
	{
		StackAllocator<AsyncToSyncWrapper> asyncData;
		SAVEStatus status = SAVEOpenDirAsync(client, block, accountSlot, path, hDir, errHandling, asyncData->GetAsyncParams());
		if (status != (FSStatus)FS_RESULT::SUCCESS)
			return status;
		asyncData->WaitForEvent();
		return asyncData->GetResult();
	}

	SAVEStatus SAVEOpenDirOtherApplicationAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint64 titleId, uint8 accountSlot, const char* path, FSDirHandlePtr hDir, FS_ERROR_MASK errHandling, const FSAsyncParams* asyncParams)
	{
		SAVEStatus result = (FSStatus)(FS_RESULT::FATAL_ERROR);

		OSLockMutex(&g_nn_save->mutex);
		uint32 persistentId;
		if (GetPersistentIdEx(accountSlot, &persistentId))
		{
			char fullPath[SAVE_MAX_PATH_SIZE];
			if (GetAbsoluteFullPathOtherApplication(persistentId, titleId, path, fullPath) == FS_RESULT::SUCCESS)
				result = coreinit::FSOpenDirAsync(client, block, fullPath, hDir, errHandling, (FSAsyncParams*)asyncParams);
		}
		else
			result = (FSStatus)FS_RESULT::NOT_FOUND;
		OSUnlockMutex(&g_nn_save->mutex);

		return result;
	}

	SAVEStatus SAVEOpenDirOtherApplication(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint64 titleId, uint8 accountSlot, const char* path, FSDirHandlePtr hDir, FS_ERROR_MASK errHandling)
	{
		StackAllocator<AsyncToSyncWrapper> asyncData;
		SAVEStatus status = SAVEOpenDirOtherApplicationAsync(client, block, titleId, accountSlot, path, hDir, errHandling, asyncData->GetAsyncParams());
		if (status != (FSStatus)FS_RESULT::SUCCESS)
			return status;
		asyncData->WaitForEvent();
		return asyncData->GetResult();
	}

	SAVEStatus SAVEOpenDirOtherNormalApplicationAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint32 uniqueId, uint8 accountSlot, const char* path, FSDirHandlePtr hDir, FS_ERROR_MASK errHandling, const FSAsyncParams* asyncParams)
	{
		uint64 titleId = SAVE_UNIQUE_TO_TITLE_ID(uniqueId);
		return SAVEOpenDirOtherApplicationAsync(client, block, titleId, accountSlot, path, hDir, errHandling, asyncParams);
	}

	SAVEStatus SAVEOpenDirOtherNormalApplication(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint32 uniqueId, uint8 accountSlot, const char* path, FSDirHandlePtr hDir, FS_ERROR_MASK errHandling)
	{
		uint64 titleId = SAVE_UNIQUE_TO_TITLE_ID(uniqueId);
		return SAVEOpenDirOtherApplication(client, block, titleId, accountSlot, path, hDir, errHandling);
	}

	SAVEStatus SAVEOpenDirOtherNormalApplicationVariationAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint32 uniqueId, uint8 variation, uint8 accountSlot, const char* path, FSDirHandlePtr hDir, FS_ERROR_MASK errHandling, const FSAsyncParams* asyncParams)
	{
		uint64 titleId = SAVE_UNIQUE_TO_TITLE_ID_VARIATION(uniqueId, variation);
		return SAVEOpenDirOtherApplicationAsync(client, block, titleId, accountSlot, path, hDir, errHandling, asyncParams);
	}

	SAVEStatus SAVEOpenDirOtherNormalApplicationVariation(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint32 uniqueId, uint8 variation, uint8 accountSlot, const char* path, FSDirHandlePtr hDir, FS_ERROR_MASK errHandling)
	{
		uint64 titleId = SAVE_UNIQUE_TO_TITLE_ID_VARIATION(uniqueId, variation);
		return SAVEOpenDirOtherApplication(client, block, titleId, accountSlot, path, hDir, errHandling);
	}

	SAVEStatus SAVEOpenFileAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, const char* path, const char* mode, FSFileHandlePtr outFileHandle, FS_ERROR_MASK errHandling, const FSAsyncParams* asyncParams)
	{
		SAVEStatus result = (FSStatus)(FS_RESULT::FATAL_ERROR);

		OSLockMutex(&g_nn_save->mutex);

		uint32 persistentId;
		if (GetPersistentIdEx(accountSlot, &persistentId))
		{
			char fullPath[SAVE_MAX_PATH_SIZE];
			if (GetAbsoluteFullPath(persistentId, path, fullPath))
				result = coreinit::FSOpenFileAsync(client, block, fullPath, (char*)mode, outFileHandle, errHandling, (FSAsyncParams*)asyncParams);
		}
		else
			result = (FSStatus)FS_RESULT::NOT_FOUND;

		OSUnlockMutex(&g_nn_save->mutex);

		return result;
	}

	SAVEStatus SAVEOpenFileOtherApplicationAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint64 titleId, uint8 accountSlot, const char* path, const char* mode, FSFileHandlePtr outFileHandle, FS_ERROR_MASK errHandling, const FSAsyncParams* asyncParams)
	{
		if (strcmp(mode, "r") != 0)
			return (SAVEStatus)(FS_RESULT::PERMISSION_ERROR);

		SAVEStatus result = (FSStatus)(FS_RESULT::FATAL_ERROR);

		OSLockMutex(&g_nn_save->mutex);

		uint32 persistentId;
		if (GetPersistentIdEx(accountSlot, &persistentId))
		{
			char fullPath[SAVE_MAX_PATH_SIZE];
			if (GetAbsoluteFullPathOtherApplication(persistentId, titleId, path, fullPath) == FS_RESULT::SUCCESS)
				result = coreinit::FSOpenFileAsync(client, block, fullPath, (char*)mode, outFileHandle, errHandling, (FSAsyncParams*)asyncParams);
		}
		else
			result = (FSStatus)FS_RESULT::NOT_FOUND;

		OSUnlockMutex(&g_nn_save->mutex);

		return result;
	}

	SAVEStatus SAVEOpenFileOtherApplication(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint64 titleId, uint8 accountSlot, const char* path, const char* mode, FSFileHandlePtr outFileHandle, FS_ERROR_MASK errHandling)
	{
		StackAllocator<AsyncToSyncWrapper> asyncData;
		SAVEStatus status = SAVEOpenFileOtherApplicationAsync(client, block, titleId, accountSlot, path, mode, outFileHandle, errHandling, asyncData->GetAsyncParams());
		if (status != (FSStatus)FS_RESULT::SUCCESS)
			return status;
		asyncData->WaitForEvent();
		return asyncData->GetResult();
	}

	SAVEStatus SAVEOpenFileOtherNormalApplicationAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint32 uniqueId, uint8 accountSlot, const char* path, const char* mode, FSFileHandlePtr outFileHandle, FS_ERROR_MASK errHandling, const FSAsyncParams* asyncParams)
	{
		uint64 titleId = SAVE_UNIQUE_TO_TITLE_ID(uniqueId);
		return SAVEOpenFileOtherApplicationAsync(client, block, titleId, accountSlot, path, mode, outFileHandle, errHandling, asyncParams);
	}

	SAVEStatus SAVEOpenFileOtherNormalApplication(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint32 uniqueId, uint8 accountSlot, const char* path, const char* mode, FSFileHandlePtr outFileHandle, FS_ERROR_MASK errHandling)
	{
		uint64 titleId = SAVE_UNIQUE_TO_TITLE_ID(uniqueId);
		return SAVEOpenFileOtherApplication(client, block, titleId, accountSlot, path, mode, outFileHandle, errHandling);
	}

	SAVEStatus SAVEOpenFileOtherNormalApplicationVariationAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint32 uniqueId, uint8 variation, uint8 accountSlot, const char* path, const char* mode, FSFileHandlePtr outFileHandle, FS_ERROR_MASK errHandling, const FSAsyncParams* asyncParams)
	{
		uint64 titleId = SAVE_UNIQUE_TO_TITLE_ID_VARIATION(uniqueId, variation);
		return SAVEOpenFileOtherApplicationAsync(client, block, titleId, accountSlot, path, mode, outFileHandle, errHandling, asyncParams);
	}

	SAVEStatus SAVEOpenFileOtherNormalApplicationVariation(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint32 uniqueId, uint8 variation, uint8 accountSlot, const char* path, const char* mode, FSFileHandlePtr outFileHandle, FS_ERROR_MASK errHandling)
	{
		uint64 titleId = SAVE_UNIQUE_TO_TITLE_ID_VARIATION(uniqueId, variation);
		return SAVEOpenFileOtherApplication(client, block, titleId, accountSlot, path, mode, outFileHandle, errHandling);
	}

	SAVEStatus SAVEGetStatAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, const char* path, FSStat_t* stat, FS_ERROR_MASK errHandling, const FSAsyncParams* asyncParams)
	{
		SAVEStatus result = (FSStatus)(FS_RESULT::FATAL_ERROR);

		OSLockMutex(&g_nn_save->mutex);

		uint32 persistentId;
		if (GetPersistentIdEx(accountSlot, &persistentId))
		{
			char fullPath[SAVE_MAX_PATH_SIZE];
			if (GetAbsoluteFullPath(persistentId, path, fullPath))
				result = coreinit::__FSQueryInfoAsync(client, block, (uint8*)fullPath, FSA_QUERY_TYPE_STAT, stat, errHandling, (FSAsyncParams*)asyncParams); // FSGetStatAsync(...)
		}
		else
			result = (FSStatus)FS_RESULT::NOT_FOUND;

		OSUnlockMutex(&g_nn_save->mutex);

		return result;
	}

	SAVEStatus SAVEGetStat(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, const char* path, FSStat_t* stat, FS_ERROR_MASK errHandling)
	{
		StackAllocator<AsyncToSyncWrapper> asyncData;
		SAVEStatus status = SAVEGetStatAsync(client, block, accountSlot, path, stat, errHandling, asyncData->GetAsyncParams());
		if (status != (FSStatus)FS_RESULT::SUCCESS)
			return status;
		asyncData->WaitForEvent();
		return asyncData->GetResult();
	}

	SAVEStatus SAVEGetStatOtherApplicationAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint64 titleId, uint8 accountSlot, const char* path, FSStat_t* stat, FS_ERROR_MASK errHandling, const FSAsyncParams* asyncParams)
	{
		SAVEStatus result = (FSStatus)(FS_RESULT::FATAL_ERROR);

		OSLockMutex(&g_nn_save->mutex);

		uint32 persistentId;
		if (GetPersistentIdEx(accountSlot, &persistentId))
		{
			char fullPath[SAVE_MAX_PATH_SIZE];
			if (GetAbsoluteFullPathOtherApplication(persistentId, titleId, path, fullPath) == FS_RESULT::SUCCESS)
				result = coreinit::__FSQueryInfoAsync(client, block, (uint8*)fullPath, FSA_QUERY_TYPE_STAT, stat, errHandling, (FSAsyncParams*)asyncParams); // FSGetStatAsync(...)
		}
		else
			result = (FSStatus)FS_RESULT::NOT_FOUND;

		OSUnlockMutex(&g_nn_save->mutex);

		return result;
	}

	SAVEStatus SAVEGetStatOtherApplication(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint64 titleId, uint8 accountSlot, const char* path, FSStat_t* stat, FS_ERROR_MASK errHandling)
	{
		StackAllocator<AsyncToSyncWrapper> asyncData;
		SAVEStatus status = SAVEGetStatOtherApplicationAsync(client, block, titleId, accountSlot, path, stat, errHandling, asyncData->GetAsyncParams());
		if (status != (FSStatus)FS_RESULT::SUCCESS)
			return status;
		asyncData->WaitForEvent();
		return asyncData->GetResult();
	}

	SAVEStatus SAVEGetStatOtherNormalApplicationAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint32 uniqueId, uint8 accountSlot, const char* path, FSStat_t* stat, FS_ERROR_MASK errHandling, const FSAsyncParams* asyncParams)
	{
		uint64 titleId = SAVE_UNIQUE_TO_TITLE_ID(uniqueId);
		return SAVEGetStatOtherApplicationAsync(client, block, titleId, accountSlot, path, stat, errHandling, asyncParams);
	}

	SAVEStatus SAVEGetStatOtherNormalApplication(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint32 uniqueId, uint8 accountSlot, const char* path, FSStat_t* stat, FS_ERROR_MASK errHandling)
	{
		uint64 titleId = SAVE_UNIQUE_TO_TITLE_ID(uniqueId);
		return SAVEGetStatOtherApplication(client, block, titleId, accountSlot, path, stat, errHandling);
	}

	SAVEStatus SAVEGetStatOtherDemoApplicationAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint32 uniqueId, uint8 accountSlot, const char* path, FSStat_t* stat, FS_ERROR_MASK errHandling, const FSAsyncParams* asyncParams)
	{
		uint64 titleId = SAVE_UNIQUE_DEMO_TO_TITLE_ID(uniqueId);
		return SAVEGetStatOtherApplicationAsync(client, block, titleId, accountSlot, path, stat, errHandling, asyncParams);
	}

	SAVEStatus SAVEGetStatOtherNormalApplicationVariationAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint32 uniqueId, uint8 variation, uint8 accountSlot, const char* path, FSStat_t* stat, FS_ERROR_MASK errHandling, const FSAsyncParams* asyncParams)
	{
		uint64 titleId = SAVE_UNIQUE_TO_TITLE_ID_VARIATION(uniqueId, variation);
		return SAVEGetStatOtherApplicationAsync(client, block, titleId, accountSlot, path, stat, errHandling, asyncParams);
	}

	SAVEStatus SAVEGetStatOtherDemoApplicationVariationAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint32 uniqueId, uint8 variation, uint8 accountSlot, const char* path, FSStat_t* stat, FS_ERROR_MASK errHandling, const FSAsyncParams* asyncParams)
	{
		uint64 titleId = SAVE_UNIQUE_DEMO_TO_TITLE_ID_VARIATION(uniqueId, variation);
		return SAVEGetStatOtherApplicationAsync(client, block, titleId, accountSlot, path, stat, errHandling, asyncParams);
	}

	SAVEStatus SAVEGetStatOtherNormalApplicationVariation(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint32 uniqueId, uint8 variation, uint8 accountSlot, const char* path, FSStat_t* stat, FS_ERROR_MASK errHandling)
	{
		//peterBreak();

		uint64 titleId = SAVE_UNIQUE_TO_TITLE_ID_VARIATION(uniqueId, variation);
		return SAVEGetStatOtherApplication(client, block, titleId, accountSlot, path, stat, errHandling);
	}

	SAVEStatus SAVEGetFreeSpaceSizeAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, FSLargeSize* freeSize, FS_ERROR_MASK errHandling, const FSAsyncParams* asyncParams)
	{
		SAVEStatus result = (FSStatus)(FS_RESULT::FATAL_ERROR);

		OSLockMutex(&g_nn_save->mutex);

		uint32 persistentId;
		if (GetPersistentIdEx(accountSlot, &persistentId))
		{
			char fullPath[SAVE_MAX_PATH_SIZE];
			if (GetAbsoluteFullPath(persistentId, nullptr, fullPath))
				result = coreinit::FSGetFreeSpaceSizeAsync(client, block, fullPath, freeSize, errHandling, (FSAsyncParams*)asyncParams);
		}
		else
			result = (FSStatus)FS_RESULT::NOT_FOUND;

		OSUnlockMutex(&g_nn_save->mutex);

		return result;
	}

	SAVEStatus SAVEGetFreeSpaceSize(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, FSLargeSize* freeSize, FS_ERROR_MASK errHandling)
	{
		StackAllocator<AsyncToSyncWrapper> asyncData;
		SAVEStatus status = SAVEGetFreeSpaceSizeAsync(client, block, accountSlot, freeSize, errHandling, asyncData->GetAsyncParams());
		if (status != (FSStatus)FS_RESULT::SUCCESS)
			return status;
		asyncData->WaitForEvent();
		return asyncData->GetResult();
	}

	SAVEStatus SAVERemoveAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, const char* path, FS_ERROR_MASK errHandling, const FSAsyncParams* asyncParams)
	{
		SAVEStatus result = (FSStatus)(FS_RESULT::FATAL_ERROR);

		OSLockMutex(&g_nn_save->mutex);

		uint32 persistentId;
		if (GetPersistentIdEx(accountSlot, &persistentId))
		{
			char fullPath[SAVE_MAX_PATH_SIZE];
			if (GetAbsoluteFullPath(persistentId, path, fullPath))
				result = coreinit::FSRemoveAsync(client, block, (uint8*)fullPath, errHandling, (FSAsyncParams*)asyncParams);
		}
		else
			result = (FSStatus)FS_RESULT::NOT_FOUND;

		OSUnlockMutex(&g_nn_save->mutex);
		return result;
	}

	SAVEStatus SAVERemove(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, const char* path, FS_ERROR_MASK errHandling)
	{
		StackAllocator<AsyncToSyncWrapper> asyncData;
		SAVEStatus status = SAVERemoveAsync(client, block, accountSlot, path, errHandling, asyncData->GetAsyncParams());
		if (status != (FSStatus)FS_RESULT::SUCCESS)
			return status;
		asyncData->WaitForEvent();
		return asyncData->GetResult();
	}

	SAVEStatus SAVERenameAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, const char* oldPath, const char* newPath, FS_ERROR_MASK errHandling, FSAsyncParams* asyncParams)
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
					result = coreinit::FSRenameAsync(client, block, fullOldPath, fullNewPath, errHandling, asyncParams);
			}
		}
		else
			result = (FSStatus)FS_RESULT::NOT_FOUND;

		OSUnlockMutex(&g_nn_save->mutex);
		return result;
	}

	SAVEStatus SAVERename(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, const char* oldPath, const char* newPath, FS_ERROR_MASK errHandling)
	{
		StackAllocator<AsyncToSyncWrapper> asyncData;
		SAVEStatus status = SAVERenameAsync(client, block, accountSlot, oldPath, newPath, errHandling, asyncData->GetAsyncParams());
		if (status != (FSStatus)FS_RESULT::SUCCESS)
			return status;
		asyncData->WaitForEvent();
		return asyncData->GetResult();
	}

	SAVEStatus SAVEMakeDirAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, const char* path, FS_ERROR_MASK errHandling, const FSAsyncParams* asyncParams)
	{
		SAVEStatus result = (FSStatus)(FS_RESULT::FATAL_ERROR);

		OSLockMutex(&g_nn_save->mutex);

		uint32 persistentId;
		if (GetPersistentIdEx(accountSlot, &persistentId))
		{
			char fullPath[SAVE_MAX_PATH_SIZE];
			if (GetAbsoluteFullPath(persistentId, path, fullPath))
				result = coreinit::FSMakeDirAsync(client, block, fullPath, errHandling, (FSAsyncParams*)asyncParams);
		}
		else
			result = (FSStatus)FS_RESULT::NOT_FOUND;

		OSUnlockMutex(&g_nn_save->mutex);

		return result;
	}

	SAVEStatus SAVEMakeDir(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, const char* path, FS_ERROR_MASK errHandling)
	{
		StackAllocator<AsyncToSyncWrapper> asyncData;
		SAVEStatus status = SAVEMakeDirAsync(client, block, accountSlot, path, errHandling, asyncData->GetAsyncParams());
		if (status != (FSStatus)FS_RESULT::SUCCESS)
			return status;
		asyncData->WaitForEvent();
		return asyncData->GetResult();
	}

	SAVEStatus SAVEOpenFile(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, const char* path, const char* mode, FSFileHandlePtr outFileHandle, FS_ERROR_MASK errHandling)
	{
		StackAllocator<AsyncToSyncWrapper> asyncData;
		SAVEStatus status = SAVEOpenFileAsync(client, block, accountSlot, path, mode, outFileHandle, errHandling, asyncData->GetAsyncParams());
		if (status != (FSStatus)FS_RESULT::SUCCESS)
			return status;
		asyncData->WaitForEvent();
		return asyncData->GetResult();
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

	SAVEStatus SAVEGetSharedSaveDataPath(uint64 titleId, const char* dataFileName, char* output, uint32 outputLength)
	{
		SAVEStatus result = (FSStatus)(FS_RESULT::FATAL_ERROR);
		int written = snprintf(output, outputLength, "/vol/storage_mlc01/usr/save/%08x/%08x/user/common/%s", GetTitleIdHigh(titleId), GetTitleIdLow(titleId), dataFileName);
		if (written >= 0 && written < (sint32)outputLength)
			result = (FSStatus)FS_RESULT::SUCCESS;
		cemu_assert_debug(result != (FSStatus)(FS_RESULT::FATAL_ERROR));
		return result;
	}

	SAVEStatus SAVEChangeDirAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, const char* path, FS_ERROR_MASK errHandling, const FSAsyncParams* asyncParams)
	{
		SAVEStatus result = (FSStatus)(FS_RESULT::FATAL_ERROR);

		OSLockMutex(&g_nn_save->mutex);
		uint32 persistentId;
		if (GetPersistentIdEx(accountSlot, &persistentId))
		{
			char fullPath[SAVE_MAX_PATH_SIZE];
			if (GetAbsoluteFullPath(persistentId, path, fullPath))
				result = coreinit::FSChangeDirAsync(client, block, fullPath, errHandling, (FSAsyncParams*)asyncParams);
		}
		else
			result = (FSStatus)FS_RESULT::NOT_FOUND;
		OSUnlockMutex(&g_nn_save->mutex);

		return result;
	}

	SAVEStatus SAVEChangeDir(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, const char* path, FS_ERROR_MASK errHandling)
	{
		StackAllocator<AsyncToSyncWrapper> asyncData;
		SAVEStatus status = SAVEChangeDirAsync(client, block, accountSlot, path, errHandling, asyncData->GetAsyncParams());
		if (status != (FSStatus)FS_RESULT::SUCCESS)
			return status;
		asyncData->WaitForEvent();
		return asyncData->GetResult();
	}

	SAVEStatus SAVEFlushQuotaAsync(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot, FS_ERROR_MASK errHandling, const FSAsyncParams* asyncParams)
	{
		SAVEStatus result = (FSStatus)(FS_RESULT::FATAL_ERROR);

		OSLockMutex(&g_nn_save->mutex);
		uint32 persistentId;
		if (GetPersistentIdEx(accountSlot, &persistentId))
		{
			char fullPath[SAVE_MAX_PATH_SIZE];
			if (GetAbsoluteFullPath(persistentId, nullptr, fullPath))
			{
				result = coreinit::FSFlushQuotaAsync(client, block, fullPath, errHandling, (FSAsyncParams*)asyncParams);
				// if(OSGetUPID != 0xF)
				UpdateSaveTimeStamp(persistentId);
			}
		}
		else
			result = (FSStatus)FS_RESULT::NOT_FOUND;
		OSUnlockMutex(&g_nn_save->mutex);

		return result;
	}

	SAVEStatus SAVEFlushQuota(coreinit::FSClient_t* client, coreinit::FSCmdBlock_t* block, uint8 accountSlot,  FS_ERROR_MASK errHandling)
	{
		StackAllocator<AsyncToSyncWrapper> asyncData;
		SAVEStatus status = SAVEFlushQuotaAsync(client, block, accountSlot, errHandling, asyncData->GetAsyncParams());
		if (status != (FSStatus)FS_RESULT::SUCCESS)
			return status;
		asyncData->WaitForEvent();
		return asyncData->GetResult();
	}

	void load()
	{
		cafeExportRegister("nn_save", SAVEInit, LogType::Save);
		cafeExportRegister("nn_save", SAVEInitSaveDir, LogType::Save);

		cafeExportRegister("nn_save", SAVEGetSharedDataTitlePath, LogType::Save);
		cafeExportRegister("nn_save", SAVEGetSharedSaveDataPath, LogType::Save);

		cafeExportRegister("nn_save", SAVEGetFreeSpaceSize, LogType::Save);
		cafeExportRegister("nn_save", SAVEGetFreeSpaceSizeAsync, LogType::Save);
		cafeExportRegister("nn_save", SAVEMakeDir, LogType::Save);
		cafeExportRegister("nn_save", SAVEMakeDirAsync, LogType::Save);
		cafeExportRegister("nn_save", SAVERemove, LogType::Save);
		cafeExportRegister("nn_save", SAVERemoveAsync, LogType::Save);
		cafeExportRegister("nn_save", SAVEChangeDir, LogType::Save);
		cafeExportRegister("nn_save", SAVEChangeDirAsync, LogType::Save);
		cafeExportRegister("nn_save", SAVERename, LogType::Save);
		cafeExportRegister("nn_save", SAVERenameAsync, LogType::Save);
		cafeExportRegister("nn_save", SAVEFlushQuota, LogType::Save);
		cafeExportRegister("nn_save", SAVEFlushQuotaAsync, LogType::Save);

		cafeExportRegister("nn_save", SAVEGetStat, LogType::Save);
		cafeExportRegister("nn_save", SAVEGetStatAsync, LogType::Save);
		cafeExportRegister("nn_save", SAVEGetStatOtherApplication, LogType::Save);
		cafeExportRegister("nn_save", SAVEGetStatOtherApplicationAsync, LogType::Save);
		cafeExportRegister("nn_save", SAVEGetStatOtherNormalApplication, LogType::Save);
		cafeExportRegister("nn_save", SAVEGetStatOtherNormalApplicationAsync, LogType::Save);
		cafeExportRegister("nn_save", SAVEGetStatOtherNormalApplicationVariation, LogType::Save);
		cafeExportRegister("nn_save", SAVEGetStatOtherNormalApplicationVariationAsync, LogType::Save);

		cafeExportRegister("nn_save", SAVEOpenFile, LogType::Save);
		cafeExportRegister("nn_save", SAVEOpenFileAsync, LogType::Save);
		cafeExportRegister("nn_save", SAVEOpenFileOtherApplication, LogType::Save);
		cafeExportRegister("nn_save", SAVEOpenFileOtherApplicationAsync, LogType::Save);
		cafeExportRegister("nn_save", SAVEOpenFileOtherNormalApplication, LogType::Save);
		cafeExportRegister("nn_save", SAVEOpenFileOtherNormalApplicationAsync, LogType::Save);
		cafeExportRegister("nn_save", SAVEOpenFileOtherNormalApplicationVariation, LogType::Save);
		cafeExportRegister("nn_save", SAVEOpenFileOtherNormalApplicationVariationAsync, LogType::Save);

		cafeExportRegister("nn_save", SAVEOpenDir, LogType::Save);
		cafeExportRegister("nn_save", SAVEOpenDirAsync, LogType::Save);
		cafeExportRegister("nn_save", SAVEOpenDirOtherApplication, LogType::Save);
		cafeExportRegister("nn_save", SAVEOpenDirOtherApplicationAsync, LogType::Save);
		cafeExportRegister("nn_save", SAVEOpenDirOtherNormalApplication, LogType::Save);
		cafeExportRegister("nn_save", SAVEOpenDirOtherNormalApplicationVariation, LogType::Save);
		cafeExportRegister("nn_save", SAVEOpenDirOtherNormalApplicationAsync, LogType::Save);
		cafeExportRegister("nn_save", SAVEOpenDirOtherNormalApplicationVariationAsync, LogType::Save);
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
