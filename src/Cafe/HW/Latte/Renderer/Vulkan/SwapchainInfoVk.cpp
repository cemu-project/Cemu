#include "SwapchainInfoVk.h"

void SwapchainInfoVk::Cleanup()
{
	m_swapchainImages.clear();

	for (auto& sem: m_swapchainPresentSemaphores)
		vkDestroySemaphore(m_device, sem, nullptr);
	m_swapchainPresentSemaphores.clear();

	for (auto& itr: m_acquireSemaphores)
		vkDestroySemaphore(m_device, itr, nullptr);
	m_acquireSemaphores.clear();

	if (m_swapchainRenderPass)
	{
		vkDestroyRenderPass(m_device, m_swapchainRenderPass, nullptr);
		m_swapchainRenderPass = nullptr;
	}

	for (auto& imageView : m_swapchainImageViews)
		vkDestroyImageView(m_device, imageView, nullptr);
	m_swapchainImageViews.clear();

	for (auto& framebuffer : m_swapchainFramebuffers)
		vkDestroyFramebuffer(m_device, framebuffer, nullptr);
	m_swapchainFramebuffers.clear();


	if (m_imageAvailableFence)
	{
		vkDestroyFence(m_device, m_imageAvailableFence, nullptr);
		m_imageAvailableFence = nullptr;
	}
	if (swapchain)
	{
		vkDestroySwapchainKHR(m_device, swapchain, nullptr);
		swapchain = VK_NULL_HANDLE;
	}
}
