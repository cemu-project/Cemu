#include "gui/input/HotkeySettings.h"
#include <config/ActiveSettings.h>
#include <gui/guiWrapper.h>
#include "input/InputManager.h"
#include "HotkeySettings.h"

#if BOOST_OS_LINUX || BOOST_OS_MACOS
#include "resource/embedded/resources.h"
#endif

extern WindowInfo g_window_info;
const std::unordered_map<sHotkeyCfg*, std::function<void(void)>> HotkeySettings::s_cfgHotkeyToFuncMap{
	{&s_cfgHotkeys.toggleFullscreen, [](void) { s_mainWindow->ShowFullScreen(!s_mainWindow->IsFullScreen()); }},
	{&s_cfgHotkeys.toggleFullscreenAlt, [](void) { s_mainWindow->ShowFullScreen(!s_mainWindow->IsFullScreen()); }},
	{&s_cfgHotkeys.exitFullscreen, [](void) { s_mainWindow->ShowFullScreen(false); }},
	{&s_cfgHotkeys.takeScreenshot, [](void) { g_window_info.has_screenshot_request = true; }},
	{&s_cfgHotkeys.toggleFastForward, [](void) { ActiveSettings::SetTimerShiftFactor((ActiveSettings::GetTimerShiftFactor() < 3) ? 3 : 1); }},
};

struct HotkeyEntry
{
	enum class InputButtonType : wxWindowID {
		Keyboard,
		Controller,
	};

	std::unique_ptr<wxStaticText> name;
	std::unique_ptr<wxButton> keyInput;
	std::unique_ptr<wxButton> controllerInput;
	sHotkeyCfg& hotkey;

	HotkeyEntry(wxStaticText* name, wxButton* keyInput, wxButton* controllerInput, sHotkeyCfg& hotkey)
		: name(name), keyInput(keyInput), controllerInput(controllerInput), hotkey(hotkey)
	{
		keyInput->SetClientData(&hotkey);
		keyInput->SetId(static_cast<wxWindowID>(InputButtonType::Keyboard));
		controllerInput->SetClientData(&hotkey);
		controllerInput->SetId(static_cast<wxWindowID>(InputButtonType::Controller));
	}
};

HotkeySettings::HotkeySettings(wxWindow* parent)
	: wxFrame(parent, wxID_ANY, "Hotkey Settings")
{
	SetIcon(wxICON(X_HOTKEY_SETTINGS));

	m_sizer = new wxFlexGridSizer(0, 3, 10, 10);
	m_sizer->AddGrowableCol(1);
	m_sizer->AddGrowableCol(2);

	m_panel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE);
	m_panel->SetSizer(m_sizer);
	Center();

	SetActiveController();

	CreateColumnHeaders();

	/* global modifier */
	CreateHotkeyRow("Hotkey modifier", s_cfgHotkeys.modifiers);
	m_hotkeys.at(0).keyInput->Hide();

	/* hotkeys */
	CreateHotkeyRow("Toggle fullscreen", s_cfgHotkeys.toggleFullscreen);
	CreateHotkeyRow("Take screenshot", s_cfgHotkeys.takeScreenshot);
	CreateHotkeyRow("Toggle fast-forward", s_cfgHotkeys.toggleFastForward);

	m_controllerTimer = new wxTimer(this);
	Bind(wxEVT_TIMER, &HotkeySettings::OnControllerTimer, this);

	m_sizer->SetSizeHints(this);
}

HotkeySettings::~HotkeySettings()
{
	m_controllerTimer->Stop();
	if (m_needToSave)
	{
		g_config.Save();
	}
}

void HotkeySettings::Init(wxFrame* mainWindowFrame)
{
	s_keyboardHotkeyToFuncMap.reserve(s_cfgHotkeyToFuncMap.size());
	for (const auto& [cfgHotkey, func] : s_cfgHotkeyToFuncMap)
	{
		auto keyboardHotkey = cfgHotkey->keyboard.raw;
		if (keyboardHotkey > 0)
		{
			s_keyboardHotkeyToFuncMap[keyboardHotkey] = func;
		}
		auto controllerHotkey = cfgHotkey->controller;
		if (controllerHotkey > 0)
		{
			s_controllerHotkeyToFuncMap[controllerHotkey] = func;
		}
	}
	s_mainWindow = mainWindowFrame;
}

void HotkeySettings::CreateColumnHeaders(void)
{
	auto* emptySpace = new wxStaticText(m_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER_HORIZONTAL);
	auto* keyboard = new wxStaticText(m_panel, wxID_ANY, "Keyboard", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER_HORIZONTAL);
	auto* controller = new wxStaticText(m_panel, wxID_ANY, "Controller", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER_HORIZONTAL);

	keyboard->SetMinSize(m_minButtonSize);
	controller->SetMinSize(m_minButtonSize);

	auto flags = wxSizerFlags().Expand();
	m_sizer->Add(emptySpace, flags);
	m_sizer->Add(keyboard, flags);
	m_sizer->Add(controller, flags);
}

