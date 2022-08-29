# SPDX-FileCopyrightText: 2022 Andrea Pappacoda <andrea@pappacoda.it>
# SPDX-License-Identifier: ISC

include(FindPackageHandleStandardArgs)

find_package(zstd CONFIG)
if (zstd_FOUND)
	# Use upstream zstdConfig.cmake if possible
	if (NOT TARGET zstd::zstd)
		if (TARGET zstd::libzstd_static)
			add_library(zstd::zstd ALIAS zstd::libzstd_static)
		elseif (TARGET zstd::libzstd_shared)
			add_library(zstd::zstd ALIAS zstd::libzstd_shared)
		endif()
	endif()
	find_package_handle_standard_args(zstd CONFIG_MODE)
else()
	# Fallback to pkg-config otherwise
	find_package(PkgConfig)
	if (PKG_CONFIG_FOUND)
		pkg_search_module(libzstd IMPORTED_TARGET GLOBAL libzstd)
		if (libzstd_FOUND)
			add_library(zstd::zstd ALIAS PkgConfig::libzstd)
		endif()
	endif()

	find_package_handle_standard_args(zstd
		REQUIRED_VARS
			libzstd_LINK_LIBRARIES
			libzstd_FOUND
		VERSION_VAR libzstd_VERSION
	)
endif()
