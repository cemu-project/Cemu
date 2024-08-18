#include <zlib.h>
#include "Cafe/OS/RPL/rpl.h"
#include "Cafe/OS/RPL/rpl_structs.h"
#include "util/VirtualHeap/VirtualHeap.h"
#include "Cafe/HW/Espresso/Recompiler/PPCRecompiler.h"
#include "Cafe/HW/Espresso/Debugger/Debugger.h"

typedef struct  
{
	/* +0x00 */ uint32be magic;
	/* +0x04 */ uint8 eiClass;
	/* +0x05 */ uint8 eiData;
	/* +0x06 */ uint8 eiVersion;
	/* +0x07 */ uint8 eiOSABI;
	/* +0x08 */ uint8 eiOSABIVersion;
	/* +0x09 */ uint8 eiPadding[7];
	/* +0x10 */ uint16be eType;
	/* +0x12 */ uint16be eMachine;
	/* +0x14 */ uint32be eVersion;
	/* +0x18 */ uint32be entrypoint;
	/* +0x1C */ uint32be phOffset;
	/* +0x20 */ uint32be shOffset;

	/* +0x24 */ uint32be eFlags;
	/* +0x28 */ uint16be eHeaderSize;
	/* +0x2A */ uint16be ePHEntrySize;
	/* +0x2C */ uint16be ePHNum;
	/* +0x2E */ uint16be eSHEntrySize;
	/* +0x30 */ uint16be eSHNum;
	/* +0x32 */ uint16be eShStrIndex;
}elfHeader_t;

static_assert(sizeof(elfHeader_t) == 0x34, "");

typedef struct
{
	/* +0x00 */ uint32be nameOffset;
	/* +0x04 */ uint32be shType;
	/* +0x08 */ uint32be shFlags;
	/* +0x0C */ uint32be shAddr;
	/* +0x10 */ uint32be shOffset;
	/* +0x14 */ uint32be shSize;

	/* +0x18 */ uint32be shLink;
	/* +0x1C */ uint32be shInfo;

	/* +0x20 */ uint32be shAddrAlign;
	/* +0x24 */ uint32be shEntSize;
}elfSectionEntry_t;

static_assert(sizeof(elfSectionEntry_t) == 0x28, "");

#define PF_X		(1 << 0)	/* Segment is executable */
#define PF_W		(1 << 1)	/* Segment is writable */
#define PF_R		(1 << 2)	/* Segment is readable */

// Map elf into memory
uint32 ELF_LoadFromMemory(uint8* elfData, sint32 size, const char* name)
{
	elfHeader_t* header = (elfHeader_t*)elfData;

	uint32 sectionCount = header->eSHNum;
	uint32 sectionTableOffset = header->shOffset;
	uint32 sectionTableEntrySize = header->eSHEntrySize;

	elfSectionEntry_t* sectionTable = (elfSectionEntry_t*)(elfData + (uint32)header->shOffset);
	memory_enableHBLELFCodeArea();
	for (uint32 i = 0; i < sectionCount; i++)
	{
		debug_printf("%02d Addr %08x Size %08x Offs %08x Flags %08x Type %08x EntSize %08x\n", i, (uint32)sectionTable[i].shAddr, (uint32)sectionTable[i].shSize, (uint32)sectionTable[i].shOffset, (uint32)sectionTable[i].shFlags, (uint32)sectionTable[i].shType, (uint32)sectionTable[i].shEntSize);
		uint32 shAddr = (uint32)sectionTable[i].shAddr;
		uint32 shSize = (uint32)sectionTable[i].shSize;
		uint32 shOffset = (uint32)sectionTable[i].shOffset;
		uint32 shType = (uint32)sectionTable[i].shType;
		uint32 shFlags = (uint32)sectionTable[i].shFlags;

		if (shOffset > (uint32)size)
		{
			cemuLog_log(LogType::Force, "ELF section {} out of bounds", i);
			continue;
		}
		uint32 copySize = std::min(shSize, size - shOffset);

		// 0x00802000
		if (shAddr != 0)
		{
			if (shType == SHT_NOBITS)
			{
				memset(memory_getPointerFromVirtualOffset(shAddr), 0, shSize);
			}
			else
			{
				memcpy(memory_getPointerFromVirtualOffset(shAddr), elfData + shOffset, copySize);
			}
			//  	SHT_NOBITS
		}
		if((shFlags & PF_X) > 0)
			PPCRecompiler_allocateRange(shAddr, shSize);
	}
	return header->entrypoint;
}

// From Homebrew Launcher:
//#define MEM_BASE                    (0x00800000)
//#define ELF_DATA_ADDR               (*(volatile unsigned int*)(MEM_BASE + 0x1300 + 0x00))
//#define ELF_DATA_SIZE               (*(volatile unsigned int*)(MEM_BASE + 0x1300 + 0x04))
//#define HBL_CHANNEL                 (*(volatile unsigned int*)(MEM_BASE + 0x1300 + 0x08))
//#define RPX_MAX_SIZE                (*(volatile unsigned int*)(MEM_BASE + 0x1300 + 0x0C))
//#define RPX_MAX_CODE_SIZE           (*(volatile unsigned int*)(MEM_BASE + 0x1300 + 0x10))
//#define MAIN_ENTRY_ADDR             (*(volatile unsigned int*)(MEM_BASE + 0x1400 + 0x00))
//#define OS_FIRMWARE                 (*(volatile unsigned int*)(MEM_BASE + 0x1400 + 0x04))
//
//#define OS_SPECIFICS                ((OsSpecifics*)(MEM_BASE + 0x1500))
//
//#define MEM_AREA_TABLE              ((s_mem_area*)(MEM_BASE + 0x1600))

//typedef struct _OsSpecifics
//{
//	unsigned int addr_OSDynLoad_Acquire;
//	unsigned int addr_OSDynLoad_FindExport;
//	unsigned int addr_OSTitle_main_entry;
//
//	unsigned int addr_KernSyscallTbl1;
//	unsigned int addr_KernSyscallTbl2;
//	unsigned int addr_KernSyscallTbl3;
//	unsigned int addr_KernSyscallTbl4;
//	unsigned int addr_KernSyscallTbl5;
//
//	int(*LiWaitIopComplete)(int, int *);
//	int(*LiWaitIopCompleteWithInterrupts)(int, int *);
//	unsigned int addr_LiWaitOneChunk;
//	unsigned int addr_PrepareTitle_hook;
//	unsigned int addr_sgIsLoadingBuffer;
//	unsigned int addr_gDynloadInitialized;
//	unsigned int orig_LiWaitOneChunkInstr;
//} OsSpecifics;
