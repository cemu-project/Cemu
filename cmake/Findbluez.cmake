# SPDX-FileCopyrightText: 2022 Andrea Pappacoda <andrea@pappacoda.it>
# SPDX-License-Identifier: ISC

find_package(bluez CONFIG)
if (NOT bluez_FOUND)
  find_package(PkgConfig)
  if (PKG_CONFIG_FOUND)
    pkg_search_module(bluez IMPORTED_TARGET GLOBAL bluez-1.0 bluez)
    if (bluez_FOUND)
      add_library(bluez::bluez ALIAS PkgConfig::bluez)
    endif ()
  endif ()
endif ()

find_package_handle_standard_args(bluez
    REQUIRED_VARS
    bluez_LINK_LIBRARIES
    bluez_FOUND
    VERSION_VAR bluez_VERSION
)
