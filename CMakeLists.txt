cmake_minimum_required(VERSION 3.21.1)

option(ENABLE_VCPKG "Enable the vcpkg package manager" ON)
option(PORTABLE "All data created and maintained by Cemu will be in the directory where the executable file is located" ON)
option(MACOS_BUNDLE "The executable when built on macOS will be created as an application bundle" OFF)
set(EXPERIMENTAL_VERSION "" CACHE STRING "") # used by CI script to set experimental version

if (EXPERIMENTAL_VERSION)
	add_definitions(-DEMULATOR_VERSION_MINOR=${EXPERIMENTAL_VERSION})
	execute_process(
			COMMAND git log --format=%h -1
			WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
			OUTPUT_VARIABLE GIT_HASH
			OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	add_definitions(-DEMULATOR_HASH=${GIT_HASH})
endif()

if (ENABLE_VCPKG)
	set(VCPKG_OVERLAY_PORTS "${CMAKE_CURRENT_LIST_DIR}/dependencies/vcpkg_overlay_ports")
	set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/vcpkg/scripts/buildsystems/vcpkg.cmake"
		CACHE STRING "Vcpkg toolchain file")
	# Set this so that all the various find_package() calls don't need an explicit
	# CONFIG option
	set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)
	if (WIN32)
		set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "")
	endif()
endif()

project(Cemu VERSION 2.0.0)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_definitions($<$<CONFIG:Debug>:CEMU_DEBUG_ASSERT>) # if build type is debug, set CEMU_DEBUG_ASSERT

if(PORTABLE)
	add_compile_definitions(PORTABLE)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# enable link time optimization for release builds
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)

if (MSVC)
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT CemuBin)
	# floating point model: precise, fiber safe optimizations
	add_compile_options(/EHsc /fp:precise)
	if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		# Speeds up static linking (especially helpful in incremental compilation)
		if((CMAKE_LINKER MATCHES ".*lld-link.*") AND (CMAKE_AR MATCHES ".*llvm-lib.*"))
			set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY STATIC_LIBRARY_OPTIONS /llvmlibthin)
		endif()
  	else()
	  add_compile_options(/GT)
	endif()
	# enable additional optimization flags for release builds
	add_compile_options($<$<CONFIG:Release,RelWithDebInfo>:/Oi>) # enable intrinsic functions
	add_compile_options($<$<CONFIG:Release,RelWithDebInfo>:/Ot>) # favor speed
endif()

if (APPLE)
    enable_language(OBJC OBJCXX)
endif()

if (UNIX AND NOT APPLE)
	option(ENABLE_WAYLAND "Build with Wayland support" ON)
endif()

option(ENABLE_OPENGL "Enables the OpenGL backend" ON)
option(ENABLE_VULKAN "Enables the Vulkan backend" ON)
option(ENABLE_DISCORD_RPC "Enables the Discord Rich Presence feature" ON)

# input backends
if (WIN32)
	option(ENABLE_XINPUT "Enables the usage of XInput" ON)
	option(ENABLE_DIRECTINPUT "Enables the usage of DirectInput" ON)
	add_compile_definitions(HAS_DIRECTINPUT)
endif()
option(ENABLE_SDL "Enables the SDLController backend" ON)

# audio backends
if (WIN32)
	option(ENABLE_DIRECTAUDIO "Enables the directaudio backend" ON)
	option(ENABLE_XAUDIO "Enables the xaudio backend" ON)
endif()
option(ENABLE_CUBEB "Enabled cubeb backend" ON)

option(ENABLE_WXWIDGETS "Build with wxWidgets UI (Currently required)" ON)

set(THREADS_PREFER_PTHREAD_FLAG true)
find_package(Threads REQUIRED)
find_package(SDL2 REQUIRED)
find_package(CURL REQUIRED)
find_package(pugixml REQUIRED)
find_package(RapidJSON REQUIRED)
find_package(Boost COMPONENTS program_options filesystem nowide REQUIRED)
find_package(libzip REQUIRED)
find_package(glslang REQUIRED)
find_package(ZLIB REQUIRED)
find_package(zstd MODULE REQUIRED) # MODULE so that zstd::zstd is available
find_package(OpenSSL COMPONENTS Crypto SSL REQUIRED)
find_package(glm REQUIRED)
find_package(fmt 9.1.0 REQUIRED)
find_package(PNG REQUIRED)

# glslang versions older than 11.11.0 define targets without a namespace
if (NOT TARGET glslang::SPIRV AND TARGET SPIRV)
	add_library(glslang::SPIRV ALIAS SPIRV)
endif()

if (UNIX AND NOT APPLE)
	find_package(X11 REQUIRED)
	if (ENABLE_WAYLAND)
		find_package(Wayland REQUIRED)
		add_compile_definitions(HAS_WAYLAND)
	endif()
	find_package(GTK3 REQUIRED)
endif()

if (ENABLE_VULKAN)
	include_directories("dependencies/Vulkan-Headers/include")
endif()

if (ENABLE_OPENGL)
	find_package(OpenGL REQUIRED)
endif()

if (ENABLE_DISCORD_RPC)
	add_compile_definitions(ENABLE_DISCORD_RPC)
	add_subdirectory(dependencies/discord-rpc EXCLUDE_FROM_ALL)
	target_include_directories(discord-rpc INTERFACE ./dependencies/discord-rpc/include)
endif()

if (ENABLE_WXWIDGETS)
	find_package(wxWidgets 3.2 REQUIRED COMPONENTS base core gl propgrid xrc)
endif()

if (ENABLE_CUBEB)
	if (NOT ENABLE_VCPKG)
	find_package(cubeb)
	endif()
	if (NOT cubeb_FOUND)
		option(BUILD_TESTS "" OFF)
		option(BUILD_TOOLS "" OFF)
		option(BUNDLE_SPEEX "" OFF)
		set(USE_WINMM OFF CACHE BOOL "")
		add_subdirectory("dependencies/cubeb" EXCLUDE_FROM_ALL)
		set_property(TARGET cubeb PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
		add_library(cubeb::cubeb ALIAS cubeb)
	endif()
	add_compile_definitions("HAS_CUBEB=1")
endif()

add_subdirectory("dependencies/ih264d" EXCLUDE_FROM_ALL)

find_package(ZArchive)
if (NOT ZArchive_FOUND)
	add_subdirectory("dependencies/ZArchive" EXCLUDE_FROM_ALL)
endif()

add_subdirectory(src)
