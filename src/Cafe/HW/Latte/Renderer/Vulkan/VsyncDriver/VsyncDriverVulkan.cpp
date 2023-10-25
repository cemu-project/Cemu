#include "VsyncDriverVulkan.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"

void VsyncDriverVulkan::PushPresentID(uint64_t presentID)
{
	std::lock_guard lock(queueMutex);
	frameQueue.emplace(presentID);
	newWork.notify_all();
}
void VsyncDriverVulkan::EmptyQueue()
{
	std::lock_guard lock(queueMutex);
	frameQueue = {};
}

uint64_t VsyncDriverVulkan::PopPresentID()
{
	using std::chrono_literals::operator""ns;
	std::unique_lock lock(queueMutex);
	while(frameQueue.empty()){
		auto waitSuccess = newWork.wait_for(lock, 10 * 16'666'666ns, [this](){return !frameQueue.empty();});
		if(!waitSuccess)
			signalVsync();
	}


	uint64_t ret = frameQueue.front();
	frameQueue.pop();
	return ret;
}

void VsyncDriverVulkan::SetDeviceAndSwapchain(VkDevice device, VkSwapchainKHR swapchain)
{
	std::lock_guard lock(deviceSwapchainMutex);
	EmptyQueue();
	this->device = device;
	this->swapchain = swapchain;
}

void VsyncDriverVulkan::vsyncThread()
{
	std::cout << "thread was started" << std::endl;
	while (!m_thd.get_stop_token().stop_requested())
	{
		auto presentId = PopPresentID();
		std::lock_guard lock(deviceSwapchainMutex);
		vkWaitForPresentKHR(device, swapchain, presentId, 10 * 16'666'666);
		signalVsync();
	}
}