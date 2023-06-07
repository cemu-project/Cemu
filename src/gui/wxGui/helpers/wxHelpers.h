#pragma once

#include <wx/control.h>
#include <wx/listbase.h>
#include <wx/string.h>

template <>
struct fmt::formatter<wxString> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const wxString& str, FormatContext& ctx)
	{
		return formatter<string_view>::format(str.c_str().AsChar(), ctx);
	}
};

class wxTempEnable
{
public:
	wxTempEnable(wxControl* control, bool state = true)
		: m_control(control), m_state(state)
	{
		m_control->Enable(m_state);
	}

	wxTempEnable(const wxTempEnable&) = delete;
	wxTempEnable(wxTempEnable&&) noexcept = default;

	~wxTempEnable()
	{
		if(m_control)
			m_control->Enable(!m_state);
	}

private:
	wxControl* m_control;
	const bool m_state;
};

class wxTempDisable : wxTempEnable
{
public:
	wxTempDisable(wxControl* control)
		: wxTempEnable(control, false) {}
};

template<typename ...TArgs>
wxString wxStringFormat2(const wxString& format, TArgs&&...args)
{
	// ignores locale?
	return fmt::format(fmt::runtime(format.ToStdString()), std::forward<TArgs>(args)...);
}

template<typename ...TArgs>
wxString wxStringFormat2W(const wxString& format, TArgs&&...args)
{
	return fmt::format(fmt::runtime(format.ToStdWstring()), std::forward<TArgs>(args)...);
}

// executes a function when destroying the obj
template<typename TFunction, typename ...TArgs>
class wxDTorFunc
{
public:
	wxDTorFunc(TFunction&& func, TArgs&&... args)
	{
		auto bound = std::bind(std::forward<TFunction>(func), std::forward<TArgs>(args)...);
		// m_function = [bound] { bound(); };
		m_function = [bound{ std::move(bound) }]{ bound(); };
		
	}
	~wxDTorFunc()
	{
		m_function();
	}
private:
	std::function<void()> m_function;
};

void wxAutosizeColumn(wxListCtrlBase* ctrl, int col);
void wxAutosizeColumns(wxListCtrlBase* ctrl, int col_start, int col_end);

// creates wxString from utf8 string
inline wxString to_wxString(std::string_view str)
{
	return wxString::FromUTF8(str.data(), str.size());
}

// creates utf8 std::string from wxString
inline std::string from_wxString(const wxString& str)
{
	const auto tmp = str.ToUTF8();
	return std::string{ tmp.data(), tmp.length() };
}


template <typename T>
T get_next_sibling(const T element)
{
	static_assert(std::is_pointer_v<T> && std::is_base_of_v<wxControl, std::remove_pointer_t<T>>, "element must be a pointer and inherit from wxControl");
	for (auto sibling = element->GetNextSibling(); sibling; sibling = sibling->GetNextSibling())
	{
		if (auto result = dynamic_cast<T>(sibling))
			return result;
	}

	return nullptr;
}

template <typename T>
T get_prev_sibling(const T element)
{
	static_assert(std::is_pointer_v<T> && std::is_base_of_v<wxControl, std::remove_pointer_t<T>>, "element must be a pointer and inherit from wxControl");
	for (auto sibling = element->GetPrevSibling(); sibling; sibling = sibling->GetPrevSibling())
	{
		auto result = dynamic_cast<T>(sibling);
		if (result)
			return result;
	}

	return nullptr;
}

void update_slider_text(wxCommandEvent& event, const wxFormatString& format = "%d%%");

uint32 fix_raw_keycode(uint32 keycode, uint32 raw_flags);
