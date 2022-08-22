project(CemuCemuhook)

add_library(CemuCemuhook
wxEvtHook.inl
wxCemuhookExports.cpp
)

set_property(TARGET CemuCemuhook PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_precompile_headers(CemuCemuhook PRIVATE ../Common/precompiled.h)

target_include_directories(CemuCemuhook PRIVATE ../)