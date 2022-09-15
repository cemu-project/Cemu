#.rst:
# Find SPIRV-Tools
# ----------------
#
# Finds the SpirvTools library. This module defines:
#
#  SpirvTools_FOUND          - True if SPIRV-Tools are found
#  SpirvTools::SpirvTools    - SpirvTools imported target
#  SpirvTools::Opt           - SpirvTools optimizer imported target. Depends on
#   SpirvTools::SpirvTools.
#
# Additionally these variables are defined for internal usage:
#
#  SpirvTools_LIBRARY        - SpirvTools library
#  SpirvTools_Opt_LIBRARY    - SpirvTools library
#  SpirvTools_INCLUDE_DIR    - Include dir
#

#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
#               2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
#
#   Permission is hereby granted, free of charge, to any person obtaining a
#   copy of this software and associated documentation files (the "Software"),
#   to deal in the Software without restriction, including without limitation
#   the rights to use, copy, modify, merge, publish, distribute, sublicense,
#   and/or sell copies of the Software, and to permit persons to whom the
#   Software is furnished to do so, subject to the following conditions:
#
#   The above copyright notice and this permission notice shall be included
#   in all copies or substantial portions of the Software.
#
#   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#   DEALINGS IN THE SOFTWARE.
#

# If unicorn-shitting rainbows were real, we could find the package by doing
# as little as
#
#   find_package(SPIRV-Tools CONFIG REQUIRED)
#
# ... err, and, well, also this and others:
#
#   find_package(SPIRV-Tools-opt CONFIG REQUIRED)
#
# The dash in a package name and a separate package for each and every damn
# target is just a minor hiccup in the grand scheme of things (A DASH?! ARE YOU
# OUT OF YOUR #$@ MIND?!), nevertheless, this would be rather straightforward.
# HOWEVER, in the GRIM reality of 2020, and now 2021:
#
# 1. The 2020.4 spirv-tools package in Homebrew doesn't bother installing the
#    CMake config modules, so we have to find it the old way.
# 2. The 2020.5 release had broken CMake config to the point of causing an
#    *unrecoverable* CMake error. The only way to work around this was to
#    override get_target_property() via a macro and insert an early-out for
#    this case, then continuing to search the classic way. The 2020.6 package
#    doesn't have this issue anymore. The workaround unfortunately caused
#    an infinite recursion if FindSpirvTools was called more than once per
#    project and so had to be removed. If you still have to use the 2020.5
#    package, set `CMAKE_DISABLE_FIND_PACKAGE_SPIRV-Tools` to skip the broken
#    config.
#
# This module is thus *deliberately* called SpirvTools instead of SPIRV-Tools
# to avoid conflicts with the upstream config file, if present.
#
# Actually, to make things even worse, there's SPIRV-Tools-shared and
# SPIRV-Tools (or SPIRV-Tools-static, in newer versions). The former is always
# a shared lib, while the latter is either a static lib or a shared lib *and
# then* there's also SPIRV-Tools-opt, which always depends on
# SPIRV-Tools[-static] and *never* on SPIRV-Tools-shared. Which means, if one
# wants to use the optimizer as well, it just doesn't make sense to link to
# SPIRV-Tools-shared at all, because we'd need to pull in the static variant as
# well. Thus, as much as I'd like to use the shared library to save memory on
# sane OSes, it's just totally F-ing pointless.

# In case we have SPIRV-Tools as a CMake subproject, SPIRV-Tools should be
# defined. If it's not, try to find the installed config file.
if(NOT TARGET SPIRV-Tools)
    find_package(SPIRV-Tools CONFIG QUIET)
endif()

