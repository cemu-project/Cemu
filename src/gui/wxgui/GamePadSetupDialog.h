#pragma once

#include <wx/dialog.h>
#include <wx/timer.h>

class wxButton;
class wxGauge;
class wxListCtrl;
class wxListEvent;
class wxProcess;
class wxProcessEvent;
class wxSimplebook;
class wxStaticText;

struct GamePadSetupStatus
{
	bool setUp = false;
	wxString adapterName;
	wxString padState; // connected | disconnected | down | off
};

// GamePad setup (fork-only): 1. choose the Wi-Fi adapter, 2. sync the GamePad, done. The work is done by the
// bridge's setup helper (cemu-gamepad-setup: scripts/gamepad-setup.sh in the GamePad bridge project); this window
// only runs it and shows its progress. Ends with wxID_OK when the GamePad is set up.
class GamePadSetupDialog : public wxDialog
{
public:
	GamePadSetupDialog(wxWindow* parent);
	~GamePadSetupDialog() override;

	static GamePadSetupStatus QueryStatus();

private:
	enum Page
	{
		kPageChoose,
		kPageWorking,
		kPageDone,
	};

	void FillAdapters();
	void ShowPage(Page page);
	void OnNext(wxCommandEvent& event);
	void OnCancel(wxCommandEvent& event);
	void OnPoll(wxTimerEvent& event);
	void OnProcessEnded(wxProcessEvent& event);
	void DrainOutput();
	void HandleLine(const wxString& line);
	void ShowFailure(const wxString& reason);

	wxString m_helper; // empty: not found
	wxSimplebook* m_book;
	Page m_page = kPageChoose;
	// choose
	wxListCtrl* m_list;
	wxStaticText* m_chooseNote;
	// working
	wxStaticText* m_workTitle;
	wxStaticText* m_workText;
	wxStaticText* m_symbols;
	wxGauge* m_gauge;
	// done
	wxStaticText* m_doneTitle;
	wxStaticText* m_doneText;

	wxButton* m_back;
	wxButton* m_next;
	wxButton* m_cancel;

	wxProcess* m_proc = nullptr;
	wxString m_partial;
	bool m_succeeded = false;
	bool m_sawResult = false;
	bool m_cancelling = false;
	wxTimer m_poll;
};
