#include "input/api/SDL/SDLControllerProvider.h"

#include "input/api/SDL/SDLController.h"
#include "util/helpers/TempState.h"

#include <SDL2/SDL.h>
#include <boost/functional/hash.hpp>

struct SDL_JoystickGUIDHash
{
	std::size_t operator()(const SDL_JoystickGUID& guid) const
	{
		return boost::hash_value(guid.data);
	}
};

SDLControllerProvider::SDLControllerProvider()
{
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5, "1");

	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");

	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_GAMECUBE, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_SWITCH, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_JOY_CONS, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_STADIA, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_STEAM, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_LUNA, "1");

	if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC | SDL_INIT_EVENTS) < 0) 
		throw std::runtime_error(fmt::format("couldn't initialize SDL: {}", SDL_GetError()));
		

	if (SDL_GameControllerEventState(SDL_ENABLE) < 0) {
		cemuLog_log(LogType::Force, "Couldn't enable SDL gamecontroller event polling: {}", SDL_GetError());
	}

	m_running = true;
	m_thread = std::thread(&SDLControllerProvider::event_thread, this);
}

SDLControllerProvider::~SDLControllerProvider()
{
	if (m_running)
	{
		m_running = false;
		// wake the thread with a quit event if it's currently waiting for events
		SDL_Event evt;
		evt.type = SDL_QUIT;
		SDL_PushEvent(&evt);
		// wait until thread exited
		m_thread.join();
	}

	SDL_QuitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC | SDL_INIT_EVENTS);
}

std::vector<std::shared_ptr<ControllerBase>> SDLControllerProvider::get_controllers()
{
	std::vector<std::shared_ptr<ControllerBase>> result;

	std::unordered_map<SDL_JoystickGUID, size_t, SDL_JoystickGUIDHash> guid_counter;

	TempState lock(SDL_LockJoysticks, SDL_UnlockJoysticks);
	for (int i = 0; i < SDL_NumJoysticks(); ++i)
	{
		if (SDL_JoystickGetDeviceType(i) == SDL_JOYSTICK_TYPE_GAMECONTROLLER)
		{
			const auto guid = SDL_JoystickGetDeviceGUID(i);

			const auto it = guid_counter.try_emplace(guid, 0);

			if (auto* controller = SDL_GameControllerOpen(i))
			{
				const char* name = SDL_GameControllerName(controller);

				result.emplace_back(std::make_shared<SDLController>(guid, it.first->second, name));
				SDL_GameControllerClose(controller);
			}
			else
				result.emplace_back(std::make_shared<SDLController>(guid, it.first->second));

			++it.first->second;
		}
	}

	return result;
}

int SDLControllerProvider::get_index(size_t guid_index, const SDL_JoystickGUID& guid) const
{
	size_t index = 0;

	TempState lock(SDL_LockJoysticks, SDL_UnlockJoysticks);
	for (int i = 0; i < SDL_NumJoysticks(); ++i)
	{
		if (SDL_JoystickGetDeviceType(i) == SDL_JOYSTICK_TYPE_GAMECONTROLLER)
		{
			if(guid == SDL_JoystickGetDeviceGUID(i))
			{
				if (index == guid_index)
				{
					return i;
				}

				++index;
			}
			
		}
	}

	return -1;
}

MotionSample SDLControllerProvider::motion_sample(int diid)
{
	std::scoped_lock lock(m_motion_data_mtx[diid]);
	return m_motion_data[diid];
}

void SDLControllerProvider::event_thread()
{
	SetThreadName("SDL_events");
	while (m_running.load(std::memory_order_relaxed))
	{
		SDL_Event event{};
		SDL_WaitEvent(&event);

		switch (event.type)
		{
		case SDL_QUIT:
			m_running = false;
			return;
			
		case SDL_CONTROLLERAXISMOTION: /**< Game controller axis motion */
		{
			break;
		}
		case SDL_CONTROLLERBUTTONDOWN: /**< Game controller button pressed */
		{
			break;
		}
		case SDL_CONTROLLERBUTTONUP: /**< Game controller button released */
		{
			break;
		}
		case SDL_CONTROLLERDEVICEADDED: /**< A new Game controller has been inserted into the system */
		{
			InputManager::instance().on_device_changed();
			break;
		}
		case SDL_CONTROLLERDEVICEREMOVED: /**< An opened Game controller has been removed */
		{
			InputManager::instance().on_device_changed();
			break;
		}
		case SDL_CONTROLLERDEVICEREMAPPED: /**< The controller mapping was updated */
		{
			break;
		}
		case SDL_CONTROLLERTOUCHPADDOWN:        /**< Game controller touchpad was touched */
		{
			break;
		}
		case SDL_CONTROLLERTOUCHPADMOTION:      /**< Game controller touchpad finger was moved */
		{
			break;
		}
		case SDL_CONTROLLERTOUCHPADUP:          /**< Game controller touchpad finger was lifted */
		{
			break;
		}
		case SDL_CONTROLLERSENSORUPDATE:        /**< Game controller sensor was updated */
		{
			const auto index = event.csensor.which;
			const auto ts = event.csensor.timestamp;
			auto& motionTracking = m_motion_tracking[index];

			if (event.csensor.sensor == SDL_SENSOR_ACCEL)
			{
				const auto dif = ts - motionTracking.lastTimestampAccel;
				if (dif <= 0)
					break;

				if (dif >= 10000)
				{
					motionTracking.hasAcc = false;
					motionTracking.hasGyro = false;
					motionTracking.lastTimestampAccel = ts;
					break;
				}

				motionTracking.lastTimestampAccel = ts;
				motionTracking.acc[0] = -event.csensor.data[0] / 9.81f;
				motionTracking.acc[1] = -event.csensor.data[1] / 9.81f;
				motionTracking.acc[2] = -event.csensor.data[2] / 9.81f;
				motionTracking.hasAcc = true;
			}
			if (event.csensor.sensor == SDL_SENSOR_GYRO)
			{
				const auto dif = ts - motionTracking.lastTimestampGyro;
				if (dif <= 0)
					break;

				if (dif >= 10000)
				{
					motionTracking.hasAcc = false;
					motionTracking.hasGyro = false;
					motionTracking.lastTimestampGyro = ts;
					break;
				}
				motionTracking.lastTimestampGyro = ts;
				motionTracking.gyro[0] = event.csensor.data[0];
				motionTracking.gyro[1] = -event.csensor.data[1];
				motionTracking.gyro[2] = -event.csensor.data[2];
				motionTracking.hasGyro = true;
			}
			if (motionTracking.hasAcc && motionTracking.hasGyro)
			{
				auto ts = std::max(motionTracking.lastTimestampGyro, motionTracking.lastTimestampAccel);
				if (ts > motionTracking.lastTimestampIntegrate)
				{
					const auto tsDif = ts - motionTracking.lastTimestampIntegrate;
					motionTracking.lastTimestampIntegrate = ts;
					float tsDifD = (float)tsDif / 1000.0f;
					if (tsDifD >= 1.0f)
						tsDifD = 1.0f;
					m_motion_handler[index].processMotionSample(tsDifD, motionTracking.gyro.x, motionTracking.gyro.y, motionTracking.gyro.z, motionTracking.acc.x, -motionTracking.acc.y, -motionTracking.acc.z);

					std::scoped_lock lock(m_motion_data_mtx[index]);
					m_motion_data[index] = m_motion_handler[index].getMotionSample();
				}
				motionTracking.hasAcc = false;
				motionTracking.hasGyro = false;
			}
			break;
		}
		}
	}
}
