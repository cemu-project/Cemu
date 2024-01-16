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
		if (ENABLE_WAYLAND)
			add_compile_definitions(VK_USE_PLATFORM_WAYLAND_KHR)
		endif()
	endif()
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
	../dist/windows/cemu.manifest
	)
endif()

set_property(TARGET CemuBin PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_property(TARGET CemuBin PROPERTY WIN32_EXECUTABLE $<NOT:$<CONFIG:Debug>>)
set(OUTPUT_NAME "Cemu_$<LOWER_CASE:$<CONFIG>>")

if (MACOS_BUNDLE)
	set_property(TARGET CemuBin PROPERTY MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/resource/MacOSXBundleInfo.plist.in")

	set(RESOURCE_FILES "${CMAKE_SOURCE_DIR}/src/resource/cemu.icns")
	target_sources(CemuBin PRIVATE "${RESOURCE_FILES}")

	set(MACOSX_BUNDLE_ICON_FILE "cemu.icns")
	set(MACOSX_BUNDLE_GUI_IDENTIFIER "info.cemu.Cemu")
	set(MACOSX_BUNDLE_BUNDLE_NAME "Cemu")
	set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${CMAKE_PROJECT_VERSION})
	set(MACOSX_BUNDLE_BUNDLE_VERSION ${CMAKE_PROJECT_VERSION})
	set(MACOSX_BUNDLE_COPYRIGHT "Copyright © 2024 Cemu Project")

	set(MACOSX_BUNDLE_CATEGORY "public.app-category.games")
 	set(MACOSX_MINIMUM_SYSTEM_VERSION "12.0")
  	set(MACOSX_BUNDLE_TYPE_EXTENSION "wua")

	set_target_properties(CemuBin PROPERTIES
		MACOSX_BUNDLE true
		RESOURCE "${RESOURCE_FILES}"
	)

	set(FOLDERS gameProfiles resources)
	foreach(folder ${FOLDERS})
		add_custom_command (TARGET CemuBin POST_BUILD
			COMMAND ${CMAKE_COMMAND} ARGS -E copy_directory "${CMAKE_SOURCE_DIR}/bin/${folder}" "${CMAKE_SOURCE_DIR}/bin/${OUTPUT_NAME}.app/Contents/SharedSupport/${folder}")
	endforeach(folder)

	add_custom_command (TARGET CemuBin POST_BUILD
		COMMAND ${CMAKE_COMMAND} ARGS -E copy "/usr/local/lib/libMoltenVK.dylib" "${CMAKE_SOURCE_DIR}/bin/${OUTPUT_NAME}.app/Contents/Frameworks/libMoltenVK.dylib"
		COMMAND ${CMAKE_COMMAND} ARGS -E copy "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-osx/lib/libusb-1.0.0.dylib" "${CMAKE_SOURCE_DIR}/bin/${OUTPUT_NAME}.app/Contents/Frameworks/libusb-1.0.0.dylib"
		COMMAND bash -c "install_name_tool -add_rpath @executable_path/../Frameworks ${CMAKE_SOURCE_DIR}/bin/${OUTPUT_NAME}.app/Contents/MacOS/${OUTPUT_NAME}"
		COMMAND bash -c "install_name_tool -change /usr/local/opt/libusb/lib/libusb-1.0.0.dylib @executable_path/../Frameworks/libusb-1.0.0.dylib ${CMAKE_SOURCE_DIR}/bin/${OUTPUT_NAME}.app/Contents/MacOS/${OUTPUT_NAME}")
endif()

set_target_properties(CemuBin PROPERTIES
	# multi-configuration generators will add a config subdirectory to RUNTIME_OUTPUT_DIRECTORY if no generator expression is used
	# to get the same behavior everywhere we append an empty generator expression
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../bin/$<1:>"
	OUTPUT_NAME "${OUTPUT_NAME}"
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

if(UNIX AND NOT APPLE)
	# due to nasm output some linkers will make stack executable
	# cemu does not require this so we explicity disable it
	target_link_options(CemuBin PRIVATE -z noexecstack)
	# some residual debug info from boost/discord-rpc is normally included
	# most likely not helpful in debugging problems with cemu code
	target_link_options(CemuBin PRIVATE "$<$<CONFIG:Release>:-Xlinker;--strip-debug>")
endif()

if (ENABLE_WXWIDGETS)
	target_link_libraries(CemuBin PRIVATE wx::base wx::core)
endif()
