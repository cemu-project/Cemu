#include "Common/precompiled.h"
#include "Debugger.h"
#include "Cafe/OS/RPL/rpl_structs.h"
#include "Cemu/PPCAssembler/ppcAssembler.h"
#include "Cafe/HW/Espresso/Recompiler/PPCRecompiler.h"
#include "Cemu/ExpressionParser/ExpressionParser.h"

#include "Cafe/OS/libs/coreinit/coreinit.h"
#include "OS/RPL/rpl.h"
#include "util/helpers/helpers.h"

#if BOOST_OS_WINDOWS
#include <Windows.h>
#endif

void debugger_updateExecutionBreakpoint(uint32 address, bool forceRestore = false);

DebuggerDispatcher g_debuggerDispatcher;

struct DebuggerState
{
	bool breakOnEntry{false};
	bool logOnlyMemoryBreakpoints{false};
	// breakpoints
	std::recursive_mutex breakpointsMtx;
	std::vector<DebuggerBreakpoint*> breakpoints;
	// patches
	std::vector<DebuggerPatch*> patches;
	// active memory breakpoint (only one for now)
	DebuggerBreakpoint* activeMemoryBreakpoint{};
	// debugging session
	struct
	{
		std::mutex debugSessionMtx;
		std::atomic_bool shouldBreak; // request any thread to break asap
		std::atomic_bool isTrapped{false};
		uint32 debuggedThreadMPTR{};
		PPCInterpreter_t* hCPU{};
		// step control
		std::atomic<DebuggerStepCommand> stepCommand{DebuggerStepCommand::None};
	}debugSession;
};

DebuggerState s_debuggerState{};

std::vector<DebuggerBreakpoint*>& debugger_lockBreakpoints()
{
	s_debuggerState.breakpointsMtx.lock();
	return s_debuggerState.breakpoints;
}

void debugger_unlockBreakpoints()
{
	s_debuggerState.breakpointsMtx.unlock();
}

DebuggerBreakpoint* debugger_getFirstBP(uint32 address)
{
	for (auto& it : s_debuggerState.breakpoints)
	{
		if (it->address == address)
			return it;
	}
	return nullptr;
}

DebuggerBreakpoint* debugger_getFirstBP(uint32 address, uint8 bpType)
{
	for (auto& it : s_debuggerState.breakpoints)
	{
		if (it->address == address)
		{
			DebuggerBreakpoint* bpItr = it;
			while (bpItr)
			{
				if (bpItr->bpType == bpType)
					return bpItr;
				bpItr = bpItr->next;
			}
			return nullptr;
		}
	}
	return nullptr;
}

DebuggerBreakpoint* debugger_getBreakpointById(BreakpointId bpId)
{
	for (auto& it : s_debuggerState.breakpoints)
	{
		DebuggerBreakpoint* chain = it;
		while (chain)
		{
			if (chain->id == bpId)
				return chain;
			chain = chain->next;
		}
	}
	return nullptr;
}

bool debuggerBPChain_hasType(DebuggerBreakpoint* bp, uint8 bpType)
{
	while (bp)
	{
		if (bp->bpType == bpType)
			return true;
		bp = bp->next;
	}
	return false;
}

void debuggerBPChain_add(uint32 address, DebuggerBreakpoint* bp)
{
	bp->next = nullptr;
	DebuggerBreakpoint* existingBP = debugger_getFirstBP(address);
	if (existingBP)
	{
		while (existingBP->next)
			existingBP = existingBP->next;
		existingBP->next = bp;
		return;
	}
	// no breakpoint chain exists for this address
	s_debuggerState.breakpoints.push_back(bp);
}

uint32 debugger_getAddressOriginalOpcode(uint32 address)
{
	auto bpItr = debugger_getFirstBP(address);
	while (bpItr)
	{
		if (bpItr->isExecuteBP())
			return bpItr->originalOpcodeValue;
		bpItr = bpItr->next;
	}
	return memory_readU32(address);
}

void debugger_updateMemoryU32(uint32 address, uint32 newValue)
{
	bool memChanged = false;
	if (newValue != memory_readU32(address))
		memChanged = true;
	memory_writeU32(address, newValue);
	if(memChanged)
		PPCRecompiler_invalidateRange(address, address + 4);
}

