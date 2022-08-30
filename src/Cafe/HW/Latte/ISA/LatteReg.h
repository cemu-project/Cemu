#pragma once

namespace Latte
{
	// common enums
	enum class E_DIM : uint32 // shared between Latte backend and GX2 code
	{
		DIM_1D = 0,
		DIM_2D = 1,
		DIM_3D = 2,
		DIM_CUBEMAP = 3,
		DIM_1D_ARRAY = 4,
		DIM_2D_ARRAY = 5,
		DIM_2D_MSAA = 6,
		DIM_2D_ARRAY_MSAA = 7
	};

	enum class E_AAMODE : uint32 // shared between Latte backend and GX2 code
	{
		AA_1X = 0,
		AA_2X = 1,
		AA_4X = 2,
		AA_8X = 3,
	};

	enum class E_HWTILEMODE
	{
		TM_LINEAR_GENERAL = 0,  // linear (pitch must be aligned to 8?)
		TM_LINEAR_ALIGNED = 1,  // pitch must be multiple of 64 pixels?

		TM_1D_TILED_THIN1 = 2,  // no macro tiling, 8x8 micro tiles
		TM_1D_TILED_THICK = 3,  // no macro tiling, 8x8x4 micro tiles

		TM_2D_TILED_THIN1 = 4,  // 4x2
		TM_2D_TILED_THIN2 = 5,  // 2x4
		TM_2D_TILED_THIN4 = 6,  // 1x8
		TM_2D_TILED_THICK = 7,  // 4x2x1

		TM_2B_TILED_THIN1 = 8,  // 4x2
		TM_2B_TILED_THIN2 = 9,  // 2x4
		TM_2B_TILED_THIN4 = 10, // 1x8
		TM_2B_TILED_THICK = 11, // 4x2x1

		TM_3D_TILED_THIN1 = 12, // 4x2
		TM_3D_TILED_THICK = 13, // 4x2x1

		TM_3B_TILED_THIN1 = 14, // 4x2
		TM_3B_TILED_THICK = 15, // 4x2x1
	};

	enum class E_GX2TILEMODE : uint32
	{
		// same as E_TILEMODE but contains additional options with special meaning
		TM_LINEAR_GENERAL = 0,
		TM_LINEAR_ALIGNED = 1, 

		// micro-tiled
		TM_1D_TILED_THIN1 = 2, 
		TM_1D_TILED_THICK = 3, 

		// macro-tiled
		TM_2D_TILED_THIN1 = 4, 
		TM_2D_TILED_THIN2 = 5, 
		TM_2D_TILED_THIN4 = 6, 
		TM_2D_TILED_THICK = 7, 

		TM_2B_TILED_THIN1 = 8, 
		TM_2B_TILED_THIN2 = 9, 
		TM_2B_TILED_THIN4 = 10,
		TM_2B_TILED_THICK = 11,

		TM_3D_TILED_THIN1 = 12,
		TM_3D_TILED_THICK = 13,

		TM_3B_TILED_THIN1 = 14,
		TM_3B_TILED_THICK = 15,

		// special
		TM_LINEAR_SPECIAL = 16,
		TM_32_SPECIAL	  = 32,
	};

	inline E_HWTILEMODE MakeHWTileMode(const E_GX2TILEMODE gx2Tilemode)
	{
		return (E_HWTILEMODE)((uint32)gx2Tilemode & 0xF);
	}

	inline E_GX2TILEMODE MakeGX2TileMode(const E_HWTILEMODE hwTilemode)
	{
		return (E_GX2TILEMODE)hwTilemode;
	}

	inline bool TM_IsMacroTiled(const E_HWTILEMODE tm)
	{
		return (uint32)tm >= 4;
	}

	inline bool TM_IsMacroTiled(const E_GX2TILEMODE tm)
	{
		return (uint32)tm >= 4 && (uint32)tm != 16;
	}

	inline bool TM_IsBankSwapped(const E_HWTILEMODE tileMode)
	{
		return
			tileMode == E_HWTILEMODE::TM_2B_TILED_THIN1 ||
			tileMode == E_HWTILEMODE::TM_2B_TILED_THIN2 ||
			tileMode == E_HWTILEMODE::TM_2B_TILED_THIN4 ||
			tileMode == E_HWTILEMODE::TM_2B_TILED_THICK ||
			tileMode == E_HWTILEMODE::TM_3B_TILED_THIN1 ||
			tileMode == E_HWTILEMODE::TM_3B_TILED_THICK;
	}

	enum class E_HWSURFFMT
	{
		INVALID_FORMAT = 0,
		// hardware formats only
		HWFMT_8 = 0x1,
		HWFMT_4_4 = 0x2,
		HWFMT_3_3_2 = 0x3,
		HWFMT_16 = 0x5,
		HWFMT_16_FLOAT = 0x6,
		HWFMT_8_8 = 0x7,
		HWFMT_5_6_5 = 0x8,
		HWFMT_6_5_5 = 0x9,
		HWFMT_1_5_5_5 = 0xA,
		HWFMT_4_4_4_4 = 0xB,
		HWFMT_5_5_5_1 = 0xC,
		HWFMT_32 = 0xD,
		HWFMT_32_FLOAT = 0xE,
		HWFMT_16_16 = 0xF,
		HWFMT_16_16_FLOAT = 0x10,
		HWFMT_8_24 = 0x11,
		HWFMT_8_24_FLOAT = 0x12,
		HWFMT_24_8 = 0x13,
		HWFMT_24_8_FLOAT = 0x14,
		HWFMT_10_11_11 = 0x15,
		HWFMT_10_11_11_FLOAT = 0x16,
		HWFMT_11_11_10 = 0x17,
		HWFMT_11_11_10_FLOAT = 0x18,
		HWFMT_2_10_10_10 = 0x19,
		HWFMT_8_8_8_8 = 0x1A,
		HWFMT_10_10_10_2 = 0x1B,
		HWFMT_X24_8_32_FLOAT = 0x1C,
		HWFMT_32_32 = 0x1D,
		HWFMT_32_32_FLOAT = 0x1E,
		HWFMT_16_16_16_16 = 0x1F,
		HWFMT_16_16_16_16_FLOAT = 0x20,
		HWFMT_32_32_32_32 = 0x22,
		HWFMT_32_32_32_32_FLOAT = 0x23,
		HWFMT_BC1 = 0x31,
		HWFMT_BC2 = 0x32,
		HWFMT_BC3 = 0x33,
		HWFMT_BC4 = 0x34,
		HWFMT_BC5 = 0x35,

		// these formats exist in R600/R700 documentation, but GX2 doesn't seem to handle them. Are they supported?
		U_HWFMT_BC6 = 0x36,
		U_HWFMT_BC7 = 0x37,
		U_HWFMT_32_32_32 = 0x2F,
		U_HWFMT_32_32_32_FLOAT = 0x30,


	};

	enum class E_GX2SURFFMT // GX2 surface format
	{
		INVALID_FORMAT = 0,
		// base hardware formats (shared with E_HWSURFFMT)
		HWFMT_8 = 0x1,
		HWFMT_4_4 = 0x2,
		HWFMT_3_3_2 = 0x3,
		HWFMT_16 = 0x5,
		HWFMT_16_FLOAT = 0x6,
		HWFMT_8_8 = 0x7,
		HWFMT_5_6_5 = 0x8,
		HWFMT_6_5_5 = 0x9,
		HWFMT_1_5_5_5 = 0xA,
		HWFMT_4_4_4_4 = 0xB,
		HWFMT_5_5_5_1 = 0xC,
		HWFMT_32 = 0xD,
		HWFMT_32_FLOAT = 0xE,		
		HWFMT_16_16 = 0xF,
		HWFMT_16_16_FLOAT = 0x10,
		HWFMT_8_24 = 0x11,
		HWFMT_8_24_FLOAT = 0x12,
		HWFMT_24_8 = 0x13,
		HWFMT_24_8_FLOAT = 0x14,
		HWFMT_10_11_11 = 0x15,
		HWFMT_10_11_11_FLOAT = 0x16,
		HWFMT_11_11_10 = 0x17,
		HWFMT_11_11_10_FLOAT = 0x18,
		HWFMT_2_10_10_10 = 0x19,
		HWFMT_8_8_8_8 = 0x1A,
		HWFMT_10_10_10_2 = 0x1B,
		HWFMT_X24_8_32_FLOAT = 0x1C,
		HWFMT_32_32 = 0x1D,
		HWFMT_32_32_FLOAT = 0x1E,
		HWFMT_16_16_16_16 = 0x1F,
		HWFMT_16_16_16_16_FLOAT = 0x20,
		HWFMT_32_32_32_32 = 0x22,
		HWFMT_32_32_32_32_FLOAT = 0x23,
		HWFMT_BC1 = 0x31,
		HWFMT_BC2 = 0x32,
		HWFMT_BC3 = 0x33,
		HWFMT_BC4 = 0x34,
		HWFMT_BC5 = 0x35,

