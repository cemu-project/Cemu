#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/HW/Latte/ISA/RegDefines.h"
#include "Cafe/HW/Espresso/PPCCallback.h"
#include "GX2.h"
#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/OS/libs/coreinit/coreinit_Time.h"
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"
#include "Cafe/CafeSystem.h"
#include "Cafe/HW/Latte/Core/LattePM4.h"

#include "GX2_Command.h"
#include "GX2_State.h"
#include "GX2_Memory.h"
#include "GX2_Event.h"
#include "GX2_Shader.h"
#include "GX2_Blit.h"
#include "GX2_Draw.h"
#include "GX2_Query.h"
#include "GX2_Misc.h"
#include "GX2_Surface.h"
#include "GX2_Surface_Copy.h"
#include "GX2_Texture.h"

#include <cinttypes>

#define GX2_TV_RENDER_NONE			0
#define GX2_TV_RENDER_480			1
#define GX2_TV_RENDER_480_WIDE		2
#define GX2_TV_RENDER_720			3
#define GX2_TV_RENDER_720I			4
#define GX2_TV_RENDER_1080			5
#define GX2_TV_RENDER_COUNT			6

struct
{
	sint32 width;
	sint32 height;
}tvScanBufferResolutions[GX2_TV_RENDER_COUNT] = {
0,0,
640,480,
854,480,
1280,720,
1280,720,
1920,1080
};

uint64 lastSwapTime = 0;

void gx2Export_GX2SwapScanBuffers(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::GX2, "GX2SwapScanBuffers()");

	bool isPokken = false;

	uint64 titleId = CafeSystem::GetForegroundTitleId();
	if (titleId == 0x00050000101DF500ull || titleId == 0x00050000101C5800ull || titleId == 0x00050000101DF400ull)
		isPokken = true;

	if (isPokken)
		GX2::GX2DrawDone();

	GX2ReserveCmdSpace(5+2);

	uint64 tick64 = PPCInterpreter_getMainCoreCycleCounter() / 20ULL;
	lastSwapTime = tick64;
	// count flip request
	// is this updated via a PM4 MEM_WRITE operation?

	// Orochi Warriors seems to call GX2SwapScanBuffers on arbitrary threads/cores. The PM4 commands should go through to the GPU as long as there is no active display list and no other core is submitting commands simultaneously
	// right now, we work around this by avoiding the infinite loop below (request counter incremented, but PM4 not sent)
	uint32 coreIndex = coreinit::OSGetCoreId();
	if (GX2::sGX2MainCoreIndex == coreIndex)
		LatteGPUState.sharedArea->flipRequestCountBE = _swapEndianU32(_swapEndianU32(LatteGPUState.sharedArea->flipRequestCountBE) + 1);

	gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_HLE_REQUEST_SWAP_BUFFERS, 1));
	gx2WriteGather_submitU32AsBE(0); // reserved

	// swap frames
	gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_HLE_TRIGGER_SCANBUFFER_SWAP, 1));
	gx2WriteGather_submitU32AsBE(0); // reserved

	// wait for flip if the CPU is too far ahead
	// doing it after swap request is how the actual console does it, but that still causes issues in Pokken
	while ((sint32)(_swapEndianU32(LatteGPUState.sharedArea->flipRequestCountBE) - _swapEndianU32(LatteGPUState.sharedArea->flipExecuteCountBE)) > 5)
	{
		GX2::GX2WaitForFlip();
	}

	GX2::GX2WriteGather_checkAndInsertWrapAroundMark();
	osLib_returnFromFunction(hCPU, 0);
}

