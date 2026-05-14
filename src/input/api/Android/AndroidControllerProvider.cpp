#include "input/api/Android/AndroidControllerProvider.h"

#include "ControllerManager.h"
#include "AndroidController.h"

std::vector<std::shared_ptr<ControllerBase>> AndroidControllerProvider::get_controllers()
{
	std::vector<std::shared_ptr<ControllerBase>> result;
	auto controllers = ControllerManager::instance().get_controllers();
	result.reserve(controllers.size());

	for (const auto& controller : controllers)
	{
		result.push_back(std::make_shared<AndroidController>(controller.descriptor, controller.name));
	}

	return result;
}
