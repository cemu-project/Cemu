#pragma once

// minimal cross-platform wrapper for non-blocking IPv4 UDP sockets with interruptible wait. Later we might also add TCP sockets.

using SocketIPv4 = uint32; // host byte order

struct pollfd;

class UDPSocket
{
	friend class UDPWaiter;

public:
	UDPSocket();
	~UDPSocket();

	UDPSocket(const UDPSocket&) = delete;
	UDPSocket& operator=(const UDPSocket&) = delete;
	UDPSocket(UDPSocket&& other) noexcept;
	UDPSocket& operator=(UDPSocket&& other) noexcept;

	bool IsValid() const { return m_socket != -1; }

	bool Bind(SocketIPv4 ip, uint16 port);
	static std::optional<SocketIPv4> Resolve(std::string_view domain);

	// returns false if the datagram could not be sent (would block or error)
	bool SendTo(SocketIPv4 ip, uint16 port, const void* buf, size_t size);

	// returns nullopt if no datagram is pending or an error occurred. Received datagrams can be empty (size is zero)
	// datagrams larger than the buffer are truncated
	std::optional<size_t> RecvFrom(SocketIPv4& ip, uint16& port, void* buf, size_t size);

private:
	void Close();

	std::intptr_t m_socket = -1; // native SOCKET or fd, -1 if invalid
};

class UDPWaiter
{
public:
	UDPWaiter();
	~UDPWaiter();

	UDPWaiter(const UDPWaiter&) = delete;
	UDPWaiter& operator=(const UDPWaiter&) = delete;

	// waits until at least one of the sockets has a datagram pending
	// timeoutMs is -1 for infinite and 0 to only check and return immediately. May return empty results
	void WaitForRecv(std::span<UDPSocket* const> sockets, std::vector<UDPSocket*>& ready, sint32 timeoutMs);

	// can be called from any thread to interrupt WaitForRecv
	void InterruptWait();

private:
	UDPSocket m_wakeSocket; // loopback socket used to wake up poll in WaitForRecv
	std::vector<pollfd> m_fds;
};
