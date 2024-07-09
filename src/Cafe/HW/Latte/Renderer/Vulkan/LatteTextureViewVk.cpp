#include "Cafe/HW/Latte/Renderer/Vulkan/LatteTextureViewVk.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/LatteTextureVk.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanRenderer.h"
#include "Cafe/HW/Latte/Core/LattePerformanceMonitor.h"

uint32 LatteTextureVk_AdjustTextureCompSel(Latte::E_GX2SURFFMT format, uint32 compSel)
{
	switch (format)
	{
	case Latte::E_GX2SURFFMT::R8_UNORM: // R8 is replicated on all channels (while OpenGL would return 1.0 for BGA instead)
	case Latte::E_GX2SURFFMT::R8_SNORM: // probably the same as _UNORM, but needs testing
		if (compSel >= 1 && compSel <= 3)
			compSel = 0;
		break;
	case Latte::E_GX2SURFFMT::A1_B5_G5_R5_UNORM: // order of components is reversed (RGBA -> ABGR)
		if (compSel >= 0 && compSel <= 3)
			compSel = 3 - compSel;
		break;
	case Latte::E_GX2SURFFMT::BC4_UNORM:
	case Latte::E_GX2SURFFMT::BC4_SNORM:
		if (compSel >= 1 && compSel <= 3)
			compSel = 0;
		break;
	case Latte::E_GX2SURFFMT::BC5_UNORM:
	case Latte::E_GX2SURFFMT::BC5_SNORM:
		// RG maps to RG
		// B maps to ?
		// A maps to G (guessed)
		if (compSel == 3)
			compSel = 1; // read Alpha as Green
		break;
	case Latte::E_GX2SURFFMT::A2_B10_G10_R10_UNORM:
		// reverse components (Wii U: ABGR, OpenGL: RGBA)
		// used in Resident Evil Revelations
		if (compSel >= 0 && compSel <= 3)
			compSel = 3 - compSel;
		break;
	case Latte::E_GX2SURFFMT::X24_G8_UINT:
		// map everything to alpha?
		if (compSel >= 0 && compSel <= 3)
			compSel = 3;
		break;
	case Latte::E_GX2SURFFMT::R4_G4_UNORM:
		// red and green swapped
		if (compSel == 0)
			compSel = 1;
		else if (compSel == 1)
			compSel = 0;
		break;
	default:
		break;
	}
	return compSel;
}

LatteTextureViewVk::LatteTextureViewVk(VkDevice device, LatteTextureVk* texture, Latte::E_DIM dim, Latte::E_GX2SURFFMT format, sint32 firstMip, sint32 mipCount, sint32 firstSlice, sint32 sliceCount)
	: LatteTextureView(texture, firstMip, mipCount, firstSlice, sliceCount, dim, format), m_device(device)
{
	if(texture->overwriteInfo.hasFormatOverwrite)
	{
		cemu_assert_debug(format == texture->format); // if format overwrite is used, the texture is no longer taking part in aliasing and the format of any view has to match
		m_format = texture->GetFormat();
	}
	else if (dim != texture->dim || format != texture->format)
	{
		VulkanRenderer::FormatInfoVK texFormatInfo;
		VulkanRenderer::GetInstance()->GetTextureFormatInfoVK(format, texture->isDepth, dim, 0, 0, &texFormatInfo);
		m_format = texFormatInfo.vkImageFormat;
	}
	else
		m_format = texture->GetFormat();
	m_uniqueId = VulkanRenderer::GetInstance()->GenUniqueId();
}

LatteTextureViewVk::~LatteTextureViewVk()
{
	while (!list_descriptorSets.empty())
		delete list_descriptorSets[0];

	if (m_smallCacheView0)
		VulkanRenderer::GetInstance()->ReleaseDestructibleObject(m_smallCacheView0);
	if (m_smallCacheView1)
		VulkanRenderer::GetInstance()->ReleaseDestructibleObject(m_smallCacheView1);

	if (m_fallbackCache)
	{
		for (auto& itr : *m_fallbackCache)
			VulkanRenderer::GetInstance()->ReleaseDestructibleObject(itr.second);
		delete m_fallbackCache;
		m_fallbackCache = nullptr;
	}
}

