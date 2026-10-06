#pragma once

#include <wx/panel.h>

class wxCheckBox;

// "GamePad" tab in the General settings dialog (fork-only).
// Reads and writes GamePadBridgeConfig directly; the dialog's normal save on close
// persists it, because GamePadBridgeConfig is a child of settings.xml.
class GamePadBridgePanel : public wxPanel
{
public:
	GamePadBridgePanel(wxWindow* parent);

private:
	void OnEnableChanged(wxCommandEvent& event);

	wxCheckBox* m_enable;
};
