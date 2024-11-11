#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/coreinit/coreinit_Alarm.h"
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"
#include "Cafe/OS/libs/coreinit/coreinit_MessageQueue.h"
#include "Cafe/OS/libs/coreinit/coreinit_Misc.h"
#include "Cafe/OS/libs/coreinit/coreinit_Memory.h"
#include "Cafe/OS/libs/coreinit/coreinit_MEM_ExpHeap.h"
#include "Cafe/OS/libs/coreinit/coreinit_Time.h"
#include "Cafe/OS/libs/coreinit/coreinit_FG.h"
#include "Cafe/OS/libs/coreinit/coreinit_DynLoad.h"
#include "Cafe/OS/libs/gx2/GX2_Misc.h"
#include "Cafe/OS/RPL/rpl.h"
#include "Common/CafeString.h"
#include "proc_ui.h"

// proc_ui is a utility wrapper to help apps with the transition between foreground and background
// some games (like Xenoblades Chronicles X) bypass proc_ui.rpl and listen to OSGetSystemMessageQueue() directly

using namespace coreinit;

namespace proc_ui
{
	enum class ProcUICoreThreadCommand
	{
		AcquireForeground = 0x0,
		ReleaseForeground = 0x1,
		Exit = 0x2,
		NetIoStart = 0x3,
		NetIoStop = 0x4,
		HomeButtonDenied = 0x5,
		Initial = 0x6
	};

	struct ProcUIInternalCallbackEntry
	{
		coreinit::OSAlarm_t alarm;
		uint64be tickDelay;
		MEMPTR<void> funcPtr;
		MEMPTR<void> userParam;
		sint32be priority;
		MEMPTR<ProcUIInternalCallbackEntry> next;
	};
	static_assert(sizeof(ProcUIInternalCallbackEntry) == 0x70);

	struct ProcUICallbackList
	{
		MEMPTR<ProcUIInternalCallbackEntry> first;
	};
	static_assert(sizeof(ProcUICallbackList) == 0x4);

	std::atomic_bool s_isInitialized;
	bool s_isInForeground;
	bool s_isInShutdown;
	bool s_isForegroundProcess;
	bool s_previouslyWasBlocking;
	ProcUIStatus s_currentProcUIStatus;
	MEMPTR<void> s_saveCallback; // no param and no return value, set by ProcUIInit()
	MEMPTR<void> s_saveCallbackEx; // with custom param and return value, set by ProcUIInitEx()
	MEMPTR<void> s_saveCallbackExUserParam;
	MEMPTR<coreinit::OSMessageQueue> s_systemMessageQueuePtr;
	SysAllocator<coreinit::OSEvent> s_eventStateMessageReceived;
	SysAllocator<coreinit::OSEvent> s_eventWaitingBeforeReleaseForeground;
	SysAllocator<coreinit::OSEvent> s_eventBackgroundThreadGotMessage;
	// procUI core threads
	uint32 s_coreThreadStackSize;
	bool s_coreThreadsCreated;
	std::atomic<ProcUICoreThreadCommand> s_commandForCoreThread;
	SysAllocator<OSThread_t> s_coreThreadArray[Espresso::CORE_COUNT];
	MEMPTR<void> s_coreThreadStackPerCore[Espresso::CORE_COUNT];
	SysAllocator<CafeString<22>> s_coreThread0NameBuffer;
	SysAllocator<CafeString<22>> s_coreThread1NameBuffer;
	SysAllocator<CafeString<22>> s_coreThread2NameBuffer;
	SysAllocator<coreinit::OSEvent> s_eventCoreThreadsNewCommandReady;
	SysAllocator<coreinit::OSEvent> s_eventCoreThreadsCommandDone;
	SysAllocator<coreinit::OSRendezvous> s_coreThreadRendezvousA;
	SysAllocator<coreinit::OSRendezvous> s_coreThreadRendezvousB;
	SysAllocator<coreinit::OSRendezvous> s_coreThreadRendezvousC;
	// background thread
	MEMPTR<void> s_backgroundThreadStack;
	SysAllocator<OSThread_t> s_backgroundThread;
	// user defined heap
	MEMPTR<void> s_memoryPoolHeapPtr;
	MEMPTR<void> s_memAllocPtr;
	MEMPTR<void> s_memFreePtr;
	// draw done release
	bool s_drawDoneReleaseCalled;
	// memory storage
	MEMPTR<void> s_bucketStorageBasePtr;
	MEMPTR<void> s_mem1StorageBasePtr;
	// callbacks
	ProcUICallbackList s_callbacksType0_AcquireForeground[Espresso::CORE_COUNT];
	ProcUICallbackList s_callbacksType1_ReleaseForeground[Espresso::CORE_COUNT];
	ProcUICallbackList s_callbacksType2_Exit[Espresso::CORE_COUNT];
	ProcUICallbackList s_callbacksType3_NetIoStart[Espresso::CORE_COUNT];
	ProcUICallbackList s_callbacksType4_NetIoStop[Espresso::CORE_COUNT];
	ProcUICallbackList s_callbacksType5_HomeButtonDenied[Espresso::CORE_COUNT];
	ProcUICallbackList* const s_CallbackTables[stdx::to_underlying(ProcUICallbackId::COUNT)] =
		{s_callbacksType0_AcquireForeground, s_callbacksType1_ReleaseForeground, s_callbacksType2_Exit, s_callbacksType3_NetIoStart, s_callbacksType4_NetIoStop, s_callbacksType5_HomeButtonDenied};
	ProcUICallbackList s_backgroundCallbackList;
	// driver
	bool s_driverIsActive;
	uint32be s_driverArgUkn1;
	uint32be s_driverArgUkn2;
	bool s_driverInBackground;
	SysAllocator<OSDriverInterface> s_ProcUIDriver;
	SysAllocator<CafeString<16>> s_ProcUIDriverName;


