#include "Cafe/HW/Latte/Renderer/Metal/LatteToMtl.h"
#include "Common/precompiled.h"
#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLVertexDescriptor.hpp"

std::map<Latte::E_GX2SURFFMT, MtlPixelFormatInfo> MTL_COLOR_FORMAT_TABLE = {
	{Latte::E_GX2SURFFMT::R4_G4_UNORM, {MTL::PixelFormatRG8Unorm, 2}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::R5_G6_B5_UNORM, {MTL::PixelFormatB5G6R5Unorm, 2}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::R5_G5_B5_A1_UNORM, {MTL::PixelFormatBGR5A1Unorm, 2}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::R4_G4_B4_A4_UNORM, {MTL::PixelFormatABGR4Unorm, 2}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::A1_B5_G5_R5_UNORM, {MTL::PixelFormatA1BGR5Unorm, 2}},
	{Latte::E_GX2SURFFMT::R8_UNORM, {MTL::PixelFormatR8Unorm, 1}},
	{Latte::E_GX2SURFFMT::R8_SNORM, {MTL::PixelFormatR8Snorm, 1}},
	{Latte::E_GX2SURFFMT::R8_UINT, {MTL::PixelFormatR8Uint, 1}},
	{Latte::E_GX2SURFFMT::R8_SINT, {MTL::PixelFormatR8Sint, 1}},
	{Latte::E_GX2SURFFMT::R8_G8_UNORM, {MTL::PixelFormatRG8Unorm, 2}},
	{Latte::E_GX2SURFFMT::R8_G8_SNORM, {MTL::PixelFormatRG8Snorm, 2}},
	{Latte::E_GX2SURFFMT::R8_G8_UINT, {MTL::PixelFormatRG8Uint, 2}},
	{Latte::E_GX2SURFFMT::R8_G8_SINT, {MTL::PixelFormatRG8Sint, 2}},
	{Latte::E_GX2SURFFMT::R8_G8_B8_A8_UNORM, {MTL::PixelFormatRGBA8Unorm, 4}},
	{Latte::E_GX2SURFFMT::R8_G8_B8_A8_SNORM, {MTL::PixelFormatRGBA8Snorm, 4}},
	{Latte::E_GX2SURFFMT::R8_G8_B8_A8_UINT, {MTL::PixelFormatRGBA8Uint, 4}},
	{Latte::E_GX2SURFFMT::R8_G8_B8_A8_SINT, {MTL::PixelFormatRGBA8Sint, 4}},
	{Latte::E_GX2SURFFMT::R8_G8_B8_A8_SRGB, {MTL::PixelFormatRGBA8Unorm_sRGB, 4}},
	{Latte::E_GX2SURFFMT::R10_G10_B10_A2_UNORM, {MTL::PixelFormatRGB10A2Unorm, 4}},
	{Latte::E_GX2SURFFMT::R10_G10_B10_A2_SNORM, {MTL::PixelFormatRGBA16Snorm, 8}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::R10_G10_B10_A2_UINT, {MTL::PixelFormatRGB10A2Uint, 4}},
	{Latte::E_GX2SURFFMT::R10_G10_B10_A2_SINT, {MTL::PixelFormatRGBA16Sint, 8}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::R10_G10_B10_A2_SRGB, {MTL::PixelFormatRGBA8Unorm_sRGB, 4}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::A2_B10_G10_R10_UNORM, {MTL::PixelFormatBGR10A2Unorm, 4}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::A2_B10_G10_R10_UINT, {MTL::PixelFormatRGB10A2Uint, 4}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::R16_UNORM, {MTL::PixelFormatR16Unorm, 2}},
	{Latte::E_GX2SURFFMT::R16_SNORM, {MTL::PixelFormatR16Snorm, 2}},
	{Latte::E_GX2SURFFMT::R16_UINT, {MTL::PixelFormatR16Uint, 2}},
	{Latte::E_GX2SURFFMT::R16_SINT, {MTL::PixelFormatR16Sint, 2}},
	{Latte::E_GX2SURFFMT::R16_FLOAT, {MTL::PixelFormatR16Float, 2}},
	{Latte::E_GX2SURFFMT::R16_G16_UNORM, {MTL::PixelFormatRG16Unorm, 4}},
	{Latte::E_GX2SURFFMT::R16_G16_SNORM, {MTL::PixelFormatRG16Snorm, 4}},
	{Latte::E_GX2SURFFMT::R16_G16_UINT, {MTL::PixelFormatRG16Uint, 4}},
	{Latte::E_GX2SURFFMT::R16_G16_SINT, {MTL::PixelFormatRG16Sint, 4}},
	{Latte::E_GX2SURFFMT::R16_G16_FLOAT, {MTL::PixelFormatRG16Float, 4}},
	{Latte::E_GX2SURFFMT::R16_G16_B16_A16_UNORM, {MTL::PixelFormatRGBA16Unorm, 8}},
	{Latte::E_GX2SURFFMT::R16_G16_B16_A16_SNORM, {MTL::PixelFormatRGBA16Snorm, 8}},
	{Latte::E_GX2SURFFMT::R16_G16_B16_A16_UINT, {MTL::PixelFormatRGBA16Uint, 8}},
	{Latte::E_GX2SURFFMT::R16_G16_B16_A16_SINT, {MTL::PixelFormatRGBA16Sint, 8}},
	{Latte::E_GX2SURFFMT::R16_G16_B16_A16_FLOAT, {MTL::PixelFormatRGBA16Float, 8}},
	{Latte::E_GX2SURFFMT::R24_X8_UNORM, {MTL::PixelFormatInvalid, 0}}, // TODO
	{Latte::E_GX2SURFFMT::R24_X8_FLOAT, {MTL::PixelFormatInvalid, 0}}, // TODO
	{Latte::E_GX2SURFFMT::X24_G8_UINT, {MTL::PixelFormatInvalid, 0}}, // TODO
	{Latte::E_GX2SURFFMT::R32_X8_FLOAT, {MTL::PixelFormatInvalid, 0}}, // TODO
	{Latte::E_GX2SURFFMT::X32_G8_UINT_X24, {MTL::PixelFormatInvalid, 0}}, // TODO
	{Latte::E_GX2SURFFMT::R11_G11_B10_FLOAT, {MTL::PixelFormatRG11B10Float, 4}},
	{Latte::E_GX2SURFFMT::R32_UINT, {MTL::PixelFormatR32Uint, 4}},
	{Latte::E_GX2SURFFMT::R32_SINT, {MTL::PixelFormatR32Sint, 4}},
	{Latte::E_GX2SURFFMT::R32_FLOAT, {MTL::PixelFormatR32Float, 4}},
	{Latte::E_GX2SURFFMT::R32_G32_UINT, {MTL::PixelFormatRG32Uint, 8}},
	{Latte::E_GX2SURFFMT::R32_G32_SINT, {MTL::PixelFormatRG32Sint, 8}},
	{Latte::E_GX2SURFFMT::R32_G32_FLOAT, {MTL::PixelFormatRG32Float, 8}},
	{Latte::E_GX2SURFFMT::R32_G32_B32_A32_UINT, {MTL::PixelFormatRGBA32Uint, 16}},
	{Latte::E_GX2SURFFMT::R32_G32_B32_A32_SINT, {MTL::PixelFormatRGBA32Sint, 16}},
	{Latte::E_GX2SURFFMT::R32_G32_B32_A32_FLOAT, {MTL::PixelFormatRGBA32Float, 16}},
	{Latte::E_GX2SURFFMT::BC1_UNORM, {MTL::PixelFormatBC1_RGBA, 8, {4, 4}}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::BC1_SRGB, {MTL::PixelFormatBC1_RGBA_sRGB, 8, {4, 4}}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::BC2_UNORM, {MTL::PixelFormatBC2_RGBA, 16, {4, 4}}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::BC2_SRGB, {MTL::PixelFormatBC2_RGBA_sRGB, 16, {4, 4}}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::BC3_UNORM, {MTL::PixelFormatBC3_RGBA, 16, {4, 4}}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::BC3_SRGB, {MTL::PixelFormatBC3_RGBA_sRGB, 16, {4, 4}}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::BC4_UNORM, {MTL::PixelFormatBC4_RUnorm, 8, {4, 4}}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::BC4_SNORM, {MTL::PixelFormatBC4_RSnorm, 8, {4, 4}}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::BC5_UNORM, {MTL::PixelFormatBC5_RGUnorm, 16, {4, 4}}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::BC5_SNORM, {MTL::PixelFormatBC5_RGSnorm, 16, {4, 4}}}, // TODO: correct?
};

