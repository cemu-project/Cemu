#include "gui/input/HotkeySettings.h"
#include <config/ActiveSettings.h>
#include <gui/guiWrapper.h>

extern WindowInfo g_window_info;
wxFrame* s_main_window = nullptr;

static auto& cfg_hotkeys = GetConfig().hotkeys;
static const std::unordered_map<uHotkey*, std::function<void(void)>> cfg_hotkey_to_func_map{
	{&cfg_hotkeys.toggle_fastforward, [](void) {ActiveSettings::SetTimerShiftFactor((ActiveSettings::GetTimerShiftFactor() < 3) ? 3 : 1);}},
	{&cfg_hotkeys.toggle_fullscreen, [](void) { s_main_window->ShowFullScreen(!s_main_window->IsFullScreen()); }},
	{&cfg_hotkeys.toggle_fullscreen_alt, [](void) { s_main_window->ShowFullScreen(!s_main_window->IsFullScreen()); }},
	{&cfg_hotkeys.exit_fullscreen, [](void) { s_main_window->ShowFullScreen(false); }},
	{&cfg_hotkeys.take_screenshot, [](void) { g_window_info.has_screenshot_request = true; }},
};

struct HotkeyEntry
{
	std::unique_ptr<wxStaticText> name;
	std::unique_ptr<wxButton> key_input;
	uHotkey& hotkey;

	HotkeyEntry(wxStaticText* name, wxButton* key_input, uHotkey& hotkey)
		: name(name), key_input(key_input), hotkey(hotkey)
	{
		key_input->SetClientData(&hotkey);
	}
};

HotkeySettings::HotkeySettings(wxWindow* parent)
	: wxFrame(parent, wxID_ANY, "Hotkey Settings")
{
	m_panel = new wxPanel(this);
	m_sizer = new wxFlexGridSizer(0, 2, wxSize(0, 0));

	m_panel->SetSizer(m_sizer);
	Center();

	create_hotkey("Toggle fullscreen", cfg_hotkeys.toggle_fullscreen);
	create_hotkey("Take screenshot", cfg_hotkeys.take_screenshot);

	m_sizer->SetSizeHints(this);
}

HotkeySettings::~HotkeySettings()
{
	if (m_need_to_save)
	{
		g_config.Save();
	}
}

void HotkeySettings::init(wxFrame* main_window_frame)
{
	hotkey_to_func_map.reserve(cfg_hotkey_to_func_map.size());
	for (const auto& [cfg_hotkey, func] : cfg_hotkey_to_func_map)
	{
		auto hotkey_raw = cfg_hotkey->raw;
		if (hotkey_raw > 0)
		{
			hotkey_to_func_map[hotkey_raw] = func;
		}
	}
	s_main_window = main_window_frame;
}

void HotkeySettings::create_hotkey(const wxString& label, uHotkey& hotkey)
{
	/* add new hotkey */
	{
		auto* name = new wxStaticText(m_panel, wxID_ANY, label, wxDefaultPosition, wxSize(240, 30), wxALIGN_CENTER);
		auto* key_input = new wxButton(m_panel, wxID_ANY, hotkey_to_wxstring(hotkey), wxDefaultPosition, wxSize(120, 30), wxWANTS_CHARS);

		key_input->Bind(wxEVT_BUTTON, &HotkeySettings::on_hotkey_input_click, this);

		auto flags = wxSizerFlags().Expand();
		m_sizer->Add(name, flags);
		m_sizer->Add(key_input, flags);

		m_hotkeys.emplace_back(name, key_input, hotkey);
	}
}

void HotkeySettings::on_hotkey_input_click(wxCommandEvent& event)
{
	if (active_hotkey_input_btn)
	{
		wxKeyEvent revert_event{};
		revert_event.SetEventObject(active_hotkey_input_btn);
		on_key_up(revert_event);
	}
	auto* obj = static_cast<wxButton*>(event.GetEventObject());
	obj->Bind(wxEVT_KEY_UP, &HotkeySettings::on_key_up, this);
	obj->SetLabelText('_');
	active_hotkey_input_btn = obj;
}

void HotkeySettings::on_key_up(wxKeyEvent& event)
{
	active_hotkey_input_btn = nullptr;
	auto* obj = static_cast<wxButton*>(event.GetEventObject());
	obj->Unbind(wxEVT_KEY_UP, &HotkeySettings::on_key_up, this);
	auto& cfg_hotkey = *static_cast<uHotkey*>(obj->GetClientData());
	if (auto keycode = event.GetKeyCode(); is_valid_keycode_up(keycode))
	{
		auto old_hotkey = cfg_hotkey;
		uHotkey new_hotkey{};
		new_hotkey.key = keycode;
		new_hotkey.alt = event.AltDown();
		new_hotkey.ctrl = event.ControlDown();
		new_hotkey.shift = event.ShiftDown();
		if ((new_hotkey.raw != old_hotkey.raw) &&
			(hotkey_to_func_map.find(new_hotkey.raw) == hotkey_to_func_map.end()))
		{
			m_need_to_save |= true;
			cfg_hotkey = new_hotkey;
			hotkey_to_func_map.erase(old_hotkey.raw);
			hotkey_to_func_map[cfg_hotkey.raw] = cfg_hotkey_to_func_map.at(&cfg_hotkey);
		}
	}
	obj->SetLabelText(hotkey_to_wxstring(cfg_hotkey));
}

bool HotkeySettings::is_valid_keycode_up(int keycode)
{
	switch (keycode)
	{
	case WXK_NONE:
	case WXK_ESCAPE:
	case WXK_ALT:
	case WXK_CONTROL:
	case WXK_SHIFT:
		return false;
	default:
		return true;
	}
}

wxString HotkeySettings::hotkey_to_wxstring(uHotkey hotkey)
{
	if (hotkey.raw <= 0)
	{
		return "";
	}
	wxString ret_val;
	if (hotkey.alt)
	{
		ret_val.append("ALT + ");
	}
	if (hotkey.ctrl)
	{
		ret_val.append("CTRL + ");
	}
	if (hotkey.shift)
	{
		ret_val.append("SHIFT + ");
	}
	ret_val.append(wxAcceleratorEntry(0, hotkey.key).ToString());
	return ret_val;
}
