#include "gui/CemuApp.h"
#include "gui/MainWindow.h"
#include "gui/wxgui.h"
#include "config/CemuConfig.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
#include "Cafe/HW/Latte/Core/LatteOverlay.h"
#include "gui/guiWrapper.h"
#include "config/ActiveSettings.h"
#include "config/LaunchSettings.h"
#include "gui/GettingStartedDialog.h"
#include "input/InputManager.h"
#include "gui/helpers/wxHelpers.h"
#include "Cemu/ncrypto/ncrypto.h"

#if BOOST_OS_LINUX && HAS_WAYLAND
#include "gui/helpers/wxWayland.h"
#endif
#if __WXGTK__
#include <glib.h>
#endif

#include <wx/image.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include "wxHelper.h"

#include "Cafe/TitleList/TitleList.h"
#include "Cafe/TitleList/SaveList.h"

wxIMPLEMENT_APP_NO_MAIN(CemuApp);

// defined in guiWrapper.cpp
extern WindowInfo g_window_info;
extern std::shared_mutex g_mutex;

// forward declarations from main.cpp
void UnitTests();
void CemuCommonInit();

void HandlePostUpdate();
// Translation strings to extract for gettext:
void unused_translation_dummy()
{
	void(_("Browse"));
	void(_("Select a file"));
	void(_("Select a directory"));

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
}

#if BOOST_OS_WINDOWS
#include <shlobj_core.h>
fs::path GetAppDataRoamingPath()
{
	PWSTR path = nullptr;
	HRESULT result = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path);
	if (result != S_OK || !path)
	{
		if (path)
			CoTaskMemFree(path);
		return {};
	}
	std::string appDataPath = boost::nowide::narrow(path);
	CoTaskMemFree(path);
	return _utf8ToPath(appDataPath);
}
#endif

#if BOOST_OS_WINDOWS
void CemuApp::DeterminePaths(std::set<fs::path>& failedWriteAccess) // for Windows
{
	std::error_code ec;
	bool isPortable = false;
	fs::path user_data_path, config_path, cache_path, data_path;
	auto standardPaths = wxStandardPaths::Get();
	fs::path exePath(wxHelper::MakeFSPath(standardPaths.GetExecutablePath()));
	fs::path portablePath = exePath.parent_path() / "portable";
	data_path = exePath.parent_path(); // the data path is always the same as the exe path
	if (fs::exists(portablePath, ec))
	{
		isPortable = true;
		user_data_path = config_path = cache_path = portablePath;
	}
	else
	{
		fs::path roamingPath = GetAppDataRoamingPath() / "Cemu";
		user_data_path = config_path = cache_path = roamingPath;
	}
	// on Windows Cemu used to be portable by default prior to 2.0-89
	// to remain backwards compatible with old installations we check for settings.xml in the Cemu directory
	// if it exists, we use the exe path as the portable directory
	if(!isPortable) // lower priority than portable directory
	{
		if (fs::exists(exePath.parent_path() / "settings.xml", ec))
		{
			isPortable = true;
			user_data_path = config_path = cache_path = exePath.parent_path();
		}
	}
	ActiveSettings::SetPaths(isPortable, exePath, user_data_path, config_path, cache_path, data_path, failedWriteAccess);
}
#endif

#if BOOST_OS_LINUX
void CemuApp::DeterminePaths(std::set<fs::path>& failedWriteAccess) // for Linux
{
	std::error_code ec;
	bool isPortable = false;
	fs::path user_data_path, config_path, cache_path, data_path;
	auto standardPaths = wxStandardPaths::Get();
	fs::path exePath(wxHelper::MakeFSPath(standardPaths.GetExecutablePath()));
	fs::path portablePath = exePath.parent_path() / "portable";
	// GetExecutablePath returns the AppImage's temporary mount location
	wxString appImagePath;
	if (wxGetEnv(("APPIMAGE"), &appImagePath))
	{
		exePath = wxHelper::MakeFSPath(appImagePath);
		portablePath = exePath.parent_path() / "portable";
	}
	if (fs::exists(portablePath, ec))
	{
		isPortable = true;
		user_data_path = config_path = cache_path = portablePath;
		// in portable mode assume the data directories (resources, gameProfiles/default/) are next to the executable
		data_path = exePath.parent_path();
	}
	else
	{
		SetAppName("Cemu");
		wxString appName = GetAppName();
		standardPaths.SetFileLayout(wxStandardPaths::FileLayout::FileLayout_XDG);
		auto getEnvDir = [&](const wxString& varName, const wxString& defaultValue)
		{
			wxString dir;
			if (!wxGetEnv(varName, &dir) || dir.empty())
				return defaultValue;
			return dir;
		};
		wxString homeDir = wxFileName::GetHomeDir();
		user_data_path = (getEnvDir(wxS("XDG_DATA_HOME"), homeDir + wxS("/.local/share")) + "/" + appName).ToStdString();
		config_path = (getEnvDir(wxS("XDG_CONFIG_HOME"), homeDir + wxS("/.config")) + "/" + appName).ToStdString();
		data_path = standardPaths.GetDataDir().ToStdString();
		cache_path = standardPaths.GetUserDir(wxStandardPaths::Dir::Dir_Cache).ToStdString();
		cache_path /= appName.ToStdString();
	}
	ActiveSettings::SetPaths(isPortable, exePath, user_data_path, config_path, cache_path, data_path, failedWriteAccess);
}
#endif

