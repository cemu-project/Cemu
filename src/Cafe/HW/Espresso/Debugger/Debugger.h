#pragma once

#include <set>
#include "Cafe/HW/Espresso/PPCState.h"

#define DEBUGGER_BP_T_NORMAL		0 // normal breakpoint
#define DEBUGGER_BP_T_ONE_SHOT		1 // normal breakpoint, deletes itself after trigger (used for stepping)
#define DEBUGGER_BP_T_MEMORY_READ	2 // memory breakpoint
#define DEBUGGER_BP_T_MEMORY_WRITE	3 // memory breakpoint
#define DEBUGGER_BP_T_LOGGING		4 // logging breakpoint, prints the breakpoint comment and stack trace whenever hit

#define DEBUGGER_BP_T_GDBSTUB		1 // breakpoint created by GDBStub
#define DEBUGGER_BP_T_DEBUGGER		2 // breakpoint created by Cemu's debugger

#define DEBUGGER_BP_T_GDBSTUB_TW    0x7C010008
#define DEBUGGER_BP_T_DEBUGGER_TW   0x7C020008


struct DebuggerBreakpoint
{
	uint32 address;
	uint32 originalOpcodeValue;
	mutable uint8 bpType;
	mutable bool enabled;
	mutable std::wstring comment;
	mutable uint8 dbType = DEBUGGER_BP_T_DEBUGGER;

	DebuggerBreakpoint(uint32 address, uint32 originalOpcode, uint8 bpType = 0, bool enabled = true, std::wstring comment = std::wstring())
		:address(address), originalOpcodeValue(originalOpcode), bpType(bpType), enabled(enabled), comment(std::move(comment)) 
	{
		next = nullptr;
	}


	bool operator<(const DebuggerBreakpoint& rhs) const
	{
		return address < rhs.address;
	}
	bool operator==(const DebuggerBreakpoint& rhs) const
	{
		return address == rhs.address;
	}

	bool isExecuteBP() const
	{
		return bpType == DEBUGGER_BP_T_NORMAL || bpType == DEBUGGER_BP_T_LOGGING || bpType == DEBUGGER_BP_T_ONE_SHOT;
	}

	bool isMemBP() const
	{
		return bpType == DEBUGGER_BP_T_MEMORY_READ || bpType == DEBUGGER_BP_T_MEMORY_WRITE;
	}

	DebuggerBreakpoint* next;
};

struct DebuggerPatch 
{
	uint32 address;
	sint32 length;
	std::vector<uint8> data;
	std::vector<uint8> origData;
};

struct PPCSnapshot
{
	uint32 gpr[32];
	FPR_t fpr[32];
	uint8 cr[32];
	uint32 spr_lr;
};

typedef struct  
{
	bool breakOnEntry;
	// breakpoints
	std::vector<DebuggerBreakpoint*> breakpoints;
	std::vector<DebuggerPatch*> patches;
	DebuggerBreakpoint* activeMemoryBreakpoint;
	// debugging state
	struct  
	{
		volatile bool shouldBreak; // debugger window requests a break asap
		volatile bool isTrapped; // if set, breakpoint is active and stepping through the code is possible
		uint32 debuggedThreadMPTR;
		volatile uint32 instructionPointer;
		PPCInterpreter_t* hCPU;
		// step control
		volatile bool stepOver;
		volatile bool stepInto;
		volatile bool run;
		// snapshot of PPC state
		PPCSnapshot ppcSnapshot;
	}debugSession;

}debuggerState_t;

extern debuggerState_t debuggerState;

// new API
DebuggerBreakpoint* debugger_getFirstBP(uint32 address);
void debugger_createCodeBreakpoint(uint32 address, uint8 bpType);
void debugger_createExecuteBreakpoint(uint32 address);
void debugger_toggleExecuteBreakpoint(uint32 address); // create/remove execute breakpoint
void debugger_toggleBreakpoint(uint32 address, bool state, DebuggerBreakpoint* bp);

void debugger_createMemoryBreakpoint(uint32 address, bool onRead, bool onWrite);

void debugger_handleEntryBreakpoint(uint32 address);

void debugger_deleteBreakpoint(DebuggerBreakpoint* bp);

void debugger_updateExecutionBreakpoint(uint32 address, bool forceRestore = false);

void debugger_createPatch(uint32 address, std::span<uint8> patchData);
bool debugger_hasPatch(uint32 address);

void debugger_forceBreak(); // force breakpoint at the next possible instruction
bool debugger_isTrapped();
void debugger_resume();

void debugger_enterTW(PPCInterpreter_t* hCPU);
void debugger_shouldBreak(PPCInterpreter_t* hCPU);

void debugger_addParserSymbols(class ExpressionParser& ep);