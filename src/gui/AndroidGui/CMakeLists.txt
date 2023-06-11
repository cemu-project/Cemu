
add_library(CemuAndroidGui
        AndroidGuiWrapper.cpp
        GameIconLoader.cpp
        GameIconLoader.h
        GameTitleLoader.cpp
        GameTitleLoader.h
        OpenGLCanvas.cpp
        Image.cpp
)

target_include_directories(CemuAndroidGui PUBLIC "../")

target_link_libraries(CemuAndroidGui PRIVATE
        CemuAudio
        CemuCafe
        CemuCommon
        CemuComponents
        CemuConfig
        CemuInput
        CemuResource
        CemuUtil
        Boost::headers
        CURL::libcurl
        libzip::zip
        OpenSSL::Crypto
        pugixml::pugixml
        ZArchive::zarchive
)
