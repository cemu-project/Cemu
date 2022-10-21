#pragma once

#include "util/math/vector2.h"
#include <vulkan/vulkan_core.h>

struct SwapchainInfoVk
{
	enum class VSync
	{
		// values here must match GeneralSettings2::m_vsync
		Immediate = 0,
		FIFO = 1,
		MAILBOX = 2,
		SYNC_AND_LIMIT = 3, // synchronize emulated vsync events to monitor vsync. But skip events if rate higher than virtual vsync period
	};

	struct QueueFamilyIndices
	{
		int32_t graphicsFamily = -1;
		int32_t presentFamily = -1;

		bool IsComplete() const	{ return graphicsFamily >= 0 && presentFamily >= 0;	}
	};

	struct SwapchainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	void Cleanup();
	void Create(VkPhysicalDevice physicalDevice, VkDevice logicalDevice);

	bool IsValid() const;

	static void UnrecoverableError(const char* errMsg);

	static QueueFamilyIndices FindQueueFamilies(VkSurfaceKHR surface, VkPhysicalDevice device);
	static SwapchainSupportDetails QuerySwapchainSupport(VkSurfaceKHR surface, const VkPhysicalDevice& device);

	VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes);
	VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, const Vector2i& size) const;

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

	SwapchainInfoVk(VkSurfaceKHR surface, bool mainWindow)
		: surface(surface), mainWindow(mainWindow) {}
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

	VkPhysicalDevice m_physicalDevice{};
	VkDevice m_logicalDevice{};
	VkSurfaceKHR surface{};
	VkSurfaceFormatKHR m_surfaceFormat;
	VkSwapchainKHR swapchain{};
	VkExtent2D swapchainExtend{};
	VkFence m_imageAvailableFence{};
	uint32 swapchainImageIndex = (uint32)-1;
	uint32 m_acquireIndex = 0; // increases with every successful vkAcquireNextImageKHR
	VSync m_vsyncState = VSync::Immediate;


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
