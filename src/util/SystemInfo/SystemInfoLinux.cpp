#include "util/SystemInfo/SystemInfo.h"

uint64 QueryRamUsage()
{
	std::ifstream file("/proc/self/smaps_rollup");
	if (file)
	{
		file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
		file.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
		uint64 kilobytes;
		file >> kilobytes;

		return kilobytes * 1024;
	}
	return 0;
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
