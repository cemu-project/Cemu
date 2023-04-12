#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanRenderer.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanTextureReadback.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/LatteTextureVk.h"

LatteTextureReadbackInfoVk::LatteTextureReadbackInfoVk(VkDevice device, LatteTextureView* textureView)
	: LatteTextureReadbackInfo(textureView), m_device(device)
{
	m_image_size = GetImageSize(textureView);
}

LatteTextureReadbackInfoVk::~LatteTextureReadbackInfoVk()
{
}

uint32 LatteTextureReadbackInfoVk::GetImageSize(LatteTextureView* textureView)
{
	const auto* baseTexture = (LatteTextureVk*)textureView->baseTexture;
	// handle format
	const auto textureFormat = baseTexture->GetFormat();
	if (textureView->format == Latte::E_GX2SURFFMT::R8_G8_B8_A8_UNORM)
	{
		cemu_assert(textureFormat == VK_FORMAT_R8G8B8A8_UNORM);
		return baseTexture->width * baseTexture->height * 4;
	}
	else if (textureView->format == Latte::E_GX2SURFFMT::R8_UNORM)
	{
		cemu_assert(textureFormat == VK_FORMAT_R8_UNORM);
		return baseTexture->width * baseTexture->height * 1;
	}
	else if (textureView->format == Latte::E_GX2SURFFMT::R8_G8_B8_A8_SRGB)
	{
		cemu_assert(textureFormat == VK_FORMAT_R8G8B8A8_SRGB);
		return baseTexture->width * baseTexture->height * 4;
	}
	else if (textureView->format == Latte::E_GX2SURFFMT::R32_G32_B32_A32_FLOAT)
	{
		cemu_assert(textureFormat == VK_FORMAT_R32G32B32A32_SFLOAT);
		return baseTexture->width * baseTexture->height * 16;
	}
	else if (textureView->format == Latte::E_GX2SURFFMT::R32_FLOAT)
	{
		cemu_assert(textureFormat == VK_FORMAT_R32_SFLOAT || textureFormat == VK_FORMAT_D32_SFLOAT);
		if (baseTexture->isDepth)
			return baseTexture->width * baseTexture->height * 4;
		else
			return baseTexture->width * baseTexture->height * 4;		
	}
	else if (textureView->format == Latte::E_GX2SURFFMT::R16_UNORM)
	{
		cemu_assert(textureFormat == VK_FORMAT_R16_UNORM);
		if (baseTexture->isDepth)
		{
			cemu_assert_debug(false);
			return baseTexture->width * baseTexture->height * 2;
		}
		else
		{
			return baseTexture->width * baseTexture->height * 2;
		}
	}
	else if (textureView->format == Latte::E_GX2SURFFMT::R16_G16_B16_A16_FLOAT)
	{
		cemu_assert(textureFormat == VK_FORMAT_R16G16B16A16_SFLOAT);
		return baseTexture->width * baseTexture->height * 8;
	}
	else if (textureView->format == Latte::E_GX2SURFFMT::R8_G8_UNORM)
	{
		cemu_assert(textureFormat == VK_FORMAT_R8G8_UNORM);
		return baseTexture->width * baseTexture->height * 2;
	}
	else if (textureView->format == Latte::E_GX2SURFFMT::R16_G16_B16_A16_UNORM)
	{
		cemu_assert(textureFormat == VK_FORMAT_R16G16B16A16_UNORM);
		return baseTexture->width * baseTexture->height * 8;
	}
	else if (textureView->format == Latte::E_GX2SURFFMT::D24_S8_UNORM)
	{
		cemu_assert(textureFormat == VK_FORMAT_D24_UNORM_S8_UINT);
		// todo - if driver does not support VK_FORMAT_D24_UNORM_S8_UINT this is represented as VK_FORMAT_D32_SFLOAT_S8_UINT which is 8 bytes
		return baseTexture->width * baseTexture->height * 4;
	}
	else
	{
		cemuLog_log(LogType::Force, "Unsupported texture readback format {:04x}", (uint32)textureView->format);
		cemu_assert_debug(false);
		return 0;
	}
}


void LatteTextureReadbackInfoVk::StartTransfer()
{
	cemu_assert(m_textureView);

	auto* baseTexture = (LatteTextureVk*)m_textureView->baseTexture;
	baseTexture->GetImageObj()->flagForCurrentCommandBuffer();

	cemu_assert_debug(m_textureView->firstSlice == 0);
	cemu_assert_debug(m_textureView->firstMip == 0);
	cemu_assert_debug(m_textureView->baseTexture->dim != Latte::E_DIM::DIM_3D);

	VkBufferImageCopy region{};
	region.bufferOffset = m_buffer_offset;
	region.bufferRowLength = baseTexture->width;
	region.bufferImageHeight = baseTexture->height;

	region.imageSubresource.aspectMask = baseTexture->GetImageAspect();
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageSubresource.mipLevel = 0;

	region.imageOffset = {0,0,0};
	region.imageExtent = {(uint32)baseTexture->width,(uint32)baseTexture->height,1};

	const auto renderer = VulkanRenderer::GetInstance();
	renderer->draw_endRenderPass();

	renderer->barrier_image<VulkanRenderer::ANY_TRANSFER | VulkanRenderer::IMAGE_WRITE, VulkanRenderer::TRANSFER_READ>(baseTexture, region.imageSubresource, VK_IMAGE_LAYOUT_GENERAL);

	renderer->barrier_sequentializeTransfer();

	vkCmdCopyImageToBuffer(renderer->getCurrentCommandBuffer(), baseTexture->GetImageObj()->m_image, VK_IMAGE_LAYOUT_GENERAL, m_buffer, 1, &region);

	renderer->barrier_sequentializeTransfer();

	renderer->barrier_image<VulkanRenderer::TRANSFER_READ, VulkanRenderer::ANY_TRANSFER | VulkanRenderer::IMAGE_WRITE>(baseTexture, region.imageSubresource, VK_IMAGE_LAYOUT_GENERAL); // make sure transfer is finished before image is modified
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

