#if BOOST_OS_LINUX

#include "util/SystemInfo/SystemInfo.h"

#include <unistd.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/resource.h>
#include <string>

uint32_t GetProcessorCount()
{
	return std::thread::hardware_concurrency();
}

uint64_t QueryRamUsage()
{
	long page_size = sysconf(_SC_PAGESIZE);

	std::ifstream file("/proc/self/statm");
	file.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
	uint64_t no_pages;
	file >> no_pages;

	return no_pages * page_size;
}

void QueryProcTime(uint64_t &out_now, uint64_t &out_user, uint64_t &out_kernel)
{
	struct tms time_info;
	clock_t clock_now = times(&time_info);
	clock_t clock_user = time_info.tms_utime;
	clock_t clock_kernel = time_info.tms_utime;
	out_now = static_cast<uint64_t>(clock_now);
	out_user = static_cast<uint64_t>(clock_user);
	out_kernel = static_cast<uint64_t>(clock_kernel);
}

void QueryCoreTimes(uint32_t count, ProcessorTime out[])
{
	std::ifstream file("/proc/stat");
	file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

	for (auto i = 0; i < count; ++i)
	{
		uint64_t user, nice, kernel, idle;
		file.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
		file >> user >> nice >> kernel >> idle;
		file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

		out[i].idle = idle;
		out[i].kernel = kernel;
		out[i].user = user + nice;
	}
}

#endif