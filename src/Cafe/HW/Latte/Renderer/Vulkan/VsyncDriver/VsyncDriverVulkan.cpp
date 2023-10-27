#include "VsyncDriverVulkan.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"

void VsyncDriverVulkan::QueueWait(VkFence waitFence)
{
	std::lock_guard lock(fenceMutex);
	this->waitFence = waitFence;
	newWork.notify_all();
}
void VsyncDriverVulkan::EmptyQueue()
{
	std::unique_lock lock(fenceMutex);
	while(waitFence != VK_NULL_HANDLE)
	{
		lock.unlock();
		newWork.notify_all();
		lock.lock();
	}
}

VkFence VsyncDriverVulkan::GetWaitFence()
{
	using std::chrono_literals::operator""ns;
	std::unique_lock lock(fenceMutex);
	while(waitFence == VK_NULL_HANDLE){
		auto waitSuccess = newWork.wait_for(lock, 10 * 16'666'666ns, [this](){return waitFence != VK_NULL_HANDLE;});
		if(!waitSuccess)
			signalVsync();
	}

	VkFence ret = waitFence;
	waitFence = VK_NULL_HANDLE;
	return ret;
}

void VsyncDriverVulkan::SetDeviceAndSwapchain(VkDevice device, VkSwapchainKHR swapchain)
{
	std::lock_guard lock(deviceSwapchainMutex);
	this->device = device;
	this->swapchain = swapchain;
}

void VsyncDriverVulkan::vsyncThread()
{
	std::cout << "thread was started" << std::endl;
	while (!m_thd.get_stop_token().stop_requested())
	{
		auto fence = GetWaitFence();
		{
			vkWaitForFences(device, 1, &fence, true, 10 * 16'666'666);
		}
		signalVsync();
	}
}