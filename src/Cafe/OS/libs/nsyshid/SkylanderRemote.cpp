#include "SkylanderRemote.h"
#include "Skylander.h"

#include <boost/asio.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

namespace nsyshid::SkylanderRemote
{
	using boost::asio::ip::tcp;
	using FigureData = std::array<uint8, SKY_FIGURE_SIZE>;

	// how long to wait after the game's last write before sending the figure back,
	// so a burst of block writes becomes one message
	constexpr auto SAVE_DEBOUNCE = std::chrono::milliseconds(250);

	struct Connection : std::enable_shared_from_this<Connection>
	{
		explicit Connection(boost::asio::io_context& ctx) : socket(ctx) {}

		tcp::socket socket;
		std::string remoteName;

		std::mutex mtx;
		std::condition_variable cv;
		std::deque<std::vector<uint8>> outQueue;
		std::map<uint8, FigureData> pendingSaves;
		std::chrono::steady_clock::time_point lastSaveTime;
		bool closed = false;
		bool flushNow = false; // skip the debounce (used when shutting down)
		bool sending = false;
		std::set<uint8> ownedSlots;

		std::thread reader;
		std::thread writer;

		void Send(std::vector<uint8> msg)
		{
			std::lock_guard lock(mtx);
			if (closed)
				return;
			outQueue.push_back(std::move(msg));
			cv.notify_all();
		}

		// called from the emulator thread (under the portal mutex) - must not block
		void QueueSave(uint8 slot, const FigureData& data)
		{
			std::lock_guard lock(mtx);
			if (closed)
				return;
			pendingSaves[slot] = data;
			lastSaveTime = std::chrono::steady_clock::now();
			cv.notify_all();
		}

		void Close()
		{
			{
				std::lock_guard lock(mtx);
				if (closed)
					return;
				closed = true;
				cv.notify_all();
			}
			boost::system::error_code ec;
			socket.shutdown(tcp::socket::shutdown_both, ec);
			socket.close(ec);
		}

		void WriterLoop()
		{
			while (true)
			{
				std::vector<std::vector<uint8>> toSend;
				{
					std::unique_lock lock(mtx);
					cv.wait(lock, [&] { return closed || !outQueue.empty() || !pendingSaves.empty(); });
					if (closed)
						return;
					// let a burst of writes settle unless a reply is waiting
					while (outQueue.empty() && !pendingSaves.empty() && !closed && !flushNow)
					{
						auto due = lastSaveTime + SAVE_DEBOUNCE;
						if (std::chrono::steady_clock::now() >= due)
							break;
						cv.wait_until(lock, due);
					}
					if (closed)
						return;
					// saves first, so a 'u' reply always arrives after the final save
					for (auto& [slot, data] : pendingSaves)
					{
						std::vector<uint8> msg;
						msg.reserve(2 + data.size());
						msg.push_back('D');
						msg.push_back(slot);
						msg.insert(msg.end(), data.begin(), data.end());
						toSend.push_back(std::move(msg));
					}
					pendingSaves.clear();
					while (!outQueue.empty())
					{
						toSend.push_back(std::move(outQueue.front()));
						outQueue.pop_front();
					}
					sending = true;
				}
				for (auto& msg : toSend)
				{
					boost::system::error_code ec;
					boost::asio::write(socket, boost::asio::buffer(msg), ec);
					if (ec)
					{
						Close();
						return;
					}
				}
				{
					std::lock_guard lock(mtx);
					sending = false;
				}
				cv.notify_all();
			}
		}

		// send everything queued right away; waits up to `timeout`
		void Flush(std::chrono::milliseconds timeout)
		{
			std::unique_lock lock(mtx);
			flushNow = true;
			cv.notify_all();
			cv.wait_for(lock, timeout, [&] { return closed || (outQueue.empty() && pendingSaves.empty() && !sending); });
		}
	};

