#include "Cafe/OS/common/OSCommon.h"
#include "gui/wxgui.h"
#include "Cafe/OS/libs/gx2/GX2.h"
#include "Cafe/GameProfile/GameProfile.h"
#include "Cafe/HW/Espresso/Interpreter/PPCInterpreterInternal.h"
#include "Cafe/HW/Espresso/Recompiler/PPCRecompiler.h"
#include "Cafe/HW/Espresso/Debugger/Debugger.h"
#include "Cafe/OS/RPL/rpl_symbol_storage.h"
#include "audio/IAudioAPI.h"
#include "audio/IAudioInputAPI.h"
#include "config/ActiveSettings.h"
#include "Cafe/TitleList/GameInfo.h"
#include "Cafe/GraphicPack/GraphicPack2.h"
#include "util/helpers/SystemException.h"
#include "Common/cpu_features.h"
#include "input/InputManager.h"
#include "Cafe/CafeSystem.h"
#include "Cafe/TitleList/TitleList.h"
#include "Cafe/TitleList/GameInfo.h"
#include "Cafe/OS/libs/coreinit/coreinit_Alarm.h"
#include "Cafe/OS/libs/snd_core/ax.h"
#include "Cafe/OS/RPL/rpl.h"
#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/Filesystem/FST/FST.h"
#include "Common/FileStream.h"
#include "GamePatch.h"
#include "HW/Espresso/Debugger/GDBStub.h"

#include "Cafe/IOSU/legacy/iosu_ioctl.h"
#include "Cafe/IOSU/legacy/iosu_act.h"
#include "Cafe/IOSU/legacy/iosu_fpd.h"
#include "Cafe/IOSU/legacy/iosu_crypto.h"
#include "Cafe/IOSU/legacy/iosu_mcp.h"
#include "Cafe/IOSU/legacy/iosu_acp.h"
#include "Cafe/IOSU/legacy/iosu_boss.h"
#include "Cafe/IOSU/legacy/iosu_nim.h"
#include "Cafe/IOSU/PDM/iosu_pdm.h"
#include "Cafe/IOSU/ccr_nfc/iosu_ccr_nfc.h"

// IOSU initializer functions
#include "Cafe/IOSU/kernel/iosu_kernel.h"
#include "Cafe/IOSU/fsa/iosu_fsa.h"
#include "Cafe/IOSU/ODM/iosu_odm.h"

// Cafe OS initializer and shutdown functions
#include "Cafe/OS/libs/avm/avm.h"
#include "Cafe/OS/libs/drmapp/drmapp.h"
#include "Cafe/OS/libs/TCL/TCL.h"
#include "Cafe/OS/libs/snd_user/snd_user.h"
#include "Cafe/OS/libs/h264_avc/h264dec.h"
#include "Cafe/OS/libs/snd_core/ax.h"
#include "Cafe/OS/libs/gx2/GX2.h"
#include "Cafe/OS/libs/gx2/GX2_Misc.h"
#include "Cafe/OS/libs/mic/mic.h"
#include "Cafe/OS/libs/nfc/nfc.h"
#include "Cafe/OS/libs/ntag/ntag.h"
#include "Cafe/OS/libs/nn_aoc/nn_aoc.h"
#include "Cafe/OS/libs/nn_pdm/nn_pdm.h"
#include "Cafe/OS/libs/nn_cmpt/nn_cmpt.h"
#include "Cafe/OS/libs/nn_ccr/nn_ccr.h"
#include "Cafe/OS/libs/nn_temp/nn_temp.h"
#include "Cafe/OS/libs/nn_save/nn_save.h"

// HW interfaces
#include "Cafe/HW/SI/si.h"

// dependency to be removed
#include "gui/guiWrapper.h"

#include <time.h>

#if BOOST_OS_LINUX
#include <sys/sysinfo.h>
#elif BOOST_OS_MACOS
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

std::string _pathToExecutable;
std::string _pathToBaseExecutable;

RPLModule* applicationRPX = nullptr;
uint32 currentBaseApplicationHash = 0;
uint32 currentUpdatedApplicationHash = 0;

bool isLaunchTypeELF = false;

MPTR _entryPoint = MPTR_NULL;

uint32 generateHashFromRawRPXData(uint8* rpxData, sint32 size)
{
	uint32 h = 0x3416DCBF;
	for (sint32 i = 0; i < size; i++)
	{
		uint32 c = rpxData[i];
		h = (h << 3) | (h >> 29);
		h += c;
	}
	return h;
}

bool ScanForRPX()
{
	bool rpxFound = false;
	sint32 fscStatus = 0;
	FSCVirtualFile* fscDirItr = fsc_openDirIterator("/internal/current_title/code/", &fscStatus);
	if (fscDirItr)
	{
		FSCDirEntry dirEntry;
		while (fsc_nextDir(fscDirItr, &dirEntry))
		{
			sint32 dirItrPathLen = strlen(dirEntry.path);
			if (dirItrPathLen < 4)
				continue;
			if (boost::iequals(dirEntry.path + dirItrPathLen - 4, ".rpx"))
			{
				rpxFound = true;
				_pathToExecutable = fmt::format("/internal/current_title/code/{}", dirEntry.path);
				break;
			}
		}
		fsc_close(fscDirItr);
	}
	return rpxFound;
}

void SetEntryPoint(MPTR entryPoint)
{
	_entryPoint = entryPoint;
}

