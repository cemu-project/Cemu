#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/coreinit/coreinit_MPQueue.h"
#include "Cafe/OS/libs/coreinit/coreinit_Time.h"
#include "Cafe/HW/Espresso/PPCCallback.h"
#include "util/helpers/fspinlock.h"

// titles that utilize MP task queue: Yoshi's Woolly World, Fast Racing Neo, Tokyo Mirage Sessions, Mii Maker

#define AcquireMPQLock() s_workaroundSpinlock.lock()
#define ReleaseMPQLock() s_workaroundSpinlock.unlock()

namespace coreinit
{

	FSpinlock s_workaroundSpinlock; // workaround for a race condition
	/*
		Race condition pseudo-code:

		WorkerThreads:
    		while( task = MPDequeTask() ) MPRunTask(task);
	
		MainThread:
			QueueTasks();
			// wait and reset
			MPWaitForTaskQWithTimeout(DONE)
			MPTermTaskQ()
			MPInitTaskQ()
	
		The race condition then happens when a worker thread calls MPDequeTask()/MPRunTask while MPInitTaskQ() is being executed on the main thread.
		Since MPInitTaskQ() (re)initializes the internal spinlock it's not thread-safe, leading to a corruption of the spinlock state for other threads

		We work around this by using a global spinlock instead of the taskQ specific one
	*/


	void MPInitTask(MPTask* task, void* func, void* data, uint32 size)
	{
		s_workaroundSpinlock.lock();
		task->thisptr = task;
		
		task->coreIndex = PPC_CORE_COUNT;
		
		task->taskFunc.func = func;
		task->taskFunc.data = data;
		task->taskFunc.size = size;
		
		task->taskState = MP_TASK_STATE_INIT;
		
		task->userdata = nullptr;
		task->runtime = 0;
		s_workaroundSpinlock.unlock();
	}

	bool MPTermTask(MPTask* task)
	{
		return true;
	}

	bool MPRunTask(MPTask* task)
	{
		if (task->taskState != MP_TASK_STATE_READY)
			return false;

		auto* taskQ = task->taskQ.GetPtr();

		if(taskQ->state != MP_TASKQ_STATE_STOPPING && taskQ->state != MP_TASKQ_STATE_STOP)
		{
			AcquireMPQLock(); // OSUninterruptibleSpinLock_Acquire(&taskQ->spinlock);
			if (taskQ->state == MP_TASKQ_STATE_STOPPING || taskQ->state == MP_TASKQ_STATE_STOP)
			{
				ReleaseMPQLock(); // OSUninterruptibleSpinLock_Release(&taskQ->spinlock);
				return false;
			}

			taskQ->taskReadyCount = taskQ->taskReadyCount - 1;
			taskQ->taskRunCount = taskQ->taskRunCount + 1;
			ReleaseMPQLock(); // OSUninterruptibleSpinLock_Release(&taskQ->spinlock);

			const auto startTime = OSGetSystemTime();
			
			task->coreIndex = OSGetCoreId();
			task->taskState = MP_TASK_STATE_RUN;
			
			task->taskFunc.result = PPCCoreCallback(task->taskFunc.func, task->taskFunc.data, task->taskFunc.size);

			task->taskState = MP_TASK_STATE_DONE;

			const auto endTime = OSGetSystemTime();
			task->runtime = endTime - startTime;

			cemu_assert_debug(taskQ->state != MP_TASKQ_STATE_DONE);

			AcquireMPQLock(); // OSUninterruptibleSpinLock_Acquire(&taskQ->spinlock);
			taskQ->taskRunCount = taskQ->taskRunCount - 1;
			taskQ->taskDoneCount[1] = taskQ->taskDoneCount[1] + 1;
			if (taskQ->state == MP_TASKQ_STATE_STOPPING && taskQ->taskRunCount == 0)
				taskQ->state = MP_TASKQ_STATE_STOP;

			if (taskQ->taskCount == taskQ->taskDoneCount[1])
				taskQ->state = MP_TASKQ_STATE_DONE;
			
			ReleaseMPQLock(); // OSUninterruptibleSpinLock_Release(&taskQ->spinlock);

			return true;
		}

		return false;
	}

