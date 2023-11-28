#include "GDBStub.h"
#include "Debugger.h"
#include "Cafe/HW/Espresso/Recompiler/PPCRecompiler.h"
#include "GDBBreakpoints.h"
#include "util/helpers/helpers.h"
#include "util/ThreadPool/ThreadPool.h"
#include "Cafe/OS/RPL/rpl.h"
#include "Cafe/OS/RPL/rpl_structs.h"
#include "Cafe/OS/libs/coreinit/coreinit_Scheduler.h"
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"
#include "Cafe/HW/Espresso/Interpreter/PPCInterpreterInternal.h"
#include "Cafe/HW/Espresso/EspressoISA.h"
#include "Common/socket.h"

#define GET_THREAD_ID(threadPtr) memory_getVirtualOffsetFromPointer(threadPtr)
#define GET_THREAD_BY_ID(threadId) (OSThread_t*)memory_getPointerFromPhysicalOffset(threadId)

static std::vector<MPTR> findNextInstruction(MPTR currAddress, uint32 lr, uint32 ctr)
{
	using namespace Espresso;

	uint32 nextInstr = memory_readU32(currAddress);
	if (GetPrimaryOpcode(nextInstr) == PrimaryOpcode::B)
	{
		uint32 LI;
		bool AA, LK;
		decodeOp_B(nextInstr, LI, AA, LK);
		if (!AA)
			LI += currAddress;
		return {LI};
	}
	if (GetPrimaryOpcode(nextInstr) == PrimaryOpcode::BC)
	{
		uint32 BD, BI;
		BOField BO{};
		bool AA, LK;
		decodeOp_BC(nextInstr, BD, BO, BI, AA, LK);
		if (!LK)
			BD += currAddress;
		return {currAddress + 4, BD};
	}
	if (GetPrimaryOpcode(nextInstr) == PrimaryOpcode::GROUP_19 && GetGroup19Opcode(nextInstr) == Opcode19::BCLR)
	{
		return {currAddress + 4, lr};
	}
	if (GetPrimaryOpcode(nextInstr) == PrimaryOpcode::GROUP_19 && GetGroup19Opcode(nextInstr) == Opcode19::BCCTR)
	{
		return {currAddress + 4, ctr};
	}
	return {currAddress + 4};
}

template<typename F>
static void selectThread(sint64 selectorId, F&& action_for_thread)
{
	__OSLockScheduler();
	cemu_assert_debug(activeThreadCount != 0);

	if (selectorId == -1)
	{
		for (sint32 i = 0; i < activeThreadCount; i++)
		{
			action_for_thread(GET_THREAD_BY_ID(activeThread[i]));
		}
	}
	else if (selectorId == 0)
	{
		// Use first thread if attempted to be stopped
		// todo: would this work better if it used main?
		action_for_thread(coreinit::OSGetDefaultThread(1));
	}
	else if (selectorId > 0)
	{
		for (sint32 i = 0; i < activeThreadCount; i++)
		{
			auto* thread = GET_THREAD_BY_ID(activeThread[i]);
			if (GET_THREAD_ID(thread) == selectorId)
			{
				action_for_thread(thread);
				break;
			}
		}
	}
	__OSUnlockScheduler();
}

template<typename F>
static void selectAndBreakThread(sint64 selectorId, F&& action_for_thread)
{
	__OSLockScheduler();
	cemu_assert_debug(activeThreadCount != 0);

	std::vector<OSThread_t*> pausedThreads;
	if (selectorId == -1)
	{
		for (sint32 i = 0; i < activeThreadCount; i++)
		{
			coreinit::__OSSuspendThreadNolock(GET_THREAD_BY_ID(activeThread[i]));
			pausedThreads.emplace_back(GET_THREAD_BY_ID(activeThread[i]));
		}
	}
	else if (selectorId == 0)
	{
		// Use first thread if attempted to be stopped
		OSThread_t* thread = GET_THREAD_BY_ID(activeThread[0]);
		for (sint32 i = 0; i < activeThreadCount; i++)
		{
			if (GET_THREAD_ID(GET_THREAD_BY_ID(activeThread[i])) < GET_THREAD_ID(thread))
			{
				thread = GET_THREAD_BY_ID(activeThread[i]);
			}
		}
		coreinit::__OSSuspendThreadNolock(thread);
		pausedThreads.emplace_back(thread);
	}
	else if (selectorId > 0)
	{
		for (sint32 i = 0; i < activeThreadCount; i++)
		{
			auto* thread = GET_THREAD_BY_ID(activeThread[i]);
			if (GET_THREAD_ID(thread) == selectorId)
			{
				coreinit::__OSSuspendThreadNolock(thread);
				pausedThreads.emplace_back(thread);
				break;
			}
		}
	}
	__OSUnlockScheduler();

	for (OSThread_t* thread : pausedThreads)
	{
		while (coreinit::OSIsThreadRunning(thread))
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		action_for_thread(thread);
	}
}

