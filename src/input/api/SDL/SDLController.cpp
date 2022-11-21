#include "input/api/SDL/SDLController.h"

#include "input/api/SDL/SDLControllerProvider.h"

SDLController::SDLController(const SDL_JoystickGUID& guid, size_t guid_index)
	: base_type(fmt::format("{}_", guid_index), fmt::format("Controller {}", guid_index + 1)), m_guid_index(guid_index),
	  m_guid(guid)
{
	char tmp[64];
	SDL_JoystickGetGUIDString(m_guid, tmp, std::size(tmp));
	m_uuid += tmp;
}

SDLController::SDLController(const SDL_JoystickGUID& guid, size_t guid_index, std::string_view display_name)
	: base_type(fmt::format("{}_", guid_index), display_name), m_guid_index(guid_index), m_guid(guid)
{
	char tmp[64];
	SDL_JoystickGetGUIDString(m_guid, tmp, std::size(tmp));
	m_uuid += tmp;
}

SDLController::~SDLController()
{
	if (m_controller)
		SDL_GameControllerClose(m_controller);
}

bool SDLController::is_connected()
{
	std::scoped_lock lock(m_controller_mutex);
	if (!m_controller)
	{
		return false;
	}

	if (!SDL_GameControllerGetAttached(m_controller))
	{
		m_controller = nullptr;
		return false;
	}

	return true;
}

bool SDLController::connect()
{
	if (is_connected())
	{
		return true;
	}

	m_has_rumble = false;

	const auto index = m_provider->get_index(m_guid_index, m_guid);

	std::scoped_lock lock(m_controller_mutex);
	m_diid = SDL_JoystickGetDeviceInstanceID(index);
	if (m_diid == -1)
		return false;

	m_controller = SDL_GameControllerOpen(index);
	if (!m_controller)
		return false;

	if (const char* name = SDL_GameControllerName(m_controller))
		m_display_name = name;

	for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i)
	{
		m_buttons[i] = SDL_GameControllerHasButton(m_controller, (SDL_GameControllerButton)i);
	}

	for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; ++i)
	{
		m_axis[i] = SDL_GameControllerHasAxis(m_controller, (SDL_GameControllerAxis)i);
	}

	if (SDL_GameControllerHasSensor(m_controller, SDL_SENSOR_ACCEL))
	{
		m_has_accel = true;
		SDL_GameControllerSetSensorEnabled(m_controller, SDL_SENSOR_ACCEL, SDL_TRUE);
	}

	if (SDL_GameControllerHasSensor(m_controller, SDL_SENSOR_GYRO))
	{
		m_has_gyro = true;
		SDL_GameControllerSetSensorEnabled(m_controller, SDL_SENSOR_GYRO, SDL_TRUE);
	}

	m_has_rumble = SDL_GameControllerRumble(m_controller, 0, 0, 0) == 0;
	return true;
}

void SDLController::start_rumble()
{
	std::scoped_lock lock(m_controller_mutex);
	if (is_connected() && !m_has_rumble)
		return;

	if (m_settings.rumble <= 0)
		return;

	SDL_GameControllerRumble(m_controller, (Uint16)(m_settings.rumble * 0xFFFF), (Uint16)(m_settings.rumble * 0xFFFF),
	                         5 * 1000);
}

void SDLController::stop_rumble()
{
	std::scoped_lock lock(m_controller_mutex);
	if (is_connected() && !m_has_rumble)
		return;

	SDL_GameControllerRumble(m_controller, 0, 0, 0);
}

MotionSample SDLController::get_motion_sample()
{
	if (is_connected() && has_motion())
	{
		return m_provider->motion_sample(m_diid);
	}

	return {};
}

std::string SDLController::get_button_name(uint64 button) const
{
	if (const char* name = SDL_GameControllerGetStringForButton((SDL_GameControllerButton)button))
		return name;

	return base_type::get_button_name(button);
}

ControllerState SDLController::raw_state()
{
	ControllerState result{};

	std::scoped_lock lock(m_controller_mutex);
	if (!is_connected())
		return result;

	for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i)
	{
		if (m_buttons[i] && SDL_GameControllerGetButton(m_controller, (SDL_GameControllerButton)i))
			result.buttons.SetButtonState(i, true);
	}

	if (m_axis[SDL_CONTROLLER_AXIS_LEFTX])
		result.axis.x = (float)SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_LEFTX) / 32767.0f;

	if (m_axis[SDL_CONTROLLER_AXIS_LEFTY])
		result.axis.y = (float)SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_LEFTY) / 32767.0f;

	if (m_axis[SDL_CONTROLLER_AXIS_RIGHTX])
		result.rotation.x = (float)SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_RIGHTX) / 32767.0f;

	if (m_axis[SDL_CONTROLLER_AXIS_RIGHTY])
		result.rotation.y = (float)SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_RIGHTY) / 32767.0f;

	if (m_axis[SDL_CONTROLLER_AXIS_TRIGGERLEFT])
		result.trigger.x = (float)SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) / 32767.0f;

	if (m_axis[SDL_CONTROLLER_AXIS_TRIGGERRIGHT])
		result.trigger.y = (float)SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) / 32767.0f;

	return result;
}