	bool MPGetTaskInfo(MPTask* task, MPTaskInfo* info)
	{
		info->state = task->taskState;
		info->coreIndex = task->coreIndex;
		info->runtime = task->runtime;
		// not setting function result?
		return true;
	}

	void* MPGetTaskUserData(MPTask* task)
	{
		return task->userdata.GetPtr();
	}

	void MPSetTaskUserData(MPTask* task, void* userdata)
	{
		task->userdata = userdata;
	}

	void MPInitTaskQ(MPTaskQ* taskQ, MPTask** tasks, uint32 taskCount)
	{
		AcquireMPQLock();
		taskQ->thisptr = taskQ;
		OSInitSpinLock(&taskQ->spinlock);
		taskQ->state = MP_TASKQ_STATE_INIT;
		taskQ->taskReadyCount = 0;
		taskQ->taskCount = 0;
		taskQ->taskRunCount = 0;
		for(uint32 i = 0; i < OSGetCoreCount(); ++i)
		{
			taskQ->taskDoneCount[i] = 0;
			taskQ->nextIndex[i] = 0;
			taskQ->endIndex[i] = 0;
		}

		taskQ->taskQueue = (MEMPTR<MPTask>*)tasks;
		taskQ->taskQueueSize = taskCount;

		ReleaseMPQLock();
	}

	bool MPEnqueTask(MPTaskQ* taskq, MPTask* task)
	{
		bool result = false;
		if(task->taskState == MP_TASK_STATE_INIT)
		{
			AcquireMPQLock(); // OSUninterruptibleSpinLock_Acquire(&taskq->spinlock);

			const uint32 taskQState = taskq->state;
			if((uint32)taskq->endIndex[1] < taskq->taskQueueSize 
				&& (taskQState == MP_TASKQ_STATE_INIT || taskQState == MP_TASKQ_STATE_RUN || taskQState == MP_TASKQ_STATE_STOPPING || taskQState == MP_TASKQ_STATE_STOP || taskQState == MP_TASKQ_STATE_DONE))
			{
				task->taskQ = taskq;
				task->taskState = MP_TASK_STATE_READY;
				
				taskq->thisptr = taskq;
				
				const uint32 endIndex = taskq->endIndex[1];
				taskq->endIndex[1] = endIndex + 1;
				
				taskq->taskCount = taskq->taskCount + 1;
				taskq->taskReadyCount = taskq->taskReadyCount + 1;
				taskq->taskQueue[endIndex] = task;

				if (taskQState == MP_TASKQ_STATE_DONE)
					taskq->state = MP_TASKQ_STATE_RUN;

				result = true;
			}
			
			ReleaseMPQLock(); // OSUninterruptibleSpinLock_Release(&taskq->spinlock);
		}

		return result;
	}

	bool MPTermTaskQ(MPTaskQ* taskq)
	{
		// workaround code for TMS
		AcquireMPQLock(); // OSUninterruptibleSpinLock_Acquire(&taskq->spinlock);
		//if ((uint32)taskq->taskReadyCount > 0 && taskq->state == MP_TASKQ_STATE_RUN)
		if (taskq->state == MP_TASKQ_STATE_RUN)
		{
			taskq->state = MP_TASKQ_STATE_STOP;
		}

		while (taskq->taskRunCount != 0)
		{
			// wait for tasks to finish
			ReleaseMPQLock(); // OSUninterruptibleSpinLock_Release(&taskq->spinlock);
			OSYieldThread();
			AcquireMPQLock(); // OSUninterruptibleSpinLock_Acquire(&taskq->spinlock);
		}
		ReleaseMPQLock(); // OSUninterruptibleSpinLock_Release(&taskq->spinlock);
		return true;
	}

	bool MPGetTaskQInfo(MPTaskQ* taskq, MPTaskQInfo* info)
	{
		AcquireMPQLock(); // OSUninterruptibleSpinLock_Acquire(&taskq->spinlock);
		info->state = taskq->state;
		info->taskCount = taskq->taskCount;
		info->taskReadyCount = taskq->taskReadyCount;
		info->taskRunCount = taskq->taskRunCount;
		info->taskDoneCount = taskq->taskDoneCount[1];
		ReleaseMPQLock(); // OSUninterruptibleSpinLock_Release(&taskq->spinlock);
		return true;
	}