VKRObjectTextureView* LatteTextureViewVk::CreateView(uint32 gpuSamplerSwizzle)
{
	uint32 compSelR = (gpuSamplerSwizzle >> 16) & 0x7;
	uint32 compSelG = (gpuSamplerSwizzle >> 19) & 0x7;
	uint32 compSelB = (gpuSamplerSwizzle >> 22) & 0x7;
	uint32 compSelA = (gpuSamplerSwizzle >> 25) & 0x7;
	compSelR = LatteTextureVk_AdjustTextureCompSel(format, compSelR);
	compSelG = LatteTextureVk_AdjustTextureCompSel(format, compSelG);
	compSelB = LatteTextureVk_AdjustTextureCompSel(format, compSelB);
	compSelA = LatteTextureVk_AdjustTextureCompSel(format, compSelA);

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = GetBaseImage()->GetImageObj()->m_image;
	viewInfo.viewType = GetImageViewTypeFromGX2Dim(dim);
	viewInfo.format = m_format;
	viewInfo.subresourceRange.aspectMask = GetBaseImage()->GetImageAspect();
	if (viewInfo.subresourceRange.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT)
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT; // make sure stencil is never set, we only support sampling depth for now
	viewInfo.subresourceRange.baseMipLevel = firstMip;
	viewInfo.subresourceRange.levelCount = this->numMip;
	if (viewInfo.viewType == VK_IMAGE_VIEW_TYPE_3D && baseTexture->Is3DTexture())
	{
		cemu_assert_debug(firstMip == 0);
		cemu_assert_debug(this->numSlice == baseTexture->depth);
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
	}
	else
	{
		viewInfo.subresourceRange.baseArrayLayer = firstSlice;
		viewInfo.subresourceRange.layerCount = this->numSlice;
	}

	static const VkComponentSwizzle swizzle[] =
	{
		VK_COMPONENT_SWIZZLE_R,
		VK_COMPONENT_SWIZZLE_G,
		VK_COMPONENT_SWIZZLE_B,
		VK_COMPONENT_SWIZZLE_A,
		VK_COMPONENT_SWIZZLE_ZERO,
		VK_COMPONENT_SWIZZLE_ONE,
		VK_COMPONENT_SWIZZLE_ZERO,
		VK_COMPONENT_SWIZZLE_ZERO
	};

	viewInfo.components.r = swizzle[compSelR];
	viewInfo.components.g = swizzle[compSelG];
	viewInfo.components.b = swizzle[compSelB];
	viewInfo.components.a = swizzle[compSelA];

	VkImageView view;
	if (vkCreateImageView(m_device, &viewInfo, nullptr, &view) != VK_SUCCESS)
		throw std::runtime_error("failed to create texture image view!");

	return new VKRObjectTextureView(GetBaseImage()->GetImageObj(), view);
}

VKRObjectTextureView* LatteTextureViewVk::GetViewRGBA()
{ 
	return GetSamplerView(0x06880000); // RGBA swizzle
}

VKRObjectTextureView* LatteTextureViewVk::GetSamplerView(uint32 gpuSamplerSwizzle)
{
	gpuSamplerSwizzle &= 0x0FFF0000;
	// look up view in cache
	// intentionally using unrolled code here instead of a loop for a small performance gain
	if (m_smallCacheSwizzle0 == gpuSamplerSwizzle)
		return m_smallCacheView0;
	if (m_smallCacheSwizzle1 == gpuSamplerSwizzle)
		return m_smallCacheView1;
	auto fallbackCache = m_fallbackCache;
	if (m_fallbackCache)
	{
		const auto it = fallbackCache->find(gpuSamplerSwizzle);
		if (it != fallbackCache->cend())
			return it->second;
	}
	// not cached, create new view and store in cache
	auto viewObj = CreateView(gpuSamplerSwizzle);
	if (m_smallCacheSwizzle0 == CACHE_EMPTY_ENTRY)
	{
		m_smallCacheSwizzle0 = gpuSamplerSwizzle;
		m_smallCacheView0 = viewObj;
	}
	else if (m_smallCacheSwizzle1 == CACHE_EMPTY_ENTRY)
	{
		m_smallCacheSwizzle1 = gpuSamplerSwizzle;
		m_smallCacheView1 = viewObj;
	}
	else
	{
		if (!m_fallbackCache)
			m_fallbackCache = new std::unordered_map<uint32, VKRObjectTextureView*>();
		m_fallbackCache->insert_or_assign(gpuSamplerSwizzle, viewObj);
	}
	return viewObj;
}

VkSampler LatteTextureViewVk::GetDefaultTextureSampler(bool useLinearTexFilter)
{
	VkSampler& sampler = GetViewRGBA()->m_textureDefaultSampler[useLinearTexFilter ? 1 : 0];

	if (sampler != VK_NULL_HANDLE)
		return sampler;

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

	if (useLinearTexFilter)
	{
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
	}
	else
	{
		samplerInfo.magFilter = VK_FILTER_NEAREST;
		samplerInfo.minFilter = VK_FILTER_NEAREST;
	}

	if (vkCreateSampler(m_device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
	{
		cemuLog_log(LogType::Force, "Failed to create default sampler");
		throw std::runtime_error("failed to create texture sampler!");
	}

	return sampler;
}

VkImageViewType LatteTextureViewVk::GetImageViewTypeFromGX2Dim(Latte::E_DIM dim)
{
	switch (dim)
	{
	case Latte::E_DIM::DIM_1D:
		return VK_IMAGE_VIEW_TYPE_1D;
	case Latte::E_DIM::DIM_2D:
	case Latte::E_DIM::DIM_2D_MSAA:
		return VK_IMAGE_VIEW_TYPE_2D;
	case Latte::E_DIM::DIM_2D_ARRAY:
		return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	case Latte::E_DIM::DIM_3D:
		return VK_IMAGE_VIEW_TYPE_3D;
	case Latte::E_DIM::DIM_CUBEMAP:
		return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
	default:
		cemu_assert_unimplemented();
	}
	return VK_IMAGE_VIEW_TYPE_2D;
}
