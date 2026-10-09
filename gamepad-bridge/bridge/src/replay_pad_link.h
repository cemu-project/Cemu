#pragma once
// PadLink that plays a HID replay file through the real parser (TASKS.md 8.5). Frames are accepted and
// dropped (reported presented immediately, flagged estimated), so the video side keeps running.
#include "event_loop.h"
#include "hid_parser.h"
#include "hid_replay_file.h"
#include "pad_link.h"

#include <string>
#include <vector>

namespace drcb {

class ReplayPadLink : public PadLink
{
public:
	ReplayPadLink(EventLoop& loop, std::string path, bool loop_forever);
	bool start();

	const char* name() const override { return "replay"; }
	drcb_pad_source source() const override { return DRCB_PAD_MOCK; }
	bool connected() const override { return m_running; }
	bool can_accept() const override { return m_running; }
	bool submit(const PadFrameView& frame) override;
	bool wants_encoded() const override { return false; } // frames are discarded

private:
	void tick();

	EventLoop& m_loop;
	std::string m_path;
	bool m_loop_forever;
	std::vector<HidRecord> m_records;
	size_t m_next = 0;
	int64_t m_t0 = 0;
	bool m_running = false;
	uint64_t m_parsed = 0, m_rejected = 0;
};

} // namespace drcb
