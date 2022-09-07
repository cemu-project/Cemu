#include "Cafe/OS/common/OSCommon.h"
#include "util/MemMapper/MemMapper.h"

#define OS_MAP_READ_ONLY (1)
#define OS_MAP_READ_WRITE (2)

namespace coreinit
{
struct OSVirtMemory
{
	MPTR virtualAddress;
	uint32 size;
	uint32 alignment;
	OSVirtMemory* next;
};

OSVirtMemory* virtualMemoryList = nullptr;

MPTR _VirtualMemoryAlloc(uint32 size, uint32 alignment)
{
	uint32 currentAddress = MEMORY_MAPABLE_VIRT_AREA_OFFSET;
	uint32 endAddress = MEMORY_MAPABLE_VIRT_AREA_OFFSET + MEMORY_MAPABLE_VIRT_AREA_SIZE;
	uint32 pageSize = (uint32)MemMapper::GetPageSize();
	while (true)
	{
		// calculated aligned start and end address for current region
		currentAddress = (currentAddress + alignment - 1) & ~(alignment - 1);
		currentAddress = (currentAddress + pageSize - 1) & ~(pageSize - 1);
		uint32 currentEndAddress = currentAddress + size;
		currentEndAddress = (currentEndAddress + pageSize - 1) & ~(pageSize - 1);
		// check if out of available space
		if (currentEndAddress >= endAddress)
		{
			debug_printf("coreinitVirtualMemory_alloc(): Unable to allocate memory\n");
			debugBreakpoint();
			return NULL;
		}
		// check for overlapping regions
		OSVirtMemory* virtMemItr = virtualMemoryList;
		bool emptySpaceFound = true;
		while (virtMemItr)
		{
			// check for range collision
			if (currentAddress < (virtMemItr->virtualAddress + virtMemItr->size) &&
				currentEndAddress > virtMemItr->virtualAddress)
			{
				// regions overlap
				// adjust current address and try again
				currentAddress = virtMemItr->virtualAddress + virtMemItr->size;
				emptySpaceFound = false;
				break;
			}
			// next
			virtMemItr = virtMemItr->next;
		}
		if (emptySpaceFound)
		{
			// add entry
			OSVirtMemory* virtMemory = (OSVirtMemory*)malloc(sizeof(OSVirtMemory));
			memset(virtMemory, 0x00, sizeof(OSVirtMemory));
			virtMemory->virtualAddress = currentAddress;
			virtMemory->size = currentEndAddress - currentAddress;
			virtMemory->alignment = alignment;
			virtMemory->next = virtualMemoryList;
			virtualMemoryList = virtMemory;
			return currentAddress;
		}
	}
	return NULL;
}

void coreinitExport_OSGetAvailPhysAddrRange(PPCInterpreter_t* hCPU)
{
	// parameters:
	// r3	MPTR*		areaStart
	// r4	uint32		areaSize
	memory_writeU32(hCPU->gpr[3], MEMORY_MAPABLE_PHYS_AREA_OFFSET);
	memory_writeU32(hCPU->gpr[4], MEMORY_MAPABLE_PHYS_AREA_SIZE);

	osLib_returnFromFunction(hCPU, 0);
}

void coreinitExport_OSAllocVirtAddr(PPCInterpreter_t* hCPU)
{
	// parameters:
	// r3	MPTR		address
	// r4	uint32		size
	// r5	uint32		align

	uint32 address = hCPU->gpr[3];
	uint32 size = hCPU->gpr[4];
	uint32 align = hCPU->gpr[5];
	if (address != MPTR_NULL)
	{
		debug_printf("coreinitExport_OSAllocVirtAddr(): Unsupported address != NULL\n");
		debugBreakpoint();
	}
	if (align == 0)
		align = 1;
	if (align != 0 && align != 1)
		assert_dbg();

	address = _VirtualMemoryAlloc(size, align);
	debug_printf("coreinitExport_OSAllocVirtAddr(): Allocated virtual memory at 0x%08x\n", address);
	osLib_returnFromFunction(hCPU, address);
}

void coreinitExport_OSMapMemory(PPCInterpreter_t* hCPU)
{
	// parameters:
	// r3	MPTR		virtualAddress
	// r4	MPTR		physicalAddress
	// r5	uint32		size
	// r6	uint32		mode
	MPTR virtualAddress = hCPU->gpr[3];
	MPTR physicalAddress = hCPU->gpr[4];
	uint32 size = hCPU->gpr[5];
	uint32 mode = hCPU->gpr[6];

	if (virtualAddress < MEMORY_MAPABLE_VIRT_AREA_OFFSET ||
		virtualAddress >= (MEMORY_MAPABLE_VIRT_AREA_OFFSET + MEMORY_MAPABLE_VIRT_AREA_SIZE))
		cemu_assert_suspicious();

	uint8* virtualPtr = memory_getPointerFromVirtualOffset(virtualAddress);

	MemMapper::PAGE_PERMISSION pageProtect = MemMapper::PAGE_PERMISSION::P_NONE;
	if (mode == OS_MAP_READ_ONLY)
		pageProtect = MemMapper::PAGE_PERMISSION::P_READ;
	else if (mode == OS_MAP_READ_WRITE)
		pageProtect = MemMapper::PAGE_PERMISSION::P_RW;
	else
		cemu_assert_unimplemented();
	void* allocationResult = MemMapper::AllocateMemory(virtualPtr, size, pageProtect, true);
	if (!allocationResult)
	{
		cemuLog_log(LogType::Force, "OSMapMemory failed");
		osLib_returnFromFunction(hCPU, 0);
		return;
	}
	osLib_returnFromFunction(hCPU, 1);
}

void coreinitExport_OSUnmapMemory(PPCInterpreter_t* hCPU)
{
	// parameters:
	// r3	MPTR		virtualAddress
	// r4	uint32		size
	MPTR virtualAddress = hCPU->gpr[3];
	uint32 size = hCPU->gpr[4];

	if (virtualAddress < MEMORY_MAPABLE_VIRT_AREA_OFFSET ||
		virtualAddress >= (MEMORY_MAPABLE_VIRT_AREA_OFFSET + MEMORY_MAPABLE_VIRT_AREA_SIZE))
		cemu_assert_suspicious();

	cemu_assert((size % MemMapper::GetPageSize()) == 0);

	uint8* virtualPtr = memory_getPointerFromVirtualOffset(virtualAddress);

	MemMapper::FreeMemory(virtualPtr, size, true);
	osLib_returnFromFunction(hCPU, 1);
}

void InitializeMemoryMapping()
{
	osLib_addFunction("coreinit", "OSGetAvailPhysAddrRange",
					  coreinitExport_OSGetAvailPhysAddrRange);
	osLib_addFunction("coreinit", "OSAllocVirtAddr", coreinitExport_OSAllocVirtAddr);
	osLib_addFunction("coreinit", "OSMapMemory", coreinitExport_OSMapMemory);
	osLib_addFunction("coreinit", "OSUnmapMemory", coreinitExport_OSUnmapMemory);
}
} // namespace coreinit