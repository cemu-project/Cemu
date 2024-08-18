#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/coreinit/coreinit.h"
#include "Cafe/HW/Latte/Core/LatteBufferCache.h"

/*
	Locked cache is mapped to the following memory regions:
			offset		size
	core0:	0xFFC00000	0x4000
	core1:	0xFFC40000	0x4000
	core2:	0xFFC80000	0x4000
*/

#define LC_LOCKED_CACHE_GRANULARITY		(0x200)	 // 512B
#define LC_LOCKED_CACHE_SIZE			(0x4000) // 16KB

#define LC_MASK_FREE			(0)
#define LC_MASK_RESERVED		(1)
#define LC_MASK_RESERVED_END	(2) // indicates end of reserved block

namespace coreinit
{
	uint8   lcCacheMask[PPC_CORE_COUNT][(LC_LOCKED_CACHE_SIZE + LC_LOCKED_CACHE_GRANULARITY - 1) / LC_LOCKED_CACHE_GRANULARITY] = { 0 };
	uint32  lcAllocatedBlocks[PPC_CORE_COUNT] = { 0 };

	MPTR lcAddr[] =
	{
		0xFFC00000, // core 0
		0xFFC40000, // core 1
		0xFFC80000  // core 2
	};

	void coreinitExport_LCGetMaxSize(PPCInterpreter_t* hCPU)
	{
		osLib_returnFromFunction(hCPU, LC_LOCKED_CACHE_SIZE);
	}

	void coreinitExport_LCAlloc(PPCInterpreter_t* hCPU)
	{
		//debug_printf("LCAlloc(0x%04x) Thread %08x Core %d", hCPU->gpr[3], coreinitThread_getCurrentThread(hCPU), PPCInterpreter_getCoreIndex(hCPU));
		uint32 size = hCPU->gpr[3];
		if (size == 0 || size > LC_LOCKED_CACHE_SIZE)
		{
			debug_printf("LCAlloc: Invalid alloc size %d\n", size);
			osLib_returnFromFunction(hCPU, MPTR_NULL);
			return;
		}
		if ((size % LC_LOCKED_CACHE_GRANULARITY) != 0)
		{
			debug_printf("LCAlloc: Unaligned alloc size 0x%04x\n", size);
			size += (LC_LOCKED_CACHE_GRANULARITY - 1);
			size &= ~(LC_LOCKED_CACHE_GRANULARITY - 1);
		}
		uint32 coreIndex = PPCInterpreter_getCoreIndex(hCPU);
		uint8* cacheMask = lcCacheMask[coreIndex];
		uint32 entryMaskLength = size / LC_LOCKED_CACHE_GRANULARITY;
		for (uint32 i = 0; i <= (LC_LOCKED_CACHE_SIZE / LC_LOCKED_CACHE_GRANULARITY) - entryMaskLength; i++)
		{
			// check if range starting at i is free
			bool rangeIsFree = true;
			for (uint32 f = 0; f < entryMaskLength; f++)
			{
				if (cacheMask[i + f] != LC_MASK_FREE)
				{
					rangeIsFree = false;
					break;
				}
			}
			if (rangeIsFree)
			{
				// found space for allocation
				MPTR allocAddr = lcAddr[coreIndex] + i * LC_LOCKED_CACHE_GRANULARITY;
				// mark range as allocated
				for (uint32 f = 0; f < entryMaskLength - 1; f++)
				{
					cacheMask[i + f] = LC_MASK_RESERVED;
				}
				cacheMask[i + entryMaskLength - 1] = LC_MASK_RESERVED_END;
				// update allocation counter
				lcAllocatedBlocks[coreIndex] += entryMaskLength;
				// return allocAddr
				//debug_printf("LCAlloc result %08x Thread %08x Core %d", (uint32)allocAddr, coreinitThread_getCurrentThread(hCPU), PPCInterpreter_getCoreIndex(hCPU));
				osLib_returnFromFunction(hCPU, allocAddr);
				return;
			}
		}
		// not enough space left	
		//debug_printf("LCAlloc failed Thread %08x Core %d", coreinitThread_getCurrentThread(hCPU), PPCInterpreter_getCoreIndex(hCPU));
		osLib_returnFromFunction(hCPU, MPTR_NULL);
	}


