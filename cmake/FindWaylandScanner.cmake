# SPDX-FileCopyrightText: 2012-2014 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause

#[=======================================================================[.rst:
FindWaylandScanner
------------------

Try to find wayland-scanner.

If the wayland-scanner executable is not in your PATH, you can provide
an alternative name or full path location with the ``WaylandScanner_EXECUTABLE``
variable.

This will define the following variables:

``WaylandScanner_FOUND``
    True if wayland-scanner is available.

``WaylandScanner_EXECUTABLE``
    The wayland-scanner executable.

If ``WaylandScanner_FOUND`` is TRUE, it will also define the following imported
target:

``Wayland::Scanner``
    The wayland-scanner executable.

This module provides the following functions to generate C protocol
implementations:

  - ``ecm_add_wayland_client_protocol``
  - ``ecm_add_wayland_server_protocol``

::

  ecm_add_wayland_client_protocol(<target>
                                  PROTOCOL <xmlfile>
                                  BASENAME <basename>)

  ecm_add_wayland_client_protocol(<source_files_var>
                                  PROTOCOL <xmlfile>
                                  BASENAME <basename>)

Generate Wayland client protocol files from ``<xmlfile>`` XML
definition for the ``<basename>`` interface and append those files
to ``<source_files_var>`` or ``<target>``.

::

  ecm_add_wayland_server_protocol(<target>
                                  PROTOCOL <xmlfile>
                                  BASENAME <basename>)

  ecm_add_wayland_server_protocol(<source_files_var>
                                  PROTOCOL <xmlfile>
                                  BASENAME <basename>)

Generate Wayland server protocol files from ``<xmlfile>`` XML
definition for the ``<basename>`` interface and append those files
to ``<source_files_var>`` or ``<target>``.

Since 1.4.0.
#]=======================================================================]

include(${CMAKE_CURRENT_LIST_DIR}/ECMFindModuleHelpersStub.cmake)

ecm_find_package_version_check(WaylandScanner)

# Find wayland-scanner
find_program(WaylandScanner_EXECUTABLE NAMES wayland-scanner)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WaylandScanner
    FOUND_VAR
        WaylandScanner_FOUND
    REQUIRED_VARS
        WaylandScanner_EXECUTABLE
)

mark_as_advanced(WaylandScanner_EXECUTABLE)

if(NOT TARGET Wayland::Scanner AND WaylandScanner_FOUND)
    add_executable(Wayland::Scanner IMPORTED)
    set_target_properties(Wayland::Scanner PROPERTIES
        IMPORTED_LOCATION "${WaylandScanner_EXECUTABLE}"
    )
endif()

include(FeatureSummary)
set_package_properties(WaylandScanner PROPERTIES
    URL "https://wayland.freedesktop.org/"
    DESCRIPTION "Executable that converts XML protocol files to C code"
)


include(CMakeParseArguments)

function(ecm_add_wayland_client_protocol target_or_sources_var)
    # Parse arguments
    set(oneValueArgs PROTOCOL BASENAME)
    cmake_parse_arguments(ARGS "" "${oneValueArgs}" "" ${ARGN})

    if(ARGS_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown keywords given to ecm_add_wayland_client_protocol(): \"${ARGS_UNPARSED_ARGUMENTS}\"")
    endif()

    get_filename_component(_infile ${ARGS_PROTOCOL} ABSOLUTE)
    set(_client_header "${CMAKE_CURRENT_BINARY_DIR}/wayland-${ARGS_BASENAME}-client-protocol.h")
    set(_code "${CMAKE_CURRENT_BINARY_DIR}/wayland-${ARGS_BASENAME}-protocol.c")

    set_source_files_properties(${_client_header} GENERATED)
    set_source_files_properties(${_code} GENERATED)
    set_property(SOURCE ${_client_header} ${_code} PROPERTY SKIP_AUTOMOC ON)

    add_custom_command(OUTPUT "${_client_header}"
        COMMAND ${WaylandScanner_EXECUTABLE} client-header ${_infile} ${_client_header}
        DEPENDS ${_infile} VERBATIM)

    add_custom_command(OUTPUT "${_code}"
        COMMAND ${WaylandScanner_EXECUTABLE} public-code ${_infile} ${_code}
        DEPENDS ${_infile} ${_client_header} VERBATIM)

    if (TARGET ${target_or_sources_var})
        target_sources(${target_or_sources_var} PRIVATE "${_client_header}" "${_code}")
    else()
        list(APPEND ${target_or_sources_var} "${_client_header}" "${_code}")
        set(${target_or_sources_var} ${${target_or_sources_var}} PARENT_SCOPE)
    endif()
endfunction()


function(ecm_add_wayland_server_protocol target_or_sources_var)
    # Parse arguments
    set(oneValueArgs PROTOCOL BASENAME)
    cmake_parse_arguments(ARGS "" "${oneValueArgs}" "" ${ARGN})

    if(ARGS_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown keywords given to ecm_add_wayland_server_protocol(): \"${ARGS_UNPARSED_ARGUMENTS}\"")
    endif()

    ecm_add_wayland_client_protocol(${target_or_sources_var}
                                    PROTOCOL ${ARGS_PROTOCOL}
                                    BASENAME ${ARGS_BASENAME})

    get_filename_component(_infile ${ARGS_PROTOCOL} ABSOLUTE)
    set(_server_header "${CMAKE_CURRENT_BINARY_DIR}/wayland-${ARGS_BASENAME}-server-protocol.h")
    set(_server_code "${CMAKE_CURRENT_BINARY_DIR}/wayland-${ARGS_BASENAME}-protocol.c")
    set_property(SOURCE ${_server_header} ${_server_code} PROPERTY SKIP_AUTOMOC ON)
    set_source_files_properties(${_server_header} GENERATED)

    add_custom_command(OUTPUT "${_server_header}"
        COMMAND ${WaylandScanner_EXECUTABLE} server-header ${_infile} ${_server_header}
        DEPENDS ${_infile} VERBATIM)

    if (TARGET ${target_or_sources_var})
        target_sources(${target_or_sources_var} PRIVATE "${_server_header}")
    else()
        list(APPEND ${target_or_sources_var} "${_server_header}")
        set(${target_or_sources_var} ${${target_or_sources_var}} PARENT_SCOPE)
    endif()
endfunction()
