#include "Cafe/HW/Latte/Renderer/Metal/LatteToMtl.h"
#include "Cemu/Logging/CemuLogging.h"
#include "HW/Latte/Renderer/Metal/MetalCommon.h"
#include "Metal/MTLPixelFormat.hpp"

std::map<Latte::E_GX2SURFFMT, MetalPixelFormatInfo> MTL_COLOR_FORMAT_TABLE = {
    {Latte::E_GX2SURFFMT::INVALID_FORMAT, {MTL::PixelFormatInvalid, MetalDataType::NONE, 0}},

	{Latte::E_GX2SURFFMT::R4_G4_UNORM, {MTL::PixelFormatRG8Unorm, MetalDataType::FLOAT, 2}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::R5_G6_B5_UNORM, {MTL::PixelFormatB5G6R5Unorm, MetalDataType::FLOAT, 2}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::R5_G5_B5_A1_UNORM, {MTL::PixelFormatBGR5A1Unorm, MetalDataType::FLOAT, 2}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::R4_G4_B4_A4_UNORM, {MTL::PixelFormatABGR4Unorm, MetalDataType::FLOAT, 2}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::A1_B5_G5_R5_UNORM, {MTL::PixelFormatA1BGR5Unorm, MetalDataType::FLOAT, 2}},
	{Latte::E_GX2SURFFMT::R8_UNORM, {MTL::PixelFormatR8Unorm, MetalDataType::FLOAT, 1}},
	{Latte::E_GX2SURFFMT::R8_SNORM, {MTL::PixelFormatR8Snorm, MetalDataType::FLOAT, 1}},
	{Latte::E_GX2SURFFMT::R8_UINT, {MTL::PixelFormatR8Uint, MetalDataType::UINT, 1}},
	{Latte::E_GX2SURFFMT::R8_SINT, {MTL::PixelFormatR8Sint, MetalDataType::INT, 1}},
	{Latte::E_GX2SURFFMT::R8_G8_UNORM, {MTL::PixelFormatRG8Unorm, MetalDataType::FLOAT, 2}},
	{Latte::E_GX2SURFFMT::R8_G8_SNORM, {MTL::PixelFormatRG8Snorm, MetalDataType::FLOAT, 2}},
	{Latte::E_GX2SURFFMT::R8_G8_UINT, {MTL::PixelFormatRG8Uint, MetalDataType::UINT, 2}},
	{Latte::E_GX2SURFFMT::R8_G8_SINT, {MTL::PixelFormatRG8Sint, MetalDataType::INT, 2}},
	{Latte::E_GX2SURFFMT::R8_G8_B8_A8_UNORM, {MTL::PixelFormatRGBA8Unorm, MetalDataType::FLOAT, 4}},
	{Latte::E_GX2SURFFMT::R8_G8_B8_A8_SNORM, {MTL::PixelFormatRGBA8Snorm, MetalDataType::FLOAT, 4}},
	{Latte::E_GX2SURFFMT::R8_G8_B8_A8_UINT, {MTL::PixelFormatRGBA8Uint, MetalDataType::UINT, 4}},
	{Latte::E_GX2SURFFMT::R8_G8_B8_A8_SINT, {MTL::PixelFormatRGBA8Sint, MetalDataType::INT, 4}},
	{Latte::E_GX2SURFFMT::R8_G8_B8_A8_SRGB, {MTL::PixelFormatRGBA8Unorm_sRGB, MetalDataType::FLOAT, 4}},
	{Latte::E_GX2SURFFMT::R10_G10_B10_A2_UNORM, {MTL::PixelFormatRGB10A2Unorm, MetalDataType::FLOAT, 4}},
	{Latte::E_GX2SURFFMT::R10_G10_B10_A2_SNORM, {MTL::PixelFormatRGBA16Snorm, MetalDataType::FLOAT, 8}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::R10_G10_B10_A2_UINT, {MTL::PixelFormatRGB10A2Uint, MetalDataType::UINT, 4}},
	{Latte::E_GX2SURFFMT::R10_G10_B10_A2_SINT, {MTL::PixelFormatRGBA16Sint, MetalDataType::INT, 8}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::R10_G10_B10_A2_SRGB, {MTL::PixelFormatRGB10A2Unorm, MetalDataType::FLOAT, 4}}, // TODO: sRGB?
	{Latte::E_GX2SURFFMT::A2_B10_G10_R10_UNORM, {MTL::PixelFormatBGR10A2Unorm, MetalDataType::FLOAT, 4}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::A2_B10_G10_R10_UINT, {MTL::PixelFormatRGB10A2Uint, MetalDataType::UINT, 4}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::R16_UNORM, {MTL::PixelFormatR16Unorm, MetalDataType::FLOAT, 2}},
	{Latte::E_GX2SURFFMT::R16_SNORM, {MTL::PixelFormatR16Snorm, MetalDataType::FLOAT, 2}},
	{Latte::E_GX2SURFFMT::R16_UINT, {MTL::PixelFormatR16Uint, MetalDataType::UINT, 2}},
	{Latte::E_GX2SURFFMT::R16_SINT, {MTL::PixelFormatR16Sint, MetalDataType::INT, 2}},
	{Latte::E_GX2SURFFMT::R16_FLOAT, {MTL::PixelFormatR16Float, MetalDataType::FLOAT, 2}},
	{Latte::E_GX2SURFFMT::R16_G16_UNORM, {MTL::PixelFormatRG16Unorm, MetalDataType::FLOAT, 4}},
	{Latte::E_GX2SURFFMT::R16_G16_SNORM, {MTL::PixelFormatRG16Snorm, MetalDataType::FLOAT, 4}},
	{Latte::E_GX2SURFFMT::R16_G16_UINT, {MTL::PixelFormatRG16Uint, MetalDataType::UINT, 4}},
	{Latte::E_GX2SURFFMT::R16_G16_SINT, {MTL::PixelFormatRG16Sint, MetalDataType::INT, 4}},
	{Latte::E_GX2SURFFMT::R16_G16_FLOAT, {MTL::PixelFormatRG16Float, MetalDataType::FLOAT, 4}},
	{Latte::E_GX2SURFFMT::R16_G16_B16_A16_UNORM, {MTL::PixelFormatRGBA16Unorm, MetalDataType::FLOAT, 8}},
	{Latte::E_GX2SURFFMT::R16_G16_B16_A16_SNORM, {MTL::PixelFormatRGBA16Snorm, MetalDataType::FLOAT, 8}},
	{Latte::E_GX2SURFFMT::R16_G16_B16_A16_UINT, {MTL::PixelFormatRGBA16Uint, MetalDataType::UINT, 8}},
	{Latte::E_GX2SURFFMT::R16_G16_B16_A16_SINT, {MTL::PixelFormatRGBA16Sint, MetalDataType::INT, 8}},
	{Latte::E_GX2SURFFMT::R16_G16_B16_A16_FLOAT, {MTL::PixelFormatRGBA16Float, MetalDataType::FLOAT, 8}},
	{Latte::E_GX2SURFFMT::R24_X8_UNORM, {MTL::PixelFormatR32Float, MetalDataType::FLOAT, 4}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::R24_X8_FLOAT, {MTL::PixelFormatR32Float, MetalDataType::FLOAT, 4}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::X24_G8_UINT, {MTL::PixelFormatRGBA8Uint, MetalDataType::UINT, 4}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::R32_X8_FLOAT, {MTL::PixelFormatR32Float, MetalDataType::FLOAT, 4}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::X32_G8_UINT_X24, {MTL::PixelFormatRGBA16Uint, MetalDataType::UINT, 8}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::R11_G11_B10_FLOAT, {MTL::PixelFormatRG11B10Float, MetalDataType::FLOAT, 4}},
	{Latte::E_GX2SURFFMT::R32_UINT, {MTL::PixelFormatR32Uint, MetalDataType::UINT, 4}},
	{Latte::E_GX2SURFFMT::R32_SINT, {MTL::PixelFormatR32Sint, MetalDataType::INT, 4}},
	{Latte::E_GX2SURFFMT::R32_FLOAT, {MTL::PixelFormatR32Float, MetalDataType::FLOAT, 4}},
	{Latte::E_GX2SURFFMT::R32_G32_UINT, {MTL::PixelFormatRG32Uint, MetalDataType::UINT, 8}},
	{Latte::E_GX2SURFFMT::R32_G32_SINT, {MTL::PixelFormatRG32Sint, MetalDataType::INT, 8}},
	{Latte::E_GX2SURFFMT::R32_G32_FLOAT, {MTL::PixelFormatRG32Float, MetalDataType::FLOAT, 8}},
	{Latte::E_GX2SURFFMT::R32_G32_B32_A32_UINT, {MTL::PixelFormatRGBA32Uint, MetalDataType::UINT, 16}},
	{Latte::E_GX2SURFFMT::R32_G32_B32_A32_SINT, {MTL::PixelFormatRGBA32Sint, MetalDataType::INT, 16}},
	{Latte::E_GX2SURFFMT::R32_G32_B32_A32_FLOAT, {MTL::PixelFormatRGBA32Float, MetalDataType::FLOAT, 16}},
	{Latte::E_GX2SURFFMT::BC1_UNORM, {MTL::PixelFormatBC1_RGBA, MetalDataType::FLOAT, 8, {4, 4}}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::BC1_SRGB, {MTL::PixelFormatBC1_RGBA_sRGB, MetalDataType::FLOAT, 8, {4, 4}}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::BC2_UNORM, {MTL::PixelFormatBC2_RGBA, MetalDataType::FLOAT, 16, {4, 4}}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::BC2_SRGB, {MTL::PixelFormatBC2_RGBA_sRGB, MetalDataType::FLOAT, 16, {4, 4}}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::BC3_UNORM, {MTL::PixelFormatBC3_RGBA, MetalDataType::FLOAT, 16, {4, 4}}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::BC3_SRGB, {MTL::PixelFormatBC3_RGBA_sRGB, MetalDataType::FLOAT, 16, {4, 4}}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::BC4_UNORM, {MTL::PixelFormatBC4_RUnorm, MetalDataType::FLOAT, 8, {4, 4}}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::BC4_SNORM, {MTL::PixelFormatBC4_RSnorm, MetalDataType::FLOAT, 8, {4, 4}}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::BC5_UNORM, {MTL::PixelFormatBC5_RGUnorm, MetalDataType::FLOAT, 16, {4, 4}}}, // TODO: correct?
	{Latte::E_GX2SURFFMT::BC5_SNORM, {MTL::PixelFormatBC5_RGSnorm, MetalDataType::FLOAT, 16, {4, 4}}}, // TODO: correct?
};

