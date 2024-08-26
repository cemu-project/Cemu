#include "gui/wxgui.h"
#include "gui/GeneralSettings2.h"
#include "gui/CemuApp.h"
#include "gui/helpers/wxControlObject.h"

#include "util/helpers/helpers.h"

#include "Cafe/OS/libs/snd_core/ax.h"

#include <wx/collpane.h>
#include <wx/clrpicker.h>
#include <wx/cshelp.h>
#include <wx/textdlg.h>
#include <wx/hyperlink.h>

#include "config/CemuConfig.h"
#include "config/NetworkSettings.h"

#include "audio/IAudioAPI.h"
#if BOOST_OS_WINDOWS
#include "audio/DirectSoundAPI.h"
#include "audio/XAudio27API.h"
#endif
#include "audio/CubebAPI.h"

#include "audio/IAudioInputAPI.h"

#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanRenderer.h"
#include "Cafe/Account/Account.h"

#include <boost/tokenizer.hpp>
#include "util/helpers/SystemException.h"
#include "gui/dialogs/CreateAccount/wxCreateAccountDialog.h"

#if BOOST_OS_WINDOWS
#include <VersionHelpers.h>
#endif

#include "config/LaunchSettings.h"
#include "config/ActiveSettings.h"
#include "gui/helpers/wxHelpers.h"

#include "resource/embedded/resources.h"

#include "Cafe/CafeSystem.h"
#include "Cemu/ncrypto/ncrypto.h"
#include "Cafe/TitleList/TitleList.h"
#include "wxHelper.h"

#include "util/ScreenSaver/ScreenSaver.h"

const wxString kDirectSound("DirectSound");
const wxString kXAudio27("XAudio2.7");
const wxString kXAudio2("XAudio2");
const wxString kCubeb("Cubeb");

const wxString kPropertyPersistentId("PersistentId");
const wxString kPropertyMiiName("MiiName");
const wxString kPropertyBirthday("Birthday");
const wxString kPropertyGender("Gender");
const wxString kPropertyEmail("Email");
const wxString kPropertyCountry("Country");

wxDEFINE_EVENT(wxEVT_ACCOUNTLIST_REFRESH, wxCommandEvent);

class wxDeviceDescription : public wxClientData
{
public:
	wxDeviceDescription(const IAudioAPI::DeviceDescriptionPtr& description) : m_description(description) {}
	const IAudioAPI::DeviceDescriptionPtr& GetDescription() const { return m_description; }
private:
	IAudioAPI::DeviceDescriptionPtr m_description;
};

class wxInputDeviceDescription : public wxClientData
{
public:
	wxInputDeviceDescription(const IAudioInputAPI::DeviceDescriptionPtr& description) : m_description(description) {}
	const IAudioInputAPI::DeviceDescriptionPtr& GetDescription() const { return m_description; }
private:
	IAudioInputAPI::DeviceDescriptionPtr m_description;
};

class wxVulkanUUID : public wxClientData
{
public:
	wxVulkanUUID(const VulkanRenderer::DeviceInfo& info)
		: m_device_info(info) {}
	const VulkanRenderer::DeviceInfo& GetDeviceInfo() const { return m_device_info; }

private:
	VulkanRenderer::DeviceInfo m_device_info;
};

class wxAccountData : public wxClientData
{
public:
	wxAccountData(const Account& account)
		: m_account(account) {}

	Account& GetAccount() { return m_account; }
	const Account& GetAccount() const { return m_account; }
	
private:
	Account m_account;
};

wxPanel* GeneralSettings2::AddGeneralPage(wxNotebook* notebook)
{
	auto* panel = new wxPanel(notebook);
	auto* general_panel_sizer = new wxBoxSizer(wxVERTICAL);

	{
		auto* box = new wxStaticBox(panel, wxID_ANY, _("Interface"));
		auto* box_sizer = new wxStaticBoxSizer(box, wxVERTICAL);

		{
			auto* first_row = new wxFlexGridSizer(0, 2, 0, 0);
			first_row->SetFlexibleDirection(wxBOTH);
			first_row->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);

			first_row->Add(new wxStaticText(box, wxID_ANY, _("Language"), wxDefaultPosition, wxDefaultSize, 0), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

			wxString language_choices[] = { _("Default") };
			m_language = new wxChoice(box, wxID_ANY, wxDefaultPosition, wxDefaultSize, std::size(language_choices), language_choices);
			m_language->SetSelection(0);
			m_language->SetToolTip(_("Changes the interface language of Cemu\nAvailable languages are stored in the translation directory\nA restart will be required after changing the language"));
			for (const auto& language : wxGetApp().GetLanguages())
			{
				m_language->Append(language->DescriptionNative);
			}

			first_row->Add(m_language, 0, wxALL | wxEXPAND, 5);

			box_sizer->Add(first_row, 1, wxEXPAND, 5);
		}

		{
			auto* second_row = new wxFlexGridSizer(0, 3, 0, 0);
			second_row->SetFlexibleDirection(wxBOTH);
			second_row->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);

			sint32 checkboxCount = 0;
			auto CountRowElement = [&]()
			{
				checkboxCount++;
				if(checkboxCount != 2)
					return;
				second_row->AddSpacer(10);
				checkboxCount = 0;
			};

			auto InsertEmptyRow = [&]()
			{
				while(checkboxCount != 0)
					CountRowElement();
				second_row->AddSpacer(10);
				second_row->AddSpacer(10);
				second_row->AddSpacer(10);
			};

			const int topflag = wxALIGN_CENTER_VERTICAL | wxALL;
			m_save_window_position_size = new wxCheckBox(box, wxID_ANY, _("Remember main window position"));
			m_save_window_position_size->SetToolTip(_("Restores the last known window position and size when starting Cemu"));
			second_row->Add(m_save_window_position_size, 0, topflag, 5);
			CountRowElement();
			//second_row->AddSpacer(10);
			m_save_padwindow_position_size = new wxCheckBox(box, wxID_ANY, _("Remember pad window position"));
			m_save_padwindow_position_size->SetToolTip(_("Restores the last known pad window position and size when opening it"));
			second_row->Add(m_save_padwindow_position_size, 0, topflag, 5);
			CountRowElement();

			const int botflag = wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT | wxBOTTOM;
			m_discord_presence = new wxCheckBox(box, wxID_ANY, _("Discord Presence"));
			m_discord_presence->SetToolTip(_("Enables the Discord Rich Presence feature\nYou will also need to enable it in the Discord settings itself!"));
			second_row->Add(m_discord_presence, 0, botflag, 5);
			CountRowElement();
#ifndef ENABLE_DISCORD_RPC
			m_discord_presence->Disable();
#endif
			//second_row->AddSpacer(10);
			m_fullscreen_menubar = new wxCheckBox(box, wxID_ANY, _("Fullscreen menu bar"));
			m_fullscreen_menubar->SetToolTip(_("Displays the menu bar when Cemu is running in fullscreen mode and the mouse cursor is moved to the top"));
			second_row->Add(m_fullscreen_menubar, 0, botflag, 5);
			CountRowElement();

			m_save_screenshot = new wxCheckBox(box, wxID_ANY, _("Save screenshot"));
			m_save_screenshot->SetToolTip(_("Pressing the screenshot key (F12) will save a screenshot directly to the screenshots folder"));
			second_row->Add(m_save_screenshot, 0, botflag, 5);
			CountRowElement();

			m_disable_screensaver = new wxCheckBox(box, wxID_ANY, _("Disable screen saver"));
			m_disable_screensaver->SetToolTip(_("Prevents the system from activating the screen saver or going to sleep while running a game."));
			second_row->Add(m_disable_screensaver, 0, botflag, 5);
			CountRowElement();

			// Enable/disable feral interactive gamemode
#if BOOST_OS_LINUX && defined(ENABLE_FERAL_GAMEMODE)
			m_feral_gamemode = new wxCheckBox(box, wxID_ANY, _("Enable Feral GameMode"));
			m_feral_gamemode->SetToolTip(_("Use FeralInteractive GameMode if installed."));
			second_row->Add(m_feral_gamemode, 0, botflag, 5);
			CountRowElement();
#endif

			// temporary workaround because feature crashes on macOS
#if BOOST_OS_MACOS
			m_disable_screensaver->Enable(false);
#endif

			// InsertEmptyRow();

			m_auto_update = new wxCheckBox(box, wxID_ANY, _("Automatically check for updates"));
			m_auto_update->SetToolTip(_("Automatically checks for new cemu versions on startup"));
			second_row->Add(m_auto_update, 0, botflag, 5);
			CountRowElement();

			m_receive_untested_releases = new wxCheckBox(box, wxID_ANY, _("Receive untested updates"));
			m_receive_untested_releases->SetToolTip(_("When checking for updates, include brand new and untested releases. These may contain bugs!"));
			second_row->Add(m_receive_untested_releases, 0, botflag, 5);
#if BOOST_OS_LINUX
			if (!std::getenv("APPIMAGE")) {
				m_auto_update->Disable();
			}
#endif

			box_sizer->Add(second_row, 0, wxEXPAND, 5);
		}

		general_panel_sizer->Add(box_sizer, 0, wxEXPAND | wxALL, 5);
	}

	{
		auto* outerMlcBox = new wxStaticBox(panel, wxID_ANY, _("Custom MLC path"));

		auto* box_sizer_mlc = new wxStaticBoxSizer(outerMlcBox, wxVERTICAL);
		box_sizer_mlc->Add(new wxStaticText(box_sizer_mlc->GetStaticBox(), wxID_ANY, _("You can configure a custom path for the emulated internal Wii U storage (MLC).\nThis is where Cemu stores saves, accounts and other Wii U system files."), wxDefaultPosition, wxDefaultSize, 0), 0, wxALL, 5);

		auto* mlcPathLineSizer = new wxBoxSizer(wxHORIZONTAL);

		m_mlc_path = new wxTextCtrl(outerMlcBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
		m_mlc_path->SetMinSize(wxSize(150, -1));
		m_mlc_path->SetToolTip(_("The mlc directory contains your save games and installed game update/dlc data"));

		mlcPathLineSizer->Add(m_mlc_path, 1, wxALL | wxEXPAND, 5);

		auto* changePath = new wxButton(outerMlcBox, wxID_ANY, "Change");
		changePath->Bind(wxEVT_BUTTON, &GeneralSettings2::OnMLCPathSelect, this);
		mlcPathLineSizer->Add(changePath, 0, wxALL, 5);
		if (LaunchSettings::GetMLCPath().has_value())
			changePath->Disable();

		auto* clearPath = new wxButton(outerMlcBox, wxID_ANY, "Clear custom path");
		clearPath->Bind(wxEVT_BUTTON, &GeneralSettings2::OnMLCPathClear, this);
		mlcPathLineSizer->Add(clearPath, 0, wxALL, 5);
		if (LaunchSettings::GetMLCPath().has_value() || !ActiveSettings::IsCustomMlcPath())
			clearPath->Disable();

		box_sizer_mlc->Add(mlcPathLineSizer, 0, wxEXPAND, 5);
		general_panel_sizer->Add(box_sizer_mlc, 0, wxEXPAND | wxALL, 5);
	}

	{
		auto* general_gamepath_box = new wxStaticBox(panel, wxID_ANY, _("Game Paths"));
		auto* general_gamepath_sizer = new wxStaticBoxSizer(general_gamepath_box, wxVERTICAL);

		m_game_paths = new wxListBox(general_gamepath_box, wxID_ANY);
		m_game_paths->SetMinSize(wxSize(150, 100));
		m_game_paths->SetToolTip(_("Add the root directory of your game(s). It will scan all directories in it for games"));
		general_gamepath_sizer->Add(m_game_paths, 1, wxALL | wxEXPAND, 5);

		auto* general_gamepath_buttons = new wxFlexGridSizer(0, 2, 0, 0);
		general_gamepath_buttons->SetFlexibleDirection(wxBOTH);
		general_gamepath_buttons->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);

		auto* general_gamepath_add_button = new wxButton(general_gamepath_box, wxID_ANY, _("Add"));
		general_gamepath_add_button->Bind(wxEVT_BUTTON, &GeneralSettings2::OnAddPathClicked, this);
		general_gamepath_add_button->SetToolTip(_("Adds a game path to scan for games displayed in the game list\nIf you have unpacked games, make sure to select the root folder of a game"));
		general_gamepath_buttons->Add(general_gamepath_add_button, 0, wxALL, 5);

		auto* general_gamepath_remove_button = new wxButton(general_gamepath_box, wxID_ANY, _("Remove"));
		general_gamepath_remove_button->Bind(wxEVT_BUTTON, &GeneralSettings2::OnRemovePathClicked, this);
		general_gamepath_remove_button->SetToolTip(_("Removes the currently selected game path from the game list"));
		general_gamepath_buttons->Add(general_gamepath_remove_button, 0, wxALL, 5);

		general_gamepath_sizer->Add(general_gamepath_buttons, 0, wxEXPAND, 5);
		general_panel_sizer->Add(general_gamepath_sizer, 1, wxEXPAND | wxALL, 5);
	}

	panel->SetSizerAndFit(general_panel_sizer);

	return panel;
}

