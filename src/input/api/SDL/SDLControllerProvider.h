#pragma once
#include <SDL3/SDL_joystick.h>
#include "input/motion/MotionHandler.h"
#include "input/api/ControllerProvider.h"

#ifndef HAS_SDL
#define HAS_SDL 1
#endif

static bool operator==(const SDL_GUID& g1, const SDL_GUID& g2)
{
	return memcmp(&g1, &g2, sizeof(SDL_GUID)) == 0;
}

class SDLControllerProvider : public ControllerProviderBase
{
	friend class SDLController;
public:
	SDLControllerProvider();
	~SDLControllerProvider();

	inline static InputAPI::Type kAPIType = InputAPI::SDLController;
	InputAPI::Type api() const override { return kAPIType; }

	std::vector<std::shared_ptr<ControllerBase>> get_controllers() override;
	
	int get_index(size_t guid_index, const SDL_GUID& guid) const;

	MotionSample motion_sample(SDL_JoystickID diid);

private:
	void event_thread();
	
	std::atomic_bool m_running = false;
	std::thread m_thread;

	struct MotionInfoTracking
	{
		uint64 lastTimestampGyro{};
		uint64 lastTimestampAccel{};
		uint64 lastTimestampIntegrate{};
		bool hasGyro{};
		bool hasAcc{};
		glm::vec3 gyro{};
		glm::vec3 acc{};
	};

	struct MotionState
	{
		WiiUMotionHandler handler;
		MotionSample data;
		mutable std::mutex mtx;
		MotionInfoTracking tracking;

		MotionState() = default;
	};

	std::unordered_map<SDL_JoystickID, MotionState> m_motion_states{};
};
