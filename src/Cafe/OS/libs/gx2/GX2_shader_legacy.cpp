#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/HW/Latte/ISA/RegDefines.h"
#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteDraw.h"
#include "Cafe/HW/Latte/ISA/LatteReg.h"
#include "Cafe/HW/Latte/Core/LattePM4.h"

#include "GX2.h"
#include "GX2_Shader.h"

void gx2Export_GX2SetFetchShader(PPCInterpreter_t* hCPU)
{
	gx2Log_printf("GX2SetFetchShader(0x%08x)\n", hCPU->gpr[3]);
	GX2ReserveCmdSpace(11);
	GX2FetchShader_t* fetchShaderPtr = (GX2FetchShader_t*)memory_getPointerFromVirtualOffset(hCPU->gpr[3]);
	cemu_assert_debug((_swapEndianU32(fetchShaderPtr->shaderPtr) & 0xFF) == 0);

	gx2WriteGather_submit(
	// setup fetch shader
	pm4HeaderType3(IT_SET_CONTEXT_REG, 1+5),
	mmSQ_PGM_START_FS-0xA000,
	_swapEndianU32(fetchShaderPtr->shaderPtr)>>8, // pointer divided by 256
	_swapEndianU32(fetchShaderPtr->shaderSize)>>3, // size divided by 8
	0x10000, // ukn (ring buffer size?)
	0x10000, // ukn (ring buffer size?)
	*(uint32be*)&(fetchShaderPtr->_regs[0]),

	// write instance step
	pm4HeaderType3(IT_SET_CONTEXT_REG, 1+2),
	mmVGT_INSTANCE_STEP_RATE_0-0xA000,
	*(uint32be*)&(fetchShaderPtr->divisors[0]),
	*(uint32be*)&(fetchShaderPtr->divisors[1]));

	osLib_returnFromFunction(hCPU, 0);
}

void gx2Export_GX2GetVertexShaderGPRs(PPCInterpreter_t* hCPU)
{
	gx2Log_printf("GX2GetVertexShaderGPRs(0x%08x)\n", hCPU->gpr[3]);
	GX2VertexShader_t* vertexShader = (GX2VertexShader_t*)memory_getPointerFromVirtualOffset(hCPU->gpr[3]);
	uint8 numGPRs = _swapEndianU32(vertexShader->regs[0])&0xFF;
	osLib_returnFromFunction(hCPU, numGPRs);
}

void gx2Export_GX2GetVertexShaderStackEntries(PPCInterpreter_t* hCPU)
{
	gx2Log_printf("GX2GetVertexShaderStackEntries(0x%08x)\n", hCPU->gpr[3]);
	GX2VertexShader_t* vertexShader = (GX2VertexShader_t*)memory_getPointerFromVirtualOffset(hCPU->gpr[3]);
	uint8 stackEntries = (_swapEndianU32(vertexShader->regs[0])>>8)&0xFF;
	osLib_returnFromFunction(hCPU, stackEntries);
}

void gx2Export_GX2GetPixelShaderGPRs(PPCInterpreter_t* hCPU)
{
	gx2Log_printf("GX2GetPixelShaderGPRs(0x%08x)\n", hCPU->gpr[3]);
	GX2PixelShader_t* pixelShader = (GX2PixelShader_t*)memory_getPointerFromVirtualOffset(hCPU->gpr[3]);
	uint8 stackEntries = (_swapEndianU32(pixelShader->regs[0]))&0xFF;
	osLib_returnFromFunction(hCPU, stackEntries);
}

void gx2Export_GX2GetPixelShaderStackEntries(PPCInterpreter_t* hCPU)
{
	gx2Log_printf("GX2GetPixelShaderStackEntries(0x%08x)\n", hCPU->gpr[3]);
	GX2PixelShader_t* pixelShader = (GX2PixelShader_t*)memory_getPointerFromVirtualOffset(hCPU->gpr[3]);
	uint8 numGPRs = (_swapEndianU32(pixelShader->regs[0]>>8))&0xFF;
	osLib_returnFromFunction(hCPU, numGPRs);
}

