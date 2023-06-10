#include "gui/guiWrapper.h"


bool g_inputConfigWindowHasFocus;
WindowInfo g_window_info;

void gui_loggingWindowLog(std::string_view filter, std::string_view message)
{
}
void gui_loggingWindowLog(std::string_view message)
{
}

void gui_create()
{
}

WindowInfo& gui_getWindowInfo()
{
    return g_window_info;
}

void gui_updateWindowTitles(bool isIdle, bool isLoading, double fps)
{
}
void gui_getWindowSize(int& w, int& h)
{
}
void gui_getPadWindowSize(int& w, int& h)
{
}
void gui_getWindowPhysSize(int& w, int& h)
{
}
void gui_getPadWindowPhysSize(int& w, int& h)
{
}
double gui_getWindowDPIScale()
{
}
double gui_getPadDPIScale()
{
}
bool gui_isPadWindowOpen()
{
}
bool gui_isKeyDown(uint32 key)
{
    return false;
}
bool gui_isKeyDown(PlatformKeyCodes key)
{
    return false;
}

void gui_notifyGameLoaded()
{
}
void gui_notifyGameExited()
{
}

bool gui_isFullScreen()
{
    return false;
}

void gui_initHandleContextFromWxWidgetsWindow(WindowHandleInfo& handleInfoOut, class wxWindow* wxw)
{
}

void gui_requestGameListRefresh()
{
}

std::string gui_RawKeyCodeToString(uint32 keyCode)
{
}

bool gui_saveScreenshotToFile(const fs::path& imagePath, std::vector<uint8>& data, int width, int height)
{
}
bool gui_saveScreenshotToClipboard(std::vector<uint8>& data, int width, int height)
{
}
/*
 * Returns true if a screenshot request is queued
 * Once this function has returned true, it will reset back to
 * false until the next time a screenshot is requested
 */
bool gui_hasScreenshotRequest()
{
}

// debugger stuff
void debuggerWindow_updateViewThreadsafe2()
{
}
void debuggerWindow_notifyDebugBreakpointHit2()
{
}
void debuggerWindow_notifyRun()
{
}
void debuggerWindow_moveIP()
{
}
void debuggerWindow_notifyModuleLoaded(void* module)
{
}
void debuggerWindow_notifyModuleUnloaded(void* module)
{
}
