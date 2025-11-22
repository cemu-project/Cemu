#include "input/api/DirectInput/DirectInputControllerProvider.h"

#include "input/api/DirectInput/DirectInputController.h"

DirectInputControllerProvider::DirectInputControllerProvider()
{
	const auto r = DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8W, (void**)&m_dinput8, nullptr);
	if (FAILED(r))
	{
		const auto error = GetLastError();
		throw std::runtime_error(fmt::format("can't create direct input object (error: {:#x})", error));
	}
}

DirectInputControllerProvider::~DirectInputControllerProvider()
{
}

std::vector<std::shared_ptr<ControllerBase>> DirectInputControllerProvider::get_controllers()
{
	std::vector<std::shared_ptr<ControllerBase>> result;

	m_dinput8->EnumDevices(DI8DEVCLASS_GAMECTRL,
		[](LPCDIDEVICEINSTANCEW lpddi, LPVOID pvRef) -> BOOL
		{
			auto* controllers = (decltype(&result))pvRef;

			std::string display_name = boost::nowide::narrow(lpddi->tszProductName);
			controllers->emplace_back(std::make_shared<DirectInputController>(lpddi->guidInstance, display_name));

			return DIENUM_CONTINUE;
		}, &result, DIEDFL_ALLDEVICES);
	

	return result;
}

LPCDIDATAFORMAT DirectInputControllerProvider::get_data_format() const
{
	return GetdfDIJoystick();
}