std::map<Latte::E_GX2SURFFMT, MetalPixelFormatInfo> MTL_DEPTH_FORMAT_TABLE = {
    {Latte::E_GX2SURFFMT::INVALID_FORMAT, {MTL::PixelFormatInvalid, MetalDataType::NONE, 0}},

	{Latte::E_GX2SURFFMT::D24_S8_UNORM, {MTL::PixelFormatDepth24Unorm_Stencil8, MetalDataType::NONE, 4, {1, 1}, true}},
	{Latte::E_GX2SURFFMT::D24_S8_FLOAT, {MTL::PixelFormatDepth32Float_Stencil8, MetalDataType::NONE, 4, {1, 1}, true}},
	{Latte::E_GX2SURFFMT::D32_S8_FLOAT, {MTL::PixelFormatDepth32Float_Stencil8, MetalDataType::NONE, 5, {1, 1}, true}},
	{Latte::E_GX2SURFFMT::D16_UNORM, {MTL::PixelFormatDepth16Unorm, MetalDataType::NONE, 2, {1, 1}}},
	{Latte::E_GX2SURFFMT::D32_FLOAT, {MTL::PixelFormatDepth32Float, MetalDataType::NONE, 4, {1, 1}}},
};

void CheckForPixelFormatSupport(const MetalPixelFormatSupport& support)
{
    // Color formats
    for (auto& [fmt, formatInfo] : MTL_COLOR_FORMAT_TABLE)
    {
        switch (formatInfo.pixelFormat)
        {
        case MTL::PixelFormatR8Unorm_sRGB:
            if (!support.m_supportsR8Unorm_sRGB)
                formatInfo.pixelFormat = MTL::PixelFormatRGBA8Unorm_sRGB;
            break;
        case MTL::PixelFormatRG8Unorm_sRGB:
            if (!support.m_supportsRG8Unorm_sRGB)
                formatInfo.pixelFormat = MTL::PixelFormatRGBA8Unorm_sRGB;
            break;
        case MTL::PixelFormatB5G6R5Unorm:
        case MTL::PixelFormatA1BGR5Unorm:
        case MTL::PixelFormatABGR4Unorm:
        case MTL::PixelFormatBGR5A1Unorm:
            if (!support.m_supportsPacked16BitFormats)
                formatInfo.pixelFormat = MTL::PixelFormatRGBA8Unorm;
            break;
        default:
            break;
        }
    }

    // Depth formats
    for (auto& [fmt, formatInfo] : MTL_DEPTH_FORMAT_TABLE)
    {
        switch (formatInfo.pixelFormat)
        {
        case MTL::PixelFormatDepth24Unorm_Stencil8:
            if (!support.m_supportsDepth24Unorm_Stencil8)
                formatInfo.pixelFormat = MTL::PixelFormatDepth32Float_Stencil8;
            break;
        default:
            break;
        }
    }
}

