#pragma once
#include "Cafe/HW/MMU/MMU.h"

enum
{
	CPUException_NOTHING,
	CPUException_FPUUNAVAILABLE,
	CPUException_EXTERNAL,
	CPUException_SYSTEMCALL
};

#define PPC_LWARX_RESERVATION_MAX	(4)

union FPR_t
{
	double fpr;
	struct
	{
		double fp0;
		double fp1;
	};
	struct
	{
		uint64 guint; 
	};
	struct
	{
		uint64 fp0int;
		uint64 fp1int;
	};
};

typedef struct  
{
	struct  
	{
		uint32 scr;
		uint32 car;
		//uint32 bcr;
	}sprGlobal;
	uint64 tb;
}PPCInterpreterGlobal_t;

struct PPCInterpreter_t
{
	uint32 instructionPointer;
	uint32 gpr[32];
	FPR_t fpr[32];
	uint32 fpscr;
	uint8 cr[32]; // 0 -> bit not set, 1 -> bit set (upper 7 bits of each byte must always be zero) (cr0 starts at index 0, cr1 at index 4 ..)
	uint8 xer_ca;  // carry from xer
	uint8 LSQE;
	uint8 PSE;
	// thread remaining cycles
	sint32 remainingCycles; // if this value goes below zero, the next thread is scheduled
	sint32 skippedCycles; // number of skipped cycles
	struct  
	{
		uint32 LR;
		uint32 CTR;
		uint32 XER;
		uint32 UPIR;
		uint32 UGQR[8];
	}spr;
	// LWARX and STWCX
	uint32 reservedMemAddr;
	uint32 reservedMemValue;
	// temporary storage for recompiler
	FPR_t temporaryFPR[8];
	uint32 temporaryGPR[4];
	// values below this are not used by Cafe OS usermode
	struct  
	{
		uint32 fpecr; // is this the same register as fpscr ?
		uint32 DEC;
		uint32 srr0;
		uint32 srr1;
		uint32 PVR;
		uint32 msr;
		uint32 sprg[4];
		// DSI/ISI
		uint32 dar;
		uint32 dsisr;
		// DMA
		uint32 dmaU;
		uint32 dmaL;
		// MMU 
		uint32 dbatU[8];
		uint32 dbatL[8];
		uint32 ibatU[8];
		uint32 ibatL[8];
		uint32 sr[16];
		uint32 sdr1;
	}sprExtended;
	// global CPU values
	PPCInterpreterGlobal_t* global;
	// interpreter control
	bool memoryException;
	// core context (starts at 0xFFFFFF00?)
	/* 0xFFFFFFE4 */ uint32 coreInterruptMask;

	// extra variables for recompiler
	void* rspTemp;
};

// parameter access (legacy C style)

static uint32 PPCInterpreter_getCallParamU32(PPCInterpreter_t* hCPU, uint32 index)
{
	if (index >= 8)
		return memory_readU32(hCPU->gpr[1] + 8 + (index - 8) * 4);
	return hCPU->gpr[3 + index];
}

static uint64 PPCInterpreter_getCallParamU64(PPCInterpreter_t* hCPU, uint32 index)
{
	uint64 v = ((uint64)PPCInterpreter_getCallParamU32(hCPU, index)) << 32ULL;
	v |= ((uint64)PPCInterpreter_getCallParamU32(hCPU, index+1));
	return v;
}

#define ppcGetCallParamU32(__index) PPCInterpreter_getCallParamU32(hCPU, __index)
#define ppcGetCallParamU16(__index) ((uint16)(PPCInterpreter_getCallParamU32(hCPU, __index)&0xFFFF))
#define ppcGetCallParamU8(__index) ((uint8)(PPCInterpreter_getCallParamU32(hCPU, __index)&0xFF))
#define ppcGetCallParamStruct(__index, __type) ((__type*)memory_getPointerFromVirtualOffsetAllowNull(PPCInterpreter_getCallParamU32(hCPU, __index)))