void gx2Export_GX2SetVertexShader(PPCInterpreter_t* hCPU)
{
	gx2Log_printf("GX2SetVertexShader(0x%08x)\n", hCPU->gpr[3]);
	GX2ReserveCmdSpace(100);

	GX2VertexShader_t* vertexShader = (GX2VertexShader_t*)memory_getPointerFromVirtualOffset(hCPU->gpr[3]);

	MPTR shaderProgramAddr;
	uint32 shaderProgramSize;

	if( _swapEndianU32(vertexShader->shaderPtr) != MPTR_NULL )
	{
		// without R API
		shaderProgramAddr = _swapEndianU32(vertexShader->shaderPtr);
		shaderProgramSize = _swapEndianU32(vertexShader->shaderSize);
	}
	else
	{
		shaderProgramAddr = vertexShader->rBuffer.GetVirtualAddr();
		shaderProgramSize = vertexShader->rBuffer.GetSize();
	}

	cemu_assert_debug(shaderProgramAddr != 0);
	cemu_assert_debug(shaderProgramSize != 0);

	if( _swapEndianU32(vertexShader->shaderMode) == GX2_SHADER_MODE_GEOMETRY_SHADER )
	{
		// in geometry shader mode the vertex shader is written to _ES register and almost all vs control registers are set by GX2SetGeometryShader
		gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONTEXT_REG, 6));
		gx2WriteGather_submitU32AsBE(mmSQ_PGM_START_ES-0xA000);
		gx2WriteGather_submitU32AsBE(memory_virtualToPhysical(shaderProgramAddr)>>8);
		gx2WriteGather_submitU32AsBE(shaderProgramSize>>3);
		gx2WriteGather_submitU32AsBE(0x100000);
		gx2WriteGather_submitU32AsBE(0x100000);
		gx2WriteGather_submitU32AsBE(_swapEndianU32(vertexShader->regs[0])); // unknown
	}
	else
	{
		gx2WriteGather_submit(
			/* vertex shader program */
			pm4HeaderType3(IT_SET_CONTEXT_REG, 6),
			mmSQ_PGM_START_VS-0xA000,
			memory_virtualToPhysical(shaderProgramAddr)>>8, // physical address
			shaderProgramSize>>3, // size
			0x100000,
			0x100000,
			_swapEndianU32(vertexShader->regs[0]), // unknown
			/* primitive id enable */
			pm4HeaderType3(IT_SET_CONTEXT_REG, 2),
			mmVGT_PRIMITIVEID_EN-0xA000,
			_swapEndianU32(vertexShader->regs[1]),
			/* output config */
			pm4HeaderType3(IT_SET_CONTEXT_REG, 2),
			mmSPI_VS_OUT_CONFIG-0xA000,
			_swapEndianU32(vertexShader->regs[2]));

		if( (_swapEndianU32(vertexShader->regs[2]) & 1) != 0 )
			debugBreakpoint(); // per-component flag?

		// ukn
		gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONTEXT_REG, 2));
		gx2WriteGather_submitU32AsBE(mmPA_CL_VS_OUT_CNTL-0xA000);
		gx2WriteGather_submitU32AsBE(_swapEndianU32(vertexShader->regs[14]));
		
		uint32 numOutputIds = _swapEndianU32(vertexShader->regs[3]);
		numOutputIds = std::min<uint32>(numOutputIds, 0xA);
		gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONTEXT_REG, 1+numOutputIds));
		gx2WriteGather_submitU32AsBE(mmSPI_VS_OUT_ID_0-0xA000);
		for(uint32 i=0; i<numOutputIds; i++)
		{
			gx2WriteGather_submitU32AsBE(_swapEndianU32(vertexShader->regs[4+i]));
		}

		/*
		 VS _regs[]:
		 0			?
		 1			mmVGT_PRIMITIVEID_EN (?)
		 2			mmSPI_VS_OUT_CONFIG
		 3			Number of used SPI_VS_OUT_ID_* entries
		 4 - 13		SPI_VS_OUT_ID_0 - SPI_VS_OUT_ID_9
		 14			pa_cl_vs_out_cntl
		 ...
		 17 - ??	semantic table entry (input)

		 ...
		 50			vgt_vertex_reuse_block_cntl
		 51			vgt_hos_reuse_depth
		 */

		// todo: mmSQ_PGM_CF_OFFSET_VS
		// todo: mmVGT_STRMOUT_BUFFER_EN
		// stream out
		if( _swapEndianU32(vertexShader->usesStreamOut) != 0 )
		{
			// stride 0
			gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONTEXT_REG, 2));
			gx2WriteGather_submitU32AsBE(mmVGT_STRMOUT_VTX_STRIDE_0-0xA000);
			gx2WriteGather_submitU32AsBE(_swapEndianU32(vertexShader->streamOutVertexStride[0])>>2);
			// stride 1
			gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONTEXT_REG, 2));
			gx2WriteGather_submitU32AsBE(mmVGT_STRMOUT_VTX_STRIDE_1-0xA000);
			gx2WriteGather_submitU32AsBE(_swapEndianU32(vertexShader->streamOutVertexStride[1])>>2);
			// stride 2
			gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONTEXT_REG, 2));
			gx2WriteGather_submitU32AsBE(mmVGT_STRMOUT_VTX_STRIDE_2-0xA000);
			gx2WriteGather_submitU32AsBE(_swapEndianU32(vertexShader->streamOutVertexStride[2])>>2);
			// stride 3
			gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONTEXT_REG, 2));
			gx2WriteGather_submitU32AsBE(mmVGT_STRMOUT_VTX_STRIDE_3-0xA000);
			gx2WriteGather_submitU32AsBE(_swapEndianU32(vertexShader->streamOutVertexStride[3])>>2);
		}
	}
	// update semantic table
	uint32 vsSemanticTableSize = _swapEndianU32(vertexShader->regs[0x40/4]);
	if( vsSemanticTableSize > 0 )
	{
		gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONTEXT_REG, 1+1));
		gx2WriteGather_submitU32AsBE(mmSQ_VTX_SEMANTIC_CLEAR-0xA000);
		gx2WriteGather_submitU32AsBE(0xFFFFFFFF);
		if( vsSemanticTableSize == 0 )
		{
			// todo: Figure out how this is done on real SW/HW (some vertex shaders don't have a semantic table)
			gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONTEXT_REG, 1+1));
			gx2WriteGather_submitU32AsBE(mmSQ_VTX_SEMANTIC_0-0xA000);
			gx2WriteGather_submitU32AsBE(0xFFFFFFFF);
		}
		else
		{
			uint32* vsSemanticTable = vertexShader->regs+(0x44/4);
			vsSemanticTableSize = std::min<uint32>(vsSemanticTableSize, 0x20);
			gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONTEXT_REG, 1+vsSemanticTableSize));
			gx2WriteGather_submitU32AsBE(mmSQ_VTX_SEMANTIC_0-0xA000);
			for(uint32 i=0; i<vsSemanticTableSize; i++)
				gx2WriteGather_submitU32AsLE(vsSemanticTable[i]);
		}
	}

	osLib_returnFromFunction(hCPU, 0);
}

