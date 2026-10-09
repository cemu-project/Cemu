#include "log.h"
#include "clock.h"

#include <cstdio>
#include <mutex>

namespace drcb {

namespace {
LogLevel g_level = LogLevel::Info;
const char* g_tag = "drcb";
std::mutex g_mutex;
const char* level_name(LogLevel l)
{
	switch (l)
	{
	case LogLevel::Debug: return "DEBUG";
	case LogLevel::Info: return "INFO ";
	case LogLevel::Warn: return "WARN ";
	case LogLevel::Error: return "ERROR";
	}
	return "?";
}
} // namespace

void log_set_level(LogLevel level) { g_level = level; }
void log_set_tag(const char* tag) { g_tag = tag; }

void log(LogLevel level, const char* fmt, ...)
{
	if (level < g_level)
		return;
	// Monotonic timestamp, so log lines line up with the frame timestamps.
	const int64_t t = now_ns();
	std::lock_guard lock(g_mutex);
	fprintf(stderr, "[%lld.%06lld] %s %s: ", (long long)(t / 1'000'000'000), (long long)(t % 1'000'000'000 / 1000),
			level_name(level), g_tag);
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

} // namespace drcb
