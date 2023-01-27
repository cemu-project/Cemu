#include "GDBStub.h"
#include "Debugger.h"
#include "Cafe/HW/Espresso/Recompiler/PPCRecompiler.h"
#include "util/helpers/helpers.h"
#include "util/ThreadPool/ThreadPool.h"
#include "Cafe/OS/RPL/rpl.h"
#include "Cafe/OS/RPL/rpl_structs.h"
#include "Cafe/OS/libs/coreinit/coreinit_Scheduler.h"
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"
#include "Cafe/HW/Espresso/Interpreter/PPCInterpreterInternal.h"

#include <ranges>
#include <HW/Espresso/EspressoISA.h>

#define GET_THREAD_ID(threadPtr) memory_getVirtualOffsetFromPointer(threadPtr)
#define GET_THREAD_BY_ID(threadId) (OSThread_t*)memory_getPointerFromPhysicalOffset(threadId)


template<typename F>
static void selectThread(sint64 selectorId, F&& action_for_thread) {
    __OSLockScheduler();
    cemu_assert_debug(activeThreadCount != 0);

    if (selectorId == -1) {
        for (sint32 i = 0; i < activeThreadCount; i++) {
            action_for_thread(GET_THREAD_BY_ID(activeThread[i]));
        }
    }
    else if (selectorId == 0) {
        // Use first thread if attempted to be stopped
        // todo: would this work better if it used main?
        action_for_thread(coreinit::OSGetDefaultThread(1));
    }
    else if (selectorId > 0) {
        for (sint32 i = 0; i < activeThreadCount; i++) {
            auto* thread = GET_THREAD_BY_ID(activeThread[i]);
            if (GET_THREAD_ID(thread) == selectorId) {
                action_for_thread(thread);
                break;
            }
        }
    }
    __OSUnlockScheduler();
}

template<typename F>
static void selectAndBreakThread(sint64 selectorId, F&& action_for_thread) {
    __OSLockScheduler();
    cemu_assert_debug(activeThreadCount != 0);

    std::vector<OSThread_t*> pausedThreads;
    if (selectorId == -1) {
        for (sint32 i = 0; i < activeThreadCount; i++) {
            coreinit::__OSSuspendThreadNolock(GET_THREAD_BY_ID(activeThread[i]));
            pausedThreads.emplace_back(GET_THREAD_BY_ID(activeThread[i]));
        }
    }
    else if (selectorId == 0) {
        // Use first thread if attempted to be stopped
        OSThread_t* thread = GET_THREAD_BY_ID(activeThread[0]);
        for (sint32 i = 0; i < activeThreadCount; i++) {
            if (GET_THREAD_ID(GET_THREAD_BY_ID(activeThread[i])) < GET_THREAD_ID(thread)) {
                thread = GET_THREAD_BY_ID(activeThread[i]);
            }
        }
        coreinit::__OSSuspendThreadNolock(thread);
        pausedThreads.emplace_back(thread);
    }
    else if (selectorId > 0) {
        for (sint32 i = 0; i < activeThreadCount; i++) {
            auto* thread = GET_THREAD_BY_ID(activeThread[i]);
            if (GET_THREAD_ID(thread) == selectorId) {
                coreinit::__OSSuspendThreadNolock(thread);
                pausedThreads.emplace_back(thread);
                break;
            }
        }
    }
    __OSUnlockScheduler();

    for (OSThread_t* thread : pausedThreads) {
        while (coreinit::OSIsThreadRunning(thread)) std::this_thread::sleep_for(std::chrono::milliseconds(50));
        action_for_thread(thread);
    }
}

static void selectAndResumeThread(sint64 selectorId) {
    __OSLockScheduler();
    cemu_assert_debug(activeThreadCount != 0);

    if (selectorId == -1) {
        for (sint32 i = 0; i < activeThreadCount; i++) {
            coreinit::__OSResumeThreadInternal(GET_THREAD_BY_ID(activeThread[i]), 4);
        }
    }
    else if (selectorId == 0) {
        // Use first thread if attempted to be stopped
        coreinit::__OSResumeThreadInternal(coreinit::OSGetDefaultThread(1), 1);
    }
    else if (selectorId > 0) {
        for (sint32 i = 0; i < activeThreadCount; i++) {
            auto* thread = GET_THREAD_BY_ID(activeThread[i]);
            if (GET_THREAD_ID(thread) == selectorId) {
                coreinit::__OSResumeThreadInternal(thread, 1);
                break;
            }
        }
    }
    __OSUnlockScheduler();
}

