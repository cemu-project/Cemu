#pragma once

void __OSLockScheduler(void* obj = nullptr);
bool __OSHasSchedulerLock();
bool __OSTryLockScheduler(void* obj = nullptr);
void __OSUnlockScheduler(void* obj = nullptr);

namespace coreinit
{
	uint32 OSIsInterruptEnabled();
	uint32 OSDisableInterrupts();
	uint32 OSRestoreInterrupts(uint32 interruptMask);
	uint32 OSEnableInterrupts();

	void InitializeSchedulerLock();
}