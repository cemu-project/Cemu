# SPDX-FileCopyrightText: 2022 Andrea Pappacoda <andrea@pappacoda.it>
# SPDX-License-Identifier: ISC

include(FindPackageHandleStandardArgs)

find_package(PkgConfig REQUIRED)
if (PKG_CONFIG_FOUND)
    pkg_search_module(GTK3 REQUIRED gtk+-3.0)
    if (GTK3_FOUND)
        add_library(GTK3::gtk IMPORTED INTERFACE)
        target_link_libraries(GTK3::gtk INTERFACE ${GTK3_LIBRARIES})
        target_link_directories(GTK3::gtk INTERFACE ${GTK3_LIBRARY_DIRS})
        target_include_directories(GTK3::gtk INTERFACE ${GTK3_INCLUDE_DIRS})
    endif()
    find_package_handle_standard_args(GTK3
            REQUIRED_VARS
            GTK3_LINK_LIBRARIES
            GTK3_FOUND
            VERSION_VAR GTK3_VERSION
    )
endif()