static void waitForBrokenThreads(std::unique_ptr<GDBServer::CommandContext> context) {
    // This should pause all threads except trapped thread
    // It should however wait for the trapped thread
    // The trapped thread should be paused by the trap word instruction handler (aka the running thread)
    std::vector<OSThread_t*> threadsList;
    __OSLockScheduler();
    for (sint32 i = 0; i < activeThreadCount; i++) {
        threadsList.emplace_back(GET_THREAD_BY_ID(activeThread[i]));
    }
    __OSUnlockScheduler();

    for (OSThread_t* thread : threadsList) {
        while (coreinit::OSIsThreadRunning(thread)) std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    context->QueueResponse("S05");
}

static void breakThreads(sint64 trappedThread) {
    __OSLockScheduler();
    cemu_assert_debug(activeThreadCount != 0);

    // First, break other threads
    OSThread_t* mainThread = nullptr;
    for (sint32 i = 0; i < activeThreadCount; i++) {
        if (GET_THREAD_ID(GET_THREAD_BY_ID(activeThread[i])) == trappedThread) {
            mainThread = GET_THREAD_BY_ID(activeThread[i]);
        }
        else {
            coreinit::__OSSuspendThreadNolock(GET_THREAD_BY_ID(activeThread[i]));
        }
    }

    // Second, break trapped thread itself which should also pause execution of this handler
    // This will temporarily lift the scheduler lock until it's resumed from its suspension
    coreinit::__OSSuspendThreadNolock(mainThread);

    __OSUnlockScheduler();
}



/* <feature name="org.gnu.gdb.power.fpu">
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
 * */
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
</target>)";


std::unique_ptr<GDBServer> g_gdbstub;

GDBServer::GDBServer(uint16 port) : m_port(port) {
#if BOOST_OS_WINDOWS
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
}

GDBServer::~GDBServer() {
	if (m_client_socket != INVALID_SOCKET) {
		// close socket from other thread to forcefully stop accept() call
        closesocket(m_client_socket);
		m_client_socket = INVALID_SOCKET;
	}

	if (m_server_socket != INVALID_SOCKET) {
        closesocket(m_server_socket);
	}
#if BOOST_OS_WINDOWS
	WSACleanup();
#endif

	m_thread.request_stop();
	m_thread.join();
}

bool GDBServer::Initialize() {
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

	m_thread = std::jthread(std::bind(&GDBServer::ThreadFunc, this, std::placeholders::_1));

	return true;
}

void GDBServer::ThreadFunc(const std::stop_token& stop_token) {
	SetThreadName("GDBServer::ThreadFunc");

	while (!stop_token.stop_requested())
	{
		if (!m_client_connected)
		{
			cemuLog_logDebug(LogType::Force, "[GDBStub] Waiting for client to connect on port {}...", m_port);
			int client_addr_size = sizeof(m_client_addr);
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

			switch (packetPrefix) {
			case '+':
			case '-':
				break;
			case '\x03': {
                cemuLog_logDebug(LogType::Force, "[GDBStub] Received interrupt (pressed CTRL+C?) from client!");
                selectAndBreakThread(-1, [](OSThread_t* thread) {
                });
                auto thread_status = fmt::format("T05thread:{:08X};", GET_THREAD_ID(coreinit::OSGetDefaultThread(1)));
                auto response_full = fmt::format("+${}#{:02x}", thread_status, CommandContext::CalculateChecksum(thread_status));
                send(m_client_socket, response_full.c_str(), (int)response_full.size(), 0);
                break;
            }
			case '$': {
				std::string message;
				uint8 checkedSum = 0;
				for (uint32_t i = 1; ; i++) {
					char c = readChar();
					if (c == '#')
						break;
					checkedSum += static_cast<uint8>(c);
					message.push_back(c);

					if (i >= s_maxPacketSize) {
						cemuLog_logDebug(LogType::Force, "[GDBStub] Received too big of a buffer: {}", message);
					}
				}
				char checkSumStr[2];
				receiveMessage(checkSumStr, 2);
				uint32_t checkSum = std::stoi(checkSumStr, nullptr, 16);
				assert((checkedSum & 0xFF) == checkSum);

				HandleCommand(message);
				break;
			}
			default:
				//cemuLog_logDebug(LogType::Force, "[GDBStub] Unknown packet start: {}", packetPrefix);
                break;
			}
		}
	}

	if (m_client_socket != INVALID_SOCKET)
		closesocket(m_client_socket);
}

