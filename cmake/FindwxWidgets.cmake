# SPDX-FileCopyrightText: 2022 Andrea Pappacoda <andrea@pappacoda.it>
# SPDX-License-Identifier: ISC

include(FindPackageHandleStandardArgs)
find_package(wxWidgets CONFIG COMPONENTS ${wxWidgets_FIND_COMPONENTS})

if (wxWidgets_FOUND)
	# Use upstream wxWidgetsConfig.cmake if possible
	find_package_handle_standard_args(wxWidgets CONFIG_MODE)
else()
	# Fall back to CMake's FindwxWidgets
	# Temporarily unset CMAKE_MODULE_PATH to avoid calling the current find
	# module recursively
	set(_tmp_module_path "${CMAKE_MODULE_PATH}")
	set(CMAKE_MODULE_PATH "")

	find_package(wxWidgets MODULE QUIET COMPONENTS ${wxWidgets_FIND_COMPONENTS})

	set(CMAKE_MODULE_PATH "${_tmp_module_path}")
	unset(_tmp_module_path)

	if (wxWidgets_FOUND)
		add_library(wx::base IMPORTED INTERFACE)
		target_include_directories(wx::base INTERFACE ${wxWidgets_INCLUDE_DIRS})
		target_link_libraries(wx::base INTERFACE ${wxWidgets_LIBRARIES})
		target_link_directories(wx::base INTERFACE ${wxWidgets_LIBRARY_DIRS})
		target_compile_definitions(wx::base INTERFACE ${wxWidgets_DEFINITIONS})
		target_compile_options(wx::base INTERFACE ${wxWidgets_CXX_FLAGS})

		# FindwxWidgets sets everything into a single set of variables, so it is
		# impossible to tell what libraries are required for what component.
		# To be compatible with wxWidgetsConfig, we create an alias for each
		# component so that the user can still use target_link_libraries(wx::gl)
		foreach(component ${wxWidgets_FIND_COMPONENTS})
			if (NOT component STREQUAL "base")
				# don't alias base to itself
				add_library(wx::${component} ALIAS wx::base)
			endif()
		endforeach()
	endif()

	find_package_handle_standard_args(wxWidgets
		REQUIRED_VARS
			wxWidgets_LIBRARIES
			wxWidgets_FOUND
		VERSION_VAR
			wxWidgets_VERSION_STRING
	)
endif()
