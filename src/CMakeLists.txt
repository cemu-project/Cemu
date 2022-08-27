project(cemuMain)

option(CEMU_CXX_FLAGS "Additional flags used for compiling Cemu source code")
if(CEMU_CXX_FLAGS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CEMU_CXX_FLAGS}")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    # all ok
else()
    message( FATAL_ERROR "Pointers are not 64bit" )
endif()

if(MSVC)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-DCURL_STATICLIB)
    #add_definitions(-DVK_USE_PLATFORM_WIN32_KHR)
    # _CRT_SECURE_NO_WARNINGS
    # _WINSOCK_DEPRECATED_NO_WARNINGS
    # _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
    # _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
elseif(UNIX)
    if(NOT APPLE)
        add_definitions(-DVK_USE_PLATFORM_XLIB_KHR) # legacy. Do we need to support XLIB surfaces?
        add_definitions(-DVK_USE_PLATFORM_XCB_KHR)
    endif()
    add_definitions(-fms-extensions)
    add_definitions(-fpermissive)
    add_definitions(-maes)
    # warnings
    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
	add_compile_options(-Wno-ambiguous-reversed-operator)
    endif()

    add_compile_options(-Wno-switch -Wno-ignored-attributes -Wno-deprecated-enum-enum-conversion)
endif()

add_definitions(-DVK_NO_PROTOTYPES)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_subdirectory(Common)
add_subdirectory(gui)
add_subdirectory(Cafe)
add_subdirectory(Cemu)
add_subdirectory(config)
add_subdirectory(input)
add_subdirectory(audio)
add_subdirectory(util)
add_subdirectory(imgui)
add_subdirectory(resource)
add_subdirectory(asm)

if(PUBLIC_RELEASE)
add_executable(CemuBin WIN32
main.cpp
mainLLE.cpp
)
else()
add_executable(CemuBin
main.cpp
mainLLE.cpp
)
endif()

target_precompile_headers(CemuBin PRIVATE Common/precompiled.h)

if(WIN32)
  target_sources(CemuBin PRIVATE
  resource/cemu.rc
  )
endif()

set_property(TARGET CemuBin PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set_target_properties(CemuBin PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/../bin/
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/../bin/
        OUTPUT_NAME "Cemu"
        )

target_link_libraries(CemuBin PRIVATE CemuCommon CemuComponents CemuCafe CemuConfig CemuGui CemuAudio CemuInput CemuUtil)
target_link_libraries(CemuBin PRIVATE CemuAsm)
target_link_libraries(CemuBin PRIVATE OpenSSL::SSL)
target_link_libraries(CemuBin PRIVATE ZLIB::ZLIB)
target_link_libraries(CemuBin PRIVATE ${wxWidgets_LIBRARIES})
target_link_libraries(CemuBin PRIVATE CURL::libcurl)
target_link_libraries(CemuBin PRIVATE imgui::imgui)
target_link_libraries(CemuBin PRIVATE pugixml pugixml::static pugixml::pugixml)

target_link_libraries(CemuBin PUBLIC
CemuCommon CemuAudio CemuInput CemuComponents CemuCafe CemuConfig CemuGui imguiImpl)

# needed because of some cyclic dependencies. fix this
target_link_libraries(CemuBin PUBLIC
CemuCommon CemuInput CemuComponents CemuCafe CemuResource CemuGui CemuAsm)