// legacy way of accessing parameters
#define ppcDefineParamU32(__name, __index) uint32 __name = PPCInterpreter_getCallParamU32(hCPU, __index)
#define ppcDefineParamU16(__name, __index) uint16 __name = (uint16)PPCInterpreter_getCallParamU32(hCPU, __index)
#define ppcDefineParamU32BEPtr(__name, __index) uint32be* __name = (uint32be*)((uint8*)memory_getPointerFromVirtualOffsetAllowNull(PPCInterpreter_getCallParamU32(hCPU, __index)))
#define ppcDefineParamS32(__name, __index) sint32 __name = (sint32)PPCInterpreter_getCallParamU32(hCPU, __index)
#define ppcDefineParamU64(__name, __index) uint64 __name = PPCInterpreter_getCallParamU64(hCPU, __index)
#define ppcDefineParamMPTR(__name, __index) MPTR __name = (MPTR)PPCInterpreter_getCallParamU32(hCPU, __index)
#define ppcDefineParamMEMPTR(__name, __type, __index) MEMPTR<__type> __name{PPCInterpreter_getCallParamU32(hCPU, __index)}
#define ppcDefineParamU8(__name, __index) uint8 __name = (PPCInterpreter_getCallParamU32(hCPU, __index)&0xFF)
#define ppcDefineParamStructPtr(__name, __type, __index) __type* __name = ((__type*)memory_getPointerFromVirtualOffsetAllowNull(PPCInterpreter_getCallParamU32(hCPU, __index)))
#define ppcDefineParamTypePtr(__name, __type, __index) __type* __name = ((__type*)memory_getPointerFromVirtualOffsetAllowNull(PPCInterpreter_getCallParamU32(hCPU, __index)))
#define ppcDefineParamPtr(__name, __type, __index) __type* __name = ((__type*)memory_getPointerFromVirtualOffsetAllowNull(PPCInterpreter_getCallParamU32(hCPU, __index)))
#define ppcDefineParamStr(__name, __index) char* __name = ((char*)memory_getPointerFromVirtualOffsetAllowNull(PPCInterpreter_getCallParamU32(hCPU, __index)))
#define ppcDefineParamUStr(__name, __index) uint8* __name = ((uint8*)memory_getPointerFromVirtualOffsetAllowNull(PPCInterpreter_getCallParamU32(hCPU, __index)))
#define ppcDefineParamWStr(__name, __index) wchar_t* __name = ((wchar_t*)memory_getPointerFromVirtualOffsetAllowNull(PPCInterpreter_getCallParamU32(hCPU, __index)))
#define ppcDefineParamWStrBE(__name, __index) uint16be* __name = ((uint16be*)memory_getPointerFromVirtualOffsetAllowNull(PPCInterpreter_getCallParamU32(hCPU, __index)))

// GPR constants

#define GPR_SP 1

// interpreter functions

PPCInterpreter_t* PPCInterpreter_createInstance(unsigned int Entrypoint);
PPCInterpreter_t* PPCInterpreter_getCurrentInstance();

uint64 PPCInterpreter_getMainCoreCycleCounter();

void PPCInterpreter_nextInstruction(PPCInterpreter_t* cpuInterpreter);
void PPCInterpreter_jumpToInstruction(PPCInterpreter_t* cpuInterpreter, uint32 newIP);

void PPCInterpreterSlim_executeInstruction(PPCInterpreter_t* hCPU);
void PPCInterpreterFull_executeInstruction(PPCInterpreter_t* hCPU);

// misc

uint32 PPCInterpreter_getXER(PPCInterpreter_t* hCPU);
void PPCInterpreter_setXER(PPCInterpreter_t* hCPU, uint32 v);

