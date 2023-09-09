#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/Filesystem/fsc.h"
#include "Cafe/OS/RPL/rpl.h"
#include "Cafe/OS/RPL/rpl_structs.h"
#include "Cafe/OS/RPL/rpl_symbol_storage.h"
#include "util/VirtualHeap/VirtualHeap.h"
#include "Cafe/HW/Espresso/Recompiler/PPCRecompiler.h"
#include "Cafe/HW/Espresso/Debugger/Debugger.h"
#include "Cafe/GraphicPack/GraphicPack2.h"
#include "util/ChunkedHeap/ChunkedHeap.h"

#include <zlib.h>

#include "util/crypto/crc32.h"
#include "config/ActiveSettings.h"
#include "Cafe/OS/libs/coreinit/coreinit_DynLoad.h"
#include "gui/guiWrapper.h"

class PPCCodeHeap : public VHeap
{
public:
	PPCCodeHeap(void* heapBase, uint32 heapSize) : VHeap(heapBase, heapSize) { };

	void* alloc(uint32 size, uint32 alignment = 4) override
	{
		return VHeap::alloc(size, alignment);
	}

	void free(void* addr) override
	{
		uint32 allocSize = getAllocationSizeFromAddr(addr);
		MPTR ppcAddr = memory_getVirtualOffsetFromPointer(addr);
		PPCRecompiler_invalidateRange(ppcAddr, ppcAddr + allocSize);
		VHeap::free(addr);
	}
};

VHeap rplLoaderHeap_workarea(nullptr, MEMORY_RPLLOADER_AREA_SIZE);
PPCCodeHeap rplLoaderHeap_lowerAreaCodeMem2(nullptr, MEMORY_CODE_TRAMPOLINE_AREA_SIZE);
PPCCodeHeap rplLoaderHeap_codeArea2(nullptr, MEMORY_CODEAREA_SIZE);

ChunkedFlatAllocator<64 * 1024> g_heapTrampolineArea;

std::vector<RPLDependency*> rplDependencyList;

RPLModule* rplModuleList[256];
sint32 rplModuleCount = 0;

bool rplLoader_applicationHasMemoryControl = false;
uint32 rplLoader_maxCodeAddress = 0; // highest used code address
uint32 rplLoader_currentTLSModuleIndex = 1; // value 0 is reserved
uint32 rplLoader_currentHandleCounter = 0x00001000;
sint16 rplLoader_currentTlsModuleIndex = 0x0001;
RPLModule* rplLoader_mainModule = nullptr;
uint32 rplLoader_sdataAddr = MPTR_NULL; // r13
uint32 rplLoader_sdata2Addr = MPTR_NULL; // r2
uint32 rplLoader_currentDataAllocatorAddr = 0x10000000;

std::map<void(*)(PPCInterpreter_t* hCPU), uint32> g_map_callableExports;

struct RPLMappingRegion
{
	MPTR baseAddress;
	uint32 endAddress;
	uint32 calcEndAddress; // used to verify endAddress
};

struct RPLRegionMappingTable
{
	RPLMappingRegion region[4];
};

#define RPL_MAPPING_REGION_DATA			0
#define RPL_MAPPING_REGION_LOADERINFO	1
#define RPL_MAPPING_REGION_TEXT			2
#define RPL_MAPPING_REGION_TEMP			3

void RPLLoader_UnloadModule(RPLModule* rpl);
void RPLLoader_RemoveDependency(const char* name);

char _ansiToLower(char c)
{
	if (c >= 'A' && c <= 'Z')
		c -= ('A' - 'a');
	return c;
}

uint8* RPLLoader_AllocateTrampolineCodeSpace(RPLModule* rplLoaderContext, sint32 size)
{	
	if (rplLoaderContext)
	{
		// allocation owned by rpl
		return (uint8*)rplLoaderContext->heapTrampolineArea.alloc(size, 4);
	}
	// allocation owned by global context
	auto result = (uint8*)g_heapTrampolineArea.alloc(size, 4);
	rplLoader_maxCodeAddress = std::max(rplLoader_maxCodeAddress, memory_getVirtualOffsetFromPointer(g_heapTrampolineArea.getCurrentBlockPtr()) + g_heapTrampolineArea.getCurrentBlockOffset());
	return result;
}

uint8* RPLLoader_AllocateTrampolineCodeSpace(sint32 size)
{
	return RPLLoader_AllocateTrampolineCodeSpace(nullptr, size);
}

MPTR RPLLoader_AllocateCodeSpace(uint32 size, uint32 alignment)
{
	cemu_assert_debug((alignment & (alignment - 1)) == 0); // alignment must be a power of 2
	MPTR codeAddr = memory_getVirtualOffsetFromPointer(rplLoaderHeap_codeArea2.alloc(size, alignment));
	rplLoader_maxCodeAddress = std::max(rplLoader_maxCodeAddress, codeAddr + size);
	PPCRecompiler_allocateRange(codeAddr, size);
	return codeAddr;
}

uint32 RPLLoader_AllocateDataSpace(RPLModule* rpl, uint32 size, uint32 alignment)
{
	if (rplLoader_applicationHasMemoryControl)
	{
		StackAllocator<uint32be> memPtr;
		*(memPtr.GetPointer()) = 0;
		PPCCoreCallback(rpl->funcAlloc.value(), size, alignment, memPtr.GetPointer());
		return (uint32)*(memPtr.GetPointer());
	}
	rplLoader_currentDataAllocatorAddr = (rplLoader_currentDataAllocatorAddr + alignment - 1) & ~(alignment - 1);
	uint32 mem = rplLoader_currentDataAllocatorAddr;
	rplLoader_currentDataAllocatorAddr += size;
	return mem;
}

void RPLLoader_FreeData(RPLModule* rpl, void* ptr)
{
	PPCCoreCallback(rpl->funcFree.value(), ptr);
}

uint32 RPLLoader_GetDataAllocatorAddr()
{
	return (rplLoader_currentDataAllocatorAddr + 0xFFF) & (~0xFFF);
}

uint32 RPLLoader_GetMaxCodeOffset()
{
	return rplLoader_maxCodeAddress;
}

#define PPCASM_OPC_R_TEMPL_SIMM(_rD, _rA, _IMM) (((_rD)<<21)|((_rA)<<16)|((_IMM)&0xFFFF))

// generates 32-bit jump. Modifies R11 and CTR
MPTR _generateTrampolineFarJump(RPLModule* rplLoaderContext, MPTR destAddr)
{
	auto itr = rplLoaderContext->trampolineMap.find(destAddr);
	if (itr != rplLoaderContext->trampolineMap.end())
		return itr->second;

	MPTR trampolineAddr = memory_getVirtualOffsetFromPointer(RPLLoader_AllocateTrampolineCodeSpace(rplLoaderContext, 4*4));
	uint32 destAddrU32 = (uint32)destAddr;
	uint32 ppcOpcode = 0;
	// ADDI R11, R0, ...
	ppcOpcode = PPCASM_OPC_R_TEMPL_SIMM(11, 0, destAddrU32 & 0xFFFF);
	ppcOpcode |= (14 << 26);
	memory_writeU32(trampolineAddr + 0x0, ppcOpcode);
	// ADDIS R11, R11, ...<<16
	ppcOpcode = PPCASM_OPC_R_TEMPL_SIMM(11, 11, ((destAddrU32 >> 16) + ((destAddrU32 >> 15) & 1)) & 0xFFFF);
	ppcOpcode |= (15 << 26);
	memory_writeU32(trampolineAddr + 0x4, ppcOpcode);
	// MTCTR r11
	memory_writeU32(trampolineAddr + 0x8, 0x7D6903A6);
	// BCTR
	memory_writeU32(trampolineAddr + 0xC, 0x4E800420);
	// if the destination is a known symbol, create a proxy (duplicate) symbol at the jump
	rplSymbolStorage_createJumpProxySymbol(trampolineAddr, destAddr);
	rplLoaderContext->trampolineMap.emplace(destAddr, trampolineAddr);
	return trampolineAddr;
}

void* RPLLoader_AllocWorkarea(uint32 size, uint32 alignment, uint32* allocSize)
{
	size = (size + 31)&~31;
	*allocSize = size;
	void* allocAddr = rplLoaderHeap_workarea.alloc(size, alignment);
	cemu_assert(allocAddr != nullptr);
	memset(allocAddr, 0, size);
	return allocAddr;
}

void RPLLoader_FreeWorkarea(void* allocAddr)
{
	rplLoaderHeap_workarea.free(allocAddr);
}

bool RPLLoader_CheckBounds(RPLModule* rplLoaderContext, uint32 offset, uint32 size)
{
	if ((offset + size) > rplLoaderContext->RPLRawData.size_bytes())
		return false;
	return true;
}

bool RPLLoader_ProcessHeaders(std::string_view moduleName, uint8* rplData, uint32 rplSize, RPLModule** rplLoaderContextOut)
{
	rplHeaderNew_t* rplHeader = (rplHeaderNew_t*)rplData;
	*rplLoaderContextOut = nullptr;
	if (rplHeader->version04 != 0x01)
		return false;
	if (rplHeader->ukn05 != 0x02)
		return false;
	if (rplHeader->magic2_0 != 0xCA)
		return false;
	if (rplHeader->magic2_1 != 0xFE)
		return false;
	if (rplHeader->ukn06 > 1)
		return false;
	if (rplHeader->ukn12 != 0x14)
		return false;
	if (rplHeader->ukn14 != 0x01)
		return false;
	if (rplHeader->sectionTableEntryCount < 2)
		return false; // RPL must end with two sections: CRCS + FILEINFO
	// setup RPL info struct
	RPLModule* rplLoaderContext = new RPLModule();
	rplLoaderContext->RPLRawData = std::span<uint8>(rplData, rplSize);
	rplLoaderContext->heapTrampolineArea.setBaseAllocator(&rplLoaderHeap_lowerAreaCodeMem2);
	// load section table
	if ((uint32)rplHeader->sectionTableEntrySize != sizeof(rplSectionEntryNew_t))
		assert_dbg();
	sint32 sectionCount = (sint32)rplHeader->sectionTableEntryCount;
	sint32 sectionTableSize = (sint32)rplHeader->sectionTableEntrySize * sectionCount;
	rplLoaderContext->sectionTablePtr = (rplSectionEntryNew_t*)malloc(sectionTableSize);
	memcpy(rplLoaderContext->sectionTablePtr, rplData + (uint32)(rplHeader->sectionTableOffset), sectionTableSize);
	// copy rpl header
	memcpy(&rplLoaderContext->rplHeader, rplHeader, sizeof(rplHeaderNew_t));
	// verify that section n-1 is FILEINFO
	rplSectionEntryNew_t* fileinfoSection = rplLoaderContext->sectionTablePtr + ((uint32)rplLoaderContext->rplHeader.sectionTableEntryCount - 1);
	if (fileinfoSection->fileOffset == 0 || (uint32)fileinfoSection->fileOffset >= rplSize || (uint32)fileinfoSection->type != SHT_RPL_FILEINFO)
	{
		cemuLog_logDebug(LogType::Force, "RPLLoader: Last section not FILEINFO");
	}
	// verify that section n-2 is CRCs
	rplSectionEntryNew_t* crcSection = rplLoaderContext->sectionTablePtr + ((uint32)rplLoaderContext->rplHeader.sectionTableEntryCount - 2);
	if (crcSection->fileOffset == 0 || (uint32)crcSection->fileOffset >= rplSize || (uint32)crcSection->type != SHT_RPL_CRCS)
	{
		cemuLog_logDebug(LogType::Force, "RPLLoader: The section before FILEINFO must be CRCs");
	}
	// load FILEINFO section
	if (fileinfoSection->sectionSize < sizeof(RPLFileInfoData))
	{
		cemuLog_log(LogType::Force, "RPLLoader: FILEINFO section size is below expected size");
		delete rplLoaderContext;
		return false;
	}

	// read RPL mapping info
	uint8* fileInfoRawPtr = (uint8*)(rplData + fileinfoSection->fileOffset);
	if (((uint64)fileinfoSection->fileOffset+fileinfoSection->sectionSize) > (uint64)rplSize)
	{
		cemuLog_log(LogType::Force, "RPLLoader: FILEINFO section outside of RPL file bounds");
		return false;
	}
	rplLoaderContext->sectionData_fileInfo.resize(fileinfoSection->sectionSize);
	memcpy(rplLoaderContext->sectionData_fileInfo.data(), fileInfoRawPtr, rplLoaderContext->sectionData_fileInfo.size());

	RPLFileInfoData* fileInfoPtr = (RPLFileInfoData*)rplLoaderContext->sectionData_fileInfo.data();
	if (fileInfoPtr->fileInfoMagic != 0xCAFE0402)
	{
		cemuLog_log(LogType::Force, "RPLLoader: Invalid FILEINFO magic");
		return false;
	}

	// process FILEINFO
	rplLoaderContext->fileInfo.textRegionSize = fileInfoPtr->textRegionSize;
	rplLoaderContext->fileInfo.dataRegionSize = fileInfoPtr->dataRegionSize;
	rplLoaderContext->fileInfo.baseAlign = fileInfoPtr->baseAlign;
	rplLoaderContext->fileInfo.ukn14 = fileInfoPtr->ukn14;
	rplLoaderContext->fileInfo.trampolineAdjustment = fileInfoPtr->trampolineAdjustment;
	rplLoaderContext->fileInfo.ukn4C = fileInfoPtr->ukn4C;
	rplLoaderContext->fileInfo.tlsModuleIndex = fileInfoPtr->tlsModuleIndex;
	rplLoaderContext->fileInfo.sdataBase1 = fileInfoPtr->sdataBase1;
	rplLoaderContext->fileInfo.sdataBase2 = fileInfoPtr->sdataBase2;

	// init section address table
	rplLoaderContext->sectionAddressTable2.resize(sectionCount);
	// init modulename
	rplLoaderContext->moduleName2.assign(moduleName);
	// convert modulename to lower-case
	for(auto& c : rplLoaderContext->moduleName2)
		c = _ansiToLower(c);

	// load CRC section
	uint32 crcTableExpectedSize = sectionCount * sizeof(uint32be);
	if (!RPLLoader_CheckBounds(rplLoaderContext, crcSection->fileOffset, crcTableExpectedSize))
	{
		cemuLog_log(LogType::Force, "RPLLoader: CRC section outside of RPL file bounds");
		crcSection->sectionSize = 0;
	}
	else if (crcSection->sectionSize < crcTableExpectedSize)
	{
		cemuLog_log(LogType::Force, "RPLLoader: CRC section size (0x{:x}) less than required (0x{:x})", (uint32)crcSection->sectionSize, crcTableExpectedSize);
	}
	else if (crcSection->sectionSize != crcTableExpectedSize)
	{
		cemuLog_log(LogType::Force, "RPLLoader: CRC section size (0x{:x}) does not match expected size (0x{:x})", (uint32)crcSection->sectionSize, crcTableExpectedSize);
	}

	uint32 crcActualSectionCount = crcSection->sectionSize / sizeof(uint32); // how many CRCs are actually stored

	rplLoaderContext->crcTable.resize(sectionCount);
	if (crcActualSectionCount > 0)
	{
		uint32be* crcTableData = (uint32be*)(rplData + crcSection->fileOffset);
		for (uint32 i = 0; i < crcActualSectionCount; i++)
			rplLoaderContext->crcTable[i] = crcTableData[i];
	}

	// verify CRC of FILEINFO section
	uint32 crcCalcFileinfo = crc32_calc(0, rplLoaderContext->sectionData_fileInfo.data(), rplLoaderContext->sectionData_fileInfo.size());
	uint32 crcFileinfo = rplLoaderContext->GetSectionCRC(sectionCount - 1);
	if (crcCalcFileinfo != crcFileinfo)
	{
		cemuLog_log(LogType::Force, "RPLLoader: FILEINFO section has CRC mismatch - Calculated: {:08x} Actual: {:08x}", crcCalcFileinfo, crcFileinfo);
	}

	rplLoaderContext->sectionAddressTable2[sectionCount - 1].ptr = rplLoaderContext->sectionData_fileInfo.data();
	rplLoaderContext->sectionAddressTable2[sectionCount - 2].ptr = nullptr;// rplLoaderContext->crcTablePtr;

	// set output
	*rplLoaderContextOut = rplLoaderContext;
	return true;
}

