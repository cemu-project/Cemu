#pragma once

#include "Cafe/OS/libs/coreinit/coreinit_Spinlock.h"

struct MEMLink_t
{
	MPTR prevObject; 
	MPTR nextObject;
};

static_assert(sizeof(MEMLink_t) == 8);

struct MEMList_t
{
	MPTR headObject;
	MPTR tailObject;
	uint16 numObjects;
	uint16 offset;
};

static_assert(sizeof(MEMList_t) == 0xC);

struct MEMAllocatorFunc
{
	MEMPTR<void> funcAlloc;
	MEMPTR<void> funcFree;
};

static_assert(sizeof(MEMAllocatorFunc) == 8);

struct MEMAllocator
{
	/* +0x000 */ MEMPTR<MEMAllocatorFunc> func;
	/* +0x004 */ MEMPTR<void> heap;
	/* +0x00C */ uint32be param1;
	/* +0x010 */ uint32be param2;
};

static_assert(sizeof(MEMAllocator) == 0x10);

MPTR coreinit_allocFromSysArea(uint32 size, uint32 alignment);
void coreinit_freeToSysArea(MPTR mem);

// mem exports
void coreinitExport_MEMInitAllocatorForDefaultHeap(PPCInterpreter_t* hCPU);
void coreinitExport_MEMGetBaseHeapHandle(PPCInterpreter_t* hCPU);

/* legacy stuff above */

namespace coreinit
{
#define MEM_HEAP_INVALID_HANDLE (nullptr)
#define MEM_HEAP_DEFAULT_ALIGNMENT 4
#define MIN_ALIGNMENT 4
#define MIN_ALIGNMENT_MINUS_ONE (MIN_ALIGNMENT-1)

#define MEM_HEAP_OPTION_NONE		(0)
#define MEM_HEAP_OPTION_CLEAR		(1 << 0)
#define MEM_HEAP_OPTION_FILL		(1 << 1)
#define MEM_HEAP_OPTION_THREADSAFE	(1 << 2)

	enum class MEMHeapMagic : uint32
	{
		UNIT_HEAP = 'UNTH',
		BLOCK_HEAP = 'BLKH',
		FRAME_HEAP = 'FRMH',
		EXP_HEAP = 'EXPH',
		USER_HEAP = 'USRH',
	};

	struct MEMLink
	{
		MEMPTR<void> prev;
		MEMPTR<void> next;
	};
	static_assert(sizeof(MEMLink) == 0x8);

	struct MEMList
	{
		/* 0x00 */ MEMPTR<void> head;
		/* 0x04 */ MEMPTR<void> tail;
		/* 0x08 */ uint16be numObjects;
		/* 0x0A */ uint16be offset;
	};
	static_assert(sizeof(MEMList) == 0xC);

	void MEMInitList(MEMList* list, uint32 offset);
	void MEMAppendListObject(MEMList* list, void* object);
	void MEMRemoveListObject(MEMList* list, void* object);

	void* MEMGetFirstListObject(MEMList* list);
	void* MEMGetNextListObject(MEMList* list, void* object);

	struct MEMHeapBase
	{
		/* +0x00 */ betype<MEMHeapMagic> magic;
		/* +0x04 */ MEMLink link;
		/* +0x0C */ MEMList childList;
		/* +0x18 */ MEMPTR<void> heapStart;
		/* +0x1C */ MEMPTR<void> heapEnd; // heap end + 1
		/* +0x20 */ OSSpinLock spinlock;
		/* +0x30 */ uint8 _ukn[3];
		/* +0x33 */ uint8 flags;

		void AcquireLock()
		{
			if (flags & MEM_HEAP_OPTION_THREADSAFE)
				OSUninterruptibleSpinLock_Acquire(&spinlock);
		}

		void ReleaseLock()
		{
			if (flags & MEM_HEAP_OPTION_THREADSAFE)
				OSUninterruptibleSpinLock_Release(&spinlock);
		}

		// if set, memset allocations to zero
		bool HasOptionClear() const
		{
			return (flags & MEM_HEAP_OPTION_CLEAR) != 0;
		}

		// if set, memset allocations/releases to specific fill values
		bool HasOptionFill() const
		{
			return (flags & MEM_HEAP_OPTION_FILL) != 0;
		}
	};

	static_assert(offsetof(MEMHeapBase, childList) == 0xC);
	static_assert(offsetof(MEMHeapBase, spinlock) == 0x20);
	static_assert(offsetof(MEMHeapBase, flags) == 0x33);
	static_assert(sizeof(MEMHeapBase) == 0x34); // heap base is actually 0x40 but bytes 0x34 to 0x40 are padding?

	typedef MEMHeapBase* MEMHeapHandle;

	/* Heap base */

	void MEMInitHeapBase(MEMHeapBase* heap, MEMHeapMagic magic, void* heapStart, void* heapEnd, uint32 createFlags);
	void MEMBaseDestroyHeap(MEMHeapBase* heap);

	MEMHeapBase* MEMGetBaseHeapHandle(uint32 index);
	MEMHeapBase* MEMSetBaseHeapHandle(uint32 index, MEMHeapBase* heapBase);

	/* Heap list */

	bool MEMHeapTable_Add(MEMHeapBase* heap);
	bool MEMHeapTable_Remove(MEMHeapBase* heap);
	MEMHeapBase* _MEMList_FindContainingHeap(MEMList* list, MEMHeapBase* heap);
	bool MEMList_ContainsHeap(MEMList* list, MEMHeapBase* heap);
	MEMList* MEMList_FindContainingHeap(MEMHeapBase* head);

	/* Heap settings */

	enum class HEAP_FILL_TYPE : uint32
	{
		ON_HEAP_CREATE = 0,
		ON_ALLOC = 1,
		ON_FREE = 2,
	};

	uint32 MEMGetFillValForHeap(HEAP_FILL_TYPE type);
	uint32 MEMSetFillValForHeap(HEAP_FILL_TYPE type, uint32 value);
	MEMHeapHandle MEMFindContainHeap(const void* memBlock);

	/* Heap default allocators */

	void InitDefaultHeaps(MEMPTR<MEMHeapBase>& mem1Heap, MEMPTR<MEMHeapBase>& memFGHeap, MEMPTR<MEMHeapBase>& mem2Heap);

	void* default_MEMAllocFromDefaultHeap(uint32 size);
	void* default_MEMAllocFromDefaultHeapEx(uint32 size, sint32 alignment);
	void default_MEMFreeToDefaultHeap(void* mem);

	void* _weak_MEMAllocFromDefaultHeapEx(uint32 size, sint32 alignment);
	void* _weak_MEMAllocFromDefaultHeap(uint32 size);
	void _weak_MEMFreeToDefaultHeap(void* ptr);

	/* Unit heap */

	void InitializeMEMUnitHeap();

	void InitializeMEM();
}
