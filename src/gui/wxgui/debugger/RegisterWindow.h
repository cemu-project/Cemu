#pragma once
#include "Cafe/HW/Espresso/Debugger/Debugger.h"

class DebuggerWindow2;

class RegisterWindow : public wxFrame
{
public:
	RegisterWindow(DebuggerWindow2& parent, const wxPoint& main_position, const wxSize& main_size);

	void OnMainMove(const wxPoint& position, const wxSize& main_size);
	void OnUpdateView();

private:
	void OnMouseDClickEvent(wxMouseEvent& event);
	void OnFPViewModePress(wxCommandEvent& event);
	void OnValueContextMenu(wxContextMenuEvent& event);
	void OnValueContextMenuSelected(wxCommandEvent& event);

	void UpdateIntegerRegister(wxTextCtrl* label, wxTextCtrl* value, uint32 registerValue, bool hasChanged);

	PPCSnapshot m_prev_snapshot;
	bool m_show_double_values;

	wxTextCtrl* m_context_ctrl;
};