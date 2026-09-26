#include "util/highresolutiontimer/HighResolutionTimer.h"

HighResolutionTimer HighResolutionTimer::now()
{
#if BOOST_OS_WINDOWS
	LARGE_INTEGER pc;
	QueryPerformanceCounter(&pc);
	return HighResolutionTimer(pc.QuadPart);
#elif BOOST_OS_LINUX
    timespec pc;
    clock_gettime(CLOCK_MONOTONIC_RAW, &pc);
    uint64 nsec = (uint64)pc.tv_sec * (uint64)1000000000 + (uint64)pc.tv_nsec;
    return HighResolutionTimer(nsec);
#elif BOOST_OS_MACOS
	return HighResolutionTimer(clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW));
#elif BOOST_OS_BSD
    timespec pc;
    clock_gettime(CLOCK_MONOTONIC, &pc);
    uint64 nsec = (uint64)pc.tv_sec * (uint64)1000000000 + (uint64)pc.tv_nsec;
    return HighResolutionTimer(nsec);
#endif
}

HRTick HighResolutionTimer::getFrequency()
{
	return m_freq;
}

uint64 HighResolutionTimer::m_freq = []() -> uint64 {
#if BOOST_OS_WINDOWS
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	return (uint64)(freq.QuadPart);
#elif BOOST_OS_MACOS
	return 1000000000;
#elif BOOST_OS_BSD
	timespec pc;
	clock_getres(CLOCK_MONOTONIC, &pc);
	return (uint64)1000000000 / (uint64)pc.tv_nsec;
#else
    timespec pc;
    clock_getres(CLOCK_MONOTONIC_RAW, &pc);
    return (uint64)1000000000 / (uint64)pc.tv_nsec;
#endif
}();

struct FrameBenchmarkHelper::Entry
{
	HRTick totalTicks{};
	HRTick startTick{};
};

struct
{
	std::map<std::string, FrameBenchmarkHelper::Entry, std::less<>> m_entries;
	uint32 m_frameCount{};
}s_frameBenchmarkHelperState;

FrameBenchmarkHelper::FrameBenchmarkHelper(std::string_view name)
{
	auto& entries = s_frameBenchmarkHelperState.m_entries;
	auto it = entries.find(name);
	if (it == entries.end())
		it = entries.try_emplace(std::string(name)).first;
	m_entry = &it->second;
	m_entry->startTick = HighResolutionTimer::now().getTick();
}

FrameBenchmarkHelper::~FrameBenchmarkHelper()
{
	HRTick endTick = HighResolutionTimer::now().getTick();
	m_entry->totalTicks += (endTick - m_entry->startTick);
}

void FrameBenchmarkHelper::FrameEnd()
{
	if (s_frameBenchmarkHelperState.m_entries.empty())
		return;
	s_frameBenchmarkHelperState.m_frameCount++;
	if (s_frameBenchmarkHelperState.m_frameCount != 100)
		return;
	s_frameBenchmarkHelperState.m_frameCount = 0;
	cemuLog_log(LogType::Force, "FrameBenchmarkHelper results (avg over last 100 frames):");
	for (auto& [name, entry] : s_frameBenchmarkHelperState.m_entries)
	{
		double millisecondsPerFrame = HighResolutionTimer::getTimeDiff(0, entry.totalTicks) * 1000.0 / 100.0;
		entry.totalTicks = 0;
		cemuLog_log(LogType::Force, "  {}: {:.4f} ms/frame", name, millisecondsPerFrame);
	}
}
