add_library(CemuGui INTERFACE)
target_include_directories(CemuGui INTERFACE "interface")

if(ENABLE_WXWIDGETS)
    add_subdirectory(wxgui)
    target_link_libraries(CemuGui INTERFACE CemuWxGui)
endif()

# optional touch-friendly Qt/QML interface, started from the wxWidgets app with --mobile-ui
if(ENABLE_QT_MOBILE_UI)
    if(NOT ENABLE_WXWIDGETS OR NOT ENABLE_VULKAN OR NOT UNIX OR APPLE)
        message(FATAL_ERROR "ENABLE_QT_MOBILE_UI requires Linux/BSD with ENABLE_WXWIDGETS and ENABLE_VULKAN")
    endif()
    add_subdirectory(qtgui)
    target_link_libraries(CemuWxGui PRIVATE CemuQtGui)
    target_compile_definitions(CemuWxGui PRIVATE HAS_QT_MOBILE_UI)
endif()
