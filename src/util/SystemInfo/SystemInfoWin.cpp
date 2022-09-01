#if BOOST_OS_WINDOWS

#include "util/SystemInfo/SystemInfo.h"

#include <Psapi.h>
#include <winternl.h>
#pragma comment(lib, "ntdll.lib")

uint32_t GetProcessorCount()
{
	SYSTEM_INFO sys_info;
	GetSystemInfo(&sys_info);
	return sys_info.dwNumberOfProcessors;
}

uint64_t QueryRamUsage()
{
	PROCESS_MEMORY_COUNTERS pmc{};
	pmc.cb = sizeof(pmc);
	GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
	return pmc.WorkingSetSize;
}

void QueryProcTime(uint64_t &out_now, uint64_t &out_user, uint64_t &out_kernel)
{
	FILETIME ftime, fkernel, fuser;
	LARGE_INTEGER now, kernel, user;
	GetSystemTimeAsFileTime(&ftime);
	now.LowPart = ftime.dwLowDateTime;
	now.HighPart = ftime.dwHighDateTime;

	GetProcessTimes(GetCurrentProcess(), &ftime, &ftime, &fkernel, &fuser);
	kernel.LowPart = fkernel.dwLowDateTime;
	kernel.HighPart = fkernel.dwHighDateTime;

	user.LowPart = fuser.dwLowDateTime;
	user.HighPart = fuser.dwHighDateTime;

	out_now = now.QuadPart;
	out_user = user.QuadPart;
	out_kernel = kernel.QuadPart;
}

void QueryCoreTimes(uint32_t count, ProcessorTime out[])
{
	std::vector<SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION> sppi(count);
	if (NT_SUCCESS(NtQuerySystemInformation(SystemProcessorPerformanceInformation, sppi.data(), sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) * count, nullptr)))
	{
		for (auto i = 0; i < count; ++i)
		{
			out[i].idle = sppi[i].IdleTime.QuadPart;			
			out[i].kernel = sppi[i].KernelTime.QuadPart;
			out[i].user = sppi[i].UserTime.QuadPart;
		}
	}
}

#endif