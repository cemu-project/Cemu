#include "HW/Latte/Renderer/Vulkan/VulkanRenderer.h"
#include "HW/Latte/Renderer/Vulkan/VulkanAPI.h"
#include "HW/Latte/Renderer/Renderer.h"
#if BOOST_OS_WINDOWS
#include "VsyncDriverDXGI.h"
#else
#include "VsyncDriverVulkan.h"
#endif
#include "VsyncDriver.h"

VsyncDriver::VsyncDriver(void (*cbVSync)())
	: m_vsyncDriverVSyncCb(cbVSync)
{}

void VsyncDriver::startThread()
{

	m_thd = std::jthread(&VsyncDriver::vsyncThread, this);
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
	{
#if BOOST_OS_WINDOWS
		g_vsyncDriver = std::make_unique<VsyncDriverDXGI>(cbVSync);
#else
		g_vsyncDriver = std::make_unique<VsyncDriverVulkan>(cbVSync);
#endif
	}
}

void VsyncDriver_notifyWindowPosChanged()
{
}
