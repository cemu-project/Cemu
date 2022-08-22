#pragma once

#include "input/motion/MotionHandler.h"
#include "input/api/DSU/DSUMessages.h"

#include "input/api/ControllerProvider.h"

#include <boost/asio.hpp>

#ifndef HAS_DSU
#define HAS_DSU 1
#endif

// #define DEBUG_DSU_CLIENT

struct DSUProviderSettings : public ControllerProviderSettings
{
	std::string ip;
	uint16 port;

	DSUProviderSettings() : ip("127.0.0.1"), port(26760) {}

	DSUProviderSettings(std::string ip, uint16 port)
		: ip(std::move(ip)), port(port)
	{
	}

	bool operator==(const DSUProviderSettings& s) const
	{
		return port == s.port && ip == s.ip;
	}

	bool operator==(const ControllerProviderSettings& s) const override
	{
		const auto* ptr = dynamic_cast<const DSUProviderSettings*>(&s);
		return ptr && *this == *ptr;
	}
};


class DSUControllerProvider : public ControllerProvider<DSUProviderSettings>
{
	friend class DSUController;
	using base_type = ControllerProvider<DSUProviderSettings>;
public:
	constexpr static int kMaxClients = 8;

	struct ControllerState
	{
		// when was info updated last time
		std::chrono::steady_clock::time_point last_update{};
		uint64_t packet_index = 0; // our packet index count

		PortInfoData info{};
		DataResponseData data{};
		MotionSample motion_sample{};

		ControllerState& operator=(const PortInfo& port_info);
		ControllerState& operator=(const DataResponse& data_response);
	};

	DSUControllerProvider();
	DSUControllerProvider(const DSUProviderSettings& settings);
	~DSUControllerProvider();

	inline static InputAPI::Type kAPIType = InputAPI::DSUClient;
	InputAPI::Type api() const override { return kAPIType; }

	std::vector<std::shared_ptr<ControllerBase>> get_controllers() override;

	bool connect();

	bool is_connected(uint8_t index) const;
	ControllerState get_state(uint8_t index) const;
	ControllerState get_prev_state(uint8_t index) const;
	MotionSample get_motion_sample(uint8_t index) const;

	std::array<bool, kMaxClients> wait_update(const std::array<uint8_t, kMaxClients>& indices, size_t timeout) const;
	bool wait_update(uint8_t index, uint32_t packet_index, size_t timeout) const;

	uint32_t get_packet_index(uint8_t index) const;
	// refresh pad info for all pads
	void request_pad_info();
	// refresh pad info for pad with given index
	void request_pad_info(uint8_t index);
	void request_version();
	void request_pad_data();
	void request_pad_data(uint8_t index);


private:
	uint16 m_server_version = 0;

	std::atomic_bool m_running = false;
	std::thread m_reader_thread, m_writer_thread;

	void reader_thread();
	void writer_thread();
	void integrate_motion(uint8_t index, const DataResponse& data_response);

	std::mutex m_writer_mutex;
	std::condition_variable m_writer_cond;

	uint32 m_uid;
	boost::asio::io_service m_io_service;
	boost::asio::ip::udp::endpoint m_receiver_endpoint;
	boost::asio::ip::udp::socket m_socket;

	std::array<ControllerState, kMaxClients> m_state{};
	std::array<ControllerState, kMaxClients> m_prev_state{};
	mutable std::array<std::mutex, kMaxClients> m_mutex;
	mutable std::array<std::condition_variable, kMaxClients> m_wait_cond;

	std::queue<std::unique_ptr<ClientMessage>> m_writer_jobs;

	std::array<WiiUMotionHandler, kMaxClients> m_motion_handler;
	std::array<uint64, kMaxClients> m_last_motion_timestamp{};
};