void gx2Export_GX2CopyColorBufferToScanBuffer(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::GX2, "GX2CopyColorBufferToScanBuffer(0x{:08x},{})", hCPU->gpr[3], hCPU->gpr[4]);
	GX2ReserveCmdSpace(5);

	// todo: proper implementation

	// hack: Avoid running to far ahead of GPU. Normally this would be guaranteed by the circular buffer model, which we currently dont fully emulate
	if(GX2::GX2WriteGather_getReadWriteDistance() > 32*1024*1024 )
	{
		debug_printf("Waiting for GPU to catch up...\n");
		PPCInterpreter_relinquishTimeslice(); // release current thread
		return;
	}
	GX2ColorBuffer* colorBuffer = (GX2ColorBuffer*)memory_getPointerFromVirtualOffset(hCPU->gpr[3]);

	gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_HLE_COPY_COLORBUFFER_TO_SCANBUFFER, 9));
	gx2WriteGather_submitU32AsBE(memory_virtualToPhysical(colorBuffer->surface.imagePtr));
	gx2WriteGather_submitU32AsBE((uint32)colorBuffer->surface.width);
	gx2WriteGather_submitU32AsBE((uint32)colorBuffer->surface.height);
	gx2WriteGather_submitU32AsBE((uint32)colorBuffer->surface.pitch);
	gx2WriteGather_submitU32AsBE((uint32)colorBuffer->surface.tileMode.value());
	gx2WriteGather_submitU32AsBE((uint32)colorBuffer->surface.swizzle);
	gx2WriteGather_submitU32AsBE(_swapEndianU32(colorBuffer->viewFirstSlice));
	gx2WriteGather_submitU32AsBE((uint32)colorBuffer->surface.format.value());
	gx2WriteGather_submitU32AsBE(hCPU->gpr[4]);

	osLib_returnFromFunction(hCPU, 0);
}

void gx2Export_GX2WaitForFreeScanBuffer(PPCInterpreter_t* hCPU)
{
	// todo: proper implementation
	debug_printf("GX2WaitForFreeScanBuffer(): Unimplemented\n");

	osLib_returnFromFunction(hCPU, 0);
}

void gx2Export_GX2GetCurrentScanBuffer(PPCInterpreter_t* hCPU)
{
	// todo: proper implementation
	uint32 scanTarget = hCPU->gpr[3];
	GX2ColorBuffer* colorBufferBE = (GX2ColorBuffer*)memory_getPointerFromVirtualOffset(hCPU->gpr[4]);
	memset(colorBufferBE, 0x00, sizeof(GX2ColorBuffer));
	colorBufferBE->surface.width = 100;
	colorBufferBE->surface.height = 100;
	// note: For now we abuse the tiling aperture memory area as framebuffer pointers
	if( scanTarget == GX2_SCAN_TARGET_TV )
	{
		colorBufferBE->surface.imagePtr = MEMORY_TILINGAPERTURE_AREA_ADDR+0x200000;
	}
	else if( scanTarget == GX2_SCAN_TARGET_DRC_FIRST )
	{
		colorBufferBE->surface.imagePtr = MEMORY_TILINGAPERTURE_AREA_ADDR+0x40000;
	}
	osLib_returnFromFunction(hCPU, 0);
}

void coreinitExport_GX2GetSystemTVScanMode(PPCInterpreter_t* hCPU)
{
	// 1080p = 7
	osLib_returnFromFunction(hCPU, 7);
}

void coreinitExport_GX2GetSystemTVAspectRatio(PPCInterpreter_t* hCPU)
{
	osLib_returnFromFunction(hCPU, 1); // 16:9
}

void gx2Export_GX2TempGetGPUVersion(PPCInterpreter_t* hCPU)
{
	osLib_returnFromFunction(hCPU, 2);
}

