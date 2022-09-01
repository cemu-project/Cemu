#include "util/ProcessorTime/ProcessorTime.h"

uint64_t ProcessorTime::work()
{
	return user + kernel;
}

uint64_t ProcessorTime::total()
{
	return idle + user + kernel;
}

double ProcessorTime::Compare(ProcessorTime &last, ProcessorTime &now)
{
	auto dwork = now.work() - last.work();
	auto dtotal = now.total() - last.total();

	return (double)dwork / dtotal;
}

void QueryProcTime(ProcessorTime &out)
{
	uint64_t now, user, kernel;
	QueryProcTime(now, user, kernel);

	out.idle = now - (user + kernel);
	out.kernel = kernel;
	out.user = user;
}