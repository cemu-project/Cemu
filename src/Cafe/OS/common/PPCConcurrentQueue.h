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
		//if(thread == nullptr)
		//	thread = coreinitThread_getCurrentThread(ppcInterpreterCurrentInstance);
		//OSThread_t* currentThread = coreinit::OSGetCurrentThread();
		//cemu_assert_debug(thread == nullptr || currentThread == thread);

		// cemuLog_logDebug(LogType::Force, "push suspend count: {}", _swapEndianU32(thread->suspend) - m_suspendCount);

		//__OSLockScheduler();

		__OSLockScheduler();
		m_queue.push(item);
		coreinit::__OSResumeThreadInternal(thread, 1);
		__OSUnlockScheduler();

		//__OSUnlockScheduler();

		//m_prevSuspendCount = _swapEndianU32(thread->suspend) - m_suspendCount;
		//coreinit_resumeThread(thread, _swapEndianU32(thread->suspend));
	}

	T pop(OSThread_t* thread = nullptr)
	{
		//if (thread == nullptr)
		//	thread = coreinitThread_getCurrentThread(ppcInterpreterCurrentInstance);

		OSThread_t* currentThread = coreinit::OSGetCurrentThread();
		cemu_assert_debug(thread == nullptr || currentThread == thread);

		//thread = coreinitThread_getCurrentThread(ppcInterpreterCurrentInstance);

		// cemuLog_logDebug(LogType::Force, "pop suspend count: {}", _swapEndianU32(thread->suspend) + m_suspendCount);

		__OSLockScheduler();
		if (m_queue.empty())
			coreinit::__OSSuspendThreadInternal(thread);
		auto val = m_queue.front();
		m_queue.pop();
		__OSUnlockScheduler();

		//coreinit_suspendThread(thread, m_suspendCount + m_prevSuspendCount);
		//m_prevSuspendCount = 0;
		//PPCCore_switchToScheduler();

		return val;
	}
private:
	//const int m_suspendCount = 8000;
	std::queue<T> m_queue;
	//std::atomic<uint32> m_prevSuspendCount;
};
