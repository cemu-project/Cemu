#pragma once
#if BOOST_OS_WINDOWS

#define DIRECTINPUT_VERSION 0x0800

#include <dinput.h>

#include "input/api/ControllerProvider.h"

#ifndef HAS_DIRECTINPUT
#define HAS_DIRECTINPUT 1
#endif

class DirectInputControllerProvider : public ControllerProviderBase
{
public:
	DirectInputControllerProvider();
	~DirectInputControllerProvider() override;

	inline static InputAPI::Type kAPIType = InputAPI::DirectInput;
	InputAPI::Type api() const override { return kAPIType; }

	std::vector<std::shared_ptr<ControllerBase>> get_controllers() override;

	IDirectInput8* get_dinput() const { return m_dinput8; }
	LPCDIDATAFORMAT get_data_format() const;

private:
	HMODULE m_module = nullptr;

	decltype(&DirectInput8Create) m_DirectInput8Create;
	decltype(&GetdfDIJoystick) m_GetdfDIJoystick = nullptr;

	IDirectInput8* m_dinput8 = nullptr;
};

#endif