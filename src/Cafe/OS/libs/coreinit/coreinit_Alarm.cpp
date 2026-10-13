#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/HW/Espresso/PPCCallback.h"
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"
#include "Cafe/OS/libs/coreinit/coreinit_Time.h"
#include "Cafe/OS/libs/coreinit/coreinit_Alarm.h"
#include "Cafe/HW/Espresso/Recompiler/PPCRecompiler.h"
#include "Cafe/OS/RPL/rpl.h"

// #define ALARM_LOGGING

namespace coreinit
{
	SysAllocator<OSEvent> g_alarmEvent;

	SysAllocator<OSThread_t> g_alarmThread;
	SysAllocator<uint8, 1024 * 128> _g_alarmThreadStack;
	SysAllocator<char, 32> _g_alarmThreadName;


	class OSHostAlarm 
	{
	public:
		OSHostAlarm(uint64 nextFire, uint64 period, void(*callbackFunc)(uint64 currentTick, void* context), void* context) : m_nextFire(nextFire), m_period(period), m_callbackFunc(callbackFunc), m_context(context)
		{
			cemu_assert_debug(__OSHasSchedulerLock()); // must hold lock
			auto r = g_activeAlarmList.emplace(this);
			cemu_assert_debug(r.second); // check if insertion was successful
			m_isActive = true;
			updateEarliestAlarmAtomic();
		}

		~OSHostAlarm()
		{
			cemu_assert_debug(__OSHasSchedulerLock()); // must hold lock
			if (m_isActive)
			{
				g_activeAlarmList.erase(g_activeAlarmList.find(this));
				updateEarliestAlarmAtomic();
			}
		}

		uint64 getFireTick() const
		{
			return m_nextFire;
		}

		void triggerAlarm(uint64 currentTick)
		{
			m_callbackFunc(currentTick, m_context);
		}

		static void updateEarliestAlarmAtomic()
		{
			cemu_assert_debug(__OSHasSchedulerLock());
			if (!g_activeAlarmList.empty())
			{
				auto firstAlarm = g_activeAlarmList.begin();
				g_soonestAlarm = (*firstAlarm)->m_nextFire;
			}
			else
			{
				g_soonestAlarm = std::numeric_limits<uint64>::max();
			}
		}

		static void updateAlarms(uint64 currentTick)
		{
			cemu_assert_debug(__OSHasSchedulerLock());
			if (g_activeAlarmList.empty())
				return;

			// debug begin
#ifdef CEMU_DEBUG_ASSERT
			uint64 prevTick = 0;
			auto itr = g_activeAlarmList.begin();
			while (itr != g_activeAlarmList.end())
			{
				uint64 t = (*itr)->m_nextFire;
				if (t < prevTick)
					cemu_assert_suspicious();
				prevTick = t;
				++itr;
			}
#endif
			// debug end
			while (!g_activeAlarmList.empty())
			{
				auto firstAlarm = g_activeAlarmList.begin();
				if (currentTick >= (*firstAlarm)->m_nextFire)
				{
					OSHostAlarm* alarm = *firstAlarm;
					g_activeAlarmList.erase(firstAlarm);
					alarm->triggerAlarm(currentTick);
					// if periodic alarm then requeue
					if (alarm->m_period > 0)
					{
						alarm->m_nextFire += alarm->m_period;
						g_activeAlarmList.emplace(alarm);
					}
					else
						alarm->m_isActive = false;
					updateEarliestAlarmAtomic();
				}
				else
					break;
			}	
		}

		uint64 getNextFire() const 
		{
			return m_nextFire;
		}

		static bool quickCheckForAlarm(uint64 currentTick)
		{
			// fast way to check if any alarm was triggered without requiring scheduler lock
			return currentTick >= g_soonestAlarm;
		}

        static void Reset()
        {
            g_activeAlarmList.clear();
            g_soonestAlarm = 0;
        }

