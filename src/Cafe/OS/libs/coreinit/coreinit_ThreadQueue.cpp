#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"

namespace coreinit
{

	// puts the thread on the waiting queue and changes state to WAITING
	// relinquishes timeslice
	// always uses thread->waitQueueLink
	void OSThreadQueueInternal::queueAndWait(OSThread_t* thread)
	{
		cemu_assert_debug(__OSHasSchedulerLock());
		cemu_assert_debug(thread->waitQueueLink.next == nullptr && thread->waitQueueLink.prev == nullptr);
		thread->currentWaitQueue = this;
		this->addThreadByPriority(thread, &thread->waitQueueLink);
		cemu_assert_debug(thread->state == OSThread_t::THREAD_STATE::STATE_RUNNING);
		thread->state = OSThread_t::THREAD_STATE::STATE_WAITING;
		PPCCore_switchToSchedulerWithLock();
		cemu_assert_debug(thread->state == OSThread_t::THREAD_STATE::STATE_RUNNING);
	}

	void OSThreadQueueInternal::queueOnly(OSThread_t* thread)
	{
		cemu_assert_debug(__OSHasSchedulerLock());
		cemu_assert_debug(thread->waitQueueLink.next == nullptr && thread->waitQueueLink.prev == nullptr);
		thread->currentWaitQueue = this;
		this->addThreadByPriority(thread, &thread->waitQueueLink);
		cemu_assert_debug(thread->state == OSThread_t::THREAD_STATE::STATE_RUNNING);
		thread->state = OSThread_t::THREAD_STATE::STATE_WAITING;
	}

	// remove thread from wait queue and wake it up
	void OSThreadQueueInternal::cancelWait(OSThread_t* thread)
	{
		cemu_assert_debug(__OSHasSchedulerLock());
		this->removeThread(thread, &thread->waitQueueLink);
		thread->state = OSThread_t::THREAD_STATE::STATE_READY;
		thread->currentWaitQueue = nullptr;
		coreinit::__OSAddReadyThreadToRunQueue(thread);
		// todo - if waking up a thread on the same core with higher priority, reschedule
	}

	void OSThreadQueueInternal::addThread(OSThread_t* thread, OSThreadLink* threadLink)
	{
		cemu_assert_debug(__OSHasSchedulerLock());
		size_t linkOffset = getLinkOffset(thread, threadLink);
		// insert after tail
		if (tail.IsNull())
		{
			threadLink->next = nullptr;
			threadLink->prev = nullptr;
			head = thread;
			tail = thread;
		}
		else
		{
			threadLink->next = nullptr;
			threadLink->prev = tail;
			_getThreadLink(tail.GetPtr(), linkOffset)->next = thread;
			tail = thread;
		}
		_debugCheckChain(thread, threadLink);
	}

	void OSThreadQueueInternal::addThreadByPriority(OSThread_t* thread, OSThreadLink* threadLink)
	{
		cemu_assert_debug(tail.IsNull() == head.IsNull()); // either must be set or none at all
		cemu_assert_debug(__OSHasSchedulerLock());
		size_t linkOffset = getLinkOffset(thread, threadLink);
		if (tail.IsNull())
		{
			threadLink->next = nullptr;
			threadLink->prev = nullptr;
			head = thread;
			tail = thread;
		}
		else
		{
			// insert towards tail based on priority
			OSThread_t* threadItr = tail.GetPtr();
			while (threadItr && threadItr->effectivePriority > thread->effectivePriority)
				threadItr = _getThreadLink(threadItr, linkOffset)->prev.GetPtr();
			if (threadItr == nullptr)
			{
				// insert in front
				threadLink->next = head;
				threadLink->prev = nullptr;
				_getThreadLink(head.GetPtr(), linkOffset)->prev = thread;
				head = thread;
			}
			else
			{
				threadLink->prev = threadItr;
				threadLink->next = _getThreadLink(threadItr, linkOffset)->next;
				if (_getThreadLink(threadItr, linkOffset)->next)
				{
					OSThread_t* threadAfterItr = _getThreadLink(threadItr, linkOffset)->next.GetPtr();
					_getThreadLink(threadAfterItr, linkOffset)->prev = thread;
					_getThreadLink(threadItr, linkOffset)->next = thread;
				}
				else
				{
					tail = thread;
					_getThreadLink(threadItr, linkOffset)->next = thread;
				}
			}
		}
		_debugCheckChain(thread, threadLink);
	}

	void OSThreadQueueInternal::removeThread(OSThread_t* thread, OSThreadLink* threadLink)
	{
		cemu_assert_debug(__OSHasSchedulerLock());
		size_t linkOffset = getLinkOffset(thread, threadLink);
		_debugCheckChain(thread, threadLink);
		if (threadLink->prev)
			_getThreadLink(threadLink->prev.GetPtr(), linkOffset)->next = threadLink->next;
		else
			head = threadLink->next;
		if (threadLink->next)
			_getThreadLink(threadLink->next.GetPtr(), linkOffset)->prev = threadLink->prev;
		else
			tail = threadLink->prev;

		threadLink->next = nullptr;
		threadLink->prev = nullptr;
	}

	// counterpart for queueAndWait
	// if reschedule is true then scheduler will switch to woken up thread (if it is runnable on the same core)
	// sharedPriorityAndAffinityWorkaround is currently a hack/placeholder for some special cases. A proper fix likely involves handling all the nuances of thread effective priority
	void OSThreadQueueInternal::wakeupEntireWaitQueue(bool reschedule, bool sharedPriorityAndAffinityWorkaround)
	{
		cemu_assert_debug(__OSHasSchedulerLock());
		bool shouldReschedule = false;
		while (OSThread_t* thread = takeFirstFromQueue(offsetof(OSThread_t, waitQueueLink)))
		{
			cemu_assert_debug(thread->state == OSThread_t::THREAD_STATE::STATE_WAITING);
			//cemu_assert_debug(thread->suspendCounter == 0);
			thread->state = OSThread_t::THREAD_STATE::STATE_READY;
			thread->currentWaitQueue = nullptr;
			coreinit::__OSAddReadyThreadToRunQueue(thread);
			if (reschedule && thread->suspendCounter == 0 && PPCInterpreter_getCurrentInstance() && __OSCoreShouldSwitchToThread(coreinit::OSGetCurrentThread(), thread, sharedPriorityAndAffinityWorkaround))
				shouldReschedule = true;
		}
		if (shouldReschedule)
			PPCCore_switchToSchedulerWithLock();
	}

	// counterpart for queueAndWait
	// if reschedule is true then scheduler will switch to woken up thread (if it is runnable on the same core)
	void OSThreadQueueInternal::wakeupSingleThreadWaitQueue(bool reschedule, bool sharedPriorityAndAffinityWorkaround)
	{
		cemu_assert_debug(__OSHasSchedulerLock());
		OSThread_t* thread = takeFirstFromQueue(offsetof(OSThread_t, waitQueueLink));
		cemu_assert_debug(thread);
		bool shouldReschedule = false;
		if (thread)
		{
			thread->state = OSThread_t::THREAD_STATE::STATE_READY;
			thread->currentWaitQueue = nullptr;
			coreinit::__OSAddReadyThreadToRunQueue(thread);
			if (reschedule && thread->suspendCounter == 0 && PPCInterpreter_getCurrentInstance() && __OSCoreShouldSwitchToThread(coreinit::OSGetCurrentThread(), thread, sharedPriorityAndAffinityWorkaround))
				shouldReschedule = true;
		}
		if (shouldReschedule)
			PPCCore_switchToSchedulerWithLock();
	}

}
