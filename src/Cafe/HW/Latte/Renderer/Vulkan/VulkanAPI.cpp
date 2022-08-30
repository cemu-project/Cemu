#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
#define VKFUNC_DEFINE
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"

#if BOOST_OS_LINUX || BOOST_OS_MACOS
#include <dlfcn.h>
#endif

bool g_vulkan_available = false;

#if BOOST_OS_WINDOWS

bool InitializeGlobalVulkan()
{
	const auto hmodule = LoadLibraryA("vulkan-1.dll");

	if(g_vulkan_available)
		return true;

	if (hmodule == nullptr)
	{
		forceLog_printf("Vulkan loader not available. Outdated graphics driver or Vulkan runtime not installed?");
		return false;
	}

	#define VKFUNC_INIT
	#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"

	if(!vkEnumerateInstanceVersion)
	{
		forceLog_printf("vkEnumerateInstanceVersion not available. Outdated graphics driver or Vulkan runtime?");
		FreeLibrary(hmodule);
		return false;
	}
	
	g_vulkan_available = true;
	return true;
}

bool InitializeInstanceVulkan(VkInstance instance)
{
	const auto hmodule = GetModuleHandleA("vulkan-1.dll");
	if (hmodule == nullptr)
		return false;

	#define VKFUNC_INSTANCE_INIT
	#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
	
	return true;
}

bool InitializeDeviceVulkan(VkDevice device)
{
	const auto hmodule = GetModuleHandleA("vulkan-1.dll");
	if (hmodule == nullptr)
		return false;

	#define VKFUNC_DEVICE_INIT
	#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
	
	return true;
}

#else

void* dlopen_vulkan_loader()
{
#if BOOST_OS_LINUX
	void* vulkan_so = dlopen("libvulkan.so", RTLD_NOW);
	if(!vulkan_so)
		vulkan_so = dlopen("libvulkan.so.1", RTLD_NOW);
#elif BOOST_OS_MACOS
	void* vulkan_so = dlopen("libMoltenVK.dylib", RTLD_NOW);
#endif
	return vulkan_so;
}

bool InitializeGlobalVulkan()
{
	void* vulkan_so = dlopen_vulkan_loader();

	if(g_vulkan_available)
		return true;

	if (!vulkan_so)
	{
		forceLog_printf("Vulkan loader not available.");
		return false;
	}

	#define VKFUNC_INIT
	#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"

	if(!vkEnumerateInstanceVersion)
	{
		forceLog_printf("vkEnumerateInstanceVersion not available. Outdated graphics driver or Vulkan runtime?");
		return false;
	}
	
	g_vulkan_available = true;
	return true;
}

bool InitializeInstanceVulkan(VkInstance instance)
{
	void* vulkan_so = dlopen_vulkan_loader();
	if (!vulkan_so)
		return false;

	#define VKFUNC_INSTANCE_INIT
	#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
	
	return true;
}

bool InitializeDeviceVulkan(VkDevice device)
{
	void* vulkan_so = dlopen_vulkan_loader();
	if (!vulkan_so)
		return false;

	#define VKFUNC_DEVICE_INIT
	#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
	
	return true;
}

#endif