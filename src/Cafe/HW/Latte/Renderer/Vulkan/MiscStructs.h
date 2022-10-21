#pragma once

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