	public:
		struct ComparatorFireTime
		{
			bool operator ()(OSHostAlarm* const & p1, OSHostAlarm* const & p2) const
			{
				auto p1Fire = p1->getNextFire();
				auto p2Fire = p2->getNextFire();
				if (p1Fire == p2Fire)
					return (uintptr_t)p1 < (uintptr_t)p2; // if time is equal, differ by pointer (to allow storing multiple alarms with same firing time)
				return p1Fire < p2Fire;
			}
		};

	private:	
		uint64 m_nextFire;
		uint64 m_period; // if zero then repeat is disabled 
		bool m_isActive{ false };

		void (*m_callbackFunc)(uint64 currentTick, void* context);
		void* m_context;

		static std::set<class OSHostAlarm*, ComparatorFireTime> g_activeAlarmList;
		static std::atomic_uint64_t g_soonestAlarm;
	};


	std::set<class OSHostAlarm*, OSHostAlarm::ComparatorFireTime> OSHostAlarm::g_activeAlarmList;
	std::atomic_uint64_t OSHostAlarm::g_soonestAlarm{};

	OSHostAlarm* OSHostAlarmCreate(uint64 nextFire, uint64 period, void(*callbackFunc)(uint64 currentTick, void* context), void* context)
	{
		OSHostAlarm* hostAlarm = new OSHostAlarm(nextFire, period, callbackFunc, context);
		return hostAlarm;
	}

	void OSHostAlarmDestroy(OSHostAlarm* hostAlarm)
	{
		delete hostAlarm;
	}

	void alarm_update()
	{	
		cemu_assert_debug(!__OSHasSchedulerLock());
		uint64 currentTick = coreinit::OSGetTime();
		if (!OSHostAlarm::quickCheckForAlarm(currentTick))
			return;
		__OSLockScheduler();
		OSHostAlarm::updateAlarms(currentTick);
		__OSUnlockScheduler();
	}

	/* alarm API */

	void OSInitAlarmQueue(OSAlarmQueue* queue)
	{
		queue->magic = (uint32)'aLmQ';
		queue->name = nullptr;
		queue->ukn08 = 0;
		OSInitThreadQueueEx(&queue->threadQueue, queue);
		queue->head = nullptr;
		queue->tail = nullptr;
	}

	void OSInitAlarmQueueEx(OSAlarmQueue* queue, const char* name)
	{
		OSInitAlarmQueue(queue);
		queue->name = name;
	}

	void OSCreateAlarm(OSAlarm_t* alarm)
	{
		memset(alarm, 0, sizeof(OSAlarm_t));
		alarm->setMagic();
	}

	void OSCreateAlarmEx(OSAlarm_t* alarm, const char* alarmName)
	{
		memset(alarm, 0, sizeof(OSAlarm_t));
		alarm->setMagic();
		alarm->name = alarmName;
	}

	std::unordered_map<OSAlarm_t*, OSHostAlarm*> g_activeAlarms;

	// fired alarms wait in their alarm queue until they are taken out of it
	static bool __OSIsAlarmInQueue(OSAlarm_t* alarm)
	{
		OSAlarmQueue* queue = alarm->alarmQueue.GetPtr();
		return queue && (queue->head.GetPtr() == alarm || alarm->prev);
	}

	static void __OSAppendAlarmToQueue(OSAlarm_t* alarm)
	{
		OSAlarmQueue* queue = alarm->alarmQueue.GetPtr();
		alarm->prev = queue->tail;
		alarm->next = nullptr;
		if (queue->tail)
			queue->tail->next = alarm;
		else
			queue->head = alarm;
		queue->tail = alarm;
		queue->threadQueue.wakeupEntireWaitQueue(false);
	}

	static void __OSRemoveAlarmFromQueue(OSAlarm_t* alarm)
	{
		OSAlarmQueue* queue = alarm->alarmQueue.GetPtr();
		if (alarm->next)
			alarm->next->prev = alarm->prev;
		else
			queue->tail = alarm->prev;
		if (alarm->prev)
			alarm->prev->next = alarm->next;
		else
			queue->head = alarm->next;
		alarm->prev = nullptr;
		alarm->next = nullptr;
	}

