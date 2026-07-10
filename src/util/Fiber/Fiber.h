#pragma once

#if BOOST_PLAT_ANDROID
#include "FiberFContext.h"
#elif BOOST_OS_WINDOWS
#include "FiberWin.h"
#else
#include "FiberUContext.h"
#endif