void debugger_updateExecutionBreakpoint(uint32 address, bool forceRestore)
{
	auto bpItr = debugger_getFirstBP(address);
	bool hasBP = false;
	uint32 originalOpcodeValue;
	while (bpItr)
	{
		if (bpItr->isExecuteBP())
		{
			if (bpItr->enabled && forceRestore == false)
			{
				// write TW instruction to memory
				debugger_updateMemoryU32(address, DEBUGGER_BP_T_DEBUGGER_TW);
				return;
			}
			else
			{
				originalOpcodeValue = bpItr->originalOpcodeValue;
				hasBP = true;
			}
		}
		bpItr = bpItr->next;
	}
	if (hasBP)
	{
		// restore instruction
		debugger_updateMemoryU32(address, originalOpcodeValue);
	}
}

void debugger_createCodeBreakpoint(uint32 address, uint8 bpType)
{
	// check if breakpoint already exists
	auto existingBP = debugger_getFirstBP(address);
	if (existingBP && debuggerBPChain_hasType(existingBP, bpType))
		return; // breakpoint already exists
	// get original opcode at address
	uint32 originalOpcode = debugger_getAddressOriginalOpcode(address);
	// init breakpoint object
	DebuggerBreakpoint* bp = new DebuggerBreakpoint(address, originalOpcode, bpType, true);
	debuggerBPChain_add(address, bp);
	debugger_updateExecutionBreakpoint(address);
}

namespace coreinit
{
	std::vector<std::thread::native_handle_type>& OSGetSchedulerThreads();
}

void debugger_updateMemoryBreakpoint(DebuggerBreakpoint* bp)
{
	std::vector<std::thread::native_handle_type> schedulerThreadHandles = coreinit::OSGetSchedulerThreads();

#if BOOST_OS_WINDOWS
	s_debuggerState.activeMemoryBreakpoint = bp;
	for (auto& hThreadNH : schedulerThreadHandles)
	{
		HANDLE hThread = (HANDLE)hThreadNH;
		CONTEXT ctx{};
		ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
		SuspendThread(hThread);
		GetThreadContext(hThread, &ctx);
		if (s_debuggerState.activeMemoryBreakpoint)
		{
			ctx.Dr0 = (DWORD64)memory_getPointerFromVirtualOffset(bp->address);
			ctx.Dr1 = (DWORD64)memory_getPointerFromVirtualOffset(bp->address);
			// breakpoint 0
			SetBits(ctx.Dr7, 0, 1, 1);  // breakpoint #0 enabled: true
			SetBits(ctx.Dr7, 16, 2, 1); // breakpoint #0 condition: 1 (write)
			SetBits(ctx.Dr7, 18, 2, 3); // breakpoint #0 length: 3 (4 bytes)
			// breakpoint 1
			SetBits(ctx.Dr7, 2, 1, 1);  // breakpoint #1 enabled: true
			SetBits(ctx.Dr7, 20, 2, 3); // breakpoint #1 condition: 3 (read & write)
			SetBits(ctx.Dr7, 22, 2, 3); // breakpoint #1 length: 3 (4 bytes)
		}
		else
		{
			// breakpoint 0
			SetBits(ctx.Dr7, 0, 1, 0);  // breakpoint #0 enabled: false
			SetBits(ctx.Dr7, 16, 2, 0); // breakpoint #0 condition: 1 (write)
			SetBits(ctx.Dr7, 18, 2, 0); // breakpoint #0 length: 3 (4 bytes)
			// breakpoint 1
			SetBits(ctx.Dr7, 2, 1, 0);  // breakpoint #1 enabled: false
			SetBits(ctx.Dr7, 20, 2, 0); // breakpoint #1 condition: 3 (read & write)
			SetBits(ctx.Dr7, 22, 2, 0); // breakpoint #1 length: 3 (4 bytes)
		}
		SetThreadContext(hThread, &ctx);
		ResumeThread(hThread);
	}
	#else
	cemuLog_log(LogType::Force, "Debugger breakpoints are not supported");
	#endif
}