wxPanel* GeneralSettings2::AddGraphicsPage(wxNotebook* notebook)
{
	// Graphics page
	auto graphics_panel = new wxPanel(notebook);
	auto graphics_panel_sizer = new wxBoxSizer(wxVERTICAL);

	{
		auto box = new wxStaticBox(graphics_panel, wxID_ANY, _("General"));
		auto box_sizer = new wxStaticBoxSizer(box, wxVERTICAL);

		auto row = new wxFlexGridSizer(0, 2, 0, 0);
		row->SetFlexibleDirection(wxBOTH);
		row->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);

		row->Add(new wxStaticText(box, wxID_ANY, _("Graphics API")), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

		sint32 api_size = 1;
		wxString choices[2] = { "OpenGL" };
		if (g_vulkan_available)
		{
			choices[1] = "Vulkan";
			api_size = 2;
		}

		m_graphic_api = new wxChoice(box, wxID_ANY, wxDefaultPosition, wxDefaultSize, api_size, choices);
		m_graphic_api->SetSelection(0);
		if (api_size > 1)
			m_graphic_api->SetToolTip(_("Select one of the available graphic back ends"));
		row->Add(m_graphic_api, 0, wxALL, 5);
		m_graphic_api->Bind(wxEVT_CHOICE, &GeneralSettings2::OnGraphicAPISelected, this);

		row->Add(new wxStaticText(box, wxID_ANY, _("Graphics Device")), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
		m_graphic_device = new wxChoice(box, wxID_ANY, wxDefaultPosition, { 230, -1 }, api_size, choices);
		m_graphic_device->SetSelection(0);
		m_graphic_device->SetToolTip(_("Select the used graphic device"));
		row->Add(m_graphic_device, 0, wxALL, 5);

		row->Add(new wxStaticText(box, wxID_ANY, _("VSync")), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
		m_vsync = new wxChoice(box, wxID_ANY, wxDefaultPosition, { 230, -1 });
		m_vsync->SetToolTip(_("Controls the vsync state"));
		row->Add(m_vsync, 0, wxALL, 5);

		box_sizer->Add(row, 0, wxEXPAND, 5);

		auto* graphic_misc_row = new wxFlexGridSizer(0, 2, 0, 0);

		m_async_compile = new wxCheckBox(box, wxID_ANY, _("Async shader compile"));
		m_async_compile->SetToolTip(_("Enables async shader and pipeline compilation. Reduces stutter at the cost of objects not rendering for a short time.\nVulkan only"));
		graphic_misc_row->Add(m_async_compile, 0, wxALL, 5);

		m_gx2drawdone_sync = new wxCheckBox(box, wxID_ANY, _("Full sync at GX2DrawDone()"));
		m_gx2drawdone_sync->SetToolTip(_("If synchronization is requested by the game, the emulated CPU will wait for the GPU to finish all operations.\nThis is more accurate behavior, but may cause lower performance"));
		graphic_misc_row->Add(m_gx2drawdone_sync, 0, wxALL, 5);

		box_sizer->Add(graphic_misc_row, 1, wxEXPAND, 5);
		graphics_panel_sizer->Add(box_sizer, 0, wxEXPAND | wxALL, 5);
	}

	{
		wxString choices[] = { _("Bilinear"), _("Bicubic"), _("Hermite"), _("Nearest Neighbor") };
		m_upscale_filter = new wxRadioBox(graphics_panel, wxID_ANY, _("Upscale filter"), wxDefaultPosition, wxDefaultSize, std::size(choices), choices, 5, wxRA_SPECIFY_COLS);
		m_upscale_filter->SetToolTip(_("Upscaling filters are used when the game resolution is smaller than the window size"));
		graphics_panel_sizer->Add(m_upscale_filter, 0, wxALL | wxEXPAND, 5);

		m_downscale_filter = new wxRadioBox(graphics_panel, wxID_ANY, _("Downscale filter"), wxDefaultPosition, wxDefaultSize, std::size(choices), choices, 5, wxRA_SPECIFY_COLS);
		m_downscale_filter->SetToolTip(_("Downscaling filters are used when the game resolution is bigger than the window size"));
		graphics_panel_sizer->Add(m_downscale_filter, 0, wxALL | wxEXPAND, 5);
	}

	{
		wxString choices[] = { _("Keep aspect ratio"), _("Stretch") };
		m_fullscreen_scaling = new wxRadioBox(graphics_panel, wxID_ANY, _("Fullscreen scaling"), wxDefaultPosition, wxDefaultSize, std::size(choices), choices, 5, wxRA_SPECIFY_COLS);
		m_fullscreen_scaling->SetToolTip(_("Controls the output aspect ratio when it doesn't match the ratio of the game"));
		graphics_panel_sizer->Add(m_fullscreen_scaling, 0, wxALL | wxEXPAND, 5);
	}

	graphics_panel->SetSizerAndFit(graphics_panel_sizer);
	return graphics_panel;
}

wxPanel* GeneralSettings2::AddAudioPage(wxNotebook* notebook)
{
	auto audio_panel = new wxPanel(notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
	auto audio_panel_sizer = new wxBoxSizer(wxVERTICAL);

	{
		auto box = new wxStaticBox(audio_panel, wxID_ANY, _("General"));
		auto box_sizer = new wxStaticBoxSizer(box, wxVERTICAL);

		auto audio_general_row = new wxFlexGridSizer(0, 3, 0, 0);
		audio_general_row->SetFlexibleDirection(wxBOTH);
		audio_general_row->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);

		audio_general_row->Add(new wxStaticText(box, wxID_ANY, _("API")), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

		m_audio_api = new wxChoice(box, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, nullptr);
		if (IAudioAPI::IsAudioAPIAvailable(IAudioAPI::DirectSound))
			m_audio_api->Append(kDirectSound);
		if (IAudioAPI::IsAudioAPIAvailable(IAudioAPI::XAudio27))
			m_audio_api->Append(kXAudio27);
		if (IAudioAPI::IsAudioAPIAvailable(IAudioAPI::XAudio2))
			m_audio_api->Append(kXAudio2);
		if (IAudioAPI::IsAudioAPIAvailable(IAudioAPI::Cubeb))
			m_audio_api->Append(kCubeb);

		m_audio_api->SetSelection(0);
		m_audio_api->SetToolTip(_("Select one of the available audio back ends"));
		audio_general_row->Add(m_audio_api, 0, wxALL, 5);

		m_audio_api->Bind(wxEVT_CHOICE, &GeneralSettings2::OnAudioAPISelected, this);

		audio_general_row->AddSpacer(0);

		audio_general_row->Add(new wxStaticText(box, wxID_ANY, _("Latency")), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
		m_audio_latency = new wxSlider(box, wxID_ANY, 2, 0, IAudioAPI::kBlockCount - 1, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
		m_audio_latency->SetToolTip(_("Controls the amount of buffered audio data\nHigher values will create a delay in audio playback, but may avoid audio problems when emulation is too slow"));
		audio_general_row->Add(m_audio_latency, 0, wxEXPAND | wxALL, 5);
		auto latency_text = new wxStaticText(box, wxID_ANY, "24ms");
		audio_general_row->Add(latency_text, 0, wxALIGN_CENTER_VERTICAL | wxALL | wxALIGN_RIGHT, 5);
		m_audio_latency->Bind(wxEVT_SLIDER, &GeneralSettings2::OnLatencySliderChanged, this, wxID_ANY, wxID_ANY, new wxControlObject(latency_text));
		m_audio_latency->Bind(wxEVT_SLIDER, &GeneralSettings2::OnAudioLatencyChanged, this);

		box_sizer->Add(audio_general_row, 1, wxEXPAND, 5);
		audio_panel_sizer->Add(box_sizer, 0, wxEXPAND | wxALL, 5);
	}

	const wxString audio_channel_choices[] = { _("Mono"), _("Stereo") , _("Surround") };
	{
		auto box = new wxStaticBox(audio_panel, wxID_ANY, _("TV"));
		auto box_sizer = new wxStaticBoxSizer(box, wxVERTICAL);

		auto audio_tv_row = new wxFlexGridSizer(0, 3, 0, 0);
		audio_tv_row->SetFlexibleDirection(wxBOTH);
		audio_tv_row->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);

		audio_tv_row->Add(new wxStaticText(box, wxID_ANY, _("Device")), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
		m_tv_device = new wxChoice(box, wxID_ANY);
		m_tv_device->SetMinSize(wxSize(300, -1));
		m_tv_device->SetToolTip(_("Select the active audio output device for Wii U TV"));
		audio_tv_row->Add(m_tv_device, 0, wxEXPAND | wxALL, 5);
		audio_tv_row->AddSpacer(0);

		m_tv_device->Bind(wxEVT_CHOICE, &GeneralSettings2::OnAudioDeviceSelected, this);

		audio_tv_row->Add(new wxStaticText(box, wxID_ANY, _("Channels")), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

		m_tv_channels = new wxChoice(box, wxID_ANY, wxDefaultPosition, wxDefaultSize, std::size(audio_channel_choices), audio_channel_choices);

		m_tv_channels->SetSelection(1); // set default to stereo
		m_tv_channels->Bind(wxEVT_CHOICE, &GeneralSettings2::OnAudioChannelsSelected, this);
		audio_tv_row->Add(m_tv_channels, 0, wxEXPAND | wxALL, 5);
		audio_tv_row->AddSpacer(0);

		audio_tv_row->Add(new wxStaticText(box, wxID_ANY, _("Volume")), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
		m_tv_volume = new wxSlider(box, wxID_ANY, 100, 0, 100, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
		audio_tv_row->Add(m_tv_volume, 0, wxEXPAND | wxALL, 5);
		auto audio_tv_volume_text = new wxStaticText(box, wxID_ANY, "100%");
		audio_tv_row->Add(audio_tv_volume_text, 0, wxALIGN_CENTER_VERTICAL | wxALL | wxALIGN_RIGHT, 5);

		m_tv_volume->Bind(wxEVT_SLIDER, &GeneralSettings2::OnSliderChangedPercent, this, wxID_ANY, wxID_ANY, new wxControlObject(audio_tv_volume_text));
		m_tv_volume->Bind(wxEVT_SLIDER, &GeneralSettings2::OnVolumeChanged, this);

		box_sizer->Add(audio_tv_row, 1, wxEXPAND, 5);
		audio_panel_sizer->Add(box_sizer, 0, wxEXPAND | wxALL, 5);
	}

	{
		auto box = new wxStaticBox(audio_panel, wxID_ANY, _("Gamepad"));
		auto box_sizer = new wxStaticBoxSizer(box, wxVERTICAL);

		auto audio_pad_row = new wxFlexGridSizer(0, 3, 0, 0);
		audio_pad_row->SetFlexibleDirection(wxBOTH);
		audio_pad_row->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);

		audio_pad_row->Add(new wxStaticText(box, wxID_ANY, _("Device")), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
		m_pad_device = new wxChoice(box, wxID_ANY, wxDefaultPosition);
		m_pad_device->SetMinSize(wxSize(300, -1));
		m_pad_device->SetToolTip(_("Select the active audio output device for Wii U GamePad"));
		audio_pad_row->Add(m_pad_device, 0, wxEXPAND | wxALL, 5);
		audio_pad_row->AddSpacer(0);

		m_pad_device->Bind(wxEVT_CHOICE, &GeneralSettings2::OnAudioDeviceSelected, this);

		const wxString audio_channel_drc_choices[] = { _("Stereo") }; // stereo for now only

		audio_pad_row->Add(new wxStaticText(box, wxID_ANY, _("Channels")), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
		m_pad_channels = new wxChoice(box, wxID_ANY, wxDefaultPosition, wxDefaultSize, std::size(audio_channel_drc_choices), audio_channel_drc_choices);

		m_pad_channels->SetSelection(0); // set default to stereo

		m_pad_channels->Bind(wxEVT_CHOICE, &GeneralSettings2::OnAudioChannelsSelected, this);
		audio_pad_row->Add(m_pad_channels, 0, wxEXPAND | wxALL, 5);
		audio_pad_row->AddSpacer(0);

		audio_pad_row->Add(new wxStaticText(box, wxID_ANY, _("Volume")), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
		m_pad_volume = new wxSlider(box, wxID_ANY, 100, 0, 100);
		audio_pad_row->Add(m_pad_volume, 0, wxEXPAND | wxALL, 5);
		auto audio_pad_volume_text = new wxStaticText(box, wxID_ANY, "100%");
		audio_pad_row->Add(audio_pad_volume_text, 0, wxALIGN_CENTER_VERTICAL | wxALL | wxALIGN_RIGHT, 5);

		m_pad_volume->Bind(wxEVT_SLIDER, &GeneralSettings2::OnSliderChangedPercent, this, wxID_ANY, wxID_ANY, new wxControlObject(audio_pad_volume_text));
		m_pad_volume->Bind(wxEVT_SLIDER, &GeneralSettings2::OnVolumeChanged, this);

		box_sizer->Add(audio_pad_row, 1, wxEXPAND, 5);
		audio_panel_sizer->Add(box_sizer, 0, wxEXPAND | wxALL, 5);
	}

	{
		auto box = new wxStaticBox(audio_panel, wxID_ANY, _("Microphone (Experimental)"));
		auto box_sizer = new wxStaticBoxSizer(box, wxVERTICAL);

		auto audio_input_row = new wxFlexGridSizer(0, 3, 0, 0);
		audio_input_row->SetFlexibleDirection(wxBOTH);
		audio_input_row->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);

		audio_input_row->Add(new wxStaticText(box, wxID_ANY, _("Device")), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
		m_input_device = new wxChoice(box, wxID_ANY, wxDefaultPosition);
		m_input_device->SetMinSize(wxSize(300, -1));
		m_input_device->SetToolTip(_("Select the active audio input device for Wii U GamePad"));
		audio_input_row->Add(m_input_device, 0, wxEXPAND | wxALL, 5);
		audio_input_row->AddSpacer(0);

		m_input_device->Bind(wxEVT_CHOICE, &GeneralSettings2::OnAudioDeviceSelected, this);

		const wxString audio_channel_drc_choices[] = { _("Mono") }; // mono for now only

		audio_input_row->Add(new wxStaticText(box, wxID_ANY, _("Channels")), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
		m_input_channels = new wxChoice(box, wxID_ANY, wxDefaultPosition, wxDefaultSize, std::size(audio_channel_drc_choices), audio_channel_drc_choices);

		m_input_channels->SetSelection(0); // set default to stereo

		m_input_channels->Bind(wxEVT_CHOICE, &GeneralSettings2::OnAudioChannelsSelected, this);
		audio_input_row->Add(m_input_channels, 0, wxEXPAND | wxALL, 5);
		audio_input_row->AddSpacer(0);

		audio_input_row->Add(new wxStaticText(box, wxID_ANY, _("Volume")), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
		m_input_volume = new wxSlider(box, wxID_ANY, 100, 0, 100);
		audio_input_row->Add(m_input_volume, 0, wxEXPAND | wxALL, 5);
		auto audio_input_volume_text = new wxStaticText(box, wxID_ANY, "100%");
		audio_input_row->Add(audio_input_volume_text, 0, wxALIGN_CENTER_VERTICAL | wxALL | wxALIGN_RIGHT, 5);

		m_input_volume->Bind(wxEVT_SLIDER, &GeneralSettings2::OnSliderChangedPercent, this, wxID_ANY, wxID_ANY, new wxControlObject(audio_input_volume_text));
		m_input_volume->Bind(wxEVT_SLIDER, &GeneralSettings2::OnVolumeChanged, this);

		box_sizer->Add(audio_input_row, 1, wxEXPAND, 5);
		audio_panel_sizer->Add(box_sizer, 0, wxEXPAND | wxALL, 5);
	}

	audio_panel->SetSizerAndFit(audio_panel_sizer);
	return audio_panel;
}

wxPanel* GeneralSettings2::AddOverlayPage(wxNotebook* notebook)
{
	auto* panel = new wxPanel(notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
	auto* panel_sizer = new wxBoxSizer(wxVERTICAL);

	const wxString positions[]{ _("Disabled"), _("Top left"), _("Top center"), _("Top right"), _("Bottom left"), _("Bottom center"), _("Bottom right") };
	const wxString text_scale[]{ "50%", "75%", "100%", "125%", "150%", "175%", "200%", "225%", "250%", "275%", "300%" };
	{
		auto box = new wxStaticBox(panel, wxID_ANY, _("Overlay"));
		auto box_sizer = new wxStaticBoxSizer(box, wxVERTICAL);

		auto position_row = new wxFlexGridSizer(1, 0, 0, 0);
		position_row->SetFlexibleDirection(wxBOTH);
		position_row->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
		{
			position_row->Add(new wxStaticText(box, wxID_ANY, _("Position")), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
			m_overlay_position = new wxChoice(box, wxID_ANY, wxDefaultPosition, wxDefaultSize, std::size(positions), positions);
			m_overlay_position->SetSelection(0);
			m_overlay_position->SetToolTip(_("Controls the overlay which displays technical information while playing"));
			position_row->Add(m_overlay_position, 0, wxALL, 5);

			position_row->AddSpacer(25);

			position_row->Add(new wxStaticText(box, wxID_ANY, _("Text Color")), 0, wxALL | wxALIGN_CENTRE_VERTICAL, 5);
			m_overlay_font_color = new wxColourPickerCtrl(box, wxID_ANY, *wxWHITE, wxDefaultPosition, wxDefaultSize, wxCLRP_SHOW_ALPHA);
			m_overlay_font_color->SetToolTip(_("Sets the text color of the overlay"));
			position_row->Add(m_overlay_font_color, 0, wxEXPAND | wxALL, 5);

			position_row->AddSpacer(25);

			position_row->Add(new wxStaticText(box, wxID_ANY, _("Scale")), 0, wxALL | wxALIGN_CENTRE_VERTICAL, 5);
			m_overlay_scale = new wxChoice(box, wxID_ANY, wxDefaultPosition, wxDefaultSize, std::size(text_scale), text_scale);
			m_overlay_scale->SetToolTip(_("Sets the scale of the overlay text"));
			position_row->Add(m_overlay_scale, 0, wxEXPAND | wxALL, 5);
		}
		box_sizer->Add(position_row, 0, wxEXPAND, 5);

		auto settings2_row = new wxFlexGridSizer(0, 4, 2, 0);
		{
			m_overlay_fps = new wxCheckBox(box, wxID_ANY, _("FPS"));
			m_overlay_fps->SetToolTip(_("The number of frames per second. Average over last 5 seconds"));
			settings2_row->Add(m_overlay_fps, 0, wxALL, 5);

			m_overlay_drawcalls = new wxCheckBox(box, wxID_ANY, _("Draw calls per frame"));
			m_overlay_drawcalls->SetToolTip(_("The number of draw calls per frame. Average over last 5 seconds"));
			settings2_row->Add(m_overlay_drawcalls, 0, wxALL, 5);

			m_overlay_cpu = new wxCheckBox(box, wxID_ANY, _("CPU usage"));
			m_overlay_cpu->SetToolTip(_("CPU usage of Cemu in percent"));
			settings2_row->Add(m_overlay_cpu, 0, wxALL, 5);

			m_overlay_cpu_per_core = new wxCheckBox(box, wxID_ANY, _("CPU per core usage"));
			m_overlay_cpu_per_core->SetToolTip(_("Total cpu usage in percent for each core"));
			settings2_row->Add(m_overlay_cpu_per_core, 0, wxALL, 5);

			m_overlay_ram = new wxCheckBox(box, wxID_ANY, _("RAM usage"));
			m_overlay_ram->SetToolTip(_("Cemu RAM usage in MB"));
			settings2_row->Add(m_overlay_ram, 0, wxALL, 5);

			m_overlay_vram = new wxCheckBox(box, wxID_ANY, _("VRAM usage"));
#if BOOST_OS_WINDOWS
			using RtlGetVersion_t = LONG(WINAPI*)(PRTL_OSVERSIONINFOW lpVersionInformation);
			const auto pRtlGetVersion = (RtlGetVersion_t)GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlGetVersion");
			//if(IsWindows8Point1OrGreater()) requires manifest
			RTL_OSVERSIONINFOW info{};
			// Windows 8.1 	6.3*
			if (pRtlGetVersion && pRtlGetVersion(&info) == 0 && ((info.dwMajorVersion == 6 && info.dwMinorVersion >= 3) || info.dwMajorVersion > 6))
				m_overlay_vram->SetToolTip(_("The VRAM usage of Cemu in MB"));
			else
			{
				m_overlay_vram->SetToolTip(_("This option requires Win8.1+"));
				m_overlay_vram->Disable();
			}
#else
			m_overlay_vram->SetToolTip(_("The VRAM usage of Cemu in MB"));
#endif

			settings2_row->Add(m_overlay_vram, 0, wxALL, 5);

			m_overlay_debug = new wxCheckBox(box, wxID_ANY, _("Debug"));
			m_overlay_debug->SetToolTip(_("Displays internal debug information (Vulkan only)"));
			settings2_row->Add(m_overlay_debug, 0, wxALL, 5);
		}
		box_sizer->Add(settings2_row, 0, wxEXPAND, 5);

		panel_sizer->Add(box_sizer, 0, wxEXPAND | wxALL, 5);
	}

	{
		auto box = new wxStaticBox(panel, wxID_ANY, _("Notifications"));
		auto box_sizer = new wxStaticBoxSizer(box, wxVERTICAL);

		auto position_row = new wxFlexGridSizer(1, 0, 0, 0);
		position_row->SetFlexibleDirection(wxBOTH);
		position_row->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
		{
			position_row->Add(new wxStaticText(box, wxID_ANY, _("Position")), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

			m_notification_position = new wxChoice(box, wxID_ANY, wxDefaultPosition, wxDefaultSize, std::size(positions), positions);
			m_notification_position->SetSelection(0);
			m_notification_position->SetToolTip(_("Controls the notification position while playing"));
			position_row->Add(m_notification_position, 0, wxALL, 5);

			position_row->AddSpacer(25);

			position_row->Add(new wxStaticText(box, wxID_ANY, _("Text Color")), 0, wxALL | wxALIGN_CENTRE_VERTICAL, 5);
			m_notification_font_color = new wxColourPickerCtrl(box, wxID_ANY, *wxWHITE, wxDefaultPosition, wxDefaultSize, wxCLRP_SHOW_ALPHA);
			m_notification_font_color->SetToolTip(_("Sets the text color of notifications"));
			position_row->Add(m_notification_font_color, 0, wxEXPAND | wxALL, 5);

			position_row->Add(new wxStaticText(box, wxID_ANY, _("Scale")), 0, wxALL | wxALIGN_CENTRE_VERTICAL, 5);
			m_notification_scale = new wxChoice(box, wxID_ANY, wxDefaultPosition, wxDefaultSize, std::size(text_scale), text_scale);
			m_notification_scale->SetToolTip(_("Sets the scale of the notification text"));
			position_row->Add(m_notification_scale, 0, wxEXPAND | wxALL, 5);
		}
		box_sizer->Add(position_row, 0, wxEXPAND, 5);

		auto settings1_row = new wxFlexGridSizer(1, 0, 2, 0);
		{
			m_controller_profile_name = new wxCheckBox(box, wxID_ANY, _("Controller profiles"));
			m_controller_profile_name->SetToolTip(_("Displays the active controller profile when starting a game"));
			settings1_row->Add(m_controller_profile_name, 0, wxALL, 5);

			m_controller_low_battery = new wxCheckBox(box, wxID_ANY, _("Low battery"));
			m_controller_low_battery->SetToolTip(_("Shows a notification when a low controller battery has been detected"));
			settings1_row->Add(m_controller_low_battery, 0, wxALL, 5);

			m_shader_compiling = new wxCheckBox(box, wxID_ANY, _("Shader compiler"));
			m_shader_compiling->SetToolTip(_("Shows a notification after shaders have been compiled"));
			settings1_row->Add(m_shader_compiling, 0, wxALL, 5);

			m_friends_data = new wxCheckBox(box, wxID_ANY, _("Friend list"));
			m_friends_data->SetToolTip(_("Shows friend list related data if online"));
			settings1_row->Add(m_friends_data, 0, wxALL, 5);
		}
		box_sizer->Add(settings1_row, 0, wxEXPAND, 5);


		panel_sizer->Add(box_sizer, 0, wxEXPAND | wxALL, 5);
	}

	panel->SetSizerAndFit(panel_sizer);

	return panel;
}

wxPanel* GeneralSettings2::AddAccountPage(wxNotebook* notebook)
{
	auto* online_panel = new wxPanel(notebook);
	auto* online_panel_sizer = new wxBoxSizer(wxVERTICAL);

	{
		auto* box = new wxStaticBox(online_panel, wxID_ANY, _("Account settings"));
		auto* box_sizer = new wxStaticBoxSizer(box, wxVERTICAL);

		auto* content = new wxFlexGridSizer(0, 4, 0, 0);
		content->SetFlexibleDirection(wxBOTH);
		content->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
		content->AddGrowableCol(1, 1);
		content->AddGrowableCol(2, 0);
		content->AddGrowableCol(3, 0);

		content->Add(new wxStaticText(box, wxID_ANY, _("Active account")), 1, wxALIGN_CENTER_VERTICAL | wxALL, 5);

		m_active_account = new wxChoice(box, wxID_ANY);
		m_active_account->SetMinSize({ 250, -1 });
		content->Add(m_active_account, 0, wxEXPAND | wxALL, 5);
		m_active_account->Bind(wxEVT_CHOICE, &GeneralSettings2::OnActiveAccountChanged, this);

		m_create_account = new wxButton(box, wxID_ANY, _("Create"));
		content->Add(m_create_account, 0, wxEXPAND | wxALL | wxALIGN_RIGHT, 5);
		m_create_account->Bind(wxEVT_BUTTON, &GeneralSettings2::OnAccountCreate, this);

		m_delete_account = new wxButton(box, wxID_ANY, _("Delete"));
		content->Add(m_delete_account, 0, wxEXPAND | wxALL | wxALIGN_RIGHT, 5);
		m_delete_account->Bind(wxEVT_BUTTON, &GeneralSettings2::OnAccountDelete, this);

		box_sizer->Add(content, 1, wxEXPAND, 5);

		online_panel_sizer->Add(box_sizer, 0, wxEXPAND | wxALL, 5);

		if (CafeSystem::IsTitleRunning())
		{
			m_active_account->Enable(false);
			m_create_account->Enable(false);
			m_delete_account->Enable(false);
		}
	}


	{
		wxString choices[] = { _("Offline"),  _("Nintendo"), _("Pretendo"), _("Custom") };
		m_active_service = new wxRadioBox(online_panel, wxID_ANY, _("Network Service"), wxDefaultPosition, wxDefaultSize, std::size(choices), choices, 4, wxRA_SPECIFY_COLS);
		if (!NetworkConfig::XMLExists())
			m_active_service->Enable(3, false);

		m_active_service->SetItemToolTip(0, _("Online functionality disabled for this account"));
		m_active_service->SetItemToolTip(1, _("Connect to the official Nintendo Network Service"));
		m_active_service->SetItemToolTip(2, _("Connect to the Pretendo Network Service"));
		m_active_service->SetItemToolTip(3, _("Connect to a custom Network Service (configured via network_services.xml)"));

		m_active_service->Bind(wxEVT_RADIOBOX, &GeneralSettings2::OnAccountServiceChanged,this);
		online_panel_sizer->Add(m_active_service, 0, wxEXPAND | wxALL, 5);

		if (CafeSystem::IsTitleRunning())
		{
			m_active_service->Enable(false);
		}
	}

	{
		auto* box = new wxStaticBox(online_panel, wxID_ANY, _("Online play requirements"));
		auto* box_sizer = new wxStaticBoxSizer(box, wxVERTICAL);

		auto* row = new wxFlexGridSizer(0, 2, 0, 0);
		row->SetFlexibleDirection(wxBOTH);
		row->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
		
		const wxImage tmp = wxBITMAP_PNG_FROM_DATA(PNG_ERROR).ConvertToImage();
		m_validate_online = new wxBitmapButton(box, wxID_ANY, tmp.Scale(16, 16));
		m_validate_online->Bind(wxEVT_BUTTON, &GeneralSettings2::OnShowOnlineValidator, this);
		row->Add(m_validate_online, 0, wxEXPAND | wxALL, 5);

		m_online_status = new wxStaticText(box, wxID_ANY, _("No account selected"));
		row->Add(m_online_status, 1, wxALL | wxALIGN_CENTRE_VERTICAL, 5);

		box_sizer->Add(row, 1, wxEXPAND, 5);
		
		auto* tutorial_link = new wxHyperlinkCtrl(box, wxID_ANY, _("Online play tutorial"), "https://cemu.info/online-guide");
		box_sizer->Add(tutorial_link, 0, wxALL, 5);


		online_panel_sizer->Add(box_sizer, 0, wxEXPAND | wxALL, 5);
	}

	{
		m_account_information = new wxCollapsiblePane(online_panel, wxID_ANY, _("Account information"));
		#if BOOST_OS_WINDOWS
		m_account_information->GetControlWidget()->SetBackgroundColour(*wxWHITE);
		#endif
		auto win = m_account_information->GetPane();

		auto content = new wxBoxSizer(wxVERTICAL);

		m_account_grid = new wxPropertyGrid(win, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxPG_HIDE_MARGIN | wxPG_STATIC_SPLITTER);
		m_account_grid->SetExtraStyle(wxPG_EX_HELP_AS_TOOLTIPS);
		m_account_grid->SetMinSize({ 300, -1 });
		//m_account_grid->Append(new wxPropertyCategory("Main"));

		auto* persistent_id_gprop = m_account_grid->Append(new wxStringProperty("PersistentId", kPropertyPersistentId));
		persistent_id_gprop->SetHelpString(_("The persistent id is the internal folder name used for your saves"));
		m_account_grid->SetPropertyReadOnly(persistent_id_gprop);

		m_account_grid->Append(new wxStringProperty(_("Mii name"), kPropertyMiiName))->SetHelpString(_("The mii name is the profile name"));
		m_account_grid->Append(new wxStringProperty(_("Birthday"), kPropertyBirthday));

		wxPGChoices gender;
		gender.Add(_("Female"), 0);
		gender.Add(_("Male"), 1);
		m_account_grid->Append(new wxEnumProperty(_("Gender"), kPropertyGender, gender));

		m_account_grid->Append(new wxStringProperty(_("Email"), kPropertyEmail));

		wxPGChoices countries;
		for (int i = 0; i < NCrypto::GetCountryCount(); ++i)
		{
			const auto country = NCrypto::GetCountryAsString(i);
			if (country && (i == 0 || !boost::equals(country, "NN")))
			{
				countries.Add(country, i);
			}
		}
		m_account_grid->Append(new wxEnumProperty(_("Country"), kPropertyCountry, countries));

		m_account_grid->Bind(wxEVT_PG_CHANGED, &GeneralSettings2::OnAccountSettingsChanged, this);

		content->Add(m_account_grid, 1, wxEXPAND | wxALL, 5);

		win->SetSizer(content);
		content->SetSizeHints(win);

		online_panel_sizer->Add(m_account_information, 0, wxEXPAND | wxALL, 5);
	}

	online_panel->SetSizerAndFit(online_panel_sizer);
	return online_panel;
}

wxPanel* GeneralSettings2::AddDebugPage(wxNotebook* notebook)
{
	auto* panel = new wxPanel(notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
	auto* debug_panel_sizer = new wxBoxSizer(wxVERTICAL);

	{
		auto* debug_row = new wxFlexGridSizer(0, 2, 0, 0);
		debug_row->SetFlexibleDirection(wxBOTH);
		debug_row->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);

		debug_row->Add(new wxStaticText(panel, wxID_ANY, _("Crash dump"), wxDefaultPosition, wxDefaultSize, 0), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

#if BOOST_OS_WINDOWS
		wxString dump_choices[] = {_("Disabled"), _("Lite"), _("Full")};
#elif BOOST_OS_UNIX
		wxString dump_choices[] = {_("Disabled"), _("Enabled")};
#endif
		m_crash_dump = new wxChoice(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, std::size(dump_choices), dump_choices);
		m_crash_dump->SetSelection(0);
#if BOOST_OS_WINDOWS
		m_crash_dump->SetToolTip(_("Creates a dump when Cemu crashes\nOnly enable when requested by a developer!\nThe Full option will create a very large dump file (includes a full RAM dump of the Cemu process)"));
#elif BOOST_OS_UNIX
		m_crash_dump->SetToolTip(_("Creates a core dump when Cemu crashes\nOnly enable when requested by a developer!"));
#endif
		debug_row->Add(m_crash_dump, 0, wxALL | wxEXPAND, 5);
		debug_panel_sizer->Add(debug_row, 0, wxALL | wxEXPAND, 5);
	}

	{
		auto* debug_row = new wxFlexGridSizer(0, 2, 0, 0);
		debug_row->SetFlexibleDirection(wxBOTH);
		debug_row->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);

		debug_row->Add(new wxStaticText(panel, wxID_ANY, _("GDB Stub port"), wxDefaultPosition, wxDefaultSize, 0), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

		m_gdb_port = new wxSpinCtrl(panel, wxID_ANY, "1337", wxDefaultPosition, wxDefaultSize, 0, 1000, 65535);
		m_gdb_port->SetToolTip(_("Changes the port that the GDB stub will use, which you can use by either starting Cemu with the --enable-gdbstub option or by enabling it the Debug tab."));

		debug_row->Add(m_gdb_port, 0, wxALL | wxEXPAND, 5);
		debug_panel_sizer->Add(debug_row, 0, wxALL | wxEXPAND, 5);
	}

	panel->SetSizerAndFit(debug_panel_sizer);

	return panel;
}

GeneralSettings2::GeneralSettings2(wxWindow* parent, bool game_launched)
	: wxDialog(parent, wxID_ANY, _("General settings"), wxDefaultPosition, wxDefaultSize, wxCLOSE_BOX | wxCLIP_CHILDREN | wxCAPTION | wxRESIZE_BORDER), m_game_launched(game_launched)
{
	SetIcon(wxICON(X_SETTINGS));

	auto* sizer = new wxBoxSizer(wxVERTICAL);
	auto* notebook = new wxNotebook(this, wxID_ANY);

	notebook->AddPage(AddGeneralPage(notebook), _("General"));
	notebook->AddPage(AddGraphicsPage(notebook), _("Graphics"));
	notebook->AddPage(AddAudioPage(notebook), _("Audio"));	
	notebook->AddPage(AddOverlayPage(notebook), _("Overlay"));
	notebook->AddPage(AddAccountPage(notebook), _("Account"));
	notebook->AddPage(AddDebugPage(notebook), _("Debug"));

	Bind(wxEVT_CLOSE_WINDOW, &GeneralSettings2::OnClose, this);

	// 

	sizer->Add(notebook, 1, wxEXPAND | wxALL, 5);

	SetSizerAndFit(sizer);
	Layout();
	Centre(wxBOTH);

	//

	UpdateOnlineAccounts();
	UpdateAudioDeviceList();

	ApplyConfig();
	HandleGraphicsApiSelection();
	
	DisableSettings(game_launched);
}

uint32 GeneralSettings2::GetSelectedAccountPersistentId()
{
	const auto active_account = m_active_account->GetSelection();
	if (active_account == wxNOT_FOUND)
		return GetConfig().account.m_persistent_id.GetInitValue();
	return dynamic_cast<wxAccountData*>(m_active_account->GetClientObject(active_account))->GetAccount().GetPersistentId();
}

void GeneralSettings2::StoreConfig() 
{
	auto* app = (CemuApp*)wxTheApp;
	auto& config = GetConfig();

	config.use_discord_presence = m_discord_presence->IsChecked();
	config.fullscreen_menubar = m_fullscreen_menubar->IsChecked();
	config.check_update = m_auto_update->IsChecked();
	config.save_screenshot = m_save_screenshot->IsChecked();
#if BOOST_OS_LINUX && defined(ENABLE_FERAL_GAMEMODE)
    config.feral_gamemode = m_feral_gamemode->IsChecked();
#endif
	config.disable_screensaver = m_disable_screensaver->IsChecked();
	// Toggle while a game is running
	if (CafeSystem::IsTitleRunning())
	{
		ScreenSaver::SetInhibit(config.disable_screensaver);
	}
	
	// -1 is default wx widget value -> set to dummy 0 so mainwindow and padwindow will update it
	config.window_position = m_save_window_position_size->IsChecked() ? Vector2i{ 0,0 } : Vector2i{-1,-1};
	config.window_size = m_save_window_position_size->IsChecked() ? Vector2i{ 0,0 } : Vector2i{-1,-1};
	config.pad_position = m_save_padwindow_position_size->IsChecked() ? Vector2i{ 0,0 } : Vector2i{-1,-1};
	config.pad_size = m_save_padwindow_position_size->IsChecked() ? Vector2i{ 0,0 } : Vector2i{-1,-1};

	config.game_paths.clear();
	for (auto& path : m_game_paths->GetStrings())
		config.game_paths.emplace_back(path.utf8_string());

	auto selection = m_language->GetSelection();
	if (selection == 0)
		GetConfig().language = wxLANGUAGE_DEFAULT;
	else
	{
		const auto language = m_language->GetStringSelection();
		for (const auto& lang : app->GetLanguages())
		{
			if (lang->DescriptionNative == language)
			{
				GetConfig().language = lang->Language;
				break;
			}
		}
	}

	// audio
	if (m_audio_api->GetStringSelection() == kDirectSound)
		config.audio_api = IAudioAPI::DirectSound;
	else if (m_audio_api->GetStringSelection() == kXAudio27)
		config.audio_api = IAudioAPI::XAudio27;
	else if (m_audio_api->GetStringSelection() == kXAudio2)
		config.audio_api = IAudioAPI::XAudio2;
	else if (m_audio_api->GetStringSelection() == kCubeb)
		config.audio_api = IAudioAPI::Cubeb;

	config.audio_delay = m_audio_latency->GetValue();
	config.tv_channels = (AudioChannels)m_tv_channels->GetSelection();
	//config.pad_channels =  (AudioChannels)m_pad_channels->GetSelection();
	config.pad_channels = kStereo; // (AudioChannels)m_pad_channels->GetSelection();
	//config.input_channels =  (AudioChannels)m_input_channels->GetSelection();
	config.input_channels = kMono; // (AudioChannels)m_input_channels->GetSelection();
	
	config.tv_volume = m_tv_volume->GetValue();
	config.pad_volume = m_pad_volume->GetValue();
	config.input_volume = m_input_volume->GetValue();

	config.tv_device.clear();
	const auto tv_device = m_tv_device->GetSelection();
	if (tv_device != wxNOT_FOUND && tv_device != 0 && m_tv_device->HasClientObjectData())
	{
		const auto* device_description = (wxDeviceDescription*)m_tv_device->GetClientObject(tv_device);
		if(device_description)
			config.tv_device = device_description->GetDescription()->GetIdentifier();
	}

	config.pad_device.clear();
	const auto pad_device = m_pad_device->GetSelection();
	if (pad_device != wxNOT_FOUND && pad_device != 0 && m_pad_device->HasClientObjectData())
	{
		const auto* device_description = (wxDeviceDescription*)m_pad_device->GetClientObject(pad_device);
		if (device_description)
			config.pad_device = device_description->GetDescription()->GetIdentifier();
	}

	config.input_device = L"";
	const auto input_device = m_input_device->GetSelection();
	if (input_device != wxNOT_FOUND && input_device != 0 && m_input_device->HasClientObjectData())
	{
		const auto* device_description = (wxDeviceDescription*)m_input_device->GetClientObject(input_device);
		if (device_description)
			config.input_device = device_description->GetDescription()->GetIdentifier();
	}

	// graphics
	config.graphic_api = (GraphicAPI)m_graphic_api->GetSelection();

	selection = m_graphic_device->GetSelection();
	if(selection != wxNOT_FOUND)
	{
		const auto* info = (wxVulkanUUID*)m_graphic_device->GetClientObject(selection);
		if(info)
			config.graphic_device_uuid = info->GetDeviceInfo().uuid;
		else
			config.graphic_device_uuid = {};
	}
	else
		config.graphic_device_uuid = {};
	

	config.vsync = m_vsync->GetSelection();
	config.gx2drawdone_sync = m_gx2drawdone_sync->IsChecked();
	config.async_compile = m_async_compile->IsChecked();
	
	config.upscale_filter = m_upscale_filter->GetSelection();
	config.downscale_filter = m_downscale_filter->GetSelection();
	config.fullscreen_scaling = m_fullscreen_scaling->GetSelection();
	
	config.overlay.position = (ScreenPosition)m_overlay_position->GetSelection(); wxASSERT((int)config.overlay.position <= (int)ScreenPosition::kBottomRight);
	config.overlay.text_color = m_overlay_font_color->GetColour().GetRGBA();
	config.overlay.text_scale = m_overlay_scale->GetSelection() * 25 + 50;

	config.overlay.fps = m_overlay_fps->GetValue();
	config.overlay.drawcalls = m_overlay_drawcalls->GetValue();
	config.overlay.cpu_usage = m_overlay_cpu->GetValue();
	config.overlay.cpu_per_core_usage = m_overlay_cpu_per_core->GetValue();
	config.overlay.ram_usage = m_overlay_ram->GetValue();
	config.overlay.vram_usage = m_overlay_vram->GetValue();
	config.overlay.debug = m_overlay_debug->GetValue();

	config.notification.position = (ScreenPosition)m_notification_position->GetSelection(); wxASSERT((int)config.notification.position <= (int)ScreenPosition::kBottomRight);
	config.notification.text_color = m_notification_font_color->GetColour().GetRGBA();
	config.notification.text_scale = m_notification_scale->GetSelection() * 25 + 50;
	config.notification.controller_profiles = m_controller_profile_name->GetValue();
	config.notification.controller_battery = m_controller_low_battery->GetValue();
	config.notification.shader_compiling = m_shader_compiling->GetValue();
	config.notification.friends = m_friends_data->GetValue();

	// account
	config.account.m_persistent_id = GetSelectedAccountPersistentId();

	// debug
	config.crash_dump = (CrashDump)m_crash_dump->GetSelection();
	config.gdb_port = m_gdb_port->GetValue();

	g_config.Save();
}

GeneralSettings2::~GeneralSettings2()
{
	Unbind(wxEVT_CLOSE_WINDOW, &GeneralSettings2::OnClose, this);
}

void GeneralSettings2::OnClose(wxCloseEvent& event)
{
	StoreConfig();
	if (m_has_account_change)
	{
		wxCommandEvent refresh_event(wxEVT_ACCOUNTLIST_REFRESH);
		GetParent()->ProcessWindowEvent(refresh_event);
	}
	event.Skip();
}

void GeneralSettings2::ValidateConfig()
{
	g_config.Load();

	auto& data = g_config.data();
	// todo
	//data.fullscreen_scaling = min(max(data.fullscreen_scaling,))
}

void GeneralSettings2::DisableSettings(bool game_launched)
{
	
}

void GeneralSettings2::OnAudioLatencyChanged(wxCommandEvent& event)
{
	IAudioAPI::s_audioDelay = event.GetInt();
	event.Skip();
}

void GeneralSettings2::OnVolumeChanged(wxCommandEvent& event)
{
	
	if(event.GetEventObject() == m_input_volume)
	{
		std::shared_lock lock(g_audioInputMutex);
		if (g_inputAudio)
			g_inputAudio->SetVolume(event.GetInt());
	}
	else
	{
		std::shared_lock lock(g_audioMutex);
		if(event.GetEventObject() == m_pad_volume)
		{
			if (g_padAudio)
			{
				g_padAudio->SetVolume(event.GetInt());
				g_padVolume = event.GetInt();
			}
		}
		else
		{
			if (g_tvAudio)
				g_tvAudio->SetVolume(event.GetInt());
		}
	}
	

	event.Skip();
}

void GeneralSettings2::OnInputVolumeChanged(wxCommandEvent& event)
{
	std::shared_lock lock(g_audioMutex);
	if (g_padAudio)
	{
		g_padAudio->SetInputVolume(event.GetInt());
		g_padVolume = event.GetInt();
	}
		
	event.Skip();
}

void GeneralSettings2::OnSliderChangedPercent(wxCommandEvent& event)
{
	const auto slider = dynamic_cast<wxSlider*>(event.GetEventObject());
	wxASSERT(slider);

	const auto control = dynamic_cast<wxControlObject*>(event.GetEventUserData());
	wxASSERT(control);

	auto slider_text = control->GetControl<wxStaticText>();
	wxASSERT(slider_text);

	const auto value = event.GetInt();
	slider->SetValue(value);
	slider_text->SetLabel(wxString::Format("%d%%", value));

	event.Skip();
}

void GeneralSettings2::OnLatencySliderChanged(wxCommandEvent& event)
{
	const auto slider = dynamic_cast<wxSlider*>(event.GetEventObject());
	wxASSERT(slider);

	const auto control = dynamic_cast<wxControlObject*>(event.GetEventUserData());
	wxASSERT(control);

	auto slider_text = control->GetControl<wxStaticText>();
	wxASSERT(slider_text);

	const auto value = event.GetInt();
	slider->SetValue(value);
	slider_text->SetLabel(wxString::Format("%dms", value * 12));

	event.Skip();
}

void GeneralSettings2::UpdateAudioDeviceList()
{
	m_tv_device->Clear();
	m_pad_device->Clear();
	m_input_device->Clear();

	m_tv_device->Append(_("Disabled"));
	m_pad_device->Append(_("Disabled"));
	m_input_device->Append(_("Disabled"));

	const auto audio_api = (IAudioAPI::AudioAPI)GetConfig().audio_api;
	const auto devices = IAudioAPI::GetDevices(audio_api);
	for (auto& device : devices)
	{
		m_tv_device->Append(device->GetName(), new wxDeviceDescription(device));
		m_pad_device->Append(device->GetName(), new wxDeviceDescription(device));
	}

	const auto input_audio_api = IAudioInputAPI::Cubeb; //(IAudioAPI::AudioAPI)GetConfig().input_audio_api;
	const auto input_devices = IAudioInputAPI::GetDevices(input_audio_api);

	for (auto& device : input_devices)
	{
		m_input_device->Append(device->GetName(), new wxInputDeviceDescription(device));
	}

	if(m_tv_device->GetCount() > 1)
		m_tv_device->SetSelection(1);
	else
		m_tv_device->SetSelection(0);

	m_pad_device->SetSelection(0);

	m_input_device->SetSelection(0);

	// todo reset global instance of audio device
}

void GeneralSettings2::ResetAccountInformation() 
{
	m_account_grid->SetSplitterPosition(100);
	m_active_account->SetSelection(0);

	for(auto it = m_account_grid->GetIterator(); !it.AtEnd(); ++it)
	{
		(*it)->SetValueToUnspecified();
	}

	// refresh pane size
	m_account_information->InvalidateBestSize();
	#if BOOST_OS_WINDOWS
	m_account_information->OnStateChange(GetBestSize());
	#endif
}

void GeneralSettings2::OnAccountCreate(wxCommandEvent& event)
{
	wxASSERT(Account::HasFreeAccountSlots());

	wxCreateAccountDialog dialog(this);
	if (dialog.ShowModal() == wxID_CANCEL)
		return;

	Account account(dialog.GetPersistentId(), dialog.GetMiiName().ToStdWstring());
	account.Save();
	Account::RefreshAccounts();
	
	const int index = m_active_account->Append(account.ToString(), new wxAccountData(account));

	// update ui
	m_active_account->SetSelection(index);
	UpdateAccountInformation();

	m_create_account->Enable(m_active_account->GetCount() < 0xC);
	m_delete_account->Enable(m_active_account->GetCount() > 1);
	
	// send main window event
	wxASSERT(GetParent());
	wxCommandEvent refresh_event(wxEVT_ACCOUNTLIST_REFRESH);
	GetParent()->ProcessWindowEvent(refresh_event);
}

void GeneralSettings2::OnAccountDelete(wxCommandEvent& event)
{
	if(m_active_account->GetCount() == 1)
	{
		wxMessageBox(_("Can't delete the only account!"), _("Error"), wxOK | wxCENTRE | wxICON_ERROR, this);
		return;
	}

	const auto selection = m_active_account->GetSelection();
	wxASSERT(selection != wxNOT_FOUND);
	auto* obj = dynamic_cast<wxAccountData*>(m_active_account->GetClientObject(selection));
	wxASSERT(obj);
	auto& account = obj->GetAccount();

	const std::wstring format_str = _("Are you sure you want to delete the account {} with id {:x}?").ToStdWstring();
	const std::wstring msg = fmt::format(fmt::runtime(format_str),
	                                     std::wstring{ account.GetMiiName() }, account.GetPersistentId());

	const int answer = wxMessageBox(msg, _("Confirmation"), wxYES_NO | wxCENTRE | wxICON_QUESTION, this);
	if (answer == wxNO)
		return;

	// todo: ask if saves should be deleted too?
	
	const fs::path path = account.GetFileName();
	try
	{
		fs::remove_all(path.parent_path());
		m_active_account->Delete(selection);
		m_active_account->SetSelection(0);
		Account::RefreshAccounts();
		UpdateAccountInformation();

		m_create_account->Enable(m_active_account->GetCount() < 0xC);
		m_delete_account->Enable(m_active_account->GetCount() > 1);
	}
	catch(const std::exception& ex)
	{
		SystemException sys(ex);
		cemuLog_log(LogType::Force, sys.what());
	}
	
}

void GeneralSettings2::OnAccountSettingsChanged(wxPropertyGridEvent& event)
{
	wxPGProperty* property = event.GetProperty();
	if (!property)
		return;

	const wxAny value = property->GetValue();
	if (value.IsNull())
		return;

	const auto selection = m_active_account->GetSelection();
	wxASSERT(selection != wxNOT_FOUND);
	auto* obj = dynamic_cast<wxAccountData*>(m_active_account->GetClientObject(selection));
	wxASSERT(obj);
	auto& account = obj->GetAccount();

	// TODO make id changeable to free ids + current it?
	bool refresh_accounts = false;
	if (property->GetName() == kPropertyMiiName)
	{
		std::wstring new_name = value.As<wxString>().ToStdWstring();
		if (new_name.empty())
			new_name = L"default";

		account.SetMiiName(new_name);
		refresh_accounts = true;
	}
	else if (property->GetName() == kPropertyBirthday)
	{
		const std::string birthday = value.As<wxString>().ToStdString();
		const boost::char_separator<char> sep{ "-" };

		std::vector<std::string> tokens;
		for (const auto& token : boost::tokenizer(birthday, sep))
		{
			tokens.emplace_back(token);
		}

		if (tokens.size() == 3)
		{
			account.SetBirthYear(ConvertString<uint16>(tokens[0]));
			account.SetBirthMonth(ConvertString<uint8>(tokens[1]));
			account.SetBirthDay(ConvertString<uint8>(tokens[2]));
		}
	}
	else if (property->GetName() == kPropertyGender)
	{
		account.SetGender(value.As<int>());
	}
	else if (property->GetName() == kPropertyEmail)
	{
		account.SetEmail(value.As<wxString>().ToStdString());
		
	}
	else if (property->GetName() == kPropertyCountry)
	{
		account.SetCountry(value.As<int>());
	}
	else
		cemu_assert_debug(false);
	
	account.Save();
	Account::RefreshAccounts(); // refresh internal account list
	UpdateAccountInformation(); // refresh on invalid values

	if(refresh_accounts)
	{
		wxCommandEvent refresh_event(wxEVT_ACCOUNTLIST_REFRESH);
		GetParent()->ProcessWindowEvent(refresh_event);
	}
}

void GeneralSettings2::UpdateAccountInformation()
{
	m_account_grid->SetSplitterPosition(100);

	const auto selection = m_active_account->GetSelection();
	if(selection == wxNOT_FOUND)
	{
		m_validate_online->SetBitmap(wxBITMAP_PNG_FROM_DATA(PNG_ERROR).ConvertToImage().Scale(16, 16));
		m_validate_online->SetWindowStyleFlag(m_validate_online->GetWindowStyleFlag() & ~wxBORDER_NONE);
		ResetAccountInformation();
		m_online_status->SetLabel(_("No account selected"));
		return;
	}

	const auto* obj = dynamic_cast<wxAccountData*>(m_active_account->GetClientObject(selection));
	wxASSERT(obj);
	const auto& account = obj->GetAccount();

	m_active_account->SetString(selection, account.ToString());

	m_account_grid->GetProperty(kPropertyPersistentId)->SetValueFromString(fmt::format("{:x}", account.GetPersistentId()));
	m_account_grid->GetProperty(kPropertyMiiName)->SetValueFromString(std::wstring{ account.GetMiiName() });
	m_account_grid->GetProperty(kPropertyBirthday)->SetValueFromString(fmt::format("{:04d}-{:02d}-{:02d}", account.GetBirthYear(), account.GetBirthMonth(), account.GetBirthDay()));

	const auto gender_property = m_account_grid->GetProperty(kPropertyGender); // gender 2 can be also female?
	gender_property->SetChoiceSelection(std::min(gender_property->GetChoices().GetCount() - 1, (uint32)account.GetGender()));

	m_account_grid->GetProperty(kPropertyEmail)->SetValueFromString(std::string{ account.GetEmail() });
	
	auto* country_property = dynamic_cast<wxEnumProperty*>(m_account_grid->GetProperty(kPropertyCountry));
	wxASSERT(country_property);
	int index = (country_property)->GetIndexForValue(account.GetCountry());
	if (index == wxNOT_FOUND)
		index = 0;
	country_property->SetChoiceSelection(index);

	const bool online_fully_valid = account.IsValidOnlineAccount() && ActiveSettings::HasRequiredOnlineFiles();
	if (ActiveSettings::HasRequiredOnlineFiles())
	{
		if(account.IsValidOnlineAccount())
			m_online_status->SetLabel(_("Selected account is a valid online account"));
		else
			m_online_status->SetLabel(_("Selected account is not linked to a NNID or PNID"));
	}
	else
	{
		if(NCrypto::OTP_IsPresent() != NCrypto::SEEPROM_IsPresent())
			m_online_status->SetLabel(_("OTP.bin or SEEPROM.bin is missing"));
		else if(NCrypto::OTP_IsPresent() && NCrypto::SEEPROM_IsPresent())
			m_online_status->SetLabel(_("OTP and SEEPROM present but no certificate files were found"));
		else
			m_online_status->SetLabel(_("Online play is not set up. Follow the guide below to get started"));
	}

	if(online_fully_valid)
	{
		m_validate_online->SetBitmap(wxBITMAP_PNG_FROM_DATA(PNG_CHECK_YES).ConvertToImage().Scale(16, 16));
		m_validate_online->SetWindowStyleFlag(m_validate_online->GetWindowStyleFlag() | wxBORDER_NONE);
	}
	else
	{
		m_validate_online->SetBitmap(wxBITMAP_PNG_FROM_DATA(PNG_ERROR).ConvertToImage().Scale(16, 16));
		m_validate_online->SetWindowStyleFlag(m_validate_online->GetWindowStyleFlag() & ~wxBORDER_NONE);
	}

	// enable/disable network service field depending on online requirements
	m_active_service->Enable(online_fully_valid && !CafeSystem::IsTitleRunning());
	if(online_fully_valid)
	{
		NetworkService service = GetConfig().GetAccountNetworkService(account.GetPersistentId());
		m_active_service->SetSelection(static_cast<int>(service));
		// set the config option here for the selected service
		// this will guarantee that it's actually written to settings.xml
		// allowing us to eventually get rid of the legacy option in the (far) future
		GetConfig().SetAccountSelectedService(account.GetPersistentId(), service);
	}
	else
	{
		m_active_service->SetSelection(0); // force offline
	}
	wxString tmp = _("Network service");
	tmp.append(" (");
	tmp.append(wxString::FromUTF8(boost::nowide::narrow(account.GetMiiName())));
	tmp.append(")");
	m_active_service->SetLabel(tmp);

	// refresh pane size
	m_account_grid->InvalidateBestSize();
	//m_account_grid->GetParent()->FitInside();
	//m_account_information->OnStateChange(GetBestSize()); idk..
}

void GeneralSettings2::UpdateOnlineAccounts()
{
	m_active_account->Clear();
	for(const auto& account : Account::GetAccounts())
	{
		m_active_account->Append(fmt::format(L"{} ({:x})", std::wstring{ account.GetMiiName() }, account.GetPersistentId()),
			new wxAccountData(account));
	}

	m_active_account->SetSelection(0);
	m_create_account->Enable(m_active_account->GetCount() < 0xC);
	m_delete_account->Enable(m_active_account->GetCount() > 1);
	UpdateAccountInformation();
}

void GeneralSettings2::HandleGraphicsApiSelection()
{
	int selection = m_vsync->GetSelection();
	if(selection == wxNOT_FOUND)
		selection = GetConfig().vsync;
		
	m_vsync->Clear();
	if(m_graphic_api->GetSelection() == 0)
	{
		// OpenGL
		m_vsync->AppendString(_("Off"));
		m_vsync->AppendString(_("On"));
		if (selection == 0)
			m_vsync->Select(0);
		else
			m_vsync->Select(1);

		m_graphic_device->Clear();
		m_graphic_device->Disable();

		m_gx2drawdone_sync->Enable();
		m_async_compile->Disable();
	}
	else
	{
		// Vulkan
		m_gx2drawdone_sync->Disable();
		m_async_compile->Enable();

		m_vsync->AppendString(_("Off"));
		m_vsync->AppendString(_("Double buffering"));
		m_vsync->AppendString(_("Triple buffering"));
#if BOOST_OS_WINDOWS
		m_vsync->AppendString(_("Match emulated display (Experimental)"));
#endif

		m_vsync->Select(selection);
		
		m_graphic_device->Enable();
		auto devices = VulkanRenderer::GetDevices();
		m_graphic_device->Clear();
		if(!devices.empty())
		{
			for(const auto& device : devices)
			{
				m_graphic_device->Append(device.name, new wxVulkanUUID(device));
			}
			m_graphic_device->SetSelection(0);

			const auto& config = GetConfig();
			for(size_t i = 0; i < devices.size(); ++i)
			{
				if(config.graphic_device_uuid == devices[i].uuid)
				{
					m_graphic_device->SetSelection(i);
					break;
				}
			}
		}
	}
}

void GeneralSettings2::ApplyConfig()
{
	ValidateConfig();
	auto& config = GetConfig();

	if (LaunchSettings::GetMLCPath().has_value())
		m_mlc_path->SetValue(wxHelper::FromPath(LaunchSettings::GetMLCPath().value()));
	else
		m_mlc_path->SetValue(wxHelper::FromUtf8(config.mlc_path.GetValue()));

	m_save_window_position_size->SetValue(config.window_position != Vector2i{-1,-1});
	m_save_padwindow_position_size->SetValue(config.pad_position != Vector2i{-1,-1});

	m_discord_presence->SetValue(config.use_discord_presence);
	m_fullscreen_menubar->SetValue(config.fullscreen_menubar);

	m_auto_update->SetValue(config.check_update);
	m_receive_untested_releases->SetValue(config.receive_untested_updates);
	m_save_screenshot->SetValue(config.save_screenshot);

	m_disable_screensaver->SetValue(config.disable_screensaver);
#if BOOST_OS_LINUX && defined(ENABLE_FERAL_GAMEMODE)
    	m_feral_gamemode->SetValue(config.feral_gamemode);
#endif
	// temporary workaround because feature crashes on macOS
#if BOOST_OS_MACOS
	m_disable_screensaver->SetValue(false);
#endif

	m_game_paths->Clear();
	for (auto& path : config.game_paths)
	{
		m_game_paths->Append(to_wxString(path));
	}

	const auto app = (CemuApp*)wxTheApp;
	for (const auto& language : app->GetLanguages())
	{
		if (config.language == language->Language)
		{
			m_language->SetStringSelection(language->DescriptionNative);
			break;
		}
	}

	// graphics
	m_graphic_api->SetSelection(config.graphic_api);
	m_vsync->SetSelection(config.vsync);
	m_async_compile->SetValue(config.async_compile);
	m_gx2drawdone_sync->SetValue(config.gx2drawdone_sync);
	m_upscale_filter->SetSelection(config.upscale_filter);
	m_downscale_filter->SetSelection(config.downscale_filter);
	m_fullscreen_scaling->SetSelection(config.fullscreen_scaling);

	wxASSERT((uint32)config.overlay.position < m_overlay_position->GetCount());
	m_overlay_position->SetSelection((int)config.overlay.position);
	m_overlay_font_color->SetColour(wxColour((unsigned long)config.overlay.text_color));

	uint32 selection = (config.overlay.text_scale - 50) / 25;
	wxASSERT(selection < m_overlay_scale->GetCount());
	m_overlay_scale->SetSelection(selection);

	m_overlay_fps->SetValue(config.overlay.fps);
	m_overlay_drawcalls->SetValue(config.overlay.drawcalls);
	m_overlay_cpu->SetValue(config.overlay.cpu_usage);
	m_overlay_cpu_per_core->SetValue(config.overlay.cpu_per_core_usage);
	m_overlay_ram->SetValue(config.overlay.ram_usage);
	m_overlay_vram->SetValue(config.overlay.vram_usage);
	m_overlay_debug->SetValue(config.overlay.debug);

	wxASSERT((uint32)config.notification.position < m_notification_position->GetCount());
	m_notification_position->SetSelection((int)config.notification.position);
	m_notification_font_color->SetColour(wxColour((unsigned long)config.notification.text_color));

	selection = (config.notification.text_scale - 50) / 25;
	wxASSERT(selection < m_notification_scale->GetCount());
	m_notification_scale->SetSelection(selection);

	m_controller_profile_name->SetValue(config.notification.controller_profiles);
	m_controller_low_battery->SetValue(config.notification.controller_battery);
	m_shader_compiling->SetValue(config.notification.shader_compiling);
	m_friends_data->SetValue(config.notification.friends);

	// audio
	if(config.audio_api == IAudioAPI::DirectSound)
		m_audio_api->SetStringSelection(kDirectSound);
	else if(config.audio_api == IAudioAPI::XAudio27)
		m_audio_api->SetStringSelection(kXAudio27);
	else if(config.audio_api == IAudioAPI::XAudio2)
		m_audio_api->SetStringSelection(kXAudio2);
	else if(config.audio_api == IAudioAPI::Cubeb)
		m_audio_api->SetStringSelection(kCubeb);

	SendSliderEvent(m_audio_latency, config.audio_delay);

	m_tv_channels->SetSelection(config.tv_channels);
	//m_pad_channels->SetSelection(config.pad_channels);
	m_pad_channels->SetSelection(0);
	//m_input_channels->SetSelection(config.pad_channels);
	m_input_channels->SetSelection(0);
	
	SendSliderEvent(m_tv_volume, config.tv_volume);

	if (!config.tv_device.empty() && m_tv_device->HasClientObjectData())
	{
		for(uint32 i = 0; i < m_tv_device->GetCount(); ++i)
		{
			const auto device_description = (wxDeviceDescription*)m_tv_device->GetClientObject(i);
			if (device_description && config.tv_device == device_description->GetDescription()->GetIdentifier())
			{
				m_tv_device->SetSelection(i);
				break;
			}
		}
	}
	else
		m_tv_device->SetSelection(0);
	
	SendSliderEvent(m_pad_volume, config.pad_volume);
	if (!config.pad_device.empty() && m_pad_device->HasClientObjectData())
	{
		for (uint32 i = 0; i < m_pad_device->GetCount(); ++i)
		{
			const auto device_description = (wxDeviceDescription*)m_pad_device->GetClientObject(i);
			if (device_description && config.pad_device == device_description->GetDescription()->GetIdentifier())
			{
				m_pad_device->SetSelection(i);
				break;
			}
		}
	}
	else
		m_pad_device->SetSelection(0);

	SendSliderEvent(m_input_volume, config.input_volume);
	if (!config.input_device.empty() && m_input_device->HasClientObjectData())
	{
		for (uint32 i = 0; i < m_input_device->GetCount(); ++i)
		{
			const auto device_description = (wxInputDeviceDescription*)m_input_device->GetClientObject(i);
			if (device_description && config.input_device == device_description->GetDescription()->GetIdentifier())
			{
				m_input_device->SetSelection(i);
				break;
			}
		}
	}
	else
		m_input_device->SetSelection(0);

	// account
	UpdateOnlineAccounts();
	m_active_account->SetSelection(0);
	for(uint32 i = 0; i < m_active_account->GetCount(); ++i)
	{
		const auto* obj = dynamic_cast<wxAccountData*>(m_active_account->GetClientObject(i));
		wxASSERT(obj);
		if(obj->GetAccount().GetPersistentId() == ActiveSettings::GetPersistentId())
		{
			m_active_account->SetSelection(i);
			break;
		}
	}
	m_active_service->SetSelection((int)config.GetAccountNetworkService(ActiveSettings::GetPersistentId()));

	UpdateAccountInformation();

	// debug
	m_crash_dump->SetSelection((int)config.crash_dump.GetValue());
	m_gdb_port->SetValue(config.gdb_port.GetValue());
}

void GeneralSettings2::OnAudioAPISelected(wxCommandEvent& event)
{
	IAudioAPI::AudioAPI api;
	if (m_audio_api->GetStringSelection() == kDirectSound)
		api = IAudioAPI::DirectSound;
	else if (m_audio_api->GetStringSelection() == kXAudio27)
		api = IAudioAPI::XAudio27;
	else if (m_audio_api->GetStringSelection() == kXAudio2)
		api = IAudioAPI::XAudio2;
	else if (m_audio_api->GetStringSelection() == kCubeb)
		api = IAudioAPI::Cubeb;
	else
	{
		wxFAIL_MSG("invalid audio api selected!");
		return;
	}

	GetConfig().audio_api = api;
	UpdateAudioDeviceList();
	OnAudioDeviceSelected(event);
}

#define AX_FRAMES_PER_GROUP 4

void GeneralSettings2::UpdateAudioDevice()
{
	auto& config = GetConfig();

	std::unique_lock lock(g_audioMutex);
	std::unique_lock inputLock(g_audioInputMutex);

	// tv audio device
	{
		const auto selection = m_tv_device->GetSelection();
		if (selection == wxNOT_FOUND)
		{
			cemu_assert_debug(false);
			return;
		}

		g_tvAudio.reset();

		if (m_tv_device->HasClientObjectData())
		{
			const auto description = (wxDeviceDescription*)m_tv_device->GetClientObject(selection);
			if (description)
			{
				sint32 channels;
				if (m_game_launched && g_tvAudio)
					channels = g_tvAudio->GetChannels();
				else
				{
					switch (config.tv_channels)
					{
					case 0:
						channels = 1;
						break;
					case 2:
						channels = 6;
						break;
					default: // stereo
						channels = 2;
						break;
					}
				}

				try
				{
					g_tvAudio = IAudioAPI::CreateDevice((IAudioAPI::AudioAPI)config.audio_api, description->GetDescription(), 48000, channels, snd_core::AX_SAMPLES_PER_3MS_48KHZ * AX_FRAMES_PER_GROUP, 16);
					g_tvAudio->SetVolume(m_tv_volume->GetValue());
				}
				catch (std::runtime_error& ex)
				{
					cemuLog_log(LogType::Force, "can't initialize tv audio: {}", ex.what());
				}
			}
		}
	}
	
	// pad audio device
	{
		const auto selection = m_pad_device->GetSelection();
		if (selection == wxNOT_FOUND)
		{
			cemu_assert_debug(false);
			return;
		}

		g_padAudio.reset();

		if (m_pad_device->HasClientObjectData())
		{
			const auto description = (wxDeviceDescription*)m_pad_device->GetClientObject(selection);
			if (description)
			{
				sint32 channels;
				if (m_game_launched && g_padAudio)
					channels = g_padAudio->GetChannels();
				else
				{
					switch (config.pad_channels)
					{
					case 0:
						channels = 1;
						break;
					case 2:
						channels = 6;
						break;
					default: // stereo
						channels = 2;
						break;
					}
				}

				try
				{
					g_padAudio = IAudioAPI::CreateDevice((IAudioAPI::AudioAPI)config.audio_api, description->GetDescription(), 48000, channels, snd_core::AX_SAMPLES_PER_3MS_48KHZ * AX_FRAMES_PER_GROUP, 16);
					g_padAudio->SetVolume(m_pad_volume->GetValue());
					g_padVolume = m_pad_volume->GetValue();
				}
				catch (std::runtime_error& ex)
				{
					cemuLog_log(LogType::Force, "can't initialize pad audio: {}", ex.what());
				}
			}
		}
	}

	// input audio device
	{
		const auto selection = m_input_device->GetSelection();
		if (selection == wxNOT_FOUND)
		{
			cemu_assert_debug(false);
			return;
		}

		g_inputAudio.reset();

		if (m_input_device->HasClientObjectData())
		{
			const auto description = (wxInputDeviceDescription*)m_input_device->GetClientObject(selection);
			if (description)
			{
				sint32 channels;
				if (m_game_launched && g_inputAudio)
					channels = g_inputAudio->GetChannels();
				else
				{
					switch (config.input_channels)
					{
					case 0:
						channels = 1;
						break;
					case 2:
						channels = 6;
						break;
					default: // stereo
						channels = 2;
						break;
					}
				}

				try
				{
					g_inputAudio = IAudioInputAPI::CreateDevice(IAudioInputAPI::AudioInputAPI::Cubeb, description->GetDescription(), 32000, channels, snd_core::AX_SAMPLES_PER_3MS_32KHZ, 16);
					g_inputAudio->SetVolume(m_input_volume->GetValue());
				}
				catch (std::runtime_error& ex)
				{
					cemuLog_log(LogType::Force, "can't initialize pad audio: {}", ex.what());
				}
			}
		}
	}
}

void GeneralSettings2::OnAudioDeviceSelected(wxCommandEvent& event)
{
	UpdateAudioDevice();
}

void GeneralSettings2::OnAudioChannelsSelected(wxCommandEvent& event)
{
	const auto obj = wxDynamicCast(event.GetEventObject(), wxChoice);
	wxASSERT(obj);
	if (obj->GetSelection() == wxNOT_FOUND)
		return;

	auto& config = GetConfig();
	if (obj == m_tv_channels)
	{
		if (config.tv_channels == (AudioChannels)obj->GetSelection())
			return;
		
		config.tv_channels = (AudioChannels)obj->GetSelection();
	}
	else if (obj == m_pad_channels)
	{
		if (config.pad_channels == (AudioChannels)obj->GetSelection())
			return;
		
		config.pad_channels = (AudioChannels)obj->GetSelection();
	}
	else
		cemu_assert_debug(false);

	if(m_game_launched)
		wxMessageBox(_("You have to restart the game in order to apply the new settings."), _("Information"), wxOK | wxCENTRE, this);
	else
		UpdateAudioDevice();
}

void GeneralSettings2::OnGraphicAPISelected(wxCommandEvent& event)
{
	HandleGraphicsApiSelection();
}

void GeneralSettings2::OnAddPathClicked(wxCommandEvent& event)
{
	wxDirDialog path_dialog(this, _("Select a directory containing games."), wxEmptyString, wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
	if (path_dialog.ShowModal() != wxID_OK || path_dialog.GetPath().empty())
		return;

	const auto path = path_dialog.GetPath();
	// test if already included
	for (auto& s : m_game_paths->GetStrings())
	{
		if (s == path)
			return;
	}

	m_game_paths->Append(path);
	m_reload_gamelist = true;

	// trigger title list rescan with new path configuration
	CafeTitleList::ClearScanPaths();
	for (auto& it : m_game_paths->GetStrings())
		CafeTitleList::AddScanPath(wxHelper::MakeFSPath(it));
	CafeTitleList::Refresh();
}

void GeneralSettings2::OnRemovePathClicked(wxCommandEvent& event)
{
	const auto selection = m_game_paths->GetSelection();
	if (selection == wxNOT_FOUND)
		return;

	m_game_paths->Delete(selection);
	m_reload_gamelist = true;
	// trigger title list rescan with new path configuration
	CafeTitleList::ClearScanPaths();
	for (auto& it : m_game_paths->GetStrings())
		CafeTitleList::AddScanPath(wxHelper::MakeFSPath(it));
	CafeTitleList::Refresh();
}

void GeneralSettings2::OnActiveAccountChanged(wxCommandEvent& event)
{
	UpdateAccountInformation();
	m_has_account_change = true;
}

void GeneralSettings2::OnAccountServiceChanged(wxCommandEvent& event)
{
	auto& config = GetConfig();
	uint32 peristentId = GetSelectedAccountPersistentId();
	config.SetAccountSelectedService(peristentId, static_cast<NetworkService>(m_active_service->GetSelection()));
	UpdateAccountInformation();
}

void GeneralSettings2::OnMLCPathSelect(wxCommandEvent& event)
{
	if(CafeSystem::IsTitleRunning())
	{
		wxMessageBox(_("Can't change MLC path while a game is running!"), _("Error"), wxOK | wxCENTRE | wxICON_ERROR, this);
		return;
	}
	// show directory dialog
	wxDirDialog path_dialog(this, _("Select MLC directory"), wxEmptyString, wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
	if (path_dialog.ShowModal() != wxID_OK || path_dialog.GetPath().empty())
		return;
	// check if the choosen MLC path is an already initialized MLC location
	fs::path newMlc = wxHelper::MakeFSPath(path_dialog.GetPath());
	if(CemuApp::CheckMLCPath(newMlc))
	{
		// ask user if they are sure they want to use this folder and let them know that accounts and saves wont transfer
		wxString message = _("Note that changing the MLC location will not transfer any accounts or save files. Are you sure you want to change the path?");
		wxMessageDialog dialog(this, message, _("Warning"), wxYES_NO | wxCENTRE | wxICON_WARNING);
		if(dialog.ShowModal() == wxID_NO)
			return;
		if( !CemuApp::CreateDefaultMLCFiles(newMlc) ) // creating also acts as a check for read+write access
		{
			wxMessageBox(_("Failed to create default MLC files in the selected directory. The MLC path has not been changed"), _("Error"), wxOK | wxCENTRE | wxICON_ERROR, this);
			return;
		}
	}
	else
	{
		// ask user if they want to create a new mlc structure at the choosen location
		wxString message = _("The selected directory does not contain the expected MLC structure. Do you want to create a new MLC structure in this directory?\nNote that changing the MLC location will not transfer any accounts or save files.");
		wxMessageDialog dialog(this, message, _("Warning"), wxYES_NO | wxCENTRE | wxICON_WARNING);
		if( !CemuApp::CreateDefaultMLCFiles(newMlc) )
		{
			wxMessageBox(_("Failed to create default MLC files in the selected directory. The MLC path has not been changed"), _("Error"), wxOK | wxCENTRE | wxICON_ERROR, this);
			return;
		}
	}
	// update MLC path and store any other modified settings
	GetConfig().SetMLCPath(newMlc);
	StoreConfig();
	wxMessageBox(_("Cemu needs to be restarted for the changes to take effect."), _("Information"), wxOK | wxCENTRE | wxICON_INFORMATION, this);
	// close settings and then cemu
	wxCloseEvent closeEvent(wxEVT_CLOSE_WINDOW);
	wxPostEvent(this, closeEvent);
	wxPostEvent(GetParent(), closeEvent);
}

void GeneralSettings2::OnMLCPathClear(wxCommandEvent& event)
{
	if(CafeSystem::IsTitleRunning())
	{
		wxMessageBox(_("Can't change MLC path while a game is running!"), _("Error"), wxOK | wxCENTRE | wxICON_ERROR, this);
		return;
	}
	wxString message = _("Note that changing the MLC location will not transfer any accounts or save files. Are you sure you want to change the path?");
	wxMessageDialog dialog(this, message, _("Warning"), wxYES_NO | wxCENTRE | wxICON_WARNING);
	if(dialog.ShowModal() == wxID_NO)
		return;
	GetConfig().SetMLCPath("");
	StoreConfig();
	g_config.Save();
	wxMessageBox(_("Cemu needs to be restarted for the changes to take effect."), _("Information"), wxOK | wxCENTRE | wxICON_INFORMATION, this);
	// close settings and then cemu
	wxCloseEvent closeEvent(wxEVT_CLOSE_WINDOW);
	wxPostEvent(this, closeEvent);
	wxPostEvent(GetParent(), closeEvent);
}

void GeneralSettings2::OnShowOnlineValidator(wxCommandEvent& event)
{
	const auto selection = m_active_account->GetSelection();
	if (selection == wxNOT_FOUND)
		return;
	
	const auto* obj = dynamic_cast<wxAccountData*>(m_active_account->GetClientObject(selection));
	wxASSERT(obj);
	const auto& account = obj->GetAccount();
	
	const auto validator = account.ValidateOnlineFiles();
	if (validator) // everything valid? shouldn't happen
		return;
	
	wxString err;
	err << _("The following error(s) have been found:") << '\n';
	
	if (validator.otp == OnlineValidator::FileState::Missing)
		err << _("otp.bin missing in Cemu directory") << '\n';
	else if(validator.otp == OnlineValidator::FileState::Corrupted)
		err << _("otp.bin is invalid") << '\n';
	
	if (validator.seeprom == OnlineValidator::FileState::Missing)
		err << _("seeprom.bin missing in Cemu directory") << '\n';
	else if(validator.seeprom == OnlineValidator::FileState::Corrupted)
		err << _("seeprom.bin is invalid") << '\n';

	if(!validator.missing_files.empty())
	{
		err << _("Missing certificate and key files:") << '\n';

		int counter = 0;
		for (const auto& f : validator.missing_files)
		{
			err << f << '\n';

			++counter;
			if(counter > 10)
			{
				err << "..." << '\n';
				break;
			}
		}

		err << '\n';
	}

	if (!validator.valid_account)
	{
		err << _("The currently selected account is not a valid or dumped online account:") << '\n';
		err << GetOnlineAccountErrorMessage(validator.account_error);
	}

	wxMessageBox(err, _("Online Status"), wxOK | wxCENTRE | wxICON_INFORMATION);
}

wxString GeneralSettings2::GetOnlineAccountErrorMessage(OnlineAccountError error)
{
	switch (error)
	{
		case OnlineAccountError::kNoAccountId:
			return _("AccountId missing (The account is not connected to a NNID/PNID)");
		case OnlineAccountError::kNoPasswordCached:
			return _("IsPasswordCacheEnabled is set to false (The remember password option on your Wii U must be enabled for this account before dumping it)");
		case OnlineAccountError::kPasswordCacheEmpty:
			return _("AccountPasswordCache is empty (The remember password option on your Wii U must be enabled for this account before dumping it)");
		case OnlineAccountError::kNoPrincipalId:
			return _("PrincipalId missing");
		default:
			return "no error";
	}
}
