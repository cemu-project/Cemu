#pragma once

#include "util/ChunkedHeap/ChunkedHeap.h"

#define RPL_MODULE_NAME_LENGTH	64
#define RPL_MODULE_PATH_LENGTH	256

// types
#define SHT_RPL_EXPORTS			(0x80000001)
#define SHT_RPL_IMPORTS			(0x80000002)
#define SHT_RPL_CRCS			(0x80000003)
#define SHT_RPL_FILEINFO		(0x80000004)

#define SHT_PROGBITS			(0x00000001)
#define SHT_SYMTAB				(0x00000002)
#define SHT_STRTAB				(0x00000003)
#define SHT_RELA				(0x00000004)
#define SHT_HASH				(0x00000005)
#define SHT_DYNAMIC				(0x00000006)
#define SHT_NOTE				(0x00000007)
#define SHT_NOBITS				(0x00000008) // this section contains no data
#define SHT_REL					(0x00000009) 
#define SHT_SHLIB				(0x0000000A) 
#define SHT_DYNSYM				(0x0000000B)

// flags
#define SHF_EXECUTE				0x00000004
#define SHF_RPL_COMPRESSED		0x08000000
#define SHF_TLS					0x04000000

// relocs
#define RPL_RELOC_ADDR32	1
#define RPL_RELOC_LO16		4
#define RPL_RELOC_HI16		5
#define RPL_RELOC_HA16		6
#define RPL_RELOC_REL24		10 
#define RPL_RELOC_REL14		11

#define R_PPC_DTPMOD32		68
#define R_PPC_DTPREL32		78

#define R_PPC_REL16_HA		251
#define R_PPC_REL16_HI		252
#define R_PPC_REL16_LO		253

#define HLE_MODULE_PTR		((RPLModule*)-1)

typedef struct
{
	/* +0x00 */ uint32be relocOffset;
	/* +0x04 */ uint32be symbolIndexAndType;
	/* +0x08 */ uint32be relocAddend;
}rplRelocNew_t;

typedef struct
{
	/* +0x00 */ uint32be nameOffset;
	/* +0x04 */ uint32be type;
	/* +0x08 */ uint32be flags;
	/* +0x0C */ uint32be virtualAddress;
	/* +0x10 */ uint32be fileOffset;
	/* +0x14 */ uint32be sectionSize;
	/* +0x18 */ uint32be symtabSectionIndex;
	/* +0x1C */ uint32be relocTargetSectionIndex;
	/* +0x20 */ uint32be alignment;
	/* +0x24 */ uint32be ukn24; // for symtab: Size of each symbol entry
}rplSectionEntryNew_t;

typedef struct
{
	/* +0x00 */ uint32be magic1;
	/* +0x04 */ uint8    version04; // probably version?
	/* +0x05 */ uint8    ukn05; // probably version?
	/* +0x06 */ uint8    ukn06; // probably version?
	/* +0x07 */ uint8    magic2_0; // part of second magic
	/* +0x08 */ uint8    magic2_1; // part of second magic
	/* +0x09 */ uint8    ukn09;
	/* +0x0A */ uint8    ukn0A;
	/* +0x0B */ uint8    ukn0B;
	/* +0x0C */ uint32be dataRegionSize;
	/* +0x10 */ uint8    ukn10;
	/* +0x11 */ uint8    ukn11;
	/* +0x12 */ uint16be ukn12;
	/* +0x14 */ uint32be ukn14;
	/* +0x18 */ uint32be entrypoint;
	/* +0x1C */ uint32be ukn1C;
	/* +0x20 */ uint32be sectionTableOffset;
	/* +0x24 */ uint32be ukn24;
	/* +0x28 */ uint16be ukn28;
	/* +0x2A */ uint16be programHeaderTableEntrySize;
	/* +0x2C */ uint16be programHeaderTableEntryCount;
	/* +0x2E */ uint16be sectionTableEntrySize;
	/* +0x30 */ uint16be sectionTableEntryCount;
	/* +0x32 */ uint16be nameSectionIndex;
}rplHeaderNew_t;

static_assert(offsetof(rplHeaderNew_t, dataRegionSize) == 0xC);
static_assert(offsetof(rplHeaderNew_t, programHeaderTableEntrySize) == 0x2A);


