#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/HW/Espresso/PPCCallback.h"
#include "Cafe/OS/libs/coreinit/coreinit.h"
#include "Cafe/OS/RPL/rpl.h"
#include "Cafe/OS/libs/padscore/padscore.h"
#include "Cafe/OS/libs/coreinit/coreinit_SysHeap.h"
#include "Cafe/OS/libs/coreinit/coreinit_Alarm.h"
#include "Cafe/OS/libs/vpad/vpad.h"
#include "Cafe/OS/libs/coreinit/coreinit_GHS.h"
#include "Cafe/OS/libs/coreinit/coreinit_DynLoad.h"
#include "Cafe/OS/libs/coreinit/coreinit_MEM.h"
#include "Cafe/OS/libs/coreinit/coreinit_FG.h"
#include "Cafe/CafeSystem.h"

extern MPTR _entryPoint;
extern RPLModule* applicationRPX;

typedef struct
{
	MPTR argv[32];
	uint32be argc;
	char argStorage[0x1000];
}coreinitInit_t;

coreinitInit_t* _coreinitInfo = nullptr;

MPTR OSAllocFromSystem(uint32 size, uint32 alignment)
{
	return coreinit_allocFromSysArea(size, alignment);
}

void OSFreeToSystem(MPTR mem)
{
	coreinit_freeToSysArea(mem);
}

extern std::string _pathToExecutable;
sint32 argStorageIndex;

void _AddArg(const char* arg, sint32 len)
{
	uint32 argc = _coreinitInfo->argc;

	char* argStorageStr = _coreinitInfo->argStorage + argStorageIndex;
	memcpy(argStorageStr, arg, len);
	argStorageStr[len] = '\0';
	argStorageIndex += (sint32)strlen(arg) + 1;

	_coreinitInfo->argv[argc] = _swapEndianU32(memory_getVirtualOffsetFromPointer(argStorageStr));
	_coreinitInfo->argc = argc+1;
}

sint32 _GetArgLength(const char* arg)
{
	sint32 c = 0;
	while (*arg)
	{
		if (*arg == ' ')
			break; // end at whitespace
		cemu_assert_debug(*arg != '\"' && *arg != '\''); // todo
		arg++;
		c++;
	}
	return c;
}

static std::string GetLaunchArgs()
{
	std::string argStr = CafeSystem::GetForegroundTitleArgStr();
	if(std::vector<std::string> overrideArgs; CafeSystem::GetOverrideArgStr(overrideArgs))
	{
		// args are overriden by launch directive (OSLaunchTitleByPath)
		// keep the rpx path but use the arguments from the override
		if (size_t pos = argStr.find(' '); pos != std::string::npos)
			argStr.resize(pos);
		for(size_t i=0; i<overrideArgs.size(); i++)
		{
			argStr += " ";
			argStr += overrideArgs[i];
		}
	}
	return argStr;
}

void CafeInit()
{
	// extract executable filename
	sint32 rpxPathStart = (sint32)_pathToExecutable.size() - 1;
	if (rpxPathStart > 0)
	{
		while (rpxPathStart > 0 && _pathToExecutable[rpxPathStart-1] != '/')
			rpxPathStart--;
	}
	else
	{
		rpxPathStart = 0;
	}
	
	std::string_view rpxFileName(_pathToExecutable.data() + rpxPathStart, _pathToExecutable.size() - rpxPathStart);

	argStorageIndex = 0;
	_coreinitInfo->argc = 0;
	_AddArg(rpxFileName.data(), rpxFileName.size());
	strcpy((char*)_coreinitInfo->argStorage, std::string(rpxFileName).c_str());

	std::string _argStr = GetLaunchArgs();
	CafeSystem::UnsetOverrideArgs(); // make sure next launch doesn't accidentally use the same arguments
	const char* argString = _argStr.c_str();
	// attach parameters from arg string
	if (argString && argString[0] != '\0')
	{
		const char* t = strstr(argString, ".rpx");
		if (t)
		{
			t += 4;
			while (*t)
			{
				// skip all whitespace
				while (*t)
				{
					if (*t == ' ')
					{
						t++;
					}
					else
						break;
				}
				// get length of arg
				sint32 argLength = _GetArgLength(t);
				if (argLength > 0)
				{
					// add arg
					_AddArg(t, argLength);
					// next
					t += argLength;
				}
			}
		}
		else
		{
			cemuLog_logDebug(LogType::Force, "Unable to find end of rpx file name in arg string");
		}
	}
	// setup UGQR
	PPCInterpreter_t* hCPU = PPCInterpreter_getCurrentInstance();
	hCPU->spr.UGQR[0 + 2] = 0x00040004;
	hCPU->spr.UGQR[0 + 3] = 0x00050005;
	hCPU->spr.UGQR[0 + 4] = 0x00060006;
	hCPU->spr.UGQR[0 + 5] = 0x00070007;
	coreinit::InitForegroundBucket();
	coreinit::InitSysHeap();
}

