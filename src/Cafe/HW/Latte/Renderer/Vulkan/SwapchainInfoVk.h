#pragma once

#include "util/math/vector2.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"

struct SwapchainInfoVk
{
	void Cleanup();

	void setSize(const Vector2i& newSize)
	{
		desiredExtent = newSize;
		sizeOutOfDate = true;
	}

	const Vector2i& getSize() const
	{
		return desiredExtent;
	}

	SwapchainInfoVk(VkDevice device, VkSurfaceKHR surface, bool mainWindow) : m_device(device), surface(surface), mainWindow(mainWindow) {}
	SwapchainInfoVk(const SwapchainInfoVk&) = delete;
	SwapchainInfoVk(SwapchainInfoVk&&) noexcept = default;
	~SwapchainInfoVk()
	{
		Cleanup();
	}

	bool mainWindow{};

	bool sizeOutOfDate{};
	bool m_usesSRGB = false;
	bool hasDefinedSwapchainImage{}; // indicates if the swapchain image is in a defined state

	VkDevice m_device;

	VkSurfaceKHR surface{};
	VkSurfaceFormatKHR m_surfaceFormat;
	VkSwapchainKHR swapchain{};
	VkExtent2D swapchainExtend{};
	VkFence m_imageAvailableFence{};
	uint32 swapchainImageIndex = (uint32)-1;
	uint32 m_acquireIndex = 0; // increases with every successful vkAcquireNextImageKHR


	// swapchain image ringbuffer (indexed by swapchainImageIndex)
	std::vector<VkImage> m_swapchainImages;
	std::vector<VkImageView> m_swapchainImageViews;
	std::vector<VkFramebuffer> m_swapchainFramebuffers;
	std::vector<VkSemaphore> m_swapchainPresentSemaphores;
	std::vector<VkSemaphore> m_acquireSemaphores; // indexed by acquireIndex

	VkRenderPass m_swapchainRenderPass = nullptr;

private:
	Vector2i desiredExtent;
};
