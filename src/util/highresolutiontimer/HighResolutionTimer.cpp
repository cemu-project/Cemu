#include "util/highresolutiontimer/HighResolutionTimer.h"
#include "Common/precompiled.h"

HighResolutionTimer HighResolutionTimer::now()
{
#if BOOST_OS_WINDOWS
	LARGE_INTEGER pc;
	QueryPerformanceCounter(&pc);
	return HighResolutionTimer(pc.QuadPart);
#elif BOOST_OS_LINUX
    timespec pc;
    clock_gettime(CLOCK_MONOTONIC_RAW, &pc);
    uint64 nsec = (uint64)pc.tv_sec * (uint64)1000000000 + (uint64)pc.tv_nsec;
    return HighResolutionTimer(nsec);
#elif BOOST_OS_MACOS
	return HighResolutionTimer(clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW));
#endif
}

HRTick HighResolutionTimer::getFrequency()
{
	return m_freq;
}

uint64 HighResolutionTimer::m_freq = []() -> uint64 {
#if BOOST_OS_WINDOWS
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	return (uint64)(freq.QuadPart);
#elif BOOST_OS_MACOS
	return 1000000000;
#else
    timespec pc;
    clock_getres(CLOCK_MONOTONIC_RAW, &pc);
    return (uint64)1000000000 / (uint64)pc.tv_nsec;
#endif
}();
