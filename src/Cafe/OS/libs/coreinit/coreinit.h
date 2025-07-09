#pragma once
#include "Cafe/HW/Espresso/Const.h"

#define PPC_CORE_COUNT     (Espresso::CORE_COUNT)

#include "Cafe/OS/libs/coreinit/coreinit_MessageQueue.h"

// async callback helper

void InitializeAsyncCallback();
void coreinitAsyncCallback_add(MPTR functionMPTR, uint32 numParameters, uint32 r3 = 0, uint32 r4 = 0, uint32 r5 = 0, uint32 r6 = 0, uint32 r7 = 0, uint32 r8 = 0, uint32 r9 = 0, uint32 r10 = 0);
void coreinitAsyncCallback_addWithLock(MPTR functionMPTR, uint32 numParameters, uint32 r3 = 0, uint32 r4 = 0, uint32 r5 = 0, uint32 r6 = 0, uint32 r7 = 0, uint32 r8 = 0, uint32 r9 = 0, uint32 r10 = 0);

void coreinit_Callbacks_Save(MemStreamWriter& s);
void coreinit_Callbacks_Restore(MemStreamReader& s);

// misc

void coreinit_save(MemStreamWriter& s);
void coreinit_restore(MemStreamReader& s);

void coreinit_load();

// coreinit shared memory
struct CoreinitSharedData
{
	MEMPTR<void> MEMAllocFromDefaultHeap;
	MEMPTR<void> MEMAllocFromDefaultHeapEx;
	MEMPTR<void> MEMFreeToDefaultHeap;
	MPTR __atexit_cleanup;
	MPTR __cpp_exception_init_ptr;
	MPTR __cpp_exception_cleanup_ptr;
	MPTR __stdio_cleanup;
};

extern CoreinitSharedData* gCoreinitData;

// coreinit init
void coreinit_start(PPCInterpreter_t* hCPU);

void coreinit_Init_Save(MemStreamWriter& s);
void coreinit_Init_Restore(MemStreamReader& s);

MPTR OSAllocFromSystem(uint32 size, uint32 alignment);
void OSFreeToSystem(MPTR mem);

// above is all the legacy stuff. New code uses namespaces

namespace coreinit
{
	sint32 OSGetCoreId();
	uint32 OSGetCoreCount();
	uint32 OSGetStackPointer();
};