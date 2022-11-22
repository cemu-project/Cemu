#include "util/SystemInfo/SystemInfo.h"

#include <unistd.h>

uint64 QueryRamUsage()
{
	static long page_size = sysconf(_SC_PAGESIZE);
	if (page_size == -1)
	{
		return 0;
	}

	std::ifstream file("/proc/self/statm");
	if (file)
	{
		file.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
		uint64 pages;
		file >> pages;

		return pages * page_size;
	}
	return 0;
}

void QueryCoreTimes(uint32 count, std::vector<ProcessorTime>& out)
{
	std::ifstream file("/proc/stat");
	if (file)
	{
		file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

		for (auto i = 0; i < out.size(); ++i)
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
