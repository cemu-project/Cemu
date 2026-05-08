#include "input/api/SDL/SDLController.h"

#include "input/api/SDL/SDLControllerProvider.h"

SDLController::SDLController(const SDL_GUID& guid, size_t guid_index)
	: base_type(fmt::format("{}_", guid_index), fmt::format("Controller {}", guid_index + 1)), m_guid_index(guid_index),
	  m_guid(guid)
{
	char tmp[64];
	SDL_GUIDToString(m_guid, tmp, std::size(tmp));
	m_uuid += tmp;
}

SDLController::SDLController(const SDL_GUID& guid, size_t guid_index, std::string_view display_name)
	: base_type(fmt::format("{}_", guid_index), display_name), m_guid_index(guid_index), m_guid(guid)
{
	char tmp[64];
	SDL_GUIDToString(m_guid, tmp, std::size(tmp));
	m_uuid += tmp;
}

SDLController::~SDLController()
{
	if (m_controller)
	{
		SDL_CloseGamepad(m_controller);
		m_controller = nullptr;
	}
}

bool SDLController::is_connected()
{
	std::scoped_lock lock(m_controller_mutex);
	if (!m_controller)
	{
		return false;
	}

	if (!SDL_GamepadConnected(m_controller))
	{
		SDL_CloseGamepad(m_controller);
		m_controller = nullptr;
		return false;
	}

	return true;
}

bool SDLController::connect()
{
	if (is_connected())
		return true;

	m_has_rumble = false;
	const auto index = m_provider->get_index(m_guid_index, m_guid);
	std::scoped_lock lock(m_controller_mutex);

	int gamepad_count = 0;

	SDL_JoystickID *gamepad_ids = SDL_GetGamepads(&gamepad_count);

	if (!gamepad_ids || index < 0 || index >= gamepad_count)
		return false;

	m_diid = gamepad_ids[index];
	SDL_free(gamepad_ids);

	m_controller = SDL_OpenGamepad(m_diid);

	if (!m_controller)
		return false;

	if (const char* name = SDL_GetGamepadName(m_controller))
		m_display_name = name;

	for (size_t i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; ++i)
		m_buttons[i] = SDL_GamepadHasButton(m_controller, (SDL_GamepadButton)i);
	for (size_t i = 0; i < SDL_GAMEPAD_AXIS_COUNT; ++i)
		m_axis[i] = SDL_GamepadHasAxis(m_controller, (SDL_GamepadAxis)i);
	if (SDL_GamepadHasSensor(m_controller, SDL_SENSOR_ACCEL))
		m_has_accel = SDL_SetGamepadSensorEnabled(m_controller, SDL_SENSOR_ACCEL, true);
	if (SDL_GamepadHasSensor(m_controller, SDL_SENSOR_GYRO))
		m_has_gyro = SDL_SetGamepadSensorEnabled(m_controller, SDL_SENSOR_GYRO, true);
	m_has_rumble = SDL_RumbleGamepad(m_controller, 0, 0, 0);
	return true;
}

void SDLController::start_rumble()
{
	std::scoped_lock lock(m_controller_mutex);
	if (is_connected() && !m_has_rumble)
		return;
	if (m_settings.rumble <= 0)
		return;
	SDL_RumbleGamepad(m_controller, (Uint16)(m_settings.rumble * 0xFFFF), (Uint16)(m_settings.rumble * 0xFFFF), 5 * 1000);
}

void SDLController::stop_rumble()
{
	std::scoped_lock lock(m_controller_mutex);
	if (is_connected() && !m_has_rumble)
		return;
	SDL_RumbleGamepad(m_controller, 0, 0, 0);
}

MotionSample SDLController::get_motion_sample()
{
	if (is_connected() && has_motion())
		return m_provider->motion_sample(m_diid);
	return {};
}

std::string SDLController::get_button_name(uint64 button) const
{
	if (const char* name = SDL_GetGamepadStringForButton((SDL_GamepadButton)button))
		return name;
	return base_type::get_button_name(button);
}

ControllerState SDLController::raw_state()
{
	ControllerState result{};
	std::scoped_lock lock(m_controller_mutex);
	if (!is_connected())
		return result;
	for (size_t i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; ++i)
	{
		if (m_buttons[i] && SDL_GetGamepadButton(m_controller, (SDL_GamepadButton)i))
			result.buttons.SetButtonState(i, true);
	}

	if (m_axis[SDL_GAMEPAD_AXIS_LEFTX])
		result.axis.x = (float)SDL_GetGamepadAxis(m_controller, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
	if (m_axis[SDL_GAMEPAD_AXIS_LEFTY])
		result.axis.y = (float)SDL_GetGamepadAxis(m_controller, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
	if (m_axis[SDL_GAMEPAD_AXIS_RIGHTX])
		result.rotation.x = (float)SDL_GetGamepadAxis(m_controller, SDL_GAMEPAD_AXIS_RIGHTX) / 32767.0f;
	if (m_axis[SDL_GAMEPAD_AXIS_RIGHTY])
		result.rotation.y = (float)SDL_GetGamepadAxis(m_controller, SDL_GAMEPAD_AXIS_RIGHTY) / 32767.0f;
	if (m_axis[SDL_GAMEPAD_AXIS_LEFT_TRIGGER])
		result.trigger.x = (float)SDL_GetGamepadAxis(m_controller, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) / 32767.0f;
	if (m_axis[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER])
		result.trigger.y = (float)SDL_GetGamepadAxis(m_controller, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 32767.0f;

	return result;
}
