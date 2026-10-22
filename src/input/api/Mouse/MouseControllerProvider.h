#pragma once

#include "input/api/ControllerProvider.h"

#ifndef HAS_MOUSE
#define HAS_MOUSE 1
#endif

class MouseControllerProvider : public ControllerProviderBase
{
public:
	inline static InputAPI::Type kAPIType = InputAPI::Mouse;
	InputAPI::Type api() const override { return kAPIType; }

	std::vector<std::shared_ptr<ControllerBase>> get_controllers() override;
};