void _GX2InitScanBuffer(GX2ColorBuffer* colorBuffer, sint32 width, sint32 height, Latte::E_GX2SURFFMT format)
{
	colorBuffer->surface.resFlag = GX2_RESFLAG_USAGE_TEXTURE | GX2_RESFLAG_USAGE_COLOR_BUFFER;
	colorBuffer->surface.width = width;
	colorBuffer->surface.height = height;
	colorBuffer->viewFirstSlice = _swapEndianU32(0);
	colorBuffer->viewNumSlices = _swapEndianU32(1);
	colorBuffer->viewMip = _swapEndianU32(0);
	colorBuffer->surface.numLevels = 1;
	colorBuffer->surface.dim = Latte::E_DIM::DIM_2D;
	colorBuffer->surface.swizzle = 0;
	colorBuffer->surface.depth = 1;
	colorBuffer->surface.tileMode = Latte::E_GX2TILEMODE::TM_LINEAR_GENERAL;
	colorBuffer->surface.format = format;
	colorBuffer->surface.mipPtr = MPTR_NULL;
	colorBuffer->surface.aa = 0;
	GX2::GX2CalcSurfaceSizeAndAlignment(&colorBuffer->surface);
	colorBuffer->surface.resFlag = GX2_RESFLAG_USAGE_TEXTURE | GX2_RESFLAG_USAGE_COLOR_BUFFER | GX2_RESFLAG_USAGE_SCAN_BUFFER;
}

void gx2Export_GX2CalcTVSize(PPCInterpreter_t* hCPU)
{
	uint32 tvRenderMode = hCPU->gpr[3];
	Latte::E_GX2SURFFMT format = (Latte::E_GX2SURFFMT)hCPU->gpr[4];
	uint32 bufferingMode = hCPU->gpr[5];
	uint32 outputSizeMPTR = hCPU->gpr[6];
	uint32 outputScaleNeededMPTR = hCPU->gpr[7];

	cemu_assert(tvRenderMode < GX2_TV_RENDER_COUNT);

	uint32 width = tvScanBufferResolutions[tvRenderMode].width;
	uint32 height = tvScanBufferResolutions[tvRenderMode].height;
	
	GX2ColorBuffer colorBuffer;
	memset(&colorBuffer, 0, sizeof(GX2ColorBuffer));
	_GX2InitScanBuffer(&colorBuffer, width, height, format);

	uint32 imageSize = colorBuffer.surface.imageSize;
	uint32 alignment = colorBuffer.surface.alignment;

	uint32 alignmentPaddingSize = (alignment - (imageSize%alignment)) % alignment;

	uint32 uknMult = 1; // probably for interlaced?
	if (tvRenderMode == GX2_TV_RENDER_720I)
		uknMult = 2;

	uint32 adjustedBufferingMode = bufferingMode;
	if (tvRenderMode < GX2_TV_RENDER_720)
		adjustedBufferingMode = 4;

	uint32 bufferedImageSize = (imageSize + alignmentPaddingSize) * adjustedBufferingMode;
	bufferedImageSize = bufferedImageSize * uknMult - alignmentPaddingSize;
	
	memory_writeU32(outputSizeMPTR, bufferedImageSize);
	memory_writeU32(outputScaleNeededMPTR, 0); // todo
	osLib_returnFromFunction(hCPU, 0);
}

void gx2Export_GX2CalcDRCSize(PPCInterpreter_t* hCPU)
{

	ppcDefineParamS32(drcMode, 0);
	ppcDefineParamU32(format, 1);
	ppcDefineParamU32(bufferingMode, 2);
	ppcDefineParamMPTR(sizeMPTR, 3);
	ppcDefineParamMPTR(scaleNeededMPTR, 4);

	uint32 width = 0;
	uint32 height = 0;
	if (drcMode > 0)
	{
		width = 854;
		height = 480;
	}

	GX2ColorBuffer colorBuffer = {};
	memset(&colorBuffer, 0, sizeof(colorBuffer));
	_GX2InitScanBuffer(&colorBuffer, width, height, (Latte::E_GX2SURFFMT)format);

	uint32 imageSize = colorBuffer.surface.imageSize;
	uint32 alignment = colorBuffer.surface.alignment;

	uint32 alignmentPaddingSize = (alignment - (imageSize%alignment)) % alignment;


	uint32 adjustedBufferingMode = bufferingMode;

	uint32 bufferedImageSize = (imageSize + alignmentPaddingSize) * adjustedBufferingMode;
	bufferedImageSize = bufferedImageSize - alignmentPaddingSize;

	memory_writeU32(sizeMPTR, bufferedImageSize);
	memory_writeU32(scaleNeededMPTR, 0);

	osLib_returnFromFunction(hCPU, 0);
}

