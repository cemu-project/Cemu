#pragma once

#include "MouseControllerProvider.h"
#include "input/api/Controller.h"


class MouseController : public Controller<MouseControllerProvider>
{
public:
	MouseController() : base_type("mouse", "Mouse") {}

	std::string_view api_name() const override;
	InputAPI::Type api() const override  { return InputAPI::Mouse; }
	bool is_connected() override         { return true; }
	ControllerState raw_state() override { return {}; }

	bool has_position() override { return true; }
	glm::vec2 get_position() override;
	PositionVisibility GetPositionVisibility() override { return PositionVisibility::FULL; }
};

