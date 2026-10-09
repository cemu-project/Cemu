#pragma once
// The Cemu side of the bridge: IPC v1 server on $XDG_RUNTIME_DIR/cemu-gamepad/bridge.sock (docs/IPC.md).
#include "shm_slots.h"
#include "unix_server.h"

#include <functional>

namespace drcb {

class CemuLink
{
public:
	CemuLink(EventLoop& loop, const std::string& socket_path, uint32_t max_w, uint32_t max_h);
	bool start();
	bool attached() const { return m_ready; }

	void send_presented(uint64_t cemu_frame_id, int64_t t_presented_ns, uint32_t flags);
	void send_input(const drcb_input_state& s);
	void send_pad_status(bool connected, drcb_pad_source source, int battery_percent);
	void shutdown(); // GOODBYE(SHUTDOWN) to the client, if any

	// Called with the frame's pixels while the bridge owns the slot. The slot is released right after
	// this returns, so the handler must copy what it needs.
	std::function<void(const drcb_frame_submit&, const uint8_t* pixels, int64_t t_received_ns)> on_frame;
	std::function<void(bool attached)> on_attach_changed;

	static constexpr uint32_t kSlots = 3;

private:
	void handle(const Message& msg);

	UnixServer m_server;
	ShmSlots m_slots;
	uint32_t m_max_w, m_max_h;
	bool m_ready = false;
	// Pad state to report in WELCOME.
	bool m_pad_connected = false;
	drcb_pad_source m_pad_source = DRCB_PAD_NONE;
};

} // namespace drcb
