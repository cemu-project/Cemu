#pragma once

#include <wx/panel.h>
#include <wx/spinctrl.h>

class wxCheckBox;
class wxSpinCtrl;

// "GamePad" tab in the General settings dialog (fork-only).
// Reads and writes GamePadBridgeConfig directly; the dialog's normal save on close
// persists it, because GamePadBridgeConfig is a child of settings.xml.
class GamePadBridgePanel : public wxPanel
{
public:
	GamePadBridgePanel(wxWindow* parent);

private:
	void OnEnableChanged(wxCommandEvent& event);
	void OnTvHoldChanged(wxSpinEvent& event);
	void OnPatternChanged(wxCommandEvent& event);
	void OnMirrorTvChanged(wxCommandEvent& event);

	wxCheckBox* m_enable;
	wxSpinCtrl* m_tvHold;
	wxCheckBox* m_pattern;
	wxCheckBox* m_mirrorTv;
};