	bool MPStartTaskQ(MPTaskQ* taskq)
	{
		bool result = false;
		AcquireMPQLock(); // OSUninterruptibleSpinLock_Acquire(&taskq->spinlock);
		if (taskq->state == MP_TASKQ_STATE_INIT || taskq->state == MP_TASKQ_STATE_STOP)
		{
			taskq->state = MP_TASKQ_STATE_RUN;
			result = true;
		}
		
		ReleaseMPQLock(); // OSUninterruptibleSpinLock_Release(&taskq->spinlock);
		return result;
	}

	bool MPRunTasksFromTaskQ(MPTaskQ* taskq, int granularity)
	{
		uint32 result = 0;
		while (true)
		{
			if (taskq->state != MP_TASKQ_STATE_RUN)
				return (taskq->state & MP_TASKQ_STATE_DONE) != 0;

			AcquireMPQLock(); // OSUninterruptibleSpinLock_Acquire(&taskq->spinlock);
			const auto nextIndex = taskq->nextIndex[1];
			const auto endIndex = taskq->endIndex[1];
			if (nextIndex == endIndex) 
				break;

			auto newNextIndex = nextIndex + granularity;
			if (endIndex < nextIndex + granularity) 
				newNextIndex = endIndex;

			const auto workCount = (newNextIndex - nextIndex);
			
			taskq->nextIndex[1] = newNextIndex;
			taskq->taskReadyCount = taskq->taskReadyCount - workCount;
			taskq->taskRunCount = taskq->taskRunCount + workCount;
			ReleaseMPQLock(); // OSUninterruptibleSpinLock_Release(&taskq->spinlock);

			// since we are having a granularity parameter, we might want to give the scheduler the chance for other stuff when having multiple tasks
			if(result != 0)
				PPCCore_switchToScheduler();

			for (int i = nextIndex; i < newNextIndex; ++i)
			{
				const auto startTime = OSGetSystemTime();
				
				const auto& task = taskq->taskQueue[i];
				result = task->thisptr.GetMPTR();
				
				task->taskState = MP_TASK_STATE_RUN;
				task->coreIndex = OSGetCoreId();
				
				task->taskFunc.result = PPCCoreCallback(task->taskFunc.func, task->taskFunc.data, task->taskFunc.size);

				task->taskState = MP_TASK_STATE_DONE;

				const auto endTime = OSGetSystemTime();
				task->runtime = endTime - startTime;
			}

			AcquireMPQLock(); // OSUninterruptibleSpinLock_Acquire(&taskq->spinlock);
			const auto runRemaining = taskq->taskRunCount - workCount;
			taskq->taskRunCount = runRemaining;
			
			const auto doneCount = taskq->taskDoneCount[1] + workCount;
			taskq->taskDoneCount[1] = doneCount;
			
			if (taskq->state == 4 && runRemaining == 0) 
				taskq->state = MP_TASKQ_STATE_STOP;
			
			if (taskq->taskCount == doneCount) 
				taskq->state = MP_TASKQ_STATE_DONE;
			
			ReleaseMPQLock(); // OSUninterruptibleSpinLock_Release(&taskq->spinlock);
		}

		ReleaseMPQLock(); // OSUninterruptibleSpinLock_Release(&taskq->spinlock);
		return result != 0;
	}

	bool MPStopTaskQ(MPTaskQ* taskq)
	{
		bool result = false;
		AcquireMPQLock(); // OSUninterruptibleSpinLock_Acquire(&taskq->spinlock);
		if (taskq->state == MP_TASKQ_STATE_RUN) 
		{
			taskq->state = MP_TASKQ_STATE_STOPPING;
			if (taskq->taskRunCount == 0) 
				taskq->state = MP_TASKQ_STATE_STOP;
						
			result = true;
		}
		ReleaseMPQLock(); // OSUninterruptibleSpinLock_Release(&taskq->spinlock);
		return result;
	}

