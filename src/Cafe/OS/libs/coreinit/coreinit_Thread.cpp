#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/HW/Espresso/PPCCallback.h"
#include "Cafe/OS/RPL/rpl.h"
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"
#include "Cafe/OS/libs/coreinit/coreinit_Time.h"
#include "Cafe/OS/libs/coreinit/coreinit_Alarm.h"
#include "Cafe/OS/libs/snd_core/ax.h"
#include "Cafe/HW/Espresso/Debugger/GDBStub.h"
#include "Cafe/HW/Espresso/Interpreter/PPCInterpreterInternal.h"
#include "Cafe/HW/Espresso/Recompiler/PPCRecompiler.h"

#include "util/helpers/Semaphore.h"
#include "util/helpers/ConcurrentQueue.h"
#include "util/Fiber/Fiber.h"

#include "util/helpers/helpers.h"

SlimRWLock srwlock_activeThreadList;

// public list of active threads
MPTR activeThread[256];
sint32 activeThreadCount = 0;

void nnNfp_update();

namespace coreinit
{
#ifdef __arm64__
	void __OSFiberThreadEntry(uint32, uint32);
#else
	void __OSFiberThreadEntry(void* thread);
#endif
	void __OSAddReadyThreadToRunQueue(OSThread_t* thread);
	void __OSRemoveThreadFromRunQueues(OSThread_t* thread);
};

namespace coreinit
{
	// scheduler state
	std::atomic<bool> sSchedulerActive;
	std::vector<std::thread> sSchedulerThreads;
	std::mutex sSchedulerStateMtx;

	SysAllocator<OSThreadQueue> g_activeThreadQueue; // list of all threads (can include non-detached inactive threads)

	SysAllocator<OSThreadQueue, 3> g_coreRunQueue;
	CounterSemaphore g_coreRunQueueThreadCount[3];

	bool g_isMulticoreMode;

	thread_local uint32 t_assignedCoreIndex;
	thread_local Fiber* t_schedulerFiber;

	struct OSHostThread
	{
		OSHostThread(OSThread_t* thread) : m_thread(thread), m_fiber((void(*)(void*))__OSFiberThreadEntry, this, this)
		{
		}

		~OSHostThread() = default;

		OSThread_t* m_thread;
		Fiber m_fiber;
		// padding (used as stack memory in recompiler)
		uint8  padding[1024 * 128];
		PPCInterpreter_t ppcInstance;
		uint32 selectedCore;
	};

	std::unordered_map<OSThread_t*, OSHostThread*> s_threadToFiber;

	bool __CemuIsMulticoreMode()
	{
		return g_isMulticoreMode;
	}

	// create host thread (fiber) that will be used to run the PPC instance
	// note that host threads are fibers and not actual threads
	void __OSCreateHostThread(OSThread_t* thread)
	{
		cemu_assert_debug(__OSHasSchedulerLock());
		cemu_assert_debug(s_threadToFiber.find(thread) == s_threadToFiber.end());

		OSHostThread* hostThread = new OSHostThread(thread);
		s_threadToFiber.emplace(thread, hostThread);
	}

	// delete host thread
	void __OSDeleteHostThread(OSThread_t* thread)
	{
		static OSHostThread* _deleteQueue = nullptr;
		cemu_assert_debug(__OSHasSchedulerLock());

		if (_deleteQueue)
		{
			delete _deleteQueue;
			_deleteQueue = nullptr;
		}

		// delete with a delay (using queue of length 1)
		// since the fiber might still be in use right now we have to delay the deletion

		auto hostThread = s_threadToFiber[thread];
		s_threadToFiber.erase(thread);
		_deleteQueue = hostThread;
	}


	// add thread to active queue
	void __OSActivateThread(OSThread_t* thread)
	{
		cemu_assert_debug(__OSHasSchedulerLock());

		g_activeThreadQueue->addThread(thread, &thread->activeThreadChain); // todo - check if thread already in queue

		MPTR threadMPTR = memory_getVirtualOffsetFromPointer(thread);

		srwlock_activeThreadList.LockWrite();
		bool alreadyActive = false;
		for (sint32 i = 0; i < activeThreadCount; i++)
		{
			if (activeThread[i] == threadMPTR)
			{
				cemu_assert_debug(false); // should not happen
				alreadyActive = true;
			}
		}
		if (alreadyActive == false)
		{
			activeThread[activeThreadCount] = threadMPTR;
			activeThreadCount++;
		}

		__OSCreateHostThread(thread);

		srwlock_activeThreadList.UnlockWrite();
	}

	// remove thread from active queue. Reset id and state
	void __OSDeactivateThread(OSThread_t* thread)
	{
		cemu_assert_debug(__OSHasSchedulerLock());

		// remove thread from active thread list
		MPTR t = memory_getVirtualOffsetFromPointer(thread);
		srwlock_activeThreadList.LockWrite();
		bool noHit = true;
		for (sint32 i = 0; i < activeThreadCount; i++)
		{
			if (activeThread[i] == t)
			{
				activeThread[i] = activeThread[activeThreadCount - 1];
				activeThreadCount--;
				noHit = false;
				break;
			}
		}
		cemu_assert_debug(noHit == false);
		srwlock_activeThreadList.UnlockWrite();
		
		g_activeThreadQueue->removeThread(thread, &thread->activeThreadChain); // todo - check if thread in queue
		
		cemu_assert_debug(thread->state == OSThread_t::THREAD_STATE::STATE_NONE);
		thread->id = 0x8000;

		__OSDeleteHostThread(thread);
	}

	bool __OSIsThreadActive(OSThread_t* thread)
	{
		cemu_assert_debug(__OSHasSchedulerLock());
		MPTR threadMPTR = memory_getVirtualOffsetFromPointer(thread);
		srwlock_activeThreadList.LockWrite();
		bool isRunable = false;
		for (sint32 i = 0; i < activeThreadCount; i++)
		{
			if (activeThread[i] == threadMPTR)
			{
				srwlock_activeThreadList.UnlockWrite();
				return true;
			}
		}
		srwlock_activeThreadList.UnlockWrite();
		return false;
	}

	// thread
	OSThread_t* __currentCoreThread[3] = {};

	void OSSetCurrentThread(uint32 coreIndex, OSThread_t* thread)
	{
		if (coreIndex < 3)
			__currentCoreThread[coreIndex] = thread;
	}

	OSThread_t* OSGetCurrentThread()
	{
		PPCInterpreter_t* currentInstance = PPCInterpreter_getCurrentInstance();
		if (currentInstance == nullptr)
			return nullptr;
		return __currentCoreThread[currentInstance->spr.UPIR];
	}

	void threadEntry(PPCInterpreter_t* hCPU)
	{
		OSThread_t* currentThread = coreinit::OSGetCurrentThread();
		uint32 r3 = hCPU->gpr[3];
		uint32 r4 = hCPU->gpr[4];
		uint32 lr = hCPU->spr.LR;

		// cpp exception init callback
		//const uint32 im = OSDisableInterrupts(); -> on an actual Wii U interrupts are disabled for this callback, but there are games that yield the thread in the callback (see Angry Birds Star Wars)
		if (gCoreinitData->__cpp_exception_init_ptr != MPTR_NULL)
		{
			PPCCoreCallback(_swapEndianU32(gCoreinitData->__cpp_exception_init_ptr), &currentThread->crt.eh_globals);
		}
		//OSRestoreInterrupts(im);
		// forward to thread entrypoint
		hCPU->spr.LR = lr;
		hCPU->gpr[3] = r3;
		hCPU->gpr[4] = r4;
		hCPU->instructionPointer = currentThread->entrypoint.GetMPTR();
	}

	void coreinitExport_OSExitThreadDepr(PPCInterpreter_t* hCPU);

