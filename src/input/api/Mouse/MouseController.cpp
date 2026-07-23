#include "MouseController.h"
#include "input/api/Controller.h"

std::string_view MouseController::api_name() const
{
	static_assert(to_string(InputAPI::Mouse) == "Mouse");
	return to_string(InputAPI::Mouse);
}

MouseController& MouseController::SetPositionEnabled(bool value)
{
	m_isPositionEnabled = value;
	return *this;
}

glm::vec2 MouseController::get_position()
{
	return InputManager::instance().GetMousePositionInRenderBox();
}

PositionVisibility MouseController::GetPositionVisibility()
{
	return IsPositionEnabled()
		? PositionVisibility::FULL
		: PositionVisibility::NONE;
}
