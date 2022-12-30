#pragma once

// minimal but efficient non-recursive spinlock implementation

#include <atomic>

class FSpinlock
{
public:
	bool is_locked() const
	{
		return m_lockBool.load(std::memory_order_relaxed);
	}

	// implement BasicLockable and Lockable
	void lock() const
	{
		while (true)
		{
			if (!m_lockBool.exchange(true, std::memory_order_acquire))
				break;
			while (m_lockBool.load(std::memory_order_relaxed)) 
                _mm_pause();
		}
	}

	bool try_lock() const
	{
		return !m_lockBool.exchange(true, std::memory_order_acquire);
	}

	void unlock() const
	{
		m_lockBool.store(false, std::memory_order_release);
	}

private:
	
	mutable std::atomic<bool> m_lockBool = false;
};