	bool OSCancelAlarm(OSAlarm_t* alarm)
	{
		__OSLockScheduler();
		bool alarmWasActive = false;
		auto itr = g_activeAlarms.find(alarm);
		if (itr != g_activeAlarms.end())
		{
			OSHostAlarmDestroy(itr->second);
			g_activeAlarms.erase(itr);
			alarmWasActive = true;
		}
		else if (__OSIsAlarmInQueue(alarm))
		{
			__OSRemoveAlarmFromQueue(alarm);
			alarmWasActive = true;
		}

		__OSUnlockScheduler();
		return alarmWasActive;
	}

	void OSCancelAlarms(uint32 group)
	{
		if (group == 0 || group == 0xFFFFFFFF)
			return;
		__OSLockScheduler();
		for (auto itr = g_activeAlarms.begin(); itr != g_activeAlarms.end();)
		{
			if (itr->first->group == group)
			{
				OSHostAlarmDestroy(itr->second);
				itr = g_activeAlarms.erase(itr);
			}
			else
				++itr;
		}
		__OSUnlockScheduler();
	}

	void OSSetAlarmTag(OSAlarm_t* alarm, uint32 group)
	{
		if (group == 0xFFFFFFFF)
			return;
		__OSLockScheduler();
		alarm->group = group;
		__OSUnlockScheduler();
	}

	void __OSHostAlarmTriggered(uint64 currentTick, void* context)
	{
#ifdef ALARM_LOGGING
		cemuLog_log(LogType::Force, "[Alarm] Alarm ready and alarm thread signalled. Current tick: {}", currentTick);
#endif
		OSSignalEventInternal(g_alarmEvent.GetPtr());
	}

	void __OSInitiateAlarm(OSAlarm_t* alarm, uint64 startTime, uint64 period, MPTR handlerFunc, bool isPeriodic, OSAlarmQueue* alarmQueue)
	{
        cemu_assert_debug(MMU_IsInPPCMemorySpace(alarm));
		cemu_assert_debug(__OSHasSchedulerLock());

		// alarms which fired and still wait in their alarm queue cannot be set
		if (__OSIsAlarmInQueue(alarm))
			return;

		uint64 nextTime = startTime;

#ifdef ALARM_LOGGING
		double periodInMS = (double)period * 1000.0 / (double)EspressoTime::GetTimerClock();
		cemuLog_log(LogType::Force, "[Alarm] Start alarm 0x{:08x}. Func 0x{:08x}. Period: {}ms", MEMPTR(alarm).GetMPTR(), handlerFunc, periodInMS);
#endif

		if (isPeriodic)
		{
			if ((sint64)period <= 0)
			{
				cemuLog_log(LogType::APIErrors, "OSSetPeriodicAlarm(): Period must be positive, alarm is not set");
				return;
			}

			uint64 currentTime = OSGetTime();
			if (startTime < currentTime)
			{
				uint64 ticksSinceStart = currentTime - startTime;
				uint64 numPeriods = ticksSinceStart / period;
				nextTime = startTime + (numPeriods + 1ull) * period;
			}

			alarm->startTime = _swapEndianU64(startTime);
			alarm->nextTime = _swapEndianU64(nextTime);
			alarm->period = _swapEndianU64(period);
			alarm->handler = _swapEndianU32(handlerFunc);
		}
		else
		{
			alarm->nextTime = _swapEndianU64(startTime);
			alarm->period = 0;
			alarm->handler = _swapEndianU32(handlerFunc);
		}
		alarm->alarmQueue = alarmQueue;

		auto existingAlarmItr = g_activeAlarms.find(alarm);
		if (existingAlarmItr != g_activeAlarms.end())
		{
			// delete existing alarm
			cemuLog_logDebug(LogType::Force, "__OSInitiateAlarm() called on alarm which was already active");
			OSHostAlarmDestroy(existingAlarmItr->second);
			g_activeAlarms.erase(existingAlarmItr);
		}

		g_activeAlarms[alarm] = OSHostAlarmCreate(nextTime, period, __OSHostAlarmTriggered, nullptr);
	}

