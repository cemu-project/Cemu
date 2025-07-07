#pragma once

#include <wx/event.h>

class wxLogEvent;
wxDECLARE_EVENT(EVT_LOG, wxLogEvent);

class wxLogEvent : public wxCommandEvent
{
public:
	wxLogEvent(const wxString& filter, const wxString& message) 
		: wxCommandEvent(EVT_LOG), m_filter(filter), m_message(message) { }

	wxLogEvent(const wxLogEvent& event)
		:  wxCommandEvent(event), m_filter(event.GetFilter()), m_message(event.GetMessage()) { }

	wxEvent* Clone() const { return new wxLogEvent(*this); }

	[[nodiscard]] const wxString& GetFilter() const
	{
		return m_filter;
	}

	[[nodiscard]] const wxString& GetMessage() const
	{
		return m_message;
	}

private:
	wxString m_filter;
	wxString m_message;
};