#if BOOST_OS_MACOS
void CemuApp::DeterminePaths(std::set<fs::path>& failedWriteAccess) // for MacOS
{
	std::error_code ec;
	bool isPortable = false;
	fs::path user_data_path, config_path, cache_path, data_path;
	auto standardPaths = wxStandardPaths::Get();
	fs::path exePath(wxHelper::MakeFSPath(standardPaths.GetExecutablePath()));
        // If run from an app bundle, use its parent directory
        fs::path appPath = exePath.parent_path().parent_path().parent_path();
        fs::path portablePath = appPath.extension() == ".app" ? appPath.parent_path() / "portable" : exePath.parent_path() / "portable";
	if (fs::exists(portablePath, ec))
	{
		isPortable = true;
		user_data_path = config_path = cache_path = portablePath;
		data_path = exePath.parent_path();
	}
	else
	{
		SetAppName("Cemu");
		wxString appName = GetAppName();
		user_data_path = config_path = standardPaths.GetUserDataDir().ToStdString();
		data_path = standardPaths.GetDataDir().ToStdString();
		cache_path = standardPaths.GetUserDir(wxStandardPaths::Dir::Dir_Cache).ToStdString();
		cache_path /= appName.ToStdString();
	}
	ActiveSettings::SetPaths(isPortable, exePath, user_data_path, config_path, cache_path, data_path, failedWriteAccess);
}
#endif

// create default MLC files or quit if it fails
void CemuApp::InitializeNewMLCOrFail(fs::path mlc)
{
	if( CemuApp::CreateDefaultMLCFiles(mlc) )
		return; // all good
	cemu_assert_debug(!ActiveSettings::IsCustomMlcPath()); // should not be possible?

	if(ActiveSettings::IsCommandLineMlcPath() || ActiveSettings::IsCustomMlcPath())
	{
		// tell user that the custom path is not writable
		wxMessageBox(formatWxString(_("Cemu failed to write to the custom mlc directory.\nThe path is:\n{}"), wxHelper::FromPath(mlc)), _("Error"), wxOK | wxCENTRE | wxICON_ERROR);
		exit(0);
	}
	wxMessageBox(formatWxString(_("Cemu failed to write to the mlc directory.\nThe path is:\n{}"), wxHelper::FromPath(mlc)), _("Error"), wxOK | wxCENTRE | wxICON_ERROR);
	exit(0);
}

void CemuApp::InitializeExistingMLCOrFail(fs::path mlc)
{
	if(CreateDefaultMLCFiles(mlc))
		return; // all good
	// failed to write mlc files
	if(ActiveSettings::IsCommandLineMlcPath() || ActiveSettings::IsCustomMlcPath())
	{
		// tell user that the custom path is not writable
		// if it's a command line path then just quit. Otherwise ask if user wants to reset the path
		if(ActiveSettings::IsCommandLineMlcPath())
		{
			wxMessageBox(formatWxString(_("Cemu failed to write to the custom mlc directory.\nThe path is:\n{}"), wxHelper::FromPath(mlc)), _("Error"), wxOK | wxCENTRE | wxICON_ERROR);
			exit(0);
		}
		// ask user if they want to reset the path
		const wxString message = formatWxString(_("Cemu failed to write to the custom mlc directory.\n\nThe path is:\n{}\n\nCemu cannot start without a valid mlc path. Do you want to reset the path? You can later change it again in the General Settings."),
												_pathToUtf8(mlc));
		wxMessageDialog dialog(nullptr, message, _("Error"), wxCENTRE | wxYES_NO | wxICON_WARNING);
		dialog.SetYesNoLabels(_("Reset path"), _("Exit"));
		const auto dialogResult = dialog.ShowModal();
		if (dialogResult == wxID_NO)
			exit(0);
		else // reset path
		{
			GetConfig().mlc_path = "";
			g_config.Save();
		}
	}
}

