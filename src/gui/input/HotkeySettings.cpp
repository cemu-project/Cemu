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
};

struct HotkeyEntry
{
	std::unique_ptr<wxStaticText> name;
	std::unique_ptr<wxButton> keyInput;
	std::unique_ptr<wxButton> controllerInput;
	sHotkeyCfg& hotkey;

	HotkeyEntry(wxStaticText* name, wxButton* keyInput, wxButton* controllerInput, sHotkeyCfg& hotkey)
		: name(name), keyInput(keyInput), controllerInput(controllerInput), hotkey(hotkey)
	{
		keyInput->SetClientData(&hotkey);
		controllerInput->SetClientData(&hotkey);
	}
};

HotkeySettings::HotkeySettings(wxWindow* parent)
	: wxFrame(parent, wxID_ANY, _("Hotkey Settings"))
{
	SetIcon(wxICON(X_HOTKEY_SETTINGS));

	m_sizer = new wxFlexGridSizer(0, 3, 5, 5);
	m_sizer->AddGrowableCol(1);
	m_sizer->AddGrowableCol(2);

	m_panel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE);
	m_panel->SetSizer(m_sizer);
	m_panel->SetBackgroundColour(*wxWHITE);

	Center();

	SetActiveController();

	CreateColumnHeaders();

	/* global modifier */
	CreateHotkeyRow("Hotkey modifier", s_cfgHotkeys.modifiers);
	m_hotkeys.at(0).keyInput->Hide();

	/* hotkeys */
	CreateHotkeyRow("Toggle fullscreen", s_cfgHotkeys.toggleFullscreen);
	CreateHotkeyRow("Take screenshot", s_cfgHotkeys.takeScreenshot);

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
		if (keyboardHotkey > sHotkeyCfg::keyboardNone)
		{
			s_keyboardHotkeyToFuncMap[keyboardHotkey] = func;
		}
		auto controllerHotkey = cfgHotkey->controller;
		if (controllerHotkey > sHotkeyCfg::controllerNone)
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
	keyInput->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(HotkeySettings::OnKeyboardHotkeyInputRightClick), NULL, this);
	controllerInput->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(HotkeySettings::OnControllerHotkeyInputRightClick), NULL, this);

	keyInput->SetMinSize(m_minButtonSize);
	controllerInput->SetMinSize(m_minButtonSize);

#if BOOST_OS_WINDOWS
	const wxColour inputButtonColor = 0xfafafa;
#else
	const wxColour inputButtonColor = GetBackgroundColour();
#endif
	keyInput->SetBackgroundColour(inputButtonColor);
	controllerInput->SetBackgroundColour(inputButtonColor);

	auto flags = wxSizerFlags(1).Expand().Border(wxALL, 5).CenterVertical();
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
			const bool isModifier = (&cfgHotkey == &s_cfgHotkeys.modifiers);
			/* ignore same hotkeys and block duplicate hotkeys */
			if ((newHotkey != oldHotkey) && (isModifier || (newHotkey != s_cfgHotkeys.modifiers.controller)) &&
				(s_controllerHotkeyToFuncMap.find(newHotkey) == s_controllerHotkeyToFuncMap.end()))
			{
				m_needToSave |= true;
				cfgHotkey.controller = newHotkey;
				/* don't bind modifier to map */
				if (!isModifier)
				{
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
		RestoreInputButton<uKeyboardHotkey>();
	}
	inputButton->Bind(wxEVT_KEY_UP, &HotkeySettings::OnKeyUp, this);
	inputButton->SetLabelText(m_editModeHotkeyText);
	m_activeInputButton = inputButton;
}

void HotkeySettings::OnControllerHotkeyInputLeftClick(wxCommandEvent& event)
{
	auto* inputButton = static_cast<wxButton*>(event.GetEventObject());
	if (m_activeInputButton)
	{
		/* ignore multiple clicks of the same button */
		if (inputButton == m_activeInputButton) return;
		RestoreInputButton<ControllerHotkey_t>();
	}
	m_controllerTimer->Stop();
	if (!SetActiveController())
	{
		return;
	}
	inputButton->SetLabelText(m_editModeHotkeyText);
	m_controllerTimer->SetClientData(inputButton);
	m_controllerTimer->Start(25);
	m_activeInputButton = inputButton;
}

