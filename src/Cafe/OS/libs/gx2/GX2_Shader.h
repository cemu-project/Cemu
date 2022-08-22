#pragma once
#include "Cafe/HW/Latte/ISA/LatteReg.h"
#include "GX2_Streamout.h"

struct GX2FetchShader_t
{
	enum class FetchShaderType : uint32
	{
		NO_TESSELATION = 0,
	};

	/* +0x00 */ betype<FetchShaderType> fetchShaderType;
	/* +0x04 */ uint32 _regs[1];
	/* +0x08 */ uint32 shaderSize;
	/* +0x0C */ MPTR shaderPtr;
	/* +0x10 */ uint32 attribCount;
	/* +0x14 */ uint32 divisorCount;
	/* +0x18 */ uint32 divisors[2];

	MPTR GetProgramAddr() const
	{
		return _swapEndianU32(shaderPtr);
	}
};

static_assert(sizeof(GX2FetchShader_t) == 0x20);
static_assert(sizeof(betype<GX2FetchShader_t::FetchShaderType>) == 4);

namespace GX2
{

	void GX2ShaderInit();
}

// code below still needs to be modernized (use betype, enum classes)

#define GX2_SHADER_MODE_UNIFORM_REGISTER	0
#define GX2_SHADER_MODE_UNIFORM_BLOCK		1
#define GX2_SHADER_MODE_GEOMETRY_SHADER		2
#define GX2_SHADER_MODE_COMPUTE_SHADER		3

struct GX2VertexShader_t
{
	/* +0x000 */ uint32 regs[52];
	/* +0x0D0 */ uint32 shaderSize;
	/* +0x0D4 */ MPTR shaderPtr;
	/* +0x0D8 */ uint32 shaderMode; // GX2_SHADER_MODE_*
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
	/* +0x10C */ uint32 ringItemsize; // for GS
	/* +0x110 */ uint32 usesStreamOut;
	/* +0x114 */ uint32 streamOutVertexStride[GX2_MAX_STREAMOUT_BUFFERS];
	/* +0x124 */ GX2RBuffer rBuffer;

	MPTR GetProgramAddr() const
	{
		if (_swapEndianU32(this->shaderPtr) != MPTR_NULL)
			return _swapEndianU32(this->shaderPtr);
		return this->rBuffer.GetVirtualAddr();
	}
};

static_assert(sizeof(GX2VertexShader_t) == 0x134);

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