void gx2Export_GX2SetPixelShader(PPCInterpreter_t* hCPU)
{
	gx2Log_printf("GX2SetPixelShader(0x%08x)\n", hCPU->gpr[3]);
	GX2ReserveCmdSpace(100);

	GX2PixelShader_t* pixelShader = (GX2PixelShader_t*)memory_getPointerFromVirtualOffset(hCPU->gpr[3]);
	MPTR shaderProgramAddr;
	uint32 shaderProgramSize;

	if( _swapEndianU32(pixelShader->shaderPtr) != MPTR_NULL )
	{
		// old format
		shaderProgramAddr = _swapEndianU32(pixelShader->shaderPtr);
		shaderProgramSize = _swapEndianU32(pixelShader->shaderSize);
	}
	else
	{
		shaderProgramAddr = pixelShader->rBuffer.GetVirtualAddr();
		shaderProgramSize = pixelShader->rBuffer.GetSize();
	}

	gx2WriteGather_submit(
		/* pixel shader program */
		pm4HeaderType3(IT_SET_CONTEXT_REG, 6),
		mmSQ_PGM_START_PS - 0xA000,
		memory_virtualToPhysical(shaderProgramAddr)>>8, // address
		shaderProgramSize>>3, // size
		0x100000,
		0x100000,
		_swapEndianU32(pixelShader->regs[0]), // ukn
  		/* setup pixel shader input control */
		pm4HeaderType3(IT_SET_CONTEXT_REG, 3),
		mmSPI_PS_IN_CONTROL_0-0xA000,
		_swapEndianU32(pixelShader->regs[2]),
		_swapEndianU32(pixelShader->regs[3]));
	// setup pixel shader extended inputs control
	uint32 numInputs = _swapEndianU32(pixelShader->regs[4]);
	if( numInputs > 0x20 )
		numInputs = 0x20;
	gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONTEXT_REG, 1+numInputs));
	gx2WriteGather_submitU32AsBE(mmSPI_PS_INPUT_CNTL_0-0xA000);
	for(uint32 i=0; i<numInputs; i++)
	{
		uint32 inputData = _swapEndianU32(pixelShader->regs[5+i]);
		gx2WriteGather_submitU32AsBE(inputData);
	}

	gx2WriteGather_submit(
		/* mmCB_SHADER_MASK */
		pm4HeaderType3(IT_SET_CONTEXT_REG, 2),
		mmCB_SHADER_MASK-0xA000,
		_swapEndianU32(pixelShader->regs[37]),
		/* mmCB_SHADER_CONTROL */
		pm4HeaderType3(IT_SET_CONTEXT_REG, 2),
		mmCB_SHADER_CONTROL-0xA000,
		_swapEndianU32(pixelShader->regs[38]),
		/* mmDB_SHADER_CONTROL */
		pm4HeaderType3(IT_SET_CONTEXT_REG, 2),
		mmDB_SHADER_CONTROL-0xA000,
		_swapEndianU32(pixelShader->regs[39]),
		/* SPI_INPUT_Z */
		pm4HeaderType3(IT_SET_CONTEXT_REG, 2),
		mmSPI_INPUT_Z-0xA000,
		_swapEndianU32(pixelShader->regs[40]));

	osLib_returnFromFunction(hCPU, 0);
}

