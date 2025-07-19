#include "DiscordRPCLite.h"

#include <string_view>
#include <optional>
#include <chrono>

#if defined(__cpp_lib_format)
#include <format>
namespace shim
{
	using std::format;
}
#else
#include <fmt/format.h>
namespace shim
{
	using fmt::format;
}
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

enum RPCOpcode : uint32_t
{
	OpcodeHandshake = 0,
	OpcodeFrame = 1,
	OpcodeClose = 2,
	OpcodePing = 3,
	OpcodePong = 4,
};

#ifdef _WIN32
class NamedPipeImpl
{
  public:
	NamedPipeImpl()
	{
		m_readEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	}

	~NamedPipeImpl()
	{
		CloseHandle(m_readEvent);
		ClosePipe();
	}

	bool OpenPipe()
	{
		std::string fullPipeName = R"(\\.\pipe\discord-ipc-0)";
		m_handle = CreateFileA(fullPipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
		if (m_handle == INVALID_HANDLE_VALUE)
			return false;
		return true;
	}

	void ClosePipe()
	{
		if (m_handle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(m_handle);
			m_handle = INVALID_HANDLE_VALUE;
		}
	}

	bool IsOpen() const
	{
		return m_handle != INVALID_HANDLE_VALUE;
	}

	int Read(uint8_t* buffer, size_t maxSize, int timeoutMs)
	{
		if (m_handle == INVALID_HANDLE_VALUE || m_readEvent == nullptr)
			return -1;
		OVERLAPPED overlapped = {0};
		overlapped.hEvent = m_readEvent;
		ResetEvent(m_readEvent);
		DWORD bytesRead = 0;
		BOOL success = ReadFile(m_handle, buffer, static_cast<DWORD>(maxSize), nullptr, &overlapped);
		if (!success && GetLastError() == ERROR_IO_PENDING)
		{
			DWORD waitResult = WaitForSingleObject(m_readEvent, timeoutMs);
			if (waitResult == WAIT_TIMEOUT)
			{
				CancelIo(m_handle);
				return 0;
			}
			else if (waitResult != WAIT_OBJECT_0) // error
			{
				ClosePipe();
				return -1;
			}
			if (!GetOverlappedResult(m_handle, &overlapped, &bytesRead, FALSE))
			{
				ClosePipe();
				return -1;
			}
		}
		else if (!success)
		{
			ClosePipe();
			return -1;
		}
		else
		{
			GetOverlappedResult(m_handle, &overlapped, &bytesRead, FALSE); // immediate completion
		}
		return static_cast<int>(bytesRead);
	}

	int Write(const uint8_t* buffer, size_t size)
	{
		if (m_handle == INVALID_HANDLE_VALUE)
		{
			return -1;
		}
		DWORD bytesWritten = 0;
		if (!WriteFile(m_handle, buffer, static_cast<DWORD>(size), &bytesWritten, nullptr))
		{
			ClosePipe();
			return -1;
		}
		return static_cast<int>(bytesWritten);
	}

  private:
	HANDLE m_handle = INVALID_HANDLE_VALUE;
	HANDLE m_readEvent;
};

#else

class NamedPipeImpl // posix
{
  public:
	NamedPipeImpl() {}
	~NamedPipeImpl()
	{
		ClosePipe();
	}

	bool OpenPipe()
	{
		const char* tempPath = getenv("XDG_RUNTIME_DIR");
		if (!tempPath)
			tempPath = getenv("TMPDIR");
		if (!tempPath)
			tempPath = getenv("TMP");
		if (!tempPath)
			tempPath = getenv("TEMP");
		if (!tempPath)
			tempPath = "/tmp";
		sockaddr_un pipeAddr{};
		pipeAddr.sun_family = AF_UNIX;
		m_fd = socket(AF_UNIX, SOCK_STREAM, 0);
		if (m_fd == -1)
			return false;
		fcntl(m_fd, F_SETFL, O_NONBLOCK);
#ifdef SO_NOSIGPIPE
		int optval = 1;
		setsockopt(m_fd, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval));
#endif
		for (int pipeIndex = 0; pipeIndex < 10; pipeIndex++)
		{
			snprintf(pipeAddr.sun_path, sizeof(pipeAddr.sun_path), "%s/discord-ipc-%d", tempPath, pipeIndex);
			int r = connect(m_fd, (const sockaddr*)&pipeAddr, sizeof(pipeAddr));
			if (r == 0)
				return true;
		}
		ClosePipe();
		return false;
	}

