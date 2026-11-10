#include "util/Socket/Socket.h"

#if BOOST_OS_WINDOWS
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#if BOOST_OS_WINDOWS
using NativeSocket = SOCKET;
using BufLen = int;

static void CloseSocket(NativeSocket s)
{
	closesocket(s);
}

static int Poll(pollfd* fds, size_t count, int timeoutMs)
{
	return WSAPoll(fds, static_cast<ULONG>(count), timeoutMs);
}

static void SetSocketSettings(NativeSocket s)
{
	u_long nonBlocking = 1;
	BOOL reportConnReset = FALSE;
	DWORD bytes = 0;
	ioctlsocket(s, FIONBIO, &nonBlocking);
	WSAIoctl(s, SIO_UDP_CONNRESET, &reportConnReset, sizeof(reportConnReset), nullptr, 0, &bytes, nullptr, nullptr);
}
#else
using NativeSocket = int;
using BufLen = size_t;

static void CloseSocket(NativeSocket s)
{
	close(s);
}

static int Poll(pollfd* fds, size_t count, int timeoutMs)
{
	return poll(fds, static_cast<nfds_t>(count), timeoutMs);
}

static void SetSocketSettings(NativeSocket s)
{
	int flags = fcntl(s, F_GETFL);
	fcntl(s, F_SETFL, flags | O_NONBLOCK);
	fcntl(s, F_SETFD, FD_CLOEXEC);
}
#endif

// -1 turns into INVALID_SOCKET on Windows and into an invalid file descriptor on POSIX
static NativeSocket Native(std::intptr_t s)
{
	return static_cast<NativeSocket>(s);
}

static sockaddr_in MakeAddress(SocketIPv4 ip, uint16 port)
{
	sockaddr_in address{};
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(ip);
	address.sin_port = htons(port);
	return address;
}

UDPSocket::UDPSocket()
{
	m_socket = static_cast<std::intptr_t>(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
	if (IsValid())
		Close();
	SetSocketSettings(Native(m_socket));
}

UDPSocket::~UDPSocket()
{
	Close();
}

UDPSocket::UDPSocket(UDPSocket&& other) noexcept
	: m_socket(std::exchange(other.m_socket, -1))
{
}

UDPSocket& UDPSocket::operator=(UDPSocket&& other) noexcept
{
	if (this != &other)
	{
		Close();
		m_socket = std::exchange(other.m_socket, -1);
	}
	return *this;
}

void UDPSocket::Close()
{
	if (IsValid())
		CloseSocket(Native(m_socket));
	m_socket = -1;
}

bool UDPSocket::Bind(SocketIPv4 ip, uint16 port)
{
	sockaddr_in address = MakeAddress(ip, port);
	return bind(Native(m_socket), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0;
}

std::optional<SocketIPv4> UDPSocket::Resolve(std::string_view domain)
{
	std::string name(domain);
	addrinfo hints{};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	addrinfo* result = nullptr;
	if (getaddrinfo(name.c_str(), nullptr, &hints, &result) != 0)
		return std::nullopt;
	SocketIPv4 ip = ntohl(reinterpret_cast<const sockaddr_in*>(result->ai_addr)->sin_addr.s_addr);
	freeaddrinfo(result);
	return ip;
}

bool UDPSocket::SendTo(SocketIPv4 ip, uint16 port, const void* buf, size_t size)
{
	sockaddr_in address = MakeAddress(ip, port);
	return sendto(Native(m_socket), static_cast<const char*>(buf), static_cast<BufLen>(size), 0, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) >= 0;
}

std::optional<size_t> UDPSocket::RecvFrom(SocketIPv4& ip, uint16& port, void* buf, size_t size)
{
	sockaddr_in address{};
	socklen_t addressSize = sizeof(address);
	auto n = recvfrom(Native(m_socket), static_cast<char*>(buf), static_cast<BufLen>(size), 0, reinterpret_cast<sockaddr*>(&address), &addressSize);
#if BOOST_OS_WINDOWS
	// on truncation Windows fills the buffer but reports an error, POSIX silently truncates
	if (n < 0 && WSAGetLastError() == WSAEMSGSIZE)
		n = static_cast<int>(size);
#endif
	if (n < 0)
		return std::nullopt;
	ip = ntohl(address.sin_addr.s_addr);
	port = ntohs(address.sin_port);
	return static_cast<size_t>(n);
}

UDPWaiter::UDPWaiter()
{
	NativeSocket wake = Native(m_wakeSocket.m_socket);
	sockaddr_in self{};
	socklen_t selfSize = sizeof(self);
	if (!m_wakeSocket.Bind(INADDR_LOOPBACK, 0))
	{
		cemuLog_log(LogType::Force, "UDPWaiter loopback socket creation failed");
		cemuLog_waitForFlush();
		m_wakeSocket.Close();
		return;
	}
	getsockname(wake, reinterpret_cast<sockaddr*>(&self), &selfSize);
	connect(wake, reinterpret_cast<const sockaddr*>(&self), selfSize);
}

UDPWaiter::~UDPWaiter() = default;

void UDPWaiter::WaitForRecv(std::span<UDPSocket* const> sockets, std::vector<UDPSocket*>& ready, sint32 timeoutMs)
{
	ready.clear();
	m_fds.resize(sockets.size() + 1);
	m_fds[0] = {Native(m_wakeSocket.m_socket), POLLIN, 0};
	for (size_t i = 0; i < sockets.size(); i++)
		m_fds[i + 1] = {Native(sockets[i]->m_socket), POLLIN, 0};

	if (Poll(m_fds.data(), m_fds.size(), timeoutMs) <= 0)
		return;

	if (m_fds[0].revents)
	{
		char byte;
		while (recv(Native(m_wakeSocket.m_socket), &byte, 1, 0) > 0)
			;
	}
	// sockets with an error are also reported as ready, the next RecvFrom() clears the error
	for (size_t i = 0; i < sockets.size(); i++)
	{
		if (m_fds[i + 1].revents & (POLLIN | POLLERR | POLLHUP))
			ready.push_back(sockets[i]);
	}
}

void UDPWaiter::InterruptWait()
{
	const char byte = 0;
	send(Native(m_wakeSocket.m_socket), &byte, 1, 0);
}