# We have a CMake subproject or a package where the config files got LUCKILY
# installed, point SpirvTools::SpirvTools there and exit -- nothing else to do
# here. As said above, we just ignore the SPIRV-Tools-shared target as the
# optimizer depends always on the static libraries.
if(TARGET SPIRV-Tools)
    # For some F reason, the optimizer is a completely separate thing?! Why
    # it's not a module of the same package, ffs?! Not doing this in case the
    # target already exists in case of a CMake subproject, ofc.
    if(NOT TARGET SPIRV-Tools-opt)
        find_package(SPIRV-Tools-opt CONFIG REQUIRED)
    endif()

    get_target_property(_SPIRVTOOLS_INTERFACE_INCLUDE_DIRECTORIES SPIRV-Tools INTERFACE_INCLUDE_DIRECTORIES)
    # In case of a CMake subproject, the SPIRV-Tools target doesn't define any
    # usable INTERFACE_INCLUDE_DIRECTORIES for some reason (the
    # $<BUILD_INTERFACE:> in there doesn't get expanded), so let's extract that
    # from the SOURCE_DIR property instead.
    if(_SPIRVTOOLS_INTERFACE_INCLUDE_DIRECTORIES MATCHES "<BUILD_INTERFACE:")
        get_target_property(_SPIRVTOOLS_INTERFACE_INCLUDE_DIRECTORIES SPIRV-Tools SOURCE_DIR)
        get_filename_component(_SPIRVTOOLS_INTERFACE_INCLUDE_DIRECTORIES ${_SPIRVTOOLS_INTERFACE_INCLUDE_DIRECTORIES} DIRECTORY)
        set(_SPIRVTOOLS_INTERFACE_INCLUDE_DIRECTORIES ${_SPIRVTOOLS_INTERFACE_INCLUDE_DIRECTORIES}/include)

        # We need to add this include dir to the imported target otherwise it
        # won't work
        set(_SPIRVTOOLS_NEEDS_INCLUDE ON)
    endif()

    if(NOT TARGET SpirvTools::SpirvTools)
        # Aliases of (global) targets are only supported in CMake 3.11, so we
        # work around it by this. This is easier than fetching all possible
        # properties (which are impossible to track of) and then attempting to
        # rebuild them into a new target.
        add_library(SpirvTools::SpirvTools INTERFACE IMPORTED)
        set_target_properties(SpirvTools::SpirvTools PROPERTIES INTERFACE_LINK_LIBRARIES SPIRV-Tools)

        # If we have a CMake subproject, this needs to be done otherwise the
        # includes won't be found
        if(_SPIRVTOOLS_NEEDS_INCLUDE)
            set_target_properties(SpirvTools::SpirvTools PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${_SPIRVTOOLS_INTERFACE_INCLUDE_DIRECTORIES})
        endif()
    endif()
    if(NOT TARGET SpirvTools::Opt)
        # Aliases of (global) targets [..] CMake 3.11 [...], as above
        add_library(SpirvTools::Opt INTERFACE IMPORTED)
        set_target_properties(SpirvTools::Opt PROPERTIES INTERFACE_LINK_LIBRARIES SPIRV-Tools-opt)
    endif()

    # Just to make FPHSA print some meaningful location, nothing else. Luckily
    # we can just reuse what we had to find above.
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args("SpirvTools" DEFAULT_MSG
        _SPIRVTOOLS_INTERFACE_INCLUDE_DIRECTORIES)

    return()
endif()

# Libraries. See above why this completely ignores SPIRV-Tools-shared.
find_library(SpirvTools_LIBRARY NAMES SPIRV-Tools)
find_library(SpirvTools_Opt_LIBRARY NAMES SPIRV-Tools-opt)

# Include dir
find_path(SpirvTools_INCLUDE_DIR
    NAMES spirv-tools/libspirv.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SpirvTools DEFAULT_MSG
    SpirvTools_LIBRARY
    SpirvTools_INCLUDE_DIR)

mark_as_advanced(FORCE
    SpirvTools_LIBRARY
    SpirvTools_INCLUDE_DIR)

if(NOT TARGET SpirvTools::SpirvTools)
    add_library(SpirvTools::SpirvTools UNKNOWN IMPORTED)
    set_target_properties(SpirvTools::SpirvTools PROPERTIES
        IMPORTED_LOCATION ${SpirvTools_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${SpirvTools_INCLUDE_DIR})
endif()

if(NOT TARGET SpirvTools::Opt)
    add_library(SpirvTools::Opt UNKNOWN IMPORTED)
    set_target_properties(SpirvTools::Opt PROPERTIES
        IMPORTED_LOCATION ${SpirvTools_Opt_LIBRARY}
        INTERFACE_LINK_LIBRARIES SpirvTools::SpirvTools)
endif()