	void* _AllocMem(uint32 size)
	{
		MEMPTR<void> r{PPCCoreCallback(s_memAllocPtr, size)};
		return r.GetPtr();
	}

	void _FreeMem(void* ptr)
	{
		PPCCoreCallback(s_memFreePtr.GetMPTR(), ptr);
	}

	void ClearCallbacksWithoutMemFree()
	{
		for (sint32 coreIndex = 0; coreIndex < Espresso::CORE_COUNT; coreIndex++)
		{
			for (sint32 i = 0; i < stdx::to_underlying(ProcUICallbackId::COUNT); i++)
				s_CallbackTables[i][coreIndex].first = nullptr;
		}
		s_backgroundCallbackList.first = nullptr;
	}

	void ShutdownThreads()
	{
		if ( !s_coreThreadsCreated)
			return;
		s_commandForCoreThread = ProcUICoreThreadCommand::Initial;
		coreinit::OSMemoryBarrier();
		OSSignalEvent(&s_eventCoreThreadsNewCommandReady);
		for (sint32 coreIndex = 0; coreIndex < Espresso::CORE_COUNT; coreIndex++)
		{
			coreinit::OSJoinThread(&s_coreThreadArray[coreIndex], nullptr);
			for (sint32 i = 0; i < stdx::to_underlying(ProcUICallbackId::COUNT); i++)
			{
				s_CallbackTables[i][coreIndex].first = nullptr; // memory is not cleanly released?
			}
		}
		OSResetEvent(&s_eventCoreThreadsNewCommandReady);
		for (sint32 coreIndex = 0; coreIndex < Espresso::CORE_COUNT; coreIndex++)
		{
			_FreeMem(s_coreThreadStackPerCore[coreIndex]);
			s_coreThreadStackPerCore[coreIndex] = nullptr;
		}
		_FreeMem(s_backgroundThreadStack);
		s_backgroundThreadStack = nullptr;
		s_backgroundCallbackList.first = nullptr; // memory is not cleanly released?
		s_coreThreadsCreated = false;
	}

	void DoCallbackChain(ProcUIInternalCallbackEntry* entry)
	{
		while (entry)
		{
			uint32 r = PPCCoreCallback(entry->funcPtr, entry->userParam);
			if ( r )
				cemuLog_log(LogType::APIErrors, "ProcUI: Callback returned error {}\n", r);
			entry = entry->next;
		}
	}

	void AlarmDoBackgroundCallback(PPCInterpreter_t* hCPU)
	{
		coreinit::OSAlarm_t* arg = MEMPTR<coreinit::OSAlarm_t>(hCPU->gpr[3]);
		ProcUIInternalCallbackEntry* entry = (ProcUIInternalCallbackEntry*)arg;
		uint32 r = PPCCoreCallback(entry->funcPtr, entry->userParam);
		if ( r )
			cemuLog_log(LogType::APIErrors, "ProcUI: Background callback returned error {}\n", r);
		osLib_returnFromFunction(hCPU, 0); // return type is void
	}

	void StartBackgroundAlarms()
	{
		ProcUIInternalCallbackEntry* cb = s_backgroundCallbackList.first;
		while(cb)
		{
			coreinit::OSCreateAlarm(&cb->alarm);
			uint64 currentTime = coreinit::OSGetTime();
			coreinit::OSSetPeriodicAlarm(&cb->alarm, currentTime, cb->tickDelay, RPLLoader_MakePPCCallable(AlarmDoBackgroundCallback));
			cb = cb->next;
		}
	}

	void CancelBackgroundAlarms()
	{
		ProcUIInternalCallbackEntry* entry = s_backgroundCallbackList.first;
		while (entry)
		{
			OSCancelAlarm(&entry->alarm);
			entry = entry->next;
		}
	}