		// GX2 extra format bits
		FMT_BIT_INT = 0x100,
		FMT_BIT_SIGNED = 0x200,
		FMT_BIT_SRGB = 0x400,
		FMT_BIT_FLOAT = 0x800,

		// GX2 formats
		R4_G4_UNORM = HWFMT_4_4,

		R5_G6_B5_UNORM = HWFMT_5_6_5,
		R5_G5_B5_A1_UNORM = HWFMT_1_5_5_5,
		R4_G4_B4_A4_UNORM = HWFMT_4_4_4_4,
		A1_B5_G5_R5_UNORM = HWFMT_5_5_5_1,

		R8_UNORM = HWFMT_8,
		R8_SNORM = (HWFMT_8 | FMT_BIT_SIGNED),
		R8_UINT = (HWFMT_8 | FMT_BIT_INT),
		R8_SINT = (HWFMT_8 | FMT_BIT_INT | FMT_BIT_SIGNED),

		R8_G8_UNORM = HWFMT_8_8,
		R8_G8_SNORM = (HWFMT_8_8 | FMT_BIT_SIGNED),
		R8_G8_UINT = (HWFMT_8_8 | FMT_BIT_INT),
		R8_G8_SINT = (HWFMT_8_8 | FMT_BIT_INT | FMT_BIT_SIGNED),

		R8_G8_B8_A8_UNORM = HWFMT_8_8_8_8,
		R8_G8_B8_A8_SNORM = (HWFMT_8_8_8_8 | FMT_BIT_SIGNED),
		R8_G8_B8_A8_UINT = (HWFMT_8_8_8_8 | FMT_BIT_INT),
		R8_G8_B8_A8_SINT = (HWFMT_8_8_8_8 | FMT_BIT_INT | FMT_BIT_SIGNED),
		R8_G8_B8_A8_SRGB = (HWFMT_8_8_8_8 | FMT_BIT_SRGB),

		R10_G10_B10_A2_UNORM = HWFMT_2_10_10_10,
		R10_G10_B10_A2_SNORM = (HWFMT_2_10_10_10 | FMT_BIT_SIGNED),
		R10_G10_B10_A2_UINT = (HWFMT_2_10_10_10 | FMT_BIT_INT),
		R10_G10_B10_A2_SINT = (HWFMT_2_10_10_10 | FMT_BIT_INT | FMT_BIT_SIGNED),
		R10_G10_B10_A2_SRGB = (HWFMT_2_10_10_10 | FMT_BIT_SRGB),

		A2_B10_G10_R10_UNORM = HWFMT_10_10_10_2,
		A2_B10_G10_R10_UINT = (HWFMT_10_10_10_2 | FMT_BIT_INT),

		R16_UNORM = HWFMT_16,
		R16_SNORM = (HWFMT_16 | FMT_BIT_SIGNED),
		R16_UINT = (HWFMT_16 | FMT_BIT_INT),
		R16_SINT = (HWFMT_16 | FMT_BIT_INT | FMT_BIT_SIGNED),
		R16_FLOAT = (HWFMT_16_FLOAT | FMT_BIT_FLOAT),

		R16_G16_UNORM = HWFMT_16_16,
		R16_G16_SNORM = (HWFMT_16_16 | FMT_BIT_SIGNED),
		R16_G16_UINT = (HWFMT_16_16 | FMT_BIT_INT),
		R16_G16_SINT = (HWFMT_16_16 | FMT_BIT_INT | FMT_BIT_SIGNED),
		R16_G16_FLOAT = (HWFMT_16_16_FLOAT | FMT_BIT_FLOAT),

		R16_G16_B16_A16_UNORM = HWFMT_16_16_16_16,
		R16_G16_B16_A16_SNORM = (HWFMT_16_16_16_16 | FMT_BIT_SIGNED),
		R16_G16_B16_A16_UINT = (HWFMT_16_16_16_16 | FMT_BIT_INT),
		R16_G16_B16_A16_SINT = (HWFMT_16_16_16_16 | FMT_BIT_INT | FMT_BIT_SIGNED),
		R16_G16_B16_A16_FLOAT = (HWFMT_16_16_16_16_FLOAT | FMT_BIT_FLOAT),


		R24_X8_UNORM = (HWFMT_8_24),
		R24_X8_FLOAT = (HWFMT_8_24 | FMT_BIT_FLOAT),
		X24_G8_UINT = (HWFMT_8_24 | FMT_BIT_INT),
		R32_X8_FLOAT = (HWFMT_X24_8_32_FLOAT | FMT_BIT_FLOAT), // R32_X8_FLOAT
		X32_G8_UINT_X24 = (HWFMT_X24_8_32_FLOAT | FMT_BIT_INT), // X32_G8_UINT

		R11_G11_B10_FLOAT = (HWFMT_10_11_11_FLOAT | FMT_BIT_FLOAT),

		// 32bit component formats do not support SNORM/UNORM (at least GX2 doesnt expose it)
		R32_UINT = (HWFMT_32 | FMT_BIT_INT),
		R32_SINT = (HWFMT_32 | FMT_BIT_INT | FMT_BIT_SIGNED),
		R32_FLOAT = (HWFMT_32_FLOAT | FMT_BIT_FLOAT),

		R32_G32_UINT = (HWFMT_32_32 | FMT_BIT_INT),
		R32_G32_SINT = (HWFMT_32_32 | FMT_BIT_INT | FMT_BIT_SIGNED),
		R32_G32_FLOAT = (HWFMT_32_32_FLOAT | FMT_BIT_FLOAT),

		R32_G32_B32_A32_UINT = (HWFMT_32_32_32_32 | FMT_BIT_INT),
		R32_G32_B32_A32_SINT = (HWFMT_32_32_32_32 | FMT_BIT_INT | FMT_BIT_SIGNED),
		R32_G32_B32_A32_FLOAT = (HWFMT_32_32_32_32_FLOAT | FMT_BIT_FLOAT),
	
		// depth
		D24_S8_UNORM = (HWFMT_8_24),
		D24_S8_FLOAT = (HWFMT_8_24 | FMT_BIT_FLOAT),
		D32_S8_FLOAT = (HWFMT_X24_8_32_FLOAT | FMT_BIT_FLOAT),
		D16_UNORM = HWFMT_16,
		D32_FLOAT = (HWFMT_32_FLOAT | FMT_BIT_FLOAT),

		// compressed formats
		BC1_UNORM = (HWFMT_BC1),
		BC1_SRGB = (HWFMT_BC1 | FMT_BIT_SRGB),
		BC2_UNORM = (HWFMT_BC2),
		BC2_SRGB = (HWFMT_BC2 | FMT_BIT_SRGB),
		BC3_UNORM = (HWFMT_BC3),
		BC3_SRGB = (HWFMT_BC3 | FMT_BIT_SRGB),
		BC4_UNORM = (HWFMT_BC4),
		BC4_SNORM = (HWFMT_BC4 | FMT_BIT_SIGNED),
		BC5_UNORM = (HWFMT_BC5),
		BC5_SNORM = (HWFMT_BC5 | FMT_BIT_SIGNED),

		// special
		NV12_UNORM = 0x81,
	};
	DEFINE_ENUM_FLAG_OPERATORS(E_GX2SURFFMT);

	inline uint32 GetFormatBits(const Latte::E_HWSURFFMT hwFmt)
	{
		const uint8 sBitsTable[0x40] = {
		0x00,0x08,0x08,0x00,0x00,0x10,0x10,0x10,
		0x10,0x10,0x10,0x10,0x10,0x20,0x20,0x20,
		0x20,0x20,0x00,0x20,0x00,0x00,0x20,0x00,
		0x00,0x20,0x20,0x20,0x40,0x40,0x40,0x40,
		0x40,0x00,0x80,0x80,0x00,0x00,0x00,0x10,
		0x10,0x20,0x20,0x20,0x00,0x00,0x00,0x60,
		0x60,0x40,0x80,0x80,0x40,0x80,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
		};
		cemu_assert((uint32)hwFmt < 0x40);
		return sBitsTable[(uint32)hwFmt];
	}

	inline uint32 GetFormatBits(const Latte::E_GX2SURFFMT gx2Fmt)
	{
		return GetFormatBits((Latte::E_HWSURFFMT)((uint32)gx2Fmt & 0x3F));
	}

	inline E_HWSURFFMT GetHWFormat(E_GX2SURFFMT format)
	{
		return (E_HWSURFFMT)((uint32)format & 0x3F);
	}

	inline bool IsCompressedFormat(Latte::E_HWSURFFMT format)
	{
		return (uint32)format >= 0x31 && (uint32)format <= 0x35;
	}

	inline bool IsCompressedFormat(Latte::E_GX2SURFFMT format)
	{
		return IsCompressedFormat((Latte::E_HWSURFFMT)((uint32)format & 0x3F));
	}

