cmake_minimum_required(VERSION 3.21.1)

option(PUBLIC_RELEASE "Compile with debug asserts disabled and no console" OFF)

if (PUBLIC_RELEASE)
  add_definitions(-DPUBLIC_RELEASE)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE) # enable LTO
endif()

if (WIN32)
  set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "")
endif()

set(VCPKG_OVERLAY_PORTS "${CMAKE_CURRENT_LIST_DIR}/dependencies/vcpkg_overlay_ports")
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/vcpkg/scripts/buildsystems/vcpkg.cmake"
  CACHE STRING "Vcpkg toolchain file")

project(Cemu VERSION 0.1)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if (MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:precise") # floating point model: precise
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GT") # fiber safe optimizations
  if (PUBLIC_RELEASE)
    message(STATUS "Using additional optimization flags for MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Oi /Ot") # enable intrinsic functions, favor speed
  endif()
endif()

option(ENABLE_OPENGL "Enables the OpenGL backend" ON)
option(ENABLE_VULKAN "Enables the Vulkan backend" ON)
option(ENABLE_DISCORD_RPC "Enables the Discord Rich Presence feature" ON)

if (WIN32)
  option(ENABLE_CEMUHOOK "Enables Cemuhook compatibility" ON)
endif()

# input backends
if (WIN32)
  option(ENABLE_XINPUT "Enables the usage of XInput" ON)
  option(ENABLE_DIRECTINPUT "Enables the usage of DirectInput" ON)
  add_definitions(-DHAS_DIRECTINPUT)
endif()
option(ENABLE_SDL "Enables the SDLController backend" ON)

# audio backends
if (WIN32)
  option(ENABLE_DIRECTAUDIO "Enables the directaudio backend" ON)
  option(ENABLE_XAUDIO "Enables the xaudio backend" ON)
endif()
option(ENABLE_CUBEB "Enabled cubeb backend" ON)

option(ENABLE_WXWIDGETS "Build with wxWidgets UI (Currently required)" ON)

find_package(SDL2 CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(RapidJSON CONFIG REQUIRED)
find_package(Boost COMPONENTS program_options filesystem nowide REQUIRED)
find_package(libzip REQUIRED)
find_package(glslang REQUIRED)
find_package(ZLIB REQUIRED)
find_package(zstd CONFIG REQUIRED)

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
  find_package(wxWidgets CONFIG REQUIRED)
endif()

find_package(OpenSSL REQUIRED)
find_package(X11)

# find a better way to handle this
link_libraries(${Boost_LIBRARIES})
link_libraries(${X11_LIBRARIES})
link_libraries(SDL2::SDL2 SDL2::SDL2main SDL2::SDL2-static)
if (ENABLE_WXWIDGETS)
  link_libraries(wx::core wx::base)
endif()

if (ENABLE_CUBEB)
  option(BUILD_TESTS "" OFF)
  option(BUILD_TOOLS "" OFF)
  option(BUNDLE_SPEEX "" OFF)
  set(USE_WINMM OFF CACHE BOOL "")
  add_subdirectory(dependencies/cubeb)
  set_property(TARGET cubeb PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  link_libraries(cubeb)
  add_compile_definitions(HAS_CUBEB=1)
endif()

add_subdirectory(dependencies/ih264d)
add_subdirectory(dependencies/ZArchive)

add_subdirectory(src)

