#include "gui/wxgui.h"
#include "gui/MainWindow.h"
#include "gui/guiWrapper.h"

#include <wx/mstream.h>
#include <wx/clipbrd.h>

#include "gui/GameUpdateWindow.h"
#include "gui/PadViewFrame.h"
#include "gui/windows/TextureRelationViewer/TextureRelationWindow.h"
#include "gui/windows/PPCThreadsViewer/DebugPPCThreadsWindow.h"
#include "audio/audioDebuggerWindow.h"
#include "gui/canvas/OpenGLCanvas.h"
#include "gui/canvas/VulkanCanvas.h"
#include "Cafe/OS/libs/nn_nfp/nn_nfp.h"
#include "Cafe/OS/libs/swkbd/swkbd.h"
#include "gui/debugger/DebuggerWindow2.h"
#include "util/helpers/helpers.h"
#include "config/CemuConfig.h"
#include "Cemu/DiscordPresence/DiscordPresence.h"
#include "util/ScreenSaver/ScreenSaver.h"
#include "gui/GeneralSettings2.h"
#include "gui/GraphicPacksWindow2.h"
#include "gui/CemuApp.h"
#include "gui/CemuUpdateWindow.h"
#include "gui/LoggingWindow.h"
#include "config/ActiveSettings.h"
#include "config/LaunchSettings.h"

#include "Cafe/Filesystem/FST/FST.h"

#include "gui/TitleManager.h"

#include "Cafe/CafeSystem.h"

#include "util/helpers/SystemException.h"
#include "gui/DownloadGraphicPacksWindow.h"
#include "gui/GettingStartedDialog.h"
#include "gui/helpers/wxHelpers.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VsyncDriver.h"
#include "gui/input/InputSettings2.h"
#include "input/InputManager.h"

#if BOOST_OS_WINDOWS
#define exit(__c) ExitProcess(__c)
#else
#define exit(__c) _Exit(__c)
#endif

#if BOOST_OS_LINUX || BOOST_OS_MACOS
#include "resource/embedded/resources.h"
#endif

#if BOOST_OS_LINUX && HAS_WAYLAND
#include "gui/helpers/wxWayland.h"
#endif

//GameMode support
#if BOOST_OS_LINUX && defined(ENABLE_FERAL_GAMEMODE)
#include "gamemode_client.h"
#endif

#include "Cafe/TitleList/TitleInfo.h"
#include "Cafe/TitleList/TitleList.h"
#include "wxHelper.h"

extern WindowInfo g_window_info;
extern std::shared_mutex g_mutex;

enum
{
	// ui elements
	MAINFRAME_GAMELIST_ID = 20000, //wxID_HIGHEST + 1,
	// file
	MAINFRAME_MENU_ID_FILE_LOAD = 20100,
	MAINFRAME_MENU_ID_FILE_INSTALL_UPDATE,
	MAINFRAME_MENU_ID_FILE_OPEN_CEMU_FOLDER,
	MAINFRAME_MENU_ID_FILE_OPEN_MLC_FOLDER,
	MAINFRAME_MENU_ID_FILE_EXIT,
	MAINFRAME_MENU_ID_FILE_END_EMULATION,
	MAINFRAME_MENU_ID_FILE_RECENT_0,
	MAINFRAME_MENU_ID_FILE_RECENT_LAST = MAINFRAME_MENU_ID_FILE_RECENT_0 + 15,
	// options
	MAINFRAME_MENU_ID_OPTIONS_FULLSCREEN = 20200,
	MAINFRAME_MENU_ID_OPTIONS_SECOND_WINDOW_PADVIEW,
	MAINFRAME_MENU_ID_OPTIONS_GRAPHIC,
	MAINFRAME_MENU_ID_OPTIONS_GRAPHIC_PACKS2,
	MAINFRAME_MENU_ID_OPTIONS_GENERAL,
	MAINFRAME_MENU_ID_OPTIONS_GENERAL2,
	MAINFRAME_MENU_ID_OPTIONS_AUDIO,
	MAINFRAME_MENU_ID_OPTIONS_INPUT,
	// options -> account
	MAINFRAME_MENU_ID_OPTIONS_ACCOUNT_1 = 20350,
	MAINFRAME_MENU_ID_OPTIONS_ACCOUNT_12 = 20350 + 11,
	
	// options -> system language
	MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_JAPANESE = 20500,
	MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_ENGLISH,
	MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_FRENCH,
	MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_GERMAN,
	MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_ITALIAN,
	MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_SPANISH,
	MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_CHINESE,
	MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_KOREAN,
	MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_DUTCH,
	MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_PORTUGUESE,
	MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_RUSSIAN,
	MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_TAIWANESE,
	// tools
	MAINFRAME_MENU_ID_TOOLS_MEMORY_SEARCHER = 20600,
	MAINFRAME_MENU_ID_TOOLS_TITLE_MANAGER,
	MAINFRAME_MENU_ID_TOOLS_DOWNLOAD_MANAGER,
	// cpu
	// cpu->timer speed
	MAINFRAME_MENU_ID_TIMER_SPEED_1X = 20700,
	MAINFRAME_MENU_ID_TIMER_SPEED_2X = 20701,
	MAINFRAME_MENU_ID_TIMER_SPEED_4X = 20702,
	MAINFRAME_MENU_ID_TIMER_SPEED_8X = 20703,
	MAINFRAME_MENU_ID_TIMER_SPEED_05X = 20704,
	MAINFRAME_MENU_ID_TIMER_SPEED_025X = 20705,
	MAINFRAME_MENU_ID_TIMER_SPEED_0125X = 20706,

	// nfc->Touch NFC file
	MAINFRAME_MENU_ID_NFC_TOUCH_NFC_FILE = 21000,
	MAINFRAME_MENU_ID_NFC_RECENT_0,
	MAINFRAME_MENU_ID_NFC_RECENT_LAST = MAINFRAME_MENU_ID_NFC_RECENT_0 + 15,
	// debug
	MAINFRAME_MENU_ID_DEBUG_RENDER_UPSIDE_DOWN = 21100,
	MAINFRAME_MENU_ID_DEBUG_VIEW_LOGGING_WINDOW,
	MAINFRAME_MENU_ID_DEBUG_TOGGLE_GDB_STUB,
	MAINFRAME_MENU_ID_DEBUG_VIEW_PPC_THREADS,
	MAINFRAME_MENU_ID_DEBUG_VIEW_PPC_DEBUGGER,
	MAINFRAME_MENU_ID_DEBUG_VIEW_AUDIO_DEBUGGER,
	MAINFRAME_MENU_ID_DEBUG_VIEW_TEXTURE_RELATIONS,
	MAINFRAME_MENU_ID_DEBUG_AUDIO_AUX_ONLY,
	MAINFRAME_MENU_ID_DEBUG_VK_ACCURATE_BARRIERS,

	// debug->logging
	MAINFRAME_MENU_ID_DEBUG_LOGGING0 = 21500,
	MAINFRAME_MENU_ID_DEBUG_ADVANCED_PPC_INFO = 21599,
	// debug->dump
	MAINFRAME_MENU_ID_DEBUG_DUMP_TEXTURES = 21600,
	MAINFRAME_MENU_ID_DEBUG_DUMP_SHADERS,
	MAINFRAME_MENU_ID_DEBUG_DUMP_RAM,
	MAINFRAME_MENU_ID_DEBUG_DUMP_FST,
	MAINFRAME_MENU_ID_DEBUG_DUMP_CURL_REQUESTS,
	// help
	MAINFRAME_MENU_ID_HELP_ABOUT = 21700,
	MAINFRAME_MENU_ID_HELP_UPDATE,
	MAINFRAME_MENU_ID_HELP_GETTING_STARTED,

	// custom
	MAINFRAME_ID_TIMER1 = 21800,
};

wxDEFINE_EVENT(wxEVT_SET_WINDOW_TITLE, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_REQUEST_GAMELIST_REFRESH, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_LAUNCH_GAME, wxLaunchGameEvent);
wxDEFINE_EVENT(wxEVT_REQUEST_RECREATE_CANVAS, wxCommandEvent);

wxBEGIN_EVENT_TABLE(MainWindow, wxFrame)
EVT_TIMER(MAINFRAME_ID_TIMER1, MainWindow::OnTimer)
EVT_CLOSE(MainWindow::OnClose)
EVT_SIZE(MainWindow::OnSizeEvent)
EVT_DPI_CHANGED(MainWindow::OnDPIChangedEvent)
EVT_MOVE(MainWindow::OnMove)
// file menu
EVT_MENU(MAINFRAME_MENU_ID_FILE_LOAD, MainWindow::OnFileMenu)
EVT_MENU(MAINFRAME_MENU_ID_FILE_INSTALL_UPDATE, MainWindow::OnInstallUpdate)
EVT_MENU(MAINFRAME_MENU_ID_FILE_OPEN_CEMU_FOLDER, MainWindow::OnOpenFolder)
EVT_MENU(MAINFRAME_MENU_ID_FILE_OPEN_MLC_FOLDER, MainWindow::OnOpenFolder)
EVT_MENU(MAINFRAME_MENU_ID_FILE_EXIT, MainWindow::OnFileExit)
EVT_MENU(MAINFRAME_MENU_ID_FILE_END_EMULATION, MainWindow::OnFileMenu)
EVT_MENU_RANGE(MAINFRAME_MENU_ID_FILE_RECENT_0 + 0, MAINFRAME_MENU_ID_FILE_RECENT_LAST, MainWindow::OnFileMenu)
// options -> region menu
EVT_MENU_RANGE(MAINFRAME_MENU_ID_OPTIONS_ACCOUNT_1, MAINFRAME_MENU_ID_OPTIONS_ACCOUNT_12, MainWindow::OnAccountSelect)
EVT_MENU_RANGE(MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_JAPANESE, MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_TAIWANESE, MainWindow::OnConsoleLanguage)
// options menu
EVT_MENU(MAINFRAME_MENU_ID_OPTIONS_FULLSCREEN, MainWindow::OnOptionsInput)
EVT_MENU(MAINFRAME_MENU_ID_OPTIONS_SECOND_WINDOW_PADVIEW, MainWindow::OnOptionsInput)
EVT_MENU(MAINFRAME_MENU_ID_OPTIONS_GRAPHIC, MainWindow::OnOptionsInput)
EVT_MENU(MAINFRAME_MENU_ID_OPTIONS_GRAPHIC_PACKS2, MainWindow::OnOptionsInput)
EVT_MENU(MAINFRAME_MENU_ID_OPTIONS_GENERAL, MainWindow::OnOptionsInput)
EVT_MENU(MAINFRAME_MENU_ID_OPTIONS_GENERAL2, MainWindow::OnOptionsInput)
EVT_MENU(MAINFRAME_MENU_ID_OPTIONS_AUDIO, MainWindow::OnOptionsInput)
EVT_MENU(MAINFRAME_MENU_ID_OPTIONS_INPUT, MainWindow::OnOptionsInput)
// tools menu
EVT_MENU(MAINFRAME_MENU_ID_TOOLS_MEMORY_SEARCHER, MainWindow::OnToolsInput)
EVT_MENU(MAINFRAME_MENU_ID_TOOLS_TITLE_MANAGER, MainWindow::OnToolsInput)
EVT_MENU(MAINFRAME_MENU_ID_TOOLS_DOWNLOAD_MANAGER, MainWindow::OnToolsInput)
// cpu menu
EVT_MENU(MAINFRAME_MENU_ID_TIMER_SPEED_8X, MainWindow::OnDebugSetting)
EVT_MENU(MAINFRAME_MENU_ID_TIMER_SPEED_4X, MainWindow::OnDebugSetting)
EVT_MENU(MAINFRAME_MENU_ID_TIMER_SPEED_2X, MainWindow::OnDebugSetting)
EVT_MENU(MAINFRAME_MENU_ID_TIMER_SPEED_1X, MainWindow::OnDebugSetting)
EVT_MENU(MAINFRAME_MENU_ID_TIMER_SPEED_05X, MainWindow::OnDebugSetting)
EVT_MENU(MAINFRAME_MENU_ID_TIMER_SPEED_025X, MainWindow::OnDebugSetting)
EVT_MENU(MAINFRAME_MENU_ID_TIMER_SPEED_0125X, MainWindow::OnDebugSetting)
// nfc menu
EVT_MENU(MAINFRAME_MENU_ID_NFC_TOUCH_NFC_FILE, MainWindow::OnNFCMenu)
EVT_MENU_RANGE(MAINFRAME_MENU_ID_NFC_RECENT_0 + 0, MAINFRAME_MENU_ID_NFC_RECENT_LAST, MainWindow::OnNFCMenu)
// debug -> logging menu
EVT_MENU_RANGE(MAINFRAME_MENU_ID_DEBUG_LOGGING0 + 0, MAINFRAME_MENU_ID_DEBUG_LOGGING0 + 98, MainWindow::OnDebugLoggingToggleFlagGeneric)
EVT_MENU(MAINFRAME_MENU_ID_DEBUG_ADVANCED_PPC_INFO, MainWindow::OnPPCInfoToggle)
// debug -> dump menu
EVT_MENU(MAINFRAME_MENU_ID_DEBUG_DUMP_TEXTURES, MainWindow::OnDebugDumpUsedTextures)
EVT_MENU(MAINFRAME_MENU_ID_DEBUG_DUMP_SHADERS, MainWindow::OnDebugDumpUsedShaders)
EVT_MENU(MAINFRAME_MENU_ID_DEBUG_DUMP_CURL_REQUESTS, MainWindow::OnDebugSetting)
// debug -> Other options
EVT_MENU(MAINFRAME_MENU_ID_DEBUG_RENDER_UPSIDE_DOWN, MainWindow::OnDebugSetting)
EVT_MENU(MAINFRAME_MENU_ID_DEBUG_AUDIO_AUX_ONLY, MainWindow::OnDebugSetting)
EVT_MENU(MAINFRAME_MENU_ID_DEBUG_VK_ACCURATE_BARRIERS, MainWindow::OnDebugSetting)
EVT_MENU(MAINFRAME_MENU_ID_DEBUG_DUMP_RAM, MainWindow::OnDebugSetting)
EVT_MENU(MAINFRAME_MENU_ID_DEBUG_DUMP_FST, MainWindow::OnDebugSetting)
// debug -> View ...
EVT_MENU(MAINFRAME_MENU_ID_DEBUG_VIEW_LOGGING_WINDOW, MainWindow::OnLoggingWindow)
EVT_MENU(MAINFRAME_MENU_ID_DEBUG_TOGGLE_GDB_STUB, MainWindow::OnGDBStubToggle)
EVT_MENU(MAINFRAME_MENU_ID_DEBUG_VIEW_PPC_THREADS, MainWindow::OnDebugViewPPCThreads)
EVT_MENU(MAINFRAME_MENU_ID_DEBUG_VIEW_PPC_DEBUGGER, MainWindow::OnDebugViewPPCDebugger)
EVT_MENU(MAINFRAME_MENU_ID_DEBUG_VIEW_AUDIO_DEBUGGER, MainWindow::OnDebugViewAudioDebugger)
EVT_MENU(MAINFRAME_MENU_ID_DEBUG_VIEW_TEXTURE_RELATIONS, MainWindow::OnDebugViewTextureRelations)
// help menu
EVT_MENU(MAINFRAME_MENU_ID_HELP_ABOUT, MainWindow::OnHelpAbout)
EVT_MENU(MAINFRAME_MENU_ID_HELP_UPDATE, MainWindow::OnHelpUpdate)
EVT_MENU(MAINFRAME_MENU_ID_HELP_GETTING_STARTED, MainWindow::OnHelpGettingStarted)
// misc
EVT_COMMAND(wxID_ANY, wxEVT_REQUEST_GAMELIST_REFRESH, MainWindow::OnRequestGameListRefresh)

