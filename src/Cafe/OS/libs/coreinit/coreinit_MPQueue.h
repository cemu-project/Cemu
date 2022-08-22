#pragma once

#include "Cafe/OS/libs/coreinit/coreinit.h"
#include "Cafe/OS/libs/coreinit/coreinit_Spinlock.h"

namespace coreinit
{
	enum MPTaskState
	{
		MP_TASK_STATE_INIT = (1 << 0),
		MP_TASK_STATE_READY = (1 << 1),
		MP_TASK_STATE_RUN = (1 << 2),
		MP_TASK_STATE_DONE = (1 << 3)
	};

	enum MPTaskQState
	{
		MP_TASKQ_STATE_INIT = (1 << 0),
		MP_TASKQ_STATE_RUN = (1 << 1),
		MP_TASKQ_STATE_STOPPING = (1 << 2),
		MP_TASKQ_STATE_STOP = (1 << 3),
		MP_TASKQ_STATE_DONE = (1 << 4)
	};

	struct MPTaskFunction
	{
		/* +0x00 */ MEMPTR<void> func;
		/* +0x04 */ MEMPTR<void> data;
		/* +0x08 */ uint32be size;
		/* +0x0C */ uint32be result;
	};
	static_assert(sizeof(MPTaskFunction) == 0x10);

#pragma pack(1)

	struct MPTask
	{
		/* +0x00 */ MEMPTR<void> thisptr;
		/* +0x04 */ MEMPTR<struct MPTaskQ> taskQ;
		/* +0x08 */ uint32be taskState;
		/* +0x0C */ MPTaskFunction taskFunc;
		/* +0x1C */ uint32be coreIndex;
		/* +0x20 */ sint64be runtime;
		/* +0x28 */ MEMPTR<void> userdata;
	};
	static_assert(sizeof(MPTask) == 0x2C);
	
#pragma pack()

	struct MPTaskQ
	{
		/* +0x00 */ MEMPTR<void> thisptr;
		/* +0x04 */ uint32be state;
		/* +0x08 */ uint32be taskCount;
		/* +0x0C */ uint32be taskReadyCount;
		/* +0x10 */ uint32be taskRunCount;
		/* +0x14 */ uint32be taskDoneCount[PPC_CORE_COUNT];
		/* +0x20 */ sint32be nextIndex[PPC_CORE_COUNT];
		/* +0x2C */ sint32be endIndex[PPC_CORE_COUNT];
		/* +0x38 */ MEMPTR<MEMPTR<MPTask>> taskQueue;
		/* +0x3C */ uint32be taskQueueSize;
		/* +0x40 */	OSSpinLock spinlock;
	};
	static_assert(sizeof(MPTaskQ) == 0x50);
	
	struct MPTaskQInfo
	{
		/* +0x00 */ uint32be state;
		/* +0x04 */ uint32be taskCount;
		/* +0x08 */ uint32be taskReadyCount;
		/* +0x0C */ uint32be taskRunCount;
		/* +0x10 */ uint32be taskDoneCount;
	};
	static_assert(sizeof(MPTaskQInfo) == 0x14);
	
	struct MPTaskInfo
	{
		/* +0x00 */ uint32be state;
		/* +0x04 */ uint32be funcResult;
		/* +0x08 */ uint32be coreIndex;
		/* +0x0C */ sint64be runtime;
	};
	static_assert(sizeof(MPTaskQInfo) == 0x14);

	void MPInitTask(MPTask* task, void* func, void* data, uint32 size);
	bool MPTermTask(MPTask* task);
	bool MPRunTask(MPTask* task);
	bool MPGetTaskInfo(MPTask* task, MPTaskInfo* info);
	void* MPGetTaskUserData(MPTask* task);
	void MPSetTaskUserData(MPTask* task, void* userdata);

	void MPInitTaskQ(MPTaskQ* taskq, MPTask** tasks, uint32 taskCount);
	bool MPEnqueTask(MPTaskQ* taskq, MPTask* task);
	bool MPTermTaskQ(MPTaskQ* taskq);
	bool MPGetTaskQInfo(MPTaskQ* taskq, MPTaskQInfo* info);
	bool MPStartTaskQ(MPTaskQ* taskq);
	bool MPRunTasksFromTaskQ(MPTaskQ* taskq, int granularity);
	bool MPStopTaskQ(MPTaskQ* taskq);
	bool MPWaitTaskQ(MPTaskQ* taskq, uint32 waitState);
	bool MPWaitTaskQWithTimeout(MPTaskQ* taskq, uint32 waitState, sint64 timeout);
	MPTask* MPDequeTask(MPTaskQ* taskq);
	uint32 MPDequeTasks(MPTaskQ* taskq, MPTask** tasks, sint32 maxTasks);
	bool MPResetTaskQ(MPTaskQ* taskq);

	void InitializeMP();
}
