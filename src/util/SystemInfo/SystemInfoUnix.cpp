#include "util/SystemInfo/SystemInfo.h"

#include <sys/times.h>

void QueryProcTime(uint64 &out_now, uint64 &out_user, uint64 &out_kernel)
{
	struct tms time_info;
	clock_t clock_now = times(&time_info);
	clock_t clock_user = time_info.tms_utime;
	clock_t clock_kernel = time_info.tms_stime;
	out_now = static_cast<uint64>(clock_now);
	out_user = static_cast<uint64>(clock_user);
	out_kernel = static_cast<uint64>(clock_kernel);
}