	bool MPWaitTaskQ(MPTaskQ* taskQ, uint32 waitState)
	{
		bool waitRun = (waitState & MP_TASKQ_STATE_RUN) != 0;
		bool waitStop = (waitState & MP_TASKQ_STATE_STOP) != 0;
		bool waitDone = (waitState & MP_TASKQ_STATE_DONE) != 0;
		
		size_t loopCounter = 0;
		while (waitStop || waitDone || waitRun)
		{
			const uint32 state = taskQ->state;
			if (waitRun && HAS_FLAG(state, MP_TASKQ_STATE_RUN))
			{
				waitRun = false;
				waitDone = false;
				continue;
			}
			
			if (waitStop && HAS_FLAG(state, MP_TASKQ_STATE_STOP))
			{
				waitStop = false;
				waitDone = false;
				continue;
			}
			
			if (waitDone && HAS_FLAG(state, MP_TASKQ_STATE_DONE))
			{
				waitDone = false;
				waitRun = false;
				waitStop = false;
				continue;
			}
			if (loopCounter > 0)
				coreinit::OSSleepTicks(EspressoTime::ConvertNsToTimerTicks(50000)); // sleep thread for 0.05ms to give other threads a chance to run (avoids softlocks in YWW)
			else
				PPCCore_switchToScheduler();
			loopCounter++;
		}
		return true;
	}
	
	bool MPWaitTaskQWithTimeout(MPTaskQ* taskQ, uint32 waitState, sint64 timeout)
	{
		bool waitRun = (waitState & MP_TASKQ_STATE_RUN) != 0;
		bool waitStop = (waitState & MP_TASKQ_STATE_STOP) != 0;
		bool waitDone = (waitState & MP_TASKQ_STATE_DONE) != 0;

		const auto startTime = OSGetSystemTime();
		const auto timerTicks = EspressoTime::ConvertNsToTimerTicks(timeout);
		const auto endTime = startTime + timerTicks;

		while (waitStop || waitDone || waitRun)
		{
			const uint32 state = taskQ->state;
			if (waitRun && HAS_FLAG(state, MP_TASKQ_STATE_RUN))
			{
				waitRun = false;
				waitDone = false;
				continue;
			}

			if (waitStop && HAS_FLAG(state, MP_TASKQ_STATE_STOP))
			{
				waitStop = false;
				waitDone = false;
				continue;
			}

			if (waitDone && HAS_FLAG(state, MP_TASKQ_STATE_DONE))
			{
				waitDone = false;
				waitRun = false;
				waitStop = false;
				continue;
			}

			if (OSGetSystemTime() >= endTime)
			{
				if (waitState == MP_TASKQ_STATE_DONE)
					cemuLog_log(LogType::Force, "MPWaitTaskQWithTimeout(): Timeout occurred while waiting for done-only state");
				return false;
			}

			PPCCore_switchToScheduler();
		}
		return true;
	}

	MPTask* MPDequeTask(MPTaskQ* taskq)
	{
		MPTask* result = nullptr;
		if (taskq->state == MP_TASKQ_STATE_RUN) 
		{
			AcquireMPQLock(); // OSUninterruptibleSpinLock_Acquire(&taskq->spinlock);
			if (taskq->state == MP_TASKQ_STATE_RUN && taskq->nextIndex[1] != taskq->endIndex[1])
			{
				result = taskq->taskQueue[taskq->nextIndex[1]].GetPtr();
				taskq->nextIndex[1] = taskq->nextIndex[1] + 1;
			}
			ReleaseMPQLock(); // OSUninterruptibleSpinLock_Release(&taskq->spinlock);
		}
		return result;
	}

