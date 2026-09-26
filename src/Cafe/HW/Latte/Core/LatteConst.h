#pragma once
#include "Cafe/HW/Latte/ISA/LatteReg.h"

// todo - this file contains legacy C-style defines, modernize and merge into LatteReg.h

// GPU7/Latte hardware info

#define LATTE_NUM_GPR						128
#define LATTE_NUM_STREAMOUT_BUFFER			4
#define LATTE_NUM_COLOR_TARGET				8

#define LATTE_NUM_MAX_TEX_UNITS				18 // number of available texture units per shader stage (this might be higher than 18? BotW is the only game which uses more than 16?)
#define LATTE_NUM_MAX_UNIFORM_BUFFERS		16 // number of supported uniform buffer binding locations per shader stage

#define LATTE_VS_ATTRIBUTE_LIMIT			32 // todo: verify
#define	LATTE_NUM_MAX_ATTRIBUTE_LOCATIONS	256 // should this be 128 since there are only 128 GPRs?

#define LATTE_MAX_VERTEX_BUFFERS			16

// Cemu-specific constants

#define LATTE_CEMU_PS_TEX_UNIT_BASE 		0
#define LATTE_CEMU_VS_TEX_UNIT_BASE 		32
#define LATTE_CEMU_GS_TEX_UNIT_BASE 		64

// vertex formats

#define LATTE_NFA_2				2
#define LATTE_NFA_3				3

#define LATTE_VTX_UNSIGNED		0
#define LATTE_VTX_SIGNED		1

// OpenGL constants

#define GLVENDOR_UNKNOWN			(0)
#define GLVENDOR_AMD				(1)	 // AMD/ATI
#define GLVENDOR_NVIDIA				(2)
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
		// shaders for drawing
		Vertex = 0,
		Pixel = 1,
		Geometry = 2,
		// compute shader
		Compute = 3,
		TotalCount = 4
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