#pragma once

#include <glm/vec2.hpp>

struct ControllerState
{
	// when does a axis counts as pressed
	constexpr static float kAxisThreshold = 0.1f;

	// on the real console the stick x or y values never really reach 0.0 if one of the axis is moved
	// some games rely on this due to incorrectly checking if the stick is tilted via if (vstick.x != 0 && vstick.y != 0)
	// here we simulate a slight bias if the axis is almost perfectly centered
	constexpr static float kMinAxisValue = 0.0000001f;

	// [-1; 1]
	glm::vec2 axis{ };
	glm::vec2 rotation{ };
	glm::vec2 trigger{ };

	std::unordered_map<uint32, bool> buttons{};

	uint64 last_state = 0;

	bool operator==(const ControllerState& other) const;
	bool operator!=(const ControllerState& other) const
	{
		return !(*this == other);
	}
};
