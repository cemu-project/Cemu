#include <Windows.h>

SlimRWLock::SlimRWLock()
{
	static_assert(sizeof(m_lock) == sizeof(SRWLOCK));
	RTL_SRWLOCK* srwLock = (RTL_SRWLOCK*)&m_lock;
	*srwLock = SRWLOCK_INIT;
	//m_lock = { SRWLOCK_INIT };
}

void SlimRWLock::LockRead()
{
	AcquireSRWLockShared((SRWLOCK*)&m_lock);
}

void SlimRWLock::UnlockRead()
{
	ReleaseSRWLockShared((SRWLOCK*)&m_lock);
}

void SlimRWLock::LockWrite()
{
	AcquireSRWLockExclusive((SRWLOCK*)&m_lock);
}

void SlimRWLock::UnlockWrite()
{
	ReleaseSRWLockExclusive((SRWLOCK*)&m_lock);
}

uint32_t GetExceptionError()
{
	return (uint32_t)GetLastError();
}