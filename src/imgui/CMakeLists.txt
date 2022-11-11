add_library(imguiImpl
	imgui_impl_opengl3.cpp
	imgui_impl_opengl3.h
	imgui_impl_vulkan.cpp
	imgui_impl_vulkan.h
	imgui_extension.cpp
	imgui_extension.h
)

set_property(TARGET imguiImpl PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_include_directories(imguiImpl PUBLIC "../")

# imgui source files
target_sources(imguiImpl PRIVATE
"../../dependencies/imgui/imgui.cpp"
"../../dependencies/imgui/imgui_draw.cpp"
"../../dependencies/imgui/imgui_tables.cpp"
"../../dependencies/imgui/imgui_widgets.cpp"
)

target_include_directories(imguiImpl PUBLIC "../../dependencies/imgui/")

target_link_libraries(imguiImpl PRIVATE
	CemuCafe
	CemuCommon
	CemuGui
	CemuInput
	CemuResource
	CemuUtil
)