std::map<Latte::E_GX2SURFFMT, MtlPixelFormatInfo> MTL_DEPTH_FORMAT_TABLE = {
	{Latte::E_GX2SURFFMT::D24_S8_UNORM, {MTL::PixelFormatDepth24Unorm_Stencil8, 4}}, // TODO: not supported on Apple sillicon, maybe find something else
	{Latte::E_GX2SURFFMT::D24_S8_FLOAT, {MTL::PixelFormatDepth32Float_Stencil8, 4}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::D32_S8_FLOAT, {MTL::PixelFormatDepth32Float_Stencil8, 5}},
	{Latte::E_GX2SURFFMT::D16_UNORM, {MTL::PixelFormatDepth16Unorm, 2}},
	{Latte::E_GX2SURFFMT::D32_FLOAT, {MTL::PixelFormatDepth32Float, 4}},
};

const MtlPixelFormatInfo GetMtlPixelFormatInfo(Latte::E_GX2SURFFMT format, bool isDepth)
{
    MtlPixelFormatInfo formatInfo;
    if (isDepth)
        formatInfo = MTL_DEPTH_FORMAT_TABLE[format];
    else
        formatInfo = MTL_COLOR_FORMAT_TABLE[format];

    // Depth24Unorm_Stencil8 is not supported on Apple sillicon
    // TODO: query if format is available instead
    if (formatInfo.pixelFormat == MTL::PixelFormatDepth24Unorm_Stencil8)
    {
        formatInfo.pixelFormat = MTL::PixelFormatDepth32Float_Stencil8;
    }

    if (formatInfo.pixelFormat == MTL::PixelFormatInvalid)
    {
        printf("invalid pixel format: %u\n", (uint32)format);
    }

    return formatInfo;
}