static void selectAndResumeThread(sint64 selectorId)
{
	__OSLockScheduler();
	cemu_assert_debug(activeThreadCount != 0);

	if (selectorId == -1)
	{
		for (sint32 i = 0; i < activeThreadCount; i++)
		{
			coreinit::__OSResumeThreadInternal(GET_THREAD_BY_ID(activeThread[i]), 4);
		}
	}
	else if (selectorId == 0)
	{
		// Use first thread if attempted to be stopped
		coreinit::__OSResumeThreadInternal(coreinit::OSGetDefaultThread(1), 1);
	}
	else if (selectorId > 0)
	{
		for (sint32 i = 0; i < activeThreadCount; i++)
		{
			auto* thread = GET_THREAD_BY_ID(activeThread[i]);
			if (GET_THREAD_ID(thread) == selectorId)
			{
				coreinit::__OSResumeThreadInternal(thread, 1);
				break;
			}
		}
	}
	__OSUnlockScheduler();
}

static void waitForBrokenThreads(std::unique_ptr<GDBServer::CommandContext> context, std::string_view reason)
{
	// This should pause all threads except trapped thread
	// It should however wait for the trapped thread
	// The trapped thread should be paused by the trap word instruction handler (aka the running thread)
	std::vector<OSThread_t*> threadsList;
	__OSLockScheduler();
	for (sint32 i = 0; i < activeThreadCount; i++)
	{
		threadsList.emplace_back(GET_THREAD_BY_ID(activeThread[i]));
	}
	__OSUnlockScheduler();

	for (OSThread_t* thread : threadsList)
	{
		while (coreinit::OSIsThreadRunning(thread))
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}

	context->QueueResponse(reason);
}

static void breakThreads(sint64 trappedThread)
{
	__OSLockScheduler();
	cemu_assert_debug(activeThreadCount != 0);

	// First, break other threads
	OSThread_t* mainThread = nullptr;
	for (sint32 i = 0; i < activeThreadCount; i++)
	{
		if (GET_THREAD_ID(GET_THREAD_BY_ID(activeThread[i])) == trappedThread)
		{
			mainThread = GET_THREAD_BY_ID(activeThread[i]);
		}
		else
		{
			coreinit::__OSSuspendThreadNolock(GET_THREAD_BY_ID(activeThread[i]));
		}
	}

	// Second, break trapped thread itself which should also pause execution of this handler
	// This will temporarily lift the scheduler lock until it's resumed from its suspension
	coreinit::__OSSuspendThreadNolock(mainThread);

	__OSUnlockScheduler();
}

std::unique_ptr<GDBServer> g_gdbstub;

GDBServer::GDBServer(uint16 port)
	: m_port(port)
{
#if BOOST_OS_WINDOWS
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
}

GDBServer::~GDBServer()
{
	if (m_client_socket != INVALID_SOCKET)
	{
		// close socket from other thread to forcefully stop accept() call
		closesocket(m_client_socket);
		m_client_socket = INVALID_SOCKET;
	}

	if (m_server_socket != INVALID_SOCKET)
	{
		closesocket(m_server_socket);
	}
#if BOOST_OS_WINDOWS
	WSACleanup();
#endif

	m_stopRequested = false;
	m_thread.join();
}

bool GDBServer::Initialize()
{
	cemuLog_createLogFile(false);

	if (m_server_socket = socket(PF_INET, SOCK_STREAM, 0); m_server_socket == SOCKET_ERROR)
		return false;

	int reuseEnabled = TRUE;
	if (setsockopt(m_server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseEnabled, sizeof(reuseEnabled)) == SOCKET_ERROR)
	{
		closesocket(m_server_socket);
		m_server_socket = INVALID_SOCKET;
		return false;
	}

	int nodelayEnabled = TRUE;
	if (setsockopt(m_server_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelayEnabled, sizeof(nodelayEnabled)) == SOCKET_ERROR)
	{
		closesocket(m_server_socket);
		m_server_socket = INVALID_SOCKET;
		return false;
	}

	memset(&m_server_addr, 0, sizeof(m_server_addr));
	m_server_addr.sin_family = AF_INET;
	m_server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	m_server_addr.sin_port = htons(m_port);

	if (bind(m_server_socket, (sockaddr*)&m_server_addr, sizeof(m_server_addr)) == SOCKET_ERROR)
	{
		closesocket(m_server_socket);
		m_server_socket = INVALID_SOCKET;
		return false;
	}

	if (listen(m_server_socket, s_maxGDBClients) == SOCKET_ERROR)
	{
		closesocket(m_server_socket);
		m_server_socket = INVALID_SOCKET;
		return false;
	}

	m_thread = std::thread(std::bind(&GDBServer::ThreadFunc, this));

	return true;
}

