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
		if (ENABLE_VULKAN)
			add_compile_definitions(
				_XOPEN_SOURCE
				VK_USE_PLATFORM_MACOS_MVK
				VK_USE_PLATFORM_METAL_EXT
			)
		else()
			add_compile_definitions(_XOPEN_SOURCE)
		endif()
	else()
		if (ENABLE_VULKAN)
			add_compile_definitions(
				VK_USE_PLATFORM_XLIB_KHR # legacy. Do we need to support XLIB surfaces?
				VK_USE_PLATFORM_XCB_KHR
			)
			if (ENABLE_WAYLAND)
				add_compile_definitions(VK_USE_PLATFORM_WAYLAND_KHR)
			endif()
		endif()
	endif()
	# warnings
	if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
		add_compile_options(-Wno-ambiguous-reversed-operator)
	endif()

    add_compile_options(-Wno-multichar -Wno-invalid-offsetof -Wno-switch -Wno-ignored-attributes -Wno-deprecated-enum-enum-conversion)
endif()

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# on msvc we can reuse the PCH from CemuCommon, on other platforms it's not straight forward to reuse the PCH so we opt out of it
function(cemu_use_precompiled_header target_name)
	if(MSVC)
		target_precompile_headers(${target_name} REUSE_FROM CemuCommon)
	else()
		target_precompile_headers(${target_name} PRIVATE "${CMAKE_SOURCE_DIR}/src/Common/precompiled.h")
	endif()
endfunction()

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

add_executable(CemuBin
	main.cpp
	mainLLE.cpp
)

cemu_use_precompiled_header(CemuBin)

if(MSVC AND MSVC_VERSION EQUAL 1940)
	# workaround for an msvc issue on VS 17.10 where generated ILK files are too large
	# see https://developercommunity.visualstudio.com/t/After-updating-to-VS-1710-the-size-of-/10665511
	set_target_properties(CemuBin PROPERTIES LINK_FLAGS "/INCREMENTAL:NO")
endif()

if(WIN32)
	target_sources(CemuBin PRIVATE
	resource/cemu.rc
	../dist/windows/cemu.manifest
	)
endif()

if(APPLE AND CMAKE_GENERATOR STREQUAL "Xcode")
    set_target_properties(CemuBin PROPERTIES
        XCODE_ATTRIBUTE_ENABLE_DEBUG_DYLIB[variant=Debug] "YES"
    )
endif()

