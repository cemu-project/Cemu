#pragma once

#include <wx/wx.h>

class DebugPPCThreadsWindow: public wxFrame
{
public:
	DebugPPCThreadsWindow(wxFrame& parent);
	~DebugPPCThreadsWindow();

	void OnCloseButton(wxCommandEvent& event);
	void OnRefreshButton(wxCommandEvent& event);
	void OnClose(wxCloseEvent& event);
	void RefreshThreadList();
	void OnThreadListPopupClick(wxCommandEvent &evt);
	void OnThreadListRightClick(wxMouseEvent& event);

	void DumpStackTrace(struct OSThread_t* thread);

	void Close();

private:
	wxListCtrl* m_thread_list;
	wxCheckBox* m_auto_refresh;
	wxTimer* m_timer;

	void OnTimer(wxTimerEvent& event);

	wxDECLARE_EVENT_TABLE();


};