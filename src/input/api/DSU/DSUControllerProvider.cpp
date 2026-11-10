#include "input/api/DSU/DSUControllerProvider.h"
#include "input/api/DSU/DSUController.h"

DSUControllerProvider::DSUControllerProvider()
	: base_type(), m_uid(rand())
{
	if (!connect())
	{
		throw std::runtime_error("dsu client can't open the udp connection");
	}

	m_running = true;
	m_workerThread = std::thread(&DSUControllerProvider::WorkerThread, this);
	request_version();
}

DSUControllerProvider::DSUControllerProvider(const DSUProviderSettings& settings)
	: base_type(settings), m_uid(rand())
{
	if (!connect())
	{
		throw std::runtime_error("dsu client can't open the udp connection");
	}

	m_running = true;
	m_workerThread = std::thread(&DSUControllerProvider::WorkerThread, this);
	request_version();
}

DSUControllerProvider::~DSUControllerProvider()
{
	if (m_running)
	{
		m_running = false;
		m_socketWaiter.InterruptWait();
		m_workerThread.join();
	}
}

std::vector<std::shared_ptr<ControllerBase>> DSUControllerProvider::get_controllers()
{
	std::vector<ControllerPtr> result;

	std::array<uint8_t, kMaxClients> indices;
	for (auto i = 0; i < kMaxClients; ++i)
		indices[i] = get_packet_index(i);

	request_pad_info();

	const auto controller_result = wait_update(indices, 3000);
	for (auto i = 0; i < kMaxClients; ++i)
	{
		if (controller_result[i] && is_connected(i))
			result.emplace_back(std::make_shared<DSUController>(i, m_settings));
	}

	return result;
}

bool DSUControllerProvider::connect()
{
	// already connected?
	if (m_receiverPort != 0)
		return true;

	std::optional<SocketIPv4> ip = UDPSocket::Resolve(get_settings().ip);
	if (!ip)
	{
		cemuLog_log(LogType::Force, "dsu client connect error: unable to resolve {}", get_settings().ip);
		return false;
	}
	if (!m_socket.IsValid() || !m_socket.Bind(0, 0))
	{
		cemuLog_log(LogType::Force, "dsu client connect error: unable to create udp socket");
		return false;
	}
	m_receiverIP = *ip;
	m_receiverPort = get_settings().port;
	// reset data
	m_state = {};
	m_prev_state = {};
	return true;
}

bool DSUControllerProvider::is_connected(uint8_t index) const
{
	if (index >= kMaxClients)
		return false;

	std::scoped_lock lock(m_mutex[index]);
	return m_state[index].info.state == DsState::Connected;
}

DSUControllerProvider::ControllerState DSUControllerProvider::get_state(uint8_t index) const
{
	if (index >= kMaxClients)
		return {};

	std::scoped_lock lock(m_mutex[index]);
	return m_state[index];
}

DSUControllerProvider::ControllerState DSUControllerProvider::get_prev_state(uint8_t index) const
{
	if (index >= kMaxClients)
		return {};

	std::scoped_lock lock(m_mutex[index]);
	return m_prev_state[index];
}

std::array<bool, DSUControllerProvider::kMaxClients> DSUControllerProvider::wait_update(
	const std::array<uint8_t, kMaxClients>& indices, size_t timeout) const
{
	std::array<bool, kMaxClients> result{false, false, false, false};

	const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
	do
	{
		for (int i = 0; i < kMaxClients; ++i)
		{
			if (result[i])
				continue;

			std::unique_lock lock(m_mutex[i]);
			result[i] = indices[i] < m_state[i].packet_index;
		}

		if (std::all_of(result.cbegin(), result.cend(), [](const bool& v) { return v == true; }))
			break;

		//std::this_thread::sleep_for(std::chrono::milliseconds(1));
		std::this_thread::yield();
	}
	while (std::chrono::steady_clock::now() < end);

	return result;
}

