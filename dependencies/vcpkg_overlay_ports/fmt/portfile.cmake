vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO fmtlib/fmt
    REF a33701196adfad74917046096bf5a2aa0ab0bb50 # v9.1.0
    SHA512 0faf00e99b332fcb3d9fc50cc9649ddc004ca9035f3652c1a001facee725dab09f67b65a9dfcce0aedb47e76c74c45a9262a1fd6e250a9e9a27c7d021c8ee6b8
    HEAD_REF master
    # PATCHES fix-warning4189.patch
)

vcpkg_cmake_configure(
    SOURCE_PATH ${SOURCE_PATH}
    DISABLE_PARALLEL_CONFIGURE # with MSBuild (on UWP), fmt writes into the source directory
    OPTIONS
        -DFMT_CMAKE_DIR=share/fmt
        -DFMT_TEST=OFF
        -DFMT_DOC=OFF
)

vcpkg_cmake_install()
file(INSTALL ${SOURCE_PATH}/LICENSE.rst DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)
if(VCPKG_LIBRARY_LINKAGE STREQUAL dynamic)
    if(VCPKG_TARGET_IS_WINDOWS)
        if(NOT DEFINED VCPKG_BUILD_TYPE OR VCPKG_BUILD_TYPE STREQUAL "debug")
            if(EXISTS "${CURRENT_PACKAGES_DIR}/debug/lib/fmtd.dll")
                file(MAKE_DIRECTORY ${CURRENT_PACKAGES_DIR}/debug/bin)
                file(RENAME ${CURRENT_PACKAGES_DIR}/debug/lib/fmtd.dll ${CURRENT_PACKAGES_DIR}/debug/bin/fmtd.dll)
            endif()
        endif()
        if(NOT DEFINED VCPKG_BUILD_TYPE OR VCPKG_BUILD_TYPE STREQUAL "release")
            if(EXISTS "${CURRENT_PACKAGES_DIR}/lib/fmt.dll")
                file(MAKE_DIRECTORY ${CURRENT_PACKAGES_DIR}/bin)
                file(RENAME ${CURRENT_PACKAGES_DIR}/lib/fmt.dll ${CURRENT_PACKAGES_DIR}/bin/fmt.dll)
            endif()
        endif()
    endif()

    vcpkg_replace_string(${CURRENT_PACKAGES_DIR}/include/fmt/core.h
        "defined(FMT_SHARED)"
        "1"
    )
endif()
file(REMOVE_RECURSE ${CURRENT_PACKAGES_DIR}/debug/include)

vcpkg_cmake_config_fixup()
vcpkg_fixup_pkgconfig()

if(VCPKG_TARGET_IS_WINDOWS)
    if(NOT DEFINED VCPKG_BUILD_TYPE OR VCPKG_BUILD_TYPE STREQUAL "debug")
        vcpkg_replace_string(${CURRENT_PACKAGES_DIR}/share/fmt/fmt-targets-debug.cmake
            "lib/fmtd.dll"
            "bin/fmtd.dll"
        )
    endif()
    if(NOT DEFINED VCPKG_BUILD_TYPE OR VCPKG_BUILD_TYPE STREQUAL "release")
        vcpkg_replace_string(${CURRENT_PACKAGES_DIR}/share/fmt/fmt-targets-release.cmake
            "lib/fmt.dll"
            "bin/fmt.dll"
        )
    endif()
endif()
file(REMOVE_RECURSE ${CURRENT_PACKAGES_DIR}/debug/share)

# Handle post-build CMake instructions
vcpkg_copy_pdbs()
file(INSTALL ${CMAKE_CURRENT_LIST_DIR}/usage DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT})
