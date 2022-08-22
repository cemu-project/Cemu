#pragma once

#include "input/api/InputAPI.h"
#include "input/api/Controller.h"
#include "input/emulated/EmulatedController.h"

class ControllerFactory
{
public:
	static ControllerPtr CreateController(InputAPI::Type api, std::string_view uuid, std::string_view display_name);
	static EmulatedControllerPtr CreateEmulatedController(size_t player_index, EmulatedController::Type type);
	static ControllerProviderPtr CreateControllerProvider(InputAPI::Type api, const ControllerProviderSettings& settings);
};
