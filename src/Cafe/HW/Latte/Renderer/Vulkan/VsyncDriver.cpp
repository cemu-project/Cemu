#include "HW/Latte/Renderer/Vulkan/VulkanRenderer.h"
#include "HW/Latte/Renderer/Vulkan/VulkanAPI.h"
#include "HW/Latte/Renderer/Renderer.h"
#include "HW/Latte/Renderer/Vulkan/VsyncDriver.h"

void VsyncDriver::PushPresentID(uint64_t presentID)
{
	std::lock_guard lock(queueMutex);
	frameQueue.emplace(presentID);
	newWork.notify_all();
}
void VsyncDriver::EmptyQueue()
{
	std::lock_guard lock(queueMutex);
	frameQueue = {};
}

uint64_t VsyncDriver::PopPresentID()
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

void VsyncDriver::SetDeviceAndSwapchain(VkDevice device, VkSwapchainKHR swapchain)
{
	std::lock_guard lock(deviceSwapchainMutex);
	EmptyQueue();
	this->device = device;
	this->swapchain = swapchain;
}

void VsyncDriver::vsyncThread()
{
	std::cout << "thread was started" << std::endl;
	if(!vkWaitForPresentKHR)
		return;
	while (!m_thd.get_stop_token().stop_requested())
	{
		auto presentId = PopPresentID();
		{
			std::lock_guard lock(deviceSwapchainMutex);
			vkWaitForPresentKHR(device, swapchain, presentId, 10 * 16'666'666);
		}
		signalVsync();
	}
}

void VsyncDriver::signalVsync()
{
	if (m_vsyncDriverVSyncCb)
		m_vsyncDriverVSyncCb();
}

void VsyncDriver::setCallback(void (*cbVSync)())
{
	m_vsyncDriverVSyncCb = cbVSync;
}

std::unique_ptr<VsyncDriver> g_vsyncDriver;

void VsyncDriver_startThread(void (*cbVSync)())
{
	if (!g_vsyncDriver)
		g_vsyncDriver = std::make_unique<VsyncDriver>(cbVSync);
}