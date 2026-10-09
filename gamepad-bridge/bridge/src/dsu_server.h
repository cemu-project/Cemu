#pragma once
// DSU ("cemuhook") server: gives Cemu the GamePad's buttons, sticks, touch and motion with no Cemu changes.
// Cemu's own DSU client is the reference for every constant and layout here
// (cemu/src/input/api/DSU/DSUMessages.{h,cpp}, DSUController.cpp, DSUControllerProvider.cpp).
//
// In Cemu: Input settings -> Wii U GamePad -> API "DSUController", 127.0.0.1:26760, controller 1.
// Touch reaches the emulated GamePad through DSU touch point 1 (VPADController::update_touch uses
// has_position/get_position). HOME is sent as the DSU touchpad-click flag (Cemu button 16), because Cemu
// never reads the DSU "ps" field.
#include "drcbridge/ipc.h"
#include "event_loop.h"

#include <cstdint>
#include <netinet/in.h>
#include <vector>

namespace drcb {

class DsuServer
{
public:
	DsuServer(EventLoop& loop, uint16_t port);
	~DsuServer();
	bool start(); // binds 127.0.0.1 only
	// Latest pad state; sends a data packet to every subscribed client.
	void update(const drcb_input_state& s, bool pad_connected);

private:
	struct Client
	{
		sockaddr_in addr;
		int64_t last_request_ns;
		int64_t last_sent_ns = 0;
	};
	void on_readable();
	void send_data(Client& c);

	EventLoop& m_loop;
	uint16_t m_port;
	int m_fd = -1;
	uint32_t m_server_id;
	uint32_t m_packet_index = 0;
	std::vector<Client> m_clients;
	drcb_input_state m_state{};
	bool m_connected = false;
	uint8_t m_touch_id = 0;
	bool m_was_touching = false;
};

} // namespace drcb
