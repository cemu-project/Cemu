#include "gui/guiWrapper.h"
#include "gui/wxgui.h"
#include "util/crypto/aes128.h"
#include "gui/MainWindow.h"
#include "Cafe/OS/RPL/rpl.h"
#include "Cafe/OS/libs/gx2/GX2.h"
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"
#include "Cafe/GameProfile/GameProfile.h"
#include "Cafe/GraphicPack/GraphicPack2.h"
#include "config/CemuConfig.h"
#include "config/NetworkSettings.h"
#include "config/LaunchSettings.h"
#include "input/InputManager.h"
#include "gui/CemuApp.h"

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

void WindowsInitCwd()
{
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
}

void CemuCommonInit()
{
	reconfigureGLDrivers();
	reconfigureVkDrivers();
	// crypto init
	AES128_init();
	// init PPC timer
	// call this as early as possible because it measures frequency of RDTSC using an asynchronous thread over 3 seconds
	PPCTimer_init();

	WindowsInitCwd();
    ExceptionHandler_Init();
	// read config
	g_config.Load();
	if (NetworkConfig::XMLExists())
		n_config.Load();
	// parallelize expensive init code
	std::future<int> futureInitAudioAPI = std::async(std::launch::async, []{ IAudioAPI::InitializeStatic(); IAudioInputAPI::InitializeStatic(); return 0; });
	std::future<int> futureInitGraphicPacks = std::async(std::launch::async, []{ GraphicPack2::LoadAll(); return 0; });
	InputManager::instance().load();
	futureInitAudioAPI.wait();
	futureInitGraphicPacks.wait();
	// init Cafe system
	CafeSystem::Initialize();
	// init title list
	CafeTitleList::Initialize(ActiveSettings::GetUserDataPath("title_list_cache.xml"));
	for (auto& it : GetConfig().game_paths)
		CafeTitleList::AddScanPath(_utf8ToPath(it));
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
}

void mainEmulatorLLE();
void ppcAsmTest();
void gx2CopySurfaceTest();
void ExpressionParser_test();
void FSTVolumeTest();
void CRCTest();

void UnitTests()
{
	ExpressionParser_test();
	gx2CopySurfaceTest();
	ppcAsmTest();
	FSTVolumeTest();
	CRCTest();
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
	const auto filename = ActiveSettings::GetExecutablePath().replace_extension("exe.backup");
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
