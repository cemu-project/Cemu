#include "Cafe/OS/common/OSCommon.h"
#include "nn_save.h"

#include "Cafe/OS/libs/nn_acp/nn_acp.h"
#include "Cafe/OS/libs/nn_act/nn_act.h"

#include <filesystem>
#include <sstream>
#include <thread>
#include "config/ActiveSettings.h"
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"
#include "Cafe/OS/libs/coreinit/coreinit_FS.h"
#include "Cafe/CafeSystem.h"
#include "Cafe/Filesystem/fsc.h"
#include "Cafe/HW/Latte/Core/LatteOverlay.h"

#if BOOST_OS_WINDOWS
#include <Windows.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdlib>
#include <vector>
extern char** environ;
#endif

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


	// Host path of the local save directory: <mlc>/usr/save/<high>/<low>/
	fs::path CloudSync_GetLocalSaveDir(uint32 high, uint32 low)
	{
		char subDir[64];
		sprintf(subDir, "usr/save/%08x/%08x", high, low);
		return ActiveSettings::GetMlcPath(subDir);
	}

	bool CloudSync_LocalSaveDirExists(uint32 high, uint32 low)
	{
		std::error_code ec;
		return fs::is_directory(CloudSync_GetLocalSaveDir(high, low), ec);
	}

	// Dropbox:Cemu Cloud Saves/<Game Name> (<Game ID>)
	std::string CloudSync_GetRemotePath(uint64 titleId)
	{
		std::string gameName = CafeSystem::GetForegroundTitleName();
		for (char& c : gameName)
		{
			if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
				c = '_';
		}
		if (gameName.empty())
			gameName = "Unknown Game";

		return fmt::format("Dropbox:Cemu Cloud Saves/{} ({:016x})", gameName, titleId);
	}

