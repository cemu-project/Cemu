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

	void Close();

private:
    void ProfileThread(struct OSThread_t* thread);
    void ProfileThreadWorker(OSThread_t* thread);
    void PresentProfileResults(OSThread_t* thread, const std::unordered_map<VAddr, uint32>& samples);
    void DumpStackTrace(struct OSThread_t* thread);

    wxListCtrl* m_thread_list;
	wxCheckBox* m_auto_refresh;
	wxTimer* m_timer;

	void OnTimer(wxTimerEvent& event);

	wxDECLARE_EVENT_TABLE();


};