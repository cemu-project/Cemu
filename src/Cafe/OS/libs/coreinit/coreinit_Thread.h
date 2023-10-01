#pragma once
#include "Cafe/HW/Espresso/Const.h"
#include "Cafe/OS/libs/coreinit/coreinit_Scheduler.h"

#define OS_CONTEXT_MAGIC_0					'OSCo'
#define OS_CONTEXT_MAGIC_1					'ntxt'

struct OSThread_t;

struct OSContextRegFPSCR_t
{
	// FPSCR is a 32bit register but it's stored as a 64bit double
	/* +0x00 */ uint32be padding;
	/* +0x04 */ uint32be fpscr;
};

struct OSContext_t
{
	/* +0x000 */ betype<uint32> magic0;
	/* +0x004 */ betype<uint32> magic1;
	/* +0x008 */ uint32 gpr[32];
	/* +0x088 */ uint32 cr;
	/* +0x08C */ uint32 lr;
	/* +0x090 */ uint32 ctr;
	/* +0x094 */ uint32 xer;
	/* +0x098 */ uint32 srr0;
	/* +0x09C */ uint32 srr1;
	/* +0x0A0 */ uint32 dsi_dsisr;
	/* +0x0A4 */ uint32 dsi_dar;
	/* +0x0A8 */ uint32 ukn0A8;
	/* +0x0AC */ uint32 ukn0AC;
	/* +0x0B0 */ OSContextRegFPSCR_t fpscr;
	/* +0x0B8 */ uint64be fp_ps0[32];
	/* +0x1B8 */ uint16be boostCount;
	/* +0x1BA */ uint16 state;			// OS_CONTEXT_STATE_*
	/* +0x1BC */ uint32 gqr[8];			// GQR/UGQR
	/* +0x1DC */ uint32be upir;			// set to current core index
	/* +0x1E0 */ uint64be fp_ps1[32];
	/* +0x2E0 */ uint64 uknTime2E0;
	/* +0x2E8 */ uint64 uknTime2E8;
	/* +0x2F0 */ uint64 uknTime2F0;
	/* +0x2F8 */ uint64 uknTime2F8;
	/* +0x300 */ uint32 error;         // returned by __gh_errno_ptr() (used by socketlasterr)
	/* +0x304 */ uint32be affinity;
	/* +0x308 */ uint32 ukn0308;
	/* +0x30C */ uint32 ukn030C;
	/* +0x310 */ uint32 ukn0310;
	/* +0x314 */ uint32 ukn0314;
	/* +0x318 */ uint32 ukn0318;
	/* +0x31C */ uint32 ukn031C;

	bool checkMagic()
	{
		return magic0 == (uint32)OS_CONTEXT_MAGIC_0 && magic1 == (uint32)OS_CONTEXT_MAGIC_1;
	}

	bool hasCoreAffinitySet(uint32 coreIndex) const
	{
		return (((uint32)affinity >> coreIndex) & 1) != 0;
	}

	void setAffinity(uint32 mask)
	{
		affinity = mask & 7;
	}

	uint32 getAffinity() const
	{
		return affinity;
	}
};

static_assert(sizeof(OSContext_t) == 0x320);

typedef struct
{
	/* +0x000 | +0x3A0 */ uint32 ukn000[0x68 / 4];
	/* +0x068 | +0x408 */ MEMPTR<void> eh_globals;
	/* +0x06C | +0x40C */ uint32 eh_mem_manage[9]; // struct
	/* +0x090 | +0x430 */ MPTR eh_store_globals[6];
	/* +0x0A8 | +0x448 */ MPTR eh_store_globals_tdeh[76];
}crt_t; // size: 0x1D8

static_assert(sizeof(crt_t) == 0x1D8, "");

#pragma pack(1)

namespace coreinit
{

	/********* OSThreadQueue *********/

	struct OSThreadLink
	{
		MEMPTR<struct OSThread_t> next;
		MEMPTR<struct OSThread_t> prev;
	};

	static_assert(sizeof(OSThreadLink) == 8);

	struct OSThreadQueueInternal
	{
		MEMPTR<OSThread_t> head;
		MEMPTR<OSThread_t> tail;

		bool isEmpty() const
		{
			cemu_assert_debug((!head.IsNull() == !tail.IsNull()) || (head.IsNull() == tail.IsNull()));
			return head.IsNull();
		}

