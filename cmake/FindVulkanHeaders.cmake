# SPDX-FileCopyrightText: 2022 Alexandre Bouvier <contact@amb.tf>
# SPDX-License-Identifier: GPL-3.0-or-later

find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
    pkg_search_module(VULKANHEADERS QUIET vulkanheaders vulkan-headers)
endif()

find_path(VulkanHeaders_INCLUDE_DIR
    NAMES vulkan.h
    HINTS ${VULKANHEADERS_INCLUDE_DIRS}
    PATH_SUFFIXES vulkan
)

if (VulkanHeaders_INCLUDE_DIR)
    file(READ "${VulkanHeaders_INCLUDE_DIR}/vulkan_core.h" _vulkan_core_file)
    string(REGEX MATCH "#define[ \t]+VK_HEADER_VERSION_COMPLETE[ \t]+VK_MAKE_API_VERSION\\(([0-9]+),[ \t]*([0-9]+),[ \t]*([0-9]+),[ \t]*VK_HEADER_VERSION\\)" _dummy_var "${_vulkan_core_file}")
    if (CMAKE_MATCH_COUNT EQUAL 3)
        set(VulkanHeaders_VERSION_TWEAK ${CMAKE_MATCH_1})
        set(VulkanHeaders_VERSION_MAJOR ${CMAKE_MATCH_2})
        set(VulkanHeaders_VERSION_MINOR ${CMAKE_MATCH_3})
        string(REGEX MATCH "#define[ \t]+VK_HEADER_VERSION[ \t]+([0-9]+)" _dummy_var "${_vulkan_core_file}")
        if (CMAKE_MATCH_COUNT EQUAL 1)
            set(VulkanHeaders_VERSION_PATCH ${CMAKE_MATCH_1})
            set(VulkanHeaders_VERSION "${VulkanHeaders_VERSION_MAJOR}.${VulkanHeaders_VERSION_MINOR}.${VulkanHeaders_VERSION_PATCH}.${VulkanHeaders_VERSION_TWEAK}")
        endif()
    endif()
    unset(_vulkan_core_file)
    unset(_dummy_var)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VulkanHeaders
    REQUIRED_VARS VulkanHeaders_INCLUDE_DIR
    VERSION_VAR VulkanHeaders_VERSION
)

if (VulkanHeaders_FOUND AND NOT TARGET Vulkan::Headers)
    add_library(Vulkan::Headers INTERFACE IMPORTED)
    set_target_properties(Vulkan::Headers PROPERTIES
        INTERFACE_COMPILE_OPTIONS "${VULKANHEADERS_CFLAGS_OTHER}"
        INTERFACE_INCLUDE_DIRECTORIES "${VulkanHeaders_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(VulkanHeaders_INCLUDE_DIR)
