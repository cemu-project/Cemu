#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/HW/Espresso/PPCCallback.h"
#include "Cafe/OS/libs/coreinit/coreinit_MEM_ExpHeap.h"

#define EXP_HEAP_GET_FROM_FREE_BLOCKCHAIN(__blockchain__) (MEMExpHeapHead2*)((uintptr_t)__blockchain__ - offsetof(MEMExpHeapHead2, expHeapHead) - offsetof(MEMExpHeapHead40_t, chainFreeBlocks))

namespace coreinit
{
	#define MBLOCK_TYPE_FREE ('FR')
	#define MBLOCK_TYPE_USED ('UD')

	struct MBlock2_t
	{
		uint32be fields; // 0x00
		uint32be dataSize; // 0x04 | size of raw data (excluding size of this struct)
		MEMPTR<MBlock2_t> prevBlock; // 0x08
		MEMPTR<MBlock2_t> nextBlock; // 0x0C
		uint16be typeCode; // 0x10
		uint16be padding; // 0x12
	};
	static_assert(sizeof(MBlock2_t) == 0x14);

	struct ExpMemBlockRegion
	{
		uintptr_t start;
		uintptr_t end;
	};

#define MBLOCK_GET_HEADER(__mblock__) (MBlock2_t*)((uintptr_t)__mblock__ - sizeof(MBlock2_t))
#define MBLOCK_GET_MEMORY(__mblock__) ((uintptr_t)__mblock__ + sizeof(MBlock2_t))
#define MBLOCK_GET_END(__mblock__) ((uintptr_t)__mblock__ + sizeof(MBlock2_t) + (uint32)__mblock__->dataSize)

#pragma region internal
	MBlock2_t* _MEMExpHeap_InitMBlock(ExpMemBlockRegion* region, uint16 typeCode)
	{
		MBlock2_t* mBlock = (MBlock2_t*)region->start;
		memset(mBlock, 0x00, sizeof(MBlock2_t));
		mBlock->typeCode = typeCode;
		mBlock->dataSize = (uint32)(region->end - (region->start + sizeof(MBlock2_t)));
		return mBlock;
	}

	bool _MEMExpHeap_CheckMBlock(MBlock2_t* mBlock, MEMHeapHandle heap, uint16 typeCode, const char* debugText, int options)
	{
#ifndef MBLOCK_DEBUG_ASSERT 
		return true;
#endif
		const uintptr_t mBlockAddr = (uintptr_t)mBlock;
		const uintptr_t mBlockRawAddr = MBLOCK_GET_MEMORY(mBlock);
		uintptr_t heapStart = 0, heapEnd = 0;
		if (heap)
		{
			heapStart = (uintptr_t)heap->heapStart.GetPtr();
			heapEnd = (uintptr_t)heap->heapEnd.GetPtr();
			if (heap == MEM_HEAP_INVALID_HANDLE)
			{
				if (mBlockAddr < heapStart || mBlockRawAddr > heapEnd)
				{
					return false;
				}
			}
		}
		if (typeCode == (uint16)mBlock->typeCode)
		{
			if (heap == MEM_HEAP_INVALID_HANDLE)
				return true;

			const uint32 dataSize = mBlock->dataSize;
			if (mBlockRawAddr + dataSize <= heapEnd)
				return true;
			return false;
		}
		return false;
	}

	bool _MEMExpHeap_IsValidUsedMBlock(const void* memBlock, MEMHeapHandle heap)
	{
#ifndef MBLOCK_DEBUG_ASSERT 
		return true;
#endif
		if (!memBlock)
			return false;

		heap->AcquireLock();

		bool valid = false;
		MBlock2_t* mBlock = MBLOCK_GET_HEADER(memBlock);

		if (heap)
		{
			MEMExpHeapHead2* expHeap = (MEMExpHeapHead2*)heap;
			MEMPTR<MBlock2_t> usedBlock = expHeap->expHeapHead.chainUsedBlocks.headMBlock;
			while (usedBlock)
			{
				if (mBlock == usedBlock.GetPtr())
				{
					valid = _MEMExpHeap_CheckMBlock(mBlock, heap, MBLOCK_TYPE_USED, "used", 0);
					break;
				}

				usedBlock = usedBlock->nextBlock;
			}
		}
		else
		{
			valid = _MEMExpHeap_CheckMBlock(mBlock, heap, MBLOCK_TYPE_USED, "used", 0);
		}

		heap->ReleaseLock();

		return valid;
	}

	void _MEMExpHeap_GetRegionOfMBlock(ExpMemBlockRegion* region, MBlock2_t* memBlock)
	{
		uint32 alignment = ((memBlock->fields) >> 8) & 0x7FFFFF;

		uintptr_t memBlockAddr = (uintptr_t)memBlock;
		region->start = memBlockAddr - alignment;
		region->end = memBlockAddr + sizeof(MBlock2_t) + (uint32)memBlock->dataSize;
	}

	void* _MEMExpHeap_RemoveMBlock(MBlockChain2_t* blockChain, MBlock2_t* block)
	{
		MEMPTR<MBlock2_t> prevBlock = block->prevBlock;
		MEMPTR<MBlock2_t> nextBlock = block->nextBlock;

		if (prevBlock)
			prevBlock->nextBlock = nextBlock;
		else
			blockChain->headMBlock = nextBlock;

		if (nextBlock)
			nextBlock->prevBlock = prevBlock;
		else
			blockChain->tailMBlock = prevBlock;

		return prevBlock.GetPtr();
	}

	MBlock2_t* _MEMExpHeap_InsertMBlock(MBlockChain2_t* blockChain, MBlock2_t* newBlock, MBlock2_t* prevBlock)
	{
		newBlock->prevBlock = prevBlock;

		MEMPTR<MBlock2_t> nextBlock;
		if (prevBlock)
		{
			nextBlock = prevBlock->nextBlock;
			prevBlock->nextBlock = newBlock;
		}
		else
		{
			nextBlock = blockChain->headMBlock;
			blockChain->headMBlock = newBlock;
		}

		newBlock->nextBlock = nextBlock;
		if (nextBlock)
			nextBlock->prevBlock = newBlock;
		else
			blockChain->tailMBlock = newBlock;

		return newBlock;
	}

