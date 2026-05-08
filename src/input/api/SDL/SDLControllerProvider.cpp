#include "input/api/SDL/SDLControllerProvider.h"

#include "input/api/SDL/SDLController.h"
#include "util/helpers/TempState.h"

#include <SDL3/SDL.h>
#include <boost/functional/hash.hpp>

struct SDL_JoystickGUIDHash
{
	std::size_t operator()(const SDL_GUID& guid) const
	{
		return boost::hash_value(guid.data);
	}
};

SDLControllerProvider::SDLControllerProvider()
{
#if !BOOST_OS_MACOS
	std::scoped_lock _l(s_mutex);
	if (s_initCount.fetch_add(1) == 0)
	{
		s_running = true;
		s_thread = std::thread(&SDLControllerProvider::event_thread, this);
	}
#endif
}

SDLControllerProvider::~SDLControllerProvider()
{
#if !BOOST_OS_MACOS
	bool shutdownSDL = false;
	{
		std::scoped_lock _l(s_mutex);
		if (s_initCount.fetch_sub(1) == 1)
		{
			cemu_assert_debug(s_running);
			s_running = false;
			shutdownSDL = true;
		}
	}

	if (shutdownSDL)
	{
		// wake the thread with a quit event if it's currently waiting for events
		SDL_Event evt;
		SDL_zero(evt);
		evt.type = SDL_EVENT_QUIT;
		SDL_PushEvent(&evt);
		if (s_thread.joinable())
		{
			s_thread.join();
		}
	}
#endif
}

std::vector<std::shared_ptr<ControllerBase>> SDLControllerProvider::get_controllers()
{
	std::vector<std::shared_ptr<ControllerBase>> result;

	std::unordered_map<SDL_GUID, size_t, SDL_JoystickGUIDHash> guid_counter;

	TempState lock(SDL_LockJoysticks, SDL_UnlockJoysticks);
	int gamepad_count = 0;
	SDL_JoystickID *gamepad_ids = SDL_GetGamepads(&gamepad_count);
	if (gamepad_ids)
	{
		for (size_t i = 0; i < gamepad_count; ++i)
		{
			const auto guid = SDL_GetGamepadGUIDForID(gamepad_ids[i]);
			const auto it = guid_counter.try_emplace(guid, 0);
			if (const char* name = SDL_GetGamepadNameForID(gamepad_ids[i]))
				result.emplace_back(std::make_shared<SDLController>(guid, it.first->second, name));
			else
				result.emplace_back(std::make_shared<SDLController>(guid, it.first->second));
			++it.first->second;
		}
		SDL_free(gamepad_ids);
	}
	return result;
}

int SDLControllerProvider::get_index(size_t guid_index, const SDL_GUID& guid) const
{
	size_t index = 0;
	int gamepad_count = 0;
	TempState lock(SDL_LockJoysticks, SDL_UnlockJoysticks);
	SDL_JoystickID *gamepad_ids = SDL_GetGamepads(&gamepad_count);
	if (gamepad_ids)
	{
		for (size_t i = 0; i < gamepad_count; ++i)
		{
			if (guid == SDL_GetGamepadGUIDForID(gamepad_ids[i]))
			{
				if (index == guid_index)
				{
					SDL_free(gamepad_ids);
					return i;
				}
				++index;
			}
		}
		SDL_free(gamepad_ids);
	}
	return -1;
}

MotionSample SDLControllerProvider::motion_sample(SDL_JoystickID diid)
{
	std::shared_lock lock(s_mutex);
	auto it = s_motion_states.find(diid);
	return (it != s_motion_states.end()) ? it->second.data : MotionSample{};
}

void SDLControllerProvider::InitSDL()
{
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_ENHANCED_REPORTS, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_GAMECUBE, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_SWITCH, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_SWITCH2, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_JOY_CONS, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_STADIA, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_STEAM, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_LUNA, "1");

	if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD | SDL_INIT_HAPTIC))
	{
		throw std::runtime_error(fmt::format("couldn't initialize SDL: {}", SDL_GetError()));
	}

	SDL_SetGamepadEventsEnabled(true);
	if (!SDL_GamepadEventsEnabled())
	{
		cemuLog_log(LogType::Force, "Couldn't enable SDL gamecontroller event polling: {}", SDL_GetError());
	}
}

void SDLControllerProvider::ShutdownSDL()
{
	SDL_QuitSubSystem(SDL_INIT_GAMEPAD | SDL_INIT_HAPTIC);
}

#if BOOST_OS_MACOS
void SDLControllerProvider::PumpSDLEvents()
{
	SDL_Event event;
	while (SDL_PollEvent(&event))
		HandleSDLEvent(event);
}
#endif

