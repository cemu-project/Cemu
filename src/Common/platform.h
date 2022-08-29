#pragma once

#include <boost/predef/os.h>
#include <cstdint>

#if BOOST_OS_WINDOWS
#include "Common/windows/platform.h"
#elif BOOST_OS_LINUX
#include <byteswap.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#include <X11/Xutil.h>
#include "Common/unix/platform.h"
#elif BOOST_OS_MACOS
#include <libkern/OSByteOrder.h>
#include "Common/unix/platform.h"
#endif