// Wii U clocks (deprecated. Moved to Espresso/Const.h)
#define ESPRESSO_CORE_CLOCK       1243125000
#define ESPRESSO_BUS_CLOCK        248625000
#define ESPRESSO_TIMER_CLOCK      (ESPRESSO_BUS_CLOCK/4) // 62156250

#define ESPRESSO_CORE_CLOCK_TO_TIMER_CLOCK(__cc) ((__cc)/20ULL)

// interrupt vectors
#define CPU_EXCEPTION_DSI			0x00000300
#define CPU_EXCEPTION_INTERRUPT		0x00000500 // todo: validate
#define CPU_EXCEPTION_FPUUNAVAIL	0x00000800 // todo: validate
#define CPU_EXCEPTION_SYSTEMCALL	0x00000C00 // todo: validate
#define CPU_EXCEPTION_DECREMENTER	0x00000900 // todo: validate

// FPU available check
//#define FPUCheckAvailable() if ((hCPU->msr & MSR_FP) == 0) { IPTException(hCPU, CPU_EXCEPTION_FPUUNAVAIL); return; }
#define FPUCheckAvailable() // since the emulated code always runs in usermode we can assume that MSR_FP is always set

// spr
void PPCSpr_set(PPCInterpreter_t* hCPU, uint32 spr, uint32 newValue);
uint32 PPCSpr_get(PPCInterpreter_t* hCPU, uint32 spr);

uint32 PPCInterpreter_getCoreIndex(PPCInterpreter_t* hCPU);
uint32 PPCInterpreter_getCurrentCoreIndex();

// decrement register
void PPCInterpreter_setDEC(PPCInterpreter_t* hCPU, uint32 newValue);

// timing for main processor
extern volatile uint64 ppcMainThreadCycleCounter;
extern uint64 ppcCyclesSince2000; // on init this is set to the cycles that passed since 1.1.2000
extern uint64 ppcCyclesSince2000TimerClock; // on init this is set to the cycles that passed since 1.1.2000 / 20
extern uint64 ppcCyclesSince2000_UTC;
extern uint64 ppcMainThreadDECCycleValue; // value that was set to dec register
extern uint64 ppcMainThreadDECCycleStart; // at which cycle the dec register was set

// PPC timer
void PPCTimer_init();
void PPCTimer_waitForInit();
uint64 PPCTimer_getFromRDTSC();

uint64 PPCTimer_microsecondsToTsc(uint64 us);
uint64 PPCTimer_tscToMicroseconds(uint64 us);
uint64 PPCTimer_getRawTsc();

void PPCTimer_start();

// core info and control
extern uint32 ppcThreadQuantum;

extern thread_local PPCInterpreter_t *ppcInterpreterCurrentInstance;
uint8* PPCInterpreterGetAndModifyStackPointer(sint32 offset);
uint8* PPCInterpreterGetStackPointer();
void PPCInterpreterModifyStackPointer(sint32 offset);

uint32 PPCInterpreter_makeCallableExportDepr(void (*ppcCallableExport)(PPCInterpreter_t* hCPU));

static inline float flushDenormalToZero(float f)
{
	uint32 v = *(uint32*)&f;
	return *(float*)&v;
}

// HLE interface

typedef void(*HLECALL)(PPCInterpreter_t* hCPU);

typedef sint32 HLEIDX;
HLEIDX PPCInterpreter_registerHLECall(HLECALL hleCall);
HLECALL PPCInterpreter_getHLECall(HLEIDX funcIndex);

// HLE scheduler

void PPCInterpreter_relinquishTimeslice();

void PPCCore_boostQuantum(sint32 numCycles);
void PPCCore_deboostQuantum(sint32 numCycles);

void PPCCore_switchToScheduler();
void PPCCore_switchToSchedulerWithLock();

PPCInterpreter_t* PPCCore_executeCallbackInternal(uint32 functionMPTR);
void PPCCore_init();

// LLE scheduler

void PPCCoreLLE_startSingleCoreScheduler(uint32 entrypoint);