typedef struct
{
	/* +0x00 */ uint32be fileInfoMagic; // always 0xCAFE0402
	/* +0x04 */ uint32be textRegionSize; // text region size
	/* +0x08 */ uint32be ukn08; // base align text
	/* +0x0C */ uint32be dataRegionSize; // size of data sections
	/* +0x10 */ uint32be baseAlign; // base align data?
	/* +0x14 */ uint32be ukn14;
	/* +0x18 */ uint32be ukn18;
	/* +0x1C */ uint32be ukn1C;
	/* +0x20 */ uint32be trampolineAdjustment;
	/* +0x24 */ uint32be sdataBase1;
	/* +0x28 */ uint32be sdataBase2;
	/* +0x2C */ uint32be ukn2C;
	/* +0x30 */ uint32be ukn30;
	/* +0x34 */ uint32be ukn34;
	/* +0x38 */ uint32be ukn38;
	/* +0x3C */ uint32be ukn3C;
	/* +0x40 */ uint32be toolkitVersion;
	/* +0x44 */ uint32be ukn44;
	/* +0x48 */ uint32be ukn48;
	/* +0x4C */ uint32be ukn4C;
	/* +0x50 */ uint32be ukn50;
	/* +0x54 */ uint32be ukn54;
	/* +0x58 */ sint16be tlsModuleIndex;
}RPLFileInfoData;

static_assert(offsetof(RPLFileInfoData, tlsModuleIndex) == 0x58);


typedef struct
{
	//uint32 address;
	void* ptr;
}rplSectionAddressEntry_t;

typedef struct
{
	uint32be virtualOffset;
	uint32be nameOffset;
}rplExportTableEntry_t;

struct RPLModule
{
	uint32 ukn00; // pointer to shared memory region? (0xEFE01000)
	uint32 ukn04; // related to text region size?
	uint32 padding14;
	uint32 padding18;
	rplHeaderNew_t rplHeader;
	rplSectionEntryNew_t* sectionTablePtr; // copy of section table

	uint32 entrypoint;

	MPTR textRegionTemp; // temporary memory for text section?

	MEMPTR<void> regionMappingBase_text; // base destination address for text region
	MPTR regionMappingBase_data; // base destination address for data region
	MPTR regionMappingBase_loaderInfo; // base destination address for loaderInfo region
	uint8* tempRegionPtr;
	uint32 tempRegionAllocSize;

	uint32 exportDCount;
	rplExportTableEntry_t* exportDDataPtr;
	uint32 exportFCount;
	rplExportTableEntry_t* exportFDataPtr;

	std::string moduleName2;
	
	std::vector<rplSectionAddressEntry_t> sectionAddressTable2;

	uint32 tlsStartAddress;
	uint32 tlsEndAddress;
	uint32 regionSize_text;
	uint32 regionSize_data;
	uint32 regionSize_loaderInfo;

	uint32 patchCRC; // Cemuhook style module crc for patches.txt

	// trampoline management
	ChunkedFlatAllocator<16 * 1024> heapTrampolineArea;
	std::unordered_map<MPTR, MPTR> trampolineMap;

	// section data
	std::vector<uint8> sectionData_fileInfo;
	std::vector<uint8> sectionData_crc;

	// parsed FILEINFO
	struct 
	{
		uint32 textRegionSize; // size of region containing all text sections
		//uint32 ukn08; // base align text?
		uint32 dataRegionSize; // size of region containing all data sections
		uint32 baseAlign;
		uint32 ukn14;
		uint32 trampolineAdjustment;
		uint32 ukn4C;
		sint16 tlsModuleIndex;

		uint32 sdataBase1;
		uint32 sdataBase2;
	}fileInfo;
	// parsed CRC
	std::vector<uint32> crcTable;

	uint32 GetSectionCRC(size_t sectionIndex) const
	{
		if (sectionIndex >= crcTable.size())
			return 0;
		return crcTable[sectionIndex];
	}

	// state
	bool isLinked; // set to true if _linkModule was called on this module
	bool entrypointCalled; // set if entrypoint was called

	// allocator	
	betype<MPTR> funcAlloc;
	betype<MPTR> funcFree;

	// replaces rplData ptr
	std::span<uint8> RPLRawData;

	bool debugSectionLoadMask[128] = { false };
	bool hasError{ false };

};

struct RPLDependency
{
	char modulename[RPL_MODULE_NAME_LENGTH];
	char filepath[RPL_MODULE_PATH_LENGTH];
	bool loadAttempted;
	bool isCafeOSModule; // name is a known Cafe OS RPL
	RPLModule* rplLoaderContext; // context of loaded module, can be nullptr for HLE COS modules
	sint32 referenceCount;
	uint32 coreinitHandle; // fake handle for coreinit
	sint16 tlsModuleIndex; // tls module index assigned to this dependency
};

RPLModule* RPLLoader_FindModuleByCodeAddr(uint32 addr);
RPLModule* RPLLoader_FindModuleByDataAddr(uint32 addr);
RPLModule* RPLLoader_FindModuleByName(std::string module);
