#include "unix_server.h"
#include "log.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace drcb {

UnixServer::UnixServer(EventLoop& loop, std::string path, const char* what)
	: m_loop(loop), m_path(std::move(path)), m_what(what) {}

UnixServer::~UnixServer()
{
	if (m_client >= 0)
	{
		m_loop.remove(m_client);
		close(m_client);
	}
	if (m_listen >= 0)
	{
		m_loop.remove(m_listen);
		close(m_listen);
		unlink(m_path.c_str());
	}
}

bool UnixServer::listen()
{
	sockaddr_un addr{};
	addr.sun_family = AF_UNIX;
	if (m_path.size() >= sizeof(addr.sun_path))
	{
		LOGE("%s socket path too long: %s", m_what, m_path.c_str());
		return false;
	}
	strcpy(addr.sun_path, m_path.c_str());

	m_listen = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
	if (m_listen < 0)
	{
		LOGE("%s socket: %s", m_what, strerror(errno));
		return false;
	}
	// A stale socket file from a crashed bridge: refuse if something is actually listening on it.
	if (connect(m_listen, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0)
	{
		LOGE("%s: another bridge is already listening on %s", m_what, m_path.c_str());
		close(m_listen);
		m_listen = -1;
		return false;
	}
	close(m_listen);
	m_listen = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
	unlink(m_path.c_str());

	if (bind(m_listen, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 || ::listen(m_listen, 2) < 0)
	{
		LOGE("%s bind/listen %s: %s", m_what, m_path.c_str(), strerror(errno));
		close(m_listen);
		m_listen = -1;
		return false;
	}
	m_loop.add(m_listen, [this] { accept_one(); });
	LOGI("%s: listening on %s", m_what, m_path.c_str());
	return true;
}

void UnixServer::accept_one()
{
	int fd = accept4(m_listen, nullptr, nullptr, SOCK_CLOEXEC);
	if (fd < 0)
	{
		LOGW("%s accept: %s", m_what, strerror(errno));
		return;
	}
	if (m_client >= 0)
	{
		// One client at a time (docs/IPC.md). Tell the newcomer why, then hang up.
		drcb_reject r{DRCB_VERSION_MAJOR, DRCB_VERSION_MINOR, DRCB_REJECT_BUSY, {}};
		snprintf(r.text, sizeof(r.text), "%s already has a client", m_what);
		send_msg(fd, DRCB_MSG_REJECT, r);
		close(fd);
		LOGW("%s: rejected a second client (busy)", m_what);
		return;
	}
	m_client = fd;
	m_loop.add(m_client, [this] { read_client(); });
	LOGI("%s: client connected", m_what);
	if (on_connect)
		on_connect();
}

void UnixServer::read_client()
{
	Message msg;
	switch (recv_msg(m_client, msg))
	{
	case RecvResult::Ok:
		if (on_message)
			on_message(msg);
		if (msg.fd >= 0)
			close(msg.fd); // handlers that keep an fd dup it
		break;
	case RecvResult::Closed:
		LOGI("%s: client disconnected", m_what);
		drop_client();
		break;
	case RecvResult::Error:
		LOGE("%s: protocol error, dropping client", m_what);
		drop_client();
		break;
	}
}

void UnixServer::drop_client()
{
	if (m_client < 0)
		return;
	m_loop.remove(m_client);
	close(m_client);
	m_client = -1;
	if (on_disconnect)
		on_disconnect();
}

} // namespace drcb
