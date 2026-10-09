#include "wxgui/GamePadBridgePanel.h"
#include "config/GamePadBridgeConfig.h"
#include "wxgui/GamePadSetupDialog.h"

#include <wx/button.h>

#include <wx/checkbox.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>

GamePadBridgePanel::GamePadBridgePanel(wxWindow* parent)
	: wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL)
{
	auto* panel_sizer = new wxBoxSizer(wxVERTICAL);

	{
		auto* box = new wxStaticBox(this, wxID_ANY, _("GamePad Bridge"));
		auto* box_sizer = new wxStaticBoxSizer(box, wxVERTICAL);

		m_enable = new wxCheckBox(box, wxID_ANY, _("Enable GamePad Bridge"));
		m_enable->SetToolTip(_("Send the GamePad screen and audio to a real Wii U GamePad, and use its controls, touch screen and motion as input.\nRequires the GamePad Bridge service to be running."));
		m_enable->Bind(wxEVT_CHECKBOX, &GamePadBridgePanel::OnEnableChanged, this);
		box_sizer->Add(m_enable, 0, wxALL, 5);

		auto* description = new wxStaticText(box, wxID_ANY,
			_("Uses a real Wii U GamePad as the emulated GamePad: its screen shows the game's GamePad view, "
			  "and its buttons, sticks, touch screen and motion sensors control the game.\n\n"
			  "If this is enabled but the GamePad Bridge service is not running when a game starts, "
			  "a notice is shown and the game runs without the GamePad."));
		description->Wrap(500);
		box_sizer->Add(description, 0, wxALL, 5);

		m_mirrorTv = new wxCheckBox(box, wxID_ANY, _("Show the TV picture on the GamePad when the game draws nothing there"));
		m_mirrorTv->SetToolTip(_("Some games (Super Smash Bros. for Wii U) never use the GamePad screen. With this on, the GamePad shows the TV picture after a second without a GamePad frame."));
		m_mirrorTv->Bind(wxEVT_CHECKBOX, &GamePadBridgePanel::OnMirrorTvChanged, this);
		box_sizer->Add(m_mirrorTv, 0, wxALL, 5);

		panel_sizer->Add(box_sizer, 0, wxEXPAND | wxALL, 5);
	}

	{
		auto* box = new wxStaticBox(this, wxID_ANY, _("Screen sync"));
		auto* box_sizer = new wxStaticBoxSizer(box, wxVERTICAL);

		auto* row = new wxBoxSizer(wxHORIZONTAL);
		row->Add(new wxStaticText(box, wxID_ANY, _("Hold TV picture (ms)")), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
		m_tvHold = new wxSpinCtrl(box, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0,
								  GamePadBridgeConfig::kMaxTvHoldMs, 0);
		m_tvHold->SetToolTip(_("Delays the TV picture so it matches the GamePad screen, which always lags a little.\n"
							   "Applied in whole frames (about 17 ms each), only while the GamePad Bridge is connected."));
		m_tvHold->Bind(wxEVT_SPINCTRL, &GamePadBridgePanel::OnTvHoldChanged, this);
		row->Add(m_tvHold, 0, wxALL, 5);
		box_sizer->Add(row, 0, wxEXPAND);

		m_pattern = new wxCheckBox(box, wxID_ANY, _("Show sync test pattern on both screens"));
		m_pattern->SetToolTip(_("For measuring screen sync with a camera: both screens flash white once a second and show a frame counter.\nSee docs/SYNC.md."));
		m_pattern->Bind(wxEVT_CHECKBOX, &GamePadBridgePanel::OnPatternChanged, this);
		box_sizer->Add(m_pattern, 0, wxALL, 5);

		panel_sizer->Add(box_sizer, 0, wxEXPAND | wxALL, 5);
	}

	{
		auto* box = new wxStaticBox(this, wxID_ANY, _("Setup"));
		auto* box_sizer = new wxStaticBoxSizer(box, wxVERTICAL);

		auto* text = new wxStaticText(box, wxID_ANY,
			_("Linux only. Choose and test the Wi-Fi adapter that hosts the GamePad's network."));
		text->Wrap(500);
		box_sizer->Add(text, 0, wxALL, 5);
		auto* setup = new wxButton(box, wxID_ANY, _("Set up GamePad..."));
		setup->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
			GamePadSetupDialog dialog(this);
			dialog.ShowModal();
		});
		box_sizer->Add(setup, 0, wxALL, 5);

		panel_sizer->Add(box_sizer, 0, wxEXPAND | wxALL, 5);
	}

	m_enable->SetValue(GetGamePadBridgeConfig().enabled);
	m_tvHold->SetValue(GetGamePadBridgeConfig().tvHoldMs);
	m_pattern->SetValue(GetGamePadBridgeConfig().syncTestPattern);
	m_mirrorTv->SetValue(GetGamePadBridgeConfig().mirrorTvWhenPadUnused);

	SetSizerAndFit(panel_sizer);
}

void GamePadBridgePanel::OnEnableChanged(wxCommandEvent& event)
{
	auto lock = GetGamePadBridgeConfigHandle().Lock();
	GetGamePadBridgeConfig().enabled = m_enable->IsChecked();
}

void GamePadBridgePanel::OnTvHoldChanged(wxSpinEvent& event)
{
	auto lock = GetGamePadBridgeConfigHandle().Lock();
	GetGamePadBridgeConfig().tvHoldMs = m_tvHold->GetValue();
}

void GamePadBridgePanel::OnPatternChanged(wxCommandEvent& event)
{
	auto lock = GetGamePadBridgeConfigHandle().Lock();
	GetGamePadBridgeConfig().syncTestPattern = m_pattern->IsChecked();
}

void GamePadBridgePanel::OnMirrorTvChanged(wxCommandEvent& event)
{
	auto lock = GetGamePadBridgeConfigHandle().Lock();
	GetGamePadBridgeConfig().mirrorTvWhenPadUnused = m_mirrorTv->IsChecked();
}