// load executable into virtual memory and set entrypoint
void LoadMainExecutable()
{
	isLaunchTypeELF = false;
	// when launching from a disc image _pathToExecutable is initially empty
	if (_pathToExecutable.empty())
	{
		// try to get the RPX path from the meta files
		// todo
		// otherwise search for first file with .rpx extension in the code folder
		if (!ScanForRPX())
		{
			cemuLog_log(LogType::Force, "Unable to find RPX executable");
			cemuLog_waitForFlush();
			cemu_assert(false);
		}
	}
	// extract and load RPX
	uint32 rpxSize = 0;
	uint8* rpxData = fsc_extractFile(_pathToExecutable.c_str(), &rpxSize);
	if (rpxData == nullptr)
	{
		cemuLog_log(LogType::Force, "Failed to load \"{}\"", _pathToExecutable);
		cemuLog_waitForFlush();
		cemu_assert(false);
	}
	currentUpdatedApplicationHash = generateHashFromRawRPXData(rpxData, rpxSize);
	// determine if this file is an ELF
	const uint8 elfHeaderMagic[9] = { 0x7F,0x45,0x4C,0x46,0x01,0x02,0x01,0x00,0x00 };
	if (rpxSize >= 10 && memcmp(rpxData, elfHeaderMagic, sizeof(elfHeaderMagic)) == 0)
	{
		// ELF
		SetEntryPoint(ELF_LoadFromMemory(rpxData, rpxSize, _pathToExecutable.c_str()));
		isLaunchTypeELF = true;
	}
	else
	{
		// RPX
		RPLLoader_AddDependency(_pathToExecutable.c_str());
		applicationRPX = RPLLoader_LoadFromMemory(rpxData, rpxSize, (char*)_pathToExecutable.c_str());
		if (!applicationRPX)
		{
			wxMessageBox(_("Failed to run this title because the executable is damaged"));
			cemuLog_createLogFile(false);
			cemuLog_waitForFlush();
			exit(0);
		}
		RPLLoader_SetMainModule(applicationRPX);
		SetEntryPoint(RPLLoader_GetModuleEntrypoint(applicationRPX));
	}
	free(rpxData);
	// get RPX hash of game without update
	uint32 baseRpxSize = 0;
	uint8* baseRpxData = fsc_extractFile(!_pathToBaseExecutable.empty() ? _pathToBaseExecutable.c_str() : _pathToExecutable.c_str(), &baseRpxSize, FSC_PRIORITY_BASE);
	if (baseRpxData == nullptr)
	{
		currentBaseApplicationHash = currentUpdatedApplicationHash;
	}
	else
	{
		currentBaseApplicationHash = generateHashFromRawRPXData(baseRpxData, baseRpxSize);
	}
	free(baseRpxData);
	debug_printf("RPXHash: 0x%08x\n", currentBaseApplicationHash);
}

fs::path getTitleSavePath()
{
	const uint64 titleId = CafeSystem::GetForegroundTitleId();
	return ActiveSettings::GetMlcPath("usr/save/{:08X}/{:08X}/user/", (uint32)(titleId >> 32), (uint32)(titleId & 0xFFFFFFFF));
}

void InfoLog_TitleLoaded()
{
	cemuLog_createLogFile(false);
	uint64 titleId = CafeSystem::GetForegroundTitleId();
	cemuLog_log(LogType::Force, "------- Loaded title -------");
	cemuLog_log(LogType::Force, "TitleId: {:08x}-{:08x}", (uint32)(titleId >> 32), (uint32)(titleId & 0xFFFFFFFF));
	cemuLog_log(LogType::Force, "TitleVersion: v{}", CafeSystem::GetForegroundTitleVersion());
	CafeConsoleRegion region = CafeSystem::GetForegroundTitleRegion();
	if(region == CafeConsoleRegion::JPN)
		cemuLog_log(LogType::Force, "TitleRegion: JP");
	else if (region == CafeConsoleRegion::EUR)
		cemuLog_log(LogType::Force, "TitleRegion: EU");
	else if (region == CafeConsoleRegion::USA)
		cemuLog_log(LogType::Force, "TitleRegion: US");

	fs::path effectiveSavePath = getTitleSavePath();
	std::error_code ec;
	const bool saveDirExists = fs::exists(effectiveSavePath, ec);
	cemuLog_log(LogType::Force, "Save path:   {}{}", _pathToUtf8(effectiveSavePath), saveDirExists ? "" : " (not present)");

	// log shader cache name
	cemuLog_log(LogType::Force, "Shader cache file: shaderCache/transferable/{:016x}.bin", titleId);
	// game profile info
	std::string gameProfilePath;
	if(g_current_game_profile->IsDefaultProfile())
		gameProfilePath = fmt::format("gameProfiles/default/{:016x}.ini", titleId);
	else
		gameProfilePath = fmt::format("gameProfiles/{:016x}.ini", titleId);
	cemuLog_log(LogType::Force, "gameprofile path: {}", g_current_game_profile->IsLoaded() ? gameProfilePath : std::string(" (not present)"));
	// rpx hash of updated game
	cemuLog_log(LogType::Force, "RPX hash (updated): {:08x}", currentUpdatedApplicationHash);
	cemuLog_log(LogType::Force, "RPX hash (base): {:08x}", currentBaseApplicationHash);

	memory_logModifiedMemoryRanges();
}

void InfoLog_PrintActiveSettings()
{
	const auto& config = GetConfig();
	cemuLog_log(LogType::Force, "------- Active settings -------");

	// settings to log:
	cemuLog_log(LogType::Force, "CPU-Mode: {}{}", fmt::format("{}", ActiveSettings::GetCPUMode()).c_str(), g_current_game_profile->GetCPUMode().has_value() ? " (gameprofile)" : "");
	cemuLog_log(LogType::Force, "Load shared libraries: {}{}", ActiveSettings::LoadSharedLibrariesEnabled() ? "true" : "false", g_current_game_profile->ShouldLoadSharedLibraries().has_value() ? " (gameprofile)" : "");
	cemuLog_log(LogType::Force, "Use precompiled shaders: {}{}", fmt::format("{}", ActiveSettings::GetPrecompiledShadersOption()), g_current_game_profile->GetPrecompiledShadersState().has_value() ? " (gameprofile)" : "");
	cemuLog_log(LogType::Force, "Full sync at GX2DrawDone: {}", ActiveSettings::WaitForGX2DrawDoneEnabled() ? "true" : "false");
	cemuLog_log(LogType::Force, "Strict shader mul: {}", g_current_game_profile->GetAccurateShaderMul() == AccurateShaderMulOption::True ? "true" : "false");
	if (ActiveSettings::GetGraphicsAPI() == GraphicAPI::kVulkan)
	{
		cemuLog_log(LogType::Force, "Async compile: {}", GetConfig().async_compile.GetValue() ? "true" : "false");
		if(!GetConfig().vk_accurate_barriers.GetValue())
			cemuLog_log(LogType::Force, "Accurate barriers are disabled!");
	}
	cemuLog_log(LogType::Force, "Console language: {}", stdx::to_underlying(config.console_language.GetValue()));
}

