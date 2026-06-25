#pragma once
#include "Cafe/HW/Espresso/Const.h"
#include "Cafe/OS/RPL/COSModule.h"

#define PPC_CORE_COUNT     (Espresso::CORE_COUNT)

#include "Cafe/OS/libs/coreinit/coreinit_MessageQueue.h"

// async callback helper

void InitializeAsyncCallback();
void coreinitAsyncCallback_add(MPTR functionMPTR, uint32 numParameters, uint32 r3 = 0, uint32 r4 = 0, uint32 r5 = 0, uint32 r6 = 0, uint32 r7 = 0, uint32 r8 = 0, uint32 r9 = 0, uint32 r10 = 0);
void coreinitAsyncCallback_addWithLock(MPTR functionMPTR, uint32 numParameters, uint32 r3 = 0, uint32 r4 = 0, uint32 r5 = 0, uint32 r6 = 0, uint32 r7 = 0, uint32 r8 = 0, uint32 r9 = 0, uint32 r10 = 0);

// coreinit shared memory
struct CoreinitSharedData
{
	MEMPTR<void> MEMAllocFromDefaultHeap{nullptr};
	MEMPTR<void> MEMAllocFromDefaultHeapEx{nullptr};
	MEMPTR<void> MEMFreeToDefaultHeap{nullptr};
	MEMPTR<void> __atexit_cleanup{nullptr};
	MEMPTR<void> __stdio_cleanup{nullptr};
	MPTR __cpp_exception_init_ptr{0};
	MPTR __cpp_exception_cleanup_ptr{0};
};

extern CoreinitSharedData* gCoreinitData;

// coreinit init
void coreinit_start(PPCInterpreter_t* hCPU);

MPTR OSAllocFromSystem(uint32 size, uint32 alignment);
void OSFreeToSystem(MPTR mem);

// above is all the legacy stuff. New code uses namespaces

namespace coreinit
{
	sint32 OSGetCoreId();
	uint32 OSGetCoreCount();
	uint32 OSGetStackPointer();

	COSModule* GetModule();
};