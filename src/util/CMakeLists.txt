project(CemuUtil)

include_directories(".")

file(GLOB_RECURSE CPP_FILES *.cpp)
file(GLOB_RECURSE H_FILES *.h)

add_library(CemuUtil ${CPP_FILES} ${H_FILES})

set_property(TARGET CemuUtil PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_precompile_headers(CemuUtil PRIVATE ../Common/precompiled.h)

target_link_libraries(CemuUtil CemuCommon CemuConfig)
target_include_directories(CemuUtil PRIVATE ../)