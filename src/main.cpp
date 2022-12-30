#include "gui/guiWrapper.h"
#include "gui/wxgui.h"
#include "util/crypto/aes128.h"
#include "gui/MainWindow.h"
#include "Cafe/OS/RPL/rpl.h"
#include "Cafe/OS/RPL/rpl_symbol_storage.h"
#include "Cafe/OS/libs/gx2/GX2.h"
#include "Cafe/GameProfile/GameProfile.h"
#include "Cafe/GraphicPack/GraphicPack2.h"
#include "config/CemuConfig.h"
#include "config/NetworkSettings.h"
#include "gui/CemuApp.h"
#include "Cafe/HW/Latte/Core/LatteOverlay.h"
#include "config/LaunchSettings.h"
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"

#include "Cafe/CafeSystem.h"
#include "Cafe/TitleList/TitleList.h"
#include "Cafe/TitleList/SaveList.h"

#include "Common/ExceptionHandler/ExceptionHandler.h"
#include "Common/cpu_features.h"

#include <wx/setup.h>
#include "util/helpers/helpers.h"
#include "config/ActiveSettings.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VsyncDriver.h"

#include "Cafe/IOSU/legacy/iosu_crypto.h"
#include "Cafe/OS/libs/vpad/vpad.h"

#include "audio/IAudioAPI.h"
#include "audio/IAudioInputAPI.h"
#if BOOST_OS_WINDOWS
#pragma comment(lib,"Dbghelp.lib")
#endif

#define SDL_MAIN_HANDLED
#include <SDL.h>

#if BOOST_OS_LINUX
#define _putenv(__s) putenv((char*)(__s))
#include <sys/sysinfo.h>
#elif BOOST_OS_MACOS
#define _putenv(__s) putenv((char*)(__s))
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#if BOOST_OS_WINDOWS
extern "C"
{
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
}
#endif

std::atomic_bool g_isGPUInitFinished = false;

std::wstring executablePath;

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
	forceLog_printf("RAM: %uMB", memoryInMB);
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

bool g_running_in_wine = false;
bool IsRunningInWine()
{
	return g_running_in_wine;
}

void checkForWine()
{
	#if BOOST_OS_WINDOWS
	const HMODULE hmodule = GetModuleHandleA("ntdll.dll");
	if (!hmodule)
		return;

	const auto pwine_get_version = (const char*(__cdecl*)())GetProcAddress(hmodule, "wine_get_version");
	if (pwine_get_version)
	{
		g_running_in_wine = true;
		forceLog_printf("Wine version: %s", pwine_get_version());
	}
	#else
	g_running_in_wine = false;
	#endif
}

void infoLog_cemuStartup()
{
	cemuLog_force("------- Init {} -------", BUILD_VERSION_WITH_NAME_STRING);
	cemuLog_force("Init Wii U memory space (base: 0x{:016x})", (size_t)memory_base);
	cemuLog_force(u8"mlc01 path: {}", ActiveSettings::GetMlcPath().generic_u8string());
	// check for wine version
	checkForWine();
	// CPU and RAM info
	logCPUAndMemoryInfo();
	cemuLog_log(LogType::Force, "Used CPU extensions: {}", g_CPUFeatures.GetCommaSeparatedExtensionList());
}

// some implementations of _putenv dont copy the string and instead only store a pointer
// thus we use a helper to keep a permanent copy
std::vector<std::string*> sPutEnvMap;

void _putenvSafe(const char* c)
{
    auto s = new std::string(c);
    sPutEnvMap.emplace_back(s);
    _putenv(s->c_str());
}

void reconfigureGLDrivers()
{
	// reconfigure GL drivers to store 
	const fs::path nvCacheDir = ActiveSettings::GetCachePath("shaderCache/driver/nvidia/");

	std::error_code err;
	fs::create_directories(nvCacheDir, err);

	std::string nvCacheDirEnvOption("__GL_SHADER_DISK_CACHE_PATH=");
	nvCacheDirEnvOption.append(_pathToUtf8(nvCacheDir));

#if BOOST_OS_WINDOWS
	std::wstring tmpW = boost::nowide::widen(nvCacheDirEnvOption);
	_wputenv(tmpW.c_str());
#else
    _putenvSafe(nvCacheDirEnvOption.c_str());
#endif
    _putenvSafe("__GL_SHADER_DISK_CACHE_SKIP_CLEANUP=1");

}

void reconfigureVkDrivers()
{
    _putenvSafe("DISABLE_LAYER_AMD_SWITCHABLE_GRAPHICS_1=1");
    _putenvSafe("DISABLE_VK_LAYER_VALVE_steam_fossilize_1=1");
}