		void addThread(OSThread_t* thread, OSThreadLink* threadLink);
		void addThreadByPriority(OSThread_t* thread, OSThreadLink* threadLink);
		void removeThread(OSThread_t* thread, OSThreadLink* threadLink);

		// puts the thread on the waiting queue and changes state to WAITING
		// relinquishes timeslice
		// always uses thread->waitQueueLink
		void queueAndWait(OSThread_t* thread);
		void queueOnly(OSThread_t* thread);

		// counterparts for queueAndWait
		void cancelWait(OSThread_t* thread);
		void wakeupEntireWaitQueue(bool reschedule);
		void wakeupSingleThreadWaitQueue(bool reschedule);

	private:
		OSThread_t* takeFirstFromQueue(size_t linkOffset)
		{
			cemu_assert_debug(__OSHasSchedulerLock());
			if (head == nullptr)
				return nullptr;
			OSThread_t* thread = head.GetPtr();
			OSThreadLink* link = _getThreadLink(thread, linkOffset);
			removeThread(thread, link);
			return thread;
		}

		static size_t getLinkOffset(OSThread_t* thread, OSThreadLink* threadLink)
		{
			cemu_assert_debug((void*)threadLink >= (void*)thread && (void*)threadLink < (void*)((uint8*)thread + 0x680));
			return (uint8*)threadLink - (uint8*)thread;
		}

		static OSThreadLink* _getThreadLink(OSThread_t* thread, size_t linkOffset)
		{
			return (OSThreadLink*)((uint8*)thread + linkOffset);
		}

		void _debugCheckChain(OSThread_t* thread, OSThreadLink* threadLink)
		{
#ifdef CEMU_DEBUG_ASSERT
			cemu_assert_debug(tail.IsNull() == head.IsNull());
			size_t linkOffset = getLinkOffset(thread, threadLink);
			// expects thread to be in the chain
			OSThread_t* threadItr = head.GetPtr();
			while (threadItr)
			{
				if (threadItr == thread)
					return;
				threadItr = _getThreadLink(threadItr, linkOffset)->next.GetPtr();
			}
			cemu_assert_debug(false); // thread not in list!
#endif
		}
	};

	static_assert(sizeof(OSThreadQueueInternal) == 0x8);

	struct OSThreadQueueSmall : public OSThreadQueueInternal
	{
		// no extra members
	};

	static_assert(sizeof(OSThreadQueueSmall) == 8);
	static_assert(offsetof(OSThreadQueueSmall, head) == 0x0);
	static_assert(offsetof(OSThreadQueueSmall, tail) == 0x4);

	struct OSThreadQueue : public OSThreadQueueInternal
	{
		MEMPTR<void> userData;
		uint32be ukn0C;
	};

	static_assert(sizeof(OSThreadQueue) == 0x10);
	static_assert(offsetof(OSThreadQueue, head) == 0x0);
	static_assert(offsetof(OSThreadQueue, tail) == 0x4);
	static_assert(offsetof(OSThreadQueue, userData) == 0x8);
	static_assert(offsetof(OSThreadQueue, ukn0C) == 0xC);

	/********* OSMutex *********/

	struct OSMutex
	{
		/* +0x00 */ uint32				magic;
		/* +0x04 */ MEMPTR<void>		userData;
		/* +0x08 */ uint32be			ukn08;
		/* +0x0C */ OSThreadQueue		threadQueue;
		/* +0x1C */ MEMPTR<OSThread_t>	owner;
		/* +0x20 */ sint32be			lockCount;
		/* +0x24 */ MEMPTR<OSMutex>		next;
		/* +0x28 */ MEMPTR<OSMutex>		prev;

	}; // size: 0x2C

	static_assert(sizeof(OSMutex) == 0x2C);

	struct OSMutexQueue
	{
		MEMPTR<OSMutex> head;
		MEMPTR<OSMutex> tail;
		MEMPTR<void> ukn08;
		uint32be ukn0C;

		bool isEmpty() const
		{
			cemu_assert_debug((!head.IsNull() == !tail.IsNull()) || (head.IsNull() == tail.IsNull()));
			return head.IsNull();
		}

