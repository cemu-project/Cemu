#include "DeviceController.h"

DeviceController::DeviceController() : base_type("device", "device")
{
}

void DeviceController::set_callbacks(std::shared_ptr<ControllerCallbacks> callbacks)
{
	std::scoped_lock lock(s_mutex);
	s_callbacks = std::move(callbacks);
}

bool DeviceController::is_connected()
{
	return true;
}

bool DeviceController::has_rumble()
{
	return true;
}

bool DeviceController::has_motion()
{
	return true;
}

MotionSample DeviceController::get_motion_sample()
{
	std::scoped_lock lock{s_mutex};
	return s_motion_handler.getMotionSample();
}

ControllerState DeviceController::raw_state()
{
	std::scoped_lock lock{s_mutex};
	return s_raw_state;
}

void DeviceController::start_rumble()
{
	std::scoped_lock lock{s_mutex};

	if (m_settings.rumble <= 0 || m_settings.rumble > 1 || !s_callbacks)
	{
		return;
	}

	s_callbacks->start_rumble(5000, m_settings.rumble);
}

void DeviceController::stop_rumble()
{
	std::scoped_lock lock{s_mutex};

	if (s_callbacks)
	{
		s_callbacks->stop_rumble();
	}
}

void DeviceController::process_motion(std::chrono::steady_clock::time_point timestamp, float gx, float gy, float gz, float accx, float accy, float accz)
{
	using namespace std::chrono_literals;
	std::scoped_lock lock{s_mutex};

	auto dif = timestamp - s_last_motion_timestamp;
	s_last_motion_timestamp = timestamp;

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

	s_motion_handler.processMotionSample(
		deltaSeconds,
		gx, gy, gz,
		accx, accy, accz);
}

void DeviceController::set_motion_enabled(bool enabled)
{
	std::scoped_lock lock{s_mutex};
	s_has_motion = enabled;
}
