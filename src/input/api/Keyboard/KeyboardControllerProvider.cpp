#include "input/api/Keyboard/KeyboardControllerProvider.h"

#include "input/api/Keyboard/KeyboardController.h"

std::vector<std::shared_ptr<ControllerBase>> KeyboardControllerProvider::get_controllers()
{
	std::vector<std::shared_ptr<ControllerBase>> result;
	result.emplace_back(std::make_shared<KeyboardController>());
	return result;
}