struct GDBBreakpoint {
    MPTR address;
    uint32 origOpCode;
    bool visible;
    bool pauseThreads;
    // type
    bool restoreAfterInterrupt;
    bool deleteAfterInterrupt;
    bool removedAfterInterrupt;
};

std::map<MPTR, GDBBreakpoint> patchedInstructions;

static void insertBreakpoint(MPTR address, bool visible, bool pauseThreads, bool restoreAfterInterrupt, bool deleteAfterInterrupt) {
    //cemuLog_logDebug(LogType::Force, "[GDBStub] Inserting breakpoint for {:08x}, restoreAfterInterrupt = {}, deleteAfterInterrupt = {}, pauseThreads = {}", address, restoreAfterInterrupt, deleteAfterInterrupt, pauseThreads);
    if (auto bpIt = patchedInstructions.find(address); bpIt != patchedInstructions.end()) {
        if (!bpIt->second.pauseThreads && pauseThreads) {
            //cemuLog_logDebug(LogType::Force, "[GDBStub] Upgraded restore point to breakpoint");
            bpIt->second.pauseThreads = true;
            bpIt->second.restoreAfterInterrupt = restoreAfterInterrupt;
        }
        else {
            // breakpoint already exists and wasn't previously restore point
            cemu_assert_suspicious();
            return;
        }
    }

    uint32 origOpCode = memory_readU32(address);
    memory_writeU32(address, DEBUGGER_BP_T_GDBSTUB_TW);
    PPCRecompiler_invalidateRange(address, address + 4);

    patchedInstructions.emplace(address, GDBBreakpoint{
        .address = address,
        .origOpCode = origOpCode,
        .visible = visible,
        .pauseThreads = pauseThreads,
        .restoreAfterInterrupt=restoreAfterInterrupt,
        .deleteAfterInterrupt=deleteAfterInterrupt,
        .removedAfterInterrupt=false
    });
}

static void restoreBreakpoint(MPTR address) {
    //cemuLog_logDebug(LogType::Force, "[GDBStub] Restoring breakpoint for {:08x}", address);
    if (!patchedInstructions.contains(address)) {
        // Restoring a breakpoint on an address that has no breakpoints?
        cemu_assert_suspicious();
        return;
    }

    memory_writeU32(address, DEBUGGER_BP_T_GDBSTUB_TW);
    PPCRecompiler_invalidateRange(address, address + 4);
    patchedInstructions[address].removedAfterInterrupt = false;
}

static void deleteBreakpoint(MPTR address, bool softRemove = false) {
    //cemuLog_logDebug(LogType::Force, "[GDBStub] {} breakpoint for {:08x}", softRemove ? "Remove" : "Delete", address);
    if (!patchedInstructions.contains(address)) {
        // Removing a breakpoint on an address that has no breakpoints?
        cemu_assert_suspicious();
        return;
    }

    memory_writeU32(address, patchedInstructions[address].origOpCode);
    PPCRecompiler_invalidateRange(address, address + 4);

    if (softRemove)
        patchedInstructions[address].removedAfterInterrupt = true;
    else
        patchedInstructions.erase(address);
}