	enum GPU_LIMITS
	{
		NUM_VERTEX_BUFFERS = 16,
		NUM_TEXTURES_PER_STAGE = 18, 
		NUM_SAMPLERS_PER_STAGE = 18, // is this 16 or 18?
		NUM_COLOR_ATTACHMENTS = 8,
	};

	enum REGADDR
	{
		VGT_PRIMITIVE_TYPE					= 0x2256,

		// each stage has 12 sets of 4 border color registers
		TD_PS_SAMPLER0_BORDER_RED			= 0x2900,
		TD_PS_SAMPLER0_BORDER_GREEN			= 0x2901,
		TD_PS_SAMPLER0_BORDER_BLUE			= 0x2902,
		TD_PS_SAMPLER0_BORDER_ALPHA			= 0x2903,

		TD_VS_SAMPLER0_BORDER_RED			= 0x2980,
		TD_VS_SAMPLER0_BORDER_GREEN			= 0x2981,
		TD_VS_SAMPLER0_BORDER_BLUE			= 0x2982,
		TD_VS_SAMPLER0_BORDER_ALPHA			= 0x2983,

		TD_GS_SAMPLER0_BORDER_RED			= 0x2A00,
		TD_GS_SAMPLER0_BORDER_GREEN			= 0x2A01,
		TD_GS_SAMPLER0_BORDER_BLUE			= 0x2A02,
		TD_GS_SAMPLER0_BORDER_ALPHA			= 0x2A03,

		DB_STENCIL_CLEAR					= 0xA00A,
		DB_DEPTH_CLEAR						= 0xA00B,

		CB_TARGET_MASK                      = 0xA08E,

		PA_SC_GENERIC_SCISSOR_TL			= 0xA090,
		PA_SC_GENERIC_SCISSOR_BR			= 0xA091,

		VGT_MULTI_PRIM_IB_RESET_INDX		= 0xA103,
		SX_ALPHA_TEST_CONTROL				= 0xA104,
		CB_BLEND_RED						= 0xA105,
		CB_BLEND_GREEN						= 0xA106,
		CB_BLEND_BLUE						= 0xA107,
		CB_BLEND_ALPHA						= 0xA108,

		DB_STENCILREFMASK					= 0xA10C,
		DB_STENCILREFMASK_BF				= 0xA10D,
		SX_ALPHA_REF						= 0xA10E,
		PA_CL_VPORT_XSCALE					= 0xA10F,
		PA_CL_VPORT_XOFFSET					= 0xA110,
		PA_CL_VPORT_YSCALE					= 0xA111,
		PA_CL_VPORT_YOFFSET					= 0xA112,
		PA_CL_VPORT_ZSCALE					= 0xA113,
		PA_CL_VPORT_ZOFFSET					= 0xA114,

		CB_BLEND0_CONTROL					= 0xA1E0, // first
		CB_BLEND7_CONTROL					= 0xA1E7, // last

		DB_DEPTH_CONTROL					= 0xA200,

		CB_COLOR_CONTROL					= 0xA202,

		PA_CL_CLIP_CNTL						= 0xA204,
		PA_SU_SC_MODE_CNTL					= 0xA205,
		PA_CL_VTE_CNTL						= 0xA206,
		
		PA_SU_POINT_SIZE					= 0xA280,
		PA_SU_POINT_MINMAX					= 0xA281,

		VGT_GS_MODE							= 0xA290,

		VGT_DMA_INDEX_TYPE					= 0xA29F, // todo - verify offset

		// HiZ early stencil test?
		DB_SRESULTS_COMPARE_STATE0			= 0xA34A,
		DB_SRESULTS_COMPARE_STATE1			= 0xA34B,

		PA_SU_POLY_OFFSET_CLAMP				= 0xA37F,
		PA_SU_POLY_OFFSET_FRONT_SCALE		= 0xA380,
		PA_SU_POLY_OFFSET_FRONT_OFFSET		= 0xA381,
		PA_SU_POLY_OFFSET_BACK_SCALE		= 0xA382,
		PA_SU_POLY_OFFSET_BACK_OFFSET		= 0xA383,

		// texture units
		SQ_TEX_RESOURCE_WORD0_N_PS			= 0xE000,
		SQ_TEX_RESOURCE_WORD0_N_VS			= 0xE460,
		SQ_TEX_RESOURCE_WORD0_N_GS			= 0xE930,
		SQ_TEX_RESOURCE_WORD_FIRST			= SQ_TEX_RESOURCE_WORD0_N_PS,
		SQ_TEX_RESOURCE_WORD_LAST			= (SQ_TEX_RESOURCE_WORD0_N_GS + GPU_LIMITS::NUM_TEXTURES_PER_STAGE * 7 - 1),
		// there are 54 samplers with 3 registers each. 18 per stage. For stage indices see SAMPLER_BASE_INDEX_*
		SQ_TEX_SAMPLER_WORD0_0				= 0xF000,
		SQ_TEX_SAMPLER_WORD1_0				= 0xF001,
		SQ_TEX_SAMPLER_WORD2_0				= 0xF002,


	};

	inline constexpr int SAMPLER_BASE_INDEX_PIXEL = 0;
	inline constexpr int SAMPLER_BASE_INDEX_VERTEX = 18;
	inline constexpr int SAMPLER_BASE_INDEX_GEOMETRY = 36;

#define LATTE_BITFIELD(__regname, __bitIndex, __bitWidth) \
auto& set_##__regname(uint32 newValue) \
{	\
	cemu_assert_debug(newValue < (1u << (__bitWidth))); \
	v &= ~((((1u << (__bitWidth)) - 1u) << (__bitIndex))); \
	v |= (newValue << (__bitIndex)); \
    return *this; \
} \
uint32 get_##__regname() const \
{	\
	return (v >> (__bitIndex))&((1u << (__bitWidth)) - 1u); \
}

#define LATTE_BITFIELD_SIGNED(__regname, __bitIndex, __bitWidth) \
auto& set_##__regname(sint32 newValue) \
{	\
	cemu_assert_debug(newValue < (1 << ((__bitWidth)-1))); \
	cemu_assert_debug(newValue >= -(1 << ((__bitWidth)-1))); \
	v &= ~((((1u << (__bitWidth)) - 1u) << (__bitIndex))); \
	v |= (((uint32)newValue & ((1u << (__bitWidth)) - 1u)) << (__bitIndex)); \
    return *this; \
} \
sint32 get_##__regname() const \
{	\
    sint32 r = (v >> (__bitIndex))&((1u << (__bitWidth)) - 1u); \
    r = (r << (32 - (__bitWidth))); \
    r = (r >> (32 - (__bitWidth))); \
	return r; \
}

#define LATTE_BITFIELD_BOOL(__regname, __bitIndex) \
auto& set_##__regname(bool newValue) \
{	\
	if(newValue) \
	v |= (1u << (__bitIndex)); \
	else \
	v &= ~(1u << (__bitIndex)); \
    return *this; \
} \
bool get_##__regname() const \
{	\
	return (v&(1u << (__bitIndex))) != 0; \
}

#define LATTE_BITFIELD_TYPED(__regname, __bitIndex, __bitWidth, __typename) \
auto& set_##__regname(__typename newValue) \
{	\
	cemu_assert_debug(static_cast<uint32>(newValue) < (1u << (__bitWidth))); \
	v &= ~((((1u << (__bitWidth)) - 1u) << (__bitIndex))); \
	v |= (static_cast<uint32>(newValue) << (__bitIndex)); \
    return *this; \
} \
__typename get_##__regname() const \
{	\
	return static_cast<__typename>((v >> (__bitIndex))&((1u << (__bitWidth)) - 1u)); \
}

#define LATTE_BITFIELD_FULL_TYPED(__regname, __typename) \
auto& set_##__regname(__typename newValue) \
{	\
	v = (static_cast<uint32>(newValue)); \
    return *this; \
} \
__typename get_##__regname() const \
{	\
	return static_cast<__typename>(v); \
}

#define LATTE_BITFIELD_FLOAT(__regname) \
auto& set_##__regname(float newValue) \
{	\
	*(float*)&v = newValue; \
    return *this; \
} \
float get_##__regname() const \
{	\
	return *(float*)&v; \
}

	class LATTEREG
	{
	public:
		uint32 getRawValue() const
		{
			return v;
		}

		uint32 getRawValueBE() const
		{
			return _swapEndianU32(v);
		}

	protected:
		uint32 v{};
	};

	// shared enums
	enum class E_COMPAREFUNC // used by depth test func and alpha test func
	{
		NEVER,
		LESS,
		EQUAL,
		LEQUAL,
		GREATER,
		NOTEQUAL,
		GEQUAL,
		ALWAYS
	};

	enum class E_ENDIAN_SWAP
	{
		SWAP_NONE = 0,
	};