	void __OSInitContext(OSContext_t* ctx, MEMPTR<void> initialIP, MEMPTR<void> initialStackPointer)
	{
		ctx->SetContextMagic();
		ctx->gpr[0] = 0; // r0 is left uninitialized on console?
		for(auto& it : ctx->gpr)
			it = 0;
		ctx->gpr[1] = _swapEndianU32(initialStackPointer.GetMPTR());
		ctx->gpr[2] = _swapEndianU32(RPLLoader_GetSDA2Base());
		ctx->gpr[13] = _swapEndianU32(RPLLoader_GetSDA1Base());
		ctx->srr0 = initialIP.GetMPTR();
		ctx->cr = 0;
		ctx->ukn0A8 = 0;
		ctx->ukn0AC = 0;
		ctx->gqr[0] = 0;
		ctx->gqr[1] = 0;
		ctx->gqr[2] = 0;
		ctx->gqr[3] = 0;
		ctx->gqr[4] = 0;
		ctx->gqr[5] = 0;
		ctx->gqr[6] = 0;
		ctx->gqr[7] = 0;
		ctx->dsi_dar = 0;
		ctx->srr1 = 0x9032;
		ctx->xer = 0;
		ctx->dsi_dsisr = 0;
		ctx->upir = 0;
		ctx->boostCount = 0;
		ctx->state = 0;
		for(auto& it : ctx->coretime)
			it = 0;
		ctx->starttime = 0;
		ctx->ghs_errno = 0;
		ctx->upmc1 = 0;
		ctx->upmc2 = 0;
		ctx->upmc3 = 0;
		ctx->upmc4 = 0;
		ctx->ummcr0 = 0;
		ctx->ummcr1 = 0;
	}

	void __OSThreadInit(OSThread_t* thread, MEMPTR<void> entrypoint, uint32 argInt, MEMPTR<void> argPtr, MEMPTR<void> stackTop, uint32 stackSize, sint32 priority, uint32 upirCoreIndex, OSThread_t::THREAD_TYPE threadType)
	{
		thread->effectivePriority = priority;
		thread->type = threadType;
		thread->basePriority = priority;
		thread->SetThreadMagic();
		thread->id = 0x8000;
		thread->waitAlarm = nullptr;
		thread->entrypoint = entrypoint;
		thread->quantumTicks = 0;
		if(entrypoint)
		{
			thread->state = OSThread_t::THREAD_STATE::STATE_READY;
			thread->suspendCounter = 1;
		}
		else
		{
			thread->state = OSThread_t::THREAD_STATE::STATE_NONE;
			thread->suspendCounter = 0;
		}
		thread->exitValue = (uint32)-1;
		thread->requestFlags = OSThread_t::REQUEST_FLAG_BIT::REQUEST_FLAG_NONE;
		thread->pendingSuspend = 0;
		thread->suspendResult = 0xFFFFFFFF;
		thread->coretimeSumQuantumStart = 0;
		thread->deallocatorFunc = nullptr;
		thread->cleanupCallback = nullptr;
		thread->waitingForFastMutex = nullptr;
		thread->stateFlags = 0;
		thread->waitingForMutex = nullptr;
		memset(&thread->crt, 0, sizeof(thread->crt));
		static_assert(sizeof(thread->crt) == 0x1D8);
		thread->tlsBlocksMPTR = 0;
		thread->numAllocatedTLSBlocks = 0;
		thread->tlsStatus = 0;
		OSInitThreadQueueEx(&thread->joinQueue, thread);
		OSInitThreadQueueEx(&thread->suspendQueue, thread);
		thread->mutexQueue.ukn08 = thread;
		thread->mutexQueue.ukn0C = 0;
		thread->mutexQueue.tail = nullptr;
		thread->mutexQueue.head = nullptr;
		thread->ownedFastMutex.next = nullptr;
		thread->ownedFastMutex.prev = nullptr;
		thread->contendedFastMutex.next = nullptr;
		thread->contendedFastMutex.prev = nullptr;

		MEMPTR<void> alignedStackTop{MEMPTR<void>(stackTop).GetMPTR() & 0xFFFFFFF8};
		MEMPTR<uint32be> alignedStackTop32{alignedStackTop};
		alignedStackTop32[-1] = 0;
		alignedStackTop32[-2] = 0;

		__OSInitContext(&thread->context, MEMPTR<void>(PPCInterpreter_makeCallableExportDepr(threadEntry)), (void*)(alignedStackTop32.GetPtr() - 2));
		thread->stackBase = stackTop; // without alignment
		thread->stackEnd = ((uint8*)stackTop.GetPtr() - stackSize);
		thread->context.upir = upirCoreIndex;
		thread->context.lr = _swapEndianU32(PPCInterpreter_makeCallableExportDepr(coreinitExport_OSExitThreadDepr));
		thread->context.gpr[3] = _swapEndianU32(argInt);
		thread->context.gpr[4] = _swapEndianU32(argPtr.GetMPTR());

		*(uint32be*)((uint8*)stackTop.GetPtr() - stackSize) = 0xDEADBABE;
		thread->alarmRelatedUkn = 0;
		for(auto& it : thread->specificArray)
			it = nullptr;
		thread->context.fpscr.fpscr = 4;
		for(sint32 i=0; i<32; i++)
		{
			thread->context.fp_ps0[i] = 0.0;
			thread->context.fp_ps1[i] = 0.0;
		}
		thread->context.gqr[2] = 0x40004;
		thread->context.gqr[3] = 0x50005;
		thread->context.gqr[4] = 0x60006;
		thread->context.gqr[5] = 0x70007;

		for(sint32 i=0; i<Espresso::CORE_COUNT; i++)
			thread->context.coretime[i] = 0;

		// currentRunQueue and waitQueueLink is not initialized by COS and instead overwritten without validation
		// since we already have integrity checks in other functions, lets initialize it here
		for(sint32 i=0; i<Espresso::CORE_COUNT; i++)
			thread->currentRunQueue[i] = nullptr;
		thread->waitQueueLink.prev = nullptr;
		thread->waitQueueLink.next = nullptr;

		thread->wakeTimeRelatedUkn2 = 0;
		thread->wakeUpCount = 0;
		thread->wakeUpTime = 0;
		thread->wakeTimeRelatedUkn1 = 0x7FFFFFFFFFFFFFFF;
		thread->quantumTicks = 0;
		thread->coretimeSumQuantumStart = 0;
		thread->totalCycles = 0;

		for(auto& it : thread->padding68C)
			it = 0;
	}

	void SetThreadAffinityToCore(OSThread_t* thread, uint32 coreIndex)
	{
		cemu_assert_debug(coreIndex < 3);
		thread->attr &= ~(OSThread_t::ATTR_BIT::ATTR_AFFINITY_CORE0 | OSThread_t::ATTR_BIT::ATTR_AFFINITY_CORE1 | OSThread_t::ATTR_BIT::ATTR_AFFINITY_CORE2 | OSThread_t::ATTR_BIT::ATTR_UKN_010);
		thread->context.affinity &= 0xFFFFFFF8;
		if (coreIndex == 0)
		{
			thread->attr |= OSThread_t::ATTR_BIT::ATTR_AFFINITY_CORE0;
			thread->context.affinity |= (1<<0);
		}
		else if (coreIndex == 1)
		{
			thread->attr |= OSThread_t::ATTR_BIT::ATTR_AFFINITY_CORE1;
			thread->context.affinity |= (1<<1);
		}
		else // if (coreIndex == 2)
		{
			thread->attr |= OSThread_t::ATTR_BIT::ATTR_AFFINITY_CORE2;
			thread->context.affinity |= (1<<2);
		}
	}

	void __OSCreateThreadOnActiveThreadWorkaround(OSThread_t* thread)
	{
		__OSLockScheduler();
		bool isThreadStillActive = __OSIsThreadActive(thread);
		if (isThreadStillActive)
		{
			// workaround for games that restart threads before they correctly entered stopped/moribund state
			// seen in Fast Racing Neo at boot (0x020617BC OSCreateThread)
			cemuLog_log(LogType::Force, "Game attempting to re-initialize existing thread");
			while ((thread->state == OSThread_t::THREAD_STATE::STATE_READY || thread->state == OSThread_t::THREAD_STATE::STATE_RUNNING) && thread->suspendCounter == 0)
			{
				// wait for thread to finish
				__OSUnlockScheduler();
				OSSleepTicks(ESPRESSO_TIMER_CLOCK / 2000); // sleep 0.5ms
				__OSLockScheduler();
			}
			if (__OSIsThreadActive(thread) && thread->state == OSThread_t::THREAD_STATE::STATE_MORIBUND)
			{
				cemuLog_log(LogType::Force, "Calling OSCreateThread() on thread which is still active (Thread exited without detached flag). Forcing OSDetachThread()...");
				__OSUnlockScheduler();
				OSDetachThread(thread);
				__OSLockScheduler();
			}

		}
		cemu_assert_debug(__OSIsThreadActive(thread) == false);
		__OSUnlockScheduler();
	}

