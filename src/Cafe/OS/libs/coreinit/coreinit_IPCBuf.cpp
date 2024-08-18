#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"
#include "coreinit_IPCBuf.h"

namespace coreinit
{
	void FIFOInit(IPCFifo_t* fifo, uint32 entryCount, void* entryArray)
	{
		fifo->entryCount = entryCount;
		fifo->entryArray = (FIFOEntry_t*)entryArray;
		fifo->writeIndex = 0;
		fifo->readIndex = -1;
		fifo->availableEntries = 0;
	}

	sint32 FIFOPush(IPCFifo_t* fifo, void* entry)
	{
		if (fifo->readIndex == fifo->writeIndex)
		{
			assert_dbg();
			return -8;
		}
		fifo->entryArray[(uint32)fifo->writeIndex].p = (uint8*)entry;
		if ((sint32)fifo->readIndex < 0)
		{
			// set readIndex to valid value when fifo was empty
			fifo->readIndex = fifo->writeIndex;
		}
		fifo->availableEntries = (uint32)fifo->availableEntries + 1;
		fifo->writeIndex = ((uint32)fifo->writeIndex + 1) % (uint32)fifo->entryCount;
		return 0;
	}

	sint32 FIFOPop(IPCFifo_t* fifo, uint8** entry)
	{
		*entry = nullptr;
		if ((sint32)fifo->readIndex < 0)
			return -7;
		fifo->availableEntries = (uint32)fifo->availableEntries - 1;
		*entry = fifo->entryArray[(uint32)fifo->readIndex].p.GetPtr();
		fifo->readIndex = ((uint32)fifo->readIndex + 1) % (uint32)fifo->entryCount;

		if (fifo->availableEntries == (uint32be)0)
		{
			fifo->readIndex = -1;
		}
		return 0;
	}

	void* getUpwardsAlignedAddr(void* addr, uint32 alignment)
	{
		uint64 v = ((uint64)addr + alignment - 1) & ~(uint64)(alignment - 1);
		return (uint8*)v;
	}

	void* getDownwardsAlignedAddr(void* addr, uint32 alignment)
	{
		uint64 v = ((uint64)addr) & ~(uint64)(alignment - 1);
		return (uint8*)v;
	}

	IPCBufPool_t* IPCBufPoolCreate(uint8* bufferArea, uint32 bufferSize, uint32 entrySize, uint32be* entryCountOutput, uint32 uknR7)
	{
		memset(bufferArea, 0, bufferSize);
		IPCBufPool_t* ipcBufPool = (IPCBufPool_t*)getUpwardsAlignedAddr(bufferArea, 4);
		uint8* alignedEnd = (uint8*)getDownwardsAlignedAddr(bufferArea + bufferSize, 4);
		uint8* dataStart = (uint8*)getUpwardsAlignedAddr(ipcBufPool + 1, 0x4);

		*entryCountOutput = 0;
		// todo: Validate parameters

		OSInitMutexEx(&ipcBufPool->mutex, NULL);

		ipcBufPool->fullBufferPtr = bufferArea;
		ipcBufPool->fullBufferSize = bufferSize;
		ipcBufPool->uknFromParamR7 = uknR7;

		uint32 paddedEntrySize = (entrySize + 0x3F) & ~0x3F;

		ipcBufPool->ukn10 = 0;
		ipcBufPool->magic = 0xBADF00D;

		ipcBufPool->entrySize1 = paddedEntrySize;
		ipcBufPool->entrySize2 = paddedEntrySize;

		uint32 remainingSize = (uint32)((bufferArea + bufferSize) - dataStart);
		uint32 entryCount = remainingSize / paddedEntrySize;
		if (entryCount <= 1)
			assert_dbg();

		ipcBufPool->entryCountMul4 = entryCount * 4;
		if ((uint32)ipcBufPool->entryCountMul4 >= paddedEntrySize)
		{
			// special handling required (need to adjust entry count?)
			assert_dbg();
		}
		else
		{
			entryCount--; // remove one entry to make room for entry pointer array
			ipcBufPool->entryCount = entryCount;
			*entryCountOutput = entryCount;
			ipcBufPool->entryStartPtr = (dataStart + (uint32)ipcBufPool->entryCountMul4);
			FIFOInit(&ipcBufPool->fifo, (uint32)ipcBufPool->entryCount, dataStart);
		}
		// add all entries to the fifo
		for (sint32 i = 0; i < (sint32)ipcBufPool->entryCount; i++)
		{
			uint8* entry = ipcBufPool->entryStartPtr.GetPtr() + i * (uint32)ipcBufPool->entrySize2;
			if (FIFOPush(&ipcBufPool->fifo, entry) != 0)
				return nullptr;
		}
		return ipcBufPool;
	}

