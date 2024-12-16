#include "L2CapWiimote.h"
#include <bluetooth/l2cap.h>

constexpr auto comparator = [](const bdaddr_t& a, const bdaddr_t& b) {
	return bacmp(&a, &b);
};

static auto s_addresses = std::map<bdaddr_t, bool, decltype(comparator)>(comparator);
static std::mutex s_addressMutex;

static bool AttemptConnect(int sockFd, const sockaddr_l2& addr)
{
	auto res = connect(sockFd, reinterpret_cast<const sockaddr*>(&addr),
					   sizeof(sockaddr_l2));
	if (res == 0)
		return true;
	return connect(sockFd, reinterpret_cast<const sockaddr*>(&addr),
				   sizeof(sockaddr_l2)) == 0;
}

static bool AttemptSetNonBlock(int sockFd)
{
	return fcntl(sockFd, F_SETFL, fcntl(sockFd, F_GETFL) | O_NONBLOCK) == 0;
}

L2CapWiimote::L2CapWiimote(int recvFd, int sendFd, bdaddr_t addr)
	: m_recvFd(recvFd), m_sendFd(sendFd), m_addr(addr)
{
}

L2CapWiimote::~L2CapWiimote()
{
	close(m_recvFd);
	close(m_sendFd);
	const auto& b = m_addr.b;
	cemuLog_logDebug(LogType::Force, "Wiimote at {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x} disconnected", b[5], b[4], b[3], b[2], b[1], b[0]);

	// Re-add to candidate vec
	s_addressMutex.lock();
	s_addresses[m_addr] = false;
	s_addressMutex.unlock();
}

void L2CapWiimote::AddCandidateAddress(bdaddr_t addr)
{
	std::scoped_lock lock(s_addressMutex);
	s_addresses.try_emplace(addr, false);
}

std::vector<WiimoteDevicePtr> L2CapWiimote::get_devices()
{
	s_addressMutex.lock();
	std::vector<bdaddr_t> unconnected;
	for (const auto& [addr, connected] : s_addresses)
	{
		if (!connected)
			unconnected.push_back(addr);
	}
	s_addressMutex.unlock();

	std::vector<WiimoteDevicePtr> outDevices;
	for (const auto& addr : unconnected)
	{
		// Socket for sending data to controller, PSM 0x11
		auto sendFd = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
		if (sendFd < 0)
		{
			cemuLog_logDebug(LogType::Force, "Failed to open send socket: {}", strerror(errno));
			continue;
		}

		sockaddr_l2 sendAddr{};
		sendAddr.l2_family = AF_BLUETOOTH;
		sendAddr.l2_psm = htobs(0x11);
		sendAddr.l2_bdaddr = addr;

		if (!AttemptConnect(sendFd, sendAddr) || !AttemptSetNonBlock(sendFd))
		{
			const auto& b = addr.b;
			cemuLog_logDebug(LogType::Force, "Failed to connect send socket to '{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}': {}",
							 b[5], b[4], b[3], b[2], b[1], b[0], strerror(errno));
			close(sendFd);
			continue;
		}

		// Socket for receiving data from controller, PSM 0x13
		auto recvFd = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
		if (recvFd < 0)
		{
			cemuLog_logDebug(LogType::Force, "Failed to open recv socket: {}", strerror(errno));
			close(sendFd);
			continue;
		}
		sockaddr_l2 recvAddr{};
		recvAddr.l2_family = AF_BLUETOOTH;
		recvAddr.l2_psm = htobs(0x13);
		recvAddr.l2_bdaddr = addr;

		if (!AttemptConnect(recvFd, recvAddr) || !AttemptSetNonBlock(recvFd))
		{
			const auto& b = addr.b;
			cemuLog_logDebug(LogType::Force, "Failed to connect recv socket to '{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}': {}",
							 b[5], b[4], b[3], b[2], b[1], b[0], strerror(errno));
			close(sendFd);
			close(recvFd);
			continue;
		}
		outDevices.emplace_back(std::make_shared<L2CapWiimote>(sendFd, recvFd, addr));

		s_addressMutex.lock();
		s_addresses[addr] = true;
		s_addressMutex.unlock();
	}
	return outDevices;
}

bool L2CapWiimote::write_data(const std::vector<uint8>& data)
{
	const auto size = data.size();
	cemu_assert_debug(size < 23);
	uint8 buffer[23];
	// All outgoing messages must be prefixed with 0xA2
	buffer[0] = 0xA2;
	std::memcpy(buffer + 1, data.data(), size);
	const auto outSize = size + 1;
	return send(m_sendFd, buffer, outSize, 0) == outSize;
}

std::optional<std::vector<uint8>> L2CapWiimote::read_data()
{
	uint8 buffer[23];
	const auto nBytes = recv(m_sendFd, buffer, 23, 0);

	if (nBytes < 0 && errno == EWOULDBLOCK)
		return std::vector<uint8>{};
	// All incoming messages must be prefixed with 0xA1
	if (nBytes < 2 || buffer[0] != 0xA1)
		return std::nullopt;
	return std::vector(buffer + 1, buffer + 1 + nBytes - 1);
}

bool L2CapWiimote::operator==(const WiimoteDevice& rhs) const
{
	auto mote = dynamic_cast<const L2CapWiimote*>(&rhs);
	if (!mote)
		return false;
	return bacmp(&m_addr, &mote->m_addr) == 0;
}