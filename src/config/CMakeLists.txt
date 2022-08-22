project(CemuConfig)

file(GLOB CPP_FILES *.cpp)
file(GLOB H_FILES *.h)
add_library(CemuConfig ${CPP_FILES} ${H_FILES})

set_property(TARGET CemuConfig PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_precompile_headers(CemuConfig PRIVATE ../Common/precompiled.h)

target_include_directories(CemuConfig PRIVATE ../)