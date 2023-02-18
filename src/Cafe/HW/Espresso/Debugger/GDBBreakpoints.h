#include <utility>

#if BOOST_OS_LINUX
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#endif

namespace coreinit
{
	std::vector<std::thread::native_handle_type>& OSGetSchedulerThreads();
}

enum class BreakpointType
{
	BP_SINGLE,
	BP_PERSISTENT,
	BP_RESTORE_POINT,
	BP_STEP_POINT
};

class GDBServer::ExecutionBreakpoint {
public:
	ExecutionBreakpoint(MPTR address, BreakpointType type, bool visible, std::string reason)
		: m_address(address), m_removedAfterInterrupt(false), m_reason(std::move(reason))
	{
		if (type == BreakpointType::BP_SINGLE)
		{
			this->m_pauseThreads = true;
			this->m_restoreAfterInterrupt = false;
			this->m_deleteAfterAnyInterrupt = false;
			this->m_pauseOnNextInterrupt = false;
			this->m_visible = visible;
		}
		else if (type == BreakpointType::BP_PERSISTENT)
		{
			this->m_pauseThreads = true;
			this->m_restoreAfterInterrupt = true;
			this->m_deleteAfterAnyInterrupt = false;
			this->m_pauseOnNextInterrupt = false;
			this->m_visible = visible;
		}
		else if (type == BreakpointType::BP_RESTORE_POINT)
		{
			this->m_pauseThreads = false;
			this->m_restoreAfterInterrupt = false;
			this->m_deleteAfterAnyInterrupt = false;
			this->m_pauseOnNextInterrupt = false;
			this->m_visible = false;
		}
		else if (type == BreakpointType::BP_STEP_POINT)
		{
			this->m_pauseThreads = false;
			this->m_restoreAfterInterrupt = false;
			this->m_deleteAfterAnyInterrupt = true;
			this->m_pauseOnNextInterrupt = true;
			this->m_visible = false;
		}

		this->m_origOpCode = memory_readU32(address);
		memory_writeU32(address, DEBUGGER_BP_T_GDBSTUB_TW);
		PPCRecompiler_invalidateRange(address, address + 4);
	};
	~ExecutionBreakpoint()
	{
		memory_writeU32(this->m_address, this->m_origOpCode);
		PPCRecompiler_invalidateRange(this->m_address, this->m_address + 4);
	};

	[[nodiscard]] uint32 GetVisibleOpCode() const
	{
		if (this->m_visible)
			return memory_readU32(this->m_address);
		else
			return this->m_origOpCode;
	};
	[[nodiscard]] bool ShouldBreakThreads() const
	{
		return this->m_pauseThreads;
	};
	[[nodiscard]] bool ShouldBreakThreadsOnNextInterrupt()
	{
		bool shouldPause = this->m_pauseOnNextInterrupt;
		this->m_pauseOnNextInterrupt = false;
		return shouldPause;
	};
	[[nodiscard]] bool IsPersistent() const
	{
		return this->m_restoreAfterInterrupt;
	};
	[[nodiscard]] bool IsSkipBreakpoint() const
	{
		return this->m_deleteAfterAnyInterrupt;
	};
	[[nodiscard]] bool IsRemoved() const
	{
		return this->m_removedAfterInterrupt;
	};
	[[nodiscard]] std::string GetReason() const
	{
		return m_reason;
	};

	void RemoveTemporarily()
	{
		memory_writeU32(this->m_address, this->m_origOpCode);
		PPCRecompiler_invalidateRange(this->m_address, this->m_address + 4);
		this->m_restoreAfterInterrupt = true;
	};
	void Restore()
	{
		memory_writeU32(this->m_address, DEBUGGER_BP_T_GDBSTUB_TW);
		PPCRecompiler_invalidateRange(this->m_address, this->m_address + 4);
		this->m_restoreAfterInterrupt = false;
	};
	void PauseOnNextInterrupt()
	{
		this->m_pauseOnNextInterrupt = true;
	};

	void WriteNewOpCode(uint32 newOpCode)
	{
		this->m_origOpCode = newOpCode;
	};

private:
	const MPTR m_address;
	std::string m_reason;
	uint32 m_origOpCode;
	bool m_visible;
	bool m_pauseThreads;
	// type
	bool m_pauseOnNextInterrupt;
	bool m_restoreAfterInterrupt;
	bool m_deleteAfterAnyInterrupt;
	bool m_removedAfterInterrupt;
};

enum class AccessPointType
{
	BP_WRITE = 2,
	BP_READ = 3,
	BP_BOTH = 4
};

