#include "wxgui/GamePadBridgePanel.h"
#include "config/GamePadBridgeConfig.h"

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

		panel_sizer->Add(box_sizer, 0, wxEXPAND | wxALL, 5);
	}

	{
		auto* box = new wxStaticBox(this, wxID_ANY, _("Status"));
		auto* box_sizer = new wxStaticBoxSizer(box, wxVERTICAL);

		auto* status = new wxStaticText(box, wxID_ANY,
			_("The GamePad Bridge service is still in development and is not available yet. "
			  "This setting is saved, but has no effect until the service exists."));
		status->Wrap(500);
		box_sizer->Add(status, 0, wxALL, 5);

		panel_sizer->Add(box_sizer, 0, wxEXPAND | wxALL, 5);
	}

	m_enable->SetValue(GetGamePadBridgeConfig().enabled);

	SetSizerAndFit(panel_sizer);
}

void GamePadBridgePanel::OnEnableChanged(wxCommandEvent& event)
{
	auto lock = GetGamePadBridgeConfigHandle().Lock();
	GetGamePadBridgeConfig().enabled = m_enable->IsChecked();
}
