#pragma once
#include "GDBStub.h"
#include <utility>

#if defined(ARCH_X86_64) && BOOST_OS_LINUX
#include <sys/types.h>

// helpers for accessing debug register
typedef unsigned long DRType;

DRType _GetDR(pid_t tid, int drIndex);
void _SetDR(pid_t tid, int drIndex, DRType newValue);
DRType _ReadDR6();
#endif

enum class BreakpointType
{
	BP_SINGLE,
	BP_PERSISTENT,
	BP_RESTORE_POINT,
	BP_STEP_POINT
};

class GDBServer::ExecutionBreakpoint {
public:
	ExecutionBreakpoint(MPTR address, BreakpointType type, bool visible, std::string reason);
	~ExecutionBreakpoint();

	[[nodiscard]] uint32 GetVisibleOpCode() const;
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

	void RemoveTemporarily();
	void Restore();
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
	AccessBreakpoint(MPTR address, AccessPointType type);
	~AccessBreakpoint();

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