	uint32 MPDequeTasks(MPTaskQ* taskq, MPTask** tasks, sint32 maxTasks)
	{
		uint32 dequeCount = 0;
		if (taskq->state == MP_TASKQ_STATE_RUN)
		{
			AcquireMPQLock(); // OSUninterruptibleSpinLock_Acquire(&taskq->spinlock);
			if (taskq->state == MP_TASKQ_STATE_RUN) 
			{
				auto nextIndex = (sint32)taskq->nextIndex[1];
				auto newEndIndex = nextIndex + maxTasks;
				if (taskq->endIndex[1] < nextIndex + maxTasks) 
					newEndIndex = taskq->endIndex[1];
				
				dequeCount = newEndIndex - nextIndex;
				taskq->nextIndex[1] = newEndIndex;

				for(int i = 0; nextIndex < newEndIndex; ++nextIndex, ++i)
				{
					tasks[i] = taskq->taskQueue[nextIndex].GetPtr();
				}
				
				auto idx = 0;
				while (nextIndex < newEndIndex) 
				{
					tasks[idx] = taskq->taskQueue[nextIndex].GetPtr();
					nextIndex = nextIndex + 1;
					idx = idx + 1;
				}
			}
			
			ReleaseMPQLock(); // OSUninterruptibleSpinLock_Release(&taskq->spinlock);
		}
		return dequeCount;
	}

	bool MPResetTaskQ(MPTaskQ* taskq)
	{
		debug_printf("MPResetTaskQ called\n");
		bool result = false;
		AcquireMPQLock(); // OSUninterruptibleSpinLock_Acquire(&taskq->spinlock);
		if (taskq->state == MP_TASKQ_STATE_DONE || taskq->state == MP_TASKQ_STATE_STOP) 
		{
			taskq->state = MP_TASKQ_STATE_INIT;
			taskq->taskRunCount = 0;
			taskq->taskCount = taskq->endIndex[1];
			taskq->taskReadyCount = taskq->endIndex[1];
			
			for(uint32 i = 0; i < OSGetCoreCount(); ++i)
			{
				taskq->taskDoneCount[i] = 0;
				taskq->nextIndex[i] = 0;
			}
			
			for(uint32 i = 0; i < taskq->taskCount; ++i)
			{
				const auto& task = taskq->taskQueue[i];
				task->taskFunc.result = 0;
				
				task->coreIndex = PPC_CORE_COUNT;
				task->runtime = 0;
				task->taskState = MP_TASK_STATE_READY;
			}

			result = true;
		}
		
		ReleaseMPQLock(); // OSUninterruptibleSpinLock_Release(&taskq->spinlock);
		return result;
	}

	void InitializeMP()
	{
		// task
		cafeExportRegister("coreinit", MPInitTask, LogType::CoreinitMP);
		cafeExportRegister("coreinit", MPTermTask, LogType::CoreinitMP);
		cafeExportRegister("coreinit", MPRunTask, LogType::CoreinitMP);
		cafeExportRegister("coreinit", MPGetTaskInfo, LogType::CoreinitMP);
		cafeExportRegister("coreinit", MPGetTaskUserData, LogType::CoreinitMP);
		cafeExportRegister("coreinit", MPSetTaskUserData, LogType::CoreinitMP);

		// taskq
		cafeExportRegister("coreinit", MPInitTaskQ, LogType::CoreinitMP);
		cafeExportRegister("coreinit", MPResetTaskQ, LogType::CoreinitMP);
		
		cafeExportRegister("coreinit", MPEnqueTask, LogType::CoreinitMP);
		cafeExportRegister("coreinit", MPDequeTask, LogType::CoreinitMP);
		cafeExportRegister("coreinit", MPDequeTasks, LogType::CoreinitMP);

		cafeExportRegister("coreinit", MPRunTasksFromTaskQ, LogType::CoreinitMP);
		cafeExportRegister("coreinit", MPStartTaskQ, LogType::CoreinitMP);
		cafeExportRegister("coreinit", MPWaitTaskQ, LogType::CoreinitMP);
		cafeExportRegister("coreinit", MPWaitTaskQWithTimeout, LogType::CoreinitMP);
		cafeExportRegister("coreinit", MPStopTaskQ, LogType::CoreinitMP);
		
		cafeExportRegister("coreinit", MPTermTaskQ, LogType::CoreinitMP);
		cafeExportRegister("coreinit", MPGetTaskQInfo, LogType::CoreinitMP);
	}
}