EVT_COMMAND(wxID_ANY, wxEVT_GAMELIST_BEGIN_UPDATE, MainWindow::OnGameListBeginUpdate)
EVT_COMMAND(wxID_ANY, wxEVT_GAMELIST_END_UPDATE, MainWindow::OnGameListEndUpdate)
EVT_COMMAND(wxID_ANY, wxEVT_ACCOUNTLIST_REFRESH, MainWindow::OnAccountListRefresh)
EVT_COMMAND(wxID_ANY, wxEVT_SET_WINDOW_TITLE, MainWindow::OnSetWindowTitle)

EVT_COMMAND(wxID_ANY, wxEVT_REQUEST_RECREATE_CANVAS, MainWindow::OnRequestRecreateCanvas)

wxEND_EVENT_TABLE()

class wxGameDropTarget : public wxFileDropTarget
{
public:
	wxGameDropTarget(MainWindow* window) : m_window(window) {}
	bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& filenames) override
	{
		if(!m_window->IsGameLaunched() && filenames.GetCount() == 1)
			return m_window->FileLoad(_utf8ToPath(filenames[0].utf8_string()), wxLaunchGameEvent::INITIATED_BY::DRAG_AND_DROP);
		
		return false;
	}

private:
	MainWindow* m_window;
};

class wxAmiiboDropTarget : public wxFileDropTarget
{
public:
	wxAmiiboDropTarget(MainWindow* window) : m_window(window) {}
	bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& filenames) override
	{
		if (!m_window->IsGameLaunched() || filenames.GetCount() != 1)
			return false;
		uint32 nfcError;
		std::string path = filenames[0].utf8_string();
		if (nnNfp_touchNfcTagFromFile(_utf8ToPath(path), &nfcError))
		{
			GetConfig().AddRecentNfcFile(path);
			m_window->UpdateNFCMenu();
			return true;
		}
		else
		{
			if (nfcError == NFC_ERROR_NO_ACCESS)
				wxMessageBox(_("Cannot open file"), _("Error"), wxOK | wxCENTRE | wxICON_ERROR);
			else if (nfcError == NFC_ERROR_INVALID_FILE_FORMAT)
				wxMessageBox(_("Not a valid NFC NTAG215 file"), _("Error"), wxOK | wxCENTRE | wxICON_ERROR);
			return false;
		}
	}

private:
	MainWindow* m_window;
};

MainWindow::MainWindow()
	: wxFrame(nullptr, -1, GetInitialWindowTitle(), wxDefaultPosition, wxSize(1280, 720), wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxSYSTEM_MENU | wxCAPTION | wxCLOSE_BOX | wxCLIP_CHILDREN | wxRESIZE_BORDER)
{
	gui_initHandleContextFromWxWidgetsWindow(g_window_info.window_main, this);
	g_mainFrame = this;
	CafeSystem::SetImplementation(this);

	RecreateMenu();
	SetClientSize(1280, 720);
	SetIcon(wxICON(M_WND_ICON128));

#if BOOST_OS_MACOS
	this->EnableFullScreenView(true);
#endif

#if BOOST_OS_WINDOWS
	HICON hWindowIcon = (HICON)LoadImageA(NULL, "M_WND_ICON16", IMAGE_ICON, 16, 16, LR_LOADFROMFILE);
	SendMessage(this->GetHWND(), WM_SETICON, ICON_SMALL, (LPARAM)hWindowIcon);
#endif

	auto* main_sizer = new wxBoxSizer(wxVERTICAL);
    auto load_file = LaunchSettings::GetLoadFile();
    auto load_title_id = LaunchSettings::GetLoadTitleID();
    bool quick_launch = false;

    if (load_file)
    {
        MainWindow::RequestLaunchGame(load_file.value(), wxLaunchGameEvent::INITIATED_BY::COMMAND_LINE);
        quick_launch = true;
    }
    else if (load_title_id)
    {
        TitleInfo info;
        TitleId baseId;
        if (CafeTitleList::FindBaseTitleId(load_title_id.value(), baseId) && CafeTitleList::GetFirstByTitleId(baseId, info))
        {
            MainWindow::RequestLaunchGame(info.GetPath(), wxLaunchGameEvent::INITIATED_BY::COMMAND_LINE);
            quick_launch = true;
        }
        else
        {
            wxString errorMsg = fmt::format("Title ID {:016x} not found", load_title_id.value());
            wxMessageBox(errorMsg, _("Error"), wxOK | wxCENTRE | wxICON_ERROR);

        }
    }
    SetSizer(main_sizer);
    if (!quick_launch)
    {
        CreateGameListAndStatusBar();
    }
    else
    {
      // launching game via -g or -t option. Don't set up or load game list
      m_game_list = nullptr;
      m_info_bar = nullptr;
    }
    SetSizer(main_sizer);

    m_last_mouse_move_time = std::chrono::steady_clock::now();

    m_timer = new wxTimer(this, MAINFRAME_ID_TIMER1);
    m_timer->Start(500);

    LoadSettings();

    auto& config = GetConfig();
    #ifdef ENABLE_DISCORD_RPC
    if (config.use_discord_presence)
            m_discord = std::make_unique<DiscordPresence>();
    #endif

    Bind(wxEVT_OPEN_GRAPHIC_PACK, &MainWindow::OnGraphicWindowOpen, this);
    Bind(wxEVT_LAUNCH_GAME, &MainWindow::OnLaunchFromFile, this);

    if (LaunchSettings::GDBStubEnabled())
    {
            g_gdbstub = std::make_unique<GDBServer>(config.gdb_port);
    }
}

MainWindow::~MainWindow()
{
	if (m_padView)
	{
		m_padView->Destroy();
		m_padView = nullptr;
	}

	m_timer->Stop();

	std::unique_lock lock(g_mutex);
	g_mainFrame = nullptr;
}

void MainWindow::CreateGameListAndStatusBar()
{
    if(m_main_panel)
        return; // already displayed
    m_main_panel = new wxPanel(this);
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    // game list
    m_game_list = new wxGameList(m_main_panel, MAINFRAME_GAMELIST_ID);
    m_game_list->Bind(wxEVT_OPEN_SETTINGS, [this](auto&) {OpenSettings(); });
    m_game_list->SetDropTarget(new wxGameDropTarget(this));
    sizer->Add(m_game_list, 1, wxEXPAND);

    // info, warning bar
    m_info_bar = new wxInfoBar(m_main_panel);
    m_info_bar->SetShowHideEffects(wxSHOW_EFFECT_BLEND, wxSHOW_EFFECT_BLEND);
    m_info_bar->SetEffectDuration(500);
    sizer->Add(m_info_bar, 0, wxALL | wxEXPAND, 5);

    m_main_panel->SetSizer(sizer);

    auto* main_sizer = this->GetSizer();
    main_sizer->Add(m_main_panel, 1, wxEXPAND, 0, nullptr);
}

void MainWindow::DestroyGameListAndStatusBar()
{
    if(!m_main_panel)
        return;
    m_main_panel->Destroy();
    m_main_panel = nullptr;
    m_game_list = nullptr;
    m_info_bar = nullptr;
}

wxString MainWindow::GetInitialWindowTitle()
{
	return BUILD_VERSION_WITH_NAME_STRING;
}

void MainWindow::ShowGettingStartedDialog()
{
	GettingStartedDialog dia(this);
	dia.ShowModal();
	if (dia.HasGamePathChanged() || dia.HasMLCChanged())
		m_game_list->ReloadGameEntries();
		
	TogglePadView();
	
	auto& config = GetConfig();
	m_padViewMenuItem->Check(config.pad_open.GetValue());
	m_fullscreenMenuItem->Check(config.fullscreen.GetValue());
}

namespace coreinit
{
	void OSSchedulerEnd();
};

void MainWindow::OnClose(wxCloseEvent& event)
{
	wxTheClipboard->Flush();

	if(m_game_list)
		m_game_list->OnClose(event);

	if (!IsMaximized() && !gui_isFullScreen())
		m_restored_size = GetSize();

	SaveSettings();
	m_timer->Stop();

	event.Skip();

    CafeSystem::Shutdown();
	DestroyCanvas();
}

bool MainWindow::InstallUpdate(const fs::path& metaFilePath)
{
	try
	{
		GameUpdateWindow frame(*this, metaFilePath);
		const int updateResult = frame.ShowModal();

		if (updateResult == wxID_OK)
		{
			CafeTitleList::AddTitleFromPath(frame.GetTargetPath()); // this will also send a notification to the game list which will update the entry
			wxMessageBox(_("Title installed!"), _("Success"));
			return true;
		}
		else
		{
			if (frame.GetExceptionMessage().empty())
				wxMessageBox(_("Title installation has been canceled!"));
			else
			{
				throw std::runtime_error(frame.GetExceptionMessage());
			}
		}		
	}
	catch(const AbortException&)
	{
		// ignored
	}
	catch (const std::exception& ex)
	{
		wxMessageBox(ex.what(), _("Update error"));
	}
	return false;
}

