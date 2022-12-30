#include "gui/CemuApp.h"
#include "gui/MainWindow.h"
#include "gui/wxgui.h"
#include "config/CemuConfig.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
#include "gui/guiWrapper.h"
#include "config/ActiveSettings.h"
#include "gui/GettingStartedDialog.h"
#include "config/PermanentConfig.h"
#include "config/PermanentStorage.h"
#include "input/InputManager.h"
#include "gui/helpers/wxHelpers.h"
#include "Cemu/ncrypto/ncrypto.h"

#include <wx/image.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>

#include "Cafe/TitleList/TitleList.h"
#include "Cafe/TitleList/SaveList.h"

wxIMPLEMENT_APP_NO_MAIN(CemuApp);

// defined in guiWrapper.cpp
extern WindowInfo g_window_info;
extern std::shared_mutex g_mutex;

int mainEmulatorHLE();
void HandlePostUpdate();
// Translation strings to extract for gettext:
void unused_translation_dummy()
{
	void(_("Browse"));
	void(_("Select a file"));
	void(_("Select a directory"));
	
	void(_("base"));
	void(_("update"));
	void(_("dlc"));
	void(_("save"));

	void(_("Japan"));
	void(_("USA"));
	void(_("Europe"));
	void(_("Australia"));
	void(_("China"));
	void(_("Korea"));
	void(_("Taiwan"));
	void(_("Auto"));
	void(_("many"));

	void(_("Japanese"));
	void(_("English"));
	void(_("French"));
	void(_("German"));
	void(_("Italian"));
	void(_("Spanish"));
	void(_("Chinese"));
	void(_("Korean"));
	void(_("Dutch"));
	void(_("Portugese"));
	void(_("Russian"));
	void(_("Taiwanese"));
	void(_("unknown"));


	// account.h
	void(_("AccountId missing (The account is not connected to a NNID)"));
	void(_("IsPasswordCacheEnabled is set to false (The remember password option on your Wii U must be enabled for this account before dumping it)"));
	void(_("AccountPasswordCache is empty (The remember password option on your Wii U must be enabled for this account before dumping it)"));
	void(_("PrincipalId missing"));
}