		void addMutex(OSMutex* mutex)
		{
			cemu_assert_debug(__OSHasSchedulerLock());
			// insert at end
			if (tail.IsNull())
			{
				mutex->next = nullptr;
				mutex->prev = nullptr;
				head = mutex;
				tail = mutex;
			}
			else
			{
				tail->next = mutex;
				mutex->prev = tail;
				mutex->next = nullptr;
				tail = mutex;
			}
		}

		void removeMutex(OSMutex* mutex)
		{
			cemu_assert_debug(__OSHasSchedulerLock());
			cemu_assert_debug(!head.IsNull() && !tail.IsNull());
			if (mutex->prev)
				mutex->prev->next = mutex->next;
			else
				head = mutex->next;
			if (mutex->next)
				mutex->next->prev = mutex->prev;
			else
				tail = mutex->prev;
			mutex->next = nullptr;
			mutex->prev = nullptr;
		}

		OSMutex* getFirst()
		{
			return head.GetPtr();
		}
	};

	static_assert(sizeof(OSMutexQueue) == 0x10);

	/********* OSFastMutex *********/

	struct OSFastMutexLink
	{
		/* +0x00 */ MEMPTR<struct OSMutex> next;
		/* +0x04 */ MEMPTR<struct OSMutex> prev;
	};

	struct OSFastMutex
	{
		/* +0x00 */ uint32be magic;
		/* +0x04 */ MEMPTR<void> userData;
		/* +0x08 */ uint32be contendedState; // tracks current contention state
		/* +0x0C */ OSThreadQueueSmall threadQueueSmall;
		/* +0x14 */ OSFastMutexLink ownedLink; // part of thread->fastMutexOwnedQueue
		/* +0x1C */ MEMPTR<OSThread_t> owner;
		/* +0x20 */ uint32be lockCount;
		/* +0x24 */ OSFastMutexLink contendedLink;
	};

	static_assert(sizeof(OSFastMutex) == 0x2C);

	/********* OSEvent *********/

	struct OSEvent
	{
		enum class EVENT_MODE : uint32
		{
			MODE_MANUAL = 0,
			MODE_AUTO = 1,
		};

		enum class EVENT_STATE : uint32
		{
			STATE_NOT_SIGNALED = 0,
			STATE_SIGNALED = 1
		};

		/* +0x00 */ uint32be magic; // 'eVnT'
		/* +0x04 */ MEMPTR<void> userData;
		/* +0x08 */ uint32be ukn08;
		/* +0x0C */ betype<EVENT_STATE> state; // 0 -> not signaled, 1 -> signaled
		/* +0x10 */ OSThreadQueue threadQueue;
		/* +0x20 */ betype<EVENT_MODE> mode;
	};

	static_assert(sizeof(OSEvent) == 0x24);

	/********* OSRendezvous *********/

	struct OSRendezvous
	{
		/* +0x00 */ uint32be coreHit[3];
		/* +0x0C */ MEMPTR<void> userData;
	};

	static_assert(sizeof(OSRendezvous) == 0x10);

	/********* OSCond *********/

	struct OSCond
	{
		uint32be magic;
		MEMPTR<void> userData;
		uint32be ukn08;
		OSThreadQueue threadQueue;
	};

	static_assert(sizeof(OSCond) == 0x1C);

	/********* OSSemaphore *********/

	struct OSSemaphore
	{
		uint32be magic;
		MEMPTR<void> userData;
		uint32be ukn08;
		sint32be count;
		OSThreadQueue threadQueue;
	};

	static_assert(sizeof(OSSemaphore) == 0x20);

	/********* OSFastCond *********/

	struct OSFastCond
	{
		uint32be magic;
		MEMPTR<void> userData;
		uint32be ukn08;
		OSThreadQueue threadQueue;
	};

	static_assert(sizeof(OSFastCond) == 0x1C);

};

struct OSThread_t
{
	enum class THREAD_TYPE : uint32
	{
		TYPE_DRIVER = 0,
		TYPE_IO = 1,
		TYPE_APP = 2
	};

	enum class THREAD_STATE : uint8
	{
		STATE_NONE = 0,
		STATE_READY = 1,
		STATE_RUNNING = 2,
		STATE_WAITING = 4,
		STATE_MORIBUND = 8,
	};

