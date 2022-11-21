#include "SwapchainInfoVk.h"

#include "config/CemuConfig.h"
#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteTiming.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"

void SwapchainInfoVk::Create(VkPhysicalDevice physicalDevice, VkDevice logicalDevice)
{
	m_physicalDevice = physicalDevice;
	m_logicalDevice = logicalDevice;
	const auto details = QuerySwapchainSupport(surface, physicalDevice);
	m_surfaceFormat = ChooseSurfaceFormat(details.formats);
	m_actualExtent = ChooseSwapExtent(details.capabilities);

	uint32_t image_count = details.capabilities.minImageCount;

	VkSwapchainCreateInfoKHR create_info = CreateSwapchainCreateInfo(surface, details, m_surfaceFormat, image_count, m_actualExtent);
	create_info.oldSwapchain = nullptr;
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	VkResult result = vkCreateSwapchainKHR(logicalDevice, &create_info, nullptr, &swapchain);
	if (result != VK_SUCCESS)
		UnrecoverableError("Error attempting to create a swapchain");

	result = vkGetSwapchainImagesKHR(logicalDevice, swapchain, &image_count, nullptr);
	if (result != VK_SUCCESS)
		UnrecoverableError("Error attempting to retrieve the count of swapchain images");


	m_swapchainImages.resize(image_count);
	result = vkGetSwapchainImagesKHR(logicalDevice, swapchain, &image_count, m_swapchainImages.data());
	if (result != VK_SUCCESS)
		UnrecoverableError("Error attempting to retrieve swapchain images");
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
		UnrecoverableError("Failed to create renderpass for swapchain");

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
			UnrecoverableError("Failed to create imageviews for swapchain");
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
		framebufferInfo.width = m_actualExtent.width;
		framebufferInfo.height = m_actualExtent.height;
		framebufferInfo.layers = 1;
		result = vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr, &m_swapchainFramebuffers[i]);
		if (result != VK_SUCCESS)
			UnrecoverableError("Failed to create framebuffer for swapchain");
	}
	m_swapchainPresentSemaphores.resize(m_swapchainImages.size());
	// create present semaphore
	VkSemaphoreCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	for (auto& semaphore : m_swapchainPresentSemaphores){
		if (vkCreateSemaphore(logicalDevice, &info, nullptr, &semaphore) != VK_SUCCESS)
			UnrecoverableError("Failed to create semaphore for swapchain present");
	}

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	result = vkCreateFence(logicalDevice, &fenceInfo, nullptr, &m_imageAvailableFence);
	if (result != VK_SUCCESS)
		UnrecoverableError("Failed to create fence for swapchain");

	hasDefinedSwapchainImage = false;
}

void SwapchainInfoVk::Cleanup()
{
	m_swapchainImages.clear();

	for (auto& sem: m_swapchainPresentSemaphores)
		vkDestroySemaphore(m_logicalDevice, sem, nullptr);
	m_swapchainPresentSemaphores.clear();

	if (m_swapchainRenderPass)
	{
		vkDestroyRenderPass(m_logicalDevice, m_swapchainRenderPass, nullptr);
		m_swapchainRenderPass = nullptr;
	}

	for (auto& imageView : m_swapchainImageViews)
		vkDestroyImageView(m_logicalDevice, imageView, nullptr);
	m_swapchainImageViews.clear();

	for (auto& framebuffer : m_swapchainFramebuffers)
		vkDestroyFramebuffer(m_logicalDevice, framebuffer, nullptr);
	m_swapchainFramebuffers.clear();


	if (m_imageAvailableFence)
	{
		vkDestroyFence(m_logicalDevice, m_imageAvailableFence, nullptr);
		m_imageAvailableFence = nullptr;
	}
	if (swapchain)
	{
		vkDestroySwapchainKHR(m_logicalDevice, swapchain, nullptr);
		swapchain = VK_NULL_HANDLE;
	}
}

bool SwapchainInfoVk::IsValid() const
{
	return swapchain && m_imageAvailableFence;
}

void SwapchainInfoVk::WaitAvailableFence() const
{
	vkWaitForFences(m_logicalDevice, 1, &m_imageAvailableFence, VK_TRUE, UINT64_MAX);
}