void debugger_handleSingleStepException(uint64 dr6)
{
	bool triggeredDR0 = GetBits(dr6, 0, 1); // write
	bool triggeredDR1 = GetBits(dr6, 1, 1); // read and write
	bool catchBP = false;
	if (triggeredDR0 && triggeredDR1)
	{
		// write (and read) access
		if (s_debuggerState.activeMemoryBreakpoint && s_debuggerState.activeMemoryBreakpoint->bpType == DEBUGGER_BP_T_MEMORY_WRITE)
			catchBP = true;
	}
	else
	{
		// read access
		if (s_debuggerState.activeMemoryBreakpoint && s_debuggerState.activeMemoryBreakpoint->bpType == DEBUGGER_BP_T_MEMORY_READ)
			catchBP = true;
	}
	if (catchBP)
	{
		PPCInterpreter_t* hCPU = PPCInterpreter_getCurrentInstance();
		if (s_debuggerState.logOnlyMemoryBreakpoints)
		{
			float memValueF = memory_readFloat(s_debuggerState.activeMemoryBreakpoint->address);
			uint32 memValue = memory_readU32(s_debuggerState.activeMemoryBreakpoint->address);
			cemuLog_log(LogType::Force, "[Debugger] 0x{:08X} was read/written! New Value: 0x{:08X} (float {}) IP: {:08X} LR: {:08X}",
				s_debuggerState.activeMemoryBreakpoint->address,
				memValue,
				memValueF,
				hCPU->instructionPointer,
				hCPU->spr.LR
			);
			if (cemuLog_advancedPPCLoggingEnabled())
				DebugLogStackTrace(coreinit::OSGetCurrentThread(), hCPU->gpr[1]);
		}
		else
		{
			debugger_createCodeBreakpoint(hCPU->instructionPointer + 4, DEBUGGER_BP_T_ONE_SHOT);
		}
	}
}

void debugger_createMemoryBreakpoint(uint32 address, bool onRead, bool onWrite)
{
	// init breakpoint object
	uint8 bpType;
	if (onRead && onWrite)
		assert_dbg();
	else if (onRead)
		bpType = DEBUGGER_BP_T_MEMORY_READ;
	else
		bpType = DEBUGGER_BP_T_MEMORY_WRITE;

	DebuggerBreakpoint* bp = new DebuggerBreakpoint(address, 0xFFFFFFFF, bpType, true);
	debuggerBPChain_add(address, bp);
	// disable any already existing memory breakpoint
	if (s_debuggerState.activeMemoryBreakpoint)
	{
		s_debuggerState.activeMemoryBreakpoint->enabled = false;
		s_debuggerState.activeMemoryBreakpoint = nullptr;
	}
	// activate new breakpoint
	debugger_updateMemoryBreakpoint(bp);
}

void debugger_handleEntryBreakpoint(uint32 address)
{
	if (!s_debuggerState.breakOnEntry)
		return;

	debugger_createCodeBreakpoint(address, DEBUGGER_BP_T_NORMAL);
}

void debugger_deleteBreakpoint(BreakpointId bpId)
{
	std::unique_lock _l(s_debuggerState.breakpointsMtx);
	DebuggerBreakpoint* bp = debugger_getBreakpointById(bpId);
	if (!bp)
		return;
	for (auto& it : s_debuggerState.breakpoints)
	{
		if (it->address == bp->address)
		{
			// for execution breakpoints make sure the instruction is restored
			if (it->isExecuteBP())
			{
				it->enabled = false;
				debugger_updateExecutionBreakpoint(it->address);
			}
			// remove
			if (it == bp)
			{
				// remove first in list
				s_debuggerState.breakpoints.erase(std::remove(s_debuggerState.breakpoints.begin(), s_debuggerState.breakpoints.end(), bp), s_debuggerState.breakpoints.end());
				DebuggerBreakpoint* nextBP = bp->next;
				if (nextBP)
					s_debuggerState.breakpoints.push_back(nextBP);
			}
			else
			{
				// remove from list
				DebuggerBreakpoint* bpItr = it;
				while (bpItr->next != bp)
					bpItr = bpItr->next;
				cemu_assert_debug(bpItr->next == bp);
				bpItr->next = bp->next;
			}
			delete bp;
			return;
		}
	}
}

void debugger_toggleExecuteBreakpoint(uint32 address)
{
	std::unique_lock _l(s_debuggerState.breakpointsMtx);
	auto existingBP = debugger_getFirstBP(address, DEBUGGER_BP_T_NORMAL);
	if (existingBP)
	{
		// delete existing breakpoint
		debugger_deleteBreakpoint(existingBP->id);
	}
	else
	{
		// create new execution breakpoint
		debugger_createCodeBreakpoint(address, DEBUGGER_BP_T_NORMAL);
	}
}

