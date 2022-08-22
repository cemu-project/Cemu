#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/coreinit/coreinit_Coroutine.h"
#include "Cafe/HW/Espresso/PPCState.h"
#include "Cafe/HW/Espresso/Interpreter/PPCInterpreterInternal.h"
#include "Cafe/HW/MMU/MMU.h"

namespace coreinit
{

	void coreinitExport_OSInitCoroutine(PPCInterpreter_t* hCPU)
	{
		OSCoroutine* coroutine = (OSCoroutine*)memory_getPointerFromVirtualOffset(hCPU->gpr[3]);
		coroutine->lr = _swapEndianU32(hCPU->gpr[4]);
		coroutine->r1 = _swapEndianU32(hCPU->gpr[5]);
		osLib_returnFromFunction(hCPU, 0);
	}

	void coreinitCoroutine_OSSaveCoroutine(OSCoroutine* coroutine, PPCInterpreter_t* hCPU)
	{
		coroutine->lr = _swapEndianU32(hCPU->spr.LR);
		coroutine->cr = _swapEndianU32(ppc_getCR(hCPU));
		coroutine->gqr1 = _swapEndianU32(hCPU->spr.UGQR[1]);
		coroutine->r1 = _swapEndianU32(hCPU->gpr[1]);
		coroutine->r2 = _swapEndianU32(hCPU->gpr[2]);
		coroutine->r13 = _swapEndianU32(hCPU->gpr[13]);
		for (sint32 i = 14; i < 32; i++)
			coroutine->gpr[i - 14] = _swapEndianU32(hCPU->gpr[i]);
		for (sint32 i = 14; i < 32; i++)
		{
			coroutine->fpr[i - 14] = _swapEndianU64(hCPU->fpr[i].fp0int);
		}
		for (sint32 i = 14; i < 32; i++)
		{
			coroutine->psr[i - 14] = _swapEndianU64(hCPU->fpr[i].fp1int);
		}
	}

	void coreinitCoroutine_OSLoadCoroutine(OSCoroutine* coroutine, PPCInterpreter_t* hCPU)
	{
		hCPU->spr.LR = _swapEndianU32(coroutine->lr);
		ppc_setCR(hCPU, _swapEndianU32(coroutine->cr));
		hCPU->spr.UGQR[1] = _swapEndianU32(coroutine->gqr1);
		hCPU->gpr[1] = _swapEndianU32(coroutine->r1);
		hCPU->gpr[2] = _swapEndianU32(coroutine->r2);
		hCPU->gpr[13] = _swapEndianU32(coroutine->r13);
		for (sint32 i = 14; i < 32; i++)
			hCPU->gpr[i] = _swapEndianU32(coroutine->gpr[i - 14]);
		for (sint32 i = 14; i < 32; i++)
		{
			hCPU->fpr[i].fp0int = _swapEndianU64(coroutine->fpr[i - 14]);
		}
		for (sint32 i = 14; i < 32; i++)
		{
			hCPU->fpr[i].fp1int = _swapEndianU64(coroutine->psr[i - 14]);
		}
	}

	void coreinitExport_OSSwitchCoroutine(PPCInterpreter_t* hCPU)
	{
		OSCoroutine* coroutineCurrent = (OSCoroutine*)memory_getPointerFromVirtualOffset(hCPU->gpr[3]);
		OSCoroutine* coroutineNext = (OSCoroutine*)memory_getPointerFromVirtualOffsetAllowNull(hCPU->gpr[4]);
		coreinitCoroutine_OSSaveCoroutine(coroutineCurrent, hCPU);
		if (coroutineNext != NULL)
		{
			coreinitCoroutine_OSLoadCoroutine(coroutineNext, hCPU);
		}
		osLib_returnFromFunction(hCPU, 0);
	}

	void coreinitExport_OSSwitchFiberEx(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(param0, 0);
		ppcDefineParamU32(param1, 1);
		ppcDefineParamU32(param2, 2);
		ppcDefineParamU32(param3, 3);
		ppcDefineParamMPTR(newInstructionPointer, 4);
		ppcDefineParamMPTR(newStackPointer, 5);

		MPTR prevStackpointer = hCPU->gpr[1];
		hCPU->gpr[1] = newStackPointer;
		hCPU->gpr[3] = param0;
		hCPU->gpr[4] = param1;
		hCPU->gpr[5] = param2;
		hCPU->gpr[6] = param3;

		PPCCore_executeCallbackInternal(newInstructionPointer);
		uint32 returnValue = hCPU->gpr[3];

		hCPU->gpr[1] = prevStackpointer;

		osLib_returnFromFunction(hCPU, returnValue);
	}

	void InitializeCoroutine()
	{
		osLib_addFunction("coreinit", "OSInitCoroutine", coreinitExport_OSInitCoroutine);
		osLib_addFunction("coreinit", "OSSwitchCoroutine", coreinitExport_OSSwitchCoroutine);
		osLib_addFunction("coreinit", "OSSwitchFiberEx", coreinitExport_OSSwitchFiberEx);
	}
}