string(TIMESTAMP CURRENT_YEAR "%Y" UTC)

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
	set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${EMULATOR_VERSION_MAJOR}.${EMULATOR_VERSION_MINOR}.${EMULATOR_VERSION_PATCH}")
	set(MACOSX_BUNDLE_BUNDLE_VERSION "${EMULATOR_VERSION_MAJOR}.${EMULATOR_VERSION_MINOR}.${EMULATOR_VERSION_PATCH}")
	set(MACOSX_BUNDLE_COPYRIGHT "Copyright © ${CURRENT_YEAR} Cemu Project")

	set(MACOSX_BUNDLE_CATEGORY "public.app-category.games")
 	set(MACOSX_MINIMUM_SYSTEM_VERSION "13.4")
  	set(MACOSX_BUNDLE_TYPE_EXTENSION "wua")

	set_target_properties(CemuBin PROPERTIES
		MACOSX_BUNDLE true
		RESOURCE "${RESOURCE_FILES}"
	)

	set(FOLDERS gameProfiles resources)
	foreach(folder ${FOLDERS})
		add_custom_command (TARGET CemuBin POST_BUILD
			COMMAND ${CMAKE_COMMAND} ARGS -E copy_directory "${CMAKE_SOURCE_DIR}/bin/${folder}" "$<TARGET_BUNDLE_DIR:CemuBin>/Contents/SharedSupport/${folder}")
	endforeach(folder)

	if (ENABLE_VULKAN)
		if (EXISTS "/usr/local/lib/libMoltenVK.dylib")
			set(MOLTENVK_PATH "/usr/local/lib/libMoltenVK.dylib")
		elseif (EXISTS "/opt/homebrew/lib/libMoltenVK.dylib")
			set(MOLTENVK_PATH "/opt/homebrew/lib/libMoltenVK.dylib")
		else()
			message(FATAL_ERROR "failed to find libMoltenVK.dylib")
		endif ()
  endif()
  
	if (ENABLE_LIBUSB)
		if(CMAKE_BUILD_TYPE STREQUAL "Debug")
		    set(LIBUSB_PATH "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/debug/lib/libusb-1.0.0.dylib")
		else()
		    set(LIBUSB_PATH "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib/libusb-1.0.0.dylib")
		endif()
	endif()

	set(UPDATE_SH_PATH "${CMAKE_SOURCE_DIR}/src/resource/update.sh")
	set(APP_BUNDLE_DIR "$<TARGET_BUNDLE_DIR:CemuBin>")
	set(FRAMEWORKS_DIR "${APP_BUNDLE_DIR}/Contents/Frameworks")
	set(RESOURCES_DIR  "${APP_BUNDLE_DIR}/Contents/Resources")

	if (ENABLE_VULKAN)
		add_custom_command(TARGET CemuBin POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy 
				"${MOLTENVK_PATH}"
				"${FRAMEWORKS_DIR}/libMoltenVK.dylib"
		)
	endif()

	add_custom_command(TARGET CemuBin POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
			"${UPDATE_SH_PATH}"
			"${RESOURCES_DIR}/update.sh"
		COMMAND chmod 755
			"${RESOURCES_DIR}/update.sh"
	)

	if (ENABLE_LIBUSB)
		add_custom_command(TARGET CemuBin POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy
				"${LIBUSB_PATH}"
				"${FRAMEWORKS_DIR}/libusb-1.0.0.dylib"
		)
	endif()

	set_target_properties(CemuBin PROPERTIES
		BUILD_WITH_INSTALL_RPATH TRUE
		INSTALL_RPATH "@executable_path/../Frameworks"
	)

	if (ENABLE_LIBUSB)
		add_custom_command(TARGET CemuBin POST_BUILD
		COMMAND install_name_tool
			-change @rpath/libusb-1.0.0.dylib
				@executable_path/../Frameworks/libusb-1.0.0.dylib
				"$<TARGET_FILE:CemuBin>"
		)
	endif()
else()
    if(APPLE)
		if (ENABLE_VULKAN)
			find_library(MOLTENVK_LIBRARY
				NAMES MoltenVK moltenvk libMoltenVK.dylib
				PATHS /usr/local/lib /opt/homebrew/lib
			)
			if(MOLTENVK_LIBRARY)
				message(STATUS "Found MoltenVK: ${MOLTENVK_LIBRARY}")
				target_link_libraries(CemuBin PRIVATE ${MOLTENVK_LIBRARY})
			else()
				message(WARNING "libMoltenVK.dylib not found")
			endif()
			set_target_properties(CemuBin PROPERTIES
				BUILD_WITH_INSTALL_RPATH TRUE
				INSTALL_RPATH "/usr/local/lib;/opt/homebrew/lib"
			)
		endif()
    endif()
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
	SDL3::SDL3
)

if(UNIX AND NOT APPLE)
	# due to nasm output some linkers will make stack executable
	# cemu does not require this so we explicity disable it
	target_link_options(CemuBin PRIVATE -z noexecstack)
	# some residual debug info from boost/discord-rpc is normally included
	# most likely not helpful in debugging problems with cemu code
	target_link_options(CemuBin PRIVATE "$<$<CONFIG:Release>:-Xlinker;--strip-debug>")
endif()

if (BSD)
	target_link_libraries(CemuBin PRIVATE execinfo SPIRV-Tools SPIRV-Tools-opt)
endif()