void SDLControllerProvider::HandleSDLEvent(SDL_Event& event)
{
	switch (event.type)
	{
		case SDL_EVENT_QUIT:
		{
			std::scoped_lock _l(s_mutex);
			s_running = false;
			break;
		}
		case SDL_EVENT_GAMEPAD_AXIS_MOTION: /**< Game controller axis motion */
		{
			break;
		}
		case SDL_EVENT_GAMEPAD_BUTTON_DOWN: /**< Game controller button pressed */
		{
			break;
		}
		case SDL_EVENT_GAMEPAD_BUTTON_UP: /**< Game controller button released */
		{
			break;
		}
		case SDL_EVENT_GAMEPAD_ADDED: /**< A new Game controller has been inserted into the system */
		{
			std::scoped_lock _l(s_mutex);
			InputManager::instance().on_device_changed();
			break;
		}
		case SDL_EVENT_GAMEPAD_REMOVED: /**< An opened Game controller has been removed */
		{
			std::scoped_lock _l(s_mutex);
			InputManager::instance().on_device_changed();
			s_motion_states.erase(event.gdevice.which);
			break;
		}
		case SDL_EVENT_GAMEPAD_REMAPPED: 			/**< The controller mapping was updated */
		{
			break;
		}
		case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:		/**< Game controller touchpad was touched */
		{
			break;
		}
		case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:		/**< Game controller touchpad finger was moved */
		{
			break;
		}
		case SDL_EVENT_GAMEPAD_TOUCHPAD_UP:			/**< Game controller touchpad finger was lifted */
		{
			break;
		}
		case SDL_EVENT_GAMEPAD_SENSOR_UPDATE:		/**< Game controller sensor was updated */
		{
			SDL_JoystickID id = event.gsensor.which;
			uint64_t ts = event.gsensor.timestamp;
			std::scoped_lock _l(s_mutex);
			auto& state = s_motion_states[id];
			auto& tracking = state.tracking;

			if (event.gsensor.sensor == SDL_SENSOR_ACCEL)
			{
				const auto dif = ts - tracking.lastTimestampAccel;
				if (dif <= 0)
				{
					break;
				}

				if (dif >= 10000000000)
				{
					tracking.hasAcc = false;
					tracking.hasGyro = false;
					tracking.lastTimestampAccel = ts;
					break;
				}

				tracking.lastTimestampAccel = ts;
				tracking.acc[0] = -event.gsensor.data[0] / 9.81f;
				tracking.acc[1] = -event.gsensor.data[1] / 9.81f;
				tracking.acc[2] = -event.gsensor.data[2] / 9.81f;
				tracking.hasAcc = true;
			}
			if (event.gsensor.sensor == SDL_SENSOR_GYRO)
			{
				const auto dif = ts - tracking.lastTimestampGyro;
				if (dif <= 0)
				{
					break;
				}

				if (dif >= 10000000000)
				{
					tracking.hasAcc = false;
					tracking.hasGyro = false;
					tracking.lastTimestampGyro = ts;
					break;
				}

				tracking.lastTimestampGyro = ts;
				tracking.gyro[0] = event.gsensor.data[0];
				tracking.gyro[1] = -event.gsensor.data[1];
				tracking.gyro[2] = -event.gsensor.data[2];
				tracking.hasGyro = true;
			}
			if (tracking.hasAcc && tracking.hasGyro)
			{
				auto ts = std::max(tracking.lastTimestampGyro, tracking.lastTimestampAccel);

				if (ts > tracking.lastTimestampIntegrate)
				{
					const auto tsDif = ts - tracking.lastTimestampIntegrate;
					tracking.lastTimestampIntegrate = ts;
					float tsDifD = (float)tsDif / 1000000000.0f;

					if (tsDifD >= 1.0f)
					{
						tsDifD = 1.0f;
					}

					state.handler.processMotionSample(tsDifD, tracking.gyro.x, tracking.gyro.y, tracking.gyro.z, tracking.acc.x, -tracking.acc.y, -tracking.acc.z);
					state.data = state.handler.getMotionSample();
				}

				tracking.hasAcc = false;
				tracking.hasGyro = false;
			}
			break;
		}
	}
}

void SDLControllerProvider::event_thread()
{
#if BOOST_OS_MACOS
	cemu_assert(false);
#endif
	SetThreadName("SDL_events");
	InitSDL();
	while (s_running.load(std::memory_order_relaxed))
	{
		SDL_Event event{};
		SDL_WaitEvent(&event);
		HandleSDLEvent(event);
	}
	ShutdownSDL();
}
