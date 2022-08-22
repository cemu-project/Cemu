#pragma once

#include <wx/event.h>
#include <wx/gauge.h>

class wxStatusBar;
class wxControl;

wxDECLARE_EVENT(wxEVT_REMOVE_ITEM, wxCommandEvent);
wxDECLARE_EVENT(wxEVT_SET_TEXT, wxCommandEvent);
wxDECLARE_EVENT(wxEVT_ENABLE, wxCommandEvent);

class wxSetStatusBarTextEvent;
wxDECLARE_EVENT(wxEVT_SET_STATUS_BAR_TEXT, wxSetStatusBarTextEvent);
class wxSetStatusBarTextEvent : public wxCommandEvent
{
public:
	wxSetStatusBarTextEvent(const wxString& text, int number = 0, wxEventType type = wxEVT_SET_STATUS_BAR_TEXT, int id = 0)
		: wxCommandEvent(type, id), m_text(text), m_number(number) {}

	wxSetStatusBarTextEvent(const wxSetStatusBarTextEvent& event)
		: wxCommandEvent(event), m_text(event.GetText()), m_number(event.GetNumber()) {}


	[[nodiscard]] const wxString& GetText() const { return m_text; }
	[[nodiscard]] int GetNumber() const { return m_number; }

	wxEvent* Clone() const override
	{
		return new wxSetStatusBarTextEvent(*this);
	}

private:
	const wxString m_text;
	const int m_number;
};
class wxSetGaugeValue;
wxDECLARE_EVENT(wxEVT_SET_GAUGE_VALUE, wxSetGaugeValue);
class wxSetGaugeValue : public wxCommandEvent
{
public:
	wxSetGaugeValue(int value, wxGauge* gauge, wxControl* text_ctrl = nullptr, const wxString& text = wxEmptyString, wxEventType type = wxEVT_SET_GAUGE_VALUE, int id = 0)
		: wxCommandEvent(type, id), m_gauge_value(value), m_gauge_range(0), m_text_ctrl(text_ctrl), m_text(text)
	{
		SetEventObject(gauge);
	}
	
	wxSetGaugeValue(int value, int range, wxGauge* gauge, wxControl* text_ctrl = nullptr, const wxString& text = wxEmptyString, wxEventType type = wxEVT_SET_GAUGE_VALUE, int id = 0)
		: wxCommandEvent(type, id), m_gauge_value(value), m_gauge_range(range), m_text_ctrl(text_ctrl), m_text(text)
	{
		SetEventObject(gauge);
	}

	wxSetGaugeValue(const wxSetGaugeValue& event)
		: wxCommandEvent(event), m_gauge_value(event.GetValue()), m_gauge_range(event.GetRange()), m_text_ctrl(event.GetTextCtrl()), m_text(event.GetText())
	{}

	[[nodiscard]] int GetValue() const { return m_gauge_value; }
	[[nodiscard]] int GetRange() const { return m_gauge_range; }
	[[nodiscard]] wxGauge* GetGauge() const { return (wxGauge*)GetEventObject(); }
	[[nodiscard]] const wxString& GetText() const { return m_text; }
	[[nodiscard]] wxControl* GetTextCtrl() const { return m_text_ctrl; }

	wxEvent* Clone() const override
	{
		return new wxSetGaugeValue(*this);
	}

private:
	const int m_gauge_value, m_gauge_range;
	wxControl* m_text_ctrl;
	const wxString m_text;
};

