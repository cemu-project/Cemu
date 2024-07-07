#include "gui/GettingStartedDialog.h"

#include <wx/sizer.h>
#include <wx/filepicker.h>
#include <wx/statbox.h>
#include <wx/msgdlg.h>
#include <wx/radiobox.h>

#include "config/ActiveSettings.h"
#include "gui/CemuApp.h"
#include "gui/DownloadGraphicPacksWindow.h"
#include "gui/GraphicPacksWindow2.h"
#include "gui/input/InputSettings2.h"
#include "config/CemuConfig.h"

#include "Cafe/TitleList/TitleList.h"

#if BOOST_OS_LINUX || BOOST_OS_MACOS
#include "resource/embedded/resources.h"
#endif

#include "wxHelper.h"

wxDEFINE_EVENT(EVT_REFRESH_FIRST_PAGE, wxCommandEvent); // used to refresh the first page after the language change

wxPanel* GettingStartedDialog::CreatePage1()
{
	auto* mainPanel = new wxPanel(m_notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
	auto* page1_sizer = new wxBoxSizer(wxVERTICAL);
	{
		auto* sizer = new wxBoxSizer(wxHORIZONTAL);
		sizer->Add(new wxStaticBitmap(mainPanel, wxID_ANY, wxICON(M_WND_ICON128)), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
		m_page1.staticText11 = new wxStaticText(mainPanel, wxID_ANY, _("It looks like you're starting Cemu for the first time.\nThis quick setup assistant will help you get the best experience"), wxDefaultPosition, wxDefaultSize, 0);
		m_page1.staticText11->Wrap(-1);
		sizer->Add(m_page1.staticText11, 0, wxALL, 5);
		page1_sizer->Add(sizer, 0, wxALL | wxEXPAND, 5);
	}

	if(ActiveSettings::IsPortableMode())
	{
		m_page1.portableModeInfoText = new wxStaticText(mainPanel, wxID_ANY, _("Cemu is running in portable mode"));
		m_page1.portableModeInfoText->Show(true);
		page1_sizer->Add(m_page1.portableModeInfoText, 0, wxALL, 5);

	}

	// language selection
#if 0
	{
		m_page1.languageBoxSizer = new wxStaticBoxSizer(wxVERTICAL, mainPanel, _("Language"));
		m_page1.languageText = new wxStaticText(m_page1.languageBoxSizer->GetStaticBox(), wxID_ANY, _("Select the language you want to use in Cemu"));
		m_page1.languageBoxSizer->Add(m_page1.languageText, 0, wxALL, 5);

		wxString language_choices[] = { _("Default") };
		wxChoice* m_language = new wxChoice(m_page1.languageBoxSizer->GetStaticBox(), wxID_ANY, wxDefaultPosition, wxDefaultSize, std::size(language_choices), language_choices);
		m_language->SetSelection(0);

		for (const auto& language : wxGetApp().GetLanguages())
		{
			m_language->Append(language->DescriptionNative);
		}

		m_language->SetSelection(0);
		m_page1.languageBoxSizer->Add(m_language, 0, wxALL | wxEXPAND, 5);

		page1_sizer->Add(m_page1.languageBoxSizer, 0, wxALL | wxEXPAND, 5);

		m_language->Bind(wxEVT_CHOICE, [this, m_language](const auto&)
		{
			const auto language = m_language->GetStringSelection();
			auto selection = m_language->GetSelection();
			if (selection == 0)
				GetConfig().language = wxLANGUAGE_DEFAULT;
			else
			{
				auto* app = (CemuApp*)wxTheApp;
				const auto language = m_language->GetStringSelection();
				for (const auto& lang : app->GetLanguages())
				{
					if (lang->DescriptionNative == language)
					{
						app->LocalizeUI(static_cast<wxLanguage>(lang->Language));
						wxCommandEvent event(EVT_REFRESH_FIRST_PAGE);
						wxPostEvent(this, event);
						break;
					}
				}
			}
		});
	}
#endif

	{
		m_page1.gamePathBoxSizer = new wxStaticBoxSizer(wxVERTICAL, mainPanel, _("Game paths"));
		m_page1.gamePathText = new wxStaticText(m_page1.gamePathBoxSizer->GetStaticBox(), wxID_ANY, _("The game path is scanned by Cemu to automatically locate your games, game updates and DLCs. We recommend creating a dedicated directory in which\nyou place all your Wii U game files. Additional paths can be set later in Cemu's general settings. All common Wii U game formats are supported by Cemu."));
		m_page1.gamePathBoxSizer->Add(m_page1.gamePathText, 0, wxALL, 5);

		auto* game_path_sizer = new wxBoxSizer(wxHORIZONTAL);

		m_page1.gamePathText2 = new wxStaticText(m_page1.gamePathBoxSizer->GetStaticBox(), wxID_ANY, _("Game path"));
		game_path_sizer->Add(m_page1.gamePathText2, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

		m_page1.gamePathPicker = new wxDirPickerCtrl(m_page1.gamePathBoxSizer->GetStaticBox(), wxID_ANY, wxEmptyString, _("Select a folder"));
		game_path_sizer->Add(m_page1.gamePathPicker, 1, wxALL, 5);

		m_page1.gamePathBoxSizer->Add(game_path_sizer, 0, wxEXPAND, 5);

		page1_sizer->Add(m_page1.gamePathBoxSizer, 0, wxALL | wxEXPAND, 5);
	}

	{
		auto* sizer = new wxStaticBoxSizer(wxVERTICAL, mainPanel, _("Graphic packs && mods"));

		sizer->Add(new wxStaticText(sizer->GetStaticBox(), wxID_ANY, _("Graphic packs improve games by offering the ability to change resolution, increase FPS, tweak visuals or add gameplay modifications.\nGet started by opening the graphic packs configuration window.\n")), 0, wxALL, 5);

		auto* download_gp = new wxButton(sizer->GetStaticBox(), wxID_ANY, _("Download and configure graphic packs"));
		download_gp->Bind(wxEVT_BUTTON, &GettingStartedDialog::OnConfigureGPs, this);
		sizer->Add(download_gp, 0, wxALIGN_CENTER | wxALL, 5);

		page1_sizer->Add(sizer, 0, wxALL | wxEXPAND, 5);
	}

	{
		auto* sizer = new wxFlexGridSizer(0, 1, 0, 0);
		sizer->AddGrowableCol(0);
		sizer->AddGrowableRow(0);
		sizer->SetFlexibleDirection(wxBOTH);
		sizer->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_ALL);

		auto* next = new wxButton(mainPanel, wxID_ANY, _("Next"), wxDefaultPosition, wxDefaultSize, 0);
		next->Bind(wxEVT_BUTTON, [this](const auto&){m_notebook->SetSelection(1); });
		sizer->Add(next, 0, wxALIGN_BOTTOM | wxALIGN_RIGHT | wxALL, 5);

		page1_sizer->Add(sizer, 1, wxEXPAND, 5);
	}

	mainPanel->SetSizer(page1_sizer);
	return mainPanel;
}

wxPanel* GettingStartedDialog::CreatePage2()
{
	auto* result = new wxPanel(m_notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
	auto* page2_sizer = new wxBoxSizer(wxVERTICAL);

	{
		auto* sizer = new wxStaticBoxSizer(new wxStaticBox(result, wxID_ANY, _("Input settings")), wxVERTICAL);

		sizer->Add(new wxStaticText(sizer->GetStaticBox(), wxID_ANY, _("You can configure one controller for each player.\nWe advise you to always use GamePad as emulated input for the first player, since many games expect the GamePad to be present.\nIt is also required for touch functionality.\nThe default global hotkeys are:\nCTRL - show pad screen\nCTRL + TAB - toggle pad screen\nALT + ENTER - toggle fullscreen\nESC - leave fullscreen\n\nIf you're having trouble configuring your controller, make sure to have it in idle state and press calibrate.\nAlso don't set the axis deadzone too low.")), 0, wxALL, 5);

		auto* input_button = new wxButton(sizer->GetStaticBox(), wxID_ANY, _("Configure input"));
		input_button->Bind(wxEVT_BUTTON, &GettingStartedDialog::OnInputSettings, this);
		sizer->Add(input_button, 0, wxALIGN_CENTER | wxALL, 5);

		page2_sizer->Add(sizer, 0, wxALL | wxEXPAND, 5);
	}

	{
		auto* sizer = new wxStaticBoxSizer(new wxStaticBox(result, wxID_ANY, _("Additional options")), wxVERTICAL);

		auto* option_sizer = new wxFlexGridSizer(0, 2, 0, 0);
		option_sizer->SetFlexibleDirection(wxBOTH);
		option_sizer->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);

		m_page2.fullscreenCheckbox = new wxCheckBox(sizer->GetStaticBox(), wxID_ANY, _("Start games with fullscreen"));
		option_sizer->Add(m_page2.fullscreenCheckbox, 0, wxALL, 5);

		m_page2.separateCheckbox = new wxCheckBox(sizer->GetStaticBox(), wxID_ANY, _("Open separate pad screen"));
		option_sizer->Add(m_page2.separateCheckbox, 0, wxALL, 5);

		m_page2.updateCheckbox = new wxCheckBox(sizer->GetStaticBox(), wxID_ANY, _("Automatically check for updates"));
		option_sizer->Add(m_page2.updateCheckbox, 0, wxALL, 5);
#if BOOST_OS_LINUX 
		if (!std::getenv("APPIMAGE")) {
			m_page2.updateCheckbox->Disable();
		} 
#endif
		sizer->Add(option_sizer, 1, wxEXPAND, 5);
		page2_sizer->Add(sizer, 0, wxALL | wxEXPAND, 5);
	}

	{
		auto* sizer = new wxFlexGridSizer(0, 3, 0, 0);
		sizer->AddGrowableCol(1);
		sizer->AddGrowableRow(0);
		sizer->SetFlexibleDirection(wxBOTH);
		sizer->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_ALL);

		auto* previous = new wxButton(result, wxID_ANY, _("Previous"));
		previous->Bind(wxEVT_BUTTON, [this](const auto&) {m_notebook->SetSelection(0); });
		sizer->Add(previous, 0, wxALIGN_BOTTOM | wxALIGN_RIGHT | wxALL, 5);

		auto* close = new wxButton(result, wxID_ANY, _("Close"));
		close->Bind(wxEVT_BUTTON, [this](const auto&){ Close(); });
		sizer->Add(close, 1, wxALIGN_BOTTOM | wxALIGN_RIGHT | wxALL, 5);

		page2_sizer->Add(sizer, 1, wxEXPAND | wxLEFT, 5);
	}
	
	result->SetSizer(page2_sizer);
	return result;
}

void GettingStartedDialog::ApplySettings()
{
	auto& config = GetConfig();
	m_page2.fullscreenCheckbox->SetValue(config.fullscreen.GetValue());
	m_page2.updateCheckbox->SetValue(config.check_update.GetValue());
	m_page2.separateCheckbox->SetValue(config.pad_open.GetValue());
}

void GettingStartedDialog::UpdateWindowSize()
{
	for (auto it = m_notebook->GetChildren().GetFirst(); it; it = it->GetNext())
	{
		it->GetData()->Layout();
	}
	m_notebook->Layout();
	Layout();
	Fit();
}

void GettingStartedDialog::OnClose(wxCloseEvent& event)
{
	event.Skip();

	auto& config = GetConfig();
	config.fullscreen = m_page2.fullscreenCheckbox->GetValue();
	config.check_update = m_page2.updateCheckbox->GetValue();
	config.pad_open = m_page2.separateCheckbox->GetValue();

	const fs::path gamePath = wxHelper::MakeFSPath(m_page1.gamePathPicker->GetPath());
	std::error_code ec;
	if (!gamePath.empty() && fs::exists(gamePath, ec))
	{
		const auto it = std::find(config.game_paths.cbegin(), config.game_paths.cend(), gamePath);
		if (it == config.game_paths.cend())
		{
			config.game_paths.emplace_back(_pathToUtf8(gamePath));
		}
	}
}

GettingStartedDialog::GettingStartedDialog(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, _("Getting started"), wxDefaultPosition, { 740,530 }, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
	auto* sizer = new wxBoxSizer(wxVERTICAL);

	m_notebook = new wxSimplebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0);

	auto* m_page1 = CreatePage1();
	m_notebook->AddPage(m_page1, wxEmptyString, false);

	auto* m_page2 = CreatePage2();
	m_notebook->AddPage(m_page2, wxEmptyString, false);

	sizer->Add(m_notebook, 1, wxEXPAND | wxALL, 5);

	this->SetSizer(sizer);
	this->Centre(wxBOTH);
	this->Bind(wxEVT_CLOSE_WINDOW, &GettingStartedDialog::OnClose, this);

	ApplySettings();
	UpdateWindowSize();
}

void GettingStartedDialog::OnConfigureGPs(wxCommandEvent& event)
{
	DownloadGraphicPacksWindow dialog(this);
	dialog.ShowModal();
	GraphicPacksWindow2::RefreshGraphicPacks();
	GraphicPacksWindow2 window(this, 0);
	window.ShowModal();
}

void GettingStartedDialog::OnInputSettings(wxCommandEvent& event)
{
	InputSettings2 dialog(this);
	dialog.ShowModal();
}