	void ClosePipe()
	{
		if (m_fd != -1)
		{
			close(m_fd);
			m_fd = -1;
		}
	}

	bool IsOpen() const
	{
		return m_fd != -1;
	}

	int Read(uint8_t* buffer, size_t maxSize, int timeoutMs)
	{
		if (m_fd == -1)
			return -1;
		if (!buffer || maxSize == 0)
			return -1;
		struct pollfd pfd;
		pfd.fd = m_fd;
		pfd.events = POLLIN;
		pfd.revents = 0;
		int ret = poll(&pfd, 1, timeoutMs);
		if (ret < 0)
		{
			ClosePipe();
			return -1;
		}
		else if (ret == 0)
		{
			return 0;
		}
		else if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
		{
			ClosePipe();
			return -1;
		}
#ifdef MSG_NOSIGNAL
		int MsgFlags = MSG_NOSIGNAL;
#else
		int MsgFlags = 0;
#endif
		int res = (int)recv(m_fd, buffer, maxSize, MsgFlags);
		if (res <= 0)
		{
			ClosePipe();
			return -1;
		}
		return res;
	}

	int Write(const uint8_t* buffer, size_t size)
	{
		if (m_fd == -1 || !buffer || size == 0)
			return -1;
#ifdef MSG_NOSIGNAL
		int MsgFlags = MSG_NOSIGNAL;
#else
		int MsgFlags = 0;
#endif
		int sentBytes = send(m_fd, buffer, size, MsgFlags);
		if (sentBytes == -1)
		{
			ClosePipe();
			return -1;
		}
		return sentBytes;
	}

  private:
	int m_fd = -1;
};

#endif

#ifdef _WIN32
int GetProcessId()
{
	return (int)GetCurrentProcessId();
}
#else
int GetProcessId()
{
	return getpid();
}
#endif

class NamedPipe : public NamedPipeImpl
{
  public:
	NamedPipe()
		: NamedPipeImpl() {};

	void SendJsonMsg(uint32_t opcode, std::string_view jsonText)
	{
		std::vector<uint8_t> buffer;
		buffer.resize(8 + jsonText.size());
		buffer[0] = (opcode >> 0) & 0xFF;
		buffer[1] = (opcode >> 8) & 0xFF;
		buffer[2] = (opcode >> 16) & 0xFF;
		buffer[3] = (opcode >> 24) & 0xFF;
		uint32_t length = static_cast<uint32_t>(jsonText.size());
		buffer[4] = (length >> 0) & 0xFF;
		buffer[5] = (length >> 8) & 0xFF;
		buffer[6] = (length >> 16) & 0xFF;
		buffer[7] = (length >> 24) & 0xFF;
		std::copy(jsonText.begin(), jsonText.end(), buffer.begin() + 8);
		Write(buffer.data(), buffer.size());
	}

