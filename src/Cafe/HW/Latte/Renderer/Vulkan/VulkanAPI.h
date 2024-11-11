#ifndef _VULKAN_API_
#define _VULKAN_API_

#if BOOST_OS_WINDOWS
#define VK_USE_PLATFORM_WIN32_KHR // todo - define in CMakeLists.txt
#endif

#include <vulkan/vulkan.h>

bool InitializeGlobalVulkan();
bool InitializeInstanceVulkan(VkInstance instance);
bool InitializeDeviceVulkan(VkDevice device);
extern bool g_vulkan_available;

#endif

#ifdef VKFUNC_DEFINE_CUSTOM
	#define VKFUNC(__FUNC__) VKFUNC_DEFINE_CUSTOM(__FUNC__)
	#define VKFUNC_INSTANCE(__FUNC__) VKFUNC_DEFINE_CUSTOM(__FUNC__)
	#define VKFUNC_DEVICE(__FUNC__) VKFUNC_DEFINE_CUSTOM(__FUNC__)
#elif defined(VKFUNC_DEFINE)
	#define VKFUNC(__FUNC__) NOEXPORT PFN_##__FUNC__ __FUNC__ = nullptr
	#define VKFUNC_INSTANCE(__FUNC__) NOEXPORT PFN_##__FUNC__ __FUNC__ = nullptr
	#define VKFUNC_DEVICE(__FUNC__) NOEXPORT PFN_##__FUNC__ __FUNC__ = nullptr
#else
	#if defined(VKFUNC_INIT)
		#if BOOST_OS_WINDOWS
		#define VKFUNC(__FUNC__) __FUNC__ = (PFN_##__FUNC__)GetProcAddress(hmodule, #__FUNC__)
		#else
		#define VKFUNC(__FUNC__) __FUNC__ = (PFN_##__FUNC__)dlsym(vulkan_so, #__FUNC__)
		#endif
		#define VKFUNC_INSTANCE(__FUNC__)
		#define VKFUNC_DEVICE(__FUNC__)
	#elif defined(VKFUNC_INSTANCE_INIT)
		#define VKFUNC(__FUNC__) 
		#define VKFUNC_INSTANCE(__FUNC__) __FUNC__ = (PFN_##__FUNC__)vkGetInstanceProcAddr(instance, #__FUNC__)
		#define VKFUNC_DEVICE(__FUNC__)
	#elif defined(VKFUNC_DEVICE_INIT)
		#define VKFUNC(__FUNC__) 
		#define VKFUNC_INSTANCE(__FUNC__)
		#define VKFUNC_DEVICE(__FUNC__) __FUNC__ = (PFN_##__FUNC__)vkGetDeviceProcAddr(device, #__FUNC__)
	#else
		#define VKFUNC(__FUNC__) extern PFN_##__FUNC__ __FUNC__
		#define VKFUNC_INSTANCE(__FUNC__) extern PFN_##__FUNC__ __FUNC__
		#define VKFUNC_DEVICE(__FUNC__) extern PFN_##__FUNC__ __FUNC__
	#endif
#endif



// global functions
VKFUNC(vkGetInstanceProcAddr);
VKFUNC(vkCreateInstance);
VKFUNC(vkGetDeviceProcAddr);
VKFUNC(vkEnumerateInstanceExtensionProperties);
VKFUNC(vkEnumerateDeviceExtensionProperties);
VKFUNC(vkEnumerateInstanceVersion);

// instance functions
VKFUNC_INSTANCE(vkDestroyInstance);
VKFUNC_INSTANCE(vkEnumeratePhysicalDevices);
VKFUNC_INSTANCE(vkCreateDevice);
VKFUNC_INSTANCE(vkDestroyDevice);
VKFUNC_INSTANCE(vkDeviceWaitIdle);

// device debug functions

// instance debug functions
VKFUNC_INSTANCE(vkCreateDebugReportCallbackEXT);

VKFUNC_INSTANCE(vkGetPhysicalDeviceToolPropertiesEXT);
VKFUNC_INSTANCE(vkSetDebugUtilsObjectNameEXT);

//VKFUNC_INSTANCE(vkCreateDebugReportCallbackEXT);