	bool _MEMExpHeap_RecycleRegion(MBlockChain2_t* blockChain, ExpMemBlockRegion* region)
	{
		ExpMemBlockRegion newRegion;
		newRegion.start = region->start;
		newRegion.end = region->end;

		MEMPTR<MBlock2_t> prev;
		MEMPTR<MBlock2_t> find = blockChain->headMBlock;
		while (find)
		{
			MBlock2_t* findMBlock = find.GetPtr();
			const uintptr_t blockAddr = (uintptr_t)findMBlock;
			if (blockAddr >= region->start)
			{
				if (blockAddr == region->end)
				{
					newRegion.end = MBLOCK_GET_END(findMBlock);
					_MEMExpHeap_RemoveMBlock(blockChain, findMBlock);

					MEMExpHeapHead2* heap = EXP_HEAP_GET_FROM_FREE_BLOCKCHAIN(blockChain);
					uint8 options = heap->flags;
					if (HAS_FLAG(options, MEM_HEAP_OPTION_FILL))
					{
						const uint32 fillVal = MEMGetFillValForHeap(HEAP_FILL_TYPE::ON_FREE);
						memset(findMBlock, fillVal, sizeof(MBlock2_t));
					}
				}

				break;
			}

			prev = find;
			find = findMBlock->nextBlock;
		}

		if (prev)
		{
			MBlock2_t* prevMBlock = prev.GetPtr();
			if (MBLOCK_GET_END(prevMBlock) == region->start)
			{
				newRegion.start = (uintptr_t)prevMBlock;
				prev = (MBlock2_t*)_MEMExpHeap_RemoveMBlock(blockChain, prevMBlock);
			}
		}

		if ((newRegion.end - newRegion.start) < sizeof(MBlock2_t))
			return false;

		MEMExpHeapHead2* heap = EXP_HEAP_GET_FROM_FREE_BLOCKCHAIN(blockChain);
		uint8 options = heap->flags;
		if (HAS_FLAG(options, MEM_HEAP_OPTION_FILL))
		{
			const uint32 fillVal = MEMGetFillValForHeap(HEAP_FILL_TYPE::ON_FREE);
			memset((void*)region->start, fillVal, region->end - region->start);
		}

		MBlock2_t* newBlock = _MEMExpHeap_InitMBlock(&newRegion, MBLOCK_TYPE_FREE);
		_MEMExpHeap_InsertMBlock(blockChain, newBlock, prev.GetPtr());
		return true;
	}

void* _MEMExpHeap_AllocUsedBlockFromFreeBlock(MBlockChain2_t* blockChain, MBlock2_t* freeBlock, uintptr_t blockMemStart, uint32 size, MEMExpHeapAllocDirection direction)
{
	MEMExpHeapHead2* heap = EXP_HEAP_GET_FROM_FREE_BLOCKCHAIN(blockChain);

	ExpMemBlockRegion freeRegion;
	_MEMExpHeap_GetRegionOfMBlock(&freeRegion, freeBlock);

	ExpMemBlockRegion newRegion = {blockMemStart + size, freeRegion.end};
	freeRegion.end = blockMemStart - sizeof(MBlock2_t);

	MBlock2_t* prevBlock = (MBlock2_t*)_MEMExpHeap_RemoveMBlock(blockChain, freeBlock);

	if ((freeRegion.end - freeRegion.start) >= 0x18 && (direction != MEMExpHeapAllocDirection::HEAD || HAS_FLAG(heap->expHeapHead.fields, MEM_EXPHEAP_USE_ALIGN_MARGIN)))
	{
		MBlock2_t* newBlock = _MEMExpHeap_InitMBlock(&freeRegion, MBLOCK_TYPE_FREE);
		prevBlock = _MEMExpHeap_InsertMBlock(blockChain, newBlock, prevBlock);
	}
	else
		freeRegion.end = freeRegion.start;

	if ((newRegion.end - newRegion.start) >= 0x18 && (direction != MEMExpHeapAllocDirection::TAIL || HAS_FLAG(heap->expHeapHead.fields, MEM_EXPHEAP_USE_ALIGN_MARGIN)))
	{
		MBlock2_t* newBlock = _MEMExpHeap_InitMBlock(&newRegion, MBLOCK_TYPE_FREE);
		prevBlock = _MEMExpHeap_InsertMBlock(blockChain, newBlock, prevBlock);
	}
	else
		newRegion.start = newRegion.end;

	uint8 options = heap->flags;
	if (HAS_FLAG(options, MEM_HEAP_OPTION_CLEAR))
	{
		memset((void*)freeRegion.end, 0x00, newRegion.start - freeRegion.end);
	}
	else if (HAS_FLAG(options, MEM_HEAP_OPTION_FILL))
	{
		const uint32 fillValue = MEMGetFillValForHeap(HEAP_FILL_TYPE::ON_ALLOC);
		memset((void*)freeRegion.end, fillValue, newRegion.start - freeRegion.end);
	}

	ExpMemBlockRegion allocRegion = {blockMemStart - sizeof(MBlock2_t), newRegion.start};
	MBlock2_t* newBlock = _MEMExpHeap_InitMBlock(&allocRegion, MBLOCK_TYPE_USED);

	uint32 fields = (uint32)newBlock->fields;
	if(direction == MEMExpHeapAllocDirection::TAIL)
		fields |= 1 << 31;
	else
		fields &= ~(1 << 31);
	uint32 alignmentPadding = (uint32)((uintptr_t)newBlock - (uintptr_t)freeRegion.end);
	fields |= (alignmentPadding & 0x7FFFFF) << 8;
	fields |= ((uint32)heap->expHeapHead.groupID & 0xFF);
	newBlock->fields = fields;
	
	MBlock2_t* tailBlock = heap->expHeapHead.chainUsedBlocks.tailMBlock.GetPtr();
	_MEMExpHeap_InsertMBlock(&heap->expHeapHead.chainUsedBlocks, newBlock, tailBlock);

	return (void*)blockMemStart;
}

void* _MEMExpHeap_AllocFromTail(MEMHeapHandle heap, uint32 size, int alignment)
{
	MEMExpHeapHead2* expHeap = (MEMExpHeapHead2*)heap;

	const bool searchForFirstEntry = (expHeap->expHeapHead.fields&1) == MEM_EXPHEAP_ALLOC_MODE_FIRST;
	const int alignmentMinusOne = alignment - 1;

	MBlock2_t* freeBlock = nullptr;
	uintptr_t blockMemStart = 0;
	uint32 foundSize = -1;

	for (MBlock2_t* findBlock = expHeap->expHeapHead.chainFreeBlocks.tailMBlock.GetPtr(); findBlock != nullptr; findBlock = findBlock->prevBlock.GetPtr())
	{
		const uintptr_t blockMemory = MBLOCK_GET_MEMORY(findBlock);

		const uint32 dataSize = (uint32)findBlock->dataSize;
		const uintptr_t alignedEndBlockMemory = (blockMemory + dataSize - size) & ~alignmentMinusOne;
		
		if (alignedEndBlockMemory < blockMemory)
			continue;

		if (foundSize <= dataSize)
			continue;

		freeBlock = findBlock;
		blockMemStart = alignedEndBlockMemory;
		foundSize = dataSize;

		if (searchForFirstEntry)
			break;

		if (foundSize == size)
			break;
	}

	void* mem = nullptr;
	if (freeBlock)
		mem = _MEMExpHeap_AllocUsedBlockFromFreeBlock(&expHeap->expHeapHead.chainFreeBlocks, freeBlock, blockMemStart, size, coreinit::MEMExpHeapAllocDirection::TAIL);

	return mem;
}

void* _MEMExpHeap_AllocFromHead(MEMHeapHandle heap, uint32 size, int alignment)
{
	MEMExpHeapHead2* expHeap = (MEMExpHeapHead2*)heap;

	const bool searchForFirstEntry = (expHeap->expHeapHead.fields&1) == MEM_EXPHEAP_ALLOC_MODE_FIRST;
	const int alignmentMinusOne = alignment - 1;

	MBlock2_t* freeBlock = nullptr;
	uintptr_t blockMemStart = 0;
	uint32 foundSize = -1;

	for (MBlock2_t* findBlock = expHeap->expHeapHead.chainFreeBlocks.headMBlock.GetPtr(); findBlock != nullptr; findBlock = findBlock->nextBlock.GetPtr())
	{
		const uintptr_t blockMemory = MBLOCK_GET_MEMORY(findBlock);
		const uintptr_t alignedBlockMemory = (blockMemory + alignmentMinusOne) & ~alignmentMinusOne;
		const uint32 dataSize = (uint32)findBlock->dataSize;

		if (dataSize < alignedBlockMemory - blockMemory + size)
			continue;

		if (foundSize <= dataSize)
			continue;

		freeBlock = findBlock;
		blockMemStart = alignedBlockMemory;
		foundSize = dataSize;

		if (searchForFirstEntry)
			break;

		if (foundSize == size)
			break;
	}

	void* mem = nullptr;
	if (freeBlock)
		mem = _MEMExpHeap_AllocUsedBlockFromFreeBlock(&expHeap->expHeapHead.chainFreeBlocks, freeBlock, blockMemStart, size, coreinit::MEMExpHeapAllocDirection::HEAD);

	return mem;
}

MEMHeapHandle _MEMExpHeap_InitHeap(void* startAddress, void* endAddress, uint32 createFlags)
{
	MEMExpHeapHead2* header = (MEMExpHeapHead2*)startAddress;
	uintptr_t heapStart = (uintptr_t)startAddress + sizeof(MEMExpHeapHead2);

	MEMInitHeapBase(header, coreinit::MEMHeapMagic::EXP_HEAP, (void*)heapStart, endAddress, createFlags);

	ExpMemBlockRegion region;
	region.start = (uintptr_t)header->heapStart.GetPtr();
	region.end = (uintptr_t)header->heapEnd.GetPtr();

	MBlock2_t* mBlock = _MEMExpHeap_InitMBlock(&region, MBLOCK_TYPE_FREE);

	header->expHeapHead.chainFreeBlocks.headMBlock = mBlock;
	header->expHeapHead.chainFreeBlocks.tailMBlock = mBlock;

	header->expHeapHead.chainUsedBlocks.headMBlock = nullptr;
	header->expHeapHead.chainUsedBlocks.tailMBlock = nullptr;

	header->expHeapHead.groupID = 0;
	header->expHeapHead.fields = 0;

	return (MEMHeapHandle)header;
}

MEMHeapHandle MEMCreateExpHeap(void* startAddress, uint32 size)
{
	return MEMCreateExpHeapEx(startAddress, size, MEM_HEAP_OPTION_NONE);
}

void* MEMAllocFromExpHeap(MEMHeapHandle heap, uint32 size)
{
	return MEMAllocFromExpHeapEx(heap, size, MEM_HEAP_DEFAULT_ALIGNMENT);
}

uint32 MEMGetAllocatableSizeForExpHeap(MEMHeapHandle heap)
{
	return MEMGetAllocatableSizeForExpHeapEx(heap, MEM_HEAP_DEFAULT_ALIGNMENT);
}

void IsValidExpHeapHandle_(MEMHeapHandle heap)
{
	cemu_assert_debug(heap != MEM_HEAP_INVALID_HANDLE);
	cemu_assert_debug(heap->magic == coreinit::MEMHeapMagic::EXP_HEAP);
}

#pragma endregion

#pragma region exported

MEMHeapHandle MEMCreateExpHeapEx(void* startAddress, uint32 size, uint32 createFlags)
{
	if (startAddress == nullptr)
		return MEM_HEAP_INVALID_HANDLE;

	const uintptr_t alignedStart = ((uintptr_t)startAddress + MIN_ALIGNMENT_MINUS_ONE) & (~MIN_ALIGNMENT_MINUS_ONE);
	const uintptr_t alignedEnd = ((uintptr_t)startAddress + size) & (~MIN_ALIGNMENT_MINUS_ONE);

	if (alignedStart > alignedEnd)
		return MEM_HEAP_INVALID_HANDLE;

	if (alignedEnd - alignedStart < 0x6C)
		return MEM_HEAP_INVALID_HANDLE;

	MEMHeapHandle result = _MEMExpHeap_InitHeap((void*)alignedStart, (void*)alignedEnd, createFlags);
	if (result)
		MEMHeapTable_Add(result);

	return result;
}

void* MEMDestroyExpHeap(MEMHeapHandle heap)
{
	IsValidExpHeapHandle_(heap);
	MEMBaseDestroyHeap(heap);
	MEMHeapTable_Remove(heap);
	return heap;
}

void* MEMAllocFromExpHeapEx(MEMHeapHandle heap, uint32 size, sint32 alignment)
{
	IsValidExpHeapHandle_(heap);
	if (alignment == 0)
	{
		// this is a guaranteed crash on the actual console
		cemuLog_log(LogType::Force, "MEMAllocFromExpHeapEx(): Alignment 0 not allowed");
		alignment = 4;
		return nullptr;
	}

	if (size == 0)
		size = 1;

	size = (size + MIN_ALIGNMENT_MINUS_ONE) & ~MIN_ALIGNMENT_MINUS_ONE;

	heap->AcquireLock();

	void* mem;
	if (alignment < 0)
	{
		alignment = -alignment;
		mem = _MEMExpHeap_AllocFromTail(heap, size, alignment);
	}
	else
	{
		mem = _MEMExpHeap_AllocFromHead(heap, size, alignment);
	}

	heap->ReleaseLock();

	return mem;
}

void MEMFreeToExpHeap(MEMHeapHandle heap, void* mem)
{
	IsValidExpHeapHandle_(heap);
	if (mem)
	{
		heap->AcquireLock();

		cemu_assert_debug(_MEMExpHeap_IsValidUsedMBlock(mem, heap) == true);
		cemu_assert_debug((uintptr_t)heap->heapStart.GetPtr() <= (uintptr_t)mem && (uintptr_t)mem < (uintptr_t)heap->heapEnd.GetPtr());

		MEMExpHeapHead2* expHeap = (MEMExpHeapHead2*)heap;

		ExpMemBlockRegion region;
		MBlock2_t* mBlock = MBLOCK_GET_HEADER(mem);
		_MEMExpHeap_GetRegionOfMBlock(&region, mBlock);

		_MEMExpHeap_RemoveMBlock(&expHeap->expHeapHead.chainUsedBlocks, mBlock);
		_MEMExpHeap_RecycleRegion(&expHeap->expHeapHead.chainFreeBlocks, &region);

		heap->ReleaseLock();
	}
}

uint16 MEMSetAllocModeForExpHeap(MEMHeapHandle heap, uint16 mode)
{
	IsValidExpHeapHandle_(heap);
	heap->AcquireLock();

	MEMExpHeapHead2* expHeap = (MEMExpHeapHead2*)heap;
	const uint16 oldMode = expHeap->expHeapHead.fields & 0x1;
	expHeap->expHeapHead.fields |= (mode & 0x1);

	heap->ReleaseLock();

	return oldMode;
}

uint16 MEMGetAllocModeForExpHeap(MEMHeapHandle heap)
{
	IsValidExpHeapHandle_(heap);
	heap->AcquireLock();

	MEMExpHeapHead2* expHeap = (MEMExpHeapHead2*)heap;
	const uint16 mode = expHeap->expHeapHead.fields & 0x1;

	heap->ReleaseLock();

	return mode;
}

uint32 MEMAdjustExpHeap(MEMHeapHandle heap)
{
	uint32 newSize = 0;

	IsValidExpHeapHandle_(heap);
	heap->AcquireLock();

	MEMExpHeapHead2* expHeap = (MEMExpHeapHead2*)heap;

	MEMPTR<MBlock2_t> tailBlock = expHeap->expHeapHead.chainFreeBlocks.tailMBlock;
	if (tailBlock)
	{
		MBlock2_t* tail = tailBlock.GetPtr();

		uintptr_t blockMemEnd = MBLOCK_GET_END(tail);
		uintptr_t heapEnd = (uintptr_t)heap->heapEnd.GetPtr();
		if (blockMemEnd == heapEnd)
		{
			_MEMExpHeap_RemoveMBlock(&expHeap->expHeapHead.chainFreeBlocks, tail);

			uint32 removedBlockSize = sizeof(MBlock2_t) + (uint32)tail->dataSize;
			uintptr_t newHeapEnd = heapEnd - removedBlockSize;
			heap->heapEnd = (void*)newHeapEnd;

			newSize = (uint32)(newHeapEnd - (uintptr_t)heap);
		}
	}

	heap->ReleaseLock();

	return newSize;
}

uint32 MEMResizeForMBlockExpHeap(MEMHeapHandle heap, void* memBlock, uint32 size)
{
	uint32 newSize = 0;

	IsValidExpHeapHandle_(heap);
	cemu_assert_debug(memBlock != nullptr);
	heap->AcquireLock();

	MBlock2_t* mBlock = MBLOCK_GET_HEADER(memBlock);
	const uint32 dataSize = mBlock->dataSize;
	if (dataSize != size)
	{
		cemu_assert_debug(_MEMExpHeap_IsValidUsedMBlock(memBlock, heap));

		MEMExpHeapHead2* expHeap = (MEMExpHeapHead2*)heap;

		if (size <= dataSize)
		{
			ExpMemBlockRegion region;
			region.start = (uintptr_t)memBlock + size;
			region.end = MBLOCK_GET_END(mBlock);

			mBlock->dataSize = size;

			if (!_MEMExpHeap_RecycleRegion(&expHeap->expHeapHead.chainFreeBlocks, &region))
				mBlock->dataSize = dataSize;

			newSize = (uint32)mBlock->dataSize;
		}
		else
		{
			MEMPTR<MBlock2_t> free = expHeap->expHeapHead.chainFreeBlocks.headMBlock;
			const uintptr_t blockEndAddr = MBLOCK_GET_END(mBlock);
			while (free)
			{
				MBlock2_t* freeBlock = free.GetPtr();
				if ((uintptr_t)freeBlock == blockEndAddr)
				{
					const uint32 freeDataSize = freeBlock->dataSize;
					if (size > (dataSize + freeDataSize + sizeof(MBlock2_t)))
					{
						newSize = 0;
						break;
					}

					ExpMemBlockRegion region;
					_MEMExpHeap_GetRegionOfMBlock(&region, freeBlock);
					MBlock2_t* prevBlock = (MBlock2_t*)_MEMExpHeap_RemoveMBlock(&expHeap->expHeapHead.chainFreeBlocks, freeBlock);

					uintptr_t oldStart = region.start;
					region.start = (uintptr_t)memBlock + size;

					if (region.end - region.start < sizeof(MBlock2_t))
						region.start = region.end;

					mBlock->dataSize = (uint32)(region.start - (uintptr_t)memBlock);
					if (region.end - region.start >= sizeof(MBlock2_t))
					{
						MBlock2_t* newBlock = _MEMExpHeap_InitMBlock(&region, MBLOCK_TYPE_FREE);
						_MEMExpHeap_InsertMBlock(&expHeap->expHeapHead.chainFreeBlocks, newBlock, prevBlock);
					}

					if (HAS_FLAG(heap->flags, MEM_HEAP_OPTION_CLEAR))
						memset((void*)oldStart, 0x00, region.start - oldStart);
					else if (HAS_FLAG(heap->flags, MEM_HEAP_OPTION_FILL))
					{
						const uint32 fillValue = MEMGetFillValForHeap(HEAP_FILL_TYPE::ON_ALLOC);
						memset((void*)oldStart, fillValue, region.start - oldStart);
					}

					newSize = (uint32)mBlock->dataSize;

					break;
				}

				free = freeBlock->nextBlock;
			}

			if (!free)
				newSize = 0;
		}
	}

	heap->ReleaseLock();

	return newSize;
}

uint32 MEMGetTotalFreeSizeForExpHeap(MEMHeapHandle heap)
{
	uint32 freeSize = 0;

	IsValidExpHeapHandle_(heap);
	heap->AcquireLock();

	MEMExpHeapHead2* expHeap = (MEMExpHeapHead2*)heap;

	MEMPTR<MBlock2_t> block = expHeap->expHeapHead.chainFreeBlocks.headMBlock;
	while (block)
	{
		freeSize += (uint32)block->dataSize;
		block = block->nextBlock;
	}

	heap->ReleaseLock();

	return freeSize;
}

uint32 MEMGetAllocatableSizeForExpHeapEx(MEMHeapHandle heap, sint32 alignment)
{
	uint32 result = 0;

	IsValidExpHeapHandle_(heap);
	cemu_assert_debug((alignment & 3) == 0);
	alignment = abs(alignment);

	heap->AcquireLock();

	MEMExpHeapHead2* exp_heap = (MEMExpHeapHead2*)heap;
	const sint32 alignment_minus_one = alignment - 1;

	uint32 smallest_alignment_space = -1;
	for(auto free_block = exp_heap->expHeapHead.chainFreeBlocks.headMBlock; free_block; free_block = free_block->nextBlock)
	{
		MBlock2_t* block = free_block.GetPtr();
		const uintptr_t block_memory = MBLOCK_GET_MEMORY(block);
		const uintptr_t block_end = MBLOCK_GET_END(block);

		const uintptr_t aligned_memory = (block_memory + alignment_minus_one) & ~alignment_minus_one;
		if (aligned_memory >= block_end)
			continue;

		const uint32 size = (uint32)(block_end - aligned_memory);
		const uint32 alignment_space = (uint32)(aligned_memory - block_memory);
		if (size < result)
			continue;

		if (size == result)
		{
			if (smallest_alignment_space <= alignment_space)
				continue;
		}

		smallest_alignment_space = alignment_space;
		result = size;
	}

	heap->ReleaseLock();

	return result;
}

uint16 MEMSetGroupIDForExpHeap(MEMHeapHandle heap, uint16 groupId)
{
	IsValidExpHeapHandle_(heap);

	heap->AcquireLock();

	MEMExpHeapHead2* expHeap = (MEMExpHeapHead2*)heap;
	const uint16 oldGroupId = expHeap->expHeapHead.groupID;
	expHeap->expHeapHead.groupID = groupId;

	heap->ReleaseLock();

	return oldGroupId;
}

uint16 MEMGetGroupIDForExpHeap(MEMHeapHandle heap)
{
	IsValidExpHeapHandle_(heap);

	heap->AcquireLock();

	MEMExpHeapHead2* expHeap = (MEMExpHeapHead2*)heap;
	const uint16 oldGroupId = expHeap->expHeapHead.groupID;

	heap->ReleaseLock();

	return oldGroupId;
}

void MEMVisitAllocatedForExpHeap(MEMHeapHandle heap, const MEMPTR<void>& visitor, uint32 userParam)
{
	IsValidExpHeapHandle_(heap);
	cemu_assert_debug(visitor != nullptr);

	heap->AcquireLock();

	MEMPTR<MEMHeapBase> heapMPTR = heap;

	MEMExpHeapHead2* expHeap = (MEMExpHeapHead2*)heap;
	MEMPTR<MBlock2_t> find = expHeap->expHeapHead.chainUsedBlocks.headMBlock;
	while (find)
	{
		MBlock2_t* mBlock = find.GetPtr();

		MEMPTR<void> memMPTR = (void*)MBLOCK_GET_MEMORY(mBlock);
		PPCCoreCallback(visitor.GetMPTR(), memMPTR.GetMPTR(), heapMPTR.GetMPTR(), userParam);

		find = mBlock->nextBlock;
	}

	heap->ReleaseLock();
}

uint32 MEMGetSizeForMBlockExpHeap(const void* memBlock)
{
	cemu_assert_debug(_MEMExpHeap_IsValidUsedMBlock(memBlock, nullptr));
	MBlock2_t* mBlock = MBLOCK_GET_HEADER(memBlock);
	return mBlock->dataSize;
}

uint16 MEMGetGroupIDForMBlockExpHeap(const void* memBlock)
{
	cemu_assert_debug(_MEMExpHeap_IsValidUsedMBlock(memBlock, nullptr));
	MBlock2_t* mBlock = MBLOCK_GET_HEADER(memBlock);
	return (uint32)mBlock->fields & 0xFF;
}

uint16 MEMGetAllocDirForMBlockExpHeap(const void* memBlock)
{
	cemu_assert_debug(_MEMExpHeap_IsValidUsedMBlock(memBlock, nullptr));
	MBlock2_t* mBlock = MBLOCK_GET_HEADER(memBlock);
	const uint32 fields = mBlock->fields;
	return (fields >> 31) & 1;
}

bool MEMCheckExpHeap(MEMHeapHandle heap, uint32 options)
{
	if (heap == MEM_HEAP_INVALID_HANDLE || heap->magic != coreinit::MEMHeapMagic::EXP_HEAP)
		return false;

	heap->AcquireLock();

	// todo

	heap->ReleaseLock();

	return true;
}

bool MEMCheckForMBlockExpHeap(const void* memBlock, MEMHeapHandle heap, uint32 options)
{
	return true;
}

void _DefaultAllocatorForExpHeap_Alloc(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(allocator, MEMAllocator, 0);
	ppcDefineParamU32(size, 1);
	MEMPTR<void> result = MEMAllocFromExpHeapEx((MEMHeapHandle)allocator->heap.GetPtr(), size, (uint32)allocator->param1);
	osLib_returnFromFunction(hCPU, result.GetMPTR());
}

void _DefaultAllocatorForExpHeap_Free(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(allocator, MEMAllocator, 0);
	ppcDefineParamMEMPTR(mem, void, 1);
	MEMFreeToExpHeap((MEMHeapHandle)allocator->heap.GetPtr(), mem);
	osLib_returnFromFunction(hCPU, 0);
}

SysAllocator<MEMAllocatorFunc> gExpHeapDefaultAllocator;

void MEMInitAllocatorForExpHeap(MEMAllocator* allocator, MEMHeapHandle heap, sint32 alignment)
{
	allocator->func = gExpHeapDefaultAllocator.GetPtr();
	gExpHeapDefaultAllocator->funcAlloc = PPCInterpreter_makeCallableExportDepr(_DefaultAllocatorForExpHeap_Alloc);
	gExpHeapDefaultAllocator->funcFree = PPCInterpreter_makeCallableExportDepr(_DefaultAllocatorForExpHeap_Free);

	allocator->heap = heap;
	allocator->param1 = alignment;
	allocator->param2 = 0;
}
#pragma endregion

#pragma region wrapper

void expheap_test();

void export_MEMCreateExpHeapEx(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(startAddress, void, 0);
	ppcDefineParamU32(size, 1);
	ppcDefineParamU16(options, 2);
	MEMPTR<MEMHeapBase> heap = MEMCreateExpHeapEx(startAddress.GetPtr(), size, options);
	cemuLog_logDebug(LogType::CoreinitMem, "MEMCreateExpHeapEx(0x{:08x}, 0x{:x}, 0x{:x}) Result: 0x{:08x}", startAddress.GetMPTR(), size, options, heap.GetMPTR());
	osLib_returnFromFunction(hCPU, heap.GetMPTR());
}

void export_MEMDestroyExpHeap(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(heap, MEMHeapBase, 0);
	cemuLog_logDebug(LogType::CoreinitMem, "MEMDestroyExpHeap(0x{:08x})", heap.GetMPTR());
	MEMPTR<MEMHeapBase> oldHeap = (MEMHeapBase*)MEMDestroyExpHeap(heap.GetPtr());
	osLib_returnFromFunction(hCPU, oldHeap.GetMPTR());
}

void export_MEMAllocFromExpHeapEx(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(heap, MEMHeapBase, 0);
	ppcDefineParamU32(size, 1);
	ppcDefineParamS32(alignment, 2);
	MEMPTR<void> mem = MEMAllocFromExpHeapEx(heap.GetPtr(), size, alignment);
	cemuLog_logDebug(LogType::CoreinitMem, "MEMAllocFromExpHeapEx(0x{:08x}, 0x{:x}, {}) Result: 0x{:08x}", heap.GetMPTR(), size, alignment, mem.GetMPTR());
	osLib_returnFromFunction(hCPU, mem.GetMPTR());
}

void export_MEMFreeToExpHeap(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(heap, MEMHeapBase, 0);
	ppcDefineParamMEMPTR(mem, void, 1);
	cemuLog_logDebug(LogType::CoreinitMem, "MEMFreeToExpHeap(0x{:08x}, 0x{:08x})", heap.GetMPTR(), mem.GetMPTR());
	MEMFreeToExpHeap(heap.GetPtr(), mem.GetPtr());
	osLib_returnFromFunction(hCPU, 0);
}

void export_MEMSetAllocModeForExpHeap(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(heap, MEMHeapBase, 0);
	ppcDefineParamU16(mode, 1);
	cemuLog_logDebug(LogType::CoreinitMem, "MEMSetAllocModeForExpHeap(0x{:08x}, {})", heap.GetMPTR(), mode);
	uint16 oldMode = MEMSetAllocModeForExpHeap(heap.GetPtr(), mode);
	osLib_returnFromFunction(hCPU, oldMode);
}

void export_MEMGetAllocModeForExpHeap(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(heap, MEMHeapBase, 0);
	cemuLog_logDebug(LogType::CoreinitMem, "MEMGetAllocModeForExpHeap(0x{:08x})", heap.GetMPTR());
	uint16 oldMode = MEMGetAllocModeForExpHeap(heap.GetPtr());
	osLib_returnFromFunction(hCPU, oldMode);
}

void export_MEMAdjustExpHeap(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(heap, MEMHeapBase, 0);
	cemuLog_logDebug(LogType::CoreinitMem, "MEMAdjustExpHeap(0x{:08x})", heap.GetMPTR());
	uint32 newSize = MEMAdjustExpHeap(heap.GetPtr());
	osLib_returnFromFunction(hCPU, newSize);
}

void export_MEMResizeForMBlockExpHeap(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(heap, MEMHeapBase, 0);
	ppcDefineParamMEMPTR(mem, void, 1);
	ppcDefineParamU32(size, 2);
	uint32 newSize = MEMResizeForMBlockExpHeap(heap.GetPtr(), mem.GetPtr(), size);
	cemuLog_logDebug(LogType::CoreinitMem, "MEMResizeForMBlockExpHeap(0x{:08x}, 0x{:08x}, 0x{:x}) Result: 0x{:x}", heap.GetMPTR(), mem.GetMPTR(), size, newSize);
	osLib_returnFromFunction(hCPU, newSize);
}

void export_MEMGetTotalFreeSizeForExpHeap(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(heap, MEMHeapBase, 0);
	uint32 size = MEMGetTotalFreeSizeForExpHeap(heap.GetPtr());
	cemuLog_logDebug(LogType::CoreinitMem, "MEMGetTotalFreeSizeForExpHeap(0x{:08x}) Result: 0x{:x}", heap.GetMPTR(), size);
	osLib_returnFromFunction(hCPU, size);
}

void export_MEMGetAllocatableSizeForExpHeapEx(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(heap, MEMHeapBase, 0);
	ppcDefineParamS32(alignment, 1);
	uint32 size = MEMGetAllocatableSizeForExpHeapEx(heap.GetPtr(), alignment);
	cemuLog_logDebug(LogType::CoreinitMem, "MEMGetAllocatableSizeForExpHeapEx(0x{:08x}, 0x{:x}) Result: 0x{:x}", heap.GetMPTR(), alignment, size);
	osLib_returnFromFunction(hCPU, size);
}

void export_MEMSetGroupIDForExpHeap(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(heap, MEMHeapBase, 0);
	ppcDefineParamU16(groupId, 1);
	cemuLog_logDebug(LogType::CoreinitMem, "MEMSetGroupIDForExpHeap(0x{:08x}, {})", heap.GetMPTR(), groupId);
#ifdef CEMU_DEBUG_ASSERT
	assert_dbg(); // someone test this and the entire groupId feature
#endif
	uint16 oldGroupId = MEMSetGroupIDForExpHeap(heap.GetPtr(), groupId);
	osLib_returnFromFunction(hCPU, oldGroupId);
}

void export_MEMGetGroupIDForExpHeap(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(heap, MEMHeapBase, 0);
	cemuLog_logDebug(LogType::Force, "MEMGetGroupIDForExpHeap(0x{:08x})", heap.GetMPTR());
	uint16 oldGroupId = MEMGetGroupIDForExpHeap(heap.GetPtr());
	osLib_returnFromFunction(hCPU, oldGroupId);
}

void export_MEMVisitAllocatedForExpHeap(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(heap, MEMHeapBase, 0);
	ppcDefineParamMEMPTR(visitor, void, 1);
	ppcDefineParamU32(userParam, 2);
	cemuLog_logDebug(LogType::CoreinitMem, "MEMVisitAllocatedForExpHeap(0x{:08x}, 0x{:08x}, 0x{:x})", heap.GetMPTR(), visitor.GetMPTR(), userParam);
	MEMVisitAllocatedForExpHeap(heap.GetPtr(), visitor, userParam);
	osLib_returnFromFunction(hCPU, 0);
}

void export_MEMGetSizeForMBlockExpHeap(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(memBlock, void, 0);
	uint32 size = MEMGetSizeForMBlockExpHeap(memBlock.GetPtr());
	cemuLog_logDebug(LogType::CoreinitMem, "MEMGetSizeForMBlockExpHeap(0x{:08x}) Result: 0x{:x}", memBlock.GetMPTR(), size);
	osLib_returnFromFunction(hCPU, size);
}

void export_MEMGetGroupIDForMBlockExpHeap(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(memBlock, void, 0);
	cemuLog_logDebug(LogType::CoreinitMem, "MEMGetGroupIDForMBlockExpHeap(0x{:08x})", memBlock.GetMPTR());
	uint16 groupId = MEMGetGroupIDForMBlockExpHeap(memBlock.GetPtr());
	osLib_returnFromFunction(hCPU, groupId);
}

void export_MEMGetAllocDirForMBlockExpHeap(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(memBlock, void, 0);
	cemuLog_logDebug(LogType::CoreinitMem, "MEMGetAllocDirForMBlockExpHeap(0x{:08x})", memBlock.GetMPTR());
	uint16 allocDir = MEMGetAllocDirForMBlockExpHeap(memBlock.GetPtr());
	osLib_returnFromFunction(hCPU, allocDir);
}

void export_MEMCheckExpHeap(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(heap, MEMHeapBase, 0);
	ppcDefineParamU32(options, 1);
	cemuLog_logDebug(LogType::CoreinitMem, "MEMCheckExpHeap(0x{:08x}, 0x{:x})", heap.GetMPTR(), options);
	bool result = MEMCheckExpHeap(heap.GetPtr(), options);
	osLib_returnFromFunction(hCPU, result ? 1 : 0);
}

void export_MEMCheckForMBlockExpHeap(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(memBlock, void, 0);
	ppcDefineParamMEMPTR(heap, MEMHeapBase, 1);
	ppcDefineParamU32(options, 2);
	bool result = MEMCheckForMBlockExpHeap(memBlock.GetPtr(), heap.GetPtr(), options);
	cemuLog_logDebug(LogType::CoreinitMem, "MEMCheckForMBlockExpHeap(0x{:08x}, 0x{:08x}, 0x{:x}) Result: {}", memBlock.GetMPTR(), heap.GetMPTR(), options, result);
	osLib_returnFromFunction(hCPU, result ? 1 : 0);
}

void export_MEMInitAllocatorForExpHeap(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(allocator, MEMAllocator, 0);
	ppcDefineParamMEMPTR(heap, MEMHeapBase, 1);
	ppcDefineParamS32(alignment, 2);
	cemuLog_logDebug(LogType::CoreinitMem, "MEMInitAllocatorForExpHeap(0x{:08x}, 0x{:08x}, {})", allocator.GetMPTR(), heap.GetMPTR(), alignment);
	MEMInitAllocatorForExpHeap(allocator.GetPtr(), heap.GetPtr(), alignment);
	osLib_returnFromFunction(hCPU, 0);
}

void expheap_test()
{
	srand(1000);

	MEMHeapHandle heapHandle = MEMCreateExpHeapEx(memory_getPointerFromVirtualOffset(0x11000000), 0x10000000, 0);
	MEMExpHeapHead2* expHeap = (MEMExpHeapHead2*)heapHandle;
	sint32 idx = 0;

	for (MBlock2_t* findBlock = expHeap->expHeapHead.chainFreeBlocks.headMBlock.GetPtr(); findBlock != nullptr; findBlock = findBlock->nextBlock.GetPtr())
	{
		const uintptr_t blockMemory = MBLOCK_GET_MEMORY(findBlock);
		const uint32 dataSize = (uint32)findBlock->dataSize;

		debug_printf(">> freeBlock %04d addr %08x - %08x\n", idx, memory_getVirtualOffsetFromPointer((void*)blockMemory), memory_getVirtualOffsetFromPointer((void*)blockMemory) + dataSize);
		idx++;
	}

	void* allocTable[1024];
	sint32 allocCount = 0;

	printf("Run ExpHeap test...\n");
	for (sint32 t = 0; t < 2; t++)
	{
		// allocate a bunch of entries
		sint32 r = rand() % 100;
		//for (sint32 i = 0; i < r; i++)
		{
			if( allocCount >= 1024 )
				continue;

			uint32 size = (rand() % 1000) * 12;
			void* mem = MEMAllocFromExpHeapEx(heapHandle, size * 12, -0x2000);
			if (mem)
			{
				allocTable[allocCount] = mem;
				allocCount++;
				
			}
		}
	}
	// free everything that remains
	for (sint32 i = 0; i < allocCount; i++)
	{
		MEMFreeToExpHeap(heapHandle, allocTable[i]);
	}
	allocCount = 0;
	// debug print free blocks (only one should remain)
	expHeap = (MEMExpHeapHead2*)heapHandle;
	idx = 0;
	for (MBlock2_t* findBlock = expHeap->expHeapHead.chainFreeBlocks.headMBlock.GetPtr(); findBlock != nullptr; findBlock = findBlock->nextBlock.GetPtr())
	{
		const uintptr_t blockMemory = MBLOCK_GET_MEMORY(findBlock);
		const uint32 dataSize = (uint32)findBlock->dataSize;

		debug_printf("freeBlock %04d addr %08x - %08x\n", idx, memory_getVirtualOffsetFromPointer((void*)blockMemory), memory_getVirtualOffsetFromPointer((void*)blockMemory) + dataSize);
		idx++;
	}

	assert_dbg();
}

void expheap_load()
{
	osLib_addFunction("coreinit", "MEMCreateExpHeapEx", export_MEMCreateExpHeapEx);
	osLib_addFunction("coreinit", "MEMDestroyExpHeap", export_MEMDestroyExpHeap);
	osLib_addFunction("coreinit", "MEMAllocFromExpHeapEx", export_MEMAllocFromExpHeapEx);
	osLib_addFunction("coreinit", "MEMFreeToExpHeap", export_MEMFreeToExpHeap);
	osLib_addFunction("coreinit", "MEMSetAllocModeForExpHeap", export_MEMSetAllocModeForExpHeap);
	osLib_addFunction("coreinit", "MEMGetAllocModeForExpHeap", export_MEMGetAllocModeForExpHeap);
	osLib_addFunction("coreinit", "MEMAdjustExpHeap", export_MEMAdjustExpHeap);
	osLib_addFunction("coreinit", "MEMResizeForMBlockExpHeap", export_MEMResizeForMBlockExpHeap);
	osLib_addFunction("coreinit", "MEMGetTotalFreeSizeForExpHeap", export_MEMGetTotalFreeSizeForExpHeap);
	osLib_addFunction("coreinit", "MEMGetAllocatableSizeForExpHeapEx", export_MEMGetAllocatableSizeForExpHeapEx);
	osLib_addFunction("coreinit", "MEMSetGroupIDForExpHeap", export_MEMSetGroupIDForExpHeap);
	osLib_addFunction("coreinit", "MEMGetGroupIDForExpHeap", export_MEMGetGroupIDForExpHeap);
	osLib_addFunction("coreinit", "MEMVisitAllocatedForExpHeap", export_MEMVisitAllocatedForExpHeap);
	osLib_addFunction("coreinit", "MEMGetSizeForMBlockExpHeap", export_MEMGetSizeForMBlockExpHeap);
	osLib_addFunction("coreinit", "MEMGetGroupIDForMBlockExpHeap", export_MEMGetGroupIDForMBlockExpHeap);
	osLib_addFunction("coreinit", "MEMGetAllocDirForMBlockExpHeap", export_MEMGetAllocDirForMBlockExpHeap);
	osLib_addFunction("coreinit", "MEMCheckExpHeap", export_MEMCheckExpHeap);
	osLib_addFunction("coreinit", "MEMCheckForMBlockExpHeap", export_MEMCheckForMBlockExpHeap);

	osLib_addFunction("coreinit", "MEMInitAllocatorForExpHeap", export_MEMInitAllocatorForExpHeap);
}
#pragma endregion
}

