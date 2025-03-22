#pragma once

#include <wx/wx.h>
#include "config/CemuConfig.h"

class HotkeyEntry;

class HotkeySettings : public wxFrame
{
public:
	static void init(wxFrame* main_window_frame);
	inline static std::unordered_map<int, std::function<void(void)>> hotkey_to_func_map{};

	HotkeySettings(wxWindow* parent);
	~HotkeySettings();

private:
	wxPanel* m_panel;
	wxFlexGridSizer* m_sizer;
	std::vector<HotkeyEntry> m_hotkeys;
	wxButton* active_hotkey_input_btn = nullptr;
	bool m_need_to_save = false;

	/* helpers */
	void create_hotkey(const wxString& label, uHotkey& hotkey);
	wxString hotkey_to_wxstring(uHotkey hotkey);
	bool is_valid_keycode_up(int keycode);

	/* events */
	void on_hotkey_input_click(wxCommandEvent& event);
	void on_key_up(wxKeyEvent& event);
};
