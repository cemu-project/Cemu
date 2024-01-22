#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanRenderer.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/LatteTextureVk.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/RendererShaderVk.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanTextureReadback.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/CocoaSurface.h"

#include "Cafe/HW/Latte/Core/LatteBufferCache.h"
#include "Cafe/HW/Latte/Core/LattePerformanceMonitor.h"

#include "Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompiler.h"

#include "Cafe/CafeSystem.h"

#include "util/helpers/helpers.h"
#include "util/helpers/StringHelpers.h"

#include "config/ActiveSettings.h"
#include "config/CemuConfig.h"
#include "gui/guiWrapper.h"

#include "imgui/imgui_extension.h"
#include "imgui/imgui_impl_vulkan.h"

#include "Cafe/TitleList/GameInfo.h"

#include "Cafe/HW/Latte/Core/LatteTiming.h" // vsync control

#include <glslang/Public/ShaderLang.h>

#include <wx/msgdlg.h>

#ifndef VK_API_VERSION_MAJOR
#define VK_API_VERSION_MAJOR(version) (((uint32_t)(version) >> 22) & 0x7FU)
#define VK_API_VERSION_MINOR(version) (((uint32_t)(version) >> 12) & 0x3FFU)
#endif

extern std::atomic_int g_compiling_pipelines;

const  std::vector<const char*> kOptionalDeviceExtensions =
{
	VK_EXT_DEPTH_RANGE_UNRESTRICTED_EXTENSION_NAME,
	VK_NV_FILL_RECTANGLE_EXTENSION_NAME,
	VK_EXT_PIPELINE_CREATION_FEEDBACK_EXTENSION_NAME,
	VK_EXT_FILTER_CUBIC_EXTENSION_NAME, // not supported by any device yet
	VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME,
	VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
	VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME
};

const std::vector<const char*> kRequiredDeviceExtensions =
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME
}; // Intel doesnt support VK_EXT_DEPTH_RANGE_UNRESTRICTED_EXTENSION_NAME

VKAPI_ATTR VkBool32 VKAPI_CALL DebugUtilsCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
#ifdef CEMU_DEBUG_ASSERT

	if (strstr(pCallbackData->pMessage, "consumes input location"))
		return VK_FALSE; // false means we dont care
	if (strstr(pCallbackData->pMessage, "blend"))
		return VK_FALSE; // 

	// note: Check if previously used location in VK_EXT_debug_report callback is the same as messageIdNumber under the new extension
	// validation errors which are difficult to fix
	if (pCallbackData->messageIdNumber == 0x6c3b517c || pCallbackData->messageIdNumber == 0xffffffffa6b17cdf || pCallbackData->messageIdNumber == 0xffffffffc406fcb7)
		return VK_FALSE; // its illegal to render to and sample from same texture
	if (pCallbackData->messageIdNumber == 0x6e633069)
		return VK_FALSE; // framebuffer attachments should have identity swizzle
	if (pCallbackData->messageIdNumber == 0xffffffffb408bc0b)
		return VK_FALSE; // too many samplers

	if (pCallbackData->messageIdNumber == 0x6bbb14)
		return VK_FALSE; // SPIR-V inconsistency

	if (strstr(pCallbackData->pMessage, "Number of currently valid sampler objects is not less than the maximum allowed"))
		return VK_FALSE;

	assert_dbg();

#endif

	cemuLog_log(LogType::Force, (char*)pCallbackData->pMessage);

	return VK_FALSE;
}

std::vector<VulkanRenderer::DeviceInfo> VulkanRenderer::GetDevices()
{
    if(!vkEnumerateInstanceVersion)
    {
        cemuLog_log(LogType::Force, "Vulkan cant list devices because Vulkan loader failed");
        return {};
    }
	uint32 apiVersion = VK_API_VERSION_1_1;
	if (vkEnumerateInstanceVersion(&apiVersion) != VK_SUCCESS)
	{
		if (VK_API_VERSION_MAJOR(apiVersion) < 1 || VK_API_VERSION_MINOR(apiVersion) < 2)
			apiVersion = VK_API_VERSION_1_1;
	}

	std::vector<DeviceInfo> result;

	std::vector<const char*> requiredExtensions;
	requiredExtensions.clear();
	requiredExtensions.emplace_back(VK_KHR_SURFACE_EXTENSION_NAME);
	#if BOOST_OS_WINDOWS
	requiredExtensions.emplace_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
	#elif BOOST_OS_LINUX
	auto backend = gui_getWindowInfo().window_main.backend;
	if(backend == WindowHandleInfo::Backend::X11)
		requiredExtensions.emplace_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
	#ifdef HAS_WAYLAND
	else if (backend == WindowHandleInfo::Backend::WAYLAND)
		requiredExtensions.emplace_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
	#endif
	#elif BOOST_OS_MACOS
	requiredExtensions.emplace_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
	#endif

	VkApplicationInfo app_info{};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = EMULATOR_NAME;
	app_info.applicationVersion = VK_MAKE_VERSION(EMULATOR_VERSION_LEAD, EMULATOR_VERSION_MAJOR, EMULATOR_VERSION_MINOR);
	app_info.pEngineName = EMULATOR_NAME;
	app_info.engineVersion = app_info.applicationVersion;
	app_info.apiVersion = apiVersion;

	VkInstanceCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;
	create_info.ppEnabledExtensionNames = requiredExtensions.data();
	create_info.enabledExtensionCount = requiredExtensions.size();
	create_info.ppEnabledLayerNames = nullptr;
	create_info.enabledLayerCount = 0;

	VkInstance instance = nullptr;
	try
	{
		VkResult err;
		if ((err = vkCreateInstance(&create_info, nullptr, &instance)) != VK_SUCCESS)
			throw std::runtime_error(fmt::format("Unable to create a Vulkan instance: {}", err));

		if (!InitializeInstanceVulkan(instance))
			throw std::runtime_error("can't initialize instanced vulkan functions");

		uint32_t device_count = 0;
		vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
		if (device_count == 0)
			throw std::runtime_error("Failed to find a GPU with Vulkan support.");

		// create tmp surface to create a logical device
		auto surface = CreateFramebufferSurface(instance, gui_getWindowInfo().window_main);
		std::vector<VkPhysicalDevice> devices(device_count);
		vkEnumeratePhysicalDevices(instance, &device_count, devices.data());
		for (const auto& device : devices)
		{
			if (IsDeviceSuitable(surface, device))
			{
				VkPhysicalDeviceIDProperties physDeviceIDProps = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES };
				VkPhysicalDeviceProperties2 physDeviceProps = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
				physDeviceProps.pNext = &physDeviceIDProps;
				vkGetPhysicalDeviceProperties2(device, &physDeviceProps);

				result.emplace_back(physDeviceProps.properties.deviceName, physDeviceIDProps.deviceUUID);
			}
		}
	}
	catch (...)
	{
	}

	if (instance)
		vkDestroyInstance(instance, nullptr);

	return result;

}

void VulkanRenderer::DetermineVendor()
{
	VkPhysicalDeviceProperties2 properties{};
	VkPhysicalDeviceDriverProperties driverProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES };
	properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	if (m_featureControl.deviceExtensions.driver_properties)
		properties.pNext = &driverProperties;

	vkGetPhysicalDeviceProperties2(m_physicalDevice, &properties);
	switch (properties.properties.vendorID)
	{
	case 0x10DE:
		m_vendor = GfxVendor::Nvidia;
		break;
	case 0x8086: // iGPU
		m_vendor = GfxVendor::Intel;
		break;
	case 0x1002:
		m_vendor = GfxVendor::AMD;
		break;
	case 0x106B:
		m_vendor = GfxVendor::Apple;
		break;
	}

	VkDriverId driverId = driverProperties.driverID;

	if(driverId == VK_DRIVER_ID_MESA_RADV || driverId == VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA)
		m_vendor = GfxVendor::Mesa;

	cemuLog_log(LogType::Force, "Using GPU: {}", properties.properties.deviceName);

	if (m_featureControl.deviceExtensions.driver_properties)
	{
		cemuLog_log(LogType::Force, "Driver version: {}", driverProperties.driverInfo);

		if(m_vendor == GfxVendor::Nvidia)
		{
			// multithreaded pipelines on nvidia (requires 515 or higher)
			m_featureControl.disableMultithreadedCompilation = (StringHelpers::ToInt(std::string(driverProperties.driverInfo)) < 515);
		}
	}

	else
	{
		cemuLog_log(LogType::Force, "Driver version (as stored in device info): {:08}", properties.properties.driverVersion);

		if(m_vendor == GfxVendor::Nvidia)
		{
			// if the driver does not support the extension,
			// it is assumed the driver is under version 515
			m_featureControl.disableMultithreadedCompilation = true;
		}
	}
}

void VulkanRenderer::GetDeviceFeatures()
{
	/* Get Vulkan features via GetPhysicalDeviceFeatures2 */
	void* prevStruct = nullptr;
	VkPhysicalDeviceCustomBorderColorFeaturesEXT bcf{};
	bcf.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT;
	bcf.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT;
	prevStruct = &bcf;

	VkPhysicalDevicePipelineCreationCacheControlFeaturesEXT pcc{};
	pcc.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_CREATION_CACHE_CONTROL_FEATURES_EXT;
	pcc.pNext = prevStruct;
	prevStruct = &pcc;

	VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{};
	physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	physicalDeviceFeatures2.pNext = prevStruct;

	vkGetPhysicalDeviceFeatures2(m_physicalDevice, &physicalDeviceFeatures2);

	/* Get Vulkan device properties and limits */
	VkPhysicalDeviceFloatControlsPropertiesKHR pfcp{};
	prevStruct = nullptr;
	if (m_featureControl.deviceExtensions.shader_float_controls)
	{
		pfcp.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES_KHR;
		pfcp.pNext = prevStruct;
		prevStruct = &pfcp;
	}

	VkPhysicalDeviceProperties2 prop2{};
	prop2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	prop2.pNext = prevStruct;

	vkGetPhysicalDeviceProperties2(m_physicalDevice, &prop2);

	/* Determine which subfeatures we can use */

	m_featureControl.deviceExtensions.pipeline_creation_cache_control = pcc.pipelineCreationCacheControl;
	m_featureControl.deviceExtensions.custom_border_color_without_format = m_featureControl.deviceExtensions.custom_border_color && bcf.customBorderColorWithoutFormat;
	m_featureControl.shaderFloatControls.shaderRoundingModeRTEFloat32 = m_featureControl.deviceExtensions.shader_float_controls && pfcp.shaderRoundingModeRTEFloat32;
	if(!m_featureControl.shaderFloatControls.shaderRoundingModeRTEFloat32)
		cemuLog_log(LogType::Force, "Shader round mode control not available on this device or driver. Some rendering issues might occur.");

	if (!m_featureControl.deviceExtensions.pipeline_creation_cache_control)
	{
		cemuLog_log(LogType::Force, "VK_EXT_pipeline_creation_cache_control not supported. Cannot use asynchronous shader and pipeline compilation");
		// if async shader compilation is enabled show warning message
		if (GetConfig().async_compile)
			wxMessageBox(_("The currently installed graphics driver does not support the Vulkan extension necessary for asynchronous shader compilation. Asynchronous compilation cannot be used.\n \nRequired extension: VK_EXT_pipeline_creation_cache_control\n\nInstalling the latest graphics driver may solve this error."), _("Information"), wxOK | wxCENTRE);
	}
	if (!m_featureControl.deviceExtensions.custom_border_color_without_format)
	{
		if (m_featureControl.deviceExtensions.custom_border_color)
		{
			cemuLog_log(LogType::Force, "VK_EXT_custom_border_color is present but only with limited support. Cannot emulate arbitrary border color");
		}
		else
		{
			cemuLog_log(LogType::Force, "VK_EXT_custom_border_color not supported. Cannot emulate arbitrary border color");
		}
	}

	// get limits
	m_featureControl.limits.minUniformBufferOffsetAlignment = std::max(prop2.properties.limits.minUniformBufferOffsetAlignment, (VkDeviceSize)4);
	m_featureControl.limits.nonCoherentAtomSize = std::max(prop2.properties.limits.nonCoherentAtomSize, (VkDeviceSize)4);
	cemuLog_log(LogType::Force, fmt::format("VulkanLimits: UBAlignment {0} nonCoherentAtomSize {1}", prop2.properties.limits.minUniformBufferOffsetAlignment, prop2.properties.limits.nonCoherentAtomSize));
}

VulkanRenderer::VulkanRenderer()
{
	glslang::InitializeProcess();

	cemuLog_log(LogType::Force, "------- Init Vulkan graphics backend -------");

	const bool useValidationLayer = cemuLog_isLoggingEnabled(LogType::VulkanValidation);
	if (useValidationLayer)
		cemuLog_log(LogType::Force, "Validation layer is enabled");

	VkResult err;

	// build list of layers
	m_layerNames.clear();
	if (useValidationLayer)
		m_layerNames.emplace_back("VK_LAYER_KHRONOS_validation");

	// check available instance extensions
	std::vector<const char*> enabledInstanceExtensions = CheckInstanceExtensionSupport(m_featureControl);

	uint32 apiVersion = VK_API_VERSION_1_1;
	if (vkEnumerateInstanceVersion(&apiVersion) != VK_SUCCESS)
	{
		if (VK_API_VERSION_MAJOR(apiVersion) < 1 || VK_API_VERSION_MINOR(apiVersion) < 2)
			apiVersion = VK_API_VERSION_1_1;
	}

	cemuLog_log(LogType::Force, fmt::format("Vulkan instance version: {}.{}", VK_API_VERSION_MAJOR(apiVersion), VK_API_VERSION_MINOR(apiVersion)));

	VkApplicationInfo app_info{};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = EMULATOR_NAME;
	app_info.applicationVersion = VK_MAKE_VERSION(EMULATOR_VERSION_LEAD, EMULATOR_VERSION_MAJOR, EMULATOR_VERSION_MINOR);
	app_info.pEngineName = EMULATOR_NAME;
	app_info.engineVersion = app_info.applicationVersion;
	app_info.apiVersion = apiVersion;

	VkInstanceCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;
	create_info.ppEnabledExtensionNames = enabledInstanceExtensions.data();
	create_info.enabledExtensionCount = enabledInstanceExtensions.size();
	create_info.ppEnabledLayerNames = m_layerNames.data();
	create_info.enabledLayerCount = m_layerNames.size();

	err = vkCreateInstance(&create_info, nullptr, &m_instance);

	if (err == VK_ERROR_LAYER_NOT_PRESENT) {
		cemuLog_log(LogType::Force, "Failed to enable vulkan validation (VK_LAYER_KHRONOS_validation)");
		create_info.enabledLayerCount = 0;
		err = vkCreateInstance(&create_info, nullptr, &m_instance);
	}

	if (err != VK_SUCCESS)
		throw std::runtime_error(fmt::format("Unable to create a Vulkan instance: {}", err));

	if (!InitializeInstanceVulkan(m_instance))
		throw std::runtime_error("Unable to load instanced Vulkan functions");

	uint32_t device_count = 0;
	vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr);
	if (device_count == 0)
		throw std::runtime_error("Failed to find a GPU with Vulkan support.");

	// create tmp surface to create a logical device
	auto surface = CreateFramebufferSurface(m_instance, gui_getWindowInfo().window_main);

	auto& config = GetConfig();
	decltype(config.graphic_device_uuid) zero{};
	const bool has_device_set = config.graphic_device_uuid != zero;

	VkPhysicalDevice fallbackDevice = VK_NULL_HANDLE;

	std::vector<VkPhysicalDevice> devices(device_count);
	vkEnumeratePhysicalDevices(m_instance, &device_count, devices.data());
	for (const auto& device : devices)
	{
		if (IsDeviceSuitable(surface, device))
		{
			if (fallbackDevice == VK_NULL_HANDLE)
				fallbackDevice = device;

			if (has_device_set)
			{
				VkPhysicalDeviceIDProperties physDeviceIDProps = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES };
				VkPhysicalDeviceProperties2 physDeviceProps = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
				physDeviceProps.pNext = &physDeviceIDProps;
				vkGetPhysicalDeviceProperties2(device, &physDeviceProps);

				if (memcmp(config.graphic_device_uuid.data(), physDeviceIDProps.deviceUUID, VK_UUID_SIZE) != 0)
					continue;
			}

			m_physicalDevice = device;
			break;
		}
	}

	if (m_physicalDevice == VK_NULL_HANDLE && fallbackDevice != VK_NULL_HANDLE)
	{
		cemuLog_log(LogType::Force, "The selected GPU could not be found or is not suitable. Falling back to first available device instead");
		m_physicalDevice = fallbackDevice;
		config.graphic_device_uuid = {}; // resetting device selection
	}
	else if (m_physicalDevice == VK_NULL_HANDLE)
	{
		cemuLog_log(LogType::Force, "No physical GPU could be found with the required extensions and swap chain support.");
		throw std::runtime_error("No physical GPU could be found with the required extensions and swap chain support.");
	}

	CheckDeviceExtensionSupport(m_physicalDevice, m_featureControl); // todo - merge this with GetDeviceFeatures and separate from IsDeviceSuitable?
	if (m_featureControl.debugMarkersSupported)
		cemuLog_log(LogType::Force, "Debug: Frame debugger attached, will use vkDebugMarkerSetObjectNameEXT");

	DetermineVendor();
	GetDeviceFeatures();

	// init memory manager
	memoryManager = new VKRMemoryManager(this);

	try
	{
		VkPhysicalDeviceIDProperties physDeviceIDProps = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES };
		VkPhysicalDeviceProperties2 physDeviceProps = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
		physDeviceProps.pNext = &physDeviceIDProps;
		vkGetPhysicalDeviceProperties2(m_physicalDevice, &physDeviceProps);

		#if BOOST_OS_WINDOWS
		m_dxgi_wrapper = std::make_unique<DXGIWrapper>(physDeviceIDProps.deviceLUID);
		#endif
	}
	catch (const std::exception& ex)
	{
		cemuLog_log(LogType::Force, "can't create dxgi wrapper: {}", ex.what());
	}

	// create logical device
	m_indices = SwapchainInfoVk::FindQueueFamilies(surface, m_physicalDevice);
	std::set<int> uniqueQueueFamilies = { m_indices.graphicsFamily, m_indices.presentFamily };
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos = CreateQueueCreateInfos(uniqueQueueFamilies);
	VkPhysicalDeviceFeatures deviceFeatures = {};

	deviceFeatures.independentBlend = VK_TRUE;
	deviceFeatures.samplerAnisotropy = VK_TRUE;
	deviceFeatures.imageCubeArray = VK_TRUE;
#if !BOOST_OS_MACOS
	deviceFeatures.geometryShader = VK_TRUE;
	deviceFeatures.logicOp = VK_TRUE;