	enum ATTR_BIT : uint32
	{
		ATTR_AFFINITY_CORE0 = 0x1,
		ATTR_AFFINITY_CORE1 = 0x2,
		ATTR_AFFINITY_CORE2 = 0x4,
		ATTR_DETACHED		= 0x8,
		// more flags?
	};

	enum REQUEST_FLAG_BIT : uint32
	{
		REQUEST_FLAG_NONE = 0,
		REQUEST_FLAG_SUSPEND = 1,
		REQUEST_FLAG_CANCEL = 2,
	};

	static sint32 GetTypePriorityBase(THREAD_TYPE threadType)
	{
		if (threadType == OSThread_t::THREAD_TYPE::TYPE_DRIVER)
			return 0;
		else if (threadType == OSThread_t::THREAD_TYPE::TYPE_IO)
			return 32;
		else if (threadType == OSThread_t::THREAD_TYPE::TYPE_APP)
			return 64;
		return 0;
	}

    void SetMagic()
    {
        context.magic0 = OS_CONTEXT_MAGIC_0;
        context.magic1 = OS_CONTEXT_MAGIC_1;
        magic = 'tHrD';
    }

    bool IsValidMagic() const
    {
        return magic == 'tHrD' && context.magic0 == OS_CONTEXT_MAGIC_0 && context.magic1 == OS_CONTEXT_MAGIC_1;
    }

	/* +0x000 */ OSContext_t						context;
	/* +0x320 */ uint32be							magic;								// 'tHrD'
	/* +0x324 */ betype<THREAD_STATE>				state;
	/* +0x325 */ uint8								attr;
	/* +0x326 */ uint16be							id;									// Warriors Orochi 3 uses this to identify threads. Seems like this is always set to 0x8000 ?
	/* +0x328 */ betype<sint32>						suspendCounter;
	/* +0x32C */ sint32be							effectivePriority;					// effective priority (lower is higher)
	/* +0x330 */ sint32be							basePriority;						// base priority (lower is higher)
	/* +0x334 */ uint32be							exitValue;							// set by OSExitThread() or by returning from the thread entrypoint

	/* +0x338 */ MEMPTR<coreinit::OSThreadQueue>	currentRunQueue[Espresso::CORE_COUNT];	// points to the OSThreadQueue on which this thread is (null if not in queue)
	/* +0x344 */ coreinit::OSThreadLink				linkRun[Espresso::CORE_COUNT];

	// general wait queue / thread dependency queue
	/* +0x35C */ MEMPTR<coreinit::OSThreadQueueInternal> currentWaitQueue;				// shared between OSThreadQueue and OSThreadQueueSmall
	/* +0x360 */ coreinit::OSThreadLink				waitQueueLink;

	/* +0x368 */ coreinit::OSThreadQueue			joinQueue;

	/* +0x378 */ MEMPTR<coreinit::OSMutex>			waitingForMutex;					// set when thread is waiting for OSMutex to unlock
	/* +0x37C */ coreinit::OSMutexQueue				mutexQueue;

	/* +0x38C */ coreinit::OSThreadLink				activeThreadChain;					// queue of active threads (g_activeThreadQueue)

	/* +0x394 */ MPTR								stackBase;							// upper limit of stack
	/* +0x398 */ MPTR								stackEnd;							// lower limit of stack

	/* +0x39C */ MPTR								entrypoint;
	/* +0x3A0 */ crt_t								crt;

	/* +0x578 */ sint32								alarmRelatedUkn;
	/* +0x57C */ std::array<MEMPTR<void>, 16>		specificArray;
	/* +0x5BC */ betype<THREAD_TYPE>				type;
	/* +0x5C0 */ MEMPTR<char>						threadName;
	/* +0x5C4 */ MPTR								waitAlarm;							// used only by OSWaitEventWithTimeout/OSSignalEvent ?

	/* +0x5C8 */ uint32								userStackPointer;

	/* +0x5CC */ MEMPTR<void>						cleanupCallback2;
	/* +0x5D0 */ MEMPTR<void>						deallocatorFunc;

	/* +0x5D4 */ uint32								stateFlags;						// 0x5D4 | various flags? Controls if canceling/suspension is allowed (at cancel points) or not? If 1 -> Cancel/Suspension not allowed, if 0 -> Cancel/Suspension allowed
	/* +0x5D8 */ betype<REQUEST_FLAG_BIT>			requestFlags;
	/* +0x5DC */ sint32								pendingSuspend;
	/* +0x5E0 */ sint32								suspendResult;
	/* +0x5E4 */ coreinit::OSThreadQueue			suspendQueue;
	/* +0x5F4 */ uint32								_padding5F4;
	/* +0x5F8 */ uint64be							quantumTicks;
	/* +0x600 */ uint64								coretimeSumQuantumStart;

