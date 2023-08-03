#pragma once
#include "gui/wxcomponents/checkedlistctrl.h"

class DebuggerWindow2;

class BreakpointWindow : public wxFrame
{
public:
	BreakpointWindow(DebuggerWindow2& parent, const wxPoint& main_position, const wxSize& main_size);
	virtual ~BreakpointWindow();

	void OnMainMove(const wxPoint& position, const wxSize& main_size);
	void OnUpdateView();
	void OnGameLoaded();

private:
	void OnBreakpointToggled(wxListEvent& event);
	void OnLeftDClick(wxMouseEvent& event);
	void OnRightDown(wxMouseEvent& event);

	void OnContextMenuClick(wxCommandEvent& evt);
	void OnContextMenuClickSelected(wxCommandEvent& evt);

	wxCheckedListCtrl* m_breakpoints;
};