void HotkeySettings::CreateHotkeyRow(const wxString& label, sHotkeyCfg& cfgHotkey)
{
	auto* name = new wxStaticText(m_panel, wxID_ANY, label);
	auto* keyInput = new wxButton(m_panel, wxID_ANY, To_wxString(cfgHotkey.keyboard), wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS | wxBU_EXACTFIT);
	auto* controllerInput = new wxButton(m_panel, wxID_ANY, To_wxString(cfgHotkey.controller), wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS | wxBU_EXACTFIT);

	/* for starting input */
	keyInput->Bind(wxEVT_BUTTON, &HotkeySettings::OnKeyboardHotkeyInputLeftClick, this);
	controllerInput->Bind(wxEVT_BUTTON, &HotkeySettings::OnControllerHotkeyInputLeftClick, this);

	/* for cancelling and clearing input */
	keyInput->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(HotkeySettings::OnHotkeyInputRightClick), NULL, this);
	controllerInput->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(HotkeySettings::OnHotkeyInputRightClick), NULL, this);

	keyInput->SetMinSize(m_minButtonSize);
	controllerInput->SetMinSize(m_minButtonSize);

	auto flags = wxSizerFlags().Expand();
	m_sizer->Add(name, flags);
	m_sizer->Add(keyInput, flags);
	m_sizer->Add(controllerInput, flags);

	m_hotkeys.emplace_back(name, keyInput, controllerInput, cfgHotkey);
}

void HotkeySettings::OnControllerTimer(wxTimerEvent& event)
{
	if (m_activeController.expired())
	{
		m_controllerTimer->Stop();
		return;
	}
	auto& controller = *m_activeController.lock();
	auto buttons = controller.update_state().buttons;
	if (!buttons.IsIdle())
	{
		for (const auto& newHotkey : buttons.GetButtonList())
		{
			m_controllerTimer->Stop();
			auto* inputButton = static_cast<wxButton*>(m_controllerTimer->GetClientData());
			auto& cfgHotkey = *static_cast<sHotkeyCfg*>(inputButton->GetClientData());
			const auto oldHotkey = cfgHotkey.controller;
			const bool is_modifier = (&cfgHotkey == &s_cfgHotkeys.modifiers);
			/* ignore same hotkeys and block duplicate hotkeys */
			if ((newHotkey != oldHotkey) && (is_modifier || (newHotkey != s_cfgHotkeys.modifiers.controller)) &&
				(s_controllerHotkeyToFuncMap.find(newHotkey) == s_controllerHotkeyToFuncMap.end()))
			{
				m_needToSave |= true;
				cfgHotkey.controller = newHotkey;
				/* don't bind modifier to map */
				if (!is_modifier) {
					s_controllerHotkeyToFuncMap.erase(oldHotkey);
					s_controllerHotkeyToFuncMap[newHotkey] = s_cfgHotkeyToFuncMap.at(&cfgHotkey);
				}
			}
			FinalizeInput<ControllerHotkey_t>(inputButton);
			return;
		}
	}
}

void HotkeySettings::OnKeyboardHotkeyInputLeftClick(wxCommandEvent& event)
{
	auto* inputButton = static_cast<wxButton*>(event.GetEventObject());
	if (m_activeInputButton)
	{
		/* ignore multiple clicks of the same button */
		if (inputButton == m_activeInputButton) return;
		RestoreInputButton(m_activeInputButton);
	}
	inputButton->Bind(wxEVT_KEY_UP, &HotkeySettings::OnKeyUp, this);
	inputButton->SetLabelText('_');
	m_activeInputButton = inputButton;
}

void HotkeySettings::OnControllerHotkeyInputLeftClick(wxCommandEvent& event)
{
	auto* inputButton = static_cast<wxButton*>(event.GetEventObject());
	if (m_activeInputButton)
	{
		/* ignore multiple clicks of the same button */
		if (inputButton == m_activeInputButton) return;
		RestoreInputButton(m_activeInputButton);
	}
	m_controllerTimer->Stop();
	if (!SetActiveController())
	{
		return;
	}
	inputButton->SetLabelText('_');
	m_controllerTimer->SetClientData(inputButton);
	m_controllerTimer->Start(25);
	m_activeInputButton = inputButton;
}