void gx2Export_GX2SetGeometryShader(PPCInterpreter_t* hCPU)
{
	gx2Log_printf("GX2SetGeometryShader(0x%08x)\n", hCPU->gpr[3]);
	GX2ReserveCmdSpace(100);

	GX2GeometryShader_t* geometryShader = (GX2GeometryShader_t*)memory_getPointerFromVirtualOffset(hCPU->gpr[3]);

	MPTR shaderProgramAddr;
	uint32 shaderProgramSize;

	if( _swapEndianU32(geometryShader->shaderPtr) != MPTR_NULL )
	{
		// old format
		shaderProgramAddr = _swapEndianU32(geometryShader->shaderPtr);
		shaderProgramSize = _swapEndianU32(geometryShader->shaderSize);
	}
	else
	{
		shaderProgramAddr = geometryShader->rBuffer.GetVirtualAddr();
		shaderProgramSize = geometryShader->rBuffer.GetSize();
	}

	gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONTEXT_REG, 6));
	gx2WriteGather_submitU32AsBE(mmSQ_PGM_START_GS-0xA000);
	gx2WriteGather_submitU32AsBE(memory_virtualToPhysical(shaderProgramAddr)>>8);
	gx2WriteGather_submitU32AsBE(shaderProgramSize>>3);
	gx2WriteGather_submitU32AsBE(0x100000);
	gx2WriteGather_submitU32AsBE(0x100000);
	gx2WriteGather_submitU32AsBE(_swapEndianU32(geometryShader->regs[0])); // unknown content (SQ_PGM_RESOURCES_GS)

	uint32 primitiveOut = _swapEndianU32(geometryShader->regs[1]);
	
	gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONTEXT_REG, 2));
	gx2WriteGather_submitU32AsBE(mmVGT_GS_OUT_PRIM_TYPE-0xA000);
	gx2WriteGather_submitU32AsLE(geometryShader->regs[1]);

	gx2WriteGather_submit(
		pm4HeaderType3(IT_SET_CONTEXT_REG, 2),
		Latte::REGADDR::VGT_GS_MODE - 0xA000,
		geometryShader->reg.VGT_GS_MODE
	);

	gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONTEXT_REG, 2));
	gx2WriteGather_submitU32AsBE(mmSQ_PGM_RESOURCES_GS-0xA000);
	gx2WriteGather_submitU32AsLE(geometryShader->regs[0]);

	gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONTEXT_REG, 2));
	gx2WriteGather_submitU32AsBE(mmSQ_GS_VERT_ITEMSIZE-0xA000);
	gx2WriteGather_submitU32AsLE(geometryShader->regs[5]);
	
	if( _swapEndianU32(geometryShader->useStreamout) != 0 )
	{
		// stride 0
		gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONTEXT_REG, 2));
		gx2WriteGather_submitU32AsBE(mmVGT_STRMOUT_VTX_STRIDE_0-0xA000);
		gx2WriteGather_submitU32AsBE(_swapEndianU32(geometryShader->streamoutStride[0])>>2);
		// stride 1
		gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONTEXT_REG, 2));
		gx2WriteGather_submitU32AsBE(mmVGT_STRMOUT_VTX_STRIDE_1-0xA000);
		gx2WriteGather_submitU32AsBE(_swapEndianU32(geometryShader->streamoutStride[1])>>2);
		// stride 2
		gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONTEXT_REG, 2));
		gx2WriteGather_submitU32AsBE(mmVGT_STRMOUT_VTX_STRIDE_2-0xA000);
		gx2WriteGather_submitU32AsBE(_swapEndianU32(geometryShader->streamoutStride[2])>>2);
		// stride 3
		gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONTEXT_REG, 2));
		gx2WriteGather_submitU32AsBE(mmVGT_STRMOUT_VTX_STRIDE_3-0xA000);
		gx2WriteGather_submitU32AsBE(_swapEndianU32(geometryShader->streamoutStride[3])>>2);
	}

	gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONTEXT_REG, 2));
	gx2WriteGather_submitU32AsBE(mmVGT_STRMOUT_BUFFER_EN-0xA000);
	gx2WriteGather_submitU32AsBE(_swapEndianU32(geometryShader->regs[18]));

	// set copy shader (written to vertex shader registers, vs in turn is written to es registers)
	MPTR copyShaderProgramAddr;
	uint32 copyShaderProgramSize;
	if( _swapEndianU32(geometryShader->copyShaderPtr) != MPTR_NULL )
	{
		copyShaderProgramAddr = _swapEndianU32(geometryShader->copyShaderPtr);
		copyShaderProgramSize = _swapEndianU32(geometryShader->copyShaderSize);
	}
	else
	{
		copyShaderProgramAddr = geometryShader->rBufferCopyProgram.GetVirtualAddr();
		copyShaderProgramSize = geometryShader->rBufferCopyProgram.GetSize();
	}

	cemu_assert_debug((copyShaderProgramAddr>>8) != 0);
	cemu_assert_debug((copyShaderProgramSize>>3) != 0);

	gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONTEXT_REG, 6));
	gx2WriteGather_submitU32AsBE(mmSQ_PGM_START_VS-0xA000);
	gx2WriteGather_submitU32AsBE(memory_virtualToPhysical(copyShaderProgramAddr)>>8);
	gx2WriteGather_submitU32AsBE(copyShaderProgramSize>>3);
	gx2WriteGather_submitU32AsBE(0x100000);
	gx2WriteGather_submitU32AsBE(0x100000);
	gx2WriteGather_submitU32AsBE(_swapEndianU32(geometryShader->regs[4])); // mmSQ_PGM_RESOURCES_VS

	gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONTEXT_REG, 2));
	gx2WriteGather_submitU32AsBE(mmPA_CL_VS_OUT_CNTL-0xA000);
	gx2WriteGather_submitU32AsBE(_swapEndianU32(geometryShader->regs[3]));

	// GS outputs
	uint32 numOutputIds = _swapEndianU32(geometryShader->regs[7]);
	numOutputIds = std::min<uint32>(numOutputIds, 0xA);
	if( numOutputIds != 0 )
	{
		gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONTEXT_REG, 1+numOutputIds));
		gx2WriteGather_submitU32AsBE(mmSPI_VS_OUT_ID_0-0xA000);
		for(uint32 i=0; i<numOutputIds; i++)
		{
			gx2WriteGather_submitU32AsBE(_swapEndianU32(geometryShader->regs[8+i]));
		}
	}

	// output config
	gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONTEXT_REG, 2));
	gx2WriteGather_submitU32AsBE(mmSPI_VS_OUT_CONFIG-0xA000);
	gx2WriteGather_submitU32AsBE(_swapEndianU32(geometryShader->regs[6]));

	gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONTEXT_REG, 2));
	gx2WriteGather_submitU32AsBE(mmSQ_GSVS_RING_ITEMSIZE-0xA000);
	gx2WriteGather_submitU32AsBE(_swapEndianU32(geometryShader->ringItemsize)&0x7FFF);

	/*
	  Geometry shader registers in regs[19]:
	  0		SQ_PGM_RESOURCES_GS ?
	  1		mmVGT_GS_OUT_PRIM_TYPE
	  2		mmVGT_GS_MODE
	  3		mmPA_CL_VS_OUT_CNTL
	  4		mmSQ_PGM_RESOURCES_VS (set in combination with mmSQ_PGM_START_VS)
	  5		mmSQ_GS_VERT_ITEMSIZE
	  6		mmSPI_VS_OUT_CONFIG
	  7		number of active mmSPI_VS_OUT_ID_* fields?
	  8-17  mmSPI_VS_OUT_ID_*
	  18	mmVGT_STRMOUT_BUFFER_EN
	 */
	osLib_returnFromFunction(hCPU, 0);
}

