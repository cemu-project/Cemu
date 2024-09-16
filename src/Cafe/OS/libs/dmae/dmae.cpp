#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/coreinit/coreinit_Time.h"
#include "Cafe/HW/Latte/Core/LatteBufferCache.h"

#define DMAE_ENDIAN_NONE	0
#define DMAE_ENDIAN_16		1
#define DMAE_ENDIAN_32		2
#define DMAE_ENDIAN_64		3

uint64 dmaeRetiredTimestamp = 0;

uint64 dmae_getTimestamp()
{
	return coreinit::coreinit_getTimerTick();
}

void dmae_setRetiredTimestamp(uint64 timestamp)
{
	dmaeRetiredTimestamp = timestamp;
}

void dmaeExport_DMAECopyMem(PPCInterpreter_t* hCPU)
{
	if( hCPU->gpr[6] == DMAE_ENDIAN_NONE )
	{
		// don't change endianness
		memcpy(memory_getPointerFromVirtualOffset(hCPU->gpr[3]), memory_getPointerFromVirtualOffset(hCPU->gpr[4]), hCPU->gpr[5]*4);
	}
	else if( hCPU->gpr[6] == DMAE_ENDIAN_32 )
	{
		// swap per uint32
		uint32* srcBuffer = (uint32*)memory_getPointerFromVirtualOffset(hCPU->gpr[4]);
		uint32* dstBuffer = (uint32*)memory_getPointerFromVirtualOffset(hCPU->gpr[3]);
		for(uint32 i=0; i<hCPU->gpr[5]; i++)
		{
			dstBuffer[i] = _swapEndianU32(srcBuffer[i]);
		}
	}
	else
	{
		cemuLog_logDebug(LogType::Force, "DMAECopyMem(): Unsupported endian swap\n");
	}
	uint64 dmaeTimestamp = dmae_getTimestamp();
	dmae_setRetiredTimestamp(dmaeTimestamp);
	if(hCPU->gpr[5] > 0)
		LatteBufferCache_notifyDCFlush(hCPU->gpr[3], hCPU->gpr[5]*4);
	osLib_returnFromFunction64(hCPU, dmaeTimestamp);
}

void dmaeExport_DMAEFillMem(PPCInterpreter_t* hCPU)
{
	uint32* dstBuffer = (uint32*)memory_getPointerFromVirtualOffset(hCPU->gpr[3]);
	uint32 value = hCPU->gpr[4];
	uint32 numU32s = hCPU->gpr[5];	
	value = _swapEndianU32(value);
	for(uint32 i=0; i<numU32s; i++)
	{
		*dstBuffer = value;
		dstBuffer++;
	}
	uint64 dmaeTimestamp = dmae_getTimestamp();
	dmae_setRetiredTimestamp(dmaeTimestamp);
	osLib_returnFromFunction64(hCPU, dmaeTimestamp);
}

void dmaeExport_DMAEWaitDone(PPCInterpreter_t* hCPU)
{
	//debug_printf("DMAEWaitDone(...)\n");
	// parameter:
	// r3/r4	uint64  		dmaeTimestamp
	osLib_returnFromFunction(hCPU, 1);
}

void dmaeExport_DMAESemaphore(PPCInterpreter_t* hCPU)
{
	// parameter:
	// r3	MPTR	addr
	// r4	uint32	actionType

	uint32 actionType = hCPU->gpr[4];

	std::atomic<uint64le>* semaphore = _rawPtrToAtomic((uint64le*)memory_getPointerFromVirtualOffset(hCPU->gpr[3]));

	if( actionType == 1 )
	{
		// Signal Semaphore
		semaphore->fetch_add(1);
	}
	else if (actionType == 0) // wait
	{
		cemuLog_logDebug(LogType::Force, "DMAESemaphore: Unsupported wait operation");
		semaphore->fetch_sub(1);
	}
	else
	{
		cemuLog_logDebug(LogType::Force, "DMAESemaphore unknown action type {}", actionType);
		cemu_assert_debug(false);
	}

	uint64 dmaeTimestamp = dmae_getTimestamp();
	dmae_setRetiredTimestamp(dmaeTimestamp);
	osLib_returnFromFunction64(hCPU, dmaeTimestamp);
}

void dmaeExport_DMAEGetRetiredTimeStamp(PPCInterpreter_t* hCPU)
{
	debug_printf("DMAEGetRetiredTimeStamp()\n");
	osLib_returnFromFunction64(hCPU, dmaeRetiredTimestamp);
}


void dmae_load()
{
	osLib_addFunction("dmae", "DMAECopyMem", dmaeExport_DMAECopyMem);
	osLib_addFunction("dmae", "DMAEFillMem", dmaeExport_DMAEFillMem);
	osLib_addFunction("dmae", "DMAEWaitDone", dmaeExport_DMAEWaitDone);
	osLib_addFunction("dmae", "DMAESemaphore", dmaeExport_DMAESemaphore);
	osLib_addFunction("dmae", "DMAEGetRetiredTimeStamp", dmaeExport_DMAEGetRetiredTimeStamp);
}