bool MainWindow::FileLoad(const fs::path launchPath, wxLaunchGameEvent::INITIATED_BY initiatedBy)
{
	TitleInfo launchTitle{ launchPath };
	if (launchTitle.IsValid())
	{
		// the title might not be in the TitleList, so we add it as a temporary entry
		CafeTitleList::AddTitleFromPath(launchPath);
		// title is valid, launch from TitleId
		TitleId baseTitleId;
		if (!CafeTitleList::FindBaseTitleId(launchTitle.GetAppTitleId(), baseTitleId))
		{
			wxString t = _("Unable to launch game because the base files were not found.");
			wxMessageBox(t, _("Error"), wxOK | wxCENTRE | wxICON_ERROR);
			return false;
		}
		CafeSystem::STATUS_CODE r = CafeSystem::PrepareForegroundTitle(baseTitleId);
		if (r == CafeSystem::STATUS_CODE::INVALID_RPX)
		{
			cemu_assert_debug(false);
			return false;
		}
		else if (r == CafeSystem::STATUS_CODE::UNABLE_TO_MOUNT)
		{
			wxString t = _("Unable to mount title.\nMake sure the configured game paths are still valid and refresh the game list.\n\nFile which failed to load:\n");
			t.append(_pathToUtf8(launchPath));
			wxMessageBox(t, _("Error"), wxOK | wxCENTRE | wxICON_ERROR);
			return false;
		}
		else if (r != CafeSystem::STATUS_CODE::SUCCESS)
		{
			wxString t = _("Failed to launch game.");
			t.append(_pathToUtf8(launchPath));
			wxMessageBox(t, _("Error"), wxOK | wxCENTRE | wxICON_ERROR);
			return false;
		}
	}
	else //if (launchTitle.GetFormat() == TitleInfo::TitleDataFormat::INVALID_STRUCTURE )
	{
		// title is invalid, if it's an RPX/ELF we can launch it directly
		// otherwise it's an error
		CafeTitleFileType fileType = DetermineCafeSystemFileType(launchPath);
		if (fileType == CafeTitleFileType::RPX || fileType == CafeTitleFileType::ELF)
		{
			CafeSystem::STATUS_CODE r = CafeSystem::PrepareForegroundTitleFromStandaloneRPX(launchPath);
			if (r != CafeSystem::STATUS_CODE::SUCCESS)
			{
				cemu_assert_debug(false); // todo
				wxString t = _("Failed to launch executable. Path: ");
				t.append(_pathToUtf8(launchPath));
				wxMessageBox(t, _("Error"), wxOK | wxCENTRE | wxICON_ERROR);
				return false;
			}
		}
		else if (initiatedBy == wxLaunchGameEvent::INITIATED_BY::GAME_LIST)
		{
			wxString t = _("Unable to launch title.\nMake sure the configured game paths are still valid and refresh the game list.\n\nPath which failed to load:\n");
			t.append(_pathToUtf8(launchPath));
			wxMessageBox(t, _("Error"), wxOK | wxCENTRE | wxICON_ERROR);
			return false;
		}
		else if (initiatedBy == wxLaunchGameEvent::INITIATED_BY::MENU ||
			initiatedBy == wxLaunchGameEvent::INITIATED_BY::COMMAND_LINE)
		{
			wxString t = _("Unable to launch game\nPath:\n");
			t.append(_pathToUtf8(launchPath));
			if(launchTitle.GetInvalidReason() == TitleInfo::InvalidReason::NO_DISC_KEY)
			{
				t.append("\n\n");
				t.append(_("Could not decrypt title. Make sure that keys.txt contains the correct disc key for this title."));
			}
			if(launchTitle.GetInvalidReason() == TitleInfo::InvalidReason::NO_TITLE_TIK)
			{
				t.append("\n\n");
				t.append(_("Could not decrypt title because title.tik is missing."));
			}
			wxMessageBox(t, _("Error"), wxOK | wxCENTRE | wxICON_ERROR);
			return false;
		}
		else
		{
			wxString t = _("Unable to launch game\nPath:\n");
			t.append(_pathToUtf8(launchPath));
			wxMessageBox(t, _("Error"), wxOK | wxCENTRE | wxICON_ERROR);
			return false;
		}
	}

	if(launchTitle.IsValid())
		GetConfig().AddRecentlyLaunchedFile(_pathToUtf8(launchTitle.GetPath()));
	else
		GetConfig().AddRecentlyLaunchedFile(_pathToUtf8(launchPath));

	wxWindowUpdateLocker lock(this);

    DestroyGameListAndStatusBar();

	m_game_launched = true;
	m_loadMenuItem->Enable(false);
	m_installUpdateMenuItem->Enable(false);
	m_memorySearcherMenuItem->Enable(true);

	m_launched_game_name = CafeSystem::GetForegroundTitleName();
	#ifdef ENABLE_DISCORD_RPC
	if (m_discord)
		m_discord->UpdatePresence(DiscordPresence::Playing, m_launched_game_name);
	#endif

	if (GetConfig().disable_screensaver)
	{
		ScreenSaver::SetInhibit(true);
		// TODO: disable when only the game, not Cemu, is closed (a feature not yet implemented)
		// currently unnecessary because this will happen automatically when Cemu closes
		// ScreenSaver::SetInhibit(false);
	}

	if (ActiveSettings::FullscreenEnabled())
		SetFullScreen(true);

    //GameMode support
#if BOOST_OS_LINUX && defined(ENABLE_FERAL_GAMEMODE)
    if(GetConfig().feral_gamemode)
    {
        // attempt to start gamemode
        if(gamemode_request_start() < 0)
        {
            // GameMode failed to start
            cemuLog_log(LogType::Force, "Could not start GameMode");
        }
        else
        {
            cemuLog_log(LogType::Force, "GameMode has been started.");
        }
    }
#endif

	CreateCanvas();
	CafeSystem::LaunchForegroundTitle();
	RecreateMenu();

	return true;
}

void MainWindow::OnLaunchFromFile(wxLaunchGameEvent& event)
{
	if (event.GetPath().empty())
		return;
	FileLoad(event.GetPath(), event.GetInitiatedBy());
}

void MainWindow::OnFileMenu(wxCommandEvent& event)
{
	const auto menuId = event.GetId();
	if (menuId == MAINFRAME_MENU_ID_FILE_LOAD)
	{
		const auto wildcard = formatWxString(
			"{}|*.wud;*.wux;*.wua;*.iso;*.rpx;*.elf;title.tmd"
			"|{}|*.wud;*.wux;*.iso"
			"|{}|title.tmd"
			"|{}|*.wua"
			"|{}|*.rpx;*.elf"
			"|{}|*",
			_("All Wii U files (*.wud, *.wux, *.wua, *.iso, *.rpx, *.elf)"),
			_("Wii U image (*.wud, *.wux, *.iso, *.wad)"),
			_("Wii U NUS content"),
			_("Wii U archive (*.wua)"),
			_("Wii U executable (*.rpx, *.elf)"),
			_("All files (*.*)")
		);
		
		wxFileDialog openFileDialog(this, _("Open file to launch"), wxEmptyString, wxEmptyString, wildcard, wxFD_OPEN | wxFD_FILE_MUST_EXIST);

		if (openFileDialog.ShowModal() == wxID_CANCEL || openFileDialog.GetPath().IsEmpty())
			return;

		const wxString wxStrFilePath = openFileDialog.GetPath();	
		FileLoad(_utf8ToPath(wxStrFilePath.utf8_string()), wxLaunchGameEvent::INITIATED_BY::MENU);
	}
	else if (menuId >= MAINFRAME_MENU_ID_FILE_RECENT_0 && menuId <= MAINFRAME_MENU_ID_FILE_RECENT_LAST)
	{
		const auto& config = GetConfig();
		const size_t index = menuId - MAINFRAME_MENU_ID_FILE_RECENT_0;
		if (index < config.recent_launch_files.size())
		{
			fs::path path = _utf8ToPath(config.recent_launch_files[index]);
			if (!path.empty())
				FileLoad(path, wxLaunchGameEvent::INITIATED_BY::MENU);
		}
	}
	else if (menuId == MAINFRAME_MENU_ID_FILE_END_EMULATION)
	{
        CafeSystem::ShutdownTitle();
		DestroyCanvas();
		m_game_launched = false;
		RecreateMenu();
        CreateGameListAndStatusBar();
        DoLayout();
	}
}

void MainWindow::OnOpenFolder(wxCommandEvent& event)
{
	if(event.GetId() == MAINFRAME_MENU_ID_FILE_OPEN_CEMU_FOLDER)
		wxLaunchDefaultApplication(wxHelper::FromPath(ActiveSettings::GetUserDataPath()));
	else if(event.GetId() == MAINFRAME_MENU_ID_FILE_OPEN_MLC_FOLDER)
		wxLaunchDefaultApplication(wxHelper::FromPath(ActiveSettings::GetMlcPath()));
}

void MainWindow::OnInstallUpdate(wxCommandEvent& event)
{
	while (true)
	{
		wxDirDialog openDirDialog(nullptr, _("Select folder of title to install"), "", wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST, wxDefaultPosition, wxDefaultSize, _("Select the folder that stores your update, DLC or base game files"));
		int modalChoice = openDirDialog.ShowModal();
		if (modalChoice == wxID_CANCEL || openDirDialog.GetPath().IsEmpty())
			break;
		if (modalChoice == wxID_OK)
		{
			#if BOOST_OS_LINUX || BOOST_OS_MACOS
			fs::path dirPath((const char*)(openDirDialog.GetPath().fn_str()));
			#else
			fs::path dirPath(openDirDialog.GetPath().fn_str());
			#endif

			if ((dirPath.filename() == "code" || dirPath.filename() == "content" || dirPath.filename() == "meta") && dirPath.has_parent_path())
			{
				if (!fs::exists(dirPath.parent_path() / "code") || !fs::exists(dirPath.parent_path() / "content") || !fs::exists(dirPath.parent_path() / "meta"))
				{
					wxMessageBox(formatWxString(_("The (parent) folder of the title you selected is missing at least one of the required subfolders (\"code\", \"content\" and \"meta\")\nMake sure that the files are complete."), dirPath.filename().string()));
					continue;
				}
				else
					dirPath = dirPath.parent_path();
			}

			if (!fs::exists(dirPath))
				wxMessageBox(_("The folder you have selected cannot be found on your system."));
			else if (!fs::exists(dirPath / "meta" / "meta.xml"))
				wxMessageBox(_("Unable to find the /meta/meta.xml file inside the selected folder."));
			else
			{
				InstallUpdate(dirPath);
				return;
			}
		}
	}
}

void MainWindow::OnNFCMenu(wxCommandEvent& event)
{
	if (event.GetId() == MAINFRAME_MENU_ID_NFC_TOUCH_NFC_FILE)
	{
		wxFileDialog
			openFileDialog(this, _("Open file to load"), "", "",
				"All NFC files (bin, dat, nfc)|*.bin;*.dat;*.nfc|All files (*.*)|*", wxFD_OPEN | wxFD_FILE_MUST_EXIST); // TRANSLATE
		if (openFileDialog.ShowModal() == wxID_CANCEL || openFileDialog.GetPath().IsEmpty())
			return;
		wxString wxStrFilePath = openFileDialog.GetPath();
		uint32 nfcError;
		if (nnNfp_touchNfcTagFromFile(_utf8ToPath(wxStrFilePath.utf8_string()), &nfcError) == false)
		{
			if (nfcError == NFC_ERROR_NO_ACCESS)
				wxMessageBox(_("Cannot open file"));
			else if (nfcError == NFC_ERROR_INVALID_FILE_FORMAT)
				wxMessageBox(_("Not a valid NFC NTAG215 file"));
		}
		else
		{
			GetConfig().AddRecentNfcFile(wxStrFilePath.utf8_string());
			UpdateNFCMenu();
		}
	}
	else if (event.GetId() >= MAINFRAME_MENU_ID_NFC_RECENT_0 && event.GetId() <= MAINFRAME_MENU_ID_NFC_RECENT_LAST)
	{
		const size_t index = event.GetId() - MAINFRAME_MENU_ID_NFC_RECENT_0;
		auto& config = GetConfig();
		if (index < config.recent_nfc_files.size())
		{
			const auto& path = config.recent_nfc_files[index];
			if (!path.empty())
			{
				uint32 nfcError = 0;
				if (nnNfp_touchNfcTagFromFile(_utf8ToPath(path), &nfcError) == false)
				{
					if (nfcError == NFC_ERROR_NO_ACCESS)
						wxMessageBox(_("Cannot open file"));
					else if (nfcError == NFC_ERROR_INVALID_FILE_FORMAT)
						wxMessageBox(_("Not a valid NFC NTAG215 file"));
				}
				else
				{
					config.AddRecentNfcFile(path);
					UpdateNFCMenu();
				}
			}
		}
	}
}

void MainWindow::OnFileExit(wxCommandEvent& event)
{
	// todo: Safely clean up everything
	SaveSettings();
	exit(0);
}

void MainWindow::TogglePadView()
{
	const auto& config = GetConfig();
	if (config.pad_open)
	{
		if (m_padView)
			return;
		
		m_padView = new PadViewFrame(this);

		m_padView->Bind(wxEVT_CLOSE_WINDOW, &MainWindow::OnPadClose, this);

		m_padView->Show(true);

#if BOOST_OS_LINUX && HAS_WAYLAND
		if (wxWlIsWaylandWindow(m_padView))
			wxWlSetAppId(m_padView, "info.cemu.Cemu");
#endif

		m_padView->Initialize();
		if (m_game_launched)
			m_padView->InitializeRenderCanvas();
	}
	else if (m_padView)
	{
		m_padView->Destroy();
		m_padView = nullptr;
	}
}

#if BOOST_OS_WINDOWS

#ifndef DBT_DEVNODES_CHANGED
#define DBT_DEVNODES_CHANGED (0x0007)
#endif
WXLRESULT MainWindow::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
	if (nMsg == WM_DEVICECHANGE)
	{
		if (wParam == DBT_DEVNODES_CHANGED)
		{
			InputManager::instance().on_device_changed();
		}
	}

	return wxFrame::MSWWindowProc(nMsg, wParam, lParam);
}
#endif

void MainWindow::OpenSettings()
{
	auto& config = GetConfig();
	const auto language = config.language;

	GeneralSettings2 frame(this, m_game_launched);
	frame.ShowModal();
	const bool paths_modified = frame.ShouldReloadGamelist();
	const bool mlc_modified = frame.MLCModified();

	if (paths_modified)
		m_game_list->ReloadGameEntries(false);
	else
		SaveSettings();

	#ifdef ENABLE_DISCORD_RPC
	if (config.use_discord_presence)
	{
		if (!m_discord)
		{
			m_discord = std::make_unique<DiscordPresence>();
			if (!m_launched_game_name.empty())
				m_discord->UpdatePresence(DiscordPresence::Playing, m_launched_game_name);
		}
	}
	else
		m_discord.reset();
	#endif

	if(config.check_update && !m_game_launched)
		m_update_available = CemuUpdateWindow::IsUpdateAvailableAsync();

	if (mlc_modified)
		RecreateMenu();

	if (!config.fullscreen_menubar && IsFullScreen())
		SetMenuVisible(false);

	if (language != config.language)
		wxMessageBox(_("Cemu must be restarted to apply the selected UI language."), _("Information"), wxOK | wxCENTRE, this); // TODO: change language to newly selected one
}

