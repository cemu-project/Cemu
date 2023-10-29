#pragma once
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"

class VsyncDriver {
  public:
	VsyncDriver(void (*cbVSync)())
		: m_vsyncDriverVSyncCb(cbVSync),
		  device(device),
		  swapchain(swapchain), m_thd{&VsyncDriver::vsyncThread, this}
	{}

	void PushPresentID(uint64_t presentID);
	void EmptyQueue();
	void SetDeviceAndSwapchain(VkDevice device, VkSwapchainKHR swapchain);

  private:
	void vsyncThread();
	void signalVsync();
	void setCallback(void (*cbVSync)());

	uint64_t PopPresentID();

	std::mutex queueMutex;
	std::condition_variable newWork;
	std::queue<uint64_t> frameQueue;

	std::mutex deviceSwapchainMutex;
	VkDevice device;
	VkSwapchainKHR swapchain;

	std::jthread m_thd;
	void (*m_vsyncDriverVSyncCb)() = nullptr;
};
class VsyncDriverDXGI;

extern std::unique_ptr<VsyncDriver> g_vsyncDriver;

void VsyncDriver_startThread(void(*cbVSync)());