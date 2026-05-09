#include "ControllerManager.h"

#include "api/Controller.h"

ControllerManager& ControllerManager::instance()
{
	static ControllerManager instance;
	return instance;
}

void ControllerManager::vibrate_controller(std::string_view deviceDescriptor, sint64 milliseconds, sint32 amplitude)
{
	std::scoped_lock lock{m_mutex};

	if (!m_controllerCallbacks)
	{
		return;
	}

	m_controllerCallbacks->vibrate_controller(deviceDescriptor, milliseconds, amplitude);
}

void ControllerManager::cancel_controller_vibration(std::string_view deviceDescriptor)
{
	std::scoped_lock lock{m_mutex};

	if (!m_controllerCallbacks)
	{
		return;
	}

	m_controllerCallbacks->cancel_controller_vibration(deviceDescriptor);
}

void ControllerManager::set_controller_callbacks(std::shared_ptr<ControllerCallbacks> callbacks)
{
	std::scoped_lock lock{m_mutex};
	m_controllerCallbacks = std::move(callbacks);
}

void ControllerManager::set_controllers(const std::vector<ControllerInfo>& controllers)
{
	std::scoped_lock lock{m_mutex};

	for (auto& [descriptor, controller] : m_states)
	{
		std::scoped_lock stateLock{controller->mutex};
		controller->isConnected = false;
	}

	for (const auto& ctrl : controllers)
	{
		auto it = m_states.find(ctrl.descriptor);

		if (it != m_states.end())
		{
			auto statePtr = it->second;
			auto& state = *statePtr;

			std::scoped_lock stateLock{state.mutex};

			state.name = ctrl.name;
			state.hasRumble = ctrl.hasRumble;
			state.hasMotion = ctrl.hasMotion;
			state.isConnected = true;
		}
	}

	m_controllers = controllers;
}

std::vector<ControllerManager::ControllerInfo> ControllerManager::get_controllers()
{
	std::scoped_lock lock{m_mutex};
	return m_controllers;
}

std::shared_ptr<ControllerManager::ControllerRuntimeState> ControllerManager::get_state(const std::string& deviceDescriptor)
{
	std::scoped_lock lock{m_mutex};

	auto it = m_states.find(deviceDescriptor);

	if (it != m_states.end())
	{
		return it->second;
	}

	auto state = std::make_shared<ControllerRuntimeState>();

	m_states[deviceDescriptor] = state;

	return state;
}

void ControllerManager::process_key_event(const std::string& deviceDescriptor, int nativeKeyCode, bool isPressed)
{
	update_state(deviceDescriptor, [&](auto& state) {
		state.inputs.buttons.SetButtonState(nativeKeyCode, isPressed);
	});
}

void ControllerManager::process_axis_event(const std::string& deviceDescriptor, int nativeAxisCode, float value)
{
	update_state(deviceDescriptor, [&](auto& state) {
		auto& controllerState = state.inputs;

		switch (nativeAxisCode)
		{
		case AMOTION_EVENT_AXIS_X:
			controllerState.axis.x = value;
			break;
		case AMOTION_EVENT_AXIS_Y:
			controllerState.axis.y = value;
			break;
		case AMOTION_EVENT_AXIS_RX:
		case AMOTION_EVENT_AXIS_Z:
			controllerState.rotation.x = value;
			break;
		case AMOTION_EVENT_AXIS_RY:
		case AMOTION_EVENT_AXIS_RZ:
			controllerState.rotation.y = value;
			break;
		case AMOTION_EVENT_AXIS_LTRIGGER:
			controllerState.trigger.x = value;
			break;
		case AMOTION_EVENT_AXIS_RTRIGGER:
			controllerState.trigger.y = value;
			break;
		case AMOTION_EVENT_AXIS_HAT_X:
			if (value == 0.0f)
			{
				controllerState.buttons.SetButtonState(kButtonRight, false);
				controllerState.buttons.SetButtonState(kButtonLeft, false);
				return;
			}
			else if (value > 0.0f)
				controllerState.buttons.SetButtonState(kButtonRight, true);
			else
				controllerState.buttons.SetButtonState(kButtonLeft, true);
			break;
		case AMOTION_EVENT_AXIS_HAT_Y:
			if (value == 0.0f)
			{
				controllerState.buttons.SetButtonState(kButtonUp, false);
				controllerState.buttons.SetButtonState(kButtonDown, false);
				return;
			}
			else if (value > 0.0f)
				controllerState.buttons.SetButtonState(kButtonDown, true);
			else
				controllerState.buttons.SetButtonState(kButtonUp, true);
			break;
		default:
			break;
		}
	});
}

void ControllerManager::process_motion(
	const std::string& deviceDescriptor,
	std::chrono::steady_clock::time_point timestamp,
	float gyroX, float gyroY, float gyroZ,
	float accelX, float accelY, float accelZ)
{
	update_state(deviceDescriptor, [&](auto& state) {
		using namespace std::chrono_literals;

		const auto dif = timestamp - state.lastMotionTimestamp;
		state.lastMotionTimestamp = timestamp;

		if (dif <= 0ms)
		{
			return;
		}

		if (dif >= 10s)
		{
			return;
		}

		constexpr auto maxDuration = 1s;
		auto clamped = std::min<std::common_type_t<decltype(dif), decltype(maxDuration)>>(dif, maxDuration);

		float deltaSeconds = std::chrono::duration_cast<std::chrono::duration<float>>(dif).count();

		state.motionHandler.processMotionSample(
			deltaSeconds,
			gyroX, gyroY, gyroZ,
			accelX, accelY, accelZ);
	});
}