void MainWindow::OnOptionsInput(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case MAINFRAME_MENU_ID_OPTIONS_FULLSCREEN:
	{
		const bool state = m_fullscreenMenuItem->IsChecked();
		SetFullScreen(state);
		break;
	}
	case MAINFRAME_MENU_ID_OPTIONS_SECOND_WINDOW_PADVIEW:
	{
		GetConfig().pad_open = !GetConfig().pad_open;
		g_config.Save();

		TogglePadView();
		break;
	}
	case MAINFRAME_MENU_ID_OPTIONS_GRAPHIC_PACKS2:
	{
		if (m_graphic_pack_window)
			return;

		uint64 titleId = 0;
		if (CafeSystem::IsTitleRunning())
			titleId = CafeSystem::GetForegroundTitleId();

		m_graphic_pack_window = new GraphicPacksWindow2(this, titleId);
		m_graphic_pack_window->Bind(wxEVT_CLOSE_WINDOW, &MainWindow::OnGraphicWindowClose, this);
		m_graphic_pack_window->Show(true);

		break;
	}

	case MAINFRAME_MENU_ID_OPTIONS_GENERAL2:
	{
		OpenSettings();
		break;
	}
	case MAINFRAME_MENU_ID_OPTIONS_INPUT:
	{
		auto* frame = new InputSettings2(this);
		frame->ShowModal();
		frame->Destroy();
		break;
	}

	}
}

void MainWindow::OnAccountSelect(wxCommandEvent& event)
{
	const int index = event.GetId() - MAINFRAME_MENU_ID_OPTIONS_ACCOUNT_1;
	const auto& accounts = Account::GetAccounts();
	wxASSERT(index >= 0 && index < (int)accounts.size());
	auto& config = GetConfig();
	config.account.m_persistent_id = accounts[index].GetPersistentId();
	// config.account.online_enabled.value = false; // reset online for safety
	g_config.Save();
}

//void MainWindow::OnConsoleRegion(wxCommandEvent& event)
//{
//	switch (event.GetId())
//	{
//	case MAINFRAME_MENU_ID_OPTIONS_REGION_AUTO:
//		GetConfig().console_region = ConsoleRegion::Auto;
//		break;
//	case MAINFRAME_MENU_ID_OPTIONS_REGION_JPN:
//		GetConfig().console_region = ConsoleRegion::JPN;
//		break;
//	case MAINFRAME_MENU_ID_OPTIONS_REGION_USA:
//		GetConfig().console_region = ConsoleRegion::USA;
//		break;
//	case MAINFRAME_MENU_ID_OPTIONS_REGION_EUR:
//		GetConfig().console_region = ConsoleRegion::EUR;
//		break;
//	case MAINFRAME_MENU_ID_OPTIONS_REGION_CHN:
//		GetConfig().console_region = ConsoleRegion::CHN;
//		break;
//	case MAINFRAME_MENU_ID_OPTIONS_REGION_KOR:
//		GetConfig().console_region = ConsoleRegion::KOR;
//		break;
//	case MAINFRAME_MENU_ID_OPTIONS_REGION_TWN:
//		GetConfig().console_region = ConsoleRegion::TWN;
//		break;
//	default:
//		cemu_assert_debug(false);
//	}
//	
//	g_config.Save();
//}

void MainWindow::OnConsoleLanguage(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_JAPANESE:
		GetConfig().console_language = CafeConsoleLanguage::JA;
		break;
	case MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_ENGLISH:
		GetConfig().console_language = CafeConsoleLanguage::EN;
		break;
	case MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_FRENCH:
		GetConfig().console_language = CafeConsoleLanguage::FR;
		break;
	case MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_GERMAN:
		GetConfig().console_language = CafeConsoleLanguage::DE;
		break;
	case MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_ITALIAN:
		GetConfig().console_language = CafeConsoleLanguage::IT;
		break;
	case MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_SPANISH:
		GetConfig().console_language = CafeConsoleLanguage::ES;
		break;
	case MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_CHINESE:
		GetConfig().console_language = CafeConsoleLanguage::ZH;
		break;
	case MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_KOREAN:
		GetConfig().console_language = CafeConsoleLanguage::KO;
		break;
	case MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_DUTCH:
		GetConfig().console_language = CafeConsoleLanguage::NL;
		break;
	case MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_PORTUGUESE:
		GetConfig().console_language = CafeConsoleLanguage::PT;
		break;
	case MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_RUSSIAN:
		GetConfig().console_language = CafeConsoleLanguage::RU;
		break;
	case MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_TAIWANESE:
		GetConfig().console_language = CafeConsoleLanguage::TW;
		break;
	default:
		cemu_assert_debug(false);
	}
	m_game_list->DeleteCachedStrings();
	m_game_list->ReloadGameEntries(false);
	g_config.Save();
}

//void MainWindow::OnCPUMode(wxCommandEvent& event)
//{
//	if (event.GetId() == MAINFRAME_MENU_ID_CPU_MODE_SINGLECORE_INTERPRETER)
//		GetConfig().cpu_mode = CPUMode::SinglecoreInterpreter;
//	else if (event.GetId() == MAINFRAME_MENU_ID_CPU_MODE_SINGLECORE_RECOMPILER)
//		GetConfig().cpu_mode = CPUMode::SinglecoreRecompiler;
//	else if (event.GetId() == MAINFRAME_MENU_ID_CPU_MODE_DUALCORE_RECOMPILER)
//		GetConfig().cpu_mode = CPUMode::DualcoreRecompiler;
//	else if (event.GetId() == MAINFRAME_MENU_ID_CPU_MODE_TRIPLECORE_RECOMPILER)
//		GetConfig().cpu_mode = CPUMode::TriplecoreRecompiler;
//	else
//		cemu_assert_debug(false);
//	
//	g_config.Save();
//}

void MainWindow::OnDebugSetting(wxCommandEvent& event)
{
	if (event.GetId() == MAINFRAME_MENU_ID_DEBUG_RENDER_UPSIDE_DOWN)
		GetConfig().render_upside_down = event.IsChecked();
	else if (event.GetId() == MAINFRAME_MENU_ID_DEBUG_VK_ACCURATE_BARRIERS)
	{
		GetConfig().vk_accurate_barriers = event.IsChecked();
		if(!GetConfig().vk_accurate_barriers)
			wxMessageBox(_("Warning: Disabling the accurate barriers option will lead to flickering graphics but may improve performance. It is highly recommended to leave it turned on."), _("Accurate barriers are off"), wxOK);
	}
	else if (event.GetId() == MAINFRAME_MENU_ID_DEBUG_AUDIO_AUX_ONLY)
		ActiveSettings::EnableAudioOnlyAux(event.IsChecked());
	else if (event.GetId() == MAINFRAME_MENU_ID_DEBUG_DUMP_RAM)
		memory_createDump();
	else if (event.GetId() == MAINFRAME_MENU_ID_DEBUG_DUMP_FST)
	{
		/*	int msgBoxAnswer = wxMessageBox(_("All files from the currently running game will be dumped to /dump/<gamefolder>. This process can take a few minutes."),
				_("Dump WUD"), wxOK | wxCANCEL | wxICON_WARNING);
			if (msgBoxAnswer == wxOK)
			{
				volumeFST_dump(bootGame_getMountedWUD());
				wxMessageBox(_("Dump complete"));
			}*/
	}
	else if (event.GetId() == MAINFRAME_MENU_ID_DEBUG_DUMP_CURL_REQUESTS)
	{
		// toggle debug -> dump -> curl requests
		const bool state = event.IsChecked();
		ActiveSettings::EnableDumpLibcurlRequests(state);
		if (state)
		{
			try
			{
				const fs::path path(ActiveSettings::GetUserDataPath());
				fs::create_directories(path / "dump" / "curl");
			}
			catch (const std::exception& ex)
			{
				SystemException sys(ex);
				cemuLog_log(LogType::Force, "error when creating dump curl folder: {}", sys.what());
				ActiveSettings::EnableDumpLibcurlRequests(false);
			}
		}
	}
	else if (event.GetId() == MAINFRAME_MENU_ID_TIMER_SPEED_8X)
		ActiveSettings::SetTimerShiftFactor(0);
	else if (event.GetId() == MAINFRAME_MENU_ID_TIMER_SPEED_4X)
		ActiveSettings::SetTimerShiftFactor(1);
	else if (event.GetId() == MAINFRAME_MENU_ID_TIMER_SPEED_2X)
		ActiveSettings::SetTimerShiftFactor(2);
	else if (event.GetId() == MAINFRAME_MENU_ID_TIMER_SPEED_1X)
		ActiveSettings::SetTimerShiftFactor(3);
	else if (event.GetId() == MAINFRAME_MENU_ID_TIMER_SPEED_05X)
		ActiveSettings::SetTimerShiftFactor(4);
	else if (event.GetId() == MAINFRAME_MENU_ID_TIMER_SPEED_025X)
		ActiveSettings::SetTimerShiftFactor(5);
	else if (event.GetId() == MAINFRAME_MENU_ID_TIMER_SPEED_0125X)
		ActiveSettings::SetTimerShiftFactor(6);
	else
		cemu_assert_debug(false);
	
	g_config.Save();
}

void MainWindow::OnDebugLoggingToggleFlagGeneric(wxCommandEvent& event)
{
	sint32 loggingIdBase = MAINFRAME_MENU_ID_DEBUG_LOGGING0;

	sint32 id = event.GetId();
	if (id >= loggingIdBase && id < (MAINFRAME_MENU_ID_DEBUG_LOGGING0 + 64))
	{
		bool isEnable = event.IsChecked();
		LogType loggingType = static_cast<LogType>(id - loggingIdBase);
		if (isEnable)
			GetConfig().log_flag = GetConfig().log_flag.GetValue() | cemuLog_getFlag(loggingType);
		else
			GetConfig().log_flag = GetConfig().log_flag.GetValue() & ~cemuLog_getFlag(loggingType);
		cemuLog_setActiveLoggingFlags(GetConfig().log_flag.GetValue());
		g_config.Save();
	}
}

void MainWindow::OnPPCInfoToggle(wxCommandEvent& event)
{
	GetConfig().advanced_ppc_logging = !GetConfig().advanced_ppc_logging.GetValue();
	g_config.Save();
}

void MainWindow::OnDebugDumpUsedTextures(wxCommandEvent& event)
{
	const bool value = event.IsChecked();
	ActiveSettings::EnableDumpTextures(value);
	if (value)
	{
		try
		{
			// create directory
			const fs::path path(ActiveSettings::GetUserDataPath());
			fs::create_directories(path / "dump" / "textures");
		}
		catch (const std::exception& ex)
		{
			SystemException sys(ex);
			cemuLog_log(LogType::Force, "can't create texture dump folder: {}", ex.what());
			ActiveSettings::EnableDumpTextures(false);
		}
	}
}

void MainWindow::OnDebugDumpUsedShaders(wxCommandEvent& event)
{
	const bool value = event.IsChecked();
	ActiveSettings::EnableDumpShaders(value);
	if (value)
	{
		try
		{
			// create directory
			const fs::path path(ActiveSettings::GetUserDataPath());
			fs::create_directories(path / "dump" / "shaders");
		}
		catch (const std::exception & ex)
		{
			SystemException sys(ex);
			cemuLog_log(LogType::Force, "can't create shaders dump folder: {}", ex.what());
			ActiveSettings::EnableDumpShaders(false);
		}
	}
}

void MainWindow::OnLoggingWindow(wxCommandEvent& event)
{
	if(m_logging_window)
		return;

	m_logging_window = new LoggingWindow(this);
	m_logging_window->Bind(wxEVT_CLOSE_WINDOW, 
		[this](wxCloseEvent& event) {
		m_logging_window = nullptr;
		event.Skip();
	});
	m_logging_window->Show(true);
}

void MainWindow::OnGDBStubToggle(wxCommandEvent& event)
{
	if (g_gdbstub)
	{
		g_gdbstub.release();
		return;
	}

	const auto& config = GetConfig();
	g_gdbstub = std::make_unique<GDBServer>(config.gdb_port);
}

void MainWindow::OnDebugViewPPCThreads(wxCommandEvent& event)
{
	auto frame = new DebugPPCThreadsWindow(*this);
	frame->Show(true);
}

void MainWindow::OnDebugViewPPCDebugger(wxCommandEvent& event)
{
	if (m_debugger_window && m_debugger_window->IsShown())
	{
		m_debugger_window->Close();
		m_debugger_window = nullptr;
		return;
	}

	auto rect = GetDesktopRect();
	/*
	sint32 new_width = max(rect.GetWidth() * 0.70, rect.GetWidth() - 850);
	this->SetSize(new_width, 480);*/

	this->SetSize(800, 450 + 50);
	this->CenterOnScreen();

	auto pos = this->GetPosition();
	pos.y = std::min(pos.y + 200, rect.GetHeight() - 400);
	this->SetPosition(pos);

	m_debugger_window = new DebuggerWindow2(*this, rect);
	m_debugger_window->Bind(wxEVT_CLOSE_WINDOW, &MainWindow::OnDebuggerClose, this);
	m_debugger_window->Show(true);
}

void MainWindow::OnDebugViewAudioDebugger(wxCommandEvent& event)
{
	auto frame = new AudioDebuggerWindow(*this);
	frame->Show(true);
}

void MainWindow::OnDebugViewTextureRelations(wxCommandEvent& event)
{
	openTextureViewer(*this);
}

void MainWindow::ShowCursor(bool state)
{
	#if BOOST_OS_WINDOWS
	CURSORINFO info{};
	info.cbSize = sizeof(CURSORINFO);
	GetCursorInfo(&info);
	const bool visible = info.flags == CURSOR_SHOWING;

	if (state == visible)
		return;

	int counter = 0;
	if(state)
	{
		do
		{
			counter = ::ShowCursor(TRUE);
		} while (counter < 0);
	}
	else
	{
		do
		{
			counter = ::ShowCursor(FALSE);
		} while (counter >= 0);
	}
	#else
	if (state)
	{
		wxSetCursor(wxNullCursor); // restore system default cursor
	}
	else
	{
		wxSetCursor(wxCursor(wxCURSOR_BLANK));
	}
	#endif
}

