#include "Cafe/OS/common/OSCommon.h"
#include "util/MemMapper/MemMapper.h"

#define OS_MAP_READ_ONLY		(1)
#define OS_MAP_READ_WRITE		(2)

#define VIRT_RANGE_ADDR			(0xA0000000) // todo: Process specific. For the main ram pid this overlaps with the overlay arena?
#define VIRT_RANGE_SIZE			(0x40000000)

#define PHYS_RANGE_ADDR			(0x10000000) // todo: Process specific
#define PHYS_RANGE_SIZE			(0x40000000)

namespace coreinit
{
	std::mutex s_memMappingMtx;

	struct OSVirtMemoryEntry
	{
		OSVirtMemoryEntry(MPTR virtualAddress, uint32 size, uint32 alignment) : virtualAddress(virtualAddress), size(size), alignment(alignment) {};

		MPTR virtualAddress;
		uint32 size;
		uint32 alignment;
	};

	std::vector<OSVirtMemoryEntry> s_allocatedVirtMemory;

	MPTR _OSAllocVirtAddr(uint32 size, uint32 alignment)
	{
		std::lock_guard _l(s_memMappingMtx);
		uint32 currentAddress = VIRT_RANGE_ADDR;
		uint32 endAddress = VIRT_RANGE_ADDR + VIRT_RANGE_SIZE;
		uint32 pageSize = (uint32)MemMapper::GetPageSize();
		pageSize = std::max<uint32>(pageSize, Espresso::MEM_PAGE_SIZE);
		while (true)
		{
			// calculated aligned start and end address for current region
			currentAddress = (currentAddress + alignment - 1) & ~(alignment - 1);
			currentAddress = (currentAddress + pageSize - 1) & ~(pageSize - 1);
			uint32 currentEndAddress = currentAddress + size;
			currentEndAddress = (currentEndAddress + pageSize - 1) & ~(pageSize - 1);
			// check if out of available space
			if (currentEndAddress > endAddress)
			{
				cemuLog_log(LogType::APIErrors, "_OSAllocVirtAddr(): Unable to allocate memory\n");
				return MPTR_NULL;
			}
			// check for overlapping regions
			bool emptySpaceFound = true;
			for(auto& virtMemIt : s_allocatedVirtMemory)
			{
				// check for range collision
				if (currentAddress < (virtMemIt.virtualAddress + virtMemIt.size) && currentEndAddress > virtMemIt.virtualAddress)
				{
					// regions overlap
					// adjust current address and keep looking
					currentAddress = virtMemIt.virtualAddress + virtMemIt.size;
					emptySpaceFound = false;
					break;
				}
			}
			if (emptySpaceFound)
			{
				// add entry
				s_allocatedVirtMemory.emplace_back(currentAddress, currentEndAddress - currentAddress, alignment);
				return currentAddress;
			}
		}
		return MPTR_NULL;
	}

	bool _OSFreeVirtAddr(MPTR virtAddr)
	{
		std::lock_guard _l(s_memMappingMtx);
		auto it = s_allocatedVirtMemory.begin();
		while (it != s_allocatedVirtMemory.end())
		{
			if (it->virtualAddress == virtAddr)
			{
				s_allocatedVirtMemory.erase(it);
				return true;
			}
			++it;
		}
		return false;
	}

	void OSGetAvailPhysAddrRange(uint32be* physRangeStart, uint32be* physRangeSize)
	{
		*physRangeStart = PHYS_RANGE_ADDR;
		*physRangeSize = PHYS_RANGE_SIZE;
	}

	void* OSAllocVirtAddr(MEMPTR<void> address, uint32 size, uint32 align)
	{
		if (align == 0)
			align = 1;
		if (align != 0 && align != 1)
			assert_dbg();
		cemu_assert_debug(address == nullptr); // todo - support for allocation with fixed address

		address = _OSAllocVirtAddr(size, align);
		debug_printf("OSAllocVirtAddr(): Allocated virtual memory at 0x%08x\n", address.GetMPTR());
		return MEMPTR<void>(address);
	}

	uint32 OSFreeVirtAddr(MEMPTR<void> address)
	{
		bool r = _OSFreeVirtAddr(address.GetMPTR());
		if(!r)
			cemuLog_log(LogType::APIErrors, "OSFreeVirtAddr: Could not find allocation with address 0x{:08x}\n", address.GetMPTR());
		return r ? 1 : 0;
	}

	uint32 OSMapMemory(MPTR virtualAddress, MPTR physicalAddress, uint32 size, uint32 mode)
	{
		if (virtualAddress < VIRT_RANGE_ADDR || virtualAddress >= (VIRT_RANGE_ADDR + VIRT_RANGE_SIZE))
		{
			cemuLog_log(LogType::APIErrors, "OSMapMemory: Virtual address out of bounds\n");
			return 0;
		}

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
			return 0;
		}
		return 1;
	}

	uint32 OSUnmapMemory(MPTR virtualAddress, uint32 size)
	{
		if (virtualAddress < VIRT_RANGE_ADDR || virtualAddress >= (VIRT_RANGE_ADDR + VIRT_RANGE_SIZE))
			cemu_assert_suspicious();

		cemu_assert((size % MemMapper::GetPageSize()) == 0);

		uint8* virtualPtr = memory_getPointerFromVirtualOffset(virtualAddress);

		MemMapper::FreeMemory(virtualPtr, size, true);
		return 1;
	}

	void InitializeMemoryMapping()
	{
		s_allocatedVirtMemory.clear();
		cafeExportRegister("coreinit", OSGetAvailPhysAddrRange, LogType::CoreinitMemoryMapping);
		cafeExportRegister("coreinit", OSAllocVirtAddr, LogType::CoreinitMemoryMapping);
		cafeExportRegister("coreinit", OSFreeVirtAddr, LogType::CoreinitMemoryMapping);
		cafeExportRegister("coreinit", OSMapMemory, LogType::CoreinitMemoryMapping);
		cafeExportRegister("coreinit", OSUnmapMemory, LogType::CoreinitMemoryMapping);
	}
}