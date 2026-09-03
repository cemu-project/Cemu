#include "MouseControllerProvider.h"

#include "MouseController.h"

std::vector<std::shared_ptr<ControllerBase>> MouseControllerProvider::get_controllers()
{
	std::vector<std::shared_ptr<ControllerBase>> result;
	result.emplace_back(std::make_shared<MouseController>());
	return result;
}
