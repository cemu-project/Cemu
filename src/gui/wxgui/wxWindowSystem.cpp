#include "input/HotkeySettings.h"
#include "interface/WindowSystem.h"

#include "helpers/wxHelpers.h"

#if BOOST_OS_LINUX
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkwindow.h>
#include <gdk/gdkx.h>
#ifdef HAS_WAYLAND
#include <gdk/gdkwayland.h>
#endif
#endif

#if BOOST_OS_MACOS
#include <Carbon/Carbon.h>
#endif

#include "wxgui/wxgui.h"
#include "wxgui/CemuApp.h"
#include "wxgui/MainWindow.h"
#include "Cafe/HW/Latte/Core/Latte.h"
#include "config/ActiveSettings.h"
#include "config/NetworkSettings.h"
#include "config/CemuConfig.h"
#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "Cafe/CafeSystem.h"

WindowSystem::WindowInfo g_window_info{};

std::shared_mutex g_mutex;
MainWindow* g_mainFrame = nullptr;

#if BOOST_OS_WINDOWS
void _wxLaunch()
{
	SetThreadName("MainThread_UI");
	wxEntry();
}
#endif

void WindowSystem::Create()
{
	SetThreadName("cemu");
#if BOOST_OS_WINDOWS
	// on Windows wxWidgets there is a bug where wxDirDialog->ShowModal will deadlock in Windows internals somehow
	// moving the UI thread off the main thread fixes this
	std::thread t = std::thread(_wxLaunch);
	t.join();
#else
	int argc = 0;
	char* argv[1]{};
	wxEntry(argc, argv);
#endif
}

void WindowSystem::ShowErrorDialog(std::string_view message, std::string_view title, std::optional<WindowSystem::ErrorCategory> /*errorId*/)
{
	wxString caption;
	if (title.empty())
		caption = wxASCII_STR(wxMessageBoxCaptionStr);
	else
		caption = to_wxString(title);
	wxMessageBox(to_wxString(message), caption, wxOK | wxCENTRE | wxICON_ERROR);
}

WindowSystem::WindowInfo& WindowSystem::GetWindowInfo()
{
	return g_window_info;
}

void WindowSystem::UpdateWindowTitles(bool isIdle, bool isLoading, double fps)
{
	std::string windowText;
	windowText = BUILD_VERSION_WITH_NAME_STRING;

	if (isIdle)
	{
		if (g_mainFrame)
			g_mainFrame->AsyncSetTitle(windowText);
		return;
	}
	if (isLoading)
	{
		windowText.append(" - loading...");
		if (g_mainFrame)
			g_mainFrame->AsyncSetTitle(windowText);
		return;
	}

	const char* renderer = "";
	if (g_renderer)
	{
		switch (g_renderer->GetType())
		{
		case RendererAPI::OpenGL:
			renderer = "[OpenGL]";
			break;
		case RendererAPI::Vulkan:
			renderer = "[Vulkan]";
			break;
		default:;
		}
	}

	// get GPU vendor/mode
	const char* graphicMode = "[Generic]";
	if (LatteGPUState.glVendor == GLVENDOR_AMD)
		graphicMode = "[AMD GPU]";
	else if (LatteGPUState.glVendor == GLVENDOR_INTEL)
		graphicMode = "[Intel GPU]";
	else if (LatteGPUState.glVendor == GLVENDOR_NVIDIA)
		graphicMode = "[NVIDIA GPU]";
	else if (LatteGPUState.glVendor == GLVENDOR_APPLE)
		graphicMode = "[Apple GPU]";

	const uint64 titleId = CafeSystem::GetForegroundTitleId();
	windowText.append(fmt::format(" - FPS: {:.2f} {} {} [TitleId: {:08x}-{:08x}]", (double)fps, renderer, graphicMode, (uint32)(titleId >> 32), (uint32)(titleId & 0xFFFFFFFF)));

	if (ActiveSettings::IsOnlineEnabled())
	{
		if (ActiveSettings::GetNetworkService() == NetworkService::Nintendo)
			windowText.append(" [Online]");
		else if (ActiveSettings::GetNetworkService() == NetworkService::Pretendo)
			windowText.append(" [Online-Pretendo]");
		else if (ActiveSettings::GetNetworkService() == NetworkService::Custom)
			windowText.append(" [Online-" + GetNetworkConfig().networkname.GetValue() + "]");
	}
	windowText.append(" ");
	windowText.append(CafeSystem::GetForegroundTitleName());
	// append region
	CafeConsoleRegion region = CafeSystem::GetForegroundTitleRegion();
	uint16 titleVersion = CafeSystem::GetForegroundTitleVersion();
	if (region == CafeConsoleRegion::JPN)
		windowText.append(fmt::format(" [JP v{}]", titleVersion));
	else if (region == CafeConsoleRegion::USA)
		windowText.append(fmt::format(" [US v{}]", titleVersion));
	else if (region == CafeConsoleRegion::EUR)
		windowText.append(fmt::format(" [EU v{}]", titleVersion));
	else
		windowText.append(fmt::format(" [v{}]", titleVersion));

	std::shared_lock lock(g_mutex);
	if (g_mainFrame)
	{
		g_mainFrame->AsyncSetTitle(windowText);
		auto* pad = g_mainFrame->GetPadView();
		if (pad)
			pad->AsyncSetTitle(fmt::format("{} - FPS: {:.02f}", _("GamePad View").utf8_string(), fps));
	}
}

