add_library(
        CemuAndroid
        SHARED
        native-lib.cpp
        JNIUtils.cpp
        JNIUtils.h
)

target_link_libraries(
        CemuAndroid
        PRIVATE
        -landroid
        CemuCommon
        CemuBin
        CemuAndroidGui
)
