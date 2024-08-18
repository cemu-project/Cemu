#pragma once
#include "Cafe/HW/Latte/ISA/LatteReg.h"
#include "GX2_Streamout.h"

struct GX2FetchShader
{
	enum class FetchShaderType : uint32
	{
		NO_TESSELATION = 0,
	};

	/* +0x00 */ betype<FetchShaderType> fetchShaderType;
	/* +0x04 */ betype<Latte::LATTE_SQ_PGM_RESOURCES_FS> reg_SQ_PGM_RESOURCES_FS;
	/* +0x08 */ uint32 shaderSize;
	/* +0x0C */ MPTR shaderPtr;
	/* +0x10 */ uint32 attribCount;
	/* +0x14 */ uint32 divisorCount;
	/* +0x18 */ uint32be divisors[2];

	MPTR GetProgramAddr() const
	{
		return _swapEndianU32(shaderPtr);
	}
};

static_assert(sizeof(GX2FetchShader) == 0x20);
static_assert(sizeof(betype<GX2FetchShader::FetchShaderType>) == 4);

namespace GX2
{

	void GX2ShaderInit();
}

// code below still needs to be modernized (use betype, enum classes, move to namespace)

// deprecated, use GX2_SHADER_MODE enum class instead
#define GX2_SHADER_MODE_UNIFORM_REGISTER	0
#define GX2_SHADER_MODE_UNIFORM_BLOCK		1
#define GX2_SHADER_MODE_GEOMETRY_SHADER		2
#define GX2_SHADER_MODE_COMPUTE_SHADER		3

enum class GX2_SHADER_MODE : uint32
{
	UNIFORM_REGISTER = 0,
	UNIFORM_BLOCK = 1,
	GEOMETRY_SHADER = 2,
	COMPUTE_SHADER = 3,
};

struct GX2VertexShader
{
	/* +0x000 */
	struct
	{
		/* +0x00 */ betype<Latte::LATTE_SQ_PGM_RESOURCES_VS> SQ_PGM_RESOURCES_VS; // compatible with SQ_PGM_RESOURCES_ES
		/* +0x04 */ betype<Latte::LATTE_VGT_PRIMITIVEID_EN> VGT_PRIMITIVEID_EN;
		/* +0x08 */ betype<Latte::LATTE_SPI_VS_OUT_CONFIG> SPI_VS_OUT_CONFIG;
		/* +0x0C */ uint32be vsOutIdTableSize;
		/* +0x10 */ betype<Latte::LATTE_SPI_VS_OUT_ID_N> LATTE_SPI_VS_OUT_ID_N[10];
		/* +0x38 */ betype<Latte::LATTE_PA_CL_VS_OUT_CNTL> PA_CL_VS_OUT_CNTL;
		/* +0x3C */ uint32be uknReg15; // ?
		/* +0x40 */ uint32be semanticTableSize;
		/* +0x44 */ betype<Latte::LATTE_SQ_VTX_SEMANTIC_X> SQ_VTX_SEMANTIC_N[32];
		/* +0xC4 */ uint32be uknReg49; // ?
		/* +0xC8 */ uint32be uknReg50; // vgt_vertex_reuse_block_cntl
		/* +0xCC */ uint32be uknReg51; // vgt_hos_reuse_depth
	}regs;
	/* +0x0D0 */ uint32be shaderSize;
	/* +0x0D4 */ MEMPTR<void> shaderPtr;
	/* +0x0D8 */ betype<GX2_SHADER_MODE> shaderMode;
	/* +0x0DC */ uint32 uniformBlockCount;
	/* +0x0E0 */ MPTR uniformBlockInfo;
	/* +0x0E4 */ uint32 uniformVarCount;
	/* +0x0E8 */ MPTR uniformVarInfo;
	/* +0x0EC */ uint32 uknEC;
	/* +0x0F0 */ MPTR uknF0;
	/* +0x0F4 */ uint32 uknF4;
	/* +0x0F8 */ MPTR uknF8; // each entry has 8 byte?
	/* +0x0FC */ uint32	samplerCount;
	/* +0x100 */ MPTR samplerInfo;
	/* +0x104 */ uint32 attribCount;
	/* +0x108 */ MPTR attribInfo;
	/* +0x10C */ uint32be ringItemsize; // for GS
	/* +0x110 */ uint32be usesStreamOut;
	/* +0x114 */ uint32be streamOutVertexStride[GX2_MAX_STREAMOUT_BUFFERS];
	/* +0x124 */ GX2RBuffer rBuffer;