void debugger_toggleLoggingBreakpoint(uint32 address)
{
	std::unique_lock _l(s_debuggerState.breakpointsMtx);
	auto existingBP = debugger_getFirstBP(address, DEBUGGER_BP_T_LOGGING);
	if (existingBP)
	{
		// delete existing breakpoint
		debugger_deleteBreakpoint(existingBP->id);
	}
	else
	{
		// create new logging breakpoint
		debugger_createCodeBreakpoint(address, DEBUGGER_BP_T_LOGGING);
	}
}

bool debugger_isTrapped()
{
	return s_debuggerState.debugSession.isTrapped;
}

PPCInterpreter_t* debugger_lockDebugSession()
{
	s_debuggerState.debugSession.debugSessionMtx.lock();
	if (!s_debuggerState.debugSession.isTrapped)
	{
		s_debuggerState.debugSession.debugSessionMtx.unlock();
		return nullptr;
	}
	return s_debuggerState.debugSession.hCPU;
}

void debugger_unlockDebugSession(PPCInterpreter_t* hCPU)
{
	cemu_assert_debug(s_debuggerState.debugSession.isTrapped);
	cemu_assert_debug(s_debuggerState.debugSession.hCPU == hCPU);
	s_debuggerState.debugSession.debugSessionMtx.unlock();
}

PPCSnapshot debugger_getSnapshotFromSession(PPCInterpreter_t* hCPU)
{
	cemu_assert_debug(s_debuggerState.debugSession.isTrapped);
	cemu_assert_debug(s_debuggerState.debugSession.hCPU == hCPU);
	PPCSnapshot snapshot{};
	memcpy(snapshot.gpr, hCPU->gpr, sizeof(uint32) * 32);
	memcpy(snapshot.fpr, hCPU->fpr, sizeof(FPR_t) * 32);
	snapshot.spr_lr = hCPU->spr.LR;
	for (uint32 i = 0; i < 32; i++)
		snapshot.cr[i] = hCPU->cr[i];
	return snapshot;
}

void debugger_resume()
{
	s_debuggerState.debugSession.stepCommand = DebuggerStepCommand::Run;
}

void debugger_stepCommand(DebuggerStepCommand stepCommand)
{
	s_debuggerState.debugSession.stepCommand = stepCommand;
}

void debugger_requestBreak()
{
	s_debuggerState.debugSession.shouldBreak = true;
}

void debugger_toggleBreakpoint(uint32 address, bool state, DebuggerBreakpoint* bp)
{
	DebuggerBreakpoint* bpItr = debugger_getFirstBP(address);
	while (bpItr)
	{
		if (bpItr == bp)
		{
			if (bp->enabled == state)
				return;
			if (bpItr->bpType == DEBUGGER_BP_T_NORMAL || bpItr->bpType == DEBUGGER_BP_T_LOGGING)
			{
				bp->enabled = state;
				debugger_updateExecutionBreakpoint(address);
				g_debuggerDispatcher.UpdateViewThreadsafe();
			}
			else if (bpItr->isMemBP())
			{
				// disable other memory breakpoints
				for (auto& it : s_debuggerState.breakpoints)
				{
					DebuggerBreakpoint* bpItr2 = it;
					while (bpItr2)
					{
						if (bpItr2->isMemBP() && bpItr2 != bp)
						{
							bpItr2->enabled = false;
						}
						bpItr2 = bpItr2->next;
					}
				}
				bpItr->enabled = state;
				if (state)
					debugger_updateMemoryBreakpoint(bpItr);
				else
					debugger_updateMemoryBreakpoint(nullptr);
				g_debuggerDispatcher.UpdateViewThreadsafe();
			}
			return;
		}
		bpItr = bpItr->next;
	}
}