	void ProcUICoreThread(PPCInterpreter_t* hCPU)
	{
		uint32 coreIndex = hCPU->gpr[3];
		cemu_assert_debug(coreIndex == OSGetCoreId());
		while (true)
		{
			OSWaitEvent(&s_eventCoreThreadsNewCommandReady);
			ProcUIInternalCallbackEntry* cbChain = nullptr;
			cemuLog_logDebug(LogType::Force, "ProcUI: Core {} got command {}", coreIndex, (uint32)s_commandForCoreThread.load());
			auto cmd = s_commandForCoreThread.load();
			switch(cmd)
			{
			case ProcUICoreThreadCommand::Initial:
			{
				// signal to shut down thread
				osLib_returnFromFunction(hCPU, 0);
				return;
			}
			case ProcUICoreThreadCommand::AcquireForeground:
				cbChain = s_callbacksType0_AcquireForeground[coreIndex].first;
				break;
			case ProcUICoreThreadCommand::ReleaseForeground:
				cbChain = s_callbacksType1_ReleaseForeground[coreIndex].first;
				break;
			case ProcUICoreThreadCommand::Exit:
				cbChain = s_callbacksType2_Exit[coreIndex].first;
				break;
			case ProcUICoreThreadCommand::NetIoStart:
				cbChain = s_callbacksType3_NetIoStart[coreIndex].first;
				break;
			case ProcUICoreThreadCommand::NetIoStop:
				cbChain = s_callbacksType4_NetIoStop[coreIndex].first;
				break;
			case ProcUICoreThreadCommand::HomeButtonDenied:
				cbChain = s_callbacksType5_HomeButtonDenied[coreIndex].first;
				break;
			default:
				cemu_assert_suspicious(); // invalid command
			}
			if(cmd == ProcUICoreThreadCommand::AcquireForeground)
			{
				if (coreIndex == 2)
					CancelBackgroundAlarms();
				cbChain = s_callbacksType0_AcquireForeground[coreIndex].first;
			}
			else if(cmd == ProcUICoreThreadCommand::ReleaseForeground)
			{
				if (coreIndex == 2)
					StartBackgroundAlarms();
				cbChain = s_callbacksType1_ReleaseForeground[coreIndex].first;
			}
			DoCallbackChain(cbChain);
			OSWaitRendezvous(&s_coreThreadRendezvousA, 7);
			if ( !coreIndex )
			{
				OSInitRendezvous(&s_coreThreadRendezvousC);
				OSResetEvent(&s_eventCoreThreadsNewCommandReady);
			}
			OSWaitRendezvous(&s_coreThreadRendezvousB, 7);
			if ( !coreIndex )
			{
				OSInitRendezvous(&s_coreThreadRendezvousA);
				OSSignalEvent(&s_eventCoreThreadsCommandDone);
			}
			OSWaitRendezvous(&s_coreThreadRendezvousC, 7);
			if ( !coreIndex )
				OSInitRendezvous(&s_coreThreadRendezvousB);
			if (cmd == ProcUICoreThreadCommand::ReleaseForeground)
			{
				OSWaitEvent(&s_eventWaitingBeforeReleaseForeground);
				OSReleaseForeground();
			}
		}
		osLib_returnFromFunction(hCPU, 0);
	}

	void RecreateProcUICoreThreads()
	{
		ShutdownThreads();
		for (sint32 coreIndex = 0; coreIndex < Espresso::CORE_COUNT; coreIndex++)
		{
			s_coreThreadStackPerCore[coreIndex] = _AllocMem(s_coreThreadStackSize);
		}
		s_backgroundThreadStack = _AllocMem(s_coreThreadStackSize);
		for (sint32 coreIndex = 0; coreIndex < Espresso::CORE_COUNT; coreIndex++)
		{
			__OSCreateThreadType(&s_coreThreadArray[coreIndex], RPLLoader_MakePPCCallable(ProcUICoreThread), coreIndex, nullptr,
				(uint8*)s_coreThreadStackPerCore[coreIndex].GetPtr() + s_coreThreadStackSize, s_coreThreadStackSize, 16,
				(1<<coreIndex), OSThread_t::THREAD_TYPE::TYPE_DRIVER);
			OSResumeThread(&s_coreThreadArray[coreIndex]);
		}
		s_coreThread0NameBuffer->assign("{SYS ProcUI Core 0}");
		s_coreThread1NameBuffer->assign("{SYS ProcUI Core 1}");
		s_coreThread2NameBuffer->assign("{SYS ProcUI Core 2}");
		OSSetThreadName(&s_coreThreadArray[0], s_coreThread0NameBuffer->c_str());
		OSSetThreadName(&s_coreThreadArray[1], s_coreThread1NameBuffer->c_str());
		OSSetThreadName(&s_coreThreadArray[2], s_coreThread2NameBuffer->c_str());
		s_coreThreadsCreated = true;
	}

	void _SubmitCommandToCoreThreads(ProcUICoreThreadCommand cmd)
	{
		s_commandForCoreThread = cmd;
		OSMemoryBarrier();
		OSResetEvent(&s_eventCoreThreadsCommandDone);
		OSSignalEvent(&s_eventCoreThreadsNewCommandReady);
		OSWaitEvent(&s_eventCoreThreadsCommandDone);
	}

