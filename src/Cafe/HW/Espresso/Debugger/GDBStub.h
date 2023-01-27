#pragma once

#include <numeric>
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"
#include "Common/precompiled.h"

class GDBServer
{
public:
	GDBServer(uint16 port);
	~GDBServer();

	bool Initialize();
	bool IsConnected() { return m_client_connected; }

    void HandleEntryStop(uint32 entryAddress);
    void HandleTrapInstruction(PPCInterpreter_t* hCPU);
private:
	static constexpr int s_maxGDBClients = 1;
	static constexpr std::string_view s_supportedFeatures = "PacketSize=4096;qXfer:features:read+;qXfer:threads:read+;qXfer:libraries:read+;swbreak+;hwbreak+;vContSupported+";
	static constexpr size_t s_maxPacketSize = 1024*4;
	const uint16 m_port;

	enum class CmdType : char {
		INVALID = '\0',
		// Extended commands
		QUERY_GET = 'q',
		QUERY_SET = 'Q',
		VCONT = 'v',
		// Normal commands
		CONTINUE = 'c',
		IS_THREAD_RUNNING = 'T',
		SET_ACTIVE_THREAD = 'H',
		ACTIVE_THREAD_STATUS = '?',
		ACTIVE_THREAD_STEP = 's',
		REGISTER_READ = 'p',
		REGISTER_SET = 'P',
		REGISTERS_READ = 'g',
		REGISTERS_WRITE = 'G',
		MEMORY_READ = 'm',
		MEMORY_WRITE = 'M',
		BREAKPOINT_SET = 'Z',
		BREAKPOINT_REMOVE = 'z',
	};

    enum RegisterID {
        PC = 64,
        MSR = 65,
        CR = 66,
        LR = 67,
        CTR = 68,
        XER = 69,
    };

	static constexpr std::string_view RESPONSE_EMPTY = "";
	static constexpr std::string_view RESPONSE_ACK = "+";
	static constexpr std::string_view RESPONSE_NACK = "+";
	static constexpr std::string_view RESPONSE_OK = "OK";
	static constexpr std::string_view RESPONSE_ERROR = "E01";

public:
	class CommandContext {
    public:
		CommandContext(const GDBServer* server, const std::string& command) : m_server(server), m_command(command) {
			std::smatch matches;
			std::regex_match(command, matches, m_regex);
			for (size_t i = 1; i < matches.size(); i++) {
				auto matchStr = matches[i].str();
                if (!matchStr.empty())
                    m_args.emplace_back(std::move(matchStr));
			}
            send(m_server->m_client_socket, RESPONSE_ACK.data(), (int)RESPONSE_ACK.size(), 0);
		};
		~CommandContext() {
			//cemuLog_logDebug(LogType::Force, "[GDBStub] Received: {}", m_command);
			//cemuLog_logDebug(LogType::Force, "[GDBStub] Responded: {}", m_response);
            auto response_data = EscapeMessage(m_response);
            auto response_full = fmt::format("${}#{:02x}", response_data, CalculateChecksum(response_data));
            send(m_server->m_client_socket, response_full.c_str(), (int) response_full.size(), 0);
		}
        CommandContext(const CommandContext&) = delete;

        [[nodiscard]] const std::string& GetCommand() const { return m_command; };
		[[nodiscard]] const std::vector<std::string>& GetArgs() const { return m_args; };
		[[nodiscard]] bool IsValid() const { return !m_args.empty(); };
        [[nodiscard]] GDBServer::CmdType GetType() const { return static_cast<GDBServer::CmdType>(m_command[0]); };

        // Respond Utils
        static uint8 CalculateChecksum(std::string_view message_data) {
            return std::accumulate(message_data.begin(), message_data.end(), (uint8)0, std::plus<>());
        }
        static std::string EscapeXMLString(std::string_view xml_data) {
            std::string escaped;
            escaped.reserve(xml_data.size());
            for (char c : xml_data) {
                switch (c) {
                    case '<': escaped += "&lt;"; break;
                    case '>': escaped += "&gt;"; break;
                    case '&': escaped += "&amp;"; break;
                    case '"': escaped += "&quot;"; break;
                    case '\'': escaped += "&apos;"; break;
                    default: escaped += c; break;
                }
            }
            return escaped;
        }
        static std::string EscapeMessage(std::string_view message) {
            std::string escaped;
            escaped.reserve(message.size());
            for (char c : message) {
                if (c == '#' || c == '$' || c == '}' || c == '*') {
                    escaped.push_back('}');
                    escaped.push_back((char)(c ^ 0x20));
                }
                else escaped.push_back(c);
            }
            return escaped;
        }