static std::vector<MPTR> findNextInstruction(MPTR currAddress, uint32 lr, uint32 ctr) {
    using namespace Espresso;

    uint32 nextInstr = memory_readU32(currAddress);
    if (GetPrimaryOpcode(nextInstr) == PrimaryOpcode::B) {
        uint32 LI;
        bool AA, LK;
        decodeOp_B(nextInstr, LI, AA, LK);
        if (!AA)
            LI += currAddress;
        return {LI};
    }
    if (GetPrimaryOpcode(nextInstr) == PrimaryOpcode::BC) {
        uint32 BD, BI;
        BOField BO{};
        bool AA, LK;
        decodeOp_BC(nextInstr, BD, BO, BI, AA, LK);
        if (!LK)
            BD += currAddress;
        return {currAddress+4, BD};
    }
    if (GetPrimaryOpcode(nextInstr) == PrimaryOpcode::GROUP_19 && GetGroup19Opcode(nextInstr) == Opcode19::BCLR) {
        return {currAddress+4, lr};
    }
    if (GetPrimaryOpcode(nextInstr) == PrimaryOpcode::GROUP_19 && GetGroup19Opcode(nextInstr) == Opcode19::BCCTR) {
        return {currAddress+4, ctr};
    }
    return {currAddress+4};
}


void GDBServer::HandleCommand(const std::string& command_str) {
	auto context = std::make_unique<CommandContext>(this, command_str);

    if (context->IsValid()) {
        //cemuLog_logDebug(LogType::Force, "[GDBStub] Extracted Command {}", fmt::join(context->GetArgs(), ","));
    }

	switch (context->GetType()) {
	// Extended commands
	case CmdType::QUERY_GET:
	case CmdType::QUERY_SET:
		return HandleQuery(context);
	case CmdType::VCONT:
		return HandleVCont(context);
	// Regular commands
	case CmdType::IS_THREAD_RUNNING:
        break;//return CMD
	case CmdType::SET_ACTIVE_THREAD:
		return CMDSetActiveThread(context);
	case CmdType::ACTIVE_THREAD_STATUS:
		return CMDGetThreadStatus(context);
    case CmdType::CONTINUE:
        return CMDContinue(context);
	case CmdType::ACTIVE_THREAD_STEP:
		break;
	case CmdType::REGISTER_READ:
		return CMDReadRegister(context);
	case CmdType::REGISTER_SET:
        return CMDWriteRegister(context);
	case CmdType::REGISTERS_READ:
		return CMDReadRegisters(context);
	case CmdType::REGISTERS_WRITE:
		return CMDWriteRegisters(context);
	case CmdType::MEMORY_READ:
		return CMDReadMemory(context);
	case CmdType::MEMORY_WRITE:
        return CMDWriteMemory(context);
	case CmdType::BREAKPOINT_SET:
		return CMDInsertBreakpoint(context);
	case CmdType::BREAKPOINT_REMOVE:
		return CMDDeleteBreakpoint(context);
	case CmdType::INVALID:
	default:
		return CMDNotFound(context);
	}

    CMDNotFound(context);
}

