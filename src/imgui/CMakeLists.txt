project(imguiImpl)

add_library(imguiImpl)

target_sources(imguiImpl PRIVATE
	imgui_impl_opengl3.cpp
	imgui_impl_opengl3.h
	imgui_impl_vulkan.cpp
	imgui_impl_vulkan.h
	imgui_extension.cpp
	imgui_extension.h
)

target_include_directories(imguiImpl PUBLIC "../")

target_link_libraries(imguiImpl PRIVATE
	CemuCafe
	CemuCommon
	CemuGui
	CemuInput
	CemuResource
	CemuUtil
	imgui::imgui
)