bool CemuApp::OnInit()
{
#if __WXGTK__
	GTKSuppressDiagnostics(G_LOG_LEVEL_MASK & ~G_LOG_FLAG_FATAL);
#endif
	std::set<fs::path> failedWriteAccess;
	DeterminePaths(failedWriteAccess);
	// make sure default cemu directories exist
	CreateDefaultCemuFiles();

	g_config.SetFilename(ActiveSettings::GetConfigPath("settings.xml").generic_wstring());

	std::error_code ec;
	bool isFirstStart = !fs::exists(ActiveSettings::GetConfigPath("settings.xml"), ec);

	NetworkConfig::LoadOnce();
	if(!isFirstStart)
	{
		g_config.Load();
		LocalizeUI(static_cast<wxLanguage>(GetConfig().language == wxLANGUAGE_DEFAULT ? wxLocale::GetSystemLanguage() : GetConfig().language.GetValue()));
	}
	else
	{
		LocalizeUI(static_cast<wxLanguage>(wxLocale::GetSystemLanguage()));
	}

	for (auto&& path : failedWriteAccess)
	{
		wxMessageBox(formatWxString(_("Cemu can't write to {}!"), wxString::FromUTF8(_pathToUtf8(path))),
					 _("Warning"), wxOK | wxCENTRE | wxICON_EXCLAMATION, nullptr);
	}

	if (isFirstStart)
	{
		// show the getting started dialog
		GettingStartedDialog dia(nullptr);
		dia.ShowModal();
		// make sure config is created. Gfx pack UI and input UI may create it earlier already, but we still want to update it
		g_config.Save();
		// create mlc, on failure the user can quit here. So do this after the Getting Started dialog
		InitializeNewMLCOrFail(ActiveSettings::GetMlcPath());
	}
	else
	{
		// check if mlc is valid and recreate default files
		InitializeExistingMLCOrFail(ActiveSettings::GetMlcPath());
	}

	ActiveSettings::Init(); // this is a bit of a misnomer, right now this call only loads certs for online play. In the future we should move the logic to a more appropriate place
	HandlePostUpdate();

	LatteOverlay_init();
	// run a couple of tests if in non-release mode
#ifdef CEMU_DEBUG_ASSERT
	UnitTests();
#endif
	CemuCommonInit();

	wxInitAllImageHandlers();

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
	InitializeGlobalVulkan();

	Bind(wxEVT_ACTIVATE_APP, &CemuApp::ActivateApp, this);

	auto& config = GetConfig();
	m_mainFrame = new MainWindow();

	std::unique_lock lock(g_mutex);
	g_window_info.app_active = true;

	SetTopWindow(m_mainFrame);
	m_mainFrame->Show();

#if BOOST_OS_LINUX && HAS_WAYLAND
	if (wxWlIsWaylandWindow(m_mainFrame))
		wxWlSetAppId(m_mainFrame, "info.cemu.Cemu");
#endif

	// show warning on macOS about state of builds
#if BOOST_OS_MACOS
	if (!GetConfig().did_show_macos_disclaimer)
	{
		const auto message = _(
			"Thank you for testing the in-development build of Cemu for macOS.\n \n"
			"The macOS port is currently purely experimental and should not be considered stable or ready for issue-free gameplay. "
			"There are also known issues with degraded performance due to the use of MoltenVk and Rosetta for ARM Macs. We appreciate your patience while we improve Cemu for macOS.");
		wxMessageDialog dialog(nullptr, message, _("Preview version"), wxCENTRE | wxOK | wxICON_WARNING);
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
	cemuLog_log(LogType::Force, "File: {0} Line: {1}", wxString(file).utf8_string(), line);
	cemuLog_log(LogType::Force, "Func: {0} Cond: {1}", wxString(func).utf8_string(), wxString(cond).utf8_string());
	cemuLog_log(LogType::Force, "Message: {}", wxString(msg).utf8_string());

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

std::vector<const wxLanguageInfo *> CemuApp::GetLanguages() const {
	std::vector availableLanguages(m_availableTranslations);
	availableLanguages.insert(availableLanguages.begin(), wxLocale::GetLanguageInfo(wxLANGUAGE_ENGLISH));
	return availableLanguages;
}

void CemuApp::LocalizeUI(wxLanguage languageToUse)
{
	std::unique_ptr<wxTranslations> translationsMgr(new wxTranslations());
	m_availableTranslations = GetAvailableTranslationLanguages(translationsMgr.get());

	bool isTranslationAvailable = std::any_of(m_availableTranslations.begin(), m_availableTranslations.end(),
											  [languageToUse](const wxLanguageInfo* info) { return info->Language == languageToUse; });
	if (languageToUse == wxLANGUAGE_DEFAULT || isTranslationAvailable)
	{
		translationsMgr->SetLanguage(static_cast<wxLanguage>(languageToUse));
		translationsMgr->AddCatalog("cemu");

		if (translationsMgr->IsLoaded("cemu") && wxLocale::IsAvailable(languageToUse))
		{
			m_locale.Init(languageToUse);
		}
		// This must be run after wxLocale::Init, as the latter sets up its own wxTranslations instance which we want to override
		wxTranslations::Set(translationsMgr.release());
	}
}

std::vector<const wxLanguageInfo*> CemuApp::GetAvailableTranslationLanguages(wxTranslations* translationsMgr)
{
	wxFileTranslationsLoader::AddCatalogLookupPathPrefix(wxHelper::FromPath(ActiveSettings::GetDataPath("resources")));
	std::vector<const wxLanguageInfo*> languages;
	for (const auto& langName : translationsMgr->GetAvailableTranslations("cemu"))
	{
		const auto* langInfo = wxLocale::FindLanguageInfo(langName);
		if (langInfo)
			languages.emplace_back(langInfo);
	}
	return languages;
}

bool CemuApp::CheckMLCPath(const fs::path& mlc)
{
	std::error_code ec;
	if (!fs::exists(mlc, ec))
		return false;
	if (!fs::exists(mlc / "usr", ec) || !fs::exists(mlc / "sys", ec))
		return false;
	return true;
}

bool CemuApp::CreateDefaultMLCFiles(const fs::path& mlc)
{
	auto CreateDirectoriesIfNotExist = [](const fs::path& path)
	{
		std::error_code ec;
		if (!fs::exists(path, ec))
			return fs::create_directories(path, ec);
		return true;
	};
	// list of directories to create
	const fs::path directories[] = {
		mlc,
		mlc / "sys",
		mlc / "usr",
		mlc / "usr/title/00050000", // base
		mlc / "usr/title/0005000c", // dlc
		mlc / "usr/title/0005000e", // update
		mlc / "usr/save/00050010/1004a000/user/common/db", // Mii Maker save folders {0x500101004A000, 0x500101004A100, 0x500101004A200}
		mlc / "usr/save/00050010/1004a100/user/common/db",
		mlc / "usr/save/00050010/1004a200/user/common/db",
		mlc / "sys/title/0005001b/1005c000/content" // lang files
	};
	for(auto& path : directories)
	{
		if(!CreateDirectoriesIfNotExist(path))
			return false;
	}
	// create sys/usr folder in mlc01
	try
	{
		const auto langDir = fs::path(mlc).append("sys/title/0005001b/1005c000/content");
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
			for (sint32 i = 0; i < NCrypto::GetCountryCount(); i++)
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
		return false;
	}
	return true;
}

void CemuApp::CreateDefaultCemuFiles()
{
	// cemu directories
	try
	{
		const auto controllerProfileFolder = ActiveSettings::GetConfigPath("controllerProfiles");
		if (!fs::exists(controllerProfileFolder))
			fs::create_directories(controllerProfileFolder);

		const auto memorySearcherFolder = ActiveSettings::GetUserDataPath("memorySearcher");
		if (!fs::exists(memorySearcherFolder))
			fs::create_directories(memorySearcherFolder);
	}
	catch (const std::exception& ex)
	{
		wxString errorMsg = formatWxString(_("Couldn't create a required cemu directory or file!\n\nError: {0}"), ex.what());

#if BOOST_OS_WINDOWS
		const DWORD lastError = GetLastError();
		if (lastError != ERROR_SUCCESS)
			errorMsg << fmt::format("\n\n{}", GetSystemErrorMessage(lastError));
#endif

		wxMessageBox(errorMsg, _("Error"), wxOK | wxCENTRE | wxICON_ERROR);
		exit(0);
	}
}

void CemuApp::ActivateApp(wxActivateEvent& event)
{
	g_window_info.app_active = event.GetActive();
	event.Skip();
}