#if BOOST_OS_WINDOWS
	// Runs rclone and blocks the calling thread until it finishes. Returns the process exit code, or -1 on launch failure.
	sint32 CloudSync_RunRcloneBlocking(std::wstring cmdLine)
	{
		STARTUPINFOW si{};
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;
		PROCESS_INFORMATION pi{};

		if (!CreateProcessW(nullptr, cmdLine.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
		{
			cemuLog_log(LogType::Save, "CloudSync: failed to launch rclone, error code {}", (uint32)GetLastError());
			return -1;
		}

		WaitForSingleObject(pi.hProcess, INFINITE);
		DWORD exitCode = 0;
		GetExitCodeProcess(pi.hProcess, &exitCode);
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		return (sint32)exitCode;
	}
#else
	// AppImages mount their payload as a squashfs and typically don't expose the host's real
	// $PATH to child processes the way a normally installed binary would, so a plain execvp("rclone", ...)
	// often fails to find a system-installed rclone. We instead probe a fixed list of common install
	// locations directly. /proc/1/root re-anchors the lookup at PID 1's root filesystem, which lets us
	// reach the real host filesystem even if we're running inside a mount namespace/sandbox that
	// otherwise hides it (e.g. some AppImage integrations, Steam Deck's read-only rootfs).
	std::string CloudSync_FindRclonePath()
	{
		std::vector<std::string> candidates = {
			"/usr/bin/rclone",
			"/usr/local/bin/rclone",
			"/var/lib/flatpak/exports/bin/rclone",
		};
		if (const char* home = getenv("HOME"))
		{
			candidates.emplace_back(std::string(home) + "/bin/rclone");
			candidates.emplace_back(std::string(home) + "/.local/bin/rclone");
		}

		const std::vector<std::string> baseCandidates = candidates;
		for (const std::string& path : baseCandidates)
			candidates.push_back("/proc/1/root" + path);

		for (const std::string& path : candidates)
		{
			if (access(path.c_str(), X_OK) == 0)
				return path;
		}
		return {};
	}

	// Builds a copy of the current environment with AppImage-specific variables stripped.
	// Cemu's AppImage sets APPDIR/APPIMAGE/OWD and may prepend its bundled lib dir to
	// LD_LIBRARY_PATH; if that leaks into rclone's process it can load Cemu-bundled versions of
	// shared libs (e.g. libssl/libcrypto) that don't match what rclone was linked/tested against,
	// causing obscure TLS/certificate failures. Stripping them lets rclone resolve its own
	// dependencies from the host system as if launched outside the AppImage.
	std::vector<std::string> CloudSync_BuildCleanEnv()
	{
		static const std::vector<std::string> blockedVars = {
			"LD_LIBRARY_PATH", "LD_PRELOAD", "APPDIR", "APPIMAGE", "OWD", "ARGV0"
		};

		std::vector<std::string> env;
		for (char** envp = environ; *envp != nullptr; ++envp)
		{
			std::string entry(*envp);
			const size_t eq = entry.find('=');
			const std::string key = (eq == std::string::npos) ? entry : entry.substr(0, eq);

			bool blocked = false;
			for (const std::string& blockedVar : blockedVars)
			{
				if (key == blockedVar)
				{
					blocked = true;
					break;
				}
			}
			if (!blocked)
				env.push_back(std::move(entry));
		}
		return env;
	}

	// Runs rclone via posix_spawn and blocks the calling thread until it finishes.
	// Returns the process exit code, or -1 if rclone couldn't be found/launched.
	sint32 CloudSync_RunRcloneBlocking(const std::vector<std::string>& args)
	{
		const std::string rclonePath = CloudSync_FindRclonePath();
		if (rclonePath.empty())
		{
			cemuLog_log(LogType::Save, "CloudSync: could not find an rclone executable");
			return -1;
		}

		std::vector<char*> argv;
		argv.push_back(const_cast<char*>(rclonePath.c_str()));
		for (const std::string& arg : args)
			argv.push_back(const_cast<char*>(arg.c_str()));
		argv.push_back(nullptr);

		std::vector<std::string> envStorage = CloudSync_BuildCleanEnv();
		std::vector<char*> envp;
		for (const std::string& entry : envStorage)
			envp.push_back(const_cast<char*>(entry.c_str()));
		envp.push_back(nullptr);

		pid_t pid;
		if (posix_spawn(&pid, rclonePath.c_str(), nullptr, nullptr, argv.data(), envp.data()) != 0)
		{
			cemuLog_log(LogType::Save, "CloudSync: failed to spawn rclone at {}", rclonePath);
			return -1;
		}

		int status = 0;
		waitpid(pid, &status, 0);
		if (WIFEXITED(status))
			return WEXITSTATUS(status);
		return -1;
	}
#endif

	// Pushes the local save directory for the current title to Dropbox. Fire-and-forget on a background thread.
	void CloudSync_PushSaveToDropbox(uint32 high, uint32 low, uint64 titleId)
	{
#if BOOST_OS_WINDOWS
		std::string localPath = _pathToUtf8(CloudSync_GetLocalSaveDir(high, low));
		std::string remotePath = CloudSync_GetRemotePath(titleId);

		std::wstring cmdLine = L"rclone copy \"" + boost::nowide::widen(localPath) + L"\" \"" + boost::nowide::widen(remotePath) + L"\" -v";
		cmdLine.push_back(L'\0'); // CreateProcessW requires a writable, mutable buffer

		std::thread([cmdLine = std::move(cmdLine)]() mutable {
			sint32 exitCode = CloudSync_RunRcloneBlocking(std::move(cmdLine));
			cemuLog_log(LogType::Save, "CloudSync: rclone push finished with exit code {}", exitCode);
			if (exitCode == 0)
				LatteOverlay_pushNotification("Save pushed to Dropbox", 3000);
			else
				LatteOverlay_pushNotification("CloudSync: push to Dropbox failed", 5000);
		}).detach();
#else
		std::string localPath = _pathToUtf8(CloudSync_GetLocalSaveDir(high, low));
		std::string remotePath = CloudSync_GetRemotePath(titleId);

		std::thread([localPath, remotePath]() {
			sint32 exitCode = CloudSync_RunRcloneBlocking({"copy", localPath, remotePath, "-v"});
			cemuLog_log(LogType::Save, "CloudSync: rclone push finished with exit code {}", exitCode);
			if (exitCode == 0)
				LatteOverlay_pushNotification("Save pushed to Dropbox", 3000);
			else
				LatteOverlay_pushNotification("CloudSync: push to Dropbox failed", 5000);
		}).detach();
#endif
	}

	// Pulls the save directory down from Dropbox. Runs synchronously (blocking) since the game
	// is about to start reading save data right after SAVEInit returns.
	void CloudSync_PullSaveFromDropbox(uint32 high, uint32 low, uint64 titleId)
	{
#if BOOST_OS_WINDOWS
		std::string localPath = _pathToUtf8(CloudSync_GetLocalSaveDir(high, low));
		std::string remotePath = CloudSync_GetRemotePath(titleId);

		cemuLog_log(LogType::Save, "CloudSync: no local save dir found, pulling from {}", remotePath);

		std::wstring cmdLine = L"rclone copy \"" + boost::nowide::widen(remotePath) + L"\" \"" + boost::nowide::widen(localPath) + L"\" -v";
		cmdLine.push_back(L'\0'); // CreateProcessW requires a writable, mutable buffer

		sint32 exitCode = CloudSync_RunRcloneBlocking(std::move(cmdLine));
		cemuLog_log(LogType::Save, "CloudSync: rclone pull finished with exit code {}", exitCode);
		if (exitCode == 0)
			LatteOverlay_pushNotification("Save pulled from Dropbox", 3000);
		else
			LatteOverlay_pushNotification("CloudSync: pull from Dropbox failed", 5000);
#else
		std::string localPath = _pathToUtf8(CloudSync_GetLocalSaveDir(high, low));
		std::string remotePath = CloudSync_GetRemotePath(titleId);

		cemuLog_log(LogType::Save, "CloudSync: no local save dir found, pulling from {}", remotePath);

		sint32 exitCode = CloudSync_RunRcloneBlocking({"copy", remotePath, localPath, "-v"});
		cemuLog_log(LogType::Save, "CloudSync: rclone pull finished with exit code {}", exitCode);
		if (exitCode == 0)
			LatteOverlay_pushNotification("Save pulled from Dropbox", 3000);
		else
			LatteOverlay_pushNotification("CloudSync: pull from Dropbox failed", 5000);
#endif
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

			uint32 high = GetTitleIdHigh(titleId) & (~0xC);
			uint32 low = GetTitleIdLow(titleId);

			if (!CloudSync_LocalSaveDirExists(high, low))
				CloudSync_PullSaveFromDropbox(high, low, titleId);

			sint32 fscStatus = FSC_STATUS_FILE_NOT_FOUND;
			char path[256];
			sprintf(path, "%susr/save/%08x/", "/vol/storage_mlc01/", high);
			fsc_createDir(path, &fscStatus);
			sprintf(path, "%susr/save/%08x/%08x/", "/vol/storage_mlc01/", high, low);
			fsc_createDir(path, &fscStatus);
			sprintf(path, "%susr/save/%08x/%08x/meta/", "/vol/storage_mlc01/", high, low);
			fsc_createDir(path, &fscStatus);

			iosu::acp::CreateSaveMetaFiles(ActiveSettings::GetPersistentId(), titleId);

			CloudSync_PushSaveToDropbox(high, low, titleId);
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

    void ResetToDefaultState()
    {
        if(g_nn_save->initialized)
        {
            SAVEUnmountSaveDir();
            g_nn_save->initialized = false;
        }
    }

	class : public COSModule
	{
		public:
		std::string_view GetName() override
		{
			return "nn_save";
		}

		void RPLMapped() override
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
		};

		void rpl_entry(uint32 moduleHandle, coreinit::RplEntryReason reason) override
		{
			if (reason == coreinit::RplEntryReason::Loaded)
			{
				ResetToDefaultState();
			}
			else if (reason == coreinit::RplEntryReason::Unloaded)
			{
				ResetToDefaultState();
			}
		}
	}s_COSnnSaveModule;

	COSModule* GetModule()
	{
		return &s_COSnnSaveModule;
	}

}
}
