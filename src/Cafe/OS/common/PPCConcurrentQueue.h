#pragma once

#include <mutex>
#include <condition_variable>
#include <queue>

#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"

template <typename T>
class PPCConcurrentQueue
{
public:
	PPCConcurrentQueue() = default;
	PPCConcurrentQueue(const PPCConcurrentQueue&) = delete;
	PPCConcurrentQueue& operator=(const PPCConcurrentQueue&) = delete;

	void push(const T& item, OSThread_t* thread)
	{
		__OSLockScheduler();
		if (!coreinit::OSIsSchedulerActive())
		{
			cemu_assert_suspicious(); // caller should check if scheduler is active
			__OSUnlockScheduler();
			return;
		}
		m_queue.push(item);
		coreinit::__OSResumeThreadInternal(thread, 1);
		__OSUnlockScheduler();
	}

	T pop(OSThread_t* thread = nullptr)
	{
		OSThread_t* currentThread = coreinit::OSGetCurrentThread();
		cemu_assert_debug(thread == nullptr || currentThread == thread);
		__OSLockScheduler();
		if (m_queue.empty())
			coreinit::__OSSuspendThreadInternal(thread);
		auto val = m_queue.front();
		m_queue.pop();
		__OSUnlockScheduler();
		return val;
	}
private:
	std::queue<T> m_queue;
};
