
struct PPCInterpreterLLEContext_t
{
	uint8 padding[1024 * 128]; // reserved memory for stack (for recompiler mode)
	PPCInterpreter_t cores[3];
};

PPCInterpreterGlobal_t globalCPUState = { 0 };

void PPCCoreLLE_initCore(PPCInterpreter_t* hCPU, uint32 coreIndex)
{
	hCPU->spr.UPIR = coreIndex;
	hCPU->global = &globalCPUState;
}

#define SCR_C2		(0x200000) // enable core 2
#define SCR_C1		(0x400000) // enable core 1

typedef struct  
{
	uint32be ukn000;
	uint32be ukn004;
	uint32be ukn008;
	uint32be ukn00C;
	uint32be ukn010;
	uint32be ukn014;
	uint32be busFreq;
	uint32be ukn01C;
	uint32be ukn020[4];
	uint32be ukn030[4];
	uint32be ukn040[4];
	uint32be ukn050[4];
	uint32be ukn060[4];
	uint32be ukn070[4];
	uint32be ukn080[4];
	uint32be ukn090[4];
	uint32be ukn0A0[4];
	uint32be ukn0B0[4];
	uint32be ukn0C0;
	struct  
	{
		uint32be id;
		uint32be baseAddress;
		uint32be size;
	}ramInfo[3];
	uint32 ukn0E8;
	uint32 ukn0EC;
	uint32 ukn0F0[4];
	uint32 ukn100[8];
	uint32 ukn120[8];
	uint32 ukn140[8];
	uint32 ukn160[8];
	uint32 ukn180[8];
	uint32 ukn1A0[8];
	uint32 ukn1C0[8];
	uint32 ukn1E0[8];
	uint32 ukn200[8];
	uint32 ukn220[8];
	uint32 ukn240[8];
	uint32 ukn260[8];
	uint32 ukn280[8];
	uint32 ukn2A0[8];
	uint32 ukn2C0[8];
	uint32 ukn2E0[8];
	uint32 ukn300[8];
	uint32 ukn320[8];
	uint32 ukn340[8];
	uint32 ukn360[8];
	uint32 ukn380[8];
	uint32be ukn3A0;
	uint32be ukn3A4;
	uint32be ukn3A8;
	uint32be ukn3AC;
	uint32be ukn3B0;
	uint32be smdpAreaPtr;
	uint32be ukn3B8;
	uint32be ukn3BC;
	uint32 ukn3C0[8];
	uint32 ukn3E0[8];
	uint32 ukn400;
	uint32 ukn404;
	uint32 ukn408;
}ppcBootParamBlock_t; // for kernel 5.5.2

static_assert(offsetof(ppcBootParamBlock_t, ramInfo) == 0xC4, "");
static_assert(offsetof(ppcBootParamBlock_t, busFreq) == 0x18, "");
static_assert(offsetof(ppcBootParamBlock_t, smdpAreaPtr) == 0x3B4, "");
static_assert(offsetof(ppcBootParamBlock_t, ukn400) == 0x400, "");

void PPCCoreLLE_setupBootParamBlock()
{
	ppcBootParamBlock_t* bootParamBlock = (ppcBootParamBlock_t*)memory_getPointerFromPhysicalOffset(0x01FFF000);
	memset(bootParamBlock, 0, sizeof(ppcBootParamBlock_t));

	// setup RAM info
	//PPCBaseAddress	0x8000000	0x00000000	0x28000000
	//PPCSize			0x120000	0x2000000	0xA8000000

	bootParamBlock->ukn004 = 0x40C;

	bootParamBlock->busFreq = ESPRESSO_BUS_CLOCK;

	bootParamBlock->ramInfo[0].id = 0;
	bootParamBlock->ramInfo[0].baseAddress = 0x8000000;
	bootParamBlock->ramInfo[0].size = 0x120000;
	bootParamBlock->ramInfo[1].id = 1;
	bootParamBlock->ramInfo[1].baseAddress = 0x00000000;
	bootParamBlock->ramInfo[1].size = 0x2000000;
	bootParamBlock->ramInfo[2].id = 2;
	bootParamBlock->ramInfo[2].baseAddress = 0x28000000;
	bootParamBlock->ramInfo[2].size = 0xA8000000;

}
typedef struct
{
	uint32be magic;
	uint32be count;
	uint32 _padding08[14];
	/* +0x0040 */ uint32be commandsReadIndex; // written by IOSU
	uint32 _padding44[15];
	/* +0x0080 */ uint32be commandsWriteIndex;
	uint32 _padding84[15];
	/* +0x00C0 */ uint32be resultsReadIndex;
	uint32 _paddingC4[15];
	/* +0x0100 */ uint32be resultsWriteIndex; // written by IOSU
	uint32 _padding104[15];
	/* +0x0140 */ uint32be commandPtrs[0xC00];
	/* +0x3140 */ uint32be resultPtrs[0xC00];
}smdpArea_t;

