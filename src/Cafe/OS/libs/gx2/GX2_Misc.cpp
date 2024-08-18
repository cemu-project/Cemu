#include "Cafe/HW/Latte/ISA/RegDefines.h"
#include "Cafe/OS/common/OSCommon.h"
#include "GX2.h"
#include "config/CemuConfig.h"
#include "Cafe/OS/libs/coreinit/coreinit_Time.h"
#include "config/ActiveSettings.h"
#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "Cafe/HW/Latte/Core/LatteBufferCache.h"
#include "Cafe/HW/Latte/Core/LattePM4.h"
#include "GX2_Command.h"
#include "GX2_Event.h"
#include "GX2_Misc.h"
#include "GX2_Memory.h"
#include "GX2_Texture.h"

void gx2Export_GX2SetSwapInterval(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::GX2, "GX2SetSwapInterval({})", hCPU->gpr[3]);
	if( hCPU->gpr[3] >= 20 )
	{
		cemuLog_log(LogType::Force, "GX2SetSwapInterval() called with out of range value ({})", hCPU->gpr[3]);
	}
	else
		LatteGPUState.sharedArea->swapInterval = hCPU->gpr[3];
	osLib_returnFromFunction(hCPU, 0);
}

void gx2Export_GX2GetSwapInterval(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::GX2, "GX2GetSwapInterval()");
	osLib_returnFromFunction(hCPU, LatteGPUState.sharedArea->swapInterval);
}

extern uint64 lastSwapTime;

void gx2Export_GX2GetSwapStatus(PPCInterpreter_t* hCPU)
{
	memory_writeU32(hCPU->gpr[3], _swapEndianU32(LatteGPUState.sharedArea->flipRequestCountBE));
	memory_writeU32(hCPU->gpr[4], _swapEndianU32(LatteGPUState.sharedArea->flipExecuteCountBE));
	memory_writeU64(hCPU->gpr[5], lastSwapTime);
	memory_writeU64(hCPU->gpr[6], lastSwapTime);

	osLib_returnFromFunction(hCPU, 0);
}

void gx2Export_GX2GetGPUTimeout(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::GX2, "GX2GetGPUTimeout()");
	osLib_returnFromFunction(hCPU, 0x3E8);
}

#define GX2_INVALID_COUNTER_VALUE_U64 0xFFFFFFFFFFFFFFFFULL

void gx2Export_GX2SampleTopGPUCycle(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::GX2, "GX2SampleTopGPUCycle(0x{:08x})", hCPU->gpr[3]);
	memory_writeU64(hCPU->gpr[3], coreinit::coreinit_getTimerTick());
	osLib_returnFromFunction(hCPU, 0);
}

void gx2Export_GX2SampleBottomGPUCycle(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::GX2, "GX2SampleBottomGPUCycle(0x{:08x})", hCPU->gpr[3]);
	memory_writeU64(hCPU->gpr[3], GX2_INVALID_COUNTER_VALUE_U64);

	osLib_returnFromFunction(hCPU, 0);
	return;
	// seems like implementing this correctly causes more harm than good as games will try to dynamically scale their resolution, which our texture cache and graphic packs cant handle well. If we just never return a valid timestamp, it seems like games stop dynamically scaling resolution
	// Whats a good solution here? Should we implement it correctly and instead rely on graphic pack patches to patch out the dynamic scaling?
	// some known affected games: Wind Waker HD, Super Mario 3D World

	gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_HLE_SAMPLE_TIMER, 1));
	gx2WriteGather_submitU32AsBE(hCPU->gpr[3]); 
	osLib_returnFromFunction(hCPU, 0);
}

namespace GX2
{
	SysAllocator<uint8, 640 * 480 * 4, 0x1000> _lastFrame;
	uint32 sGX2MainCoreIndex = 0;

	void _test_AddrLib();

	void GX2Init(void* initSettings)
	{
		if (LatteGPUState.gx2InitCalled)
		{
			cemuLog_logDebug(LogType::Force, "GX2Init() called while already initialized");
			return;
		}
		uint32 coreIndex = coreinit::OSGetCoreId();
		cemuLog_log(LogType::GX2, "GX2Init() on core {} by thread 0x{:08x}", coreIndex, MEMPTR<OSThread_t>(coreinit::OSGetCurrentThread()).GetMPTR());
		sGX2MainCoreIndex = coreIndex;
		// init submodules
		GX2::GX2Init_event();
		GX2::GX2Init_writeGather();
		// init shared area
		if (LatteGPUState.sharedAreaAddr == MPTR_NULL)
		{
			LatteGPUState.sharedAreaAddr = coreinit_allocFromSysArea(sizeof(gx2GPUSharedArea_t), 0x20);
			LatteGPUState.sharedArea = (gx2GPUSharedArea_t*)memory_getPointerFromVirtualOffset(LatteGPUState.sharedAreaAddr);
		}
		// init shared variables
		LatteGPUState.sharedArea->flipRequestCountBE = _swapEndianU32(0);
		LatteGPUState.sharedArea->flipExecuteCountBE = _swapEndianU32(0);
		LatteGPUState.sharedArea->swapInterval = 1;
		// init memory handling
		GX2::GX2MEMAllocatorsInit();
		// let GPU know that GX2 is initialized
		LatteGPUState.gx2InitCalled++;
		// run tests
		_test_AddrLib();
	}