class RPLUncompressedSection
{
public:
	std::vector<uint8> sectionData;
};

rplSectionEntryNew_t* RPLLoader_GetSection(RPLModule* rplLoaderContext, sint32 sectionIndex)
{
	sint32 sectionCount = rplLoaderContext->rplHeader.sectionTableEntryCount;
	if (sectionIndex < 0 || sectionIndex >= sectionCount)
	{
		cemuLog_log(LogType::Force, "RPLLoader: Section index out of bounds");
		rplLoaderContext->hasError = true;
		return nullptr;
	}
	rplSectionEntryNew_t* section = rplLoaderContext->sectionTablePtr + sectionIndex;
	return section;
}

RPLUncompressedSection* RPLLoader_LoadUncompressedSection(RPLModule* rplLoaderContext, sint32 sectionIndex)
{
	const rplSectionEntryNew_t* section = RPLLoader_GetSection(rplLoaderContext, sectionIndex);
	if (section == nullptr)
		return nullptr;

	RPLUncompressedSection* uSection = new RPLUncompressedSection();

	if ((uint32)section->type == 0x8)
	{
		uSection->sectionData.resize(section->sectionSize);
		std::fill(uSection->sectionData.begin(), uSection->sectionData.end(), 0);
		return uSection;
	}

	// check if raw size does not exceed bounds of rpl
	if (!RPLLoader_CheckBounds(rplLoaderContext, section->fileOffset, section->sectionSize))
	{
		// BSS
		cemuLog_log(LogType::Force, "RPLLoader: Raw data for section {} exceeds bounds of RPL file", sectionIndex);
		rplLoaderContext->hasError = true;
		delete uSection;
		return nullptr;
	}

	uint32 sectionFlags = section->flags;
	if ((sectionFlags & SHF_RPL_COMPRESSED) != 0)
	{
		// decompress
		if (!RPLLoader_CheckBounds(rplLoaderContext, section->fileOffset, sizeof(uint32be)) )
		{
			cemuLog_log(LogType::Force, "RPLLoader: Uncompressed data of section {} is too large", sectionIndex);
			rplLoaderContext->hasError = true;
			delete uSection;
			return nullptr;
		}
		uint32 uncompressedSize = *(uint32be*)(rplLoaderContext->RPLRawData.data() + (uint32)section->fileOffset);
		if (uncompressedSize >= 1*1024*1024*1024) // sections bigger than 1GB not allowed
		{
			cemuLog_log(LogType::Force, "RPLLoader: Uncompressed data of section {} is too large", sectionIndex);
			rplLoaderContext->hasError = true;
			delete uSection;
			return nullptr;
		}
		int ret;
		z_stream strm;
		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		ret = inflateInit(&strm);
		if (ret == Z_OK)
		{
			strm.avail_in = (uint32)section->sectionSize - 4;
			strm.next_in = rplLoaderContext->RPLRawData.data() + (uint32)section->fileOffset + 4;
			strm.avail_out = uncompressedSize;
			uSection->sectionData.resize(uncompressedSize);
			strm.next_out = uSection->sectionData.data();
			ret = inflate(&strm, Z_FULL_FLUSH);
			inflateEnd(&strm);
			if ((ret != Z_OK && ret != Z_STREAM_END) || strm.avail_in != 0 || strm.avail_out != 0)
			{
				cemuLog_log(LogType::Force, "RPLLoader: Error while inflating data for section {}", sectionIndex);
				rplLoaderContext->hasError = true;
				delete uSection;
				return nullptr;
			}
		}
	}
	else
	{
		// no decompression
		uSection->sectionData.resize(section->sectionSize);
		const uint8* sectionDataBegin = rplLoaderContext->RPLRawData.data() + (uint32)section->fileOffset;
		std::copy(sectionDataBegin, sectionDataBegin + section->sectionSize, uSection->sectionData.data());
	}
	return uSection;
}

bool RPLLoader_LoadSingleSection(RPLModule* rplLoaderContext, sint32 sectionIndex, RPLMappingRegion* regionMappingInfo, MPTR mappedAddress)
{
	rplSectionEntryNew_t* section = RPLLoader_GetSection(rplLoaderContext, sectionIndex);
	if (section == nullptr)
		return false;

	uint32 mappingOffset = (uint32)section->virtualAddress - (uint32)regionMappingInfo->baseAddress;
	if (mappingOffset >= 0x10000000)
		cemuLog_logDebug(LogType::Force, "Suspicious section mapping offset: 0x{:08x}", mappingOffset);
	uint32 sectionAddress = mappedAddress + mappingOffset;

	rplLoaderContext->sectionAddressTable2[sectionIndex].ptr = memory_getPointerFromVirtualOffset(sectionAddress);

	cemu_assert(rplLoaderContext->debugSectionLoadMask[sectionIndex] == false);
	rplLoaderContext->debugSectionLoadMask[sectionIndex] = true;

	// extract section
	RPLUncompressedSection* uncompressedSection = RPLLoader_LoadUncompressedSection(rplLoaderContext, sectionIndex);
	if (uncompressedSection == nullptr)
	{
		rplLoaderContext->hasError = true;
		return false;
	}

	// copy to mapped address
	if(section->virtualAddress < regionMappingInfo->baseAddress || (section->virtualAddress + uncompressedSection->sectionData.size()) > regionMappingInfo->endAddress)
		cemuLog_log(LogType::Force, "RPLLoader: Section {} (0x{:08x} to 0x{:08x}) is not fully contained in it's bounding region (0x{:08x} to 0x{:08x})", sectionIndex, section->virtualAddress, section->virtualAddress + uncompressedSection->sectionData.size(), regionMappingInfo->baseAddress, regionMappingInfo->endAddress);
	uint8* sectionAddressPtr = memory_getPointerFromVirtualOffset(sectionAddress);
	std::copy(uncompressedSection->sectionData.begin(), uncompressedSection->sectionData.end(), sectionAddressPtr);

	// update size in section (todo - use separate field)
	if (uncompressedSection->sectionData.size() < section->sectionSize)
		cemuLog_log(LogType::Force, "RPLLoader: Section {} uncompresses to {} bytes but sectionSize is {}", sectionIndex, uncompressedSection->sectionData.size(), (uint32)section->sectionSize);

	section->sectionSize = uncompressedSection->sectionData.size();

	delete uncompressedSection;
	return true;
}