		void QueueResponse(std::string_view data) {
			m_response += data;
		}
	private:
		const std::regex m_regex{
			R"((?:)"
				R"((\?))"
                R"(|(vCont\?))"
                R"(|(vCont;)([a-zA-Z0-9-+=,\+:;]+))"
				R"(|(qAttached))"
				R"(|(qSupported):([a-zA-Z0-9-+=,\+;]+))"
                R"(|(qTStatus))"
                R"(|(qC))"
                R"(|(qXfer):((?:features)|(?:threads)|(?:libraries)):read:([\w\.]*):([0-9a-zA-Z]+),([0-9a-zA-Z]+))"
                R"(|(qfThreadInfo))"
                R"(|(qsThreadInfo))"
				R"(|(D))" // Detach
                R"(|(H)(c|g)((?:-1)|(?:[0-9A-Fa-f]+)))" // Set active thread for other operations (not c)
				R"(|(c)([0-9A-Fa-f]+)?)" // (Legacy, supported by vCont) Continue all for active thread
				R"(|([Zz])([0-4]),([0-9A-Fa-f]+),([0-9]))" // Insert/delete breakpoints
				R"(|(g))" // Read registers for active thread
				R"(|(G)([0-9A-Fa-f]+))" // Write registers for active thread
                R"(|(p)([0-9A-Fa-f]+))" // Read register for active thread
                R"(|(P)([0-9A-Fa-f]+)=([0-9A-Fa-f]+))" // Write register for active thread
				R"(|(m)([0-9A-Fa-f]+),([0-9A-Fa-f]+))" // Read memory
				R"(|(M)([0-9A-Fa-f]+),([0-9A-Fa-f]+):([0-9A-Fa-f]+))" // Write memory
                //R"(|(X)([0-9A-Fa-f]+),([0-9A-Fa-f]+):([0-9A-Fa-f]+))" // Write memory
			R"())"
		};
		const GDBServer* m_server;
		const std::string m_command;
		std::vector<std::string> m_args;
		std::string m_response;
	};
private:

	void ThreadFunc(const std::stop_token& stop_token);
	void HandleCommand(const std::string& command_str);
	void HandleQuery(std::unique_ptr<CommandContext>& context) const;
	void HandleVCont(std::unique_ptr<CommandContext>& context);

    // Commands
    sint64 m_activeThreadSelector = 0;
    sint64 m_activeThreadContinueSelector = 0;
    void CMDSetActiveThread(std::unique_ptr<CommandContext>& context); // H
    void CMDGetThreadStatus(std::unique_ptr<CommandContext>& context); // ?
    void CMDContinue(std::unique_ptr<CommandContext>& context) const;
    void CMDNotFound(std::unique_ptr<CommandContext>& context);

    void CMDReadRegister(std::unique_ptr<CommandContext>& context) const;
    void CMDWriteRegister(std::unique_ptr<CommandContext>& context) const;
    void CMDReadRegisters(std::unique_ptr<CommandContext>& context) const;
    void CMDWriteRegisters(std::unique_ptr<CommandContext>& context) const;
    void CMDReadMemory(std::unique_ptr<CommandContext>& context);
    void CMDWriteMemory(std::unique_ptr<CommandContext>& context);
    void CMDInsertBreakpoint(std::unique_ptr<CommandContext>& context);
    void CMDDeleteBreakpoint(std::unique_ptr<CommandContext>& context);

	std::jthread m_thread;
    std::atomic_bool m_resume_startup = false;
    MPTR m_entry_point{};
    std::unique_ptr<CommandContext> m_resumed_context;

	std::atomic_bool m_client_connected;
	SOCKET m_server_socket = INVALID_SOCKET;
	sockaddr_in m_server_addr{};
	SOCKET m_client_socket = INVALID_SOCKET;
	sockaddr_in m_client_addr{};
};


extern std::unique_ptr<GDBServer> g_gdbstub;
