#include "gui/GettingStartedDialog.h"

#include <wx/sizer.h>
#include <wx/filepicker.h>
#include <wx/statbox.h>
#include <wx/msgdlg.h>

#include "config/ActiveSettings.h"
#include "gui/CemuApp.h"
#include "gui/DownloadGraphicPacksWindow.h"
#include "gui/GraphicPacksWindow2.h"
#include "gui/input/InputSettings2.h"
#include "config/CemuConfig.h"
#include "config/PermanentConfig.h"

#include "Cafe/TitleList/TitleList.h"

#if BOOST_OS_LINUX || BOOST_OS_MACOS
#include "resource/embedded/resources.h"
#endif

#include "wxHelper.h"

wxPanel* GettingStartedDialog::CreatePage1()
{
	auto* result = new wxPanel(m_notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
	auto* page1_sizer = new wxBoxSizer(wxVERTICAL);
	{
		auto* sizer = new wxBoxSizer(wxHORIZONTAL);

		sizer->Add(new wxStaticBitmap(result, wxID_ANY, wxICON(M_WND_ICON128)), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

		auto* m_staticText11 = new wxStaticText(result, wxID_ANY, _("It looks like you're starting Cemu for the first time.\nThis quick setup assistant will help you get the best experience"), wxDefaultPosition, wxDefaultSize, 0);
		m_staticText11->Wrap(-1);
		sizer->Add(m_staticText11, 0, wxALL, 5);

		page1_sizer->Add(sizer, 0, wxALL | wxEXPAND, 5);
	}

	{
		m_mlc_box_sizer = new wxStaticBoxSizer(wxVERTICAL, result, _("mlc01 path"));
		m_mlc_box_sizer->Add(new wxStaticText(m_mlc_box_sizer->GetStaticBox(), wxID_ANY, _("The mlc path is the root folder of the emulated Wii U internal flash storage. It contains all your saves, installed updates and DLCs.\nIt is strongly recommend that you create a dedicated folder for it (example: C:\\wiiu\\mlc\\) \nIf left empty, the mlc folder will be created inside the Cemu folder.")), 0, wxALL, 5);

		m_prev_mlc_warning = new wxStaticText(m_mlc_box_sizer->GetStaticBox(), wxID_ANY, _("A custom mlc path from a previous Cemu installation has been found and filled in."));
		m_prev_mlc_warning->SetForegroundColour(*wxRED);
		m_prev_mlc_warning->Show(false);
		m_mlc_box_sizer->Add(m_prev_mlc_warning, 0, wxALL, 5);
		
		auto* mlc_path_sizer = new wxBoxSizer(wxHORIZONTAL);
		mlc_path_sizer->Add(new wxStaticText(m_mlc_box_sizer->GetStaticBox(), wxID_ANY, _("Custom mlc01 path")), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

		// workaround since we can't specify our own browse label? >> _("Browse")
		m_mlc_folder = new wxDirPickerCtrl(m_mlc_box_sizer->GetStaticBox(), wxID_ANY, wxEmptyString, _("Select a folder"), wxDefaultPosition, wxDefaultSize, wxDIRP_DEFAULT_STYLE);
		auto tTest1 = m_mlc_folder->GetTextCtrl();
       if(m_mlc_folder->HasTextCtrl())
       {
           m_mlc_folder->GetTextCtrl()->SetEditable(false);
           m_mlc_folder->GetTextCtrl()->Bind(wxEVT_CHAR, &GettingStartedDialog::OnMLCPathChar, this);
       }
		mlc_path_sizer->Add(m_mlc_folder, 1, wxALL, 5);

		mlc_path_sizer->Add(new wxStaticText(m_mlc_box_sizer->GetStaticBox(), wxID_ANY, _("(optional)")), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

		m_mlc_box_sizer->Add(mlc_path_sizer, 0, wxEXPAND, 5);

		page1_sizer->Add(m_mlc_box_sizer, 0, wxALL | wxEXPAND, 5);
	}

	{
		auto* sizer = new wxStaticBoxSizer(wxVERTICAL, result, _("Game paths"));

		sizer->Add(new wxStaticText(sizer->GetStaticBox(), wxID_ANY, _("The game path is scanned by Cemu to locate your games. We recommend creating a dedicated directory in which\nyou place all your Wii U games. (example: C:\\wiiu\\games\\)\n\nYou can also set additional paths in the general settings of Cemu.")), 0, wxALL, 5);

		auto* game_path_sizer = new wxBoxSizer(wxHORIZONTAL);

		game_path_sizer->Add(new wxStaticText(sizer->GetStaticBox(), wxID_ANY, _("Game path")), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

		m_game_path = new wxDirPickerCtrl(sizer->GetStaticBox(), wxID_ANY, wxEmptyString, _("Select a folder"));
		game_path_sizer->Add(m_game_path, 1, wxALL, 5);

		sizer->Add(game_path_sizer, 0, wxEXPAND, 5);

		page1_sizer->Add(sizer, 0, wxALL | wxEXPAND, 5);
	}

	{
		auto* sizer = new wxStaticBoxSizer(wxVERTICAL, result, _("Graphic packs"));

		sizer->Add(new wxStaticText(sizer->GetStaticBox(), wxID_ANY, _("Graphic packs improve games by offering the possibility to change resolution, tweak FPS or add other visual or gameplay modifications.\nDownload the community graphic packs to get started.\n")), 0, wxALL, 5);

		auto* download_gp = new wxButton(sizer->GetStaticBox(), wxID_ANY, _("Download community graphic packs"));
		download_gp->Bind(wxEVT_BUTTON, &GettingStartedDialog::OnDownloadGPs, this);
		sizer->Add(download_gp, 0, wxALIGN_CENTER | wxALL, 5);

		page1_sizer->Add(sizer, 0, wxALL | wxEXPAND, 5);
	}

	{
		auto* sizer = new wxFlexGridSizer(0, 1, 0, 0);
		sizer->AddGrowableCol(0);
		sizer->AddGrowableRow(0);
		sizer->SetFlexibleDirection(wxBOTH);
		sizer->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_ALL);

		auto* next = new wxButton(result, wxID_ANY, _("Next"), wxDefaultPosition, wxDefaultSize, 0);
		next->Bind(wxEVT_BUTTON, [this](const auto&){m_notebook->SetSelection(1); });
		sizer->Add(next, 0, wxALIGN_BOTTOM | wxALIGN_RIGHT | wxALL, 5);

		page1_sizer->Add(sizer, 1, wxEXPAND, 5);
	}


	result->SetSizer(page1_sizer);
	return result;
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

		m_fullscreen = new wxCheckBox(sizer->GetStaticBox(), wxID_ANY, _("Start games with fullscreen"));
		option_sizer->Add(m_fullscreen, 0, wxALL, 5);

		m_separate = new wxCheckBox(sizer->GetStaticBox(), wxID_ANY, _("Open separate pad screen"));
		option_sizer->Add(m_separate, 0, wxALL, 5);

		m_update = new wxCheckBox(sizer->GetStaticBox(), wxID_ANY, _("Automatically check for updates"));
		option_sizer->Add(m_update, 0, wxALL, 5);
#if BOOST_OS_LINUX || BOOST_OS_MACOS
		m_update->Disable();
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

		m_dont_show = new wxCheckBox(result, wxID_ANY, _("Don't show this again"));
		m_dont_show->SetValue(true);
		sizer->Add(m_dont_show, 0, wxALIGN_BOTTOM | wxALL, 5);

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
	m_fullscreen->SetValue(config.fullscreen.GetValue());
	m_update->SetValue(config.check_update.GetValue());
	m_separate->SetValue(config.pad_open.GetValue());
	m_dont_show->SetValue(true); // we want it always enabled by default
   	m_mlc_folder->SetPath(config.mlc_path.GetValue());

	try
	{
		const auto pconfig = PermanentConfig::Load();
		if(!pconfig.custom_mlc_path.empty())
		{
   			m_mlc_folder->SetPath(wxString::FromUTF8(pconfig.custom_mlc_path));
			m_prev_mlc_warning->Show(true);
		}
	}
	catch (const PSDisabledException&) {}
	catch (...)	{}
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
	config.fullscreen = m_fullscreen->GetValue();
	config.check_update = m_update->GetValue();
	config.pad_open = m_separate->GetValue();
	config.did_show_graphic_pack_download = m_dont_show->GetValue();

	const fs::path gamePath = wxHelper::MakeFSPath(m_game_path->GetPath());
	if (!gamePath.empty() && fs::exists(gamePath))
	{
		const auto it = std::find(config.game_paths.cbegin(), config.game_paths.cend(), gamePath);
		if (it == config.game_paths.cend())
		{
			config.game_paths.emplace_back(_pathToUtf8(gamePath));
			m_game_path_changed = true;
		}
	}

    const fs::path mlcPath = wxHelper::MakeFSPath(m_mlc_folder->GetPath());
	if(config.mlc_path.GetValue() != mlcPath && (mlcPath.empty() || fs::exists(mlcPath)))
	{
		config.SetMLCPath(mlcPath, false);
		m_mlc_changed = true;
	}
	
	g_config.Save();

	if(m_mlc_changed)
		CemuApp::CreateDefaultFiles();

	CafeTitleList::ClearScanPaths();
	for (auto& it : GetConfig().game_paths)
		CafeTitleList::AddScanPath(_utf8ToPath(it));
	CafeTitleList::Refresh();
}


GettingStartedDialog::GettingStartedDialog(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, _("Getting started"), wxDefaultPosition, { 740,530 }, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
	//this->SetSizeHints(wxDefaultSize, { 740,530 });

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

void GettingStartedDialog::OnDownloadGPs(wxCommandEvent& event)
{
	DownloadGraphicPacksWindow dialog(this);
	dialog.ShowModal();

	GraphicPacksWindow2::RefreshGraphicPacks();

	wxMessageDialog ask_dialog(this, _("Do you want to view the downloaded graphic packs?"), _("Graphic packs"), wxCENTRE | wxYES_NO);
	if (ask_dialog.ShowModal() == wxID_YES)
	{
		GraphicPacksWindow2 window(this, 0);
		window.ShowModal();
	}
}

void GettingStartedDialog::OnInputSettings(wxCommandEvent& event)
{
	InputSettings2 dialog(this);
	dialog.ShowModal();
}

void GettingStartedDialog::OnMLCPathChar(wxKeyEvent& event)
{
	//if (LaunchSettings::GetMLCPath().has_value())
	//	return;

	if (event.GetKeyCode() == WXK_DELETE || event.GetKeyCode() == WXK_BACK)
	{
		m_mlc_folder->GetTextCtrl()->SetValue(wxEmptyString);
		if(m_prev_mlc_warning->IsShown())
		{
			m_prev_mlc_warning->Show(false);
			UpdateWindowSize();
		}	
	}
}