	bool OSSetAlarm(OSAlarm_t* alarm, uint64 delayInTicks, MPTR handlerFunc)
	{
		if ((sint64)delayInTicks <= 0)
		{
			cemuLog_log(LogType::APIErrors, "OSSetAlarm(): Delay must be positive, alarm is not set");
			return true;
		}
		__OSLockScheduler();
		__OSInitiateAlarm(alarm, OSGetTime() + delayInTicks, 0, handlerFunc, false, nullptr);
		__OSUnlockScheduler();
		return true;
	}

	bool OSSetPeriodicAlarm(OSAlarm_t* alarm, uint64 nextFire, uint64 period, MPTR handlerFunc)
	{
		__OSLockScheduler();
		__OSInitiateAlarm(alarm, nextFire, period, handlerFunc, true, nullptr);
		__OSUnlockScheduler();
		return true;
	}

	bool OSSetAlarmEx(OSAlarm_t* alarm, uint64 delayInTicks, MPTR handlerFunc, OSAlarmQueue* alarmQueue)
	{
		if ((sint64)delayInTicks <= 0)
		{
			cemuLog_log(LogType::APIErrors, "OSSetAlarmEx(): Delay must be positive, alarm is not set");
			return true;
		}
		__OSLockScheduler();
		__OSInitiateAlarm(alarm, OSGetTime() + delayInTicks, 0, handlerFunc, false, alarmQueue);
		__OSUnlockScheduler();
		return true;
	}

	bool OSSetPeriodicAlarmEx(OSAlarm_t* alarm, uint64 nextFire, uint64 period, MPTR handlerFunc, OSAlarmQueue* alarmQueue)
	{
		__OSLockScheduler();
		__OSInitiateAlarm(alarm, nextFire, period, handlerFunc, true, alarmQueue);
		__OSUnlockScheduler();
		return true;
	}

	bool OSGetAlarmFromQueue(OSAlarmQueue* queue, MEMPTR<OSAlarm_t>* alarmOut, MEMPTR<void>* contextOut, bool noWait)
	{
		__OSLockScheduler();
		while (!queue->head)
		{
			if (noWait)
			{
				*alarmOut = nullptr;
				*contextOut = nullptr;
				__OSUnlockScheduler();
				return false;
			}
			queue->threadQueue.queueAndWait(OSGetCurrentThread());
		}
		OSAlarm_t* alarm = queue->head.GetPtr();
		__OSRemoveAlarmFromQueue(alarm);
		// periodic alarms are only set again once they are taken out of their alarm queue
		if (alarm->period != 0)
			__OSInitiateAlarm(alarm, _swapEndianU64(alarm->startTime), _swapEndianU64(alarm->period), _swapEndianU32(alarm->handler), true, queue);
		*alarmOut = alarm;
		*contextOut = alarm->context;
		__OSUnlockScheduler();
		return true;
	}

	void OSSetAlarmUserData(OSAlarm_t* alarm, uint32 userData)
	{
		alarm->userData = userData;
	}

	uint32 OSGetAlarmUserData(OSAlarm_t* alarm)
	{
		return alarm->userData;
	}

	void OSAlarm_Shutdown()
	{
        __OSLockScheduler();
        if(g_activeAlarms.empty())
        {
            __OSUnlockScheduler();
            return;
        }
        for(auto& itr : g_activeAlarms)
        {
            OSHostAlarmDestroy(itr.second);
        }
        g_activeAlarms.clear();
        OSHostAlarm::Reset();
        __OSUnlockScheduler();
	}

