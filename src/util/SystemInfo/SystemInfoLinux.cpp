#if BOOST_OS_LINUX

#include "util/SystemInfo/SystemInfo.h"

#include <unistd.h>
#include <sys/times.h>

uint64 QueryRamUsage()
{
	long page_size = sysconf(_SC_PAGESIZE);

	std::ifstream file("/proc/self/statm");
	if (file)
	{
		file.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
		uint64 no_pages;
		file >> no_pages;

		return no_pages * page_size;
	}
	else
	{
		return 0;
	}
}

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

void QueryCoreTimes(uint32 count, ProcessorTime out[])
{
	std::ifstream file("/proc/stat");
	if (file)
	{
		file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

		for (auto i = 0; i < count; ++i)
		{
			uint64 user, nice, kernel, idle;
			file.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
			file >> user >> nice >> kernel >> idle;
			file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

			out[i].idle = idle;
			out[i].kernel = kernel;
			out[i].user = user + nice;
		}
	}
	else
	{
		for (auto i = 0; i < count; ++i) out[i] = { };
	}
}

#endif