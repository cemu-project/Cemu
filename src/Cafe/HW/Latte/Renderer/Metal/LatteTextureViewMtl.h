#pragma once

#include <Metal/Metal.hpp>

#include "Cafe/HW/Latte/Core/LatteTexture.h"

class LatteTextureViewMtl : public LatteTextureView
{
public:
	LatteTextureViewMtl(class MetalRenderer* mtlRenderer, class LatteTextureMtl* texture, Latte::E_DIM dim, Latte::E_GX2SURFFMT format, sint32 firstMip, sint32 mipCount, sint32 firstSlice, sint32 sliceCount);
	~LatteTextureViewMtl();

	MTL::Texture* GetTexture() const {
	    return m_texture;
	}

private:
	class MetalRenderer* m_mtlr;

	MTL::Texture* m_texture;
};