bool RPLLoader_LoadSections(sint32 aProcId, RPLModule* rplLoaderContext)
{
	RPLRegionMappingTable regionMappingTable;
	memset(&regionMappingTable, 0, sizeof(RPLRegionMappingTable));
	regionMappingTable.region[0].baseAddress = 0xFFFFFFFF;
	regionMappingTable.region[1].baseAddress = 0xFFFFFFFF;
	regionMappingTable.region[2].baseAddress = 0xFFFFFFFF;
	regionMappingTable.region[3].baseAddress = 0xFFFFFFFF;
	for (sint32 i = 0; i < (sint32)rplLoaderContext->rplHeader.sectionTableEntryCount; i++)
	{
		rplSectionEntryNew_t* section = rplLoaderContext->sectionTablePtr + i;
		uint32 sectionType = section->type;
		uint32 sectionFlags = section->flags;
		uint32 sectionVirtualAddr = section->virtualAddress;
		uint32 sectionFileOffset = section->fileOffset;
		uint32 sectionSize = section->sectionSize;
		if(sectionSize == 0)
			continue;
		if (sectionType == SHT_RPL_CRCS)
			continue;
		if (sectionType == SHT_RPL_FILEINFO)
			continue;
		//if (sectionType == SHT_RPL_IMPORTS) -> The official loader seems to skip these, leading to incorrect boundary calculations
		//	continue;
		if ((sectionFlags & 2) == 0)
		{
			uint32 endFileOffset = sectionFileOffset + sectionSize;
			regionMappingTable.region[RPL_MAPPING_REGION_TEMP].baseAddress = std::min(regionMappingTable.region[RPL_MAPPING_REGION_TEMP].baseAddress, sectionFileOffset);
			regionMappingTable.region[RPL_MAPPING_REGION_TEMP].endAddress = std::max(regionMappingTable.region[RPL_MAPPING_REGION_TEMP].endAddress, endFileOffset);
			continue;
		}
		if ((sectionFlags & 4) != 0 && sectionType != SHT_RPL_EXPORTS && sectionType != SHT_RPL_IMPORTS)
		{
			regionMappingTable.region[RPL_MAPPING_REGION_TEXT].baseAddress = std::min(regionMappingTable.region[RPL_MAPPING_REGION_TEXT].baseAddress, sectionVirtualAddr);
			continue;
		}
		if ((sectionFlags & 1) != 0)
		{
			regionMappingTable.region[RPL_MAPPING_REGION_DATA].baseAddress = std::min(regionMappingTable.region[RPL_MAPPING_REGION_DATA].baseAddress, sectionVirtualAddr);
			continue;
		}
		else
		{ 
			regionMappingTable.region[RPL_MAPPING_REGION_LOADERINFO].baseAddress = std::min(regionMappingTable.region[RPL_MAPPING_REGION_LOADERINFO].baseAddress, sectionVirtualAddr);
			continue;

		}
	}
	for (sint32 i = 0; i < 4; i++)
	{
		if (regionMappingTable.region[i].baseAddress == 0xFFFFFFFF)
			regionMappingTable.region[i].baseAddress = 0;
	}
	regionMappingTable.region[RPL_MAPPING_REGION_TEXT].endAddress = (regionMappingTable.region[RPL_MAPPING_REGION_TEXT].baseAddress + rplLoaderContext->fileInfo.textRegionSize) - rplLoaderContext->fileInfo.trampolineAdjustment;
	regionMappingTable.region[RPL_MAPPING_REGION_DATA].endAddress = regionMappingTable.region[RPL_MAPPING_REGION_DATA].baseAddress + rplLoaderContext->fileInfo.dataRegionSize;
	regionMappingTable.region[RPL_MAPPING_REGION_LOADERINFO].endAddress = (regionMappingTable.region[RPL_MAPPING_REGION_LOADERINFO].baseAddress + rplLoaderContext->fileInfo.ukn14) - rplLoaderContext->fileInfo.ukn4C;

	// calculate region size
	uint32 regionDataSize = regionMappingTable.region[RPL_MAPPING_REGION_DATA].endAddress - regionMappingTable.region[RPL_MAPPING_REGION_DATA].baseAddress;
	uint32 regionLoaderinfoSize = regionMappingTable.region[RPL_MAPPING_REGION_LOADERINFO].endAddress - regionMappingTable.region[RPL_MAPPING_REGION_LOADERINFO].baseAddress;
	uint32 regionTextSize = regionMappingTable.region[RPL_MAPPING_REGION_TEXT].endAddress - regionMappingTable.region[RPL_MAPPING_REGION_TEXT].baseAddress;

	rplLoaderContext->regionMappingBase_data = RPLLoader_AllocateDataSpace(rplLoaderContext, regionDataSize, 0x1000);
	rplLoaderContext->regionMappingBase_loaderInfo = RPLLoader_AllocateDataSpace(rplLoaderContext, regionLoaderinfoSize, 0x1000);
	rplLoaderContext->regionMappingBase_text = rplLoaderHeap_codeArea2.alloc(regionTextSize + 0x1000, 0x1000);
	rplLoader_maxCodeAddress = std::max(rplLoader_maxCodeAddress, rplLoaderContext->regionMappingBase_text.GetMPTR() + regionTextSize + 0x1000);
	PPCRecompiler_allocateRange(rplLoaderContext->regionMappingBase_text.GetMPTR(), regionTextSize + 0x1000);

	// workaround for DKC Tropical Freeze
	if (boost::iequals(rplLoaderContext->moduleName2, "rs10_production"))
	{
		// allocate additional 12MB of unused data to get below a size of 0x3E200000 for the main ExpHeap
		// otherwise the game will assume it's running on a Devkit unit with 2GB of RAM and subtract 1GB from available space
		RPLLoader_AllocateDataSpace(rplLoaderContext, 12*1024*1024, 0x1000);
	}
	// set region sizes
	rplLoaderContext->regionSize_data = regionDataSize;
	rplLoaderContext->regionSize_loaderInfo = regionLoaderinfoSize;
	rplLoaderContext->regionSize_text = regionTextSize;

	// load data sections
	for (sint32 i = 0; i < (sint32)rplLoaderContext->rplHeader.sectionTableEntryCount; i++)
	{
		rplSectionEntryNew_t* section = rplLoaderContext->sectionTablePtr + i;
		uint32 sectionType = section->type;
		uint32 sectionFlags = section->flags;
		if (section->sectionSize == 0)
			continue;
		if( rplLoaderContext->sectionAddressTable2[i].ptr != nullptr )
			continue;
		if ((sectionFlags & 2) == 0)
			continue;
		if ((sectionFlags & 1) == 0)
			continue;

		RPLLoader_LoadSingleSection(rplLoaderContext, i, regionMappingTable.region + RPL_MAPPING_REGION_DATA, rplLoaderContext->regionMappingBase_data);
	}
	// load loaderinfo sections
	for (sint32 i = 0; i < (sint32)rplLoaderContext->rplHeader.sectionTableEntryCount; i++)
	{
		rplSectionEntryNew_t* section = rplLoaderContext->sectionTablePtr + i;
		uint32 sectionType = section->type;
		uint32 sectionFlags = section->flags;
		if (section->sectionSize == 0)
			continue;
		if (rplLoaderContext->sectionAddressTable2[i].ptr != nullptr)
			continue;
		if ((sectionFlags & 2) == 0)
			continue;
		if(sectionType != SHT_RPL_EXPORTS && sectionType != SHT_RPL_IMPORTS && (sectionFlags&5) != 0 )
			continue;
		bool readRaw = false;

		RPLLoader_LoadSingleSection(rplLoaderContext, i, regionMappingTable.region + RPL_MAPPING_REGION_LOADERINFO, rplLoaderContext->regionMappingBase_loaderInfo);

		if (sectionType == SHT_RPL_EXPORTS)
		{
			uint8* sectionAddress = (uint8*)rplLoaderContext->sectionAddressTable2[i].ptr;
			if ((sectionFlags & 4) != 0)
			{
				rplLoaderContext->exportFCount = *(uint32be*)(sectionAddress + 0);
				rplLoaderContext->exportFDataPtr = (rplExportTableEntry_t*)(sectionAddress + 8);
			}
			else
			{
				rplLoaderContext->exportDCount = *(uint32be*)(sectionAddress + 0);
				rplLoaderContext->exportDDataPtr = (rplExportTableEntry_t*)(sectionAddress + 8);
			}
		}
	}
	// load text sections
	uint32 textSectionMappedBase = rplLoaderContext->regionMappingBase_text.GetMPTR() + (uint32)rplLoaderContext->fileInfo.trampolineAdjustment; // leave some space for trampolines before the code section begins
	for (sint32 i = 0; i < (sint32)rplLoaderContext->rplHeader.sectionTableEntryCount; i++)
	{
		rplSectionEntryNew_t* section = rplLoaderContext->sectionTablePtr + i;
		uint32 sectionType = section->type;
		uint32 sectionFlags = section->flags;
		if( section->sectionSize == 0 )
			continue;
		if (rplLoaderContext->sectionAddressTable2[i].ptr != nullptr)
			continue;
		if ((sectionFlags & 2) == 0)
			continue;
		if ((sectionFlags & 4) == 0)
			continue;
		if( sectionType == SHT_RPL_EXPORTS)
			continue;

		if (section->type == 0x8)
		{
			cemuLog_log(LogType::Force, "RPLLoader: Unsupported text section type 0x8");
			cemu_assert_debug(false);
		}

		RPLLoader_LoadSingleSection(rplLoaderContext, i, regionMappingTable.region + RPL_MAPPING_REGION_TEXT, textSectionMappedBase);
	}
	// load temp region sections
	uint32 tempRegionSize = regionMappingTable.region[RPL_MAPPING_REGION_TEMP].endAddress - regionMappingTable.region[RPL_MAPPING_REGION_TEMP].baseAddress;
	uint8* tempRegionPtr;
	uint32 tempRegionAllocSize = 0;
	tempRegionPtr = (uint8*)RPLLoader_AllocWorkarea(tempRegionSize, 0x20, &tempRegionAllocSize);
	rplLoaderContext->tempRegionPtr = tempRegionPtr;
	rplLoaderContext->tempRegionAllocSize = tempRegionAllocSize;
	memcpy(tempRegionPtr, rplLoaderContext->RPLRawData.data()+regionMappingTable.region[RPL_MAPPING_REGION_TEMP].baseAddress, tempRegionSize);
	// load temp region sections
	for (sint32 i = 0; i < (sint32)rplLoaderContext->rplHeader.sectionTableEntryCount; i++)
	{
		rplSectionEntryNew_t* section = rplLoaderContext->sectionTablePtr + i;
		uint32 sectionType = section->type;
		uint32 sectionFlags = section->flags;
		if (section->sectionSize == 0)
			continue;
		if (rplLoaderContext->sectionAddressTable2[i].ptr != nullptr)
			continue;
		if (sectionType == SHT_RPL_FILEINFO || sectionType == SHT_RPL_CRCS)
			continue;
		// calculate offset within temp section
		uint32 sectionFileOffset = section->fileOffset;
		uint32 sectionSize = section->sectionSize;
		cemu_assert_debug(sectionFileOffset >= regionMappingTable.region[RPL_MAPPING_REGION_TEMP].baseAddress);
		cemu_assert_debug((sectionFileOffset + sectionSize) <= regionMappingTable.region[RPL_MAPPING_REGION_TEMP].endAddress);
		rplLoaderContext->sectionAddressTable2[i].ptr = (tempRegionPtr + (sectionFileOffset - regionMappingTable.region[RPL_MAPPING_REGION_TEMP].baseAddress));

		uint32 sectionEndAddress = sectionFileOffset + sectionSize;
		regionMappingTable.region[RPL_MAPPING_REGION_TEMP].calcEndAddress = std::max(regionMappingTable.region[RPL_MAPPING_REGION_TEMP].calcEndAddress, sectionEndAddress);
	}
	// todo: Verify calcEndAddress<=endAddress for each region

	// dump loaded sections
	/*
	for (sint32 i = 0; i < (sint32)rplLoaderContext->rplHeader.sectionTableEntryCount; i++)
	{
		rplSectionEntryNew_t* section = rplLoaderContext->sectionTablePtr + i;
		uint32 sectionType = section->type;
		uint32 sectionFlags = section->flags;
		if (section->sectionSize == 0)
			continue;
		if (rplLoaderContext->sectionAddressTable2[i].ptr == nullptr)
			continue;
		FileStream* fs = FileStream::createFile2(fmt::format("dump/rpl_sections/{}_{:08x}_type{:08x}.bin", i, (uint32)section->virtualAddress, (uint32)sectionType));
		fs->writeData(rplLoaderContext->sectionAddressTable2[i].ptr, section->sectionSize);
		delete fs;
	}
	*/
	return true;
}

struct RPLFileSymtabEntry
{
	/* +0x0 */ uint32be ukn00;
	/* +0x4 */ uint32be symbolAddress;
	/* +0x8 */ uint32be ukn08;
	/* +0xC */ uint8    info;
	/* +0xD */ uint8    ukn0D;
	/* +0xE */ uint16be sectionIndex;
};

struct RPLSharedImportTracking
{
	RPLModule* rplLoaderContext; // rpl loader context of module with exports
	rplSectionEntryNew_t* exportSection; // export section
	char modulename[RPL_MODULE_NAME_LENGTH];
};

static_assert(sizeof(RPLFileSymtabEntry) == 0x10, "rplSymtabEntry_t has invalid size");

typedef struct
{
	uint64 hash1;
	uint64 hash2;
	uint32 address;
}mappedFunctionImport_t;

std::vector<mappedFunctionImport_t> list_mappedFunctionImports = std::vector<mappedFunctionImport_t>();

void _calculateMappedImportNameHash(const char* rplName, const char* funcName, uint64* h1Out, uint64* h2Out)
{
	uint64 h1 = 0;
	uint64 h2 = 0;
	// rplName
	while (*rplName)
	{
		uint64 v = (uint64)*rplName;
		h1 += v;
		h2 ^= v;
		h1 = std::rotl<uint64>(h1, 3);
		h2 = std::rotl<uint64>(h2, 7);
		rplName++;
	}
	// funcName
	while (*funcName)
	{
		uint64 v = (uint64)*funcName;
		h1 += v;
		h2 ^= v;
		h1 = std::rotl<uint64>(h1, 3);
		h2 = std::rotl<uint64>(h2, 7);
		funcName++;
	}
	*h1Out = h1;
	*h2Out = h2;
}

uint32 RPLLoader_MakePPCCallable(void(*ppcCallableExport)(PPCInterpreter_t* hCPU))
{
	auto it = g_map_callableExports.find(ppcCallableExport);
	if (it != g_map_callableExports.end())
		return it->second;
	// get HLE function index
	sint32 functionIndex = PPCInterpreter_registerHLECall(ppcCallableExport);
	MPTR codeAddr = memory_getVirtualOffsetFromPointer(RPLLoader_AllocateTrampolineCodeSpace(4));
	uint32 opcode = (1 << 26) | functionIndex;
	memory_write<uint32>(codeAddr, opcode);
	g_map_callableExports[ppcCallableExport] = codeAddr;
	return codeAddr;
}

uint32 rpl_mapHLEImport(RPLModule* rplLoaderContext, const char* rplName, const char* funcName, bool functionMustExist)
{
	// calculate import name hash
	uint64 mappedImportHash1;
	uint64 mappedImportHash2;
	_calculateMappedImportNameHash(rplName, funcName, &mappedImportHash1, &mappedImportHash2);
	// find already mapped name
	for (auto& importItr : list_mappedFunctionImports)
	{
		if (importItr.hash1 == mappedImportHash1 && importItr.hash2 == mappedImportHash2)
		{
			return importItr.address;
		}
	}
	// copy lib file name and cut off .rpl from libName if present
	char libName[512];
	strcpy_s(libName, rplName);
	for (sint32 i = 0; i < sizeof(libName); i++)
	{
		if (libName[i] == '\0')
			break;
		if (libName[i] == '.')
		{
			libName[i] = '\0';
			break;
		}
	}
	// find import in list of known exports
	sint32 functionIndex = osLib_getFunctionIndex(libName, funcName);
	if (functionIndex >= 0)
	{
		MPTR codeAddr = memory_getVirtualOffsetFromPointer(RPLLoader_AllocateTrampolineCodeSpace(4));
		uint32 opcode = (1 << 26) | functionIndex;
		memory_write<uint32>(codeAddr, opcode);
		// register mapped import
		mappedFunctionImport_t newImport;
		newImport.hash1 = mappedImportHash1;
		newImport.hash2 = mappedImportHash2;
		newImport.address = codeAddr;
		list_mappedFunctionImports.push_back(newImport);
		// remember in symbol storage for debugger
		rplSymbolStorage_store(libName, funcName, codeAddr);
		return codeAddr;
	}
	else
	{
		if (functionMustExist == false)
			return MPTR_NULL;
	}
	// create code for unsupported import
	uint32 codeStart = memory_getVirtualOffsetFromPointer(RPLLoader_AllocateTrampolineCodeSpace(256));
	uint32 currentAddress = codeStart;
	uint32 opcode = (1 << 26) | (0xFFD0); // opcode for HLE: Unsupported import
	memory_write<uint32>(codeStart + 0, opcode);
	memory_write<uint32>(codeStart + 4, 0x4E800020);
	currentAddress += 8;
	// write name of lib/function
	sint32 libNameLength = std::min(128, (sint32)strlen(libName));
	sint32 funcNameLength = std::min(128, (sint32)strlen(funcName));
	memcpy(memory_getPointerFromVirtualOffset(currentAddress), libName, libNameLength);
	currentAddress += libNameLength;
	memory_writeU8(currentAddress, '.');
	currentAddress++;
	memcpy(memory_getPointerFromVirtualOffset(currentAddress), funcName, funcNameLength);
	currentAddress += funcNameLength;
	memory_writeU8(currentAddress, '\0');
	currentAddress++;
	// align address to 4 byte boundary
	currentAddress = (currentAddress + 3)&~3;
	// register mapped import
	mappedFunctionImport_t newImport;
	newImport.hash1 = mappedImportHash1;
	newImport.hash2 = mappedImportHash2;
	newImport.address = codeStart;
	list_mappedFunctionImports.push_back(newImport);
	// remember in symbol storage for debugger
	rplSymbolStorage_store(libName, funcName, codeStart);
	// return address of code start
	return codeStart;
}