	void ProcUIInitInternal()
	{
		if( s_isInitialized.exchange(true) )
			return;
		if (!s_memoryPoolHeapPtr)
		{
			// user didn't specify a custom heap, use default heap instead
			s_memAllocPtr = gCoreinitData->MEMAllocFromDefaultHeap.GetMPTR();
			s_memFreePtr = gCoreinitData->MEMFreeToDefaultHeap.GetMPTR();
		}
		OSInitEvent(&s_eventStateMessageReceived, OSEvent::EVENT_STATE::STATE_NOT_SIGNALED, OSEvent::EVENT_MODE::MODE_MANUAL);
		OSInitEvent(&s_eventCoreThreadsNewCommandReady, OSEvent::EVENT_STATE::STATE_NOT_SIGNALED, OSEvent::EVENT_MODE::MODE_MANUAL);
		OSInitEvent(&s_eventWaitingBeforeReleaseForeground, OSEvent::EVENT_STATE::STATE_NOT_SIGNALED, OSEvent::EVENT_MODE::MODE_MANUAL);
		OSInitEvent(&s_eventCoreThreadsCommandDone, OSEvent::EVENT_STATE::STATE_NOT_SIGNALED, OSEvent::EVENT_MODE::MODE_MANUAL);
		OSInitRendezvous(&s_coreThreadRendezvousA);
		OSInitRendezvous(&s_coreThreadRendezvousB);
		OSInitRendezvous(&s_coreThreadRendezvousC);
		OSInitEvent(&s_eventBackgroundThreadGotMessage, OSEvent::EVENT_STATE::STATE_NOT_SIGNALED, OSEvent::EVENT_MODE::MODE_MANUAL);
		s_currentProcUIStatus = ProcUIStatus::Foreground;
		s_drawDoneReleaseCalled = false;
		s_isInForeground = true;
		s_coreThreadStackSize = 0x2000;
		s_systemMessageQueuePtr = coreinit::OSGetSystemMessageQueue();
		uint32 upid = coreinit::OSGetUPID();
		s_isForegroundProcess = upid == 2 || upid == 15; // either Wii U Menu or game title, both are RAMPID 7 (foreground process)
		RecreateProcUICoreThreads();
		ClearCallbacksWithoutMemFree();
	}

	void ProcUIInit(MEMPTR<void> callbackReadyToRelease)
	{
		s_saveCallback = callbackReadyToRelease;
		s_saveCallbackEx = nullptr;
		s_saveCallbackExUserParam = nullptr;
		ProcUIInitInternal();
	}

	void ProcUIInitEx(MEMPTR<void> callbackReadyToReleaseEx, MEMPTR<void> userParam)
	{
		s_saveCallback = nullptr;
		s_saveCallbackEx = callbackReadyToReleaseEx;
		s_saveCallbackExUserParam = userParam;
		ProcUIInitInternal();
	}

	void ProcUIShutdown()
	{
		if (!s_isInitialized.exchange(false))
			return;
		if ( !s_isInForeground )
			CancelBackgroundAlarms();
		for (sint32 i = 0; i < Espresso::CORE_COUNT; i++)
			OSSetThreadPriority(&s_coreThreadArray[i], 0);
		_SubmitCommandToCoreThreads(ProcUICoreThreadCommand::Exit);
		ProcUIClearCallbacks();
		ShutdownThreads();
	}

	bool ProcUIIsRunning()
	{
		return s_isInitialized;
	}

	bool ProcUIInForeground()
	{
		return s_isInForeground;
	}

	bool ProcUIInShutdown()
	{
		return s_isInShutdown;
	}

	void AddCallbackInternal(void* funcPtr, void* userParam, uint64 tickDelay, sint32 priority, ProcUICallbackList& callbackList)
	{
		if ( __OSGetProcessSDKVersion() < 21102 )
		{
			// in earlier COS versions it was possible/allowed to register a callback before initializing ProcUI
			s_memAllocPtr = gCoreinitData->MEMAllocFromDefaultHeap.GetMPTR();
			s_memFreePtr = gCoreinitData->MEMFreeToDefaultHeap.GetMPTR();
		}
		else if ( !s_isInitialized )
		{
			cemuLog_log(LogType::Force, "ProcUI: Trying to register callback before init");
			cemu_assert_suspicious();
			// this shouldn't happen but lets set the memory pointers anyway to prevent a crash in case the user has incorrect meta info
			s_memAllocPtr = gCoreinitData->MEMAllocFromDefaultHeap.GetMPTR();
			s_memFreePtr = gCoreinitData->MEMFreeToDefaultHeap.GetMPTR();
		}
		ProcUIInternalCallbackEntry* entry = (ProcUIInternalCallbackEntry*)_AllocMem(sizeof(ProcUIInternalCallbackEntry));
		entry->funcPtr = funcPtr;
		entry->userParam = userParam;
		entry->tickDelay = tickDelay;
		entry->priority = priority;
		ProcUIInternalCallbackEntry* cur = callbackList.first;
		cur = callbackList.first;
		if (!cur || cur->priority > priority)
		{
			// insert as the first element
			entry->next = cur;
			callbackList.first = entry;
		}
		else
		{
			// find the correct position to insert
			while (cur->next && cur->next->priority < priority)
				cur = cur->next;
			entry->next = cur->next;
			cur->next = entry;
		}
	}

	void ProcUIRegisterCallbackCore(ProcUICallbackId callbackType, void* funcPtr, void* userParam, sint32 priority, uint32 coreIndex)
	{
		if(callbackType >= ProcUICallbackId::COUNT)
		{
			cemuLog_log(LogType::Force, "ProcUIRegisterCallback: Invalid callback type {}", stdx::to_underlying(callbackType));
			return;
		}
		if(callbackType != ProcUICallbackId::AcquireForeground)
			priority = -priority;
		AddCallbackInternal(funcPtr, userParam, 0, priority, s_CallbackTables[stdx::to_underlying(callbackType)][coreIndex]);
	}

	void ProcUIRegisterCallback(ProcUICallbackId callbackType, void* funcPtr, void* userParam, sint32 priority)
	{
		ProcUIRegisterCallbackCore(callbackType, funcPtr, userParam, priority, OSGetCoreId());
	}