	MPTR GetProgramAddr() const
	{
		if (this->shaderPtr)
			return this->shaderPtr.GetMPTR();
		return this->rBuffer.GetVirtualAddr();
	}
};

static_assert(sizeof(GX2VertexShader) == 0x134);

typedef struct _GX2PixelShader
{
	uint32  regs[41];
	// regs:
	// 0 ?		Used by GPR count API?
	// 1 ?
	// 2 mmSPI_PS_IN_CONTROL_0
	// 3 mmSPI_PS_IN_CONTROL_1
	// 4 numInputs
	// 5 mmSPI_PS_INPUT_CNTL_0
	// ...
	// 36 mmSPI_PS_INPUT_CNTL_31
	// 37 mmCB_SHADER_MASK
	// 38 mmCB_SHADER_CONTROL
	// 39 mmDB_SHADER_CONTROL
	// 40 mmSPI_INPUT_Z

	/* +0xA4 */ uint32 shaderSize;
	/* +0xA8 */ MPTR shaderPtr;
	/* +0xAC */ uint32 shaderMode;
	/* +0xB0 */ uint32 uniformBlockCount;
	/* +0xB4 */ MPTR uniformBlockInfo;
	/* +0xB8 */ uint32 uniformVarCount;
	/* +0xBC */ MPTR uniformVarInfo;
	/* +0xC0 */ uint32 uknC0;
	/* +0xC4 */ MPTR uknC4;
	/* +0xC8 */ uint32 uknC8;
	/* +0xCC */ MPTR uknCC;
	/* +0xD0 */ uint32 samplerCount;
	/* +0xD4 */ MPTR samplerInfo;
	/* +0xD8 */ GX2RBuffer rBuffer;

	MPTR GetProgramAddr() const
	{
		if (_swapEndianU32(shaderPtr) != MPTR_NULL)
			return _swapEndianU32(shaderPtr);
		return rBuffer.GetVirtualAddr();
	}
}GX2PixelShader_t;

static_assert(sizeof(GX2PixelShader_t) == 0xE8);

struct GX2GeometryShader_t
{
	union
	{
		/* +0x00 */ uint32 regs[19];
		struct
		{
			uint32be reg0;
			uint32be reg1;
			uint32be VGT_GS_MODE;
			uint32be reg3;
			uint32be reg4;
			uint32be reg5;
			uint32be reg6;
			uint32be reg7;
			// todo
		}reg;
	};
	/* +0x4C */ uint32 shaderSize;
	/* +0x50 */ MPTR shaderPtr;
	/* +0x54 */ uint32 copyShaderSize;
	/* +0x58 */ MPTR copyShaderPtr;
	/* +0x5C */ uint32 shaderMode;
	/* +0x60 */ uint32 uniformBlockCount;
	/* +0x64 */ MPTR uniformBlockInfo;
	/* +0x68 */ uint32 uniformVarCount;
	/* +0x6C */ MPTR uniformVarInfo;
	/* +0x70 */ uint32 ukn70;
	/* +0x74 */ MPTR ukn74;
	/* +0x78 */ uint32 ukn78;
	/* +0x7C */ MPTR ukn7C;
	/* +0x80 */ uint32 samplerCount;
	/* +0x84 */ MPTR samplerInfo;
	/* +0x88 */ uint32 ringItemsize;
	/* +0x8C */ uint32 useStreamout;
	/* +0x90 */ uint32 streamoutStride[GX2_MAX_STREAMOUT_BUFFERS];
	/* +0xA0 */ GX2RBuffer rBuffer;
	/* +0xB0 */ GX2RBuffer rBufferCopyProgram;

	MPTR GetGeometryProgramAddr() const
	{
		if (_swapEndianU32(shaderPtr) != MPTR_NULL)
			return _swapEndianU32(shaderPtr);
		return rBuffer.GetVirtualAddr();
	}

	MPTR GetCopyProgramAddr() const
	{
		if (_swapEndianU32(copyShaderPtr) != MPTR_NULL)
			return _swapEndianU32(copyShaderPtr);
		return rBufferCopyProgram.GetVirtualAddr();
	}

};

static_assert(sizeof(GX2GeometryShader_t) == 0xC0);