	void SendPresenceUpdate(const DiscordLiteRichPresence& presence)
	{
		// note: According to older documentation SET_ACTIVITY has a rate limit of 15 seconds
		std::string jsonPayload;
		jsonPayload.append("{");
		jsonPayload.append(R"("cmd": "SET_ACTIVITY")");
		jsonPayload.append(shim::format(R"(,"nonce": "{}")", GetNonceStr()));
		jsonPayload.append(R"(,"args": {)");
		// args
		std::string pidStr = shim::format("{}", GetProcessId());
		jsonPayload.append(R"("pid": )").append(pidStr).append(R"(,)");
		// args->activity
		if (!presence.state.empty() || !presence.details.empty())
		{
			jsonPayload.append(R"("activity": {)");
			if (!presence.state.empty())
				jsonPayload.append(shim::format(R"("state": "{}",)", presence.state));
			if (!presence.details.empty())
				jsonPayload.append(shim::format(R"("details": "{}",)", presence.details));
			if (presence.startTimestamp || presence.endTimestamp)
			{
				jsonPayload.append(R"("timestamps": {)");
				if (presence.startTimestamp)
					jsonPayload.append(shim::format(R"("start": {},)", presence.startTimestamp));
				if (presence.endTimestamp)
					jsonPayload.append(shim::format(R"("end": {},)", presence.endTimestamp));
				if (jsonPayload.back() == ',')
					jsonPayload.pop_back();
				jsonPayload.append("},");
			}
			if (!presence.largeImageKey.empty() && !presence.largeImageText.empty() && !presence.smallImageKey.empty() && !presence.smallImageText.empty())
			{
				jsonPayload.append(R"("assets": {)");
				jsonPayload.append(shim::format(R"("large_image": "{}",)", presence.largeImageKey));
				jsonPayload.append(shim::format(R"("large_text": "{}",)", presence.largeImageText));
				jsonPayload.append(shim::format(R"("small_image": "{}",)", presence.smallImageKey));
				jsonPayload.append(shim::format(R"("small_text": "{}",)", presence.smallImageText));
				jsonPayload.append("},");
			}
			// party
			if (!presence.partyId.empty() || (presence.partySize > 0 && presence.partyMax > 0))
			{
				jsonPayload.append(R"("party": {)");
				if (!presence.partyId.empty())
					jsonPayload.append(shim::format(R"("id": "{}",)", presence.partyId));
				if (presence.partySize > 0 && presence.partyMax > 0)
					jsonPayload.append(shim::format(R"("size": [{}, {}],)", presence.partySize, presence.partyMax));
				jsonPayload.append(R"("privacy": )").append(std::to_string(presence.partyPrivacy)).append(",");
				jsonPayload.append("},");
			}
			// match secret
			if (!presence.matchSecret.empty() || !presence.joinSecret.empty() || !presence.spectateSecret.empty())
			{
				jsonPayload.append(R"("secrets": {)");
				if (!presence.matchSecret.empty())
					jsonPayload.append(shim::format(R"("match": "{}",)", presence.matchSecret));
				if (!presence.joinSecret.empty())
					jsonPayload.append(shim::format(R"("join": "{}",)", presence.joinSecret));
				if (!presence.spectateSecret.empty())
					jsonPayload.append(shim::format(R"("spectate": "{}",)", presence.spectateSecret));
				if (jsonPayload.back() == ',')
					jsonPayload.pop_back();
				jsonPayload.append("},");
			}
			if (jsonPayload.back() == ',')
				jsonPayload.pop_back();
			// end of args->activity
			jsonPayload.append("}");
		}
		if (jsonPayload.back() == ',')
			jsonPayload.pop_back();
		jsonPayload.append("}}");
		SendJsonMsg(OpcodeFrame, jsonPayload);
	}

  private:
	std::string GetNonceStr()
	{
		std::string nonceStr = shim::format("{:016x}", nonce);
		nonce++;
		return nonceStr;
	}
	int64_t nonce = 1;
};