	void ProcUIRegisterBackgroundCallback(void* funcPtr, void* userParam, uint64 tickDelay)
	{
		AddCallbackInternal(funcPtr, userParam, tickDelay, 0, s_backgroundCallbackList);
	}

	void FreeCallbackChain(ProcUICallbackList& callbackList)
	{
		ProcUIInternalCallbackEntry* entry = callbackList.first;
		while (entry)
		{
			ProcUIInternalCallbackEntry* next = entry->next;
			_FreeMem(entry);
			entry = next;
		}
		callbackList.first = nullptr;
	}

	void ProcUIClearCallbacks()
	{
		for (sint32 coreIndex = 0; coreIndex < Espresso::CORE_COUNT; coreIndex++)
		{
			for (sint32 i = 0; i < stdx::to_underlying(ProcUICallbackId::COUNT); i++)
			{
				FreeCallbackChain(s_CallbackTables[i][coreIndex]);
			}
		}
		if (!s_isInForeground)
			CancelBackgroundAlarms();
		FreeCallbackChain(s_backgroundCallbackList);
	}

	void ProcUISetSaveCallback(void* funcPtr, void* userParam)
	{
		s_saveCallback = nullptr;
		s_saveCallbackEx = funcPtr;
		s_saveCallbackExUserParam = userParam;
	}

	void ProcUISetCallbackStackSize(uint32 newStackSize)
	{
		s_coreThreadStackSize = newStackSize;
		if( s_isInitialized )
			RecreateProcUICoreThreads();
	}

	uint32 ProcUICalcMemorySize(uint32 numCallbacks)
	{
		// each callback entry is 0x70 bytes with 0x14 bytes of allocator overhead (for ExpHeap). But for some reason proc_ui on 5.5.5 seems to reserve 0x8C0 bytes per callback?
		uint32 stackReserveSize = (Espresso::CORE_COUNT + 1) * s_coreThreadStackSize; // 3 core threads + 1 message receive thread
		uint32 callbackReserveSize = 0x8C0 * numCallbacks;
		return stackReserveSize + callbackReserveSize + 100;
	}

	void _MemAllocFromMemoryPool(PPCInterpreter_t* hCPU)
	{
		uint32 size = hCPU->gpr[3];
		MEMPTR<void> r = MEMAllocFromExpHeapEx((MEMHeapHandle)s_memoryPoolHeapPtr.GetPtr(), size, 4);
		osLib_returnFromFunction(hCPU, r.GetMPTR());
	}

	void _FreeToMemoryPoolExpHeap(PPCInterpreter_t* hCPU)
	{
		MEMPTR<void> mem{hCPU->gpr[3]};
		MEMFreeToExpHeap((MEMHeapHandle)s_memoryPoolHeapPtr.GetPtr(), mem.GetPtr());
		osLib_returnFromFunction(hCPU, 0);
	}

	sint32 ProcUISetMemoryPool(void* memBase, uint32 size)
	{
		s_memAllocPtr = RPLLoader_MakePPCCallable(_MemAllocFromMemoryPool);
		s_memFreePtr = RPLLoader_MakePPCCallable(_FreeToMemoryPoolExpHeap);
		s_memoryPoolHeapPtr = MEMCreateExpHeapEx(memBase, size, MEM_HEAP_OPTION_THREADSAFE);
		return s_memoryPoolHeapPtr ? 0 : -1;
	}

	void ProcUISetBucketStorage(void* memBase, uint32 size)
	{
		MEMPTR<void> fgBase;
		uint32be fgFreeSize;
		OSGetForegroundBucketFreeArea(&fgBase, &fgFreeSize);
		if(fgFreeSize < size)
			cemuLog_log(LogType::Force, "ProcUISetBucketStorage: Buffer size too small");
		s_bucketStorageBasePtr = memBase;
	}

	void ProcUISetMEM1Storage(void* memBase, uint32 size)
	{
		MEMPTR<void> memBound;
		uint32be memBoundSize;
		OSGetMemBound(1, &memBound, &memBoundSize);
		if(memBoundSize < size)
			cemuLog_log(LogType::Force, "ProcUISetMEM1Storage: Buffer size too small");
		s_mem1StorageBasePtr = memBase;
	}

	void ProcUIDrawDoneRelease()
	{
		s_drawDoneReleaseCalled = true;
	}

	OSMessage g_lastMsg;

	void ProcUI_BackgroundThread_ReceiveSingleMessage(PPCInterpreter_t* hCPU)
	{
		// the background thread receives messages in a loop until the title is either exited or foreground is acquired
		while ( true )
		{
			OSReceiveMessage(s_systemMessageQueuePtr, &g_lastMsg, OS_MESSAGE_BLOCK); // blocking receive
			SysMessageId lastMsgId = static_cast<SysMessageId>((uint32)g_lastMsg.data0);
			if(lastMsgId == SysMessageId::MsgExit || lastMsgId == SysMessageId::MsgAcquireForeground)
				break;
			else if (lastMsgId == SysMessageId::HomeButtonDenied)
			{
				cemu_assert_suspicious(); // Home button denied should not be sent to background app
			}
			else if ( lastMsgId == SysMessageId::NetIoStartOrStop )
			{
				if (g_lastMsg.data1 )
				{
					// NetIo start message
					for (sint32 i = 0; i < Espresso::CORE_COUNT; i++)
						DoCallbackChain(s_callbacksType3_NetIoStart[i].first);
				}
				else
				{
					// NetIo stop message
					for (sint32 i = 0; i < Espresso::CORE_COUNT; i++)
						DoCallbackChain(s_callbacksType4_NetIoStop[i].first);
				}
			}
			else
			{
				cemuLog_log(LogType::Force, "ProcUI: BackgroundThread received invalid message 0x{:08x}", lastMsgId);
			}
		}
		OSSignalEvent(&s_eventBackgroundThreadGotMessage);
		osLib_returnFromFunction(hCPU, 0);
	}

