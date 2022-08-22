#pragma once

// minimal but efficient non-recursive spinlock implementation

#include <atomic>

class FSpinlock
{
public:
	void acquire()
	{
		while( true )
		{
			if (!m_lockBool.exchange(true, std::memory_order_acquire)) 
				break;
			while (m_lockBool.load(std::memory_order_relaxed)) _mm_pause();
		}
	}

	bool tryAcquire()
	{
		return !m_lockBool.exchange(true, std::memory_order_acquire);
	}

	void release()
	{
		m_lockBool.store(false, std::memory_order_release);
	}

	bool isHolding() const
	{
		return m_lockBool.load(std::memory_order_relaxed);
	}

private:
	
	std::atomic<bool> m_lockBool = false;
};