static std::optional<std::string> ParseJSONField(std::string_view json, std::string_view fieldPath)
{
	size_t pos = 0;
	auto skipWhitespace = [&]() {
		while (pos < json.size() && isspace(json[pos]))
			++pos;
	};
	auto nextToken = [&]() -> std::string_view {
		skipWhitespace();
		if (pos >= json.size())
			return "";
		char c = json[pos];
		if (c == '{' || c == '}' || c == '[' || c == ']' || c == ':' || c == ',')
		{
			return json.substr(pos++, 1);
		}
		if (c == '"')
		{
			size_t start = ++pos;
			while (pos < json.size() && json[pos] != '"')
				++pos;
			auto tok = json.substr(start, pos > start ? pos - start : 0);
			if (pos < json.size())
				++pos;
			return tok;
		}
		size_t start = pos;
		while (pos < json.size() && !isspace(json[pos]) && json[pos] != ',' && json[pos] != '}' && json[pos] != ']')
			++pos;
		return json.substr(start, pos - start);
	};
	auto skipValue = [&]() {
		skipWhitespace();
		if (pos >= json.size())
			return;
		char c = json[pos];
		if (c == '{')
		{
			int depth = 1;
			++pos;
			while (pos < json.size() && depth > 0)
			{
				auto tok = nextToken();
				if (tok == "{")
					++depth;
				else if (tok == "}")
					--depth;
			}
		}
		else if (c == '[')
		{
			int depth = 1;
			++pos;
			while (pos < json.size() && depth > 0)
			{
				auto tok = nextToken();
				if (tok == "[")
					++depth;
				else if (tok == "]")
					--depth;
			}
		}
		else
		{
			nextToken();
		}
	};
	auto findField = [&](std::string_view key) {
		while (true)
		{
			auto tok = nextToken();
			if (tok == "}")
				return false;
			if (tok == "")
				return false;
			if (tok == key)
			{
				if (nextToken() == ":")
					return true;
				return false;
			}
			if (nextToken() != ":")
				return false;
			skipValue();
			auto sep = nextToken();
			if (sep == "}")
				return false;
			if (sep != ",")
				return false;
		}
	};
	// initial "{"
	skipWhitespace();
	if (nextToken() != "{")
		return std::nullopt;
	// traverse the path
	size_t off = 0;
	while (off <= fieldPath.size())
	{
		size_t sep = fieldPath.find('/', off);
		if (sep == std::string_view::npos)
			sep = fieldPath.size();
		std::string_view seg = fieldPath.substr(off, sep - off);
		if (!findField(seg))
			return std::nullopt;
		auto val = nextToken(); // after ":"
		if (sep == fieldPath.size())
			return std::string(val);
		if (val == "{")
		{
			// traverse object
		}
		else if (val == "[")
		{
			// array
			return std::nullopt;
		}
		else
		{
			return std::nullopt; // not object
		}
		off = sep + 1;
	}
	return std::nullopt;
}

DiscordRPCLite::DiscordRPCLite(const char* applicationId)
{
	m_workerThread = std::thread(&DiscordRPCLite::WorkerThread, this, std::string(applicationId));
}

DiscordRPCLite::~DiscordRPCLite()
{
	m_shutdownSignal.store(true);
	if (m_workerThread.joinable())
		m_workerThread.join();
}

static bool CompareStringI(std::string_view a, std::string_view b)
{
	auto toLower = [](char c) -> char {
		if (c >= 'A' && c <= 'Z')
			return c + ('a' - 'A');
		return c;
	};
	if (a.size() != b.size())
		return false;
	for (size_t i = 0; i < a.size(); i++)
	{
		if (toLower(a[i]) != toLower(b[i]))
			return false;
	}
	return true;
}

