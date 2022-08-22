#pragma once
#include "Cafe/OS/libs/coreinit/coreinit_MEM.h"

namespace coreinit
{
	struct MEMUnitHeapBlock
	{
		MEMPTR<MEMUnitHeapBlock> nextBlock;
	};
	static_assert(sizeof(MEMUnitHeapBlock) == 4);

	struct MEMUnitHeap : public MEMHeapBase
	{
		/* +0x34 */ uint32 padding034;
		/* +0x38 */ uint32 padding038;
		/* +0x3C */ uint32 padding03C;
		/* +0x40 */ MEMPTR<MEMUnitHeapBlock> firstFreeBlock;
		/* +0x44 */ uint32be blockSize;
	};
	static_assert(sizeof(MEMUnitHeap) == 0x48);

	MEMHeapBase* MEMCreateUnitHeapEx(void* memStart, uint32 heapSize, uint32 memBlockSize, uint32 alignment, uint32 createFlags);
	void* MEMDestroyUnitHeap(MEMHeapHandle hHeap);
}
