#pragma once
#include <SDL2/SDL_joystick.h>
#include "input/motion/MotionHandler.h"
#include "input/api/ControllerProvider.h"

#ifndef HAS_SDL
#define HAS_SDL 1
#endif

static bool operator==(const SDL_JoystickGUID& g1, const SDL_JoystickGUID& g2)
{
	return memcmp(&g1, &g2, sizeof(SDL_JoystickGUID)) == 0;
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
	
	int get_index(size_t guid_index, const SDL_JoystickGUID& guid) const;

	MotionSample motion_sample(int diid);

private:
	void event_thread();
	
	std::atomic_bool m_running = false;
	std::thread m_thread;

	std::array<WiiUMotionHandler, 8> m_motion_handler{};
	std::array<MotionSample, 8> m_motion_data{};
	std::array<std::mutex, 8> m_motion_data_mtx{};

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

	std::array<MotionInfoTracking, 8> m_motion_tracking{};

};
