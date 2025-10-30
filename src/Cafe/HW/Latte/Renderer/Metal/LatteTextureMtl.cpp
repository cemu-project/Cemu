#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureViewMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteToMtl.h"

LatteTextureMtl::LatteTextureMtl(class MetalRenderer* mtlRenderer, Latte::E_DIM dim, MPTR physAddress, MPTR physMipAddress, Latte::E_GX2SURFFMT format, uint32 width, uint32 height, uint32 depth, uint32 pitch, uint32 mipLevels, uint32 swizzle,
	Latte::E_HWTILEMODE tileMode, bool isDepth)
	: LatteTexture(dim, physAddress, physMipAddress, format, width, height, depth, pitch, mipLevels, swizzle, tileMode, isDepth), m_mtlr(mtlRenderer)
{
    NS_STACK_SCOPED MTL::TextureDescriptor* desc = MTL::TextureDescriptor::alloc()->init();
    desc->setStorageMode(MTL::StorageModePrivate);
    //desc->setCpuCacheMode(MTL::CPUCacheModeWriteCombined);

	sint32 effectiveBaseWidth = width;
	sint32 effectiveBaseHeight = height;
	sint32 effectiveBaseDepth = depth;
	if (overwriteInfo.hasResolutionOverwrite)
	{
		effectiveBaseWidth = overwriteInfo.width;
		effectiveBaseHeight = overwriteInfo.height;
		effectiveBaseDepth = overwriteInfo.depth;
	}
	effectiveBaseWidth = std::max(1, effectiveBaseWidth);
	effectiveBaseHeight = std::max(1, effectiveBaseHeight);
	effectiveBaseDepth = std::max(1, effectiveBaseDepth);

	MTL::TextureType textureType;
	switch (dim)
    {
    case Latte::E_DIM::DIM_1D:
        textureType = MTL::TextureType1D;
        effectiveBaseHeight = 1;
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
        cemu_assert_debug(effectiveBaseDepth % 6 == 0 && "cubemaps must have an array length multiple of 6");

        textureType = MTL::TextureTypeCubeArray;
        break;
    default:
        cemu_assert_unimplemented();
        textureType = MTL::TextureType2D;
        break;
    }
    desc->setTextureType(textureType);

    // Clamp mip levels
    mipLevels = std::min(mipLevels, (uint32)maxPossibleMipLevels);
    mipLevels = std::max(mipLevels, (uint32)1);

	desc->setWidth(effectiveBaseWidth);
	desc->setHeight(effectiveBaseHeight);
	desc->setMipmapLevelCount(mipLevels);

	if (textureType == MTL::TextureType3D)
	{
		desc->setDepth(effectiveBaseDepth);
	}
	else if (textureType == MTL::TextureTypeCubeArray)
	{
		desc->setArrayLength(effectiveBaseDepth / 6);
	}
	else if (textureType == MTL::TextureType2DArray)
	{
		desc->setArrayLength(effectiveBaseDepth);
	}

	auto pixelFormat = GetMtlPixelFormat(format, isDepth);
	desc->setPixelFormat(pixelFormat);

	MTL::TextureUsage usage = MTL::TextureUsageShaderRead | MTL::TextureUsagePixelFormatView;
	if (FormatIsRenderable(format))
		usage |= MTL::TextureUsageRenderTarget;
	desc->setUsage(usage);

	m_texture = mtlRenderer->GetDevice()->newTexture(desc);
}

LatteTextureMtl::~LatteTextureMtl()
{
	m_texture->release();
}

LatteTextureView* LatteTextureMtl::CreateView(Latte::E_DIM dim, Latte::E_GX2SURFFMT format, sint32 firstMip, sint32 mipCount, sint32 firstSlice, sint32 sliceCount)
{
	cemu_assert_debug(mipCount > 0);
	cemu_assert_debug(sliceCount > 0);
	cemu_assert_debug((firstMip + mipCount) <= this->mipLevels);
	cemu_assert_debug((firstSlice + sliceCount) <= this->depth);

	return new LatteTextureViewMtl(m_mtlr, this, dim, format, firstMip, mipCount, firstSlice, sliceCount);
}

// TODO: lazy allocation?
void LatteTextureMtl::AllocateOnHost()
{
	// The texture is already allocated
}
