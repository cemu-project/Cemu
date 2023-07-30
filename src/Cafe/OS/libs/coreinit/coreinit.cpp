#include "Cafe/OS/common/OSCommon.h"
#include "Common/SysAllocator.h"
#include "Cafe/OS/RPL/rpl.h"

#include "Cafe/OS/libs/coreinit/coreinit_Misc.h"

// includes for Initialize coreinit submodules
#include "Cafe/OS/libs/coreinit/coreinit_BSP.h"
#include "Cafe/OS/libs/coreinit/coreinit_Scheduler.h"
#include "Cafe/OS/libs/coreinit/coreinit_Atomic.h"
#include "Cafe/OS/libs/coreinit/coreinit_OverlayArena.h"
#include "Cafe/OS/libs/coreinit/coreinit_DynLoad.h"
#include "Cafe/OS/libs/coreinit/coreinit_GHS.h"
#include "Cafe/OS/libs/coreinit/coreinit_HWInterface.h"
#include "Cafe/OS/libs/coreinit/coreinit_Memory.h"
#include "Cafe/OS/libs/coreinit/coreinit_IM.h"
#include "Cafe/OS/libs/coreinit/coreinit_LockedCache.h"
#include "Cafe/OS/libs/coreinit/coreinit_MemoryMapping.h"
#include "Cafe/OS/libs/coreinit/coreinit_IPC.h"
#include "Cafe/OS/libs/coreinit/coreinit_IPCBuf.h"
#include "Cafe/OS/libs/coreinit/coreinit_Coroutine.h"
#include "Cafe/OS/libs/coreinit/coreinit_OSScreen.h"
#include "Cafe/OS/libs/coreinit/coreinit_FG.h"
#include "Cafe/OS/libs/coreinit/coreinit_SystemInfo.h"
#include "Cafe/OS/libs/coreinit/coreinit_SysHeap.h"
#include "Cafe/OS/libs/coreinit/coreinit_MCP.h"
#include "Cafe/OS/libs/coreinit/coreinit_Time.h"
#include "Cafe/OS/libs/coreinit/coreinit_Alarm.h"
#include "Cafe/OS/libs/coreinit/coreinit_CodeGen.h"
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"
#include "Cafe/OS/libs/coreinit/coreinit_MPQueue.h"
#include "Cafe/OS/libs/coreinit/coreinit_FS.h"
#include "Cafe/OS/libs/coreinit/coreinit_MEM_UnitHeap.h"
#include "Cafe/OS/libs/coreinit/coreinit_MEM_FrmHeap.h"
#include "Cafe/OS/libs/coreinit/coreinit_MEM_BlockHeap.h"
#include "Cafe/OS/libs/coreinit/coreinit_MEM_ExpHeap.h"

coreinitData_t* gCoreinitData = NULL;

sint32 ScoreStackTrace(OSThread_t* thread, MPTR sp)
{
	uint32 stackMinAddr = _swapEndianU32(thread->stackEnd);
	uint32 stackMaxAddr = _swapEndianU32(thread->stackBase);

	sint32 score = 0;
	uint32 currentStackPtr = sp;
	for (sint32 i = 0; i < 50; i++)
	{
		uint32 nextStackPtr = memory_readU32(currentStackPtr);
		if (nextStackPtr < currentStackPtr)
			break;
		if (nextStackPtr < stackMinAddr || nextStackPtr > stackMaxAddr)
			break;
		if ((nextStackPtr & 3) != 0)
			break;
		score += 10;

		uint32 returnAddress = 0;
		returnAddress = memory_readU32(nextStackPtr + 4);
		//cemuLog_log(LogType::Force, fmt::format("SP {0:08x} ReturnAddress {1:08x}", nextStackPtr, returnAddress));
		if (returnAddress > 0 && returnAddress < 0x10000000 && (returnAddress&3) == 0)
			score += 5; // within code region
		else
			score -= 5;

		currentStackPtr = nextStackPtr;

	}
	return score;
}

