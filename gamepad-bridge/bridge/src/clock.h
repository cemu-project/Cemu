#pragma once
#include <cstdint>
#include <ctime>

namespace drcb {

// CLOCK_MONOTONIC in nanoseconds. The only clock used across the Cemu <-> bridge boundary (docs/IPC.md).
inline int64_t now_ns()
{
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return int64_t(ts.tv_sec) * 1'000'000'000 + ts.tv_nsec;
}

} // namespace drcb
