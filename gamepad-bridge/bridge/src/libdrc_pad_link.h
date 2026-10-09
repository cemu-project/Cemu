#pragma once
// The real GamePad: libdrc's own host stack, composed exactly as drc::Streamer::Start does
// (libdrc src/streamer.cpp) - VideoStreamer (encode + vstrm/astrm, TSF-paced 59.94 Hz), AudioStreamer,
// CmdClient, UvcUacStateSynchronizer, DeviceConfig, the resync message server - except that HID input goes
// to OUR parser (src/hid_parser.cpp), because libdrc's InputReceiver drops the IMU.
//
// Untested against hardware (POST-HW). Preflight fails loudly without 192.168.1.10 or without a TSF.
#include "event_loop.h"
#include "hid_parser.h"
#include "pad_link.h"

#include <atomic>
#include <memory>
#include <mutex>

namespace drcb {

class LibdrcPadLink : public PadLink
{
public:
	explicit LibdrcPadLink(EventLoop& loop);
	~LibdrcPadLink() override;
	bool start();

	const char* name() const override { return "libdrc (real GamePad)"; }
	drcb_pad_source source() const override { return DRCB_PAD_REAL; }
	bool connected() const override { return m_connected; }
	// libdrc's VideoStreamer latches the newest frame on its own timer: a push never blocks or queues.
	bool can_accept() const override { return m_started; }
	bool submit(const PadFrameView& frame) override;
	// libdrc encodes inside its VideoStreamer; the bridge must not encode as well.
	bool wants_encoded() const override { return false; }
	int battery_level() const override;

private:
	struct Impl;
	std::unique_ptr<Impl> m;
	EventLoop& m_loop;
	bool m_started = false;
	bool m_connected = false;
	void on_input_event(); // event loop thread
	void on_liveness_tick();
};

} // namespace drcb