	uint8* IPCBufPoolAllocate(IPCBufPool_t* ipcBufPool, uint32 size)
	{
		uint8* entry = nullptr;
		OSLockMutex(&ipcBufPool->mutex);
		if (ipcBufPool->magic == (uint32be)0xBADF00D && size <= (uint32)ipcBufPool->entrySize1)
		{
			FIFOPop(&ipcBufPool->fifo, &entry);
		}
		else
		{
			assert_dbg();
		}
		OSUnlockMutex(&ipcBufPool->mutex);
		return entry;
	}

	sint32 IPCBufPoolFree(IPCBufPool_t* ipcBufPool, uint8* entry)
	{
		OSLockMutex(&ipcBufPool->mutex);
		sint32 res = 0;
		if (ipcBufPool->magic == (uint32be)0xBADF00D)
		{
			// check if entry is actually part of the pool
			uint32 offset = (uint32)(entry - (uint8*)ipcBufPool->entryStartPtr.GetPtr());
			if ((offset % (uint32)ipcBufPool->entrySize2) != 0)
				assert_dbg();
			uint32 index = offset / (uint32)ipcBufPool->entrySize2;
			if ((index >= (uint32)ipcBufPool->entryCount))
				assert_dbg();
			FIFOPush(&ipcBufPool->fifo, entry);
		}
		else
		{
			assert_dbg();
			res = -4;
		}
		OSUnlockMutex(&ipcBufPool->mutex);
		return res;
	}

	void coreinitExport_IPCBufPoolCreate(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamTypePtr(bufferArea, uint8, 0);
		ppcDefineParamU32(bufferSize, 1);
		ppcDefineParamU32(entrySize, 2);
		ppcDefineParamU32BEPtr(entryCountOutput, 3);
		ppcDefineParamU32(uknR7, 4);
		IPCBufPool_t* ipcBufPool = IPCBufPoolCreate(bufferArea, bufferSize, entrySize, entryCountOutput, uknR7);
		osLib_returnFromFunction(hCPU, memory_getVirtualOffsetFromPointer(ipcBufPool));
		return;
		// example dump of IPC buffer (at 0x1011FF40):
		// 0000   0B AD F0 0D 10 11 FF 40  00 00 53 01 00 00 00 01  00 00 00 00 00 00 02 C0  00 00 02 C0 00 00 00 1D
		// 0020   10 12 00 40 00 00 00 78  00 00 00 00 00 00 00 00  00 00 00 1D 00 00 00 1D  10 11 FF A8 6D 55 74 58
		// 0040   10 00 87 2C 00 00 00 00  00 00 00 00 00 00 00 00  10 11 FF 7C 00 00 00 00  00 00 00 00 00 00 00 00
		// 0060   00 00 00 00 00 00 00 00  10 12 00 40 10 12 03 00  10 12 05 C0 10 12 08 80  10 12 0B 40 10 12 0E 00
		// 0080   10 12 10 C0 10 12 13 80  10 12 16 40 10 12 19 00  10 12 1B C0 10 12 1E 80  10 12 21 40 10 12 24 00
		// 00A0   10 12 26 C0 10 12 29 80  10 12 2C 40 10 12 2F 00  10 12 31 C0 10 12 34 80  10 12 37 40 10 12 3A 00
		// 00C0   10 12 3C C0 10 12 3F 80  10 12 42 40 10 12 45 00  10 12 47 C0 10 12 4A 80  10 12 4D 40 00 00 00 00
		// 00E0   00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
	}

	void coreinitExport_IPCBufPoolAllocate(PPCInterpreter_t* hCPU)
	{
		debug_printf("IPCBufPoolAllocate(0x%08x,0x%x)\n", hCPU->gpr[3], hCPU->gpr[4]);
		ppcDefineParamStructPtr(ipcBufPool, IPCBufPool_t, 0);
		ppcDefineParamU32(size, 1);
		uint8* entry = IPCBufPoolAllocate(ipcBufPool, size);
		osLib_returnFromFunction(hCPU, memory_getVirtualOffsetFromPointer(entry));
	}

	void coreinitExport_IPCBufPoolFree(PPCInterpreter_t* hCPU)
	{
		debug_printf("IPCBufPoolFree(0x%08x,0x%08x)\n", hCPU->gpr[3], hCPU->gpr[4]);
		ppcDefineParamStructPtr(ipcBufPool, IPCBufPool_t, 0);
		ppcDefineParamTypePtr(entry, uint8, 1);
		sint32 res = IPCBufPoolFree(ipcBufPool, entry);
		osLib_returnFromFunction(hCPU, res);
	}

	void InitializeIPCBuf()
	{
		osLib_addFunction("coreinit", "IPCBufPoolCreate", coreinitExport_IPCBufPoolCreate);
		osLib_addFunction("coreinit", "IPCBufPoolAllocate", coreinitExport_IPCBufPoolAllocate);
		osLib_addFunction("coreinit", "IPCBufPoolFree", coreinitExport_IPCBufPoolFree);
	}
}