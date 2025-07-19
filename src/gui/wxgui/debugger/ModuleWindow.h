#pragma once

class DebuggerWindow2;

class ModuleWindow : public wxFrame
{
public:
	ModuleWindow(DebuggerWindow2& parent, const wxPoint& main_position, const wxSize& main_size);

	void OnMainMove(const wxPoint& position, const wxSize& main_size);
	void OnGameLoaded();

private:
	void OnLeftDClick(wxMouseEvent& event);

	wxListView* m_modules;
};