#include "Cafe/HW/MMU/MMU.h"
#include "Cafe/GraphicPack/GraphicPack2.h"
#include "util/MemMapper/MemMapper.h"
#include <wx/msgdlg.h>
#include "config/ActiveSettings.h"

uint8* memory_base = NULL; // base address of the reserved 4GB space
uint8* memory_elfCodeArena = NULL;

void checkMemAlloc(void* result)
{
	if (result == nullptr)
		assert_dbg();
}

void memory_initPhysicalLayout()
{
	assert_dbg();
	// todo - rewrite this using new MemMapper and MMU tables
	//memory_base = (uint8*)VirtualAlloc(NULL, 0x100000000ULL, MEM_RESERVE, PAGE_READWRITE);
	//VirtualFree(memory_base, 0, MEM_RELEASE);

	//// todo - figure out all the ranges and allocate them properly

	//// allocate memory for the kernel
	////checkMemAlloc(VirtualAlloc(memory_base + 0x08000000, 1024*1024*2, MEM_COMMIT, PAGE_READWRITE));
	//// allocate memory for bootrom
	//checkMemAlloc(VirtualAlloc(memory_base + 0x00000000, 1024*16, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE));


	//// allocate memory at 0x016FFFFC (is this some sort of register interface or maybe just temporary storage?)
	//checkMemAlloc(VirtualAlloc(memory_base + 0x016FF000, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));

	//// temporary storage for bootrom copy
	//checkMemAlloc(VirtualAlloc(memory_base + 0x016c0000, 0x4000 + 0x4000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
	//// 0x016c0000

	//// L2
	//checkMemAlloc(VirtualAlloc(memory_base + 0xE0000000, 1024 * 16, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));

	//// kernel memory
	//// currently it is unknown if this is it's own physical memory region or if this is mapped somehow
	//// considering the ancast is never copied here and no memory mapping is setup it seems like a hardwired mirror to 0x08000000? 
	////checkMemAlloc(VirtualAlloc(memory_base + 0xFFE00000, 0x180000, MEM_COMMIT, PAGE_READWRITE));
	//HANDLE hKernelMem = CreateFileMappingA(
	//	INVALID_HANDLE_VALUE,    // use paging file
	//	NULL,                    // default security
	//	PAGE_READWRITE,          // read/write access
	//	0,                       // maximum object size (high-order DWORD)
	//	1024 * 1024 * 2,         // maximum object size (low-order DWORD)
	//	"kernelMem08000000");    // name of mapping object
	//
	//checkMemAlloc(MapViewOfFileEx(hKernelMem, FILE_MAP_ALL_ACCESS, 0, 0, 1024 * 1024 * 2, memory_base + 0x08000000));
	//checkMemAlloc(MapViewOfFileEx(hKernelMem, FILE_MAP_ALL_ACCESS, 0, 0, 1024 * 1024 * 2, memory_base + 0xFFE00000));

	//// IOSU->PPC bootParamBlock
	//checkMemAlloc(VirtualAlloc(memory_base + 0x01FFF000, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));

	//// used as dynamic kernel memory?
	//checkMemAlloc(VirtualAlloc(memory_base + 0x1C000000, 0x01000000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));

	//// mapped by kernel to FF200000 (loader.elf?)
	//checkMemAlloc(VirtualAlloc(memory_base + 0x1B800000, 0x00800000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
}

std::vector<struct MMURange*> g_mmuRanges;

std::vector<MMURange*> memory_getMMURanges()
{
	return g_mmuRanges;
}

MMURange* memory_getMMURangeByAddress(MPTR address)
{
	for (auto& itr : g_mmuRanges)
	{
		if (address >= itr->getBase() && address < itr->getEnd())
			return itr;
	}
	return nullptr;
}

MMURange::MMURange(const uint32 baseAddress, const uint32 size, MMU_MEM_AREA_ID areaId, const std::string_view name, MFLAG flags) : baseAddress(baseAddress), size(size), initSize(size), areaId(areaId), name(name), flags(flags)
{
	g_mmuRanges.emplace_back(this);
}

void MMURange::mapMem()
{
	cemu_assert_debug(!m_isMapped);
	if (MemMapper::AllocateMemory(memory_base + baseAddress, size, MemMapper::PAGE_PERMISSION::P_RW, true) == nullptr)
	{
		std::string errorMsg = fmt::format("Unable to allocate {} memory", name);
		wxMessageBox(errorMsg.c_str(), "Error", wxOK | wxCENTRE | wxICON_ERROR);
		#if BOOST_OS_WINDOWS
		ExitProcess(-1);
		#else
		exit(-1);
		#endif
	}
	m_isMapped = true;
}

