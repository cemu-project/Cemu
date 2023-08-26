# SPDX-FileCopyrightText: 2022 Andrea Pappacoda <andrea@pappacoda.it>
# SPDX-License-Identifier: ISC

find_package(libusb CONFIG)
if (NOT libusb_FOUND)
    find_package(PkgConfig)
    if (PKG_CONFIG_FOUND)
        pkg_search_module(libusb IMPORTED_TARGET GLOBAL libusb-1.0 libusb)
        if (libusb_FOUND)
            add_library(libusb::libusb ALIAS PkgConfig::libusb)
        endif ()
    endif ()
endif ()

find_package_handle_standard_args(libusb
        REQUIRED_VARS
        libusb_LINK_LIBRARIES
        libusb_FOUND
        VERSION_VAR libusb_VERSION
)
