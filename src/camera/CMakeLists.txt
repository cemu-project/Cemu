add_library(CemuCamera
    CameraManager.cpp
    CameraManager.h
    Rgb2Nv12.cpp
    Rgb2Nv12.h
)
target_include_directories(CemuCamera PUBLIC "../")
target_link_libraries(CemuCamera PRIVATE CemuCommon CemuUtil PUBLIC openpnp-capture)