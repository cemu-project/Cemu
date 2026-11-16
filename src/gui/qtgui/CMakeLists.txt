# Touch-friendly Qt/QML interface ("mobile UI"), started with `Cemu --mobile-ui`.
#
# It is split in two libraries on purpose:
#  - CemuQtBridge: talks to the emulator core, uses the Cemu precompiled header, no Qt
#  - CemuQtGui:    the actual Qt/QML code, never includes Cemu headers
# Mixing both worlds in one translation unit causes macro/type clashes (X11, GTK, signals/slots, ...).

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Qml Quick QuickControls2)
# Qt >= 6.9 ships the private modules as separate components; older versions define Qt6::GuiPrivate with Gui
find_package(Qt6 QUIET OPTIONAL_COMPONENTS GuiPrivate)

add_library(CemuQtBridge STATIC
  MobileBackend.cpp
  MobileBackend.h
)

cemu_use_precompiled_header(CemuQtBridge)

target_include_directories(CemuQtBridge PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" PRIVATE "../" "../interface")

target_link_libraries(CemuQtBridge PRIVATE
  CemuCommon
  CemuComponents
  CemuCafe
  CemuConfig
  CemuInput
  CemuUtil
  glm::glm
  pugixml::pugixml
  ZArchive::zarchive
  libzip::zip
  CURL::libcurl
)

if(ENABLE_SDL)
  target_link_libraries(CemuQtBridge PRIVATE SDL3::SDL3)
endif()

add_library(CemuQtGui STATIC
  QtMobileUI.cpp
  QtMobileUI.h
  Controller.cpp
  Controller.h
  GameListModel.cpp
  GameListModel.h
  GameWindow.cpp
  GameWindow.h
  cemu_mobile.qrc
)

set_target_properties(CemuQtGui PROPERTIES
  AUTOMOC ON
  AUTORCC ON
  AUTOGEN_ORIGIN_DEPENDS OFF
)

target_compile_definitions(CemuQtGui PRIVATE QT_NO_KEYWORDS)

target_include_directories(CemuQtGui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(CemuQtGui
  PRIVATE
  Qt6::Core
  Qt6::Gui
  Qt6::GuiPrivate # qpa/qplatformnativeinterface.h, to get the wl_surface / X11 window of the render window
  Qt6::Widgets
  Qt6::Qml
  Qt6::Quick
  Qt6::QuickControls2
  PUBLIC
  CemuQtBridge
)
