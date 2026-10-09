#pragma once

#include <wx/panel.h>

class wxCheckBox;

// Settings > GamePad (fork-only). Before setup: only a "Set up GamePad" button. After: status and a few options.
class GamePadBridgePanel : public wxPanel
{
public:
	GamePadBridgePanel(wxWindow* parent);

private:
	void Build();
	void RunSetup();
	void OnEnableChanged(wxCommandEvent& event);
	void OnMirrorTvChanged(wxCommandEvent& event);

	wxCheckBox* m_enable = nullptr;
	wxCheckBox* m_mirrorTv = nullptr;
};