	bool __OSCreateThreadInternal2(OSThread_t* thread, MEMPTR<void> entrypoint, uint32 argInt, MEMPTR<void> argPtr, MEMPTR<void> stackBase, uint32 stackSize, sint32 priority, uint32 attrBits, OSThread_t::THREAD_TYPE threadType)
	{
		__OSCreateThreadOnActiveThreadWorkaround(thread);
		OSThread_t* currentThread = OSGetCurrentThread();
		if (priority < 0 || priority >= 32)
		{
			cemuLog_log(LogType::APIErrors, "OSCreateThreadInternal: Thread priority must be in range 0-31");
			return false;
		}
		if (threadType == OSThread_t::THREAD_TYPE::TYPE_IO)
		{
			priority = priority + 0x20;
		}
		else if (threadType == OSThread_t::THREAD_TYPE::TYPE_APP)
		{
			priority = priority + 0x40;
		}
		if(attrBits >= 0x20 || stackBase == nullptr || stackSize == 0)
		{
			cemuLog_logDebug(LogType::APIErrors, "OSCreateThreadInternal: Invalid attributes, stack base or size");
			return false;
		}
		uint32 im = OSDisableInterrupts();
		__OSLockScheduler(thread);

		uint32 coreIndex = PPCInterpreter_getCurrentInstance() ? OSGetCoreId() : 1;
		__OSThreadInit(thread, entrypoint, argInt, argPtr, stackBase, stackSize, priority, coreIndex, threadType);
		thread->threadName = nullptr;
		thread->context.affinity = attrBits & 7;
		thread->attr = attrBits;
		if ((attrBits & 7) == 0) // if no explicit affinity is given, use the current core
			SetThreadAffinityToCore(thread, OSGetCoreId());
		if(currentThread)
		{
			for(sint32 i=0; i<Espresso::CORE_COUNT; i++)
			{
				thread->dsiCallback[i] = currentThread->dsiCallback[i];
				thread->isiCallback[i] = currentThread->isiCallback[i];
				thread->programCallback[i] = currentThread->programCallback[i];
				thread->perfMonCallback[i] = currentThread->perfMonCallback[i];
				thread->alignmentExceptionCallback[i] = currentThread->alignmentExceptionCallback[i];
			}
			thread->context.srr1 = thread->context.srr1 | (currentThread->context.srr1 & 0x900);
			thread->context.fpscr.fpscr = thread->context.fpscr.fpscr | (currentThread->context.fpscr.fpscr & 0xF8);
		}
		else
		{
			for(sint32 i=0; i<Espresso::CORE_COUNT; i++)
			{
				thread->dsiCallback[i] = 0;
				thread->isiCallback[i] = 0;
				thread->programCallback[i] = 0;
				thread->perfMonCallback[i] = 0;
				thread->alignmentExceptionCallback[i] = nullptr;
			}
		}
		if (entrypoint)
		{
			thread->id = 0x8000;
			__OSActivateThread(thread); // also handles adding the thread to g_activeThreadQueue
		}
		__OSUnlockScheduler(thread);
		OSRestoreInterrupts(im);
		// recompile entry point function
		if (entrypoint)
			PPCRecompiler_recompileIfUnvisited(entrypoint.GetMPTR());
		return true;
	}

	bool OSCreateThreadType(OSThread_t* thread, MPTR entryPoint, sint32 numParam, void* ptrParam, void* stackTop, sint32 stackSize, sint32 priority, uint32 attr, OSThread_t::THREAD_TYPE threadType)
	{
		if(threadType != OSThread_t::THREAD_TYPE::TYPE_APP && threadType != OSThread_t::THREAD_TYPE::TYPE_IO)
		{
			cemuLog_logDebug(LogType::APIErrors, "OSCreateThreadType: Invalid thread type");
			cemu_assert_suspicious();
			return false;
		}
		return __OSCreateThreadInternal2(thread, MEMPTR<void>(entryPoint), numParam, ptrParam, stackTop, stackSize, priority, attr, threadType);
	}

	bool OSCreateThread(OSThread_t* thread, MPTR entryPoint, sint32 numParam, void* ptrParam, void* stackTop, sint32 stackSize, sint32 priority, uint32 attr)
	{
		return __OSCreateThreadInternal2(thread, MEMPTR<void>(entryPoint), numParam, ptrParam, stackTop, stackSize, priority, attr, OSThread_t::THREAD_TYPE::TYPE_APP);
	}

	// similar to OSCreateThreadType, but can be used to create any type of thread
	bool __OSCreateThreadType(OSThread_t* thread, MPTR entryPoint, sint32 numParam, void* ptrParam, void* stackTop, sint32 stackSize, sint32 priority, uint32 attr, OSThread_t::THREAD_TYPE threadType)
	{
		return __OSCreateThreadInternal2(thread, MEMPTR<void>(entryPoint), numParam, ptrParam, stackTop, stackSize, priority, attr, threadType);
	}

	bool OSRunThread(OSThread_t* thread, MPTR funcAddress, sint32 numParam, void* ptrParam)
	{
		__OSLockScheduler();

		cemu_assert_debug(PPCInterpreter_getCurrentInstance() == nullptr || OSGetCurrentThread() != thread); // called on self, what should this function do?

		if (thread->state != OSThread_t::THREAD_STATE::STATE_NONE && thread->state != OSThread_t::THREAD_STATE::STATE_MORIBUND)
		{
			// unsure about this case
			cemuLog_logDebug(LogType::Force, "OSRunThread called on thread which cannot be ran");
			__OSUnlockScheduler();
			return false;
		}

		if (thread->state == OSThread_t::THREAD_STATE::STATE_MORIBUND)
		{
			thread->state = OSThread_t::THREAD_STATE::STATE_NONE;
			coreinit::__OSDeactivateThread(thread);
			coreinit::__OSRemoveThreadFromRunQueues(thread);
		}

		// set thread state
		// todo - this should fully reinitialize the thread?

		thread->entrypoint = funcAddress;
		thread->context.srr0 = PPCInterpreter_makeCallableExportDepr(threadEntry);
		thread->context.lr = _swapEndianU32(PPCInterpreter_makeCallableExportDepr(coreinitExport_OSExitThreadDepr));
		thread->context.gpr[3] = _swapEndianU32(numParam);
		thread->context.gpr[4] = _swapEndianU32(memory_getVirtualOffsetFromPointer(ptrParam));
		thread->suspendCounter = 0;	// verify

		MPTR threadMPTR = memory_getVirtualOffsetFromPointer(thread);

		coreinit::__OSActivateThread(thread);
		thread->state = OSThread_t::THREAD_STATE::STATE_READY;

		__OSAddReadyThreadToRunQueue(thread);

		__OSUnlockScheduler();

		return true;
	}

	void OSExitThread(sint32 exitValue)
	{
		PPCInterpreter_t* hCPU = PPCInterpreter_getCurrentInstance();
		hCPU->gpr[3] = exitValue;
		OSThread_t* currentThread = coreinit::OSGetCurrentThread();

		// thread cleanup callback
		if (currentThread->cleanupCallback)
		{
			currentThread->stateFlags = _swapEndianU32(_swapEndianU32(currentThread->stateFlags) | 0x00000001);
			PPCCoreCallback(currentThread->cleanupCallback.GetMPTR(), currentThread, currentThread->stackEnd);
		}
		// cpp exception cleanup
		if (gCoreinitData->__cpp_exception_cleanup_ptr != 0 && currentThread->crt.eh_globals != nullptr)
		{
			PPCCoreCallback(_swapEndianU32(gCoreinitData->__cpp_exception_cleanup_ptr), &currentThread->crt.eh_globals);
			currentThread->crt.eh_globals = nullptr;
		}
		// set exit code
		currentThread->exitValue = exitValue;

		__OSLockScheduler();

		// release held synchronization primitives
		if (!currentThread->mutexQueue.isEmpty())
		{
			cemuLog_log(LogType::Force, "OSExitThread: Thread is holding mutexes");
			while (true)
			{
				OSMutex* mutex = currentThread->mutexQueue.getFirst();
				if (!mutex)
					break;
				if (mutex->owner != currentThread)
				{
					cemuLog_log(LogType::Force, "OSExitThread: Thread is holding mutex which it doesn't own");
					currentThread->mutexQueue.removeMutex(mutex);
					continue;
				}
				coreinit::OSUnlockMutexInternal(mutex);
			}
		}
		// todo - release all fast mutexes

		// handle join queue
		if (!currentThread->joinQueue.isEmpty())
			currentThread->joinQueue.wakeupEntireWaitQueue(false);
	
		if ((currentThread->attr & 8) != 0)
		{
			// deactivate thread since it is detached
			currentThread->state = OSThread_t::THREAD_STATE::STATE_NONE;
			coreinit::__OSDeactivateThread(currentThread);
			// queue call to thread deallocator if set
			if (!currentThread->deallocatorFunc.IsNull())
				__OSQueueThreadDeallocation(currentThread);
		}
		else
		{
			// non-detached threads remain active
			currentThread->state = OSThread_t::THREAD_STATE::STATE_MORIBUND;
		}
		PPCCore_switchToSchedulerWithLock();
	}

