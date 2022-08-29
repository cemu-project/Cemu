cmake_minimum_required(VERSION 3.24)

option(PUBLIC_RELEASE "Compile with debug asserts disabled and no console" OFF)
option(ENABLE_VCPKG "Enable the vcpkg package manager" ON)

# This needs to come before the project declaration for some reason
if (ENABLE_VCPKG)
  set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/vcpkg/scripts/buildsystems/vcpkg.cmake")
endif()

project(Cemu VERSION 0.1)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (PUBLIC_RELEASE)
  add_definitions(-DPUBLIC_RELEASE)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE) # enable LTO
endif()

if (ENABLE_VCPKG)
  set(VCPKG_LIBRARY_LINKAGE "static")
  # Set this so that all the various find_package() calls don't need an explicit
  # CONFIG option
  set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)
endif()

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

include_directories(src)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if (MSVC)
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Cemu)
endif()

if (MSVC)
  add_compile_options(/EHsc)
  add_compile_options(/fp:precise) # floating point model: precise
  add_compile_options(/GT) # fiber safe optimizations
  add_compile_options(/MDd)
  if (PUBLIC_RELEASE)
    message(STATUS "Using additional optimization flags for MSVC")
    add_compile_options(/Oi /Ot) # enable intrinsic functions, favor speed
  endif()
endif()

option(ENABLE_OPENGL "Enables the OpenGL video backend" ON)
option(ENABLE_VULKAN "Enables the Vulkan video backend" ON)
option(ENABLE_DISCORD_RPC "Enables the Discord Rich Presence feature" ON)
option(ENABLE_SDL "Enables the SDL input backend" ON)
option(ENABLE_CUBEB "Enabled cubeb audio backend" ON)
option(ENABLE_WXWIDGETS "Build with wxWidgets UI (Currently required)" ON)

if (WIN32)
  option(ENABLE_XINPUT "Enables the XInput input backend" ON)
  option(ENABLE_DIRECTINPUT "Enables the DirectInput input backend" ON)
  option(ENABLE_DIRECTAUDIO "Enables the DirectAudio audio backend" ON)
  option(ENABLE_XAUDIO "Enables the Xaudio audio backend" ON)
endif()

if (ENABLE_DIRECTINPUT)
  add_definitions(-DHAS_DIRECTINPUT)
endif()

set(THREADS_PREFER_PTHREAD_FLAG true)
find_package(Threads REQUIRED)
find_package(SDL2 REQUIRED)
find_package(CURL REQUIRED)
find_package(pugixml REQUIRED)
find_package(imgui REQUIRED)
find_package(RapidJSON REQUIRED)
find_package(Boost COMPONENTS program_options filesystem nowide REQUIRED)
find_package(libzip REQUIRED)
find_package(glslang REQUIRED)
find_package(ZLIB REQUIRED)
find_package(zstd MODULE REQUIRED) # MODULE so that zstd::zstd is available
find_package(OpenSSL COMPONENTS Crypto SSL REQUIRED)
find_package(glm REQUIRED)
find_package(fmt 7.0.2 EXACT REQUIRED)
find_package(PNG REQUIRED)

# glslang versions older than 11.11.0 define targets without a namespace
if (NOT TARGET glslang::SPIRV AND TARGET SPIRV)
  add_library(glslang::SPIRV ALIAS SPIRV)
endif()

if (UNIX)
  find_package(X11 REQUIRED)
endif()

if (ENABLE_VULKAN)
  include_directories("dependencies/Vulkan-Headers/include")
endif()

if (ENABLE_OPENGL)  
  find_package(OpenGL REQUIRED)
endif()

if (ENABLE_DISCORD_RPC)
  add_definitions(-DENABLE_DISCORD_RPC)
  add_subdirectory(dependencies/discord-rpc EXCLUDE_FROM_ALL)
  target_include_directories(discord-rpc INTERFACE ./dependencies/discord-rpc/include)
endif()

if (ENABLE_WXWIDGETS)
  find_package(wxWidgets 3.2.0 REQUIRED COMPONENTS base core gl propgrid xrc)
endif()

if (ENABLE_CUBEB)
  find_package(cubeb)
  if (NOT cubeb_FOUND)
    option(BUILD_TESTS "" OFF)
    option(BUILD_TOOLS "" OFF)
    option(BUNDLE_SPEEX "" OFF)
    set(USE_WINMM OFF CACHE BOOL "")
    add_subdirectory("dependencies/cubeb")
  endif()
endif()

add_subdirectory("dependencies/ih264d")

find_package(ZArchive)
if (NOT ZArchive_FOUND)
  add_subdirectory("dependencies/ZArchive")
endif()

add_subdirectory(src)
