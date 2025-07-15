#include <boost/container/small_vector.hpp>

#include "input/api/Keyboard/KeyboardController.h"
#include "WindowSystem.h"

KeyboardController::KeyboardController()
	: base_type("keyboard", "Keyboard")
{
	
}

std::string KeyboardController::get_button_name(uint64 button) const
{
	return WindowSystem::GetKeyCodeName(button);
}

ControllerState KeyboardController::raw_state()
{
	ControllerState result{};
	boost::container::small_vector<uint32, 16> pressedKeys;
	WindowSystem::GetWindowInfo().iter_keystates([&pressedKeys](const std::pair<const uint32, bool>& keyState) { if (keyState.second) pressedKeys.emplace_back(keyState.first); });
	result.buttons.SetPressedButtons(pressedKeys);
	return result;
}
