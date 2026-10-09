#include "wxgui/GamePadBridgePanel.h"
#include "config/CemuConfig.h"
#include "config/GamePadBridgeConfig.h"
#include "wxgui/GamePadSetupDialog.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/utils.h>

GamePadBridgePanel::GamePadBridgePanel(wxWindow* parent)
	: wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL)
{
	Build();
}

void GamePadBridgePanel::Build()
{
	DestroyChildren();
	m_enable = m_mirrorTv = nullptr;
	auto* sizer = new wxBoxSizer(wxVERTICAL);
	const GamePadSetupStatus status = GamePadSetupDialog::QueryStatus();

	if (!status.setUp)
	{
		auto* text = new wxStaticText(this, wxID_ANY, _("Play with a real Wii U GamePad."));
		sizer->Add(text, 0, wxALL, 10);
		auto* setup = new wxButton(this, wxID_ANY, _("Set up GamePad"));
		setup->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { RunSetup(); });
		sizer->Add(setup, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
		SetSizer(sizer, true);
		Layout();
		return;
	}

	const wxString padState = status.padState == "connected" ? _("Connected")
							  : status.padState == "disconnected" ? _("Turn on the GamePad to connect")
																  : _("Plug in your Wi-Fi adapter");
	auto* state = new wxStaticText(this, wxID_ANY, _("GamePad: ") + padState);
	sizer->Add(state, 0, wxALL, 10);

	m_enable = new wxCheckBox(this, wxID_ANY, _("Use the GamePad"));
	m_enable->SetValue(GetGamePadBridgeConfig().enabled);
	m_enable->Bind(wxEVT_CHECKBOX, &GamePadBridgePanel::OnEnableChanged, this);
	sizer->Add(m_enable, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

	m_mirrorTv = new wxCheckBox(this, wxID_ANY, _("Show the TV picture when a game doesn't use the GamePad screen"));
	m_mirrorTv->SetValue(GetGamePadBridgeConfig().mirrorTvWhenPadUnused);
	m_mirrorTv->Bind(wxEVT_CHECKBOX, &GamePadBridgePanel::OnMirrorTvChanged, this);
	sizer->Add(m_mirrorTv, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

	auto* again = new wxButton(this, wxID_ANY, _("Set up again"));
	again->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { RunSetup(); });
	sizer->Add(again, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

	SetSizer(sizer, true);
	Layout();
}

void GamePadBridgePanel::RunSetup()
{
	GamePadSetupDialog dialog(this);
	if (dialog.ShowModal() == wxID_OK)
	{
		{
			auto lock = GetGamePadBridgeConfigHandle().Lock();
			GetGamePadBridgeConfig().enabled = true;
		}
		GetConfigHandle().Save();
	}
	// Rebuild after the dialog's event loop is gone: this handler's button is one of the children Build destroys.
	CallAfter([this] { Build(); });
}

void GamePadBridgePanel::OnEnableChanged(wxCommandEvent& event)
{
	auto lock = GetGamePadBridgeConfigHandle().Lock();
	GetGamePadBridgeConfig().enabled = m_enable->IsChecked();
}

void GamePadBridgePanel::OnMirrorTvChanged(wxCommandEvent& event)
{
	auto lock = GetGamePadBridgeConfigHandle().Lock();
	GetGamePadBridgeConfig().mirrorTvWhenPadUnused = m_mirrorTv->IsChecked();
}
