#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureViewMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"

LatteTextureViewMtl::LatteTextureViewMtl(MetalRenderer* mtlRenderer, LatteTextureMtl* texture, Latte::E_DIM dim, Latte::E_GX2SURFFMT format, sint32 firstMip, sint32 mipCount, sint32 firstSlice, sint32 sliceCount)
	: LatteTextureView(texture, firstMip, mipCount, firstSlice, sliceCount, dim, format), m_mtlr(mtlRenderer)
{
    // TODO: don't hardcode the format
    MTL::PixelFormat pixelFormat = MTL::PixelFormatRGBA8Unorm;
    MTL::TextureType textureType;
    switch (dim)
	{
	case Latte::E_DIM::DIM_1D:
		textureType = MTL::TextureType1D;
	case Latte::E_DIM::DIM_2D:
	case Latte::E_DIM::DIM_2D_MSAA:
		textureType = MTL::TextureType2D;
	case Latte::E_DIM::DIM_2D_ARRAY:
		textureType = MTL::TextureType2DArray;
	case Latte::E_DIM::DIM_3D:
		textureType = MTL::TextureType3D;
	case Latte::E_DIM::DIM_CUBEMAP:
		textureType = MTL::TextureTypeCube; // TODO: check this
	default:
		cemu_assert_unimplemented();
		textureType = MTL::TextureType2D;
	}

	uint32 baseLevel = firstMip;
	uint32 levelCount = this->numMip;
	uint32 baseLayer;
	uint32 layerCount;
	// TODO: check if base texture is 3D texture as well
	if (textureType == MTL::TextureType3D)
	{
		cemu_assert_debug(firstMip == 0);
		// TODO: uncomment
		//cemu_assert_debug(this->numSlice == baseTexture->depth);
		baseLayer = 0;
		layerCount = 1;
	}
	else
	{
		baseLayer = firstSlice;
		layerCount = this->numSlice;
	}

	// TODO: swizzle

	m_texture = texture->GetTexture()->newTextureView(pixelFormat, textureType, NS::Range::Make(baseLevel, levelCount), NS::Range::Make(baseLayer, layerCount));
}

LatteTextureViewMtl::~LatteTextureViewMtl()
{
	m_texture->release();
}