	struct Server
	{
		boost::asio::io_context ctx;
		std::unique_ptr<tcp::acceptor> acceptor;
		std::thread acceptThread;
		std::string password;
		uint16_t port = 0;
		std::atomic<bool> stopping = false;

		std::mutex connMtx;
		std::vector<std::shared_ptr<Connection>> connections;
	};

	static std::mutex s_serverMtx;
	static std::unique_ptr<Server> s_server;

	static bool ReadExact(tcp::socket& socket, void* buf, size_t len)
	{
		boost::system::error_code ec;
		boost::asio::read(socket, boost::asio::buffer(buf, len), ec);
		return !ec;
	}

	static void RemoveFromPortal(Connection& conn, uint8 slot)
	{
		g_skyportal.RemoveSkylander(slot);
		std::lock_guard lock(conn.mtx);
		conn.ownedSlots.erase(slot);
	}

	static void ReaderLoop(Server& server, std::shared_ptr<Connection> conn)
	{
		auto& socket = conn->socket;
		bool authed = false;

		while (true)
		{
			uint8 type;
			if (!ReadExact(socket, &type, 1))
				break;

			if (!authed && type != 'H')
				break;

			if (type == 'H')
			{
				uint8 len;
				if (!ReadExact(socket, &len, 1))
					break;
				std::string pw(len, '\0');
				if (len && !ReadExact(socket, pw.data(), len))
					break;
				authed = (pw == server.password);
				conn->Send({'h', uint8(authed ? 1 : 0)});
				if (!authed)
				{
					// give the writer a moment to deliver the refusal
					std::this_thread::sleep_for(std::chrono::milliseconds(200));
					break;
				}
			}
			else if (type == 'L')
			{
				uint8 tag;
				FigureData data;
				if (!ReadExact(socket, &tag, 1) || !ReadExact(socket, data.data(), data.size()))
					break;

				std::weak_ptr<Connection> weak = conn;
				// slot isn't known until LoadSkylander returns, so the hook looks it up
				auto slotHolder = std::make_shared<std::atomic<uint8>>(0xFF);
				uint8 slot = g_skyportal.LoadSkylander(
					data.data(), nullptr,
					[weak, slotHolder](const FigureData& d) {
						uint8 s = slotHolder->load();
						if (s == 0xFF)
							return;
						if (auto c = weak.lock())
							c->QueueSave(s, d);
					});
				if (slot != 0xFF)
				{
					slotHolder->store(slot);
					std::lock_guard lock(conn->mtx);
					conn->ownedSlots.insert(slot);
				}
				conn->Send({'l', tag, slot});
			}
			else if (type == 'U')
			{
				uint8 slot;
				if (!ReadExact(socket, &slot, 1))
					break;
				bool owned;
				{
					std::lock_guard lock(conn->mtx);
					owned = conn->ownedSlots.count(slot) != 0;
				}
				// only allow removing figures this player placed
				if (owned)
					RemoveFromPortal(*conn, slot);
				conn->Send({'u', slot, uint8(owned ? 1 : 0)});
			}
			else if (type == 'P')
			{
				conn->Send({'p'});
			}
			else
			{
				break; // unknown message, protocol mismatch
			}
		}

		// disconnected: take this player's figures off the portal
		std::set<uint8> slots;
		{
			std::lock_guard lock(conn->mtx);
			slots = conn->ownedSlots;
		}
		for (uint8 slot : slots)
			RemoveFromPortal(*conn, slot);
		conn->Close();
	}

