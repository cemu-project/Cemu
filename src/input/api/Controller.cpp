#include "input/api/Controller.h"

#include "gui/guiWrapper.h"

ControllerBase::ControllerBase(std::string_view uuid, std::string_view display_name)
	: m_uuid{uuid}, m_display_name{display_name}
{
}

const ControllerState& ControllerBase::update_state()
{
	if (!m_is_calibrated)
		calibrate();

	ControllerState result = raw_state();

	// ignore default buttons
	result.buttons.UnsetButtons(m_default_state.buttons);
	// apply deadzone and range and ignore default axis values
	apply_axis_setting(result.axis, m_default_state.axis, m_settings.axis);
	apply_axis_setting(result.rotation, m_default_state.rotation, m_settings.rotation);
	apply_axis_setting(result.trigger, m_default_state.trigger, m_settings.trigger);

#define APPLY_AXIS_BUTTON(_axis_, _flag_) \
	if (result._axis_.x < -ControllerState::kAxisThreshold) \
		result.buttons.SetButtonState((_flag_) + (kAxisXN - kAxisXP), true); \
	else if (result._axis_.x > ControllerState::kAxisThreshold) \
		result.buttons.SetButtonState((_flag_), true); \
	if (result._axis_.y < -ControllerState::kAxisThreshold) \
		result.buttons.SetButtonState((_flag_) + 1 + (kAxisXN - kAxisXP), true); \
	else if (result._axis_.y > ControllerState::kAxisThreshold) \
		result.buttons.SetButtonState((_flag_) + 1, true);

	if (result.axis.x < -ControllerState::kAxisThreshold) 
		result.buttons.SetButtonState((kAxisXP) + (kAxisXN - kAxisXP), true);
	else if (result.axis.x > ControllerState::kAxisThreshold) 
		result.buttons.SetButtonState((kAxisXP), true);
	if (result.axis.y < -ControllerState::kAxisThreshold) 
		result.buttons.SetButtonState((kAxisXP) + 1 + (kAxisXN - kAxisXP), true);
	else if (result.axis.y > ControllerState::kAxisThreshold) 
		result.buttons.SetButtonState((kAxisXP) + 1, true);
	APPLY_AXIS_BUTTON(rotation, kRotationXP);
	APPLY_AXIS_BUTTON(trigger, kTriggerXP);

	/*
// positive values
	kAxisXP,
	kAxisYP,

	kRotationXP,
	kRotationYP,

	kTriggerXP,
	kTriggerYP,

	// negative values
	kAxisXN,
	kAxisYN,

	kRotationXN,
	kRotationYN,

	kTriggerXN,
	kTriggerYN,
	 */


#undef APPLY_AXIS_BUTTON

	m_last_state = std::move(result);
	return m_last_state;
}

void ControllerBase::apply_axis_setting(glm::vec2& axis, const glm::vec2& default_value,
                                        const AxisSetting& setting) const
{
	constexpr float kMaxValue = 1.0f + ControllerState::kMinAxisValue;
	if (setting.deadzone < 1.0f)
	{
		if (axis.x < default_value.x)
			axis.x = (axis.x - default_value.x) / (kMaxValue + default_value.x);
		else
			axis.x = (axis.x - default_value.x) / (kMaxValue - default_value.x);

		if (axis.y < default_value.y)
			axis.y = (axis.y - default_value.y) / (kMaxValue + default_value.y);
		else
			axis.y = (axis.y - default_value.y) / (kMaxValue - default_value.y);

		auto len = length(axis);
		if (len >= setting.deadzone)
		{
			axis *= setting.range;
			len = length(axis);

			// Scaled Radial Dead Zone: stickInput = stickInput.normalized * ((stickInput.magnitude - deadzone) / (1 - deadzone));
			if (len > 0)
			{
				axis = normalize(axis);
				axis *= ((len - setting.deadzone) / (kMaxValue - setting.deadzone));

				if (length(axis) > 1.0f)
					axis = normalize(axis);
			}

			if (axis.x != 0 || axis.y != 0)
			{
				if (std::abs(axis.x) < ControllerState::kMinAxisValue)
					axis.x = ControllerState::kMinAxisValue;

				if (std::abs(axis.y) < ControllerState::kMinAxisValue)
					axis.y = ControllerState::kMinAxisValue;
			}

			return;
		}
	}

	axis = {0, 0};
}

bool ControllerBase::operator==(const ControllerBase& c) const
{
	return api() == c.api() && uuid() == c.uuid();
}

float ControllerBase::get_axis_value(uint64 button) const
{
	if (m_last_state.buttons.GetButtonState(button))
	{
		if (button <= kButtonNoneAxisMAX || !has_axis())
			return 1.0f;

		switch (button)
		{
		case kAxisXP:
		case kAxisXN:
			return std::abs(m_last_state.axis.x);
		case kAxisYP:
		case kAxisYN:
			return std::abs(m_last_state.axis.y);

		case kRotationXP:
		case kRotationXN:
			return std::abs(m_last_state.rotation.x);
		case kRotationYP:
		case kRotationYN:
			return std::abs(m_last_state.rotation.y);

		case kTriggerXP:
		case kTriggerXN:
			return std::abs(m_last_state.trigger.x);
		case kTriggerYP:
		case kTriggerYN:
			return std::abs(m_last_state.trigger.y);
		}
	}

	return 0;
}

const ControllerState& ControllerBase::calibrate()
{
	m_default_state = raw_state();
	m_is_calibrated = is_connected();
	return m_default_state;
}


std::string ControllerBase::get_button_name(uint64 button) const
{
	switch (button)
	{
	case kButtonZL: return "ZL";
	case kButtonZR: return "ZR";

	case kButtonUp: return "DPAD-Up";
	case kButtonDown: return "DPAD-Down";
	case kButtonLeft: return "DPAD-Left";
	case kButtonRight: return "DPAD-Right";

	case kAxisXP: return "X-Axis+";
	case kAxisYP: return "Y-Axis+";

	case kAxisXN: return "X-Axis-";
	case kAxisYN: return "Y-Axis-";

	case kRotationXP: return "X-Rotation+";
	case kRotationYP: return "Y-Rotation+";

	case kRotationXN: return "X-Rotation-";
	case kRotationYN: return "Y-Rotation-";

	case kTriggerXP: return "X-Trigger+";
	case kTriggerYP: return "Y-Trigger+";

	case kTriggerXN: return "X-Trigger-";
	case kTriggerYN: return "y-Trigger-";
	}


	return fmt::format("Button {}", (uint64)button);
}

ControllerBase::Settings ControllerBase::get_settings() const
{
	std::scoped_lock lock(m_settings_mutex);
	return m_settings;
}

void ControllerBase::set_settings(const Settings& settings)
{
	std::scoped_lock lock(m_settings_mutex);
	m_settings = settings;
}

void ControllerBase::set_axis_settings(const AxisSetting& settings)
{
	std::scoped_lock lock(m_settings_mutex);
	m_settings.axis = settings;
}

void ControllerBase::set_rotation_settings(const AxisSetting& settings)
{
	std::scoped_lock lock(m_settings_mutex);
	m_settings.rotation = settings;
}

void ControllerBase::set_trigger_settings(const AxisSetting& settings)
{
	std::scoped_lock lock(m_settings_mutex);
	m_settings.trigger = settings;
}

void ControllerBase::set_rumble(float rumble)
{
	std::scoped_lock lock(m_settings_mutex);
	m_settings.rumble = rumble;
}

void ControllerBase::set_use_motion(bool state)
{
	std::scoped_lock lock(m_settings_mutex);
	m_settings.motion = state;
}
