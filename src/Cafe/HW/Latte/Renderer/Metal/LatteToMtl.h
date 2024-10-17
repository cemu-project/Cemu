#pragma once

#include "Cafe/HW/Latte/Renderer/Metal/MetalCommon.h"

#include "Cafe/HW/Latte/ISA/LatteReg.h"
#include "Cafe/HW/Latte/Core/LatteConst.h"
//#include "Cafe/HW/Latte/Core/FetchShader.h"
#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "Common/precompiled.h"
#include "HW/Latte/Core/LatteTextureLoader.h"

struct Uvec2 {
    uint32 x;
    uint32 y;
};

enum class MetalDataType
{
    NONE,
    INT,
    UINT,
    FLOAT,
};

struct MetalPixelFormatInfo {
    MTL::PixelFormat pixelFormat;
    MetalDataType dataType;
    size_t bytesPerBlock;
    Uvec2 blockTexelSize = {1, 1};
    bool hasStencil = false;
    TextureDecoder* textureDecoder = nullptr;
};

void CheckForPixelFormatSupport(const MetalPixelFormatSupport& support);

const MetalPixelFormatInfo GetMtlPixelFormatInfo(Latte::E_GX2SURFFMT format, bool isDepth);

MTL::PixelFormat GetMtlPixelFormat(Latte::E_GX2SURFFMT format, bool isDepth);

inline MetalDataType GetColorBufferDataType(const uint32 index, const LatteContextRegister& lcr)
{
    auto format = LatteMRT::GetColorBufferFormat(index, lcr);
    return GetMtlPixelFormatInfo(format, false).dataType;
}

inline const char* GetDataTypeStr(MetalDataType dataType)
{
	switch (dataType)
	{
	case MetalDataType::INT:
	    return "int4";
	case MetalDataType::UINT:
	    return "uint4";
	case MetalDataType::FLOAT:
	    return "float4";
	default:
	    cemu_assert_suspicious();
		return "";
	}
}

size_t GetMtlTextureBytesPerRow(Latte::E_GX2SURFFMT format, bool isDepth, uint32 width);

size_t GetMtlTextureBytesPerImage(Latte::E_GX2SURFFMT format, bool isDepth, uint32 height, size_t bytesPerRow);

MTL::PrimitiveType GetMtlPrimitiveType(LattePrimitiveMode primitiveMode);

MTL::VertexFormat GetMtlVertexFormat(uint8 format);

uint32 GetMtlVertexFormatSize(uint8 format);

MTL::IndexType GetMtlIndexType(Renderer::INDEX_TYPE indexType);

MTL::BlendOperation GetMtlBlendOp(Latte::LATTE_CB_BLENDN_CONTROL::E_COMBINEFUNC combineFunc);

MTL::BlendFactor GetMtlBlendFactor(Latte::LATTE_CB_BLENDN_CONTROL::E_BLENDFACTOR factor);

MTL::CompareFunction GetMtlCompareFunc(Latte::E_COMPAREFUNC func);

MTL::SamplerAddressMode GetMtlSamplerAddressMode(Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_CLAMP clamp);

MTL::TextureSwizzle GetMtlTextureSwizzle(uint32 swizzle);

MTL::StencilOperation GetMtlStencilOp(Latte::LATTE_DB_DEPTH_CONTROL::E_STENCILACTION action);

MTL::ColorWriteMask GetMtlColorWriteMask(uint8 mask);