inline uint32 CeilDivide(uint32 a, uint32 b) {
    return (a + b - 1) / b;
}

size_t GetMtlTextureBytesPerRow(Latte::E_GX2SURFFMT format, bool isDepth, uint32 width)
{
    const auto& formatInfo = GetMtlPixelFormatInfo(format, isDepth);

    return CeilDivide(width, formatInfo.blockTexelSize.x) * formatInfo.bytesPerBlock;
}

size_t GetMtlTextureBytesPerImage(Latte::E_GX2SURFFMT format, bool isDepth, uint32 height, size_t bytesPerRow)
{
    const auto& formatInfo = GetMtlPixelFormatInfo(format, isDepth);

    return CeilDivide(height, formatInfo.blockTexelSize.y) * bytesPerRow;
}

MTL::PrimitiveType GetMtlPrimitiveType(LattePrimitiveMode mode)
{
    switch (mode)
    {
        case LattePrimitiveMode::POINTS:
            return MTL::PrimitiveTypePoint;
        case LattePrimitiveMode::LINES:
            return MTL::PrimitiveTypeLine;
        case LattePrimitiveMode::TRIANGLES:
            return MTL::PrimitiveTypeTriangle;
        case LattePrimitiveMode::TRIANGLE_STRIP:
            return MTL::PrimitiveTypeTriangleStrip;
        default:
            printf("unimplemented primitive type %u\n", (uint32)mode);
            cemu_assert_debug(false);
            return MTL::PrimitiveTypeTriangle;
    }
}

MTL::VertexFormat GetMtlVertexFormat(uint8 format)
{
    switch (format)
	{
	case FMT_32_32_32_32_FLOAT:
		return MTL::VertexFormatUInt4;
	case FMT_32_32_32_FLOAT:
		return MTL::VertexFormatUInt3;
	case FMT_32_32_FLOAT:
		return MTL::VertexFormatUInt2;
	case FMT_32_FLOAT:
		return MTL::VertexFormatUInt;
	case FMT_8_8_8_8:
		return MTL::VertexFormatUChar4;
	case FMT_8_8_8:
		return MTL::VertexFormatUChar3;
	case FMT_8_8:
		return MTL::VertexFormatUChar2;
	case FMT_8:
		return MTL::VertexFormatUChar;
	case FMT_32_32_32_32:
		return MTL::VertexFormatUInt4;
	case FMT_32_32_32:
		return MTL::VertexFormatUInt3;
	case FMT_32_32:
		return MTL::VertexFormatUInt2;
	case FMT_32:
		return MTL::VertexFormatUInt;
	case FMT_16_16_16_16:
		return MTL::VertexFormatUShort4; // verified to match OpenGL
	case FMT_16_16_16:
		return MTL::VertexFormatUShort3;
	case FMT_16_16:
		return MTL::VertexFormatUShort2;
	case FMT_16:
		return MTL::VertexFormatUShort;
	case FMT_16_16_16_16_FLOAT:
		return MTL::VertexFormatUShort4; // verified to match OpenGL
	case FMT_16_16_16_FLOAT:
		return MTL::VertexFormatUShort3;
	case FMT_16_16_FLOAT:
		return MTL::VertexFormatUShort2;
	case FMT_16_FLOAT:
		return MTL::VertexFormatUShort;
	case FMT_2_10_10_10:
		return MTL::VertexFormatUInt; // verified to match OpenGL
	default:
		printf("unsupported vertex format %u\n", (uint32)format);
		assert_dbg();
		return MTL::VertexFormatInvalid;
	}
}

MTL::IndexType GetMtlIndexType(Renderer::INDEX_TYPE indexType)
{
    switch (indexType)
    {
    case Renderer::INDEX_TYPE::U16:
        return MTL::IndexTypeUInt16;
    case Renderer::INDEX_TYPE::U32:
        return MTL::IndexTypeUInt32;
    default:
        printf("unsupported index type %u\n", (uint32)indexType);
        assert_dbg();
        return MTL::IndexTypeUInt32;
    }
}