	struct LATTE_VGT_PRIMITIVE_TYPE : LATTEREG // 0x2256
	{
		enum class E_PRIMITIVE_TYPE
		{
			NONE = 0x0,
			POINTS = 0x1,
			LINES = 0x2,
			LINE_STRIP = 0x3,
			TRIANGLES = 0x4,
			TRIANGLE_FAN = 0x5,
			TRIANGLE_STRIP = 0x6,

			LINES_ADJACENT = 0xA,
			LINE_STRIP_ADJACENT = 0xB,
			TRIANGLES_ADJACENT = 0xC,
			TRIANGLE_STRIP_ADJACENT = 0xD,

			RECTS = 0x11,
			LINE_LOOP = 0x12,
			QUADS = 0x13,
			QUAD_STRIP = 0x14,
		};

		LATTE_BITFIELD_FULL_TYPED(PRIMITIVE_MODE, E_PRIMITIVE_TYPE);
	};

	struct LATTE_TD_BORDER_COLOR : LATTEREG // 0x2900 - 0x2A47
	{
		LATTE_BITFIELD_FLOAT(channelValue);
	};

	struct LATTE_DB_STENCIL_CLEAR : LATTEREG // 0xA00A
	{
		LATTE_BITFIELD(clearValue, 0, 8);
	};

	struct LATTE_DB_DEPTH_CLEAR : LATTEREG // 0xA00B
	{
		LATTE_BITFIELD_FLOAT(clearValue);
	};

	struct LATTE_CB_TARGET_MASK : LATTEREG // 0xA08E
	{
		LATTE_BITFIELD_FULL_TYPED(MASK, uint32);
	};

	struct LATTE_PA_SC_GENERIC_SCISSOR_TL : LATTEREG // 0xA090
	{
		LATTE_BITFIELD(TL_X, 0, 15);
		LATTE_BITFIELD(TL_Y, 16, 15);
		LATTE_BITFIELD_BOOL(WINDOW_OFFSET_DISABLE, 31);
	};

	struct LATTE_PA_SC_GENERIC_SCISSOR_BR : LATTEREG // 0xA091
	{
		LATTE_BITFIELD(BR_X, 0, 15);
		LATTE_BITFIELD(BR_Y, 16, 15);
	};

	struct LATTE_VGT_MULTI_PRIM_IB_RESET_INDX : LATTEREG // 0xA103
	{
		LATTE_BITFIELD_FULL_TYPED(RESTART_INDEX, uint32);
	};

	struct LATTE_SX_ALPHA_TEST_CONTROL : LATTEREG // 0xA104
	{
		using E_ALPHA_FUNC = E_COMPAREFUNC;

		LATTE_BITFIELD_TYPED(ALPHA_FUNC, 0, 3, E_ALPHA_FUNC);
		LATTE_BITFIELD_BOOL(ALPHA_TEST_ENABLE, 3);
		LATTE_BITFIELD_BOOL(ALPHA_TEST_BYPASS, 8);
	};

	struct LATTE_CB_BLEND_RED : LATTEREG // 0xA105
	{
		LATTE_BITFIELD_FLOAT(RED);
	};

	struct LATTE_CB_BLEND_GREEN : LATTEREG // 0xA106
	{
		LATTE_BITFIELD_FLOAT(GREEN);
	};

	struct LATTE_CB_BLEND_BLUE : LATTEREG // 0xA107
	{
		LATTE_BITFIELD_FLOAT(BLUE);
	};

	struct LATTE_CB_BLEND_ALPHA : LATTEREG // 0xA108
	{
		LATTE_BITFIELD_FLOAT(ALPHA);
	};

	struct LATTE_DB_STENCILREFMASK : LATTEREG // 0xA10C
	{
		LATTE_BITFIELD(STENCILREF_F, 0, 8);
		LATTE_BITFIELD(STENCILMASK_F, 8, 8);
		LATTE_BITFIELD(STENCILWRITEMASK_F, 16, 8);
	};

	struct LATTE_DB_STENCILREFMASK_BF : LATTEREG // 0xA10D
	{
		LATTE_BITFIELD(STENCILREF_B, 0, 8);
		LATTE_BITFIELD(STENCILMASK_B, 8, 8);
		LATTE_BITFIELD(STENCILWRITEMASK_B, 16, 8);
	};

	struct LATTE_SX_ALPHA_REF : LATTEREG // 0xA10E
	{
		LATTE_BITFIELD_FLOAT(ALPHA_TEST_REF);
	};

	struct LATTE_PA_CL_VPORT_XSCALE : LATTEREG // 0xA10F
	{
		LATTE_BITFIELD_FLOAT(SCALE);
	};

	struct LATTE_PA_CL_VPORT_XOFFSET : LATTEREG // 0xA110
	{
		LATTE_BITFIELD_FLOAT(OFFSET);
	};

	struct LATTE_PA_CL_VPORT_YSCALE : LATTEREG // 0xA111
	{
		LATTE_BITFIELD_FLOAT(SCALE);
	};

	struct LATTE_PA_CL_VPORT_YOFFSET : LATTEREG // 0xA112
	{
		LATTE_BITFIELD_FLOAT(OFFSET);
	};

	struct LATTE_PA_CL_VPORT_ZSCALE : LATTEREG // 0xA113
	{
		LATTE_BITFIELD_FLOAT(SCALE);
	};

	struct LATTE_PA_CL_VPORT_ZOFFSET : LATTEREG // 0xA114
	{
		LATTE_BITFIELD_FLOAT(OFFSET);
	};

	struct LATTE_CB_BLENDN_CONTROL : LATTEREG // 0xA1E0 - 0xA1E7
	{
		enum class E_BLENDFACTOR
		{
			BLEND_ZERO						= 0x00,
			BLEND_ONE						= 0x01,
			BLEND_SRC_COLOR					= 0x02,
			BLEND_ONE_MINUS_SRC_COLOR		= 0x03,
			BLEND_SRC_ALPHA					= 0x04,
			BLEND_ONE_MINUS_SRC_ALPHA		= 0x05,
			BLEND_DST_ALPHA					= 0x06,
			BLEND_ONE_MINUS_DST_ALPHA		= 0x07,
			BLEND_DST_COLOR					= 0x08,
			BLEND_ONE_MINUS_DST_COLOR		= 0x09,
			BLEND_SRC_ALPHA_SATURATE		= 0x0A,
			BLEND_BOTH_SRC_ALPHA			= 0x0B,
			BLEND_BOTH_INV_SRC_ALPHA		= 0x0C,
			BLEND_CONST_COLOR				= 0x0D,
			BLEND_ONE_MINUS_CONST_COLOR		= 0x0E,
			BLEND_SRC1_COLOR				= 0x0F,
			BLEND_INV_SRC1_COLOR			= 0x10,
			BLEND_SRC1_ALPHA				= 0x11,
			BLEND_INV_SRC1_ALPHA			= 0x12,
			BLEND_CONST_ALPHA				= 0x13,
			BLEND_ONE_MINUS_CONST_ALPHA		= 0x14
		};

		enum class E_COMBINEFUNC
		{
			DST_PLUS_SRC					= 0,
			SRC_MINUS_DST					= 1,
			MIN_DST_SRC						= 2,
			MAX_DST_SRC						= 3,
			DST_MINUS_SRC					= 4
		};

		LATTE_BITFIELD_TYPED(COLOR_SRCBLEND, 0, 5, E_BLENDFACTOR);
		LATTE_BITFIELD_TYPED(COLOR_COMB_FCN, 5, 3, E_COMBINEFUNC);
		LATTE_BITFIELD_TYPED(COLOR_DSTBLEND, 8, 5, E_BLENDFACTOR);
		LATTE_BITFIELD_BOOL(OPACITY_WEIGHT, 13);
		LATTE_BITFIELD_TYPED(ALPHA_SRCBLEND, 16, 5, E_BLENDFACTOR);
		LATTE_BITFIELD_TYPED(ALPHA_COMB_FCN, 21, 3, E_COMBINEFUNC);
		LATTE_BITFIELD_TYPED(ALPHA_DSTBLEND, 24, 5, E_BLENDFACTOR);
		LATTE_BITFIELD_BOOL(SEPARATE_ALPHA_BLEND, 29);
	};

	struct LATTE_DB_DEPTH_CONTROL : LATTEREG // 0xA200
	{
		using E_ZFUNC = E_COMPAREFUNC;
		using E_STENCILFUNC = E_COMPAREFUNC;

		enum class E_STENCILACTION
		{
			KEEP,
			ZERO,
			REPLACE,
			INCR,
			DECR,
			INVERT,
			INCR_WRAP,
			DECR_WRAP,
		};

		LATTE_BITFIELD_BOOL(STENCIL_ENABLE, 0);
		LATTE_BITFIELD_BOOL(Z_ENABLE, 1);
		LATTE_BITFIELD_BOOL(Z_WRITE_ENABLE, 2);
		// bit 3 is unused?
		LATTE_BITFIELD_TYPED(Z_FUNC, 4, 3, E_ZFUNC);
		LATTE_BITFIELD_BOOL(BACK_STENCIL_ENABLE, 7);