void gx2Export_GX2SetDRCScale(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::GX2, "GX2SetDRCScale({},{})", hCPU->gpr[3], hCPU->gpr[4]);
	osLib_returnFromFunction(hCPU, 0);
}

void gx2Export_GX2SetDRCConnectCallback(PPCInterpreter_t* hCPU)
{
	ppcDefineParamS32(channel, 0);
	ppcDefineParamMEMPTR(callback, void, 1);
	cemuLog_log(LogType::GX2, "GX2SetDRCConnectCallback({}, 0x{:08x})", channel, callback.GetMPTR());
	if(callback.GetPtr())
		PPCCoreCallback(callback, channel, TRUE);
	osLib_returnFromFunction(hCPU, 0);
}

void gx2Export_GX2SetSemaphore(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::GX2, "GX2SetSemaphore(0x{:08x},{})", hCPU->gpr[3], hCPU->gpr[4]);
	ppcDefineParamMPTR(semaphoreMPTR, 0);
	ppcDefineParamS32(mode, 1);

	uint32 SEM_SEL;

	if (mode == 0)
	{
		// wait
		SEM_SEL = 7;
	}
	else if (mode == 1)
	{
		// signal
		SEM_SEL = 6;
	}
	else
	{
		cemu_assert_debug(false);
		osLib_returnFromFunction(hCPU, 0);
		return;
	}
	uint32 semaphoreControl = (SEM_SEL << 29);
	semaphoreControl |= 0x1000; // WAIT_ON_SIGNAL
	gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_MEM_SEMAPHORE, 2));
	gx2WriteGather_submitU32AsBE(memory_virtualToPhysical(semaphoreMPTR)); // semaphore physical address
	gx2WriteGather_submitU32AsBE(semaphoreControl); // control

	osLib_returnFromFunction(hCPU, 0);
}

void gx2Export_GX2Flush(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::GX2, "GX2Flush()");
	_GX2SubmitToTCL();
	osLib_returnFromFunction(hCPU, 0);
}

uint8* _GX2LastFlushPtr[PPC_CORE_COUNT] = {NULL};

uint64 _prevReturnedGPUTime = 0;

uint64 Latte_GetTime()
{
	uint64 gpuTime = coreinit::coreinit_getTimerTick();
	gpuTime *= 20000ULL;
	if (gpuTime <= _prevReturnedGPUTime)
		gpuTime = _prevReturnedGPUTime + 1; // avoid ever returning identical timestamps
	_prevReturnedGPUTime = gpuTime;
	return gpuTime;
}

void _GX2SubmitToTCL()
{
	uint32 coreIndex = PPCInterpreter_getCoreIndex(PPCInterpreter_getCurrentInstance());
	// do nothing if called from non-main GX2 core
	if (GX2::sGX2MainCoreIndex != coreIndex)
	{
		cemuLog_logDebug(LogType::Force, "_GX2SubmitToTCL() called on non-main GX2 core");
		return;
	}
	if( gx2WriteGatherPipe.displayListStart[coreIndex] != MPTR_NULL )
		return; // quit if in display list
	_GX2LastFlushPtr[coreIndex] = (gx2WriteGatherPipe.writeGatherPtrGxBuffer[coreIndex]);
	// update last submitted CB timestamp
	uint64 commandBufferTimestamp = Latte_GetTime();
	LatteGPUState.lastSubmittedCommandBufferTimestamp.store(commandBufferTimestamp);
	cemuLog_log(LogType::GX2, "Submitting GX2 command buffer with timestamp {:016x}", commandBufferTimestamp);
	// submit HLE packet to write retirement timestamp
	gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_HLE_SET_CB_RETIREMENT_TIMESTAMP, 2));
	gx2WriteGather_submitU32AsBE((uint32)(commandBufferTimestamp>>32ULL));
	gx2WriteGather_submitU32AsBE((uint32)(commandBufferTimestamp&0xFFFFFFFFULL));
}