	void OSSetThreadSpecific(uint32 index, void* value)
	{
		OSThread_t* currentThread = OSGetCurrentThread();
		if (index >= (uint32)currentThread->specificArray.size())
			return;
		currentThread->specificArray[index] = value;
	}

	void* OSGetThreadSpecific(uint32 index)
	{
		OSThread_t* currentThread = OSGetCurrentThread();
		if (index >= (uint32)currentThread->specificArray.size())
			return nullptr;
		return currentThread->specificArray[index].GetPtr();
	}

	void OSSetThreadName(OSThread_t* thread, const char* name)
	{
		thread->threadName = name;
	}

	const char* OSGetThreadName(OSThread_t* thread)
	{
		return thread->threadName.GetPtr();
	}

	void coreinitExport_OSExitThreadDepr(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(exitCode, 0);
		OSExitThread(exitCode);
	}

	void OSYieldThread()
	{
		PPCCore_switchToScheduler();
	}

	void _OSSleepTicks_alarmHandler(uint64 currentTick, void* context)
	{
		cemu_assert_debug(__OSHasSchedulerLock());
		OSThreadQueue* threadQueue = (OSThreadQueue*)context;
		threadQueue->wakeupEntireWaitQueue(false);
	}

	void OSSleepTicks(uint64 ticks)
	{
		cemu_assert_debug(__OSHasSchedulerLock() == false);
		StackAllocator<OSThreadQueue> _threadQueue;
		OSInitThreadQueue(_threadQueue.GetPointer());
		__OSLockScheduler();
		OSHostAlarm* hostAlarm = OSHostAlarmCreate(OSGetTime() + ticks, 0, _OSSleepTicks_alarmHandler, _threadQueue.GetPointer());
		_threadQueue.GetPointer()->queueAndWait(OSGetCurrentThread());
		OSHostAlarmDestroy(hostAlarm);
		__OSUnlockScheduler();
	}

	void OSDetachThread(OSThread_t* thread)
	{
		__OSLockScheduler();
		thread->attr |= OSThread_t::ATTR_BIT::ATTR_DETACHED;
		if (thread->state == OSThread_t::THREAD_STATE::STATE_MORIBUND)
		{
			// exit thread
			// ?

			// todo -> call deallocator
			thread->state = OSThread_t::THREAD_STATE::STATE_NONE;
			thread->id = 0x8000;
			coreinit::__OSDeactivateThread(thread);
			if (!thread->joinQueue.isEmpty())
			{
				// handle join queue
				thread->joinQueue.wakeupEntireWaitQueue(true);
			}
		}
		__OSUnlockScheduler();
	}

	bool OSJoinThread(OSThread_t* thread, uint32be* exitValue)
	{
		__OSLockScheduler();

		if ((thread->attr & OSThread_t::ATTR_DETACHED) == 0 && thread->state != OSThread_t::THREAD_STATE::STATE_MORIBUND)
		{
			cemu_assert_debug(thread->joinQueue.isEmpty());
			// thread still running, wait in join queue
			thread->joinQueue.queueAndWait(OSGetCurrentThread());
		}
		else if (thread->state != OSThread_t::THREAD_STATE::STATE_MORIBUND)
		{
			// cannot join detached and active threads
			cemuLog_logDebug(LogType::Force, "Cannot join detached active thread");
			__OSUnlockScheduler();
			return false;
		}

		// thread already ended and is still attached, get return value
		cemu_assert_debug(thread->state == OSThread_t::THREAD_STATE::STATE_MORIBUND);
		cemu_assert_debug((thread->attr & OSThread_t::ATTR_DETACHED) == 0);
		if (exitValue)
			*exitValue = thread->exitValue;
		// end thread
		thread->state = OSThread_t::THREAD_STATE::STATE_NONE;
		__OSDeactivateThread(thread);
		coreinit::__OSRemoveThreadFromRunQueues(thread);
		thread->id = 0x8000;

		if (!thread->deallocatorFunc.IsNull())
			__OSQueueThreadDeallocation(thread);

		__OSUnlockScheduler();

		return true;
	}

	// adds the thread to each core's run queue if in runable state
	void __OSAddReadyThreadToRunQueue(OSThread_t* thread)
	{
        cemu_assert_debug(MMU_IsInPPCMemorySpace(thread));
        cemu_assert_debug(thread->IsValidMagic());
		cemu_assert_debug(__OSHasSchedulerLock());

		if (thread->state != OSThread_t::THREAD_STATE::STATE_READY)
			return;
		if (thread->suspendCounter != 0)
			return;
		for (sint32 i = 0; i < PPC_CORE_COUNT; i++)
		{
			if (thread->currentRunQueue[i] != nullptr)
				continue; // already on the queue
			// check affinity
			if(!thread->context.hasCoreAffinitySet(i))
				continue;
			g_coreRunQueue.GetPtr()[i].addThread(thread, thread->linkRun + i);
			thread->currentRunQueue[i] = (g_coreRunQueue.GetPtr() + i);
			g_coreRunQueueThreadCount[i].increment();
		}
	}

	void __OSRemoveThreadFromRunQueues(OSThread_t* thread)
	{
		cemu_assert_debug(__OSHasSchedulerLock());
		for (sint32 i = 0; i < PPC_CORE_COUNT; i++)
		{
			if(thread->currentRunQueue[i] == nullptr)
				continue;
			g_coreRunQueue.GetPtr()[i].removeThread(thread, thread->linkRun + i);
			thread->currentRunQueue[i] = nullptr;
			g_coreRunQueueThreadCount[i].decrement();
		}
	}

	// returns true if thread runs on same core and has higher priority
	bool __OSCoreShouldSwitchToThread(OSThread_t* currentThread, OSThread_t* newThread, bool sharedPriorityAndAffinityWorkaround)
	{
		uint32 coreIndex = OSGetCoreId();
		if (!newThread->context.hasCoreAffinitySet(coreIndex))
			return false;
		// special case: if current and new thread are running only on the same core then reschedule even if priority is equal
		// this resolves a deadlock in Just Dance 2019 where one thread would always reacquire the same mutex within it's timeslice, blocking another thread on the same core from acquiring it
		if (sharedPriorityAndAffinityWorkaround && (1<<coreIndex) == newThread->context.affinity && currentThread->context.affinity == newThread->context.affinity && currentThread->effectivePriority == newThread->effectivePriority)
			return true;
		// otherwise reschedule if new thread has higher priority
		return newThread->effectivePriority < currentThread->effectivePriority;
	}

	sint32 __OSResumeThreadInternal(OSThread_t* thread, sint32 resumeCount)
	{
		cemu_assert_debug(__OSHasSchedulerLock());
		sint32 previousSuspendCount = thread->suspendCounter;
		cemu_assert_debug(previousSuspendCount >= 0);
		if (previousSuspendCount == 0)
		{
			cemuLog_log(LogType::APIErrors, "OSResumeThread: Resuming thread 0x{:08x} which isn't suspended", MEMPTR<OSThread_t>(thread).GetMPTR());
			return 0;
		}
		thread->suspendCounter = previousSuspendCount - resumeCount;
		if (thread->suspendCounter < 0)
			thread->suspendCounter = 0;
		if (thread->suspendCounter == 0)
		{
			__OSAddReadyThreadToRunQueue(thread);
			// set awakened time if thread is ready
			// todo - only set this once?
			thread->wakeUpTime = PPCInterpreter_getMainCoreCycleCounter();
			// reschedule if thread has higher priority
			if (PPCInterpreter_getCurrentInstance() && __OSCoreShouldSwitchToThread(coreinit::OSGetCurrentThread(), thread, false))
				PPCCore_switchToSchedulerWithLock();
		}
		return previousSuspendCount;
	}