struct PreinitUserHeapStruct
{
	MEMPTR<coreinit::MEMHeapBase> heapTempMEM1;
	MEMPTR<coreinit::MEMHeapBase> heapTempFG;
	MEMPTR<coreinit::MEMHeapBase> heapTempMEM2;
};

SysAllocator<PreinitUserHeapStruct> g_preinitUserParam;

void InitCafeHeaps()
{
	// init default heaps
	g_preinitUserParam->heapTempMEM1 = nullptr;
	g_preinitUserParam->heapTempFG = nullptr;
	g_preinitUserParam->heapTempMEM2 = nullptr;
	coreinit::InitDefaultHeaps(g_preinitUserParam->heapTempMEM1, g_preinitUserParam->heapTempFG, g_preinitUserParam->heapTempMEM2);
	// if __preinit_user export exists in main executable, run it and pass our heaps
	MPTR exportAddr = applicationRPX ? RPLLoader_FindRPLExport(applicationRPX, "__preinit_user", false) : MPTR_NULL;
	if (exportAddr != MPTR_NULL)
	{
		PPCCoreCallback(exportAddr, &g_preinitUserParam->heapTempMEM1, &g_preinitUserParam->heapTempFG, &g_preinitUserParam->heapTempMEM2);
	}
	// setup heaps
	if (g_preinitUserParam->heapTempMEM1 != nullptr)
		coreinit::MEMSetBaseHeapHandle(0, g_preinitUserParam->heapTempMEM1);
	if (g_preinitUserParam->heapTempFG != nullptr)
		coreinit::MEMSetBaseHeapHandle(8, g_preinitUserParam->heapTempFG);
	if (g_preinitUserParam->heapTempMEM2 != nullptr)
		coreinit::MEMSetBaseHeapHandle(1, g_preinitUserParam->heapTempMEM2);
}

MPTR CoreInitEntry(sint32 argc, MPTR argv)
{
	const char* rpxPath = (char*)memory_getPointerFromVirtualOffset(memory_readU32(argv + 0));
	InitCafeHeaps();
	// do a dummy allocation via the OSDynLoad allocator
	// Watch Dogs relies on this to correctly set up its malloc() allocator
	// must be larger than 0x190 to trigger creation of a new memory segment. But also must not be close to page alignment (0x1000) or else the bug will trigger
	void* dummyAlloc = coreinit::OSDynLoad_AllocatorAlloc(0x500, 0x4);
	if (dummyAlloc)
		coreinit::OSDynLoad_AllocatorFree(dummyAlloc);
	return _entryPoint;
}

sint32 _coreinitTitleEntryPoint;

void coreinit_start(PPCInterpreter_t* hCPU)
{
	_coreinitInfo = (coreinitInit_t*)memory_getPointerFromVirtualOffset(coreinit_allocFromSysArea(sizeof(coreinitInit_t), 32));
	memset(_coreinitInfo, 0, sizeof(coreinitInit_t));

	CafeInit();
	_coreinitTitleEntryPoint = CoreInitEntry(_coreinitInfo->argc, memory_getVirtualOffsetFromPointer(_coreinitInfo->argv));
	
	RPLLoader_CallEntrypoints();

	// init vpadbase (todo - simulate entrypoints for HLE modules)
	padscore::start();
	vpad::start();

	// continue at main executable entrypoint
	hCPU->gpr[4] = memory_getVirtualOffsetFromPointer(_coreinitInfo->argv);
	hCPU->gpr[3] = _coreinitInfo->argc;
	hCPU->instructionPointer = _coreinitTitleEntryPoint;
}
