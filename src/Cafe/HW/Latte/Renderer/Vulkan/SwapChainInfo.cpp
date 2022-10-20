#include "SwapChainInfo.h"

void SwapChainInfo::Cleanup(bool semaphores)
{
	m_swapchainImages.clear();

	if(semaphores)
	{
		std::cout << "destroying semaphores" << std::endl;
		for (auto& sem: m_renderFinishedSemaphores)
			vkDestroySemaphore(m_device, sem, nullptr);
		m_renderFinishedSemaphores.clear();

		std::cout << "destroying semaphores" << std::endl;
		for (auto& itr: m_imageAvailableSemaphores)
			vkDestroySemaphore(m_device, itr, nullptr);
		m_imageAvailableSemaphores.clear();
	}

	if (m_swapChainRenderPass)
	{
		vkDestroyRenderPass(m_device, m_swapChainRenderPass, nullptr);
		m_swapChainRenderPass = nullptr;
	}

	for (auto& imageView : m_swapchainImageViews)
		vkDestroyImageView(m_device, imageView, nullptr);
	m_swapchainImageViews.clear();

	for (auto& framebuffer : m_swapchainFramebuffers)
		vkDestroyFramebuffer(m_device, framebuffer, nullptr);
	m_swapchainFramebuffers.clear();

	if (swapChain)
	{
		vkDestroySwapchainKHR(m_device, swapChain, nullptr);
		swapChain = VK_NULL_HANDLE;
	}
}

bool SwapChainInfo::Ready()
{
	return surface && swapChain;
}