void GDBServer::HandleQuery(std::unique_ptr<CommandContext>& context) const {
    if (!context->IsValid()) return context->QueueResponse(RESPONSE_EMPTY);

	const auto& query_cmd = context->GetArgs()[0];
    const auto& query_args = context->GetArgs().begin()+1;

    if (query_cmd == "qSupported") {
        context->QueueResponse(s_supportedFeatures);
    }
    else if (query_cmd == "qAttached") {
        context->QueueResponse("1");
    }
    else if (query_cmd == "qRcmd") {
    }
    else if (query_cmd == "qC") {
        context->QueueResponse("QC");
        context->QueueResponse(std::to_string(m_activeThreadContinueSelector));
    }
    else if (query_cmd == "qOffsets") {
        const auto module_count = RPLLoader_GetModuleCount();
        const auto module_list = RPLLoader_GetModuleList();
        for (sint32 i = 0; i < module_count; i++) {
            const RPLModule* rpl = module_list[i];
            if (rpl->entrypoint == m_entry_point) {
                context->QueueResponse(fmt::format("TextSeg={:08X};DataSeg={:08X}", rpl->regionMappingBase_text.GetMPTR(), rpl->regionMappingBase_data));
            }
        }
    }
    else if (query_cmd == "qfThreadInfo") {
        std::vector<std::string> threadIds;
        selectThread(-1, [&threadIds](OSThread_t* thread) {
            threadIds.emplace_back(fmt::format("{:08X}", memory_getVirtualOffsetFromPointer(thread)));
        });
        context->QueueResponse(fmt::format("m{}", fmt::join(threadIds, ",")));
    }
    else if (query_cmd == "qsThreadInfo") {
        context->QueueResponse("l");
    }
    else if (query_cmd == "qXfer") {
        auto& type = query_args[0];

        if (type == "features") {
            auto& annex = query_args[1];
            sint64 read_offset = std::stoul(query_args[2], nullptr, 16);
            sint64 read_length = std::stoul(query_args[3], nullptr, 16);
            if (annex == "target.xml") {
                if (read_offset >= GDBTargetXML.size()) context->QueueResponse("l");
                else {
                    auto paginated_str = GDBTargetXML.substr(read_offset, read_length);
                    context->QueueResponse((paginated_str.size() == read_length) ? "m" : "l");
                    context->QueueResponse(paginated_str);
                }
            }
            else cemuLog_logDebug(LogType::Force, "[GDBStub] qXfer:features:read:{} isn't a known feature document", annex);
        }
        else if (type == "threads") {
            sint64 read_offset = std::stoul(query_args[1], nullptr, 16);
            sint64 read_length = std::stoul(query_args[2], nullptr, 16);

            std::string threads_res;
            threads_res += R"(<?xml version="1.0"?>)";
            threads_res += "<threads>";
            // note: clion seems to default to the first thread
            std::map<sint64, std::string> threads_list;
            selectThread(-1, [&threads_list](OSThread_t *thread) {
                std::string entry;
                entry += fmt::format(R"(<thread id="{:x}" core="{}")", GET_THREAD_ID(thread), thread->context.affinity.value());
                if (!thread->threadName.IsNull())
                    entry += fmt::format(R"( name="{}")", CommandContext::EscapeXMLString(thread->threadName.GetPtr()));
                // todo: could add a human readable description of the thread here
                entry += fmt::format("></thread>");
                threads_list.emplace(GET_THREAD_ID(thread), entry);
            });
            for (auto& entry : threads_list) {
                threads_res += entry.second;
            }
            threads_res += "</threads>";

            if (read_offset >= threads_res.size()) context->QueueResponse("l");
            else {
                auto paginated_str = threads_res.substr(read_offset, read_length);
                context->QueueResponse((paginated_str.size() == read_length) ? "m" : "l");
                context->QueueResponse(paginated_str);
            }
        }
        else if (type == "libraries") {
            sint64 read_offset = std::stoul(query_args[1], nullptr, 16);
            sint64 read_length = std::stoul(query_args[2], nullptr, 16);

            std::string library_list;
            library_list += R"(<?xml version="1.0"?>)";
            library_list += "<library-list>";

            const auto module_count = RPLLoader_GetModuleCount();
            const auto module_list = RPLLoader_GetModuleList();
            for (sint32 i = 0; i < module_count; i++) {
                library_list += fmt::format(R"(<library name="{}"><segment address="{:#x}"/></library>)", CommandContext::EscapeXMLString(module_list[i]->moduleName2), module_list[i]->regionMappingBase_text.GetMPTR());
            }
            library_list += "</library-list>";

            if (read_offset >= library_list.size()) context->QueueResponse("l");
            else {
                auto paginated_str = library_list.substr(read_offset, read_length);
                context->QueueResponse((paginated_str.size() == read_length) ? "m" : "l");
                context->QueueResponse(paginated_str);
            }
        }
        else {
            context->QueueResponse(RESPONSE_EMPTY);
        }
    }
    else {
        context->QueueResponse(RESPONSE_EMPTY);
    }
}

void GDBServer::HandleVCont(std::unique_ptr<CommandContext>& context) {
    if (!context->IsValid()) {
        cemuLog_logDebug(LogType::Force, "[GDBStub] Received unsupported vCont command: {}", context->GetCommand());
        //cemu_assert_unimplemented();
        return context->QueueResponse(RESPONSE_EMPTY);
    }

	const std::string& vcont_cmd = context->GetArgs()[0];
    if (vcont_cmd == "vCont?") {
        return context->QueueResponse("vCont;c;C;s;S");
    }
    else if (vcont_cmd != "vCont;") {
        return context->QueueResponse(RESPONSE_EMPTY);
    }

    m_resumed_context = std::move(context);

    bool resumedNoThreads = true;
    for (const auto operationView : std::ranges::split_view(m_resumed_context->GetArgs()[1], ';')) {
        std::string_view operation{operationView.begin(), operationView.end()};

        // todo: this might have issues with the signal versions (C/S)
        // todo: test whether this works with multiple vCont;c:123123;c:123123
        std::string_view operationType = operation.substr(0, operation.find(':'));
        sint64 threadSelector = operationType.size() == operation.size() ? -1 : std::stoll(std::string(operation.substr(operationType.size()+1)), nullptr, 16);

        if (operationType == "c" || operationType.starts_with("C")) {
            selectAndResumeThread(threadSelector);
            resumedNoThreads = false;
        }
        else if (operationType == "s" || operationType.starts_with("S")) {
            selectThread(threadSelector, [](OSThread_t *thread) {
                auto nextInstructions = findNextInstruction(thread->context.srr0, thread->context.lr, thread->context.ctr);
                for (MPTR nextInstr : nextInstructions) {
                    if (auto bpIt = patchedInstructions.find(nextInstr); bpIt == patchedInstructions.end() || !bpIt->second.pauseThreads) {
                        insertBreakpoint(nextInstr, false, true, false, true);
                    }
                }
            });
        }
    }

    if (resumedNoThreads) {
        selectAndResumeThread(-1);
        cemuLog_logDebug(LogType::Force, "[GDBStub] Resumed all threads after skip instructions");
    }
}

void GDBServer::CMDContinue(std::unique_ptr<CommandContext>& context) const {
    selectAndResumeThread(m_activeThreadContinueSelector);
}


void GDBServer::CMDNotFound(std::unique_ptr<CommandContext>& context) {
	return context->QueueResponse(RESPONSE_EMPTY);
}

void GDBServer::CMDSetActiveThread(std::unique_ptr<CommandContext>& context) {
    sint64 threadSelector = std::stoll(context->GetArgs()[2], nullptr, 16);
    if (threadSelector >= 0) {
        bool foundThread = false;
        selectThread(threadSelector, [&foundThread](OSThread_t *thread) {
            foundThread = true;
        });
        if (!foundThread)
            return context->QueueResponse(RESPONSE_ERROR);
    }
    if (context->GetArgs()[1] == "c") m_activeThreadContinueSelector = threadSelector;
    else m_activeThreadSelector = threadSelector;
    context->QueueResponse(RESPONSE_OK);
}

void GDBServer::CMDGetThreadStatus(std::unique_ptr<CommandContext>& context) {
    selectThread(0, [&context](OSThread_t* thread) {
        context->QueueResponse(fmt::format("T05thread:{:08X};", memory_getVirtualOffsetFromPointer(thread)));
    });
}

void GDBServer::CMDReadRegister(std::unique_ptr<CommandContext>& context) const {
    sint32 reg = std::stoi(context->GetArgs()[1], nullptr, 16);
    selectThread(m_activeThreadSelector, [reg, &context](OSThread_t* thread) {
        auto& cpu = thread->context;
        if (reg < 32) {
            return context->QueueResponse(fmt::format("{:08X}", CPU_swapEndianU32(cpu.gpr[reg])));
        }
        else {
            switch (reg) {
                case RegisterID::PC: return context->QueueResponse(fmt::format("{:08X}", cpu.srr0));
                case RegisterID::MSR: return context->QueueResponse("xxxxxxxx"); // return context->QueueResponse(fmt::format("{:08X}", CPU_swapEndianU32(cpu.state)));
                case RegisterID::CR: return context->QueueResponse(fmt::format("{:08X}", cpu.cr));
                case RegisterID::LR: return context->QueueResponse(fmt::format("{:08X}", CPU_swapEndianU32(cpu.lr)));
                case RegisterID::CTR: return context->QueueResponse(fmt::format("{:08X}", cpu.ctr));
                case RegisterID::XER: return context->QueueResponse(fmt::format("{:08X}", cpu.xer));
                default: break;
            }
        }
    });
}

void GDBServer::CMDWriteRegister(std::unique_ptr<CommandContext>& context) const {
    sint32 reg = std::stoi(context->GetArgs()[1], nullptr, 16);
    uint32 value = std::stoi(context->GetArgs()[2], nullptr, 16);
    selectThread(m_activeThreadSelector, [reg, value, &context](OSThread_t* thread) {
        auto& cpu = thread->context;
        if (reg < 32) {
            cpu.gpr[reg] = CPU_swapEndianU32(value);
            return context->QueueResponse(RESPONSE_OK);
        }
        else {
            switch (reg) {
                case RegisterID::PC:
                    cpu.srr0 = value;
                    return context->QueueResponse(RESPONSE_OK);
                case RegisterID::MSR:
                    return context->QueueResponse(RESPONSE_ERROR); // return context->QueueResponse(fmt::format("{:08X}", CPU_swapEndianU32(cpu.state)));
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

void GDBServer::CMDReadRegisters(std::unique_ptr<CommandContext>& context) const {
    selectThread(m_activeThreadSelector, [&context](OSThread_t *thread) {
        for (uint32& reg : thread->context.gpr) {
            context->QueueResponse(fmt::format("{:08X}", CPU_swapEndianU32(reg)));
        }
    });
}

void GDBServer::CMDWriteRegisters(std::unique_ptr<CommandContext>& context) const {
    auto& registers = context->GetArgs()[1];
    selectThread(m_activeThreadSelector, [&registers, &context](OSThread_t *thread) {
        for (uint32 i=0; i<32; i++) {
            thread->context.gpr[i] = CPU_swapEndianU32(std::stoi(registers.substr(i*2, 2), nullptr, 16));
        }
    });
}

void GDBServer::CMDReadMemory(std::unique_ptr<CommandContext>& context) {
    sint64 addr = std::stoul(context->GetArgs()[1], nullptr, 16);
    sint64 length = std::stoul(context->GetArgs()[2], nullptr, 16);

    // todo: handle cross-mmu-range memory requests
    if (!memory_isAddressRangeAccessible(addr, length))
        return context->QueueResponse(RESPONSE_ERROR);

    std::string memoryRepr;
    uint8* values = memory_getPointerFromVirtualOffset(addr);
    for (sint64 i=0; i<length; i++) {
        memoryRepr += fmt::format("{:02X}", values[i]);
    }

    auto patchesRange = patchedInstructions.lower_bound(addr);
    while (patchesRange != patchedInstructions.end() && patchesRange->first < (addr+length)) {
        if (!patchesRange->second.visible) {
            auto replStr = fmt::format("{:02X}", patchesRange->second.origOpCode);
            memoryRepr[(patchesRange->first-addr)*2] = replStr[0];
            memoryRepr[(patchesRange->first-addr)*2+1] = replStr[1];
        }
        patchesRange++;
    }
    return context->QueueResponse(memoryRepr);
}

void GDBServer::CMDWriteMemory(std::unique_ptr<CommandContext>& context) {
    sint64 addr = std::stoul(context->GetArgs()[1], nullptr, 16);
    sint64 length = std::stoul(context->GetArgs()[2], nullptr, 16);
    auto source = context->GetArgs()[3];

    // todo: handle cross-mmu-range memory requests
    if (!memory_isAddressRangeAccessible(addr, length))
        return context->QueueResponse(RESPONSE_ERROR);

    uint8* values = memory_getPointerFromVirtualOffset(addr);
    for (sint64 i=0; i<length; i++) {
        uint8 hexValue;
        const std::from_chars_result result = std::from_chars(source.data() + (i*2), (source.data() + (i*2) + 2), hexValue, 16);
        if (result.ec == std::errc::invalid_argument || result.ec == std::errc::result_out_of_range)
            return context->QueueResponse(RESPONSE_ERROR);

        if (auto it = patchedInstructions.find(addr+i); it != patchedInstructions.end()) {
            uint32 byteIndex = 3-((addr+i)%4); // inverted because of big endian, so address 0 is the highest byte
            it->second.origOpCode &= ~(0xFF<<(byteIndex*8)); // mask out the byte
            it->second.origOpCode |= ((uint32)hexValue<<(byteIndex*8)); // set new byte with OR
        }
        else {
            values[i] = hexValue;
        }
    }
    return context->QueueResponse(RESPONSE_OK);
}

void GDBServer::CMDInsertBreakpoint(std::unique_ptr<CommandContext>& context) {
    auto type = std::stoul(context->GetArgs()[1], nullptr, 16);
    MPTR addr = static_cast<MPTR>(std::stoul(context->GetArgs()[2], nullptr, 16));

    if (!(type == 0 || type == 1))
        return context->QueueResponse(RESPONSE_EMPTY);

    insertBreakpoint(addr, type == 0, true, true, false);
    context->QueueResponse(RESPONSE_OK);
}

void GDBServer::CMDDeleteBreakpoint(std::unique_ptr<CommandContext>& context) {
    auto type = std::stoul(context->GetArgs()[1], nullptr, 16);
    MPTR addr = static_cast<MPTR>(std::stoul(context->GetArgs()[2], nullptr, 16));

    if (!(type == 0 || type == 1))
        return context->QueueResponse(RESPONSE_EMPTY);

    deleteBreakpoint(addr);
    context->QueueResponse(RESPONSE_OK);
}

// Internal functions for control
void GDBServer::HandleTrapInstruction(PPCInterpreter_t* hCPU) {
    // First, restore any removed breakpoints
    for (auto& bp : patchedInstructions) {
        if (bp.second.removedAfterInterrupt) {
            restoreBreakpoint(bp.first);
        }
    }

    // Find patched instruction that triggered trap
    auto patchedBP = patchedInstructions.find(hCPU->instructionPointer);
    if (patchedBP == patchedInstructions.end()) {
        cemu_assert_suspicious();
        return;
    }

    // Secondly, delete one-shot breakpoints but also temporarily delete patched instruction to run original instruction
    bool pauseThreads = patchedBP->second.pauseThreads;
    if (patchedBP->second.restoreAfterInterrupt) {
        // Insert new restore breakpoint at next possible instructions which restores breakpoints but won't pause the CPU
        std::vector<MPTR> nextInstructions = findNextInstruction(hCPU->instructionPointer, hCPU->spr.LR, hCPU->spr.CTR);
        for (MPTR nextInstr : nextInstructions) {
            if (!patchedInstructions.contains(nextInstr)) {
                insertBreakpoint(nextInstr, false, false, false, true);
            }
        }

        // Disable current BP breakpoint to run original instruction
        deleteBreakpoint(patchedBP->first, true);
    }
    else {
        deleteBreakpoint(patchedBP->first);
    }

    // Thirdly, delete any instructions that were generated by a skip instruction
    for (auto it = patchedInstructions.cbegin(), next_it = it; it != patchedInstructions.cend(); it = next_it) {
        ++next_it;
        if (it->second.deleteAfterInterrupt) {
            deleteBreakpoint(it->first);
        }
    }

    // Fourthly, the stub can insert breakpoints that are just meant to restore patched instructions, in which case we just want to continue
    if (pauseThreads) {
        cemuLog_logDebug(LogType::Force, "[GDBStub] Got trapped by a breakpoint!");
        if (m_resumed_context) {
            // Spin up thread to signal when another GDB stub trap is found
            ThreadPool::FireAndForget(&waitForBrokenThreads, std::move(m_resumed_context));
        }

        breakThreads(GET_THREAD_ID(coreinitThread_getCurrentThreadDepr(hCPU)));
        cemuLog_logDebug(LogType::Force, "[GDBStub] Resumed from a breakpoint!");
    }
}

void GDBServer::HandleEntryStop(uint32 entryAddress) {
    insertBreakpoint(entryAddress, false, true, true, false);
    m_entry_point = entryAddress;
}