void HotkeySettings::OnKeyboardHotkeyInputRightClick(wxMouseEvent& event)
{
	if (m_activeInputButton)
	{
		RestoreInputButton<uKeyboardHotkey>();
		return;
	}
	auto* inputButton = static_cast<wxButton*>(event.GetEventObject());
	auto& cfgHotkey = *static_cast<sHotkeyCfg*>(inputButton->GetClientData());
	uKeyboardHotkey newHotkey{ sHotkeyCfg::keyboardNone };
	if (cfgHotkey.keyboard.raw != newHotkey.raw)
	{
		m_needToSave |= true;
		s_keyboardHotkeyToFuncMap.erase(cfgHotkey.keyboard.raw);
		cfgHotkey.keyboard = newHotkey;
		FinalizeInput<uKeyboardHotkey>(inputButton);
	}
}

void HotkeySettings::OnControllerHotkeyInputRightClick(wxMouseEvent& event)
{
	if (m_activeInputButton)
	{
		RestoreInputButton<ControllerHotkey_t>();
		return;
	}
	auto* inputButton = static_cast<wxButton*>(event.GetEventObject());
	auto& cfgHotkey = *static_cast<sHotkeyCfg*>(inputButton->GetClientData());
	ControllerHotkey_t newHotkey{ sHotkeyCfg::controllerNone };
	if (cfgHotkey.controller != newHotkey)
	{
		m_needToSave |= true;
		s_controllerHotkeyToFuncMap.erase(cfgHotkey.controller);
		cfgHotkey.controller = newHotkey;
		FinalizeInput<ControllerHotkey_t>(inputButton);
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

template <typename T>
void HotkeySettings::RestoreInputButton(void)
{
	FinalizeInput<T>(m_activeInputButton);
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
	if (hotkey.raw == sHotkeyCfg::keyboardNone) {
		return m_disabledHotkeyText;
	}
	wxString ret{};
	if (hotkey.alt)
	{
		ret.append(_("Alt + "));
	}
	if (hotkey.ctrl)
	{
		ret.append(_("Ctrl + "));
	}
	if (hotkey.shift)
	{
		ret.append(_("Shift + "));
	}
	ret.append(wxAcceleratorEntry(0, hotkey.key).ToString());
	return ret;
}

wxString HotkeySettings::To_wxString(ControllerHotkey_t hotkey)
{
	if ((hotkey == sHotkeyCfg::controllerNone) || m_activeController.expired()) {
		return m_disabledHotkeyText;
	}
	return m_activeController.lock()->get_button_name(hotkey);
}

void HotkeySettings::CaptureInput(wxKeyEvent& event)
{
	uKeyboardHotkey hotkey{};
	hotkey.key = event.GetKeyCode();
	hotkey.alt = event.AltDown();
	hotkey.ctrl = event.ControlDown();
	hotkey.shift = event.ShiftDown();
	const auto it = s_keyboardHotkeyToFuncMap.find(hotkey.raw);
	if (it != s_keyboardHotkeyToFuncMap.end())
		it->second();
}

void HotkeySettings::CaptureInput(const ControllerState& currentState, const ControllerState& lastState)
{
	const auto& modifier = s_cfgHotkeys.modifiers.controller;
	if ((modifier >= 0) && currentState.buttons.GetButtonState(modifier))
	{
		for (const auto& buttonId : currentState.buttons.GetButtonList())
		{
			const auto it = s_controllerHotkeyToFuncMap.find(buttonId);
			if (it == s_controllerHotkeyToFuncMap.end())
				continue;
			/* only capture clicks */
			if (lastState.buttons.GetButtonState(buttonId))
				break;
			it->second();
			break;
		}
	}
}
