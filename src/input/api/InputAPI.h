#pragma once

#include "util/helpers/helpers.h"

namespace InputAPI
{
	enum Type
	{
		Keyboard,
		SDLController,
		XInput,
		DirectInput,
		DSUClient,
		GameCube,
		Wiimote,

		WGIGamepad,
		WGIRawController,

		MAX
	};

	constexpr std::string_view to_string(Type type)
	{
		switch (type)
		{
		case Keyboard:
			return "Keyboard";
		case DirectInput:
			return "DirectInput";
		case XInput:
			return "XInput";
		case Wiimote:
			return "Wiimote";
		case GameCube:
			return "GameCube";
		case DSUClient:
			return "DSUController";
		case WGIGamepad:
			return "WGIGamepad";
		case WGIRawController:
			return "WGIRawController";
		case SDLController:
			return "SDLController";
		default:
			break;
		}

		throw std::runtime_error(fmt::format("unknown input api: {}", to_underlying(type)));
	}

	constexpr Type from_string(std::string_view str)
	{
		if (str == to_string(Keyboard))
			return Keyboard;
		else if (str == to_string(DirectInput))
			return DirectInput;
		else if (str == to_string(XInput))
			return XInput;
		else if (str == to_string(Wiimote))
			return Wiimote;
		else if (str == to_string(GameCube))
			return GameCube;
		else if (str == to_string(DSUClient))
			return DSUClient;
		else if (str == to_string(SDLController))
			return SDLController;
		else if (str == "DSU") // legacy
			return DSUClient;
		
		//else if (str == "WGIGamepad")
		//	return WGIGamepad;
		//
		//else if (str == "WGIRawController")
		//	return WGIRawController;

		throw std::runtime_error(fmt::format("unknown input api: {}", str));
	}
}
