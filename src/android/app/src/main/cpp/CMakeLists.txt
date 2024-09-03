add_library(CemuAndroid SHARED
        AndroidAudio.cpp
        AndroidAudio.h
        AndroidEmulatedController.cpp
        AndroidEmulatedController.h
        AndroidFilesystemCallbacks.h
        AndroidGameTitleLoadedCallback.h
        CMakeLists.txt
        CafeSystemUtils.cpp
        CafeSystemUtils.h
        EmulationState.h
        GameTitleLoader.cpp
        GameTitleLoader.h
        Image.cpp
        Image.h
        JNIUtils.cpp
        JNIUtils.h
        Utils.cpp
        Utils.h
        native-lib.cpp
        stb_image.h
)

target_link_libraries(CemuAndroid PRIVATE
        -landroid
        CemuCommon
        CemuAudio
        CemuComponents
        CemuCafe
        CemuBin
)
