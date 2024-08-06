#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureViewMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteToMtl.h"

LatteTextureViewMtl::LatteTextureViewMtl(MetalRenderer* mtlRenderer, LatteTextureMtl* texture, Latte::E_DIM dim, Latte::E_GX2SURFFMT format, sint32 firstMip, sint32 mipCount, sint32 firstSlice, sint32 sliceCount)
	: LatteTextureView(texture, firstMip, mipCount, firstSlice, sliceCount, dim, format), m_mtlr(mtlRenderer), m_baseTexture(texture)
{
}

LatteTextureViewMtl::~LatteTextureViewMtl()
{
	for (sint32 i = 0; i < std::size(m_viewCache); i++)
    {
        if (m_viewCache[i].key != INVALID_SWIZZLE)
            m_viewCache[i].texture->release();
    }

    for (auto& [key, texture] : m_fallbackViewCache)
    {
        texture->release();
    }
}

MTL::Texture* LatteTextureViewMtl::GetSwizzledView(uint32 gpuSamplerSwizzle)
{
    // Mask out
    gpuSamplerSwizzle &= 0x0FFF0000;

    // RGBA swizzle == no swizzle
    if (gpuSamplerSwizzle == RGBA_SWIZZLE)
    {
        return m_baseTexture->GetTexture();
    }

    // First, try to find a view in the cache

    // Fast cache
    sint32 freeIndex = -1;
    for (sint32 i = 0; i < std::size(m_viewCache); i++)
    {
        if (m_viewCache[i].key == gpuSamplerSwizzle)
        {
            return m_viewCache[i].texture;
        }
        else if (m_viewCache[i].key == INVALID_SWIZZLE && freeIndex == -1)
        {
            freeIndex = i;
        }
    }

    // Fallback cache
    auto it = m_fallbackViewCache.find(gpuSamplerSwizzle);
    if (it != m_fallbackViewCache.end())
    {
        return it->second;
    }

    MTL::Texture* texture = CreateSwizzledView(gpuSamplerSwizzle);
    if (freeIndex != -1)
        m_viewCache[freeIndex] = {gpuSamplerSwizzle, texture};
    else
        it->second = texture;

    return texture;
}

MTL::Texture* LatteTextureViewMtl::CreateSwizzledView(uint32 gpuSamplerSwizzle)
{
    uint32 compSelR = (gpuSamplerSwizzle >> 16) & 0x7;
	uint32 compSelG = (gpuSamplerSwizzle >> 19) & 0x7;
	uint32 compSelB = (gpuSamplerSwizzle >> 22) & 0x7;
	uint32 compSelA = (gpuSamplerSwizzle >> 25) & 0x7;
	// TODO: adjust

	MTL::TextureType textureType;
    switch (dim)
    {
    case Latte::E_DIM::DIM_1D:
        textureType = MTL::TextureType1D;
        break;
    case Latte::E_DIM::DIM_2D:
    case Latte::E_DIM::DIM_2D_MSAA:
        textureType = MTL::TextureType2D;
        break;
    case Latte::E_DIM::DIM_2D_ARRAY:
        textureType = MTL::TextureType2DArray;
        break;
    case Latte::E_DIM::DIM_3D:
        textureType = MTL::TextureType3D;
        break;
    case Latte::E_DIM::DIM_CUBEMAP:
        textureType = MTL::TextureTypeCube; // TODO: check this
        break;
    default:
        cemu_assert_unimplemented();
        textureType = MTL::TextureType2D;
        break;
    }

    uint32 baseLevel = firstMip;
    uint32 levelCount = this->numMip;
    uint32 baseLayer;
    uint32 layerCount;
    // TODO: check if base texture is 3D texture as well
    if (textureType == MTL::TextureType3D)
    {
        cemu_assert_debug(firstMip == 0);
        cemu_assert_debug(this->numSlice == baseTexture->depth);
        baseLayer = 0;
        layerCount = 1;
    }
    else
    {
        baseLayer = firstSlice;
        layerCount = this->numSlice;
    }

    MTL::TextureSwizzleChannels swizzle;
    swizzle.red = GetMtlTextureSwizzle(compSelR);
    swizzle.green = GetMtlTextureSwizzle(compSelG);
    swizzle.blue = GetMtlTextureSwizzle(compSelB);
    swizzle.alpha = GetMtlTextureSwizzle(compSelA);

    auto formatInfo = GetMtlPixelFormatInfo(format, m_baseTexture->IsDepth());
    MTL::Texture* texture = m_baseTexture->GetTexture()->newTextureView(formatInfo.pixelFormat, textureType, NS::Range::Make(baseLevel, levelCount), NS::Range::Make(baseLayer, layerCount), swizzle);

    return texture;
}
