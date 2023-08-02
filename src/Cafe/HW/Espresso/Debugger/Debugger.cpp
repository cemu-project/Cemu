#include "gui/guiWrapper.h"
#include "Debugger.h"
#include "Cafe/OS/RPL/rpl_structs.h"
#include "Cemu/PPCAssembler/ppcAssembler.h"
#include "Cafe/HW/Espresso/Recompiler/PPCRecompiler.h"
#include "Cemu/ExpressionParser/ExpressionParser.h"

#include "gui/debugger/DebuggerWindow2.h"

#include "Cafe/OS/libs/coreinit/coreinit.h"

#if BOOST_OS_WINDOWS
#include <Windows.h>
#endif

debuggerState_t debuggerState{ };

DebuggerBreakpoint* debugger_getFirstBP(uint32 address)
{
	for (auto& it : debuggerState.breakpoints)
	{
		if (it->address == address)
			return it;
	}
	return nullptr;
}

DebuggerBreakpoint* debugger_getFirstBP(uint32 address, uint8 bpType)
{
	for (auto& it : debuggerState.breakpoints)
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
	debuggerState.breakpoints.push_back(bp);
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

void debugger_createExecuteBreakpoint(uint32 address)
{
	debugger_createCodeBreakpoint(address, DEBUGGER_BP_T_NORMAL);
}

namespace coreinit
{
	std::vector<std::thread::native_handle_type>& OSGetSchedulerThreads();
}

void debugger_updateMemoryBreakpoint(DebuggerBreakpoint* bp)
{
	std::vector<std::thread::native_handle_type> schedulerThreadHandles = coreinit::OSGetSchedulerThreads();

#if BOOST_OS_WINDOWS
	debuggerState.activeMemoryBreakpoint = bp;
	for (auto& hThreadNH : schedulerThreadHandles)
	{
		HANDLE hThread = (HANDLE)hThreadNH;
		CONTEXT ctx{};
		ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
		SuspendThread(hThread);
		GetThreadContext(hThread, &ctx);
		if (debuggerState.activeMemoryBreakpoint)
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
		if (debuggerState.activeMemoryBreakpoint && debuggerState.activeMemoryBreakpoint->bpType == DEBUGGER_BP_T_MEMORY_WRITE)
			catchBP = true;
	}
	else
	{
		// read access
		if (debuggerState.activeMemoryBreakpoint && debuggerState.activeMemoryBreakpoint->bpType == DEBUGGER_BP_T_MEMORY_READ)
			catchBP = true;
	}
	if (catchBP)
	{
		debugger_createCodeBreakpoint(ppcInterpreterCurrentInstance->instructionPointer + 4, DEBUGGER_BP_T_ONE_SHOT);
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
	if (debuggerState.activeMemoryBreakpoint)
	{
		debuggerState.activeMemoryBreakpoint->enabled = false;
		debuggerState.activeMemoryBreakpoint = nullptr;
	}
	// activate new breakpoint
	debugger_updateMemoryBreakpoint(bp);
}

void debugger_handleEntryBreakpoint(uint32 address)
{
	if (!debuggerState.breakOnEntry)
		return;

	debugger_createCodeBreakpoint(address, DEBUGGER_BP_T_NORMAL);
}

void debugger_deleteBreakpoint(DebuggerBreakpoint* bp)
{
	for (auto& it : debuggerState.breakpoints)
	{
		if (it->address == bp->address)
		{
			// for execution breakpoints make sure the instruction is restored
			if (bp->isExecuteBP())
			{
				bp->enabled = false;
				debugger_updateExecutionBreakpoint(bp->address);
			}
			// remove
			if (it == bp)
			{
				// remove first in list
				debuggerState.breakpoints.erase(std::remove(debuggerState.breakpoints.begin(), debuggerState.breakpoints.end(), bp), debuggerState.breakpoints.end());
				DebuggerBreakpoint* nextBP = bp->next;
				if (nextBP)
					debuggerState.breakpoints.push_back(nextBP);
			}
			else
			{
				// remove from list
				DebuggerBreakpoint* bpItr = it;
				while (bpItr->next != bp)
				{
					bpItr = bpItr->next;
				}
				cemu_assert_debug(bpItr->next != bp);
				bpItr->next = bp->next;
			}
			delete bp;
			return;
		}
	}
}

void debugger_toggleExecuteBreakpoint(uint32 address)
{
	auto existingBP = debugger_getFirstBP(address, DEBUGGER_BP_T_NORMAL);
	if (existingBP)
	{ 
		// delete existing breakpoint
		debugger_deleteBreakpoint(existingBP);
	}
	else
	{
		// create new breakpoint
		debugger_createExecuteBreakpoint(address);
	}
}

void debugger_forceBreak()
{
	debuggerState.debugSession.shouldBreak = true;
}

bool debugger_isTrapped()
{
	return debuggerState.debugSession.isTrapped;
}

void debugger_resume()
{
	// if there is a breakpoint on the current instruction then do a single 'step into' to skip it
	debuggerState.debugSession.run = true;
}

void debugger_toggleBreakpoint(uint32 address, bool state, DebuggerBreakpoint* bp)
{
	DebuggerBreakpoint* bpItr = debugger_getFirstBP(address);
	while (bpItr)
	{
		if (bpItr == bp)
		{
			if (bpItr->bpType == DEBUGGER_BP_T_NORMAL || bpItr->bpType == DEBUGGER_BP_T_LOGGING)
			{
				bp->enabled = state;
				debugger_updateExecutionBreakpoint(address);
				debuggerWindow_updateViewThreadsafe2();
			}
			else if (bpItr->isMemBP())
			{
				// disable other memory breakpoints
				for (auto& it : debuggerState.breakpoints)
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
				debuggerWindow_updateViewThreadsafe2();
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
	for(sint32 i=0; i<debuggerState.patches.size(); i++)
	{
		auto& patchItr = debuggerState.patches[i];
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
			debuggerState.patches.erase(debuggerState.patches.begin()+i);
			i--;
		}
	}
	debuggerState.patches.push_back(patch);
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
	for (auto& patch : debuggerState.patches)
	{
		if (address + 4 > patch->address && address < patch->address + patch->length)
			return true;
	}
	return false;
}

void debugger_stepInto(PPCInterpreter_t* hCPU, bool updateDebuggerWindow = true)
{
	bool isRecEnabled = ppcRecompilerEnabled;
	ppcRecompilerEnabled = false;
	uint32 initialIP = debuggerState.debugSession.instructionPointer;
	debugger_updateExecutionBreakpoint(initialIP, true);
	PPCInterpreterSlim_executeInstruction(hCPU);
	debugger_updateExecutionBreakpoint(initialIP);
	debuggerState.debugSession.instructionPointer = hCPU->instructionPointer;
	if(updateDebuggerWindow)
		debuggerWindow_moveIP();
	ppcRecompilerEnabled = isRecEnabled;
}

bool debugger_stepOver(PPCInterpreter_t* hCPU)
{
	bool isRecEnabled = ppcRecompilerEnabled;
	ppcRecompilerEnabled = false;
	// disassemble current instruction
	PPCDisassembledInstruction disasmInstr = { 0 };
	uint32 initialIP = debuggerState.debugSession.instructionPointer;
	debugger_updateExecutionBreakpoint(initialIP, true);
	ppcAssembler_disassemble(initialIP, memory_readU32(initialIP), &disasmInstr);
	if (disasmInstr.ppcAsmCode != PPCASM_OP_BL &&
		disasmInstr.ppcAsmCode != PPCASM_OP_BCTRL)
	{
		// nothing to skip, use step-into
		debugger_stepInto(hCPU);
		debugger_updateExecutionBreakpoint(initialIP);
		debuggerWindow_moveIP();
		ppcRecompilerEnabled = isRecEnabled;
		return false;
	}
	// create one-shot breakpoint at next instruction
	debugger_createCodeBreakpoint(initialIP + 4, DEBUGGER_BP_T_ONE_SHOT);
	// step over current instruction (to avoid breakpoint)
	debugger_stepInto(hCPU);
	debuggerWindow_moveIP();
	// restore breakpoints
	debugger_updateExecutionBreakpoint(initialIP);
	// run
	ppcRecompilerEnabled = isRecEnabled;
	return true;
}

void debugger_createPPCStateSnapshot(PPCInterpreter_t* hCPU)
{
	memcpy(debuggerState.debugSession.ppcSnapshot.gpr, hCPU->gpr, sizeof(uint32) * 32);
	memcpy(debuggerState.debugSession.ppcSnapshot.fpr, hCPU->fpr, sizeof(FPR_t) * 32);
	debuggerState.debugSession.ppcSnapshot.spr_lr = hCPU->spr.LR;
	for (uint32 i = 0; i < 32; i++)
		debuggerState.debugSession.ppcSnapshot.cr[i] = hCPU->cr[i];
}

void DebugLogStackTrace(OSThread_t* thread, MPTR sp);

void debugger_enterTW(PPCInterpreter_t* hCPU)
{
	// handle logging points
	DebuggerBreakpoint* bp = debugger_getFirstBP(hCPU->instructionPointer);
	bool shouldBreak = debuggerBPChain_hasType(bp, DEBUGGER_BP_T_NORMAL) || debuggerBPChain_hasType(bp, DEBUGGER_BP_T_ONE_SHOT);
	while (bp)
	{
		if (bp->bpType == DEBUGGER_BP_T_LOGGING && bp->enabled)
		{
			std::wstring logName = !bp->comment.empty() ? L"Breakpoint '"+bp->comment+L"'" : fmt::format(L"Breakpoint at 0x{:08X} (no comment)", bp->address);
			std::wstring logContext = fmt::format(L"Thread: {:08x} LR: 0x{:08x}", coreinitThread_getCurrentThreadMPTRDepr(hCPU), hCPU->spr.LR, cemuLog_advancedPPCLoggingEnabled() ? L" Stack Trace:" : L"");
			cemuLog_log(LogType::Force, L"[Debugger] {} was executed! {}", logName, logContext);
			if (cemuLog_advancedPPCLoggingEnabled())
				DebugLogStackTrace(coreinitThread_getCurrentThreadDepr(hCPU), hCPU->gpr[1]);
			break;
		}
		bp = bp->next;
	}

	// return early if it's only a non-pausing logging breakpoint to prevent a modified debugger state and GUI updates
	if (!shouldBreak)
	{
		uint32 backupIP = debuggerState.debugSession.instructionPointer;
		debuggerState.debugSession.instructionPointer = hCPU->instructionPointer;
		debugger_stepInto(hCPU, false);
		PPCInterpreterSlim_executeInstruction(hCPU);
		debuggerState.debugSession.instructionPointer = backupIP;
		return;
	}

	// handle breakpoints
	debuggerState.debugSession.isTrapped = true;
	debuggerState.debugSession.debuggedThreadMPTR = coreinitThread_getCurrentThreadMPTRDepr(hCPU);
	debuggerState.debugSession.instructionPointer = hCPU->instructionPointer;
	debuggerState.debugSession.hCPU = hCPU;
	debugger_createPPCStateSnapshot(hCPU);
	// remove one-shot breakpoint if it exists
	DebuggerBreakpoint* singleshotBP = debugger_getFirstBP(debuggerState.debugSession.instructionPointer, DEBUGGER_BP_T_ONE_SHOT);
	if (singleshotBP)
		debugger_deleteBreakpoint(singleshotBP);
	debuggerWindow_notifyDebugBreakpointHit2();
	debuggerWindow_updateViewThreadsafe2();
	// reset step control
	debuggerState.debugSession.stepInto = false;
	debuggerState.debugSession.stepOver = false;
	debuggerState.debugSession.run = false;
	while (true)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		// check for step commands
		if (debuggerState.debugSession.stepOver)
		{
			if (debugger_stepOver(hCPU))
			{
				debugger_createPPCStateSnapshot(hCPU);
				break; // if true is returned, continue with execution
			}
			debugger_createPPCStateSnapshot(hCPU);
			debuggerWindow_updateViewThreadsafe2();
			debuggerState.debugSession.stepOver = false;
		}
		if (debuggerState.debugSession.stepInto)
		{
			debugger_stepInto(hCPU);
			debugger_createPPCStateSnapshot(hCPU);
			debuggerWindow_updateViewThreadsafe2();
			debuggerState.debugSession.stepInto = false;
			continue;
		}
		if (debuggerState.debugSession.run)
		{
			debugger_createPPCStateSnapshot(hCPU);
			debugger_stepInto(hCPU, false);
			PPCInterpreterSlim_executeInstruction(hCPU);
			debuggerState.debugSession.instructionPointer = hCPU->instructionPointer;
			debuggerState.debugSession.run = false;
			break;
		}
	}

	debuggerState.debugSession.isTrapped = false;
	debuggerState.debugSession.hCPU = nullptr;
	debuggerWindow_updateViewThreadsafe2();
	debuggerWindow_notifyRun();
}

void debugger_shouldBreak(PPCInterpreter_t* hCPU)
{
	if(debuggerState.debugSession.shouldBreak 
		// exclude emulator trampoline area
		&& (hCPU->instructionPointer < MEMORY_CODE_TRAMPOLINE_AREA_ADDR || hCPU->instructionPointer > MEMORY_CODE_TRAMPOLINE_AREA_ADDR + MEMORY_CODE_TRAMPOLINE_AREA_SIZE))
	{
		debuggerState.debugSession.shouldBreak = false;

		const uint32 address = (uint32)hCPU->instructionPointer;
		assert_dbg();
		//debugger_createBreakpoint(address, DEBUGGER_BP_TYPE_ONE_SHOT);
	}
}

void debugger_addParserSymbols(class ExpressionParser& ep)
{
	const auto module_count = RPLLoader_GetModuleCount();
	const auto module_list = RPLLoader_GetModuleList();

	std::vector<double> module_tmp(module_count);
	for (int i = 0; i < module_count; i++)
	{
		const auto module = module_list[i];
		if (module)
		{
			module_tmp[i] = (double)module->regionMappingBase_text.GetMPTR();
			ep.AddConstant(module->moduleName2, module_tmp[i]);
		}
	}

	for (sint32 i = 0; i < 32; i++)
		ep.AddConstant(fmt::format("r{}", i), debuggerState.debugSession.ppcSnapshot.gpr[i]);
}