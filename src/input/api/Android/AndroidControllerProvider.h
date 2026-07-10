#pragma once

#include "input/api/ControllerProvider.h"

class AndroidControllerProvider : public ControllerProviderBase
{
  public:
	inline static InputAPI::Type kAPIType = InputAPI::Android;

	InputAPI::Type api() const override
	{
		return kAPIType;
	}

	std::vector<std::shared_ptr<ControllerBase>> get_controllers() override;
};
