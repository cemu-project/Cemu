#pragma once

#if BOOST_OS_LINUX
#include "xcb/xproto.h"
#include <gdk/gdkkeysyms.h>
#endif

#if BOOST_OS_MACOS
#include <Carbon/Carbon.h>
#endif

struct WindowHandleInfo
{
#if BOOST_OS_WINDOWS
	std::atomic<HWND> hwnd;
#elif BOOST_OS_LINUX
	enum class Backend
	{
		X11,
		WAYLAND,
	} backend;
    // XLIB
    Display* xlib_display{};
    Window xlib_window{};

	// XCB (not used by GTK so we cant retrieve these without making our own window)
	//xcb_connection_t* xcb_con{};
	//xcb_window_t xcb_window{};
	#ifdef HAS_WAYLAND
	struct wl_display* display;
	struct wl_surface* surface;
	#endif // HAS_WAYLAND
#else
	void* handle;
#endif
};

enum struct PlatformKeyCodes : uint32
{
#if BOOST_OS_WINDOWS
	LCONTROL = VK_LCONTROL,
	RCONTROL = VK_RCONTROL,
	TAB = VK_TAB,
	ESCAPE = VK_ESCAPE,
#elif BOOST_OS_LINUX
	LCONTROL = GDK_KEY_Control_L,
	RCONTROL = GDK_KEY_Control_R,
	TAB = GDK_KEY_Tab,
	ESCAPE = GDK_KEY_Escape,
#elif BOOST_OS_MACOS
	LCONTROL = kVK_Control,
	RCONTROL = kVK_RightControl,
	TAB = kVK_Tab,
	ESCAPE = kVK_Escape,
#else
	LCONTROL = 0,
	RCONTROL = 0,
	TAB = 0,
	ESCAPE = 0,
#endif
};

struct WindowInfo
{
	std::atomic_bool app_active; // our app is active/has focus

	std::atomic_int32_t width, height; 	// client size of main window
	std::atomic_int32_t phys_width, phys_height; 	// client size of main window in physical pixels
	std::atomic<double> dpi_scale;

	std::atomic_bool pad_open; // if separate pad view is open
	std::atomic_int32_t pad_width, pad_height; 	// client size of pad window
	std::atomic_int32_t phys_pad_width, phys_pad_height; 	// client size of pad window in physical pixels
	std::atomic<double> pad_dpi_scale;

	std::atomic_bool pad_maximized = false;
	std::atomic_int32_t restored_pad_x = -1, restored_pad_y = -1;
	std::atomic_int32_t restored_pad_width = -1, restored_pad_height = -1;

	std::atomic_bool has_screenshot_request;
	std::atomic_bool is_fullscreen;

	void set_keystate(uint32 keycode, bool state)
	{
		const std::lock_guard<std::mutex> lock(keycode_mutex);
		m_keydown[keycode] = state;
	}

	bool get_keystate(uint32 keycode)
	{
		const std::lock_guard<std::mutex> lock(keycode_mutex);
		auto result = m_keydown.find(keycode);
		if (result == m_keydown.end())
			return false;
		return result->second;
	}

	void set_keystatesup()
	{
		const std::lock_guard<std::mutex> lock(keycode_mutex);
		std::for_each(m_keydown.begin(), m_keydown.end(), [](std::pair<const uint32, bool>& el){ el.second = false; });
	}

	template <typename fn>
	void iter_keystates(fn f)
	{
		const std::lock_guard<std::mutex> lock(keycode_mutex);
		std::for_each(m_keydown.cbegin(), m_keydown.cend(), f);
	}

	WindowHandleInfo window_main;
	WindowHandleInfo window_pad;

	// canvas
	WindowHandleInfo canvas_main;
	WindowHandleInfo canvas_pad;
   private:
   	std::mutex keycode_mutex;
	// m_keydown keys must be valid ImGuiKey values
	std::unordered_map<uint32, bool> m_keydown;
};

void gui_create();

WindowInfo& gui_getWindowInfo();

void gui_updateWindowTitles(bool isIdle, bool isLoading, double fps);
void gui_getWindowSize(int& w, int& h);
void gui_getPadWindowSize(int& w, int& h);
void gui_getWindowPhysSize(int& w, int& h);
void gui_getPadWindowPhysSize(int& w, int& h);
double gui_getWindowDPIScale();
double gui_getPadDPIScale();
bool gui_isPadWindowOpen();
bool gui_isKeyDown(uint32 key);
bool gui_isKeyDown(PlatformKeyCodes key);

void gui_notifyGameLoaded();
void gui_notifyGameExited();

bool gui_isFullScreen();

void gui_initHandleContextFromWxWidgetsWindow(WindowHandleInfo& handleInfoOut, class wxWindow* wxw);

#if BOOST_OS_LINUX
std::string gui_gtkRawKeyCodeToString(uint32 keyCode);
#endif
/*
* Returns true if a screenshot request is queued
* Once this function has returned true, it will reset back to
* false until the next time a screenshot is requested
*/
bool gui_hasScreenshotRequest();

// debugger stuff
void debuggerWindow_updateViewThreadsafe2();
void debuggerWindow_notifyDebugBreakpointHit2();
void debuggerWindow_notifyRun();
void debuggerWindow_moveIP();
void debuggerWindow_notifyModuleLoaded(void* module);
void debuggerWindow_notifyModuleUnloaded(void* module);
