#pragma once

#include "input/api/ControllerProvider.h"

class DeviceControllerProvider : public ControllerProviderBase
{
  public:
	inline static InputAPI::Type kAPIType = InputAPI::Device;

	InputAPI::Type api() const override
	{
		return kAPIType;
	}

	std::vector<std::shared_ptr<ControllerBase>> get_controllers() override;
};