	/* +0x608 */ uint64be							wakeUpCount;						// number of times the thread entered running state
	/* +0x610 */ uint64be							totalCycles;						// sum of cycles this thread was active since it was created
	/* +0x618 */ uint64be							wakeUpTime;							// time when thread first became active (Should only be set once(?) but we currently set this on every timeslice since thats more useful for debugging)
	/* +0x620 */ uint64								wakeTimeRelatedUkn1;
	/* +0x628 */ uint64								wakeTimeRelatedUkn2;

	// set via OSSetExceptionCallback
	/* +0x630 */ MPTR								dsiCallback[Espresso::CORE_COUNT];
	/* +0x63C */ MPTR								isiCallback[Espresso::CORE_COUNT];
	/* +0x648 */ MPTR								programCallback[Espresso::CORE_COUNT];
	/* +0x654 */ MPTR								perfMonCallback[Espresso::CORE_COUNT];

	/* +0x660 */ uint32								ukn660;

	// TLS
	/* +0x664 */ uint16								numAllocatedTLSBlocks;
	/* +0x666 */ sint16								tlsStatus;
	/* +0x668 */ MPTR								tlsBlocksMPTR;

	/* +0x66C */ MEMPTR<coreinit::OSFastMutex>		waitingForFastMutex;
	/* +0x670 */ coreinit::OSFastMutexLink			contendedFastMutex;
	/* +0x678 */ coreinit::OSFastMutexLink			ownedFastMutex;

	/* +0x680 */ uint32								padding680[28 / 4];
};

static_assert(sizeof(OSThread_t) == 0x6A0-4); // todo - determine correct size

namespace coreinit
{
	void InitializeThread();
	void InitializeConcurrency();

	bool __CemuIsMulticoreMode();

	OSThread_t* OSGetDefaultThread(sint32 coreIndex);
	void* OSGetDefaultThreadStack(sint32 coreIndex, uint32& size);

	bool OSCreateThreadType(OSThread_t* thread, MPTR entryPoint, sint32 numParam, void* ptrParam, void* stackTop2, sint32 stackSize, sint32 priority, uint32 attr, OSThread_t::THREAD_TYPE threadType);
	void OSCreateThreadInternal(OSThread_t* thread, uint32 entryPoint, MPTR stackLowerBaseAddr, uint32 stackSize, uint8 affinityMask, OSThread_t::THREAD_TYPE threadType);
	bool OSRunThread(OSThread_t* thread, MPTR funcAddress, sint32 numParam, void* ptrParam);
	void OSExitThread(sint32 exitValue);
	void OSDetachThread(OSThread_t* thread);

	OSThread_t* OSGetCurrentThread();
	void OSSetCurrentThread(uint32 coreIndex, OSThread_t* thread);

	void __OSSetThreadBasePriority(OSThread_t* thread, sint32 newPriority);
	void __OSUpdateThreadEffectivePriority(OSThread_t* thread);

	bool OSSetThreadPriority(OSThread_t* thread, sint32 newPriority);
	uint32 OSGetThreadAffinity(OSThread_t* thread);

	void OSSetThreadName(OSThread_t* thread, char* name);
	char* OSGetThreadName(OSThread_t* thread);

	sint32 __OSResumeThreadInternal(OSThread_t* thread, sint32 resumeCount);
	sint32 OSResumeThread(OSThread_t* thread);
	void OSContinueThread(OSThread_t* thread);
	void __OSSuspendThreadInternal(OSThread_t* thread);
	void __OSSuspendThreadNolock(OSThread_t* thread);
	void OSSuspendThread(OSThread_t* thread);
	void OSSleepThread(OSThreadQueue* threadQueue);
	void OSWakeupThread(OSThreadQueue* threadQueue);

	void OSTestThreadCancelInternal();

	void OSYieldThread();
	void OSSleepTicks(uint64 ticks);