bool DSUControllerProvider::wait_update(uint8_t index, uint32_t packet_index, size_t timeout) const
{
	if (index >= kMaxClients)
		return false;

	std::unique_lock lock(m_mutex[index]);

	if (packet_index < m_state[index].packet_index)
		return true;

	const auto result = m_wait_cond[index].wait_for(lock, std::chrono::milliseconds(timeout),
	                                                [this, index, packet_index]()
	                                                {
		                                                return packet_index < m_state[index].packet_index;
	                                                });

	return result;
}

uint32_t DSUControllerProvider::get_packet_index(uint8_t index) const
{
	std::scoped_lock lock(m_mutex[index]);
	return m_state[index].packet_index;
}

void DSUControllerProvider::QueueMessage(std::unique_ptr<ClientMessage> msg)
{
	m_sendQueue.push(std::move(msg));
	m_socketWaiter.InterruptWait();
}

void DSUControllerProvider::request_version()
{
	auto msg = std::make_unique<VersionRequest>(m_uid);
	QueueMessage(std::move(msg));
}

void DSUControllerProvider::request_pad_info()
{
	auto msg = std::make_unique<ListPorts>(m_uid, 4, std::array<uint8_t, 4>{0, 1, 2, 3});
	QueueMessage(std::move(msg));
}

void DSUControllerProvider::request_pad_info(uint8_t index)
{
	if (index >= kMaxClients)
		return;

	auto msg = std::make_unique<ListPorts>(m_uid, 1, std::array<uint8_t, 4>{index});
	QueueMessage(std::move(msg));
}

void DSUControllerProvider::request_pad_data()
{
	auto msg = std::make_unique<DataRequest>(m_uid);
	QueueMessage(std::move(msg));
}

void DSUControllerProvider::request_pad_data(uint8_t index)
{
	if (index >= kMaxClients)
		return;

	m_padDataRequested[index] = true;
	auto msg = std::make_unique<DataRequest>(m_uid, index);
	QueueMessage(std::move(msg));
}

MotionSample DSUControllerProvider::get_motion_sample(uint8_t index) const
{
	if (index >= kMaxClients)
		return MotionSample();
	std::scoped_lock lock(m_mutex[index]);
	return m_state[index].motion_sample;
}


void DSUControllerProvider::WorkerThread()
{
	SetThreadName("DSU-Worker");
	UDPSocket* sockets[] = {&m_socket};
	std::vector<UDPSocket*> readySockets;
	while (m_running.load(std::memory_order_relaxed))
	{
		std::array<char, 100> recvBuf;
		SocketIPv4 senderIP;
		uint16 senderPort;
		// process any received messages
		while (std::optional<size_t> len = m_socket.RecvFrom(senderIP, senderPort, recvBuf.data(), recvBuf.size()))
			ProcessMessage(recvBuf.data(), *len);
		// send any queued outgoing messages
		std::unique_ptr<ClientMessage> msg;
		while (m_sendQueue.try_pop(msg))
			SendClientMessage(*msg);

		ResendPadDataRequests();
		// wait for next message to be received or until interrupted to send a message. With 250ms timeout so that ResendStalledPadDataRequests can be called periodically
		m_socketWaiter.WaitForRecv(sockets, readySockets, 250);
	}
}

void DSUControllerProvider::ProcessMessage(const char* data, size_t len)
{
	cemuLog_log(LogType::DSUProvider, "DSU: received message with len: 0x{:x}", len);
	if (len < sizeof(ServerMessage)) // too short to be a valid message
		return;
	const auto msg = (const ServerMessage*)data;
	switch (msg->GetMessageType())
	{
	case MessageType::Version:
		{
			const auto rsp = (const VersionResponse*)msg;
			if (!rsp->IsValid())
			{
				cemuLog_log(LogType::DSUProvider, "DSU: VersionResponse is invalid!");
				return;
			}
			cemuLog_log(LogType::DSUProvider, "DSU: server version is: 0x{:x}", rsp->GetVersion());
			m_server_version = rsp->GetVersion();
			break;
		}
	case MessageType::Information:
		{
			const auto info = (const PortInfo*)msg;
			if (!info->IsValid())
			{
				cemuLog_log(LogType::DSUProvider, "DSU: PortInfo is invalid!");
				return;
			}

			const uint8 index = info->GetIndex();
			cemu_assert(index < kMaxClients);
			cemuLog_log(LogType::DSUProvider, "DSU: received PortInfo for index {}", index);

			{
				std::scoped_lock lock(m_mutex[index]);
				m_prev_state[index] = m_state[index];
				m_state[index] = *info;
				m_wait_cond[index].notify_all();
			}
			SendPadDataRequest(index);
			break;
		}
	case MessageType::Data:
		{
			const auto rsp = (const DataResponse*)msg;
			if (!rsp->IsValid())
			{
				cemuLog_log(LogType::DSUProvider, "DSU: DataResponse is invalid!");
				return;
			}

			const uint8 index = rsp->GetIndex();
			cemu_assert(index < kMaxClients);
			cemuLog_log(LogType::DSUProvider, "DSU: received DataResponse for index {}", index);

			{
				std::scoped_lock lock(m_mutex[index]);
				m_prev_state[index] = m_state[index];
				m_state[index] = *rsp;
				m_wait_cond[index].notify_all();
				// update motion info immediately, guaranteeing that we dont drop packets
				integrate_motion(index, *rsp);
			}
			m_padDataResendCount[index] = 0;
			SendPadDataRequest(index);
			break;
		}
	}
}