	void _GX2DriverReset()
	{
		LatteGPUState.gx2InitCalled = 0;
        sGX2MainCoreIndex = 0;
        GX2CommandResetToDefaultState();
        GX2EventResetToDefaultState();
	}

	sint32 GX2GetMainCoreId(PPCInterpreter_t* hCPU)
	{
		if (LatteGPUState.gx2InitCalled == 0)
			return -1;
		return sGX2MainCoreIndex;
	}

	void GX2ResetGPU(uint32 ukn)
	{
		cemuLog_log(LogType::Force, "GX2ResetGPU()"); // always log this
		GX2::GX2DrawDone();
	}

	void GX2SetTVBuffer(void* imageBuffePtr, uint32 imageBufferSize, E_TVRES tvResolutionMode, uint32 _surfaceFormat, E_TVBUFFERMODE bufferMode)
	{
		Latte::E_GX2SURFFMT surfaceFormat = (Latte::E_GX2SURFFMT)_surfaceFormat;
		LatteGPUState.tvBufferUsesSRGB = HAS_FLAG(surfaceFormat, Latte::E_GX2SURFFMT::FMT_BIT_SRGB);
		// todo - actually allocate a scanbuffer
	}

	void GX2SetTVGamma(float gamma)
	{
		if (abs(gamma - 1.0f) > 0.01f)
			cemuLog_logDebug(LogType::Force, "TV gamma set to {} which is not supported", gamma);
	}

	bool GX2GetLastFrame(uint32 deviceId, GX2Texture* textureOut)
	{
		// return a 480p image
		textureOut->viewFirstMip = 0;
		textureOut->viewFirstSlice = 0;
		textureOut->viewNumMips = 1;
		textureOut->viewNumSlices = 1;
		textureOut->compSel = 0x00010203;

		textureOut->surface.width = 640;
		textureOut->surface.height = 480;
		textureOut->surface.depth = 1;
		textureOut->surface.dim = Latte::E_DIM::DIM_2D;
		textureOut->surface.format = Latte::E_GX2SURFFMT::R8_G8_B8_A8_UNORM;
		textureOut->surface.tileMode = Latte::E_GX2TILEMODE::TM_LINEAR_ALIGNED;
		textureOut->surface.pitch = 0;
		textureOut->surface.resFlag = 0;
		textureOut->surface.aa = 0;

		GX2CalcSurfaceSizeAndAlignment(&textureOut->surface);

		textureOut->surface.imagePtr = _lastFrame.GetMPTR();

		GX2InitTextureRegs(textureOut);

		return true;
	}

	bool GX2GetLastFrameGammaA(uint32 deviceId, float32be* gamma)
	{
		*gamma = 1.0f;
		return true;
	}

	bool GX2GetLastFrameGammaB(uint32 deviceId, float32be* gamma)
	{
		*gamma = 1.0f;
		return true;
	}

	uint64 GX2GPUTimeToCPUTime(uint64 gpuTime)
	{
		return 0; // hack, see note in GX2SampleBottomGPUCycle
	}

	uint32 GX2GetSystemDRCMode()
	{
		return 1;
	}

	uint32 GX2IsVideoOutReady()
	{
		return 1;
	}

	void GX2Invalidate(uint32 invalidationFlags, MPTR invalidationAddr, uint32 invalidationSize)
	{
		uint32 surfaceSyncFlags = 0;

		if (invalidationFlags & 0x04)
		{
			// uniform block
			surfaceSyncFlags |= 0x8800000;
		}
		if (invalidationFlags & 0x01)
		{
			// attribute data
			surfaceSyncFlags |= 0x800000;
		}

		if (invalidationFlags & 0x40)
		{
			// CPU cache
			LatteBufferCache_notifyDCFlush(invalidationAddr, invalidationSize);
		}

		if (surfaceSyncFlags != 0)
		{
			GX2ReserveCmdSpace(5);
			// write PM4 command
			gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SURFACE_SYNC, 4)); // IT_SURFACE_SYNC + 4 data dwords
			gx2WriteGather_submitU32AsBE(surfaceSyncFlags);
			gx2WriteGather_submitU32AsBE((invalidationSize + 0xFF) >> 8); // size
			gx2WriteGather_submitU32AsBE(memory_virtualToPhysical(invalidationAddr) >> 8); // base address (divided by 0x100)
			gx2WriteGather_submitU32AsBE(0x00000004); // poll interval
		}
	}

	void GX2MiscInit()
	{
		cafeExportRegister("gx2", GX2Init, LogType::GX2);
		cafeExportRegister("gx2", GX2GetMainCoreId, LogType::GX2);
		cafeExportRegister("gx2", GX2ResetGPU, LogType::GX2);

		cafeExportRegister("gx2", GX2SetTVBuffer, LogType::GX2);
		cafeExportRegister("gx2", GX2SetTVGamma, LogType::GX2);

		cafeExportRegister("gx2", GX2GetLastFrame, LogType::GX2);
		cafeExportRegister("gx2", GX2GetLastFrameGammaA, LogType::GX2);
		cafeExportRegister("gx2", GX2GetLastFrameGammaB, LogType::GX2);

		cafeExportRegister("gx2", GX2GPUTimeToCPUTime, LogType::GX2);
		cafeExportRegister("gx2", GX2GetSystemDRCMode, LogType::GX2);
		cafeExportRegister("gx2", GX2IsVideoOutReady, LogType::GX2);

		cafeExportRegister("gx2", GX2Invalidate, LogType::GX2);

		sGX2MainCoreIndex = 0;
	}
};
