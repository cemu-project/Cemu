#pragma once

#include "Common/precompiled.h"
#include "Common/socket.h"
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"

#include <numeric>

class GDBServer {
public:
	explicit GDBServer(uint16 port);
	~GDBServer();

	bool Initialize();
	bool IsConnected()
	{
		return m_client_connected;
	}

	void HandleEntryStop(uint32 entryAddress);
	void HandleTrapInstruction(PPCInterpreter_t* hCPU);
	void HandleAccessException(uint64 dr6);

	enum class CMDType : char
	{
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

	class CommandContext {
	public:
		CommandContext(const GDBServer* server, const std::string& command)
			: m_server(server), m_command(command)
		{
			std::smatch matches;
			std::regex_match(command, matches, m_regex);
			for (size_t i = 1; i < matches.size(); i++)
			{
				auto matchStr = matches[i].str();
				if (!matchStr.empty())
					m_args.emplace_back(std::move(matchStr));
			}
			// send acknowledgement ahead of response
			send(m_server->m_client_socket, RESPONSE_ACK.data(), (int)RESPONSE_ACK.size(), 0);
		};
		~CommandContext()
		{
			// cemuLog_logDebug(LogType::Force, "[GDBStub] Received: {}", m_command);
			// cemuLog_logDebug(LogType::Force, "[GDBStub] Responded: +{}", m_response);
			auto response_data = EscapeMessage(m_response);
			auto response_full = fmt::format("${}#{:02x}", response_data, CalculateChecksum(response_data));
			send(m_server->m_client_socket, response_full.c_str(), (int)response_full.size(), 0);
		}
		CommandContext(const CommandContext&) = delete;

		[[nodiscard]] const std::string& GetCommand() const
		{
			return m_command;
		};
		[[nodiscard]] const std::vector<std::string>& GetArgs() const
		{
			return m_args;
		};
		[[nodiscard]] bool IsValid() const
		{
			return !m_args.empty();
		};
		[[nodiscard]] CMDType GetType() const
		{
			return static_cast<CMDType>(m_command[0]);
		};

