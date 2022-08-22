#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/timer.h>
#include <wx/slider.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/listbase.h>
#include <wx/display.h>
#include <wx/aboutdlg.h>
#include <wx/dialog.h>
#include <wx/timer.h>
#include <wx/gauge.h>
#include <wx/dataview.h>
#include <wx/statline.h>
#include <wx/wrapsizer.h>
#include <wx/gbsizer.h>
#include <wx/radiobox.h>
#include <wx/combobox.h>
#include <wx/sizer.h>
#include <wx/scrolbar.h>
#include <wx/richtext/richtextctrl.h>
#include <wx/dcbuffer.h>
#include <wx/combo.h>
#include <wx/wupdlock.h>
#include <wx/infobar.h>
#include <wx/splitter.h>

extern bool g_inputConfigWindowHasFocus;

// wx helper functions
#include <wx/string.h>
struct wxStringFormatParameters
{
	sint32 parameter_index;
	sint32 parameter_count;

	wchar_t* token_buffer;
	wchar_t* substitude_parameter;
};

template <typename ...Args>
wxString wxStringFormat(std::wstring& format, wxStringFormatParameters& parameters)
{
	return format;
}

template <typename T, typename ...Args>
wxString wxStringFormat(std::wstring& format, wxStringFormatParameters& parameters, T arg, Args... args)
{
	wchar_t tmp[64];
	swprintf(tmp, 64, LR"(\{[%d]+\})", parameters.parameter_index);
	const std::wregex placeholder_regex(tmp);

	auto result = format;
	while (std::regex_search(result, placeholder_regex))
	{
		result = std::regex_replace(result, placeholder_regex, parameters.substitude_parameter, std::regex_constants::format_first_only);
		result = wxString::Format(wxString(result), arg);
	}

	parameters.parameter_index++;
	if (parameters.parameter_index == parameters.parameter_count)
		return result;

	parameters.substitude_parameter = std::wcstok(nullptr, LR"( )", &parameters.token_buffer);
	return wxStringFormat(result, parameters, args...);
}

template <typename ...T>
wxString wxStringFormat(const wxString& format, const wchar_t* parameters, T... args)
{
	const auto parameter_count = std::count(parameters, parameters + wcslen(parameters), '%');
	if (parameter_count == 0)
		return format;

	const auto copy = wcsdup(parameters);

	wxStringFormatParameters para;
	para.substitude_parameter = std::wcstok(copy, LR"( )", &para.token_buffer);
	para.parameter_count = parameter_count;
	para.parameter_index = 0;

	auto tmp_string = format.ToStdWstring();
	auto result = wxStringFormat(tmp_string, para, args...);

	free(copy);

	return result;
}

inline bool SendSliderEvent(wxSlider* slider, int new_value)
{
	wxCommandEvent cevent(wxEVT_SLIDER, slider->GetId());
	cevent.SetInt(new_value);
	cevent.SetEventObject(slider);
	return slider->HandleWindowEvent(cevent);
}