static_assert(offsetof(smdpArea_t, commandsReadIndex) == 0x0040, "");
static_assert(offsetof(smdpArea_t, commandsWriteIndex) == 0x0080, "");
static_assert(offsetof(smdpArea_t, resultsReadIndex) == 0x00C0, "");
static_assert(offsetof(smdpArea_t, resultsWriteIndex) == 0x0100, "");
static_assert(offsetof(smdpArea_t, resultPtrs) == 0x3140, "");

typedef struct  
{
	uint32be type;
	uint32be ukn04;
	uint32be ukn08;
	uint32be ukn0C;
	uint32be ukn10;
	uint32be ukn14;
	uint32be ukn18;
	uint32be ukn1C;
	uint32be ukn20;
	uint32be ukn24;
	uint32be ukn28;
	uint32be ukn2C;
}smdpCommand_t;

void smdpArea_pushResult(smdpArea_t* smdpArea, MPTR result)
{
	//smdpArea.
	smdpArea->resultPtrs[(uint32)smdpArea->resultsWriteIndex] = result;
	smdpArea->resultsWriteIndex = ((uint32)smdpArea->resultsWriteIndex + 1)%(uint32)smdpArea->count;
}

void smdpArea_processCommand(smdpArea_t* smdpArea, smdpCommand_t* cmd)
{
	if (cmd->type == 1)
	{
		cmd->ukn08 = 1;
		// cmd->ukn2C ?
		cemuLog_logDebug(LogType::Force, "SMDP command received - todo");
		smdpArea_pushResult(smdpArea, memory_getVirtualOffsetFromPointer(cmd));
	}
	else
	{
		assert_dbg();
	}
}

void smdpArea_thread()
{
	while (true)
	{
		ppcBootParamBlock_t* bootParamBlock = (ppcBootParamBlock_t*)memory_getPointerFromPhysicalOffset(0x01FFF000);
		if(bootParamBlock->smdpAreaPtr != MPTR_NULL)	
		{ 
			smdpArea_t* smdpArea = (smdpArea_t*)memory_getPointerFromPhysicalOffset(bootParamBlock->smdpAreaPtr);
			if (smdpArea->magic == 'smdp')
			{
				uint32 cmdReadIndex = smdpArea->commandsReadIndex;
				uint32 cmdWriteIndex = smdpArea->commandsWriteIndex;
				if (cmdReadIndex != cmdWriteIndex)
				{
					// new command
					smdpArea_processCommand(smdpArea, (smdpCommand_t*)memory_getPointerFromPhysicalOffset(smdpArea->commandPtrs[cmdReadIndex]));
					// increment read counter
					cmdReadIndex = (cmdReadIndex + 1) % (uint32)smdpArea->count;
					smdpArea->commandsReadIndex = cmdReadIndex;
				}
			}
		}
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

void PPCCoreLLE_startSingleCoreScheduler(uint32 entrypoint)
{
	PPCInterpreterLLEContext_t* cpuContext = (PPCInterpreterLLEContext_t*)malloc(sizeof(PPCInterpreterLLEContext_t));
	memset(cpuContext, 0, sizeof(PPCInterpreterLLEContext_t));

	PPCCoreLLE_setupBootParamBlock();

	PPCCoreLLE_initCore(cpuContext->cores + 0, 0);
	PPCCoreLLE_initCore(cpuContext->cores + 1, 1);
	PPCCoreLLE_initCore(cpuContext->cores + 2, 2);
	
	cpuContext->cores[0].instructionPointer = entrypoint;
	cpuContext->cores[1].instructionPointer = 0xFFF00100;
	cpuContext->cores[2].instructionPointer = 0xFFF00100;
	// todo - calculate instruction pointer when core 1/2 is enabled (because entry point is determined by MSR exception vector bit)
	std::thread(smdpArea_thread).detach();

	while (true)
	{
		for (uint32 coreIndex = 0; coreIndex < 3; coreIndex++)
		{
			PPCInterpreter_t* hCPU = cpuContext->cores+coreIndex;
			PPCInterpreter_setCurrentInstance(hCPU);
			if (coreIndex == 1)
			{
				// check SCR core 1 enable bit
				if ((globalCPUState.sprGlobal.scr&SCR_C1) == 0)
					continue;
			}
			else if (coreIndex == 2)
			{
				// check SCR core 2 enable bit
				if ((globalCPUState.sprGlobal.scr&SCR_C2) == 0)
					continue;
			}

			hCPU->remainingCycles = 10000;
			while ((--hCPU->remainingCycles) >= 0)
			{
				PPCInterpreterFull_executeInstruction(hCPU);
			};
		}
	}
	assert_dbg();
}