bool CemuApp::OnInit()
{
	fs::path user_data_path, config_path, cache_path, data_path;
	auto standardPaths = wxStandardPaths::Get();
#ifdef PORTABLE
	fs::path exePath(standardPaths.GetExecutablePath().ToStdString());
#if MACOS_BUNDLE
    exePath = exePath.parent_path().parent_path().parent_path();
#endif
	user_data_path = config_path = cache_path = data_path = exePath.parent_path();
#else
	SetAppName("Cemu");
	wxString appName=GetAppName();
	#if BOOST_OS_LINUX
	standardPaths.SetFileLayout(wxStandardPaths::FileLayout::FileLayout_XDG);
	auto getEnvDir = [&](const wxString& varName, const wxString& defaultValue)
	{
		wxString dir;
		if (!wxGetEnv(varName, &dir) || dir.empty())
			return defaultValue;
		return dir;
	};
	wxString homeDir=wxFileName::GetHomeDir();
	user_data_path = (getEnvDir(wxS("XDG_DATA_HOME"), homeDir + wxS("/.local/share")) + "/" + appName).ToStdString();
	config_path = (getEnvDir(wxS("XDG_CONFIG_HOME"), homeDir + wxS("/.config")) + "/" + appName).ToStdString();
	#else
	user_data_path = config_path = standardPaths.GetUserDataDir().ToStdString();
	#endif
	data_path = standardPaths.GetDataDir().ToStdString();
	cache_path = standardPaths.GetUserDir(wxStandardPaths::Dir::Dir_Cache).ToStdString();
	cache_path /= appName.ToStdString();
#endif
	auto failed_write_access = ActiveSettings::LoadOnce(user_data_path, config_path, cache_path, data_path);
	for (auto&& path : failed_write_access)
		wxMessageBox(fmt::format("Cemu can't write to {} !", path.generic_string()), _("Warning"), wxOK | wxCENTRE | wxICON_EXCLAMATION, nullptr);

	NetworkConfig::LoadOnce();

	HandlePostUpdate();
	mainEmulatorHLE();

	wxInitAllImageHandlers();

	g_config.Load();
	m_languages = GetAvailableLanguages();

	const sint32 language = GetConfig().language;
	const auto it = std::find_if(m_languages.begin(), m_languages.end(), [language](const wxLanguageInfo* info) { return info->Language == language; });
	if (it != m_languages.end() && wxLocale::IsAvailable(language))
	{
		if (m_locale.Init(language))
		{
			m_locale.AddCatalogLookupPathPrefix(ActiveSettings::GetDataPath("resources").generic_string());
			m_locale.AddCatalog("cemu");
		}
	}

	if (!m_locale.IsOk())
	{
		if (!wxLocale::IsAvailable(wxLANGUAGE_DEFAULT) || !m_locale.Init(wxLANGUAGE_DEFAULT))
		{
            m_locale.Init(wxLANGUAGE_ENGLISH);
            m_locale.AddCatalogLookupPathPrefix(ActiveSettings::GetDataPath("resources").generic_string());
            m_locale.AddCatalog("cemu");
		}
	}

	// fill colour db
	wxTheColourDatabase->AddColour("ERROR", wxColour(0xCC, 0, 0));
	wxTheColourDatabase->AddColour("SUCCESS", wxColour(0, 0xbb, 0));

#if BOOST_OS_WINDOWS
	const auto parent_path = GetParentProcess();
	if(parent_path.has_filename())
	{
		const auto filename = parent_path.filename().generic_string();
		if (boost::icontains(filename, "WiiU_USB_Helper"))
			__fastfail(0);
	}
#endif

	// init input
	InputManager::instance().load();
	
	InitializeGlobalVulkan();

	Bind(wxEVT_ACTIVATE_APP, &CemuApp::ActivateApp, this);

	auto& config = GetConfig();
	const bool first_start = !config.did_show_graphic_pack_download;

	CreateDefaultFiles(first_start);

	m_mainFrame = new MainWindow();

	if (first_start)
		m_mainFrame->ShowGettingStartedDialog();

	std::unique_lock lock(g_mutex);
	g_window_info.app_active = true;

	SetTopWindow(m_mainFrame);
	m_mainFrame->Show();

	// show warning on macOS about state of builds
#if BOOST_OS_MACOS
	if (!GetConfig().did_show_macos_disclaimer)
	{
		const auto message = _(
			"Thank you for testing the in-development build of Cemu for macOS.\n \n"
			"The macOS port is currently purely experimental and should not be considered stable or ready for issue-free gameplay. "
			"There are also known issues with degraded performance due to the use of MoltenVk and Rosetta for ARM Macs. We appreciate your patience while we improve Cemu for macOS.");
		wxMessageDialog dialog(nullptr, message, "Preview version", wxCENTRE | wxOK | wxICON_WARNING);
		dialog.SetOKLabel(_("I understand"));
		dialog.ShowModal();
		GetConfig().did_show_macos_disclaimer = true;
		g_config.Save();
	}
#endif

	return true;
}

int CemuApp::OnExit()
{
	wxApp::OnExit();
#if BOOST_OS_WINDOWS
	ExitProcess(0);
#else
	_Exit(0);
#endif
}

#if BOOST_OS_WINDOWS
void DumpThreadStackTrace();
#endif

void CemuApp::OnAssertFailure(const wxChar* file, int line, const wxChar* func, const wxChar* cond, const wxChar* msg)
{
	cemuLog_createLogFile(false);
	cemuLog_log(LogType::Force, "Encountered wxWidgets assert!");
	cemuLog_log(LogType::Force, fmt::format(L"File: {0} Line: {1}", std::wstring_view(file), line));
	cemuLog_log(LogType::Force, fmt::format(L"Func: {0} Cond: {1}", func, std::wstring_view(cond)));
	cemuLog_log(LogType::Force, fmt::format(L"Message: {}", std::wstring_view(msg)));

#if BOOST_OS_WINDOWS
	DumpThreadStackTrace();
#endif
	cemu_assert_debug(false);
}

