add_library(CemuResource)

set_property(TARGET CemuResource PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

enable_language(ASM)

# icon resources
target_sources(CemuResource PRIVATE
embedded/resources.cpp
embedded/resources.h
)

if(UNIX)
    if(NOT APPLE)
        target_sources(CemuResource PRIVATE embedded/fontawesome.S)
    else()
        target_sources(CemuResource PRIVATE embedded/fontawesome_macos.S)
    endif()
endif()

target_sources(CemuResource PRIVATE CafeDefaultFont.cpp)

target_include_directories(CemuResource PUBLIC "../")

target_link_libraries(CemuResource PRIVATE CemuCommon CemuComponents)
