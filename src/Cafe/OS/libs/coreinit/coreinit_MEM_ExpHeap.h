#pragma once

#include "Cafe/OS/libs/coreinit/coreinit_Spinlock.h"
#include "Cafe/OS/libs/coreinit/coreinit_MEM.h"

namespace coreinit
{
	void expheap_load();

#define MEM_EXPHEAP_ALLOC_MODE_FIRST	(0)
#define MEM_EXPHEAP_ALLOC_MODE_NEAR		(1)
#define MEM_EXPHEAP_USE_ALIGN_MARGIN	(2)

	enum class MEMExpHeapAllocDirection : uint32
	{
		HEAD = 0,
		TAIL = 1
	};

	struct MBlockChain2_t
	{
		MEMPTR<struct MBlock2_t> headMBlock; // 0x00
		MEMPTR<struct MBlock2_t> tailMBlock; // 0x04
	};
	static_assert(sizeof(MBlockChain2_t) == 8);

	struct MEMExpHeapHead40_t
	{
		/* +0x00 */ MBlockChain2_t chainFreeBlocks; // 0x00
		/* +0x08 */ MBlockChain2_t chainUsedBlocks; // 0x08
		/* +0x10 */ uint16 groupID;
		/* +0x12 */ uint16 fields; // Bit 0 -> Alloc mode, Bit 1 -> Allocate within alignment (create free blocks inside alignment padding)
	};

	static_assert(sizeof(MEMExpHeapHead40_t) == 0x14);

	struct MEMExpHeapHead2 : MEMHeapBase
	{
		// Base
		/* +0x34 */ uint32be ukn34;
		/* +0x38 */ uint32be ukn38;
		/* +0x3C */ uint32be ukn3C;
		/* +0x40 */ MEMExpHeapHead40_t expHeapHead;
	};

	static_assert(sizeof(MEMExpHeapHead2) == 0x54);

	MEMHeapHandle MEMCreateExpHeapEx(void* startAddress, uint32 size, uint32 createFlags);
	void* MEMAllocFromExpHeapEx(MEMHeapHandle heap, uint32 size, sint32 alignment);
	void MEMFreeToExpHeap(MEMHeapHandle heap, void* mem);
	uint32 MEMGetAllocatableSizeForExpHeapEx(MEMHeapHandle heap, sint32 alignment);
}
