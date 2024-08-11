#pragma once

#include <Metal/Metal.hpp>

#include "Cafe/HW/Latte/ISA/LatteReg.h"
#include "Cafe/HW/Latte/Core/LatteConst.h"
//#include "Cafe/HW/Latte/Core/FetchShader.h"
#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "Metal/MTLDepthStencil.hpp"
#include "Metal/MTLRenderPipeline.hpp"
#include "Metal/MTLSampler.hpp"
#include "Metal/MTLTexture.hpp"

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
};

const MetalPixelFormatInfo GetMtlPixelFormatInfo(Latte::E_GX2SURFFMT format, bool isDepth);

size_t GetMtlTextureBytesPerRow(Latte::E_GX2SURFFMT format, bool isDepth, uint32 width);

size_t GetMtlTextureBytesPerImage(Latte::E_GX2SURFFMT format, bool isDepth, uint32 height, size_t bytesPerRow);

TextureDecoder* GetMtlTextureDecoder(Latte::E_GX2SURFFMT format, bool isDepth);

MTL::PrimitiveType GetMtlPrimitiveType(LattePrimitiveMode primitiveMode);

MTL::VertexFormat GetMtlVertexFormat(uint8 format);

MTL::IndexType GetMtlIndexType(Renderer::INDEX_TYPE indexType);

MTL::BlendOperation GetMtlBlendOp(Latte::LATTE_CB_BLENDN_CONTROL::E_COMBINEFUNC combineFunc);

MTL::BlendFactor GetMtlBlendFactor(Latte::LATTE_CB_BLENDN_CONTROL::E_BLENDFACTOR factor);

MTL::CompareFunction GetMtlCompareFunc(Latte::E_COMPAREFUNC func);

MTL::SamplerAddressMode GetMtlSamplerAddressMode(Latte::LATTE_SQ_TEX_SAMPLER_WORD0_0::E_CLAMP clamp);

MTL::TextureSwizzle GetMtlTextureSwizzle(uint32 swizzle);

MTL::StencilOperation GetMtlStencilOp(Latte::LATTE_DB_DEPTH_CONTROL::E_STENCILACTION action);

MTL::ColorWriteMask GetMtlColorWriteMask(uint8 mask);
