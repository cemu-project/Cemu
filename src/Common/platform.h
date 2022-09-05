#pragma once

#include <boost/predef/os.h>
#include <cstdint>

#if BOOST_OS_WINDOWS
#include "Common/windows/platform.h"
#elif BOOST_OS_LINUX
#include "Common/unix/platform.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>
#include <byteswap.h>
#elif BOOST_OS_MACOS
#include "Common/unix/platform.h"
#include <libkern/OSByteOrder.h>
#endif