void SwapchainInfoVk::UnrecoverableError(const char* errMsg)
{
	forceLog_printf("Unrecoverable error in Vulkan swapchain");
	forceLog_printf("Msg: %s", errMsg);
	throw std::runtime_error(errMsg);
}

SwapchainInfoVk::QueueFamilyIndices SwapchainInfoVk::FindQueueFamilies(VkSurfaceKHR surface, VkPhysicalDevice device)
{
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	QueueFamilyIndices indices;
	for (int i = 0; i < (int)queueFamilies.size(); ++i)
	{
		const auto& queueFamily = queueFamilies[i];
		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			indices.graphicsFamily = i;

		VkBool32 presentSupport = false;
		const VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
		if (result != VK_SUCCESS)
			throw std::runtime_error(fmt::format("Error while attempting to check if a surface supports presentation: {}", result));

		if (queueFamily.queueCount > 0 && presentSupport)
			indices.presentFamily = i;

		if (indices.IsComplete())
			break;
	}

	return indices;
}

SwapchainInfoVk::SwapchainSupportDetails SwapchainInfoVk::QuerySwapchainSupport(VkSurfaceKHR surface, const VkPhysicalDevice& device)
{
	SwapchainSupportDetails details;

	VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
	if (result != VK_SUCCESS)
	{
		if (result != VK_ERROR_SURFACE_LOST_KHR)
			forceLog_printf("vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed. Error %d", (sint32)result);
		throw std::runtime_error(fmt::format("Unable to retrieve physical device surface capabilities: {}", result));
	}

	uint32_t formatCount = 0;
	result = vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
	if (result != VK_SUCCESS)
	{
		forceLog_printf("vkGetPhysicalDeviceSurfaceFormatsKHR failed. Error %d", (sint32)result);
		throw std::runtime_error(fmt::format("Unable to retrieve the number of formats for a surface on a physical device: {}", result));
	}

	if (formatCount != 0)
	{
		details.formats.resize(formatCount);
		result = vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
		if (result != VK_SUCCESS)
		{
			forceLog_printf("vkGetPhysicalDeviceSurfaceFormatsKHR failed. Error %d", (sint32)result);
			throw std::runtime_error(fmt::format("Unable to retrieve the formats for a surface on a physical device: {}", result));
		}
	}

	uint32_t presentModeCount = 0;
	result = vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
	if (result != VK_SUCCESS)
	{
		forceLog_printf("vkGetPhysicalDeviceSurfacePresentModesKHR failed. Error %d", (sint32)result);
		throw std::runtime_error(fmt::format("Unable to retrieve the count of present modes for a surface on a physical device: {}", result));
	}

	if (presentModeCount != 0)
	{
		details.presentModes.resize(presentModeCount);
		result = vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
		if (result != VK_SUCCESS)
		{
			forceLog_printf("vkGetPhysicalDeviceSurfacePresentModesKHR failed. Error %d", (sint32)result);
			throw std::runtime_error(fmt::format("Unable to retrieve the present modes for a surface on a physical device: {}", result));
		}
	}

	return details;
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

VkExtent2D SwapchainInfoVk::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const
{
	if (capabilities.currentExtent.width != std::numeric_limits<uint32>::max())
		return capabilities.currentExtent;

	VkExtent2D actualExtent = { (uint32)m_desiredExtent.x, (uint32)m_desiredExtent.y };
	actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
	actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));
	return actualExtent;
}

VkPresentModeKHR SwapchainInfoVk::ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes)
{
	const auto vsyncState = (VSync)GetConfig().vsync.GetValue();
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

	const QueueFamilyIndices indices = FindQueueFamilies(surface, m_physicalDevice);
	m_swapchainQueueFamilyIndices = { (uint32)indices.graphicsFamily, (uint32)indices.presentFamily };
	if (indices.graphicsFamily != indices.presentFamily)
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = m_swapchainQueueFamilyIndices.size();
		createInfo.pQueueFamilyIndices = m_swapchainQueueFamilyIndices.data();
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
