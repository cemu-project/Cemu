#include "input/api/XInput/XInputController.h"

XInputController::XInputController(uint32 index)
	: base_type(fmt::format("{}", index), fmt::format("Controller {}", index + 1))
{
	if (index >= XUSER_MAX_COUNT)
		throw std::runtime_error(fmt::format("invalid xinput index {} (must be smaller than {})", index,
		                                     XUSER_MAX_COUNT));

	m_index = index;

	m_settings.axis.deadzone = m_settings.rotation.deadzone = m_settings.trigger.deadzone = 0.15f;
}

bool XInputController::connect()
{
	if (m_connected)
		return true;

	m_has_battery = false;

	XINPUT_CAPABILITIES caps{};
	m_connected = m_provider->m_XInputGetCapabilities(m_index, XINPUT_FLAG_GAMEPAD, &caps) !=
		ERROR_DEVICE_NOT_CONNECTED;
	if (!m_connected) return false;

	m_has_rumble = (caps.Vibration.wLeftMotorSpeed > 0) || (caps.Vibration.wRightMotorSpeed > 0);

	if (m_provider->m_XInputGetBatteryInformation)
	{
		XINPUT_BATTERY_INFORMATION battery{};
		if (m_provider->m_XInputGetBatteryInformation(m_index, BATTERY_DEVTYPE_GAMEPAD, &battery) == ERROR_SUCCESS)
		{
			m_has_battery = (battery.BatteryType == BATTERY_TYPE_ALKALINE || battery.BatteryType ==
				BATTERY_TYPE_NIMH);
		}
	}
	
	return m_connected;
}

bool XInputController::is_connected()
{
	return m_connected;
}


void XInputController::start_rumble()
{
	if (!has_rumble() || m_settings.rumble <= 0)
		return;

	XINPUT_VIBRATION vibration;
	vibration.wLeftMotorSpeed = static_cast<WORD>(m_settings.rumble * std::numeric_limits<uint16>::max());
	vibration.wRightMotorSpeed = static_cast<WORD>(m_settings.rumble * std::numeric_limits<uint16>::max());
	m_provider->m_XInputSetState(m_index, &vibration);
}

void XInputController::stop_rumble()
{
	if (!has_rumble())
		return;

	XINPUT_VIBRATION vibration{};
	m_provider->m_XInputSetState(m_index, &vibration);
}

bool XInputController::has_low_battery()
{
	if (!has_battery())
		return false;

	XINPUT_BATTERY_INFORMATION battery{};
	if (m_provider->m_XInputGetBatteryInformation(m_index, BATTERY_DEVTYPE_GAMEPAD, &battery) == ERROR_SUCCESS)
	{
		return (battery.BatteryType == BATTERY_TYPE_ALKALINE || battery.BatteryType == BATTERY_TYPE_NIMH) && battery
			.BatteryLevel <= BATTERY_LEVEL_LOW;
	}

	return false;
}

std::string XInputController::get_button_name(uint64 button) const
{
	switch (1ULL << button)
	{
	case XINPUT_GAMEPAD_A: return "A";
	case XINPUT_GAMEPAD_B: return "B";
	case XINPUT_GAMEPAD_X: return "X";
	case XINPUT_GAMEPAD_Y: return "Y";

	case XINPUT_GAMEPAD_LEFT_SHOULDER: return "L";
	case XINPUT_GAMEPAD_RIGHT_SHOULDER: return "R";

	case XINPUT_GAMEPAD_START: return "Start";
	case XINPUT_GAMEPAD_BACK: return "Select";

	case XINPUT_GAMEPAD_LEFT_THUMB: return "L-Stick";
	case XINPUT_GAMEPAD_RIGHT_THUMB: return "R-Stick";
	case XINPUT_GAMEPAD_DPAD_UP: return "DPAD-Up";
	case XINPUT_GAMEPAD_DPAD_DOWN: return "DPAD-Down";
	case XINPUT_GAMEPAD_DPAD_LEFT: return "DPAD-Left";
	case XINPUT_GAMEPAD_DPAD_RIGHT: return "DPAD-Right";
	}

	return Controller::get_button_name(button);
}

ControllerState XInputController::raw_state()
{
	ControllerState result{};
	if (!m_connected)
		return result;

	XINPUT_STATE state;
	if (m_provider->m_XInputGetState(m_index, &state) != ERROR_SUCCESS)
	{
		m_connected = false;
		return result;
	}

	// Buttons
	for(int i=0;i<std::numeric_limits<WORD>::digits;i++)
		result.buttons.SetButtonState(i, (state.Gamepad.wButtons & (1 << i)) != 0);

	if (state.Gamepad.sThumbLX > 0)
		result.axis.x = (float)state.Gamepad.sThumbLX / std::numeric_limits<sint16>::max();
	else if (state.Gamepad.sThumbLX < 0)
		result.axis.x = (float)-state.Gamepad.sThumbLX / std::numeric_limits<sint16>::min();

	if (state.Gamepad.sThumbLY > 0)
		result.axis.y = (float)state.Gamepad.sThumbLY / std::numeric_limits<sint16>::max();
	else if (state.Gamepad.sThumbLY < 0)
		result.axis.y = (float)-state.Gamepad.sThumbLY / std::numeric_limits<sint16>::min();

	// Right Stick
	if (state.Gamepad.sThumbRX > 0)
		result.rotation.x = (float)state.Gamepad.sThumbRX / std::numeric_limits<sint16>::max();
	else if (state.Gamepad.sThumbRX < 0)
		result.rotation.x = (float)-state.Gamepad.sThumbRX / std::numeric_limits<sint16>::min();

	if (state.Gamepad.sThumbRY > 0)
		result.rotation.y = (float)state.Gamepad.sThumbRY / std::numeric_limits<sint16>::max();
	else if (state.Gamepad.sThumbRY < 0)
		result.rotation.y = (float)-state.Gamepad.sThumbRY / std::numeric_limits<sint16>::min();

	// Trigger
	result.trigger.x = (float)state.Gamepad.bLeftTrigger / std::numeric_limits<uint8>::max();
	result.trigger.y = (float)state.Gamepad.bRightTrigger / std::numeric_limits<uint8>::max();
	
	return result;
}
