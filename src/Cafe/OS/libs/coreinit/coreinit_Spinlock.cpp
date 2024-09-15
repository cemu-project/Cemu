#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"
#include "Cafe/OS/libs/coreinit/coreinit_Spinlock.h"
#include "Cafe/OS/libs/coreinit/coreinit_Time.h"
#include "Cafe/OS/libs/coreinit/coreinit.h"

namespace coreinit
{
	void __OSBoostThread(OSThread_t* thread)
	{
		__OSLockScheduler();
		thread->stateFlags |= 0x20000;
		thread->context.boostCount += 1;
		__OSUpdateThreadEffectivePriority(thread); // sets thread->effectivePriority to zero since boostCount != 0
		__OSUnlockScheduler();
	}

	void __OSDeboostThread(OSThread_t* thread)
	{
		__OSLockScheduler();
		cemu_assert_debug(thread->context.boostCount != 0);
		thread->context.boostCount -= 1;
		if (thread->context.boostCount == 0)
		{
			thread->stateFlags &= ~0x20000;
			__OSUpdateThreadEffectivePriority(thread);
			// todo - reschedule if lower priority than other threads on current core?
		}
		__OSUnlockScheduler();
	}

	void OSInitSpinLock(OSSpinLock* spinlock)
	{
		spinlock->userData = spinlock;
		spinlock->ownerThread = nullptr;
		spinlock->count = 0;
		spinlock->interruptMask = 1;
	}

	bool OSAcquireSpinLock(OSSpinLock* spinlock)
	{
		OSThread_t* currentThread = OSGetCurrentThread();
		if (spinlock->ownerThread == currentThread)
		{
			spinlock->count += 1;
			return true;
		}
		else
		{
			// loop until lock acquired
			while (!spinlock->ownerThread.atomic_compare_exchange(nullptr, currentThread))
			{
				OSYieldThread();
			}
		}
		__OSBoostThread(currentThread);
		return true;
	}

	bool OSTryAcquireSpinLock(OSSpinLock* spinlock)
	{
		OSThread_t* currentThread = OSGetCurrentThread();
		if (spinlock->ownerThread == currentThread)
		{
			spinlock->count += 1;
			return true;
		}
		// try acquire once
		if (!spinlock->ownerThread.atomic_compare_exchange(nullptr, currentThread))
			return false;
		__OSBoostThread(currentThread);
		return true;
	}

	bool OSTryAcquireSpinLockWithTimeout(OSSpinLock* spinlock, uint64 timeout)
	{
		// used by CoD: Ghosts
		cemu_assert_debug((timeout >> 63) == 0); // negative?

		OSThread_t* currentThread = OSGetCurrentThread();
		if (spinlock->ownerThread == currentThread)
		{
			spinlock->count += 1;
			return true;
		}
		else
		{
			// loop until lock acquired or timeout occurred
			uint64 timeoutValue = coreinit_getTimerTick() + coreinit::EspressoTime::ConvertNsToTimerTicks(timeout);
			while (!spinlock->ownerThread.atomic_compare_exchange(nullptr, currentThread))
			{
				OSYieldThread();
				if (coreinit_getTimerTick() >= timeoutValue)
				{
					return false;
				}
			}
		}
		__OSBoostThread(currentThread);
		return true;
	}

	bool OSReleaseSpinLock(OSSpinLock* spinlock)
	{
		OSThread_t* currentThread = OSGetCurrentThread();
		cemu_assert_debug(spinlock->ownerThread == currentThread);
		if (spinlock->count != 0)
		{
			spinlock->count -= 1;
			return true;
		}
		// release spinlock
		while (!spinlock->ownerThread.atomic_compare_exchange(currentThread, nullptr));
		__OSDeboostThread(currentThread);
		return true;
	}