uint32 _GX2GetUnflushedBytes(uint32 coreIndex)
{
	uint32 unflushedBytes = 0;
	if (_GX2LastFlushPtr[coreIndex] != NULL)
	{
		if (_GX2LastFlushPtr[coreIndex] > gx2WriteGatherPipe.writeGatherPtrGxBuffer[coreIndex])
			unflushedBytes = (uint32)(gx2WriteGatherPipe.writeGatherPtrGxBuffer[coreIndex] - gx2WriteGatherPipe.gxRingBuffer + 4); // this isn't 100% correct since we ignore the bytes between the last flush address and the start of the wrap around
		else
			unflushedBytes = (uint32)(gx2WriteGatherPipe.writeGatherPtrGxBuffer[coreIndex] - _GX2LastFlushPtr[coreIndex]);
	}
	else
		unflushedBytes = (uint32)(gx2WriteGatherPipe.writeGatherPtrGxBuffer[coreIndex] - gx2WriteGatherPipe.gxRingBuffer);
	return unflushedBytes;
}

/*
 * Guarantees that the requested amount of space is available on the current command buffer
 * If the space is not available, the current command buffer is pushed to the GPU and a new one is allocated
 */
void GX2ReserveCmdSpace(uint32 reservedFreeSpaceInU32)
{
	uint32 coreIndex = coreinit::OSGetCoreId();
	// if we are in a display list then do nothing
	if( gx2WriteGatherPipe.displayListStart[coreIndex] != MPTR_NULL )
		return;
	uint32 unflushedBytes = _GX2GetUnflushedBytes(coreIndex);
	if( unflushedBytes >= 0x1000 )
	{
		_GX2SubmitToTCL();
	}
}

