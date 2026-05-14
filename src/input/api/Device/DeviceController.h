#pragma once

#include "input/api/InputAPI.h"
#include "input/api/Controller.h"
#include "DeviceControllerProvider.h"

class DeviceController : public Controller<DeviceControllerProvider>
{
  public:
	DeviceController();

	class ControllerCallbacks
	{
	  public:
		virtual void start_rumble(sint64 duration, float rumble) = 0;
		virtual void stop_rumble() = 0;
	};

	std::string_view api_name() const override
	{
		static_assert(to_string(InputAPI::Device) == "Device");
		return to_string(InputAPI::Device);
	}

	InputAPI::Type api() const override
	{
		return InputAPI::Device;
	}

	static void set_callbacks(std::shared_ptr<ControllerCallbacks> callbacks);

	bool is_connected() override;

	bool has_rumble() override;

	bool has_motion() override;

	MotionSample get_motion_sample() override;

	ControllerState raw_state() override;

	void start_rumble() override;

	void stop_rumble() override;

	static void process_motion(std::chrono::steady_clock::time_point timestamp, float gx, float gy, float gz, float accx, float accy, float accz);

	static void set_motion_enabled(bool enabled);

  private:
	inline static bool s_has_motion{};
	inline static std::mutex s_mutex{};
	inline static WiiUMotionHandler s_motion_handler{};
	inline static std::chrono::steady_clock::time_point s_last_motion_timestamp{};
	inline static std::shared_ptr<ControllerCallbacks> s_callbacks{};
	inline static ControllerState s_raw_state{};
};
