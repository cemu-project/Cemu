#include "L2CapWiimote.h"
#include <bluetooth/l2cap.h>

namespace {
	std::vector<bdaddr_t> s_address;
	std::mutex s_addressMutex;

	bool AttemptConnect(int& sockFd, const sockaddr_l2& addr)
	{
		return connect(sockFd, reinterpret_cast<const sockaddr*>(&addr),
					   sizeof(sockaddr_l2)) == 0;
	}
	bool AttemptSetNonBlock(int& sockFd)
	{
		return fcntl(sockFd, F_SETFL, fcntl(sockFd, F_GETFL) | O_NONBLOCK) == 0;
	}
}

L2CapWiimote::L2CapWiimote(int recvFd,int sendFd)
: m_recvFd(recvFd), m_sendFd(sendFd)
{

}

L2CapWiimote::~L2CapWiimote()
{
	::close(m_recvFd);
	::close(m_sendFd);
}

void L2CapWiimote::AddCandidateAddress(bdaddr_t addr)
{
	std::scoped_lock lock(s_addressMutex);
	s_address.push_back(addr);
}

std::vector<WiimoteDevicePtr> L2CapWiimote::get_devices()
{
	s_addressMutex.lock();
	const auto addresses = s_address;
	s_addressMutex.unlock();


	std::vector<WiimoteDevicePtr> outDevices;
	for (auto addr : addresses)
	{
		// Socket for sending data to controller, PSM 0x11
		auto sendFd = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
		if (sendFd < 0) {
			cemuLog_logDebug(LogType::Force, "Failed to open send socket: {}", strerror(errno));
			continue;
		}

		sockaddr_l2 sendAddr{};
		sendAddr.l2_family = AF_BLUETOOTH;
		sendAddr.l2_psm = htobs(0x11);
		sendAddr.l2_bdaddr = addr;

		if (!AttemptConnect(sendFd, sendAddr) || !AttemptSetNonBlock(sendFd)) {
			cemuLog_logDebug(LogType::Force,"Failed to connect send socket to '{:02x}': {}",
							 fmt::join(addr.b, ":"), strerror(errno));
			close(sendFd);
			continue;
		}

		// Socket for receiving data from controller, PSM 0x13
		auto recvFd = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
		if (recvFd < 0) {
			cemuLog_logDebug(LogType::Force,"Failed to open recv socket: {}", strerror(errno));
			close(sendFd);
			continue;
		}
		sockaddr_l2 recvAddr{};
		recvAddr.l2_family = AF_BLUETOOTH;
		recvAddr.l2_psm = htobs(0x13);
		recvAddr.l2_bdaddr = addr;

		if (!AttemptConnect(recvFd, recvAddr) || !AttemptSetNonBlock(recvFd)) {
			cemuLog_logDebug(LogType::Force,"Failed to connect recv socket to '{:02x}': {}",
							 fmt::join(addr.b, ":"), strerror(errno));
			close(sendFd);
			close(recvFd);
			continue;
		}

		outDevices.emplace_back(std::make_shared<L2CapWiimote>(sendFd, recvFd));
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

	// All incoming messages must be prefixed with 0xA1
	if (nBytes < 2 || buffer[0] != 0xA1)
		return {};
	return std::vector(buffer + 1, buffer + 1 + nBytes - 1);
}


bool L2CapWiimote::operator==(const WiimoteDevice& rhs) const
{
	auto mote = dynamic_cast<const L2CapWiimote*>(&rhs);
	if (!mote)
		return false;
	return m_recvFd == mote->m_recvFd || m_recvFd == mote->m_sendFd;
}