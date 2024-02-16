#pragma once
#include "Cafe/HW/Latte/Core/LatteConst.h"

// base defines for GX2
#define GX2_TRUE	1
#define GX2_FALSE	0
#define GX2_ENABLE	1
#define GX2_DISABLE	0

#include "GX2_Surface.h"

// general

void gx2_load();

// shader

void gx2Export_GX2SetPixelShader(PPCInterpreter_t* hCPU);
void gx2Export_GX2SetGeometryShader(PPCInterpreter_t* hCPU);
void gx2Export_GX2SetComputeShader(PPCInterpreter_t* hCPU);
void gx2Export_GX2SetVertexUniformReg(PPCInterpreter_t* hCPU);
void gx2Export_GX2SetVertexUniformBlock(PPCInterpreter_t* hCPU);
void gx2Export_GX2RSetVertexUniformBlock(PPCInterpreter_t* hCPU);
void gx2Export_GX2SetPixelUniformBlock(PPCInterpreter_t* hCPU);
void gx2Export_GX2SetPixelUniformReg(PPCInterpreter_t* hCPU);
void gx2Export_GX2SetGeometryUniformBlock(PPCInterpreter_t* hCPU);
void gx2Export_GX2SetShaderModeEx(PPCInterpreter_t* hCPU);
void gx2Export_GX2CalcGeometryShaderInputRingBufferSize(PPCInterpreter_t* hCPU);
void gx2Export_GX2CalcGeometryShaderOutputRingBufferSize(PPCInterpreter_t* hCPU);

// write gather / command queue

#define GX2_COMMAND_RING_BUFFER_SIZE	(64*1024*1024) // 64MB

void gx2Export_GX2GetContextStateDisplayList(PPCInterpreter_t* hCPU);

#include "GX2_Command.h"

// misc
void gx2Export_GX2AllocateTilingApertureEx(PPCInterpreter_t* hCPU);
void gx2Export_GX2FreeTilingAperture(PPCInterpreter_t* hCPU);

void gx2Export_GX2SetSwapInterval(PPCInterpreter_t* hCPU);
void gx2Export_GX2GetSwapInterval(PPCInterpreter_t* hCPU);
void gx2Export_GX2GetSwapStatus(PPCInterpreter_t* hCPU);
void gx2Export_GX2GetGPUTimeout(PPCInterpreter_t* hCPU);
void gx2Export_GX2SampleTopGPUCycle(PPCInterpreter_t* hCPU);
void gx2Export_GX2SampleBottomGPUCycle(PPCInterpreter_t* hCPU);

// color/depth buffers

#define GX2_SCAN_TARGET_TV				1
#define GX2_SCAN_TARGET_TV_RIGH			2
#define GX2_SCAN_TARGET_DRC_FIRST		4
#define GX2_SCAN_TARGET_DRC_SECOND		8

void gx2Export_GX2InitColorBufferRegs(PPCInterpreter_t* hCPU);
void gx2Export_GX2InitDepthBufferRegs(PPCInterpreter_t* hCPU);
void gx2Export_GX2SetColorBuffer(PPCInterpreter_t* hCPU);
void gx2Export_GX2SetDepthBuffer(PPCInterpreter_t* hCPU);
void gx2Export_GX2SetDRCBuffer(PPCInterpreter_t* hCPU);
void gx2Export_GX2MarkScanBufferCopied(PPCInterpreter_t* hCPU);

// special state

#define GX2_SPECIAL_STATE_COUNT			9

// context state

void gx2Export_GX2SetDefaultState(PPCInterpreter_t* hCPU);
void gx2Export_GX2SetupContextStateEx(PPCInterpreter_t* hCPU);
void gx2Export_GX2SetContextState(PPCInterpreter_t* hCPU);

// command buffer

uint32 _GX2GetUnflushedBytes(uint32 coreIndex);
void _GX2SubmitToTCL();
void GX2ReserveCmdSpace(uint32 reservedFreeSpaceInU32);