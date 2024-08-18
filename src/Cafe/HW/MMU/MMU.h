#pragma once

void memory_init();
void memory_mapForCurrentTitle();
void memory_unmapForCurrentTitle();
void memory_logModifiedMemoryRanges();

void memory_enableOverlayArena();
void memory_enableHBLELFCodeArea();
uint32 memory_getVirtualOffsetFromPointer(void* ptr);
uint8* memory_getPointerFromVirtualOffset(uint32 virtualOffset);
uint8* memory_getPointerFromVirtualOffsetAllowNull(uint32 virtualOffset);

uint8* memory_getPointerFromPhysicalOffset(uint32 physicalOffset);

uint32 memory_virtualToPhysical(uint32 virtualOffset);
uint32 memory_physicalToVirtual(uint32 physicalOffset);

extern uint8* memory_base; // points to base of PowerPC address space

enum class MMU_MEM_AREA_ID
{
	CODE_LOW0,
	CODE_TRAMPOLINE,
	CODE_CAVE,
	CODE_MAIN,
	MEM2_DATA,
	FGBUCKET,
	TILING_APERATURE,
	OVERLAY,
	MAPABLE_SPACE,
	MEM1,
	RPLLOADER,
	SHAREDDATA,

	CPU_LC0,
	CPU_LC1,
	CPU_LC2,
	CPU_PER_CORE,

	CEMU_PRIVATE,
};

struct MMURange
{
	enum MFLAG
	{
		FLAG_OPTIONAL = (1 << 0), // allocate only on explicit request
		FLAG_MAP_EARLY = (1 << 1), // map at Cemu launch, normally memory is mapped when a game is loaded
	};

	MMURange(const uint32 baseAddress, const uint32 size, MMU_MEM_AREA_ID areaId, const std::string_view name, MFLAG flags = (MFLAG)0);

	void mapMem();
	void unmapMem();

	uint8* getPtr() const
	{
		cemu_assert_debug(m_isMapped);
		return memory_base + baseAddress;
	}

	uint32 getBase() const
	{
		return baseAddress;
	}

	// reset to initial parameters
	void resetConfig()
	{
		size = initSize;
	}

	void setEnd(uint32 endAddress)
	{
		cemu_assert_debug(!m_isMapped);
		cemu_assert_debug((endAddress & 0xFFF) == 0);
		size = endAddress - baseAddress;
	}

	// returns offset of last byte + 1 (base + size)
	uint32 getEnd() const
	{
		return baseAddress + size;
	}

	uint32 getSize() const
	{
		return size;
	}

	uint32 getInitSize() const
	{
		return initSize;
	}

	std::string_view getName() const
	{
		return name;
	}

	bool containsAddress(uint32 addr) const
	{
		return addr >= getBase() && addr < getEnd();
	}

	bool isMapped() const { return m_isMapped; };
	bool isOptional() const { return (flags & MFLAG::FLAG_OPTIONAL) != 0; };
	bool isMappedEarly() const { return (flags & MFLAG::FLAG_MAP_EARLY) != 0; };

	const uint32 baseAddress;
	const uint32 initSize; // initial size
	const std::string name;
	const MFLAG flags;
	const MMU_MEM_AREA_ID areaId;
	// runtime parameters
	uint32 size;
	bool m_isMapped{};
};


extern MMURange mmuRange_LOW0;
extern MMURange mmuRange_TRAMPOLINE_AREA;
extern MMURange mmuRange_CODECAVE;
extern MMURange mmuRange_TEXT_AREA;
extern MMURange mmuRange_MEM2;
extern MMURange mmuRange_CEMU_AREA;
extern MMURange mmuRange_OVERLAY_AREA;
extern MMURange mmuRange_FGBUCKET;
extern MMURange mmuRange_TILINGAPERTURE;
extern MMURange mmuRange_MEM1;
extern MMURange mmuRange_RPLLOADER;
extern MMURange mmuRange_SHARED_AREA;
extern MMURange mmuRange_CORE0_LC;
extern MMURange mmuRange_CORE1_LC;
extern MMURange mmuRange_CORE2_LC;
std::vector<MMURange*> memory_getMMURanges();
MMURange* memory_getMMURangeByAddress(MPTR address);

bool memory_isAddressRangeAccessible(MPTR virtualAddress, uint32 size);

#define MEMORY_CODELOW0_ADDR				(0x00010000)
#define MEMORY_CODELOW0_SIZE				(0x000F0000) // ~1MB

#define MEMORY_CODE_TRAMPOLINE_AREA_ADDR	(0x00E00000) // code area for trampolines and imports
#define MEMORY_CODE_TRAMPOLINE_AREA_SIZE	(0x00200000) // 2MB

#define MEMORY_CODECAVEAREA_ADDR			(0x01800000)
#define MEMORY_CODECAVEAREA_SIZE			(0x00400000) // 4MB

#define MEMORY_CODEAREA_ADDR				(0x02000000)
#define MEMORY_CODEAREA_SIZE				(0x0E000000) // 224MB

#define MEMORY_DATA_AREA_ADDR				(0x10000000)
#define MEMORY_DATA_AREA_SIZE				(0x40000000)

#define MEMORY_FGBUCKET_AREA_ADDR			(0xE0000000) // actual offset is 0xE0000000 according to PPC kernel
#define MEMORY_FGBUCKET_AREA_SIZE			(0x04000000) // 64MB split up into multiple subareas, size is verified with value from PPC kernel

