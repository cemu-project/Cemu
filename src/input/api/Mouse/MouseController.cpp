#include "MouseController.h"
#include "input/api/Controller.h"

std::string_view MouseController::api_name() const
{
	static_assert(to_string(InputAPI::Mouse) == "Mouse");
	return to_string(InputAPI::Mouse);
}

glm::vec2 MouseController::get_position()
{
	return InputManager::instance().GetMousePositionInRenderBox();
}
