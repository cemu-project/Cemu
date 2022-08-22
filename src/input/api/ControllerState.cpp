#include "input/api/ControllerState.h"

bool ControllerState::operator==(const ControllerState& other) const
{
	return buttons == other.buttons;
		/*&& (std::signbit(axis.x) == std::signbit(other.axis.x) && std::abs(axis.x - other.axis.x) <= kAxisThreshold)
		&& (std::signbit(axis.y) == std::signbit(other.axis.y) && std::abs(axis.y - other.axis.y) <= kAxisThreshold)
		&& (std::signbit(rotation.x) == std::signbit(other.rotation.x) && std::abs(rotation.x - other.rotation.x) <= kAxisThreshold)
		&& (std::signbit(rotation.y) == std::signbit(other.rotation.y) && std::abs(rotation.y - other.rotation.y) <= kAxisThreshold)
		&& (std::signbit(trigger.x) == std::signbit(other.trigger.x) && std::abs(trigger.x - other.trigger.x) <= kAxisThreshold)
		&& (std::signbit(trigger.y) == std::signbit(other.trigger.y) && std::abs(trigger.y - other.trigger.y) <= kAxisThreshold);*/
		
}