MPTR RPLLoader_FindRPLExport(RPLModule* rplLoaderContext, const char* symbolName, bool isData)
{
	if (isData)
	{
		cemu_assert_debug(false);
		// todo - look in DDataPtr
	}
	if (rplLoaderContext->exportFDataPtr)
	{
		char* exportNameData = (char*)((uint8*)rplLoaderContext->exportFDataPtr - 8);
		for (uint32 f = 0; f < rplLoaderContext->exportFCount; f++)
		{
			char* name = exportNameData + (uint32)rplLoaderContext->exportFDataPtr[f].nameOffset;
			if (strcmp(name, symbolName) == 0)
			{
				return (uint32)rplLoaderContext->exportFDataPtr[f].virtualOffset;
			}
		}
	}
	return MPTR_NULL;
}

MPTR _findHLEExport(RPLModule* rplLoaderContext, RPLSharedImportTracking* sharedImportTrackingEntry, char* libname, char* symbolName, bool isData)
{
	if (isData)
	{
		// data export
		MPTR weakExportAddr = osLib_getPointer(libname, symbolName);
		if (weakExportAddr != 0xFFFFFFFF)
			return weakExportAddr;
		cemuLog_logDebug(LogType::Force, "Unsupported data export ({}): {}.{}", rplLoaderContext->moduleName2, libname, symbolName);
		return MPTR_NULL;
	}
	else
	{
		// try to find HLE/placeholder export
		MPTR mappedFunctionAddr = rpl_mapHLEImport(rplLoaderContext, libname, symbolName, true);
		if (mappedFunctionAddr == MPTR_NULL)
			cemu_assert_debug(false);
		return mappedFunctionAddr;
	}
}

uint32 RPLLoader_FindModuleExport(RPLModule* rplLoaderContext, bool isData, const char* exportName)
{
	if (isData == false)
	{
		// find function export
		char* exportNameData = (char*)((uint8*)rplLoaderContext->exportFDataPtr - 8);
		for (uint32 f = 0; f < rplLoaderContext->exportFCount; f++)
		{
			char* name = exportNameData + (uint32)rplLoaderContext->exportFDataPtr[f].nameOffset;
			if (strcmp(name, exportName) == 0)
			{
				uint32 exportAddress = rplLoaderContext->exportFDataPtr[f].virtualOffset;
				return exportAddress;
			}
		}
	}
	else
	{
		// find data export
		char* exportNameData = (char*)((uint8*)rplLoaderContext->exportDDataPtr - 8);
		for (uint32 f = 0; f < rplLoaderContext->exportDCount; f++)
		{
			char* name = exportNameData + (uint32)rplLoaderContext->exportDDataPtr[f].nameOffset;
			if (strcmp(name, exportName) == 0)
			{
				uint32 exportAddress = rplLoaderContext->exportDDataPtr[f].virtualOffset;
				return exportAddress;
			}
		}
	}
	return 0;
}

bool RPLLoader_FixImportSymbols(RPLModule* rplLoaderContext, sint32 symtabSectionIndex, rplSectionEntryNew_t* symTabSection, std::span<RPLSharedImportTracking> sharedImportTracking, uint32 linkMode)
{
	uint32 sectionSize = symTabSection->sectionSize;
	uint32 symbolEntrySize = symTabSection->ukn24;
	if (symbolEntrySize == 0)
		symbolEntrySize = 0x10;
	cemu_assert(symbolEntrySize == 0x10);
	cemu_assert((sectionSize % symbolEntrySize) == 0);
	uint32 symbolCount = sectionSize / symbolEntrySize;
	cemu_assert(symbolCount >= 2);

	uint16 sectionCount = rplLoaderContext->rplHeader.sectionTableEntryCount;
	uint8* symtabData = (uint8*)rplLoaderContext->sectionAddressTable2[symtabSectionIndex].ptr;

	uint32 strtabSectionIndex = symTabSection->symtabSectionIndex;
	uint8* strtabData = (uint8*)rplLoaderContext->sectionAddressTable2[strtabSectionIndex].ptr;
	uint32 strtabSize = rplLoaderContext->sectionTablePtr[strtabSectionIndex].sectionSize;

	for (uint32 i = 0; i < symbolCount; i++)
	{
		RPLFileSymtabEntry* sym = (RPLFileSymtabEntry*)(symtabData + i*symbolEntrySize);
		uint16 symSectionIndex = sym->sectionIndex;
		if (symSectionIndex == 0 || symSectionIndex >= sectionCount)
			continue;
		void* symbolSectionAddress = rplLoaderContext->sectionAddressTable2[symSectionIndex].ptr;
		if (symbolSectionAddress == nullptr)
		{
			sym->symbolAddress = 0xCD000000 | i;
			continue;
		}
		rplSectionEntryNew_t* symbolSection = rplLoaderContext->sectionTablePtr + symSectionIndex;
		uint32 symbolOffset = sym->symbolAddress - symbolSection->virtualAddress;
		
		if (symSectionIndex >= sharedImportTracking.size())
		{
			cemuLog_log(LogType::Force, "RPL-Loader: Symbol {} references invalid section", i);
		}
		else if (sharedImportTracking[symSectionIndex].rplLoaderContext != nullptr)
		{
			if (linkMode == 0)
			{
				continue; // ?
			}
			if (symbolOffset < 8)
			{
				cemu_assert(symbolSectionAddress >= memory_base && symbolSectionAddress <= (memory_base + 0x100000000ULL));
				uint32 symbolSectionMPTR = memory_getVirtualOffsetFromPointer(symbolSectionAddress);
				uint32 symbolRelativeAddress = (uint32)sym->symbolAddress - (uint32)symbolSection->virtualAddress;

				sym->symbolAddress = (symbolSectionMPTR + symbolRelativeAddress);
				continue; // ?
			}

			if (sharedImportTracking[symSectionIndex].rplLoaderContext == HLE_MODULE_PTR)
			{
				// get address
				uint32 nameOffset = sym->ukn00;
				char* symbolName = (char*)strtabData + nameOffset;
				if (nameOffset >= strtabSize)
				{
					cemuLog_log(LogType::Force, "RPLLoader: Symbol {} in section {} has out-of-bounds name offset", i, symSectionIndex);
					continue;
				}

				uint32 exportAddress;
				if (nameOffset == 0)
				{
					cemu_assert_debug(symbolName[0] == '\0');
					exportAddress = 0;
				}
				else
				{
					bool isDataExport = (rplLoaderContext->sectionTablePtr[symSectionIndex].flags & 0x4) == 0;
					exportAddress = _findHLEExport(rplLoaderContext, sharedImportTracking.data() + symSectionIndex, sharedImportTracking[symSectionIndex].modulename, symbolName, isDataExport);
				}

				sym->symbolAddress = exportAddress;
			}
			else
			{
				RPLModule* ctxExportModule = sharedImportTracking[symSectionIndex].rplLoaderContext;
				uint32 nameOffset = sym->ukn00;
				char* symbolName = (char*)strtabData + nameOffset;

				bool foundExport = false;
				if ((rplLoaderContext->sectionTablePtr[symSectionIndex].flags & 0x4) != 0)
				{
					// find function export
					char* exportNameData = (char*)((uint8*)ctxExportModule->exportFDataPtr - 8);
					for (uint32 f = 0; f < ctxExportModule->exportFCount; f++)
					{
						char* name = exportNameData + (uint32)ctxExportModule->exportFDataPtr[f].nameOffset;
						if (strcmp(name, symbolName) == 0)
						{
							uint32 exportAddress = ctxExportModule->exportFDataPtr[f].virtualOffset;
							sym->symbolAddress = exportAddress;
							foundExport = true;
							break;
						}
					}
				}
				else
				{
					// find data export
					char* exportNameData = (char*)((uint8*)ctxExportModule->exportDDataPtr - 8);
					for (uint32 f = 0; f < ctxExportModule->exportDCount; f++)
					{
						char* name = exportNameData + (uint32)ctxExportModule->exportDDataPtr[f].nameOffset;
						if (strcmp(name, symbolName) == 0)
						{
							uint32 exportAddress = ctxExportModule->exportDDataPtr[f].virtualOffset;
							sym->symbolAddress = exportAddress;
							foundExport = true;
							break;
						}
					}
				}
				if (foundExport == false)
				{
#ifdef CEMU_DEBUG_ASSERT
					if (nameOffset > 0)
					{
						cemuLog_logDebug(LogType::Force, "export not found - force lookup in function exports");
						// workaround - force look up export in function exports
						char* exportNameData = (char*)((uint8*)ctxExportModule->exportFDataPtr - 8);
						for (uint32 f = 0; f < ctxExportModule->exportFCount; f++)
						{
							char* name = exportNameData + (uint32)ctxExportModule->exportFDataPtr[f].nameOffset;
							if (strcmp(name, symbolName) == 0)
							{
								uint32 exportAddress = ctxExportModule->exportFDataPtr[f].virtualOffset;
								sym->symbolAddress = exportAddress;
								foundExport = true;
								break;
							}
						}
					}
#endif
					continue;
				}
			}
		}
		else
		{
			uint32 symbolType = sym->info & 0xF;
			if (symbolType == 6)
				continue;
			if (((uint32)symbolSection->type != SHT_RPL_IMPORTS && linkMode != 2) ||
				((uint32)symbolSection->type == SHT_RPL_IMPORTS && linkMode != 1 && linkMode != 2)
				)
			{
				// update virtual address to match actual mapped address
				cemu_assert(symbolSectionAddress >= memory_base && symbolSectionAddress <= (memory_base + 0x100000000ULL));
				uint32 symbolSectionMPTR = memory_getVirtualOffsetFromPointer(symbolSectionAddress);
				uint32 symbolRelativeAddress = (uint32)sym->symbolAddress - (uint32)symbolSection->virtualAddress;
				sym->symbolAddress = (symbolSectionMPTR + symbolRelativeAddress);
			}
		}
	}
	return true;
}

