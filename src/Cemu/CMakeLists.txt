project(CemuComponents)

file(GLOB_RECURSE CPP_FILES *.cpp)
file(GLOB_RECURSE H_FILES *.h)
add_library(CemuComponents ${CPP_FILES} ${H_FILES})

set_property(TARGET CemuComponents PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_precompile_headers(CemuComponents PRIVATE ../Common/precompiled.h)

target_include_directories(CemuComponents PRIVATE ../)

if(ENABLE_DISCORD_RPC)
target_link_libraries(CemuComponents PRIVATE discord-rpc)
endif()
