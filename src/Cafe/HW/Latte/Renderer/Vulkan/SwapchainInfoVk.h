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

	struct SwapchainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	void Cleanup();
	void Create();

	bool IsValid() const;

	bool AcquireImage();
	// retrieve semaphore of last acquire for submitting a wait operation
	// only one wait operation must be submitted per acquire (which submits a single signal operation)
	// therefore subsequent calls will return a NULL handle
	VkSemaphore ConsumeAcquireSemaphore();

	static void UnrecoverableError(const char* errMsg);

	static SwapchainSupportDetails QuerySwapchainSupport(VkSurfaceKHR surface, const VkPhysicalDevice& device);

	VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes);
	VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;

	VkSwapchainCreateInfoKHR CreateSwapchainCreateInfo(VkSurfaceKHR surface, const SwapchainSupportDetails& swapchainSupport, const VkSurfaceFormatKHR& surfaceFormat, uint32 imageCount, const VkExtent2D& extent);


	VkExtent2D getExtent() const
	{
		return m_actualExtent;
	}

	SwapchainInfoVk(bool mainWindow, Vector2i size);
	SwapchainInfoVk(const SwapchainInfoVk&) = delete;
	SwapchainInfoVk(SwapchainInfoVk&&) noexcept = default;
	~SwapchainInfoVk();

	bool mainWindow{};

	bool m_shouldRecreate = false;
	bool m_usesSRGB = false;
	VSync m_vsyncState = VSync::Immediate;
	bool hasDefinedSwapchainImage{}; // indicates if the swapchain image is in a defined state

	VkInstance m_instance{};
	VkPhysicalDevice m_physicalDevice{};
	VkDevice m_logicalDevice{};
	VkSurfaceKHR m_surface{};
	VkSurfaceFormatKHR m_surfaceFormat{};
	VkSwapchainKHR m_swapchain{};
	Vector2i m_desiredExtent{};
	uint32 swapchainImageIndex = (uint32)-1;


	// swapchain image ringbuffer (indexed by swapchainImageIndex)
	std::vector<VkImage> m_swapchainImages;
	std::vector<VkImageView> m_swapchainImageViews;
	std::vector<VkFramebuffer> m_swapchainFramebuffers;
	std::vector<VkSemaphore> m_presentSemaphores; // indexed by swapchainImageIndex

	VkRenderPass m_swapchainRenderPass = nullptr;

private:
	uint32 m_acquireIndex = 0;
	std::vector<VkSemaphore> m_acquireSemaphores; // indexed by m_acquireIndex
	VkSemaphore m_currentSemaphore = VK_NULL_HANDLE;

	std::array<uint32, 2> m_swapchainQueueFamilyIndices;
	VkExtent2D m_actualExtent{};
};