void MMURange::unmapMem()
{
    MemMapper::FreeMemory(memory_base + baseAddress, size, true);
	m_isMapped = false;
}

MMURange mmuRange_LOW0					{ 0x00010000, 0x000F0000, MMU_MEM_AREA_ID::CODE_LOW0, "CODE_LOW0" }; // code cave (Cemuhook)
MMURange mmuRange_TRAMPOLINE_AREA		{ 0x00E00000, 0x00200000, MMU_MEM_AREA_ID::CODE_TRAMPOLINE, "TRAMPOLINE_AREA" }; // code area for trampolines and imports
MMURange mmuRange_CODECAVE				{ 0x01800000, 0x00400000, MMU_MEM_AREA_ID::CODE_CAVE, "CODECAVE" }; // code cave area (4MiB)
MMURange mmuRange_TEXT_AREA				{ 0x02000000, 0x0C000000, MMU_MEM_AREA_ID::CODE_MAIN, "TEXT_AREA" }; // module text sections go here (0x02000000 to 0x10000000, 224MiB)
MMURange mmuRange_CEMU_AREA				{ 0x0E000000, 0x02000000, MMU_MEM_AREA_ID::CEMU_PRIVATE, "CEMU_AREA", MMURange::MFLAG::FLAG_MAP_EARLY }; // Cemu-only, 32MiB. Should be allocated early for SysAllocator
MMURange mmuRange_MEM2					{ 0x10000000, 0x40000000, MMU_MEM_AREA_ID::MEM2_DATA, "MEM2" }; // main memory area (1GB)
MMURange mmuRange_OVERLAY_AREA			{ 0xA0000000, 0x1C000000, MMU_MEM_AREA_ID::OVERLAY, "OVERLAY_AREA", MMURange::MFLAG::FLAG_OPTIONAL }; // has to be requested, 448MiB
MMURange mmuRange_FGBUCKET				{ 0xE0000000, 0x04000000, MMU_MEM_AREA_ID::FGBUCKET, "FGBUCKET" }; // foreground bucket (64MiB)
MMURange mmuRange_TILINGAPERTURE		{ 0xE8000000, 0x02000000, MMU_MEM_AREA_ID::TILING_APERATURE, "TILINGAPERTURE" }; // tiling aperture
MMURange mmuRange_MEM1					{ 0xF4000000, 0x02000000, MMU_MEM_AREA_ID::MEM1, "MEM1" }; // 32MiB
MMURange mmuRange_RPLLOADER				{ 0xF6000000, 0x02000000, MMU_MEM_AREA_ID::RPLLOADER, "RPLLOADER_AREA" }; // shared with RPLLoader
MMURange mmuRange_SHARED_AREA			{ 0xF8000000, 0x02000000, MMU_MEM_AREA_ID::SHAREDDATA, "SHARED_AREA", MMURange::MFLAG::FLAG_MAP_EARLY }; // 32MiB, Cemuhook accesses this memory region at boot
MMURange mmuRange_CORE0_LC				{ 0xFFC00000, 0x00005000, MMU_MEM_AREA_ID::CPU_LC0, "CORE0_LC" }; // locked L2 cache of core 0
MMURange mmuRange_CORE1_LC				{ 0xFFC40000, 0x00005000, MMU_MEM_AREA_ID::CPU_LC1, "CORE1_LC" }; // locked L2 cache of core 1
MMURange mmuRange_CORE2_LC				{ 0xFFC80000, 0x00005000, MMU_MEM_AREA_ID::CPU_LC2, "CORE2_LC" }; // locked L2 cache of core 2
MMURange mmuRange_HIGHMEM				{ 0xFFFFF000, 0x00001000, MMU_MEM_AREA_ID::CPU_PER_CORE, "PER-CORE" }; // per-core memory? Used by coreinit and PPC kernel to store core context specific data (like current thread ptr). We dont use it but Project Zero has a bug where it writes a byte at 0xfffffffe thus this memory range needs to be writable

void memory_init()
{
	// reserve a continous range of 4GB
	if(!memory_base)
		memory_base = (uint8*)MemMapper::ReserveMemory(nullptr, (size_t)0x100000000, MemMapper::PAGE_PERMISSION::P_RW);
	if( !memory_base )
	{
		debug_printf("memory_init(): Unable to reserve 4GB of memory\n");
		debugBreakpoint();
		wxMessageBox("Unable to reserve 4GB of memory\n", "Error", wxOK | wxCENTRE | wxICON_ERROR);
		exit(-1);
	}
	for (auto& itr : g_mmuRanges)
	{
		if (itr->isMappedEarly())
			itr->mapMem();
	}
}

