#include "Cafe/OS/common/OSCommon.h"

// APD = Automatic Power Down

namespace coreinit
{
	#define IM_ERROR_NONE 0

	void coreinitExport_IMIsAPDEnabledBySysSettings(PPCInterpreter_t* hCPU)
	{
		debug_printf("IMIsAPDEnabledBySysSettings(0x%08x)\n", hCPU->gpr[3]);
		ppcDefineParamTypePtr(isAPDEnabled, uint32be, 0);
		*isAPDEnabled = 0;
		osLib_returnFromFunction(hCPU, IM_ERROR_NONE);
	}

	void coreinitExport_IMGetTimeBeforeAPD(PPCInterpreter_t* hCPU)
	{
		// parameters:
		// r3	uint32*		returns the remaining number of seconds until auto-shutdown
		memory_writeU32(hCPU->gpr[3], 60 * 30); // 30 minutes
		osLib_returnFromFunction(hCPU, IM_ERROR_NONE);
	}

	void coreinitExport_IMGetTimeBeforeDimming(PPCInterpreter_t* hCPU)
	{
		// parameters:
		// r3	uint32*		returns the remaining number of seconds until dimming
		memory_writeU32(hCPU->gpr[3], 60 * 30); // 30 minutes
		osLib_returnFromFunction(hCPU, IM_ERROR_NONE);
	}

	bool imDimIsEnabled = true;

	void coreinitExport_IMEnableDim(PPCInterpreter_t* hCPU)
	{
		imDimIsEnabled = true;
		osLib_returnFromFunction(hCPU, IM_ERROR_NONE);
	}

	void coreinitExport_IMIsDimEnabled(PPCInterpreter_t* hCPU)
	{
		// parameters:
		// r3	uint32*		returns the remaining number of seconds until auto-shutdown
		memory_writeU32(hCPU->gpr[3], imDimIsEnabled ? 1 : 0); // enabled
		osLib_returnFromFunction(hCPU, IM_ERROR_NONE);
	}

	void coreinitExport_IMGetAPDPeriod(PPCInterpreter_t* hCPU)
	{
		cemuLog_logDebug(LogType::Force, "IMGetAPDPeriod(0x{:08x})", hCPU->gpr[3]);
		// parameters:
		// r3	uint32*		returns the number of seconds until auto-shutdown occurs
		memory_writeU32(hCPU->gpr[3], 600);
		osLib_returnFromFunction(hCPU, IM_ERROR_NONE);
	}

	void coreinitExport_IM_GetParameter(PPCInterpreter_t* hCPU)
	{
		cemuLog_logDebug(LogType::Force, "IM_GetParameter()");

		ppcDefineParamS32(imHandle, 0); // handle from IM_Open()
		ppcDefineParamS32(uknR4, 1);
		ppcDefineParamS32(parameterId, 2);
		ppcDefineParamStructPtr(output, void, 3);
		ppcDefineParamS32(uknR7, 4);
		ppcDefineParamS32(uknR8, 5);

		if (parameterId == 0)
		{
			// inactive seconds
			*(uint32be*)output = 600;
		}
		else
		{
			cemu_assert_unimplemented();
		}

		osLib_returnFromFunction(hCPU, IM_ERROR_NONE);
	}

	void coreinitExport_IM_GetRuntimeParameter(PPCInterpreter_t* hCPU)
	{
		cemuLog_logDebug(LogType::Force, "IM_GetRuntimeParameter()");

		ppcDefineParamS32(parameterId, 0);
		ppcDefineParamStructPtr(output, void, 1);

		if (parameterId == 8)
		{
			// indicates if last session was ended due to auto-power-down
			*(uint32be*)output = 0;
		}
		else
		{
			cemu_assert_unimplemented();
		}

		osLib_returnFromFunction(hCPU, IM_ERROR_NONE);
	}

	void coreinitExport_IM_GetHomeButtonParams(PPCInterpreter_t* hCPU)
	{
		debug_printf("IM_GetHomeButtonParams(...)\n");
		ppcDefineParamS32(imObj, 0);
		ppcDefineParamMPTR(ipcBuf, 1);
		ppcDefineParamMPTR(paramOut, 2);
		ppcDefineParamS32(uknR6, 3);
		ppcDefineParamS32(uknR7, 4);

		// todo
		// note: No idea what these values mean. But they were chosen so that the Browser (surf.rpx) does not OSPanic()
		memory_writeU32(paramOut + 0x0, 0);
		memory_writeU32(paramOut + 0x4, 0);

		// for scope.rpx (Download Manager)
		//memory_writeU32(paramOut + 0x0, 1);
		//memory_writeU32(paramOut + 0x4, 2); // some sort of index (starting at 1?)


		osLib_returnFromFunction(hCPU, IM_ERROR_NONE);
	}

	void InitializeIM()
	{
		osLib_addFunction("coreinit", "IMIsAPDEnabledBySysSettings", coreinitExport_IMIsAPDEnabledBySysSettings);
		osLib_addFunction("coreinit", "IMGetTimeBeforeAPD", coreinitExport_IMGetTimeBeforeAPD);
		osLib_addFunction("coreinit", "IMGetTimeBeforeDimming", coreinitExport_IMGetTimeBeforeDimming);
		osLib_addFunction("coreinit", "IMEnableDim", coreinitExport_IMEnableDim);
		osLib_addFunction("coreinit", "IMIsDimEnabled", coreinitExport_IMIsDimEnabled);
		osLib_addFunction("coreinit", "IMGetAPDPeriod", coreinitExport_IMGetAPDPeriod);
		osLib_addFunction("coreinit", "IM_GetHomeButtonParams", coreinitExport_IM_GetHomeButtonParams);
		osLib_addFunction("coreinit", "IM_GetParameter", coreinitExport_IM_GetParameter);
		osLib_addFunction("coreinit", "IM_GetRuntimeParameter", coreinitExport_IM_GetRuntimeParameter);
	}

};
