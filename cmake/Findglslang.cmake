#.rst:
# Find Glslang
# ------------
#
# Finds the glslang library. This module defines:
#
#  Glslang_FOUND        - True if the Glslang library is found
#  Glslang::Glslang     - Glslang imported target.
#  Glslang::SPIRV       - Glslang SPIRV imported target. Depends on
#   Glslang::Glslang.
#  Glslang::HLSL        - Glslang HLSL imported target, if found. Static
#   dependency of Glslang::Glslang.
#  Glslang::OSDependent - Glslang OSDependent imported target, if found. Static
#   dependency of Glslang::Glslang.
#  Glslang::OGLCompiler - Glslang OGLCompiler imported target, if found. Static
#   dependency of Glslang::Glslang.
#
# Link to Glslang::Glslang and Glslang::SPIRV. The other dependencies, if
# needed, will get linked transitively.
#
# Additionally these variables are defined for internal usage:
#
#  Glslang_LIBRARY_{DEBUG,RELEASE} - Glslang library
#  Glslang_SPIRV_LIBRARY_{DEBUG,RELEASE} - Glslang SPIRV library
#  Glslang_HLSL_LIBRARY_{DEBUG,RELEASE} - Glslang HLSL library
#  Glslang_OSDependent_LIBRARY_{DEBUG,RELEASE} - Glslang OSDependent library
#  Glslang_OGLCompiler_LIBRARY_{DEBUG,RELEASE} - Glslang OGLCompiler library
#  Glslang_INCLUDE_DIR  - Include dir
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

# Actually, it's 2020 and none of this should be needed, but apparently Magnum
# is the first project ever to use this thing from Khronos as an actual library
# instead of the CLI tool. Their CMake system installs a ton of
# ``glslangTargets.cmake`` files, but no actual ``glslangConfig.cmake``, so I
# have to look for it myself. Scroll below for a continuation of this angry
# rant.

# If we have a CMake subproject, the glslang target should be defined. In that
# case we just alias Glslang::Glslang to it and exit.
if(TARGET glslang)
    # The glslang target doesn't define any usable
    # INTERFACE_INCLUDE_DIRECTORIES for some reason (the $<BUILD_INTERFACE:> in
    # there doesn't get expanded), so let's extract that from the SOURCE_DIR
    # property instead.
    get_target_property(_GLSLANG_INTERFACE_INCLUDE_DIRECTORIES glslang SOURCE_DIR)
    get_filename_component(_GLSLANG_INTERFACE_INCLUDE_DIRECTORIES ${_GLSLANG_INTERFACE_INCLUDE_DIRECTORIES} DIRECTORY)

    if(NOT TARGET Glslang::Glslang)
        # Aliases of (global) targets are only supported in CMake 3.11, so
        # we work around it by this. This is easier than fetching all
        # possible properties (which are impossible to track of) and then
        # attempting to rebuild them into a new target.
        add_library(Glslang::Glslang INTERFACE IMPORTED)
        set_target_properties(Glslang::Glslang PROPERTIES
            INTERFACE_LINK_LIBRARIES glslang
            INTERFACE_INCLUDE_DIRECTORIES ${_GLSLANG_INTERFACE_INCLUDE_DIRECTORIES})
    endif()
    if(NOT TARGET Glslang::SPIRV)
        # Aliases of (global) targets [..] CMake 3.11 [...], as above
        add_library(Glslang::SPIRV INTERFACE IMPORTED)
        set_target_properties(Glslang::SPIRV PROPERTIES
            # Make this depend on the Glslang::Glslang target to get the
            # include path along
            INTERFACE_LINK_LIBRARIES "SPIRV;Glslang::Glslang")
    endif()

    # Just to make FPHSA print some meaningful location, nothing else. Luckily
    # we can just reuse what we had to find above.
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args("Glslang" DEFAULT_MSG
        _GLSLANG_INTERFACE_INCLUDE_DIRECTORIES)

    return()
endif()

