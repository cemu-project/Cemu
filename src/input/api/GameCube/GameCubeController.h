#pragma once

#include "input/api/Controller.h"
#include "input/api/GameCube/GameCubeControllerProvider.h"

#ifdef HAS_GAMECUBE

class GameCubeController : public Controller<GameCubeControllerProvider>
{
public:
	GameCubeController(uint32 adapter, uint32 index);
	
	std::string_view api_name() const override
	{
		static_assert(to_string(InputAPI::GameCube) == "GameCube");
		return to_string(InputAPI::GameCube);
	}
	InputAPI::Type api() const override { return InputAPI::GameCube; }

	bool is_connected() override;
	bool has_rumble() override;
	void start_rumble() override;
	void stop_rumble() override;

	std::string get_button_name(uint64 button) const override;

protected:
	ControllerState raw_state() override;

	uint32 m_adapter;
	uint32 m_index;
};

#endif