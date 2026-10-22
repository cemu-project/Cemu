#include "SkylanderPortalIPC.h"
#include "Skylander.h"
#include <iostream>
#include <sstream>

namespace nsyshid
{
	std::unique_ptr<SkylanderPortalIPCServer> g_skylanderIpcServer = nullptr;

	SkylanderPortalIPCServer::SkylanderPortalIPCServer(uint16_t port)
		: m_port(port)
	{
	}

	SkylanderPortalIPCServer::~SkylanderPortalIPCServer()
	{
		Stop();
	}

	void SkylanderPortalIPCServer::Start()
	{
		if (m_running.exchange(true))
			return;

		m_serverThread = std::thread([this]() {
			try
			{
#ifdef _WIN32
				Protocol::endpoint endpoint(boost::asio::ip::address_v4::loopback(), m_port);
#else
				const std::string socketPath = "/tmp/rpcs3.skylanders.sock";
				std::remove(socketPath.c_str());
				Protocol::endpoint endpoint(socketPath);
#endif
				m_acceptor = std::make_unique<Protocol::acceptor>(m_ioContext, endpoint);
				ServerLoop();
			}
			catch (std::exception& e)
			{
				std::cerr << "Skylander IPC Server Error: " << e.what() << std::endl;
			}
		});
	}

	void SkylanderPortalIPCServer::Stop()
	{
		if (!m_running.exchange(false))
			return;

		if (m_acceptor)
		{
			boost::system::error_code ec;
			m_acceptor->close(ec);
		}

		if (m_serverThread.joinable())
		{
			m_serverThread.join();
		}

#ifndef _WIN32
		std::remove("/tmp/rpcs3.skylanders.sock");
#endif
	}

	void SkylanderPortalIPCServer::ServerLoop()
	{
		while (m_running)
		{
			try
			{
				Protocol::socket socket(m_ioContext);
				m_acceptor->accept(socket);
				if (m_running)
				{
					HandleClient(socket);
				}
			}
			catch (...) 
			{
				break;
			}
		}
	}

	void SkylanderPortalIPCServer::HandleClient(Protocol::socket& socket)
	{
		try
		{
			boost::asio::streambuf buffer(4096);
			boost::system::error_code ec;
			boost::asio::read_until(socket, buffer, '\n', ec);

			if (!ec)
			{
				std::istream is(&buffer);
				std::string line;
				std::getline(is, line);

				if (!line.empty() && line.back() == '\r')
					line.pop_back();

				std::string response = ProcessCommand(line);
				boost::asio::write(socket, boost::asio::buffer(response));
			}

			socket.close(ec);
		}
		catch (...) {}
	}

	std::string SkylanderPortalIPCServer::HandleLoad(int slot, const std::string& path)
	{
		if (slot < -1 || slot > 15)
			return "error invalid slot\n";

		auto skyFile = std::unique_ptr<FileStream>(FileStream::openFile2(fs::path(path), true));
		if (!skyFile) 
			return "error cannot open file\n";

		std::array<uint8, nsyshid::SKY_FIGURE_SIZE> buf;
		if (skyFile->readData(buf.data(), buf.size()) != buf.size())
			return "error file too small\n";

		uint8 result = g_skyportal.LoadSkylander(buf.data(), std::move(skyFile), slot);
		if (result == 0xFF)
			return "error no free slot\n";

		return "ok " + std::to_string(result) + "\n";
	}

	std::string SkylanderPortalIPCServer::HandleRemove(int slot)
	{
		if (slot < 0 || slot > 15)
		return "error invalid slot\n";

		if (!g_skyportal.RemoveSkylander(static_cast<uint8>(slot)))
			return "error slot empty\n";

		return "ok\n";
	}

	std::string SkylanderPortalIPCServer::HandleStatus()
	{
		std::string result;
		for (uint8 i = 0; i < 16; i++)
		{
			uint8 status;
			uint16 id, variant;
			g_skyportal.GetFigureInfo(i, status, id, variant);
			if (status & 1)
				result += "slot" + std::to_string(i) + " loaded " + std::to_string(id) + " " + std::to_string(variant) + "\n";
			else
				result += "slot" + std::to_string(i) + " empty\n";
		}
		result += "ok\n";
		return result;
	}

	std::string SkylanderPortalIPCServer::HandleClear()
	{
		for (uint8 i = 0; i < 16; i++)
			g_skyportal.RemoveSkylander(i);
		return "ok\n";
	}

		std::string SkylanderPortalIPCServer::ProcessCommand(const std::string& line)
	{
		if (line.empty())
			return "error empty command\n";

		const size_t firstSpace = line.find(' ');
		const std::string cmd  = line.substr(0, firstSpace);

		if (cmd == "status")
			return HandleStatus();

		if (cmd == "clear")
			return HandleClear();

		if (cmd == "remove")
		{
			if (firstSpace == std::string::npos)
				return "error missing slot\n";
			
			int slot;
			try {
				slot = std::stoi(line.substr(firstSpace + 1));
			} catch (...) {
				return "error invalid slot\n";
			}
			return HandleRemove(slot);
		}

		if (cmd == "load")
		{
			if (firstSpace == std::string::npos)
				return "error missing arguments\n";
			const std::string rest  = line.substr(firstSpace + 1);
			const size_t secondSpace  = rest.find(' ');
			if (secondSpace == std::string::npos)
				return "error missing path\n";
			int slot;
			try {
				slot = std::stoi(rest.substr(0, secondSpace));
			} catch (...) {
				return "error invalid arguments\n";
			}
			
			std::string path = rest.substr(secondSpace + 1);
			return HandleLoad(slot, path);
		}

		return "error unknown command\n";
	}
}