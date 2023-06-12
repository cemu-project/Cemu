#include <Cafe/TitleList/TitleInfo.h>
#include <Cafe/TitleList/TitleList.h>
#include <Cafe/CafeSystem.h>
#include "AndroidGuiWrapper.h"
#include "GameIconLoader.h"
#include "GameTitleLoader.h"

bool g_inputConfigWindowHasFocus;
WindowInfo g_window_info;

GameTitleLoader g_gameTitleLoader;
GameIconLoader g_gameIconLoader;

void gui_setOnGameTitleLoaded(const std::shared_ptr<GameTitleLoadedCallback> &onGameTitleLoaded)
{
    g_gameTitleLoader.setOnTitleLoaded(onGameTitleLoaded);
}

void gui_setOnGameIconLoaded(const std::shared_ptr<class GameIconLoadedCallback>& onGameIconLoaded)
{
    g_gameIconLoader.setOnIconLoaded(onGameIconLoaded);
}

void gui_requestGameIcon(TitleId titleId)
{
    g_gameIconLoader.requestIcon(titleId);
}
void gui_reloadGameTitles()
{
    g_gameTitleLoader.reloadGameTitles();
}
void gui_createCemuDirs()
{

}
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
    w = gui_getWindowInfo().width;
    h = gui_getWindowInfo().height;
}
void gui_getPadWindowSize(int& w, int& h)
{
    w = gui_getWindowInfo().pad_width;
    h = gui_getWindowInfo().pad_height;
}
void gui_getWindowPhysSize(int& w, int& h)
{
    gui_getWindowSize(w, h);
}
void gui_getPadWindowPhysSize(int& w, int& h)
{
    gui_getPadWindowSize(w, h);
}
double gui_getWindowDPIScale()
{
    return 1.0;
}
double gui_getPadDPIScale()
{
    return 1.0;
}
bool gui_isPadWindowOpen()
{
    return false;
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
    return false;
}
bool gui_saveScreenshotToClipboard(std::vector<uint8>& data, int width, int height)
{
    return false;
}
/*
 * Returns true if a screenshot request is queued
 * Once this function has returned true, it will reset back to
 * false until the next time a screenshot is requested
 */
bool gui_hasScreenshotRequest()
{
    return false;
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

void gui_startGame(TitleId titleId) {
    TitleInfo launchTitle;

    if (!CafeTitleList::GetFirstByTitleId(titleId, launchTitle))
        return;

    if (launchTitle.IsValid())
    {
        // the title might not be in the TitleList, so we add it as a temporary entry
        CafeTitleList::AddTitleFromPath(launchTitle.GetPath());
        // title is valid, launch from TitleId
        TitleId baseTitleId;
        if (!CafeTitleList::FindBaseTitleId(launchTitle.GetAppTitleId(), baseTitleId))
        {
        }
        CafeSystem::STATUS_CODE statusCode = CafeSystem::PrepareForegroundTitle(baseTitleId);
        if (statusCode == CafeSystem::STATUS_CODE::INVALID_RPX)
        {
        }
        else if (statusCode == CafeSystem::STATUS_CODE::UNABLE_TO_MOUNT)
        {
        }
        else if (statusCode != CafeSystem::STATUS_CODE::SUCCESS)
        {
        }
    }
    else  // if (launchTitle.GetFormat() == TitleInfo::TitleDataFormat::INVALID_STRUCTURE )
    {
        // title is invalid, if its an RPX/ELF we can launch it directly
        // otherwise its an error
        CafeTitleFileType fileType = DetermineCafeSystemFileType(launchTitle.GetPath());
        if (fileType == CafeTitleFileType::RPX || fileType == CafeTitleFileType::ELF)
        {
            CafeSystem::STATUS_CODE statusCode = CafeSystem::PrepareForegroundTitleFromStandaloneRPX(
                    launchTitle.GetPath());
            if (statusCode != CafeSystem::STATUS_CODE::SUCCESS)
            {
            }
        }
    }

    CafeSystem::LaunchForegroundTitle();
}