void memory_mapForCurrentTitle()
{
	for (auto& itr : g_mmuRanges)
		if(!itr->isMapped())
			itr->resetConfig();
	// expand ranges
	auto gfxPackMappings = GraphicPack2::GetActiveRAMMappings();
	for (auto& mapping : gfxPackMappings)
	{
		MMURange* mmuRange = nullptr;
		for (auto& itr : g_mmuRanges)
		{
			if (itr->getBase() == mapping.first)
			{
				mmuRange = itr;
				break;
			}
		}
		if (!mmuRange)
		{
			cemuLog_log(LogType::Force, fmt::format("Graphic pack error: Unable to apply modified RAM mapping {:08x}-{:08x}. Start address must match one of the existing MMU ranges:", mapping.first, mapping.second));
			for (auto& itr : g_mmuRanges)
			{
				if(itr->isMapped())
					continue;
				cemuLog_log(LogType::Force, fmt::format("{:08x}-{:08x} ({:})", itr->getBase(), itr->getEnd(), itr->getName()));
			}
			continue;
		}
		// make sure the new range isn't overlapping with anything
		bool isOverlapping = false;
		for (auto& itr : g_mmuRanges)
		{
			if(itr == mmuRange)
				continue;
			if (mapping.first < itr->getEnd() && mapping.second > itr->getBase())
			{
				cemuLog_log(LogType::Force, fmt::format("Graphic pack error: Unable to apply modified memory range {:08x}-{:08x} since it is overlapping with {:08x}-{:08x} ({:})", mapping.first, mapping.second, itr->getBase(), itr->getEnd(), itr->getName()));
				isOverlapping = true;
			}
		}
		if(isOverlapping)
			continue;
		mmuRange->setEnd(mapping.second);
	}

	for (auto& itr : g_mmuRanges)
	{
		if (!itr->isOptional() && !itr->isMappedEarly())
			itr->mapMem();
	}
}

void memory_unmapForCurrentTitle()
{
    for (auto& itr : g_mmuRanges)
    {
        if (itr->isMapped() && !itr->isMappedEarly())
            itr->unmapMem();
    }
}

void memory_logModifiedMemoryRanges()
{
	auto gfxPackMappings = GraphicPack2::GetActiveRAMMappings();
	for (auto& mapping : gfxPackMappings)
	{
		MMURange* mmuRange = nullptr;
		for (auto& itr : g_mmuRanges)
		{
			if (itr->getBase() == mapping.first)
			{
				mmuRange = itr;
				break;
			}
		}
		if (!mmuRange)
			continue;
		sint32 extraMem = (sint32)mapping.second - (sint32)(mmuRange->getBase() + mmuRange->getInitSize());
		extraMem = (extraMem + 1023) / 1024;
		std::string memAmountStr;
		if (extraMem >= 8 * 1024 * 1024)
			memAmountStr = fmt::format("{:+}MiB", (extraMem + 1023) / 1024);
		else
			memAmountStr = fmt::format("{:+}KiB", extraMem);
		cemuLog_log(LogType::Force, fmt::format("Graphic pack: Using modified RAM mapping {:08x}-{:08x} ({})", mapping.first, mapping.second, memAmountStr));
	}
}

void memory_enableOverlayArena()
{
	if (mmuRange_OVERLAY_AREA.isMapped())
		return;
	mmuRange_OVERLAY_AREA.mapMem();
}

void memory_enableHBLELFCodeArea()
{
	if (memory_elfCodeArena != NULL)
		return;
	memory_elfCodeArena = (uint8*)MemMapper::AllocateMemory(memory_base + 0x00800000, 0x00800000, MemMapper::PAGE_PERMISSION::P_RW, true);
	if (memory_elfCodeArena == NULL)
	{
		debug_printf("memory_enableHBLELFCodeArea(): Unable to allocate memory for ELF arena\n");
		debugBreakpoint();
	}
}

bool memory_isAddressRangeAccessible(MPTR virtualAddress, uint32 size)
{
	for (auto& itr : g_mmuRanges)
	{
		if(!itr->isMapped())
			continue;
		if (virtualAddress >= itr->getBase() && virtualAddress < itr->getEnd())
		{
			uint32 remainingSize = itr->getEnd() - virtualAddress;
			return size <= remainingSize && itr->isMapped();
		}
	}
	return false;
}

uint32 memory_virtualToPhysical(uint32 virtualOffset)
{
	// currently we map virtual to physical space 1:1
	return virtualOffset;
}

uint32 memory_physicalToVirtual(uint32 physicalOffset)
{
	// currently we map virtual to physical space 1:1
	return physicalOffset;
}

