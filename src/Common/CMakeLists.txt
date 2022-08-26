project(CemuCommon)

#include_directories(".")

file(GLOB CPP_FILES *.cpp)
file(GLOB H_FILES *.h)
add_library(CemuCommon ${CPP_FILES} ${H_FILES})

set_property(TARGET CemuCommon PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(WIN32)
target_sources(CemuCommon
PRIVATE
    windows/platform.cpp
    windows/platform.h
)
else()
target_sources(CemuCommon
PRIVATE
    unix/platform.cpp
    unix/platform.h
)
endif()

target_sources(CemuCommon
        PRIVATE
        ExceptionHandler/ExceptionHandler.cpp
        ExceptionHandler/ExceptionHandler.h
        )

target_precompile_headers(CemuCommon PUBLIC precompiled.h)
target_include_directories(CemuCommon PRIVATE ../)