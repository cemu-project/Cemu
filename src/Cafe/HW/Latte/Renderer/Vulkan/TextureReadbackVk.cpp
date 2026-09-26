#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanRenderer.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanTextureReadback.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/LatteTextureVk.h"

LatteTextureReadbackInfoVk::LatteTextureReadbackInfoVk(VkDevice device, LatteTextureView* textureView)
	: LatteTextureReadbackInfo(textureView, textureView->firstMip), m_device(device)
{
	m_image_size = GetImageSize(textureView);
	m_rowPitch = m_image_size / textureView->baseTexture->GetMipHeight(m_firstMip);
}

LatteTextureReadbackInfoVk::~LatteTextureReadbackInfoVk()
{
}

uint32 LatteTextureReadbackInfoVk::GetImageSize(LatteTextureView* textureView)
{
	auto* baseTexture = (LatteTextureVk*)textureView->baseTexture;
	if (baseTexture->m_isAlternateFormat || baseTexture->IsCompressedFormat())
	{
		cemuLog_logDebug(LogType::Force, "Vulkan does not support readback of texture format 0x{:x}", (uint32)baseTexture->format);
		return 0;
	}
	return LatteTextureReadbackInfo::GetReadbackImageSize(textureView, GetReadbackRowPitch(textureView));
}


void LatteTextureReadbackInfoVk::StartTransfer()
{
	cemu_assert(m_textureView);

	auto* baseTexture = (LatteTextureVk*)m_textureView->baseTexture;
	baseTexture->GetImageObj()->flagForCurrentCommandBuffer();

	cemu_assert_debug(m_textureView->baseTexture->dim != Latte::E_DIM::DIM_3D);

	VkBufferImageCopy region{};
	region.bufferOffset = m_buffer_offset;
	region.bufferRowLength = baseTexture->GetMipWidth(m_firstMip);
	region.bufferImageHeight = baseTexture->GetMipHeight(m_firstMip);

	region.imageSubresource.aspectMask = baseTexture->GetImageAspect();
	region.imageSubresource.baseArrayLayer = m_firstSlice;
	region.imageSubresource.layerCount = 1;
	region.imageSubresource.mipLevel = m_firstMip;

	region.imageOffset = {0,0,0};
	region.imageExtent = {region.bufferRowLength, region.bufferImageHeight, 1};

	const auto renderer = VulkanRenderer::GetInstance();
	renderer->draw_endRenderPass();

	renderer->barrier_image<VulkanRenderer::ANY_TRANSFER | VulkanRenderer::IMAGE_WRITE, VulkanRenderer::TRANSFER_READ>(baseTexture, region.imageSubresource, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

	renderer->barrier_sequentializeTransfer();

	vkCmdCopyImageToBuffer(renderer->getCurrentCommandBuffer(), baseTexture->GetImageObj()->m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_buffer, 1, &region);

	renderer->barrier_sequentializeTransfer();

	renderer->barrier_image<VulkanRenderer::TRANSFER_READ, VulkanRenderer::ANY_TRANSFER | VulkanRenderer::IMAGE_WRITE>(baseTexture, region.imageSubresource, baseTexture->GetDefaultLayout()); // make sure transfer is finished before image is modified
	renderer->barrier_bufferRange<VulkanRenderer::TRANSFER_WRITE, VulkanRenderer::HOST_READ>(m_buffer, m_buffer_offset, m_image_size); // make sure transfer is finished before result is read

	m_associatedCommandBufferId = renderer->GetCurrentCommandBufferId();
	m_textureView = nullptr;

	// to decrease latency of readbacks make sure that the current command buffer is submitted soon
	renderer->RequestSubmitSoon();
	renderer->RequestSubmitOnIdle();
}

bool LatteTextureReadbackInfoVk::IsFinished()
{
	const auto renderer = VulkanRenderer::GetInstance();
	return renderer->HasCommandBufferFinished(m_associatedCommandBufferId);
}

void LatteTextureReadbackInfoVk::ForceFinish()
{
	const auto renderer = VulkanRenderer::GetInstance();
	renderer->WaitCommandBufferFinished(m_associatedCommandBufferId);
}