void HotkeySettings::OnHotkeyInputRightClick(wxMouseEvent& event)
{
	if (m_activeInputButton)
	{
		RestoreInputButton(m_activeInputButton);
		return;
	}
	auto* inputButton = static_cast<wxButton*>(event.GetEventObject());
	auto& cfgHotkey = *static_cast<sHotkeyCfg*>(inputButton->GetClientData());
	using InputButtonType = HotkeyEntry::InputButtonType;
	switch (static_cast<InputButtonType>(inputButton->GetId()))
	{
	case InputButtonType::Keyboard: {
		uKeyboardHotkey newHotkey{};
		if (cfgHotkey.keyboard.raw != newHotkey.raw)
		{
			m_needToSave |= true;
			s_keyboardHotkeyToFuncMap.erase(cfgHotkey.keyboard.raw);
			cfgHotkey.keyboard = newHotkey;
			FinalizeInput<uKeyboardHotkey>(inputButton);
		}
	} break;
	case InputButtonType::Controller: {
		ControllerHotkey_t newHotkey{ -1 };
		if (cfgHotkey.controller != newHotkey)
		{
			m_needToSave |= true;
			s_controllerHotkeyToFuncMap.erase(cfgHotkey.controller);
			cfgHotkey.controller = newHotkey;
			FinalizeInput<ControllerHotkey_t>(inputButton);
		}
	} break;
	default: break;
	}
}

bool HotkeySettings::SetActiveController(void)
{
	auto emulatedController = InputManager::instance().get_controller(0);
	if (emulatedController.use_count() <= 1)
	{
		return false;
	}
	const auto& controllers = emulatedController->get_controllers();
	if (controllers.empty())
	{
		return false;
	}
	m_activeController = controllers.at(0);
	return true;
}

void HotkeySettings::OnKeyUp(wxKeyEvent& event)
{
	auto* inputButton = static_cast<wxButton*>(event.GetEventObject());
	auto& cfgHotkey = *static_cast<sHotkeyCfg*>(inputButton->GetClientData());
	if (auto keycode = event.GetKeyCode(); IsValidKeycodeUp(keycode))
	{
		auto oldHotkey = cfgHotkey.keyboard;
		uKeyboardHotkey newHotkey{};
		newHotkey.key = keycode;
		newHotkey.alt = event.AltDown();
		newHotkey.ctrl = event.ControlDown();
		newHotkey.shift = event.ShiftDown();
		if ((newHotkey.raw != oldHotkey.raw) &&
			(s_keyboardHotkeyToFuncMap.find(newHotkey.raw) == s_keyboardHotkeyToFuncMap.end()))
		{
			m_needToSave |= true;
			cfgHotkey.keyboard = newHotkey;
			s_keyboardHotkeyToFuncMap.erase(oldHotkey.raw);
			s_keyboardHotkeyToFuncMap[newHotkey.raw] = s_cfgHotkeyToFuncMap.at(&cfgHotkey);
		}
	}
	FinalizeInput<uKeyboardHotkey>(inputButton);
}

template <typename T>
void HotkeySettings::FinalizeInput(wxButton* inputButton)
{
	auto& cfgHotkey = *static_cast<sHotkeyCfg*>(inputButton->GetClientData());
	if constexpr (std::is_same_v<T, uKeyboardHotkey>)
	{
		inputButton->Unbind(wxEVT_KEY_UP, &HotkeySettings::OnKeyUp, this);
		inputButton->SetLabelText(To_wxString(cfgHotkey.keyboard));
	} else if constexpr (std::is_same_v<T, ControllerHotkey_t>)
	{
		inputButton->SetLabelText(To_wxString(cfgHotkey.controller));
	}
	m_activeInputButton = nullptr;
}

void HotkeySettings::RestoreInputButton(wxButton* inputButton)
{
	using InputButtonType = HotkeyEntry::InputButtonType;
	switch (static_cast<InputButtonType>(inputButton->GetId()))
	{
	case InputButtonType::Keyboard: {
		FinalizeInput<uKeyboardHotkey>(inputButton);
	} break;
	case InputButtonType::Controller: {
		FinalizeInput<ControllerHotkey_t>(inputButton);
	} break;
	default: break;
	}
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

wxString HotkeySettings::To_wxString(uKeyboardHotkey hotkey)
{
	wxString ret{};
	if (hotkey.raw)
	{
		if (hotkey.alt)
		{
			ret.append("ALT + ");
		}
		if (hotkey.ctrl)
		{
			ret.append("CTRL + ");
		}
		if (hotkey.shift)
		{
			ret.append("SHIFT + ");
		}
		ret.append(wxAcceleratorEntry(0, hotkey.key).ToString());
	}
	return ret;
}

wxString HotkeySettings::To_wxString(ControllerHotkey_t hotkey)
{
	wxString ret{};
	if ((hotkey != -1) && !m_activeController.expired())
	{
		ret = m_activeController.lock()->get_button_name(hotkey);
	}
	return ret;
}
