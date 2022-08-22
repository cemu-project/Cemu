#pragma once

#include "input/api/Controller.h"
#include "input/api/SDL/SDLControllerProvider.h"

#include <SDL2/SDL_gamecontroller.h>

class SDLController : public Controller<SDLControllerProvider>
{
public:
	SDLController(const SDL_JoystickGUID& guid, size_t guid_index);
	SDLController(const SDL_JoystickGUID& guid, size_t guid_index, std::string_view display_name);
	
	~SDLController() override;
	
	std::string_view api_name() const override
	{
		static_assert(to_string(InputAPI::SDLController) == "SDLController");
		return to_string(InputAPI::SDLController);
	}
	InputAPI::Type api() const override { return InputAPI::SDLController; }

	bool is_connected() override;
	bool connect() override;
	
	bool has_motion() override { return m_has_gyro && m_has_accel; }
	bool has_rumble() override { return m_has_rumble; }
	
	void start_rumble() override;
	void stop_rumble() override;

	MotionSample get_motion_sample() override;

	std::string get_button_name(uint64 button) const override;
	const SDL_JoystickGUID& get_guid() const { return m_guid; }

	constexpr static SDL_JoystickGUID kLeftJoyCon{ 0x03, 0x00, 0x00, 0x00, 0x7e, 0x05, 0x00, 0x00, 0x06, 0x20, 0x00, 0x00, 0x00, 0x00,0x68 ,0x00 };
	constexpr static SDL_JoystickGUID kRightJoyCon{ 0x03, 0x00, 0x00, 0x00, 0x7e, 0x05, 0x00, 0x00, 0x07, 0x20, 0x00, 0x00, 0x00, 0x00, 0x68, 0x00 };
	constexpr static SDL_JoystickGUID kSwitchProController{ 0x03, 0x00, 0x00, 0x00, 0x7e, 0x05, 0x00, 0x00, 0x09, 0x20, 0x00, 0x00, 0x00, 0x00, 0x68, 0x00 };

protected:
	ControllerState raw_state() override;

private:
	inline static SDL_JoystickGUID kEmptyGUID{};

	size_t m_guid_index;
	SDL_JoystickGUID m_guid;
	std::recursive_mutex m_controller_mutex;
	SDL_GameController* m_controller = nullptr;
	SDL_JoystickID m_diid = -1;

	bool m_has_gyro = false;
	bool m_has_accel = false;
	bool m_has_rumble = false;
	
	std::array<bool, SDL_CONTROLLER_BUTTON_MAX> m_buttons{};
	std::array<bool, SDL_CONTROLLER_AXIS_MAX> m_axis{};
};

