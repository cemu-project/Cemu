#include "replay_pad_link.h"
#include "clock.h"
#include "log.h"

namespace drcb {

ReplayPadLink::ReplayPadLink(EventLoop& loop, std::string path, bool loop_forever)
	: m_loop(loop), m_path(std::move(path)), m_loop_forever(loop_forever) {}

bool ReplayPadLink::start()
{
	if (!load_hid_replay(m_path, m_records))
		return false;
	LOGI("replay: %zu HID packets (%.2f s) from %s%s", m_records.size(), double(m_records.back().t_us) / 1e6,
		 m_path.c_str(), m_loop_forever ? ", looping" : "");
	m_t0 = now_ns();
	m_running = true;
	// 1 kHz is plenty to honour 180 Hz packet timing.
	m_loop.add_timer(1'000'000, [this] { tick(); });
	if (on_connection_changed)
		on_connection_changed(true);
	return true;
}

void ReplayPadLink::tick()
{
	if (!m_running)
		return;
	const int64_t elapsed_us = (now_ns() - m_t0) / 1000;
	while (m_next < m_records.size() && m_records[m_next].t_us <= elapsed_us)
	{
		drcb_input_state s;
		HidExtra extra;
		if (parse_hid(m_records[m_next].packet, sizeof(m_records[m_next].packet), TouchCalibration{}, s, extra))
		{
			m_parsed++;
			s.seq = m_parsed;
			s.t_received_ns = now_ns();
			if (on_input)
				on_input(s);
		}
		else
			m_rejected++;
		m_next++;
	}
	if (m_next >= m_records.size())
	{
		LOGI("replay: finished (%llu parsed, %llu rejected)", (unsigned long long)m_parsed, (unsigned long long)m_rejected);
		if (m_loop_forever)
		{
			m_next = 0;
			m_t0 = now_ns();
		}
		else
			m_running = false;
	}
}

bool ReplayPadLink::submit(const PadFrameView& frame)
{
	if (on_presented)
		on_presented(frame.pad_frame_id, now_ns(), DRCB_PRESENTED_ESTIMATED);
	return true;
}

} // namespace drcb
