# SPDX-FileCopyrightText: 2022 Andrea Pappacoda <andrea@pappacoda.it>
# SPDX-License-Identifier: ISC

find_package(PkgConfig)

if (PKG_CONFIG_FOUND)
	pkg_search_module(zarchive IMPORTED_TARGET GLOBAL zarchive)
	if (zarchive_FOUND)
		add_library(ZArchive::zarchive ALIAS PkgConfig::zarchive)
	endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ZArchive
	REQUIRED_VARS
		zarchive_LINK_LIBRARIES
		zarchive_FOUND
	VERSION_VAR
		zarchive_VERSION
)
