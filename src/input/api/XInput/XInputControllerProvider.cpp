#include <Windows.h>

#include "input/api/XInput/XInputControllerProvider.h"
#include "input/api/XInput/XInputController.h"

XInputControllerProvider::XInputControllerProvider()
{
	// try to load newest to oldest
	m_module = LoadLibraryA("XInput1_4.DLL");
	if (!m_module)
	{
		m_module = LoadLibraryA("XInput1_3.DLL");
		if (!m_module)
		{
			m_module = LoadLibraryA("XInput9_1_0.dll");
			if (!m_module)
				throw std::runtime_error("can't load any xinput dll");
		}
	}

#define GET_XINPUT_PROC(__FUNC__) m_##__FUNC__ = (decltype(m_##__FUNC__))GetProcAddress(m_module, #__FUNC__)
	GET_XINPUT_PROC(XInputGetCapabilities);
	GET_XINPUT_PROC(XInputGetState);
	GET_XINPUT_PROC(XInputSetState);

	if (!m_XInputGetCapabilities || !m_XInputGetState || !m_XInputSetState)
	{
		FreeLibrary(m_module);
		throw std::runtime_error("can't find necessary xinput functions");
	}

	// only available in XInput1_4 and XInput1_3
	GET_XINPUT_PROC(XInputGetBatteryInformation);
#undef GET_XINPUT_PROC

}

XInputControllerProvider::~XInputControllerProvider()
{
	if (m_module)
		FreeLibrary(m_module);
}

std::vector<std::shared_ptr<ControllerBase>> XInputControllerProvider::get_controllers()
{
	std::vector<std::shared_ptr<ControllerBase>> result;
	for(DWORD i = 0; i < XUSER_MAX_COUNT; ++i)
	{
		XINPUT_CAPABILITIES caps;
		if (m_XInputGetCapabilities(i, XINPUT_FLAG_GAMEPAD, &caps) == ERROR_SUCCESS)
		{
			result.emplace_back(std::make_shared<XInputController>(i));
		}
	}

	return result;
}