	// handle received message
	// if the message is Exit this function returns false, in all other cases it returns true
	bool ProcessSysMessage(OSMessage* msg)
	{
		SysMessageId lastMsgId = static_cast<SysMessageId>((uint32)msg->data0);
		if ( lastMsgId == SysMessageId::MsgAcquireForeground )
		{
			cemuLog_logDebug(LogType::Force, "ProcUI: Received Acquire Foreground message");
			s_isInShutdown = false;
			_SubmitCommandToCoreThreads(ProcUICoreThreadCommand::AcquireForeground);
			s_currentProcUIStatus = ProcUIStatus::Foreground;
			s_isInForeground = true;
			OSMemoryBarrier();
			OSSignalEvent(&s_eventStateMessageReceived);
			return true;
		}
		else if (lastMsgId == SysMessageId::MsgExit)
		{
			cemuLog_logDebug(LogType::Force, "ProcUI: Received Exit message");
			s_isInShutdown = true;
			_SubmitCommandToCoreThreads(ProcUICoreThreadCommand::Exit);
			for (sint32 i = 0; i < Espresso::CORE_COUNT; i++)
				FreeCallbackChain(s_callbacksType2_Exit[i]);
			s_currentProcUIStatus = ProcUIStatus::Exit;
			OSMemoryBarrier();
			OSSignalEvent(&s_eventStateMessageReceived);
			return 0;
		}
		if (lastMsgId == SysMessageId::MsgReleaseForeground)
		{
			if (msg->data1 != 0)
			{
				cemuLog_logDebug(LogType::Force, "ProcUI: Received Release Foreground message as part of shutdown initiation");
				s_isInShutdown = true;
			}
			else
			{
				cemuLog_logDebug(LogType::Force, "ProcUI: Received Release Foreground message");
			}
			s_currentProcUIStatus = ProcUIStatus::Releasing;
			OSResetEvent(&s_eventStateMessageReceived);
			// dont submit a command for the core threads yet, we need to wait for ProcUIDrawDoneRelease()
		}
		else if (lastMsgId == SysMessageId::HomeButtonDenied)
		{
			cemuLog_logDebug(LogType::Force, "ProcUI: Received Home Button Denied message");
			_SubmitCommandToCoreThreads(ProcUICoreThreadCommand::HomeButtonDenied);
		}
		else if ( lastMsgId == SysMessageId::NetIoStartOrStop )
		{
			if (msg->data1 != 0)
			{
				cemuLog_logDebug(LogType::Force, "ProcUI: Received Net IO Start message");
				_SubmitCommandToCoreThreads(ProcUICoreThreadCommand::NetIoStart);
			}
			else
			{
				cemuLog_logDebug(LogType::Force, "ProcUI: Received Net IO Stop message");
				_SubmitCommandToCoreThreads(ProcUICoreThreadCommand::NetIoStop);
			}
		}
		else
		{
			cemuLog_log(LogType::Force, "ProcUI: Received unknown message 0x{:08x}", (uint32)lastMsgId);
		}
		return true;
	}