uintptr_t MainWindow::GetRenderCanvasHWND()
{
	// deprecated. We can use the global cross-platform window info structs now
	#if BOOST_OS_WINDOWS
	if (!m_render_canvas)
		return 0;
	return (uintptr_t)m_render_canvas->GetHWND();
	#else
	return 0;
	#endif
}

wxRect MainWindow::GetDesktopRect()
{
	const auto pos = GetPosition();
	const auto middle = pos.x + GetSize().GetWidth() / 2;

	const auto displayCount = wxDisplay::GetCount();
	for (uint32 i = 0; i < displayCount; ++i)
	{
		wxDisplay display(i);
		if (!display.IsOk())
			continue;

		const auto geo = display.GetGeometry();
		if (geo.x <= middle && middle <= geo.x + geo.width)
			return geo;
	}
	return { 0,0,800,600 };
}

void MainWindow::LoadSettings()
{
	g_config.Load();
	const auto& config = GetConfig();

	if(config.check_update)
		m_update_available = CemuUpdateWindow::IsUpdateAvailableAsync();

	if (config.window_position != Vector2i{ -1,-1 })
		this->SetPosition({ config.window_position.x, config.window_position.y });

	if (config.window_size != Vector2i{ -1,-1 })
	{
		this->SetSize({ config.window_size.x, config.window_size.y });

		if (config.window_maximized)
			this->Maximize();
	}

	if (config.pad_position != Vector2i{ -1,-1 })
	{
		g_window_info.restored_pad_x = config.pad_position.x;
		g_window_info.restored_pad_y = config.pad_position.y;
	}

	if (config.pad_size != Vector2i{ -1,-1 })
	{
		g_window_info.restored_pad_width = config.pad_size.x;
		g_window_info.restored_pad_height = config.pad_size.y;

		g_window_info.pad_maximized = config.pad_maximized;
	}

	this->TogglePadView();

	if(m_game_list)
		m_game_list->LoadConfig();
}

void MainWindow::SaveSettings()
{
	auto lock = g_config.Lock();
	auto& config = GetConfig();
	
	if (config.window_position != Vector2i{ -1,-1 })
	{
		config.window_position.x = m_restored_position.x;
		config.window_position.y = m_restored_position.y;
	}
	if (config.window_size != Vector2i{ -1,-1 })
	{
		config.window_size.x = m_restored_size.x;
		config.window_size.y = m_restored_size.y;
		config.window_maximized = IsMaximized();
	}
	else
	{
		config.window_maximized = false;
	}

	config.pad_open = m_padView != nullptr;

	if (config.pad_position != Vector2i{ -1,-1 } && g_window_info.restored_pad_x != -1)
	{
		config.pad_position.x = g_window_info.restored_pad_x;
		config.pad_position.y = g_window_info.restored_pad_y;
	}
	if (config.pad_size != Vector2i{ -1,-1 } && g_window_info.restored_pad_width != -1)
	{
		config.pad_size.x = g_window_info.restored_pad_width;
		config.pad_size.y = g_window_info.restored_pad_height;
		config.pad_maximized = g_window_info.pad_maximized;
	}
	else
	{
		config.pad_maximized = false;
	}

	if(m_game_list)
		m_game_list->SaveConfig();
	
	g_config.Save();
}

void MainWindow::OnMouseMove(wxMouseEvent& event)
{
	event.Skip();

	m_last_mouse_move_time = std::chrono::steady_clock::now();
	m_mouse_position = wxGetMousePosition();
	ShowCursor(true);

	auto& instance = InputManager::instance();
	std::unique_lock lock(instance.m_main_mouse.m_mutex);
	auto physPos = ToPhys(event.GetPosition());
	instance.m_main_mouse.position = { physPos.x, physPos.y };
	lock.unlock();

	if (!IsFullScreen())
		return;

	const auto& config = GetConfig();
	// if mouse goes to upper screen then show our menu in fullscreen mode
	if (config.fullscreen_menubar)
		SetMenuVisible(event.GetPosition().y < 50);
}

void MainWindow::OnMouseLeft(wxMouseEvent& event)
{
	auto& instance = InputManager::instance();
	
	std::scoped_lock lock(instance.m_main_mouse.m_mutex);
	instance.m_main_mouse.left_down = event.ButtonDown(wxMOUSE_BTN_LEFT);
	auto physPos = ToPhys(event.GetPosition());
	instance.m_main_mouse.position = { physPos.x, physPos.y };
	if (event.ButtonDown(wxMOUSE_BTN_LEFT))
		instance.m_main_mouse.left_down_toggle = true;
	
	event.Skip();
}

void MainWindow::OnMouseRight(wxMouseEvent& event)
{
	auto& instance = InputManager::instance();

	std::scoped_lock lock(instance.m_main_mouse.m_mutex);
	instance.m_main_mouse.right_down = event.ButtonDown(wxMOUSE_BTN_RIGHT);
	auto physPos = ToPhys(event.GetPosition());
	instance.m_main_mouse.position = { physPos.x, physPos.y };
	if(event.ButtonDown(wxMOUSE_BTN_RIGHT))
		instance.m_main_mouse.right_down_toggle = true;
	
	event.Skip();
}

void MainWindow::OnGameListBeginUpdate(wxCommandEvent& event)
{
	if (m_game_list->IsShown())
		m_info_bar->ShowMessage(_("Updating game list..."));
}

void MainWindow::OnGameListEndUpdate(wxCommandEvent& event)
{
	m_info_bar->Dismiss();
}

void MainWindow::OnAccountListRefresh(wxCommandEvent& event)
{
	RecreateMenu();
}

void MainWindow::OnRequestGameListRefresh(wxCommandEvent& event)
{
	m_game_list->ReloadGameEntries(false);
}

void MainWindow::OnSetWindowTitle(wxCommandEvent& event)
{
	this->SetTitle(event.GetString());
}

void MainWindow::OnKeyUp(wxKeyEvent& event)
{
	event.Skip();

	if (swkbd_hasKeyboardInputHook())
		return;

	const auto code = event.GetKeyCode();
	if (code == WXK_ESCAPE)
		SetFullScreen(false);
	else if (code == WXK_RETURN && event.AltDown() || code == WXK_F11)
		SetFullScreen(!IsFullScreen());
	else if (code == WXK_F12)
		g_window_info.has_screenshot_request = true; // async screenshot request
}

void MainWindow::OnChar(wxKeyEvent& event)
{
	if (swkbd_hasKeyboardInputHook())
		swkbd_keyInput(event.GetUnicodeKey());
	
	// event.Skip();
}

void MainWindow::OnToolsInput(wxCommandEvent& event)
{
	const auto id = event.GetId();
	switch (id)
	{
	case MAINFRAME_MENU_ID_TOOLS_MEMORY_SEARCHER:
	{
		if (m_toolWindow)
			m_toolWindow->SetFocus();
		else
		{
			m_toolWindow = new MemorySearcherTool(this);
			m_toolWindow->Bind(wxEVT_CLOSE_WINDOW, &MainWindow::OnMemorySearcherClose, this);
			m_toolWindow->Show(true);
		}
		break;
	}
	case MAINFRAME_MENU_ID_TOOLS_TITLE_MANAGER:
	case MAINFRAME_MENU_ID_TOOLS_DOWNLOAD_MANAGER:
	{
		const auto default_tab = id == MAINFRAME_MENU_ID_TOOLS_TITLE_MANAGER ? TitleManagerPage::TitleManager : TitleManagerPage::DownloadManager;
			
		if (m_title_manager)
			m_title_manager->SetFocusAndTab(default_tab);
		else
		{
			m_title_manager = new TitleManager(this, default_tab);
			m_title_manager->Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& event)
				{
					m_title_manager = nullptr;
					event.Skip();
				});
			m_title_manager->Show();
		}
	}
	break;
	}
}

void MainWindow::OnGesturePan(wxPanGestureEvent& event)
{
	auto& instance = InputManager::instance();
	std::scoped_lock lock(instance.m_main_touch.m_mutex);
	auto physPos = ToPhys(event.GetPosition());
	instance.m_main_touch.position = { physPos.x, physPos.y };
	instance.m_main_touch.left_down = event.IsGestureStart() || !event.IsGestureEnd();
	if (event.IsGestureStart() || !event.IsGestureEnd())
		instance.m_main_touch.left_down_toggle = true;
	

	event.Skip();
}

void MainWindow::OnGameLoaded()
{
	if (m_debugger_window)
		m_debugger_window->OnGameLoaded();
}

void MainWindow::AsyncSetTitle(std::string_view windowTitle)
{
	wxCommandEvent set_title_event(wxEVT_SET_WINDOW_TITLE);
	set_title_event.SetString(wxHelper::FromUtf8(windowTitle));
	g_mainFrame->QueueEvent(set_title_event.Clone());
}

void MainWindow::CreateCanvas()
{
    // create panel for canvas
    m_game_panel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxNO_BORDER | wxWANTS_CHARS);
    auto* sizer = new wxBoxSizer(wxVERTICAL);

    // shouldn't be needed, but who knows
    m_game_panel->Bind(wxEVT_KEY_UP, &MainWindow::OnKeyUp, this);
    m_game_panel->Bind(wxEVT_CHAR, &MainWindow::OnChar, this);

    m_game_panel->SetSizer(sizer);
    this->GetSizer()->Add(m_game_panel, 1, wxEXPAND, 0, nullptr);

    // create canvas
    if (ActiveSettings::GetGraphicsAPI() == kVulkan)
		m_render_canvas = new VulkanCanvas(m_game_panel, wxSize(1280, 720), true);
	else
		m_render_canvas = GLCanvas_Create(m_game_panel, wxSize(1280, 720), true);

	// mouse events
	m_render_canvas->Bind(wxEVT_MOTION, &MainWindow::OnMouseMove, this);
	m_render_canvas->Bind(wxEVT_MOUSEWHEEL, &MainWindow::OnMouseWheel, this);
	m_render_canvas->Bind(wxEVT_LEFT_DOWN, &MainWindow::OnMouseLeft, this);
	m_render_canvas->Bind(wxEVT_LEFT_UP, &MainWindow::OnMouseLeft, this);
	m_render_canvas->Bind(wxEVT_RIGHT_DOWN, &MainWindow::OnMouseRight, this);
	m_render_canvas->Bind(wxEVT_RIGHT_UP, &MainWindow::OnMouseRight, this);

	m_render_canvas->Bind(wxEVT_GESTURE_PAN, &MainWindow::OnGesturePan, this);

	// key events
	m_render_canvas->Bind(wxEVT_KEY_UP, &MainWindow::OnKeyUp, this);
	m_render_canvas->Bind(wxEVT_CHAR, &MainWindow::OnChar, this);

	m_render_canvas->SetDropTarget(new wxAmiiboDropTarget(this));
	m_game_panel->GetSizer()->Add(m_render_canvas, 1, wxEXPAND, 0, nullptr);

	GetSizer()->Layout();
	m_render_canvas->SetFocus();

	if (m_padView)
		m_padView->InitializeRenderCanvas();
}

void MainWindow::DestroyCanvas()
{
	if (m_padView)
	{
		m_padView->DestroyCanvas();
	}
	if (m_render_canvas)
	{
		m_render_canvas->Destroy();
		m_render_canvas = nullptr;
	}
    if(m_game_panel)
    {
        m_game_panel->Destroy();
        m_game_panel = nullptr;
    }
}

void MainWindow::OnSizeEvent(wxSizeEvent& event)
{
	if (!IsMaximized() && !gui_isFullScreen())
		m_restored_size = GetSize();

	const wxSize client_size = GetClientSize();
	g_window_info.width = client_size.GetWidth();
	g_window_info.height = client_size.GetHeight();
	g_window_info.phys_width = ToPhys(client_size.GetWidth());
	g_window_info.phys_height = ToPhys(client_size.GetHeight());
	g_window_info.dpi_scale = GetDPIScaleFactor();

	if (m_debugger_window && m_debugger_window->IsShown())
		m_debugger_window->OnParentMove(GetPosition(), event.GetSize());

	event.Skip();

	VsyncDriver_notifyWindowPosChanged();
}

void MainWindow::OnDPIChangedEvent(wxDPIChangedEvent& event)
{
	event.Skip();
	const wxSize client_size = GetClientSize();
	g_window_info.width = client_size.GetWidth();
	g_window_info.height = client_size.GetHeight();
	g_window_info.phys_width = ToPhys(client_size.GetWidth());
	g_window_info.phys_height = ToPhys(client_size.GetHeight());
	g_window_info.dpi_scale = GetDPIScaleFactor();
}

void MainWindow::OnMove(wxMoveEvent& event)
{
	if (!IsMaximized() && !gui_isFullScreen())
		m_restored_position = GetPosition();

	if (m_debugger_window && m_debugger_window->IsShown())
		m_debugger_window->OnParentMove(GetPosition(), GetSize());
	VsyncDriver_notifyWindowPosChanged();
}

void MainWindow::OnDebuggerClose(wxCloseEvent& event)
{
	m_debugger_window = nullptr;
	event.Skip();
}

void MainWindow::OnPadClose(wxCloseEvent& event)
{
	auto& config = GetConfig();
	config.pad_open = false;
	if (config.pad_position != Vector2i{ -1,-1 })
		m_padView->GetPosition(&config.pad_position.x, &config.pad_position.y);

	if (config.pad_size != Vector2i{ -1,-1 })
		m_padView->GetSize(&config.pad_size.x, &config.pad_size.y);

	g_config.Save();

	// already deleted by wxwidget
	m_padView = nullptr;

	if (m_padViewMenuItem)
		m_padViewMenuItem->Check(false);

	event.Skip();
}