int CemuApp::FilterEvent(wxEvent& event)
{
	if(event.GetEventType() == wxEVT_KEY_DOWN)
	{
		const auto& key_event = (wxKeyEvent&)event;
		wxGetKeyState(wxKeyCode::WXK_F17);
		g_window_info.set_keystate(fix_raw_keycode(key_event.GetRawKeyCode(), key_event.GetRawKeyFlags()), true);
	}
	else if(event.GetEventType() == wxEVT_KEY_UP)
	{
		const auto& key_event = (wxKeyEvent&)event;
		g_window_info.set_keystate(fix_raw_keycode(key_event.GetRawKeyCode(), key_event.GetRawKeyFlags()), false);
	}
	else if(event.GetEventType() == wxEVT_ACTIVATE_APP)
	{
		const auto& activate_event = (wxActivateEvent&)event;
		if(!activate_event.GetActive())
			g_window_info.set_keystatesup();
	}

	return wxApp::FilterEvent(event);
}

std::vector<const wxLanguageInfo*> CemuApp::GetAvailableLanguages()
{
	const auto path = ActiveSettings::GetDataPath("resources");
	if (!exists(path))
		return {};
	
	std::vector<const wxLanguageInfo*> result;
	for (const auto& p : fs::directory_iterator(path))
	{
		if (!fs::is_directory(p))
			continue;

		const auto& path = p.path();
		auto filename = path.filename();

		const auto* lang_info = wxLocale::FindLanguageInfo(filename.c_str());
		if (!lang_info)
			continue;

		const auto language_file = path / "cemu.mo";
		if (!fs::exists(language_file))
			continue;

		result.emplace_back(lang_info);
	}

	return result;
}

void CemuApp::CreateDefaultFiles(bool first_start)
{
	std::wstring mlc = GetMLCPath().ToStdWstring();

	// check for mlc01 folder missing if custom path has been set
	if (!fs::exists(mlc) && !first_start)
	{
		const std::wstring message = fmt::format(fmt::runtime(_(L"Your mlc01 folder seems to be missing.\n\nThis is where Cemu stores save files, game updates and other Wii U files.\n\nThe expected path is:\n{}\n\nDo you want to create the folder at the expected path?").ToStdWstring()), mlc);
		
		wxMessageDialog dialog(nullptr, message, "Error", wxCENTRE | wxYES_NO | wxCANCEL| wxICON_WARNING);
		dialog.SetYesNoCancelLabels(_("Yes"), _("No"), _("Select a custom path"));
		const auto dialogResult = dialog.ShowModal();
		if (dialogResult == wxID_NO)
			exit(0);
		else if(dialogResult == wxID_CANCEL)
		{
			if (!SelectMLCPath())
				return;

			mlc = GetMLCPath();
		}
		else
		{
			GetConfig().mlc_path = L"";
			g_config.Save();
		}
	}

	// create sys/usr folder in mlc01
	try
	{
		const auto sysFolder = fs::path(mlc).append(L"sys");
		fs::create_directories(sysFolder);

		const auto usrFolder = fs::path(mlc).append(L"usr");
		fs::create_directories(usrFolder);
		fs::create_directories(fs::path(usrFolder).append("title/00050000")); // base
		fs::create_directories(fs::path(usrFolder).append("title/0005000c")); // dlc
		fs::create_directories(fs::path(usrFolder).append("title/0005000e")); // update

		// Mii Maker save folders {0x500101004A000, 0x500101004A100, 0x500101004A200},
		fs::create_directories(fs::path(mlc).append(L"usr/save/00050010/1004a000/user/common/db"));
		fs::create_directories(fs::path(mlc).append(L"usr/save/00050010/1004a100/user/common/db"));
		fs::create_directories(fs::path(mlc).append(L"usr/save/00050010/1004a200/user/common/db"));

		// lang files
		auto langDir = fs::path(mlc).append(L"sys/title/0005001b/1005c000/content");
		fs::create_directories(langDir);

		auto langFile = fs::path(langDir).append("language.txt");
		if (!fs::exists(langFile))
		{
			std::ofstream file(langFile);
			if (file.is_open())
			{
				const char* langStrings[] = { "ja","en","fr","de","it","es","zh","ko","nl","pt","ru","zh" };
				for (const char* lang : langStrings)
					file << fmt::format(R"("{}",)", lang) << std::endl;

				file.flush();
				file.close();
			}
		}

		auto countryFile = fs::path(langDir).append("country.txt");
		if (!fs::exists(countryFile))
		{
			std::ofstream file(countryFile);
			for (sint32 i = 0; i < 201; i++)
			{
				const char* countryCode = NCrypto::GetCountryAsString(i);
				if (boost::iequals(countryCode, "NN"))
					file << "NULL," << std::endl;
				else
					file << fmt::format(R"("{}",)", countryCode) << std::endl;
			}
			file.flush();
			file.close();
		}
	}
	catch (const std::exception& ex)
	{
		std::stringstream errorMsg;
		errorMsg << fmt::format(fmt::runtime(_("Couldn't create a required mlc01 subfolder or file!\n\nError: {0}\nTarget path:\n{1}").ToStdString()), ex.what(), boost::nowide::narrow(mlc));

#if BOOST_OS_WINDOWS
		const DWORD lastError = GetLastError();
		if (lastError != ERROR_SUCCESS)
			errorMsg << fmt::format("\n\n{}", GetSystemErrorMessage(lastError));

		wxMessageBox(errorMsg.str(), "Error", wxOK | wxCENTRE | wxICON_ERROR);
#endif
		exit(0);
	}

	// cemu directories
	try
	{
		const auto controllerProfileFolder = GetConfigPath(L"controllerProfiles").ToStdWstring();
		if (!fs::exists(controllerProfileFolder))
			fs::create_directories(controllerProfileFolder);

		const auto memorySearcherFolder = GetUserDataPath(L"memorySearcher").ToStdWstring();
		if (!fs::exists(memorySearcherFolder))
			fs::create_directories(memorySearcherFolder);
	}
	catch (const std::exception& ex)
	{
		std::stringstream errorMsg;
		errorMsg << fmt::format(fmt::runtime(_("Couldn't create a required cemu directory or file!\n\nError: {0}").ToStdString()), ex.what());

#if BOOST_OS_WINDOWS
		const DWORD lastError = GetLastError();
		if (lastError != ERROR_SUCCESS)
			errorMsg << fmt::format("\n\n{}", GetSystemErrorMessage(lastError));


		wxMessageBox(errorMsg.str(), "Error", wxOK | wxCENTRE | wxICON_ERROR);
#endif
		exit(0);
	}
}


