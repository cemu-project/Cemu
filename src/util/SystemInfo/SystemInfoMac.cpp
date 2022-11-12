#include "util/SystemInfo/SystemInfo.h"

#include <unistd.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <mach/mach.h>

#include <mach/processor_info.h>
#include <mach/mach_host.h>
#include <mach/kern_return.h>

// borrowed from https://en.wikichip.org/wiki/resident_set_size#OS_X
uint64 QueryRamUsage()
{
	mach_task_basic_info info;
	mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
	if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) == KERN_SUCCESS)
		return info.resident_size;
	return 0;
}

// based on https://stackoverflow.com/questions/20471920/how-to-get-total-cpu-idle-time-in-objective-c-c-on-os-x
void QueryCoreTimes(uint32 count, std::vector<ProcessorTime>& out)
{
	// initialize default
	for (auto i = 0; i < out.size(); ++i)
	{
		out[i] = { };
	}

	processor_cpu_load_info_t cpuLoad;
	mach_msg_type_number_t processorMsgCount;
    natural_t processorCount;

	kern_return_t err = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &processorCount, (processor_info_array_t *)&cpuLoad, &processorMsgCount);
	if(err != KERN_SUCCESS)
		return;

	for (auto i = 0; i < processorMsgCount; ++i)
	{
		uint64 system = cpuLoad[i].cpu_ticks[CPU_STATE_SYSTEM];
		uint64 user = cpuLoad[i].cpu_ticks[CPU_STATE_USER] + cpuLoad[i].cpu_ticks[CPU_STATE_NICE];
		uint64 idle = cpuLoad[i].cpu_ticks[CPU_STATE_IDLE];

		out[i].idle = idle;
		out[i].kernel = system;
		out[i].user = user;
	}
}
