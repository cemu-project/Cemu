#pragma once

#include <wx/wx.h>
#include <wx/dataview.h>
#include <wx/infobar.h>
#include "wxcomponents/checkedlistctrl.h"

#include "gui/PadViewFrame.h"
#include "gui/MemorySearcherTool.h"

#include "config/XMLConfig.h"

#include "gui/LoggingWindow.h"
#include "gui/components/wxGameList.h"

#include <future>
#include "Cafe/HW/Espresso/Debugger/GDBStub.h"
#include "Cafe/CafeSystem.h"

class DebuggerWindow2;
struct GameEntry;
class DiscordPresence;
class TitleManager;
class wxLaunchGameEvent;

wxDECLARE_EVENT(wxEVT_LAUNCH_GAME, wxLaunchGameEvent);
wxDECLARE_EVENT(wxEVT_SET_WINDOW_TITLE, wxCommandEvent);

class wxLaunchGameEvent : public wxCommandEvent
{
public:
	enum class INITIATED_BY
	{
		MENU, // via file menu
		DRAG_AND_DROP,
		GAME_LIST,
		TITLE_MANAGER,
		COMMAND_LINE, // -g parameter
	};

	wxLaunchGameEvent(fs::path path, INITIATED_BY initiatedBy)
		: wxCommandEvent(wxEVT_LAUNCH_GAME), m_launchPath(path), m_initiatedBy(initiatedBy) {}

	[[nodiscard]] fs::path GetPath() const { return m_launchPath; }
	[[nodiscard]] INITIATED_BY GetInitiatedBy() const { return m_initiatedBy; }

	wxEvent* Clone() const { return new wxLaunchGameEvent(*this); }

private:
	fs::path m_launchPath;
	INITIATED_BY m_initiatedBy;
};

class MainWindow : public wxFrame, public CafeSystem::SystemImplementation
{
	friend class CemuApp;

public:
	MainWindow();
	~MainWindow();

    void CreateGameListAndStatusBar();
    void DestroyGameListAndStatusBar();

	void UpdateSettingsAfterGameLaunch();
	void RestoreSettingsAfterGameExited();

	bool FileLoad(const fs::path launchPath, wxLaunchGameEvent::INITIATED_BY initiatedBy);

	[[nodiscard]] bool IsGameLaunched() const { return m_game_launched; }

	void SetFullScreen(bool state);
	void SetMenuVisible(bool state);
	void UpdateNFCMenu();
	bool IsMenuHidden() const;
	void TogglePadView();

#if BOOST_OS_WINDOWS
	WXLRESULT MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam) override;
#endif
	void OpenSettings();

	PadViewFrame* GetPadView() const { return m_padView; }

	void OnSizeEvent(wxSizeEvent& event);
	void OnDPIChangedEvent(wxDPIChangedEvent& event);
	void OnMove(wxMoveEvent& event);

	void OnDebuggerClose(wxCloseEvent& event);
	void OnPadClose(wxCloseEvent& event);
	void OnMemorySearcherClose(wxCloseEvent& event);
	void OnMouseWheel(wxMouseEvent& event);
	void OnClose(wxCloseEvent& event);
	void OnFileMenu(wxCommandEvent& event);
	void OnOpenFolder(wxCommandEvent& event);
	void OnLaunchFromFile(wxLaunchGameEvent& event);
	void OnInstallUpdate(wxCommandEvent& event);
	void OnFileExit(wxCommandEvent& event);
	void OnNFCMenu(wxCommandEvent& event);
	void OnOptionsInput(wxCommandEvent& event);
	void OnAccountSelect(wxCommandEvent& event);
	void OnConsoleLanguage(wxCommandEvent& event);
	void OnHelpAbout(wxCommandEvent& event);
	void OnHelpGettingStarted(wxCommandEvent& event);
	void OnHelpUpdate(wxCommandEvent& event);
	void OnDebugSetting(wxCommandEvent& event);
	void OnDebugLoggingToggleFlagGeneric(wxCommandEvent& event);
	void OnPPCInfoToggle(wxCommandEvent& event);
	void OnDebugDumpUsedTextures(wxCommandEvent& event);
	void OnDebugDumpUsedShaders(wxCommandEvent& event);
	void OnLoggingWindow(wxCommandEvent& event);
	void OnGDBStubToggle(wxCommandEvent& event);
	void OnDebugViewPPCThreads(wxCommandEvent& event);
	void OnDebugViewPPCDebugger(wxCommandEvent& event);
	void OnDebugViewAudioDebugger(wxCommandEvent& event);
	void OnDebugViewTextureRelations(wxCommandEvent& event);
	void OnMouseMove(wxMouseEvent& event);
	void OnMouseLeft(wxMouseEvent& event);
	void OnMouseRight(wxMouseEvent& event);
	void OnGameListBeginUpdate(wxCommandEvent& event);
	void OnGameListEndUpdate(wxCommandEvent& event);
	void OnAccountListRefresh(wxCommandEvent& event);
	void OnRequestGameListRefresh(wxCommandEvent& event);
	void OnSetWindowTitle(wxCommandEvent& event);

	void OnKeyUp(wxKeyEvent& event);
	void OnChar(wxKeyEvent& event);

	void OnToolsInput(wxCommandEvent& event);
	void OnGesturePan(wxPanGestureEvent& event);

	void OnGameLoaded();

	void AsyncSetTitle(std::string_view windowTitle);

	void CreateCanvas();
	void DestroyCanvas();

	static void ShowCursor(bool state);

	uintptr_t GetRenderCanvasHWND();

	static void RequestGameListRefresh();
	static void RequestLaunchGame(fs::path filePath, wxLaunchGameEvent::INITIATED_BY initiatedBy);

