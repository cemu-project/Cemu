#pragma once

#include <wx/wx.h>

class AudioDebuggerWindow : public wxFrame
{
public:
	AudioDebuggerWindow(wxFrame& parent);

	void OnCloseButton(wxCommandEvent& event);
	void OnRefreshButton(wxCommandEvent& event);
	void OnClose(wxCloseEvent& event);
	void RefreshVoiceList_sndgeneric();
	void RefreshVoiceList();
	void OnRefreshTimer(wxTimerEvent& event);
	void OnVoiceListPopupClick(wxCommandEvent &evt);
	void OnVoiceListRightClick(wxMouseEvent& event);
	
	void Close();

private:
	wxListCtrl* voiceListbox;
	wxTimer* refreshTimer;

	wxDECLARE_EVENT_TABLE();


};