		LATTE_BITFIELD_TYPED(STENCIL_FUNC_F, 8, 3, E_STENCILFUNC);
		LATTE_BITFIELD_TYPED(STENCIL_FAIL_F, 11, 3, E_STENCILACTION);
		LATTE_BITFIELD_TYPED(STENCIL_ZPASS_F, 14, 3, E_STENCILACTION);
		LATTE_BITFIELD_TYPED(STENCIL_ZFAIL_F, 17, 3, E_STENCILACTION);

		LATTE_BITFIELD_TYPED(STENCIL_FUNC_B, 20, 3, E_STENCILFUNC);
		LATTE_BITFIELD_TYPED(STENCIL_FAIL_B, 23, 3, E_STENCILACTION);
		LATTE_BITFIELD_TYPED(STENCIL_ZPASS_B, 26, 3, E_STENCILACTION);
		LATTE_BITFIELD_TYPED(STENCIL_ZFAIL_B, 29, 3, E_STENCILACTION);
	};

	struct LATTE_CB_COLOR_CONTROL : LATTEREG // 0xA202
	{
		enum class E_SPECIALOP
		{
			NORMAL = 0, // use state to render
			DISABLE = 1, // dont write color results
		};

		enum class E_LOGICOP
		{
			CLEAR = 0x00,
			COPY = 0xCC,
			OR = 0xEE,
			SET = 0xFF,
		};

		LATTE_BITFIELD_BOOL(FOG_ENABLE, 0);
		LATTE_BITFIELD_BOOL(MULTIWRITE_ENABLE, 1);
		LATTE_BITFIELD_BOOL(DITHER_ENABLE, 2);
		LATTE_BITFIELD_BOOL(DEGAMMA_ENABLE, 3);
		LATTE_BITFIELD_TYPED(SPECIAL_OP, 4, 3, E_SPECIALOP);
		LATTE_BITFIELD_BOOL(PER_MRT_BLEND, 7);
		LATTE_BITFIELD(BLEND_MASK, 8, 8);
		LATTE_BITFIELD_TYPED(ROP, 16, 8, E_LOGICOP); // aka logic op
	};

	struct LATTE_PA_CL_CLIP_CNTL : LATTEREG // 0xA204
	{
		// todo - other fields
		// see R6xx_3D_Registers.pdf

		LATTE_BITFIELD_BOOL(CLIP_DISABLE, 16);

		LATTE_BITFIELD_BOOL(DX_CLIP_SPACE_DEF, 19); // GX2 calls this flag HalfZ


		LATTE_BITFIELD_BOOL(DX_RASTERIZATION_KILL, 22);

		LATTE_BITFIELD_BOOL(DX_LINEAR_ATTR_CLIP_ENA, 24); // what does this do?

		LATTE_BITFIELD_BOOL(ZCLIP_NEAR_DISABLE, 26);
		LATTE_BITFIELD_BOOL(ZCLIP_FAR_DISABLE, 27);

	};

	struct LATTE_PA_CL_VTE_CNTL : LATTEREG // 0xA206
	{
		LATTE_BITFIELD_BOOL(VPORT_X_SCALE_ENA, 0);
		LATTE_BITFIELD_BOOL(VPORT_X_OFFSET_ENA, 1);

		LATTE_BITFIELD_BOOL(VPORT_Y_SCALE_ENA, 2);
		LATTE_BITFIELD_BOOL(VPORT_Y_OFFSET_ENA, 3);

		LATTE_BITFIELD_BOOL(VPORT_Z_SCALE_ENA, 4);
		LATTE_BITFIELD_BOOL(VPORT_Z_OFFSET_ENA, 5);

		LATTE_BITFIELD_BOOL(VTX_XY_FMT, 8);
		LATTE_BITFIELD_BOOL(VTX_Z_FMT, 9);
		LATTE_BITFIELD_BOOL(VTX_W0_FMT, 10);
	};

	struct LATTE_PA_SU_POINT_SIZE : LATTEREG // 0xA280
	{
		LATTE_BITFIELD(HEIGHT, 0, 16);
		LATTE_BITFIELD(WIDTH, 16, 16);
	};

	struct LATTE_PA_SU_POINT_MINMAX : LATTEREG // 0xA281
	{
		LATTE_BITFIELD(MIN_SIZE, 0, 16);
		LATTE_BITFIELD(MAX_SIZE, 16, 16);
	};

	struct LATTE_VGT_GS_MODE : LATTEREG // 0xA290
	{
		enum class E_MODE
		{
			OFF = 0,
			SCENARIO_A = 1,
			SCENARIO_B = 2,
			SCENARIO_G = 3
		};

		enum class E_CUT_MODE
		{
			CUT_1024 = 0,
			CUT_512 = 1,
			CUT_256 = 2,
			CUT_128 = 3,
		};

		enum class E_COMPUTE_MODE
		{
			OFF = 0,
			UKN_1 = 1,
			UKN_2 = 2,
			ON = 3,
		};

		LATTE_BITFIELD_TYPED(MODE, 0, 2, E_MODE);
		LATTE_BITFIELD_BOOL(ES_PASSTHRU, 2);
		LATTE_BITFIELD_TYPED(CUT_MODE, 3, 2, E_CUT_MODE);
		LATTE_BITFIELD_TYPED(COMPUTE_MODE, 14, 2, E_COMPUTE_MODE);
		LATTE_BITFIELD_BOOL(PARTIAL_THD_AT_EOI, 17);
	};

	struct LATTE_VGT_DMA_INDEX_TYPE : LATTEREG // 0xA29F
	{
		enum class E_INDEX_TYPE
		{
			U16_LE = 0, // U16
			U32_LE = 1, // U32

			U16_BE = 4, // U16 + SwapU16
			U32_BE = 9, // U32 + SwapU32

			// index 0 -> U16
			// index 1 -> U32
			// bit 0x2 -> swap U16
			// bit 0x4 -> swap U32
			// bit 0x8 -> swap U32 (machine word?)

			AUTO = 0xFFFF, // helper value for tracking auto-generated indices. Not part of the actual enum
		};

		LATTE_BITFIELD_FULL_TYPED(INDEX_TYPE, E_INDEX_TYPE);
	};

	struct LATTE_PA_SU_POLY_OFFSET_CLAMP : LATTEREG // 0xA37F
	{
		LATTE_BITFIELD_FLOAT(CLAMP);
	};

	struct LATTE_PA_SU_POLY_OFFSET_FRONT_SCALE : LATTEREG // 0xA380
	{
		LATTE_BITFIELD_FLOAT(SCALE);
	};

	struct LATTE_PA_SU_POLY_OFFSET_FRONT_OFFSET : LATTEREG // 0xA381
	{
		LATTE_BITFIELD_FLOAT(OFFSET);
	};

	struct LATTE_PA_SU_POLY_OFFSET_BACK_SCALE : LATTEREG // 0xA382
	{
		LATTE_BITFIELD_FLOAT(SCALE);
	};

	struct LATTE_PA_SU_POLY_OFFSET_BACK_OFFSET : LATTEREG // 0xA383
	{
		LATTE_BITFIELD_FLOAT(OFFSET);
	};

	struct LATTE_SQ_TEX_RESOURCE_WORD0_N : LATTEREG // 0xE000 + index * 7
	{
		LATTE_BITFIELD_TYPED(DIM, 0, 3, E_DIM);
		LATTE_BITFIELD_TYPED(TILE_MODE, 3, 4, E_HWTILEMODE);

		LATTE_BITFIELD_BOOL(TILE_TYPE, 7);

		LATTE_BITFIELD(PITCH, 8, 11);
		LATTE_BITFIELD(WIDTH, 19, 13);
	};

	struct LATTE_SQ_TEX_RESOURCE_WORD1_N : LATTEREG // 0xE001 + index * 7
	{
		LATTE_BITFIELD(HEIGHT, 0, 13);
		LATTE_BITFIELD(DEPTH, 13, 13);
		LATTE_BITFIELD_TYPED(DATA_FORMAT, 26, 6, E_HWSURFFMT);
	};

	struct LATTE_SQ_TEX_RESOURCE_WORD2_N : LATTEREG // 0xE002 + index * 7
	{
		LATTE_BITFIELD_FULL_TYPED(BASE_ADDRESS, uint32);
	};

	struct LATTE_SQ_TEX_RESOURCE_WORD3_N : LATTEREG // 0xE003 + index * 7
	{
		LATTE_BITFIELD_FULL_TYPED(MIP_ADDRESS, uint32);
	};

	struct LATTE_SQ_TEX_RESOURCE_WORD4_N : LATTEREG // 0xE004 + index * 7
	{
		enum class E_FORMAT_COMP
		{
			COMP_UNSIGNED = 0,
			COMP_SIGNED = 1,
			COMP_UNSIGNED_BIASED = 2,
		};

