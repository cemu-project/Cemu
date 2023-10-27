#pragma once
#include "VsyncDriver.h"
#include "HW/Latte/Renderer/Vulkan/VulkanAPI.h"

class VsyncDriverVulkan : public VsyncDriver {
  public:
	VsyncDriverVulkan(void (*cbVSync)())
		: VsyncDriver(cbVSync),
		  device(device),
		  swapchain(swapchain)
	{startThread();}

  public:
	void QueueWait(VkFence waitFence);
	void EmptyQueue();

	void SetDeviceAndSwapchain(VkDevice device, VkSwapchainKHR swapchain);

  private:
	VkFence GetWaitFence();
	void vsyncThread();

	std::mutex fenceMutex;
	std::condition_variable newWork;
	VkFence waitFence = VK_NULL_HANDLE;

	std::mutex deviceSwapchainMutex;
	VkDevice device;
	VkSwapchainKHR swapchain;
};