	static void AcceptLoop(Server* server)
	{
		while (!server->stopping)
		{
			auto conn = std::make_shared<Connection>(server->ctx);
			boost::system::error_code ec;
			// non-blocking accept + polling, so Stop() can always end this loop
			server->acceptor->accept(conn->socket, ec);
			if (ec)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}
			conn->socket.non_blocking(false, ec);
			conn->socket.set_option(tcp::no_delay(true), ec);
			auto ep = conn->socket.remote_endpoint(ec);
			conn->remoteName = ec ? "unknown" : ep.address().to_string();

			std::lock_guard lock(server->connMtx);
			// drop finished connections
			for (auto it = server->connections.begin(); it != server->connections.end();)
			{
				bool done;
				{
					std::lock_guard l((*it)->mtx);
					done = (*it)->closed;
				}
				if (done)
				{
					if ((*it)->reader.joinable())
						(*it)->reader.join();
					if ((*it)->writer.joinable())
						(*it)->writer.join();
					it = server->connections.erase(it);
				}
				else
					++it;
			}
			conn->writer = std::thread([conn] { conn->WriterLoop(); });
			conn->reader = std::thread([server, conn] { ReaderLoop(*server, conn); });
			server->connections.push_back(conn);
		}
	}

	bool Start(uint16_t port, const std::string& password, std::string& error)
	{
		std::lock_guard lock(s_serverMtx);
		if (s_server)
		{
			error = "Already running";
			return false;
		}
		if (password.empty() || password.size() > 255)
		{
			error = "Set a password (1-255 characters)";
			return false;
		}
		auto server = std::make_unique<Server>();
		server->password = password;
		server->port = port;
		try
		{
			server->acceptor = std::make_unique<tcp::acceptor>(server->ctx);
			tcp::endpoint ep(boost::asio::ip::address_v6::any(), port);
			boost::system::error_code ec;
			server->acceptor->open(ep.protocol(), ec);
			if (!ec)
			{
				// accept IPv4 as well as IPv6
				server->acceptor->set_option(boost::asio::ip::v6_only(false), ec);
			}
			else
			{
				ep = tcp::endpoint(tcp::v4(), port);
				server->acceptor->open(ep.protocol());
			}
			server->acceptor->set_option(tcp::acceptor::reuse_address(true));
			server->acceptor->bind(ep);
			server->acceptor->listen();
			server->acceptor->non_blocking(true);
		}
		catch (const std::exception& e)
		{
			error = e.what();
			return false;
		}
		server->acceptThread = std::thread(AcceptLoop, server.get());
		s_server = std::move(server);
		return true;
	}

	void Stop()
	{
		std::unique_ptr<Server> server;
		{
			std::lock_guard lock(s_serverMtx);
			server = std::move(s_server);
		}
		if (!server)
			return;

		server->stopping = true;
		if (server->acceptThread.joinable())
			server->acceptThread.join();
		boost::system::error_code ec;
		server->acceptor->close(ec);

		std::vector<std::shared_ptr<Connection>> conns;
		{
			std::lock_guard lock(server->connMtx);
			conns = std::move(server->connections);
		}
		// take remote figures off first and deliver their final save to the owner
		for (auto& c : conns)
		{
			std::set<uint8> slots;
			{
				std::lock_guard lock(c->mtx);
				slots = c->ownedSlots;
			}
			for (uint8 slot : slots)
				RemoveFromPortal(*c, slot);
			c->Flush(std::chrono::seconds(2));
			c->Close();
		}
		for (auto& c : conns)
		{
			if (c->reader.joinable())
				c->reader.join(); // reader removes the figures on exit
			if (c->writer.joinable())
				c->writer.join();
		}
	}

	bool IsRunning()
	{
		std::lock_guard lock(s_serverMtx);
		return s_server != nullptr;
	}

	std::string GetStatus()
	{
		std::lock_guard lock(s_serverMtx);
		if (!s_server)
			return "Remote portal: off";
		size_t players = 0, figures = 0;
		{
			std::lock_guard l(s_server->connMtx);
			for (auto& c : s_server->connections)
			{
				std::lock_guard cl(c->mtx);
				if (c->closed)
					continue;
				players++;
				figures += c->ownedSlots.size();
			}
		}
		return "Remote portal: listening on port " + std::to_string(s_server->port) + ", " +
			   std::to_string(players) + " connected, " + std::to_string(figures) + " remote figure(s)";
	}
} // namespace nsyshid::SkylanderRemote
