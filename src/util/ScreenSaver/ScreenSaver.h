#include "Cemu/Logging/CemuLogging.h"

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
    // Adapted from https://github.com/FeralInteractive/gamemode/blob/b11d2912e280acb87d9ad114d6c7cd8846c4ef02/daemon/gamemode-dbus.c#L711, available under the BSD 3-Clause license
    static unsigned int screensaver_inhibit_cookie = 0;

    const char *service = "org.freedesktop.ScreenSaver";
    const char *object_path = "/org/freedesktop/ScreenSaver";
    const char *interface = "org.freedesktop.ScreenSaver";
    const char *function = inhibit ? "Inhibit" : "UnInhibit";

    sd_bus_message *msg = NULL;
    sd_bus *bus_local = NULL;
    sd_bus_error err;
    memset(&err, 0, sizeof(sd_bus_error));

    int result = -1;

    // Open the user bus
    int ret = sd_bus_open_user(&bus_local);
    if (ret < 0)
    {
      cemuLog_force("Could not disable screen saver (user bus error)");
      return;
    }

    if (inhibit)
    {
      ret = sd_bus_call_method(bus_local,
                               service,
                               object_path,
                               interface,
                               function,
                               &err,
                               &msg,
                               "ss",
                               "Cemu",
                               "Setting to disable screen saver is enabled");
    }
    else
    {
      ret = sd_bus_call_method(bus_local,
                               service,
                               object_path,
                               interface,
                               function,
                               &err,
                               &msg,
                               "u",
                               screensaver_inhibit_cookie);
    }

    if (ret < 0)
    {
      cemuLog_force("Could not disable screen saver (org.freedesktop.ScreenSaver not available)");
    }
    else if (inhibit)
    {
      // Read the reply
      ret = sd_bus_message_read(msg, "u", &screensaver_inhibit_cookie);
      if (ret < 0)
      {
        cemuLog_force("Could not disable screen saver (org.freedesktop.ScreenSaver not available)");
      }
      else
      {
        result = 0;
      }
    }
    else
    {
      result = 0;
    }

    sd_bus_unref(bus_local);
#endif
  };
};