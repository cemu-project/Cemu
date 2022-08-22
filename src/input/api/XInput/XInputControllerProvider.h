#pragma once
#if BOOST_OS_WINDOWS
#include "input/api/ControllerProvider.h"

#include <Xinput.h>

#ifndef HAS_XINPUT
#define HAS_XINPUT 1
#endif


class XInputControllerProvider : public ControllerProviderBase
{
	friend class XInputController;
public:
	XInputControllerProvider();
	~XInputControllerProvider() override;

	inline static InputAPI::Type kAPIType = InputAPI::XInput;
	InputAPI::Type api() const override { return kAPIType; }

	std::vector<std::shared_ptr<ControllerBase>> get_controllers() override;

private:
	HMODULE m_module = nullptr;
	decltype(&XInputGetBatteryInformation) m_XInputGetBatteryInformation;
	decltype(&XInputGetCapabilities) m_XInputGetCapabilities;
	decltype(&XInputSetState) m_XInputSetState;
	decltype(&XInputGetState) m_XInputGetState;
};

#endif