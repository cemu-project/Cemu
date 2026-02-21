
add_library(CemuCamera
        CameraManager.cpp
        CameraManager.h
        Rgb2Nv12.cpp
        Rgb2Nv12.h
        CameraManager.h
        CameraManager.cpp
        Rgb2Nv12.h
        Rgb2Nv12.cpp
)

set_property(TARGET CemuCamera PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_include_directories(CemuCamera PUBLIC "../")
target_link_libraries(CemuCamera PRIVATE CemuCommon CemuUtil openpnp-capture)
