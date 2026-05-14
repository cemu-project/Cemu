#pragma once

#include "input/api/ControllerState.h"
#include "input/motion/MotionHandler.h"

#include <android/input.h>

class ControllerManager
{
  public:
	struct ControllerRuntimeState
	{
		bool isConnected{};

		std::string name{};

		ControllerState inputs{};

		bool hasRumble{};
		bool hasMotion{};

		std::chrono::steady_clock::time_point lastMotionTimestamp{};
		WiiUMotionHandler motionHandler{};

		std::shared_mutex mutex{};
	};

	struct ControllerInfo
	{
		sint32 id{};
		std::string descriptor{};
		std::string name{};
		bool hasRumble{};
		bool hasMotion{};
	};

	class ControllerCallbacks
	{
	  public:
		virtual void vibrate_controller(std::string_view deviceDescriptor, sint64 milliseconds, sint32 amplitude) = 0;
		virtual void cancel_controller_vibration(std::string_view deviceDescriptor) = 0;
	};

	static ControllerManager& instance();

	void vibrate_controller(std::string_view deviceDescriptor, sint64 milliseconds, sint32 amplitude);

	void cancel_controller_vibration(std::string_view deviceDescriptor);

	void set_controller_callbacks(std::shared_ptr<ControllerCallbacks> callbacks);

	void set_controllers(const std::vector<ControllerInfo>& controllers);

	std::vector<ControllerInfo> get_controllers();

	std::shared_ptr<ControllerRuntimeState> get_state(const std::string& deviceDescriptor);

	void process_key_event(const std::string& deviceDescriptor, int nativeKeyCode, bool isPressed);

	void process_axis_event(const std::string& deviceDescriptor, int nativeAxisCode, float value);

	void process_motion(
		const std::string& deviceDescriptor,
		std::chrono::steady_clock::time_point timestamp,
		float gyroX,
		float gyroY,
		float gyroZ,
		float accelX,
		float accelY,
		float accelZ);

  private:
	ControllerManager() = default;

	inline void update_state(const std::string& deviceDescriptor, std::invocable<ControllerRuntimeState&> auto f)
	{
		std::scoped_lock lock{m_mutex};

		auto it = m_states.find(deviceDescriptor);

		if (it == m_states.end())
		{
			return;
		}

		auto statePtr = it->second;
		auto& state = *statePtr;
		std::scoped_lock stateLock{state.mutex};

		f(state);
	}

	std::mutex m_mutex;
	std::shared_ptr<ControllerCallbacks> m_controllerCallbacks;
	std::vector<ControllerInfo> m_controllers;
	std::unordered_map<std::string, std::shared_ptr<ControllerRuntimeState>> m_states;
};