struct SharedDataEntry
{
	/* +0x00 */ uint32be name;
	/* +0x04 */ uint32be fileType; // 2 = font
	/* +0x08 */ uint32be kernelFilenamePtr;
	/* +0x0C */ MEMPTR<void> data;
	/* +0x10 */ uint32be size;
	/* +0x14 */ uint32be ukn14;
	/* +0x18 */ uint32be ukn18;
};

struct
{
	uint32 name;
	uint32 fileType;
	const char* fileName;
	const char* resourcePath;
	const char* mlcPath;
}shareddataDef[] =
{
	0xFFCAFE01, 2, "CafeCn.ttf", "resources/sharedFonts/CafeCn.ttf", "sys/title/0005001b/10042400/content/CafeCn.ttf",
	0xFFCAFE02, 2, "CafeKr.ttf", "resources/sharedFonts/CafeKr.ttf", "sys/title/0005001b/10042400/content/CafeKr.ttf",
	0xFFCAFE03, 2, "CafeStd.ttf", "resources/sharedFonts/CafeStd.ttf", "sys/title/0005001b/10042400/content/CafeStd.ttf",
	0xFFCAFE04, 2, "CafeTw.ttf", "resources/sharedFonts/CafeTw.ttf", "sys/title/0005001b/10042400/content/CafeTw.ttf"
};

static_assert(sizeof(SharedDataEntry) == 0x1C);

uint32 LoadSharedData()
{
	// check if font files are dumped
	bool hasAllShareddataFiles = true;
	for (sint32 i = 0; i < sizeof(shareddataDef) / sizeof(shareddataDef[0]); i++)
	{
		bool existsInMLC = fs::exists(ActiveSettings::GetMlcPath(shareddataDef[i].mlcPath));
		bool existsInResources = fs::exists(ActiveSettings::GetDataPath(shareddataDef[i].resourcePath));

		if (!existsInMLC && !existsInResources)
		{
			cemuLog_log(LogType::Force, "Shared font {} is not present", shareddataDef[i].fileName);
			hasAllShareddataFiles = false;
			break;
		}
	}
	sint32 numEntries = sizeof(shareddataDef) / sizeof(shareddataDef[0]);
	if (hasAllShareddataFiles)
	{
		// all shareddata font files are present -> load them
		SharedDataEntry* shareddataTable = (SharedDataEntry*)memory_getPointerFromVirtualOffset(0xF8000000);
		memset(shareddataTable, 0, sizeof(SharedDataEntry) * numEntries);
		uint8* dataWritePtr = memory_getPointerFromVirtualOffset(0xF8000000 + sizeof(SharedDataEntry) * numEntries);
		// setup entries
		for (sint32 i = 0; i < numEntries; i++)
		{
			// try to read font from MLC first
			auto path = ActiveSettings::GetMlcPath(shareddataDef[i].mlcPath);
			FileStream* fontFile = FileStream::openFile2(path);
			// alternatively fall back to our shared fonts
			if (!fontFile)
			{
				path = ActiveSettings::GetDataPath(shareddataDef[i].resourcePath);
				fontFile = FileStream::openFile2(path);
			}
			if (!fontFile)
			{
				cemuLog_log(LogType::Force, "Failed to load shared font {}", shareddataDef[i].fileName);
				continue;
			}
			uint32 fileSize = fontFile->GetSize();
			fontFile->readData(dataWritePtr, fileSize);
			delete fontFile;
			// setup entry
			shareddataTable[i].name = shareddataDef[i].name;
			shareddataTable[i].fileType = shareddataDef[i].fileType;
			shareddataTable[i].kernelFilenamePtr = 0x00000000;
			shareddataTable[i].data = dataWritePtr;
			shareddataTable[i].size = fileSize;
			shareddataTable[i].ukn14 = 0x00000000;
			shareddataTable[i].ukn18 = 0x00000000;
			// advance write offset and pad to 16 byte alignment
			dataWritePtr += ((fileSize + 15) & ~15);
		}
		cemuLog_log(LogType::Force, "COS: System fonts found. Generated shareddata ({}KB)", (uint32)(dataWritePtr - (uint8*)shareddataTable) / 1024);
		return memory_getVirtualOffsetFromPointer(dataWritePtr);
	}
	// alternative method: load RAM dump
	const auto path = ActiveSettings::GetUserDataPath("shareddata.bin");
	FileStream* ramDumpFile = FileStream::openFile2(path);
	if (ramDumpFile)
	{
		ramDumpFile->readData(memory_getPointerFromVirtualOffset(0xF8000000), 0x02000000);
		delete ramDumpFile;
		return (mmuRange_SHARED_AREA.getBase() + 0x02000000);
	}
	return mmuRange_SHARED_AREA.getBase() + sizeof(SharedDataEntry) * numEntries;
}

