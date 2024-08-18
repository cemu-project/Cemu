#include "input/ControllerFactory.h"

#include "input/emulated/VPADController.h"
#include "input/emulated/ProController.h"
#include "input/emulated/ClassicController.h"
#include "input/emulated/WiimoteController.h"

#include "input/api/SDL/SDLController.h"
#include "input/api/Keyboard/KeyboardController.h"
#include "input/api/DSU/DSUController.h"
#include "input/api/GameCube/GameCubeController.h"

#if BOOST_OS_WINDOWS
#include "input/api/XInput/XInputController.h"
#include "input/api/DirectInput/DirectInputController.h"
#endif

#if HAS_WIIMOTE
#include "input/api/Wiimote/NativeWiimoteController.h"
#endif

ControllerPtr ControllerFactory::CreateController(InputAPI::Type api, std::string_view uuid,
                                                  std::string_view display_name)
{
	switch (api)
	{
#if HAS_KEYBOARD
	case InputAPI::Keyboard:
		return std::make_shared<KeyboardController>();
#endif
#if HAS_DIRECTINPUT
	case InputAPI::DirectInput:
		{
			GUID guid;
			// Workaround for mouse2joystick users, which has 0 as it's uuid in it's profile and counts on Cemu applying it to the first directinput controller. GUIDFromString also doesn't allow for invalid uuids either.
			if (uuid == "0")
			{
				const auto provider = InputManager::instance().get_api_provider(InputAPI::DirectInput);
				const auto controllers = provider->get_controllers();
				if (controllers.empty())
					throw std::invalid_argument(fmt::format(
						"can't apply non-uuid-specific directinput profile when no controllers are available"));
				if (!GUIDFromString(controllers.front()->uuid().c_str(), guid))
					throw std::invalid_argument(fmt::format("invalid guid format: {}", uuid));
			}
			else
			{
				if (!GUIDFromString(uuid.data(), guid))
					throw std::invalid_argument(fmt::format("invalid guid format: {}", uuid));
			}

			return std::make_shared<DirectInputController>(guid);
		}
#endif
#if HAS_XINPUT
	case InputAPI::XInput:
		{
			const auto index = ConvertString<uint32>(uuid);
			return std::make_shared<XInputController>(index);
		}
#endif
#if HAS_SDL
	case InputAPI::SDLController:
		{
			// diid_guid
			const auto index = uuid.find_first_of('_');
			if (index == std::string_view::npos)
				throw std::invalid_argument(fmt::format("invalid sdl uuid format: {}", uuid));

			const auto guid_index = ConvertString<size_t>(uuid.substr(0, index));
			const auto guid = SDL_JoystickGetGUIDFromString(std::string{uuid.substr(index + 1)}.c_str());

			if (display_name.empty())
				return std::make_shared<SDLController>(guid, guid_index);
			else
				return std::make_shared<SDLController>(guid, guid_index, display_name);
		}
#endif
#if HAS_DSU
	case InputAPI::DSUClient:
		{
			const auto index = ConvertString<uint32>(uuid);
			return std::make_shared<DSUController>(index);
		}
#endif
#if HAS_GAMECUBE
	case InputAPI::GameCube:
		{
			const auto index = uuid.find_first_of('_');
			if (index == std::string_view::npos)
				throw std::invalid_argument(fmt::format("invalid gamecube uuid format: {}", uuid));

			const auto adapter = ConvertString<int>(uuid.substr(0, index));
			const auto controller_index = ConvertString<int>(uuid.substr(index + 1));
			return std::make_shared<GameCubeController>(adapter, controller_index);
		}
#endif
#if HAS_WIIMOTE
	case InputAPI::Wiimote:
		{
			const auto index = ConvertString<uint32>(uuid);
			return std::make_shared<NativeWiimoteController>(index);
		}
#endif
	default:
		throw std::invalid_argument(fmt::format("unhandled controller api: {}", api));
	}
	/*
	case InputAPI::WGIGamepad: break;
	case InputAPI::WGIRawController: break;
	*/
}

EmulatedControllerPtr
ControllerFactory::CreateEmulatedController(size_t player_index, EmulatedController::Type type)
{
	switch (type)
	{
	case EmulatedController::Type::VPAD:
		return std::make_shared<VPADController>(player_index);
	case EmulatedController::Type::Pro:
		return std::make_shared<ProController>(player_index);
	case EmulatedController::Type::Classic:
		return std::make_shared<ClassicController>(player_index);
	case EmulatedController::Type::Wiimote:
		return std::make_shared<WiimoteController>(player_index);
	default:
		throw std::runtime_error(fmt::format("unknown emulated controller type: {}", type));
	}
}

ControllerProviderPtr ControllerFactory::CreateControllerProvider(InputAPI::Type api, const ControllerProviderSettings& settings)
{
	switch (api)
	{
#if HAS_KEYBOARD
	case InputAPI::Keyboard:
		return std::make_shared<KeyboardControllerProvider>();
#endif
#if HAS_SDL
	case InputAPI::SDLController:
		return std::make_shared<SDLControllerProvider>();
#endif
#if HAS_XINPUT
	case InputAPI::XInput:
		return std::make_shared<XInputControllerProvider>();
#endif
#if HAS_DIRECTINPUT
	case InputAPI::DirectInput:
		return std::make_shared<DirectInputControllerProvider>();
#endif
#if HAS_DSU
	case InputAPI::DSUClient:
		{
			try
			{
				const auto& dsu_settings = dynamic_cast<const DSUProviderSettings&>(settings);
				return std::make_shared<DSUControllerProvider>(dsu_settings);
			}
			catch (const std::bad_cast&)
			{
				cemuLog_log(LogType::Force, "failing to cast ControllerProviderSettings class to DSUControllerProvider");
				return std::make_shared<DSUControllerProvider>();
			}
		}

#endif
#if HAS_GAMECUBE
	case InputAPI::GameCube:
		return std::make_shared<GameCubeControllerProvider>();
#endif
#if HAS_WIIMOTE
	case InputAPI::Wiimote:
		return std::make_shared<WiimoteControllerProvider>();
#endif
	default:
		cemu_assert_debug(false);
		return {};
	}
}
