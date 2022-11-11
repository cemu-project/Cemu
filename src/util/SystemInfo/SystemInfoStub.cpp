#include "util/SystemInfo/SystemInfo.h"

uint64 QueryRamUsage()
{
	return 0;
}

#ifndef BOOST_OS_UNIX
void QueryProcTime(uint64 &out_now, uint64 &out_user, uint64 &out_kernel)
{
	out_now = 0;
	out_user = 0;
	out_kernel = 0;
}
#endif

void QueryCoreTimes(uint32 count, ProcessorTime out[])
{
	for (auto i = 0; i < count; ++i)
	{
		out[i] = { };
	}
}
