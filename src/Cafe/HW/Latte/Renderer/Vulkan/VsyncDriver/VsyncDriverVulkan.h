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
	void PushPresentID(uint64_t presentID);
	void EmptyQueue();

	void SetDeviceAndSwapchain(VkDevice device, VkSwapchainKHR swapchain);

  private:
	uint64_t PopPresentID();
	void vsyncThread();

	std::mutex queueMutex;
	std::condition_variable newWork;
	std::queue<uint64_t> frameQueue;

	std::mutex deviceSwapchainMutex;
	VkDevice device;
	VkSwapchainKHR swapchain;
};