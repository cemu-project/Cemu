#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
#define VKFUNC_DEFINE
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
#include <numeric> // for std::iota

#if BOOST_OS_LINUX || BOOST_OS_MACOS
#include <dlfcn.h>
#endif

#define VULKAN_API_CPU_BENCHMARK 0	// if 1, Cemu will log the CPU time spent per Vulkan API function

bool g_vulkan_available = false;

#if VULKAN_API_CPU_BENCHMARK != 0
uint64 s_vulkanBenchmarkLastResultsTime = 0;

struct VulkanBenchmarkFuncInfo
{
	std::string funcName;
	uint64 cycles;
	uint32 numCalls;
};

std::vector<VulkanBenchmarkFuncInfo*> s_vulkanBenchmarkFuncs;

template<typename TRet, typename... Args>
auto VkWrapperFuncGenTest(TRet (*func)(Args...), const char* name)
{
	static VulkanBenchmarkFuncInfo _FuncInfo;
	static auto _FuncPtrCopy = func;
	TRet (*newFunc)(Args...);
	if constexpr(std::is_void_v<TRet>)
	{
		newFunc = +[](Args... args) { uint64 t = __rdtsc(); _mm_mfence(); _FuncPtrCopy(args...); _mm_mfence(); _FuncInfo.cycles += (__rdtsc() - t); _FuncInfo.numCalls++; };
	}
	else
		newFunc = +[](Args... args) -> TRet { uint64 t = __rdtsc(); _mm_mfence(); TRet r = _FuncPtrCopy(args...); _mm_mfence(); _FuncInfo.cycles += (__rdtsc() - t); _FuncInfo.numCalls++; return r; };
	if(func && func != newFunc)
		_FuncPtrCopy = func;
	if(_FuncInfo.funcName.empty())
	{
		_FuncInfo = {.funcName = name, .cycles = 0, .numCalls = 0};
		s_vulkanBenchmarkFuncs.emplace_back(&_FuncInfo);
	}
	return newFunc;
};
#endif

// called when a TV SwapBuffers is called
void VulkanBenchmarkPrintResults()
{
#if VULKAN_API_CPU_BENCHMARK != 0
	// note: This could be done by hooking vk present functions
	uint64 currentCycle = __rdtsc();
	uint64 elapsedCycles = currentCycle - s_vulkanBenchmarkLastResultsTime;
	s_vulkanBenchmarkLastResultsTime = currentCycle;
	double elapsedCyclesDbl = (double)elapsedCycles;
	cemuLog_log(LogType::Force, "--- Vulkan API CPU benchmark ---");
	cemuLog_log(LogType::Force, "Elapsed cycles this frame: {:} | Current cycle {:} | NumFunc {:}", elapsedCycles, currentCycle, s_vulkanBenchmarkFuncs.size());

	std::vector<sint32> sortedIndices(s_vulkanBenchmarkFuncs.size());
	std::iota(sortedIndices.begin(), sortedIndices.end(), 0);
	std::sort(sortedIndices.begin(), sortedIndices.end(),
			  [](int32_t a, int32_t b) {
				  return s_vulkanBenchmarkFuncs[a]->cycles > s_vulkanBenchmarkFuncs[b]->cycles;
			  });
	for (sint32 idx : sortedIndices)
	{
		auto& func = s_vulkanBenchmarkFuncs[idx];
		if(func->cycles == 0)
			return;
		cemuLog_log(LogType::Force, "{}: {} cycles ({:.4}%) {} calls", func->funcName.c_str(), func->cycles, ((double)func->cycles / elapsedCyclesDbl) * 100.0, func->numCalls);
		func->cycles = 0;
		func->numCalls = 0;
	}
#endif
}

#if BOOST_OS_WINDOWS

bool InitializeGlobalVulkan()
{
	const auto hmodule = LoadLibraryA("vulkan-1.dll");

	if(g_vulkan_available)
		return true;

	if (hmodule == nullptr)
	{
		cemuLog_log(LogType::Force, "Vulkan loader not available. Outdated graphics driver or Vulkan runtime not installed?");
		return false;
	}

	#define VKFUNC_INIT
	#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"

	if(!vkEnumerateInstanceVersion)
	{
		cemuLog_log(LogType::Force, "vkEnumerateInstanceVersion not available. Outdated graphics driver or Vulkan runtime?");
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

#if VULKAN_API_CPU_BENCHMARK != 0
	#define VKFUNC_DEFINE_CUSTOM(__func) __func = VkWrapperFuncGenTest(__func, #__func)
	#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
#endif

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
		cemuLog_log(LogType::Force, "Vulkan loader not available.");
		return false;
	}

	#define VKFUNC_INIT
	#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"

	if(!vkEnumerateInstanceVersion)
	{
		cemuLog_log(LogType::Force, "vkEnumerateInstanceVersion not available. Outdated graphics driver or Vulkan runtime?");
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

#if VULKAN_API_CPU_BENCHMARK != 0
	#define VKFUNC_DEFINE_CUSTOM(__func) __func = VkWrapperFuncGenTest(__func, #__func)
	#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
#endif

	return true;
}

#endif