class GDBServer::AccessBreakpoint {
public:
	AccessBreakpoint(MPTR address, AccessPointType type)
		: m_address(address), m_type(type)
	{
#if BOOST_OS_WINDOWS
		for (auto& hThreadNH : coreinit::OSGetSchedulerThreads())
		{
			HANDLE hThread = (HANDLE)hThreadNH;
			CONTEXT ctx{};
			ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
			SuspendThread(hThread);
			GetThreadContext(hThread, &ctx);

			// use BP 2/3 for gdb stub since cemu's internal debugger uses BP 0/1 already
			ctx.Dr2 = (DWORD64)memory_getPointerFromVirtualOffset(address);
			ctx.Dr3 = (DWORD64)memory_getPointerFromVirtualOffset(address);
			// breakpoint 2
			SetBits(ctx.Dr7, 4, 1, 1);	// breakpoint #3 enabled: true
			SetBits(ctx.Dr7, 24, 2, 1); // breakpoint #3 condition: 1 (write)
			SetBits(ctx.Dr7, 26, 2, 3); // breakpoint #3 length: 3 (4 bytes)
			// breakpoint 3
			SetBits(ctx.Dr7, 6, 1, 1);	// breakpoint #4 enabled: true
			SetBits(ctx.Dr7, 28, 2, 3); // breakpoint #4 condition: 3 (read & write)
			SetBits(ctx.Dr7, 30, 2, 3); // breakpoint #4 length: 3 (4 bytes)

			SetThreadContext(hThread, &ctx);
			ResumeThread(hThread);
		}
#else
		for (auto& hThreadNH : coreinit::OSGetSchedulerThreads())
		{
			pid_t pid = (pid_t)hThreadNH;
			ptrace(PTRACE_ATTACH, pid, nullptr, nullptr);
			waitpid(pid, nullptr, 0);

			struct user_regs_struct regs;
			memset(&regs, 0, sizeof(regs));
			ptrace(PTRACE_GETREGS, pid, nullptr, &regs);

			// use BP 2/3 for gdb stub since cemu's internal debugger uses BP 0/1 already
			regs.r_dr2 = (uint64)memory_getPointerFromVirtualOffset(address);
			regs.r_dr3 = (uint64)memory_getPointerFromVirtualOffset(address);
			// breakpoint 2
			SetBits(regs.r_dr7, 4, 1, 1);  // breakpoint #3 enabled: true
			SetBits(regs.r_dr7, 24, 2, 1); // breakpoint #3 condition: 1 (write)
			SetBits(regs.r_dr7, 26, 2, 3); // breakpoint #3 length: 3 (4 bytes)
			// breakpoint 3
			SetBits(regs.r_dr7, 6, 1, 1);  // breakpoint #4 enabled: true
			SetBits(regs.r_dr7, 28, 2, 3); // breakpoint #4 condition: 3 (read & write)
			SetBits(regs.r_dr7, 30, 2, 3); // breakpoint #4 length: 3 (4 bytes)

			ptrace(PTRACE_SETREGS, pid, nullptr, &regs);
			ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
		}
#endif
	};
	~AccessBreakpoint()
	{
#if BOOST_OS_WINDOWS
		for (auto& hThreadNH : coreinit::OSGetSchedulerThreads())
		{
			HANDLE hThread = (HANDLE)hThreadNH;
			CONTEXT ctx{};
			ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
			SuspendThread(hThread);
			GetThreadContext(hThread, &ctx);

			// reset BP 2/3 to zero
			ctx.Dr2 = (DWORD64)0;
			ctx.Dr3 = (DWORD64)0;
			// breakpoint 2
			SetBits(ctx.Dr7, 4, 1, 0);
			SetBits(ctx.Dr7, 24, 2, 0);
			SetBits(ctx.Dr7, 26, 2, 0);
			// breakpoint 3
			SetBits(ctx.Dr7, 6, 1, 0);
			SetBits(ctx.Dr7, 28, 2, 0);
			SetBits(ctx.Dr7, 30, 2, 0);
			SetThreadContext(hThread, &ctx);
			ResumeThread(hThread);
		}
#else
		for (auto& hThreadNH : coreinit::OSGetSchedulerThreads())
		{
			pid_t pid = (pid_t)hThreadNH;
			ptrace(PTRACE_ATTACH, pid, nullptr, nullptr);
			waitpid(pid, nullptr, 0);

			struct user_regs_struct regs;
			memset(&regs, 0, sizeof(regs));
			ptrace(PTRACE_GETREGS, pid, nullptr, &regs);

			regs.r_dr0 = (uint64)0;
			regs.r_dr1 = (uint64)0;
			regs.r_dr7 = (uint64)0;

			// reset BP 2/3 to zero
			regs.r_dr2 = (DWORD64)0;
			regs.r_dr3 = (DWORD64)0;
			// breakpoint 2
			SetBits(regs.r_dr7, 4, 1, 0);
			SetBits(regs.r_dr7, 24, 2, 0);
			SetBits(regs.r_dr7, 26, 2, 0);
			// breakpoint 3
			SetBits(regs.r_dr7, 6, 1, 0);
			SetBits(regs.r_dr7, 28, 2, 0);
			SetBits(regs.r_dr7, 30, 2, 0);

			ptrace(PTRACE_SETREGS, pid, nullptr, &regs);
			ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
		}
#endif
	};

	MPTR GetAddress() const
	{
		return m_address;
	};
	AccessPointType GetType() const
	{
		return m_type;
	};

private:
	const MPTR m_address;
	const AccessPointType m_type;
};