void GDBServer::ThreadFunc()
{
	SetThreadName("GDBServer::ThreadFunc");

	while (!m_stopRequested)
	{
		if (!m_client_connected)
		{
			cemuLog_logDebug(LogType::Force, "[GDBStub] Waiting for client to connect on port {}...", m_port);
			socklen_t client_addr_size = sizeof(m_client_addr);
			m_client_socket = accept(m_server_socket, (struct sockaddr*)&m_client_addr, &client_addr_size);
			m_client_connected = m_client_socket != SOCKET_ERROR;
		}
		else
		{
			auto receiveMessage = [&](char* buffer, const int32_t length) -> bool {
				if (recv(m_client_socket, buffer, length, 0) != SOCKET_ERROR)
					return false;
				return true;
			};

			auto readChar = [&]() -> char {
				char ret = 0;
				recv(m_client_socket, &ret, 1, 0);
				return ret;
			};

			char packetPrefix = readChar();

			switch (packetPrefix)
			{
			case '+':
			case '-':
				break;
			case '\x03':
			{
				cemuLog_logDebug(LogType::Force, "[GDBStub] Received interrupt (pressed CTRL+C?) from client!");
				selectAndBreakThread(-1, [](OSThread_t* thread) {
				});
				auto thread_status = fmt::format("T05thread:{:08X};", GET_THREAD_ID(coreinit::OSGetDefaultThread(1)));
				if (this->m_resumed_context)
				{
					this->m_resumed_context->QueueResponse(thread_status);
					this->m_resumed_context.reset();
				}
				else
				{
					auto response_full = fmt::format("+${}#{:02x}", thread_status, CommandContext::CalculateChecksum(thread_status));
					send(m_client_socket, response_full.c_str(), (int)response_full.size(), 0);
				}
				break;
			}
			case '$':
			{
				std::string message;
				uint8 checkedSum = 0;
				for (uint32_t i = 1;; i++)
				{
					char c = readChar();
					if (c == '#')
						break;
					checkedSum += static_cast<uint8>(c);
					message.push_back(c);

					if (i >= s_maxPacketSize)
						cemuLog_logDebug(LogType::Force, "[GDBStub] Received too big of a buffer: {}", message);
				}
				char checkSumStr[2];
				receiveMessage(checkSumStr, 2);
				uint32_t checkSum = std::stoi(std::string(checkSumStr, sizeof(checkSumStr)), nullptr, 16);
				assert((checkedSum & 0xFF) == checkSum);

				HandleCommand(message);
				break;
			}
			default:
				// cemuLog_logDebug(LogType::Force, "[GDBStub] Unknown packet start: {}", packetPrefix);
				break;
			}
		}
	}

	if (m_client_socket != INVALID_SOCKET)
		closesocket(m_client_socket);
}

void GDBServer::HandleCommand(const std::string& command_str)
{
	auto context = std::make_unique<CommandContext>(this, command_str);

	if (context->IsValid())
	{
		// cemuLog_logDebug(LogType::Force, "[GDBStub] Extracted Command {}", fmt::join(context->GetArgs(), ","));
	}

	switch (context->GetType())
	{
	// Extended commands
	case CMDType::QUERY_GET:
	case CMDType::QUERY_SET:
		return HandleQuery(context);
	case CMDType::VCONT:
		return HandleVCont(context);
	// Regular commands
	case CMDType::IS_THREAD_RUNNING:
		return CMDIsThreadActive(context);
	case CMDType::SET_ACTIVE_THREAD:
		return CMDSetActiveThread(context);
	case CMDType::ACTIVE_THREAD_STATUS:
		return CMDGetThreadStatus(context);
	case CMDType::CONTINUE:
		return CMDContinue(context);
	case CMDType::ACTIVE_THREAD_STEP:
		break;
	case CMDType::REGISTER_READ:
		return CMDReadRegister(context);
	case CMDType::REGISTER_SET:
		return CMDWriteRegister(context);
	case CMDType::REGISTERS_READ:
		return CMDReadRegisters(context);
	case CMDType::REGISTERS_WRITE:
		return CMDWriteRegisters(context);
	case CMDType::MEMORY_READ:
		return CMDReadMemory(context);
	case CMDType::MEMORY_WRITE:
		return CMDWriteMemory(context);
	case CMDType::BREAKPOINT_SET:
		return CMDInsertBreakpoint(context);
	case CMDType::BREAKPOINT_REMOVE:
		return CMDDeleteBreakpoint(context);
	case CMDType::INVALID:
	default:
		return CMDNotFound(context);
	}

	CMDNotFound(context);
}