	ProcUIStatus ProcUIProcessMessages(bool isBlockingInBackground)
	{
		OSMessage msg;
		if (!s_isInitialized)
		{
			cemuLog_logOnce(LogType::Force, "ProcUIProcessMessages: ProcUI not initialized");
			cemu_assert_suspicious();
			return ProcUIStatus::Foreground;
		}
		if ( !isBlockingInBackground && OSGetCoreId() != 2 )
		{
			cemuLog_logOnce(LogType::Force, "ProcUIProcessMessages: Non-blocking call must run on core 2");
		}
		if (s_previouslyWasBlocking && isBlockingInBackground )
		{
			cemuLog_logOnce(LogType::Force, "ProcUIProcessMessages: Cannot switch to blocking mode when in background");
		}
		s_currentProcUIStatus = s_isInForeground ? ProcUIStatus::Foreground : ProcUIStatus::Background;
		if (s_drawDoneReleaseCalled)
		{
			s_isInForeground = false;
			s_currentProcUIStatus = ProcUIStatus::Background;
			_SubmitCommandToCoreThreads(ProcUICoreThreadCommand::ReleaseForeground);
			OSResetEvent(&s_eventWaitingBeforeReleaseForeground);
			if(s_saveCallback)
				PPCCoreCallback(s_saveCallback);
			if(s_saveCallbackEx)
				PPCCoreCallback(s_saveCallbackEx, s_saveCallbackExUserParam);
			if (s_isForegroundProcess && isBlockingInBackground)
			{
				// start background thread
				__OSCreateThreadType(&s_backgroundThread, RPLLoader_MakePPCCallable(ProcUI_BackgroundThread_ReceiveSingleMessage),
					0, nullptr, (uint8*)s_backgroundThreadStack.GetPtr() + s_coreThreadStackSize, s_coreThreadStackSize,
					16, (1<<2), OSThread_t::THREAD_TYPE::TYPE_DRIVER);
				OSResumeThread(&s_backgroundThread);
				s_previouslyWasBlocking = true;
			}
			cemuLog_logDebug(LogType::Force, "ProcUI: Releasing foreground");
			OSSignalEvent(&s_eventWaitingBeforeReleaseForeground);
			s_drawDoneReleaseCalled = false;
		}
		if (s_isInForeground || !isBlockingInBackground)
		{
			// non-blocking mode
			if ( OSReceiveMessage(s_systemMessageQueuePtr, &msg, 0) )
			{
				s_previouslyWasBlocking = false;
				if ( !ProcessSysMessage(&msg) )
					return s_currentProcUIStatus;
				// continue below, if we are now in background then ProcUIProcessMessages enters blocking mode
			}
		}
		// blocking mode (if in background and param is true)
		while (!s_isInForeground && isBlockingInBackground)
		{
			if ( !s_isForegroundProcess)
			{
				OSReceiveMessage(s_systemMessageQueuePtr, &msg, OS_MESSAGE_BLOCK);
				s_previouslyWasBlocking = false;
				if ( !ProcessSysMessage(&msg) )
					return s_currentProcUIStatus;
			}
			// this code should only run if the background thread was started? Maybe rearrange the code to make this more clear
			OSWaitEvent(&s_eventBackgroundThreadGotMessage);
			OSResetEvent(&s_eventBackgroundThreadGotMessage);
			OSJoinThread(&s_backgroundThread, nullptr);
			msg = g_lastMsg; // g_lastMsg is set by the background thread
			s_previouslyWasBlocking = false;
			if ( !ProcessSysMessage(&msg) )
				return s_currentProcUIStatus;
		}
		return s_currentProcUIStatus;
	}

	ProcUIStatus ProcUISubProcessMessages(bool isBlockingInBackground)
	{
		if (isBlockingInBackground)
		{
			while (s_currentProcUIStatus == ProcUIStatus::Background)
				OSWaitEvent(&s_eventStateMessageReceived);
		}
		return s_currentProcUIStatus;
	}

	const char* ProcUIDriver_GetName()
	{
		s_ProcUIDriverName->assign("ProcUI");
		return s_ProcUIDriverName->c_str();
	}

	void ProcUIDriver_Init(/* parameters unknown */)
	{
		s_driverIsActive = true;
		OSMemoryBarrier();
	}

	void ProcUIDriver_OnDone(/* parameters unknown */)
	{
		if (s_driverIsActive)
		{
			ProcUIShutdown();
			s_driverIsActive = false;
			OSMemoryBarrier();
		}
	}

	void StoreMEM1AndFGBucket()
	{
		if (s_mem1StorageBasePtr)
		{
			MEMPTR<void> memBound;
			uint32be memBoundSize;
			OSGetMemBound(1, &memBound, &memBoundSize);
			OSBlockMove(s_mem1StorageBasePtr.GetPtr(), memBound.GetPtr(), memBoundSize, true);
		}
		if (s_bucketStorageBasePtr)
		{
			MEMPTR<void> memBound;
			uint32be memBoundSize;
			OSGetForegroundBucketFreeArea(&memBound, &memBoundSize);
			OSBlockMove(s_bucketStorageBasePtr.GetPtr(), memBound.GetPtr(), memBoundSize, true);
		}
	}

	void RestoreMEM1AndFGBucket()
	{
		if (s_mem1StorageBasePtr)
		{
			MEMPTR<void> memBound;
			uint32be memBoundSize;
			OSGetMemBound(1, &memBound, &memBoundSize);
			OSBlockMove(memBound.GetPtr(), s_mem1StorageBasePtr, memBoundSize, true);
			GX2::GX2Invalidate(0x40, s_mem1StorageBasePtr.GetMPTR(), memBoundSize);
		}
		if (s_bucketStorageBasePtr)
		{
			MEMPTR<void> memBound;
			uint32be memBoundSize;
			OSGetForegroundBucketFreeArea(&memBound, &memBoundSize);
			OSBlockMove(memBound.GetPtr(), s_bucketStorageBasePtr, memBoundSize, true);
			GX2::GX2Invalidate(0x40, memBound.GetMPTR(), memBoundSize);
		}
	}

	void ProcUIDriver_OnAcquiredForeground(/* parameters unknown */)
	{
		if (s_driverInBackground)
		{
			ProcUIDriver_Init();
			s_driverInBackground = false;
		}
		else
		{
			RestoreMEM1AndFGBucket();
			s_driverIsActive = true;
			OSMemoryBarrier();
		}
	}

	void ProcUIDriver_OnReleaseForeground(/* parameters unknown */)
	{
		StoreMEM1AndFGBucket();
		s_driverIsActive = false;
		OSMemoryBarrier();
	}