void debugger_createPatch(uint32 address, std::span<uint8> patchData)
{
	DebuggerPatch* patch = new DebuggerPatch();
	patch->address = address;
	patch->length = patchData.size();
	patch->data.resize(4);
	patch->origData.resize(4);
	memcpy(&patch->data.front(), patchData.data(), patchData.size());
	memcpy(&patch->origData.front(), memory_getPointerFromVirtualOffset(address), patchData.size());
	// get original data from breakpoints
	if ((address & 3) != 0)
		cemu_assert_debug(false);
	for (sint32 i = 0; i < patchData.size() / 4; i++)
	{
		DebuggerBreakpoint* bpItr = debugger_getFirstBP(address);
		while (bpItr)
		{
			if (bpItr->isExecuteBP())
			{
				*(uint32*)(&patch->origData.front() + i * 4) = _swapEndianU32(bpItr->originalOpcodeValue);
			}
			bpItr = bpItr->next;
		}
	}
	// merge with existing patches if the ranges touch
	for(sint32 i=0; i<s_debuggerState.patches.size(); i++)
	{
		auto& patchItr = s_debuggerState.patches[i];
		if (address + patchData.size() >= patchItr->address && address <= patchItr->address + patchItr->length)
		{
			uint32 newAddress = std::min(patch->address, patchItr->address);
			uint32 newEndAddress = std::max(patch->address + patch->length, patchItr->address + patchItr->length);
			uint32 newLength = newEndAddress - newAddress;

			DebuggerPatch* newPatch = new DebuggerPatch();
			newPatch->address = newAddress;
			newPatch->length = newLength;
			newPatch->data.resize(newLength);
			newPatch->origData.resize(newLength);
			memcpy(&newPatch->data.front() + (address - newAddress), &patch->data.front(), patch->length);
			memcpy(&newPatch->data.front() + (patchItr->address - newAddress), &patchItr->data.front(), patchItr->length);

			memcpy(&newPatch->origData.front() + (address - newAddress), &patch->origData.front(), patch->length);
			memcpy(&newPatch->origData.front() + (patchItr->address - newAddress), &patchItr->origData.front(), patchItr->length);

			delete patch;
			patch = newPatch;
			delete patchItr;
			// remove currently iterated patch
			s_debuggerState.patches.erase(s_debuggerState.patches.begin()+i);
			i--;
		}
	}
	s_debuggerState.patches.push_back(patch);
	// apply patch (if breakpoints exist then update those instead of actual data)
	if ((address & 3) != 0)
		cemu_assert_debug(false);
	if ((patchData.size() & 3) != 0)
		cemu_assert_debug(false);
	for (sint32 i = 0; i < patchData.size() / 4; i++)
	{
		DebuggerBreakpoint* bpItr = debugger_getFirstBP(address);
		bool hasActiveExecuteBP = false;
		while (bpItr)
		{
			if (bpItr->isExecuteBP())
			{
				bpItr->originalOpcodeValue = *(uint32be*)(patchData.data() + i * 4);
				if (bpItr->enabled)
					hasActiveExecuteBP = true;
			}
			bpItr = bpItr->next;
		}
		if (hasActiveExecuteBP == false)
		{
			memcpy(memory_getPointerFromVirtualOffset(address + i * 4), patchData.data() + i * 4, 4);
			PPCRecompiler_invalidateRange(address, address + 4);
		}
	}
}

bool debugger_hasPatch(uint32 address)
{
	for (auto& patch : s_debuggerState.patches)
	{
		if (address + 4 > patch->address && address < patch->address + patch->length)
			return true;
	}
	return false;
}

void debugger_removePatch(uint32 address)
{
	for (sint32 i = 0; i < s_debuggerState.patches.size(); i++)
	{
		auto& patch = s_debuggerState.patches[i];
		if (address < patch->address || address >= (patch->address + patch->length))
			continue;
		MPTR startAddress = patch->address;
		MPTR endAddress = patch->address + patch->length;
		// remove any breakpoints overlapping with the patch
		for (auto& bp : s_debuggerState.breakpoints)
		{
			if (bp->address + 4 > startAddress && bp->address < endAddress)
			{
				bp->enabled = false;
				debugger_updateExecutionBreakpoint(bp->address);
			}
		}
		// restore original data
		memcpy(MEMPTR<void>(startAddress).GetPtr(), patch->origData.data(), patch->length);
		PPCRecompiler_invalidateRange(startAddress, endAddress);
		// remove patch
		delete patch;
		s_debuggerState.patches.erase(s_debuggerState.patches.begin() + i);
		return;
	}
}

