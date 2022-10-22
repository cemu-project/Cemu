project(cemuMain)

option(CEMU_CXX_FLAGS "Additional flags used for compiling Cemu source code")
if(CEMU_CXX_FLAGS)
	add_compile_options(${CEMU_CXX_FLAGS})
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
	message( FATAL_ERROR "Pointers are not 64bit" )
endif()

if(MSVC)
	add_compile_definitions(WIN32_LEAN_AND_MEAN CURL_STATICLIB)
	#add_compile_definitions(VK_USE_PLATFORM_WIN32_KHR)
	# _CRT_SECURE_NO_WARNINGS
	# _WINSOCK_DEPRECATED_NO_WARNINGS
	# _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
	# _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
elseif(UNIX)
	if(APPLE)
		add_compile_definitions(
			_XOPEN_SOURCE
			VK_USE_PLATFORM_MACOS_MVK
			VK_USE_PLATFORM_METAL_EXT
		)
	else()
		add_compile_definitions(
			VK_USE_PLATFORM_XLIB_KHR # legacy. Do we need to support XLIB surfaces?
			VK_USE_PLATFORM_XCB_KHR
		)
	endif()
	add_compile_options(-maes)
	# warnings
	if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
		add_compile_options(-Wno-ambiguous-reversed-operator)
	endif()

    add_compile_options(-Wno-multichar -Wno-invalid-offsetof -Wno-switch -Wno-ignored-attributes -Wno-deprecated-enum-enum-conversion)
endif()

add_compile_definitions(VK_NO_PROTOTYPES)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_subdirectory(Common)
add_subdirectory(gui)
add_subdirectory(Cafe)
add_subdirectory(Cemu)
add_subdirectory(config)
add_subdirectory(input)
add_subdirectory(audio)
add_subdirectory(util)
add_subdirectory(imgui)
add_subdirectory(resource)
add_subdirectory(asm)

add_executable(CemuBin
	main.cpp
	mainLLE.cpp
)

if(WIN32)
	target_sources(CemuBin PRIVATE
	resource/cemu.rc
)
endif()

set_property(TARGET CemuBin PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_property(TARGET CemuBin PROPERTY WIN32_EXECUTABLE $<NOT:$<CONFIG:Debug>>)

set_target_properties(CemuBin PROPERTIES
	# multi-configuration generators will add a config subdirectory to RUNTIME_OUTPUT_DIRECTORY if no generator expression is used
	# to get the same behavior everywhere we append an empty generator expression
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../bin/$<1:>"
	OUTPUT_NAME "Cemu_$<LOWER_CASE:$<CONFIG>>"
)

target_link_libraries(CemuBin PRIVATE
	CemuAudio
	CemuCafe
	CemuCommon
	CemuComponents
	CemuConfig
	CemuGui
	CemuInput
	CemuUtil
	OpenGL::GL
	SDL2::SDL2
)

if (ENABLE_WXWIDGETS)
	target_link_libraries(CemuBin PRIVATE wx::base wx::core)
endif()