void GDBServer::HandleQuery(std::unique_ptr<CommandContext>& context) const
{
	if (!context->IsValid())
		return context->QueueResponse(RESPONSE_EMPTY);

	const auto& query_cmd = context->GetArgs()[0];
	const auto& query_args = context->GetArgs().begin() + 1;

	if (query_cmd == "qSupported")
	{
		context->QueueResponse(s_supportedFeatures);
	}
	else if (query_cmd == "qAttached")
	{
		context->QueueResponse("1");
	}
	else if (query_cmd == "qRcmd")
	{
	}
	else if (query_cmd == "qC")
	{
		context->QueueResponse("QC");
		context->QueueResponse(std::to_string(m_activeThreadContinueSelector));
	}
	else if (query_cmd == "qOffsets")
	{
		const auto module_count = RPLLoader_GetModuleCount();
		const auto module_list = RPLLoader_GetModuleList();
		for (sint32 i = 0; i < module_count; i++)
		{
			const RPLModule* rpl = module_list[i];
			if (rpl->entrypoint == m_entry_point)
			{
				context->QueueResponse(fmt::format("TextSeg={:08X};DataSeg={:08X}", rpl->regionMappingBase_text.GetMPTR(), rpl->regionMappingBase_data));
			}
		}
	}
	else if (query_cmd == "qfThreadInfo")
	{
		std::vector<std::string> threadIds;
		selectThread(-1, [&threadIds](OSThread_t* thread) {
			threadIds.emplace_back(fmt::format("{:08X}", memory_getVirtualOffsetFromPointer(thread)));
		});
		context->QueueResponse(fmt::format("m{}", fmt::join(threadIds, ",")));
	}
	else if (query_cmd == "qsThreadInfo")
	{
		context->QueueResponse("l");
	}
	else if (query_cmd == "qXfer")
	{
		auto& type = query_args[0];

		if (type == "features")
		{
			auto& annex = query_args[1];
			sint64 read_offset = std::stoul(query_args[2], nullptr, 16);
			sint64 read_length = std::stoul(query_args[3], nullptr, 16);
			if (annex == "target.xml")
			{
				if (read_offset >= GDBTargetXML.size())
					context->QueueResponse("l");
				else
				{
					auto paginated_str = GDBTargetXML.substr(read_offset, read_length);
					context->QueueResponse((paginated_str.size() == read_length) ? "m" : "l");
					context->QueueResponse(paginated_str);
				}
			}
			else
				cemuLog_logDebug(LogType::Force, "[GDBStub] qXfer:features:read:{} isn't a known feature document", annex);
		}
		else if (type == "threads")
		{
			sint64 read_offset = std::stoul(query_args[1], nullptr, 16);
			sint64 read_length = std::stoul(query_args[2], nullptr, 16);

			std::string threads_res;
			threads_res += R"(<?xml version="1.0"?>)";
			threads_res += "<threads>";
			// note: clion seems to default to the first thread
			std::map<sint64, std::string> threads_list;
			selectThread(-1, [&threads_list](OSThread_t* thread) {
				std::string entry;
				entry += fmt::format(R"(<thread id="{:x}" core="{}")", GET_THREAD_ID(thread), thread->context.upir.value());
				if (!thread->threadName.IsNull())
					entry += fmt::format(R"( name="{}")", CommandContext::EscapeXMLString(thread->threadName.GetPtr()));
				// todo: could add a human-readable description of the thread here
				entry += fmt::format("></thread>");
				threads_list.emplace(GET_THREAD_ID(thread), entry);
			});
			for (auto& entry : threads_list)
			{
				threads_res += entry.second;
			}
			threads_res += "</threads>";

			if (read_offset >= threads_res.size())
				context->QueueResponse("l");
			else
			{
				auto paginated_str = threads_res.substr(read_offset, read_length);
				context->QueueResponse((paginated_str.size() == read_length) ? "m" : "l");
				context->QueueResponse(paginated_str);
			}
		}
		else if (type == "libraries")
		{
			sint64 read_offset = std::stoul(query_args[1], nullptr, 16);
			sint64 read_length = std::stoul(query_args[2], nullptr, 16);

			std::string library_list;
			library_list += R"(<?xml version="1.0"?>)";
			library_list += "<library-list>";

			const auto module_count = RPLLoader_GetModuleCount();
			const auto module_list = RPLLoader_GetModuleList();
			for (sint32 i = 0; i < module_count; i++)
			{
				library_list += fmt::format(R"(<library name="{}"><segment address="{:#x}"/></library>)", CommandContext::EscapeXMLString(module_list[i]->moduleName2), module_list[i]->regionMappingBase_text.GetMPTR());
			}
			library_list += "</library-list>";

			if (read_offset >= library_list.size())
				context->QueueResponse("l");
			else
			{
				auto paginated_str = library_list.substr(read_offset, read_length);
				context->QueueResponse((paginated_str.size() == read_length) ? "m" : "l");
				context->QueueResponse(paginated_str);
			}
		}
		else
		{
			context->QueueResponse(RESPONSE_EMPTY);
		}
	}
	else
	{
		context->QueueResponse(RESPONSE_EMPTY);
	}
}

void GDBServer::HandleVCont(std::unique_ptr<CommandContext>& context)
{
	if (!context->IsValid())
	{
		cemuLog_logDebug(LogType::Force, "[GDBStub] Received unsupported vCont command: {}", context->GetCommand());
		// cemu_assert_unimplemented();
		return context->QueueResponse(RESPONSE_EMPTY);
	}

	const std::string& vcont_cmd = context->GetArgs()[0];
	if (vcont_cmd == "vCont?")
		return context->QueueResponse("vCont;c;C;s;S");

	else if (vcont_cmd != "vCont;")
		return context->QueueResponse(RESPONSE_EMPTY);

	m_resumed_context = std::move(context);

	bool resumedNoThreads = true;
	for (const auto operation : TokenizeView(m_resumed_context->GetArgs()[1], ';'))
	{
		// todo: this might have issues with the signal versions (C/S)
		// todo: test whether this works with multiple vCont;c:123123;c:123123
		std::string_view operationType = operation.substr(0, operation.find(':'));
		sint64 threadSelector = operationType.size() == operation.size() ? -1 : std::stoll(std::string(operation.substr(operationType.size() + 1)), nullptr, 16);

		if (operationType == "c" || operationType.starts_with("C"))
		{
			selectAndResumeThread(threadSelector);
			resumedNoThreads = false;
		}
		else if (operationType == "s" || operationType.starts_with("S"))
		{
			selectThread(threadSelector, [this](OSThread_t* thread) {
				auto nextInstructions = findNextInstruction(thread->context.srr0, thread->context.lr, thread->context.ctr);
				for (MPTR nextInstr : nextInstructions)
				{
					auto bpIt = m_patchedInstructions.find(nextInstr);
					if (bpIt == m_patchedInstructions.end())
						this->m_patchedInstructions.try_emplace(nextInstr, nextInstr, BreakpointType::BP_STEP_POINT, false, "swbreak:;");
					else
						bpIt->second.PauseOnNextInterrupt();
				}
			});
		}
	}

	if (resumedNoThreads)
	{
		selectAndResumeThread(-1);
		cemuLog_logDebug(LogType::Force, "[GDBStub] Resumed all threads after skip instructions");
	}
}

void GDBServer::CMDContinue(std::unique_ptr<CommandContext>& context)
{
	m_resumed_context = std::move(context);
	selectAndResumeThread(m_activeThreadContinueSelector);
}

void GDBServer::CMDNotFound(std::unique_ptr<CommandContext>& context)
{
	return context->QueueResponse(RESPONSE_EMPTY);
}

void GDBServer::CMDIsThreadActive(std::unique_ptr<CommandContext>& context)
{
	sint64 threadSelector = std::stoll(context->GetArgs()[1], nullptr, 16);
	bool foundThread = false;
	selectThread(threadSelector, [&foundThread](OSThread_t* thread) {
		foundThread = true;
	});

	if (foundThread)
		return context->QueueResponse(RESPONSE_OK);
	else
		return context->QueueResponse(RESPONSE_ERROR);
}

void GDBServer::CMDSetActiveThread(std::unique_ptr<CommandContext>& context)
{
	sint64 threadSelector = std::stoll(context->GetArgs()[2], nullptr, 16);
	if (threadSelector >= 0)
	{
		bool foundThread = false;
		selectThread(threadSelector, [&foundThread](OSThread_t* thread) {
			foundThread = true;
		});
		if (!foundThread)
			return context->QueueResponse(RESPONSE_ERROR);
	}
	if (context->GetArgs()[1] == "c")
		m_activeThreadContinueSelector = threadSelector;
	else
		m_activeThreadSelector = threadSelector;
	return context->QueueResponse(RESPONSE_OK);
}

void GDBServer::CMDGetThreadStatus(std::unique_ptr<CommandContext>& context)
{
	selectThread(0, [&context](OSThread_t* thread) {
		context->QueueResponse(fmt::format("T05thread:{:08X};", memory_getVirtualOffsetFromPointer(thread)));
	});
}

void GDBServer::CMDReadRegister(std::unique_ptr<CommandContext>& context) const
{
	sint32 reg = std::stoi(context->GetArgs()[1], nullptr, 16);
	selectThread(m_activeThreadSelector, [reg, &context](OSThread_t* thread) {
		auto& cpu = thread->context;
		if (reg >= RegisterID::R0_START && reg <= RegisterID::R31_END)
		{
			return context->QueueResponse(fmt::format("{:08X}", CPU_swapEndianU32(cpu.gpr[reg])));
		}
		else if (reg >= RegisterID::F0_START && reg <= RegisterID::F31_END)
		{
			return context->QueueResponse(fmt::format("{:016X}", cpu.fp_ps0[reg - RegisterID::F0_START].value()));
		}
		else if (reg == RegisterID::FPSCR)
		{
			return context->QueueResponse(fmt::format("{:08X}", cpu.fpscr.fpscr.value()));
		}
		else
		{
			switch (reg)
			{
			case RegisterID::PC: return context->QueueResponse(fmt::format("{:08X}", cpu.srr0));
			case RegisterID::MSR: return context->QueueResponse("xxxxxxxx");
			case RegisterID::CR: return context->QueueResponse(fmt::format("{:08X}", cpu.cr));
			case RegisterID::LR: return context->QueueResponse(fmt::format("{:08X}", CPU_swapEndianU32(cpu.lr)));
			case RegisterID::CTR: return context->QueueResponse(fmt::format("{:08X}", cpu.ctr));
			case RegisterID::XER: return context->QueueResponse(fmt::format("{:08X}", cpu.xer));
			default: break;
			}
		}
	});
}

void GDBServer::CMDWriteRegister(std::unique_ptr<CommandContext>& context) const
{
	sint32 reg = std::stoi(context->GetArgs()[1], nullptr, 16);
	uint64 value = std::stoll(context->GetArgs()[2], nullptr, 16);
	selectThread(m_activeThreadSelector, [reg, value, &context](OSThread_t* thread) {
		auto& cpu = thread->context;
		if (reg >= RegisterID::R0_START && reg <= RegisterID::R31_END)
		{
			cpu.gpr[reg] = CPU_swapEndianU32(value);
			return context->QueueResponse(RESPONSE_OK);
		}
		else if (reg >= RegisterID::F0_START && reg <= RegisterID::F31_END)
		{
			// todo: figure out how to properly write to paired single registers
			cpu.fp_ps0[reg - RegisterID::F0_START] = uint64be{value};
			return context->QueueResponse(RESPONSE_OK);
		}
		else if (reg == RegisterID::FPSCR)
		{
			cpu.fpscr.fpscr = uint32be{(uint32)value};
			return context->QueueResponse(RESPONSE_OK);
		}
		else
		{
			switch (reg)
			{
			case RegisterID::PC:
				cpu.srr0 = value;
				return context->QueueResponse(RESPONSE_OK);
			case RegisterID::MSR:
				return context->QueueResponse(RESPONSE_ERROR);
			case RegisterID::CR:
				cpu.cr = value;
				return context->QueueResponse(RESPONSE_OK);
			case RegisterID::LR:
				cpu.lr = CPU_swapEndianU32(value);
				return context->QueueResponse(RESPONSE_OK);
			case RegisterID::CTR:
				cpu.ctr = value;
				return context->QueueResponse(RESPONSE_OK);
			case RegisterID::XER:
				cpu.xer = value;
				return context->QueueResponse(RESPONSE_OK);
			default:
				return context->QueueResponse(RESPONSE_ERROR);
			}
		}
	});
}

void GDBServer::CMDReadRegisters(std::unique_ptr<CommandContext>& context) const
{
	selectThread(m_activeThreadSelector, [&context](OSThread_t* thread) {
		for (uint32& reg : thread->context.gpr)
		{
			context->QueueResponse(fmt::format("{:08X}", CPU_swapEndianU32(reg)));
		}
	});
}

void GDBServer::CMDWriteRegisters(std::unique_ptr<CommandContext>& context) const
{
	selectThread(m_activeThreadSelector, [&context](OSThread_t* thread) {
		auto& registers = context->GetArgs()[1];
		for (uint32 i = 0; i < 32; i++)
		{
			thread->context.gpr[i] = CPU_swapEndianU32(std::stoi(registers.substr(i * 2, 2), nullptr, 16));
		}
	});
}

void GDBServer::CMDReadMemory(std::unique_ptr<CommandContext>& context)
{
	sint64 addr = std::stoul(context->GetArgs()[1], nullptr, 16);
	sint64 length = std::stoul(context->GetArgs()[2], nullptr, 16);

	// todo: handle cross-mmu-range memory requests
	if (!memory_isAddressRangeAccessible(addr, length))
		return context->QueueResponse(RESPONSE_ERROR);

	std::string memoryRepr;
	uint8* values = memory_getPointerFromVirtualOffset(addr);
	for (sint64 i = 0; i < length; i++)
	{
		memoryRepr += fmt::format("{:02X}", values[i]);
	}

	auto patchesRange = m_patchedInstructions.lower_bound(addr);
	while (patchesRange != m_patchedInstructions.end() && patchesRange->first < (addr + length))
	{
		auto replStr = fmt::format("{:02X}", patchesRange->second.GetVisibleOpCode());
		memoryRepr[(patchesRange->first - addr) * 2] = replStr[0];
		memoryRepr[(patchesRange->first - addr) * 2 + 1] = replStr[1];
		patchesRange++;
	}
	return context->QueueResponse(memoryRepr);
}

void GDBServer::CMDWriteMemory(std::unique_ptr<CommandContext>& context)
{
	sint64 addr = std::stoul(context->GetArgs()[1], nullptr, 16);
	sint64 length = std::stoul(context->GetArgs()[2], nullptr, 16);
	auto source = context->GetArgs()[3];

	// todo: handle cross-mmu-range memory requests
	if (!memory_isAddressRangeAccessible(addr, length))
		return context->QueueResponse(RESPONSE_ERROR);

	uint8* values = memory_getPointerFromVirtualOffset(addr);
	for (sint64 i = 0; i < length; i++)
	{
		uint8 hexValue;
		const std::from_chars_result result = std::from_chars(source.data() + (i * 2), (source.data() + (i * 2) + 2), hexValue, 16);
		if (result.ec == std::errc::invalid_argument || result.ec == std::errc::result_out_of_range)
			return context->QueueResponse(RESPONSE_ERROR);

		if (auto it = m_patchedInstructions.find(addr + i); it != m_patchedInstructions.end())
		{
			uint32 newOpCode = it->second.GetVisibleOpCode();
			uint32 byteIndex = 3 - ((addr + i) % 4);			// inverted because of big endian, so address 0 is the highest byte
			newOpCode &= ~(0xFF << (byteIndex * 8));			// mask out the byte
			newOpCode |= ((uint32)hexValue << (byteIndex * 8)); // set new byte with OR
			it->second.WriteNewOpCode(newOpCode);
		}
		else
		{
			values[i] = hexValue;
		}
	}
	return context->QueueResponse(RESPONSE_OK);
}

void GDBServer::CMDInsertBreakpoint(std::unique_ptr<CommandContext>& context)
{
	auto type = std::stoul(context->GetArgs()[1], nullptr, 16);
	MPTR addr = static_cast<MPTR>(std::stoul(context->GetArgs()[2], nullptr, 16));

	if (type == 0 || type == 1)
	{
		auto bp = this->m_patchedInstructions.find(addr);
		if (bp != this->m_patchedInstructions.end())
			this->m_patchedInstructions.erase(bp);
		this->m_patchedInstructions.try_emplace(addr, addr, BreakpointType::BP_PERSISTENT, type == 0, type == 0 ? "swbreak:;" : "hwbreak:;");
	}
	else if (type == 2 || type == 3 || type == 4)
	{
		if (this->m_watch_point)
			return context->QueueResponse(RESPONSE_ERROR);

		this->m_watch_point = std::make_unique<AccessBreakpoint>(addr, (AccessPointType)type);
	}

	return context->QueueResponse(RESPONSE_OK);
}

void GDBServer::CMDDeleteBreakpoint(std::unique_ptr<CommandContext>& context)
{
	auto type = std::stoul(context->GetArgs()[1], nullptr, 16);
	MPTR addr = static_cast<MPTR>(std::stoul(context->GetArgs()[2], nullptr, 16));

	if (type == 0 || type == 1)
	{
		auto bp = this->m_patchedInstructions.find(addr);
		if (bp == this->m_patchedInstructions.end() || !bp->second.ShouldBreakThreads())
			return context->QueueResponse(RESPONSE_ERROR);
		else
			this->m_patchedInstructions.erase(bp);
	}
	else if (type == 2 || type == 3 || type == 4)
	{
		if (!this->m_watch_point || this->m_watch_point->GetAddress() != addr)
			return context->QueueResponse(RESPONSE_ERROR);

		this->m_watch_point.reset();
	}

	return context->QueueResponse(RESPONSE_OK);
}

// Internal functions for control
void GDBServer::HandleTrapInstruction(PPCInterpreter_t* hCPU)
{
	// First, restore any removed breakpoints
	for (auto& bp : m_patchedInstructions)
	{
		if (bp.second.IsRemoved())
			bp.second.Restore();
	}

	auto patchedBP = m_patchedInstructions.find(hCPU->instructionPointer);
	if (patchedBP == m_patchedInstructions.end())
		return cemu_assert_suspicious();

	// Secondly, delete one-shot breakpoints but also temporarily delete patched instruction to run original instruction
	OSThread_t* currThread = coreinit::OSGetCurrentThread();
	std::string pauseReason = fmt::format("T05thread:{:08X};core:{:02X};{}", GET_THREAD_ID(currThread), PPCInterpreter_getCoreIndex(hCPU), patchedBP->second.GetReason());
	bool pauseThreads = patchedBP->second.ShouldBreakThreads() || patchedBP->second.ShouldBreakThreadsOnNextInterrupt();
	if (patchedBP->second.IsPersistent())
	{
		// Insert new restore breakpoints at next possible instructions which restores breakpoints but won't pause the CPU
		std::vector<MPTR> nextInstructions = findNextInstruction(hCPU->instructionPointer, hCPU->spr.LR, hCPU->spr.CTR);
		for (MPTR nextInstr : nextInstructions)
		{
			if (!m_patchedInstructions.contains(nextInstr))
				this->m_patchedInstructions.try_emplace(nextInstr, nextInstr, BreakpointType::BP_STEP_POINT, false, "");
		}
		patchedBP->second.RemoveTemporarily();
	}
	else
	{
		m_patchedInstructions.erase(patchedBP);
	}

	// Thirdly, delete any instructions that were generated by a skip instruction
	for (auto it = m_patchedInstructions.cbegin(), next_it = it; it != m_patchedInstructions.cend(); it = next_it)
	{
		++next_it;
		if (it->second.IsSkipBreakpoint())
		{
			m_patchedInstructions.erase(it);
		}
	}

	// Fourthly, the stub can insert breakpoints that are just meant to restore patched instructions, in which case we just want to continue
	if (pauseThreads)
	{
		cemuLog_logDebug(LogType::Force, "[GDBStub] Got trapped by a breakpoint!");
		if (m_resumed_context)
		{
			// Spin up thread to signal when another GDB stub trap is found
			ThreadPool::FireAndForget(&waitForBrokenThreads, std::move(m_resumed_context), pauseReason);
		}

		breakThreads(GET_THREAD_ID(coreinit::OSGetCurrentThread()));
		cemuLog_logDebug(LogType::Force, "[GDBStub] Resumed from a breakpoint!");
	}
}

void GDBServer::HandleAccessException(uint64 dr6)
{
	bool triggeredWrite = GetBits(dr6, 2, 1);
	bool triggeredReadWrite = GetBits(dr6, 3, 1);

	std::string response;
	if (m_watch_point->GetType() == AccessPointType::BP_WRITE && triggeredWrite)
		response = fmt::format("watch:{:08X};", m_watch_point->GetAddress());
	else if (m_watch_point->GetType() == AccessPointType::BP_READ && triggeredReadWrite && !triggeredWrite)
		response = fmt::format("rwatch:{:08X};", m_watch_point->GetAddress());
	else if (m_watch_point->GetType() == AccessPointType::BP_BOTH && triggeredReadWrite)
		response = fmt::format("awatch:{:08X};", m_watch_point->GetAddress());

	if (!response.empty())
	{
		PPCInterpreter_t* hCPU = PPCInterpreter_getCurrentInstance();
		cemuLog_logDebug(LogType::Force, "Received matching breakpoint exception: {}", response);
		auto nextInstructions = findNextInstruction(hCPU->instructionPointer, hCPU->spr.LR, hCPU->spr.CTR);
		for (MPTR nextInstr : nextInstructions)
		{
			auto bpIt = m_patchedInstructions.find(nextInstr);
			if (bpIt == m_patchedInstructions.end())
				this->m_patchedInstructions.try_emplace(nextInstr, nextInstr, BreakpointType::BP_STEP_POINT, false, response);
			else
				bpIt->second.PauseOnNextInterrupt();
		}
	}
}

void GDBServer::HandleEntryStop(uint32 entryAddress)
{
	this->m_patchedInstructions.try_emplace(entryAddress, entryAddress, BreakpointType::BP_SINGLE, false, "");
	m_entry_point = entryAddress;
}