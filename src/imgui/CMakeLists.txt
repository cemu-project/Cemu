project(imguiImpl)

include_directories(".")

add_library(imguiImpl)

set_property(TARGET imguiImpl PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_sources(imguiImpl PRIVATE
  imgui_impl_opengl3.cpp
  imgui_impl_opengl3.h
  imgui_impl_vulkan.cpp
  imgui_impl_vulkan.h
  imgui_extension.cpp
  imgui_extension.h
)

target_precompile_headers(imguiImpl PRIVATE ../Common/precompiled.h)

target_link_libraries(CemuCommon)
target_include_directories(imguiImpl PRIVATE ../)