void WindowSystem::GetWindowSize(int& w, int& h)
{
	w = g_window_info.width;
	h = g_window_info.height;
}

void WindowSystem::GetPadWindowSize(int& w, int& h)
{
	if (g_window_info.pad_open)
	{
		w = g_window_info.pad_width;
		h = g_window_info.pad_height;
	}
	else
	{
		w = 0;
		h = 0;
	}
}

void WindowSystem::GetWindowPhysSize(int& w, int& h)
{
	w = g_window_info.phys_width;
	h = g_window_info.phys_height;
}

void WindowSystem::GetPadWindowPhysSize(int& w, int& h)
{
	if (g_window_info.pad_open)
	{
		w = g_window_info.phys_pad_width;
		h = g_window_info.phys_pad_height;
	}
	else
	{
		w = 0;
		h = 0;
	}
}

double WindowSystem::GetWindowDPIScale()
{
	return g_window_info.dpi_scale;
}

double WindowSystem::GetPadDPIScale()
{
	return g_window_info.pad_open ? g_window_info.pad_dpi_scale.load() : 1.0;
}

bool WindowSystem::IsPadWindowOpen()
{
	return g_window_info.pad_open;
}

bool WindowSystem::IsKeyDown(uint32 key)
{
	return g_window_info.get_keystate(key);
}

bool WindowSystem::IsKeyDown(PlatformKeyCodes platformKey)
{
	uint32 key = 0;

	switch (platformKey)
	{
#if BOOST_OS_WINDOWS
	case PlatformKeyCodes::LCONTROL:
		key = VK_LCONTROL;
		break;
	case PlatformKeyCodes::RCONTROL:
		key = VK_RCONTROL;
		break;
	case PlatformKeyCodes::TAB:
		key = VK_TAB;
		break;
	case PlatformKeyCodes::ESCAPE:
		key = VK_ESCAPE;
		break;
#elif BOOST_OS_LINUX
	case PlatformKeyCodes::LCONTROL:
		key = GDK_KEY_Control_L;
		break;
	case PlatformKeyCodes::RCONTROL:
		key = GDK_KEY_Control_R;
		break;
	case PlatformKeyCodes::TAB:
		key = GDK_KEY_Tab;
		break;
	case PlatformKeyCodes::ESCAPE:
		key = GDK_KEY_Escape;
		break;
#elif BOOST_OS_MACOS
	case PlatformKeyCodes::LCONTROL:
		key = kVK_Control;
		break;
	case PlatformKeyCodes::RCONTROL:
		key = kVK_RightControl;
		break;
	case PlatformKeyCodes::TAB:
		key = kVK_Tab;
		break;
	case PlatformKeyCodes::ESCAPE:
		key = kVK_Escape;
		break;
#endif
	default:
		return false;
	}

	return WindowSystem::IsKeyDown(key);
}

std::string WindowSystem::GetKeyCodeName(uint32 button)
{
#if BOOST_OS_WINDOWS
	LONG scan_code = MapVirtualKeyA((UINT)button, MAPVK_VK_TO_VSC_EX);
	if (HIBYTE(scan_code))
		scan_code |= 0x100;

	// because MapVirtualKey strips the extended bit for some keys
	switch (button)
	{
	case VK_LEFT:
	case VK_UP:
	case VK_RIGHT:
	case VK_DOWN: // arrow keys
	case VK_PRIOR:
	case VK_NEXT: // page up and page down
	case VK_END:
	case VK_HOME:
	case VK_INSERT:
	case VK_DELETE:
	case VK_DIVIDE: // numpad slash
	case VK_NUMLOCK:
	{
		scan_code |= 0x100; // set extended bit
		break;
	}
	}

	scan_code <<= 16;

	char key_name[128];
	if (GetKeyNameTextA(scan_code, key_name, std::size(key_name)) != 0)
		return key_name;
	else
		return fmt::format("key_{}", button);
#elif BOOST_OS_LINUX
	return gdk_keyval_name(button);
#else
	return fmt::format("key_{}", button);
#endif
}

bool WindowSystem::InputConfigWindowHasFocus()
{
	return g_inputConfigWindowHasFocus;
}

void WindowSystem::NotifyGameLoaded()
{
	std::shared_lock lock(g_mutex);
	if (g_mainFrame)
	{
		g_mainFrame->OnGameLoaded();
		g_mainFrame->UpdateSettingsAfterGameLaunch();
	}
}

void WindowSystem::NotifyGameExited()
{
	std::shared_lock lock(g_mutex);
	if (g_mainFrame)
		g_mainFrame->RestoreSettingsAfterGameExited();
}

void WindowSystem::RefreshGameList()
{
	std::shared_lock lock(g_mutex);
	if (g_mainFrame)
	{
		g_mainFrame->RequestGameListRefresh();
	}
}

void WindowSystem::CaptureInput(const ControllerState& currentState, const ControllerState& lastState)
{
	HotkeySettings::CaptureInput(currentState, lastState);
}

bool WindowSystem::IsFullScreen()
{
	return g_window_info.is_fullscreen;
}