# The library depends on SPIRV-Tools, except if that's disabled, which we have
# no way to check anyway.
find_package(SpirvTools REQUIRED)

# Libraries. The debug suffix is used only on Windows.
find_library(Glslang_LIBRARY_RELEASE NAMES glslang)
find_library(Glslang_LIBRARY_DEBUG NAMES glslangd)
find_library(Glslang_SPIRV_LIBRARY_RELEASE NAMES SPIRV)
find_library(Glslang_SPIRV_LIBRARY_DEBUG NAMES SPIRVd)

include(SelectLibraryConfigurations)
select_library_configurations(Glslang)
select_library_configurations(Glslang_SPIRV)

# These are needed only in a static build, search for them only in case the
# main library is static (or on Windows, because there it's impossible to
# know -- in which case we'll just treat them as optional).
if(Glslang_LIBRARY MATCHES "${CMAKE_STATIC_LIBRARY_SUFFIX}$")
    set(_GLSLANG_IS_STATIC ON)
endif()
if(CORRADE_TARGET_WINDOWS OR _GLSLANG_IS_STATIC)
    # ARE YOU MAD?! Why the fuck can't you put everything into a single lib FFS
    set(_GLSLANG_STATIC_LIBRARIES HLSL OSDependent OGLCompiler)
    # Update 2021-06-19: WHAT THE FUCK, why do you keep adding new fucking
    # libs?! https://github.com/KhronosGroup/glslang/pull/2301 Also, why such a
    # huge breaking change is not even hinted in the 11.0 changelog?!
    set(_GLSLANG_STATIC_LIBRARIES_11 GenericCodeGen MachineIndependent)
    # For FPHSA, except the libraries that got added in glslang 11 -- we'll
    # treat them as optional, if they are there, we link them, if not, we
    # assume it's version 10 or older.
    set(_GLSLANG_EXTRA_LIBRARIES )
    foreach(_library ${_GLSLANG_STATIC_LIBRARIES} ${_GLSLANG_STATIC_LIBRARIES_11})
        find_library(Glslang_${_library}_LIBRARY_DEBUG NAMES ${_library}d)
        find_library(Glslang_${_library}_LIBRARY_RELEASE NAMES ${_library})
        select_library_configurations(Glslang_${_library})
        if(_library IN_LIST _GLSLANG_STATIC_LIBRARIES)
            list(APPEND _GLSLANG_EXTRA_LIBRARIES Glslang_${_library}_LIBRARY)
        endif()
    endforeach()
else()
    set(_GLSLANG_STATIC_LIBRARIES )
    set(_GLSLANG_EXTRA_LIBRARIES )
endif()

# Include dir
find_path(Glslang_INCLUDE_DIR
    # Actually, WHAT THE FUCK, I get that some people suck at naming, but this
    # is an actual naming skill black hole. Even naming it Windows.h would make
    # more sense than this. Like, what the hell.
    NAMES glslang/Public/ShaderLang.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Glslang DEFAULT_MSG
    Glslang_LIBRARY Glslang_SPIRV_LIBRARY ${_GLSLANG_EXTRA_LIBRARIES}
    Glslang_INCLUDE_DIR)

mark_as_advanced(FORCE Glslang_INCLUDE_DIR)