	bool OSUninterruptibleSpinLock_Acquire(OSSpinLock* spinlock)
	{
		// frequently used by VC DS
		OSThread_t* currentThread = OSGetCurrentThread();
		cemu_assert_debug(currentThread != nullptr);
		if (spinlock->ownerThread == currentThread)
		{
			spinlock->count += 1;
			return true;
		}
		else
		{
			// loop until lock acquired
			if (coreinit::__CemuIsMulticoreMode())
			{
				while (!spinlock->ownerThread.atomic_compare_exchange(nullptr, currentThread))
				{
					_mm_pause();
				}
			}
			else
			{
				// we are in single-core mode and the lock will never be released unless we let other threads resume work
				// to avoid an infinite loop we have no choice but to yield the thread even it is in an uninterruptible state
				if( !OSIsInterruptEnabled() )
					cemuLog_logOnce(LogType::APIErrors, "OSUninterruptibleSpinLock_Acquire(): Lock is occupied which requires a wait but current thread is already in an uninterruptible state (Avoid cascaded OSDisableInterrupts and/or OSUninterruptibleSpinLock)");
				while (!spinlock->ownerThread.atomic_compare_exchange(nullptr, currentThread))
				{
					OSYieldThread();
				}
			}
		}
		__OSBoostThread(currentThread);
		spinlock->interruptMask = OSDisableInterrupts();
		cemu_assert_debug(spinlock->ownerThread == currentThread);
		return true;
	}

	bool OSUninterruptibleSpinLock_TryAcquire(OSSpinLock* spinlock)
	{
		OSThread_t* currentThread = OSGetCurrentThread();
		if (spinlock->ownerThread == currentThread)
		{
			spinlock->count += 1;
			return true;
		}
		// try acquire once
		if (!spinlock->ownerThread.atomic_compare_exchange(nullptr, currentThread))
			return false;
		__OSBoostThread(currentThread);
		spinlock->interruptMask = OSDisableInterrupts();
		return true;
	}

	bool OSUninterruptibleSpinLock_TryAcquireWithTimeout(OSSpinLock* spinlock, uint64 timeout)
	{
		cemu_assert_debug((timeout >> 63) == 0); // negative?

		OSThread_t* currentThread = OSGetCurrentThread();
		if (spinlock->ownerThread == currentThread)
		{
			spinlock->count += 1;
			return true;
		}
		else
		{
			// loop until lock acquired or timeout occurred
			uint64 timeoutValue = coreinit_getTimerTick() + coreinit::EspressoTime::ConvertNsToTimerTicks(timeout);
			while (!spinlock->ownerThread.atomic_compare_exchange(nullptr, currentThread))
			{
				OSYieldThread();
				if (coreinit_getTimerTick() >= timeoutValue)
				{
					return false;
				}
			}
		}
		__OSBoostThread(currentThread);
		spinlock->interruptMask = OSDisableInterrupts();
		return true;
	}

	bool OSUninterruptibleSpinLock_Release(OSSpinLock* spinlock)
	{
		OSThread_t* currentThread = OSGetCurrentThread();
		cemu_assert_debug(spinlock->ownerThread == currentThread);
		if (spinlock->count != 0)
		{
			spinlock->count -= 1;
			return true;
		}
		// release spinlock
		OSRestoreInterrupts(spinlock->interruptMask);
		spinlock->interruptMask = 1;
		while (!spinlock->ownerThread.atomic_compare_exchange(currentThread, nullptr));
		__OSDeboostThread(currentThread);
		return true;
	}

	void InitializeSpinlock()
	{
		cafeExportRegister("coreinit", OSInitSpinLock, LogType::Placeholder);
		cafeExportRegister("coreinit", OSAcquireSpinLock, LogType::Placeholder);
		cafeExportRegister("coreinit", OSTryAcquireSpinLock, LogType::Placeholder);
		cafeExportRegister("coreinit", OSTryAcquireSpinLockWithTimeout, LogType::Placeholder);
		cafeExportRegister("coreinit", OSReleaseSpinLock, LogType::Placeholder);

		cafeExportRegister("coreinit", OSUninterruptibleSpinLock_Acquire, LogType::Placeholder);
		cafeExportRegister("coreinit", OSUninterruptibleSpinLock_TryAcquire, LogType::Placeholder);
		cafeExportRegister("coreinit", OSUninterruptibleSpinLock_TryAcquireWithTimeout, LogType::Placeholder);
		cafeExportRegister("coreinit", OSUninterruptibleSpinLock_Release, LogType::Placeholder);
	}
#pragma endregion
}
