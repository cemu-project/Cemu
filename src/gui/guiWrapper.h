#pragma once

#include <atomic>

#if BOOST_OS_LINUX
#include "xcb/xproto.h"
#endif

struct WindowHandleInfo
{
#if BOOST_OS_WINDOWS
	std::atomic<HWND> hwnd;
#elif BOOST_OS_LINUX
    // XLIB
    Display* xlib_display{};
    Window xlib_window{};

	// XCB (not used by GTK so we cant retrieve these without making our own window)
	//xcb_connection_t* xcb_con{};
	//xcb_window_t xcb_window{};
	// Wayland
	// todo
#else
	void* handle;
#endif
};

struct WindowInfo
{
	std::atomic_bool app_active; // our app is active/has focus

	std::atomic_int32_t width, height; 	// client size of main window

	std::atomic_bool pad_open; // if separate pad view is open
	std::atomic_int32_t pad_width, pad_height; 	// client size of pad window

	std::atomic_bool pad_maximized = false;
	std::atomic_int32_t restored_pad_x = -1, restored_pad_y = -1;
	std::atomic_int32_t restored_pad_width = -1, restored_pad_height = -1;

	std::atomic_bool has_screenshot_request;
	std::atomic_bool is_fullscreen;

	std::atomic_bool keydown[256]{};

	WindowHandleInfo window_main;
	WindowHandleInfo window_pad;

	// canvas
	WindowHandleInfo canvas_main;
	WindowHandleInfo canvas_pad;
};

void gui_create();

WindowInfo& gui_getWindowInfo();

void gui_updateWindowTitles(bool isIdle, bool isLoading, double fps);
void gui_getWindowSize(int* w, int* h);
void gui_getPadWindowSize(int* w, int* h);
bool gui_isPadWindowOpen();
bool gui_isKeyDown(int key);

void gui_notifyGameLoaded();
void gui_notifyGameExited();

bool gui_isFullScreen();

void gui_initHandleContextFromWxWidgetsWindow(WindowHandleInfo& handleInfoOut, class wxWindow* wxw);

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