	bool OSIsThreadTerminated(OSThread_t* thread);
	bool OSIsThreadSuspended(OSThread_t* thread);
    bool OSIsThreadRunningNoLock(OSThread_t* thread);
	bool OSIsThreadRunning(OSThread_t* thread);

	// OSThreadQueue
	void OSInitThreadQueue(OSThreadQueue* threadQueue);
	void OSInitThreadQueueEx(OSThreadQueue* threadQueue, void* userData);

	// OSEvent
	void OSInitEvent(OSEvent* event, OSEvent::EVENT_STATE initialState, OSEvent::EVENT_MODE mode);
	void OSInitEventEx(OSEvent* event, OSEvent::EVENT_STATE initialState, OSEvent::EVENT_MODE mode, void* userData);
	void OSResetEvent(OSEvent* event);
	void OSWaitEventInternal(OSEvent* event); // assumes lock is already held
	void OSWaitEvent(OSEvent* event);
	bool OSWaitEventWithTimeout(OSEvent* event, uint64 timeout);
	void OSSignalEventInternal(OSEvent* event); // assumes lock is already held
	void OSSignalEvent(OSEvent* event);
	void OSSignalEventAllInternal(OSEvent* event); // assumes lock is already held
	void OSSignalEventAll(OSEvent* event);

	// OSRendezvous
	void OSInitRendezvous(OSRendezvous* rendezvous);
	bool OSWaitRendezvous(OSRendezvous* rendezvous, uint32 coreMask);

	// OSMutex
	void OSInitMutex(OSMutex* mutex);
	void OSInitMutexEx(OSMutex* mutex, void* userData);
	void OSLockMutex(OSMutex* mutex);
	bool OSTryLockMutex(OSMutex* mutex);
	void OSUnlockMutex(OSMutex* mutex);
	void OSUnlockMutexInternal(OSMutex* mutex);

	// OSCond
	void OSInitCond(OSCond* cond);
	void OSInitCondEx(OSCond* cond, void* userData);
	void OSSignalCond(OSCond* cond);
	void OSWaitCond(OSCond* cond, OSMutex* mutex);

	// OSSemaphore
	void OSInitSemaphore(OSSemaphore* semaphore, sint32 initialCount);
	void OSInitSemaphoreEx(OSSemaphore* semaphore, sint32 initialCount, void* userData);
	sint32 OSWaitSemaphoreInternal(OSSemaphore* semaphore);
	sint32 OSWaitSemaphore(OSSemaphore* semaphore);
	sint32 OSTryWaitSemaphore(OSSemaphore* semaphore);
	sint32 OSSignalSemaphore(OSSemaphore* semaphore);
	sint32 OSSignalSemaphoreInternal(OSSemaphore* semaphore, bool reschedule);
	sint32 OSGetSemaphoreCount(OSSemaphore* semaphore);

	// OSFastMutex
	void OSFastMutex_Init(OSFastMutex* fastMutex, void* userData);
	void OSFastMutex_Lock(OSFastMutex* fastMutex);
	bool OSFastMutex_TryLock(OSFastMutex* fastMutex);
	void OSFastMutex_Unlock(OSFastMutex* fastMutex);

	// OSFastCond
	void OSFastCond_Init(OSFastCond* fastCond, void* userData);
	void OSFastCond_Wait(OSFastCond* fastCond, OSFastMutex* fastMutex);
	void OSFastCond_Signal(OSFastCond* fastCond);

	// scheduler
	void OSSchedulerBegin(sint32 numCPUEmulationThreads);
	void OSSchedulerEnd();

	// internal
	void __OSAddReadyThreadToRunQueue(OSThread_t* thread);
	bool __OSCoreShouldSwitchToThread(OSThread_t* currentThread, OSThread_t* newThread);
	void __OSQueueThreadDeallocation(OSThread_t* thread);

    bool __OSIsThreadActive(OSThread_t* thread);
	void __OSDeleteAllActivePPCThreads();
}

#pragma pack()

// deprecated / clean up required
void coreinit_suspendThread(OSThread_t* OSThreadBE, sint32 count = 1);
void coreinit_resumeThread(OSThread_t* OSThreadBE, sint32 count = 1);

OSThread_t* coreinitThread_getCurrentThreadDepr(PPCInterpreter_t* hCPU);

extern MPTR activeThread[256];
extern sint32 activeThreadCount;

extern SlimRWLock srwlock_activeThreadList;