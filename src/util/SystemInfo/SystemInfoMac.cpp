#include <unistd.h>
#include <sys/resource.h>
#include <mach/mach.h>

// borrowed from https://en.wikichip.org/wiki/resident_set_size#OS_X
uint64 QueryRamUsage()
{
	mach_task_basic_info info;
	mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
	if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) == KERN_SUCCESS)
		return info.resident_size;
	return 0;
}