	sint32 OSResumeThread(OSThread_t* thread)
	{
		__OSLockScheduler();
		sint32 previousSuspendCount = __OSResumeThreadInternal(thread, 1);
		__OSUnlockScheduler();
		return previousSuspendCount;
	}

	void OSContinueThread(OSThread_t* thread)
	{
		__OSLockScheduler();
		__OSResumeThreadInternal(thread, thread->suspendCounter);
		__OSUnlockScheduler();
	}

	void __OSSuspendThreadInternal(OSThread_t* thread)
	{
		cemu_assert_debug(__OSHasSchedulerLock());
		cemu_assert_debug(thread->state != OSThread_t::THREAD_STATE::STATE_NONE && thread->state != OSThread_t::THREAD_STATE::STATE_MORIBUND); // how to handle these?

		sint32 previousSuspendCount = thread->suspendCounter;

		if (OSGetCurrentThread() == thread)
		{
			thread->suspendCounter = thread->suspendCounter + 1;
			PPCCore_switchToSchedulerWithLock();
		}
		else
		{
			thread->suspendCounter = thread->suspendCounter + 1;
			if (previousSuspendCount == 0)
			{
				__OSRemoveThreadFromRunQueues(thread);
				// todo - if thread is still running find a way to cancel it's timeslice immediately
			}
		}
	}

	void OSSuspendThread(OSThread_t* thread)
	{
		__OSLockScheduler();
		__OSSuspendThreadInternal(thread);
		__OSUnlockScheduler();
	}

    void __OSSuspendThreadNolock(OSThread_t* thread)
    {
        __OSSuspendThreadInternal(thread);
    }

	void OSSleepThread(OSThreadQueue* threadQueue)
	{
		__OSLockScheduler();
		threadQueue->queueAndWait(OSGetCurrentThread());
		__OSUnlockScheduler();
	}

	void OSWakeupThread(OSThreadQueue* threadQueue)
	{
		__OSLockScheduler();
		threadQueue->wakeupEntireWaitQueue(true);
		__OSUnlockScheduler();
	}

	bool OSSetThreadAffinity(OSThread_t* thread, uint32 affinityMask)
	{
		cemu_assert_debug((affinityMask & ~7) == 0);
		__OSLockScheduler();
		uint32 prevAffinityMask = thread->context.getAffinity();
		if (thread->state == OSThread_t::THREAD_STATE::STATE_RUNNING)
		{
			thread->attr = (thread->attr & ~7) | (affinityMask & 7);
			thread->context.setAffinity(affinityMask);
			// should this reschedule the thread?
		}
		else if (prevAffinityMask != affinityMask)
		{
            if(thread->state != OSThread_t::THREAD_STATE::STATE_NONE)
            {
                __OSRemoveThreadFromRunQueues(thread);
                thread->attr = (thread->attr & ~7) | (affinityMask & 7);
                thread->context.setAffinity(affinityMask);
                __OSAddReadyThreadToRunQueue(thread);
            }
            else
            {
                thread->attr = (thread->attr & ~7) | (affinityMask & 7);
                thread->context.setAffinity(affinityMask);
            }
		}
		__OSUnlockScheduler();
		return true;
	}

	uint32 OSGetThreadAffinity(OSThread_t* thread)
	{
		auto affinityMask = thread->context.getAffinity();
		cemu_assert_debug((affinityMask & ~7) == 0);
		return affinityMask;
	}

	void* OSSetThreadDeallocator(OSThread_t* thread, void* newDeallocatorFunc)
	{
		__OSLockScheduler();
		void* previousFunc = thread->deallocatorFunc.GetPtr();
		thread->deallocatorFunc = newDeallocatorFunc;
		__OSUnlockScheduler();
		return previousFunc;
	}

	void* OSSetThreadCleanupCallback(OSThread_t* thread, void* cleanupCallback)
	{
		__OSLockScheduler();
		void* previousFunc = thread->cleanupCallback.GetPtr();
		thread->cleanupCallback = cleanupCallback;
		__OSUnlockScheduler();
		return previousFunc;
	}

	void __OSSetThreadBasePriority(OSThread_t* thread, sint32 newPriority)
	{
		cemu_assert_debug(newPriority >= 0 && newPriority < 32);
		newPriority += OSThread_t::GetTypePriorityBase(thread->type);
		thread->basePriority = newPriority;
	}

	void __OSUpdateThreadEffectivePriority(OSThread_t* thread)
	{
		if (thread->context.boostCount != 0)
		{
			// temporarily boosted threads have their priority set to 0 (maximum)
			thread->effectivePriority = 0;
			return;
		}
		thread->effectivePriority = thread->basePriority;
	}

	bool OSSetThreadPriority(OSThread_t* thread, sint32 newPriority)
	{
		if (newPriority < 0 || newPriority >= 0x20)
		{
			cemu_assert_debug(false);
			return false; // invalid priority value
		}
		__OSLockScheduler();
		__OSSetThreadBasePriority(thread, newPriority);
		__OSUpdateThreadEffectivePriority(thread);
		// reschedule if needed
		OSThread_t* currentThread = OSGetCurrentThread();
		if (currentThread && currentThread != thread)
		{
			if (__OSCoreShouldSwitchToThread(currentThread, thread, false))
				PPCCore_switchToSchedulerWithLock();
		}
		__OSUnlockScheduler();
		return true;
	}

	sint32 OSGetThreadPriority(OSThread_t* thread)
	{
		sint32 threadPriority = thread->basePriority;
		OSThread_t::THREAD_TYPE threadType = thread->type;
		threadPriority -= OSThread_t::GetTypePriorityBase(threadType);
		cemu_assert_debug(threadPriority >= 0 && threadPriority < 32);
		return threadPriority;
	}

	bool OSIsThreadTerminated(OSThread_t* thread)
	{
		__OSLockScheduler();
		bool isTerminated = false;
		if (thread->state == OSThread_t::THREAD_STATE::STATE_MORIBUND || thread->state == OSThread_t::THREAD_STATE::STATE_NONE)
			isTerminated = true;
		__OSUnlockScheduler();
		return isTerminated;
	}

	bool OSIsThreadSuspended(OSThread_t* thread)
	{
		__OSLockScheduler();
		sint32 suspendCounter = thread->suspendCounter;
		__OSUnlockScheduler();
		return suspendCounter > 0;
	}

    bool OSIsThreadRunningNoLock(OSThread_t* thread)
    {
        cemu_assert_debug(__OSHasSchedulerLock());
        return thread->state == OSThread_t::THREAD_STATE::STATE_RUNNING;
    }

    bool OSIsThreadRunning(OSThread_t* thread)
    {
        bool isRunning = false;
        __OSLockScheduler();
        isRunning = OSIsThreadRunningNoLock(thread);
        __OSUnlockScheduler();
        return isRunning;
    }

    void OSCancelThread(OSThread_t* thread)
	{
		__OSLockScheduler();
		cemu_assert_debug(thread->requestFlags == 0 || thread->requestFlags == OSThread_t::REQUEST_FLAG_CANCEL); // todo - how to handle cases where other flags are already set?
		thread->requestFlags = OSThread_t::REQUEST_FLAG_CANCEL;
		__OSUnlockScheduler();
		// if the canceled thread is self, then exit immediately
		if (thread == OSGetCurrentThread())
			OSExitThread(-1);
	}

	void OSTestThreadCancelInternal()
	{
		// also handles suspend request ?
		cemu_assert_debug(__OSHasSchedulerLock());
		OSThread_t* thread = OSGetCurrentThread();
		if (thread->requestFlags == OSThread_t::REQUEST_FLAG_CANCEL)
		{
			__OSUnlockScheduler();
			OSExitThread(-1);
		}
	}

	void OSTestThreadCancel()
	{
		__OSLockScheduler();
		OSTestThreadCancelInternal();
		__OSUnlockScheduler();
	}

