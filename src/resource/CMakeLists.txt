add_library(CemuResource)

set_property(TARGET CemuResource PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_precompile_headers(CemuResource PRIVATE ../Common/precompiled.h)

# icon resources
if(UNIX)
  target_sources(CemuResource PRIVATE
  linux/resources.cpp
  linux/resources.h
  )
endif()

target_sources(CemuResource PRIVATE
        CafeDefaultFont.cpp
        )

target_include_directories(CemuResource PRIVATE ../)
