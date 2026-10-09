#pragma once
// Per-stage latency recording. Percentiles, never just averages: pacing jitter matters as much as
// the mean (CLAUDE.md). Samples accumulate until report() logs them and clears them.
#include <cstdint>
#include <string>
#include <vector>

namespace drcb {

class StageStats
{
public:
	explicit StageStats(std::string name) : m_name(std::move(name)) {}
	void add_ns(int64_t ns) { m_samples.push_back(ns); }
	void report_and_clear(double window_s); // logs count, rate, p50/p95/p99/p99.9/max in ms
	size_t count() const { return m_samples.size(); }

private:
	std::string m_name;
	std::vector<int64_t> m_samples;
};

} // namespace drcb