bool RPLLoader_ApplySingleReloc(RPLModule* rplLoaderContext, uint32 uknR3, uint8* relocTargetSectionAddress, uint32 relocType, bool isSymbolBinding2, uint32 relocOffset, uint32 relocAddend, uint32 symbolAddress, sint16 tlsModuleIndex)
{
	MPTR relocTargetSectionMPTR = memory_getVirtualOffsetFromPointer(relocTargetSectionAddress);
	MPTR relocAddrMPTR = relocTargetSectionMPTR + relocOffset;
	uint8* relocAddr = memory_getPointerFromVirtualOffset(relocAddrMPTR);

	if (relocType == RPL_RELOC_HA16)
	{
		uint32 relocDestAddr = symbolAddress + relocAddend;
		uint32 p = (relocDestAddr >> 16);
		p += (relocDestAddr >> 15) & 1;
		*(uint16be*)(relocAddr) = (uint16)p;
	}
	else if (relocType == RPL_RELOC_LO16)
	{
		uint32 relocDestAddr = symbolAddress + relocAddend;
		uint32 p = relocDestAddr;
		*(uint16be*)(relocAddr) = (uint16)p;
	}
	else if (relocType == RPL_RELOC_HI16)
	{
		uint32 relocDestAddr = symbolAddress + relocAddend;
		uint32 p = relocDestAddr>>16;
		*(uint16be*)(relocAddr) = (uint16)p;
	}
	else if (relocType == RPL_RELOC_REL24)
	{
		// todo - effect of isSymbolBinding2?
		uint32 opc = *(uint32be*)relocAddr;
		uint32 relocDestAddr = symbolAddress + relocAddend;
		uint32 jumpDistance = relocDestAddr - memory_getVirtualOffsetFromPointer(relocAddr);
		if ((jumpDistance>>25) != 0 && (jumpDistance >> 25) != 0x7F)
		{
			// can't reach with 24bit jump, use trampoline + absolute branch
			MPTR trampolineAddr = _generateTrampolineFarJump(rplLoaderContext, relocDestAddr);
			// make absolute branch
			cemu_assert_debug((opc >> 26) == 18); // should be B/BL instruction
			opc &= ~0x03fffffc;
			opc |= (trampolineAddr & 0x3FFFFFC);
			opc |= (1 << 1); // absolute jump
			*(uint32be*)relocAddr = opc;
		}
		else
		{
			// within range, update jump opcode
			if ((jumpDistance & 3) != 0)
				cemuLog_log(LogType::Force, "RPL-Loader: Encountered unaligned RPL_RELOC_REL24");
			opc &= ~0x03fffffc;
			opc |= (jumpDistance &0x03fffffc);
			*(uint32be*)relocAddr = opc;
		}
	}
	else if (relocType == RPL_RELOC_REL14)
	{
		// seen in Your Shape: Fitness Evolved
		// todo - effect of isSymbolBinding2?
		uint32 opc = *(uint32be*)relocAddr;
		uint32 relocDestAddr = symbolAddress + relocAddend;
		uint32 jumpDistance = relocDestAddr - memory_getVirtualOffsetFromPointer(relocAddr);
		if ((jumpDistance & ~0x7fff) != 0xFFFF8000 && (jumpDistance & ~0x7fff) != 0x00000000)
		{
			cemu_assert_debug(false);
		}
		else
		{
			// within range, update jump opcode
			if ((jumpDistance & 3) != 0)
				cemuLog_log(LogType::Force, "RPL-Loader: Encountered unaligned RPL_RELOC_REL14");
			opc &= ~0xfffc;
			opc |= (jumpDistance & 0xfffc);
			*(uint32be*)relocAddr = opc;
		}
	}
	else if (relocType == RPL_RELOC_ADDR32)
	{
		uint32 relocDestAddr = symbolAddress + relocAddend;
		uint32 p = relocDestAddr;
		*(uint32be*)(relocAddr) = (uint32)p;
	}
	else if (relocType == R_PPC_DTPMOD32)
	{
		// patch tls_index.moduleIndex
		*(uint32be*)(relocAddr) = (uint32)(sint32)tlsModuleIndex;
	}
	else if (relocType == R_PPC_DTPREL32)
	{
		// patch tls_index.size
		*(uint32be*)(relocAddr) = (uint32)(sint32)(symbolAddress + relocAddend);
	}
	else if (relocType == R_PPC_REL16_HA)
	{
		// used by WUT
		uint32 relAddr = (symbolAddress + relocAddend) - relocAddrMPTR;
		uint32 p = (relAddr >> 16);
		p += (relAddr >> 15) & 1;
		*(uint16be*)(relocAddr) = (uint16)p;
	}
	else if (relocType == R_PPC_REL16_HI)
	{
		// used by WUT
		uint32 relAddr = (symbolAddress + relocAddend) - relocAddrMPTR;
		uint32 p = (relAddr >> 16);
		*(uint16be*)(relocAddr) = (uint16)p;
	}
	else if (relocType == R_PPC_REL16_LO)
	{
		// used by WUT
		uint32 relAddr = (symbolAddress + relocAddend) - relocAddrMPTR;
		uint32 p = (relAddr & 0xFFFF);
		*(uint16be*)(relocAddr) = (uint16)p;
	}
	else if (relocType == 0x6D) // SDATA reloc
	{
		uint32 opc = *(uint32be*)relocAddr;

		uint32 registerIndex = (opc >> 16) & 0x1F;
		uint32 destination = (symbolAddress + relocAddend);;

		if (registerIndex == 2)
		{
			uint32 offset = destination - rplLoader_sdata2Addr;
			uint32 newOpc = (opc & 0xffe00000) | (offset & 0xffff) | (registerIndex << 16);
			*(uint32be*)relocAddr = newOpc;
		}
		else if (registerIndex == 13)
		{
			uint32 offset = destination - rplLoader_sdataAddr;
			uint32 newOpc = (opc & 0xffe00000) | (offset & 0xffff) | (registerIndex << 16);
			*(uint32be*)relocAddr = newOpc;
		}
		else
		{
			cemuLog_log(LogType::Force, "RPLLoader: sdata reloc uses register other than r2/r13");
			cemu_assert(false);
		}
	}
	else if (relocType == 0xFB)
	{
		// relative offset - high
		uint32 relocDestAddr = symbolAddress + relocAddend;
		uint32 relativeOffset = relocDestAddr - relocAddrMPTR;
		uint16 prevValue = *(uint16be*)relocAddr;
		uint32 newImm = ((relativeOffset >> 16) + ((relativeOffset >> 15) & 0x1));
		newImm &= 0xFFFF;
		*(uint16be*)relocAddr = newImm;
		if (symbolAddress != 0)
		{
			cemu_assert_debug((uint32)prevValue == newImm);
		}
	}
	else if (relocType == 0xFD)
	{
		// relative offset - low
		uint32 relocDestAddr = symbolAddress + relocAddend;
		uint32 relativeOffset = relocDestAddr - relocAddrMPTR;
		uint16 prevValue = *(uint16be*)relocAddr;
		uint32 newImm = relativeOffset;
		newImm &= 0xFFFF;
		*(uint16be*)relocAddr = newImm;
		if (symbolAddress != 0)
		{
			cemu_assert_debug((uint32)prevValue == newImm);
		}
	}
	else
	{
		cemuLog_log(LogType::Force, "RPLLoader: Unsupported reloc type 0x{:02x}", relocType);
		cemu_assert_debug(false); // unknown reloc type
	}
	return true;
}

bool RPLLoader_ApplyRelocs(RPLModule* rplLoaderContext, sint32 relaSectionIndex, rplSectionEntryNew_t* section, uint32 linkMode)
{
	uint32 relocTargetSectionIndex = section->relocTargetSectionIndex;
	if (relocTargetSectionIndex >= (uint32)rplLoaderContext->rplHeader.sectionTableEntryCount)
		assert_dbg();
	uint32 symtabSectionIndex = section->symtabSectionIndex;
	uint8* relocTargetSectionAddress = (uint8*)(rplLoaderContext->sectionAddressTable2[relocTargetSectionIndex].ptr);
	cemu_assert(relocTargetSectionAddress);
	// get symtab info
	rplSectionEntryNew_t* symtabSection = rplLoaderContext->sectionTablePtr + symtabSectionIndex;
	uint32 symtabSectionSize = symtabSection->sectionSize;
	uint32 symbolEntrySize = symtabSection->ukn24;
	if (symbolEntrySize == 0)
		symbolEntrySize = 0x10;
	cemu_assert(symbolEntrySize == 0x10);
	cemu_assert((symtabSectionSize % symbolEntrySize) == 0);
	uint32 symbolCount = symtabSectionSize / symbolEntrySize;
	cemu_assert(symbolCount >= 2);
	uint8* symtabData = (uint8*)rplLoaderContext->sectionAddressTable2[symtabSectionIndex].ptr;
	// decompress reloc section if needed
	uint8* relocData;
	uint32 relocSize;
	if ((uint32)(section->flags) & SHF_RPL_COMPRESSED)
	{
		uint8* relocRawData = (uint8*)rplLoaderContext->sectionAddressTable2[relaSectionIndex].ptr;
		uint32 relocUncompressedSize = *(uint32be*)relocRawData;
		relocData = (uint8*)malloc(relocUncompressedSize);
		relocSize = relocUncompressedSize;
		// decompress
		int ret;
		z_stream strm;
		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		ret = inflateInit(&strm);
		if (ret == Z_OK)
		{
			strm.avail_in = (uint32)section->sectionSize - 4;
			strm.next_in = relocRawData + 4;
			strm.avail_out = relocUncompressedSize;
			strm.next_out = relocData;
			ret = inflate(&strm, Z_FULL_FLUSH);
			cemu_assert_debug(ret == Z_OK || ret == Z_STREAM_END);
			cemu_assert_debug(strm.avail_in == 0 && strm.avail_out == 0);
			inflateEnd(&strm);
		}
	}
	else
	{
		relocData = (uint8*)rplLoaderContext->sectionAddressTable2[relaSectionIndex].ptr;
		relocSize = section->sectionSize;
	}
	// check CRC
	uint32 calcCRC = crc32_calc(0, relocData, relocSize);
	uint32 crc = rplLoaderContext->GetSectionCRC(relaSectionIndex);
	if (calcCRC != crc)
	{
		cemuLog_log(LogType::Force, "RPLLoader {} - Relocation section {} has CRC mismatch - Calc: {:08x} Actual: {:08x}", rplLoaderContext->moduleName2.c_str(), relaSectionIndex, calcCRC, crc);
	}
	// process relocations
	sint32 relocCount = relocSize / sizeof(rplRelocNew_t);
	rplRelocNew_t* reloc = (rplRelocNew_t*)relocData;
	for (sint32 i = 0; i < relocCount; i++)
	{
		uint32 relocType = (uint32)reloc->symbolIndexAndType & 0xFF;
		uint32 relocSymbolIndex = (uint32)reloc->symbolIndexAndType >> 8;
		if (relocType == 0)
		{
			// next
			reloc++;
			continue;
		}
		if (relocSymbolIndex >= symbolCount)
		{
			cemuLog_logDebug(LogType::Force, "reloc with symbol index out of range 0x{:04x}", (uint32)relocSymbolIndex);
			reloc++;
			continue;
		}
		// get symbol
		RPLFileSymtabEntry* sym = (RPLFileSymtabEntry*)(symtabData + symbolEntrySize*relocSymbolIndex);

		if ((uint32)sym->sectionIndex >= (uint32)rplLoaderContext->rplHeader.sectionTableEntryCount)
		{
			cemuLog_logDebug(LogType::Force, "reloc with sectionIndex out of range 0x{:04x}", (uint32)sym->sectionIndex);
			reloc++;
			continue;
		}
		// exclude symbols that arent ready yet
		if (linkMode == 0)
		{
			if ((uint32)rplLoaderContext->sectionTablePtr[(uint32)sym->sectionIndex].type == SHT_RPL_IMPORTS)
			{
				reloc++;
				continue;
			}
		}
		uint32 symbolAddress = sym->symbolAddress;
		uint8 symbolType = (sym->info >> 0) & 0xF;
		uint8 symbolBinding = (sym->info >> 4) & 0xF;
		if ((symbolAddress&0xFF000000) == 0xCD000000)
		{
			cemu_assert_unimplemented();
			// next
			reloc++;
			continue;
		}
		sint16 tlsModuleIndex = -1;
		if (relocType == R_PPC_DTPMOD32 || relocType == R_PPC_DTPREL32)
		{
			// TLS-relocation
			if (symbolType != 6)
				assert_dbg(); // not a TLS symbol
			if (rplLoaderContext->fileInfo.tlsModuleIndex == -1)
			{
				cemuLog_log(LogType::Force, "RPLLoader: TLS relocs applied to non-TLS module");
				cemu_assert_debug(false); // module not a TLS-module
			}
			tlsModuleIndex = rplLoaderContext->fileInfo.tlsModuleIndex;
		}
		uint32 relocOffset = (uint32)reloc->relocOffset - (uint32)rplLoaderContext->sectionTablePtr[relocTargetSectionIndex].virtualAddress;
		RPLLoader_ApplySingleReloc(rplLoaderContext, 0, relocTargetSectionAddress, relocType, symbolBinding == 2, relocOffset, reloc->relocAddend, symbolAddress, tlsModuleIndex);

		// next reloc
		reloc++;
	}

	if ((uint32)(section->flags) & SHF_RPL_COMPRESSED)
		free(relocData);
	return true;
}

bool RPLLoader_HandleRelocs(RPLModule* rplLoaderContext, std::span<RPLSharedImportTracking> sharedImportTracking, uint32 linkMode)
{
	// resolve relocs
	for (sint32 i = 0; i < (sint32)rplLoaderContext->rplHeader.sectionTableEntryCount; i++)
	{
		rplSectionEntryNew_t* section = rplLoaderContext->sectionTablePtr + i;
		uint32 sectionType = section->type;
		if( sectionType != SHT_SYMTAB )
			continue;
		RPLLoader_FixImportSymbols(rplLoaderContext, i, section, sharedImportTracking, linkMode);
	}

	// apply relocs again after we have fixed the import section
	for (sint32 i = 0; i < (sint32)rplLoaderContext->rplHeader.sectionTableEntryCount; i++)
	{
		rplSectionEntryNew_t* section = rplLoaderContext->sectionTablePtr + i;
		uint32 sectionType = section->type;
		if (sectionType != SHT_RELA)
			continue;
		RPLLoader_ApplyRelocs(rplLoaderContext, i, section, linkMode);
	}
	return true;
}

