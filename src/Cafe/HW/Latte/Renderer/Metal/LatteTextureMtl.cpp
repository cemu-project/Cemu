#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureViewMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"

LatteTextureMtl::LatteTextureMtl(class MetalRenderer* mtlRenderer, Latte::E_DIM dim, MPTR physAddress, MPTR physMipAddress, Latte::E_GX2SURFFMT format, uint32 width, uint32 height, uint32 depth, uint32 pitch, uint32 mipLevels, uint32 swizzle,
	Latte::E_HWTILEMODE tileMode, bool isDepth)
	: LatteTexture(dim, physAddress, physMipAddress, format, width, height, depth, pitch, mipLevels, swizzle, tileMode, isDepth), m_mtlr(mtlRenderer)
{
    MTL::TextureDescriptor* desc = MTL::TextureDescriptor::alloc()->init();
	sint32 effectiveBaseWidth = width;
	sint32 effectiveBaseHeight = height;
	sint32 effectiveBaseDepth = depth;
	if (overwriteInfo.hasResolutionOverwrite)
	{
		effectiveBaseWidth = overwriteInfo.width;
		effectiveBaseHeight = overwriteInfo.height;
		effectiveBaseDepth = overwriteInfo.depth;
	}
	effectiveBaseDepth = std::max(1, effectiveBaseDepth);

	desc->setWidth(effectiveBaseWidth);
	desc->setHeight(effectiveBaseHeight);
	desc->setMipmapLevelCount(mipLevels);

	if (dim == Latte::E_DIM::DIM_3D)
	{
		desc->setDepth(effectiveBaseDepth);
	}
	else
	{
		desc->setArrayLength(effectiveBaseDepth);
	}

	// TODO: uncomment
	//MetalRenderer::FormatInfoMTL texFormatInfo;
	//mtlRenderer->GetTextureFormatInfoMTL(format, isDepth, dim, effectiveBaseWidth, effectiveBaseHeight, &texFormatInfo);
	//cemu_assert_debug(hasStencil == ((texFormatInfo.vkImageAspect & VK_IMAGE_ASPECT_STENCIL_BIT) != 0));
	//imageInfo.format = texFormatInfo.mtlPixelFormat;
	desc->setPixelFormat(MTL::PixelFormatRGBA8Unorm);

	// TODO: is write needed?
	MTL::TextureUsage usage = MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite;
	// TODO: add more conditions
	if (Latte::IsCompressedFormat(format) == false)
	{
		usage |= MTL::TextureUsageRenderTarget;
	}
	desc->setUsage(usage);

	if (dim == Latte::E_DIM::DIM_2D)
		desc->setTextureType(MTL::TextureType2D);
	else if (dim == Latte::E_DIM::DIM_1D)
	    desc->setTextureType(MTL::TextureType1D);
	else if (dim == Latte::E_DIM::DIM_3D)
	    desc->setTextureType(MTL::TextureType3D);
	else if (dim == Latte::E_DIM::DIM_2D_ARRAY)
        desc->setTextureType(MTL::TextureType2DArray);
	else if (dim == Latte::E_DIM::DIM_CUBEMAP)
	    desc->setTextureType(MTL::TextureTypeCube); // TODO: is this correct?
	else if (dim == Latte::E_DIM::DIM_2D_MSAA)
	    desc->setTextureType(MTL::TextureType2D);
	else
	{
		cemu_assert_unimplemented();
	}

	m_texture = mtlRenderer->GetDevice()->newTexture(desc);
	desc->release();
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

void LatteTextureMtl::AllocateOnHost()
{
	cemuLog_logDebug(LogType::Force, "not implemented");
}