void DebugLogStackTrace(OSThread_t* thread, MPTR sp)
{
	// sp might not point to a valid stackframe
	// scan stack and evaluate which sp is most likely the beginning of the stackframe

	// scan 0x400 bytes
	sint32 highestScore = -1;
	uint32 highestScoreSP = sp;
	for (sint32 i = 0; i < 0x100; i++)
	{
		uint32 sampleSP = sp + i * 4;
		sint32 score = ScoreStackTrace(thread, sampleSP);
		if (score > highestScore)
		{
			highestScore = score;
			highestScoreSP = sampleSP;
		}
	}

	if (highestScoreSP != sp)
		cemuLog_log(LogType::Force, fmt::format("Trace starting at SP {0:08x} r1 = {1:08x}", highestScoreSP, sp));
	else
		cemuLog_log(LogType::Force, fmt::format("Trace starting at SP/r1 {0:08x}", highestScoreSP));

	// print stack trace
	uint32 currentStackPtr = highestScoreSP;
	uint32 stackMinAddr = _swapEndianU32(thread->stackEnd);
	uint32 stackMaxAddr = _swapEndianU32(thread->stackBase);
	for (sint32 i = 0; i < 20; i++)
	{
		uint32 nextStackPtr = memory_readU32(currentStackPtr);
		if (nextStackPtr < currentStackPtr)
			break;
		if (nextStackPtr < stackMinAddr || nextStackPtr > stackMaxAddr)
			break;

		uint32 returnAddress = 0;
		returnAddress = memory_readU32(nextStackPtr + 4);
		cemuLog_log(LogType::Force, fmt::format("SP {0:08x} ReturnAddr {1:08x}", nextStackPtr, returnAddress));

		currentStackPtr = nextStackPtr;
	}
}

typedef struct
{
	/* +0x00 */ uint32be name;
	/* +0x04 */ uint32be fileType; // 2 = font
	/* +0x08 */ uint32be kernelFilenamePtr;
	/* +0x0C */ MEMPTR<void> data;
	/* +0x10 */ uint32be size;
	/* +0x14 */ uint32be ukn14;
	/* +0x18 */ uint32be ukn18;
}coreinitShareddataEntry_t;

static_assert(sizeof(coreinitShareddataEntry_t) == 0x1C, "");

uint8* extractCafeDefaultFont(sint32* size);

MPTR placeholderFont = MPTR_NULL;
sint32 placeholderFontSize = 0;

void coreinitExport_OSGetSharedData(PPCInterpreter_t* hCPU)
{
	// parameters:
	// r3		sharedAreaId
	// r4		flags
	// r5		areaPtrPtr
	// r6		areaSizePtr

	// on real Wii U hw/sw there is a list of shared area entries starting at offset +0xF8000000
	// properly formated (each entry is 0x1C bytes) it looks like this:
	// FF CA FE 01 00 00 00 02 FF E8 47 AC F8 00 00 70 00 C8 0D 4C 00 00 00 00 FF FF FF FC
	// FF CA FE 02 00 00 00 02 FF E8 47 B7 F8 C8 0D C0 00 22 7E B4 00 00 00 00 00 00 11 D5
	// FF CA FE 03 00 00 00 02 FF E8 47 A0 F8 EA 8C 80 00 25 44 E0 00 00 00 00 FF A0 00 00
	// FF CA FE 04 00 00 00 02 FF E8 47 C2 F9 0F D1 60 00 7D 93 5C 00 00 00 00 FF FF FF FC

	uint32 sharedAreaId = hCPU->gpr[3];

	coreinitShareddataEntry_t* shareddataTable = (coreinitShareddataEntry_t*)memory_getPointerFromVirtualOffset(MEMORY_SHAREDDATA_AREA_ADDR);

	uint32 name = 0xFFCAFE01 + sharedAreaId;
	for (sint32 i = 0; i < 4; i++)
	{
		if ((uint32)shareddataTable[i].name == name)
		{
			memory_writeU32(hCPU->gpr[5], shareddataTable[i].data.GetMPTR());
			memory_writeU32(hCPU->gpr[6], (uint32)shareddataTable[i].size);
			osLib_returnFromFunction(hCPU, 1);
			return;
		}
	}
	// some games require a valid result or they will crash, return a pointer to our placeholder font
	cemuLog_log(LogType::Force, "OSGetSharedData() called by game but no shareddata fonts loaded. Use placeholder font");
	if (placeholderFont == MPTR_NULL)
	{
		// load and then return placeholder font
		uint8* placeholderFontPtr = extractCafeDefaultFont(&placeholderFontSize);
		placeholderFont = coreinit_allocFromSysArea(placeholderFontSize, 256);
		if (placeholderFont == MPTR_NULL)
			cemuLog_log(LogType::Force, "Failed to alloc placeholder font sys memory");
		memcpy(memory_getPointerFromVirtualOffset(placeholderFont), placeholderFontPtr, placeholderFontSize);
		free(placeholderFontPtr);
	}
	// return placeholder font
	memory_writeU32(hCPU->gpr[5], placeholderFont);
	memory_writeU32(hCPU->gpr[6], placeholderFontSize);
	osLib_returnFromFunction(hCPU, 1);
}

