#pragma once

#include "gui/debugger/DumpCtrl.h"

class DebuggerWindow2;

class DumpWindow : public wxFrame
{
public:
	DumpWindow(DebuggerWindow2& parent, const wxPoint& main_position, const wxSize& main_size);

	void OnMainMove(const wxPoint& position, const wxSize& main_size);
	void OnGameLoaded();

private:
	wxScrolledWindow* m_scrolled_window;
	DumpCtrl* m_dump_ctrl;
};