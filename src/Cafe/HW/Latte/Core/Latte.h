#pragma once
#include "Cafe/HW/Latte/Core/LatteConst.h"
#include "Cafe/HW/Latte/ISA/LatteReg.h"
#include "util/VirtualHeap/VirtualHeap.h"

struct LatteTextureDefinition;
class LatteTexture;
class LatteTextureView;

struct gx2GPUSharedArea_t
{
	volatile uint32 flipRequestCountBE; // counts how many buffer swaps were requested
	volatile uint32 flipExecuteCountBE; // counts how many buffer swaps were executed
	volatile uint32 swapInterval; // vsync swap interval (0 means vsync is deactivated)
};

struct LatteGPUState_t
{
	union
	{
		uint32 contextRegister[LATTE_MAX_REGISTER];
		LatteContextRegister contextNew;
	};
	MPTR contextRegisterShadowAddr[LATTE_MAX_REGISTER];
	// context control
	uint32 contextControl0;
	uint32 contextControl1;
	// draw context
	struct  
	{
		uint32 numInstances;
	}drawContext;
	// stats
	uint32 frameCounter;
	uint32 flipCounter; // increased by one everytime a vsync + flip happens
	uint32 currentDrawCallTick; // set to current time at the beginning of a drawcall
	uint32 drawCallCounter; // increased after every drawcall
	uint32 textureBindCounter; // increased at the beginning of _updateTextures()
	std::atomic<uint64> flipRequestCount;
	// timer & vsync
	uint64 timer_frequency; // contains frequency of HPC
	uint64 timer_bootUp; // contains the timestamp of when the GPU thread timer was initialized
	uint64 timer_nextVSync;
	// shared
	gx2GPUSharedArea_t* sharedArea; // quick reference to shared area
	MPTR sharedAreaAddr;
	// other
	// todo: Currently we have the command buffer logic implemented as a FIFO ringbuffer. On real HW it's handled as a series of command buffers that are pushed individually.
	std::atomic<uint64> lastSubmittedCommandBufferTimestamp;
	uint32 gx2InitCalled; // incremented every time GX2Init() is called
	// OpenGL control
	uint32 glVendor; // GLVENDOR_*
	bool alwaysDisplayDRC = false;
	// temporary (replace with proper solution later)
	bool tvBufferUsesSRGB;
	bool drcBufferUsesSRGB;
	// draw state
	bool activeShaderHasError; // if try, at least one currently bound shader stage has an error and cannot be used for drawing
	bool repeatTextureInitialization; // if set during rendertarget or texture initialization, repeat the process (textures likely have been invalidated)
	bool requiresTextureBarrier; // set if glTextureBarrier should be called
	// OSScreen
	struct  
	{
		struct  
		{
			bool isEnabled;
			MPTR physPtr;
			std::atomic<uint32> flipRequestCount;
			std::atomic<uint32> flipExecuteCount;
		}screen[2];
	}osScreen;
};

extern LatteGPUState_t LatteGPUState;

extern uint8* gxRingBufferReadPtr; // currently active read pointer (gx2 ring buffer or display list)

// texture

#include "Cafe/HW/Latte/Core/LatteTexture.h"

// texture loader

void LatteTextureLoader_estimateAccessedDataRange(LatteTexture* texture, sint32 sliceIndex, sint32 mipIndex, uint32& addrStart, uint32& addrEnd);

// render target

#define RENDER_TARGET_TV (1 << 0)
#define RENDER_TARGET_DRC (1 << 2)

void LatteRenderTarget_updateScissorBox();

void LatteRenderTarget_trackUpdates();

void LatteRenderTarget_getScreenImageArea(sint32* x, sint32* y, sint32* width, sint32* height, sint32* fullWidth, sint32* fullHeight, bool padView = false);
void LatteRenderTarget_copyToBackbuffer(LatteTextureView* textureView, bool isPadView);

void LatteRenderTarget_GetCurrentVirtualViewportSize(sint32* viewportWidth, sint32* viewportHeight);

void LatteRenderTarget_itHLESwapScanBuffer();
void LatteRenderTarget_itHLEClearColorDepthStencil(uint32 clearMask, MPTR colorBufferMPTR, MPTR colorBufferFormat, Latte::E_HWTILEMODE colorBufferTilemode, uint32 colorBufferWidth, uint32 colorBufferHeight, uint32 colorBufferPitch, uint32 colorBufferViewFirstSlice, uint32 colorBufferViewNumSlice, MPTR depthBufferMPTR, MPTR depthBufferFormat, Latte::E_HWTILEMODE depthBufferTileMode, sint32 depthBufferWidth, sint32 depthBufferHeight, sint32 depthBufferPitch, sint32 depthBufferViewFirstSlice, sint32 depthBufferViewNumSlice, float r, float g, float b, float a, float clearDepth, uint32 clearStencil);
void LatteRenderTarget_itHLECopyColorBufferToScanBuffer(MPTR colorBufferPtr, uint32 colorBufferWidth, uint32 colorBufferHeight, uint32 colorBufferSliceIndex, uint32 colorBufferFormat, uint32 colorBufferPitch, Latte::E_HWTILEMODE colorBufferTilemode, uint32 colorBufferSwizzle, uint32 renderTarget);

