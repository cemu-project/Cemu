#include "Cemu/Logging/CemuLogging.h"

#ifdef HAS_SDL
#include <SDL3/SDL.h>
#endif

class ScreenSaver
{
#ifdef HAS_SDL
  public:
	static void SetInhibit(bool inhibit)
	{
		bool* inhibitArg = new bool(inhibit);

		if (!SDL_RunOnMainThread(SetInhibitCallback, inhibitArg, false))
		{
			delete inhibitArg;
			cemuLog_log(LogType::Force, "Failed to schedule screen saver logic on main thread: {}", SDL_GetError());
		}
	}

  private:
	static void SDLCALL SetInhibitCallback(void* userdata)
	{
		if (!userdata)
		{
			return;
		}

		auto* inhibitArg = static_cast<bool*>(userdata);
		bool inhibit = *inhibitArg;
		delete inhibitArg;

		if (SDL_WasInit(SDL_INIT_VIDEO) == 0)
		{
			if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
			{
				cemuLog_log(LogType::Force, "Could not disable screen saver (SDL video subsystem initialization error: {})", SDL_GetError());
				return;
			}
		}

		if (inhibit)
		{
			if (!SDL_DisableScreenSaver())
			{
				cemuLog_log(LogType::Force, "Could not disable screen saver: {}", SDL_GetError());
			}
			else if (SDL_ScreenSaverEnabled())
			{
				cemuLog_log(LogType::Force, "Could not verify if screen saver was disabled (`SDL_IsScreenSaverEnabled()` returned true)");
			}
		}
		else
		{
			if (!SDL_EnableScreenSaver())
			{
				cemuLog_log(LogType::Force, "Could not enable screen saver: {}", SDL_GetError());
			}
			else if (!SDL_ScreenSaverEnabled())
			{
				cemuLog_log(LogType::Force, "Could not verify if screen saver was re-enabled (`SDL_IsScreenSaverEnabled()` returned false)");
			}
		}
	}
#else
  public:
	static void SetInhibit(bool /*inhibit*/)
	{
	}
#endif
};
