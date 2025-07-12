#pragma once
#include "TextList.h"

class wxLogCtrl : public TextList
{
public:
	wxLogCtrl(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style);
	~wxLogCtrl();

	void SetActiveFilter(const std::string& active_filter);
	[[nodiscard]] const std::string& GetActiveFilter() const;
	void SetFilterMessage(bool state);
	[[nodiscard]] bool GetFilterMessage() const;

	void PushEntry(const wxString& filter, const wxString& message);
	
protected:
	void OnDraw(wxDC& dc, sint32 start, sint32 count, const wxPoint& start_position);

private:
	void OnTimer(wxTimerEvent& event);
	void OnActiveListUpdated(wxEvent& event);

	wxTimer* m_timer;

	std::string m_active_filter;
	std::thread m_update_worker;
	bool m_filter_messages = false;

	std::mutex m_mutex, m_active_mutex;
	using ListEntry_t = std::pair<wxString, wxString>;
	using ListIt_t = std::list<ListEntry_t>::const_reference;
	std::list<ListEntry_t> m_log_entries;
	std::list<std::reference_wrapper<const ListEntry_t>> m_active_entries;
	void UpdateActiveEntries();
};