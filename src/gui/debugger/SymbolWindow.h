#pragma once

#include "gui/debugger/SymbolCtrl.h"

class DebuggerWindow2;

class SymbolWindow : public wxFrame
{
public:
	SymbolWindow(DebuggerWindow2& parent, const wxPoint& main_position, const wxSize& main_size);

	void OnMainMove(const wxPoint& position, const wxSize& main_size);
	void OnGameLoaded();

	void OnLeftDClick(wxListEvent& event);

private:
	wxTextCtrl* m_filter;
	SymbolListCtrl* m_symbol_ctrl;

	void OnFilterChanged(wxCommandEvent& event);
};