uint8* memory_getPointerFromPhysicalOffset(uint32 physicalOffset)
{
	return memory_base + physicalOffset;
}

uint32 memory_getVirtualOffsetFromPointer(void* ptr)
{
	if( !ptr )
		return MPTR_NULL;
	return (uint32)((uint8*)ptr - (uint8*)memory_base);
}

uint8* memory_getPointerFromVirtualOffset(uint32 virtualOffset)
{	
	return memory_base + virtualOffset;
}

uint8* memory_getPointerFromVirtualOffsetAllowNull(uint32 virtualOffset)
{	
	if( virtualOffset == MPTR_NULL )
		return nullptr;
	return memory_getPointerFromVirtualOffset(virtualOffset);
}

// write access
void memory_writeDouble(uint32 address, double vf)
{
	uint64 v = *(uint64*)&vf;
	uint32 v1 = v&0xFFFFFFFF;
	uint32 v2 = v>>32;
	uint8* ptr = memory_getPointerFromVirtualOffset(address);
	*(uint32*)(ptr+4) = CPU_swapEndianU32(v1);
	*(uint32*)(ptr+0) = CPU_swapEndianU32(v2);
}

void memory_writeFloat(uint32 address, float vf)
{
	uint32 v = *(uint32*)&vf;
	*(uint32*)(memory_getPointerFromVirtualOffset(address)) = CPU_swapEndianU32(v);
}

void memory_writeU32(uint32 address, uint32 v)
{
	*(uint32*)(memory_getPointerFromVirtualOffset(address)) = CPU_swapEndianU32(v);
}

void memory_writeU64(uint32 address, uint64 v)
{
	*(uint64*)(memory_getPointerFromVirtualOffset(address)) = CPU_swapEndianU64(v);
}

void memory_writeU16(uint32 address, uint16 v)
{
	*(uint16*)(memory_getPointerFromVirtualOffset(address)) = CPU_swapEndianU16(v);
}

void memory_writeU8(uint32 address, uint8 v)
{
	*(uint8*)(memory_getPointerFromVirtualOffset(address)) = v;
}

// read access

double memory_readDouble(uint32 address)
{
	uint32 v[2];
	v[1] = *(uint32*)(memory_getPointerFromVirtualOffset(address));
	v[0] = *(uint32*)(memory_getPointerFromVirtualOffset(address)+4);
	v[0] = CPU_swapEndianU32(v[0]);
	v[1] = CPU_swapEndianU32(v[1]);
	return *(double*)v;
}

float memory_readFloat(uint32 address)
{
	uint32 v = *(uint32*)(memory_getPointerFromVirtualOffset(address));
	v = CPU_swapEndianU32(v);
	return *(float*)&v;
}

uint64 memory_readU64(uint32 address)
{
	uint64 v = *(uint64*)(memory_getPointerFromVirtualOffset(address));
	return CPU_swapEndianU64(v);
}

uint32 memory_readU32(uint32 address)
{
	uint32 v = *(uint32*)(memory_getPointerFromVirtualOffset(address));
	return CPU_swapEndianU32(v);
}

uint16 memory_readU16(uint32 address)
{
	uint16 v = *(uint16*)(memory_getPointerFromVirtualOffset(address));
	return CPU_swapEndianU16(v);
}

uint8 memory_readU8(uint32 address)
{
	return *(uint8*)(memory_getPointerFromVirtualOffset(address));
}

extern "C" DLLEXPORT void* memory_getBase()
{
	return memory_base;
}

void memory_writeDumpFile(uint32 startAddr, uint32 size, const fs::path& path)
{
	fs::path filePath = path;
	filePath /= fmt::format("{:08x}.bin", startAddr);
	FileStream* fs = FileStream::createFile2(filePath);
	if (fs)
	{
		fs->writeData(memory_base + startAddr, size);
		delete fs;
	}
}

void memory_createDump()
{
	const uint32 pageSize = MemMapper::GetPageSize();
	fs::path path = ActiveSettings::GetUserDataPath("dump/ramDump{:}", (uint32)time(nullptr));
	fs::create_directories(path);

	for (auto& itr : g_mmuRanges)
	{
		if(!itr->isMapped())
			continue;
		memory_writeDumpFile(itr->getBase(), itr->getSize(), path);
	}
}

namespace MMU
{
	// MMIO access handler
	// located in address region 0x0C000000 - 0x0E000000
	// there seem to be multiple subregions + special meanings for some address bits maybe?
	// Try to figure this out. We know these regions (in Wii U mode):
	// 0x0C000000 (the old GC register interface?)
	// 0x0D000000 (new Wii U stuff?)

