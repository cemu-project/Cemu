#pragma once

struct SwapchainInfoVk;

#include "util/math/vector2.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/MiscStructs.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanRenderer.h"

struct SwapchainInfoVk
{
	void Cleanup();
	void Create();

	bool IsValid() const;

	VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, const Vector2i& size) const;
	VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes);

	VkSwapchainCreateInfoKHR CreateSwapchainCreateInfo(VkSurfaceKHR surface, const SwapchainSupportDetails& swapchainSupport, const VkSurfaceFormatKHR& surfaceFormat, uint32 imageCount, const VkExtent2D& extent);


	void setSize(const Vector2i& newSize)
	{
		desiredExtent = newSize;
		sizeOutOfDate = true;
	}

	const Vector2i& getSize() const
	{
		return desiredExtent;
	}

	SwapchainInfoVk(VulkanRenderer& renderer, VkSurfaceKHR surface, bool mainWindow)
		: renderer(renderer), surface(surface), mainWindow(mainWindow) {}
	SwapchainInfoVk(const SwapchainInfoVk&) = delete;
	SwapchainInfoVk(SwapchainInfoVk&&) noexcept = default;
	~SwapchainInfoVk()
	{
		Cleanup();
	}

	VulkanRenderer& renderer;

	bool mainWindow{};

	bool sizeOutOfDate{};
	bool m_usesSRGB = false;
	bool hasDefinedSwapchainImage{}; // indicates if the swapchain image is in a defined state

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