void DSUControllerProvider::SendClientMessage(const ClientMessage& msg)
{
	cemuLog_log(LogType::DSUProvider, "DSU: sending message: 0x{:x} (len: 0x{:x})", (int)msg.GetMessageType(), msg.GetSize());
	m_socket.SendTo(m_receiverIP, m_receiverPort, &msg, msg.GetSize()); // failed sends are not retried, stalled streams recover via ResendStalledPadDataRequests()
}

void DSUControllerProvider::SendPadDataRequest(uint8 index)
{
	SendClientMessage(DataRequest(m_uid, index));
	m_lastPadDataRequest[index] = std::chrono::steady_clock::now();
}

// re-request data from pad servers which didn't respond, with increasing cooldown (500ms to 5s)
void DSUControllerProvider::ResendPadDataRequests()
{
	auto now = std::chrono::steady_clock::now();
	for (uint8 i = 0; i < kMaxClients; i++)
	{
		auto delay = std::min(std::chrono::milliseconds(500 << (m_padDataResendCount[i]/10)), std::chrono::milliseconds(5000));
		if (m_padDataRequested[i] && now - m_lastPadDataRequest[i] >= delay)
		{
			SendPadDataRequest(i);
			if (m_padDataResendCount[i] < 40)
				m_padDataResendCount[i]++;
		}
	}
}

void DSUControllerProvider::integrate_motion(uint8_t index, const DataResponse& data_response)
{
	const uint64 ts = data_response.GetMotionTimestamp();
	if (ts <= m_last_motion_timestamp[index])
	{
		const uint64 dif = m_last_motion_timestamp[index] - ts;
		if (dif >= 10000000) // timestamp more than 10 seconds in the past, a controller reset probably happened
			m_last_motion_timestamp[index] = 0;
		return;
	}

	const uint64 elapsedTime = ts - m_last_motion_timestamp[index];
	m_last_motion_timestamp[index] = ts;
	const double elapsedTimeD = (double)elapsedTime / 1000000.0;
	const auto& acc = data_response.GetAcceleration();
	const auto& gyro = data_response.GetGyro();

	m_motion_handler[index].processMotionSample((float)elapsedTimeD,
	                                            gyro.x * 0.0174533f,
	                                            gyro.y * 0.0174533f,
	                                            gyro.z * 0.0174533f,
	                                            acc.x,
	                                            -acc.y,
	                                            -acc.z);

	m_state[index].motion_sample = m_motion_handler[index].getMotionSample();
}

DSUControllerProvider::ControllerState& DSUControllerProvider::ControllerState::operator=(const PortInfo& port_info)
{
	info = port_info.GetInfo();
	last_update = std::chrono::steady_clock::now();
	packet_index++; // increase packet index for every packet we assign/recv
	return *this;
}

DSUControllerProvider::ControllerState& DSUControllerProvider::ControllerState::operator=(
	const DataResponse& data_response)
{
	this->operator=(static_cast<const PortInfo&>(data_response));
	data = data_response.GetData();
	return *this;
}