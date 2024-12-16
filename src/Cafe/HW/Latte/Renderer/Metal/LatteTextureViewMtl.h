#pragma once

#include <Metal/Metal.hpp>
#include <unordered_map>

#include "Cafe/HW/Latte/Core/LatteTexture.h"

#define RGBA_SWIZZLE 0x06880000
#define INVALID_SWIZZLE 0xFFFFFFFF

class LatteTextureViewMtl : public LatteTextureView
{
public:
	LatteTextureViewMtl(class MetalRenderer* mtlRenderer, class LatteTextureMtl* texture, Latte::E_DIM dim, Latte::E_GX2SURFFMT format, sint32 firstMip, sint32 mipCount, sint32 firstSlice, sint32 sliceCount);
	~LatteTextureViewMtl();

    MTL::Texture* GetSwizzledView(uint32 gpuSamplerSwizzle);

    MTL::Texture* GetRGBAView()
    {
        return GetSwizzledView(RGBA_SWIZZLE);
    }

private:
	class MetalRenderer* m_mtlr;

	class LatteTextureMtl* m_baseTexture;

	MTL::Texture* m_rgbaView;
	struct {
	    uint32 key;
	    MTL::Texture* texture;
	} m_viewCache[4] = {{INVALID_SWIZZLE, nullptr}, {INVALID_SWIZZLE, nullptr}, {INVALID_SWIZZLE, nullptr}, {INVALID_SWIZZLE, nullptr}};
	std::unordered_map<uint32, MTL::Texture*> m_fallbackViewCache;

    MTL::Texture* CreateSwizzledView(uint32 gpuSamplerSwizzle);
};