void _RPLLoader_ExtractModuleNameFromPath(char* output, std::string_view input)
{
	// scan to last '/'
	cemu_assert(!input.empty());
	size_t startIndex = input.size() - 1;
	while (startIndex > 0)
	{
		if (input[startIndex] == '/')
		{
			startIndex++;
			break;
		}
		startIndex--;
	}
	// cut off after '.'
	size_t endIndex = startIndex;
	while (endIndex < input.size())
	{
		if (input[endIndex] == '.')
			break;
		endIndex++;
	}
	size_t nameLen = endIndex - startIndex;
	cemu_assert(nameLen != 0);
	nameLen = std::min<size_t>(nameLen, RPL_MODULE_NAME_LENGTH-1);
	memcpy(output, input.data() + startIndex, nameLen);
	output[nameLen] = '\0';
	// convert to lower case
	std::for_each(output, output + nameLen, [](char& c) {c = _ansiToLower(c);});
}

void RPLLoader_InitState()
{
	cemu_assert_debug(!rplLoaderHeap_lowerAreaCodeMem2.hasAllocations());
	cemu_assert_debug(!rplLoaderHeap_codeArea2.hasAllocations());
	cemu_assert_debug(!rplLoaderHeap_workarea.hasAllocations());
	rplLoaderHeap_lowerAreaCodeMem2.setHeapBase(memory_getPointerFromVirtualOffset(MEMORY_CODE_TRAMPOLINE_AREA_ADDR));
	rplLoaderHeap_codeArea2.setHeapBase(memory_getPointerFromVirtualOffset(MEMORY_CODEAREA_ADDR));
	rplLoaderHeap_workarea.setHeapBase(memory_getPointerFromVirtualOffset(MEMORY_RPLLOADER_AREA_ADDR));
	g_heapTrampolineArea.setBaseAllocator(&rplLoaderHeap_lowerAreaCodeMem2);
    RPLLoader_ResetState();
}

void RPLLoader_BeginCemuhookCRC(RPLModule* rpl)
{
	// calculate some values required for CRC
	sint32 sectionSymTableIndex = -1;
	sint32 sectionStrTableIndex = -1;
	for (sint32 i = 0; i < rpl->rplHeader.sectionTableEntryCount; i++)
	{
		if (rpl->sectionTablePtr[i].type == SHT_SYMTAB)
			sectionSymTableIndex = i;
		if (rpl->sectionTablePtr[i].type == SHT_STRTAB && i != rpl->rplHeader.nameSectionIndex && sectionStrTableIndex == -1)
			sectionStrTableIndex = i;
	}
	// init patches CRC
	rpl->patchCRC = 0;
	static const uint8 rplMagic[4] = { 0x7F, 'R', 'P', 'X' };
	rpl->patchCRC = crc32_calc(rpl->patchCRC, rplMagic, sizeof(rplMagic));
	sint32 sectionCount = rpl->rplHeader.sectionTableEntryCount;
	rpl->patchCRC = crc32_calc(rpl->patchCRC, &sectionCount, sizeof(sectionCount));
	rpl->patchCRC = crc32_calc(rpl->patchCRC, &sectionSymTableIndex, sizeof(sectionSymTableIndex));
	rpl->patchCRC = crc32_calc(rpl->patchCRC, &sectionStrTableIndex, sizeof(sectionStrTableIndex));
	sint32 sectionSectNameTableIndex = rpl->rplHeader.nameSectionIndex;
	rpl->patchCRC = crc32_calc(rpl->patchCRC, &sectionSectNameTableIndex, sizeof(sectionSectNameTableIndex));

	// sections
	for (sint32 i = 0; i < rpl->rplHeader.sectionTableEntryCount; i++)
	{
		auto sect = rpl->sectionTablePtr + i;
		uint32 nameOffset = sect->nameOffset;
		uint32 shType = sect->type;
		uint32 flags = sect->flags;
		uint32 virtualAddress = sect->virtualAddress;
		uint32 alignment = sect->alignment;
		uint32 sectionFileOffset = sect->fileOffset;
		uint32 sectionCompressedSize = sect->sectionSize;
		uint32 rawSize = 0;
		bool memoryAllocated = false;
		void* rawData = nullptr;
		if (shType == SHT_NOBITS)
		{
			rawData = NULL;
			rawSize = sectionCompressedSize;
		}
		else if ((flags&SHF_RPL_COMPRESSED) != 0)
		{
			uint32 decompressedSize = _swapEndianU32(*(uint32*)(rpl->RPLRawData.data() + sectionFileOffset));
			rawSize = decompressedSize;
			if (rawSize >= 1024*1024*1024)
			{
				cemuLog_logDebug(LogType::Force, "RPLLoader-CRC: Cannot load section {} which is too large ({} bytes)", i, decompressedSize);
				cemu_assert_debug(false);
				continue;
			}
			rawData = (uint8*)malloc(decompressedSize);
			if (rawData == nullptr)
			{
				cemuLog_logDebug(LogType::Force, "RPLLoader-CRC: Failed to allocate memory for uncompressed section {} ({} bytes)", i, decompressedSize);
				cemu_assert_debug(false);
				continue;
			}
			memoryAllocated = true;
			// decompress
			z_stream strm;
			strm.zalloc = Z_NULL;
			strm.zfree = Z_NULL;
			strm.opaque = Z_NULL;
			inflateInit(&strm);
			strm.avail_in = sectionCompressedSize - 4;
			strm.next_in = rpl->RPLRawData.data() + (uint32)sectionFileOffset + 4;
			strm.avail_out = decompressedSize;
			strm.next_out = (Bytef*)rawData;
			auto ret = inflate(&strm, Z_FULL_FLUSH);
			if (ret != Z_OK && ret != Z_STREAM_END || strm.avail_in != 0 || strm.avail_out != 0)
			{
				cemuLog_logDebug(LogType::Force, "RPLLoader-CRC: Unable to decompress section {}", i);
				cemuLog_logDebug(LogType::Force, "zRet {} availIn {} availOut {}", ret, (sint32)strm.avail_in, (sint32)strm.avail_out);
				cemu_assert_debug(false);
				free(rawData);
				inflateEnd(&strm);
				continue;
			}
			inflateEnd(&strm);
		}
		else
		{
			rawSize = sectionCompressedSize;
			rawData = rpl->RPLRawData.data() + sectionFileOffset;
		}

		rpl->patchCRC = crc32_calc(rpl->patchCRC, &nameOffset, sizeof(nameOffset));
		rpl->patchCRC = crc32_calc(rpl->patchCRC, &shType, sizeof(shType));
		rpl->patchCRC = crc32_calc(rpl->patchCRC, &flags, sizeof(flags));
		rpl->patchCRC = crc32_calc(rpl->patchCRC, &virtualAddress, sizeof(virtualAddress));
		rpl->patchCRC = crc32_calc(rpl->patchCRC, &rawSize, sizeof(rawSize));
		rpl->patchCRC = crc32_calc(rpl->patchCRC, &alignment, sizeof(alignment));

		if (rawData != nullptr && rawSize > 0)
		{
			rpl->patchCRC = crc32_calc(rpl->patchCRC, rawData, rawSize);
		}
		if (memoryAllocated && rawData)
			free(rawData);
	}
}

void RPLLoader_incrementModuleDependencyRefs(RPLModule* rpl)
{
	for (uint32 i = 0; i < (uint32)rpl->rplHeader.sectionTableEntryCount; i++)
	{
		if (rpl->sectionTablePtr[i].type != (uint32be)SHT_RPL_IMPORTS)
			continue;
		char* libName = (char*)((uint8*)rpl->sectionAddressTable2[i].ptr + 8);
		RPLLoader_AddDependency(libName);
	}
}

void RPLLoader_decrementModuleDependencyRefs(RPLModule* rpl)
{
	for (uint32 i = 0; i < (uint32)rpl->rplHeader.sectionTableEntryCount; i++)
	{
		if (rpl->sectionTablePtr[i].type != (uint32be)SHT_RPL_IMPORTS)
			continue;
		char* libName = (char*)((uint8*)rpl->sectionAddressTable2[i].ptr + 8);
		RPLLoader_RemoveDependency(libName);
	}
}

void RPLLoader_UpdateEntrypoint(RPLModule* rpl)
{
	uint32 virtualEntrypoint = rpl->rplHeader.entrypoint;
	uint32 entrypoint = 0xFFFFFFFF;
	for (sint32 i = 0; i < (sint32)rpl->rplHeader.sectionTableEntryCount; i++)
	{
		rplSectionEntryNew_t* section = rpl->sectionTablePtr + i;
		uint32 sectionStartAddr = (uint32)section->virtualAddress;
		uint32 sectionEndAddr = (uint32)section->virtualAddress + (uint32)section->sectionSize;
		if (virtualEntrypoint >= sectionStartAddr && virtualEntrypoint < sectionEndAddr)
		{
			cemu_assert_debug(entrypoint == 0xFFFFFFFF);
			entrypoint = (virtualEntrypoint - sectionStartAddr + memory_getVirtualOffsetFromPointer(rpl->sectionAddressTable2[i].ptr));
		}
	}
	cemu_assert(entrypoint != 0xFFFFFFFF);
	rpl->entrypoint = entrypoint;
}

void RPLLoader_InitModuleAllocator(RPLModule* rpl)
{
	if (!rplLoader_applicationHasMemoryControl)
	{
		rpl->funcAlloc = 0;
		rpl->funcFree = 0;
		return;
	}
	coreinit::OSDynLoad_GetAllocator(&rpl->funcAlloc, &rpl->funcFree);
}

// map rpl into memory, but do not resolve relocs and imports yet
RPLModule* RPLLoader_LoadFromMemory(uint8* rplData, sint32 size, char* name)
{
	char moduleName[RPL_MODULE_NAME_LENGTH];
	_RPLLoader_ExtractModuleNameFromPath(moduleName, name);
	RPLModule* rpl = nullptr;
	if (RPLLoader_ProcessHeaders({ moduleName }, rplData, size, &rpl) == false)
	{
		delete rpl;
		return nullptr;
	}
	RPLLoader_InitModuleAllocator(rpl);
	RPLLoader_BeginCemuhookCRC(rpl);
	if (RPLLoader_LoadSections(0, rpl) == false)
	{
		delete rpl;
		return nullptr;
	}

	cemuLog_logDebug(LogType::Force, "Load {} Code-Offset: -0x{:x}", name, rpl->regionMappingBase_text.GetMPTR() - 0x02000000);

	// sdata (r2/r13)
	uint32 sdataBaseAddress = rpl->fileInfo.sdataBase1; // base + 0x8000
	uint32 sdataBaseAddress2 = rpl->fileInfo.sdataBase2; // base + 0x8000
	for (uint32 i = 0; i < (uint32)rpl->rplHeader.sectionTableEntryCount; i++)
	{
		if(rpl->sectionTablePtr[i].sectionSize == 0)
			continue;
		uint32 sectionFlags = rpl->sectionTablePtr[i].flags;
		uint32 sectionVirtualAddress = rpl->sectionTablePtr[i].virtualAddress;
		uint32 sectionSize = rpl->sectionTablePtr[i].sectionSize;
		if( (sectionFlags&4) != 0 )
			continue;
		if(sdataBaseAddress == 0x00008000 && sdataBaseAddress2 == 0x00008000)
			continue; // sdata not used (this workaround is needed for Wii U Party to boot)
		// sdata 1
		if ((sdataBaseAddress - 0x8000) >= (sectionVirtualAddress) &&
			(sdataBaseAddress - 0x8000) <= (sectionVirtualAddress + sectionSize))
		{
			uint32 rplLoader_sdataAddrNew = memory_getVirtualOffsetFromPointer(rpl->sectionAddressTable2[i].ptr) + (sdataBaseAddress - sectionVirtualAddress);
			rplLoader_sdataAddr = rplLoader_sdataAddrNew;
		}
		// sdata 2
		if ((sdataBaseAddress2 - 0x8000) >= (sectionVirtualAddress) &&
			(sdataBaseAddress2 - 0x8000) <= (sectionVirtualAddress + sectionSize))
		{
			rplLoader_sdata2Addr = memory_getVirtualOffsetFromPointer(rpl->sectionAddressTable2[i].ptr) + (sdataBaseAddress2 - sectionVirtualAddress);
		}

	}
	// cancel if error
	if (rpl->hasError)
	{
		cemuLog_log(LogType::Force, "RPLLoader: Unable to load RPL due to errors");
		delete rpl;
		return nullptr;
	}
	// find TLS section
	uint32 tlsStartAddress = 0xFFFFFFFF;
	uint32 tlsEndAddress = 0;
	for (uint32 i = 0; i < (uint32)rpl->rplHeader.sectionTableEntryCount; i++)
	{
		if ( ((uint32)rpl->sectionTablePtr[i].flags & SHF_TLS) == 0 )
			continue;
		uint32 sectionVirtualAddress = rpl->sectionTablePtr[i].virtualAddress;
		uint32 sectionSize = rpl->sectionTablePtr[i].sectionSize;
		tlsStartAddress = std::min(tlsStartAddress, sectionVirtualAddress);
		tlsEndAddress = std::max(tlsEndAddress, sectionVirtualAddress+sectionSize);
	}
	if (tlsStartAddress == 0xFFFFFFFF)
	{
		tlsStartAddress = 0;
		tlsEndAddress = 0;
	}
	rpl->tlsStartAddress = tlsStartAddress;
	rpl->tlsEndAddress = tlsEndAddress;

	// add to module list
	cemu_assert(rplModuleCount < 256);
	rplModuleList[rplModuleCount] = rpl;
	rplModuleCount++;

	// track dependencies
	RPLLoader_incrementModuleDependencyRefs(rpl);

	// update entrypoint
	RPLLoader_UpdateEntrypoint(rpl);
	return rpl;
}

