#pragma once
// PadLink to the mock-pad window (tools/mock-pad.cpp) over $XDG_RUNTIME_DIR/cemu-gamepad/mockpad.sock.
// Reuses the IPC v1 structs in the other direction: the bridge offers frames, the mock pad returns
// FRAME_RELEASE, FRAME_PRESENTED (time it drew the frame, flagged as estimated) and INPUT_STATE.
#include "pad_link.h"
#include "shm_slots.h"
#include "unix_server.h"

namespace drcb {

class MockPadLink : public PadLink
{
public:
	MockPadLink(EventLoop& loop, const std::string& socket_path);
	bool start();

	const char* name() const override { return "mock-pad"; }
	drcb_pad_source source() const override { return DRCB_PAD_MOCK; }
	bool connected() const override { return m_ready; }
	bool can_accept() const override { return m_ready && m_slots.find_free() >= 0; }
	bool submit(const PadFrameView& frame) override;
	bool wants_encoded() const override { return true; } // the mock pad decodes the real bitstream

	static constexpr uint32_t kSlots = 3;

private:
	void handle(const Message& msg);

	UnixServer m_server;
	ShmSlots m_slots;
	bool m_ready = false;
};

} // namespace drcb
