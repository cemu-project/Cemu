#pragma once
#include "Cafe/OS/libs/coreinit/coreinit_MEM.h"

namespace coreinit
{
	struct MEMBlockHeapTrack2_t
	{
		/* +0x00 */ MEMPTR<void> addrStart;
		/* +0x04 */ MEMPTR<void> addrEnd;
		/* +0x08 */ uint32be isFree; // if 0 -> block is used
		/* +0x0C */ MEMPTR<void> previousBlock;
		/* +0x10 */ MEMPTR<void> nextBlock;
	};
	static_assert(sizeof(MEMBlockHeapTrack2_t) == 0x14);

	struct MEMBlockHeap2_t : MEMHeapBase
	{
		/* +0x34 */ uint8 ukn[0x50 - 0x34];
		/* +0x50 */ MEMBlockHeapTrack2_t track;
		/* +0x64 */ MEMPTR<void> headBlock;
		/* +0x68 */ MEMPTR<void> tailBlock;
		/* +0x6C */ MEMPTR<void> nextFreeBlock;
		/* +0x70 */ uint32be freeBlocksLeft;
		/* +0x74 */ uint8 padding[0x80 - 0x74];
	};
	static_assert(sizeof(MEMBlockHeap2_t) == 0x80);

	MEMHeapHandle MEMInitBlockHeap(MEMBlockHeap2_t* memStart, void* startAddr, void* endAddr, void* initTrackMem, uint32 initTrackMemSize, uint32 createFlags);
	void* MEMDestroyBlockHeap(MEMHeapHandle hHeap);

	sint32 MEMAddBlockHeapTracking(MPTR heap, MPTR trackMem, uint32 trackMemSize);

	void InitializeMEMBlockHeap();
}
