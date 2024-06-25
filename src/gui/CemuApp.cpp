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

#if BOOST_OS_LINUX && HAS_WAYLAND
#include "gui/helpers/wxWayland.h"
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

int mainEmulatorHLE();
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

bool CemuApp::OnInit()
{
	fs::path user_data_path, config_path, cache_path, data_path;
	auto standardPaths = wxStandardPaths::Get();
	fs::path exePath(wxHelper::MakeFSPath(standardPaths.GetExecutablePath()));
#if BOOST_OS_LINUX
	// GetExecutablePath returns the AppImage's temporary mount location
	wxString appImagePath;
	if (wxGetEnv(("APPIMAGE"), &appImagePath))
		exePath = wxHelper::MakeFSPath(appImagePath);
#endif
	// Try a portable path first, if it exists.
	user_data_path = config_path = cache_path = data_path = exePath.parent_path() / "portable";
#if BOOST_OS_MACOS
	// If run from an app bundle, use its parent directory.
	fs::path appPath = exePath.parent_path().parent_path().parent_path();
	if (appPath.extension() == ".app")
		user_data_path = config_path = cache_path = data_path = appPath.parent_path() / "portable";
#endif

	if (!fs::exists(user_data_path))
	{
#if BOOST_OS_WINDOWS
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
	}

	auto failed_write_access = ActiveSettings::LoadOnce(exePath, user_data_path, config_path, cache_path, data_path);
	for (auto&& path : failed_write_access)
		wxMessageBox(formatWxString(_("Cemu can't write to {}!"), wxString::FromUTF8(_pathToUtf8(path))),
			_("Warning"), wxOK | wxCENTRE | wxICON_EXCLAMATION, nullptr);

	NetworkConfig::LoadOnce();
	g_config.Load();

	HandlePostUpdate();
	mainEmulatorHLE();

	wxInitAllImageHandlers();

	LocalizeUI();

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
	const bool first_start = !config.did_show_graphic_pack_download;

	CreateDefaultFiles(first_start);

	m_mainFrame = new MainWindow();

	if (first_start)
		m_mainFrame->ShowGettingStartedDialog();

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

void CemuApp::LocalizeUI()
{
	std::unique_ptr<wxTranslations> translationsMgr(new wxTranslations());
	m_availableTranslations = GetAvailableTranslationLanguages(translationsMgr.get());

	const sint32 configuredLanguage = GetConfig().language;
	bool isTranslationAvailable = std::any_of(m_availableTranslations.begin(), m_availableTranslations.end(),
											  [configuredLanguage](const wxLanguageInfo* info) { return info->Language == configuredLanguage; });
	if (configuredLanguage == wxLANGUAGE_DEFAULT || isTranslationAvailable)
	{
		translationsMgr->SetLanguage(static_cast<wxLanguage>(configuredLanguage));
		translationsMgr->AddCatalog("cemu");

		if (translationsMgr->IsLoaded("cemu") && wxLocale::IsAvailable(configuredLanguage))
			m_locale.Init(configuredLanguage);

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

void CemuApp::CreateDefaultFiles(bool first_start)
{
	std::error_code ec;
	fs::path mlc = ActiveSettings::GetMlcPath();
	// check for mlc01 folder missing if custom path has been set
	if (!fs::exists(mlc, ec) && !first_start)
	{
		const wxString message = formatWxString(_("Your mlc01 folder seems to be missing.\n\nThis is where Cemu stores save files, game updates and other Wii U files.\n\nThe expected path is:\n{}\n\nDo you want to create the folder at the expected path?"),
												_pathToUtf8(mlc));
		
		wxMessageDialog dialog(nullptr, message, _("Error"), wxCENTRE | wxYES_NO | wxCANCEL| wxICON_WARNING);
		dialog.SetYesNoCancelLabels(_("Yes"), _("No"), _("Select a custom path"));
		const auto dialogResult = dialog.ShowModal();
		if (dialogResult == wxID_NO)
			exit(0);
		else if(dialogResult == wxID_CANCEL)
		{
			if (!SelectMLCPath())
				return;
			mlc = ActiveSettings::GetMlcPath();
		}
		else
		{
			GetConfig().mlc_path = "";
			g_config.Save();
		}
	}

	// create sys/usr folder in mlc01
	try
	{
		const auto sysFolder = fs::path(mlc).append("sys");
		fs::create_directories(sysFolder);

		const auto usrFolder = fs::path(mlc).append("usr");
		fs::create_directories(usrFolder);
		fs::create_directories(fs::path(usrFolder).append("title/00050000")); // base
		fs::create_directories(fs::path(usrFolder).append("title/0005000c")); // dlc
		fs::create_directories(fs::path(usrFolder).append("title/0005000e")); // update

		// Mii Maker save folders {0x500101004A000, 0x500101004A100, 0x500101004A200},
		fs::create_directories(fs::path(mlc).append("usr/save/00050010/1004a000/user/common/db"));
		fs::create_directories(fs::path(mlc).append("usr/save/00050010/1004a100/user/common/db"));
		fs::create_directories(fs::path(mlc).append("usr/save/00050010/1004a200/user/common/db"));

		// lang files
		const auto langDir = fs::path(mlc).append("sys/title/0005001b/1005c000/content");
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
		wxString errorMsg = formatWxString(_("Couldn't create a required mlc01 subfolder or file!\n\nError: {0}\nTarget path:\n{1}"), ex.what(), _pathToUtf8(mlc));

#if BOOST_OS_WINDOWS
		const DWORD lastError = GetLastError();
		if (lastError != ERROR_SUCCESS)
			errorMsg << fmt::format("\n\n{}", GetSystemErrorMessage(lastError));
#endif

		wxMessageBox(errorMsg, _("Error"), wxOK | wxCENTRE | wxICON_ERROR);
		exit(0);
	}

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


bool CemuApp::TrySelectMLCPath(fs::path path)
{
	if (path.empty())
		path = ActiveSettings::GetDefaultMLCPath();

	if (!TestWriteAccess(path))
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
	
	fs::path default_path;
	if (fs::exists(_utf8ToPath(config.mlc_path.GetValue())))
		default_path = _utf8ToPath(config.mlc_path.GetValue());

	// try until users selects a valid path or aborts
	while(true)
	{
		wxDirDialog path_dialog(parent, _("Select a mlc directory"), wxHelper::FromPath(default_path), wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
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

void CemuApp::ActivateApp(wxActivateEvent& event)
{
	g_window_info.app_active = event.GetActive();
	event.Skip();
}