	sint32 rpl_entry(uint32 moduleHandle, RplEntryReason reason)
	{
		if ( reason == RplEntryReason::Loaded )
		{
			s_ProcUIDriver->getDriverName = RPLLoader_MakePPCCallable([](PPCInterpreter_t* hCPU) {MEMPTR<const char> namePtr(ProcUIDriver_GetName()); osLib_returnFromFunction(hCPU, namePtr.GetMPTR()); });
			s_ProcUIDriver->init = RPLLoader_MakePPCCallable([](PPCInterpreter_t* hCPU) {ProcUIDriver_Init(); osLib_returnFromFunction(hCPU, 0); });
			s_ProcUIDriver->onAcquireForeground = RPLLoader_MakePPCCallable([](PPCInterpreter_t* hCPU) {ProcUIDriver_OnAcquiredForeground(); osLib_returnFromFunction(hCPU, 0); });
			s_ProcUIDriver->onReleaseForeground = RPLLoader_MakePPCCallable([](PPCInterpreter_t* hCPU) {ProcUIDriver_OnReleaseForeground(); osLib_returnFromFunction(hCPU, 0); });
			s_ProcUIDriver->done = RPLLoader_MakePPCCallable([](PPCInterpreter_t* hCPU) {ProcUIDriver_OnDone(); osLib_returnFromFunction(hCPU, 0); });

			s_driverIsActive = false;
			s_driverArgUkn1 = 0;
			s_driverArgUkn2 = 0;
			s_driverInBackground = false;
			uint32be ukn3;
			OSDriver_Register(moduleHandle, 200, &s_ProcUIDriver, 0, &s_driverArgUkn1, &s_driverArgUkn2, &ukn3);
			if ( ukn3 )
			{
				if ( OSGetForegroundBucket(nullptr, nullptr) )
				{
					ProcUIDriver_Init();
					OSMemoryBarrier();
					return 0;
				}
				s_driverInBackground = true;
			}
			OSMemoryBarrier();
		}
		else if ( reason == RplEntryReason::Unloaded )
		{
			ProcUIDriver_OnDone();
			OSDriver_Deregister(moduleHandle, 0);
		}
		return 0;
	}

	void reset()
	{
		// set variables to their initial state as if the RPL was just loaded
		s_isInitialized = false;
		s_isInShutdown = false;
		s_isInForeground = false; // ProcUIInForeground returns false until ProcUIInit(Ex) is called
		s_isForegroundProcess = true;
		s_saveCallback = nullptr;
		s_saveCallbackEx = nullptr;
		s_systemMessageQueuePtr = nullptr;
		ClearCallbacksWithoutMemFree();
		s_currentProcUIStatus = ProcUIStatus::Foreground;
		s_bucketStorageBasePtr = nullptr;
		s_mem1StorageBasePtr = nullptr;
		s_drawDoneReleaseCalled = false;
		s_previouslyWasBlocking = false;
		// core threads
		s_coreThreadStackSize = 0;
		s_coreThreadsCreated = false;
		s_commandForCoreThread = ProcUICoreThreadCommand::Initial;
		// background thread
		s_backgroundThreadStack = nullptr;
		// user defined heap
		s_memoryPoolHeapPtr = nullptr;
		s_memAllocPtr = nullptr;
		s_memFreePtr = nullptr;
		// driver
		s_driverIsActive = false;
		s_driverInBackground = false;
	}

	void load()
	{
		reset();

		cafeExportRegister("proc_ui", ProcUIInit, LogType::ProcUi);
		cafeExportRegister("proc_ui", ProcUIInitEx, LogType::ProcUi);
		cafeExportRegister("proc_ui", ProcUIShutdown, LogType::ProcUi);
		cafeExportRegister("proc_ui", ProcUIIsRunning, LogType::ProcUi);
		cafeExportRegister("proc_ui", ProcUIInForeground, LogType::ProcUi);
		cafeExportRegister("proc_ui", ProcUIInShutdown, LogType::ProcUi);

		cafeExportRegister("proc_ui", ProcUIRegisterCallbackCore, LogType::ProcUi);
		cafeExportRegister("proc_ui", ProcUIRegisterCallback, LogType::ProcUi);
		cafeExportRegister("proc_ui", ProcUIRegisterBackgroundCallback, LogType::ProcUi);
		cafeExportRegister("proc_ui", ProcUIClearCallbacks, LogType::ProcUi);
		cafeExportRegister("proc_ui", ProcUISetSaveCallback, LogType::ProcUi);

		cafeExportRegister("proc_ui", ProcUISetCallbackStackSize, LogType::ProcUi);
		cafeExportRegister("proc_ui", ProcUICalcMemorySize, LogType::ProcUi);
		cafeExportRegister("proc_ui", ProcUISetMemoryPool, LogType::ProcUi);
		cafeExportRegister("proc_ui", ProcUISetBucketStorage, LogType::ProcUi);
		cafeExportRegister("proc_ui", ProcUISetMEM1Storage, LogType::ProcUi);

		cafeExportRegister("proc_ui", ProcUIDrawDoneRelease, LogType::ProcUi);
		cafeExportRegister("proc_ui", ProcUIProcessMessages, LogType::ProcUi);
		cafeExportRegister("proc_ui", ProcUISubProcessMessages, LogType::ProcUi);

		// manually call rpl_entry for now
		rpl_entry(-1, RplEntryReason::Loaded);
	}
};
