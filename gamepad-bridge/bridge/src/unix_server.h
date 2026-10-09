#pragma once
// Listening SOCK_SEQPACKET socket at a path, accepting at most one client at a time.
#include "event_loop.h"
#include "socket_io.h"

#include <functional>
#include <string>

namespace drcb {

class UnixServer
{
public:
	UnixServer(EventLoop& loop, std::string path, const char* what);
	~UnixServer();
	bool listen();
	bool has_client() const { return m_client >= 0; }
	int client() const { return m_client; }
	void drop_client(); // closes the client socket and notifies on_disconnect

	std::function<void()> on_connect;
	std::function<void(const Message&)> on_message;
	std::function<void()> on_disconnect;

private:
	void accept_one();
	void read_client();

	EventLoop& m_loop;
	std::string m_path;
	const char* m_what;
	int m_listen = -1;
	int m_client = -1;
};

} // namespace drcb
