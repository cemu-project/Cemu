#pragma once

#include <wx/dialog.h>
#include <wx/timer.h>

class wxButton;
class wxListCtrl;
class wxListEvent;
class wxProcess;
class wxProcessEvent;
class wxStaticText;
class wxTextCtrl;

// GamePad setup (fork-only): lists the Wi-Fi adapters with a verdict each and tests the chosen one. The work is done
// by the bridge's setup helper (scripts/gamepad-setup.sh in the GamePad bridge project); this window only runs it.
class GamePadSetupDialog : public wxDialog
{
public:
	GamePadSetupDialog(wxWindow* parent);
	~GamePadSetupDialog() override;

private:
	void Refresh();
	void OnRefresh(wxCommandEvent& event);
	void OnTest(wxCommandEvent& event);
	void OnSelectionChanged(wxListEvent& event);
	void OnPoll(wxTimerEvent& event);
	void OnProcessEnded(wxProcessEvent& event);
	void DrainOutput();
	void HandleLine(const wxString& line);
	void UpdateButtons();

	wxString m_helper; // empty: not found
	wxListCtrl* m_list;
	wxTextCtrl* m_log;
	wxStaticText* m_result;
	wxButton* m_refresh;
	wxButton* m_test;
	wxProcess* m_proc = nullptr;
	wxString m_partial;
	bool m_sawResult = false;
	wxTimer m_poll;
};
