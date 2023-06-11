add_library(
        CemuAndroid
        SHARED
        native-lib.cpp
)

target_link_libraries(
        CemuAndroid
        PRIVATE
        CemuCommon
        CemuAndroidGui
)
