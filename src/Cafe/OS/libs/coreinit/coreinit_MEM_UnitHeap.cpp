#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/coreinit/coreinit_MEM.h"
#include "Cafe/OS/libs/coreinit/coreinit_MEM_UnitHeap.h"
#include "Cafe/OS/libs/coreinit/coreinit_Spinlock.h"

namespace coreinit
{
	void _MEMUnitHeap_IsValidHeap(MEMHeapHandle heap)
	{
		cemu_assert(heap != MEM_HEAP_INVALID_HANDLE);
		cemu_assert(heap->magic == MEMHeapMagic::UNIT_HEAP);
	}

	MEMHeapBase* MEMCreateUnitHeapEx(void* memStart, uint32 heapSize, uint32 blockSize, uint32 alignment, uint32 createFlags)
	{
		cemu_assert_debug(memStart != nullptr);
		cemu_assert_debug(alignment % MIN_ALIGNMENT == 0);
		cemu_assert_debug(MIN_ALIGNMENT <= alignment && alignment <= 32);

		uintptr_t startAddr = (uintptr_t)memStart;
		uintptr_t endAddr = startAddr + heapSize;

		startAddr = (startAddr + MIN_ALIGNMENT_MINUS_ONE) & (~MIN_ALIGNMENT_MINUS_ONE);
		endAddr &= (~MIN_ALIGNMENT_MINUS_ONE);

		if (startAddr > endAddr)
			return nullptr;

		const uint32 alignmentMinusOne = alignment - 1;

		MEMUnitHeap* unitHeap = (MEMUnitHeap*)startAddr;
		uintptr_t alignedStart = startAddr + sizeof(MEMUnitHeap) + alignmentMinusOne & ~((uintptr_t)alignmentMinusOne);
		if (alignedStart > endAddr)
			return nullptr;

		blockSize = blockSize + alignmentMinusOne & ~alignmentMinusOne;
		uint32 totalBlockSize = (uint32)(endAddr - alignedStart);
		uint32 numBlocks = totalBlockSize / blockSize;
		if (numBlocks == 0)
			return nullptr;

		MEMInitHeapBase(unitHeap, MEMHeapMagic::UNIT_HEAP, (void*)alignedStart, (void*)(alignedStart + (numBlocks * blockSize)), createFlags);

		unitHeap->firstFreeBlock = (MEMUnitHeapBlock*)alignedStart;
		unitHeap->blockSize = blockSize;

		MEMUnitHeapBlock* currentBlock = (MEMUnitHeapBlock*)alignedStart;
		for (uint32 i = 0; i < numBlocks - 1; ++i)
		{
			MEMUnitHeapBlock* nextBlock = (MEMUnitHeapBlock*)((uintptr_t)currentBlock + blockSize);
			currentBlock->nextBlock = nextBlock;
			currentBlock = nextBlock;
		}

		currentBlock->nextBlock = nullptr;

		if ((MEMHeapHandle*)startAddr != nullptr)
		{
			MEMHeapTable_Add((MEMHeapHandle)startAddr);
		}

		return (MEMHeapBase*)startAddr;
	}

	void* MEMDestroyUnitHeap(MEMHeapHandle heap)
	{
		_MEMUnitHeap_IsValidHeap(heap);
		MEMBaseDestroyHeap(heap);
		MEMHeapTable_Remove(heap);
		return heap;
	}

	uint32 MEMCalcHeapSizeForUnitHeap(uint32 blockSize, uint32 blockCount, uint32 alignment)
	{
		uint32 alignedBlockSize = (blockSize + (alignment - 1)) & ~(alignment - 1);
		uint32 blockTotalSize = blockCount * alignedBlockSize;
		uint32 heapSize = blockTotalSize + (alignment - 4) + sizeof(MEMUnitHeap);
		return heapSize;
	}

	uint32 MEMCountFreeBlockForUnitHeap(coreinit::MEMUnitHeap* heap)
	{
		_MEMUnitHeap_IsValidHeap(heap);
		heap->AcquireLock();
		MEMUnitHeapBlock* currentBlock = heap->firstFreeBlock;
		uint32 blockCount = 0;
		while (currentBlock)
		{
			blockCount++;
			currentBlock = currentBlock->nextBlock;
		}
		heap->ReleaseLock();
		return blockCount;
	}

	void* MEMAllocFromUnitHeap(MEMUnitHeap* heap)
	{
		_MEMUnitHeap_IsValidHeap(heap);
		heap->AcquireLock();
		MEMUnitHeapBlock* currentBlock = heap->firstFreeBlock;
		if (!currentBlock)
		{
			heap->ReleaseLock();
			return nullptr;
		}
		// remove from list of free blocks
		heap->firstFreeBlock = currentBlock->nextBlock;
		// fill block
		if (heap->HasOptionClear())
		{
			memset(currentBlock, 0, heap->blockSize);
		}
		else if (heap->HasOptionFill())
		{
			memset(currentBlock, coreinit::MEMGetFillValForHeap(coreinit::HEAP_FILL_TYPE::ON_ALLOC), heap->blockSize);
		}
		heap->ReleaseLock();
		return currentBlock;
	}

	void MEMFreeToUnitHeap(MEMUnitHeap* heap, void* mem)
	{
		_MEMUnitHeap_IsValidHeap(heap);
		if (!mem)
			return;
		heap->AcquireLock();
		cemu_assert_debug(mem >= heap->heapStart.GetPtr() && mem < heap->heapEnd.GetPtr());
		// add to list of free blocks
		MEMUnitHeapBlock* releasedBlock = (MEMUnitHeapBlock*)mem;
		releasedBlock->nextBlock = heap->firstFreeBlock;
		heap->firstFreeBlock = releasedBlock;
		if (heap->HasOptionFill())
			memset(releasedBlock, coreinit::MEMGetFillValForHeap(coreinit::HEAP_FILL_TYPE::ON_FREE), heap->blockSize);
		heap->ReleaseLock();
	}

	void InitializeMEMUnitHeap()
	{
		cafeExportRegister("coreinit", MEMCreateUnitHeapEx, LogType::CoreinitMem);
		cafeExportRegister("coreinit", MEMDestroyUnitHeap, LogType::CoreinitMem);
		cafeExportRegister("coreinit", MEMCalcHeapSizeForUnitHeap, LogType::CoreinitMem);
		cafeExportRegister("coreinit", MEMCountFreeBlockForUnitHeap, LogType::CoreinitMem);
		cafeExportRegister("coreinit", MEMAllocFromUnitHeap, LogType::CoreinitMem);
		cafeExportRegister("coreinit", MEMFreeToUnitHeap, LogType::CoreinitMem);
	}
}
