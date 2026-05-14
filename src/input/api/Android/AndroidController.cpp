#include "input/api/Android/AndroidController.h"
#include "ControllerManager.h"

#include <android/keycodes.h>

AndroidController::AndroidController(std::string_view device_descriptor, std::string_view device_name)
	: base_type(device_descriptor, device_name)
{
	m_state = ControllerManager::instance().get_state(m_uuid);
}

ControllerState AndroidController::raw_state()
{
	std::shared_lock lock{m_state->mutex};
	return m_state->inputs;
}

bool AndroidController::has_rumble()
{
	std::shared_lock lock{m_state->mutex};
	return m_state->hasRumble;
}

void AndroidController::start_rumble()
{
	if (is_connected() && !has_rumble())
	{
		return;
	}

	if (m_settings.rumble <= 0 || m_settings.rumble > 1)
	{
		return;
	}

	ControllerManager::instance().vibrate_controller(
		m_uuid,
		5000,
		static_cast<int>(255.0f * m_settings.rumble));
}

void AndroidController::stop_rumble()
{
	if (is_connected() && !has_rumble())
	{
		return;
	}

	if (m_settings.rumble <= 0)
	{
		return;
	}

	ControllerManager::instance().cancel_controller_vibration(m_uuid);
}

MotionSample AndroidController::get_motion_sample()
{
	std::shared_lock lock{m_state->mutex};
	return m_state->motionHandler.getMotionSample();
}

bool AndroidController::has_motion()
{
	std::shared_lock lock{m_state->mutex};
	return m_state->hasMotion;
}

bool AndroidController::is_connected()
{
	std::shared_lock lock{m_state->mutex};
	return m_state->isConnected;
}

std::string AndroidController::get_button_name(uint64 button) const
{
	switch (button)
	{
	case AKEYCODE_BUTTON_A: return "Button A";
	case AKEYCODE_BUTTON_B: return "Button B";
	case AKEYCODE_BUTTON_C: return "Button C";
	case AKEYCODE_BUTTON_X: return "Button X";
	case AKEYCODE_BUTTON_Y: return "Button Y";
	case AKEYCODE_BUTTON_Z: return "Button Z";
	case AKEYCODE_BUTTON_L1: return "Button L1";
	case AKEYCODE_BUTTON_R1: return "Button R1";
	case AKEYCODE_BUTTON_L2: return "Button L2";
	case AKEYCODE_BUTTON_R2: return "Button R2";
	case AKEYCODE_BUTTON_THUMBL: return "Button ThumbL";
	case AKEYCODE_BUTTON_THUMBR: return "Button ThumbR";
	case AKEYCODE_BUTTON_START: return "Button Start";
	case AKEYCODE_BUTTON_SELECT: return "Button Select";
	case AKEYCODE_BUTTON_MODE: return "Button Mode";
	case AKEYCODE_BUTTON_1: return "Button 1";
	case AKEYCODE_BUTTON_2: return "Button 2";
	case AKEYCODE_BUTTON_3: return "Button 3";
	case AKEYCODE_BUTTON_4: return "Button 4";
	case AKEYCODE_BUTTON_5: return "Button 5";
	case AKEYCODE_BUTTON_6: return "Button 6";
	case AKEYCODE_BUTTON_7: return "Button 7";
	case AKEYCODE_BUTTON_8: return "Button 8";
	case AKEYCODE_BUTTON_9: return "Button 9";
	case AKEYCODE_BUTTON_10: return "Button 10";
	case AKEYCODE_BUTTON_11: return "Button 11";
	case AKEYCODE_BUTTON_12: return "Button 12";
	case AKEYCODE_BUTTON_13: return "Button 13";
	case AKEYCODE_BUTTON_14: return "Button 14";
	case AKEYCODE_BUTTON_15: return "Button 15";
	case AKEYCODE_BUTTON_16: return "Button 16";
	default: return base_type::get_button_name(button);
	}
}