	void __OSSwitchToThreadFiber(OSThread_t* thread, uint32 coreIndex)
	{
		cemu_assert_debug(__OSHasSchedulerLock());
		cemu_assert_debug(s_threadToFiber.find(thread) != s_threadToFiber.end());

		OSHostThread* hostThread = s_threadToFiber.find(thread)->second;
		hostThread->selectedCore = coreIndex;
		Fiber::Switch(hostThread->m_fiber);
	}

	void __OSThreadLoadContext(PPCInterpreter_t* hCPU, OSThread_t* thread)
	{
		// load GPR
		for (uint32 i = 0; i < 32; i++)
			hCPU->gpr[i] = _swapEndianU32(thread->context.gpr[i]);

		// load SPR cr, lr, ctr, xer
		ppc_setCR(hCPU, thread->context.cr);
		hCPU->spr.LR = _swapEndianU32(thread->context.lr);
		hCPU->spr.CTR = thread->context.ctr;
		PPCInterpreter_setXER(hCPU, thread->context.xer);

		hCPU->fpscr = thread->context.fpscr.fpscr;

		// store floating point and Gekko registers
		for (uint32 i = 0; i < 32; i++)
		{
			hCPU->fpr[i].fp0int = thread->context.fp_ps0[i];
			hCPU->fpr[i].fp1int = thread->context.fp_ps1[i];
		}

		for (uint32 i = 0; i < 8; i++)
		{
			hCPU->spr.UGQR[0 + i] = thread->context.gqr[i];
		}

		hCPU->instructionPointer = thread->context.srr0;
	}

	void __OSThreadStoreContext(PPCInterpreter_t* hCPU, OSThread_t* thread)
	{
		// store GPR
		for (uint32 i = 0; i < 32; i++)
			thread->context.gpr[i] = _swapEndianU32(hCPU->gpr[i]);
		// store SPRs
		thread->context.cr = ppc_getCR(hCPU);
		thread->context.lr = _swapEndianU32(hCPU->spr.LR);
		thread->context.ctr = hCPU->spr.CTR;
		thread->context.xer = PPCInterpreter_getXER(hCPU);

		thread->context.fpscr.fpscr = hCPU->fpscr;
		thread->context.fpscr.padding = 0;

		// store floating point and Gekko registers
		for (uint32 i = 0; i < 32; i++)
		{
			thread->context.fp_ps0[i] = hCPU->fpr[i].fp0int;
			thread->context.fp_ps1[i] = hCPU->fpr[i].fp1int;
		}

		for (uint32 i = 0; i < 8; i++)
		{
			thread->context.gqr[i] = hCPU->spr.UGQR[0 + i];
		}

		thread->context.srr0 = hCPU->instructionPointer;
	}

	void __OSStoreThread(OSThread_t* thread, PPCInterpreter_t* hCPU)
	{
		if (thread->state == OSThread_t::THREAD_STATE::STATE_RUNNING)
		{
			thread->state = OSThread_t::THREAD_STATE::STATE_READY;
			__OSAddReadyThreadToRunQueue(thread);
		}
		else if (thread->state == OSThread_t::THREAD_STATE::STATE_MORIBUND || thread->state == OSThread_t::THREAD_STATE::STATE_NONE || thread->state == OSThread_t::THREAD_STATE::STATE_WAITING)
		{
			// thread exited or suspending/waiting
		}
		else
		{
			cemu_assert_debug(false);
		}

		thread->requestFlags = (OSThread_t::REQUEST_FLAG_BIT)(thread->requestFlags & OSThread_t::REQUEST_FLAG_CANCEL); // remove all flags except cancel flag

		// update total cycles
		sint64 executedCycles = (sint64)thread->quantumTicks - (sint64)hCPU->remainingCycles;
		executedCycles = std::max<sint64>(executedCycles, 0);
		if (executedCycles < (sint64)hCPU->skippedCycles)
			executedCycles = 0;
		else
			executedCycles -= hCPU->skippedCycles;
		thread->totalCycles += (uint64)executedCycles;
		// store context and set current thread to null
		__OSThreadStoreContext(hCPU, thread);
		OSSetCurrentThread(OSGetCoreId(), nullptr);
		PPCInterpreter_setCurrentInstance(nullptr);
	}

	void __OSLoadThread(OSThread_t* thread, PPCInterpreter_t* hCPU, uint32 coreIndex)
	{
		hCPU->LSQE = 1;
		hCPU->PSE = 1;
		hCPU->reservedMemAddr = MPTR_NULL;
		hCPU->reservedMemValue = 0;
		hCPU->spr.UPIR = coreIndex;
		hCPU->coreInterruptMask = 1;
		PPCInterpreter_setCurrentInstance(hCPU);
		OSSetCurrentThread(OSGetCoreId(), thread);
		__OSThreadLoadContext(hCPU, thread);
		thread->context.upir = coreIndex;
		thread->quantumTicks = ppcThreadQuantum;
		// statistics
		thread->wakeUpTime = PPCInterpreter_getMainCoreCycleCounter();
		thread->wakeUpCount = thread->wakeUpCount + 1;
	}

	uint32 s_lehmer_lcg[PPC_CORE_COUNT] = { 0 };

	void __OSThreadStartTimeslice(OSThread_t* thread, PPCInterpreter_t* hCPU)
	{
		uint32 coreIndex = PPCInterpreter_getCoreIndex(hCPU);
		// run one timeslice
		hCPU->remainingCycles = ppcThreadQuantum;
		hCPU->skippedCycles = 0;
		// we add a slight randomized variance to the thread quantum to avoid getting stuck in repeated code sequences where one or multiple threads always unload inside a lock
		// this was seen in Mario Party 10 during early boot where several OSLockMutex operations would align in such a way that one thread would never successfully acquire the lock
		if (s_lehmer_lcg[coreIndex] == 0)
			s_lehmer_lcg[coreIndex] = 12345;
		hCPU->remainingCycles += (s_lehmer_lcg[coreIndex] & 0x7F);
		s_lehmer_lcg[coreIndex] = (uint32)((uint64)s_lehmer_lcg[coreIndex] * 279470273ull % 0xfffffffbull);
	}

	OSThread_t* __OSGetNextRunableThread(uint32 coreIndex)
	{
		cemu_assert_debug(__OSHasSchedulerLock());
		// pick thread, then remove from run queue
		OSThreadQueue* runQueue = g_coreRunQueue.GetPtr() + coreIndex;

		OSThread_t* threadItr = runQueue->head.GetPtr();
		OSThread_t* selectedThread = nullptr;
		if (!threadItr)
			return nullptr;
		selectedThread = threadItr;

		while (threadItr)
		{
			if (threadItr->effectivePriority < selectedThread->effectivePriority)
				selectedThread = threadItr;
			threadItr = threadItr->linkRun[coreIndex].next.GetPtr();
		}

		cemu_assert_debug(selectedThread->state == OSThread_t::THREAD_STATE::STATE_READY);

		__OSRemoveThreadFromRunQueues(selectedThread);
		selectedThread->state = OSThread_t::THREAD_STATE::STATE_RUNNING;

		return selectedThread;
	}

    void __OSDeleteAllActivePPCThreads()
    {
        __OSLockScheduler();
        while(activeThreadCount > 0)
        {
            MEMPTR<OSThread_t> t{activeThread[0]};
            t->state = OSThread_t::THREAD_STATE::STATE_NONE;
            __OSDeactivateThread(t.GetPtr());
        }
        __OSUnlockScheduler();
    }

	void __OSCheckSystemEvents()
	{
		// AX update
		snd_core::AXOut_update();
		// alarm update
		coreinit::alarm_update();
		// nfp update
		nnNfp_update();
	}

	Fiber* g_idleLoopFiber[3]{};

