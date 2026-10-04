#pragma once

#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <boost/asio.hpp>



namespace nsyshid
{
	class SkylanderPortalIPCServer
	{
	public:
		SkylanderPortalIPCServer(uint16_t port);
		~SkylanderPortalIPCServer();

		void Start();
		void Stop();

#ifdef _WIN32
		using Protocol = boost::asio::ip::tcp;
#else
		using Protocol = boost::asio::local::stream_protocol;
#endif

		void ServerLoop();
		void HandleClient(Protocol::socket& socket);
		std::string ProcessCommand(const std::string& command);

		std::string HandleLoad(int slot, const std::string& path);
		std::string HandleRemove(int slot);
		std::string HandleStatus();
		std::string HandleClear();

		uint16_t m_port;
		std::atomic<bool> m_running{false};
		std::thread m_serverThread;

		boost::asio::io_context m_ioContext;
		std::unique_ptr<Protocol::acceptor> m_acceptor;
	};

	extern std::unique_ptr<SkylanderPortalIPCServer> g_skylanderIpcServer;
}