bool CemuApp::TrySelectMLCPath(std::wstring path)
{
	if (path.empty())
		path = ActiveSettings::GetDefaultMLCPath().wstring();

	if (!TestWriteAccess(fs::path{ path }))
		return false;

	GetConfig().SetMLCPath(path);
	CemuApp::CreateDefaultFiles();

	// update TitleList and SaveList scanner with new MLC path
	CafeTitleList::SetMLCPath(path);
	CafeTitleList::Refresh();
	CafeSaveList::SetMLCPath(path);
	CafeSaveList::Refresh();
	return true;
}

bool CemuApp::SelectMLCPath(wxWindow* parent)
{
	auto& config = GetConfig();
	
	std::wstring default_path;
	if (fs::exists(config.mlc_path.GetValue()))
		default_path = config.mlc_path.GetValue();

	// try until users selects a valid path or aborts
	while(true)
	{
		wxDirDialog path_dialog(parent, _("Select a mlc directory"), default_path, wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
		if (path_dialog.ShowModal() != wxID_OK || path_dialog.GetPath().empty())
			return false;

		const auto path = path_dialog.GetPath().ToStdWstring();

		if (!TrySelectMLCPath(path))
		{
			const auto result = wxMessageBox(_("Cemu can't write to the selected mlc path!\nDo you want to select another path?"), _("Error"), wxYES_NO | wxCENTRE | wxICON_ERROR);
			if (result == wxYES)
				continue;

			break;
		}

		return true;
	}

	return false;
}


wxString CemuApp::GetMLCPath()
{
	return ActiveSettings::GetMlcPath().generic_wstring();
}

wxString CemuApp::GetMLCPath(const wxString& cat)
{
	return ActiveSettings::GetMlcPath(cat.ToStdString()).generic_wstring();
}

wxString CemuApp::GetConfigPath()
{
	return ActiveSettings::GetConfigPath().generic_wstring();
};

wxString CemuApp::GetConfigPath(const wxString& cat)
{
	return ActiveSettings::GetConfigPath(cat.ToStdString()).generic_wstring();
};

wxString CemuApp::GetUserDataPath()
{
	return ActiveSettings::GetUserDataPath().generic_wstring();
};

wxString CemuApp::GetUserDataPath(const wxString& cat)
{
	return ActiveSettings::GetUserDataPath(cat.ToStdString()).generic_wstring();
};

void CemuApp::ActivateApp(wxActivateEvent& event)
{
	g_window_info.app_active = event.GetActive();
	event.Skip();
}


