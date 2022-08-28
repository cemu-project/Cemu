# SPDX-FileCopyrightText: 2022 Andrea Pappacoda <andrea@pappacoda.it>
# SPDX-License-Identifier: ISC

include(FindPackageHandleStandardArgs)

find_package(imgui CONFIG)
if (imgui_FOUND)
	# Use upstream imguiConfig.cmake if possible
	find_package_handle_standard_args(imgui CONFIG_MODE)
else()
	# Fallback to pkg-config otherwise
	find_package(PkgConfig)
	if (PKG_CONFIG_FOUND)
		pkg_search_module(imgui IMPORTED_TARGET GLOBAL imgui)
		if (imgui_FOUND)
			add_library(imgui::imgui ALIAS PkgConfig::imgui)
		endif()
	endif()

	find_package_handle_standard_args(imgui
		REQUIRED_VARS
			imgui_LINK_LIBRARIES
			imgui_FOUND
		VERSION_VAR
			imgui_VERSION
	)
endif()
