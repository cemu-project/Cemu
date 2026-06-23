#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanRenderer.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/CachedFBOVk.h"

void CachedFBOVk::CreateRenderPass()
{
	VKRObjectRenderPass::AttachmentInfo_t attachmentInfo;

	for (int i = 0; i < 8; ++i)
	{
		auto& buffer = colorBuffer[i];
		auto textureViewVk = (LatteTextureViewVk*)buffer.texture;
		if (!textureViewVk)
		{
			attachmentInfo.colorAttachment[i].viewObj = nullptr;
			continue;
		}
		// setup color attachment
		auto viewObj = textureViewVk->GetViewRGBA();
		attachmentInfo.colorAttachment[i].viewObj = viewObj;
		attachmentInfo.colorAttachment[i].format = textureViewVk->GetFormat();
	}

	// setup depth attachment
	if (depthBuffer.texture)
	{
		LatteTextureViewVk* depthTexVk = static_cast<LatteTextureViewVk*>(depthBuffer.texture);
		auto depthBufferViewObj = depthTexVk->GetViewRGBA();
		attachmentInfo.depthAttachment.viewObj = depthBufferViewObj;
		attachmentInfo.depthAttachment.format = depthTexVk->GetFormat();
		attachmentInfo.depthAttachment.hasStencil = depthTexVk->baseTexture->hasStencil;
	}
	else
	{
		// no depth attachment
		attachmentInfo.depthAttachment.viewObj = nullptr;
	}

	m_vkrObjRenderPass = new VKRObjectRenderPass(attachmentInfo);
}

CachedFBOVk::~CachedFBOVk()
{
	while (!m_usedByPipelines.empty())
		delete m_usedByPipelines[0];
	auto vkr = VulkanRenderer::GetInstance();
	vkr->ReleaseDestructibleObject(m_vkrObjFramebuffer);
	m_vkrObjFramebuffer = nullptr;
	vkr->ReleaseDestructibleObject(m_vkrObjRenderPass);
	m_vkrObjRenderPass = nullptr;
}

VKRObjectTextureView* CachedFBOVk::GetColorBufferImageView(uint32 index)
{
	cemu_assert(index < 8);
	auto& cb = colorBuffer[index];
	auto textureViewVk = (LatteTextureViewVk*)cb.texture;
	if (!textureViewVk)
		return nullptr;
	auto viewDim = textureViewVk->dim;
	if (viewDim == Latte::E_DIM::DIM_3D)
		viewDim = Latte::E_DIM::DIM_2D; // bind 3D texture slices as 2D images
	return textureViewVk->GetViewRGBA();
}

VKRObjectTextureView* CachedFBOVk::GetDepthStencilBufferImageView(bool& hasStencil)
{
	hasStencil = false;
	auto textureViewVk = (LatteTextureViewVk*)depthBuffer.texture;
	if (!textureViewVk)
		return nullptr;
	cemu_assert_debug(textureViewVk->numMip == 1);
	hasStencil = textureViewVk->baseTexture->hasStencil;
	return textureViewVk->GetViewRGBA();
}

void CachedFBOVk::CreateFramebuffer()
{
	std::array<VKRObjectTextureView*, 9> imageViews{};
	int imageViewIndex = 0;
	for (uint32 i = 0; i < 8; ++i)
	{
		VKRObjectTextureView* cbView = GetColorBufferImageView(i);
		if (!cbView)
			continue;
		imageViews[imageViewIndex++] = cbView;
	}

	bool hasStencil = false;
	VKRObjectTextureView* depthStencilView = GetDepthStencilBufferImageView(hasStencil);
	if (depthStencilView)
		imageViews[imageViewIndex++] = depthStencilView;

	cemu_assert_debug(imageViewIndex < 9);

	m_vkrObjFramebuffer = new VKRObjectFramebuffer(m_vkrObjRenderPass, std::span<VKRObjectTextureView*>(imageViews.data(), imageViewIndex), m_size);

	m_extend = { (uint32)m_size.x, (uint32)m_size.y };
}


