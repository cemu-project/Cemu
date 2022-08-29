if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Pointers are not 64bit" )
endif()

add_executable(Cemu main.cpp mainLLE.cpp)

target_precompile_headers(Cemu PRIVATE Common/precompiled.h)

if (MSVC)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-DCURL_STATICLIB)
    #add_definitions(-DVK_USE_PLATFORM_WIN32_KHR)
elseif (UNIX)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      add_definitions(-fms-extensions)
      add_definitions(-fpermissive)
      add_definitions(-maes)
    endif()
    if (NOT APPLE)
      add_definitions(-DVK_USE_PLATFORM_XLIB_KHR) # legacy. Do we need to support XLIB surfaces?
      add_definitions(-DVK_USE_PLATFORM_XCB_KHR)
    endif()
endif()

add_definitions(-DVK_NO_PROTOTYPES)

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

if (WIN32)
  target_sources(Cemu PRIVATE resource/cemu.rc)
  set_target_properties(Cemu PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_link_libraries(Cemu PRIVATE
  Audio
  Cafe
  Common
  Components
  Config
  Gui
  Input
  Util
)

target_link_libraries(Cemu PRIVATE Assembly)
target_link_libraries(Cemu PRIVATE SDL2::SDL2) #SDL2::SDL2main) # is SDL2main needed?
target_link_libraries(Cemu PRIVATE imguiImpl OpenGL::GL)

if (ENABLE_WXWIDGETS)
  target_link_libraries(Cemu PRIVATE wx::base wx::core)
endif()