void cemu_initForGame()
{
	gui_updateWindowTitles(false, true, 0.0);
	// input manager apply game profile
	InputManager::instance().apply_game_profile();
	// log info for launched title
	InfoLog_TitleLoaded();
	// determine cycle offset since 1.1.2000
	uint64 secondsSince2000_UTC = (uint64)(time(NULL) - 946684800);
	ppcCyclesSince2000_UTC = secondsSince2000_UTC * (uint64)ESPRESSO_CORE_CLOCK;
	time_t theTime = (time(NULL) - 946684800);
	{
		tm* lt = localtime(&theTime);
#if BOOST_OS_WINDOWS
		theTime = _mkgmtime(lt);
#else
		theTime = timegm(lt);
#endif
	}
	ppcCyclesSince2000 = theTime * (uint64)ESPRESSO_CORE_CLOCK;
	ppcCyclesSince2000TimerClock = ppcCyclesSince2000 / 20ULL;
	PPCTimer_start();
	// this must happen after the RPX/RPL files are mapped to memory (coreinit sets up heaps so that they don't overwrite RPX/RPL data)
	osLib_load();
	// link all modules
	uint32 linkTimeStart = GetTickCount();
	RPLLoader_UpdateDependencies();
	RPLLoader_Link();
	RPLLoader_NotifyControlPassedToApplication();
	uint32 linkTime = GetTickCount() - linkTimeStart;
	cemuLog_log(LogType::Force, "RPL link time: {}ms", linkTime);
	// for HBL ELF: Setup OS-specifics struct
	if (isLaunchTypeELF)
	{
		memory_writeU32(0x801500, rpl_mapHLEImport(nullptr, "coreinit", "OSDynLoad_Acquire", true));
		memory_writeU32(0x801504, rpl_mapHLEImport(nullptr, "coreinit", "OSDynLoad_FindExport", true));
	}
	else
	{
		// replace any known function signatures with our HLE implementations and patch bugs in the games
		GamePatch_scan();
	}
	LatteGPUState.isDRCPrimary = ActiveSettings::DisplayDRCEnabled();
	InfoLog_PrintActiveSettings();
	Latte_Start();
	// check for debugger entrypoint bp
    if (g_gdbstub)
    {
        g_gdbstub->HandleEntryStop(_entryPoint);
        g_gdbstub->Initialize();
    }
	debugger_handleEntryBreakpoint(_entryPoint);
	// load graphic packs
	cemuLog_log(LogType::Force, "------- Activate graphic packs -------");
	GraphicPack2::ActivateForCurrentTitle();
	// print audio log
	IAudioAPI::PrintLogging();
	IAudioInputAPI::PrintLogging();
	// everything initialized
	cemuLog_log(LogType::Force, "------- Run title -------");
	// wait till GPU thread is initialized
	while (g_isGPUInitFinished == false) std::this_thread::sleep_for(std::chrono::milliseconds(50));
	// init initial thread
	OSThread_t* initialThread = coreinit::OSGetDefaultThread(1);
	coreinit::OSSetThreadPriority(initialThread, 16);
	coreinit::OSRunThread(initialThread, PPCInterpreter_makeCallableExportDepr(coreinit_start), 0, nullptr);
	// init AX and start AX I/O thread
	snd_core::AXOut_init();
}

namespace CafeSystem
{
	void InitVirtualMlcStorage();
	void MlcStorageMountTitle(TitleInfo& titleInfo);
    void MlcStorageUnmountAllTitles();

    static bool s_initialized = false;
	static SystemImplementation* s_implementation{nullptr};
    bool sLaunchModeIsStandalone = false;
	std::optional<std::vector<std::string>> s_overrideArgs;

	bool sSystemRunning = false;
	TitleId sForegroundTitleId = 0;

	GameInfo2 sGameInfo_ForegroundTitle;


	static void _CheckForWine()
	{
		#if BOOST_OS_WINDOWS
		const HMODULE hmodule = GetModuleHandleA("ntdll.dll");
		if (!hmodule)
			return;

		const auto pwine_get_version = (const char*(__cdecl*)())GetProcAddress(hmodule, "wine_get_version");
		if (pwine_get_version)
		{
			cemuLog_log(LogType::Force, "Wine version: {}", pwine_get_version());
		}
		#endif
	}

	void logCPUAndMemoryInfo()
	{
		std::string cpuName = g_CPUFeatures.GetCPUName();
		if (!cpuName.empty())
			cemuLog_log(LogType::Force, "CPU: {}", cpuName);
		#if BOOST_OS_WINDOWS
		MEMORYSTATUSEX statex;
		statex.dwLength = sizeof(statex);
		GlobalMemoryStatusEx(&statex);
		uint32 memoryInMB = (uint32)(statex.ullTotalPhys / 1024LL / 1024LL);
		cemuLog_log(LogType::Force, "RAM: {}MB", memoryInMB);
		#elif BOOST_OS_LINUX
		struct sysinfo info {};
		sysinfo(&info);
		cemuLog_log(LogType::Force, "RAM: {}MB", ((static_cast<uint64_t>(info.totalram) * info.mem_unit) / 1024LL / 1024LL));
		#elif BOOST_OS_MACOS
		int64_t totalRam;
		size_t size = sizeof(totalRam);
		int result = sysctlbyname("hw.memsize", &totalRam, &size, NULL, 0);
		if (result == 0)
			cemuLog_log(LogType::Force, "RAM: {}MB", (totalRam / 1024LL / 1024LL));
		#endif
	}