struct GX2ComputeShader_t
{
	/* +0x00 */ uint32be regs[12];
	/* +0x30 */ uint32be programSize;
	/* +0x34 */ uint32be programPtr;
	/* +0x38 */ uint32   ukn38;
	/* +0x3C */ uint32   ukn3C;
	/* +0x40 */ uint32   ukn40[8];
	/* +0x60 */ uint32be workgroupSizeX;
	/* +0x64 */ uint32be workgroupSizeY;
	/* +0x68 */ uint32be workgroupSizeZ;
	/* +0x6C */ uint32be workgroupSizeSpecial;
	/* +0x70 */ uint32be ukn70;
	/* +0x74 */ GX2RBuffer rBuffer;
};

static_assert(offsetof(GX2ComputeShader_t, programSize) == 0x30);
static_assert(offsetof(GX2ComputeShader_t, workgroupSizeX) == 0x60);
static_assert(offsetof(GX2ComputeShader_t, rBuffer) == 0x74);

void gx2Export_GX2SetComputeShader(PPCInterpreter_t* hCPU)
{
	ppcDefineParamTypePtr(computeShader, GX2ComputeShader_t, 0);
	gx2Log_printf("GX2SetComputeShader(0x%08x)", hCPU->gpr[3]);

	MPTR shaderPtr;
	uint32 shaderSize;
	if (computeShader->programPtr)
	{
		shaderPtr = computeShader->programPtr;
		shaderSize = computeShader->programSize;
	}
	else
	{
		shaderPtr = computeShader->rBuffer.GetVirtualAddr();
		shaderSize = computeShader->rBuffer.GetSize();
	}

	GX2ReserveCmdSpace(0x11);

	gx2WriteGather_submit(pm4HeaderType3(IT_SET_CONTEXT_REG, 6),
		mmSQ_PGM_START_ES-0xA000,
		memory_virtualToPhysical(shaderPtr) >> 8,
		shaderSize >> 3,
		0x100000,
		0x100000,
		computeShader->regs[0]);

	// todo: Other registers

	osLib_returnFromFunction(hCPU, 0);
}