bool debugger_CanStepOverInstruction(MPTR address)
{
	PPCDisassembledInstruction disasmInstr = { 0 };
	ppcAssembler_disassemble(address, debugger_getAddressOriginalOpcode(address), &disasmInstr);
	return disasmInstr.ppcAsmCode == PPCASM_OP_BL || disasmInstr.ppcAsmCode == PPCASM_OP_BLA || disasmInstr.ppcAsmCode == PPCASM_OP_BCTRL;
}

void debugger_handleLoggingBreakpoint(PPCInterpreter_t* hCPU, DebuggerBreakpoint* bp)
{
	std::string comment = !bp->comment.empty() ? boost::nowide::narrow(bp->comment) : fmt::format("Breakpoint at 0x{:08X} (no comment)", bp->address);

	auto replacePlaceholders = [&](const std::string& prefix, const auto& formatFunc)
	{
		size_t pos = 0;
		while ((pos = comment.find(prefix, pos)) != std::string::npos)
		{
			size_t endPos = comment.find('}', pos);
			if (endPos == std::string::npos)
				break;

			try
			{
				if (int regNum = ConvertString<int>(comment.substr(pos + prefix.length(), endPos - pos - prefix.length())); regNum >= 0 && regNum < 32)
				{
					std::string replacement = formatFunc(regNum);
					comment.replace(pos, endPos - pos + 1, replacement);
					pos += replacement.length();
				}
				else
				{
					pos = endPos + 1;
				}
			}
			catch (...)
			{
				pos = endPos + 1;
			}
		}
	};

	// Replace integer register placeholders {rX}
	replacePlaceholders("{r", [&](int regNum) {
		return fmt::format("0x{:08X}", hCPU->gpr[regNum]);
	});

	// Replace floating point register placeholders {fX}
	replacePlaceholders("{f", [&](int regNum) {
		return fmt::format("{}", hCPU->fpr[regNum].fpr);
	});

	std::string logName = "Breakpoint '" + comment + "'";
	std::string logContext = fmt::format("Thread: {:08x} LR: 0x{:08x}", MEMPTR<OSThread_t>(coreinit::OSGetCurrentThread()).GetMPTR(), hCPU->spr.LR, cemuLog_advancedPPCLoggingEnabled() ? " Stack Trace:" : "");
	cemuLog_log(LogType::Force, "[Debugger] {} was executed! {}", logName, logContext);
	if (cemuLog_advancedPPCLoggingEnabled())
		DebugLogStackTrace(coreinit::OSGetCurrentThread(), hCPU->gpr[1]);
}

// used to escape the breakpoint on the current instruction
void debugger_stepOverCurrentBreakpoint(PPCInterpreter_t* hCPU)
{
	std::unique_lock _l(s_debuggerState.breakpointsMtx);
	PPCRecompiler_Enable();
	MPTR bpAddress = hCPU->instructionPointer;
	debugger_updateExecutionBreakpoint(bpAddress, true);
	PPCInterpreterSlim_executeInstruction(hCPU);
	debugger_updateExecutionBreakpoint(bpAddress);
	PPCRecompiler_Disable();
}