private:
	void RecreateMenu();
	static wxString GetInitialWindowTitle();
	void ShowGettingStartedDialog();

	bool InstallUpdate(const fs::path& metaFilePath);

	void OnTimer(wxTimerEvent& event);

	// CafeSystem implementation
	void CafeRecreateCanvas() override;

	void OnRequestRecreateCanvas(wxCommandEvent& event);

	wxRect GetDesktopRect();

	MemorySearcherTool* m_toolWindow = nullptr;
	TitleManager* m_title_manager = nullptr;
	PadViewFrame* m_padView = nullptr;
	wxWindow* m_graphic_pack_window = nullptr;

	wxTimer* m_timer;
	wxPoint m_mouse_position{};
	std::chrono::steady_clock::time_point m_last_mouse_move_time;
	wxSize m_restored_size;
	wxPoint m_restored_position;

	bool m_menu_visible = false;
	bool m_game_launched = false;

	#ifdef ENABLE_DISCORD_RPC
	std::unique_ptr<DiscordPresence> m_discord;
	#endif

	std::string m_launched_game_name;

	wxMenuItem* m_gdbstub_toggle{};
	DebuggerWindow2* m_debugger_window = nullptr;
	LoggingWindow* m_logging_window = nullptr;

	std::future<bool> m_update_available;

	wxMenuItem* m_check_update_menu{};
	void LoadSettings();
	void SaveSettings();

	void OnGraphicWindowClose(wxCloseEvent& event);
	void OnGraphicWindowOpen(wxTitleIdEvent& event);

	// panels
	wxPanel* m_main_panel{}, * m_game_panel{};

	// rendering
	wxWindow* m_render_canvas{};

	// gamelist
	wxGameList* m_game_list{};
	wxInfoBar* m_info_bar{};

	// menu
	wxMenuBar* m_menuBar{};

	// file
	wxMenu* m_fileMenu{};
	wxMenuItem* m_fileMenuSeparator0{};
	wxMenuItem* m_fileMenuSeparator1{};
	wxMenuItem* m_loadMenuItem{};
	wxMenuItem* m_installUpdateMenuItem{};
	wxMenuItem* m_exitMenuItem{};

	// options
	wxMenu* m_optionsAccountMenu{};

	wxMenuItem* m_fullscreenMenuItem{};
	wxMenuItem* m_padViewMenuItem{};

	// tools
	wxMenuItem* m_memorySearcherMenuItem{};

	// cpu
	wxMenu* m_cpuTimerSubmenu{};

	// nfc
	wxMenu* m_nfcMenu{};
	wxMenuItem* m_nfcMenuSeparator0{};

	// debug
	wxMenu* m_debugMenu{};
	wxMenu* m_loggingSubmenu{};
	wxMenuItem* m_asyncCompile{};

wxDECLARE_EVENT_TABLE();
};

extern MainWindow* g_mainFrame;