		enum class E_NUM_FORMAT_ALL
		{
			NUM_FORMAT_NORM = 0,
			NUM_FORMAT_INT = 1,
			NUM_FORMAT_SCALED = 2,
		};

		enum class E_SRF_MODE_ALL
		{
			SRF_MODE_ZERO_CLAMP_MINUS_ONE = 0,
			SRF_MODE_NO_ZERO = 1,
		};

		// using E_ENDIAN_SWAP = E_ENDIAN_SWAP;

		enum class E_SEL
		{
			SEL_X = 0,
			SEL_Y = 1,
			SEL_Z = 2,
			SEL_W = 3,
			SEL_0 = 4,
			SEL_1 = 5
		};

		LATTE_BITFIELD_TYPED(FORMAT_COMP_X, 0, 2, E_FORMAT_COMP);
		LATTE_BITFIELD_TYPED(FORMAT_COMP_Y, 2, 2, E_FORMAT_COMP);
		LATTE_BITFIELD_TYPED(FORMAT_COMP_Z, 4, 2, E_FORMAT_COMP);
		LATTE_BITFIELD_TYPED(FORMAT_COMP_W, 6, 2, E_FORMAT_COMP);
		LATTE_BITFIELD_TYPED(NUM_FORM_ALL, 8, 2, E_NUM_FORMAT_ALL);
		LATTE_BITFIELD_TYPED(SRF_MODE_ALL, 10, 1, E_SRF_MODE_ALL);
		LATTE_BITFIELD_BOOL(FORCE_DEGAMMA, 11);
		LATTE_BITFIELD_TYPED(ENDIAN_SWAP, 12, 2, E_ENDIAN_SWAP);
		LATTE_BITFIELD(REQUEST_SIZE, 14, 2);
		LATTE_BITFIELD_TYPED(DST_SEL_X, 16, 3, E_SEL);
		LATTE_BITFIELD_TYPED(DST_SEL_Y, 19, 3, E_SEL);
		LATTE_BITFIELD_TYPED(DST_SEL_Z, 22, 3, E_SEL);
		LATTE_BITFIELD_TYPED(DST_SEL_W, 25, 3, E_SEL);
		LATTE_BITFIELD(BASE_LEVEL, 28, 4);
	};

	struct LATTE_SQ_TEX_RESOURCE_WORD5_N : LATTEREG // 0xE005 + index * 7
	{
		LATTE_BITFIELD(LAST_LEVEL, 0, 4); // for MSAA textures, this stores the AA level
		LATTE_BITFIELD(BASE_ARRAY, 4, 13);
		LATTE_BITFIELD(LAST_ARRAY, 17, 13);
		LATTE_BITFIELD_BOOL(UKN_BIT_30, 30); // may be a 2 bit value?
	};

	struct LATTE_SQ_TEX_RESOURCE_WORD6_N : LATTEREG // 0xE006 + index * 7
	{
		enum class E_MPEG_CLAMP
		{
			UKN = 0,
		};

		enum class E_TYPE
		{
			VTX_INVALID_TEXTURE = 0,
			VTX_INVALID_BUFFER = 1,
			VTX_VALID_TEXTURE = 2,
			VTX_VALID_BUFFER = 3,
		};

		// unsure if these are correct

		LATTE_BITFIELD_TYPED(MPEG_CLAMP, 0, 2, E_MPEG_CLAMP);
		LATTE_BITFIELD(MAX_ANISO, 2, 3);
		LATTE_BITFIELD(PERF_MODULATION, 5, 3);
		LATTE_BITFIELD_BOOL(INTERLACED, 8);
		LATTE_BITFIELD_TYPED(TYPE, 30, 2, E_TYPE);
	};

	struct LATTE_SQ_TEX_SAMPLER_WORD0_0 : LATTEREG // 0xF000+n*3 - 0xF???
	{
		enum class E_CLAMP
		{
			WRAP = 0,
			MIRROR = 1,
			CLAMP_LAST_TEXEL = 2,
			MIRROR_ONCE_LAST_TEXEL = 3,
			CLAMP_HALF_BORDER = 4,
			MIRROR_ONCE_HALF_BORDER = 5,
			CLAMP_BORDER = 6,
			MIRROR_ONCE_BORDER = 7,
		};

		enum class E_XY_FILTER
		{
			POINT = 0,
			BILINEAR = 1,
			BICUBIC = 2,
			// 3 unused ?
			ANISO_POINT = 4,
			ANISO_BILINEAR = 5,
			// 6, 7 unused ?
		};

		enum class E_Z_FILTER
		{
			NONE = 0,
			POINT = 1,
			LINEAR = 2,
			// 3 is unused ?
		};

		enum class E_BORDER_COLOR_TYPE
		{
			TRANSPARENT_BLACK = 0,
			OPAQUE_BLACK = 1,
			OPAQUE_WHITE = 2,
			REGISTER = 3,
		};

		enum class E_DEPTH_COMPARE
		{
			NEVER = 0,
			LESS = 1,
			EQUAL = 2,
			LEQUAL = 3,
			GREATER = 4,
			NOTEQUAL = 5,
			GEQUAL = 6,
			ALWAYS = 7
		};


		enum class E_CHROMA_KEY
		{
			DISABLE = 0,
			KILL = 1,
			BLEND = 2,
			// 3 is unused
		};

		LATTE_BITFIELD_TYPED(CLAMP_X, 0, 3, E_CLAMP);
		LATTE_BITFIELD_TYPED(CLAMP_Y, 3, 3, E_CLAMP);
		LATTE_BITFIELD_TYPED(CLAMP_Z, 6, 3, E_CLAMP);
		LATTE_BITFIELD_TYPED(XY_MAG_FILTER, 9, 3, E_XY_FILTER);
		LATTE_BITFIELD_TYPED(XY_MIN_FILTER, 12, 3, E_XY_FILTER);
		LATTE_BITFIELD_TYPED(Z_FILTER, 15, 2, E_Z_FILTER);
		LATTE_BITFIELD_TYPED(MIP_FILTER, 17, 2, E_Z_FILTER);
		LATTE_BITFIELD(MAX_ANISO_RATIO, 19, 3);
		LATTE_BITFIELD_TYPED(BORDER_COLOR_TYPE, 22, 2, E_BORDER_COLOR_TYPE);
		LATTE_BITFIELD_BOOL(POINT_SAMPLING_CLAMP, 24);
		LATTE_BITFIELD_BOOL(TEX_ARRAY_OVERRIDE, 25);
		LATTE_BITFIELD_TYPED(DEPTH_COMPARE_FUNCTION, 26, 3, E_DEPTH_COMPARE);
		LATTE_BITFIELD_TYPED(CHROMA_KEY, 28, 2, E_CHROMA_KEY);
		LATTE_BITFIELD_BOOL(LOD_USES_MINOR_AXIS, 25);
	};

	struct LATTE_SQ_TEX_SAMPLER_WORD1_0 : LATTEREG // 0xF001+n*3 - 0xF???
	{
		LATTE_BITFIELD(MIN_LOD, 0, 10);
		LATTE_BITFIELD(MAX_LOD, 10, 10);
		LATTE_BITFIELD_SIGNED(LOD_BIAS, 20, 12);
	};

	struct LATTE_SQ_TEX_SAMPLER_WORD2_0 : LATTEREG // 0xF002+n*3 - 0xF???
	{
		enum E_SAMPLER_TYPE
		{
			UKN0 = 0,
			UKN1 = 1,
		};

		LATTE_BITFIELD(LOD_BIAS_SECONDARY, 0, 12);
		LATTE_BITFIELD_BOOL(MC_COORD_TRUNCATE, 12);
		LATTE_BITFIELD_BOOL(FORCE_DEGAMMA, 13);
		LATTE_BITFIELD_BOOL(HIGH_PRECISION_FILTER, 14);

		// PERF_MIP at 15, 3 bits
		// PERF_Z at 18, 3 bits
		// bit 21 is unused?

		LATTE_BITFIELD(ANISO_BIAS, 22, 6); // is size correct?

		//LATTE_BITFIELD_BOOL(FETCH_4, 26); overlaps with ANISO_BIAS
		//LATTE_BITFIELD_BOOL(SAMPLE_IS_PCF, 27); overlaps with ANISO_BIAS

		LATTE_BITFIELD_TYPED(TYPE, 31, 1, E_SAMPLER_TYPE);
	};

	struct LATTE_PA_SU_SC_MODE_CNTL : LATTEREG // 0xA205
	{
		enum class E_FRONTFACE
		{
			CCW = 0,
			CW = 1
		};

		enum class E_POLYGONMODE
		{
			UKN0 = 0, // default - render triangles
		};

		enum class E_PTYPE
		{
			POINTS = 0,
			LINES = 1,
			TRIANGLES = 2,
		};

