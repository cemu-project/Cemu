

enum class BreakpointType
{
	BP_SINGLE,
	BP_PERSISTENT,
	BP_RESTORE_POINT,
	BP_STEP_POINT
};

class GDBServer::ExecutionBreakpoint {
  public:
	ExecutionBreakpoint(MPTR address, BreakpointType type, bool visible)
		: m_address(address), m_removedAfterInterrupt(false)
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
	uint32 m_origOpCode;
	bool m_visible;
	bool m_pauseThreads;
	// type
	bool m_pauseOnNextInterrupt;
	bool m_restoreAfterInterrupt;
	bool m_deleteAfterAnyInterrupt;
	bool m_removedAfterInterrupt{};
};