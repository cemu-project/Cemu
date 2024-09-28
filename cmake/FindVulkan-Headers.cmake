# SPDX-FileCopyrightText: 2024 Cemu-Project
# SPDX-License-Identifier: ISC

find_package(PkgConfig QUIET)
pkg_search_module(VULKAN-HEADERS QUIET IMPORTED_TARGET vulkan-headers)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(vulkan-headers
    REQUIRED_VARS VULKAN-HEADERS_LINK_LIBRARIES)

if (vulkan-headers_FOUND AND NOT TARGET Vulkan::Headers)
    add_library(Vulkan::Headers ALIAS PkgConfig::VULKAN-HEADERS)
endif()
