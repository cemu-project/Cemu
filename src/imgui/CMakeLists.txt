add_library(imguiImpl
  imgui_impl_opengl3.cpp
  imgui_impl_opengl3.h
  imgui_impl_vulkan.cpp
  imgui_impl_vulkan.h
  imgui_extension.cpp
  imgui_extension.h
)

target_precompile_headers(imguiImpl PRIVATE ../Common/precompiled.h)

target_link_libraries(imguiImpl PRIVATE
  Cafe
  Common
  Gui
  Input
  Resource
  Util
)

target_link_libraries(imguiImpl PRIVATE imgui::imgui)