void MainWindow::OnMemorySearcherClose(wxCloseEvent& event)
{
	m_toolWindow = nullptr;
	event.Skip();
}

void MainWindow::OnMouseWheel(wxMouseEvent& event)
{
	const float delta = event.GetWheelRotation(); // in 120 steps -> max reached ~480 (?)
	auto& instance = InputManager::instance();
	instance.m_mouse_wheel = (delta / 120.0f);

	event.Skip();
}

void MainWindow::SetFullScreen(bool state)
{
	// only update config entry if we dont't have launch parameters
	if (!LaunchSettings::FullscreenEnabled().has_value())
	{
		GetConfig().fullscreen = state;
		g_config.Save();
	}
	if (state && !m_game_launched)
		return;
	g_window_info.is_fullscreen = state;
	m_fullscreenMenuItem->Check(state);

	this->ShowFullScreen(state);

	if (state)
		m_menu_visible = false; // menu gets always disabled by wxFULLSCREEN_NOMENUBAR
	else
		SetMenuVisible(true);
}

void MainWindow::SetMenuVisible(bool state)
{
	if (m_menu_visible == state)
		return;

	SetMenuBar(state ? m_menuBar : nullptr);
	m_menu_visible = state;
}

void MainWindow::UpdateNFCMenu()
{
	if (m_nfcMenuSeparator0)
	{
		m_nfcMenu->Remove(m_nfcMenuSeparator0->GetId());
		m_nfcMenuSeparator0 = nullptr;
	}
	// remove recent files list
	for (sint32 i = 0; i < CemuConfig::kMaxRecentEntries; i++)
	{
		if (m_nfcMenu->FindChildItem(MAINFRAME_MENU_ID_NFC_RECENT_0 + i) == nullptr)
			continue;
		m_nfcMenu->Remove(MAINFRAME_MENU_ID_NFC_RECENT_0 + i);
	}
	// add entries
	const auto& config = GetConfig();
	sint32 recentFileIndex = 0;
	for (size_t i = 0; i < config.recent_nfc_files.size(); i++)
	{
		const auto& entry = config.recent_nfc_files[i];
		if (entry.empty())
			continue;
		
		if (!fs::exists(_utf8ToPath(entry)))
			continue;
		
		if (recentFileIndex == 0)
			m_nfcMenuSeparator0 = m_nfcMenu->AppendSeparator();

		m_nfcMenu->Append(MAINFRAME_MENU_ID_NFC_RECENT_0 + i, to_wxString(fmt::format("{}. {}", recentFileIndex, entry)));

		recentFileIndex++;
		if (recentFileIndex >= 12)
			break;
	}
}

bool MainWindow::IsMenuHidden() const
{
	return m_menu_visible;
}

void MainWindow::OnTimer(wxTimerEvent& event)
{
	if(m_update_available.valid() && future_is_ready(m_update_available))
	{
		if(m_update_available.get())
		{
			wxMessageDialog dialog(this, _("There's a new update available.\nDo you want to update?"), _("Update notification"), wxCENTRE | wxYES_NO);
			if(dialog.ShowModal() == wxID_YES)
			{
				CemuUpdateWindow update_window(this);
				update_window.ShowModal();
			}
		}

		m_update_available = {};
	}

	if (!IsFullScreen() || m_menu_visible)
		return;

	const auto mouse_position = wxGetMousePosition();
	if(m_mouse_position != mouse_position)
	{
		m_last_mouse_move_time = std::chrono::steady_clock::now();
		m_mouse_position = mouse_position;
		ShowCursor(true);
		return;
	}

	auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_last_mouse_move_time);
	if (diff.count() > 3000)
	{
		ShowCursor(false);
	}
		
}

#define BUILD_DATE __DATE__ " " __TIME__

class CemuAboutDialog : public wxDialog
{
public:
	CemuAboutDialog(wxWindow* parent = NULL)
		: wxDialog(NULL, wxID_ANY, _("About Cemu"), wxDefaultPosition, wxSize(500, 700))
	{
		Create(parent);
	}

	void Create(wxWindow* parent = NULL)
	{
		SetIcon(wxICON(M_WND_ICON128));

		wxScrolledWindow* scrolledWindow = new wxScrolledWindow(this);

		wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

		m_scrolledSizer = new wxBoxSizer(wxVERTICAL);

		AddHeaderInfo(scrolledWindow, m_scrolledSizer);
		m_scrolledSizer->AddSpacer(5);
		AddLibInfo(scrolledWindow, m_scrolledSizer);
		m_scrolledSizer->AddSpacer(5);
		AddThanks(scrolledWindow, m_scrolledSizer);

		scrolledWindow->SetSizer(m_scrolledSizer);
		scrolledWindow->FitInside();
		scrolledWindow->SetScrollRate(25, 25);
		mainSizer->Add(scrolledWindow, wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT, 10));

		SetSizer(mainSizer);
		CentreOnParent();
	}

	void AddHeaderInfo(wxWindow* parent, wxSizer* sizer)
	{
		auto versionString = formatWxString(_("Cemu\nVersion {0}\nCompiled on {1}\nOriginal authors: {2}"), BUILD_VERSION_STRING, BUILD_DATE, "Exzap, Petergov");

		sizer->Add(new wxStaticText(parent, wxID_ANY, versionString), wxSizerFlags().Border(wxALL, 3).Border(wxTOP, 10));
		sizer->Add(new wxHyperlinkCtrl(parent, -1, "https://cemu.info", "https://cemu.info"), wxSizerFlags().Expand().Border(wxTOP | wxBOTTOM, 3));

		sizer->AddSpacer(3);
		sizer->Add(new wxStaticLine(parent), wxSizerFlags().Expand().Border(wxRIGHT, 4));
		sizer->AddSpacer(5);

		wxString extraInfo(_("Cemu is a Wii U emulator.\n\nWii and Wii U are trademarks of Nintendo.\nCemu is not affiliated with Nintendo."));
		sizer->Add(new wxStaticText(parent, wxID_ANY, extraInfo), wxSizerFlags());
	}

	void AddLibInfo(wxWindow* parent, wxSizer* sizer)
	{
		sizer->AddSpacer(3);
		sizer->Add(new wxStaticLine(parent), wxSizerFlags().Expand().Border(wxRIGHT, 4));
		sizer->AddSpacer(3);

		sizer->Add(new wxStaticText(parent, wxID_ANY, _("Used libraries and utilities:")), wxSizerFlags().Expand().Border(wxTOP | wxBOTTOM, 2));
		// zLib
		{
			wxSizer* lineSizer = new wxBoxSizer(wxHORIZONTAL);
			lineSizer->Add(new wxStaticText(parent, -1, "zLib ("), 0);
			lineSizer->Add(new wxHyperlinkCtrl(parent, -1, "https://www.zlib.net", "https://www.zlib.net"), 0);
			lineSizer->Add(new wxStaticText(parent, -1, ")"), 0);
			sizer->Add(lineSizer);
		}
		// wxWidgets
		{
			wxSizer* lineSizer = new wxBoxSizer(wxHORIZONTAL);
			lineSizer->Add(new wxStaticText(parent, -1, "wxWidgets ("), 0);
			lineSizer->Add(new wxHyperlinkCtrl(parent, -1, "https://www.wxwidgets.org/", "https://www.wxwidgets.org/"), 0);
			lineSizer->Add(new wxStaticText(parent, -1, ")"), 0);
			sizer->Add(lineSizer);
		}
		// OpenSSL
		{
			wxSizer* lineSizer = new wxBoxSizer(wxHORIZONTAL);
			lineSizer->Add(new wxStaticText(parent, -1, "OpenSSL ("), 0);
			lineSizer->Add(new wxHyperlinkCtrl(parent, -1, "https://www.openssl.org/", "https://www.openssl.org/"), 0);
			lineSizer->Add(new wxStaticText(parent, -1, ")"), 0);
			sizer->Add(lineSizer);
		}
		// libcurl
		{
			wxSizer* lineSizer = new wxBoxSizer(wxHORIZONTAL);
			lineSizer->Add(new wxStaticText(parent, -1, "libcurl ("), 0);
			lineSizer->Add(new wxHyperlinkCtrl(parent, -1, "https://curl.haxx.se/libcurl/", "https://curl.haxx.se/libcurl/"), 0);
			lineSizer->Add(new wxStaticText(parent, -1, ")"), 0);
			sizer->Add(lineSizer);
		}
		// imgui
		{
			wxSizer* lineSizer = new wxBoxSizer(wxHORIZONTAL);
			lineSizer->Add(new wxStaticText(parent, -1, "imgui ("), 0);
			lineSizer->Add(new wxHyperlinkCtrl(parent, -1, "https://github.com/ocornut/imgui", "https://github.com/ocornut/imgui"), 0);
			lineSizer->Add(new wxStaticText(parent, -1, ")"), 0);
			sizer->Add(lineSizer);
		}
		// fontawesome
		{
			wxSizer* lineSizer = new wxBoxSizer(wxHORIZONTAL);
			lineSizer->Add(new wxStaticText(parent, -1, "fontawesome ("), 0);
			lineSizer->Add(new wxHyperlinkCtrl(parent, -1, "https://github.com/FortAwesome/Font-Awesome", "https://github.com/FortAwesome/Font-Awesome"), 0);
			lineSizer->Add(new wxStaticText(parent, -1, ")"), 0);
			sizer->Add(lineSizer);
		}
		// boost
		{
			wxSizer* lineSizer = new wxBoxSizer(wxHORIZONTAL);
			lineSizer->Add(new wxStaticText(parent, -1, "boost ("), 0);
			lineSizer->Add(new wxHyperlinkCtrl(parent, -1, "https://www.boost.org", "https://www.boost.org"), 0);
			lineSizer->Add(new wxStaticText(parent, -1, ")"), 0);
			sizer->Add(lineSizer);
		}
		// libusb
		{
			wxSizer* lineSizer = new wxBoxSizer(wxHORIZONTAL);
			lineSizer->Add(new wxStaticText(parent, -1, "libusb ("), 0);
			lineSizer->Add(new wxHyperlinkCtrl(parent, -1, "https://libusb.info", "https://libusb.info"), 0);
			lineSizer->Add(new wxStaticText(parent, -1, ")"), 0);
			sizer->Add(lineSizer);
		}
		// icons
		{
			wxSizer* lineSizer = new wxBoxSizer(wxHORIZONTAL);
			lineSizer->Add(new wxStaticText(parent, -1, "icons from "), 0);
			lineSizer->Add(new wxHyperlinkCtrl(parent, -1, "https://icons8.com", "https://icons8.com"), 0);
			sizer->Add(lineSizer);
		}
		// Lato font (are we still using it?)
		{
			wxSizer* lineSizer = new wxBoxSizer(wxHORIZONTAL);
			lineSizer->Add(new wxStaticText(parent, -1, "\"Lato\" font by tyPoland Lukasz Dziedzic (OFL, V1.1)"), 0);
			sizer->Add(lineSizer);
		}
		// SDL
		{
			wxSizer* lineSizer = new wxBoxSizer(wxHORIZONTAL);
			lineSizer->Add(new wxStaticText(parent, -1, "SDL ("), 0);
			lineSizer->Add(new wxHyperlinkCtrl(parent, -1, "https://github.com/libsdl-org/SDL", "https://github.com/libsdl-org/SDL"), 0);
			lineSizer->Add(new wxStaticText(parent, -1, ")"), 0);
			sizer->Add(lineSizer);
		}
		// IH264
		{
			wxSizer* lineSizer = new wxBoxSizer(wxHORIZONTAL);
			lineSizer->Add(new wxStaticText(parent, -1, "Modified ih264 from Android project ("), 0);
			lineSizer->Add(new wxHyperlinkCtrl(parent, -1, "Source", "https://cemu.info/oss/ih264d.zip"), 0);
			lineSizer->Add(new wxStaticText(parent, -1, "  "), 0);
			wxHyperlinkCtrl* noticeLink = new wxHyperlinkCtrl(parent, -1, "NOTICE", "");
			noticeLink->Bind(wxEVT_LEFT_DOWN, [](wxMouseEvent& event)
				{
					fs::path tempPath = fs::temp_directory_path();
					tempPath.append("NOTICE_IH264.txt");
					FileStream* fs = FileStream::createFile2(tempPath);
					if (!fs)
						return;
					fs->writeString(
						"/******************************************************************************\r\n"
						" *\r\n"
						" * Copyright (C) 2015 The Android Open Source Project\r\n"
						" *\r\n"
						" * Licensed under the Apache License, Version 2.0 (the \"License\");\r\n"
						" * you may not use this file except in compliance with the License.\r\n"
						" * You may obtain a copy of the License at:"
						" *\r\n"
						" * http://www.apache.org/licenses/LICENSE-2.0\r\n"
						" *\r\n"
						" * Unless required by applicable law or agreed to in writing, software\r\n"
						" * distributed under the License is distributed on an \"AS IS\" BASIS,\r\n"
						" * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\r\n"
						" * See the License for the specific language governing permissions and\r\n"
						" * limitations under the License.\r\n"
						" *\r\n"
						" *****************************************************************************\r\n"
						" * Originally developed and contributed by Ittiam Systems Pvt. Ltd, Bangalore\r\n"
						"*/\r\n"
						"/*****************************************************************************/\r\n"
					);
					delete fs;
					wxLaunchDefaultBrowser(wxHelper::FromUtf8(fmt::format("file:{}", _pathToUtf8(tempPath))));
				});
			lineSizer->Add(noticeLink, 0);
			lineSizer->Add(new wxStaticText(parent, -1, ")"), 0);
			sizer->Add(lineSizer);
		}
	}

	void AddThanks(wxWindow* parent, wxSizer* sizer)
	{
		sizer->AddSpacer(3);
		sizer->Add(new wxStaticLine(parent), wxSizerFlags().Expand().Border(wxRIGHT, 4));
		sizer->AddSpacer(3);

		wxGridSizer* gridSizer = new wxGridSizer(1, 2, 0, 0);

		sizer->AddSpacer(2);

		sizer->Add(new wxStaticText(parent, wxID_ANY, _("Thanks to our Patreon supporters:")), wxSizerFlags().Expand().Border(wxTOP | wxBOTTOM, 2));

		std::vector<const char*> patreonSupporterNames{ "Maufeat", "lvlv", "F34R", "John Godgames", "Jameel Lewis", "skooks", "Cheesy", "Barrowsx", "Mored1984", "madmat007"
			, "Kuhnnl", "Owen M", "lucianobugalu", "KimoMaka", "nick palma aka renaissance18", "TheGiantBros", "SpiGAndromeda"
			, "Chimech0", "Nicolás Pino", "Pezzatti", "Barry Wallace", "REGNR8 Productions", "Lagia", "Freestyler316", "Dentora"
			, "tactics", "Merola.C", "Ceigyx", "Mata", "BobSchneeder45", "fenixDG", "jjalapeno55", "FissionMetroid101", "Jetta88"
			, "nesxdie", "Mikah", "PornfoxVR.com", "Hunter4everosa", "Bbzx", "Salim Sanehi", "FalloutpunkX", "NashOH-CL", "RaheemWala"
			, "Faris Leonhart", "MahvZero", "PlaguedGuardian", "Stuffie", "CaptainLester", "Qtech", "Zaurexus", "Leonidas", "Artifesto"
			, "Alca259", "SirWestofAsh", "Loli Co.", "The Technical Revolutionary", "MegaYama", "mitori", "Seymordius", "Adrian Josh Cruz", "Manuel Hoenings", "Just A Jabb"
			, "pgantonio", "CannonXIII", "Lonewolf00708", "AlexsDesign.com", "NoskLo", "MrSirHaku", "xElite_V AKA William H. Johnson", "Zalnor", "Pig", "James \"SE4LS\"", "DairyOrange", "Horoko Lawrence", "bloodmc", "Officer Jenny", "Quasar", "Postposterous", "Jake Jackson", "Kaydax", "CthePredatorG"
			, "Hengi", "Pyrochaser", "luma.x3"};

		wxString nameListLeft, nameListRight;
		for (size_t i = 0; i < patreonSupporterNames.size(); i++)
		{
			const char* name = patreonSupporterNames[i];
			wxString& nameList = ((i % 2) == 0) ? nameListLeft : nameListRight;
			if (i >= 2)
				nameList.append("\n");
			nameList.append(name);
		}

		gridSizer->Add(new wxStaticText(parent, wxID_ANY, nameListLeft), wxSizerFlags());
		gridSizer->Add(new wxStaticText(parent, wxID_ANY, nameListRight), wxSizerFlags());

		sizer->AddSpacer(4);

		sizer->Add(gridSizer, 1, wxEXPAND);

		sizer->AddSpacer(2);
		sizer->Add(new wxStaticText(parent, wxID_ANY, _("Special thanks:")), wxSizerFlags().Expand().Border(wxTOP, 2));
		sizer->Add(new wxStaticText(parent, wxID_ANY, "espes - Also try XQEMU!\nWaltzz92"), wxSizerFlags().Expand().Border(wxTOP, 1));
	}

