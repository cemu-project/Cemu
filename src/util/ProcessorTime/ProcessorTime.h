#pragma once

struct ProcessorTime
{
	uint64_t idle{}, kernel{}, user{};
	
	uint64_t work();
	uint64_t total();

	static double Compare(ProcessorTime &last, ProcessorTime &now);
};

uint32_t GetProcessorCount();
void QueryProcTime(uint64_t &out_now, uint64_t &out_user, uint64_t &out_kernel);
void QueryProcTime(ProcessorTime &out);
void QueryCoreTimes(uint32_t count, ProcessorTime out[]);

uint64_t QueryRamUsage();