typedef struct
{
	MPTR getDriverName;
	MPTR ukn04;
	MPTR onAcquiredForeground;
	MPTR onReleaseForeground;
	MPTR ukn10;
}OSDriverCallbacks_t;

void coreinitExport_OSDriver_Register(PPCInterpreter_t* hCPU)
{
#ifdef CEMU_DEBUG_ASSERT
	cemuLog_log(LogType::Force, "OSDriver_Register(0x{:08x},0x{:08x},0x{:08x},0x{:08x},0x{:08x},0x{:08x})", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6], hCPU->gpr[7], hCPU->gpr[8]);
#endif
	OSDriverCallbacks_t* driverCallbacks = (OSDriverCallbacks_t*)memory_getPointerFromVirtualOffset(hCPU->gpr[5]);

	// todo

	osLib_returnFromFunction(hCPU, 0);
}

namespace coreinit
{
	sint32 OSGetCoreId()
	{
		return PPCInterpreter_getCoreIndex(ppcInterpreterCurrentInstance);
	}

	uint32 OSGetCoreCount()
	{
		return Espresso::CORE_COUNT;
	}

	uint32 OSIsDebuggerInitialized()
	{
		return 0;
	}

	uint32 OSIsDebuggerPresent()
	{
		return 0;
	}

	uint32 OSGetConsoleType()
	{
		return 0x03000050;
	}

	uint32 OSGetMainCoreId()
	{
		return 1;
	}

	bool OSIsMainCore()
	{
		return OSGetCoreId() == OSGetMainCoreId();
	}

	uint32 OSGetStackPointer()
	{
		return ppcInterpreterCurrentInstance->gpr[1];
	}

	void coreinitExport_ENVGetEnvironmentVariable(PPCInterpreter_t* hCPU)
	{
		cemuLog_logDebug(LogType::Force, "ENVGetEnvironmentVariable(\"{}\",0x08x,0x{:x})", (char*)memory_getPointerFromVirtualOffset(hCPU->gpr[3]), hCPU->gpr[4], hCPU->gpr[5]);
		char* envKeyStr = (char*)memory_getPointerFromVirtualOffset(hCPU->gpr[3]);
		char* outputString = (char*)memory_getPointerFromVirtualOffset(hCPU->gpr[4]);
		sint32 outputStringMaxLen = (sint32)hCPU->gpr[5];
		// also return the string "" just in case
		if (outputStringMaxLen > 0)
		{
			outputString[0] = '\0';
		}
		osLib_returnFromFunction(hCPU, 1);
	}