protected:
	wxSizer* m_scrolledSizer;
};

void MainWindow::OnHelpAbout(wxCommandEvent& event)
{
	CemuAboutDialog dlgAbout(this);
	dlgAbout.ShowModal();
}

void MainWindow::OnHelpUpdate(wxCommandEvent& event)
{
	CemuUpdateWindow test(this);
	test.ShowModal();
}

void MainWindow::OnHelpGettingStarted(wxCommandEvent& event)
{
	ShowGettingStartedDialog();
}

void MainWindow::RecreateMenu()
{
	if (m_menuBar)
	{
		SetMenuBar(nullptr);
		m_menuBar->Destroy();
		m_menuBar = nullptr;
	}
	
	auto& config = GetConfig();
	
	m_menuBar = new wxMenuBar();
	// file submenu
	m_fileMenu = new wxMenu();

	if (!m_game_launched)
	{
		m_loadMenuItem = m_fileMenu->Append(MAINFRAME_MENU_ID_FILE_LOAD, _("&Load..."));
		m_installUpdateMenuItem = m_fileMenu->Append(MAINFRAME_MENU_ID_FILE_INSTALL_UPDATE, _("&Install game title, update or DLC..."));

		sint32 recentFileIndex = 0;
		m_fileMenuSeparator0 = nullptr;
		m_fileMenuSeparator1 = nullptr;
		for (size_t i = 0; i < config.recent_launch_files.size(); i++)
		{
			const std::string& pathStr = config.recent_launch_files[i];
			if (pathStr.empty())
				continue;
			if (recentFileIndex == 0)
				m_fileMenuSeparator0 = m_fileMenu->AppendSeparator();
			m_fileMenu->Append(MAINFRAME_MENU_ID_FILE_RECENT_0 + i, to_wxString(fmt::format("{}. {}", recentFileIndex, pathStr)));
			recentFileIndex++;

			if (recentFileIndex >= 8)
				break;
		}
		m_fileMenuSeparator1 = m_fileMenu->AppendSeparator();
	}
	else
	{
		// add 'Stop emulation' menu entry to file menu
#ifdef CEMU_DEBUG_ASSERT
		m_fileMenu->Append(MAINFRAME_MENU_ID_FILE_END_EMULATION, _("Stop emulation"));
#endif
	}

	m_fileMenu->Append(MAINFRAME_MENU_ID_FILE_OPEN_CEMU_FOLDER, _("&Open Cemu folder"));
	m_fileMenu->Append(MAINFRAME_MENU_ID_FILE_OPEN_MLC_FOLDER, _("&Open MLC folder"));
	m_fileMenu->AppendSeparator();

	m_exitMenuItem = m_fileMenu->Append(MAINFRAME_MENU_ID_FILE_EXIT, _("&Exit"));
	m_menuBar->Append(m_fileMenu, _("&File"));
	// options->account submenu
	m_optionsAccountMenu = new wxMenu();
	const auto account_id = ActiveSettings::GetPersistentId();
	int index = 0;
	for(const auto& account : Account::GetAccounts())
	{
		wxMenuItem* item = m_optionsAccountMenu->AppendRadioItem(MAINFRAME_MENU_ID_OPTIONS_ACCOUNT_1 + index, account.ToString());
		item->Check(account_id == account.GetPersistentId());
		if (m_game_launched || LaunchSettings::GetPersistentId().has_value())
			item->Enable(false);
		
		++index;
	}

	// options->console language submenu
	wxMenu* optionsConsoleLanguageMenu = new wxMenu();
	optionsConsoleLanguageMenu->AppendRadioItem(MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_ENGLISH, _("&English"), wxEmptyString)->Check(config.console_language == CafeConsoleLanguage::EN);
	optionsConsoleLanguageMenu->AppendRadioItem(MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_JAPANESE, _("&Japanese"), wxEmptyString)->Check(config.console_language == CafeConsoleLanguage::JA);
	optionsConsoleLanguageMenu->AppendRadioItem(MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_FRENCH, _("&French"), wxEmptyString)->Check(config.console_language == CafeConsoleLanguage::FR);
	optionsConsoleLanguageMenu->AppendRadioItem(MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_GERMAN, _("&German"), wxEmptyString)->Check(config.console_language == CafeConsoleLanguage::DE);
	optionsConsoleLanguageMenu->AppendRadioItem(MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_ITALIAN, _("&Italian"), wxEmptyString)->Check(config.console_language == CafeConsoleLanguage::IT);
	optionsConsoleLanguageMenu->AppendRadioItem(MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_SPANISH, _("&Spanish"), wxEmptyString)->Check(config.console_language == CafeConsoleLanguage::ES);
	optionsConsoleLanguageMenu->AppendRadioItem(MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_CHINESE, _("&Chinese"), wxEmptyString)->Check(config.console_language == CafeConsoleLanguage::ZH);
	optionsConsoleLanguageMenu->AppendRadioItem(MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_KOREAN, _("&Korean"), wxEmptyString)->Check(config.console_language == CafeConsoleLanguage::KO);
	optionsConsoleLanguageMenu->AppendRadioItem(MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_DUTCH, _("&Dutch"), wxEmptyString)->Check(config.console_language == CafeConsoleLanguage::NL);
	optionsConsoleLanguageMenu->AppendRadioItem(MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_PORTUGUESE, _("&Portuguese"), wxEmptyString)->Check(config.console_language == CafeConsoleLanguage::PT);
	optionsConsoleLanguageMenu->AppendRadioItem(MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_RUSSIAN, _("&Russian"), wxEmptyString)->Check(config.console_language == CafeConsoleLanguage::RU);
	optionsConsoleLanguageMenu->AppendRadioItem(MAINFRAME_MENU_ID_OPTIONS_LANGUAGE_TAIWANESE, _("&Taiwanese"), wxEmptyString)->Check(config.console_language == CafeConsoleLanguage::TW);

	// options submenu
	wxMenu* optionsMenu = new wxMenu();
	m_fullscreenMenuItem = optionsMenu->AppendCheckItem(MAINFRAME_MENU_ID_OPTIONS_FULLSCREEN, _("&Fullscreen"), wxEmptyString);
	m_fullscreenMenuItem->Check(ActiveSettings::FullscreenEnabled());		
	
	optionsMenu->Append(MAINFRAME_MENU_ID_OPTIONS_GRAPHIC_PACKS2, _("&Graphic packs"));
	m_padViewMenuItem = optionsMenu->AppendCheckItem(MAINFRAME_MENU_ID_OPTIONS_SECOND_WINDOW_PADVIEW, _("&Separate GamePad view"), wxEmptyString);
	m_padViewMenuItem->Check(GetConfig().pad_open);
	optionsMenu->AppendSeparator();
	optionsMenu->Append(MAINFRAME_MENU_ID_OPTIONS_GENERAL2, _("&General settings"));
	optionsMenu->Append(MAINFRAME_MENU_ID_OPTIONS_INPUT, _("&Input settings"));

	optionsMenu->AppendSeparator();
	optionsMenu->AppendSubMenu(m_optionsAccountMenu, _("&Active account"));
	optionsMenu->AppendSubMenu(optionsConsoleLanguageMenu, _("&Console language"));
	m_menuBar->Append(optionsMenu, _("&Options"));

	// tools submenu
	wxMenu* toolsMenu = new wxMenu();
	m_memorySearcherMenuItem = toolsMenu->Append(MAINFRAME_MENU_ID_TOOLS_MEMORY_SEARCHER, _("&Memory searcher"));
	m_memorySearcherMenuItem->Enable(false);
	toolsMenu->Append(MAINFRAME_MENU_ID_TOOLS_TITLE_MANAGER, _("&Title Manager"));
	toolsMenu->Append(MAINFRAME_MENU_ID_TOOLS_DOWNLOAD_MANAGER, _("&Download Manager"));

	m_menuBar->Append(toolsMenu, _("&Tools"));

	// cpu timer speed menu
	wxMenu* timerSpeedMenu = new wxMenu();
	timerSpeedMenu->AppendRadioItem(MAINFRAME_MENU_ID_TIMER_SPEED_1X, _("&1x speed"), wxEmptyString)->Check(ActiveSettings::GetTimerShiftFactor() == 3);
	timerSpeedMenu->AppendRadioItem(MAINFRAME_MENU_ID_TIMER_SPEED_2X, _("&2x speed"), wxEmptyString)->Check(ActiveSettings::GetTimerShiftFactor() == 2);
	timerSpeedMenu->AppendRadioItem(MAINFRAME_MENU_ID_TIMER_SPEED_4X, _("&4x speed"), wxEmptyString)->Check(ActiveSettings::GetTimerShiftFactor() == 1);
	timerSpeedMenu->AppendRadioItem(MAINFRAME_MENU_ID_TIMER_SPEED_8X, _("&8x speed"), wxEmptyString)->Check(ActiveSettings::GetTimerShiftFactor() == 0);
	timerSpeedMenu->AppendRadioItem(MAINFRAME_MENU_ID_TIMER_SPEED_05X, _("&0.5x speed"), wxEmptyString)->Check(ActiveSettings::GetTimerShiftFactor() == 4);
	timerSpeedMenu->AppendRadioItem(MAINFRAME_MENU_ID_TIMER_SPEED_025X, _("&0.25x speed"), wxEmptyString)->Check(ActiveSettings::GetTimerShiftFactor() == 5);
	timerSpeedMenu->AppendRadioItem(MAINFRAME_MENU_ID_TIMER_SPEED_0125X, _("&0.125x speed"), wxEmptyString)->Check(ActiveSettings::GetTimerShiftFactor() == 6);

	// cpu submenu
	wxMenu* cpuMenu = new wxMenu();
	cpuMenu->AppendSubMenu(timerSpeedMenu, _("&Timer speed"));
	m_menuBar->Append(cpuMenu, _("&CPU"));

	// nfc submenu
	wxMenu* nfcMenu = new wxMenu();
	m_nfcMenu = nfcMenu;
	nfcMenu->Append(MAINFRAME_MENU_ID_NFC_TOUCH_NFC_FILE, _("&Scan NFC tag from file"))->Enable(false);
	m_menuBar->Append(nfcMenu, _("&NFC"));
	m_nfcMenuSeparator0 = nullptr;
	// debug->logging submenu
	wxMenu* debugLoggingMenu = new wxMenu;

	debugLoggingMenu->AppendCheckItem(MAINFRAME_MENU_ID_DEBUG_LOGGING0 + stdx::to_underlying(LogType::UnsupportedAPI), _("&Unsupported API calls"), wxEmptyString)->Check(cemuLog_isLoggingEnabled(LogType::UnsupportedAPI));
	debugLoggingMenu->AppendCheckItem(MAINFRAME_MENU_ID_DEBUG_LOGGING0 + stdx::to_underlying(LogType::CoreinitLogging), _("&Coreinit Logging (OSReport/OSConsole)"), wxEmptyString)->Check(cemuLog_isLoggingEnabled(LogType::CoreinitLogging));
	debugLoggingMenu->AppendCheckItem(MAINFRAME_MENU_ID_DEBUG_LOGGING0 + stdx::to_underlying(LogType::CoreinitFile), _("&Coreinit File-Access API"), wxEmptyString)->Check(cemuLog_isLoggingEnabled(LogType::CoreinitFile));
	debugLoggingMenu->AppendCheckItem(MAINFRAME_MENU_ID_DEBUG_LOGGING0 + stdx::to_underlying(LogType::CoreinitThreadSync), _("&Coreinit Thread-Synchronization API"), wxEmptyString)->Check(cemuLog_isLoggingEnabled(LogType::CoreinitThreadSync));
	debugLoggingMenu->AppendCheckItem(MAINFRAME_MENU_ID_DEBUG_LOGGING0 + stdx::to_underlying(LogType::CoreinitMem), _("&Coreinit Memory API"), wxEmptyString)->Check(cemuLog_isLoggingEnabled(LogType::CoreinitMem));
	debugLoggingMenu->AppendCheckItem(MAINFRAME_MENU_ID_DEBUG_LOGGING0 + stdx::to_underlying(LogType::CoreinitMP), _("&Coreinit MP API"), wxEmptyString)->Check(cemuLog_isLoggingEnabled(LogType::CoreinitMP));
	debugLoggingMenu->AppendCheckItem(MAINFRAME_MENU_ID_DEBUG_LOGGING0 + stdx::to_underlying(LogType::CoreinitThread), _("&Coreinit Thread API"), wxEmptyString)->Check(cemuLog_isLoggingEnabled(LogType::CoreinitThread));
	debugLoggingMenu->AppendCheckItem(MAINFRAME_MENU_ID_DEBUG_LOGGING0 + stdx::to_underlying(LogType::NN_NFP), _("&NN NFP"), wxEmptyString)->Check(cemuLog_isLoggingEnabled(LogType::NN_NFP));
	debugLoggingMenu->AppendCheckItem(MAINFRAME_MENU_ID_DEBUG_LOGGING0 + stdx::to_underlying(LogType::NN_FP), _("&NN FP"), wxEmptyString)->Check(cemuLog_isLoggingEnabled(LogType::NN_FP));
	debugLoggingMenu->AppendCheckItem(MAINFRAME_MENU_ID_DEBUG_LOGGING0 + stdx::to_underlying(LogType::GX2), _("&GX2 API"), wxEmptyString)->Check(cemuLog_isLoggingEnabled(LogType::GX2));
	debugLoggingMenu->AppendCheckItem(MAINFRAME_MENU_ID_DEBUG_LOGGING0 + stdx::to_underlying(LogType::SoundAPI), _("&Audio API"), wxEmptyString)->Check(cemuLog_isLoggingEnabled(LogType::SoundAPI));
	debugLoggingMenu->AppendCheckItem(MAINFRAME_MENU_ID_DEBUG_LOGGING0 + stdx::to_underlying(LogType::InputAPI), _("&Input API"), wxEmptyString)->Check(cemuLog_isLoggingEnabled(LogType::InputAPI));
	debugLoggingMenu->AppendCheckItem(MAINFRAME_MENU_ID_DEBUG_LOGGING0 + stdx::to_underlying(LogType::Socket), _("&Socket API"), wxEmptyString)->Check(cemuLog_isLoggingEnabled(LogType::Socket));
	debugLoggingMenu->AppendCheckItem(MAINFRAME_MENU_ID_DEBUG_LOGGING0 + stdx::to_underlying(LogType::Save), _("&Save API"), wxEmptyString)->Check(cemuLog_isLoggingEnabled(LogType::Save));
	debugLoggingMenu->AppendCheckItem(MAINFRAME_MENU_ID_DEBUG_LOGGING0 + stdx::to_underlying(LogType::H264), _("&H264 API"), wxEmptyString)->Check(cemuLog_isLoggingEnabled(LogType::H264));
	debugLoggingMenu->AppendSeparator();
	debugLoggingMenu->AppendCheckItem(MAINFRAME_MENU_ID_DEBUG_LOGGING0 + stdx::to_underlying(LogType::Patches), _("&Graphic pack patches"), wxEmptyString)->Check(cemuLog_isLoggingEnabled(LogType::Patches));
	debugLoggingMenu->AppendCheckItem(MAINFRAME_MENU_ID_DEBUG_LOGGING0 + stdx::to_underlying(LogType::TextureCache), _("&Texture cache warnings"), wxEmptyString)->Check(cemuLog_isLoggingEnabled(LogType::TextureCache));
	debugLoggingMenu->AppendCheckItem(MAINFRAME_MENU_ID_DEBUG_LOGGING0 + stdx::to_underlying(LogType::TextureReadback), _("&Texture readback"), wxEmptyString)->Check(cemuLog_isLoggingEnabled(LogType::TextureReadback));
	debugLoggingMenu->AppendSeparator();
	debugLoggingMenu->AppendCheckItem(MAINFRAME_MENU_ID_DEBUG_LOGGING0 + stdx::to_underlying(LogType::OpenGLLogging), _("&OpenGL debug output"), wxEmptyString)->Check(cemuLog_isLoggingEnabled(LogType::OpenGLLogging));
	debugLoggingMenu->AppendCheckItem(MAINFRAME_MENU_ID_DEBUG_LOGGING0 + stdx::to_underlying(LogType::VulkanValidation), _("&Vulkan validation layer (slow)"), wxEmptyString)->Check(cemuLog_isLoggingEnabled(LogType::VulkanValidation));
	debugLoggingMenu->AppendCheckItem(MAINFRAME_MENU_ID_DEBUG_ADVANCED_PPC_INFO, _("&Log PPC context for API"), wxEmptyString)->Check(cemuLog_advancedPPCLoggingEnabled());
	m_loggingSubmenu = debugLoggingMenu;
	// debug->dump submenu
	wxMenu* debugDumpMenu = new wxMenu;
	debugDumpMenu->AppendCheckItem(MAINFRAME_MENU_ID_DEBUG_DUMP_TEXTURES, _("&Textures"), wxEmptyString)->Check(ActiveSettings::DumpTexturesEnabled());
	debugDumpMenu->AppendCheckItem(MAINFRAME_MENU_ID_DEBUG_DUMP_SHADERS, _("&Shaders"), wxEmptyString)->Check(ActiveSettings::DumpShadersEnabled());
	debugDumpMenu->AppendCheckItem(MAINFRAME_MENU_ID_DEBUG_DUMP_CURL_REQUESTS, _("&nlibcurl HTTP/HTTPS requests"), wxEmptyString);
	// debug submenu
	wxMenu* debugMenu = new wxMenu();
	m_debugMenu = debugMenu;
	debugMenu->AppendSubMenu(debugLoggingMenu, _("&Logging"));
	debugMenu->AppendSubMenu(debugDumpMenu, _("&Dump"));
	debugMenu->AppendSeparator();
	
	auto upsidedownItem = debugMenu->AppendCheckItem(MAINFRAME_MENU_ID_DEBUG_RENDER_UPSIDE_DOWN, _("&Render upside-down"), wxEmptyString);
	upsidedownItem->Check(ActiveSettings::RenderUpsideDownEnabled());
	if(LaunchSettings::RenderUpsideDownEnabled().has_value())
		upsidedownItem->Enable(false);

	auto accurateBarriers = debugMenu->AppendCheckItem(MAINFRAME_MENU_ID_DEBUG_VK_ACCURATE_BARRIERS, _("&Accurate barriers (Vulkan)"), wxEmptyString);
	accurateBarriers->Check(GetConfig().vk_accurate_barriers);

	debugMenu->AppendSeparator();

#ifdef CEMU_DEBUG_ASSERT
	auto audioAuxOnly = debugMenu->AppendCheckItem(MAINFRAME_MENU_ID_DEBUG_AUDIO_AUX_ONLY, _("&Audio AUX only"), wxEmptyString);
	audioAuxOnly->Check(ActiveSettings::AudioOutputOnlyAux());
#endif

	debugMenu->Append(MAINFRAME_MENU_ID_DEBUG_VIEW_LOGGING_WINDOW, _("&Open logging window"));
	m_gdbstub_toggle = debugMenu->AppendCheckItem(MAINFRAME_MENU_ID_DEBUG_TOGGLE_GDB_STUB, _("&Launch with GDB stub"), wxEmptyString);
	m_gdbstub_toggle->Check(g_gdbstub != nullptr);
	m_gdbstub_toggle->Enable(!m_game_launched);

	debugMenu->Append(MAINFRAME_MENU_ID_DEBUG_VIEW_PPC_THREADS, _("&View PPC threads"));
	debugMenu->Append(MAINFRAME_MENU_ID_DEBUG_VIEW_PPC_DEBUGGER, _("&View PPC debugger"));
	debugMenu->Append(MAINFRAME_MENU_ID_DEBUG_VIEW_AUDIO_DEBUGGER, _("&View audio debugger"));
	debugMenu->Append(MAINFRAME_MENU_ID_DEBUG_VIEW_TEXTURE_RELATIONS, _("&View texture cache info"));
	debugMenu->Append(MAINFRAME_MENU_ID_DEBUG_DUMP_RAM, _("&Dump current RAM"));
	// debugMenu->Append(MAINFRAME_MENU_ID_DEBUG_DUMP_FST, _("&Dump WUD filesystem"))->Enable(false);

	m_menuBar->Append(debugMenu, _("&Debug"));
	// help menu
	wxMenu* helpMenu = new wxMenu();
	m_check_update_menu = helpMenu->Append(MAINFRAME_MENU_ID_HELP_UPDATE, _("&Check for updates"));
#if BOOST_OS_LINUX || BOOST_OS_MACOS
	m_check_update_menu->Enable(false);
#endif
	helpMenu->Append(MAINFRAME_MENU_ID_HELP_GETTING_STARTED, _("&Getting started"));
	helpMenu->AppendSeparator();
	helpMenu->Append(MAINFRAME_MENU_ID_HELP_ABOUT, _("&About Cemu"));

	m_menuBar->Append(helpMenu, _("&Help"));

	SetMenuBar(m_menuBar);
	m_menu_visible = true;

	if (m_game_launched)
	{
		if (m_check_update_menu)
			m_check_update_menu->Enable(false);

		m_memorySearcherMenuItem->Enable(true);
		m_nfcMenu->Enable(MAINFRAME_MENU_ID_NFC_TOUCH_NFC_FILE, true);

		// these options cant be toggled after the renderer backend is initialized:
		m_loggingSubmenu->Enable(MAINFRAME_MENU_ID_DEBUG_LOGGING0 + stdx::to_underlying(LogType::OpenGLLogging), false);
		m_loggingSubmenu->Enable(MAINFRAME_MENU_ID_DEBUG_LOGGING0 + stdx::to_underlying(LogType::VulkanValidation), false);

		UpdateNFCMenu();
	}

	// hide new menu in fullscreen
	if (IsFullScreen())
		SetMenuVisible(false);
}