	std::unordered_map<PAddr, MMIOFuncWrite32>* g_mmioHandlerW32{};
	std::unordered_map<PAddr, MMIOFuncWrite16>* g_mmioHandlerW16{};
	std::unordered_map<PAddr, MMIOFuncRead32>* g_mmioHandlerR32{};
	std::unordered_map<PAddr, MMIOFuncRead16>* g_mmioHandlerR16{};

	void _initHandlers()
	{
		if (g_mmioHandlerW32)
			return;
		g_mmioHandlerW32 = new std::unordered_map<PAddr, MMIOFuncWrite32>();
		g_mmioHandlerW16 = new std::unordered_map<PAddr, MMIOFuncWrite16>();
		g_mmioHandlerR32 = new std::unordered_map<PAddr, MMIOFuncRead32>();
		g_mmioHandlerR16 = new std::unordered_map<PAddr, MMIOFuncRead16>();
	}

	PAddr _MakeMMIOAddress(MMIOInterface interfaceLocation, uint32 relativeAddress)
	{
		PAddr addr = 0;
		if (interfaceLocation == MMIOInterface::INTERFACE_0C000000)
			addr = 0x0C000000;
		else if (interfaceLocation == MMIOInterface::INTERFACE_0D000000)
			addr = 0x0D000000;
		else
			assert_dbg();
		return addr + relativeAddress;
	}

	void RegisterMMIO_W32(MMIOInterface interfaceLocation, uint32 relativeAddress, MMIOFuncWrite32 ptr)
	{
		_initHandlers();
		g_mmioHandlerW32->emplace(_MakeMMIOAddress(interfaceLocation, relativeAddress), ptr);
	}

	void RegisterMMIO_W16(MMIOInterface interfaceLocation, uint32 relativeAddress, MMIOFuncWrite16 ptr)
	{
		_initHandlers();
		g_mmioHandlerW16->emplace(_MakeMMIOAddress(interfaceLocation, relativeAddress), ptr);
	}

	void RegisterMMIO_R32(MMIOInterface interfaceLocation, uint32 relativeAddress, MMIOFuncRead32 ptr)
	{
		_initHandlers();
		PAddr addr = _MakeMMIOAddress(interfaceLocation, relativeAddress);
		g_mmioHandlerR32->emplace(addr, ptr);
	}

	void RegisterMMIO_R16(MMIOInterface interfaceLocation, uint32 relativeAddress, MMIOFuncRead16 ptr)
	{
		_initHandlers();
		g_mmioHandlerR16->emplace(_MakeMMIOAddress(interfaceLocation, relativeAddress), ptr);
	}

	void WriteMMIO_32(PAddr address, uint32 value)
	{
		cemu_assert_debug((address & 0x3) == 0);
		auto itr = g_mmioHandlerW32->find(address);
		if (itr == g_mmioHandlerW32->end())
		{
			//cemuLog_logDebug(LogType::Force, "[MMU] MMIO write u32 0x{:08x} from unhandled address 0x{:08x}", value, address);
			return;
		}
		return itr->second(address, value);
	}

	void WriteMMIO_16(PAddr address, uint16 value)
	{
		cemu_assert_debug((address & 0x1) == 0);
		auto itr = g_mmioHandlerW16->find(address);
		if (itr == g_mmioHandlerW16->end())
		{
			//cemuLog_logDebug(LogType::Force, "[MMU] MMIO write u16 0x{:04x} from unhandled address 0x{:08x}", (uint32)value, address);
			return;
		}
		return itr->second(address, value);
	}


	// todo - instead of passing the physical address to Read/WriteMMIO we should pass an interface id and a relative address? This would allow remapping the hardware address (tho we can just unregister + register at different addresses)

	uint16 ReadMMIO_32(PAddr address)
	{
		cemu_assert_debug((address & 0x3) == 0);
		auto itr = g_mmioHandlerR32->find(address);
		if(itr == g_mmioHandlerR32->end())
		{
			//cemuLog_logDebug(LogType::Force, "[MMU] MMIO read u32 from unhandled address 0x{:08x}", address);
			return 0;
		}
		return itr->second(address);
	}

	uint16 ReadMMIO_16(PAddr address)
	{
		cemu_assert_debug((address & 0x1) == 0);
		auto itr = g_mmioHandlerR16->find(address);
		if (itr == g_mmioHandlerR16->end())
		{
			//cemuLog_logDebug(LogType::Force, "[MMU] MMIO read u16 from unhandled address 0x{:08x}", address);
			return 0;
		}
		return itr->second(address);
	}


}
