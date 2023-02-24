#include "Cemu/Logging/CemuLogging.h"
#include <SDL2/SDL.h>

class ScreenSaver
{
public:
  static void SetInhibit(bool inhibit)
  {
    // Initialize video subsystem if necessary
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0)
    {
      int initErr = SDL_InitSubSystem(SDL_INIT_VIDEO);
      if (initErr)
      {
        cemuLog_force("Could not disable screen saver (SDL video subsystem initialization error)");
      }
    }
    // Toggle SDL's screen saver inhibition
    if (inhibit)
    {
      SDL_DisableScreenSaver();
      if (SDL_IsScreenSaverEnabled() == SDL_TRUE)
      {
        cemuLog_force("Could not verify if screen saver was disabled (`SDL_IsScreenSaverEnabled()` returned SDL_TRUE)");
      }
    }
    else
    {
      SDL_EnableScreenSaver();
      if (SDL_IsScreenSaverEnabled() == SDL_FALSE)
      {
        cemuLog_force("Could not verify if screen saver was re-enabled (`SDL_IsScreenSaverEnabled()` returned SDL_FALSE)");
      }
    }
  };
};
