#pragma once
#include "Cafe/HW/Latte/ISA/LatteReg.h"

// todo - this file contains legacy C-style defines, modernize and merge into LatteReg.h

// GPU7/Latte hardware info

#define LATTE_NUM_GPR						128
#define LATTE_NUM_STREAMOUT_BUFFER			4
#define LATTE_NUM_COLOR_TARGET				8

#define LATTE_NUM_MAX_TEX_UNITS				18 // number of available texture units per shader stage (this might be higher than 18? BotW is the only game which uses more than 16?)
#define LATTE_NUM_MAX_UNIFORM_BUFFERS		16 // number of supported uniform buffer binding locations

#define LATTE_VS_ATTRIBUTE_LIMIT			32 // todo: verify
#define	LATTE_NUM_MAX_ATTRIBUTE_LOCATIONS	256 // should this be 128 since there are only 128 GPRs?

#define LATTE_MAX_VERTEX_BUFFERS			16

// Cemu-specific constants

#define LATTE_CEMU_PS_TEX_UNIT_BASE 		0
#define LATTE_CEMU_VS_TEX_UNIT_BASE 		32
#define LATTE_CEMU_GS_TEX_UNIT_BASE 		64

// vertex formats

#define	FMT_INVALID				0x00
#define	FMT_8					0x01
#define	FMT_4_4					0x02
#define	FMT_3_3_2				0x03
#define	FMT_16					0x05
#define	FMT_16_FLOAT			0x06
#define	FMT_8_8					0x07
#define	FMT_5_6_5				0x08
#define	FMT_6_5_5				0x09
#define	FMT_1_5_5_5				0x0A
#define	FMT_4_4_4_4				0x0B
#define	FMT_5_5_5_1				0x0C
#define	FMT_32					0x0D
#define	FMT_32_FLOAT			0x0E
#define	FMT_16_16				0x0F
#define	FMT_16_16_FLOAT			0x10
#define	FMT_8_24				0x11
#define	FMT_8_24_FLOAT			0x12
#define	FMT_24_8				0x13
#define	FMT_24_8_FLOAT			0x14
#define	FMT_10_11_11			0x15
#define	FMT_10_11_11_FLOAT		0x16
#define	FMT_11_11_10			0x17
#define	FMT_11_11_10_FLOAT		0x18
#define	FMT_2_10_10_10			0x19
#define	FMT_8_8_8_8				0x1A
#define	FMT_10_10_10_2			0x1B
#define	FMT_X24_8_32_FLOAT		0x1C
#define	FMT_32_32				0x1D
#define	FMT_32_32_FLOAT			0x1E
#define	FMT_16_16_16_16			0x1F
#define	FMT_16_16_16_16_FLOAT	0x20
#define	FMT_32_32_32_32			0x22
#define	FMT_32_32_32_32_FLOAT	0x23
#define	FMT_1					0x25
#define	FMT_GB_GR				0x27
#define	FMT_BG_RG				0x28
#define	FMT_32_AS_8				0x29
#define	FMT_32_AS_8_8			0x2A
#define	FMT_5_9_9_9_SHAREDEXP	0x2B
#define	FMT_8_8_8				0x2C
#define	FMT_16_16_16			0x2D
#define	FMT_16_16_16_FLOAT		0x2E
#define	FMT_32_32_32			0x2F
#define	FMT_32_32_32_FLOAT		0x30

#define LATTE_NFA_2				2
#define LATTE_NFA_3				3

#define LATTE_VTX_UNSIGNED		0
#define LATTE_VTX_SIGNED		1

// OpenGL constants

#define GLVENDOR_UNKNOWN			(0)
#define GLVENDOR_AMD				(1)	 // AMD/ATI
#define GLVENDOR_NVIDIA				(2)
#define GLVENDOR_INTEL_LEGACY		(3)
#define GLVENDOR_INTEL_NOLEGACY		(4)
#define GLVENDOR_INTEL				(5)
#define GLVENDOR_APPLE				(6)

// decompiler

#define LATTE_DECOMPILER_DTYPE_UNDETERMINED			(0) // data type is unknown
#define LATTE_DECOMPILER_DTYPE_UNSIGNED_INT			(1) // 32bit unsigned integer
#define LATTE_DECOMPILER_DTYPE_SIGNED_INT			(2) // 32bit signed integer
#define LATTE_DECOMPILER_DTYPE_FLOAT				(3) // 32bit IEEE float

#define LATTE_DECOMPILER_UNIFORM_MODE_NONE			(0) // no uniform access at all
#define LATTE_DECOMPILER_UNIFORM_MODE_REMAPPED		(1)	// use remapped uniform array
#define LATTE_DECOMPILER_UNIFORM_MODE_FULL_CFILE	(2) // load full cfile (uniform registers)
#define LATTE_DECOMPILER_UNIFORM_MODE_FULL_CBANK	(3) // load full uniform banks (uniform buffers)

#define LATTE_ANALYZER_IMPORT_INDEX_PARAM_MAX		(0xFF)
#define LATTE_ANALYZER_IMPORT_INDEX_SPIPOSITION		(0x40000000) // gl_FragCoord

#define LATTE_DECOMPILER_SAMPLER_NONE				(0xFF)

using LattePrimitiveMode = Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE;
using LatteIndexType = Latte::LATTE_VGT_DMA_INDEX_TYPE::E_INDEX_TYPE;

namespace LatteConst
{
	enum class ShaderType : uint32
	{
		Reserved = 0,
		// shaders for drawing
		FirstRender = 1,
		Vertex = 1,
		Pixel = 2,
		Geometry = 3,
		LastRender = 3,
		// compute shader
		Compute = 4,
		TotalCount = 5
	};

	enum class VertexFetchNFA
	{
		NUM_FORMAT_NORMALIZED,
		NUM_FORMAT_INT,
		NUM_FORMAT_SCALED,
	};

	enum class VertexFetchEndianMode
	{
		SWAP_NONE = 0, // little endian
		SWAP_U16 = 1, // U16 big endian
		SWAP_U32 = 2, // U32 big endian
		// helper for GX2 API
		SWAP_DEFAULT = 3,
	};

	enum class VertexFetchFormat : uint32
	{
		// some formats are for texture fetches only

		VTX_FMT_INVALID					= 0x00,

		VTX_FMT_8						= 0x01,
		VTX_FMT_8_8						= 0x07,
		VTX_FMT_8_8_8					= 0x2C,
		VTX_FMT_8_8_8_8					= 0x1A,

		VTX_FMT_32_32					= 0x1D,
		VTX_FMT_32_32_FLOAT				= 0x1E,

		VTX_FMT_16_16_16				= 0x2D,
		VTX_FMT_16_16_16_FLOAT			= 0x2E,
		VTX_FMT_32_32_32				= 0x2F,
		VTX_FMT_32_32_32_FLOAT			= 0x30
	};

	enum class VertexFetchDstSel : uint8
	{
		X = 0,
		Y = 1,
		Z = 2,
		W = 3,
		CONST_0F = 4,
		CONST_1F = 5,
		UNUSED = 6,
		MASKED = 7
	};

	// used in VTX_WORD0
	enum VertexFetchType2 : uint8
	{
		VERTEX_DATA = 0,
		INSTANCE_DATA = 1,
		NO_INDEX_OFFSET_DATA = 2,
	};

};

#define LATTE_MAX_REGISTER			(0x10000)