# Except for SPIRV these are all optional because needed only in a static
# build, which is impossible to detect (and some are present only in newer
# versions, FFS). And SPIRV was checked to be present in the FPHSA above, so
# it's guaranteed to be present.
foreach(_library SPIRV ${_GLSLANG_STATIC_LIBRARIES} ${_GLSLANG_STATIC_LIBRARIES_11})
    if(Glslang_${_library}_LIBRARY AND NOT TARGET Glslang::${_library})
        add_library(Glslang::${_library} UNKNOWN IMPORTED)
        if(Glslang_${_library}_LIBRARY_RELEASE)
            set_property(TARGET Glslang::${_library} APPEND PROPERTY
                IMPORTED_CONFIGURATIONS RELEASE)
            set_target_properties(Glslang::${_library} PROPERTIES
                IMPORTED_LOCATION_RELEASE ${Glslang_${_library}_LIBRARY_RELEASE})
        endif()
        if(Glslang_${_library}_LIBRARY_DEBUG)
            set_property(TARGET Glslang::${_library} APPEND PROPERTY
                IMPORTED_CONFIGURATIONS DEBUG)
            set_target_properties(Glslang::${_library} PROPERTIES
                IMPORTED_LOCATION_DEBUG ${Glslang_${_library}_LIBRARY_DEBUG})
        endif()

        # SPIRV depends on SpirvTools and glslang (which is created later)
        if(_library STREQUAL SPIRV)
            set_property(TARGET Glslang::${_library} APPEND PROPERTY
                INTERFACE_LINK_LIBRARIES SpirvTools::SpirvTools SpirvTools::Opt Glslang::Glslang)
        endif()

        # OGLCompiler needs pthread
        if(_library STREQUAL OGLCompiler)
            find_package(Threads REQUIRED)
            set_property(TARGET Glslang::${_library} APPEND PROPERTY
                INTERFACE_LINK_LIBRARIES Threads::Threads)
        endif()
    endif()
endforeach()

# Glslang::Glslang puts that all together
if(NOT TARGET Glslang::Glslang)
    add_library(Glslang::Glslang UNKNOWN IMPORTED)
    if(Glslang_LIBRARY_RELEASE)
        set_property(TARGET Glslang::Glslang APPEND PROPERTY
            IMPORTED_CONFIGURATIONS RELEASE)
        set_target_properties(Glslang::Glslang PROPERTIES
            IMPORTED_LOCATION_RELEASE ${Glslang_LIBRARY_RELEASE})
    endif()
    if(Glslang_LIBRARY_DEBUG)
        set_property(TARGET Glslang::Glslang APPEND PROPERTY
            IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(Glslang::Glslang PROPERTIES
            IMPORTED_LOCATION_DEBUG ${Glslang_LIBRARY_DEBUG})
    endif()
    set_target_properties(Glslang::Glslang PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES
            # Second entry to account for broken relative includes in version
            # 7.13. Sigh. https://github.com/KhronosGroup/glslang/issues/2007
            # This also papers over the difference between paths in the repo
            # and paths actually installed (in particular
            # <glslang/SPIRV/GlslangToSpirv.h> vs <SPIRV/GlslangToSpirv.h>)
            "${Glslang_INCLUDE_DIR};${Glslang_INCLUDE_DIR}/glslang")

    # On Windows these are all optional because a static build is impossible to
    # detect there
    if(_GLSLANG_IS_STATIC OR (CORRADE_TARGET_WINDOWS AND TARGET Glslang::HLSL))
        set_property(TARGET Glslang::Glslang APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES Glslang::HLSL)
    endif()
    if(_GLSLANG_IS_STATIC OR (CORRADE_TARGET_WINDOWS AND TARGET Glslang::OSDependent))
        set_property(TARGET Glslang::Glslang APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES Glslang::OSDependent)
    endif()
    if(_GLSLANG_IS_STATIC OR (CORRADE_TARGET_WINDOWS AND TARGET Glslang::OGLCompiler))
        set_property(TARGET Glslang::Glslang APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES Glslang::OGLCompiler)
    endif()
    # These two are new in 11.0, so link them only if they exist
    if((_GLSLANG_IS_STATIC OR CORRADE_TARGET_WINDOWS) AND TARGET Glslang::GenericCodeGen)
        set_property(TARGET Glslang::Glslang APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES Glslang::GenericCodeGen)
    endif()
    if((_GLSLANG_IS_STATIC OR CORRADE_TARGET_WINDOWS) AND TARGET Glslang::MachineIndependent)
        set_property(TARGET Glslang::Glslang APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES Glslang::MachineIndependent)
    endif()
endif()