void CachedFBOVk::InitDynamicRenderingData()
{
	// init struct for KHR_dynamic_rendering
	for (int i = 0; i < 8; ++i)
	{
		m_vkColorAttachments[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
		m_vkColorAttachments[i].pNext = nullptr;
		m_vkColorAttachments[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		m_vkColorAttachments[i].resolveMode = VK_RESOLVE_MODE_NONE;
		m_vkColorAttachments[i].resolveImageView = VK_NULL_HANDLE;
		m_vkColorAttachments[i].resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		m_vkColorAttachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		m_vkColorAttachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;

		// ignore clearValue

		VKRObjectTextureView* cbView = GetColorBufferImageView(i);

		auto& buffer = colorBuffer[i];
		if (!cbView)
		{
			m_vkColorAttachments[i].imageView = VK_NULL_HANDLE;
			continue;
		}
		else
		{
			m_vkColorAttachments[i].imageLayout = static_cast<LatteTextureVk*>(buffer.texture->baseTexture)->GetDefaultLayout();
			m_vkColorAttachments[i].imageView = cbView->m_textureImageView;
		}
	}

	m_vkRenderingInfo.pColorAttachments = m_vkColorAttachments;
	m_vkRenderingInfo.colorAttachmentCount = 8;
	// trim the color attachment list if tail entries are not set
	while (m_vkRenderingInfo.colorAttachmentCount > 0)
	{
		if (m_vkColorAttachments[m_vkRenderingInfo.colorAttachmentCount - 1].imageView)
			break;
		m_vkRenderingInfo.colorAttachmentCount--;
	}

	// initially set both stencil and depth attachment to an empty default
	m_vkDepthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
	m_vkDepthAttachment.pNext = nullptr;
	m_vkDepthAttachment.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	m_vkDepthAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
	m_vkDepthAttachment.resolveImageView = VK_NULL_HANDLE;
	m_vkDepthAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	m_vkDepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	m_vkDepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	m_vkStencilAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
	m_vkStencilAttachment.pNext = nullptr;
	m_vkStencilAttachment.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	m_vkStencilAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
	m_vkStencilAttachment.resolveImageView = VK_NULL_HANDLE;
	m_vkStencilAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	m_vkStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	m_vkStencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	m_vkRenderingInfo.pDepthAttachment = nullptr;
	m_vkRenderingInfo.pStencilAttachment = nullptr;

	bool hasStencil = false;
	VKRObjectTextureView* depthStencilView = GetDepthStencilBufferImageView(hasStencil);

	// setup depth and stencil attachment
	if (depthStencilView)
	{
		auto depthTexVk = static_cast<LatteTextureVk*>(depthBuffer.texture->baseTexture);
		m_vkDepthAttachment.imageLayout = depthTexVk->GetDefaultLayout();
		m_vkStencilAttachment.imageLayout = depthTexVk->GetDefaultLayout();
		m_vkDepthAttachment.imageView = depthStencilView->m_textureImageView;
		m_vkDepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		m_vkDepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		m_vkRenderingInfo.pDepthAttachment = &m_vkDepthAttachment;
		if (hasStencil)
		{
			m_vkStencilAttachment.imageView = depthStencilView->m_textureImageView;
			m_vkStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			m_vkStencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			m_vkRenderingInfo.pStencilAttachment = &m_vkStencilAttachment;
		}
	}

	m_vkRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
	m_vkRenderingInfo.pNext = nullptr;
	m_vkRenderingInfo.flags = 0;
	m_vkRenderingInfo.renderArea.offset = { 0, 0 };
	m_vkRenderingInfo.renderArea.extent = m_extend;
	m_vkRenderingInfo.viewMask = 0; // multiview disabled
	m_vkRenderingInfo.layerCount = 1;
}

static uint32 s_selfDependencyCheckIndex = 1;

CachedFBOVk::RendertargetSelfDependencyMask CachedFBOVk::CheckForSelfDependency(VkDescriptorSetInfo* vsDS, VkDescriptorSetInfo* gsDS, VkDescriptorSetInfo* psDS) const
{
	s_selfDependencyCheckIndex++;
	const uint32 curColIndex = s_selfDependencyCheckIndex;
	for (auto& colorAttachment : colorBuffer)
	{
		if (colorAttachment.texture)
		{
			LatteTextureVk* vkTex = static_cast<LatteTextureVk*>(colorAttachment.texture->baseTexture);
			vkTex->m_selfDependencyCheckIndex = curColIndex;
			vkTex->m_selfDependencyCheckAspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}
	}
	if (depthBuffer.texture)
	{
		LatteTextureVk* vkTex = static_cast<LatteTextureVk*>(depthBuffer.texture->baseTexture);
		vkTex->m_selfDependencyCheckIndex = curColIndex;
		vkTex->m_selfDependencyCheckAspectMask = depthBuffer.hasStencil ? (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT): VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	auto getSelfDependencyMask = [curColIndex](VkDescriptorSetInfo* ds) -> VkImageAspectFlags
	{
		VkImageAspectFlags aspectMask = 0;
		if (!ds)
			return aspectMask;
		for (auto& itr : ds->list_fboCandidates)
		{
			if (itr->m_selfDependencyCheckIndex == curColIndex)
				aspectMask |= itr->m_selfDependencyCheckAspectMask;
		}
		return aspectMask;
	};

	RendertargetSelfDependencyMask selfDepInfo{};
	VkImageAspectFlags vertexAspectFlags = getSelfDependencyMask(vsDS);
	VkImageAspectFlags geometryAspectFlags = getSelfDependencyMask(gsDS);
	VkImageAspectFlags pixelAspectFlags = getSelfDependencyMask(psDS);
	selfDepInfo.aspectMaskFlags = vertexAspectFlags | geometryAspectFlags | pixelAspectFlags;
	selfDepInfo.hasNonPixelSelfDependency = (vertexAspectFlags | geometryAspectFlags) != 0;
	return selfDepInfo;
}