void gx2_load()
{
	osLib_addFunction("gx2", "GX2GetContextStateDisplayList", gx2Export_GX2GetContextStateDisplayList);

	// swap, vsync & timing
	osLib_addFunction("gx2", "GX2SwapScanBuffers", gx2Export_GX2SwapScanBuffers);
	osLib_addFunction("gx2", "GX2GetSwapStatus", gx2Export_GX2GetSwapStatus);
	osLib_addFunction("gx2", "GX2CopyColorBufferToScanBuffer", gx2Export_GX2CopyColorBufferToScanBuffer);
	osLib_addFunction("gx2", "GX2WaitForFreeScanBuffer", gx2Export_GX2WaitForFreeScanBuffer);
	osLib_addFunction("gx2", "GX2GetCurrentScanBuffer", gx2Export_GX2GetCurrentScanBuffer);

	// shader stuff
	osLib_addFunction("gx2", "GX2SetPixelShader", gx2Export_GX2SetPixelShader);
	osLib_addFunction("gx2", "GX2SetGeometryShader", gx2Export_GX2SetGeometryShader);
	osLib_addFunction("gx2", "GX2SetComputeShader", gx2Export_GX2SetComputeShader);
	osLib_addFunction("gx2", "GX2SetVertexUniformBlock", gx2Export_GX2SetVertexUniformBlock);
	osLib_addFunction("gx2", "GX2RSetVertexUniformBlock", gx2Export_GX2RSetVertexUniformBlock);

	osLib_addFunction("gx2", "GX2SetPixelUniformBlock", gx2Export_GX2SetPixelUniformBlock);
	osLib_addFunction("gx2", "GX2SetGeometryUniformBlock", gx2Export_GX2SetGeometryUniformBlock);
	osLib_addFunction("gx2", "GX2SetShaderModeEx", gx2Export_GX2SetShaderModeEx);

	osLib_addFunction("gx2", "GX2CalcGeometryShaderInputRingBufferSize", gx2Export_GX2CalcGeometryShaderInputRingBufferSize);
	osLib_addFunction("gx2", "GX2CalcGeometryShaderOutputRingBufferSize", gx2Export_GX2CalcGeometryShaderOutputRingBufferSize);

	// color/depth buffers
	osLib_addFunction("gx2", "GX2InitColorBufferRegs", gx2Export_GX2InitColorBufferRegs);
	osLib_addFunction("gx2", "GX2InitDepthBufferRegs", gx2Export_GX2InitDepthBufferRegs);
	osLib_addFunction("gx2", "GX2SetColorBuffer", gx2Export_GX2SetColorBuffer);
	osLib_addFunction("gx2", "GX2SetDepthBuffer", gx2Export_GX2SetDepthBuffer);

	osLib_addFunction("gx2", "GX2SetDRCBuffer", gx2Export_GX2SetDRCBuffer);
	osLib_addFunction("gx2", "GX2MarkScanBufferCopied", gx2Export_GX2MarkScanBufferCopied);

	// misc
	osLib_addFunction("gx2", "GX2TempGetGPUVersion", gx2Export_GX2TempGetGPUVersion);
	osLib_addFunction("gx2", "GX2CalcTVSize", gx2Export_GX2CalcTVSize);
	osLib_addFunction("gx2", "GX2CalcDRCSize", gx2Export_GX2CalcDRCSize);
	osLib_addFunction("gx2", "GX2SetDRCScale", gx2Export_GX2SetDRCScale);
	osLib_addFunction("gx2", "GX2SetDRCConnectCallback", gx2Export_GX2SetDRCConnectCallback);

	osLib_addFunction("gx2", "GX2GetSystemTVScanMode", coreinitExport_GX2GetSystemTVScanMode);
	osLib_addFunction("gx2", "GX2GetSystemTVAspectRatio", coreinitExport_GX2GetSystemTVAspectRatio);
	
	osLib_addFunction("gx2", "GX2SetSwapInterval", gx2Export_GX2SetSwapInterval);
	osLib_addFunction("gx2", "GX2GetSwapInterval", gx2Export_GX2GetSwapInterval);
	osLib_addFunction("gx2", "GX2GetGPUTimeout", gx2Export_GX2GetGPUTimeout);
	osLib_addFunction("gx2", "GX2SampleTopGPUCycle", gx2Export_GX2SampleTopGPUCycle);
	osLib_addFunction("gx2", "GX2SampleBottomGPUCycle", gx2Export_GX2SampleBottomGPUCycle);

	osLib_addFunction("gx2", "GX2AllocateTilingApertureEx", gx2Export_GX2AllocateTilingApertureEx);
	osLib_addFunction("gx2", "GX2FreeTilingAperture", gx2Export_GX2FreeTilingAperture);

	// context state
	osLib_addFunction("gx2", "GX2SetDefaultState", gx2Export_GX2SetDefaultState);
	osLib_addFunction("gx2", "GX2SetupContextStateEx", gx2Export_GX2SetupContextStateEx);
	osLib_addFunction("gx2", "GX2SetContextState", gx2Export_GX2SetContextState);

	// semaphore
	osLib_addFunction("gx2", "GX2SetSemaphore", gx2Export_GX2SetSemaphore);

	// command buffer
	osLib_addFunction("gx2", "GX2Flush", gx2Export_GX2Flush);

	GX2::GX2Init_writeGather();
	GX2::GX2MemInit();
	GX2::GX2ResourceInit();
	GX2::GX2CommandInit();
	GX2::GX2SurfaceInit();
	GX2::GX2SurfaceCopyInit();
	GX2::GX2TextureInit();
	GX2::GX2StateInit();
	GX2::GX2ShaderInit();
	GX2::GX2EventInit();
	GX2::GX2BlitInit();
	GX2::GX2DrawInit();
	GX2::GX2StreamoutInit();
	GX2::GX2QueryInit();
	GX2::GX2MiscInit();
}
