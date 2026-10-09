#pragma once
// SOCK_SEQPACKET message helpers shared by the bridge, fake-cemu and mock-pad.
// One drcb_header + payload per packet, optionally with one fd in SCM_RIGHTS.
#include "drcbridge/ipc.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace drcb {

struct Message
{
	drcb_header header{};
	uint8_t payload[DRCB_MAX_PACKET - sizeof(drcb_header)]{};
	int fd = -1; // received fd, if any (caller owns it)

	template<typename T> const T* as() const
	{
		return header.payload_size >= sizeof(T) ? reinterpret_cast<const T*>(payload) : nullptr;
	}
};

// Returns false on error or a malformed packet (logged).
bool send_msg(int sock, uint16_t type, const void* payload, uint32_t size, int fd_to_pass = -1);

template<typename T> bool send_msg(int sock, uint16_t type, const T& payload, int fd_to_pass = -1)
{
	return send_msg(sock, type, &payload, sizeof(T), fd_to_pass);
}

enum class RecvResult { Ok, Closed, Error };
RecvResult recv_msg(int sock, Message& out);

std::string runtime_dir(); // $XDG_RUNTIME_DIR/cemu-gamepad, created 0700. Empty on failure (logged).

// Creates a memfd of the given size. Returns -1 on failure (logged).
int create_shared_buffer(const char* name, size_t size);

} // namespace drcb
