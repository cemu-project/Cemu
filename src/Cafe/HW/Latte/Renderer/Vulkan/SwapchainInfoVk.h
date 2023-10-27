#pragma once

#include "util/math/vector2.h"
#include "VKRMemoryManager.h"
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

	struct FBImage {
		VkImage image;
		VkImageView view;
		VkFramebuffer frameBuffer;
		VkImageMemAllocation* alloc = nullptr;
	};

	void Cleanup();
	void Create(VkPhysicalDevice physicalDevice, VkDevice logicalDevice);

	bool IsValid() const;

	void WaitAvailableFence();
	void ResetAvailableFence() const;

	bool AcquireImage(uint64 timeout);
	// retrieve semaphore of last acquire for submitting a wait operation
	// only one wait operation must be submitted per acquire (which submits a single signal operation)
	// therefore subsequent calls will return a NULL handle
	VkSemaphore ConsumeAcquireSemaphore();

	static void UnrecoverableError(const char* errMsg);

	// todo: move this function somewhere more sensible. Not directly swapchain related
	static QueueFamilyIndices FindQueueFamilies(VkSurfaceKHR surface, VkPhysicalDevice device);
	static SwapchainSupportDetails QuerySwapchainSupport(VkSurfaceKHR surface, const VkPhysicalDevice& device);

	VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes);
	VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;

	VkSwapchainCreateInfoKHR CreateSwapchainCreateInfo(VkSurfaceKHR surface, const SwapchainSupportDetails& swapchainSupport, const VkSurfaceFormatKHR& surfaceFormat, uint32 imageCount, const VkExtent2D& extent);

	void CreateImageFromSwapchain(VkImage* image, VkSwapchainCreateInfoKHR swapchainCreateInfo);

	VkExtent2D getExtent() const
	{
		return m_actualExtent;
	}

	FBImage& GetBackBuffer();
	void SwapBuffers();

	SwapchainInfoVk(VkSurfaceKHR surface, bool mainWindow)
		: surface(surface), mainWindow(mainWindow) {}
	SwapchainInfoVk(const SwapchainInfoVk&) = delete;
	SwapchainInfoVk(SwapchainInfoVk&&) noexcept = default;
	~SwapchainInfoVk()
	{
		Cleanup();
	}

	bool mainWindow{};

	bool m_shouldRecreate = false;
	bool m_usesSRGB = false;
	VSync m_vsyncState = VSync::Immediate;
	bool hasDefinedBackBuffer{}; // indicates if the swapchain image is in a defined state

	VkPhysicalDevice m_physicalDevice{};
	VkDevice m_logicalDevice{};
	VkSurfaceKHR surface{};
	VkSurfaceFormatKHR m_surfaceFormat{};
	VkSwapchainKHR swapchain{};
	Vector2i m_desiredExtent{};
	uint32 swapchainImageIndex = (uint32)-1;
	uint64 m_presentId = 1;

	// swapchain image ringbuffer (indexed by swapchainImageIndex)
	std::vector<VkImage> m_swapchainImages;
	std::vector<VkSemaphore> m_presentSemaphores; // indexed by swapchainImageIndex


	std::array<FBImage, 2> m_images;
	FBImage* frontBuffer = &m_images[0];
	FBImage* backBuffer = &m_images[1];

	VkRenderPass m_swapchainRenderPass = nullptr;

private:
	uint32 m_acquireIndex = 0;
	std::vector<VkSemaphore> m_acquireSemaphores; // indexed by m_acquireIndex
	VkFence m_imageAvailableFence{};
	VkSemaphore m_currentSemaphore = VK_NULL_HANDLE;
	VkFence m_awaitableFence = VK_NULL_HANDLE;

	std::array<uint32, 2> m_swapchainQueueFamilyIndices;
	VkExtent2D m_actualExtent{};
};