const MetalPixelFormatInfo GetMtlPixelFormatInfo(Latte::E_GX2SURFFMT format, bool isDepth)
{
    if (isDepth)
    {
        auto it = MTL_DEPTH_FORMAT_TABLE.find(format);
        if (it == MTL_DEPTH_FORMAT_TABLE.end())
            return {MTL::PixelFormatDepth16Unorm, MetalDataType::NONE, 2}; // Fallback
        else
            return it->second;
    }
    else
    {
        auto it = MTL_COLOR_FORMAT_TABLE.find(format);
        if (it == MTL_COLOR_FORMAT_TABLE.end())
            return {MTL::PixelFormatR8Unorm, MetalDataType::FLOAT, 1}; // Fallback
        else
            return it->second;
    }
}

MTL::PixelFormat GetMtlPixelFormat(Latte::E_GX2SURFFMT format, bool isDepth)
{
    auto pixelFormat = GetMtlPixelFormatInfo(format, isDepth).pixelFormat;
    if (pixelFormat == MTL::PixelFormatInvalid)
        cemuLog_log(LogType::Force, "invalid pixel format 0x{:x}, is depth: {}\n", format, isDepth);

    return pixelFormat;
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

TextureDecoder* GetMtlTextureDecoder(Latte::E_GX2SURFFMT format, bool isDepth)
{
    if (isDepth)
    {
    	switch (format)
    	{
    	case Latte::E_GX2SURFFMT::D24_S8_UNORM:
    		return TextureDecoder_D24_S8::getInstance();
    	case Latte::E_GX2SURFFMT::D24_S8_FLOAT:
    		return TextureDecoder_NullData64::getInstance();
    	case Latte::E_GX2SURFFMT::D32_FLOAT:
    		return TextureDecoder_R32_FLOAT::getInstance();
    	case Latte::E_GX2SURFFMT::D16_UNORM:
    		return TextureDecoder_R16_UNORM::getInstance();
    	case Latte::E_GX2SURFFMT::D32_S8_FLOAT:
    		return TextureDecoder_D32_S8_UINT_X24::getInstance();
     	default:
    		debug_printf("invalid depth texture format %u\n", (uint32)format);
    		cemu_assert_debug(false);
    		return nullptr;
    	}
    } else
    {
        switch (format)
        {
    	case Latte::E_GX2SURFFMT::R32_G32_B32_A32_FLOAT:
    		return TextureDecoder_R32_G32_B32_A32_FLOAT::getInstance();
    	case Latte::E_GX2SURFFMT::R32_G32_B32_A32_UINT:
    		return TextureDecoder_R32_G32_B32_A32_UINT::getInstance();
    	case Latte::E_GX2SURFFMT::R16_G16_B16_A16_FLOAT:
    		return TextureDecoder_R16_G16_B16_A16_FLOAT::getInstance();
    	case Latte::E_GX2SURFFMT::R16_G16_B16_A16_UINT:
    		return TextureDecoder_R16_G16_B16_A16_UINT::getInstance();
    	case Latte::E_GX2SURFFMT::R16_G16_B16_A16_UNORM:
    		return TextureDecoder_R16_G16_B16_A16::getInstance();
    	case Latte::E_GX2SURFFMT::R16_G16_B16_A16_SNORM:
    		return TextureDecoder_R16_G16_B16_A16::getInstance();
    	case Latte::E_GX2SURFFMT::R8_G8_B8_A8_UNORM:
    		return TextureDecoder_R8_G8_B8_A8::getInstance();
    	case Latte::E_GX2SURFFMT::R8_G8_B8_A8_SNORM:
    		return TextureDecoder_R8_G8_B8_A8::getInstance();
    	case Latte::E_GX2SURFFMT::R8_G8_B8_A8_SRGB:
    		return TextureDecoder_R8_G8_B8_A8::getInstance();
    	case Latte::E_GX2SURFFMT::R8_G8_B8_A8_UINT:
    		return TextureDecoder_R8_G8_B8_A8::getInstance();
    	case Latte::E_GX2SURFFMT::R8_G8_B8_A8_SINT:
    		return TextureDecoder_R8_G8_B8_A8::getInstance();
    	case Latte::E_GX2SURFFMT::R32_G32_FLOAT:
    		return TextureDecoder_R32_G32_FLOAT::getInstance();
    	case Latte::E_GX2SURFFMT::R32_G32_UINT:
    		return TextureDecoder_R32_G32_UINT::getInstance();
    	case Latte::E_GX2SURFFMT::R16_G16_UNORM:
    		return TextureDecoder_R16_G16::getInstance();
    	case Latte::E_GX2SURFFMT::R16_G16_FLOAT:
    		return TextureDecoder_R16_G16_FLOAT::getInstance();
    	case Latte::E_GX2SURFFMT::R8_G8_UNORM:
    		return TextureDecoder_R8_G8::getInstance();
    	case Latte::E_GX2SURFFMT::R8_G8_SNORM:
    		return TextureDecoder_R8_G8::getInstance();
    	case Latte::E_GX2SURFFMT::R4_G4_UNORM:
    		return TextureDecoder_R4_G4::getInstance();
    	case Latte::E_GX2SURFFMT::R32_FLOAT:
    		return TextureDecoder_R32_FLOAT::getInstance();
    	case Latte::E_GX2SURFFMT::R32_UINT:
    		return TextureDecoder_R32_UINT::getInstance();
    	case Latte::E_GX2SURFFMT::R16_FLOAT:
    		return TextureDecoder_R16_FLOAT::getInstance();
    	case Latte::E_GX2SURFFMT::R16_UNORM:
    		return TextureDecoder_R16_UNORM::getInstance();
    	case Latte::E_GX2SURFFMT::R16_SNORM:
    		return TextureDecoder_R16_SNORM::getInstance();
    	case Latte::E_GX2SURFFMT::R16_UINT:
    		return TextureDecoder_R16_UINT::getInstance();
    	case Latte::E_GX2SURFFMT::R8_UNORM:
    		return TextureDecoder_R8::getInstance();
    	case Latte::E_GX2SURFFMT::R8_SNORM:
    		return TextureDecoder_R8::getInstance();
    	case Latte::E_GX2SURFFMT::R8_UINT:
    		return TextureDecoder_R8_UINT::getInstance();
    	case Latte::E_GX2SURFFMT::R5_G6_B5_UNORM:
    		return TextureDecoder_R5_G6_B5_swappedRB::getInstance();
    	case Latte::E_GX2SURFFMT::R5_G5_B5_A1_UNORM:
    		return TextureDecoder_R5_G5_B5_A1_UNORM_swappedRB::getInstance();
    	case Latte::E_GX2SURFFMT::A1_B5_G5_R5_UNORM:
    		return TextureDecoder_A1_B5_G5_R5_UNORM_vulkan::getInstance();
    	case Latte::E_GX2SURFFMT::R11_G11_B10_FLOAT:
    		return TextureDecoder_R11_G11_B10_FLOAT::getInstance();
    	case Latte::E_GX2SURFFMT::R4_G4_B4_A4_UNORM:
    		return TextureDecoder_R4_G4_B4_A4_UNORM::getInstance();
    	case Latte::E_GX2SURFFMT::R10_G10_B10_A2_UNORM:
    		return TextureDecoder_R10_G10_B10_A2_UNORM::getInstance();
    	case Latte::E_GX2SURFFMT::R10_G10_B10_A2_SNORM:
    		return TextureDecoder_R10_G10_B10_A2_SNORM_To_RGBA16::getInstance();
    	case Latte::E_GX2SURFFMT::R10_G10_B10_A2_SRGB:
    		return TextureDecoder_R10_G10_B10_A2_UNORM::getInstance();
    	case Latte::E_GX2SURFFMT::BC1_SRGB:
    		return TextureDecoder_BC1::getInstance();
    	case Latte::E_GX2SURFFMT::BC1_UNORM:
    		return TextureDecoder_BC1::getInstance();
    	case Latte::E_GX2SURFFMT::BC2_UNORM:
    		return TextureDecoder_BC2::getInstance();
    	case Latte::E_GX2SURFFMT::BC2_SRGB:
    		return TextureDecoder_BC2::getInstance();
    	case Latte::E_GX2SURFFMT::BC3_UNORM:
    		return TextureDecoder_BC3::getInstance();
    	case Latte::E_GX2SURFFMT::BC3_SRGB:
    		return TextureDecoder_BC3::getInstance();
    	case Latte::E_GX2SURFFMT::BC4_UNORM:
    		return TextureDecoder_BC4::getInstance();
    	case Latte::E_GX2SURFFMT::BC4_SNORM:
    		return TextureDecoder_BC4::getInstance();
    	case Latte::E_GX2SURFFMT::BC5_UNORM:
    		return TextureDecoder_BC5::getInstance();
    	case Latte::E_GX2SURFFMT::BC5_SNORM:
    		return TextureDecoder_BC5::getInstance();
    	case Latte::E_GX2SURFFMT::R24_X8_UNORM:
    		return TextureDecoder_R24_X8::getInstance();
    	case Latte::E_GX2SURFFMT::X24_G8_UINT:
    		return TextureDecoder_X24_G8_UINT::getInstance(); // todo - verify
    	default:
    		debug_printf("invalid color texture format %u\n", (uint32)format);
    		cemu_assert_debug(false);
    		return nullptr;
    	}
    }
}

MTL::PrimitiveType GetMtlPrimitiveType(LattePrimitiveMode primitiveMode)
{
    switch (primitiveMode)
    {
	case Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::POINTS:
		return MTL::PrimitiveTypePoint;
	case Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::LINES:
		return MTL::PrimitiveTypeLine;
	case Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::LINE_STRIP:
		return MTL::PrimitiveTypeLineStrip;
	case Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::LINE_LOOP:
		return MTL::PrimitiveTypeLineStrip; // line loops are emulated as line strips with an extra connecting strip at the end
	case Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::LINE_STRIP_ADJACENT: // Tropical Freeze level 3-6
	    debug_printf("Metal doesn't support line strip adjacent primitive, using line strip instead\n");
		return MTL::PrimitiveTypeLineStrip;
	case Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::TRIANGLES:
		return MTL::PrimitiveTypeTriangle;
	case Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::TRIANGLE_FAN:
		return MTL::PrimitiveTypeTriangleStrip;
	case Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::TRIANGLE_STRIP:
		return MTL::PrimitiveTypeTriangleStrip;
	case Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::QUADS:
		return MTL::PrimitiveTypeTriangle; // quads are emulated as 2 triangles
	case Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::QUAD_STRIP:
		return MTL::PrimitiveTypeTriangle; // quad strips are emulated as (count-2)/2 triangles
	case Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::RECTS:
		return MTL::PrimitiveTypeTriangle; // rects are emulated as 2 triangles
	default:
		cemuLog_logDebug(LogType::Force, "Metal-Unsupported: Render pipeline with primitive mode {} created", primitiveMode);
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

uint32 GetMtlVertexFormatSize(uint8 format)
{
    switch (format)
	{
	case FMT_32_32_32_32_FLOAT:
		return 16;
	case FMT_32_32_32_FLOAT:
		return 12;
	case FMT_32_32_FLOAT:
		return 8;
	case FMT_32_FLOAT:
		return 4;
	case FMT_8_8_8_8:
		return 4;
	case FMT_8_8_8:
		return 3;
	case FMT_8_8:
		return 2;
	case FMT_8:
		return 1;
	case FMT_32_32_32_32:
		return 16;
	case FMT_32_32_32:
		return 12;
	case FMT_32_32:
		return 8;
	case FMT_32:
		return 4;
	case FMT_16_16_16_16:
		return 8;
	case FMT_16_16_16:
		return 6;
	case FMT_16_16:
		return 4;
	case FMT_16:
		return 2;
	case FMT_16_16_16_16_FLOAT:
		return 8;
	case FMT_16_16_16_FLOAT:
		return 6;
	case FMT_16_16_FLOAT:
		return 4;
	case FMT_16_FLOAT:
		return 2;
	case FMT_2_10_10_10:
		return 4;
	default:
		return 0;
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
        cemu_assert_suspicious();
        return MTL::IndexTypeUInt32;
    }
}

MTL::BlendOperation GetMtlBlendOp(Latte::LATTE_CB_BLENDN_CONTROL::E_COMBINEFUNC combineFunc)
{
    switch (combineFunc)
	{
	case Latte::LATTE_CB_BLENDN_CONTROL::E_COMBINEFUNC::DST_PLUS_SRC:
		return MTL::BlendOperationAdd;
	case Latte::LATTE_CB_BLENDN_CONTROL::E_COMBINEFUNC::SRC_MINUS_DST:
		return MTL::BlendOperationSubtract;
	case Latte::LATTE_CB_BLENDN_CONTROL::E_COMBINEFUNC::MIN_DST_SRC:
		return MTL::BlendOperationMin;
	case Latte::LATTE_CB_BLENDN_CONTROL::E_COMBINEFUNC::MAX_DST_SRC:
		return MTL::BlendOperationMax;
	case Latte::LATTE_CB_BLENDN_CONTROL::E_COMBINEFUNC::DST_MINUS_SRC:
		return MTL::BlendOperationReverseSubtract;
	default:
		cemu_assert_suspicious();
		return MTL::BlendOperationAdd;
	}
}

const MTL::BlendFactor MTL_BLEND_FACTORS[] =
{
    /* 0x00 */ MTL::BlendFactorZero,
    /* 0x01 */ MTL::BlendFactorOne,
    /* 0x02 */ MTL::BlendFactorSourceColor,
    /* 0x03 */ MTL::BlendFactorOneMinusSourceColor,
    /* 0x04 */ MTL::BlendFactorSourceAlpha,
    /* 0x05 */ MTL::BlendFactorOneMinusSourceAlpha,
    /* 0x06 */ MTL::BlendFactorDestinationAlpha,
    /* 0x07 */ MTL::BlendFactorOneMinusDestinationAlpha,
    /* 0x08 */ MTL::BlendFactorDestinationColor,
    /* 0x09 */ MTL::BlendFactorOneMinusDestinationColor,
    /* 0x0A */ MTL::BlendFactorSourceAlphaSaturated,
    /* 0x0B */ MTL::BlendFactorZero, // TODO
    /* 0x0C */ MTL::BlendFactorZero, // TODO
    /* 0x0D */ MTL::BlendFactorBlendColor,
    /* 0x0E */ MTL::BlendFactorOneMinusBlendColor,
    /* 0x0F */ MTL::BlendFactorSource1Color,
    /* 0x10 */ MTL::BlendFactorOneMinusSource1Color,
    /* 0x11 */ MTL::BlendFactorSource1Alpha,
    /* 0x12 */ MTL::BlendFactorOneMinusSource1Alpha,
    /* 0x13 */ MTL::BlendFactorBlendAlpha,
    /* 0x14 */ MTL::BlendFactorOneMinusBlendAlpha
};

MTL::BlendFactor GetMtlBlendFactor(Latte::LATTE_CB_BLENDN_CONTROL::E_BLENDFACTOR factor)
{
	cemu_assert_debug((uint32)factor < std::size(MTL_BLEND_FACTORS));
	return MTL_BLEND_FACTORS[(uint32)factor];
}

const MTL::CompareFunction MTL_COMPARE_FUNCTIONS[8] =
{
	MTL::CompareFunctionNever,
	MTL::CompareFunctionLess,
	MTL::CompareFunctionEqual,
	MTL::CompareFunctionLessEqual,
	MTL::CompareFunctionGreater,
	MTL::CompareFunctionNotEqual,
	MTL::CompareFunctionGreaterEqual,
	MTL::CompareFunctionAlways
};

MTL::CompareFunction GetMtlCompareFunc(Latte::E_COMPAREFUNC func)
{
    cemu_assert_debug((uint32)func < std::size(MTL_COMPARE_FUNCTIONS));
    return MTL_COMPARE_FUNCTIONS[(uint32)func];
}

// TODO: clamp to border color? (should be fine though)
const MTL::SamplerAddressMode MTL_SAMPLER_ADDRESS_MODES[] = {
	MTL::SamplerAddressModeRepeat, // WRAP
	MTL::SamplerAddressModeMirrorRepeat, // MIRROR
	MTL::SamplerAddressModeClampToEdge, // CLAMP_LAST_TEXEL
	MTL::SamplerAddressModeMirrorClampToEdge, // MIRROR_ONCE_LAST_TEXEL
	MTL::SamplerAddressModeClampToEdge, // unsupported HALF_BORDER
	MTL::SamplerAddressModeClampToBorderColor, // unsupported MIRROR_ONCE_HALF_BORDER
	MTL::SamplerAddressModeClampToBorderColor, // CLAMP_BORDER
	MTL::SamplerAddressModeClampToBorderColor // MIRROR_ONCE_BORDER
};

MTL::SamplerAddressMode GetMtlSamplerAddressMode(Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_CLAMP clamp)
{
    cemu_assert_debug((uint32)clamp < std::size(MTL_SAMPLER_ADDRESS_MODES));
    return MTL_SAMPLER_ADDRESS_MODES[(uint32)clamp];
}

const MTL::TextureSwizzle MTL_TEXTURE_SWIZZLES[] = {
    MTL::TextureSwizzleRed,
    MTL::TextureSwizzleGreen,
    MTL::TextureSwizzleBlue,
    MTL::TextureSwizzleAlpha,
    MTL::TextureSwizzleZero,
    MTL::TextureSwizzleOne,
    MTL::TextureSwizzleZero,
    MTL::TextureSwizzleZero
};

MTL::TextureSwizzle GetMtlTextureSwizzle(uint32 swizzle)
{
    cemu_assert_debug(swizzle < std::size(MTL_TEXTURE_SWIZZLES));
    return MTL_TEXTURE_SWIZZLES[swizzle];
}

const MTL::StencilOperation MTL_STENCIL_OPERATIONS[8] = {
	MTL::StencilOperationKeep,
	MTL::StencilOperationZero,
	MTL::StencilOperationReplace,
	MTL::StencilOperationIncrementClamp,
	MTL::StencilOperationDecrementClamp,
	MTL::StencilOperationInvert,
	MTL::StencilOperationIncrementWrap,
	MTL::StencilOperationDecrementWrap
};

MTL::StencilOperation GetMtlStencilOp(Latte::LATTE_DB_DEPTH_CONTROL::E_STENCILACTION action)
{
    cemu_assert_debug((uint32)action < std::size(MTL_STENCIL_OPERATIONS));
    return MTL_STENCIL_OPERATIONS[(uint32)action];
}

MTL::ColorWriteMask GetMtlColorWriteMask(uint8 mask)
{
    MTL::ColorWriteMask mtlMask = MTL::ColorWriteMaskNone;
    if (mask & 0x1) mtlMask |= MTL::ColorWriteMaskRed;
    if (mask & 0x2) mtlMask |= MTL::ColorWriteMaskGreen;
    if (mask & 0x4) mtlMask |= MTL::ColorWriteMaskBlue;
    if (mask & 0x8) mtlMask |= MTL::ColorWriteMaskAlpha;

    return mtlMask;
}
