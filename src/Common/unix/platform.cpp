#include <stdint.h>
#include <time.h>

uint32_t GetTickCount()
{
        struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
        return (1000 * ts.tv_sec + ts.tv_nsec / 1000000);
}