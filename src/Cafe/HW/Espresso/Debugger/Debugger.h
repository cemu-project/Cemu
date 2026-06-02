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

class DebuggerCallbacks
{
  public:
	virtual void UpdateViewThreadsafe() {}
	virtual void NotifyDebugBreakpointHit() {}
	virtual void NotifyRun() {}
	virtual void MoveToAddressInDisassembly(MPTR address) {}
	virtual void NotifyModuleLoaded(struct RPLModule* module) {}
	virtual void NotifyModuleUnloaded(struct RPLModule* module) {}
	virtual void NotifyGraphicPacksModified() {}
	virtual ~DebuggerCallbacks() = default;
};

class DebuggerDispatcher
{
  private:
	static inline class DefaultDebuggerCallbacks : public DebuggerCallbacks
	{
	} s_defaultDebuggerCallbacks;
	DebuggerCallbacks* m_callbacks = &s_defaultDebuggerCallbacks;

  public:
	void SetDebuggerCallbacks(DebuggerCallbacks* debuggerCallbacks)
	{
		cemu_assert_debug(m_callbacks == &s_defaultDebuggerCallbacks);
		m_callbacks = debuggerCallbacks;
	}

	void ClearDebuggerCallbacks()
	{
		cemu_assert_debug(m_callbacks != &s_defaultDebuggerCallbacks);
		m_callbacks = &s_defaultDebuggerCallbacks;
	}

	void UpdateViewThreadsafe()
	{
		m_callbacks->UpdateViewThreadsafe();
	}

	void NotifyDebugBreakpointHit()
	{
		m_callbacks->NotifyDebugBreakpointHit();
	}

	void NotifyRun()
	{
		m_callbacks->NotifyRun();
	}

	void MoveToAddressInDisassembly(MPTR address)
	{
		m_callbacks->MoveToAddressInDisassembly(address);
	}

	void NotifyModuleLoaded(struct RPLModule* module)
	{
		m_callbacks->NotifyModuleLoaded(module);
	}

	void NotifyModuleUnloaded(struct RPLModule* module)
	{
		m_callbacks->NotifyModuleUnloaded(module);
	}

	void NotifyGraphicPacksModified()
	{
		m_callbacks->NotifyGraphicPacksModified();
	}
} extern g_debuggerDispatcher;

using BreakpointId = uint64;

struct DebuggerBreakpoint
{
	BreakpointId id;
	uint32 address;
	uint32 originalOpcodeValue;
	mutable uint8 bpType;
	mutable bool enabled;
	mutable std::wstring comment;
	mutable uint8 dbType = DEBUGGER_BP_T_DEBUGGER;

	DebuggerBreakpoint(uint32 address, uint32 originalOpcode, uint8 bpType = 0, bool enabled = true, std::wstring comment = std::wstring())
		:address(address), originalOpcodeValue(originalOpcode), bpType(bpType), enabled(enabled), comment(std::move(comment)) 
	{
		static std::atomic<uint64_t> bpIdGen{1};
		id = bpIdGen.fetch_add(1);
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
	uint32 gpr[32]{};
	FPR_t fpr[32]{};
	uint8 cr[32]{};
	uint32 spr_lr{};
};

enum class DebuggerStepCommand : uint8
{
	None,
	StepInto,
	StepOver,
	Run
};

// breakpoint API
void debugger_createCodeBreakpoint(uint32 address, uint8 bpType);
void debugger_createMemoryBreakpoint(uint32 address, bool onRead, bool onWrite);
void debugger_toggleExecuteBreakpoint(uint32 address); // create/remove execute breakpoint
void debugger_toggleLoggingBreakpoint(uint32 address); // create/remove logging breakpoint
void debugger_toggleBreakpoint(uint32 address, bool state, DebuggerBreakpoint* bp);
void debugger_deleteBreakpoint(BreakpointId bpId);
std::vector<DebuggerBreakpoint*>& debugger_lockBreakpoints();
DebuggerBreakpoint* debugger_getFirstBP(uint32 address);
DebuggerBreakpoint* debugger_getBreakpointById(BreakpointId bpId);
void debugger_unlockBreakpoints();

// patch API
void debugger_createPatch(uint32 address, std::span<uint8> patchData);
bool debugger_hasPatch(uint32 address);
void debugger_removePatch(uint32 address);

// debug session
PPCInterpreter_t* debugger_lockDebugSession();
void debugger_unlockDebugSession(PPCInterpreter_t* hCPU);
PPCSnapshot debugger_getSnapshotFromSession(PPCInterpreter_t* hCPU);
void debugger_stepCommand(DebuggerStepCommand stepCommand);

// misc
void debugger_requestBreak(); // break at the next possible instruction (any thread)
bool debugger_isTrapped();
void debugger_handleEntryBreakpoint(uint32 address);
void debugger_enterTW(PPCInterpreter_t* hCPU, bool isSingleStep = false);
void debugger_jumpToAddressInDisasm(MPTR address);
void debugger_addParserSymbols(class ExpressionParser& ep);

// options
void debugger_setOptionBreakOnEntry(bool isEnabled);
bool debugger_getOptionBreakOnEntry();
void debugger_setOptionLogOnlyMemoryBreakpoints(bool isEnabled);
bool debugger_getOptionLogOnlyMemoryBreakpoints();