	void coreinitExport_LCDealloc(PPCInterpreter_t* hCPU)
	{
		uint32 coreIndex = PPCInterpreter_getCoreIndex(hCPU);
		uint8* cacheMask = lcCacheMask[coreIndex];
		//printf("LCDealloc(0x%08x)\n", hCPU->gpr[3]);

		MPTR deallocAddr = hCPU->gpr[3];
		if (deallocAddr < lcAddr[coreIndex] || deallocAddr >= (lcAddr[coreIndex] + LC_LOCKED_CACHE_SIZE))
		{
			// out of bounds
#ifdef CEMU_DEBUG_ASSERT
			cemuLog_log(LogType::Force, "LCDealloc(): Out of bounds");
#endif
			osLib_returnFromFunction(hCPU, 0);
			return;
		}
		uint32 relativeOffset = (uint32)deallocAddr - lcAddr[coreIndex];
		if ((relativeOffset % LC_LOCKED_CACHE_GRANULARITY) != 0)
		{
			debug_printf("LCDealloc: Offset alignment is invalid (0x%08x / 0x%08x)\n", deallocAddr, relativeOffset);
			osLib_returnFromFunction(hCPU, 0);
			return;
		}
		uint32 startIndex = relativeOffset / LC_LOCKED_CACHE_GRANULARITY;
		// check this is really the beginning of an allocated block
		if (startIndex > 0 && (cacheMask[startIndex - 1] != LC_MASK_RESERVED_END && cacheMask[startIndex - 1] != LC_MASK_FREE))
		{
			debug_printf("LCDealloc: Offset is invalid (0x%08x / 0x%08x)\n", deallocAddr, relativeOffset);
			osLib_returnFromFunction(hCPU, 0);
			return;
		}
		// free range by reseting mask in allocated region
		for (uint32 i = startIndex; i < (LC_LOCKED_CACHE_SIZE / LC_LOCKED_CACHE_GRANULARITY); i++)
		{
			if (cacheMask[i] == LC_MASK_RESERVED_END)
			{
				// end of allocated block reached
				cacheMask[i] = LC_MASK_FREE;
				// update allocation counter
				lcAllocatedBlocks[coreIndex]--;
				break;
			}
			else if (cacheMask[i] == LC_MASK_FREE)
			{
				debug_printf("LCDealloc: Allocation mask error detected\n");
				break;
			}
			cacheMask[i] = LC_MASK_FREE;
			// update allocation counter
			lcAllocatedBlocks[coreIndex]--;
		}
		osLib_returnFromFunction(hCPU, 0);
	}

	void coreinitExport_LCGetUnallocated(PPCInterpreter_t* hCPU)
	{
		uint32 unallocatedBytes = 0;
		uint32 coreIndex = PPCInterpreter_getCoreIndex(hCPU);
		unallocatedBytes = LC_LOCKED_CACHE_SIZE - (lcAllocatedBlocks[coreIndex] * LC_LOCKED_CACHE_GRANULARITY);
		//debug_printf("LCGetUnallocated() Result: 0x%x\n", unallocatedBytes);
		osLib_returnFromFunction(hCPU, unallocatedBytes);
	}

	void coreinitExport_LCGetAllocatableSize(PPCInterpreter_t* hCPU)
	{
		debug_printf("LCGetAllocatableSize()\n");
		// no parameters, returns largest allocatable block size
		// get core LC parameters
		uint32 coreIndex = PPCInterpreter_getCoreIndex(hCPU);
		uint8* cacheMask = lcCacheMask[coreIndex];
		// scan for largest range of available blocks
		uint32 largestFreeRange = 0;
		uint32 currentRangeSize = 0;
		for (uint32 i = 0; i < LC_LOCKED_CACHE_SIZE / LC_LOCKED_CACHE_GRANULARITY; i++)
		{
			if (cacheMask[i] == LC_MASK_FREE)
			{
				currentRangeSize++;
			}
			else
			{
				largestFreeRange = std::max(largestFreeRange, currentRangeSize);
				currentRangeSize = 0;
			}
		}
		largestFreeRange = std::max(largestFreeRange, currentRangeSize);
		osLib_returnFromFunction(hCPU, largestFreeRange * LC_LOCKED_CACHE_GRANULARITY);
	}

	uint32 LCIsEnabled[PPC_CORE_COUNT] = { 0 };

	void coreinitExport_LCEnableDMA(PPCInterpreter_t* hCPU)
	{
		//debug_printf("LCEnableDMA()\n");

		LCIsEnabled[PPCInterpreter_getCoreIndex(hCPU)]++;
		osLib_returnFromFunction(hCPU, 1);
	}

	sint32 _lcDisableErrorCounter = 0;

	void coreinitExport_LCDisableDMA(PPCInterpreter_t* hCPU)
	{
		//debug_printf("LCDisableDMA()\n");
#ifndef PUBLIC_RELASE
		if (LCIsEnabled[PPCInterpreter_getCoreIndex(hCPU)] == 0)
			assert_dbg();
#endif
		LCIsEnabled[PPCInterpreter_getCoreIndex(hCPU)]--;
#ifdef CEMU_DEBUG_ASSERT
		if (LCIsEnabled[PPCInterpreter_getCoreIndex(hCPU)] == 0)
		{
			uint32 coreIndex = PPCInterpreter_getCoreIndex(hCPU);
			uint8* cacheMask = lcCacheMask[coreIndex];
			for (uint32 i = 0; i <= (LC_LOCKED_CACHE_SIZE / LC_LOCKED_CACHE_GRANULARITY); i++)
			{
				if (cacheMask[i] != LC_MASK_FREE)
				{
					if (_lcDisableErrorCounter < 15)
					{
						cemuLog_log(LogType::Force, "LC disabled but there is still memory allocated");
						_lcDisableErrorCounter++;
					}
				}
			}
		}
#endif

		osLib_returnFromFunction(hCPU, 1);
	}

