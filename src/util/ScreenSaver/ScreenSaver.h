#if BOOST_OS_MACOS
#include <IOKit/pwr_mgt/IOPMLib.h>
#endif

#if BOOST_OS_LINUX
#include <spawn.h>
#ifdef USE_ELOGIND
#include <elogind/sd-bus.h>
#include <elogind/sd-daemon.h>
#else
#include <systemd/sd-bus.h>
#include <systemd/sd-daemon.h>
#endif
#endif

class ScreenSaver
{
public:
  static void SetInhibit(bool inhibit)
  {
    // Adapted from Dolphin's implementation: https://github.com/dolphin-emu/dolphin/blob/e3e6c3dfa41d377520f74ec2488fc1f7b6c05be3/Source/Core/UICommon/UICommon.cpp#L454
#if BOOST_OS_WINDOWS
    SetThreadExecutionState(ES_CONTINUOUS |
                            (inhibit ? (ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED) : 0));
#endif

#if BOOST_OS_MACOS
    static IOPMAssertionID s_power_assertion = kIOPMNullAssertionID;
    if (inhibit)
    {
      CFStringRef reason_for_activity = CFSTR("Emulation Running");
      if (IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleDisplaySleep,
                                      kIOPMAssertionLevelOn, reason_for_activity,
                                      &s_power_assertion) != kIOReturnSuccess)
      {
        s_power_assertion = kIOPMNullAssertionID;
      }
    }
    else
    {
      if (s_power_assertion != kIOPMNullAssertionID)
      {
        IOPMAssertionRelease(s_power_assertion);
        s_power_assertion = kIOPMNullAssertionID;
      }
    }
#endif

#if BOOST_OS_LINUX
    char id[11];
    char *argv[4] = {(char *)"xdg-screensaver", (char *)(inhibit ? "suspend" : "resume"), id, nullptr};
    posix_spawnp(nullptr, "xdg-screensaver", nullptr, nullptr, argv, environ);
#endif
  };
};