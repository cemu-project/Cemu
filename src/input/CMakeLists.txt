add_library(CemuInput
	InputManager.cpp
	InputManager.h
	ControllerFactory.cpp
	ControllerFactory.h
	api/ControllerState.h
	api/Controller.cpp
	api/Controller.h
	api/ControllerState.cpp
	api/InputAPI.h
	api/ControllerProvider.h
	api/DSU/DSUController.h
	api/DSU/DSUControllerProvider.cpp
	api/DSU/DSUController.cpp
	api/DSU/DSUControllerProvider.h
	api/DSU/DSUMessages.h
	api/DSU/DSUMessages.cpp
	api/SDL/SDLController.cpp
	api/SDL/SDLControllerProvider.cpp
	api/SDL/SDLController.h
	api/SDL/SDLControllerProvider.h
	api/Keyboard/KeyboardControllerProvider.h
	api/Keyboard/KeyboardControllerProvider.cpp
	api/Keyboard/KeyboardController.cpp
	api/Keyboard/KeyboardController.h
	api/GameCube/GameCubeController.cpp
	api/GameCube/GameCubeControllerProvider.h
	api/GameCube/GameCubeControllerProvider.cpp
	api/GameCube/GameCubeController.h
	emulated/ProController.cpp
	emulated/EmulatedController.h
	emulated/EmulatedController.cpp
	emulated/ProController.h
	emulated/WPADController.cpp
	emulated/WPADController.h
	emulated/WiimoteController.h
	emulated/VPADController.cpp
	emulated/WiimoteController.cpp
	emulated/VPADController.h
	emulated/ClassicController.cpp
	emulated/ClassicController.h
)

set_property(TARGET CemuInput PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(WIN32)
	# XInput
	target_sources(CemuInput PRIVATE
		api/XInput/XInputControllerProvider.cpp
		api/XInput/XInputControllerProvider.h
		api/XInput/XInputController.cpp
		api/XInput/XInputController.h
	)

	# DirectInput
	target_sources(CemuInput PRIVATE
		api/DirectInput/DirectInputControllerProvider.cpp
		api/DirectInput/DirectInputController.h
		api/DirectInput/DirectInputControllerProvider.h
		api/DirectInput/DirectInputController.cpp
	)
endif()

if (ENABLE_WIIMOTE)
	target_compile_definitions(CemuInput PUBLIC SUPPORTS_WIIMOTE)
	target_sources(CemuInput PRIVATE
			api/Wiimote/WiimoteControllerProvider.h
			api/Wiimote/WiimoteControllerProvider.cpp
			api/Wiimote/WiimoteMessages.h
			api/Wiimote/NativeWiimoteController.h
			api/Wiimote/NativeWiimoteController.cpp
			api/Wiimote/WiimoteDevice.h
	)
	if (ENABLE_HIDAPI)
		target_sources(CemuInput PRIVATE
			api/Wiimote/hidapi/HidapiWiimote.cpp
			api/Wiimote/hidapi/HidapiWiimote.h
		)
	elseif (WIN32)
		target_sources(CemuInput PRIVATE
			api/Wiimote/windows/WinWiimoteDevice.cpp
			api/Wiimote/windows/WinWiimoteDevice.h
		)
	endif()
endif ()


target_include_directories(CemuInput PUBLIC "../")

target_link_libraries(CemuInput PRIVATE
	CemuCafe
	CemuCommon
	CemuConfig
	CemuGui
	CemuUtil
	Boost::headers
	Boost::program_options
	glm::glm
	pugixml::pugixml
	SDL2::SDL2
)
if (ENABLE_HIDAPI)
	target_link_libraries(CemuInput PRIVATE hidapi::hidapi)
endif()

if (ENABLE_WXWIDGETS)
	target_link_libraries(CemuInput PRIVATE wx::base wx::core)
endif()