		LATTE_BITFIELD_BOOL(CULL_FRONT, 0);
		LATTE_BITFIELD_BOOL(CULL_BACK, 1);
		LATTE_BITFIELD_TYPED(FRONT_FACE, 2, 1, E_FRONTFACE);
		LATTE_BITFIELD_TYPED(POLYGON_MODE, 3, 2, E_POLYGONMODE);
		LATTE_BITFIELD_TYPED(FRONT_POLY_MODE, 5, 3, E_PTYPE);
		LATTE_BITFIELD_TYPED(BACK_POLY_MODE, 8, 3, E_PTYPE);
		LATTE_BITFIELD_BOOL(OFFSET_FRONT_ENABLED, 11);
		LATTE_BITFIELD_BOOL(OFFSET_BACK_ENABLED, 12);
		LATTE_BITFIELD_BOOL(OFFSET_PARA_ENABLED, 13); // offset enable for lines and points?
		// additional fields?
	};
}

struct _LatteRegisterSetTextureUnit
{
	Latte::LATTE_SQ_TEX_RESOURCE_WORD0_N word0;
	Latte::LATTE_SQ_TEX_RESOURCE_WORD1_N word1;
	Latte::LATTE_SQ_TEX_RESOURCE_WORD2_N word2;
	Latte::LATTE_SQ_TEX_RESOURCE_WORD3_N word3;
	Latte::LATTE_SQ_TEX_RESOURCE_WORD4_N word4;
	Latte::LATTE_SQ_TEX_RESOURCE_WORD5_N word5;
	Latte::LATTE_SQ_TEX_RESOURCE_WORD6_N word6;
};

static_assert(sizeof(_LatteRegisterSetTextureUnit) == 28);

struct _LatteRegisterSetSampler
{
	Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0 WORD0;
	Latte::LATTE_SQ_TEX_SAMPLER_WORD1_0 WORD1;
	Latte::LATTE_SQ_TEX_SAMPLER_WORD2_0 WORD2;
};

static_assert(sizeof(_LatteRegisterSetSampler) == 12);

struct _LatteRegisterSetSamplerBorderColor
{
	Latte::LATTE_TD_BORDER_COLOR red;
	Latte::LATTE_TD_BORDER_COLOR green;
	Latte::LATTE_TD_BORDER_COLOR blue;
	Latte::LATTE_TD_BORDER_COLOR alpha;
};

static_assert(sizeof(_LatteRegisterSetSamplerBorderColor) == 16);

struct LatteContextRegister
{
	uint8 padding0[0x08958];

	/* +0x08958 */ Latte::LATTE_VGT_PRIMITIVE_TYPE VGT_PRIMITIVE_TYPE;
	uint8 padding5[0x0A400 - 0x0895C];
	/* +0x0A400 */ _LatteRegisterSetSamplerBorderColor TD_PS_SAMPLER_BORDER_COLOR[Latte::GPU_LIMITS::NUM_SAMPLERS_PER_STAGE];
	uint8 padding6[0x0A600 - 0x0A520];
	/* +0x0A600 */ _LatteRegisterSetSamplerBorderColor TD_VS_SAMPLER_BORDER_COLOR[Latte::GPU_LIMITS::NUM_SAMPLERS_PER_STAGE];
	uint8 padding7[0x0A800 - 0x0A720];
	/* +0x0A800 */ _LatteRegisterSetSamplerBorderColor TD_GS_SAMPLER_BORDER_COLOR[Latte::GPU_LIMITS::NUM_SAMPLERS_PER_STAGE];
	uint8 padding8[0x28238 - 0x0A920];
	/* +0x28238 */ Latte::LATTE_CB_TARGET_MASK CB_TARGET_MASK;
	uint8 padding_2823C[4];
	/* +0x28240 */ Latte::LATTE_PA_SC_GENERIC_SCISSOR_TL PA_SC_GENERIC_SCISSOR_TL;
	/* +0x28244 */ Latte::LATTE_PA_SC_GENERIC_SCISSOR_BR PA_SC_GENERIC_SCISSOR_BR;
	uint8 padding_28248[0x2840C - 0x28248];
	/* +0x2840C */ Latte::LATTE_VGT_MULTI_PRIM_IB_RESET_INDX VGT_MULTI_PRIM_IB_RESET_INDX;
	/* +0x28410 */ Latte::LATTE_SX_ALPHA_TEST_CONTROL SX_ALPHA_TEST_CONTROL;
	/* +0x28414 */ Latte::LATTE_CB_BLEND_RED CB_BLEND_RED;
	/* +0x28418 */ Latte::LATTE_CB_BLEND_GREEN CB_BLEND_GREEN;
	/* +0x2841C */ Latte::LATTE_CB_BLEND_BLUE CB_BLEND_BLUE;
	/* +0x28420 */ Latte::LATTE_CB_BLEND_ALPHA CB_BLEND_ALPHA;
	uint8 padding_28424[0x28430 - 0x28424];
	/* +0x28430 */ Latte::LATTE_DB_STENCILREFMASK DB_STENCILREFMASK;
	/* +0x28434 */ Latte::LATTE_DB_STENCILREFMASK_BF DB_STENCILREFMASK_BF;
	/* +0x28438 */ Latte::LATTE_SX_ALPHA_REF SX_ALPHA_REF;
	/* +0x2843C */ Latte::LATTE_PA_CL_VPORT_XSCALE	PA_CL_VPORT_XSCALE;
	/* +0x28440 */ Latte::LATTE_PA_CL_VPORT_XOFFSET PA_CL_VPORT_XOFFSET;
	/* +0x28444 */ Latte::LATTE_PA_CL_VPORT_YSCALE	PA_CL_VPORT_YSCALE;
	/* +0x28448 */ Latte::LATTE_PA_CL_VPORT_YOFFSET PA_CL_VPORT_YOFFSET;
	/* +0x2844C */ Latte::LATTE_PA_CL_VPORT_ZSCALE	PA_CL_VPORT_ZSCALE;
	/* +0x28450 */ Latte::LATTE_PA_CL_VPORT_ZOFFSET PA_CL_VPORT_ZOFFSET;

	uint8 padding_28450[0x28780 - 0x28454];

	/* +0x28780 */ Latte::LATTE_CB_BLENDN_CONTROL CB_BLENDN_CONTROL[8];

	uint8 padding_287A0[0x28800 - 0x287A0];

	/* +0x28800 */ Latte::LATTE_DB_DEPTH_CONTROL DB_DEPTH_CONTROL;
	uint8 padding_28804[4];
	/* +0x28808 */ Latte::LATTE_CB_COLOR_CONTROL CB_COLOR_CONTROL;
	uint8 padding_2880C[4];
	/* +0x28810 */ Latte::LATTE_PA_CL_CLIP_CNTL PA_CL_CLIP_CNTL;
	/* +0x28814 */ Latte::LATTE_PA_SU_SC_MODE_CNTL PA_SU_SC_MODE_CNTL;
	/* +0x28818 */ Latte::LATTE_PA_CL_VTE_CNTL PA_CL_VTE_CNTL;

	uint8 padding_2881C[0x28A00 - 0x2881C];

	/* +0x28A00 */ Latte::LATTE_PA_SU_POINT_SIZE PA_SU_POINT_SIZE;
	/* +0x28A04 */ Latte::LATTE_PA_SU_POINT_MINMAX PA_SU_POINT_MINMAX;

	uint8 padding_28A08[0x28A40 - 0x28A08];

	/* +0x28A40 */ Latte::LATTE_VGT_GS_MODE VGT_GS_MODE;

	uint8 padding_28A44[0x28A7C - 0x28A44];

	/* +0x28A7C */ Latte::LATTE_VGT_DMA_INDEX_TYPE VGT_DMA_INDEX_TYPE;

	uint8 padding_28A80[0x28DFC - 0x28A80];

	/* +0x28DFC */ Latte::LATTE_PA_SU_POLY_OFFSET_CLAMP PA_SU_POLY_OFFSET_CLAMP;
	/* +0x28E00 */ Latte::LATTE_PA_SU_POLY_OFFSET_FRONT_SCALE PA_SU_POLY_OFFSET_FRONT_SCALE;
	/* +0x28E04 */ Latte::LATTE_PA_SU_POLY_OFFSET_FRONT_OFFSET PA_SU_POLY_OFFSET_FRONT_OFFSET;
	/* +0x28E08 */ Latte::LATTE_PA_SU_POLY_OFFSET_BACK_SCALE PA_SU_POLY_OFFSET_BACK_SCALE;
	/* +0x28E0C */ Latte::LATTE_PA_SU_POLY_OFFSET_BACK_OFFSET PA_SU_POLY_OFFSET_BACK_OFFSET;

	uint8 padding_28E10[0x38000 - 0x28E10];

