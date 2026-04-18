enable_language(Swift)

add_library(CemuSwiftUiGui STATIC
  WindowSystemSwiftUI.mm
  ContentView.swift
)

set_target_properties(CemuSwiftUiGui PROPERTIES
  Swift_LANGUAGE_VERSION 5
)

set_property(TARGET CemuSwiftUiGui PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_include_directories(CemuSwiftUiGui PRIVATE "../../")
target_include_directories(CemuSwiftUiGui PUBLIC "../")

target_link_libraries(CemuSwiftUiGui PRIVATE
  CemuCommon
  CemuConfig
  CemuCafe
  CemuResource
)

find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
target_link_libraries(CemuSwiftUiGui PRIVATE ${COCOA_FRAMEWORK})

if(ALLOW_PORTABLE)
  target_compile_definitions(CemuSwiftUiGui PRIVATE CEMU_ALLOW_PORTABLE)
endif()
