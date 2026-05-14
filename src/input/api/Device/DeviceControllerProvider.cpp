#include "DeviceControllerProvider.h"
#include "DeviceController.h"

std::vector<std::shared_ptr<ControllerBase>> DeviceControllerProvider::get_controllers()
{
	return {std::make_shared<DeviceController>()};
}