	// texture units are mapped as following (18 sets each):
	// 0xE000 0x38000 -> pixel shader
	// 0xE460 0x39180 -> vertex shader / compute shader
	// 0xE930 0x3A4C0 -> geometry shader
	// there is register space for exactly 160 ps tex units and 176 vs tex units. It's unknown how many GS units there are.

	/* +0x38000 */ _LatteRegisterSetTextureUnit SQ_TEX_START_PS[Latte::GPU_LIMITS::NUM_TEXTURES_PER_STAGE];
	_LatteRegisterSetTextureUnit _DUMMY_TEX_UNITS_PS[160 - Latte::GPU_LIMITS::NUM_TEXTURES_PER_STAGE];
	/* +0x39180 */ _LatteRegisterSetTextureUnit SQ_TEX_START_VS[Latte::GPU_LIMITS::NUM_TEXTURES_PER_STAGE];
	_LatteRegisterSetTextureUnit _DUMMY_TEX_UNITS_VS[176 - Latte::GPU_LIMITS::NUM_TEXTURES_PER_STAGE];
	/* +0x3A4C0 */ _LatteRegisterSetTextureUnit SQ_TEX_START_GS[Latte::GPU_LIMITS::NUM_TEXTURES_PER_STAGE];

	uint8 padding_3A6B8[0x3C000 - 0x3A6B8];
	
	/* +0x3C000 */ _LatteRegisterSetSampler SQ_TEX_SAMPLER[18 * 3];

	/* +0x3C288 */
	uint8 padding_3C288[0x40000 - 0x3C288];

	/* +0x40000 */
	// special state registers
	uint32 hleSpecialState[9]; // deprecated

	uint32* GetRawView() const
	{
		return (uint32*)padding0;
	}

	uint32* GetSpecialStateValues() const
	{
		return (uint32*)hleSpecialState;
	}
};

static_assert(sizeof(LatteContextRegister) == 0x10000 * 4 + 9 * 4);

static_assert(offsetof(LatteContextRegister, VGT_PRIMITIVE_TYPE) == Latte::REGADDR::VGT_PRIMITIVE_TYPE * 4);
static_assert(offsetof(LatteContextRegister, TD_PS_SAMPLER_BORDER_COLOR) == Latte::REGADDR::TD_PS_SAMPLER0_BORDER_RED * 4);
static_assert(offsetof(LatteContextRegister, TD_VS_SAMPLER_BORDER_COLOR) == Latte::REGADDR::TD_VS_SAMPLER0_BORDER_RED * 4);
static_assert(offsetof(LatteContextRegister, TD_GS_SAMPLER_BORDER_COLOR) == Latte::REGADDR::TD_GS_SAMPLER0_BORDER_RED * 4);
static_assert(offsetof(LatteContextRegister, CB_TARGET_MASK) == Latte::REGADDR::CB_TARGET_MASK * 4);
static_assert(offsetof(LatteContextRegister, PA_SC_GENERIC_SCISSOR_TL) == Latte::REGADDR::PA_SC_GENERIC_SCISSOR_TL * 4);
static_assert(offsetof(LatteContextRegister, PA_SC_GENERIC_SCISSOR_BR) == Latte::REGADDR::PA_SC_GENERIC_SCISSOR_BR * 4);
static_assert(offsetof(LatteContextRegister, VGT_MULTI_PRIM_IB_RESET_INDX) == Latte::REGADDR::VGT_MULTI_PRIM_IB_RESET_INDX * 4);
static_assert(offsetof(LatteContextRegister, SX_ALPHA_TEST_CONTROL) == Latte::REGADDR::SX_ALPHA_TEST_CONTROL * 4);
static_assert(offsetof(LatteContextRegister, DB_STENCILREFMASK) == Latte::REGADDR::DB_STENCILREFMASK * 4);
static_assert(offsetof(LatteContextRegister, DB_STENCILREFMASK_BF) == Latte::REGADDR::DB_STENCILREFMASK_BF * 4);
static_assert(offsetof(LatteContextRegister, SX_ALPHA_REF) == Latte::REGADDR::SX_ALPHA_REF * 4);
static_assert(offsetof(LatteContextRegister, CB_BLEND_RED) == Latte::REGADDR::CB_BLEND_RED * 4);
static_assert(offsetof(LatteContextRegister, CB_BLEND_GREEN) == Latte::REGADDR::CB_BLEND_GREEN * 4);
static_assert(offsetof(LatteContextRegister, CB_BLEND_BLUE) == Latte::REGADDR::CB_BLEND_BLUE * 4);
static_assert(offsetof(LatteContextRegister, CB_BLEND_ALPHA) == Latte::REGADDR::CB_BLEND_ALPHA * 4);
static_assert(offsetof(LatteContextRegister, PA_CL_VPORT_XSCALE) == Latte::REGADDR::PA_CL_VPORT_XSCALE * 4);
static_assert(offsetof(LatteContextRegister, PA_CL_VPORT_XOFFSET) == Latte::REGADDR::PA_CL_VPORT_XOFFSET * 4);
static_assert(offsetof(LatteContextRegister, PA_CL_VPORT_YSCALE) == Latte::REGADDR::PA_CL_VPORT_YSCALE * 4);
static_assert(offsetof(LatteContextRegister, PA_CL_VPORT_YOFFSET) == Latte::REGADDR::PA_CL_VPORT_YOFFSET * 4);
static_assert(offsetof(LatteContextRegister, PA_CL_VPORT_ZSCALE) == Latte::REGADDR::PA_CL_VPORT_ZSCALE * 4);
static_assert(offsetof(LatteContextRegister, PA_CL_VPORT_ZOFFSET) == Latte::REGADDR::PA_CL_VPORT_ZOFFSET * 4);
static_assert(offsetof(LatteContextRegister, PA_CL_CLIP_CNTL) == Latte::REGADDR::PA_CL_CLIP_CNTL * 4);
static_assert(offsetof(LatteContextRegister, PA_SU_SC_MODE_CNTL) == Latte::REGADDR::PA_SU_SC_MODE_CNTL * 4);
static_assert(offsetof(LatteContextRegister, PA_CL_VTE_CNTL) == Latte::REGADDR::PA_CL_VTE_CNTL * 4);
static_assert(offsetof(LatteContextRegister, PA_SU_POINT_SIZE) == Latte::REGADDR::PA_SU_POINT_SIZE * 4);
static_assert(offsetof(LatteContextRegister, PA_SU_POINT_MINMAX) == Latte::REGADDR::PA_SU_POINT_MINMAX * 4);
static_assert(offsetof(LatteContextRegister, CB_BLENDN_CONTROL) == Latte::REGADDR::CB_BLEND0_CONTROL * 4);
static_assert(offsetof(LatteContextRegister, DB_DEPTH_CONTROL) == Latte::REGADDR::DB_DEPTH_CONTROL * 4);
static_assert(offsetof(LatteContextRegister, CB_COLOR_CONTROL) == Latte::REGADDR::CB_COLOR_CONTROL * 4);
static_assert(offsetof(LatteContextRegister, VGT_GS_MODE) == Latte::REGADDR::VGT_GS_MODE * 4);
static_assert(offsetof(LatteContextRegister, VGT_DMA_INDEX_TYPE) == Latte::REGADDR::VGT_DMA_INDEX_TYPE * 4);
static_assert(offsetof(LatteContextRegister, PA_SU_POLY_OFFSET_CLAMP) == Latte::REGADDR::PA_SU_POLY_OFFSET_CLAMP * 4);
static_assert(offsetof(LatteContextRegister, PA_SU_POLY_OFFSET_FRONT_SCALE) == Latte::REGADDR::PA_SU_POLY_OFFSET_FRONT_SCALE * 4);
static_assert(offsetof(LatteContextRegister, PA_SU_POLY_OFFSET_FRONT_OFFSET) == Latte::REGADDR::PA_SU_POLY_OFFSET_FRONT_OFFSET * 4);
static_assert(offsetof(LatteContextRegister, PA_SU_POLY_OFFSET_BACK_SCALE) == Latte::REGADDR::PA_SU_POLY_OFFSET_BACK_SCALE * 4);
static_assert(offsetof(LatteContextRegister, PA_SU_POLY_OFFSET_BACK_OFFSET) == Latte::REGADDR::PA_SU_POLY_OFFSET_BACK_OFFSET * 4);
static_assert(offsetof(LatteContextRegister, SQ_TEX_START_PS) == Latte::REGADDR::SQ_TEX_RESOURCE_WORD0_N_PS * 4);
static_assert(offsetof(LatteContextRegister, SQ_TEX_START_VS) == Latte::REGADDR::SQ_TEX_RESOURCE_WORD0_N_VS * 4);
static_assert(offsetof(LatteContextRegister, SQ_TEX_START_GS) == Latte::REGADDR::SQ_TEX_RESOURCE_WORD0_N_GS * 4);
static_assert(offsetof(LatteContextRegister, SQ_TEX_SAMPLER) == Latte::REGADDR::SQ_TEX_SAMPLER_WORD0_0 * 4);