	void coreinitExport_LCIsDMAEnabled(PPCInterpreter_t* hCPU)
	{
		osLib_returnFromFunction(hCPU, LCIsEnabled[PPCInterpreter_getCoreIndex(hCPU)] ? 1 : 0);
	}

	void coreinitExport_LCHardwareIsAvailable(PPCInterpreter_t* hCPU)
	{
		// on the real HW, LC is shared between processes and not all processes can access it. In the emulator we don't care and always return true.
		osLib_returnFromFunction(hCPU, 1);
	}

	void coreinitExport_LCLoadDMABlocks(PPCInterpreter_t* hCPU)
	{
		//printf("LCLoadDMABlocks(0x%08x, 0x%08x, 0x%08x)\n", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5]);
		uint32 numBlocks = hCPU->gpr[5];
		if (numBlocks == 0)
			numBlocks = 128;
		uint32 transferSize = numBlocks * 32;
		uint8* destPtr = memory_getPointerFromVirtualOffset(hCPU->gpr[3]);
		uint8* srcPtr = memory_getPointerFromVirtualOffset(hCPU->gpr[4]);
		// copy right away, we don't emulate the DMAQueue currently
		memcpy(destPtr, srcPtr, transferSize);

		osLib_returnFromFunction(hCPU, 0);
	}

	void coreinitExport_LCStoreDMABlocks(PPCInterpreter_t* hCPU)
	{
		//printf("LCStoreDMABlocks(0x%08x, 0x%08x, 0x%08x)\n", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5]);
		uint32 numBlocks = hCPU->gpr[5];
		if (numBlocks == 0)
			numBlocks = 128;
		//uint32 transferSize = numBlocks*32;
		uint8* destPtr = memory_getPointerFromVirtualOffset(hCPU->gpr[3]);
		uint8* srcPtr = memory_getPointerFromVirtualOffset(hCPU->gpr[4]);
		// copy right away, we don't emulate the DMAQueue currently
		memcpy_qwords(destPtr, srcPtr, numBlocks * (32 / sizeof(uint64)));

		LatteBufferCache_notifyDCFlush(hCPU->gpr[3], numBlocks * 32);

		osLib_returnFromFunction(hCPU, 0);
	}

	void coreinitExport_LCWaitDMAQueue(PPCInterpreter_t* hCPU)
	{
		//printf("LCWaitDMAQueue(%d)\n", hCPU->gpr[3]);
		uint32 len = hCPU->gpr[3];
		// todo: Implement (be aware DMA queue is per core)
		osLib_returnFromFunction(hCPU, 0);
	}

	void InitializeLC()
	{
		for (sint32 f = 0; f < PPC_CORE_COUNT; f++)
		{
			memset(lcCacheMask[f], LC_MASK_FREE, (LC_LOCKED_CACHE_SIZE + LC_LOCKED_CACHE_GRANULARITY - 1) / LC_LOCKED_CACHE_GRANULARITY);
		}

		osLib_addFunction("coreinit", "LCGetMaxSize", coreinitExport_LCGetMaxSize);

		osLib_addFunction("coreinit", "LCAlloc", coreinitExport_LCAlloc);
		osLib_addFunction("coreinit", "LCDealloc", coreinitExport_LCDealloc);
		osLib_addFunction("coreinit", "LCGetUnallocated", coreinitExport_LCGetUnallocated);
		osLib_addFunction("coreinit", "LCGetAllocatableSize", coreinitExport_LCGetAllocatableSize);

		osLib_addFunction("coreinit", "LCEnableDMA", coreinitExport_LCEnableDMA);
		osLib_addFunction("coreinit", "LCDisableDMA", coreinitExport_LCDisableDMA);
		osLib_addFunction("coreinit", "LCIsDMAEnabled", coreinitExport_LCIsDMAEnabled);
		osLib_addFunction("coreinit", "LCHardwareIsAvailable", coreinitExport_LCHardwareIsAvailable);

		osLib_addFunction("coreinit", "LCLoadDMABlocks", coreinitExport_LCLoadDMABlocks);
		osLib_addFunction("coreinit", "LCStoreDMABlocks", coreinitExport_LCStoreDMABlocks);
		osLib_addFunction("coreinit", "LCWaitDMAQueue", coreinitExport_LCWaitDMAQueue);

	}

}