void LatteRenderTarget_unloadAll();

// surface copy

void LatteSurfaceCopy_copySurfaceNew(MPTR srcPhysAddr, MPTR srcMipAddr, uint32 srcSwizzle, Latte::E_GX2SURFFMT srcSurfaceFormat, sint32 srcWidth, sint32 srcHeight, sint32 srcDepth, uint32 srcPitch, sint32 srcSlice, Latte::E_DIM srcDim, Latte::E_HWTILEMODE srcTilemode, sint32 srcAA, sint32 srcLevel, MPTR dstPhysAddr, MPTR dstMipAddr, uint32 dstSwizzle, Latte::E_GX2SURFFMT dstSurfaceFormat, sint32 dstWidth, sint32 dstHeight, sint32 dstDepth, uint32 dstPitch, sint32 dstSlice, Latte::E_DIM dstDim, Latte::E_HWTILEMODE dstTilemode, sint32 dstAA, sint32 dstLevel);

// texture cache

void LatteTC_Init();

void LatteTC_RegisterTexture(LatteTexture* tex);
void LatteTC_UnregisterTexture(LatteTexture* tex);

uint32 LatteTexture_CalculateTextureDataHash(LatteTexture* hostTexture);
void LatteTexture_ReloadData(LatteTexture* hostTexture, uint32 textureUnit);

bool LatteTC_HasTextureChanged(LatteTexture* hostTexture, bool force = false);
void LatteTC_ResetTextureChangeTracker(LatteTexture* hostTexture, bool force = false);

void LatteTC_MarkTextureStillInUse(LatteTexture* texture); // lets the texture garbage collector know the texture is still in use at the time of this function call
void LatteTC_CleanupUnusedTextures();

std::vector<LatteTexture*> LatteTC_GetDeleteableTextures();

void LatteTC_UnloadAllTextures();

// texture readback

void LatteTextureReadback_Initate(LatteTextureView* textureView);
void LatteTextureReadback_StartTransfer(LatteTextureView* textureView);
bool LatteTextureReadback_Update(bool forceStart = false);
void LatteTextureReadback_NotifyTextureDeletion(LatteTexture* texture);
void LatteTextureReadback_UpdateFinishedTransfers(bool forceFinish);

// query

void LatteQuery_Init();
void LatteQuery_BeginOcclusionQuery(MPTR queryMPTR);
void LatteQuery_EndOcclusionQuery(MPTR queryMPTR);
void LatteQuery_UpdateFinishedQueries();
void LatteQuery_UpdateFinishedQueriesForceFinishAll();
void LatteQuery_CancelActiveGPU7Queries();

// streamout

void LatteStreamout_InitCache();
sint32 LatteStreamout_GetRingBufferSize();
void LatteStreamout_PrepareDrawcall(uint32 count, uint32 instanceCount);
void LatteStreamout_FinishDrawcall(bool useDirectMemoryMode);

// timing

void LatteTiming_Init();
void LatteTiming_HandleTimedVsync();

// command processor

void LatteCP_ProcessRingbuffer();

// buffer cache

bool LatteBufferCache_Sync(uint32 minIndex, uint32 maxIndex, uint32 baseInstance, uint32 instanceCount);
void LatteBufferCache_LoadRemappedUniforms(struct LatteDecompilerShader* shader, float* uniformData);

void LatteRenderTarget_updateViewport();

#define LATTE_GLSL_DYNAMIC_UNIFORM_BLOCK_SIZE	(4096) // maximum size for uniform blocks (in vec4s). On Nvidia hardware 4096 is the maximum (64K / 16 = 4096) all other vendors have much higher limits

//static uint32 glTempError;
//#define catchOpenGLError() glFinish(); if( (glTempError = glGetError()) != 0 ) { printf("OpenGL error 0x%x: %s : %d timestamp %08x\n", glTempError, __FILE__, __LINE__, GetTickCount()); __debugbreak(); }

#define catchOpenGLError()

// Latte emulation control
void Latte_Start();
void Latte_Stop();
bool Latte_GetStopSignal(); // returns true if stop was requested or if in stopped state
void LatteThread_Exit();