void _GX2SubmitUniformReg(uint32 aluRegisterOffset, MPTR virtualAddress, uint32 count)
{
	uint32* dataWords = (uint32*)memory_getPointerFromVirtualOffset(virtualAddress);
	GX2ReserveCmdSpace(2 + (count / 0xFF) * 2 + count);
	// write PM4 command(s)
	uint32 currentRegisterOffset = aluRegisterOffset;
	while (count > 0)
	{
		uint32 subCount = std::min(count, 0xFFu); // a single command can write at most 0xFF values
		gx2WriteGather_submit(pm4HeaderType3(IT_SET_ALU_CONST, 1 + subCount),
			currentRegisterOffset);
		gx2WriteGather_submitU32AsLEArray(dataWords, subCount);

		dataWords += subCount;
		count -= subCount;
		currentRegisterOffset += subCount;
	}
}

void gx2Export_GX2SetVertexUniformReg(PPCInterpreter_t* hCPU)
{
	gx2Log_printf("GX2SetVertexUniformReg(0x%08x,0x%x,0x%08x)", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5]);
	_GX2SubmitUniformReg(hCPU->gpr[3] + 0x400, hCPU->gpr[5], hCPU->gpr[4]);
	cemu_assert_debug((hCPU->gpr[3] + hCPU->gpr[4]) <= 0x400);
	osLib_returnFromFunction(hCPU, 0);
}

