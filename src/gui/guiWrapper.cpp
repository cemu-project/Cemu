#if BOOST_OS_LINUX
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkwindow.h>
#include <gdk/gdkx.h>
#ifdef HAS_WAYLAND
#include <gdk/gdkwayland.h>
#endif
#endif

#include "gui/wxgui.h"
#include "gui/guiWrapper.h"
#include "gui/CemuApp.h"
#include "gui/MainWindow.h"
#include "gui/debugger/DebuggerWindow2.h"
#include "Cafe/HW/Latte/Core/Latte.h"
#include "config/ActiveSettings.h"
#include "config/NetworkSettings.h"
#include "config/CemuConfig.h"
#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "Cafe/CafeSystem.h"

#include "wxHelper.h"

WindowInfo g_window_info {};

std::shared_mutex g_mutex;
MainWindow* g_mainFrame = nullptr;

#if BOOST_OS_WINDOWS
void _wxLaunch()
{
	SetThreadName("MainThread_UI");
	wxEntry();
}
#endif

void gui_create()
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

WindowInfo& gui_getWindowInfo()
{
	return g_window_info;
}

void gui_updateWindowTitles(bool isIdle, bool isLoading, double fps)
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
	if(g_renderer)
	{
		switch(g_renderer->GetType())
		{
		case RendererAPI::OpenGL:
			renderer =  "[OpenGL]";
			break;
		case RendererAPI::Vulkan: 
			renderer = "[Vulkan]";
			break;
		default: ;
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

void gui_getWindowSize(int& w, int& h)
{
	w = g_window_info.width;
	h = g_window_info.height;
}

void gui_getPadWindowSize(int& w, int& h)
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

void gui_getWindowPhysSize(int& w, int& h)
{
	w = g_window_info.phys_width;
	h = g_window_info.phys_height;
}

void gui_getPadWindowPhysSize(int& w, int& h)
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

double gui_getWindowDPIScale()
{
	return g_window_info.dpi_scale;
}

double gui_getPadDPIScale()
{
	return g_window_info.pad_open ? g_window_info.pad_dpi_scale.load() : 1.0;
}

bool gui_isPadWindowOpen()
{
	return g_window_info.pad_open;
}

#if BOOST_OS_LINUX
std::string gui_gtkRawKeyCodeToString(uint32 keyCode)
{
	return gdk_keyval_name(keyCode);
}
#endif

void gui_initHandleContextFromWxWidgetsWindow(WindowHandleInfo& handleInfoOut, class wxWindow* wxw)
{
#if BOOST_OS_WINDOWS
	handleInfoOut.hwnd = wxw->GetHWND();
#elif BOOST_OS_LINUX
    GtkWidget* gtkWidget = (GtkWidget*)wxw->GetHandle(); // returns GtkWidget
    gtk_widget_realize(gtkWidget);
    GdkWindow* gdkWindow = gtk_widget_get_window(gtkWidget);
	GdkDisplay* gdkDisplay = gdk_window_get_display(gdkWindow);
	if(GDK_IS_X11_WINDOW(gdkWindow))
	{
		handleInfoOut.backend = WindowHandleInfo::Backend::X11;
		handleInfoOut.xlib_window = gdk_x11_window_get_xid(gdkWindow);
		handleInfoOut.xlib_display = gdk_x11_display_get_xdisplay(gdkDisplay);
		if(!handleInfoOut.xlib_display)
		{
			cemuLog_log(LogType::Force, "Unable to get xlib display");
		}
	}
	else 
#ifdef HAS_WAYLAND
	if(GDK_IS_WAYLAND_WINDOW(gdkWindow))
	{
		handleInfoOut.backend = WindowHandleInfo::Backend::WAYLAND;
		handleInfoOut.surface = gdk_wayland_window_get_wl_surface(gdkWindow);
		handleInfoOut.display = gdk_wayland_display_get_wl_display(gdkDisplay);
	}
	else
#endif
	{
		cemuLog_log(LogType::Force, "Unsuported GTK backend");
	}
#else
	handleInfoOut.handle = wxw->GetHandle();
#endif
}

bool gui_isKeyDown(uint32 key)
{
	return g_window_info.get_keystate(key);
}

bool gui_isKeyDown(PlatformKeyCodes key)
{
	return gui_isKeyDown((std::underlying_type_t<PlatformKeyCodes>)key);
}

void gui_notifyGameLoaded()
{
	std::shared_lock lock(g_mutex);
	if (g_mainFrame)
	{
		g_mainFrame->OnGameLoaded();
		g_mainFrame->UpdateSettingsAfterGameLaunch();
	}
}

void gui_notifyGameExited()
{
	std::shared_lock lock(g_mutex);
	if(g_mainFrame)
		g_mainFrame->RestoreSettingsAfterGameExited();
}

bool gui_isFullScreen()
{
	return g_window_info.is_fullscreen;
}

bool gui_hasScreenshotRequest()
{
	const bool result = g_window_info.has_screenshot_request;
	g_window_info.has_screenshot_request = false;
	return result;
}

extern DebuggerWindow2* g_debugger_window;
void debuggerWindow_updateViewThreadsafe2()
{
	if (g_debugger_window)
	{
		auto* evt = new wxCommandEvent(wxEVT_UPDATE_VIEW);
		wxQueueEvent(g_debugger_window, evt);
	}
}

void debuggerWindow_moveIP()
{
	if (g_debugger_window)
	{
		auto* evt = new wxCommandEvent(wxEVT_MOVE_IP);
		wxQueueEvent(g_debugger_window, evt);
	}
}

void debuggerWindow_notifyDebugBreakpointHit2()
{
	if (g_debugger_window)
	{
		auto* evt = new wxCommandEvent(wxEVT_BREAKPOINT_HIT);
		wxQueueEvent(g_debugger_window, evt);
	}
}

void debuggerWindow_notifyRun()
{
	if (g_debugger_window)
	{
		auto* evt = new wxCommandEvent(wxEVT_RUN);
		wxQueueEvent(g_debugger_window, evt);
	}
}

void debuggerWindow_notifyModuleLoaded(void* module)
{
	if (g_debugger_window)
	{
		auto* evt = new wxCommandEvent(wxEVT_NOTIFY_MODULE_LOADED);
		evt->SetClientData(module);
		wxQueueEvent(g_debugger_window, evt);
	}
}

void debuggerWindow_notifyModuleUnloaded(void* module)
{
	if (g_debugger_window)
	{
		auto* evt = new wxCommandEvent(wxEVT_NOTIFY_MODULE_UNLOADED);
		evt->SetClientData(module);
		wxQueueEvent(g_debugger_window, evt);
	}
}
