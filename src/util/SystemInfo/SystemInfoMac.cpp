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

// apple official documentation is non-existsent.
// based on https://github.com/giampaolo/psutil/blob/master/psutil/_psutil_osx.c#L623
void QueryCoreTimes(uint32 count, std::vector<ProcessorTime>& out)
{
	// initialize default
	for (auto i = 0; i < out.size(); ++i)
	{
		out[i] = {};
	}

	natural_t cpu_count;
	processor_info_array_t info_array;
	mach_msg_type_number_t info_count;
	kern_return_t error;

	mach_port_t host_port = mach_host_self();
	error = host_processor_info(host_port, PROCESSOR_CPU_LOAD_INFO, &cpu_count, &info_array, &info_count);
	mach_port_deallocate(mach_task_self(), host_port);

	if (error != KERN_SUCCESS)
		return;

	processor_cpu_load_info_data_t* cpuLoad = (processor_cpu_load_info_data_t*) info_array;

	for (auto i = 0; i < cpu_count; ++i)
	{
		uint64 system = cpuLoad[i].cpu_ticks[CPU_STATE_SYSTEM];
		uint64 user = cpuLoad[i].cpu_ticks[CPU_STATE_USER] + cpuLoad[i].cpu_ticks[CPU_STATE_NICE];
		uint64 idle = cpuLoad[i].cpu_ticks[CPU_STATE_IDLE];

		out[i].idle = idle;
		out[i].kernel = system;
		out[i].user = user;
	}

	int ret = vm_deallocate(mach_task_self(), (vm_address_t) info_array,
						info_count * sizeof(int));
	if (ret != KERN_SUCCESS)
		cemuLog_log(LogType::Force, "vm_deallocate() failed");
}
