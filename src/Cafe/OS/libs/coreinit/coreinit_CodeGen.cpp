#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/RPL/rpl.h"
#include "Cafe/OS/libs/coreinit/coreinit_CodeGen.h"
#include "Cafe/HW/Espresso/Recompiler/PPCRecompiler.h"
#include "Common/ExceptionHandler/ExceptionHandler.h"

namespace coreinit
{
	struct
	{
		bool rangeIsAllocated;
		MPTR rangeStart;
		uint32 rangeSize;
		uint8* cacheStateCopy; // holds a copy of the entire range, simulates icache state (updated via ICBI)
	}coreinitCodeGen = {0};

	void codeGenArea_memoryWriteCallback(void* pageStart, size_t size)
	{
		uint32 ea = memory_getVirtualOffsetFromPointer(pageStart);
		uint32 eaEnd = ea + (uint32)size;
		while (ea <= eaEnd)
		{
			codeGenHandleICBI(ea);
			ea += 0x20;
		}
	}

	void OSGetCodegenVirtAddrRange(betype<uint32>* rangeStart, betype<uint32>* rangeSize)
	{
		uint32 codegenSize = 0x01000000; // todo: Read from cos.xml
		//debug_printf("OSGetCodegenVirtAddrRange(0x%08x,0x%08x)\n", hCPU->gpr[3], hCPU->gpr[4]);
		// on first call, allocate range
		if( coreinitCodeGen.rangeIsAllocated == false )
		{
			coreinitCodeGen.rangeStart = RPLLoader_AllocateCodeSpace(codegenSize, 0x1000);
			coreinitCodeGen.rangeSize = codegenSize;
			coreinitCodeGen.cacheStateCopy = new uint8[codegenSize];
			memset(coreinitCodeGen.cacheStateCopy, 0, codegenSize);
			coreinitCodeGen.rangeIsAllocated = true;
		}
		*rangeStart = coreinitCodeGen.rangeStart;
		*rangeSize = coreinitCodeGen.rangeSize;
	}

	void OSGetCodegenVirtAddrRangeInternal(uint32& rangeStart, uint32& rangeSize)
	{
		if (coreinitCodeGen.rangeIsAllocated == 0)
		{
			rangeStart = 0;
			rangeSize = 0;
			return;
		}
		rangeStart = coreinitCodeGen.rangeStart;
		rangeSize = coreinitCodeGen.rangeSize;
	}

	void ICInvalidateRange(uint32 startAddress, uint32 size)
	{
		uint32 ea = startAddress & ~0x1F;
		uint32 eaEnd = (startAddress + size + 0x1F)&~0x1F;
		while (ea <= eaEnd)
		{
			codeGenHandleICBI(ea);
			ea += 0x20;
		}

	}

	void coreinitExport_OSCodegenCopy(PPCInterpreter_t* hCPU)
	{
		if( coreinitCodeGen.rangeIsAllocated == false )
			assert_dbg();

		uint32 codeAddrDest = hCPU->gpr[3];
		uint32 memAddrSrc = hCPU->gpr[4];
		uint32 size = hCPU->gpr[5];

		if( codeAddrDest < coreinitCodeGen.rangeStart || codeAddrDest >= (coreinitCodeGen.rangeStart+coreinitCodeGen.rangeSize) )
			assert_dbg();
		if( (codeAddrDest+size) < coreinitCodeGen.rangeStart || (codeAddrDest+size) > (coreinitCodeGen.rangeStart+coreinitCodeGen.rangeSize) )
			assert_dbg();

		memcpy(memory_getPointerFromVirtualOffset(codeAddrDest), memory_getPointerFromVirtualOffset(memAddrSrc), size);

		// invalidate recompiler range
		uint32 ea = codeAddrDest & ~0x1F;
		uint32 eaEnd = (codeAddrDest + size + 0x1F)&~0x1F;
		while (ea <= eaEnd)
		{
			codeGenHandleICBI(ea);
			ea += 0x20;
		}

		osLib_returnFromFunction(hCPU, 0);
	}

	void codeGenHandleICBI(uint32 ea)
	{
		cemu_assert_debug((ea & 0x1F) == 0);
		if (coreinitCodeGen.rangeIsAllocated == false)
			return;
		cemu_assert_debug((coreinitCodeGen.rangeStart & 0x1F) == 0);
		cemu_assert_debug((coreinitCodeGen.rangeSize & 0x1F) == 0);
		if (ea >= coreinitCodeGen.rangeStart && ea < (coreinitCodeGen.rangeStart + coreinitCodeGen.rangeSize))
		{
			uint8* cacheCopy = coreinitCodeGen.cacheStateCopy + (ea - coreinitCodeGen.rangeStart);
			uint8* currentState = memory_getPointerFromVirtualOffset(ea);
			if (memcmp(currentState, cacheCopy, 32) != 0)
			{
				// instructions changed
				// flush cache
				PPCRecompiler_invalidateRange(ea, ea+0x20);
				// update icache copy
				memcpy(cacheCopy, currentState, 32);
			}
		}
	}

	bool _avoidCodeGenJIT = false;

	// currently we dont handle code invalidation well for direct write access
	// therefore if we detect attempts to write we will disable JITing the area
	bool codeGenShouldAvoid()
	{
		return _avoidCodeGenJIT;
	}

	bool OSSwitchSecCodeGenMode(bool isRXOnly)
	{
		if (!_avoidCodeGenJIT)
		{
			cemuLog_log(LogType::Force, "Disable JIT on dynamic code area");
		}
		_avoidCodeGenJIT = true; // this function getting called is usually a sign that 
		// does this have a return value?
		return true;
	}

	void InitializeCodeGen()
	{
		cafeExportRegister("coreinit", OSGetCodegenVirtAddrRange, LogType::Placeholder);
		cafeExportRegister("coreinit", ICInvalidateRange, LogType::Placeholder);

		osLib_addFunction("coreinit", "OSCodegenCopy", coreinitExport_OSCodegenCopy);

		cafeExportRegister("coreinit", OSSwitchSecCodeGenMode, LogType::Placeholder);
	}
}