	void _OSAlarmThread(PPCInterpreter_t* hCPU)
	{
		while( true )
		{
			OSWaitEvent(g_alarmEvent.GetPtr());
			uint64 currentTick = OSGetTime();
			while (true)
			{
				// get alarm to fire
				OSAlarm_t* alarm = nullptr;
				__OSLockScheduler();
				auto itr = g_activeAlarms.begin();
				while(itr != g_activeAlarms.end())
				{
					if (currentTick >= _swapEndianU64(itr->first->nextTime))
					{
						alarm = itr->first;
						if (alarm->alarmQueue)
						{
							OSHostAlarmDestroy(itr->second);
							g_activeAlarms.erase(itr);
						}
						else if (alarm->period == 0)
						{
							// end alarm
							g_activeAlarms.erase(itr);
							break;
						}
						else
						{
							alarm->nextTime = _swapEndianU64(_swapEndianU64(alarm->nextTime) + _swapEndianU64(alarm->period));
						}
						break;
					}
					++itr;
				}
				if (alarm && alarm->alarmQueue)
				{
					alarm->context = &g_alarmThread.GetPtr()->context;
					__OSAppendAlarmToQueue(alarm);
					__OSUnlockScheduler();
					continue;
				}
				__OSUnlockScheduler();
				if (!alarm)
					break;
				// do callback for alarm
#ifdef ALARM_LOGGING
				double periodInMS = (double)_swapEndianU64(alarm->period) * 1000.0 / (double)EspressoTime::GetTimerClock();
				cemuLog_log(LogType::Force, "[Alarm] Callback 0x{:08x} for alarm 0x{:08x}. Current tick: {}. Period: {}ms", _swapEndianU32(alarm->handler), MEMPTR(alarm).GetMPTR(), currentTick, periodInMS);
#endif
				PPCCoreCallback(_swapEndianU32(alarm->handler), alarm, &(g_alarmThread.GetPtr()->context));
			}
		}
	}

	void MapAlarmExports()
	{
		cafeExportRegister("coreinit", OSCreateAlarm, LogType::CoreinitAlarm);
		cafeExportRegister("coreinit", OSCreateAlarmEx, LogType::CoreinitAlarm);
		cafeExportRegister("coreinit", OSCancelAlarm, LogType::CoreinitAlarm);
		cafeExportRegister("coreinit", OSCancelAlarms, LogType::CoreinitAlarm);
		cafeExportRegister("coreinit", OSSetAlarmTag, LogType::CoreinitAlarm);
		cafeExportRegister("coreinit", OSSetAlarm, LogType::CoreinitAlarm);
		cafeExportRegister("coreinit", OSSetPeriodicAlarm, LogType::CoreinitAlarm);
		cafeExportRegister("coreinit", OSSetAlarmEx, LogType::CoreinitAlarm);
		cafeExportRegister("coreinit", OSSetPeriodicAlarmEx, LogType::CoreinitAlarm);
		cafeExportRegister("coreinit", OSInitAlarmQueue, LogType::CoreinitAlarm);
		cafeExportRegister("coreinit", OSInitAlarmQueueEx, LogType::CoreinitAlarm);
		cafeExportRegister("coreinit", OSGetAlarmFromQueue, LogType::CoreinitAlarm);
		cafeExportRegister("coreinit", OSSetAlarmUserData, LogType::CoreinitAlarm);
		cafeExportRegister("coreinit", OSGetAlarmUserData, LogType::CoreinitAlarm);
	}

	void InitializeAlarm()
	{
		// init event
		OSInitEvent(g_alarmEvent.GetPtr(), OSEvent::EVENT_STATE::STATE_NOT_SIGNALED, OSEvent::EVENT_MODE::MODE_AUTO);

		// create alarm callback handler thread
		coreinit::OSCreateThreadType(g_alarmThread.GetPtr(), RPLLoader_MakePPCCallable(_OSAlarmThread), 0, nullptr, _g_alarmThreadStack.GetPtr() + _g_alarmThreadStack.GetByteSize(), (sint32)_g_alarmThreadStack.GetByteSize(), 0, 0x7, OSThread_t::THREAD_TYPE::TYPE_IO);
		OSResumeThread(g_alarmThread.GetPtr());
		strcpy(_g_alarmThreadName.GetPtr(), "Alarm Thread");
		coreinit::OSSetThreadName(g_alarmThread.GetPtr(), _g_alarmThreadName.GetPtr());
	}
}
