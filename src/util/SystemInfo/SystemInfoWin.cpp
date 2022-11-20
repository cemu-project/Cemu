#include "util/SystemInfo/SystemInfo.h"

#include <Psapi.h>
#include <winternl.h>
#pragma comment(lib, "ntdll.lib")

uint64 QueryRamUsage()
{
	PROCESS_MEMORY_COUNTERS pmc{};
	pmc.cb = sizeof(pmc);
	if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
	{
		return pmc.WorkingSetSize;
	}
	else
	{
		return 0;
	}
}

void QueryProcTime(uint64 &out_now, uint64 &out_user, uint64 &out_kernel)
{
	FILETIME ftime, fkernel, fuser;
	LARGE_INTEGER now, kernel, user;
	GetSystemTimeAsFileTime(&ftime);
	now.LowPart = ftime.dwLowDateTime;
	now.HighPart = ftime.dwHighDateTime;

	if (GetProcessTimes(GetCurrentProcess(), &ftime, &ftime, &fkernel, &fuser))
	{
		kernel.LowPart = fkernel.dwLowDateTime;
		kernel.HighPart = fkernel.dwHighDateTime;

		user.LowPart = fuser.dwLowDateTime;
		user.HighPart = fuser.dwHighDateTime;

		out_now = now.QuadPart;
		out_user = user.QuadPart;
		out_kernel = kernel.QuadPart;
	}
	else
	{
		out_now = 0;
		out_user = 0;
		out_kernel = 0;
	}
}

void QueryCoreTimes(uint32 count, std::vector<ProcessorTime>& out)
{
	std::vector<SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION> sppi(count);
	if (NT_SUCCESS(NtQuerySystemInformation(SystemProcessorPerformanceInformation, sppi.data(), sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) * count, nullptr)))
	{
		for (auto i = 0; i < out.size(); ++i)
		{
			out[i].idle = sppi[i].IdleTime.QuadPart;
			out[i].kernel = sppi[i].KernelTime.QuadPart;
			out[i].kernel -= out[i].idle;
			out[i].user = sppi[i].UserTime.QuadPart;
		}
	}
	else
	{
		for (auto i = 0; i < count; ++i)
		{
			out[i] = { };
		}
	}
}