		// Respond Utils
		static uint8 CalculateChecksum(std::string_view message_data)
		{
			return std::accumulate(message_data.begin(), message_data.end(), (uint8)0, std::plus<>());
		}
		static std::string EscapeXMLString(std::string_view xml_data)
		{
			std::string escaped;
			escaped.reserve(xml_data.size());
			for (char c : xml_data)
			{
				switch (c)
				{
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
		static std::string EscapeMessage(std::string_view message)
		{
			std::string escaped;
			escaped.reserve(message.size());
			for (char c : message)
			{
				if (c == '#' || c == '$' || c == '}' || c == '*')
				{
					escaped.push_back('}');
					escaped.push_back((char)(c ^ 0x20));
				}
				else
					escaped.push_back(c);
			}
			return escaped;
		}
		void QueueResponse(std::string_view data)
		{
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
			R"(|(T)((?:-1)|(?:[0-9A-Fa-f]+)))"
			R"(|(D))"											  // Detach
			R"(|(H)(c|g)((?:-1)|(?:[0-9A-Fa-f]+)))"				  // Set active thread for other operations (not c)
			R"(|(c)([0-9A-Fa-f]+)?)"							  // (Legacy, supported by vCont) Continue all for active thread
			R"(|([Zz])([0-4]),([0-9A-Fa-f]+),([0-9]))"			  // Insert/delete breakpoints
			R"(|(g))"											  // Read registers for active thread
			R"(|(G)([0-9A-Fa-f]+))"								  // Write registers for active thread
			R"(|(p)([0-9A-Fa-f]+))"								  // Read register for active thread
			R"(|(P)([0-9A-Fa-f]+)=([0-9A-Fa-f]+))"				  // Write register for active thread
			R"(|(m)([0-9A-Fa-f]+),([0-9A-Fa-f]+))"				  // Read memory
			R"(|(M)([0-9A-Fa-f]+),([0-9A-Fa-f]+):([0-9A-Fa-f]+))" // Write memory
			// R"(|(X)([0-9A-Fa-f]+),([0-9A-Fa-f]+):([0-9A-Fa-f]+))" // Write memory
			R"())"};
		const GDBServer* m_server;
		const std::string m_command;
		std::vector<std::string> m_args;
		std::string m_response;
	};

	class ExecutionBreakpoint;
	std::map<MPTR, ExecutionBreakpoint> m_patchedInstructions;

	class AccessBreakpoint;
	std::unique_ptr<AccessBreakpoint> m_watch_point;

private:
	static constexpr int s_maxGDBClients = 1;
	static constexpr std::string_view s_supportedFeatures = "PacketSize=4096;qXfer:features:read+;qXfer:threads:read+;qXfer:libraries:read+;swbreak+;hwbreak+;vContSupported+";
	static constexpr size_t s_maxPacketSize = 1024 * 4;
	const uint16 m_port;

	enum RegisterID
	{
		R0_START = 0,
		R31_END = R0_START + 31,
		PC = 64,
		MSR = 65,
		CR = 66,
		LR = 67,
		CTR = 68,
		XER = 69,
		F0_START = 71,
		F31_END = F0_START + 31,
		FPSCR = 103
	};

	static constexpr std::string_view RESPONSE_EMPTY = "";
	static constexpr std::string_view RESPONSE_ACK = "+";
	static constexpr std::string_view RESPONSE_NACK = "-";
	static constexpr std::string_view RESPONSE_OK = "OK";
	static constexpr std::string_view RESPONSE_ERROR = "E01";

	void ThreadFunc();
	std::atomic_bool m_stopRequested;
	void HandleCommand(const std::string& command_str);
	void HandleQuery(std::unique_ptr<CommandContext>& context) const;
	void HandleVCont(std::unique_ptr<CommandContext>& context);

	// Commands
	sint64 m_activeThreadSelector = 0;
	sint64 m_activeThreadContinueSelector = 0;
	void CMDContinue(std::unique_ptr<CommandContext>& context);
	void CMDNotFound(std::unique_ptr<CommandContext>& context);
	void CMDIsThreadActive(std::unique_ptr<CommandContext>& context);
	void CMDSetActiveThread(std::unique_ptr<CommandContext>& context);
	void CMDGetThreadStatus(std::unique_ptr<CommandContext>& context);

	void CMDReadRegister(std::unique_ptr<CommandContext>& context) const;
	void CMDWriteRegister(std::unique_ptr<CommandContext>& context) const;
	void CMDReadRegisters(std::unique_ptr<CommandContext>& context) const;
	void CMDWriteRegisters(std::unique_ptr<CommandContext>& context) const;
	void CMDReadMemory(std::unique_ptr<CommandContext>& context);
	void CMDWriteMemory(std::unique_ptr<CommandContext>& context);
	void CMDInsertBreakpoint(std::unique_ptr<CommandContext>& context);
	void CMDDeleteBreakpoint(std::unique_ptr<CommandContext>& context);

	std::thread m_thread;
	std::atomic_bool m_resume_startup = false;
	MPTR m_entry_point{};
	std::unique_ptr<CommandContext> m_resumed_context;

	std::atomic_bool m_client_connected;
	SOCKET m_server_socket = INVALID_SOCKET;
	sockaddr_in m_server_addr{};
	SOCKET m_client_socket = INVALID_SOCKET;
	sockaddr_in m_client_addr{};
};

static constexpr std::string_view GDBTargetXML = R"(<?xml version="1.0"?>
<!DOCTYPE target SYSTEM "gdb-target.dtd">
<target version="1.0">
    <architecture>powerpc:common</architecture>
    <feature name="org.gnu.gdb.power.core">
        <reg name="r0" bitsize="32" type="uint32"/>
        <reg name="r1" bitsize="32" type="uint32"/>
        <reg name="r2" bitsize="32" type="uint32"/>
        <reg name="r3" bitsize="32" type="uint32"/>
        <reg name="r4" bitsize="32" type="uint32"/>
        <reg name="r5" bitsize="32" type="uint32"/>
        <reg name="r6" bitsize="32" type="uint32"/>
        <reg name="r7" bitsize="32" type="uint32"/>
        <reg name="r8" bitsize="32" type="uint32"/>
        <reg name="r9" bitsize="32" type="uint32"/>
        <reg name="r10" bitsize="32" type="uint32"/>
        <reg name="r11" bitsize="32" type="uint32"/>
        <reg name="r12" bitsize="32" type="uint32"/>
        <reg name="r13" bitsize="32" type="uint32"/>
        <reg name="r14" bitsize="32" type="uint32"/>
        <reg name="r15" bitsize="32" type="uint32"/>
        <reg name="r16" bitsize="32" type="uint32"/>
        <reg name="r17" bitsize="32" type="uint32"/>
        <reg name="r18" bitsize="32" type="uint32"/>
        <reg name="r19" bitsize="32" type="uint32"/>
        <reg name="r20" bitsize="32" type="uint32"/>
        <reg name="r21" bitsize="32" type="uint32"/>
        <reg name="r22" bitsize="32" type="uint32"/>
        <reg name="r23" bitsize="32" type="uint32"/>
        <reg name="r24" bitsize="32" type="uint32"/>
        <reg name="r25" bitsize="32" type="uint32"/>
        <reg name="r26" bitsize="32" type="uint32"/>
        <reg name="r27" bitsize="32" type="uint32"/>
        <reg name="r28" bitsize="32" type="uint32"/>
        <reg name="r29" bitsize="32" type="uint32"/>
        <reg name="r30" bitsize="32" type="uint32"/>
        <reg name="r31" bitsize="32" type="uint32"/>
        <reg name="pc" bitsize="32" type="code_ptr" regnum="64"/>
        <reg name="msr" bitsize="32" type="uint32"/>
        <reg name="cr" bitsize="32" type="uint32"/>
        <reg name="lr" bitsize="32" type="code_ptr"/>
        <reg name="ctr" bitsize="32" type="uint32"/>
        <reg name="xer" bitsize="32" type="uint32"/>
    </feature>
    <feature name="org.gnu.gdb.power.fpu">
        <reg name="f0" bitsize="64" type="ieee_double" regnum="71"/>
        <reg name="f1" bitsize="64" type="ieee_double"/>
        <reg name="f2" bitsize="64" type="ieee_double"/>
        <reg name="f3" bitsize="64" type="ieee_double"/>
        <reg name="f4" bitsize="64" type="ieee_double"/>
        <reg name="f5" bitsize="64" type="ieee_double"/>
        <reg name="f6" bitsize="64" type="ieee_double"/>
        <reg name="f7" bitsize="64" type="ieee_double"/>
        <reg name="f8" bitsize="64" type="ieee_double"/>
        <reg name="f9" bitsize="64" type="ieee_double"/>
        <reg name="f10" bitsize="64" type="ieee_double"/>
        <reg name="f11" bitsize="64" type="ieee_double"/>
        <reg name="f12" bitsize="64" type="ieee_double"/>
        <reg name="f13" bitsize="64" type="ieee_double"/>
        <reg name="f14" bitsize="64" type="ieee_double"/>
        <reg name="f15" bitsize="64" type="ieee_double"/>
        <reg name="f16" bitsize="64" type="ieee_double"/>
        <reg name="f17" bitsize="64" type="ieee_double"/>
        <reg name="f18" bitsize="64" type="ieee_double"/>
        <reg name="f19" bitsize="64" type="ieee_double"/>
        <reg name="f20" bitsize="64" type="ieee_double"/>
        <reg name="f21" bitsize="64" type="ieee_double"/>
        <reg name="f22" bitsize="64" type="ieee_double"/>
        <reg name="f23" bitsize="64" type="ieee_double"/>
        <reg name="f24" bitsize="64" type="ieee_double"/>
        <reg name="f25" bitsize="64" type="ieee_double"/>
        <reg name="f26" bitsize="64" type="ieee_double"/>
        <reg name="f27" bitsize="64" type="ieee_double"/>
        <reg name="f28" bitsize="64" type="ieee_double"/>
        <reg name="f29" bitsize="64" type="ieee_double"/>
        <reg name="f30" bitsize="64" type="ieee_double"/>
        <reg name="f31" bitsize="64" type="ieee_double"/>
        <reg name="fpscr" bitsize="32" group="float"/>
    </feature>
</target>)";

extern std::unique_ptr<GDBServer> g_gdbstub;
