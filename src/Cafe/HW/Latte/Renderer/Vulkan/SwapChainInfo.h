#pragma once

#include "util/math/vector2.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"

struct SwapChainInfo
{
	void Cleanup(bool semaphores);

	bool Ready();

	SwapChainInfo(VkDevice device, VkSurfaceKHR surface, bool mainWindow) : m_device(device), surface(surface), mainWindow(mainWindow) {}
	SwapChainInfo(const SwapChainInfo&) = delete;
	SwapChainInfo(SwapChainInfo&&) noexcept = default;
	~SwapChainInfo()
	{
		Cleanup(true);
	}

	bool shouldExist{};
	bool mainWindow{};

	bool m_usesSRGB = false;
	bool hasDefinedSwapchainImage{}; // indicates if the swapchain image is in a defined state

	VkDevice m_device;

	VkSurfaceKHR surface{};
	VkSwapchainKHR swapChain{};
	VkExtent2D swapchainExtend{};
	uint32 swapchainImageIndex = (uint32)-1;
	uint32 m_acquireIndex = 0; // increases with every successful vkAcquireNextImageKHR

	bool resizeRequested{};
	Vector2i desiredExtent;

	// swapchain image ringbuffer (indexed by swapchainImageIndex)
	std::vector<VkImage> m_swapchainImages;
	std::vector<VkImageView> m_swapchainImageViews;
	std::vector<VkFramebuffer> m_swapchainFramebuffers;
	std::vector<VkSemaphore> m_renderFinishedSemaphores;
	std::vector<VkSemaphore> m_imageAvailableSemaphores; // indexed by acquireIndex

	VkRenderPass m_swapChainRenderPass = nullptr;
};
