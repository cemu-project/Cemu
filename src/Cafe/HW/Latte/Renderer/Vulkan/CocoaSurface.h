#pragma once

#if BOOST_OS_MACOS

#include <vulkan/vulkan.h>

VkSurfaceKHR CreateCocoaSurface(VkInstance instance, void* handle);

#endif
