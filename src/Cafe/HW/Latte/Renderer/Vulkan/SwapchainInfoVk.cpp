#include "SwapchainInfoVk.h"

#include "config/CemuConfig.h"
#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteTiming.h"

void SwapchainInfoVk::Create()
{
	const auto details = renderer.QuerySwapchainSupport(surface, renderer.GetPhysicalDevice());
	m_surfaceFormat = ChooseSurfaceFormat(details.formats);
	swapchainExtend = ChooseSwapExtent(details.capabilities, getSize());

	const auto logicalDevice = renderer.GetLogicalDevice();

	// calculate number of swapchain presentation images
	uint32_t image_count = details.capabilities.minImageCount + 1;
	if (details.capabilities.maxImageCount > 0 && image_count > details.capabilities.maxImageCount)
		image_count = details.capabilities.maxImageCount;

	VkSwapchainCreateInfoKHR create_info = CreateSwapchainCreateInfo(surface, details, m_surfaceFormat, image_count, swapchainExtend);
	create_info.oldSwapchain = nullptr;
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	VkResult result = vkCreateSwapchainKHR(logicalDevice, &create_info, nullptr, &swapchain);
	if (result != VK_SUCCESS)
		renderer.UnrecoverableError("Error attempting to create a swapchain");

	sizeOutOfDate = false;

	result = vkGetSwapchainImagesKHR(logicalDevice, swapchain, &image_count, nullptr);
	if (result != VK_SUCCESS)
		renderer.UnrecoverableError("Error attempting to retrieve the count of swapchain images");


	m_swapchainImages.resize(image_count);
	result = vkGetSwapchainImagesKHR(logicalDevice, swapchain, &image_count, m_swapchainImages.data());
	if (result != VK_SUCCESS)
		renderer.UnrecoverableError("Error attempting to retrieve swapchain images");
	// create default renderpass
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = m_surfaceFormat.format;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	result = vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &m_swapchainRenderPass);
	if (result != VK_SUCCESS)
		renderer.UnrecoverableError("Failed to create renderpass for swapchain");

	// create swapchain image views
	m_swapchainImageViews.resize(m_swapchainImages.size());
	for (sint32 i = 0; i < m_swapchainImages.size(); i++)
	{
		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = m_swapchainImages[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = m_surfaceFormat.format;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;
		result = vkCreateImageView(logicalDevice, &createInfo, nullptr, &m_swapchainImageViews[i]);
		if (result != VK_SUCCESS)
			renderer.UnrecoverableError("Failed to create imageviews for swapchain");
	}

	// create swapchain framebuffers
	m_swapchainFramebuffers.resize(m_swapchainImages.size());
	for (size_t i = 0; i < m_swapchainImages.size(); i++)
	{
		VkImageView attachments[1];
		attachments[0] = m_swapchainImageViews[i];
		// create framebuffer
		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = m_swapchainRenderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = swapchainExtend.width;
		framebufferInfo.height = swapchainExtend.height;
		framebufferInfo.layers = 1;
		result = vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr, &m_swapchainFramebuffers[i]);
		if (result != VK_SUCCESS)
			renderer.UnrecoverableError("Failed to create framebuffer for swapchain");
	}
	m_swapchainPresentSemaphores.resize(m_swapchainImages.size());
	// create present semaphore
	VkSemaphoreCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	for (auto& semaphore : m_swapchainPresentSemaphores){
		if (vkCreateSemaphore(logicalDevice, &info, nullptr, &semaphore) != VK_SUCCESS)
			renderer.UnrecoverableError("Failed to create semaphore for swapchain present");
	}

	m_acquireSemaphores.resize(m_swapchainImages.size());
	for (auto& semaphore : m_acquireSemaphores)
	{
		VkSemaphoreCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		if (vkCreateSemaphore(logicalDevice, &info, nullptr, &semaphore) != VK_SUCCESS)
			renderer.UnrecoverableError("Failed to create semaphore for swapchain acquire");
	}
	m_acquireIndex = 0;

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	result = vkCreateFence(logicalDevice, &fenceInfo, nullptr, &m_imageAvailableFence);
	if (result != VK_SUCCESS)
		renderer.UnrecoverableError("Failed to create fence for swapchain");
}

