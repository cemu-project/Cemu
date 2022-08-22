#include "input/api/GameCube/GameCubeController.h"

#ifdef HAS_GAMECUBE

GameCubeController::GameCubeController(uint32 adapter, uint32 index)
	: base_type(fmt::format("{}_{}", adapter, index), fmt::format("Controller {}", index + 1)), m_adapter(adapter),
	  m_index(index)
{
	// update names if multiple adapters are connected
	if (adapter > 0)
		m_display_name = fmt::format("Controller {} ({})", index + 1, adapter);

	m_settings.axis.range = 1.20f;
	m_settings.rotation.range = 1.25f;
	m_settings.trigger.range = 1.07f;
}

bool GameCubeController::is_connected()
{
	return m_provider->is_connected(m_adapter);
}

bool GameCubeController::has_rumble()
{
	return m_provider->has_rumble_connected(m_adapter);
}

void GameCubeController::start_rumble()
{
	if (m_settings.rumble <= 0)
		return;

	m_provider->set_rumble_state(m_adapter, m_index, true);
}

void GameCubeController::stop_rumble()
{
	m_provider->set_rumble_state(m_adapter, m_index, false);
}

std::string GameCubeController::get_button_name(uint64 button) const
{
	switch (button)
	{
	case kButton0: return "A";
	case kButton1: return "B";
	case kButton2: return "X";
	case kButton3: return "Y";

	case kButton4: return "Left";
	case kButton5: return "Right";
	case kButton6: return "Down";
	case kButton7: return "Up";

	case kButton8: return "Start";
	case kButton9: return "Z";

	case kButton10: return "Trigger R";
	case kButton11: return "Trigger L";
	}

	return base_type::get_button_name(button);
}

ControllerState GameCubeController::raw_state()
{
	ControllerState result{};
	if (!is_connected())
		return result;

	const auto state = m_provider->get_state(m_adapter, m_index);
	if (state.valid)
	{
		for (auto i = 0; i <= kButton11; ++i)
		{
			if (HAS_BIT(state.button, i))
			{
				result.buttons.set(i);
			}
		}

		// printf("(%d, %d) - (%d, %d) - (%d, %d)\n", state.lstick_x, state.lstick_y, state.rstick_x, state.rstick_y, state.lstick, state.rstick);
		result.axis.x = (float)state.lstick_x / std::numeric_limits<uint8>::max();
		result.axis.x = (result.axis.x * 2.0f) - 1.0f;

		result.axis.y = (float)state.lstick_y / std::numeric_limits<uint8>::max();
		result.axis.y = (result.axis.y * 2.0f) - 1.0f;

		result.rotation.x = (float)state.rstick_x / std::numeric_limits<uint8>::max();
		result.rotation.x = (result.rotation.x * 2.0f) - 1.0f;

		result.rotation.y = (float)state.rstick_y / std::numeric_limits<uint8>::max();
		result.rotation.y = (result.rotation.y * 2.0f) - 1.0f;


		result.trigger.x = (float)state.lstick / std::numeric_limits<uint8>::max();
		result.trigger.y = (float)state.rstick / std::numeric_limits<uint8>::max();
	}
	
	return result;
}

#endif