void RPLLoader_FlushMemory(RPLModule* rpl)
{
	// invalidate recompiler cache
	PPCRecompiler_invalidateRange(rpl->regionMappingBase_text.GetMPTR(), rpl->regionMappingBase_text.GetMPTR() + rpl->regionSize_text);
	rpl->heapTrampolineArea.forEachBlock([](void* mem, uint32 size)
	{
		MEMPTR<void> memVirtual = mem;
		PPCRecompiler_invalidateRange(memVirtual.GetMPTR(), memVirtual.GetMPTR() + size);
	});
}

// resolve relocs and imports of all modules. Or resolve exports
void RPLLoader_LinkSingleModule(RPLModule* rplLoaderContext, bool resolveOnlyExports)
{
	// setup shared import tracking
	std::vector<RPLSharedImportTracking> sharedImportTracking;

	sharedImportTracking.resize(rplLoaderContext->rplHeader.sectionTableEntryCount - 2);

	memset(sharedImportTracking.data(), 0, sizeof(RPLSharedImportTracking) * sharedImportTracking.size());

	for (uint32 i = 0; i < (uint32)rplLoaderContext->rplHeader.sectionTableEntryCount; i++)
	{
		if( rplLoaderContext->sectionTablePtr[i].type != (uint32be)SHT_RPL_IMPORTS )
			continue;
		char* libName = (char*)((uint8*)rplLoaderContext->sectionAddressTable2[i].ptr + 8);
		// make module name
		char _importModuleName[RPL_MODULE_NAME_LENGTH];
		_RPLLoader_ExtractModuleNameFromPath(_importModuleName, libName);
		// find in loaded module list
		std::string importModuleName{_importModuleName};
		bool foundModule = false;
		for (sint32 f = 0; f < rplModuleCount; f++)
		{
			if (boost::iequals(rplModuleList[f]->moduleName2, importModuleName))
			{
				sharedImportTracking[i].rplLoaderContext = rplModuleList[f];
				memset(sharedImportTracking[i].modulename, 0, sizeof(sharedImportTracking[i].modulename));
				strcpy_s(sharedImportTracking[i].modulename, importModuleName.c_str());
				foundModule = true;
				break;
			}
		}
		if( foundModule )
			continue;
		// if not found, we assume it's a HLE lib
		sharedImportTracking[i].rplLoaderContext = HLE_MODULE_PTR;
		sharedImportTracking[i].exportSection = nullptr;
		strcpy_s(sharedImportTracking[i].modulename, libName);
	}

	if (resolveOnlyExports)
		RPLLoader_HandleRelocs(rplLoaderContext, sharedImportTracking, 2);
	else
		RPLLoader_HandleRelocs(rplLoaderContext, sharedImportTracking, 0);

	RPLLoader_FlushMemory(rplLoaderContext);
}

void RPLLoader_LoadSectionDebugSymbols(RPLModule* rplLoaderContext, rplSectionEntryNew_t* section, int symtabSectionIndex)
{
	uint32 sectionSize = section->sectionSize;
	uint32 symbolEntrySize = section->ukn24;
	if (symbolEntrySize == 0)
		symbolEntrySize = 0x10;
	cemu_assert(symbolEntrySize == 0x10);
	cemu_assert((sectionSize % symbolEntrySize) == 0);
	uint32 symbolCount = sectionSize / symbolEntrySize;
	cemu_assert(symbolCount >= 2);

	uint16 sectionCount = rplLoaderContext->rplHeader.sectionTableEntryCount;
	uint8* symtabData = (uint8*)rplLoaderContext->sectionAddressTable2[symtabSectionIndex].ptr;

	uint32 strtabSectionIndex = section->symtabSectionIndex;
	uint8* strtabData = (uint8*)rplLoaderContext->sectionAddressTable2[strtabSectionIndex].ptr;

	for (uint32 i = 0; i < symbolCount; i++)
	{
		RPLFileSymtabEntry* sym = (RPLFileSymtabEntry*)(symtabData + i * symbolEntrySize);

		uint16 symSectionIndex = sym->sectionIndex;
		if (symSectionIndex == 0 || symSectionIndex >= sectionCount)
			continue;
		void* symbolSectionAddress = rplLoaderContext->sectionAddressTable2[symSectionIndex].ptr;
		if (symbolSectionAddress == nullptr)
			continue;
		rplSectionEntryNew_t* symbolSection = rplLoaderContext->sectionTablePtr + symSectionIndex;
		if(symbolSection->type == SHT_RPL_EXPORTS || symbolSection->type == SHT_RPL_IMPORTS)
			continue; // exports and imports are handled separately

		uint32 symbolOffset = sym->symbolAddress - symbolSection->virtualAddress;

		uint32 nameOffset = sym->ukn00;
		if (nameOffset > 0)
		{
			char* symbolName = (char*)strtabData + nameOffset;
			if (sym->info == 0x12)
			{
				rplSymbolStorage_store(rplLoaderContext->moduleName2.c_str(), symbolName, sym->symbolAddress);
			}
		}
	}
}

void RPLLoader_LoadDebugSymbols(RPLModule* rplLoaderContext)
{
	for (sint32 i = 0; i < (sint32)rplLoaderContext->rplHeader.sectionTableEntryCount; i++)
	{
		rplSectionEntryNew_t* section = rplLoaderContext->sectionTablePtr + i;
		uint32 sectionType = section->type;
		if (sectionType != SHT_SYMTAB)
			continue;
		RPLLoader_LoadSectionDebugSymbols(rplLoaderContext, section, i);
	}
}

void RPLLoader_UnloadModule(RPLModule* rpl)
{
	/*
	  A note:
	  Mario Party 10's mg0480.rpl (minigame Spike Ball Scramble) has a bug where it keeps running code (function 0x02086BCC for example) after RPL unload
	  It seems to rely on the RPL loader not zeroing released memory
	*/

	// decrease reference counters of all dependencies
	RPLLoader_decrementModuleDependencyRefs(rpl);

	// save module config for this module in the debugger
	debuggerWindow_notifyModuleUnloaded(rpl);

	// release memory
	rplLoaderHeap_codeArea2.free(rpl->regionMappingBase_text.GetPtr());
	rpl->regionMappingBase_text = nullptr;

	// for some reason freeing the data allocations causes a crash in MP10 on boot
	//RPLLoader_FreeData(rpl, MEMPTR<void>(rpl->regionMappingBase_data).GetPtr());
	//rpl->regionMappingBase_data = 0;
	//RPLLoader_FreeData(rpl, MEMPTR<void>(rpl->regionMappingBase_loaderInfo).GetPtr());
	//rpl->regionMappingBase_loaderInfo = 0;

	rpl->heapTrampolineArea.releaseAll();

	// todo - remove from rplSymbolStorage_store

	if (rpl->sectionTablePtr)
	{
		free(rpl->sectionTablePtr);
		rpl->sectionTablePtr = nullptr;
	}

	// unload temp region
	if (rpl->tempRegionPtr)
	{
		RPLLoader_FreeWorkarea(rpl->tempRegionPtr);
		rpl->tempRegionPtr = nullptr;
	}

	// remove from rpl module list
	for (sint32 i = 0; i < rplModuleCount; i++)
	{
		if (rplModuleList[i] == rpl)
		{
			rplModuleList[i] = rplModuleList[rplModuleCount-1];
			rplModuleCount--;
			break;
		}
	}

	delete rpl;
}

void RPLLoader_FixModuleTLSIndex(RPLModule* rplLoaderContext)
{
	sint16 tlsModuleIndex = -1;
	for (auto& dep : rplDependencyList)
	{
		if (boost::iequals(rplLoaderContext->moduleName2, dep->modulename))
		{
			tlsModuleIndex = dep->tlsModuleIndex;
			break;
		}
	}
	cemu_assert(tlsModuleIndex != -1);
	rplLoaderContext->fileInfo.tlsModuleIndex = tlsModuleIndex;
}

void RPLLoader_Link()
{
	// calculate TLS index
	for (sint32 i = 0; i < rplModuleCount; i++)
	{
		if (rplModuleList[i]->isLinked)
			continue;
		RPLLoader_FixModuleTLSIndex(rplModuleList[i]);
	}
	// resolve relocs
	for (sint32 i = 0; i < rplModuleCount; i++)
	{
		if(rplModuleList[i]->isLinked)
			continue;
		RPLLoader_LinkSingleModule(rplModuleList[i], false);
	}
	// resolve imports and load debug symbols
	for (sint32 i = 0; i < rplModuleCount; i++)
	{
		if (rplModuleList[i]->isLinked)
			continue;
		RPLLoader_LinkSingleModule(rplModuleList[i], true);
		RPLLoader_LoadDebugSymbols(rplModuleList[i]);
		rplModuleList[i]->isLinked = true; // mark as linked
		GraphicPack2::NotifyModuleLoaded(rplModuleList[i]);
		debuggerWindow_notifyModuleLoaded(rplModuleList[i]);
	}
}

uint32 RPLLoader_GetModuleEntrypoint(RPLModule* rplLoaderContext)
{
	return rplLoaderContext->entrypoint;
}

// takes a module name without extension, returns true if the RPL module is a known Cafe OS module
bool RPLLoader_IsKnownCafeOSModule(std::string_view name)
{
	static std::unordered_set<std::string> s_systemModules556 = {
			"avm","camera","coreinit","dc","dmae","drmapp","erreula",
			"gx2","h264","lzma920","mic","nfc","nio_prof","nlibcurl",
			"nlibnss","nlibnss2","nn_ac","nn_acp","nn_act","nn_aoc","nn_boss",
			"nn_ccr","nn_cmpt","nn_dlp","nn_ec","nn_fp","nn_hai","nn_hpad",
			"nn_idbe","nn_ndm","nn_nets2","nn_nfp","nn_nim","nn_olv","nn_pdm",
			"nn_save","nn_sl","nn_spm","nn_temp","nn_uds","nn_vctl","nsysccr",
			"nsyshid","nsyskbd","nsysnet","nsysuhs","nsysuvd","ntag","padscore",
			"proc_ui","sndcore2","snduser2","snd_core","snd_user","swkbd","sysapp",
			"tcl","tve","uac","uac_rpl","usb_mic","uvc","uvd","vpad","vpadbase",
			"zlib125"};
	std::string nameLower{name};
	std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), _ansiToLower);
	return s_systemModules556.contains(nameLower);
}

// increment reference counter for module
void RPLLoader_AddDependency(const char* name)
{
	cemu_assert(name[0] != '\0');
	// if name includes a path, cut it off
	const char* namePtr = name + strlen(name) - 1;
	while (namePtr > name)
	{
		if (*namePtr == '/')
		{
			namePtr++;
			break;
		}
		namePtr--;
	}
	name = namePtr;
	// get module name from path
	char moduleName[RPL_MODULE_NAME_LENGTH];
	_RPLLoader_ExtractModuleNameFromPath(moduleName, name);
	// check if dependency already exists
	for (auto& dep : rplDependencyList)
	{
		if (strcmp(moduleName, dep->modulename) == 0)
		{
			dep->referenceCount++;
			return;
		}
	}
	// add new entry
	RPLDependency* newDependency = new RPLDependency();
	strcpy(newDependency->modulename, moduleName);
	newDependency->referenceCount = 1;
	newDependency->coreinitHandle = rplLoader_currentHandleCounter;
	newDependency->tlsModuleIndex = rplLoader_currentTlsModuleIndex;
	newDependency->isCafeOSModule = RPLLoader_IsKnownCafeOSModule(moduleName);
	rplLoader_currentTlsModuleIndex++; // todo - delay handle and tls allocation until the module is actually loaded. It may not exist
	rplLoader_currentHandleCounter++;
	if (rplLoader_currentTlsModuleIndex == 0x7FFF)
		cemuLog_log(LogType::Force, "RPLLoader: Exhausted TLS module indices pool");
	// convert name to path/filename if it isn't already one
	if (strchr(name, '.'))
	{
		strcpy_s(newDependency->filepath, name);
	}
	else
	{
		strcpy_s(newDependency->filepath, name);
		strcat_s(newDependency->filepath, ".rpl");
	}
	newDependency->filepath[RPL_MODULE_PATH_LENGTH - 1] = '\0';
	rplDependencyList.push_back(newDependency);
}

// decrement reference counter for dependency by module path
void RPLLoader_RemoveDependency(const char* name)
{
	cemu_assert(*name != '\0');
	// if name includes a path, cut it off
	const char* namePtr = name + strlen(name) - 1;
	while (namePtr > name)
	{
		if (*namePtr == '/')
		{
			namePtr++;
			break;
		}
		namePtr--;
	}
	name = namePtr;
	// get module name from path
	char moduleName[RPL_MODULE_NAME_LENGTH];
	_RPLLoader_ExtractModuleNameFromPath(moduleName, name);
	// find dependency and decrement ref count
	for (auto& dep : rplDependencyList)
	{
		if (strcmp(moduleName, dep->modulename) == 0)
		{
			dep->referenceCount--;
			return;
		}
	}
}