#define MEMORY_TILINGAPERTURE_AREA_ADDR		(0xE8000000)
#define MEMORY_TILINGAPERTURE_AREA_SIZE		(0x02000000) // 32MB

#define MEMORY_OVERLAY_AREA_OFFSET			(0xA0000000)
#define MEMORY_OVERLAY_AREA_SIZE			(448*1024*1024) // 448MB (recycled background app memory)

#define MEMORY_MEM1_AREA_ADDR				(0xF4000000)
#define MEMORY_MEM1_AREA_SIZE				(0x02000000) // 32MB

#define MEMORY_RPLLOADER_AREA_ADDR			(0xF6000000) // workarea for RPLLoader (normally this is kernel workspace)
#define MEMORY_RPLLOADER_AREA_SIZE			(0x02000000) // 32MB

#define MEMORY_SHAREDDATA_AREA_ADDR			(0xF8000000)
#define MEMORY_SHAREDDATA_AREA_SIZE			(0x02000000) // 32MB

#if BOOST_OS_WINDOWS
#define CPU_swapEndianU64(_v) _byteswap_uint64((uint64)(_v))
#define CPU_swapEndianU32(_v) _byteswap_ulong((uint32)(_v))
#define CPU_swapEndianU16(_v) _byteswap_ushort((uint16)(_v))
#elif BOOST_OS_LINUX
#define CPU_swapEndianU64(_v) bswap_64((uint64)(_v))
#define CPU_swapEndianU32(_v) bswap_32((uint32)(_v))
#define CPU_swapEndianU16(_v) bswap_16((uint16)(_v))
#elif BOOST_OS_MACOS
#define CPU_swapEndianU64(_v) OSSwapInt64((uint64)(_v))
#define CPU_swapEndianU32(_v) OSSwapInt32((uint32)(_v))
#define CPU_swapEndianU16(_v) OSSwapInt16((uint16)(_v))
#endif

// C-style memory access, deprecated. Use memory_read<> and memory_write<> templates instead
void memory_writeDouble(uint32 address, double vf);
void memory_writeFloat(uint32 address, float vf);
void memory_writeU32(uint32 address, uint32 v);
void memory_writeU16(uint32 address, uint16 v);
void memory_writeU8(uint32 address, uint8 v);
void memory_writeU64(uint32 address, uint64 v);

double memory_readDouble(uint32 address);
float memory_readFloat(uint32 address);
uint64 memory_readU64(uint32 address);
uint32 memory_readU32(uint32 address);
uint16 memory_readU16(uint32 address);
uint8 memory_readU8(uint32 address);

void memory_createDump();

template<size_t count>
void memory_readBytes(VAddr address, std::array<uint8, count>& buffer)
{
	memcpy(buffer.data(), memory_getPointerFromVirtualOffset(address), count);
}

template <typename T> inline T memory_read(VAddr address)
{
	return *(betype<T>*)(memory_base + address);
}

template <typename T> inline void memory_write(VAddr address, T value)
{
	*(betype<T>*)(memory_base + address) = value;
}

// LLE implementation
void memory_initPhysicalLayout();

namespace MMU
{
	using MMIOFuncWrite32 = void (*)(PAddr addr, uint32 value);
	using MMIOFuncWrite16 = void (*)(PAddr addr, uint16 value);

	using MMIOFuncRead32 = uint32(*)(PAddr addr);
	using MMIOFuncRead16 = uint16(*)(PAddr addr);

	enum class MMIOInterface
	{
		INTERFACE_0C000000,
		INTERFACE_0D000000,
		//INTERFACE_0D000000,
	};

	void RegisterMMIO_W16(MMIOInterface interfaceLocation, uint32 relativeAddress, MMIOFuncWrite16 ptr);
	void RegisterMMIO_W32(MMIOInterface interfaceLocation, uint32 relativeAddress, MMIOFuncWrite32 ptr);
	void RegisterMMIO_R32(MMIOInterface interfaceLocation, uint32 relativeAddress, MMIOFuncRead32 ptr);
	void RegisterMMIO_R16(MMIOInterface interfaceLocation, uint32 relativeAddress, MMIOFuncRead16 ptr);

	template<typename TRegType, auto TReadFunc, auto TWriteFunc>
	void RegisterMMIO_32(MMIOInterface interfaceLocation, uint32 relativeAddress)
	{
		RegisterMMIO_W32(interfaceLocation, relativeAddress,
			[](PAddr addr, uint32 value) -> void
		{
			TRegType temp;
			temp.setFromRaw(value);
			TWriteFunc(addr, temp);
		});
		RegisterMMIO_R32(interfaceLocation, relativeAddress,
			[](PAddr addr) -> uint32
		{
			return TReadFunc(addr).getRawValue();
		});
	}

	void WriteMMIO_32(PAddr address, uint32 value);
	void WriteMMIO_16(PAddr address, uint16 value);
	uint16 ReadMMIO_32(PAddr address);
	uint16 ReadMMIO_16(PAddr address);

}

#define MMU_IsInPPCMemorySpace(__ptr) ((const uint8*)(__ptr) >= memory_base && (const uint8*)(__ptr) < (memory_base + 0x100000000))