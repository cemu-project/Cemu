#include <cstdint>
#include <ctime>

uint32_t GetTickCount()
{
#if BOOST_OS_LINUX
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
	return (1000 * ts.tv_sec + ts.tv_nsec / 1000000);
#elif BOOST_OS_MACOS
	return clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW) / 1000000;
#endif

}