	// idle fiber per core if no thread is runnable
	// this is necessary since we can't block in __OSThreadSwitchToNext() (__OSStoreThread + thread switch must happen inside same scheduler lock)
	void __OSThreadCoreIdle(void* unusedParam)
	{
		bool isMainCore = g_isMulticoreMode == false || t_assignedCoreIndex == 1;
		sint32 coreIndex = t_assignedCoreIndex;
		__OSUnlockScheduler();
		while (true)
		{
			if (!g_coreRunQueueThreadCount[coreIndex].isZero()) // avoid hammering the lock on the main core if there is no runable thread
			{
				__OSLockScheduler();
				OSThread_t* nextThread = __OSGetNextRunableThread(coreIndex);
				if (nextThread)
				{
					cemu_assert_debug(nextThread->state == OSThread_t::THREAD_STATE::STATE_RUNNING);
					__OSSwitchToThreadFiber(nextThread, coreIndex);
				}
				__OSUnlockScheduler();
			}
			if (isMainCore)
			{
				__OSCheckSystemEvents();
				if(g_isMulticoreMode == false)
					coreIndex = (coreIndex + 1) % 3;
			}
			else
			{
				// wait for semaphore (only in multicore mode)
				g_coreRunQueueThreadCount[t_assignedCoreIndex].waitUntilNonZero();
				if (!sSchedulerActive.load(std::memory_order::relaxed))
					Fiber::Switch(*t_schedulerFiber); // switch back to original thread to exit
			}
		}
	}

	void __OSThreadSwitchToNext()
	{
		cemu_assert_debug(__OSHasSchedulerLock());

		OSHostThread* hostThread = (OSHostThread*)Fiber::GetFiberPrivateData();
        cemu_assert_debug(hostThread);

		//if (ppcInterpreterCurrentInstance)
		//	debug_printf("Core %d store thread %08x (t = %d)\n", hostThread->ppcInstance.sprNew.UPIR, memory_getVirtualOffsetFromPointer(hostThread->thread), t_assignedCoreIndex);

		// store context of current thread
		__OSStoreThread(OSGetCurrentThread(), &hostThread->ppcInstance);
		cemu_assert_debug(PPCInterpreter_getCurrentInstance() == nullptr);

		if (!sSchedulerActive.load(std::memory_order::relaxed))
		{
			__OSUnlockScheduler();
			Fiber::Switch(*t_schedulerFiber); // switch back to original thread entry for it to exit
		}
		// choose core
		static uint32 _coreCounter = 0;

		sint32 coreIndex;
		if (g_isMulticoreMode)
			coreIndex = t_assignedCoreIndex;
		else
		{
			coreIndex = _coreCounter;
			_coreCounter = (_coreCounter + 1) % 3;
		}

		// if main thread then dont forget to do update checks
		bool isMainThread = g_isMulticoreMode == false || t_assignedCoreIndex == 1;

		// find next thread to run
		// for main thread we force switching to the idle loop since it calls __OSCheckSystemEvents()
		if(isMainThread)
			Fiber::Switch(*g_idleLoopFiber[t_assignedCoreIndex]);
		else if (OSThread_t* nextThread = __OSGetNextRunableThread(coreIndex))
		{
			cemu_assert_debug(nextThread->state == OSThread_t::THREAD_STATE::STATE_RUNNING);
			__OSSwitchToThreadFiber(nextThread, coreIndex);
		}
		else
		{
			Fiber::Switch(*g_idleLoopFiber[t_assignedCoreIndex]);
		}

		cemu_assert_debug(__OSHasSchedulerLock());	
		cemu_assert_debug(g_isMulticoreMode == false || hostThread->selectedCore == t_assignedCoreIndex);

		// received next time slice, load self again
		__OSLoadThread(hostThread->m_thread, &hostThread->ppcInstance, hostThread->selectedCore);
		__OSThreadStartTimeslice(hostThread->m_thread, &hostThread->ppcInstance);
	}

#ifdef __arm64__
	void __OSFiberThreadEntry(uint32 _high, uint32 _low)
	{
		uint64 _thread = (uint64) _high << 32 | _low;
#else
	void __OSFiberThreadEntry(void* _thread)
	{
#endif
		OSHostThread* hostThread = (OSHostThread*)_thread;

        #if defined(ARCH_X86_64)
		_mm_setcsr(_mm_getcsr() | 0x8000); // flush denormals to zero
        #endif

		PPCInterpreter_t* hCPU = &hostThread->ppcInstance;
		__OSLoadThread(hostThread->m_thread, hCPU, hostThread->selectedCore);
		__OSThreadStartTimeslice(hostThread->m_thread, &hostThread->ppcInstance);
		__OSUnlockScheduler(); // lock is always held when switching to a fiber, so we need to unlock it here
		while (true)
		{
			if (hCPU->remainingCycles > 0)
			{
				// try to enter recompiler immediately
				PPCRecompiler_attemptEnterWithoutRecompile(hCPU, hCPU->instructionPointer);
				// keep executing as long as there are cycles left
				while ((--hCPU->remainingCycles) >= 0)
					PPCInterpreterSlim_executeInstruction(hCPU);
			}

			// reset reservation
			hCPU->reservedMemAddr = 0;
			hCPU->reservedMemValue = 0;

			// reschedule
			__OSLockScheduler();
			__OSThreadSwitchToNext();
			__OSUnlockScheduler();
		}
	}

#if BOOST_OS_LINUX
	#include <unistd.h>
	#include <sys/prctl.h>

	std::vector<pid_t> g_schedulerThreadIds;
	std::mutex g_schedulerThreadIdsLock;

	std::vector<pid_t>& OSGetSchedulerThreadIds()
	{
		std::lock_guard schedulerThreadIdsLockGuard(g_schedulerThreadIdsLock);
		return g_schedulerThreadIds;
	}
#endif

	void OSSchedulerCoreEmulationThread(void* _assignedCoreIndex)
	{
		SetThreadName(fmt::format("OSSched[core={}]", (uintptr_t)_assignedCoreIndex).c_str());
		t_assignedCoreIndex = (sint32)(uintptr_t)_assignedCoreIndex;
        #if defined(ARCH_X86_64)
		_mm_setcsr(_mm_getcsr() | 0x8000); // flush denormals to zero
        #endif

#if BOOST_OS_LINUX
		if (g_gdbstub)
		{
			// need to allow the GDBStub to attach to our thread
			prctl(PR_SET_DUMPABLE, (unsigned long)1);
			prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY);
		}

		pid_t tid = gettid();
		{
			std::lock_guard schedulerThreadIdsLockGuard(g_schedulerThreadIdsLock);
			g_schedulerThreadIds.emplace_back(tid);
		}
#endif

		t_schedulerFiber = Fiber::PrepareCurrentThread();

		// create scheduler idle fiber and switch to it
		g_idleLoopFiber[t_assignedCoreIndex] = new Fiber(__OSThreadCoreIdle, nullptr, nullptr);
		cemu_assert_debug(PPCInterpreter_getCurrentInstance() == nullptr);
		__OSLockScheduler();
		Fiber::Switch(*g_idleLoopFiber[t_assignedCoreIndex]);
		// returned from scheduler loop, exit thread
		cemu_assert_debug(!__OSHasSchedulerLock());
	}

	std::vector<std::thread::native_handle_type> g_schedulerThreadHandles;

	std::vector<std::thread::native_handle_type>& OSGetSchedulerThreads()
	{
		return g_schedulerThreadHandles;
	}

	// starts PPC core emulation
	void OSSchedulerBegin(sint32 numCPUEmulationThreads)
	{
		std::unique_lock _lock(sSchedulerStateMtx);
		if (sSchedulerActive.exchange(true))
			return;
		cemu_assert_debug(numCPUEmulationThreads == 1 || numCPUEmulationThreads == 3);
		g_isMulticoreMode = numCPUEmulationThreads > 1;
		if (numCPUEmulationThreads == 1)
			sSchedulerThreads.emplace_back(OSSchedulerCoreEmulationThread, (void*)0);
		else if (numCPUEmulationThreads == 3)
		{
			for (size_t i = 0; i < 3; i++)
				sSchedulerThreads.emplace_back(OSSchedulerCoreEmulationThread, (void*)i);
		}
		else
			cemu_assert_debug(false);
		for (auto& it : sSchedulerThreads)
			g_schedulerThreadHandles.emplace_back(it.native_handle());
	}

