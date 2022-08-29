add_library(Input
  api/Controller.cpp
  api/Controller.h
  api/ControllerProvider.h
  api/ControllerState.cpp
  api/ControllerState.h
  api/DirectInput
  api/DirectInput/DirectInputController.cpp
  api/DirectInput/DirectInputController.h
  api/DirectInput/DirectInputControllerProvider.cpp
  api/DirectInput/DirectInputControllerProvider.h
  api/DSU/DSUController.cpp
  api/DSU/DSUController.h
  api/DSU/DSUControllerProvider.cpp
  api/DSU/DSUControllerProvider.h
  api/DSU/DSUMessages.cpp
  api/DSU/DSUMessages.h
  api/GameCube/GameCubeController.cpp
  api/GameCube/GameCubeController.h
  api/GameCube/GameCubeControllerProvider.cpp
  api/GameCube/GameCubeControllerProvider.h
  api/InputAPI.h
  api/Keyboard/KeyboardController.cpp
  api/Keyboard/KeyboardController.h
  api/Keyboard/KeyboardControllerProvider.cpp
  api/Keyboard/KeyboardControllerProvider.h
  api/SDL/SDLController.cpp
  api/SDL/SDLController.h
  api/SDL/SDLControllerProvider.cpp
  api/SDL/SDLControllerProvider.h
  ControllerFactory.cpp
  ControllerFactory.h
  emulated/ClassicController.cpp
  emulated/ClassicController.h
  emulated/EmulatedController.cpp
  emulated/EmulatedController.h
  emulated/ProController.cpp
  emulated/ProController.h
  emulated/VPADController.cpp
  emulated/VPADController.h
  emulated/WiimoteController.cpp
  emulated/WiimoteController.h
  emulated/WPADController.cpp
  emulated/WPADController.h
  InputManager.cpp
  InputManager.h
  motion/Mahony.h
  motion/MotionHandler.h
  motion/MotionSample.h
)

if (WIN32)
  target_sources(Input PRIVATE
    api/DirectInput/DirectInputControllerProvider.cpp
    api/DirectInput/DirectInputController.h
    api/DirectInput/DirectInputControllerProvider.h
    api/DirectInput/DirectInputController.cpp
    api/XInput/XInputControllerProvider.cpp
    api/XInput/XInputControllerProvider.h
    api/XInput/XInputController.cpp
    api/XInput/XInputController.h
    # Native wiimote is Win32 only for now
    api/Wiimote/WiimoteControllerProvider.h
    api/Wiimote/windows/WinWiimoteDevice.cpp
    api/Wiimote/windows/WinWiimoteDevice.h
    api/Wiimote/WiimoteControllerProvider.cpp
    api/Wiimote/WiimoteMessages.h
    api/Wiimote/NativeWiimoteController.h
    api/Wiimote/NativeWiimoteController.cpp
    api/Wiimote/WiimoteDevice.h
  )
endif()

target_precompile_headers(Input PRIVATE ../Common/precompiled.h)

target_link_libraries(Input PRIVATE
  Cafe
  Common
  Config
  Gui
  Util
)

target_link_libraries(Input PRIVATE
  Boost::headers
  Boost::program_options
  glm::glm
  pugixml::pugixml
  SDL2::SDL2
)

if (ENABLE_WXWIDGETS)
  target_link_libraries(Input PRIVATE wx::base wx::core)
endif()