void SwapchainInfoVk::Cleanup()
{
	m_swapchainImages.clear();

	auto logicalDevice = renderer.GetLogicalDevice();

	for (auto& sem: m_swapchainPresentSemaphores)
		vkDestroySemaphore(logicalDevice, sem, nullptr);
	m_swapchainPresentSemaphores.clear();

	for (auto& itr: m_acquireSemaphores)
		vkDestroySemaphore(logicalDevice, itr, nullptr);
	m_acquireSemaphores.clear();

	if (m_swapchainRenderPass)
	{
		vkDestroyRenderPass(logicalDevice, m_swapchainRenderPass, nullptr);
		m_swapchainRenderPass = nullptr;
	}

	for (auto& imageView : m_swapchainImageViews)
		vkDestroyImageView(logicalDevice, imageView, nullptr);
	m_swapchainImageViews.clear();

	for (auto& framebuffer : m_swapchainFramebuffers)
		vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
	m_swapchainFramebuffers.clear();


	if (m_imageAvailableFence)
	{
		vkDestroyFence(logicalDevice, m_imageAvailableFence, nullptr);
		m_imageAvailableFence = nullptr;
	}
	if (swapchain)
	{
		vkDestroySwapchainKHR(logicalDevice, swapchain, nullptr);
		swapchain = VK_NULL_HANDLE;
	}
}

bool SwapchainInfoVk::IsValid() const
{
	return swapchain && m_imageAvailableFence;
}


VkSurfaceFormatKHR SwapchainInfoVk::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const
{
	if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
		return{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };

	for (const auto& format : formats)
	{
		bool useSRGB = mainWindow ? LatteGPUState.tvBufferUsesSRGB : LatteGPUState.drcBufferUsesSRGB;

		if (useSRGB)
		{
			if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				return format;
		}
		else
		{
			if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				return format;
		}
	}

	return formats[0];
}

VkExtent2D SwapchainInfoVk::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, const Vector2i& size) const
{
	if (capabilities.currentExtent.width != std::numeric_limits<uint32>::max())
		return capabilities.currentExtent;

	VkExtent2D actualExtent = { (uint32)size.x, (uint32)size.y };
	actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
	actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));
	return actualExtent;
}

VkSwapchainCreateInfoKHR SwapchainInfoVk::CreateSwapchainCreateInfo(VkSurfaceKHR surface, const SwapchainSupportDetails& swapchainSupport, const VkSurfaceFormatKHR& surfaceFormat, uint32 imageCount, const VkExtent2D& extent)
{
	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	const QueueFamilyIndices indices = VulkanRenderer::FindQueueFamilies(surface, renderer.GetPhysicalDevice());
	uint32_t queueFamilyIndices[] = { (uint32)indices.graphicsFamily, (uint32)indices.presentFamily };
	if (indices.graphicsFamily != indices.presentFamily)
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

	createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = ChoosePresentMode(swapchainSupport.presentModes);
	createInfo.clipped = VK_TRUE;

	forceLogDebug_printf("vulkan presentation mode: %d", createInfo.presentMode);
	return createInfo;
}



VkPresentModeKHR SwapchainInfoVk::ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes)
{
	VSync vsyncState = renderer.GetVSyncState();
	if (vsyncState == VSync::MAILBOX)
	{
		if (std::find(modes.cbegin(), modes.cend(), VK_PRESENT_MODE_MAILBOX_KHR) != modes.cend())
			return VK_PRESENT_MODE_MAILBOX_KHR;

		forceLog_printf("Vulkan: Can't find mailbox present mode");
	}
	else if (vsyncState == VSync::Immediate)
	{
		if (std::find(modes.cbegin(), modes.cend(), VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.cend())
			return VK_PRESENT_MODE_IMMEDIATE_KHR;

		forceLog_printf("Vulkan: Can't find immediate present mode");
	}
	else if (vsyncState == VSync::SYNC_AND_LIMIT)
	{
		LatteTiming_EnableHostDrivenVSync();
		// use immediate mode if available, other wise fall back to
		//if (std::find(modes.cbegin(), modes.cend(), VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.cend())
		//	return VK_PRESENT_MODE_IMMEDIATE_KHR;
		//else
		//	forceLog_printf("Vulkan: Present mode 'immediate' not available. Vsync might not behave as intended");
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}
