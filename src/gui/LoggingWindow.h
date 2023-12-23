#pragma once

#include <wx/frame.h>
#include <wx/listbox.h>
#include <wx/combobox.h>
#include "components/wxLogCtrl.h"

class wxLogEvent;

class LoggingWindow : public wxFrame, LogCallbacks
{
public:
	LoggingWindow(wxFrame* parent);
	~LoggingWindow();

	virtual void Log(std::string_view filter, std::string_view message) override;
	virtual void Log(std::string_view filter, std::wstring_view message) override;

private:
	void OnLogMessage(wxLogEvent& event);
	void OnFilterChange(wxCommandEvent& event);
	void OnFilterMessageChange(wxCommandEvent& event);

	wxComboBox* m_filter;
	wxLogCtrl* m_log_list;
	wxCheckBox* m_filter_message;
};
