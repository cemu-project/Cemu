#include <stdint.h>
#include <time.h>

uint32_t GetTickCount()
{
        struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
        return (1000 * ts.tv_sec + ts.tv_nsec / 1000000);
}

#include <cpuid.h>

void (__cpuid)(int __cpuVal[4], unsigned int __leaf)
{
	__cpuid(__cpuVal[0], __cpuVal[1], __cpuVal[2], __cpuVal[3], __leaf);
}
#undef __cpuid

void __cpuidex1(int __cpuid_info[4], int __leaf, int __subleaf)
{
  __cpuid_count (__leaf, __subleaf, __cpuid_info[0], __cpuid_info[1],
		 __cpuid_info[2], __cpuid_info[3]);
}