static int64_t GetMs()
{
	auto now = std::chrono::system_clock::now();
	auto duration = now.time_since_epoch();
	return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

void DiscordRPCLite::WorkerThread(std::string applicationId)
{
	enum class ConnectionState
	{
		Disconnected,
		ConnectingBeforeHandshake,
		ConnectingAfterHandshake,
		Connected
	};
	ConnectionState connectionState = ConnectionState::Disconnected;

	std::vector<uint8_t> recvBuffer;
	uint8_t recvMsgHeader[8] = {0};
	size_t recvSize = 0; // if < 8 then we are still receiving the header. If >= 8 then we are receiving the body
	NamedPipe pipe;
	auto SetDisconnected = [&]() {
		pipe.ClosePipe();
		connectionState = ConnectionState::Disconnected;
		recvSize = 0;
		m_richPresenceDirty = true; // set so we always send current rich presence after (re)connecting
	};
	SetDisconnected(); // set initial state
	int64_t lastConnectionAttempt = 0;
	int32_t numFailedConnectionAttempts = 0; // increases every time we try to connect but fail, resets to 0 when we successfully connect and receive a READY event
	int64_t lastRichPresenceUpdate = 0;
	while (!m_shutdownSignal)
	{
		if (connectionState == ConnectionState::Disconnected)
		{
			// wait at least one second before trying to reconnect
			int64_t now = GetMs();
			int64_t delayTime = 1000 + (numFailedConnectionAttempts * 500); // increase delay by 500ms for each failed connection attempt
			if (delayTime >= 90000)
				delayTime = 90000; // max delay of 90 seconds
			if ((now - lastConnectionAttempt) < delayTime)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
				continue;
			}
			lastConnectionAttempt = now;
			numFailedConnectionAttempts++;
			if (!pipe.OpenPipe())
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}
			connectionState = ConnectionState::ConnectingBeforeHandshake;
			continue;
		}
		if (connectionState == ConnectionState::ConnectingBeforeHandshake)
		{
			std::string handshakeJson = shim::format(R"({{"v":1,"client_id":"{}"}})", applicationId);
			pipe.SendJsonMsg(OpcodeHandshake, handshakeJson);
			connectionState = ConnectionState::ConnectingAfterHandshake;
		}
		// check if we should send a presence update (rate limited to 1 per 15 seconds)
		if (connectionState == ConnectionState::Connected && m_richPresenceDirty)
		{
			int64_t now = GetMs();
			if ((now - lastRichPresenceUpdate) >= 15000)
			{
				DiscordLiteRichPresence presence = {};
				{
					std::lock_guard lock(m_presenceMutex);
					presence = m_presence;
					m_richPresenceDirty = false;
				}
				pipe.SendPresenceUpdate(presence);
				lastRichPresenceUpdate = now;
			}
		}
		// continue receiving messages
		if (recvSize < 8)
		{
			// receive header
			int readSize = pipe.Read(recvMsgHeader + recvSize, 8 - recvSize, 200);
			if (readSize < 0)
			{
				SetDisconnected();
				continue;
			}
			recvSize += readSize;
		}
		else
		{
			// receive body
			uint32_t opcode = (recvMsgHeader[0] << 0) | (recvMsgHeader[1] << 8) | (recvMsgHeader[2] << 16) | (recvMsgHeader[3] << 24);
			uint32_t bodyLength = (recvMsgHeader[4] << 0) | (recvMsgHeader[5] << 8) | (recvMsgHeader[6] << 16) | (recvMsgHeader[7] << 24);
			if (bodyLength >= 8 * 1024 * 1024)
			{
				SetDisconnected();
				continue;
			}
			if (recvBuffer.size() < bodyLength)
				recvBuffer.resize(bodyLength);
			int readSize = pipe.Read(recvBuffer.data() + (recvSize - 8), bodyLength - (recvSize - 8), 1000);
			if (readSize < 0)
			{
				SetDisconnected();
				continue;
			}
			recvSize += readSize;
			if (recvSize == bodyLength + 8)
			{
				std::string_view jsonText((const char*)recvBuffer.data(), bodyLength);
				recvSize = 0;
				// full message received
				if (opcode == OpcodeFrame)
				{
					std::optional<std::string> cmd = ParseJSONField(jsonText, "cmd");
					std::optional<std::string> evt = ParseJSONField(jsonText, "evt");
					if (CompareStringI(*cmd, "DISPATCH"))
					{
						if (evt && CompareStringI(*evt, "READY"))
						{
							if (connectionState == ConnectionState::ConnectingAfterHandshake)
							{
								connectionState = ConnectionState::Connected;
								numFailedConnectionAttempts = 0;
							}
						}
					}
				}
				else if (opcode == OpcodePing)
				{
					pipe.SendJsonMsg(OpcodePong, jsonText);
				}
				else if (opcode == OpcodeClose)
				{
					SetDisconnected();
				}
				if (recvBuffer.size() > 2048)
				{
					recvBuffer.resize(2048);
					recvBuffer.shrink_to_fit();
				}
			}
		}
	}
	// if we are still connected then explicitly clear presence just in case
	if (pipe.IsOpen())
	{
		pipe.SendPresenceUpdate({});
		std::this_thread::sleep_for(std::chrono::milliseconds(75));
	}
}