// getters
VKFUNC_DEVICE(vkGetDeviceQueue);
VKFUNC_INSTANCE(vkGetPhysicalDeviceQueueFamilyProperties);
VKFUNC_INSTANCE(vkGetPhysicalDeviceSurfaceSupportKHR);
VKFUNC_INSTANCE(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
VKFUNC_INSTANCE(vkGetPhysicalDeviceSurfaceFormatsKHR);
VKFUNC_INSTANCE(vkGetPhysicalDeviceSurfacePresentModesKHR);
VKFUNC_INSTANCE(vkGetPhysicalDeviceMemoryProperties);
VKFUNC_INSTANCE(vkGetPhysicalDeviceProperties);
VKFUNC_INSTANCE(vkGetPhysicalDeviceProperties2);
VKFUNC_INSTANCE(vkGetPhysicalDeviceFeatures2);
VKFUNC_INSTANCE(vkGetPhysicalDeviceFormatProperties);

// semaphore
VKFUNC_DEVICE(vkCreateSemaphore);
VKFUNC_DEVICE(vkDestroySemaphore);

// imageview
VKFUNC_DEVICE(vkCreateImageView);
VKFUNC_DEVICE(vkDestroyImageView);

// shader
VKFUNC_DEVICE(vkCreateShaderModule);
VKFUNC_DEVICE(vkDestroyShaderModule);

// framebuffer
VKFUNC_DEVICE(vkCreateFramebuffer);
VKFUNC_DEVICE(vkDestroyFramebuffer);

// renderpass
VKFUNC_DEVICE(vkCreateRenderPass);
VKFUNC_DEVICE(vkDestroyRenderPass);
VKFUNC_DEVICE(vkCmdBeginRenderPass);
VKFUNC_DEVICE(vkCmdEndRenderPass);

// command buffers
VKFUNC_DEVICE(vkCreateCommandPool);
VKFUNC_DEVICE(vkDestroyCommandPool);
VKFUNC_DEVICE(vkAllocateCommandBuffers);
VKFUNC_DEVICE(vkFreeCommandBuffers);
VKFUNC_DEVICE(vkBeginCommandBuffer);
VKFUNC_DEVICE(vkResetCommandBuffer);
VKFUNC_DEVICE(vkEndCommandBuffer);
VKFUNC_DEVICE(vkQueueSubmit);

// pipeline
VKFUNC_DEVICE(vkCreatePipelineCache);
VKFUNC_DEVICE(vkMergePipelineCaches);
VKFUNC_DEVICE(vkGetPipelineCacheData);
VKFUNC_DEVICE(vkDestroyPipelineCache);
VKFUNC_DEVICE(vkCreatePipelineLayout);
VKFUNC_DEVICE(vkDestroyPipelineLayout);
VKFUNC_DEVICE(vkCreateGraphicsPipelines);
VKFUNC_DEVICE(vkDestroyPipeline);
VKFUNC_DEVICE(vkCmdBindPipeline);

// swapchain
#if BOOST_OS_LINUX
VKFUNC_INSTANCE(vkCreateXlibSurfaceKHR);
VKFUNC_INSTANCE(vkCreateXcbSurfaceKHR);
#ifdef HAS_WAYLAND
VKFUNC_INSTANCE(vkCreateWaylandSurfaceKHR);
#endif
#endif

#if BOOST_OS_WINDOWS
VKFUNC_INSTANCE(vkCreateWin32SurfaceKHR);
#endif

#if BOOST_OS_MACOS
VKFUNC_INSTANCE(vkCreateMetalSurfaceEXT);
#endif

VKFUNC_INSTANCE(vkDestroySurfaceKHR);
VKFUNC_DEVICE(vkCreateSwapchainKHR);
VKFUNC_DEVICE(vkDestroySwapchainKHR);
VKFUNC_DEVICE(vkGetSwapchainImagesKHR);
VKFUNC_DEVICE(vkAcquireNextImageKHR);
VKFUNC_INSTANCE(vkQueuePresentKHR);

// fences
VKFUNC_DEVICE(vkCreateFence);
VKFUNC_DEVICE(vkWaitForFences);
VKFUNC_DEVICE(vkGetFenceStatus);
VKFUNC_DEVICE(vkResetFences);
VKFUNC_DEVICE(vkDestroyFence);

// cmd
VKFUNC_DEVICE(vkCmdDraw);
VKFUNC_DEVICE(vkCmdCopyBufferToImage);
VKFUNC_DEVICE(vkCmdCopyImageToBuffer);
VKFUNC_DEVICE(vkCmdClearColorImage);
VKFUNC_DEVICE(vkCmdBindIndexBuffer);
VKFUNC_DEVICE(vkCmdBindVertexBuffers);
VKFUNC_DEVICE(vkCmdDrawIndexed);
VKFUNC_DEVICE(vkCmdSetViewport);
VKFUNC_DEVICE(vkCmdSetScissor);
VKFUNC_DEVICE(vkCmdBindDescriptorSets);
VKFUNC_DEVICE(vkCmdPipelineBarrier);
VKFUNC_DEVICE(vkCmdClearDepthStencilImage);
VKFUNC_DEVICE(vkCmdCopyBuffer);
VKFUNC_DEVICE(vkCmdCopyImage);
VKFUNC_DEVICE(vkCmdBlitImage);
VKFUNC_DEVICE(vkCmdPushConstants);
VKFUNC_DEVICE(vkCmdSetBlendConstants);
VKFUNC_DEVICE(vkCmdSetDepthBias);

// synchronization2

VKFUNC_DEVICE(vkCmdPipelineBarrier2KHR);

// khr_dynamic_rendering
VKFUNC_DEVICE(vkCmdBeginRenderingKHR);
VKFUNC_DEVICE(vkCmdEndRenderingKHR);

// khr_present_wait
VKFUNC_DEVICE(vkWaitForPresentKHR);

// transform feedback extension
VKFUNC_DEVICE(vkCmdBindTransformFeedbackBuffersEXT);
VKFUNC_DEVICE(vkCmdBeginTransformFeedbackEXT);
VKFUNC_DEVICE(vkCmdEndTransformFeedbackEXT);

// query
VKFUNC_DEVICE(vkCreateQueryPool);
VKFUNC_DEVICE(vkCmdResetQueryPool);
VKFUNC_DEVICE(vkCmdBeginQuery);
VKFUNC_DEVICE(vkCmdEndQuery);
VKFUNC_DEVICE(vkCmdCopyQueryPoolResults);

// event
VKFUNC_DEVICE(vkCreateEvent);
VKFUNC_DEVICE(vkCmdSetEvent);
VKFUNC_DEVICE(vkCmdWaitEvents);
VKFUNC_DEVICE(vkGetEventStatus);
VKFUNC_DEVICE(vkDestroyEvent);

// memory
VKFUNC_DEVICE(vkAllocateMemory);
VKFUNC_DEVICE(vkFreeMemory);
VKFUNC_DEVICE(vkCreateBuffer);
VKFUNC_DEVICE(vkDestroyBuffer);
VKFUNC_DEVICE(vkBindBufferMemory);
VKFUNC_DEVICE(vkMapMemory);
VKFUNC_DEVICE(vkUnmapMemory);
VKFUNC_DEVICE(vkGetBufferMemoryRequirements);
VKFUNC_DEVICE(vkFlushMappedMemoryRanges);
VKFUNC_DEVICE(vkInvalidateMappedMemoryRanges);

// image
VKFUNC_DEVICE(vkCreateImage);
VKFUNC_DEVICE(vkDestroyImage);
VKFUNC_DEVICE(vkGetImageMemoryRequirements);
VKFUNC_DEVICE(vkBindImageMemory);

VKFUNC_DEVICE(vkCreateSampler);
VKFUNC_DEVICE(vkDestroySampler);

VKFUNC_DEVICE(vkCreateDescriptorSetLayout);
VKFUNC_DEVICE(vkAllocateDescriptorSets);
VKFUNC_DEVICE(vkFreeDescriptorSets);
VKFUNC_DEVICE(vkUpdateDescriptorSets);
VKFUNC_DEVICE(vkCreateDescriptorPool);
VKFUNC_DEVICE(vkDestroyDescriptorSetLayout);

#undef VKFUNC_INIT
#undef VKFUNC_INSTANCE_INIT
#undef VKFUNC_DEVICE_INIT
#undef VKFUNC_DEFINE

#undef VKFUNC
#undef VKFUNC_INSTANCE
#undef VKFUNC_DEVICE
#undef VKFUNC_DEFINE_CUSTOM