bool RPLLoader_HasDependency(std::string_view name)
{
	char moduleName[RPL_MODULE_NAME_LENGTH];
	_RPLLoader_ExtractModuleNameFromPath(moduleName, name);
	for (const auto& dep : rplDependencyList)
	{
		if (strcmp(moduleName, dep->modulename) == 0)
			return true;
	}
	return false;
}

// decrement reference counter for dependency by module handle
void RPLLoader_RemoveDependency(uint32 handle)
{
	for (auto& dep : rplDependencyList)
	{
		if (dep->coreinitHandle == handle)
		{
			cemu_assert_debug(dep->referenceCount != 0);
			if(dep->referenceCount > 0)
				dep->referenceCount--;
			return;
		}
	}
}

uint32 RPLLoader_GetHandleByModuleName(const char* name)
{
	// get module name from path
	char moduleName[RPL_MODULE_NAME_LENGTH];
	_RPLLoader_ExtractModuleNameFromPath(moduleName, name);
	// search for existing dependency
	for (auto& dep : rplDependencyList)
	{
		if (strcmp(moduleName, dep->modulename) == 0)
		{
			cemu_assert_debug(dep->loadAttempted);
			if (!dep->isCafeOSModule && !dep->rplLoaderContext)
				return RPL_INVALID_HANDLE; // module not found
			return dep->coreinitHandle;
		}
	}
	return RPL_INVALID_HANDLE;
}

uint32 RPLLoader_GetMaxTLSModuleIndex()
{
	return rplLoader_currentTlsModuleIndex - 1;
}

bool RPLLoader_GetTLSDataByTLSIndex(sint16 tlsModuleIndex, uint8** tlsData, sint32* tlsSize)
{
	RPLModule* rplLoaderContext = nullptr;
	for (auto& dep : rplDependencyList)
	{
		if (dep->tlsModuleIndex == tlsModuleIndex)
		{
			rplLoaderContext = dep->rplLoaderContext;
			break;
		}
	}
	if (rplLoaderContext == nullptr)
		return false;
	cemu_assert(rplLoaderContext->tlsStartAddress != 0);
	uint32 tlsDataSize = rplLoaderContext->tlsEndAddress - rplLoaderContext->tlsStartAddress;
	cemu_assert_debug(tlsDataSize < 0x10000); // check for suspiciously large TLS area
	*tlsData = (uint8*)memory_getPointerFromVirtualOffset(rplLoaderContext->tlsStartAddress);
	*tlsSize = tlsDataSize;
	return true;
}

bool RPLLoader_LoadFromVirtualPath(RPLDependency* dependency, char* filePath)
{
	uint32 rplSize = 0;
	uint8* rplData = fsc_extractFile(filePath, &rplSize);
	if (rplData)
	{
		cemuLog_logDebug(LogType::Force, "Loading: {}", filePath);
		dependency->rplLoaderContext = RPLLoader_LoadFromMemory(rplData, rplSize, filePath);
		free(rplData);
		return true;
	}
	return false;
}

void RPLLoader_LoadDependency(RPLDependency* dependency)
{
	dependency->loadAttempted = true;
	// check if module is already loaded
	for (sint32 i = 0; i < rplModuleCount; i++)
	{
		if(!boost::iequals(rplModuleList[i]->moduleName2, dependency->modulename))
			continue;
		dependency->rplLoaderContext = rplModuleList[i];
		return;
	}
	char filePath[RPL_MODULE_PATH_LENGTH];
	// check if path is absolute
	if (dependency->filepath[0] == '/')
	{
		strcpy_s(filePath, dependency->filepath);
		RPLLoader_LoadFromVirtualPath(dependency, filePath);
		return;
	}
	// attempt to load rpl from code directory of current title
	strcpy_s(filePath, "/internal/current_title/code/");
	strcat_s(filePath, dependency->filepath);
	// except if it is blacklisted
	bool isBlacklisted = false;
	if (boost::iequals(dependency->filepath, "erreula.rpl"))
	{
		if (fsc_doesFileExist(filePath))
			isBlacklisted = true;
	}
	if (isBlacklisted)
		cemuLog_log(LogType::Force, fmt::format("Game tried to load \"{}\" but it is blacklisted (using Cemu's implementation instead)", filePath));
	else if (RPLLoader_LoadFromVirtualPath(dependency, filePath))
		return;
	// attempt to load rpl from Cemu's /cafeLibs/ directory
	if (ActiveSettings::LoadSharedLibrariesEnabled())
	{
		const auto filePath = ActiveSettings::GetUserDataPath("cafeLibs/{}", dependency->filepath);
		auto fileData = FileStream::LoadIntoMemory(filePath);
		if (fileData)
		{
			cemuLog_log(LogType::Force, "Loading RPL: /cafeLibs/{}", dependency->filepath);
			dependency->rplLoaderContext = RPLLoader_LoadFromMemory(fileData->data(), fileData->size(),
																	dependency->filepath);
			return;
		}
	}
}

// loads and unloads modules based on the current dependency list
void RPLLoader_UpdateDependencies()
{
	bool repeat = true;
	while (repeat)
	{
		repeat = false;
		for(auto idx = 0; idx<rplDependencyList.size(); )
		{
			auto dependency = rplDependencyList[idx];
			// debug_printf("DEP 0x%02x %s\n", dependency->referenceCount, dependency->modulename);
			if(dependency->referenceCount == 0)
			{
				// unload RPLs
				// todo - should we let HLE modules know if they are being unloaded?
				if (dependency->rplLoaderContext)
				{
					RPLLoader_UnloadModule(dependency->rplLoaderContext);
					dependency->rplLoaderContext = nullptr;
				}
				// remove from dependency list
				rplDependencyList.erase(rplDependencyList.begin()+idx);
				idx--;
				repeat = true; // unload can effect reference count of other dependencies
				break;
			}
			else if (!dependency->loadAttempted)
			{
				// load
				if (dependency->rplLoaderContext == nullptr)
				{
					RPLLoader_LoadDependency(dependency);
					repeat = true;
					idx++;
					break;
				}
			}
			idx++;
		}
	}
	RPLLoader_Link();
}

void RPLLoader_SetMainModule(RPLModule* rplLoaderContext)
{
	rplLoaderContext->entrypointCalled = true;
	rplLoader_mainModule = rplLoaderContext;
}

uint32 RPLLoader_GetMainModuleHandle()
{
	for (auto& dep : rplDependencyList)
	{
		if (dep->rplLoaderContext == rplLoader_mainModule)
		{
			return dep->coreinitHandle;
		}
	}
	cemu_assert(false);
	return 0;
}

RPLModule* RPLLoader_FindModuleByCodeAddr(uint32 addr)
{
	for (sint32 i = 0; i < rplModuleCount; i++)
	{
		uint32 startAddr = rplModuleList[i]->regionMappingBase_text.GetMPTR();
		uint32 endAddr = rplModuleList[i]->regionMappingBase_text.GetMPTR() + rplModuleList[i]->regionSize_text;
		if (addr >= startAddr && addr < endAddr)
			return rplModuleList[i];
	}
	return nullptr;
}

RPLModule* RPLLoader_FindModuleByDataAddr(uint32 addr)
{
	for (sint32 i = 0; i < rplModuleCount; i++)
	{
		// data
		uint32 startAddr = rplModuleList[i]->regionMappingBase_data;
		uint32 endAddr = rplModuleList[i]->regionMappingBase_data + rplModuleList[i]->regionSize_data;
		if (addr >= startAddr && addr < endAddr)
			return rplModuleList[i];
		// loaderinfo
		startAddr = rplModuleList[i]->regionMappingBase_loaderInfo;
		endAddr = rplModuleList[i]->regionMappingBase_loaderInfo + rplModuleList[i]->regionSize_loaderInfo;
		if (addr >= startAddr && addr < endAddr)
			return rplModuleList[i];
	}
	return nullptr;
}

RPLModule* RPLLoader_FindModuleByName(std::string module)
{
	for (sint32 i = 0; i < rplModuleCount; i++)
	{
		if (rplModuleList[i]->moduleName2 == module) return rplModuleList[i];
	}
	return nullptr;
}

void RPLLoader_CallEntrypoints()
{
	for (sint32 i = 0; i < rplModuleCount; i++)
	{
		if( rplModuleList[i]->entrypointCalled )
			continue;
		uint32 moduleHandle = RPLLoader_GetHandleByModuleName(rplModuleList[i]->moduleName2.c_str());
		MPTR entryPoint = RPLLoader_GetModuleEntrypoint(rplModuleList[i]);
		PPCCoreCallback(entryPoint, moduleHandle, 1); // 1 -> load, 2 -> unload
		rplModuleList[i]->entrypointCalled = true;
	}
}

void RPLLoader_NotifyControlPassedToApplication()
{
	rplLoader_applicationHasMemoryControl = true;
}

uint32 RPLLoader_FindModuleOrHLEExport(uint32 moduleHandle, bool isData, const char* exportName)
{
	// find dependency from handle
	RPLModule* rplLoaderContext = nullptr;
	RPLDependency* dependency = nullptr;
	for (auto& dep : rplDependencyList)
	{
		if (dep->coreinitHandle == moduleHandle)
		{
			rplLoaderContext = dep->rplLoaderContext;
			dependency = dep;
			break;
		}
	}

	uint32 exportResult = 0;
	if (rplLoaderContext)
	{
		exportResult = RPLLoader_FindModuleExport(rplLoaderContext, isData, exportName);
	}
	else
	{
		// attempt to find HLE export
		if (isData)
		{
			MPTR weakExportAddr = osLib_getPointer(dependency->modulename, exportName);
			cemu_assert_debug(weakExportAddr != 0xFFFFFFFF);
			exportResult = weakExportAddr;
		}
		else
		{
			exportResult = rpl_mapHLEImport(rplLoaderContext, dependency->modulename, exportName, true);
		}
	}

	return exportResult;
}

uint32 RPLLoader_GetSDA1Base()
{
	return rplLoader_sdataAddr;
}

uint32 RPLLoader_GetSDA2Base()
{
	return rplLoader_sdata2Addr;
}

RPLModule** RPLLoader_GetModuleList()
{
	return rplModuleList;
}

sint32 RPLLoader_GetModuleCount()
{
	return rplModuleCount;
}

template<typename TAddr, typename TSize>
class SimpleHeap
{
	struct allocEntry_t
	{
		TAddr start;
		TAddr end;
		allocEntry_t(TAddr start, TAddr end) : start(start), end(end) {};
	};

public:
	SimpleHeap(TAddr baseAddress, TSize size) : m_base(baseAddress), m_size(size)
	{

	}

	TAddr alloc(TSize size, TSize alignment)
	{
		cemu_assert_debug(alignment != 0);
		TAddr allocBase = m_base;
		allocBase = (allocBase + alignment - 1)&~(alignment-1);
		while (true)
		{
			bool hasCollision = false;
			for (auto& itr : list_allocatedEntries)
			{
				if (allocBase < itr.end && (allocBase + size) >= itr.start)
				{
					allocBase = itr.end;
					allocBase = (allocBase + alignment - 1)&~(alignment - 1);
					hasCollision = true;
					break;
				}
			}
			if(hasCollision == false)
				break;
		}
		if ((allocBase + size) > (m_base + m_size))
			return 0;
		list_allocatedEntries.emplace_back(allocBase, allocBase + size);
		return allocBase;
	}

	void free(TAddr addr)
	{
		for (sint32 i = 0; i < list_allocatedEntries.size(); i++)
		{
			if (list_allocatedEntries[i].start == addr)
			{
				list_allocatedEntries.erase(list_allocatedEntries.begin() + i);
				return;
			}
		}
		cemu_assert(false);
		return;
	}

private:
	TAddr  m_base;
	TSize  m_size;
	std::vector<allocEntry_t> list_allocatedEntries;
};

SimpleHeap<uint32, uint32> heapCodeCaveArea(MEMORY_CODECAVEAREA_ADDR, MEMORY_CODECAVEAREA_SIZE);

MEMPTR<void> RPLLoader_AllocateCodeCaveMem(uint32 alignment, uint32 size)
{
	uint32 addr = heapCodeCaveArea.alloc(size, 256);
	return MEMPTR<void>{addr};
}

void RPLLoader_ReleaseCodeCaveMem(MEMPTR<void> addr)
{
	heapCodeCaveArea.free(addr.GetMPTR());
}

void RPLLoader_ResetState()
{
	// unload all RPL modules
	while (rplModuleCount > 0)
		RPLLoader_UnloadModule(rplModuleList[0]);
	rplDependencyList.clear();
	// unload all remaining symbols
	rplSymbolStorage_unloadAll();
	// free all code imports
	g_heapTrampolineArea.releaseAll();
	list_mappedFunctionImports.clear();
	g_map_callableExports.clear();
	rplLoader_applicationHasMemoryControl = false;
	rplLoader_maxCodeAddress = 0;
	rplLoader_currentDataAllocatorAddr = 0x10000000;
	rplLoader_currentTLSModuleIndex = 1;
	rplLoader_sdataAddr = MPTR_NULL;
	rplLoader_sdata2Addr = MPTR_NULL;
	rplLoader_mainModule = nullptr;
}