void MainWindow::RestoreSettingsAfterGameExited()
{
	RecreateMenu();
}

void MainWindow::UpdateSettingsAfterGameLaunch()
{
	m_update_available = {};
	RecreateMenu();
}

void MainWindow::OnGraphicWindowClose(wxCloseEvent& event)
{
	m_graphic_pack_window->Destroy();
	m_graphic_pack_window = nullptr;
}

void MainWindow::OnGraphicWindowOpen(wxTitleIdEvent& event)
{
	if (m_graphic_pack_window)
		return;
	m_graphic_pack_window = new GraphicPacksWindow2(this, event.GetTitleId());
	m_graphic_pack_window->Bind(wxEVT_CLOSE_WINDOW, &MainWindow::OnGraphicWindowClose, this);
	m_graphic_pack_window->Show(true);
}

void MainWindow::RequestGameListRefresh()
{
	auto* evt = new wxCommandEvent(wxEVT_REQUEST_GAMELIST_REFRESH);
	wxQueueEvent(g_mainFrame, evt);
}

void MainWindow::RequestLaunchGame(fs::path filePath, wxLaunchGameEvent::INITIATED_BY initiatedBy)
{
	wxLaunchGameEvent evt(filePath, initiatedBy);
	wxPostEvent(g_mainFrame, evt);
}

void MainWindow::OnRequestRecreateCanvas(wxCommandEvent& event)
{
	CounterSemaphore* sem = (CounterSemaphore*)event.GetClientData();
	DestroyCanvas();
	CreateCanvas();
	sem->increment();
}

void MainWindow::CafeRecreateCanvas()
{
	CounterSemaphore sem;
	auto* evt = new wxCommandEvent(wxEVT_REQUEST_RECREATE_CANVAS);
	evt->SetClientData((void*)&sem);
	wxQueueEvent(g_mainFrame, evt);
	sem.decrementWithWait();
}
