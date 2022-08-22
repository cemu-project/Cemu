#pragma once

#include "boost/predef/os.h"
#include <cstdint>

#if BOOST_OS_WINDOWS
#include "Common/windows/platform.h"
#elif BOOST_OS_LINUX
#include "byteswap.h"
//#include <boost/core/enable_if.hpp>
// #include <boost/type_traits.hpp>
#include "Common/linux/platform.h"

#elif BOOST_OS_MACOS

#endif