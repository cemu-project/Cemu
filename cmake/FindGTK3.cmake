# SPDX-FileCopyrightText: 2022 Andrea Pappacoda <andrea@pappacoda.it>
# SPDX-License-Identifier: ISC

include(FindPackageHandleStandardArgs)

find_package(PkgConfig)
if (PKG_CONFIG_FOUND)
    pkg_search_module(GTK3 IMPORTED_TARGET gtk+-3.0)
    if (GTK3_FOUND)
        add_library(GTK3::gtk ALIAS PkgConfig::GTK3)
    endif()
    find_package_handle_standard_args(GTK3
            REQUIRED_VARS GTK3_LINK_LIBRARIES
            VERSION_VAR GTK3_VERSION
    )
endif()
