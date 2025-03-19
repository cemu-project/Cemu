#include "gui/input/HotkeySettings.h"
#include <config/ActiveSettings.h>
#include <gui/guiWrapper.h>

extern WindowInfo g_window_info;
const std::unordered_map<uHotkey*, std::function<void(void)>> HotkeySettings::s_cfgHotkeyToFuncMap{
	{&s_cfgHotkeys.toggleFullscreen, [](void) { s_mainWindow->ShowFullScreen(!s_mainWindow->IsFullScreen()); }},
	{&s_cfgHotkeys.toggleFullscreenAlt, [](void) { s_mainWindow->ShowFullScreen(!s_mainWindow->IsFullScreen()); }},
	{&s_cfgHotkeys.exitFullscreen, [](void) { s_mainWindow->ShowFullScreen(false); }},
	{&s_cfgHotkeys.takeScreenshot, [](void) { g_window_info.has_screenshot_request = true; }},
	{&s_cfgHotkeys.toggleFastForward, [](void) { ActiveSettings::SetTimerShiftFactor((ActiveSettings::GetTimerShiftFactor() < 3) ? 3 : 1); }},
};

struct HotkeyEntry
{
	std::unique_ptr<wxStaticText> name;
	std::unique_ptr<wxButton> keyInput;
	uHotkey& hotkey;

	HotkeyEntry(wxStaticText* name, wxButton* keyInput, uHotkey& hotkey)
		: name(name), keyInput(keyInput), hotkey(hotkey)
	{
		keyInput->SetClientData(&hotkey);
	}
};

HotkeySettings::HotkeySettings(wxWindow* parent)
	: wxFrame(parent, wxID_ANY, "Hotkey Settings")
{
	m_panel = new wxPanel(this);
	m_sizer = new wxFlexGridSizer(0, 2, wxSize(0, 0));

	m_panel->SetSizer(m_sizer);
	Center();

	CreateHotkey("Toggle fullscreen", s_cfgHotkeys.toggleFullscreen);
	CreateHotkey("Take screenshot", s_cfgHotkeys.takeScreenshot);
	CreateHotkey("Toggle fast-forward", s_cfgHotkeys.toggleFastForward);

	m_sizer->SetSizeHints(this);
}

HotkeySettings::~HotkeySettings()
{
	if (m_needToSave)
	{
		g_config.Save();
	}
}

void HotkeySettings::Init(wxFrame* mainWindowFrame)
{
	s_hotkeyToFuncMap.reserve(s_cfgHotkeyToFuncMap.size());
	for (const auto& [cfgHotkey, func] : s_cfgHotkeyToFuncMap)
	{
		auto hotkeyRaw = cfgHotkey->raw;
		if (hotkeyRaw > 0)
		{
			s_hotkeyToFuncMap[hotkeyRaw] = func;
		}
	}
	s_mainWindow = mainWindowFrame;
}

void HotkeySettings::CreateHotkey(const wxString& label, uHotkey& hotkey)
{
	/* add new hotkey */
	{
		auto* name = new wxStaticText(m_panel, wxID_ANY, label, wxDefaultPosition, wxSize(240, 30), wxALIGN_CENTER);
		auto* keyInput = new wxButton(m_panel, wxID_ANY, To_wxString(hotkey), wxDefaultPosition, wxSize(120, 30), wxWANTS_CHARS);

		keyInput->Bind(wxEVT_BUTTON, &HotkeySettings::OnHotkeyInputLeftClick, this);
		keyInput->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(HotkeySettings::OnHotkeyInputRightClick), NULL, this);

		auto flags = wxSizerFlags().Expand();
		m_sizer->Add(name, flags);
		m_sizer->Add(keyInput, flags);

		m_hotkeys.emplace_back(name, keyInput, hotkey);
	}
}

void HotkeySettings::OnHotkeyInputLeftClick(wxCommandEvent& event)
{
	auto* inputButton = static_cast<wxButton*>(event.GetEventObject());
	if (m_activeInputButton)
	{
		/* ignore multiple clicks of the same button */
		if (inputButton == m_activeInputButton) return;
		FinalizeInput(m_activeInputButton);
	}
	inputButton->Bind(wxEVT_KEY_UP, &HotkeySettings::OnKeyUp, this);
	inputButton->SetLabelText('_');
	m_activeInputButton = inputButton;
}

void HotkeySettings::OnHotkeyInputRightClick(wxMouseEvent& event) {
	if (m_activeInputButton)
	{
		FinalizeInput(m_activeInputButton);
		return;
	}
	auto* inputButton = static_cast<wxButton*>(event.GetEventObject());
	auto& cfgHotkey = *static_cast<uHotkey*>(inputButton->GetClientData());
	uHotkey newHotkey{};
	if (cfgHotkey.raw != newHotkey.raw)
	{
		m_needToSave |= true;
		s_hotkeyToFuncMap.erase(cfgHotkey.raw);
		cfgHotkey = newHotkey;
	}
	FinalizeInput(inputButton);
}

void HotkeySettings::OnKeyUp(wxKeyEvent& event)
{
	auto* inputButton = static_cast<wxButton*>(event.GetEventObject());
	auto& cfgHotkey = *static_cast<uHotkey*>(inputButton->GetClientData());
	if (auto keycode = event.GetKeyCode(); IsValidKeycodeUp(keycode))
	{
		auto oldHotkey = cfgHotkey;
		uHotkey newHotkey{};
		newHotkey.key = keycode;
		newHotkey.alt = event.AltDown();
		newHotkey.ctrl = event.ControlDown();
		newHotkey.shift = event.ShiftDown();
		if ((newHotkey.raw != oldHotkey.raw) &&
			(s_hotkeyToFuncMap.find(newHotkey.raw) == s_hotkeyToFuncMap.end()))
		{
			m_needToSave |= true;
			cfgHotkey = newHotkey;
			s_hotkeyToFuncMap.erase(oldHotkey.raw);
			s_hotkeyToFuncMap[cfgHotkey.raw] = s_cfgHotkeyToFuncMap.at(&cfgHotkey);
		}
	}
	FinalizeInput(inputButton);
}

void HotkeySettings::FinalizeInput(wxButton* inputButton)
{
	auto& cfgHotkey = *static_cast<uHotkey*>(inputButton->GetClientData());
	inputButton->Unbind(wxEVT_KEY_UP, &HotkeySettings::OnKeyUp, this);
	inputButton->SetLabelText(To_wxString(cfgHotkey));
	m_activeInputButton = nullptr;
}

bool HotkeySettings::IsValidKeycodeUp(int keycode)
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

wxString HotkeySettings::To_wxString(uHotkey hotkey)
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
