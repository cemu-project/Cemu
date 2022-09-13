#include "input/api/Keyboard/KeyboardController.h"
#include "input/api/Keyboard/KeyToString.h"

#include "gui/guiWrapper.h"

KeyboardController::KeyboardController()
	: base_type("keyboard", "Keyboard")
{
	
}

std::string KeyboardController::get_button_name(uint64 button) const
{
	return ImGuiKey_to_string(button);
}

extern WindowInfo g_window_info;

ControllerState KeyboardController::raw_state()
{
	ControllerState result{};
	if (g_window_info.app_active)
	{
		g_window_info.get_keystates(result.buttons);
	}
	return result;
}
