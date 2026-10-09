#include "stats.h"
#include "log.h"

#include <algorithm>

namespace drcb {

void StageStats::report_and_clear(double window_s)
{
	if (m_samples.empty())
	{
		LOGI("stats %-22s n=0", m_name.c_str());
		return;
	}
	std::sort(m_samples.begin(), m_samples.end());
	auto pct = [&](double p) {
		size_t i = size_t(p / 100.0 * double(m_samples.size() - 1) + 0.5);
		return double(m_samples[std::min(i, m_samples.size() - 1)]) / 1e6;
	};
	LOGI("stats %-22s n=%-5zu %6.1f/s  p50 %7.3f  p95 %7.3f  p99 %7.3f  p99.9 %7.3f  max %7.3f ms", m_name.c_str(),
		 m_samples.size(), double(m_samples.size()) / window_s, pct(50), pct(95), pct(99), pct(99.9),
		 double(m_samples.back()) / 1e6);
	m_samples.clear();
}

} // namespace drcb