void gx2Export_GX2SetPixelUniformReg(PPCInterpreter_t* hCPU)
{
	gx2Log_printf("GX2SetPixelUniformReg(0x%08x,0x%x,0x%08x)", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5]);
	_GX2SubmitUniformReg(hCPU->gpr[3], hCPU->gpr[5], hCPU->gpr[4]);
	cemu_assert_debug((hCPU->gpr[3] + hCPU->gpr[4]) <= 0x400);
	osLib_returnFromFunction(hCPU, 0);
}

void _GX2SubmitUniformBlock(uint32 registerBase, uint32 index, MPTR virtualAddress, uint32 size)
{
	GX2ReserveCmdSpace(9);
	gx2WriteGather_submit(pm4HeaderType3(IT_SET_RESOURCE, 8),
		registerBase + index * 7,
		memory_virtualToPhysical(virtualAddress),
		size - 1,
		0,
		1,
		0, // ukn
		0, // ukn
		0xC0000000);
}

void gx2Export_GX2SetVertexUniformBlock(PPCInterpreter_t* hCPU)
{
	gx2Log_printf("GX2SetVertexUniformBlock(0x%08x,0x%x,0x%08x)", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5]);
	_GX2SubmitUniformBlock(mmSQ_VTX_UNIFORM_BLOCK_START - mmSQ_TEX_RESOURCE_WORD0, hCPU->gpr[3], hCPU->gpr[5], hCPU->gpr[4]);
	osLib_returnFromFunction(hCPU, 0);
}