void debugger_enterTW(PPCInterpreter_t* hCPU, bool isSingleStep)
{
	s_debuggerState.debugSession.debugSessionMtx.lock();
	while ( true )
	{
		bool trappedExpectedVal = false;
		if ( s_debuggerState.debugSession.isTrapped.compare_exchange_strong(trappedExpectedVal, true) )
			break;
		s_debuggerState.debugSession.debugSessionMtx.unlock();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		s_debuggerState.debugSession.debugSessionMtx.lock();
	}
	// init debug state
	s_debuggerState.debugSession.debuggedThreadMPTR = MEMPTR<OSThread_t>(coreinit::OSGetCurrentThread()).GetMPTR();
	s_debuggerState.debugSession.hCPU = hCPU;
	s_debuggerState.debugSession.stepCommand = DebuggerStepCommand::None;
	s_debuggerState.debugSession.debugSessionMtx.unlock();
	// handle logging points
	DebuggerBreakpoint* bp = debugger_getFirstBP(hCPU->instructionPointer);
	bool shouldBreak = debuggerBPChain_hasType(bp, DEBUGGER_BP_T_NORMAL) || debuggerBPChain_hasType(bp, DEBUGGER_BP_T_ONE_SHOT);
	while (bp)
	{
		if (bp->bpType == DEBUGGER_BP_T_LOGGING && bp->enabled)
			debugger_handleLoggingBreakpoint(hCPU, bp);
		bp = bp->next;
	}
	// for logging breakpoints skip the waiting and notifying of the UI
	DebuggerStepCommand stepCommand = DebuggerStepCommand::Run;
	if (shouldBreak || isSingleStep)
	{
		// remove one-shot breakpoint if it exists
		DebuggerBreakpoint* singleshotBP = debugger_getFirstBP(hCPU->instructionPointer, DEBUGGER_BP_T_ONE_SHOT);
		if (singleshotBP)
			debugger_deleteBreakpoint(singleshotBP->id);
		g_debuggerDispatcher.NotifyDebugBreakpointHit();
		g_debuggerDispatcher.UpdateViewThreadsafe();
		while (s_debuggerState.debugSession.isTrapped)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			// check for step commands from UI
			stepCommand = s_debuggerState.debugSession.stepCommand.exchange(DebuggerStepCommand::None);
			if (stepCommand != DebuggerStepCommand::None)
				break;
			if (!coreinit::OSIsSchedulerActive())
			{
				// quit if scheduler is being shutdown
				stepCommand = DebuggerStepCommand::Run;
				break;
			}
		}
		if (stepCommand == DebuggerStepCommand::StepOver && !debugger_CanStepOverInstruction(hCPU->instructionPointer))
		{
			stepCommand = DebuggerStepCommand::StepInto;
		}
		g_debuggerDispatcher.UpdateViewThreadsafe();
		g_debuggerDispatcher.NotifyRun();
	}
	s_debuggerState.debugSession.debugSessionMtx.lock();
	s_debuggerState.debugSession.hCPU = nullptr;
	s_debuggerState.debugSession.isTrapped = false;
	s_debuggerState.debugSession.debugSessionMtx.unlock();
	// leave the breakpointed instruction
	MPTR initialIP = hCPU->instructionPointer;
	debugger_stepOverCurrentBreakpoint(hCPU);

	// todo - if the current instruction is a HLE gateway, then we need to handle it with special logic (instead of generic debugger_stepOverCurrentBreakpoint):
	// First grab the HLE index from the original opcode and look up the function pointer
	// And then call the HLE func (must happen last, since it may not return immediately or ever)

	if (stepCommand == DebuggerStepCommand::StepInto)
	{
		debugger_enterTW(hCPU, true);
	}
	else if (stepCommand == DebuggerStepCommand::StepOver)
	{
		debugger_createCodeBreakpoint(initialIP + 4, DEBUGGER_BP_T_ONE_SHOT);
	}
}

void debugger_addParserSymbols(class ExpressionParser& ep)
{
	const auto moduleCount = RPLLoader_GetModuleCount();
	const auto moduleList = RPLLoader_GetModuleList();
	std::vector<double> moduleTmp(moduleCount);
	for (sint32 i = 0; i < moduleCount; i++)
	{
		const auto module = moduleList[i];
		if (module)
		{
			moduleTmp[i] = (double)module->regionMappingBase_text.GetMPTR();
			ep.AddConstant(module->moduleName2, moduleTmp[i]);
		}
	}
	PPCInterpreter_t* hCPU = debugger_lockDebugSession();
	if (hCPU)
	{
		PPCSnapshot snapshot = debugger_getSnapshotFromSession(hCPU);
		for (sint32 i = 0; i < 32; i++)
			ep.AddConstant(fmt::format("r{}", i), snapshot.gpr[i]);
		debugger_unlockDebugSession(hCPU);
	}
}

void debugger_jumpToAddressInDisasm(MPTR address)
{
	g_debuggerDispatcher.MoveToAddressInDisassembly(address);
}

void debugger_setOptionBreakOnEntry(bool isEnabled)
{
	s_debuggerState.breakOnEntry = isEnabled;
}

void debugger_setOptionLogOnlyMemoryBreakpoints(bool isEnabled)
{
	s_debuggerState.logOnlyMemoryBreakpoints = isEnabled;
}

bool debugger_getOptionBreakOnEntry()
{
	return s_debuggerState.breakOnEntry;
}

bool debugger_getOptionLogOnlyMemoryBreakpoints()
{
	return s_debuggerState.logOnlyMemoryBreakpoints;
}
