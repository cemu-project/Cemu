#include <boost/container/small_vector.hpp>

#include "Cemu/GuiSystem/GuiSystem.h"
#include "input/api/Keyboard/KeyboardController.h"

KeyboardController::KeyboardController()
	: base_type("keyboard", "Keyboard")
{
	
}

std::string KeyboardController::get_button_name(uint64 button) const
{
	return GuiSystem::keyCodeToString(button);
}

ControllerState KeyboardController::raw_state()
{
	ControllerState result{};
	boost::container::small_vector<uint32, 16> pressedKeys;
	GuiSystem::getWindowInfo().iter_keystates([&pressedKeys](const std::pair<const uint32, bool>& keyState) { if (keyState.second) pressedKeys.emplace_back(keyState.first); });
	result.buttons.SetPressedButtons(pressedKeys);
	return result;
}