void gx2Export_GX2SetPixelUniformBlock(PPCInterpreter_t* hCPU)
{
	gx2Log_printf("GX2SetPixelUniformBlock(0x%08x,0x%x,0x%08x)", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5]);
	_GX2SubmitUniformBlock(mmSQ_PS_UNIFORM_BLOCK_START - mmSQ_TEX_RESOURCE_WORD0, hCPU->gpr[3], hCPU->gpr[5], hCPU->gpr[4]);
	osLib_returnFromFunction(hCPU, 0);
}

void gx2Export_GX2SetGeometryUniformBlock(PPCInterpreter_t* hCPU)
{
	gx2Log_printf("GX2SetGeometryUniformBlock(0x%08x,0x%x,0x%08x)", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5]);
	_GX2SubmitUniformBlock(mmSQ_GS_UNIFORM_BLOCK_START - mmSQ_TEX_RESOURCE_WORD0, hCPU->gpr[3], hCPU->gpr[5], hCPU->gpr[4]);
	osLib_returnFromFunction(hCPU, 0);
}

void gx2Export_GX2RSetVertexUniformBlock(PPCInterpreter_t* hCPU)
{
	GX2ReserveCmdSpace(9);

	GX2RBuffer* bufferPtr = (GX2RBuffer*)memory_getPointerFromVirtualOffset(hCPU->gpr[3]);
	uint32 index = hCPU->gpr[4];
	uint32 offset = hCPU->gpr[5];

	_GX2SubmitUniformBlock(mmSQ_VTX_UNIFORM_BLOCK_START - mmSQ_TEX_RESOURCE_WORD0, index, bufferPtr->GetVirtualAddr() + offset, bufferPtr->GetSize() - offset);

	osLib_returnFromFunction(hCPU, 0);
}

void gx2Export_GX2SetShaderModeEx(PPCInterpreter_t* hCPU)
{
	GX2ReserveCmdSpace(8+4);
	uint32 mode = hCPU->gpr[3];

	uint32 sqConfig = hCPU->gpr[3] == 0 ? 4 : 0;
	if (mode == GX2_SHADER_MODE_COMPUTE_SHADER)
		sqConfig |= 0xE4000000; // ES/GS/PS priority?
	// todo - other sqConfig bits

	gx2WriteGather_submit((uint32)(pm4HeaderType3(IT_SET_CONFIG_REG, 7)),
			(uint32)(mmSQ_CONFIG - 0x2000),
			sqConfig,
			0, // ukn / todo
			0, // ukn / todo
			0, // ukn / todo
			0, // ukn / todo
			0 // ukn / todo
		);

	// if not GS, then update mmVGT_GS_MODE
	if( mode != GX2_SHADER_MODE_GEOMETRY_SHADER )
	{
		// update VGT_GS_MODE only if no geometry shader is used (else this register is already set by GX2SetGeometryShader)
		gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONTEXT_REG, 2));
		gx2WriteGather_submitU32AsBE(Latte::REGADDR::VGT_GS_MODE-0xA000);
		if (mode == GX2_SHADER_MODE_COMPUTE_SHADER)
			gx2WriteGather_submitU32AsBE(Latte::LATTE_VGT_GS_MODE().set_MODE(Latte::LATTE_VGT_GS_MODE::E_MODE::SCENARIO_G).set_COMPUTE_MODE(Latte::LATTE_VGT_GS_MODE::E_COMPUTE_MODE::ON).set_PARTIAL_THD_AT_EOI(true).getRawValueBE());
		else
			gx2WriteGather_submitU32AsBE(_swapEndianU32(0));
	}

	osLib_returnFromFunction(hCPU, 0);
}

void gx2Export_GX2CalcGeometryShaderInputRingBufferSize(PPCInterpreter_t* hCPU)
{
	uint32 size = (hCPU->gpr[3]*4) * 0x1000;
	osLib_returnFromFunction(hCPU, size);
}

void gx2Export_GX2CalcGeometryShaderOutputRingBufferSize(PPCInterpreter_t* hCPU)
{
	uint32 size = (hCPU->gpr[3]*4) * 0x1000;
	osLib_returnFromFunction(hCPU, size);
}