#endif
	deviceFeatures.occlusionQueryPrecise = VK_TRUE;
	deviceFeatures.depthClamp = VK_TRUE;
	deviceFeatures.depthBiasClamp = VK_TRUE;
	if (m_vendor == GfxVendor::AMD)
	{
		deviceFeatures.robustBufferAccess = VK_TRUE;
		cemuLog_log(LogType::Force, "Enable robust buffer access");
	}
	if (m_featureControl.mode.useTFEmulationViaSSBO)
	{
		deviceFeatures.vertexPipelineStoresAndAtomics = true;
	}

	void* deviceExtensionFeatures = nullptr;

	// enable VK_EXT_pipeline_creation_cache_control
	VkPhysicalDevicePipelineCreationCacheControlFeaturesEXT cacheControlFeature{};
	if (m_featureControl.deviceExtensions.pipeline_creation_cache_control)
	{
		cacheControlFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_CREATION_CACHE_CONTROL_FEATURES_EXT;
		cacheControlFeature.pNext = deviceExtensionFeatures;
		deviceExtensionFeatures = &cacheControlFeature;
		cacheControlFeature.pipelineCreationCacheControl = VK_TRUE;
	}
	// enable VK_EXT_custom_border_color
	VkPhysicalDeviceCustomBorderColorFeaturesEXT customBorderColorFeature{};
	if (m_featureControl.deviceExtensions.custom_border_color_without_format)
	{
		customBorderColorFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT;
		customBorderColorFeature.pNext = deviceExtensionFeatures;
		deviceExtensionFeatures = &customBorderColorFeature;
		customBorderColorFeature.customBorderColors = VK_TRUE;
		customBorderColorFeature.customBorderColorWithoutFormat = VK_TRUE;
	}

	std::vector<const char*> used_extensions;
	VkDeviceCreateInfo createInfo = CreateDeviceCreateInfo(queueCreateInfos, deviceFeatures, deviceExtensionFeatures, used_extensions);

	VkResult result = vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_logicalDevice);
	if (result != VK_SUCCESS)
	{
		cemuLog_log(LogType::Force, "Vulkan: Unable to create a logical device. Error {}", (sint32)result);
		throw std::runtime_error(fmt::format("Unable to create a logical device: {}", result));
	}

	InitializeDeviceVulkan(m_logicalDevice);

	vkGetDeviceQueue(m_logicalDevice, m_indices.graphicsFamily, 0, &m_graphicsQueue);
	vkGetDeviceQueue(m_logicalDevice, m_indices.graphicsFamily, 0, &m_presentQueue);

	vkDestroySurfaceKHR(m_instance, surface, nullptr);

	if (useValidationLayer && m_featureControl.instanceExtensions.debug_utils)
	{
		PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));

		VkDebugUtilsMessengerCreateInfoEXT debugCallback{};
		debugCallback.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
		debugCallback.pNext = nullptr;
		debugCallback.flags = 0;
		debugCallback.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
		debugCallback.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debugCallback.pfnUserCallback = &DebugUtilsCallback;

		vkCreateDebugUtilsMessengerEXT(m_instance, &debugCallback, nullptr, &m_debugCallback);
	}

	if (m_featureControl.instanceExtensions.debug_utils)
		cemuLog_log(LogType::Force, "Using available debug function: vkCreateDebugUtilsMessengerEXT()");

	// set initial viewport and scissor box size
	m_state.currentViewport.width = 4;
	m_state.currentViewport.height = 4;
	m_state.currentScissorRect.extent.width = 4;
	m_state.currentScissorRect.extent.height = 4;

	QueryMemoryInfo();
	QueryAvailableFormats();
	CreateBackbufferIndexBuffer();
	CreateCommandPool();
	CreateCommandBuffers();
	CreateDescriptorPool();
	swapchain_createDescriptorSetLayout();

	// extension info
	// cemuLog_log(LogType::Force, "VK_KHR_dynamic_rendering: {}", m_featureControl.deviceExtensions.dynamic_rendering?"supported":"not supported");

	void* bufferPtr;
	// init ringbuffer for uniform vars
	m_uniformVarBufferMemoryIsCoherent = false;
	if (memoryManager->CreateBuffer2(UNIFORMVAR_RINGBUFFER_SIZE, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, m_uniformVarBuffer, m_uniformVarBufferMemory))
		m_uniformVarBufferMemoryIsCoherent = true;
	else if (memoryManager->CreateBuffer2(UNIFORMVAR_RINGBUFFER_SIZE, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_uniformVarBuffer, m_uniformVarBufferMemory))
		m_uniformVarBufferMemoryIsCoherent = true; // unified memory
	else if (memoryManager->CreateBuffer2(UNIFORMVAR_RINGBUFFER_SIZE, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_uniformVarBuffer, m_uniformVarBufferMemory))
		m_uniformVarBufferMemoryIsCoherent = true;
	else
	{
		memoryManager->CreateBuffer2(UNIFORMVAR_RINGBUFFER_SIZE, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, m_uniformVarBuffer, m_uniformVarBufferMemory);
	}

	if (!m_uniformVarBufferMemoryIsCoherent)
		cemuLog_log(LogType::Force, "[Vulkan-Info] Using non-coherent memory for uniform data");
	bufferPtr = nullptr;
	vkMapMemory(m_logicalDevice, m_uniformVarBufferMemory, 0, VK_WHOLE_SIZE, 0, &bufferPtr);
	m_uniformVarBufferPtr = (uint8*)bufferPtr;

	// texture readback buffer
	memoryManager->CreateBuffer(TEXTURE_READBACK_SIZE, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, m_textureReadbackBuffer, m_textureReadbackBufferMemory);
	bufferPtr = nullptr;
	vkMapMemory(m_logicalDevice, m_textureReadbackBufferMemory, 0, VK_WHOLE_SIZE, 0, &bufferPtr);
	m_textureReadbackBufferPtr = (uint8*)bufferPtr;

	// transform feedback ringbuffer
	memoryManager->CreateBuffer(LatteStreamout_GetRingBufferSize(), VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | (m_featureControl.mode.useTFEmulationViaSSBO ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : 0), 0, m_xfbRingBuffer, m_xfbRingBufferMemory);

	// occlusion query result buffer
	memoryManager->CreateBuffer(OCCLUSION_QUERY_POOL_SIZE * sizeof(uint64), VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, m_occlusionQueries.bufferQueryResults, m_occlusionQueries.memoryQueryResults);
	bufferPtr = nullptr;
	vkMapMemory(m_logicalDevice, m_occlusionQueries.memoryQueryResults, 0, VK_WHOLE_SIZE, 0, &bufferPtr);
	m_occlusionQueries.ptrQueryResults = (uint64*)bufferPtr;

	for (sint32 i = 0; i < OCCLUSION_QUERY_POOL_SIZE; i++)
		m_occlusionQueries.list_availableQueryIndices.emplace_back(i);

	// enable surface copies via buffer if we have plenty of memory available (otherwise use drawcalls)
	size_t availableSurfaceCopyBufferMem = memoryManager->GetTotalMemoryForBufferType(VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	//m_featureControl.mode.useBufferSurfaceCopies = availableSurfaceCopyBufferMem >= 2000ull * 1024ull * 1024ull; // enable if at least 2000MB VRAM
	m_featureControl.mode.useBufferSurfaceCopies = false;

	if (m_featureControl.mode.useBufferSurfaceCopies)
	{
		//cemuLog_log(LogType::Force, "Enable surface copies via buffer");
	}
	else
	{
		//cemuLog_log(LogType::Force, "Disable surface copies via buffer (Requires 2GB. Has only {}MB available)", availableSurfaceCopyBufferMem / 1024ull / 1024ull);
	}

	// start compilation threads
	RendererShaderVk::Init();
}

VulkanRenderer::~VulkanRenderer()
{
	SubmitCommandBuffer();
	WaitDeviceIdle();
	WaitCommandBufferFinished(GetCurrentCommandBufferId());
	// shut down compilation threads
	RendererShaderVk::Shutdown();
	// shut down pipeline save thread
	m_destructionRequested = true;
	m_pipeline_cache_semaphore.notify();
	m_pipeline_cache_save_thread.join();

	// shut down imgui
	ImGui_ImplVulkan_Shutdown();

	// delete null objects
	DeleteNullObjects();

	// delete buffers
	memoryManager->DeleteBuffer(m_indexBuffer, m_indexBufferMemory);
	memoryManager->DeleteBuffer(m_uniformVarBuffer, m_uniformVarBufferMemory);
	memoryManager->DeleteBuffer(m_textureReadbackBuffer, m_textureReadbackBufferMemory);
	memoryManager->DeleteBuffer(m_xfbRingBuffer, m_xfbRingBufferMemory);
	memoryManager->DeleteBuffer(m_occlusionQueries.bufferQueryResults, m_occlusionQueries.memoryQueryResults);
	memoryManager->DeleteBuffer(m_bufferCache, m_bufferCacheMemory);
	// texture memory
	// todo
	// upload buffers
	// todo

	m_padSwapchainInfo = nullptr;
	m_mainSwapchainInfo = nullptr;

	// clean up resources used for surface copy
	surfaceCopy_cleanup();

	// clean up default shaders
	delete defaultShaders.copySurface_vs;
	defaultShaders.copySurface_vs = nullptr;
	delete defaultShaders.copySurface_psColor2Depth;
	defaultShaders.copySurface_psColor2Depth = nullptr;
	delete defaultShaders.copySurface_psDepth2Color;
	defaultShaders.copySurface_psDepth2Color = nullptr;

	// destroy misc
	for (auto& it : m_cmd_buffer_fences)
	{
		vkDestroyFence(m_logicalDevice, it, nullptr);
		it = VK_NULL_HANDLE;
	}

	if (m_pipelineLayout != VK_NULL_HANDLE)
		vkDestroyPipelineLayout(m_logicalDevice, m_pipelineLayout, nullptr);

	if (m_commandPool != VK_NULL_HANDLE)
		vkDestroyCommandPool(m_logicalDevice, m_commandPool, nullptr);

	// destroy debug callback
	if (m_debugCallback)
	{
		PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
		vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugCallback, nullptr);
	}

	// destroy instance, devices
	if (m_instance != VK_NULL_HANDLE)
	{
		if (m_logicalDevice != VK_NULL_HANDLE)
		{
			vkDestroyDevice(m_logicalDevice, nullptr);
		}

		vkDestroyInstance(m_instance, nullptr);
	}

	// destroy memory manager
	delete memoryManager;

	// crashes?
	//glslang::FinalizeProcess();
}

VulkanRenderer* VulkanRenderer::GetInstance()
{
#ifdef CEMU_DEBUG_ASSERT
	cemu_assert_debug(g_renderer && dynamic_cast<VulkanRenderer*>(g_renderer.get()));
	// Use #if here because dynamic_casts dont get optimized away even if the result is not stored as with cemu_assert_debug
#endif
	return (VulkanRenderer*)g_renderer.get();
}

void VulkanRenderer::InitializeSurface(const Vector2i& size, bool mainWindow)
{
	auto& windowHandleInfo = mainWindow ? gui_getWindowInfo().canvas_main : gui_getWindowInfo().canvas_pad;

	const auto surface = CreateFramebufferSurface(m_instance, windowHandleInfo);
	if (mainWindow)
	{
		m_mainSwapchainInfo = std::make_unique<SwapchainInfoVk>(surface, mainWindow);
		m_mainSwapchainInfo->m_desiredExtent = size;
		m_mainSwapchainInfo->Create(m_physicalDevice, m_logicalDevice);

		// aquire first command buffer
		InitFirstCommandBuffer();
	}
	else
	{
		m_padSwapchainInfo = std::make_unique<SwapchainInfoVk>(surface, mainWindow);
		m_padSwapchainInfo->m_desiredExtent = size;
		// todo: figure out a way to exclusively create swapchain on main LatteThread
		m_padSwapchainInfo->Create(m_physicalDevice, m_logicalDevice);
	}
}

const std::unique_ptr<SwapchainInfoVk>& VulkanRenderer::GetChainInfoPtr(bool mainWindow) const
{
	return mainWindow ? m_mainSwapchainInfo : m_padSwapchainInfo;
}

SwapchainInfoVk& VulkanRenderer::GetChainInfo(bool mainWindow) const
{
	return *GetChainInfoPtr(mainWindow);
}

void VulkanRenderer::StopUsingPadAndWait()
{
	m_destroyPadSwapchainNextAcquire = true;
	m_padCloseReadySemaphore.wait();
}

bool VulkanRenderer::IsPadWindowActive()
{
	return IsSwapchainInfoValid(false);
}