void mainEmulatorCommonInit()
{
	reconfigureGLDrivers();
	reconfigureVkDrivers();
	// crypto init
	AES128_init();
	// init PPC timer (call this as early as possible because it measures frequency of RDTSC using an asynchronous thread over 3 seconds)
	PPCTimer_init();

#if BOOST_OS_WINDOWS
	executablePath.resize(4096);
	int i = GetModuleFileName(NULL, executablePath.data(), executablePath.size());
	if(i >= 0)
		executablePath.resize(i);
	else
		executablePath.clear();
	SetCurrentDirectory(executablePath.c_str());

	// set high priority
	SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
#endif
    ExceptionHandler_init();
	// read config
	g_config.Load();
	if (NetworkConfig::XMLExists())
	n_config.Load();
	// symbol storage
	rplSymbolStorage_init();
	// static initialization
	IAudioAPI::InitializeStatic();
	IAudioInputAPI::InitializeStatic();
	// load graphic packs (must happen before config is loaded)
	GraphicPack2::LoadAll();
	// initialize file system
	fsc_init();
}

void mainEmulatorLLE();
void ppcAsmTest();
void gx2CopySurfaceTest();
void ExpressionParser_test();
void FSTVolumeTest();
void CRCTest();

void unitTests()
{
	ExpressionParser_test();
	gx2CopySurfaceTest();
	ppcAsmTest();
	FSTVolumeTest();
	CRCTest();
}

int mainEmulatorHLE()
{
	LatteOverlay_init();
	// run a couple of tests if in non-release mode
#ifdef CEMU_DEBUG_ASSERT
	unitTests();
#endif
	// init common
	mainEmulatorCommonInit();
	// reserve memory (no allocations yet)
	memory_init();
	// init ppc core
	PPCCore_init();
	// log Cemu startup info
	infoLog_cemuStartup();
	// init RPL loader
	RPLLoader_InitState();
	// init IOSU components
	iosuCrypto_init();
	// init Cafe system (todo - the stuff above should be part of this too)
	CafeSystem::Initialize();
	// init title list
	CafeTitleList::Initialize(ActiveSettings::GetUserDataPath("title_list_cache.xml"));
	for (auto& it : GetConfig().game_paths)
		CafeTitleList::AddScanPath(it);
	fs::path mlcPath = ActiveSettings::GetMlcPath();
	if (!mlcPath.empty())
		CafeTitleList::SetMLCPath(mlcPath);
	CafeTitleList::Refresh();
	// init save list
	CafeSaveList::Initialize();
	if (!mlcPath.empty())
	{
		CafeSaveList::SetMLCPath(mlcPath);
		CafeSaveList::Refresh();
	}
	return 0;
}

bool isConsoleConnected = false;
void requireConsole()
{
	#if BOOST_OS_WINDOWS
	if (isConsoleConnected)
		return;

	if (AttachConsole(ATTACH_PARENT_PROCESS) != FALSE)
	{
		freopen("CONIN$", "r", stdin);
		freopen("CONOUT$", "w", stdout);
		freopen("CONOUT$", "w", stderr);
		isConsoleConnected = true;
	}
	#endif
}

void HandlePostUpdate()
{
	// finalize update process
	// delete update cemu.exe.backup if available
	const auto filename = ActiveSettings::GetFullPath().replace_extension("exe.backup");
	if (fs::exists(filename))
	{
#if BOOST_OS_WINDOWS
		HANDLE lock;
		do
		{
			lock = CreateMutex(nullptr, TRUE, L"Global\\cemu_update_lock");
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		} while (lock == nullptr);
		const DWORD wait_result = WaitForSingleObject(lock, 2000);
		CloseHandle(lock);

		if (wait_result == WAIT_OBJECT_0)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			std::error_code ec;
			fs::remove(filename, ec);
		}
#else
		while (fs::exists(filename))
		{
			std::error_code ec;
			fs::remove(filename, ec);
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		}
#endif
	}
}

void ToolShaderCacheMerger();

#if BOOST_OS_WINDOWS

// entrypoint for release builds
int wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPTSTR lpCmdLine, _In_ int nShowCmd)
{
	if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE)))
		cemuLog_log(LogType::Force, "CoInitializeEx() failed");
	SDL_SetMainReady();
	if (!LaunchSettings::HandleCommandline(lpCmdLine))
		return 0;
	gui_create();
	return 0;
}

// entrypoint for debug builds with console
int main(int argc, char* argv[])
{
	if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE)))
		cemuLog_log(LogType::Force, "CoInitializeEx() failed");
	SDL_SetMainReady();
	if (!LaunchSettings::HandleCommandline(argc, argv))
		return 0;
	gui_create();
	return 0;
}

#else

int main(int argc, char *argv[])
{
#if BOOST_OS_LINUX
    XInitThreads();
#endif
    if (!LaunchSettings::HandleCommandline(argc, argv))
		return 0;
	gui_create();
	return 0;
}
#endif

extern "C" DLLEXPORT uint64 gameMeta_getTitleId()
{
	return CafeSystem::GetForegroundTitleId();
}
