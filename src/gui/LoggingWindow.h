#pragma once

#include <wx/frame.h>
#include <wx/listbox.h>
#include <wx/combobox.h>
#include "gui/components/wxLogCtrl.h"

class wxLogEvent;

class LoggingWindow : public wxFrame
{
public:
	LoggingWindow(wxFrame* parent);
	~LoggingWindow();

	static void Log(std::string_view filter, std::string_view message);
	static void Log(std::string_view message) { Log("", message); }
	static void Log(std::string_view filter, std::wstring_view message);
	static void Log(std::wstring_view message){ Log("", message); }

	template<typename ...TArgs>
	static void Log(std::string_view filter, std::string_view format, TArgs&&... args)
	{
		Log(filter, fmt::format(format, std::forward<TArgs>(args)...));
	}

	template<typename ...TArgs>
	static void Log(std::string_view filter, std::wstring_view format, TArgs&&... args)
	{
		Log(filter, fmt::format(format, std::forward<TArgs>(args)...));
	}
private:
	void OnLogMessage(wxLogEvent& event);
	void OnFilterChange(wxCommandEvent& event);
	void OnFilterMessageChange(wxCommandEvent& event);

	wxComboBox* m_filter;
	wxLogCtrl* m_log_list;
	wxCheckBox* m_filter_message;

	inline static std::shared_mutex s_mutex;
	inline static LoggingWindow* s_instance = nullptr;
};

