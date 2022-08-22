#pragma once

#include "input/api/ControllerProvider.h"

#ifndef HAS_KEYBOARD
#define HAS_KEYBOARD 1
#endif

class KeyboardControllerProvider : public ControllerProviderBase
{
	friend class KeyboardController;
public:
	inline static InputAPI::Type kAPIType = InputAPI::Keyboard;
	InputAPI::Type api() const override { return kAPIType; }

	std::vector<std::shared_ptr<ControllerBase>> get_controllers() override;
};