void VulkanRenderer::HandleScreenshotRequest(LatteTextureView* texView, bool padView)
{
	const bool hasScreenshotRequest = gui_hasScreenshotRequest();
	if (!hasScreenshotRequest && m_screenshot_state == ScreenshotState::None)
		return;

	if (IsSwapchainInfoValid(false))
	{
		// we already took a pad view screenshow and want a main window screenshot
		if (m_screenshot_state == ScreenshotState::Main && padView)
			return;

		if (m_screenshot_state == ScreenshotState::Pad && !padView)
			return;

		// remember which screenshot is left to take
		if (m_screenshot_state == ScreenshotState::None)
			m_screenshot_state = padView ? ScreenshotState::Main : ScreenshotState::Pad;
		else
			m_screenshot_state = ScreenshotState::None;
	}
	else
		m_screenshot_state = ScreenshotState::None;

	auto texViewVk = (LatteTextureViewVk*)texView;
	auto baseImageTex = texViewVk->GetBaseImage();
	baseImageTex->GetImageObj()->flagForCurrentCommandBuffer();
	auto baseImageTexVkImage = baseImageTex->GetImageObj()->m_image;

	//auto baseImageObj = baseImage->GetTextureImageView();

	auto dumpImage = baseImageTex->GetImageObj()->m_image;
	//dumpImage->flagForCurrentCommandBuffer();

	int width, height;
	LatteTexture_getEffectiveSize(baseImageTex, &width, &height, nullptr, 0);

	VkImage image = nullptr;
	VkDeviceMemory imageMemory = nullptr;;

	auto format = baseImageTex->GetFormat();
	if (format != VK_FORMAT_R8G8B8A8_UNORM && format != VK_FORMAT_R8G8B8A8_SRGB && format != VK_FORMAT_R8G8B8_UNORM && format != VK_FORMAT_R8G8B8_SNORM)
	{
		VkFormatProperties formatProps;
		vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &formatProps);
		bool supportsBlit = (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) != 0;

		const bool dstUsesSRGB = (!padView && LatteGPUState.tvBufferUsesSRGB) || (padView && LatteGPUState.drcBufferUsesSRGB);
		const auto blitFormat = dstUsesSRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;

		vkGetPhysicalDeviceFormatProperties(m_physicalDevice, blitFormat, &formatProps);
		supportsBlit &= (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) != 0;

		// convert texture using blitting
		if (supportsBlit)
		{
			VkImageCreateInfo imageInfo{};
			imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageInfo.format = blitFormat;
			imageInfo.extent = { (uint32)width, (uint32)height, 1 };
			imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageInfo.arrayLayers = 1;
			imageInfo.mipLevels = 1;
			imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			imageInfo.imageType = VK_IMAGE_TYPE_2D;
			imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;

			if (vkCreateImage(m_logicalDevice, &imageInfo, nullptr, &image) != VK_SUCCESS)
				return;

			VkMemoryRequirements memRequirements;
			vkGetImageMemoryRequirements(m_logicalDevice, image, &memRequirements);

			VkMemoryAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = memRequirements.size;
			allocInfo.memoryTypeIndex = memoryManager->FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			if (vkAllocateMemory(m_logicalDevice, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
			{
				vkDestroyImage(m_logicalDevice, image, nullptr);
				return;
			}

			vkBindImageMemory(m_logicalDevice, image, imageMemory, 0);

			// prepare dest image
			{
				VkImageMemoryBarrier barrier = {};
				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.image = image;
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				barrier.subresourceRange.baseMipLevel = 0;
				barrier.subresourceRange.levelCount = 1;
				barrier.subresourceRange.baseArrayLayer = 0;
				barrier.subresourceRange.layerCount = 1;
				barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.srcAccessMask = 0;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				vkCmdPipelineBarrier(getCurrentCommandBuffer(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
			}
			// prepare src image for blitting
			{
				VkImageMemoryBarrier barrier = {};
				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.image = baseImageTexVkImage;
				barrier.subresourceRange.aspectMask = baseImageTex->GetImageAspect();
				barrier.subresourceRange.baseMipLevel = texViewVk->firstMip;
				barrier.subresourceRange.levelCount = 1;
				barrier.subresourceRange.baseArrayLayer = texViewVk->firstSlice;
				barrier.subresourceRange.layerCount = 1;
				barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				vkCmdPipelineBarrier(getCurrentCommandBuffer(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
			}

			VkOffset3D blitSize{ width, height, 1 };
			VkImageBlit imageBlitRegion{};
			imageBlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageBlitRegion.srcSubresource.layerCount = 1;
			imageBlitRegion.srcOffsets[1] = blitSize;
			imageBlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageBlitRegion.dstSubresource.layerCount = 1;
			imageBlitRegion.dstOffsets[1] = blitSize;

			// Issue the blit command
			vkCmdBlitImage(getCurrentCommandBuffer(), baseImageTexVkImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlitRegion, VK_FILTER_NEAREST);

			// dest image to general layout
			{
				VkImageMemoryBarrier barrier = {};
				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.image = image;
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				barrier.subresourceRange.baseMipLevel = 0;
				barrier.subresourceRange.levelCount = 1;
				barrier.subresourceRange.baseArrayLayer = 0;
				barrier.subresourceRange.layerCount = 1;
				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
				vkCmdPipelineBarrier(getCurrentCommandBuffer(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
			}
			// transition image back
			{
				VkImageMemoryBarrier barrier = {};
				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.image = baseImageTexVkImage;
				barrier.subresourceRange.aspectMask = baseImageTex->GetImageAspect();
				barrier.subresourceRange.baseMipLevel = texViewVk->firstMip;
				barrier.subresourceRange.levelCount = 1;
				barrier.subresourceRange.baseArrayLayer = texViewVk->firstSlice;
				barrier.subresourceRange.layerCount = 1;
				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
				vkCmdPipelineBarrier(getCurrentCommandBuffer(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
			}

			format = VK_FORMAT_R8G8B8A8_UNORM;
			dumpImage = image;
		}
	}

	uint32 size;
	switch (format)
	{
	case VK_FORMAT_R8G8B8A8_UNORM:
	case VK_FORMAT_R8G8B8A8_SRGB:
		size = 4 * width * height;
		break;
	case VK_FORMAT_R8G8B8_UNORM:
	case VK_FORMAT_R8G8B8_SRGB:
		size = 3 * width * height;
		break;
	default:
		size = 0;
	}

	if (size == 0)
	{
		cemu_assert_debug(false);
		return;
	}

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = width;
	region.bufferImageHeight = height;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageSubresource.mipLevel = 0;

	region.imageOffset = { 0,0,0 };
	region.imageExtent = { (uint32)width,(uint32)height,1 };

	void* bufferPtr = nullptr;

	VkBuffer buffer = nullptr;
	VkDeviceMemory bufferMemory = nullptr;
	memoryManager->CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, buffer, bufferMemory);
	vkMapMemory(m_logicalDevice, bufferMemory, 0, VK_WHOLE_SIZE, 0, &bufferPtr);

	{
		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = dumpImage;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		vkCmdPipelineBarrier(getCurrentCommandBuffer(), VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
	}

	vkCmdCopyImageToBuffer(getCurrentCommandBuffer(), dumpImage, VK_IMAGE_LAYOUT_GENERAL, buffer, 1, &region);

	SubmitCommandBuffer();
	WaitCommandBufferFinished(GetCurrentCommandBufferId());

	bool formatValid = true;

	std::vector<uint8> rgb_data;
	rgb_data.reserve(3 * width * height);

	switch (format)
	{
	case VK_FORMAT_R8G8B8A8_UNORM:
		for (auto ptr = (uint8*)bufferPtr; ptr < (uint8*)bufferPtr + size; ptr += 4)
		{
			rgb_data.emplace_back(*ptr);
			rgb_data.emplace_back(*(ptr + 1));
			rgb_data.emplace_back(*(ptr + 2));
		}
		break;
	case VK_FORMAT_R8G8B8A8_SRGB:
		for (auto ptr = (uint8*)bufferPtr; ptr < (uint8*)bufferPtr + size; ptr += 4)
		{
			rgb_data.emplace_back(SRGBComponentToRGB(*ptr));
			rgb_data.emplace_back(SRGBComponentToRGB(*(ptr + 1)));
			rgb_data.emplace_back(SRGBComponentToRGB(*(ptr + 2)));
		}
		break;
	case VK_FORMAT_R8G8B8_UNORM:
		std::copy((uint8*)bufferPtr, (uint8*)bufferPtr + size, rgb_data.begin());
		break;
	case VK_FORMAT_R8G8B8_SRGB:
		std::transform((uint8*)bufferPtr, (uint8*)bufferPtr + size, rgb_data.begin(), SRGBComponentToRGB);
		break;
	default:
		formatValid = false;
		cemu_assert_debug(false);
	}

	vkUnmapMemory(m_logicalDevice, bufferMemory);
	vkFreeMemory(m_logicalDevice, bufferMemory, nullptr);
	vkDestroyBuffer(m_logicalDevice, buffer, nullptr);

	if (image)
		vkDestroyImage(m_logicalDevice, image, nullptr);
	if (imageMemory)
		vkFreeMemory(m_logicalDevice, imageMemory, nullptr);

	if (formatValid)
		SaveScreenshot(rgb_data, width, height, !padView);
}

static const float kQueuePriority = 1.0f;

std::vector<VkDeviceQueueCreateInfo> VulkanRenderer::CreateQueueCreateInfos(const std::set<sint32>& uniqueQueueFamilies) const
{
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

	for (int queueFamily : uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &kQueuePriority;
		queueCreateInfos.emplace_back(queueCreateInfo);
	}

	return queueCreateInfos;
}

VkDeviceCreateInfo VulkanRenderer::CreateDeviceCreateInfo(const std::vector<VkDeviceQueueCreateInfo>& queueCreateInfos, const VkPhysicalDeviceFeatures& deviceFeatures, const void* deviceExtensionStructs, std::vector<const char*>& used_extensions) const
{
	used_extensions = kRequiredDeviceExtensions;
	if (m_featureControl.deviceExtensions.tooling_info)
		used_extensions.emplace_back(VK_EXT_TOOLING_INFO_EXTENSION_NAME);
	if (m_featureControl.deviceExtensions.depth_range_unrestricted)
		used_extensions.emplace_back(VK_EXT_DEPTH_RANGE_UNRESTRICTED_EXTENSION_NAME);
	if (m_featureControl.deviceExtensions.nv_fill_rectangle)
		used_extensions.emplace_back(VK_NV_FILL_RECTANGLE_EXTENSION_NAME);
	if (m_featureControl.deviceExtensions.pipeline_feedback)
		used_extensions.emplace_back(VK_EXT_PIPELINE_CREATION_FEEDBACK_EXTENSION_NAME);
	if (m_featureControl.deviceExtensions.cubic_filter)
		used_extensions.emplace_back(VK_EXT_FILTER_CUBIC_EXTENSION_NAME);
	if (m_featureControl.deviceExtensions.custom_border_color)
		used_extensions.emplace_back(VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME);
	if (m_featureControl.deviceExtensions.driver_properties)
		used_extensions.emplace_back(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME);
	if (m_featureControl.deviceExtensions.external_memory_host)
		used_extensions.emplace_back(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME);
	if (m_featureControl.deviceExtensions.synchronization2)
		used_extensions.emplace_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
	if (m_featureControl.deviceExtensions.dynamic_rendering)
		used_extensions.emplace_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
	if (m_featureControl.deviceExtensions.shader_float_controls)
		used_extensions.emplace_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledExtensionCount = used_extensions.size();
	createInfo.ppEnabledExtensionNames = used_extensions.data();

	createInfo.pNext = deviceExtensionStructs;

	if (!m_layerNames.empty())
	{
		createInfo.enabledLayerCount = m_layerNames.size();
		createInfo.ppEnabledLayerNames = m_layerNames.data();
	}

	return createInfo;
}

RendererShader* VulkanRenderer::shader_create(RendererShader::ShaderType type, uint64 baseHash, uint64 auxHash, const std::string& source, bool isGameShader, bool isGfxPackShader)
{
	return new RendererShaderVk(type, baseHash, auxHash, isGameShader, isGfxPackShader, source);
}

bool VulkanRenderer::CheckDeviceExtensionSupport(const VkPhysicalDevice device, FeatureControl& info)
{
	std::vector<VkExtensionProperties> availableDeviceExtensions;

	auto isExtensionAvailable = [&availableDeviceExtensions](const char* extensionName) -> bool
	{
		return std::find_if(availableDeviceExtensions.begin(), availableDeviceExtensions.end(),
			[&extensionName](const VkExtensionProperties& prop) -> bool
		{
			return strcmp(prop.extensionName, extensionName) == 0;
		}) != availableDeviceExtensions.cend();
	};

	uint32_t extensionCount;
	VkResult result = vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
	if (result != VK_SUCCESS)
		throw std::runtime_error(fmt::format("Cannot retrieve count of properties for a physical device: {}", result));

	availableDeviceExtensions.resize(extensionCount);
	result = vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableDeviceExtensions.data());
	if (result != VK_SUCCESS)
		throw std::runtime_error(fmt::format("Cannot retrieve properties for a physical device: {}", result));

	std::set<std::string> requiredExtensions(kRequiredDeviceExtensions.begin(), kRequiredDeviceExtensions.end());
	for (const auto& extension : availableDeviceExtensions)
	{
		requiredExtensions.erase(extension.extensionName);
	}

	info.deviceExtensions.tooling_info = isExtensionAvailable(VK_EXT_TOOLING_INFO_EXTENSION_NAME);
	info.deviceExtensions.transform_feedback = isExtensionAvailable(VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME);
	info.deviceExtensions.depth_range_unrestricted = isExtensionAvailable(VK_EXT_DEPTH_RANGE_UNRESTRICTED_EXTENSION_NAME);
	info.deviceExtensions.nv_fill_rectangle = isExtensionAvailable(VK_NV_FILL_RECTANGLE_EXTENSION_NAME);
	info.deviceExtensions.pipeline_feedback = isExtensionAvailable(VK_EXT_PIPELINE_CREATION_FEEDBACK_EXTENSION_NAME);
	info.deviceExtensions.cubic_filter = isExtensionAvailable(VK_EXT_FILTER_CUBIC_EXTENSION_NAME);
	info.deviceExtensions.custom_border_color = isExtensionAvailable(VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME);
	info.deviceExtensions.driver_properties = isExtensionAvailable(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME);
	info.deviceExtensions.external_memory_host = isExtensionAvailable(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME);
	info.deviceExtensions.synchronization2 = isExtensionAvailable(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
	info.deviceExtensions.shader_float_controls = isExtensionAvailable(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
	info.deviceExtensions.dynamic_rendering = false; // isExtensionAvailable(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
	// dynamic rendering doesn't provide any benefits for us right now. Driver implementations are very unoptimized as of Feb 2022

	// check for framedebuggers
	info.debugMarkersSupported = false;
	if (info.deviceExtensions.tooling_info && vkGetPhysicalDeviceToolPropertiesEXT)
	{
		uint32_t toolCount = 0;
		if (vkGetPhysicalDeviceToolPropertiesEXT(device, &toolCount, nullptr) == VK_SUCCESS)
		{
			std::vector<VkPhysicalDeviceToolPropertiesEXT> toolProperties(toolCount);
			if (toolCount > 0 && vkGetPhysicalDeviceToolPropertiesEXT(device, &toolCount, toolProperties.data()) == VK_SUCCESS)
			{
				for (auto& itr : toolProperties)
				{
					if ((itr.purposes & VK_TOOL_PURPOSE_DEBUG_MARKERS_BIT_EXT) != 0)
						info.debugMarkersSupported = true;
				}
			}
		}
	}

	return requiredExtensions.empty();
}

std::vector<const char*> VulkanRenderer::CheckInstanceExtensionSupport(FeatureControl& info)
{
	std::vector<VkExtensionProperties> availableInstanceExtensions;
	std::vector<const char*> enabledInstanceExtensions;
	VkResult err;

	auto isExtensionAvailable = [&availableInstanceExtensions](const char* extensionName) -> bool
	{
		return std::find_if(availableInstanceExtensions.begin(), availableInstanceExtensions.end(),
			[&extensionName](const VkExtensionProperties& prop) -> bool
		{
			return strcmp(prop.extensionName, extensionName) == 0;
		}) != availableInstanceExtensions.cend();
	};

	// get list of available instance extensions
	uint32_t count;
	if ((err = vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr)) != VK_SUCCESS)
		throw std::runtime_error(fmt::format("Failed to retrieve the instance extension properties : {}", err));

	availableInstanceExtensions.resize(count);
	if ((err = vkEnumerateInstanceExtensionProperties(nullptr, &count, availableInstanceExtensions.data())) != VK_SUCCESS)
		throw std::runtime_error(fmt::format("Failed to retrieve the instance extension properties: {}", err));

	// build list of required extensions
	std::vector<const char*> requiredInstanceExtensions;
	requiredInstanceExtensions.emplace_back(VK_KHR_SURFACE_EXTENSION_NAME);
	#if BOOST_OS_WINDOWS
	requiredInstanceExtensions.emplace_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
	#elif BOOST_OS_LINUX
	auto backend = gui_getWindowInfo().window_main.backend;
	if(backend == WindowHandleInfo::Backend::X11)
		requiredInstanceExtensions.emplace_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
	#if HAS_WAYLAND
	else if (backend == WindowHandleInfo::Backend::WAYLAND)
		requiredInstanceExtensions.emplace_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
	#endif
	#elif BOOST_OS_MACOS
	requiredInstanceExtensions.emplace_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
	#endif
	if (cemuLog_isLoggingEnabled(LogType::VulkanValidation))
		requiredInstanceExtensions.emplace_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

	// make sure all required extensions are supported
	for (const auto& extension : availableInstanceExtensions)
	{
		for (auto it = requiredInstanceExtensions.begin(); it < requiredInstanceExtensions.end(); ++it)
		{
			if (strcmp(*it, extension.extensionName) == 0)
			{
				enabledInstanceExtensions.emplace_back(*it);
				requiredInstanceExtensions.erase(it);
				break;
			}
		}
	}
	if (!requiredInstanceExtensions.empty())
	{
		cemuLog_log(LogType::Force, "The following required Vulkan instance extensions are not supported:");

		std::stringstream ss;
		for (const auto& extension : requiredInstanceExtensions)
			cemuLog_log(LogType::Force, "{}", extension);
		cemuLog_waitForFlush();
		throw std::runtime_error(ss.str());
	}

	// check for optional extensions
	info.instanceExtensions.debug_utils = isExtensionAvailable(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	if (info.instanceExtensions.debug_utils)
		enabledInstanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	return enabledInstanceExtensions;
}

bool VulkanRenderer::IsDeviceSuitable(VkSurfaceKHR surface, const VkPhysicalDevice& device)
{
	if (!SwapchainInfoVk::FindQueueFamilies(surface, device).IsComplete())
		return false;

	// check API version (using Vulkan 1.0 way of querying properties)
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(device, &properties);
	uint32 vkVersionMajor = VK_API_VERSION_MAJOR(properties.apiVersion);
	uint32 vkVersionMinor = VK_API_VERSION_MINOR(properties.apiVersion);
	if (vkVersionMajor < 1 || (vkVersionMajor == 1 && vkVersionMinor < 1))
		return false; // minimum required version is Vulkan 1.1

	FeatureControl info;
	if (!CheckDeviceExtensionSupport(device, info))
		return false;

	const auto swapchainSupport = SwapchainInfoVk::QuerySwapchainSupport(surface, device);

	return !swapchainSupport.formats.empty() && !swapchainSupport.presentModes.empty();
}

#if BOOST_OS_WINDOWS
VkSurfaceKHR VulkanRenderer::CreateWinSurface(VkInstance instance, HWND hwindow)
{
	VkWin32SurfaceCreateInfoKHR sci{};
	sci.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	sci.hwnd = hwindow;
	sci.hinstance = GetModuleHandle(nullptr);

	VkSurfaceKHR result;
	VkResult err;
	if ((err = vkCreateWin32SurfaceKHR(instance, &sci, nullptr, &result)) != VK_SUCCESS)
	{
		cemuLog_log(LogType::Force, "Cannot create a Win32 Vulkan surface: {}", (sint32)err);
		throw std::runtime_error(fmt::format("Cannot create a Win32 Vulkan surface: {}", err));
	}

	return result;
}
#endif

#if BOOST_OS_LINUX
VkSurfaceKHR VulkanRenderer::CreateXlibSurface(VkInstance instance, Display* dpy, Window window)
{
    VkXlibSurfaceCreateInfoKHR sci{};
    sci.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    sci.flags = 0;
	sci.dpy = dpy;
    sci.window = window;

    VkSurfaceKHR result;
    VkResult err;
    if ((err = vkCreateXlibSurfaceKHR(instance, &sci, nullptr, &result)) != VK_SUCCESS)
    {
		cemuLog_log(LogType::Force, "Cannot create a X11 Vulkan surface: {}", (sint32)err);
        throw std::runtime_error(fmt::format("Cannot create a X11 Vulkan surface: {}", err));
    }

    return result;
}

VkSurfaceKHR VulkanRenderer::CreateXcbSurface(VkInstance instance, xcb_connection_t* connection, xcb_window_t window)
{
    VkXcbSurfaceCreateInfoKHR sci{};
    sci.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    sci.flags = 0;
    sci.connection = connection;
    sci.window = window;

    VkSurfaceKHR result;
    VkResult err;
    if ((err = vkCreateXcbSurfaceKHR(instance, &sci, nullptr, &result)) != VK_SUCCESS)
    {
        cemuLog_log(LogType::Force, "Cannot create a XCB Vulkan surface: {}", (sint32)err);
        throw std::runtime_error(fmt::format("Cannot create a XCB Vulkan surface: {}", err));
    }

    return result;
}
#ifdef HAS_WAYLAND
VkSurfaceKHR VulkanRenderer::CreateWaylandSurface(VkInstance instance, wl_display* display, wl_surface* surface)
{
    VkWaylandSurfaceCreateInfoKHR sci{};
    sci.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    sci.flags = 0;
	sci.display = display;
	sci.surface = surface;

    VkSurfaceKHR result;
    VkResult err;
    if ((err = vkCreateWaylandSurfaceKHR(instance, &sci, nullptr, &result)) != VK_SUCCESS)
    {
        cemuLog_log(LogType::Force, "Cannot create a Wayland Vulkan surface: {}", (sint32)err);
        throw std::runtime_error(fmt::format("Cannot create a Wayland Vulkan surface: {}", err));
    }

    return result;
}
#endif // HAS_WAYLAND
#endif // BOOST_OS_LINUX

VkSurfaceKHR VulkanRenderer::CreateFramebufferSurface(VkInstance instance, struct WindowHandleInfo& windowInfo)
{
#if BOOST_OS_WINDOWS
	return CreateWinSurface(instance, windowInfo.hwnd);
#elif BOOST_OS_LINUX
	if(windowInfo.backend == WindowHandleInfo::Backend::X11)
		return CreateXlibSurface(instance, windowInfo.xlib_display, windowInfo.xlib_window);
	#ifdef HAS_WAYLAND
	if(windowInfo.backend == WindowHandleInfo::Backend::WAYLAND)
		return CreateWaylandSurface(instance, windowInfo.display, windowInfo.surface);
	#endif
	return {};
#elif BOOST_OS_MACOS
	return CreateCocoaSurface(instance, windowInfo.handle);
#endif
}

void VulkanRenderer::CreateCommandPool()
{
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = m_indices.graphicsFamily;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VkResult result = vkCreateCommandPool(m_logicalDevice, &poolInfo, nullptr, &m_commandPool);
	if (result != VK_SUCCESS)
		throw std::runtime_error(fmt::format("Failed to create command pool: {}", result));
}

void VulkanRenderer::CreateCommandBuffers()
{
	auto it = m_cmd_buffer_fences.begin();
	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	vkCreateFence(m_logicalDevice, &fenceInfo, nullptr, &*it);

	++it;
	fenceInfo.flags = 0;
	for (; it != m_cmd_buffer_fences.end(); ++it)
	{
		vkCreateFence(m_logicalDevice, &fenceInfo, nullptr, &*it);
	}

	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t)m_commandBuffers.size();

	const VkResult result = vkAllocateCommandBuffers(m_logicalDevice, &allocInfo, m_commandBuffers.data());
	if (result != VK_SUCCESS)
	{
		cemuLog_log(LogType::Force, "Failed to allocate command buffers: {}", result);
		throw std::runtime_error(fmt::format("Failed to allocate command buffers: {}", result));
	}

	for (auto& semItr : m_commandBufferSemaphores)
	{
		VkSemaphoreCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		if (vkCreateSemaphore(m_logicalDevice, &info, nullptr, &semItr) != VK_SUCCESS)
			UnrecoverableError("Failed to create semaphore for command buffer");
	}
}

bool VulkanRenderer::IsSwapchainInfoValid(bool mainWindow) const
{
	auto& chainInfo = GetChainInfoPtr(mainWindow);
	return chainInfo && chainInfo->IsValid();
}


void VulkanRenderer::CreateNullTexture(NullTexture& nullTex, VkImageType imageType)
{
	// these are used when the game requests NULL ptr textures or buffers
	// texture
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	if (imageType == VK_IMAGE_TYPE_1D)
	{
		imageInfo.extent.width = 4;
		imageInfo.extent.height = 1;
	}
	else if (imageType == VK_IMAGE_TYPE_2D)
	{
		imageInfo.extent.width = 4;
		imageInfo.extent.height = 1;
	}
	else
	{
		cemu_assert(false);
	}
	imageInfo.mipLevels = 1;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.extent.depth = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.imageType = imageType;
	imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	if (vkCreateImage(m_logicalDevice, &imageInfo, nullptr, &nullTex.image) != VK_SUCCESS)
		UnrecoverableError("Failed to create nullTex image");
	nullTex.allocation = memoryManager->imageMemoryAllocate(nullTex.image);

	VkClearColorValue clrColor{};
	ClearColorImageRaw(nullTex.image, 0, 0, clrColor, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	// texture view
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = nullTex.image;
	if (imageType == VK_IMAGE_TYPE_1D)
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_1D;
	else if (imageType == VK_IMAGE_TYPE_2D)
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	else
	{
		cemu_assert(false);
	}
	viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	if (vkCreateImageView(m_logicalDevice, &viewInfo, nullptr, &nullTex.view) != VK_SUCCESS)
		UnrecoverableError("Failed to create nullTex image view");
	// sampler
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;
	samplerInfo.maxAnisotropy = 1.0;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	vkCreateSampler(m_logicalDevice, &samplerInfo, nullptr, &nullTex.sampler);
}

void VulkanRenderer::CreateNullObjects()
{
	CreateNullTexture(nullTexture1D, VK_IMAGE_TYPE_1D);
	CreateNullTexture(nullTexture2D, VK_IMAGE_TYPE_2D);
}

void VulkanRenderer::DeleteNullTexture(NullTexture& nullTex)
{
	vkDestroySampler(m_logicalDevice, nullTex.sampler, nullptr);
	nullTex.sampler = VK_NULL_HANDLE;
	vkDestroyImageView(m_logicalDevice, nullTex.view, nullptr);
	nullTex.view = VK_NULL_HANDLE;
	vkDestroyImage(m_logicalDevice, nullTex.image, nullptr);
	nullTex.image = VK_NULL_HANDLE;
	memoryManager->imageMemoryFree(nullTex.allocation);
	nullTex.allocation = nullptr;
}

void VulkanRenderer::DeleteNullObjects()
{
	DeleteNullTexture(nullTexture1D);
	DeleteNullTexture(nullTexture2D);
}

void VulkanRenderer::ImguiInit()
{
	if (m_imguiRenderPass == VK_NULL_HANDLE)
	{
		// TODO: renderpass swapchain format may change between srgb and rgb -> need reinit
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = m_mainSwapchainInfo->m_surfaceFormat.format;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef = {};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		const auto result = vkCreateRenderPass(m_logicalDevice, &renderPassInfo, nullptr, &m_imguiRenderPass);
		if (result != VK_SUCCESS)
			throw VkException(result, "can't create imgui renderpass");
	}

	ImGui_ImplVulkan_InitInfo info{};
	info.Instance = m_instance;
	info.PhysicalDevice = m_physicalDevice;
	info.Device = m_logicalDevice;
	info.QueueFamily = m_indices.presentFamily;
	info.Queue = m_presentQueue;
	info.PipelineCache = m_pipeline_cache;
	info.DescriptorPool = m_descriptorPool;
	info.MinImageCount = m_mainSwapchainInfo->m_swapchainImages.size();
	info.ImageCount = info.MinImageCount;

	ImGui_ImplVulkan_Init(&info, m_imguiRenderPass);
}

void VulkanRenderer::Initialize()
{
	Renderer::Initialize();
	CreatePipelineCache();
	ImguiInit();
	CreateNullObjects();
}

void VulkanRenderer::Shutdown()
{
	Renderer::Shutdown();
	SubmitCommandBuffer();
	WaitDeviceIdle();

	if (m_imguiRenderPass != VK_NULL_HANDLE)
	{
		vkDestroyRenderPass(m_logicalDevice, m_imguiRenderPass, nullptr);
		m_imguiRenderPass = VK_NULL_HANDLE;
	}
}

void VulkanRenderer::UnrecoverableError(const char* errMsg) const
{
	cemuLog_log(LogType::Force, "Unrecoverable error in Vulkan renderer");
	cemuLog_log(LogType::Force, "Msg: {}", errMsg);
	throw std::runtime_error(errMsg);
}

struct VulkanRequestedFormat_t
{
	VkFormat fmt;
	const char* name;
	bool isDepth;
	bool mustSupportAttachment;
	bool mustSupportBlending;
};

#define reqColorFormat(__name, __reqAttachment, __reqBlend) {__name, ""#__name, false, __reqAttachment, __reqBlend}
#define reqDepthFormat(__name) {__name, ""#__name, true, true, false}

VulkanRequestedFormat_t requestedFormatList[] =
{
	reqDepthFormat(VK_FORMAT_D32_SFLOAT_S8_UINT),
	reqDepthFormat(VK_FORMAT_D24_UNORM_S8_UINT),
	reqDepthFormat(VK_FORMAT_D32_SFLOAT),
	reqDepthFormat(VK_FORMAT_D16_UNORM),
	reqColorFormat(VK_FORMAT_R32G32B32A32_SFLOAT, true, true),
	reqColorFormat(VK_FORMAT_R32G32B32A32_UINT, true, false),
	reqColorFormat(VK_FORMAT_R16G16B16A16_SFLOAT, true, true),
	reqColorFormat(VK_FORMAT_R16G16B16A16_UINT, true, false),
	reqColorFormat(VK_FORMAT_R16G16B16A16_UNORM, true, true),
	reqColorFormat(VK_FORMAT_R16G16B16A16_SNORM, true, true),
	reqColorFormat(VK_FORMAT_R8G8B8A8_UNORM, true, true),
	reqColorFormat(VK_FORMAT_R8G8B8A8_SNORM, true, true),
	reqColorFormat(VK_FORMAT_R8G8B8A8_SRGB, true, true),
	reqColorFormat(VK_FORMAT_R8G8B8A8_UINT, true, false),
	reqColorFormat(VK_FORMAT_R8G8B8A8_SINT, true, false),
	reqColorFormat(VK_FORMAT_R4G4B4A4_UNORM_PACK16, true, true),
	reqColorFormat(VK_FORMAT_R32G32_SFLOAT, true, true),
	reqColorFormat(VK_FORMAT_R32G32_UINT, true, false),
	reqColorFormat(VK_FORMAT_R16G16_UNORM, true, true),
	reqColorFormat(VK_FORMAT_R16G16_SFLOAT, true, true),
	reqColorFormat(VK_FORMAT_R8G8_UNORM, true, true),
	reqColorFormat(VK_FORMAT_R8G8_SNORM, true, true),
	reqColorFormat(VK_FORMAT_R4G4_UNORM_PACK8, true, true),
	reqColorFormat(VK_FORMAT_R32_SFLOAT, true, true),
	reqColorFormat(VK_FORMAT_R32_UINT, true, false),
	reqColorFormat(VK_FORMAT_R16_SFLOAT, true, true),
	reqColorFormat(VK_FORMAT_R16_UNORM, true, true),
	reqColorFormat(VK_FORMAT_R16_SNORM, true, true),
	reqColorFormat(VK_FORMAT_R8_UNORM, true, true),
	reqColorFormat(VK_FORMAT_R8_SNORM, true, true),
	reqColorFormat(VK_FORMAT_R5G6B5_UNORM_PACK16, true, true),
	reqColorFormat(VK_FORMAT_R5G5B5A1_UNORM_PACK16, true, true),
	reqColorFormat(VK_FORMAT_B10G11R11_UFLOAT_PACK32, true, true),
	reqColorFormat(VK_FORMAT_R16G16B16A16_SNORM, true, true),
	reqColorFormat(VK_FORMAT_BC1_RGBA_SRGB_BLOCK, false, false),
	reqColorFormat(VK_FORMAT_BC1_RGBA_UNORM_BLOCK, false, false),
	reqColorFormat(VK_FORMAT_BC2_UNORM_BLOCK, false, false),
	reqColorFormat(VK_FORMAT_BC2_SRGB_BLOCK, false, false),
	reqColorFormat(VK_FORMAT_BC3_UNORM_BLOCK, false, false),
	reqColorFormat(VK_FORMAT_BC3_SRGB_BLOCK, false, false),
	reqColorFormat(VK_FORMAT_BC4_UNORM_BLOCK, false, false),
	reqColorFormat(VK_FORMAT_BC4_SNORM_BLOCK, false, false),
	reqColorFormat(VK_FORMAT_BC5_UNORM_BLOCK, false, false),
	reqColorFormat(VK_FORMAT_BC5_SNORM_BLOCK, false, false),
	reqColorFormat(VK_FORMAT_A2B10G10R10_UNORM_PACK32, true, true),
	reqColorFormat(VK_FORMAT_R32_SFLOAT, true, true)
};

void VulkanRenderer::QueryMemoryInfo()
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);
	cemuLog_log(LogType::Force, "Vulkan device memory info:");
	for (uint32 i = 0; i < memProperties.memoryHeapCount; i++)
	{
		cemuLog_log(LogType::Force, "Heap {} - Size {}MB Flags 0x{:08x}", i, (sint32)(memProperties.memoryHeaps[i].size / 1024ll / 1024ll), (uint32)memProperties.memoryHeaps[i].flags);
	}
	for (uint32 i = 0; i < memProperties.memoryTypeCount; i++)
	{
		cemuLog_log(LogType::Force, "Memory {} - HeapIndex {} Flags 0x{:08x}", i, (sint32)memProperties.memoryTypes[i].heapIndex, (uint32)memProperties.memoryTypes[i].propertyFlags);
	}
}

void VulkanRenderer::QueryAvailableFormats()
{
	VkFormatProperties fmtProp{};
	vkGetPhysicalDeviceFormatProperties(m_physicalDevice, VK_FORMAT_D24_UNORM_S8_UINT, &fmtProp);
	// D24S8
	if (fmtProp.optimalTilingFeatures != 0) // todo - more restrictive check
	{
		m_supportedFormatInfo.fmt_d24_unorm_s8_uint = true;
	}
	// R4G4
	fmtProp = {};
	vkGetPhysicalDeviceFormatProperties(m_physicalDevice, VK_FORMAT_R4G4_UNORM_PACK8, &fmtProp);
	if (fmtProp.optimalTilingFeatures != 0)
	{
		m_supportedFormatInfo.fmt_r4g4_unorm_pack = true;
	}
	// R5G6B5
	fmtProp = {};
	vkGetPhysicalDeviceFormatProperties(m_physicalDevice, VK_FORMAT_R5G6B5_UNORM_PACK16, &fmtProp);
	if (fmtProp.optimalTilingFeatures != 0)
	{
		m_supportedFormatInfo.fmt_r5g6b5_unorm_pack = true;
	}
	// R4G4B4A4
	fmtProp = {};
	vkGetPhysicalDeviceFormatProperties(m_physicalDevice, VK_FORMAT_R4G4B4A4_UNORM_PACK16, &fmtProp);
	if (fmtProp.optimalTilingFeatures != 0)
	{
		m_supportedFormatInfo.fmt_r4g4b4a4_unorm_pack = true;
	}
	// A1R5G5B5
	fmtProp = {};
	vkGetPhysicalDeviceFormatProperties(m_physicalDevice, VK_FORMAT_A1R5G5B5_UNORM_PACK16, &fmtProp);
	if (fmtProp.optimalTilingFeatures != 0)
	{
		m_supportedFormatInfo.fmt_a1r5g5b5_unorm_pack = true;
	}
	// print info about unsupported formats to log
	for (auto& it : requestedFormatList)
	{
		fmtProp = {};
		vkGetPhysicalDeviceFormatProperties(m_physicalDevice, it.fmt, &fmtProp);
		VkFormatFeatureFlags requestedBits = 0;
		if (it.mustSupportAttachment)
		{
			if (it.isDepth)
				requestedBits |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
			else
				requestedBits |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
			if (!it.isDepth && it.mustSupportBlending)
				requestedBits |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
		}
		requestedBits |= VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
		requestedBits |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;

		if (fmtProp.optimalTilingFeatures == 0)
		{
			cemuLog_log(LogType::Force, "{} not supported", it.name);
		}
		else if ((fmtProp.optimalTilingFeatures & requestedBits) != requestedBits)
		{
			//std::string missingStr;
			//missingStr.assign(fmt::format("{} missing features:", it.name));
			//if (!(fmtProp.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) && !it.isDepth && it.mustSupportAttachment)
			//	missingStr.append(" COLOR_ATTACHMENT");
			//if (!(fmtProp.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT) && !it.isDepth && it.mustSupportBlending)
			//	missingStr.append(" COLOR_ATTACHMENT_BLEND");
			//if (!(fmtProp.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) && it.isDepth && it.mustSupportAttachment)
			//	missingStr.append(" DEPTH_ATTACHMENT");
			//if (!(fmtProp.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT))
			//	missingStr.append(" TRANSFER_DST");
			//if (!(fmtProp.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
			//	missingStr.append(" SAMPLED_IMAGE");
			//cemuLog_log(LogType::Force, "{}", missingStr.c_str());
		}
	}
}

bool VulkanRenderer::ImguiBegin(bool mainWindow)
{
	if (!Renderer::ImguiBegin(mainWindow))
		return false;

	auto& chainInfo = GetChainInfo(mainWindow);

	if (!AcquireNextSwapchainImage(mainWindow))
		return false;

	draw_endRenderPass();
	m_state.currentPipeline = VK_NULL_HANDLE;

	ImGui_ImplVulkan_CreateFontsTexture(m_state.currentCommandBuffer);
	ImGui_ImplVulkan_NewFrame(m_state.currentCommandBuffer, chainInfo.m_swapchainFramebuffers[chainInfo.swapchainImageIndex], chainInfo.getExtent());
	ImGui_UpdateWindowInformation(mainWindow);
	ImGui::NewFrame();
	return true;
}

void VulkanRenderer::ImguiEnd()
{
	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_state.currentCommandBuffer);
	vkCmdEndRenderPass(m_state.currentCommandBuffer);
}

std::vector<LatteTextureVk*> g_imgui_textures; // TODO manage better
ImTextureID VulkanRenderer::GenerateTexture(const std::vector<uint8>& data, const Vector2i& size)
{
	try
	{
		std::vector <uint8> tmp(size.x * size.y * 4);
		for (size_t i = 0; i < data.size() / 3; ++i)
		{
			tmp[(i * 4) + 0] = data[(i * 3) + 0];
			tmp[(i * 4) + 1] = data[(i * 3) + 1];
			tmp[(i * 4) + 2] = data[(i * 3) + 2];
			tmp[(i * 4) + 3] = 0xFF;
		}
		return (ImTextureID)ImGui_ImplVulkan_GenerateTexture(m_state.currentCommandBuffer, tmp, size);
	}
	catch (const std::exception& ex)
	{
		cemuLog_log(LogType::Force, "can't generate imgui texture: {}", ex.what());
		return nullptr;
	}
}

void VulkanRenderer::DeleteTexture(ImTextureID id)
{
	WaitDeviceIdle();
	ImGui_ImplVulkan_DeleteTexture(id);
}

void VulkanRenderer::DeleteFontTextures()
{
	ImGui_ImplVulkan_DestroyFontsTexture();
}


bool VulkanRenderer::BeginFrame(bool mainWindow)
{
	if (!AcquireNextSwapchainImage(mainWindow))
		return false;

	auto& chainInfo = GetChainInfo(mainWindow);

	VkClearColorValue clearColor{ 0, 0, 0, 0 };
	ClearColorImageRaw(chainInfo.m_swapchainImages[chainInfo.swapchainImageIndex], 0, 0, clearColor, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	// mark current swapchain image as well defined
	chainInfo.hasDefinedSwapchainImage = true;

	return true;
}

void VulkanRenderer::DrawEmptyFrame(bool mainWindow)
{
	if (!BeginFrame(mainWindow))
		return;
	SwapBuffers(mainWindow, !mainWindow);
}

void VulkanRenderer::PreparePresentationFrame(bool mainWindow)
{
	AcquireNextSwapchainImage(mainWindow);
}

void VulkanRenderer::ProcessDestructionQueues(size_t commandBufferIndex)
{
	auto& current_descriptor_cache = m_destructionQueues.m_cmd_descriptor_set_objects[commandBufferIndex];
	if (!current_descriptor_cache.empty())
	{
		assert_dbg();
		//for (const auto& descriptor : current_descriptor_cache)
		//{
		//	vkFreeDescriptorSets(m_logicalDevice, m_descriptorPool, 1, &descriptor);
		//	performanceMonitor.vk.numDescriptorSets.decrement();
		//}

		current_descriptor_cache.clear();
	}

	// destroy buffers
	for (auto& itr : m_destructionQueues.m_buffers[commandBufferIndex])
		vkDestroyBuffer(m_logicalDevice, itr, nullptr);
	m_destructionQueues.m_buffers[commandBufferIndex].clear();

	// destroy device memory objects
	for (auto& itr : m_destructionQueues.m_memory[commandBufferIndex])
		vkFreeMemory(m_logicalDevice, itr, nullptr);
	m_destructionQueues.m_memory[commandBufferIndex].clear();

	// destroy image views
	for (auto& itr : m_destructionQueues.m_cmd_image_views[commandBufferIndex])
		vkDestroyImageView(m_logicalDevice, itr, nullptr);
	m_destructionQueues.m_cmd_image_views[commandBufferIndex].clear();

	// destroy host textures
	for (auto itr : m_destructionQueues.m_host_textures[commandBufferIndex])
		delete itr;
	m_destructionQueues.m_host_textures[commandBufferIndex].clear();

	ProcessDestructionQueue2();
}

void VulkanRenderer::InitFirstCommandBuffer()
{
	cemu_assert_debug(m_state.currentCommandBuffer == nullptr);
	// m_commandBufferIndex always points to the currently used command buffer, so we set it to 0
	m_commandBufferIndex = 0;
	m_commandBufferSyncIndex = 0;

	m_state.currentCommandBuffer = m_commandBuffers[m_commandBufferIndex];
	vkResetFences(m_logicalDevice, 1, &m_cmd_buffer_fences[m_commandBufferIndex]);
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	vkBeginCommandBuffer(m_state.currentCommandBuffer, &beginInfo);

	vkCmdSetViewport(m_state.currentCommandBuffer, 0, 1, &m_state.currentViewport);
	vkCmdSetScissor(m_state.currentCommandBuffer, 0, 1, &m_state.currentScissorRect);

	m_state.resetCommandBufferState();
}

void VulkanRenderer::ProcessFinishedCommandBuffers()
{
	bool finishedCmdBuffers = false;
	while (m_commandBufferSyncIndex != m_commandBufferIndex)
	{
		VkResult fenceStatus = vkGetFenceStatus(m_logicalDevice, m_cmd_buffer_fences[m_commandBufferSyncIndex]);
		if (fenceStatus == VK_SUCCESS)
		{
			ProcessDestructionQueues(m_commandBufferSyncIndex);
			m_commandBufferSyncIndex = (m_commandBufferSyncIndex + 1) % m_commandBuffers.size();
			memoryManager->cleanupBuffers(m_countCommandBufferFinished);
			m_countCommandBufferFinished++;
			finishedCmdBuffers = true;
			continue;
		}
		else if (fenceStatus == VK_NOT_READY)
		{
			// not signaled
			break;
		}
		cemuLog_log(LogType::Force, "vkGetFenceStatus returned unexpected error {}", (sint32)fenceStatus);
		cemu_assert_debug(false);
	}
	if (finishedCmdBuffers)
	{
		LatteTextureReadback_UpdateFinishedTransfers(false);
	}
}

void VulkanRenderer::WaitForNextFinishedCommandBuffer()
{
	cemu_assert_debug(m_commandBufferSyncIndex != m_commandBufferIndex);
	// wait on least recently submitted command buffer
	VkResult result = vkWaitForFences(m_logicalDevice, 1, &m_cmd_buffer_fences[m_commandBufferSyncIndex], true, UINT64_MAX);
	if (result == VK_TIMEOUT)
	{
		cemuLog_log(LogType::Force, "vkWaitForFences: Returned VK_TIMEOUT on infinite fence");
	}
	else if (result != VK_SUCCESS)
	{
		cemuLog_log(LogType::Force, "vkWaitForFences: Returned unhandled error {}", (sint32)result);
	}
	// process
	ProcessFinishedCommandBuffers();
}

void VulkanRenderer::SubmitCommandBuffer(VkSemaphore signalSemaphore, VkSemaphore waitSemaphore)
{
	draw_endRenderPass();

	occlusionQuery_notifyEndCommandBuffer();

	vkEndCommandBuffer(m_state.currentCommandBuffer);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_state.currentCommandBuffer;

	// signal current command buffer semaphore
	VkSemaphore signalSemArray[2];
	if (signalSemaphore != VK_NULL_HANDLE)
	{
		submitInfo.signalSemaphoreCount = 2;
		signalSemArray[0] = m_commandBufferSemaphores[m_commandBufferIndex]; // signal current
		signalSemArray[1] = signalSemaphore; // signal current
		submitInfo.pSignalSemaphores = signalSemArray;
	}
	else
	{
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &m_commandBufferSemaphores[m_commandBufferIndex]; // signal current
	}

	// wait for previous command buffer semaphore
	VkSemaphore prevSem = GetLastSubmittedCmdBufferSemaphore();
	const VkPipelineStageFlags semWaitStageMask[2] = { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT };
	VkSemaphore waitSemArray[2];
	submitInfo.waitSemaphoreCount = 0;
	if (m_numSubmittedCmdBuffers > 0)
		waitSemArray[submitInfo.waitSemaphoreCount++] = prevSem; // wait on semaphore from previous submit
	if (waitSemaphore != VK_NULL_HANDLE)
		waitSemArray[submitInfo.waitSemaphoreCount++] = waitSemaphore;
	submitInfo.pWaitDstStageMask = semWaitStageMask;
	submitInfo.pWaitSemaphores = waitSemArray;

	const VkResult result = vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_cmd_buffer_fences[m_commandBufferIndex]);
	if (result != VK_SUCCESS)
		UnrecoverableError(fmt::format("failed to submit command buffer. Error {}", result).c_str());
	m_numSubmittedCmdBuffers++;

	// check if any previously submitted command buffers have finished execution
	ProcessFinishedCommandBuffers();

	// acquire next command buffer
	auto nextCmdBufferIndex = (m_commandBufferIndex + 1) % m_commandBuffers.size();
	if (nextCmdBufferIndex == m_commandBufferSyncIndex)
	{
		// force wait for the next command buffer
		cemuLog_logDebug(LogType::Force, "Vulkan: Waiting for available command buffer...");
		WaitForNextFinishedCommandBuffer();
	}
	m_commandBufferIndex = nextCmdBufferIndex;


	m_state.currentCommandBuffer = m_commandBuffers[m_commandBufferIndex];
	vkResetFences(m_logicalDevice, 1, &m_cmd_buffer_fences[m_commandBufferIndex]);
	vkResetCommandBuffer(m_state.currentCommandBuffer, 0);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	vkBeginCommandBuffer(m_state.currentCommandBuffer, &beginInfo);

	// make sure some states are set for this command buffer
	vkCmdSetViewport(m_state.currentCommandBuffer, 0, 1, &m_state.currentViewport);
	vkCmdSetScissor(m_state.currentCommandBuffer, 0, 1, &m_state.currentScissorRect);

	// DEBUG
	//debug_genericBarrier();

	// reset states which are bound to a command buffer
	m_state.resetCommandBufferState();

	occlusionQuery_notifyBeginCommandBuffer();

	m_recordedDrawcalls = 0;
	m_submitThreshold = 300;
	m_submitOnIdle = false;
}

// submit within next 10 drawcalls
void VulkanRenderer::RequestSubmitSoon()
{
	m_submitThreshold = std::min(m_submitThreshold, m_recordedDrawcalls + 10);
}

// command buffer will be submitted when GPU has no more commands to process or when threshold is reached
void VulkanRenderer::RequestSubmitOnIdle()
{
	m_submitOnIdle = true;
}

uint64 VulkanRenderer::GetCurrentCommandBufferId() const
{
	return m_numSubmittedCmdBuffers;
}

bool VulkanRenderer::HasCommandBufferFinished(uint64 commandBufferId) const
{
	return m_countCommandBufferFinished > commandBufferId;
}

void VulkanRenderer::WaitCommandBufferFinished(uint64 commandBufferId)
{
	if (commandBufferId == m_numSubmittedCmdBuffers)
		SubmitCommandBuffer();
	while (HasCommandBufferFinished(commandBufferId) == false)
		WaitForNextFinishedCommandBuffer();
}

void VulkanRenderer::PipelineCacheSaveThread(size_t cache_size)
{
	const auto dir = ActiveSettings::GetCachePath("shaderCache/driver/vk");
	if (!fs::exists(dir))
	{
		try
		{
			fs::create_directories(dir);
		}
		catch (const std::exception& ex)
		{
			cemuLog_log(LogType::Force, "can't create vulkan pipeline cache directory \"{}\": {}", _pathToUtf8(dir), ex.what());
			return;
		}
	}

	const auto filename = dir / fmt::format(L"{:016x}.bin", CafeSystem::GetForegroundTitleId());

	while (true)
	{
		if (m_destructionRequested)
			return;
		m_pipeline_cache_semaphore.wait();
		if (m_destructionRequested)
			return;
		for (sint32 i = 0; i < 15 * 4; i++)
		{
			if (m_destructionRequested)
				return;
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
		}

		// always prioritize the compiler threads over this thread
		// avoid calling stalling lock() since it will block other threads from entering even when the lock is currently held in shared mode
		while (!m_pipeline_cache_save_mutex.try_lock())
			std::this_thread::sleep_for(std::chrono::milliseconds(250));

		size_t size = 0;
		VkResult res = vkGetPipelineCacheData(m_logicalDevice, m_pipeline_cache, &size, nullptr);
		if (res == VK_SUCCESS && size > 0 && size != cache_size)
		{
			std::vector<uint8_t> cacheData(size);
			res = vkGetPipelineCacheData(m_logicalDevice, m_pipeline_cache, &size, cacheData.data());
			m_pipeline_cache_semaphore.reset();
			m_pipeline_cache_save_mutex.unlock();

			if (res == VK_SUCCESS)
			{

				auto file = std::ofstream(filename, std::ios::out | std::ios::binary);
				if (file.is_open())
				{
					file.write((char*)cacheData.data(), cacheData.size());
					file.close();

					cache_size = size;
					cemuLog_logDebug(LogType::Force, "pipeline cache saved");
				}
				else
				{
					cemuLog_log(LogType::Force, "can't write pipeline cache to disk");
				}
			}
			else
			{
				cemuLog_log(LogType::Force, "can't retrieve pipeline cache data: 0x{:x}", res);
			}
		}
		else
		{
			m_pipeline_cache_semaphore.reset();
			m_pipeline_cache_save_mutex.unlock();
		}
	}
}

void VulkanRenderer::CreatePipelineCache()
{
	std::vector<uint8_t> cacheData;
	const auto dir = ActiveSettings::GetCachePath("shaderCache/driver/vk");
	if (fs::exists(dir))
	{
		const auto filename = dir / fmt::format("{:016x}.bin", CafeSystem::GetForegroundTitleId());
		auto file = std::ifstream(filename, std::ios::in | std::ios::binary | std::ios::ate);
		if (file.is_open())
		{
			const size_t fileSize = file.tellg();
			file.seekg(0, std::ifstream::beg);
			cacheData.resize(fileSize);
			file.read((char*)cacheData.data(), cacheData.size());
			file.close();
		}
	}

	VkPipelineCacheCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	createInfo.initialDataSize = cacheData.size();
	createInfo.pInitialData = cacheData.data();
	VkResult result = vkCreatePipelineCache(m_logicalDevice, &createInfo, nullptr, &m_pipeline_cache);
	if (result != VK_SUCCESS)
	{
		cemuLog_log(LogType::Force, "Failed to open Vulkan pipeline cache: {}", result);
		// unable to load the existing cache, start with an empty cache instead
		createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		createInfo.initialDataSize = 0;
		createInfo.pInitialData = nullptr;
		result = vkCreatePipelineCache(m_logicalDevice, &createInfo, nullptr, &m_pipeline_cache);
		if (result != VK_SUCCESS)
			UnrecoverableError(fmt::format("Failed to create new Vulkan pipeline cache: {}", result).c_str());
	}

	size_t cache_size = 0;
	vkGetPipelineCacheData(m_logicalDevice, m_pipeline_cache, &cache_size, nullptr);

	m_pipeline_cache_save_thread = std::thread(&VulkanRenderer::PipelineCacheSaveThread, this, cache_size);
}

void VulkanRenderer::swapchain_createDescriptorSetLayout()
{
	VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
	samplerLayoutBinding.binding = 0;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.pImmutableSamplers = nullptr;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutBinding bindings[] = { samplerLayoutBinding };
	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = std::size(bindings);
	layoutInfo.pBindings = bindings;

	if (vkCreateDescriptorSetLayout(m_logicalDevice, &layoutInfo, nullptr, &m_swapchainDescriptorSetLayout) != VK_SUCCESS)
		UnrecoverableError("failed to create descriptor set layout for swapchain");
}

void VulkanRenderer::GetTextureFormatInfoVK(Latte::E_GX2SURFFMT format, bool isDepth, Latte::E_DIM dim, sint32 width, sint32 height, FormatInfoVK* formatInfoOut)
{
	formatInfoOut->texelCountX = width;
	formatInfoOut->texelCountY = height;
	formatInfoOut->isCompressed = false;
	if (isDepth)
	{
		switch (format)
		{
		case Latte::E_GX2SURFFMT::D24_S8_UNORM:
			if (m_supportedFormatInfo.fmt_d24_unorm_s8_uint == false)
			{
				formatInfoOut->vkImageFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
				formatInfoOut->vkImageAspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
				formatInfoOut->decoder = TextureDecoder_NullData64::getInstance();
			}
			else
			{
				formatInfoOut->vkImageFormat = VK_FORMAT_D24_UNORM_S8_UINT;
				formatInfoOut->vkImageAspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
				formatInfoOut->decoder = TextureDecoder_D24_S8::getInstance();
			}
			break;
		case Latte::E_GX2SURFFMT::D24_S8_FLOAT:
			// alternative format
			formatInfoOut->vkImageFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
			formatInfoOut->vkImageAspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
			formatInfoOut->decoder = TextureDecoder_NullData64::getInstance();
			break;
		case Latte::E_GX2SURFFMT::D32_FLOAT:
			formatInfoOut->vkImageFormat = VK_FORMAT_D32_SFLOAT;
			formatInfoOut->vkImageAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
			formatInfoOut->decoder = TextureDecoder_R32_FLOAT::getInstance();
			break;
		case Latte::E_GX2SURFFMT::D16_UNORM:
			formatInfoOut->vkImageFormat = VK_FORMAT_D16_UNORM;
			formatInfoOut->vkImageAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
			formatInfoOut->decoder = TextureDecoder_R16_UNORM::getInstance();
			break;
		case Latte::E_GX2SURFFMT::D32_S8_FLOAT:
			formatInfoOut->vkImageFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
			formatInfoOut->vkImageAspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
			formatInfoOut->decoder = TextureDecoder_D32_S8_UINT_X24::getInstance();
			break;
		default:
			cemuLog_log(LogType::Force, "Unsupported depth texture format {:04x}", (uint32)format);
			// default to placeholder format
			formatInfoOut->vkImageFormat = VK_FORMAT_D16_UNORM;
			formatInfoOut->vkImageAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
			formatInfoOut->decoder = nullptr;
			break;
		}
	}
	else
	{
		formatInfoOut->vkImageAspect = VK_IMAGE_ASPECT_COLOR_BIT;
		switch (format)
		{
			// RGBA formats
		case Latte::E_GX2SURFFMT::R32_G32_B32_A32_FLOAT:
			formatInfoOut->vkImageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
			formatInfoOut->decoder = TextureDecoder_R32_G32_B32_A32_FLOAT::getInstance();
			break;
		case Latte::E_GX2SURFFMT::R32_G32_B32_A32_UINT:
			formatInfoOut->vkImageFormat = VK_FORMAT_R32G32B32A32_UINT;
			formatInfoOut->decoder = TextureDecoder_R32_G32_B32_A32_UINT::getInstance();
			break;
		case Latte::E_GX2SURFFMT::R16_G16_B16_A16_FLOAT:
			formatInfoOut->vkImageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
			formatInfoOut->decoder = TextureDecoder_R16_G16_B16_A16_FLOAT::getInstance();
			break;
		case Latte::E_GX2SURFFMT::R16_G16_B16_A16_UINT:
			formatInfoOut->vkImageFormat = VK_FORMAT_R16G16B16A16_UINT;
			formatInfoOut->decoder = TextureDecoder_R16_G16_B16_A16_UINT::getInstance();
			break;
		case Latte::E_GX2SURFFMT::R16_G16_B16_A16_UNORM:
			formatInfoOut->vkImageFormat = VK_FORMAT_R16G16B16A16_UNORM;
			formatInfoOut->decoder = TextureDecoder_R16_G16_B16_A16::getInstance();
			break;
		case Latte::E_GX2SURFFMT::R16_G16_B16_A16_SNORM:
			formatInfoOut->vkImageFormat = VK_FORMAT_R16G16B16A16_SNORM;
			formatInfoOut->decoder = TextureDecoder_R16_G16_B16_A16::getInstance();
			break;
		case Latte::E_GX2SURFFMT::R8_G8_B8_A8_UNORM:
			formatInfoOut->vkImageFormat = VK_FORMAT_R8G8B8A8_UNORM;
			formatInfoOut->decoder = TextureDecoder_R8_G8_B8_A8::getInstance();
			break;
		case Latte::E_GX2SURFFMT::R8_G8_B8_A8_SNORM:
			formatInfoOut->vkImageFormat = VK_FORMAT_R8G8B8A8_SNORM;
			formatInfoOut->decoder = TextureDecoder_R8_G8_B8_A8::getInstance();
			break;
		case Latte::E_GX2SURFFMT::R8_G8_B8_A8_SRGB:
			formatInfoOut->vkImageFormat = VK_FORMAT_R8G8B8A8_SRGB;
			formatInfoOut->decoder = TextureDecoder_R8_G8_B8_A8::getInstance();
			break;
		case Latte::E_GX2SURFFMT::R8_G8_B8_A8_UINT:
			formatInfoOut->vkImageFormat = VK_FORMAT_R8G8B8A8_UINT;
			formatInfoOut->decoder = TextureDecoder_R8_G8_B8_A8::getInstance();
			break;
		case Latte::E_GX2SURFFMT::R8_G8_B8_A8_SINT:
			formatInfoOut->vkImageFormat = VK_FORMAT_R8G8B8A8_SINT;
			formatInfoOut->decoder = TextureDecoder_R8_G8_B8_A8::getInstance();
			break;
			// RG formats
		case Latte::E_GX2SURFFMT::R32_G32_FLOAT:
			formatInfoOut->vkImageFormat = VK_FORMAT_R32G32_SFLOAT;
			formatInfoOut->decoder = TextureDecoder_R32_G32_FLOAT::getInstance();
			break;
		case Latte::E_GX2SURFFMT::R32_G32_UINT:
			formatInfoOut->vkImageFormat = VK_FORMAT_R32G32_UINT;
			formatInfoOut->decoder = TextureDecoder_R32_G32_UINT::getInstance();
			break;
		case Latte::E_GX2SURFFMT::R16_G16_UNORM:
			formatInfoOut->vkImageFormat = VK_FORMAT_R16G16_UNORM;
			formatInfoOut->decoder = TextureDecoder_R16_G16::getInstance();
			break;
		case Latte::E_GX2SURFFMT::R16_G16_FLOAT:
			formatInfoOut->vkImageFormat = VK_FORMAT_R16G16_SFLOAT;
			formatInfoOut->decoder = TextureDecoder_R16_G16_FLOAT::getInstance();
			break;
		case Latte::E_GX2SURFFMT::R8_G8_UNORM:
			formatInfoOut->vkImageFormat = VK_FORMAT_R8G8_UNORM;
			formatInfoOut->decoder = TextureDecoder_R8_G8::getInstance();
			break;
		case Latte::E_GX2SURFFMT::R8_G8_SNORM:
			formatInfoOut->vkImageFormat = VK_FORMAT_R8G8_SNORM;
			formatInfoOut->decoder = TextureDecoder_R8_G8::getInstance();
			break;
		case Latte::E_GX2SURFFMT::R4_G4_UNORM:
			if (m_supportedFormatInfo.fmt_r4g4_unorm_pack == false)
			{
				if (m_supportedFormatInfo.fmt_r4g4b4a4_unorm_pack == false) {
					formatInfoOut->vkImageFormat = VK_FORMAT_R8G8B8A8_UNORM;
					formatInfoOut->decoder = TextureDecoder_R4G4_UNORM_To_RGBA8::getInstance();
				}
				else {
					formatInfoOut->vkImageFormat = VK_FORMAT_R4G4B4A4_UNORM_PACK16;
					formatInfoOut->decoder = TextureDecoder_R4_G4_UNORM_To_RGBA4_vk::getInstance();
				}
			}
			else
			{
				formatInfoOut->vkImageFormat = VK_FORMAT_R4G4_UNORM_PACK8;
				formatInfoOut->decoder = TextureDecoder_R4_G4::getInstance();
			}
			break;
			// R formats
		case Latte::E_GX2SURFFMT::R32_FLOAT:
			formatInfoOut->vkImageFormat = VK_FORMAT_R32_SFLOAT;
			formatInfoOut->decoder = TextureDecoder_R32_FLOAT::getInstance();
			break;
		case Latte::E_GX2SURFFMT::R32_UINT:
			formatInfoOut->vkImageFormat = VK_FORMAT_R32_UINT;
			formatInfoOut->decoder = TextureDecoder_R32_UINT::getInstance();
			break;
		case Latte::E_GX2SURFFMT::R16_FLOAT:
			formatInfoOut->vkImageFormat = VK_FORMAT_R16_SFLOAT;
			formatInfoOut->decoder = TextureDecoder_R16_FLOAT::getInstance();
			break;
		case Latte::E_GX2SURFFMT::R16_UNORM:
			formatInfoOut->vkImageFormat = VK_FORMAT_R16_UNORM;
			formatInfoOut->decoder = TextureDecoder_R16_UNORM::getInstance();
			break;
		case Latte::E_GX2SURFFMT::R16_SNORM:
			formatInfoOut->vkImageFormat = VK_FORMAT_R16_SNORM;
			formatInfoOut->decoder = TextureDecoder_R16_SNORM::getInstance();
			break;
		case Latte::E_GX2SURFFMT::R16_UINT:
			formatInfoOut->vkImageFormat = VK_FORMAT_R16_UINT;
			formatInfoOut->decoder = TextureDecoder_R16_UINT::getInstance();
			break;
		case Latte::E_GX2SURFFMT::R8_UNORM:
			formatInfoOut->vkImageFormat = VK_FORMAT_R8_UNORM;
			formatInfoOut->decoder = TextureDecoder_R8::getInstance();
			break;
		case Latte::E_GX2SURFFMT::R8_SNORM:
			formatInfoOut->vkImageFormat = VK_FORMAT_R8_SNORM;
			formatInfoOut->decoder = TextureDecoder_R8::getInstance();
			break;
		case Latte::E_GX2SURFFMT::R8_UINT:
			formatInfoOut->vkImageFormat = VK_FORMAT_R8_UINT;
			formatInfoOut->decoder = TextureDecoder_R8_UINT::getInstance();
			break;
			// special formats
		case Latte::E_GX2SURFFMT::R5_G6_B5_UNORM:
			if (m_supportedFormatInfo.fmt_r5g6b5_unorm_pack == false) {
				formatInfoOut->vkImageFormat = VK_FORMAT_R8G8B8A8_UNORM;
				formatInfoOut->decoder = TextureDecoder_R5G6B5_UNORM_To_RGBA8::getInstance();
			}
			else {
				// Vulkan has R in MSB, GPU7 has it in LSB
				formatInfoOut->vkImageFormat = VK_FORMAT_R5G6B5_UNORM_PACK16;
				formatInfoOut->decoder = TextureDecoder_R5_G6_B5_swappedRB::getInstance();
			}
			break;
		case Latte::E_GX2SURFFMT::R5_G5_B5_A1_UNORM:
			if (m_supportedFormatInfo.fmt_a1r5g5b5_unorm_pack == false) {
				formatInfoOut->vkImageFormat = VK_FORMAT_R8G8B8A8_UNORM;
				formatInfoOut->decoder = TextureDecoder_R5_G5_B5_A1_UNORM_swappedRB_To_RGBA8::getInstance();
			}
			else {
				// used in Super Mario 3D World for the hidden Luigi sprites
				// since order of channels is reversed in Vulkan compared to GX2 the format we need is A1B5G5R5
				formatInfoOut->vkImageFormat = VK_FORMAT_A1R5G5B5_UNORM_PACK16;
				formatInfoOut->decoder = TextureDecoder_R5_G5_B5_A1_UNORM_swappedRB::getInstance();
			}
			break;
		case Latte::E_GX2SURFFMT::A1_B5_G5_R5_UNORM:
			if (m_supportedFormatInfo.fmt_a1r5g5b5_unorm_pack == false) {
				formatInfoOut->vkImageFormat = VK_FORMAT_R8G8B8A8_UNORM;
				formatInfoOut->decoder = TextureDecoder_A1_B5_G5_R5_UNORM_vulkan_To_RGBA8::getInstance();
			}
			else {
				// used by VC64 (e.g. Ocarina of Time)
				formatInfoOut->vkImageFormat = VK_FORMAT_A1R5G5B5_UNORM_PACK16; // A 15 R 10..14, G 5..9 B 0..4
				formatInfoOut->decoder = TextureDecoder_A1_B5_G5_R5_UNORM_vulkan::getInstance();
			}
			break;
		case Latte::E_GX2SURFFMT::R11_G11_B10_FLOAT:
			formatInfoOut->vkImageFormat = VK_FORMAT_B10G11R11_UFLOAT_PACK32; // verify if order of channels is still the same as GX2
			formatInfoOut->decoder = TextureDecoder_R11_G11_B10_FLOAT::getInstance();
			break;
		case Latte::E_GX2SURFFMT::R4_G4_B4_A4_UNORM:
			if (m_supportedFormatInfo.fmt_r4g4b4a4_unorm_pack == false) {
				formatInfoOut->vkImageFormat = VK_FORMAT_R8G8B8A8_UNORM;
				formatInfoOut->decoder = TextureDecoder_R4G4B4A4_UNORM_To_RGBA8::getInstance();
			}
			else {
				formatInfoOut->vkImageFormat = VK_FORMAT_R4G4B4A4_UNORM_PACK16;
 				formatInfoOut->decoder = TextureDecoder_R4_G4_B4_A4_UNORM::getInstance();
			}
			break;
			// special formats - R10G10B10_A2
		case Latte::E_GX2SURFFMT::R10_G10_B10_A2_UNORM:
			formatInfoOut->vkImageFormat = VK_FORMAT_A2B10G10R10_UNORM_PACK32; // todo - verify
			formatInfoOut->decoder = TextureDecoder_R10_G10_B10_A2_UNORM::getInstance();
			break;
		case Latte::E_GX2SURFFMT::R10_G10_B10_A2_SNORM:
			formatInfoOut->vkImageFormat = VK_FORMAT_R16G16B16A16_SNORM; // Vulkan has VK_FORMAT_A2R10G10B10_SNORM_PACK32 but it doesnt work?
			formatInfoOut->decoder = TextureDecoder_R10_G10_B10_A2_SNORM_To_RGBA16::getInstance();
			break;
		case Latte::E_GX2SURFFMT::R10_G10_B10_A2_SRGB:
			//formatInfoOut->vkImageFormat = VK_FORMAT_R16G16B16A16_SNORM; // Vulkan has no uncompressed SRGB format with more than 8 bits per channel
			//formatInfoOut->decoder = TextureDecoder_R10_G10_B10_A2_SNORM_To_RGBA16::getInstance();
			//break;
			formatInfoOut->vkImageFormat = VK_FORMAT_A2B10G10R10_UNORM_PACK32; // todo - verify
			formatInfoOut->decoder = TextureDecoder_R10_G10_B10_A2_UNORM::getInstance();
			break;
			// compressed formats
		case Latte::E_GX2SURFFMT::BC1_SRGB:
			formatInfoOut->vkImageFormat = VK_FORMAT_BC1_RGBA_SRGB_BLOCK; // todo - verify
			formatInfoOut->decoder = TextureDecoder_BC1::getInstance();
			break;
		case Latte::E_GX2SURFFMT::BC1_UNORM:
			formatInfoOut->vkImageFormat = VK_FORMAT_BC1_RGBA_UNORM_BLOCK; // todo - verify
			formatInfoOut->decoder = TextureDecoder_BC1::getInstance();
			break;
		case Latte::E_GX2SURFFMT::BC2_UNORM:
			formatInfoOut->vkImageFormat = VK_FORMAT_BC2_UNORM_BLOCK; // todo - verify
			formatInfoOut->decoder = TextureDecoder_BC2::getInstance();
			break;
		case Latte::E_GX2SURFFMT::BC2_SRGB:
			formatInfoOut->vkImageFormat = VK_FORMAT_BC2_SRGB_BLOCK; // todo - verify
			formatInfoOut->decoder = TextureDecoder_BC2::getInstance();
			break;
		case Latte::E_GX2SURFFMT::BC3_UNORM:
			formatInfoOut->vkImageFormat = VK_FORMAT_BC3_UNORM_BLOCK;
			formatInfoOut->decoder = TextureDecoder_BC3::getInstance();
			break;
		case Latte::E_GX2SURFFMT::BC3_SRGB:
			formatInfoOut->vkImageFormat = VK_FORMAT_BC3_SRGB_BLOCK;
			formatInfoOut->decoder = TextureDecoder_BC3::getInstance();
			break;
		case Latte::E_GX2SURFFMT::BC4_UNORM:
			formatInfoOut->vkImageFormat = VK_FORMAT_BC4_UNORM_BLOCK;
			formatInfoOut->decoder = TextureDecoder_BC4::getInstance();
			break;
		case Latte::E_GX2SURFFMT::BC4_SNORM:
			formatInfoOut->vkImageFormat = VK_FORMAT_BC4_SNORM_BLOCK;
			formatInfoOut->decoder = TextureDecoder_BC4::getInstance();
			break;
		case Latte::E_GX2SURFFMT::BC5_UNORM:
			formatInfoOut->vkImageFormat = VK_FORMAT_BC5_UNORM_BLOCK;
			formatInfoOut->decoder = TextureDecoder_BC5::getInstance();
			break;
		case Latte::E_GX2SURFFMT::BC5_SNORM:
			formatInfoOut->vkImageFormat = VK_FORMAT_BC5_SNORM_BLOCK;
			formatInfoOut->decoder = TextureDecoder_BC5::getInstance();
			break;
		case Latte::E_GX2SURFFMT::R24_X8_UNORM:
			formatInfoOut->vkImageFormat = VK_FORMAT_R32_SFLOAT;
			formatInfoOut->decoder = TextureDecoder_R24_X8::getInstance();
			break;
		case Latte::E_GX2SURFFMT::X24_G8_UINT:
			// used by Color Splash and Resident Evil
			formatInfoOut->vkImageFormat = VK_FORMAT_R8G8B8A8_UINT; // todo - should we use ABGR format?
			formatInfoOut->decoder = TextureDecoder_X24_G8_UINT::getInstance(); // todo - verify
		default:
			cemuLog_log(LogType::Force, "Unsupported color texture format {:04x}", (uint32)format);
			cemu_assert_debug(false);
		}
	}
}

VkPipelineShaderStageCreateInfo VulkanRenderer::CreatePipelineShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule& module, const char* entryName) const
{
	VkPipelineShaderStageCreateInfo shaderStageInfo{};
	shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageInfo.stage = stage;
	shaderStageInfo.module = module;
	shaderStageInfo.pName = entryName;
	return shaderStageInfo;
}

VkPipeline VulkanRenderer::backbufferBlit_createGraphicsPipeline(VkDescriptorSetLayout descriptorLayout, bool padView, RendererOutputShader* shader)
{
	auto& chainInfo = GetChainInfo(!padView);

	RendererShaderVk* vertexRendererShader = static_cast<RendererShaderVk*>(shader->GetVertexShader());
	RendererShaderVk* fragmentRendererShader = static_cast<RendererShaderVk*>(shader->GetFragmentShader());

	uint64 hash = 0;
	hash += (uint64)vertexRendererShader;
	hash += (uint64)fragmentRendererShader;
	hash += (uint64)(chainInfo.m_usesSRGB);
	hash += ((uint64)padView) << 1;

	static std::unordered_map<uint64, VkPipeline> s_pipeline_cache;
	const auto it = s_pipeline_cache.find(hash);
	if (it != s_pipeline_cache.cend())
		return it->second;

	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
	if (vertexRendererShader)
		shaderStages.emplace_back(CreatePipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertexRendererShader->GetShaderModule(), "main"));

	if (fragmentRendererShader)
		shaderStages.emplace_back(CreatePipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentRendererShader->GetShaderModule(), "main"));

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = std::size(dynamicStates);
	dynamicState.pDynamicStates = dynamicStates;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &descriptorLayout;

	VkResult result = vkCreatePipelineLayout(m_logicalDevice, &pipelineLayoutInfo, nullptr, &m_pipelineLayout);
	if (result != VK_SUCCESS)
		throw std::runtime_error(fmt::format("Failed to create pipeline layout: {}", result));

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = shaderStages.size();
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.layout = m_pipelineLayout;
	pipelineInfo.renderPass = chainInfo.m_swapchainRenderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

	VkPipeline pipeline = nullptr;
	std::shared_lock lock(m_pipeline_cache_save_mutex);
	result = vkCreateGraphicsPipelines(m_logicalDevice, m_pipeline_cache, 1, &pipelineInfo, nullptr, &pipeline);
	if (result != VK_SUCCESS)
	{
		cemuLog_log(LogType::Force, "Failed to create graphics pipeline. Error {}", result);
		throw std::runtime_error(fmt::format("Failed to create graphics pipeline: {}", result));
	}

	s_pipeline_cache[hash] = pipeline;
	m_pipeline_cache_semaphore.notify();

	return pipeline;
}

bool VulkanRenderer::AcquireNextSwapchainImage(bool mainWindow)
{
	if(!IsSwapchainInfoValid(mainWindow))
		return false;

	if(!mainWindow && m_destroyPadSwapchainNextAcquire)
	{
		RecreateSwapchain(mainWindow, true);
		m_destroyPadSwapchainNextAcquire = false;
		m_padCloseReadySemaphore.notify();
		return false;
	}

	auto& chainInfo = GetChainInfo(mainWindow);

	if (chainInfo.swapchainImageIndex != -1)
		return true; // image already reserved

	if (!UpdateSwapchainProperties(mainWindow))
		return false;

	bool result = chainInfo.AcquireImage(UINT64_MAX);
	if (!result)
		return false;

	SubmitCommandBuffer(VK_NULL_HANDLE, chainInfo.ConsumeAcquireSemaphore());
	return true;
}

void VulkanRenderer::RecreateSwapchain(bool mainWindow, bool skipCreate)
{
	SubmitCommandBuffer();
	WaitDeviceIdle();
	auto& chainInfo = GetChainInfo(mainWindow);
	// make sure fence has no signal operation submitted
	chainInfo.WaitAvailableFence();

	Vector2i size;
	if (mainWindow)
	{
		ImGui_ImplVulkan_Shutdown();
		gui_getWindowPhysSize(size.x, size.y);
	}
	else
	{
		gui_getPadWindowPhysSize(size.x, size.y);
	}

	chainInfo.swapchainImageIndex = -1;
	chainInfo.Cleanup();
	chainInfo.m_desiredExtent = size;
	if(!skipCreate)
	{
		chainInfo.Create(m_physicalDevice, m_logicalDevice);
	}

	if (mainWindow)
		ImguiInit();
}

bool VulkanRenderer::UpdateSwapchainProperties(bool mainWindow)
{
	auto& chainInfo = GetChainInfo(mainWindow);
	bool stateChanged = chainInfo.m_shouldRecreate;

	const auto configValue =  (VSync)GetConfig().vsync.GetValue();
	if(chainInfo.m_vsyncState != configValue)
		stateChanged = true;

	const bool latteBufferUsesSRGB = mainWindow ? LatteGPUState.tvBufferUsesSRGB : LatteGPUState.drcBufferUsesSRGB;
	if (chainInfo.m_usesSRGB != latteBufferUsesSRGB)
		stateChanged = true;

	int width, height;
	if (mainWindow)
		gui_getWindowPhysSize(width, height);
	else
		gui_getPadWindowPhysSize(width, height);
	auto extent = chainInfo.getExtent();
	if (width != extent.width || height != extent.height)
		stateChanged = true;

	if(stateChanged)
	{
		try
		{
			RecreateSwapchain(mainWindow);
		}
		catch (std::exception&)
		{
			cemu_assert_debug(false);
			return false;
		}
	}

	chainInfo.m_shouldRecreate = false;
	chainInfo.m_vsyncState = configValue;
	chainInfo.m_usesSRGB = latteBufferUsesSRGB;
	return true;
}

void VulkanRenderer::SwapBuffer(bool mainWindow)
{
	if(!AcquireNextSwapchainImage(mainWindow))
		return;

	auto& chainInfo = GetChainInfo(mainWindow);

	if (!chainInfo.hasDefinedSwapchainImage)
	{
		// set the swapchain image to a defined state
		VkClearColorValue clearColor{ 0, 0, 0, 0 };
		ClearColorImageRaw(chainInfo.m_swapchainImages[chainInfo.swapchainImageIndex], 0, 0, clearColor, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	}

	VkSemaphore presentSemaphore = chainInfo.m_presentSemaphores[chainInfo.swapchainImageIndex];
	SubmitCommandBuffer(presentSemaphore); // submit all command and signal semaphore

	cemu_assert_debug(m_numSubmittedCmdBuffers > 0);

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &chainInfo.swapchain;
	presentInfo.pImageIndices = &chainInfo.swapchainImageIndex;
	// wait on command buffer semaphore
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &presentSemaphore;

	VkResult result = vkQueuePresentKHR(m_presentQueue, &presentInfo);
	if (result < 0 && result != VK_ERROR_OUT_OF_DATE_KHR)
	{
		throw std::runtime_error(fmt::format("Failed to present image: {}", result));
	}
	if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		chainInfo.m_shouldRecreate = true;

	chainInfo.hasDefinedSwapchainImage = false;

	chainInfo.swapchainImageIndex = -1;
}

void VulkanRenderer::Flush(bool waitIdle)
{
	if (m_recordedDrawcalls > 0 || m_submitOnIdle)
		SubmitCommandBuffer();
	if (waitIdle)
		WaitCommandBufferFinished(GetCurrentCommandBufferId());
}

void VulkanRenderer::NotifyLatteCommandProcessorIdle()
{
	if (m_submitOnIdle)
		SubmitCommandBuffer();
}

void VulkanBenchmarkPrintResults();

void VulkanRenderer::SwapBuffers(bool swapTV, bool swapDRC)
{
	SubmitCommandBuffer();

	if (swapTV && IsSwapchainInfoValid(true))
		SwapBuffer(true);

	if (swapDRC && IsSwapchainInfoValid(false))
		SwapBuffer(false);

	if(swapTV)
		VulkanBenchmarkPrintResults();
}

void VulkanRenderer::ClearColorbuffer(bool padView)
{
	if (!IsSwapchainInfoValid(!padView))
		return;

	auto& chainInfo = GetChainInfo(!padView);
	if (chainInfo.swapchainImageIndex == -1)
		return;

	VkClearColorValue clearColor{ 0, 0, 0, 0 };
	ClearColorImageRaw(chainInfo.m_swapchainImages[chainInfo.swapchainImageIndex], 0, 0, clearColor, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
}

void VulkanRenderer::ClearColorImageRaw(VkImage image, uint32 sliceIndex, uint32 mipIndex, const VkClearColorValue& color, VkImageLayout inputLayout, VkImageLayout outputLayout)
{
	draw_endRenderPass();

	VkImageSubresourceRange subresourceRange{};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = mipIndex;
	subresourceRange.levelCount = 1;
	subresourceRange.baseArrayLayer = sliceIndex;
	subresourceRange.layerCount = 1;

	barrier_image<SYNC_OP::ANY_TRANSFER | SYNC_OP::IMAGE_READ | SYNC_OP::IMAGE_WRITE, SYNC_OP::ANY_TRANSFER>(image, subresourceRange, inputLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	vkCmdClearColorImage(m_state.currentCommandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &subresourceRange);

    barrier_image<ANY_TRANSFER, SYNC_OP::ANY_TRANSFER | SYNC_OP::IMAGE_READ | SYNC_OP::IMAGE_WRITE>(image, subresourceRange, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, outputLayout);
}

void VulkanRenderer::ClearColorImage(LatteTextureVk* vkTexture, uint32 sliceIndex, uint32 mipIndex, const VkClearColorValue& color, VkImageLayout outputLayout)
{
	VkImageSubresourceRange subresourceRange;

	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = mipIndex;
	subresourceRange.levelCount = 1;
	subresourceRange.baseArrayLayer = sliceIndex;
	subresourceRange.layerCount = 1;

	auto imageObj = vkTexture->GetImageObj();
	imageObj->flagForCurrentCommandBuffer();

	VkImageLayout inputLayout = vkTexture->GetImageLayout(subresourceRange);
	ClearColorImageRaw(imageObj->m_image, sliceIndex, mipIndex, color, inputLayout, outputLayout);
	vkTexture->SetImageLayout(subresourceRange, outputLayout);
}

void VulkanRenderer::CreateBackbufferIndexBuffer()
{
	const VkDeviceSize bufferSize = sizeof(uint16) * 6;
	memoryManager->CreateBuffer(bufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, m_indexBuffer, m_indexBufferMemory);

	uint16* data;
	vkMapMemory(m_logicalDevice, m_indexBufferMemory, 0, bufferSize, 0, (void**)&data);
	const uint16 tmp[] = { 0, 1, 2, 3, 4, 5 };
	std::copy(std::begin(tmp), std::end(tmp), data);
	vkUnmapMemory(m_logicalDevice, m_indexBufferMemory);
}

void VulkanRenderer::DrawBackbufferQuad(LatteTextureView* texView, RendererOutputShader* shader, bool useLinearTexFilter, sint32 imageX, sint32 imageY, sint32 imageWidth, sint32 imageHeight, bool padView, bool clearBackground)
{
	if(!AcquireNextSwapchainImage(!padView))
		return;

	auto& chainInfo = GetChainInfo(!padView);
	LatteTextureViewVk* texViewVk = (LatteTextureViewVk*)texView;
	draw_endRenderPass();

	if (clearBackground)
		ClearColorbuffer(padView);

	// barrier for input texture
	VkMemoryBarrier memoryBarrier{};
	memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
	VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	memoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
	memoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(m_state.currentCommandBuffer, srcStage, dstStage, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

	auto pipeline = backbufferBlit_createGraphicsPipeline(m_swapchainDescriptorSetLayout, padView, shader);

	VkRenderPassBeginInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = chainInfo.m_swapchainRenderPass;
	renderPassInfo.framebuffer = chainInfo.m_swapchainFramebuffers[chainInfo.swapchainImageIndex];
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = chainInfo.getExtent();
	renderPassInfo.clearValueCount = 0;

	VkViewport viewport{};
	viewport.x = imageX;
	viewport.y = imageY;
	viewport.width = imageWidth;
	viewport.height = imageHeight;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(m_state.currentCommandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.extent = chainInfo.getExtent();
	vkCmdSetScissor(m_state.currentCommandBuffer, 0, 1, &scissor);

	auto descriptSet = backbufferBlit_createDescriptorSet(m_swapchainDescriptorSetLayout, texViewVk, useLinearTexFilter);

	vkCmdBeginRenderPass(m_state.currentCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(m_state.currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	m_state.currentPipeline = pipeline;

	vkCmdBindIndexBuffer(m_state.currentCommandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT16);

	vkCmdBindDescriptorSets(m_state.currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &descriptSet, 0, nullptr);

	vkCmdDrawIndexed(m_state.currentCommandBuffer, 6, 1, 0, 0, 0);

	vkCmdEndRenderPass(m_state.currentCommandBuffer);

	// restore viewport
	vkCmdSetViewport(m_state.currentCommandBuffer, 0, 1, &m_state.currentViewport);

	// mark current swapchain image as well defined
	chainInfo.hasDefinedSwapchainImage = true;
}

void VulkanRenderer::CreateDescriptorPool()
{
	std::array<VkDescriptorPoolSize, 4> poolSizes = {};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[0].descriptorCount = 1024 * 128;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[1].descriptorCount = 1024 * 1;
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	poolSizes[2].descriptorCount = 1024 * 128;
	poolSizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSizes[3].descriptorCount = 1024 * 4;

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = poolSizes.size();
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = 1024 * 256;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

	if (vkCreateDescriptorPool(m_logicalDevice, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS)
		UnrecoverableError("Failed to create descriptor pool!");
}

VkDescriptorSet VulkanRenderer::backbufferBlit_createDescriptorSet(VkDescriptorSetLayout descriptor_set_layout, LatteTextureViewVk* texViewVk, bool useLinearTexFilter)
{
	uint64 hash = 0;
	hash += (uint64)texViewVk->GetViewRGBA();
	hash += (uint64)texViewVk->GetDefaultTextureSampler(useLinearTexFilter);

	static std::unordered_map<uint64, VkDescriptorSet> s_set_cache;
	const auto it = s_set_cache.find(hash);
	if (it != s_set_cache.cend())
		return it->second;

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &descriptor_set_layout;

	VkDescriptorSet result;
	if (vkAllocateDescriptorSets(m_logicalDevice, &allocInfo, &result) != VK_SUCCESS)
		UnrecoverableError("Failed to allocate descriptor sets for backbuffer blit");
	performanceMonitor.vk.numDescriptorSets.increment();

	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageInfo.imageView = texViewVk->GetViewRGBA()->m_textureImageView;
	imageInfo.sampler = texViewVk->GetDefaultTextureSampler(useLinearTexFilter);

	VkWriteDescriptorSet descriptorWrites = {};
	descriptorWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites.dstSet = result;
	descriptorWrites.dstBinding = 0;
	descriptorWrites.dstArrayElement = 0;
	descriptorWrites.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrites.descriptorCount = 1;
	descriptorWrites.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(m_logicalDevice, 1, &descriptorWrites, 0, nullptr);
	performanceMonitor.vk.numDescriptorSamplerTextures.increment();

	s_set_cache[hash] = result;
	return result;
}

void VulkanRenderer::renderTarget_setViewport(float x, float y, float width, float height, float nearZ, float farZ, bool halfZ)
{
	// the Vulkan renderer handles halfZ in the vertex shader

	float vpNewX = x;
	float vpNewY = y + height;
	float vpNewWidth = width;
	float vpNewHeight = -height;

	if (m_state.currentViewport.x == vpNewX && m_state.currentViewport.y == vpNewY && m_state.currentViewport.width == vpNewWidth && m_state.currentViewport.height == vpNewHeight && m_state.currentViewport.minDepth == nearZ && m_state.currentViewport.maxDepth == farZ)
		return; // viewport did not change

	m_state.currentViewport.x = vpNewX;
	m_state.currentViewport.y = vpNewY;
	m_state.currentViewport.width = vpNewWidth;
	m_state.currentViewport.height = vpNewHeight;

	m_state.currentViewport.minDepth = nearZ;
	m_state.currentViewport.maxDepth = farZ;

	vkCmdSetViewport(m_state.currentCommandBuffer, 0, 1, &m_state.currentViewport);
}


void VulkanRenderer::renderTarget_setScissor(sint32 scissorX, sint32 scissorY, sint32 scissorWidth, sint32 scissorHeight)
{
	m_state.currentScissorRect.offset.x = scissorX;
	m_state.currentScissorRect.offset.y = scissorY;
	m_state.currentScissorRect.extent.width = scissorWidth;
	m_state.currentScissorRect.extent.height = scissorHeight;
	vkCmdSetScissor(m_state.currentCommandBuffer, 0, 1, &m_state.currentScissorRect);
}

LatteCachedFBO* VulkanRenderer::rendertarget_createCachedFBO(uint64 key)
{
	return new CachedFBOVk(key, m_logicalDevice);
}

void VulkanRenderer::rendertarget_deleteCachedFBO(LatteCachedFBO* cfbo)
{
	if (cfbo == m_state.activeFBO)
		m_state.activeFBO = nullptr;
}

void VulkanRenderer::rendertarget_bindFramebufferObject(LatteCachedFBO* cfbo)
{
	m_state.activeFBO = (CachedFBOVk*)cfbo;
}

void* VulkanRenderer::texture_acquireTextureUploadBuffer(uint32 size)
{
	return memoryManager->TextureUploadBufferAcquire(size);
}

void VulkanRenderer::texture_releaseTextureUploadBuffer(uint8* mem)
{
	memoryManager->TextureUploadBufferRelease(mem);
}

TextureDecoder* VulkanRenderer::texture_chooseDecodedFormat(Latte::E_GX2SURFFMT format, bool isDepth, Latte::E_DIM dim, uint32 width, uint32 height)
{
	FormatInfoVK texFormatInfo{};
	GetTextureFormatInfoVK(format, isDepth, dim, width, height, &texFormatInfo);
	return texFormatInfo.decoder;
}

void VulkanRenderer::texture_reserveTextureOnGPU(LatteTexture* hostTexture)
{
	LatteTextureVk* vkTexture = (LatteTextureVk*)hostTexture;
	auto allocationInfo = memoryManager->imageMemoryAllocate(vkTexture->GetImageObj()->m_image);
	vkTexture->setAllocation(allocationInfo);
}

void VulkanRenderer::texture_destroy(LatteTexture* hostTexture)
{
	LatteTextureVk* texVk = (LatteTextureVk*)hostTexture;
	delete texVk;
}

void VulkanRenderer::destroyViewDepr(VkImageView imageView)
{
	cemu_assert_debug(false);

	m_destructionQueues.m_cmd_image_views[m_commandBufferIndex].emplace_back(imageView);
}

void VulkanRenderer::destroyBuffer(VkBuffer buffer)
{
	m_destructionQueues.m_buffers[m_commandBufferIndex].emplace_back(buffer);
}

void VulkanRenderer::destroyDeviceMemory(VkDeviceMemory mem)
{
	m_destructionQueues.m_memory[m_commandBufferIndex].emplace_back(mem);
}

void VulkanRenderer::destroyPipelineInfo(PipelineInfo* pipelineInfo)
{
	cemu_assert_debug(false);
}

void VulkanRenderer::destroyShader(RendererShaderVk* shader)
{
	while (!shader->list_pipelineInfo.empty())
		delete shader->list_pipelineInfo[0];
}

void VulkanRenderer::releaseDestructibleObject(VKRDestructibleObject* destructibleObject)
{
	// destroy immediately if possible
	if (destructibleObject->canDestroy())
	{
		delete destructibleObject;
		return;
	}
	// otherwise put on queue
	m_spinlockDestructionQueue.lock();
	m_destructionQueue.emplace_back(destructibleObject);
	m_spinlockDestructionQueue.unlock();
}

void VulkanRenderer::ProcessDestructionQueue2()
{
	m_spinlockDestructionQueue.lock();
	for (auto it = m_destructionQueue.begin(); it != m_destructionQueue.end();)
	{
		if ((*it)->canDestroy())
		{
			delete (*it);
			it = m_destructionQueue.erase(it);
			continue;
		}
		++it;
	}
	m_spinlockDestructionQueue.unlock();
}

VkDescriptorSetInfo::~VkDescriptorSetInfo()
{
	for (auto& it : list_referencedViews)
		it->RemoveDescriptorSetReference(this);
	// unregister
	switch (shaderType)
	{
	case LatteConst::ShaderType::Vertex:
	{
		auto r = pipeline_info->vertex_ds_cache.erase(stateHash);
		cemu_assert_debug(r == 1);
		break;
	}
	case LatteConst::ShaderType::Pixel:
	{
		auto r = pipeline_info->pixel_ds_cache.erase(stateHash);
		cemu_assert_debug(r == 1);
		break;
	}
	case LatteConst::ShaderType::Geometry:
	{
		auto r = pipeline_info->geometry_ds_cache.erase(stateHash);
		cemu_assert_debug(r == 1);
		break;
	}
	default:
		UNREACHABLE;
	}
	// update global stats
	performanceMonitor.vk.numDescriptorSamplerTextures.decrement(statsNumSamplerTextures);
	performanceMonitor.vk.numDescriptorDynUniformBuffers.decrement(statsNumDynUniformBuffers);
	performanceMonitor.vk.numDescriptorStorageBuffers.decrement(statsNumStorageBuffers);

	VulkanRenderer::GetInstance()->releaseDestructibleObject(m_vkObjDescriptorSet);
	m_vkObjDescriptorSet = nullptr;
}

void VulkanRenderer::texture_clearSlice(LatteTexture* hostTexture, sint32 sliceIndex, sint32 mipIndex)
{
	draw_endRenderPass();
	auto vkTexture = (LatteTextureVk*)hostTexture;
	if (vkTexture->isDepth)
		texture_clearDepthSlice(hostTexture, sliceIndex, mipIndex, true, vkTexture->hasStencil, 0.0f, 0);
	else
	{
		cemu_assert_debug(vkTexture->dim != Latte::E_DIM::DIM_3D);
		if (hostTexture->IsCompressedFormat())
		{
			auto imageObj = vkTexture->GetImageObj();
			imageObj->flagForCurrentCommandBuffer();

			cemuLog_logDebug(LogType::Force, "Compressed texture ({}/{} fmt {:04x}) unsupported clear", vkTexture->width, vkTexture->height, (uint32)vkTexture->format);

			VkImageSubresourceLayers subresourceRange{};
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresourceRange.mipLevel = mipIndex;
			subresourceRange.baseArrayLayer = sliceIndex;
			subresourceRange.layerCount = 1;
			barrier_image<ANY_TRANSFER | IMAGE_READ, ANY_TRANSFER | IMAGE_READ | IMAGE_WRITE>(vkTexture, subresourceRange, VK_IMAGE_LAYOUT_GENERAL);
		}
		else
		{
			ClearColorImage(vkTexture, sliceIndex, mipIndex, { 0,0,0,0 }, VK_IMAGE_LAYOUT_GENERAL);
		}
	}
}

void VulkanRenderer::texture_clearColorSlice(LatteTexture* hostTexture, sint32 sliceIndex, sint32 mipIndex, float r, float g, float b, float a)
{
	auto vkTexture = (LatteTextureVk*)hostTexture;
	cemu_assert_debug(vkTexture->dim != Latte::E_DIM::DIM_3D);
	ClearColorImage(vkTexture, sliceIndex, mipIndex, { r,g,b,a }, VK_IMAGE_LAYOUT_GENERAL);
}

void VulkanRenderer::texture_clearDepthSlice(LatteTexture* hostTexture, uint32 sliceIndex, sint32 mipIndex, bool clearDepth, bool clearStencil, float depthValue, uint32 stencilValue)
{
	draw_endRenderPass(); // vkCmdClearDepthStencilImage must not be inside renderpass

	auto vkTexture = (LatteTextureVk*)hostTexture;

	VkImageAspectFlags imageAspect = vkTexture->GetImageAspect();

	VkImageAspectFlags aspectMask = 0;
	if (clearDepth && (imageAspect & VK_IMAGE_ASPECT_DEPTH_BIT) != 0)
		aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
	if (clearStencil && (imageAspect & VK_IMAGE_ASPECT_STENCIL_BIT) != 0)
		aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

	auto imageObj = vkTexture->GetImageObj();
	imageObj->flagForCurrentCommandBuffer();

	VkImageSubresourceLayers subresourceRange{};
	subresourceRange.aspectMask = vkTexture->GetImageAspect();
	subresourceRange.mipLevel = mipIndex;
	subresourceRange.baseArrayLayer = sliceIndex;
	subresourceRange.layerCount = 1;
	barrier_image<ANY_TRANSFER | IMAGE_READ | IMAGE_WRITE, ANY_TRANSFER>(vkTexture, subresourceRange, VK_IMAGE_LAYOUT_GENERAL);

	VkClearDepthStencilValue depthStencilValue{};
	depthStencilValue.depth = depthValue;
	depthStencilValue.stencil = stencilValue;

	VkImageSubresourceRange range{};
	range.baseMipLevel = mipIndex;
	range.levelCount = 1;
	range.baseArrayLayer = sliceIndex;
	range.layerCount = 1;

	range.aspectMask = aspectMask;

	vkCmdClearDepthStencilImage(m_state.currentCommandBuffer, imageObj->m_image, VK_IMAGE_LAYOUT_GENERAL, &depthStencilValue, 1, &range);

	barrier_image<ANY_TRANSFER, ANY_TRANSFER | IMAGE_READ | IMAGE_WRITE>(vkTexture, subresourceRange, VK_IMAGE_LAYOUT_GENERAL);
}

void VulkanRenderer::texture_loadSlice(LatteTexture* hostTexture, sint32 width, sint32 height, sint32 depth, void* pixelData, sint32 sliceIndex, sint32 mipIndex, uint32 compressedImageSize)
{
	auto vkTexture = (LatteTextureVk*)hostTexture;
	auto vkImageObj = vkTexture->GetImageObj();
	vkImageObj->flagForCurrentCommandBuffer();

	draw_endRenderPass();

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(m_logicalDevice, vkImageObj->m_image, &memRequirements);

	uint32 uploadSize = compressedImageSize;// memRequirements.size;
	uint32 uploadAlignment = memRequirements.alignment;

	VKRSynchronizedRingAllocator& vkMemAllocator = memoryManager->getStagingAllocator();

	auto uploadResv = vkMemAllocator.AllocateBufferMemory(uploadSize, uploadAlignment);
	memcpy(uploadResv.memPtr, pixelData, compressedImageSize);
	vkMemAllocator.FlushReservation(uploadResv);

	FormatInfoVK texFormatInfo;
	GetTextureFormatInfoVK(hostTexture->format, hostTexture->isDepth, hostTexture->dim, 0, 0, &texFormatInfo);

	bool is3DTexture = hostTexture->Is3DTexture();

	VkImageSubresourceLayers barrierSubresourceRange{};
	barrierSubresourceRange.aspectMask = texFormatInfo.vkImageAspect;
	barrierSubresourceRange.mipLevel = mipIndex;
	barrierSubresourceRange.baseArrayLayer = is3DTexture ? 0 : sliceIndex;
	barrierSubresourceRange.layerCount = 1;
	barrier_image<ANY_TRANSFER | IMAGE_READ | IMAGE_WRITE | HOST_WRITE, ANY_TRANSFER>(vkTexture, barrierSubresourceRange, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	VkBufferImageCopy imageRegion[2]{};
	sint32 imageRegionCount = 0;
	if (texFormatInfo.vkImageAspect == VK_IMAGE_ASPECT_COLOR_BIT || texFormatInfo.vkImageAspect == VK_IMAGE_ASPECT_DEPTH_BIT)
	{
		imageRegion[0].bufferOffset = uploadResv.bufferOffset;
		imageRegion[0].imageExtent.width = width;
		imageRegion[0].imageExtent.height = height;
		imageRegion[0].imageExtent.depth = 1;
		imageRegion[0].imageOffset.z = is3DTexture ? sliceIndex : 0;

		imageRegion[0].imageSubresource.mipLevel = mipIndex;
		imageRegion[0].imageSubresource.aspectMask = texFormatInfo.vkImageAspect;
		imageRegion[0].imageSubresource.baseArrayLayer = is3DTexture ? 0 : sliceIndex;
		imageRegion[0].imageSubresource.layerCount = 1;
		imageRegionCount = 1;
	}
	else if (texFormatInfo.vkImageAspect == VK_IMAGE_ASPECT_DEPTH_BIT)
	{
		if (is3DTexture)
			cemu_assert_debug(false);

		// depth only copy
		imageRegion[0].bufferOffset = uploadResv.bufferOffset;
		imageRegion[0].imageExtent.width = width;
		imageRegion[0].imageExtent.height = height;
		imageRegion[0].imageExtent.depth = 1;

		imageRegion[0].imageSubresource.mipLevel = mipIndex;
		imageRegion[0].imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		imageRegion[0].imageSubresource.baseArrayLayer = sliceIndex;
		imageRegion[0].imageSubresource.layerCount = 1;

		imageRegionCount = 1;
	}
	else if (texFormatInfo.vkImageAspect == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
	{
		if (is3DTexture)
			cemu_assert_debug(false);

		// depth copy
		imageRegion[0].bufferOffset = uploadResv.bufferOffset;
		imageRegion[0].imageExtent.width = width;
		imageRegion[0].imageExtent.height = height;
		imageRegion[0].imageExtent.depth = 1;

		imageRegion[0].imageSubresource.mipLevel = mipIndex;
		imageRegion[0].imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		imageRegion[0].imageSubresource.baseArrayLayer = sliceIndex;
		imageRegion[0].imageSubresource.layerCount = 1;

		// stencil copy
		imageRegion[1].bufferOffset = uploadResv.bufferOffset;
		imageRegion[1].imageExtent.width = width;
		imageRegion[1].imageExtent.height = height;
		imageRegion[1].imageExtent.depth = 1;

		imageRegion[1].imageSubresource.mipLevel = mipIndex;
		imageRegion[1].imageSubresource.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
		imageRegion[1].imageSubresource.baseArrayLayer = sliceIndex;
		imageRegion[1].imageSubresource.layerCount = 1;

		imageRegionCount = 2;
	}
	else
		cemu_assert_debug(false);

	vkCmdCopyBufferToImage(m_state.currentCommandBuffer, uploadResv.vkBuffer, vkImageObj->m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageRegionCount, imageRegion);

	barrier_image<ANY_TRANSFER, ANY_TRANSFER | IMAGE_READ | IMAGE_WRITE>(vkTexture, barrierSubresourceRange, VK_IMAGE_LAYOUT_GENERAL);
}

LatteTexture* VulkanRenderer::texture_createTextureEx(Latte::E_DIM dim, MPTR physAddress, MPTR physMipAddress, Latte::E_GX2SURFFMT format, uint32 width, uint32 height, uint32 depth, uint32 pitch, uint32 mipLevels,
	uint32 swizzle, Latte::E_HWTILEMODE tileMode, bool isDepth)
{
	return new LatteTextureVk(this, dim, physAddress, physMipAddress, format, width, height, depth, pitch, mipLevels, swizzle, tileMode, isDepth);
}

void VulkanRenderer::texture_setLatteTexture(LatteTextureView* textureView, uint32 textureUnit)
{
	m_state.boundTexture[textureUnit] = static_cast<LatteTextureViewVk*>(textureView);
}

void VulkanRenderer::texture_copyImageSubData(LatteTexture* src, sint32 srcMip, sint32 effectiveSrcX, sint32 effectiveSrcY, sint32 srcSlice, LatteTexture* dst, sint32 dstMip, sint32 effectiveDstX, sint32 effectiveDstY, sint32 dstSlice, sint32 effectiveCopyWidth, sint32 effectiveCopyHeight, sint32 srcDepth)
{
	LatteTextureVk* srcVk = static_cast<LatteTextureVk*>(src);
	LatteTextureVk* dstVk = static_cast<LatteTextureVk*>(dst);

	draw_endRenderPass(); // vkCmdCopyImage must be called outside of a renderpass

	VKRObjectTexture* srcVkObj = srcVk->GetImageObj();
	VKRObjectTexture* dstVkObj = dstVk->GetImageObj();
	srcVkObj->flagForCurrentCommandBuffer();
	dstVkObj->flagForCurrentCommandBuffer();

	VkImageCopy region{};
	region.srcOffset.x = effectiveSrcX;
	region.srcOffset.y = effectiveSrcY;
	region.dstOffset.x = effectiveDstX;
	region.dstOffset.y = effectiveDstY;
	region.extent.width = effectiveCopyWidth;
	region.extent.height = effectiveCopyHeight;
	region.extent.depth = 1;

	if (src->Is3DTexture())
	{
		region.srcOffset.z = srcSlice;
		region.extent.depth = srcDepth;
		region.srcSubresource.baseArrayLayer = 0;
		region.srcSubresource.layerCount = 1;
	}
	else
	{
		region.srcOffset.z = 0;
		region.extent.depth = 1;
		region.srcSubresource.baseArrayLayer = srcSlice;
		region.srcSubresource.layerCount = srcDepth;
	}

	if (dst->Is3DTexture())
	{
		region.dstOffset.z = dstSlice;
		region.dstSubresource.baseArrayLayer = 0;
		region.dstSubresource.layerCount = 1;
	}
	else
	{
		region.dstOffset.z = 0;
		region.dstSubresource.baseArrayLayer = dstSlice;
		region.dstSubresource.layerCount = srcDepth;
	}

	region.srcSubresource.mipLevel = srcMip;
	region.srcSubresource.aspectMask = srcVk->GetImageAspect();

	region.dstSubresource.mipLevel = dstMip;
	region.dstSubresource.aspectMask = dstVk->GetImageAspect();

	bool srcIsCompressed = Latte::IsCompressedFormat(srcVk->format);
	bool dstIsCompressed = Latte::IsCompressedFormat(dstVk->format);

	if (!srcIsCompressed && dstIsCompressed)
	{
		// handle the special case where the destination is compressed and not a multiple of the texel size (4)
		sint32 mipWidth = std::max(dst->width >> dstMip, 1);
		sint32 mipHeight = std::max(dst->height >> dstMip, 1);


		if (mipWidth < 4 || mipHeight < 4)
		{
			cemuLog_logDebug(LogType::Force, "vkCmdCopyImage - blocked copy for unsupported uncompressed->compressed copy with dst smaller than 4x4");
			return;
		}

	}

	// make sure all write operations to the src image have finished
	barrier_image<SYNC_OP::IMAGE_WRITE | SYNC_OP::ANY_TRANSFER, SYNC_OP::ANY_TRANSFER>(srcVk, region.srcSubresource, VK_IMAGE_LAYOUT_GENERAL);
	// make sure all read and write operations to the dst image have finished
	barrier_image<SYNC_OP::IMAGE_READ | SYNC_OP::IMAGE_WRITE | SYNC_OP::ANY_TRANSFER, SYNC_OP::ANY_TRANSFER>(dstVk, region.dstSubresource, VK_IMAGE_LAYOUT_GENERAL);

	vkCmdCopyImage(m_state.currentCommandBuffer, srcVkObj->m_image, VK_IMAGE_LAYOUT_GENERAL, dstVkObj->m_image, VK_IMAGE_LAYOUT_GENERAL, 1, &region);

	// make sure the transfer is finished before the image is read or written
	barrier_image<SYNC_OP::ANY_TRANSFER, SYNC_OP::IMAGE_READ | SYNC_OP::IMAGE_WRITE | SYNC_OP::ANY_TRANSFER>(dstVk, region.dstSubresource, VK_IMAGE_LAYOUT_GENERAL);
}

LatteTextureReadbackInfo* VulkanRenderer::texture_createReadback(LatteTextureView* textureView)
{
	auto* result = new LatteTextureReadbackInfoVk(m_logicalDevice, textureView);

	LatteTextureVk* vkTex = (LatteTextureVk*)textureView->baseTexture;

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(m_logicalDevice, vkTex->GetImageObj()->m_image, &memRequirements);

	const uint32 linearImageSize = result->GetImageSize();
	const uint32 uploadSize = (linearImageSize == 0) ? memRequirements.size : linearImageSize;
	const uint32 uploadAlignment = 256; // todo - use Vk optimalBufferCopyOffsetAlignment
	m_textureReadbackBufferWriteIndex = (m_textureReadbackBufferWriteIndex + uploadAlignment - 1) & ~(uploadAlignment - 1);

	if ((m_textureReadbackBufferWriteIndex + uploadSize + 256) > TEXTURE_READBACK_SIZE)
	{
		m_textureReadbackBufferWriteIndex = 0;
	}

	const uint32 uploadBufferOffset = m_textureReadbackBufferWriteIndex;
	m_textureReadbackBufferWriteIndex += uploadSize;

	result->SetBuffer(m_textureReadbackBuffer, m_textureReadbackBufferPtr, uploadBufferOffset);

	return result;
}

uint32 s_vkCurrentUniqueId = 0;

uint64 VulkanRenderer::GenUniqueId()
{
	s_vkCurrentUniqueId++;
	return s_vkCurrentUniqueId;
}

void VulkanRenderer::streamout_setupXfbBuffer(uint32 bufferIndex, sint32 ringBufferOffset, uint32 rangeAddr, uint32 rangeSize)
{
	VkDeviceSize tfBufferOffset = ringBufferOffset;
	m_streamoutState.buffer[bufferIndex].enabled = true;
	m_streamoutState.buffer[bufferIndex].ringBufferOffset = ringBufferOffset;
}

void VulkanRenderer::streamout_begin()
{
	if (m_featureControl.mode.useTFEmulationViaSSBO)
		return;
	if (m_state.hasActiveXfb == false)
		m_state.hasActiveXfb = true;
}

void VulkanRenderer::streamout_applyTransformFeedbackState()
{
	if (m_featureControl.mode.useTFEmulationViaSSBO)
		return;
	cemu_assert_debug(m_state.hasActiveXfb == false);
	if (m_state.hasActiveXfb)
	{
		// set buffers
		for (sint32 i = 0; i < LATTE_NUM_STREAMOUT_BUFFER; i++)
		{
			if (m_streamoutState.buffer[i].enabled)
			{
				VkBuffer tfBuffer = m_xfbRingBuffer;
				VkDeviceSize tfBufferOffset = m_streamoutState.buffer[i].ringBufferOffset;
				VkDeviceSize tfBufferSize = VK_WHOLE_SIZE;
				vkCmdBindTransformFeedbackBuffersEXT(m_state.currentCommandBuffer, i, 1, &tfBuffer, &tfBufferOffset, &tfBufferSize);
			}
		}
		// begin transform feedback
		vkCmdBeginTransformFeedbackEXT(m_state.currentCommandBuffer, 0, 0, nullptr, nullptr);
	}
}

void VulkanRenderer::streamout_rendererFinishDrawcall()
{
	if (m_state.hasActiveXfb)
	{
		vkCmdEndTransformFeedbackEXT(m_state.currentCommandBuffer, 0, 0, nullptr, nullptr);
		m_streamoutState.buffer[0].enabled = false;
		m_streamoutState.buffer[1].enabled = false;
		m_streamoutState.buffer[2].enabled = false;
		m_streamoutState.buffer[3].enabled = false;
		m_state.hasActiveXfb = false;
	}
}


void VulkanRenderer::buffer_bindVertexBuffer(uint32 bufferIndex, uint32 offset, uint32 size)
{
	cemu_assert_debug(!m_useHostMemoryForCache);
	if (m_state.currentVertexBinding[bufferIndex].offset == offset)
		return;
	cemu_assert_debug(bufferIndex < LATTE_MAX_VERTEX_BUFFERS);
	m_state.currentVertexBinding[bufferIndex].offset = offset;
	VkBuffer attrBuffer = m_bufferCache;
	VkDeviceSize attrOffset = offset;
	vkCmdBindVertexBuffers(m_state.currentCommandBuffer, bufferIndex, 1, &attrBuffer, &attrOffset);
}

void VulkanRenderer::buffer_bindVertexStrideWorkaroundBuffer(VkBuffer fixedBuffer, uint32 offset, uint32 bufferIndex, uint32 size)
{
	cemu_assert_debug(bufferIndex < LATTE_MAX_VERTEX_BUFFERS);
	m_state.currentVertexBinding[bufferIndex].offset = 0xFFFFFFFF;
	VkBuffer attrBuffer = fixedBuffer;
	VkDeviceSize attrOffset = offset;
	vkCmdBindVertexBuffers(m_state.currentCommandBuffer, bufferIndex, 1, &attrBuffer, &attrOffset);
}

std::pair<VkBuffer, uint32> VulkanRenderer::buffer_genStrideWorkaroundVertexBuffer(MPTR buffer, uint32 size, uint32 oldStride)
{
	cemu_assert_debug(oldStride % 4 != 0);

	std::span<uint8> old_buffer{memory_getPointerFromPhysicalOffset(buffer), size};

	//new stride is the nearest multiple of 4
	uint32 newStride = oldStride + (4-(oldStride % 4));
	uint32 newSize = size / oldStride * newStride;

	auto new_buffer_alloc = memoryManager->getMetalStrideWorkaroundAllocator().AllocateBufferMemory(newSize, 128);

	std::span<uint8> new_buffer{new_buffer_alloc.memPtr, new_buffer_alloc.size};

	for(size_t elem = 0; elem < size / oldStride; elem++)
	{
		memcpy(&new_buffer[elem * newStride], &old_buffer[elem * oldStride], oldStride);
	}
	return {new_buffer_alloc.vkBuffer, new_buffer_alloc.bufferOffset};
}

void VulkanRenderer::buffer_bindUniformBuffer(LatteConst::ShaderType shaderType, uint32 bufferIndex, uint32 offset, uint32 size)
{
	cemu_assert_debug(!m_useHostMemoryForCache);
	cemu_assert_debug(bufferIndex < 16);
	switch (shaderType)
	{
	case LatteConst::ShaderType::Vertex:
		dynamicOffsetInfo.shaderUB[VulkanRendererConst::SHADER_STAGE_INDEX_VERTEX].unformBufferOffset[bufferIndex] = offset;
		break;
	case LatteConst::ShaderType::Geometry:
		dynamicOffsetInfo.shaderUB[VulkanRendererConst::SHADER_STAGE_INDEX_GEOMETRY].unformBufferOffset[bufferIndex] = offset;
		break;
	case LatteConst::ShaderType::Pixel:
		dynamicOffsetInfo.shaderUB[VulkanRendererConst::SHADER_STAGE_INDEX_FRAGMENT].unformBufferOffset[bufferIndex] = offset;
		break;
	default:
		cemu_assert_debug(false);
	}
}

void VulkanRenderer::bufferCache_init(const sint32 bufferSize)
{
	m_importedMemBaseAddress = 0x10000000;
	size_t hostAllocationSize = 0x40000000ull;
	// todo - get size of allocation
	bool configUseHostMemory = false; // todo - replace this with a config option
	m_useHostMemoryForCache = false;
	if (m_featureControl.deviceExtensions.external_memory_host && configUseHostMemory)
	{
		m_useHostMemoryForCache = memoryManager->CreateBufferFromHostMemory(memory_getPointerFromVirtualOffset(m_importedMemBaseAddress), hostAllocationSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 0, m_importedMem, m_importedMemMemory);
		if (!m_useHostMemoryForCache)
		{
			cemuLog_log(LogType::Force, "Unable to import host memory to Vulkan buffer. Use default cache system instead");
		}
	}
	if(!m_useHostMemoryForCache)
		memoryManager->CreateBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 0, m_bufferCache, m_bufferCacheMemory);
}

void VulkanRenderer::bufferCache_upload(uint8* buffer, sint32 size, uint32 bufferOffset)
{
	draw_endRenderPass();

	VKRSynchronizedRingAllocator& vkMemAllocator = memoryManager->getStagingAllocator();

	auto uploadResv = vkMemAllocator.AllocateBufferMemory(size, 256);
	memcpy(uploadResv.memPtr, buffer, size);

	vkMemAllocator.FlushReservation(uploadResv);

	barrier_bufferRange<ANY_TRANSFER | HOST_WRITE, ANY_TRANSFER,
		BUFFER_SHADER_READ, TRANSFER_WRITE>(
			uploadResv.vkBuffer, uploadResv.bufferOffset, uploadResv.size, // make sure any in-flight transfers are completed
			m_bufferCache, bufferOffset, size); // make sure all reads are completed before we overwrite the data

	VkBufferCopy region;
	region.srcOffset = uploadResv.bufferOffset;
	region.dstOffset = bufferOffset;
	region.size = size;
	vkCmdCopyBuffer(m_state.currentCommandBuffer, uploadResv.vkBuffer, m_bufferCache, 1, &region);

	barrier_sequentializeTransfer();
}

void VulkanRenderer::bufferCache_copy(uint32 srcOffset, uint32 dstOffset, uint32 size)
{
	cemu_assert_debug(!m_useHostMemoryForCache);
	draw_endRenderPass();

	barrier_sequentializeTransfer();

	bool isOverlapping = (srcOffset + size) > dstOffset && (srcOffset) < (dstOffset + size);
	cemu_assert_debug(!isOverlapping);

	VkBufferCopy bufferCopy{};
	bufferCopy.srcOffset = srcOffset;
	bufferCopy.dstOffset = dstOffset;
	bufferCopy.size = size;
	vkCmdCopyBuffer(m_state.currentCommandBuffer, m_bufferCache, m_bufferCache, 1, &bufferCopy);

	barrier_sequentializeTransfer();
}

void VulkanRenderer::bufferCache_copyStreamoutToMainBuffer(uint32 srcOffset, uint32 dstOffset, uint32 size)
{
	draw_endRenderPass();

	VkBuffer dstBuffer;
	if (m_useHostMemoryForCache)
	{
		// in host memory mode, dstOffset is physical address instead of cache address
		dstBuffer = m_importedMem;
		dstOffset -= m_importedMemBaseAddress;
	}
	else
		dstBuffer = m_bufferCache;

	barrier_bufferRange<BUFFER_SHADER_WRITE, TRANSFER_READ,
		ANY_TRANSFER | BUFFER_SHADER_READ, TRANSFER_WRITE>(
			m_xfbRingBuffer, srcOffset, size, // wait for all writes to finish
			dstBuffer, dstOffset, size); // wait for all reads to finish

	barrier_sequentializeTransfer();

	VkBufferCopy bufferCopy{};
	bufferCopy.srcOffset = srcOffset;
	bufferCopy.dstOffset = dstOffset;
	bufferCopy.size = size;
	vkCmdCopyBuffer(m_state.currentCommandBuffer, m_xfbRingBuffer, dstBuffer, 1, &bufferCopy);

	barrier_sequentializeTransfer();
}

void VulkanRenderer::AppendOverlayDebugInfo()
{
	ImGui::Text("--- Vulkan info ---");
	ImGui::Text("GfxPipelines   %u", performanceMonitor.vk.numGraphicPipelines.get());
	ImGui::Text("DescriptorSets %u", performanceMonitor.vk.numDescriptorSets.get());
	ImGui::Text("DS ImgSamplers %u", performanceMonitor.vk.numDescriptorSamplerTextures.get());
	ImGui::Text("DS DynUniform  %u", performanceMonitor.vk.numDescriptorDynUniformBuffers.get());
	ImGui::Text("DS StorageBuf  %u", performanceMonitor.vk.numDescriptorStorageBuffers.get());
	ImGui::Text("Images         %u", performanceMonitor.vk.numImages.get());
	ImGui::Text("ImageView      %u", performanceMonitor.vk.numImageViews.get());
	ImGui::Text("RenderPass     %u", performanceMonitor.vk.numRenderPass.get());
	ImGui::Text("Framebuffer    %u", performanceMonitor.vk.numFramebuffer.get());
	m_spinlockDestructionQueue.lock();
	ImGui::Text("DestructionQ   %u", (unsigned int)m_destructionQueue.size());
	m_spinlockDestructionQueue.unlock();


	ImGui::Text("BeginRP/f      %u", performanceMonitor.vk.numBeginRenderpassPerFrame.get());
	ImGui::Text("Barriers/f     %u", performanceMonitor.vk.numDrawBarriersPerFrame.get());
	ImGui::Text("--- Cache info ---");

	uint32 bufferCacheHeapSize = 0;
	uint32 bufferCacheAllocationSize = 0;
	uint32 bufferCacheNumAllocations = 0;

	LatteBufferCache_getStats(bufferCacheHeapSize, bufferCacheAllocationSize, bufferCacheNumAllocations);

	ImGui::Text("Buffer");
	ImGui::SameLine(60.0f);
	ImGui::Text("%06uKB / %06uKB Allocs: %u", (uint32)(bufferCacheAllocationSize + 1023) / 1024, ((uint32)bufferCacheHeapSize + 1023) / 1024, (uint32)bufferCacheNumAllocations);

	uint32 numBuffers;
	size_t totalSize, freeSize;

	memoryManager->getStagingAllocator().GetStats(numBuffers, totalSize, freeSize);
	ImGui::Text("Staging");
	ImGui::SameLine(60.0f);
	ImGui::Text("%06uKB / %06uKB Buffers: %u", ((uint32)(totalSize - freeSize) + 1023) / 1024, ((uint32)totalSize + 1023) / 1024, (uint32)numBuffers);

	memoryManager->getIndexAllocator().GetStats(numBuffers, totalSize, freeSize);
	ImGui::Text("Index");
	ImGui::SameLine(60.0f);
	ImGui::Text("%06uKB / %06uKB Buffers: %u", ((uint32)(totalSize - freeSize) + 1023) / 1024, ((uint32)totalSize + 1023) / 1024, (uint32)numBuffers);

	ImGui::Text("--- Tex heaps ---");
	memoryManager->appendOverlayHeapDebugInfo();
}

void VKRDestructibleObject::flagForCurrentCommandBuffer()
{
	m_lastCmdBufferId = VulkanRenderer::GetInstance()->GetCurrentCommandBufferId();
}

bool VKRDestructibleObject::canDestroy()
{
	if (refCount > 0)
		return false;
	return VulkanRenderer::GetInstance()->HasCommandBufferFinished(m_lastCmdBufferId);
}

VKRObjectTexture::VKRObjectTexture()
{
	performanceMonitor.vk.numImages.increment();
}

VKRObjectTexture::~VKRObjectTexture()
{
	auto vkr = VulkanRenderer::GetInstance();
	if (m_allocation)
	{
		vkr->GetMemoryManager()->imageMemoryFree(m_allocation);
		m_allocation = nullptr;
	}
	if (m_image)
		vkDestroyImage(vkr->GetLogicalDevice(), m_image, nullptr);
	performanceMonitor.vk.numImages.decrement();
}

VKRObjectTextureView::VKRObjectTextureView(VKRObjectTexture* tex, VkImageView view)
{
	m_textureImageView = view;
	this->addRef(tex);
	performanceMonitor.vk.numImageViews.increment();
}

VKRObjectTextureView::~VKRObjectTextureView()
{
	auto logicalDevice = VulkanRenderer::GetInstance()->GetLogicalDevice();
	if (m_textureDefaultSampler[0] != VK_NULL_HANDLE)
		vkDestroySampler(logicalDevice, m_textureDefaultSampler[0], nullptr);
	if (m_textureDefaultSampler[1] != VK_NULL_HANDLE)
		vkDestroySampler(logicalDevice, m_textureDefaultSampler[1], nullptr);
	vkDestroyImageView(logicalDevice, m_textureImageView, nullptr);
	performanceMonitor.vk.numImageViews.decrement();
}

VKRObjectRenderPass::VKRObjectRenderPass(AttachmentInfo_t& attachmentInfo, sint32 colorAttachmentCount)
{
	// generate helper hash for pipeline state
	uint64 stateHash = 0;
	for (int i = 0; i < Latte::GPU_LIMITS::NUM_COLOR_ATTACHMENTS; ++i)
	{
		if (attachmentInfo.colorAttachment[i].isPresent || attachmentInfo.colorAttachment[i].viewObj)
		{
			stateHash += attachmentInfo.colorAttachment[i].format + i * 31;
			stateHash = std::rotl<uint64>(stateHash, 7);
		}
	}
	if (attachmentInfo.depthAttachment.isPresent || attachmentInfo.depthAttachment.viewObj)
	{
		stateHash += attachmentInfo.depthAttachment.format;
		stateHash = std::rotl<uint64>(stateHash, 7);
	}
	m_hashForPipeline = stateHash;

	// setup Vulkan renderpass
	std::vector<VkAttachmentDescription> attachments_descriptions;
	std::array<VkAttachmentReference, Latte::GPU_LIMITS::NUM_COLOR_ATTACHMENTS> color_attachments_references{};
	cemu_assert(colorAttachmentCount <= color_attachments_references.size());
	sint32 numColorAttachments = 0;
	for (int i = 0; i < 8; ++i)
	{
		if (attachmentInfo.colorAttachment[i].viewObj == nullptr && attachmentInfo.colorAttachment[i].isPresent == false)
		{
			color_attachments_references[i].attachment = VK_ATTACHMENT_UNUSED;
			m_colorAttachmentFormat[i] = VK_FORMAT_UNDEFINED;
			continue;
		}
		m_colorAttachmentFormat[i] = attachmentInfo.colorAttachment[i].format;

		color_attachments_references[i].attachment = (uint32)attachments_descriptions.size();
		color_attachments_references[i].layout = VK_IMAGE_LAYOUT_GENERAL;

		VkAttachmentDescription entry{};
		entry.format = attachmentInfo.colorAttachment[i].format;
		entry.samples = VK_SAMPLE_COUNT_1_BIT;
		entry.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		entry.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		entry.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		entry.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		entry.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
		entry.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
		attachments_descriptions.emplace_back(entry);

		numColorAttachments = i + 1;
	}

	VkAttachmentReference depth_stencil_attachments_references{};
	bool hasDepthStencilAttachment = false;
	if (attachmentInfo.depthAttachment.viewObj == nullptr && attachmentInfo.depthAttachment.isPresent == false)
	{
		depth_stencil_attachments_references.attachment = VK_ATTACHMENT_UNUSED;
		m_depthAttachmentFormat = VK_FORMAT_UNDEFINED;
	}
	else
	{
		hasDepthStencilAttachment = true;
		depth_stencil_attachments_references.attachment = (uint32)attachments_descriptions.size();
		depth_stencil_attachments_references.layout = VK_IMAGE_LAYOUT_GENERAL;
		m_depthAttachmentFormat = attachmentInfo.depthAttachment.format;

		VkAttachmentDescription entry{};
		entry.format = attachmentInfo.depthAttachment.format;
		entry.samples = VK_SAMPLE_COUNT_1_BIT;
		entry.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		entry.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		if (attachmentInfo.depthAttachment.hasStencil)
		{
			entry.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			entry.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
		}
		else
		{
			entry.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			entry.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		}
		entry.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
		entry.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
		attachments_descriptions.emplace_back(entry);
	}

	// todo - use numColorAttachments instead of .size() or colorAttachmentCount (needs adjusting in many places)

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = colorAttachmentCount;
	subpass.pColorAttachments = color_attachments_references.data();
	subpass.inputAttachmentCount = 0;
	subpass.pInputAttachments = nullptr;
	subpass.pDepthStencilAttachment = &depth_stencil_attachments_references;

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = (uint32)attachments_descriptions.size();
	renderPassInfo.pAttachments = attachments_descriptions.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	renderPassInfo.pDependencies = nullptr;
	renderPassInfo.dependencyCount = 0;
	// before Cemu 1.25.5 we used zero here, which means implicit synchronization. For 1.25.5 it was changed to 2 (using the subpass dependencies above)
	// Reverted this again to zero for Cemu 1.25.5b as the performance cost is just too high. Manual synchronization is preferred

	if (vkCreateRenderPass(VulkanRenderer::GetInstance()->GetLogicalDevice(), &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS)
	{
		cemuLog_log(LogType::Force, "Vulkan-Error: Failed to create render pass");
		throw std::runtime_error("failed to create render pass!");
	}

	// track references
	for (int i = 0; i < 8; ++i)
	{
		if (attachmentInfo.colorAttachment[i].viewObj)
			addRef(attachmentInfo.colorAttachment[i].viewObj);
	}
	if (attachmentInfo.depthAttachment.viewObj)
		addRef(attachmentInfo.depthAttachment.viewObj);
	performanceMonitor.vk.numRenderPass.increment();
}

VKRObjectRenderPass::~VKRObjectRenderPass()
{
	if (m_renderPass != VK_NULL_HANDLE)
		vkDestroyRenderPass(VulkanRenderer::GetInstance()->GetLogicalDevice(), m_renderPass, nullptr);
	performanceMonitor.vk.numRenderPass.decrement();
}

VKRObjectFramebuffer::VKRObjectFramebuffer(VKRObjectRenderPass* renderPass, std::span<VKRObjectTextureView*> attachments, Vector2i size)
{
	// convert VKRObjectTextureView* array to vkImageView array
	std::array<VkImageView, 16> attachmentViews;
	cemu_assert(attachments.size() < attachmentViews.size());
	for (size_t i = 0; i < attachments.size(); i++)
		attachmentViews[i] = attachments[i]->m_textureImageView;

	VkFramebufferCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	createInfo.pAttachments = attachmentViews.data();
	createInfo.attachmentCount = attachments.size();
	createInfo.renderPass = renderPass->m_renderPass;
	createInfo.layers = 1;
	createInfo.width = size.x;
	createInfo.height = size.y;
	if (vkCreateFramebuffer(VulkanRenderer::GetInstance()->GetLogicalDevice(), &createInfo, nullptr, &m_frameBuffer) != VK_SUCCESS)
		throw std::runtime_error("failed to create framebuffer!");

	// track refs
	this->addRef(renderPass);
	for (auto& itr : attachments)
		this->addRef(itr);

	performanceMonitor.vk.numFramebuffer.increment();
}

VKRObjectFramebuffer::~VKRObjectFramebuffer()
{
	if (m_frameBuffer != VK_NULL_HANDLE)
		vkDestroyFramebuffer(VulkanRenderer::GetInstance()->GetLogicalDevice(), m_frameBuffer, nullptr);
	performanceMonitor.vk.numFramebuffer.decrement();
}

VKRObjectPipeline::VKRObjectPipeline()
{
	// todo
}

void VKRObjectPipeline::setPipeline(VkPipeline newPipeline)
{
	cemu_assert_debug(pipeline == VK_NULL_HANDLE);
	pipeline = newPipeline;
	if(newPipeline != VK_NULL_HANDLE)
		performanceMonitor.vk.numGraphicPipelines.increment();
}

VKRObjectPipeline::~VKRObjectPipeline()
{
	auto vkr = VulkanRenderer::GetInstance();
	if (pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(vkr->GetLogicalDevice(), pipeline, nullptr);
		performanceMonitor.vk.numGraphicPipelines.decrement();
	}
	if (vertexDSL != VK_NULL_HANDLE)
		vkDestroyDescriptorSetLayout(vkr->GetLogicalDevice(), vertexDSL, nullptr);
	if (pixelDSL != VK_NULL_HANDLE)
		vkDestroyDescriptorSetLayout(vkr->GetLogicalDevice(), pixelDSL, nullptr);
	if (geometryDSL != VK_NULL_HANDLE)
		vkDestroyDescriptorSetLayout(vkr->GetLogicalDevice(), geometryDSL, nullptr);
	if (pipeline_layout != VK_NULL_HANDLE)
		vkDestroyPipelineLayout(vkr->GetLogicalDevice(), pipeline_layout, nullptr);
}

VKRObjectDescriptorSet::VKRObjectDescriptorSet()
{
	performanceMonitor.vk.numDescriptorSets.increment();
}

VKRObjectDescriptorSet::~VKRObjectDescriptorSet()
{
	auto vkr = VulkanRenderer::GetInstance();
	vkFreeDescriptorSets(vkr->GetLogicalDevice(), vkr->GetDescriptorPool(), 1, &descriptorSet);
	performanceMonitor.vk.numDescriptorSets.decrement();
}
