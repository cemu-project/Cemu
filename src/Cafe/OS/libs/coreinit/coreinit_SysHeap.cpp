#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/coreinit/coreinit_SysHeap.h"
#include "Cafe/OS/libs/coreinit/coreinit_MEM_ExpHeap.h"

namespace coreinit
{
	coreinit::MEMHeapHandle _sysHeapHandle = MPTR_NULL;
	sint32 _sysHeapAllocCounter = 0;
	sint32 _sysHeapFreeCounter = 0;

	void* OSAllocFromSystem(uint32 size, uint32 alignment)
	{
		_sysHeapAllocCounter++;
		return coreinit::MEMAllocFromExpHeapEx(_sysHeapHandle, size, alignment);
	}

	void export_OSAllocFromSystem(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(size, 0);
		ppcDefineParamS32(alignment, 1);
		MEMPTR<void> mem = OSAllocFromSystem(size, alignment);
		forceLogDebug_printf("OSAllocFromSystem(0x%x, %d) -> 0x%08x", size, alignment, mem.GetMPTR());
		osLib_returnFromFunction(hCPU, mem.GetMPTR());
	}

	void InitSysHeap()
	{
		uint32 sysHeapSize = 8 * 1024 * 1024; // actual size is unknown
		MEMPTR<void> heapBaseAddress = memory_getPointerFromVirtualOffset(coreinit_allocFromSysArea(sysHeapSize, 0x1000));
		_sysHeapHandle = coreinit::MEMCreateExpHeapEx(heapBaseAddress.GetPtr(), sysHeapSize, MEM_HEAP_OPTION_THREADSAFE);
		_sysHeapAllocCounter = 0;
		_sysHeapFreeCounter = 0;
	}

	void InitializeSysHeap()
	{
		osLib_addFunction("coreinit", "OSAllocFromSystem", export_OSAllocFromSystem);
	}

}