	void coreinit_exit(uint32 r)
	{
		cemuLog_log(LogType::Force, "The title terminated the process by calling coreinit.exit({})", (sint32)r);
        DebugLogStackTrace(coreinit::OSGetCurrentThread(), coreinit::OSGetStackPointer());
		cemu_assert_debug(false);
		// never return
		while (true) std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	bool OSIsOffBoot()
	{
		return true;
	}

	uint32 OSGetBootPMFlags()
	{
		cemuLog_logDebug(LogType::Force, "OSGetBootPMFlags() - placeholder");
		return 0;
	}

	uint32 OSGetSystemMode()
	{
		cemuLog_logDebug(LogType::Force, "OSGetSystemMode() - placeholder");
		// if this returns 2, barista softlocks shortly after boot
		return 0;
	}

	void OSPanic(const char* file, sint32 lineNumber, const char* msg)
	{
		cemuLog_log(LogType::Force, "OSPanic!");
		cemuLog_log(LogType::Force, "File: {}:{}", file, lineNumber);
		cemuLog_log(LogType::Force, "Msg: {}", msg);
		DebugLogStackTrace(coreinit::OSGetCurrentThread(), coreinit::OSGetStackPointer());
#ifdef CEMU_DEBUG_ASSERT
		while (true) std::this_thread::sleep_for(std::chrono::milliseconds(100));
#endif
	}

	void InitializeCore()
	{
		cafeExportRegister("coreinit", OSGetCoreId, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSGetCoreCount, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSIsDebuggerInitialized, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSIsDebuggerPresent, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSGetConsoleType, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSGetMainCoreId, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSIsMainCore, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSGetStackPointer, LogType::CoreinitThread);

		osLib_addFunction("coreinit", "ENVGetEnvironmentVariable", coreinitExport_ENVGetEnvironmentVariable);

		cafeExportRegisterFunc(coreinit_exit, "coreinit", "exit", LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSIsOffBoot, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSGetBootPMFlags, LogType::CoreinitThread);
		cafeExportRegister("coreinit", OSGetSystemMode, LogType::CoreinitThread);

		cafeExportRegister("coreinit", OSPanic, LogType::Placeholder);
	}
};

void coreinit_load()
{
	coreinit::InitializeCore();
	coreinit::InitializeSchedulerLock();
	coreinit::InitializeSysHeap();

	// allocate coreinit global data
	gCoreinitData = (coreinitData_t*)memory_getPointerFromVirtualOffset(coreinit_allocFromSysArea(sizeof(coreinitData_t), 32));
	memset(gCoreinitData, 0x00, sizeof(coreinitData_t));

	// coreinit weak links
	osLib_addVirtualPointer("coreinit", "MEMAllocFromDefaultHeap", memory_getVirtualOffsetFromPointer(&gCoreinitData->MEMAllocFromDefaultHeap));
	osLib_addVirtualPointer("coreinit", "MEMAllocFromDefaultHeapEx", memory_getVirtualOffsetFromPointer(&gCoreinitData->MEMAllocFromDefaultHeapEx));
	osLib_addVirtualPointer("coreinit", "MEMFreeToDefaultHeap", memory_getVirtualOffsetFromPointer(&gCoreinitData->MEMFreeToDefaultHeap));
	osLib_addVirtualPointer("coreinit", "__atexit_cleanup", memory_getVirtualOffsetFromPointer(&gCoreinitData->__atexit_cleanup));
	osLib_addVirtualPointer("coreinit", "__stdio_cleanup", memory_getVirtualOffsetFromPointer(&gCoreinitData->__stdio_cleanup));
	osLib_addVirtualPointer("coreinit", "__cpp_exception_cleanup_ptr", memory_getVirtualOffsetFromPointer(&gCoreinitData->__cpp_exception_cleanup_ptr));
	osLib_addVirtualPointer("coreinit", "__cpp_exception_init_ptr", memory_getVirtualOffsetFromPointer(&gCoreinitData->__cpp_exception_init_ptr));

	// init GHS and threads
	coreinit::PrepareGHSRuntime();
	coreinit::InitializeThread();

	// reset threads
	activeThreadCount = 0;
	// init submodules
	coreinit::InitializeMEM();
	coreinit::InitializeMEMFrmHeap();
	coreinit::InitializeMEMUnitHeap();
	coreinit::InitializeMEMBlockHeap();
	coreinit::InitializeFG();
	coreinit::InitializeBSP();
	coreinit::InitializeMCP();
	coreinit::InitializeOverlayArena();
	coreinit::InitializeDynLoad();
	coreinit::InitializeGHS();
	coreinit::InitializeHWInterface();
	coreinit::InitializeAtomic();
	coreinit::InitializeMemory();
	coreinit::InitializeIM();
	coreinit::InitializeLC();
	coreinit::InitializeMP();
	coreinit::InitializeTimeAndCalendar();
	coreinit::InitializeAlarm();
	coreinit::InitializeFS();
	coreinit::InitializeSystemInfo();
	coreinit::InitializeConcurrency();
	coreinit::InitializeSpinlock();
	coreinit::InitializeMessageQueue();
	coreinit::InitializeIPC();
	coreinit::InitializeIPCBuf();
	coreinit::InitializeMemoryMapping();
	coreinit::InitializeCodeGen();
	coreinit::InitializeCoroutine();
	coreinit::InitializeOSScreen();
	
	// legacy mem stuff
	coreinit::expheap_load();

	// misc exports
	coreinit::miscInit();
	osLib_addFunction("coreinit", "OSGetSharedData", coreinitExport_OSGetSharedData);
	osLib_addFunction("coreinit", "UCReadSysConfig", coreinitExport_UCReadSysConfig);
	osLib_addFunction("coreinit", "OSDriver_Register", coreinitExport_OSDriver_Register);

	// async callbacks
	InitializeAsyncCallback();
}