    // shuts down all scheduler host threads and deletes all fibers and ppc threads
	void OSSchedulerEnd()
	{
		std::unique_lock _lock(sSchedulerStateMtx);
		sSchedulerActive.store(false);
		for (size_t i = 0; i < Espresso::CORE_COUNT; i++)
			g_coreRunQueueThreadCount[i].increment(); // make sure to wake up cores if they are paused and waiting for runnable threads
		// wait for threads to stop execution
		for (auto& threadItr : sSchedulerThreads)
			threadItr.join();
		sSchedulerThreads.clear();
		g_schedulerThreadHandles.clear();
#if BOOST_OS_LINUX
		{
			std::lock_guard schedulerThreadIdsLockGuard(g_schedulerThreadIdsLock);
			g_schedulerThreadIds.clear();
		}
#endif
		// clean up all fibers
		for (auto& it : g_idleLoopFiber)
		{
			delete it;
			it = nullptr;
		}
		for (auto& it : s_threadToFiber)
		{
			OSHostThread* hostThread = it.second;
			delete hostThread;
		}
		s_threadToFiber.clear();
	}

	SysAllocator<OSThread_t, PPC_CORE_COUNT> s_defaultThreads;
	SysAllocator<uint8, PPC_CORE_COUNT * 1024 * 1024> s_stack;

	OSThread_t* OSGetDefaultThread(sint32 coreIndex)
	{
		if (coreIndex < 0 || coreIndex >= PPC_CORE_COUNT)
			return nullptr;
		return s_defaultThreads.GetPtr() + coreIndex;
	}

	void* OSGetDefaultThreadStack(sint32 coreIndex, uint32& size)
	{
		if (coreIndex < 0 || coreIndex >= PPC_CORE_COUNT)
			return nullptr;
		size = 1024 * 1024;
		return s_stack.GetPtr() + coreIndex * size;
	}

	void OSInitThreadQueue(OSThreadQueue* threadQueue)
	{
		threadQueue->head = nullptr;
		threadQueue->tail = nullptr;
		threadQueue->userData = nullptr;
		threadQueue->ukn0C = 0;
	}
	
	void OSInitThreadQueueEx(OSThreadQueue* threadQueue, void* userData)
	{
		threadQueue->head = nullptr;
		threadQueue->tail = nullptr;
		threadQueue->userData = userData;
		threadQueue->ukn0C = 0;
	}
	
	/* Thread terminator threads (for calling thread deallocators) */
	struct TerminatorThread 
	{
		struct DeallocatorQueueEntry
		{
			DeallocatorQueueEntry() = default;
			DeallocatorQueueEntry(OSThread_t* thread, MEMPTR<void> stack, MEMPTR<void> deallocatorFunc) : thread(thread), stack(stack), deallocatorFunc(deallocatorFunc) {};

			OSThread_t* thread{};
			MEMPTR<void> stack;
			MEMPTR<void> deallocatorFunc;
		};

		SysAllocator<OSThread_t> terminatorThread;
		SysAllocator<uint8, 16 * 1024> threadStack;
		SysAllocator<char, 64> threadName;
		SysAllocator<OSSemaphore> semaphoreQueuedDeallocators;
		ConcurrentQueue<DeallocatorQueueEntry> queueDeallocators;
	};

	TerminatorThread s_terminatorThreads[PPC_CORE_COUNT];

	void __OSTerminatorThreadFunc(PPCInterpreter_t* hCPU)
	{
		uint32 coreIndex = OSGetCoreId();
		while (OSWaitSemaphore(s_terminatorThreads[coreIndex].semaphoreQueuedDeallocators.GetPtr()))
		{
			TerminatorThread::DeallocatorQueueEntry queueEntry;
			s_terminatorThreads[coreIndex].queueDeallocators.pop(queueEntry);
			PPCCoreCallback(queueEntry.deallocatorFunc, queueEntry.thread, queueEntry.stack);
		}
		osLib_returnFromFunction(hCPU, 0);
	}

	// queue thread deallocation to run after current thread finishes
	// the termination threads run at a higher priority on the same threads
	void __OSQueueThreadDeallocation(OSThread_t* thread)
	{
		uint32 coreIndex = OSGetCoreId();
		TerminatorThread::DeallocatorQueueEntry queueEntry(thread, thread->stackEnd, thread->deallocatorFunc);
		s_terminatorThreads[coreIndex].queueDeallocators.push(queueEntry);
		OSSignalSemaphoreInternal(s_terminatorThreads[coreIndex].semaphoreQueuedDeallocators.GetPtr(), false); // do not reschedule here! Current thread must not be interrupted otherwise deallocator will run too early
	}

	void __OSInitTerminatorThreads()
	{
		for (sint32 i = 0; i < PPC_CORE_COUNT; i++)
		{
			OSInitSemaphore(s_terminatorThreads[i].semaphoreQueuedDeallocators.GetPtr(), 0);
			sprintf(s_terminatorThreads[i].threadName.GetPtr(), "{SYS Thread Terminator Core %d}", i);
			OSCreateThreadType(s_terminatorThreads[i].terminatorThread.GetPtr(), PPCInterpreter_makeCallableExportDepr(__OSTerminatorThreadFunc), 0, nullptr, (uint8*)s_terminatorThreads[i].threadStack.GetPtr() + s_terminatorThreads[i].threadStack.GetByteSize(), (sint32)s_terminatorThreads[i].threadStack.GetByteSize(), 0, 1 << i, OSThread_t::THREAD_TYPE::TYPE_IO);
			OSSetThreadName(s_terminatorThreads[i].terminatorThread.GetPtr(), s_terminatorThreads[i].threadName.GetPtr());
			OSResumeThread(s_terminatorThreads[i].terminatorThread.GetPtr());
		}
	}

	SysAllocator<char, 64> _defaultThreadName[PPC_CORE_COUNT];

	void __OSInitDefaultThreads()
	{
		for (sint32 i = 0; i < PPC_CORE_COUNT; i++)
		{
			sprintf(_defaultThreadName[i].GetPtr(), "Default Core %d", i);
			OSThread_t* coreDefaultThread = coreinit::OSGetDefaultThread(i);
			uint32 stackSize;
			void* stackBase = coreinit::OSGetDefaultThreadStack(i, stackSize);
			coreinit::OSCreateThreadType(coreDefaultThread, MPTR_NULL, 0, nullptr, (uint8*)stackBase + stackSize, stackSize, 16, 1 << i, OSThread_t::THREAD_TYPE::TYPE_APP);
			coreinit::OSSetThreadName(coreDefaultThread, _defaultThreadName[i].GetPtr());
		}
	}

	void InitializeThread()
	{
		cafeExportRegister("coreinit", OSCreateThreadType, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSCreateThread, LogType::CoreinitThread);
		cafeExportRegister("coreinit", __OSCreateThreadType, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSExitThread, LogType::CoreinitThread);

		cafeExportRegister("coreinit", OSGetCurrentThread, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSSetThreadSpecific, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSGetThreadSpecific, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSSetThreadName, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSGetThreadName, LogType::CoreinitThread);

		cafeExportRegister("coreinit", OSRunThread, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSDetachThread, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSJoinThread, LogType::CoreinitThread);

		cafeExportRegister("coreinit", OSResumeThread, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSContinueThread, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSSuspendThread, LogType::CoreinitThread);
		cafeExportRegister("coreinit", __OSSuspendThreadNolock, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSSleepThread, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSWakeupThread, LogType::CoreinitThread);

		cafeExportRegister("coreinit", OSYieldThread, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSSleepTicks, LogType::CoreinitThread);

		cafeExportRegister("coreinit", OSSetThreadDeallocator, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSSetThreadCleanupCallback, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSSetThreadPriority, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSGetThreadPriority, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSSetThreadAffinity, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSGetThreadAffinity, LogType::CoreinitThread);

		cafeExportRegister("coreinit", OSIsThreadTerminated, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSIsThreadSuspended, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSCancelThread, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSTestThreadCancel, LogType::CoreinitThread);

		cafeExportRegister("coreinit", OSGetDefaultThread, LogType::CoreinitThread);

		// OSThreadQueue
		cafeExportRegister("coreinit", OSInitThreadQueue, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSInitThreadQueueEx, LogType::CoreinitThread);

		OSInitThreadQueue(g_activeThreadQueue.GetPtr());
		for (sint32 i = 0; i < PPC_CORE_COUNT; i++)
			OSInitThreadQueue(g_coreRunQueue.GetPtr() + i);

		for (sint32 i = 0; i < PPC_CORE_COUNT; i++)
			__currentCoreThread[i] = nullptr;

        __OSInitDefaultThreads();
		__OSInitTerminatorThreads();

    }
}