	#if BOOST_OS_WINDOWS
	std::string GetWindowsNamedVersion(uint32& buildNumber)
	{
		char productName[256];
		HKEY hKey;
		DWORD dwType = REG_SZ;
		DWORD dwSize = sizeof(productName);
		if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS)
		{
			if (RegQueryValueExA(hKey, "ProductName", NULL, &dwType, (LPBYTE)productName, &dwSize) != ERROR_SUCCESS)
				strcpy(productName, "Windows");
			RegCloseKey(hKey);
		}
		OSVERSIONINFO osvi;
		ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
		osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		GetVersionEx(&osvi);
		buildNumber = osvi.dwBuildNumber;
		return std::string(productName);
	}
	#endif

	void logPlatformInfo()
	{
		std::string buffer;
		const char* platform = NULL;
		#if BOOST_OS_WINDOWS
		uint32 buildNumber;
		std::string windowsVersionName = GetWindowsNamedVersion(buildNumber);
		buffer = fmt::format("{} (Build {})", windowsVersionName, buildNumber);
		platform = buffer.c_str();
		#elif BOOST_OS_LINUX
		if (getenv ("APPIMAGE"))
			platform = "Linux (AppImage)";
		else if (getenv ("SNAP"))
			platform = "Linux (Snap)";
		else if (platform = getenv ("container"))
		{
			if (strcmp (platform, "flatpak") == 0)
				platform = "Linux (Flatpak)";
		}
		else
			platform = "Linux";
		#elif BOOST_OS_MACOS
		platform = "MacOS";
		#endif
		cemuLog_log(LogType::Force, "Platform: {}", platform);
	}

	static std::vector<IOSUModule*> s_iosuModules =
	{
		// entries in this list are ordered by initialization order. Shutdown in reverse order
		iosu::kernel::GetModule(),
		iosu::acp::GetModule(),
		iosu::fpd::GetModule(),
		iosu::pdm::GetModule(),
		iosu::ccr_nfc::GetModule(),
	};

	// initialize all subsystems which are persistent and don't depend on a game running
	void Initialize()
	{
		if (s_initialized)
			return;
		s_initialized = true;
		// init core systems
		cemuLog_log(LogType::Force, "------- Init {} -------", BUILD_VERSION_WITH_NAME_STRING);
		fsc_init();
		memory_init();
		cemuLog_log(LogType::Force, "Init Wii U memory space (base: 0x{:016x})", (size_t)memory_base);
		PPCCore_init();
		RPLLoader_InitState();
		cemuLog_log(LogType::Force, "mlc01 path: {}", _pathToUtf8(ActiveSettings::GetMlcPath()));
		_CheckForWine();
		// CPU and RAM info
		logCPUAndMemoryInfo();
		logPlatformInfo();
		cemuLog_log(LogType::Force, "Used CPU extensions: {}", g_CPUFeatures.GetCommaSeparatedExtensionList());
		// misc systems
		rplSymbolStorage_init();
		// allocate memory for all SysAllocators
		// must happen before COS module init, but also before iosu::kernel::Initialize()
		SysAllocatorContainer::GetInstance().Initialize();
		// init IOSU modules
		for(auto& module : s_iosuModules)
			module->SystemLaunch();
		// init IOSU (deprecated manual init)
		iosuCrypto_init();
		iosu::fsa::Initialize();
		iosuIoctl_init();
		iosuAct_init_depr();
		iosu::act::Initialize();
		iosu::iosuMcp_init();
		iosu::mcp::Init();
		iosu::iosuAcp_init();
		iosu::boss_init();
		iosu::nim::Initialize();
		iosu::odm::Initialize();
		// init Cafe OS
		avm::Initialize();
		drmapp::Initialize();
		TCL::Initialize();
		nn::cmpt::Initialize();
		nn::ccr::Initialize();
		nn::temp::Initialize();
		nn::aoc::Initialize();
		nn::pdm::Initialize();
		snd::user::Initialize();
		H264::Initialize();
		snd_core::Initialize();
		mic::Initialize();
		nfc::Initialize();
		ntag::Initialize();
		// init hardware register interfaces
		HW_SI::Initialize();
	}

	void SetImplementation(SystemImplementation* impl)
	{
		s_implementation = impl;
	}

    void Shutdown()
    {
        cemu_assert_debug(s_initialized);
        // if a title is running, shut it down
        if (sSystemRunning)
            ShutdownTitle();
        // shutdown persistent subsystems (deprecated manual shutdown)
		iosu::odm::Shutdown();
		iosu::act::Stop();
        iosu::mcp::Shutdown();
        iosu::fsa::Shutdown();
		// shutdown IOSU modules
		for(auto it = s_iosuModules.rbegin(); it != s_iosuModules.rend(); ++it)
			(*it)->SystemExit();
        s_initialized = false;
    }

	std::string GetInternalVirtualCodeFolder()
	{
		return "/internal/current_title/code/";
	}

    void MountBaseDirectories()
    {
        const auto mlc = ActiveSettings::GetMlcPath();
        FSCDeviceHostFS_Mount("/cemuBossStorage/", _pathToUtf8(mlc / "usr/boss/"), FSC_PRIORITY_BASE);
        FSCDeviceHostFS_Mount("/vol/storage_mlc01/", _pathToUtf8(mlc / ""), FSC_PRIORITY_BASE);
    }

    void UnmountBaseDirectories()
    {
        fsc_unmount("/vol/storage_mlc01/", FSC_PRIORITY_BASE);
        fsc_unmount("/cemuBossStorage/", FSC_PRIORITY_BASE);
    }

	PREPARE_STATUS_CODE LoadAndMountForegroundTitle(TitleId titleId)
	{
        cemuLog_log(LogType::Force, "Mounting title {:016x}", (uint64)titleId);
		sGameInfo_ForegroundTitle = CafeTitleList::GetGameInfo(titleId);
		if (!sGameInfo_ForegroundTitle.IsValid())
		{
			cemuLog_log(LogType::Force, "Mounting failed: Game meta information is either missing, inaccessible or not valid (missing or invalid .xml files in code and meta folder)");
			return PREPARE_STATUS_CODE::UNABLE_TO_MOUNT;
		}
		// check base
		TitleInfo& titleBase = sGameInfo_ForegroundTitle.GetBase();
		if (!titleBase.IsValid())
			return PREPARE_STATUS_CODE::UNABLE_TO_MOUNT;
		if(!titleBase.ParseXmlInfo())
			return PREPARE_STATUS_CODE::UNABLE_TO_MOUNT;
		cemuLog_log(LogType::Force, "Base: {}", titleBase.GetPrintPath());
		// mount base
		if (!titleBase.Mount("/vol/content", "content", FSC_PRIORITY_BASE) || !titleBase.Mount(GetInternalVirtualCodeFolder(), "code", FSC_PRIORITY_BASE))
		{
			cemuLog_log(LogType::Force, "Mounting failed");
			return PREPARE_STATUS_CODE::UNABLE_TO_MOUNT;
		}
		// check update
		TitleInfo& titleUpdate = sGameInfo_ForegroundTitle.GetUpdate();
		if (titleUpdate.IsValid())
		{
			if (!titleUpdate.ParseXmlInfo())
				return PREPARE_STATUS_CODE::UNABLE_TO_MOUNT;
			cemuLog_log(LogType::Force, "Update: {}", titleUpdate.GetPrintPath());
			// mount update
			if (!titleUpdate.Mount("/vol/content", "content", FSC_PRIORITY_PATCH) || !titleUpdate.Mount(GetInternalVirtualCodeFolder(), "code", FSC_PRIORITY_PATCH))
			{
				cemuLog_log(LogType::Force, "Mounting failed");
				return PREPARE_STATUS_CODE::UNABLE_TO_MOUNT;
			}
		}
		else
			cemuLog_log(LogType::Force, "Update: Not present");
		// check AOC
		auto aocList = sGameInfo_ForegroundTitle.GetAOC();
		if (!aocList.empty())
		{
			// todo - support for multi-title AOC
			TitleInfo& titleAOC = aocList[0];
			if (!titleAOC.ParseXmlInfo())
				return PREPARE_STATUS_CODE::UNABLE_TO_MOUNT;
			cemu_assert_debug(titleAOC.IsValid());
			cemuLog_log(LogType::Force, "DLC: {}", titleAOC.GetPrintPath());
			// mount AOC
			if (!titleAOC.Mount(fmt::format("/vol/aoc{:016x}", titleAOC.GetAppTitleId()), "content", FSC_PRIORITY_PATCH))
			{
				cemuLog_log(LogType::Force, "Mounting failed");
				return PREPARE_STATUS_CODE::UNABLE_TO_MOUNT;
			}
		}
		else
			cemuLog_log(LogType::Force, "DLC: Not present");
		sForegroundTitleId = titleId;
		return PREPARE_STATUS_CODE::SUCCESS;
	}

    void UnmountForegroundTitle()
    {
        if(sLaunchModeIsStandalone)
            return;
        cemu_assert_debug(sGameInfo_ForegroundTitle.IsValid()); // unmounting title which was never mounted?
        if (!sGameInfo_ForegroundTitle.IsValid())
            return;
        sGameInfo_ForegroundTitle.GetBase().Unmount("/vol/content");
        sGameInfo_ForegroundTitle.GetBase().Unmount(GetInternalVirtualCodeFolder());
        if (sGameInfo_ForegroundTitle.HasUpdate())
        {
            if(auto& update = sGameInfo_ForegroundTitle.GetUpdate(); update.IsValid())
            {
                update.Unmount("/vol/content");
                update.Unmount(GetInternalVirtualCodeFolder());
            }
        }
        auto aocList = sGameInfo_ForegroundTitle.GetAOC();
        if (!aocList.empty())
        {
            TitleInfo& titleAOC = aocList[0];
            titleAOC.Unmount(fmt::format("/vol/aoc{:016x}", titleAOC.GetAppTitleId()));
        }
    }

	PREPARE_STATUS_CODE SetupExecutable()
	{
		// set rpx path from cos.xml if available
		_pathToBaseExecutable = _pathToExecutable;
		if (!sLaunchModeIsStandalone)
		{
			std::string _argstr = CafeSystem::GetForegroundTitleArgStr();
			const char* argstr = _argstr.c_str();
			if (argstr && *argstr != '\0')
			{
				const std::string tmp = argstr;
				const auto index = tmp.find(".rpx");
				if (index != std::string::npos)
				{
					fs::path rpx = _pathToExecutable;
					rpx.replace_filename(tmp.substr(0, index + 4)); // cut off after .rpx

					std::string rpxPath;
					rpxPath = "/internal/current_title/code/";
					rpxPath.append(rpx.generic_string());

					int status;
					const auto file = fsc_open(rpxPath.c_str(), FSC_ACCESS_FLAG::OPEN_FILE | FSC_ACCESS_FLAG::READ_PERMISSION, &status);
					if (file)
					{
						_pathToExecutable = std::move(rpxPath);
						fsc_close(file);
					}
				}
			}
		}
		LoadMainExecutable();
		return PREPARE_STATUS_CODE::SUCCESS;
	}

    void SetupMemorySpace()
    {
        memory_mapForCurrentTitle();
        LoadSharedData();
    }

    void DestroyMemorySpace()
    {
        memory_unmapForCurrentTitle();
    }

	PREPARE_STATUS_CODE PrepareForegroundTitle(TitleId titleId)
	{
		CafeTitleList::WaitForMandatoryScan();
		sLaunchModeIsStandalone = false;
        _pathToExecutable.clear();
		TitleIdParser tip(titleId);
		if (tip.GetType() == TitleIdParser::TITLE_TYPE::AOC || tip.GetType() == TitleIdParser::TITLE_TYPE::BASE_TITLE_UPDATE)
			cemuLog_log(LogType::Force, "Launched titleId is not the base of a title");
        // mount mlc storage
        MountBaseDirectories();
        // mount title folders
		PREPARE_STATUS_CODE r = LoadAndMountForegroundTitle(titleId);
		if (r != PREPARE_STATUS_CODE::SUCCESS)
			return r;
		gameProfile_load();
		// setup memory space and PPC recompiler
        SetupMemorySpace();
        PPCRecompiler_init();
		r = SetupExecutable(); // load RPX
		if (r != PREPARE_STATUS_CODE::SUCCESS)
			return r;
		InitVirtualMlcStorage();
		return PREPARE_STATUS_CODE::SUCCESS;
	}

	PREPARE_STATUS_CODE PrepareForegroundTitleFromStandaloneRPX(const fs::path& path)
	{
		sLaunchModeIsStandalone = true;
		cemuLog_log(LogType::Force, "Launching executable in standalone mode due to incorrect layout or missing meta files");
		fs::path executablePath = path;
		std::string dirName = _pathToUtf8(executablePath.parent_path().filename());
		if (boost::iequals(dirName, "code"))
		{
			// check for content folder
			fs::path contentPath = executablePath.parent_path().parent_path().append("content");
			std::error_code ec;
			if (fs::is_directory(contentPath, ec))
			{
				// mounting content folder
				bool r = FSCDeviceHostFS_Mount(std::string("/vol/content").c_str(), _pathToUtf8(contentPath), FSC_PRIORITY_BASE);
				if (!r)
				{
					cemuLog_log(LogType::Force, "Failed to mount {}", _pathToUtf8(contentPath));
					return PREPARE_STATUS_CODE::UNABLE_TO_MOUNT;
				}
			}
		}
		// mount code folder to a virtual temporary path
		FSCDeviceHostFS_Mount(std::string("/internal/code/").c_str(), _pathToUtf8(executablePath.parent_path()), FSC_PRIORITY_BASE);
		std::string internalExecutablePath = "/internal/code/";
		internalExecutablePath.append(_pathToUtf8(executablePath.filename()));
		_pathToExecutable = internalExecutablePath;
		// since a lot of systems (including save folder location) rely on a TitleId, we derive a placeholder id from the executable hash
		auto execData = fsc_extractFile(_pathToExecutable.c_str());
		if (!execData)
			return PREPARE_STATUS_CODE::INVALID_RPX;
		uint32 h = generateHashFromRawRPXData(execData->data(), execData->size());
		sForegroundTitleId = 0xFFFFFFFF00000000ULL | (uint64)h;
		cemuLog_log(LogType::Force, "Generated placeholder TitleId: {:016x}", sForegroundTitleId);
		// setup memory space and ppc recompiler
        SetupMemorySpace();
        PPCRecompiler_init();
        // load executable
        SetupExecutable();
		InitVirtualMlcStorage();
		return PREPARE_STATUS_CODE::SUCCESS;
	}

	void _LaunchTitleThread()
	{
		for(auto& module : s_iosuModules)
			module->TitleStart();
		cemu_initForGame();
		// enter scheduler
		if (ActiveSettings::GetCPUMode() == CPUMode::MulticoreRecompiler)
			coreinit::OSSchedulerBegin(3);
		else
			coreinit::OSSchedulerBegin(1);
	}

	void LaunchForegroundTitle()
	{
		PPCTimer_waitForInit();
		// start system
		sSystemRunning = true;
		gui_notifyGameLoaded();
		std::thread t(_LaunchTitleThread);
		t.detach();
	}

	bool IsTitleRunning()
	{
		return sSystemRunning;
	}

	TitleId GetForegroundTitleId()
	{
		cemu_assert_debug(sForegroundTitleId != 0);
		return sForegroundTitleId;
	}

	uint16 GetForegroundTitleVersion()
	{
		if (sLaunchModeIsStandalone)
			return 0;
		return sGameInfo_ForegroundTitle.GetVersion();
	}

	uint32 GetForegroundTitleSDKVersion()
	{
		if (sLaunchModeIsStandalone)
			return 999999;
		return sGameInfo_ForegroundTitle.GetSDKVersion();
	}

	CafeConsoleRegion GetForegroundTitleRegion()
	{
		if (sLaunchModeIsStandalone)
			return CafeConsoleRegion::USA;
		return sGameInfo_ForegroundTitle.GetRegion();
	}

	std::string GetForegroundTitleName()
	{
		if (sLaunchModeIsStandalone)
			return "Unknown Game";
		std::string applicationName;
		applicationName = sGameInfo_ForegroundTitle.GetBase().GetMetaInfo()->GetShortName(GetConfig().console_language);
		if (applicationName.empty()) //Try to get the English Title
			applicationName = sGameInfo_ForegroundTitle.GetBase().GetMetaInfo()->GetShortName(CafeConsoleLanguage::EN);
		if (applicationName.empty()) //Unknown Game
			applicationName = "Unknown Game";
		return applicationName;
	}

	uint32 GetForegroundTitleOlvAccesskey()
	{
		if (sLaunchModeIsStandalone)
			return -1;
		return sGameInfo_ForegroundTitle.GetBase().GetMetaInfo()->GetOlvAccesskey();
	}

	std::string GetForegroundTitleArgStr()
	{
		if (sLaunchModeIsStandalone)
			return "";
		auto& update = sGameInfo_ForegroundTitle.GetUpdate();
		if (update.IsValid())
			return update.GetArgStr();
		return sGameInfo_ForegroundTitle.GetBase().GetArgStr();
	}

	CosCapabilityBits GetForegroundTitleCosCapabilities(CosCapabilityGroup group)
	{
		if (sLaunchModeIsStandalone)
			return CosCapabilityBits::All;
		auto& update = sGameInfo_ForegroundTitle.GetUpdate();
		if (update.IsValid())
		{
			ParsedCosXml* cosXml = update.GetCosInfo();
			if (cosXml)
				return cosXml->GetCapabilityBits(group);
		}
		auto& base = sGameInfo_ForegroundTitle.GetBase();
		if(base.IsValid())
		{
			ParsedCosXml* cosXml = base.GetCosInfo();
			if (cosXml)
				return cosXml->GetCapabilityBits(group);
		}
		return CosCapabilityBits::All;
	}

	// when switching titles custom parameters can be passed, returns true if override args are used
	bool GetOverrideArgStr(std::vector<std::string>& args)
	{
		args.clear();
		if(!s_overrideArgs)
			return false;
		args = *s_overrideArgs;
		return true;
	}

	void SetOverrideArgs(std::span<std::string> args)
	{
		s_overrideArgs = std::vector<std::string>(args.begin(), args.end());
	}

	void UnsetOverrideArgs()
	{
		s_overrideArgs = std::nullopt;
	}

	// pick platform region based on title region
	CafeConsoleRegion GetPlatformRegion()
	{
		CafeConsoleRegion titleRegion = GetForegroundTitleRegion();
		CafeConsoleRegion platformRegion = CafeConsoleRegion::USA;
		if (HAS_FLAG(titleRegion, CafeConsoleRegion::JPN))
			platformRegion = CafeConsoleRegion::JPN;
		else if (HAS_FLAG(titleRegion, CafeConsoleRegion::EUR))
			platformRegion = CafeConsoleRegion::EUR;
		else if (HAS_FLAG(titleRegion, CafeConsoleRegion::USA))
			platformRegion = CafeConsoleRegion::USA;
		return platformRegion;
	}

	void UnmountCurrentTitle()
	{
        UnmountForegroundTitle();
        fsc_unmount("/internal/code/", FSC_PRIORITY_BASE);
	}

	void ShutdownTitle()
	{
		if(!sSystemRunning)
			return;
        coreinit::OSSchedulerEnd();
        Latte_Stop();
        // reset Cafe OS userspace modules
        snd_core::reset();
        coreinit::OSAlarm_Shutdown();
        GX2::_GX2DriverReset();
        nn::save::ResetToDefaultState();
        coreinit::__OSDeleteAllActivePPCThreads();
        RPLLoader_ResetState();
		for(auto it = s_iosuModules.rbegin(); it != s_iosuModules.rend(); ++it)
			(*it)->TitleStop();
        // reset Cemu subsystems
        PPCRecompiler_Shutdown();
        GraphicPack2::Reset();
        UnmountCurrentTitle();
        MlcStorageUnmountAllTitles();
        UnmountBaseDirectories();
        DestroyMemorySpace();
		sSystemRunning = false;
	}

	/* Virtual mlc storage */

	void InitVirtualMlcStorage()
	{
		// starting with Cemu 1.27.0 /vol/storage_mlc01/ is virtualized, meaning that it doesn't point to one singular host os folder anymore
		// instead it now uses a more complex solution to source titles with various formats (folder, wud, wua) from the game paths and host mlc path
		
		// todo - mount /vol/storage_mlc01/ with base priority to the host mlc?

		// since mounting titles is an expensive operation we have to avoid mounting all titles at once
		// only the current title gets mounted immediately, every other title should be mounted lazily on first access

		// always mount the currently running title
		if (sGameInfo_ForegroundTitle.GetBase().IsValid())
			MlcStorageMountTitle(sGameInfo_ForegroundTitle.GetBase());
		if (sGameInfo_ForegroundTitle.GetUpdate().IsValid())
			MlcStorageMountTitle(sGameInfo_ForegroundTitle.GetUpdate());
		for(auto& it : sGameInfo_ForegroundTitle.GetAOC())
			MlcStorageMountTitle(it);

		// setup system for lazy-mounting of other known titles
		// todo - how to handle this?
		// when something iterates /vol/storage_mlc01/usr/title/ we can use a fake FS device mounted to /vol/storage_mlc01/usr/title and sys/title that simulates the title id folders
		// the same device would then have to mount titles when their folders are actually accessed
	}

	// /vol/storage_mlc01/<usr or sys>/title/<titleIdHigh>/<titleIdLow>
	std::string GetMlcStoragePath(TitleId titleId)
	{
		TitleIdParser tip(titleId);
		return fmt::format("/vol/storage_mlc01/{}/title/{:08x}/{:08x}", tip.IsSystemTitle() ? "sys" : "usr", (uint32)(titleId >> 32), (uint32)(titleId & 0xFFFFFFFF));
	}

	std::map<TitleId, TitleInfo*> m_mlcMountedTitles;

	// mount title to our virtual MLC storage
	// /vol/storage_mlc01/<usr or sys>/title/<titleIdHigh>/<titleIdLow>
	void MlcStorageMountTitle(TitleInfo& titleInfo)
	{
		if (!titleInfo.IsValid())
		{
			cemu_assert_suspicious();
			return;
		}
		TitleId titleId = titleInfo.GetAppTitleId();
		if (m_mlcMountedTitles.find(titleId) != m_mlcMountedTitles.end())
			return;
		std::string mlcStoragePath = GetMlcStoragePath(titleId);
		TitleInfo* mountTitleInfo = new TitleInfo(titleInfo);
		if (!mountTitleInfo->Mount(mlcStoragePath, "", FSC_PRIORITY_BASE))
		{
			cemuLog_log(LogType::Force, "Failed to mount title to virtual storage");
			delete mountTitleInfo;
			return;
		}
		m_mlcMountedTitles.emplace(titleId, mountTitleInfo);
	}

	void MlcStorageMountTitle(TitleId titleId)
	{
		TitleInfo titleInfo;
		if (!CafeTitleList::GetFirstByTitleId(titleId, titleInfo))
			return;
		MlcStorageMountTitle(titleInfo);
	}

	void MlcStorageMountAllTitles()
	{
		std::vector<uint64> titleIds = CafeTitleList::GetAllTitleIds();
		for (auto& it : titleIds)
			MlcStorageMountTitle(it);
	}

    void MlcStorageUnmountAllTitles()
    {
        for(auto& it : m_mlcMountedTitles)
        {
            std::string mlcStoragePath = GetMlcStoragePath(it.first);
            it.second->Unmount(mlcStoragePath);
        }
        m_mlcMountedTitles.clear();
    }

	uint32 GetRPXHashBase()
	{
		return currentBaseApplicationHash;
	}

	uint32 GetRPXHashUpdated()
	{
		return currentUpdatedApplicationHash;
	}

	void RequestRecreateCanvas()
	{
		s_implementation->CafeRecreateCanvas();
	}

}
