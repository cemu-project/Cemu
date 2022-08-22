#include "input/api/DirectInput/DirectInputControllerProvider.h"

#include "input/api/DirectInput/DirectInputController.h"

DirectInputControllerProvider::DirectInputControllerProvider()
{
	/*m_module = LoadLibraryA("dinput8.dll");
	if (!m_module)
		throw std::runtime_error("can't load any xinput dll");

	m_DirectInput8Create = (decltype(&DirectInput8Create))GetProcAddress(m_module, "DirectInput8Create");
	m_GetdfDIJoystick = (decltype(&GetdfDIJoystick))GetProcAddress(m_module, "GetdfDIJoystick");
	
	if (!m_DirectInput8Create)
	{
		FreeLibrary(m_module);
		throw std::runtime_error("can't find the DirectInput8Create export");
	}*/
		

	const auto r = DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&m_dinput8, nullptr);
	if (FAILED(r) || !m_dinput8)
	{
		const auto error = GetLastError();
		//FreeLibrary(m_module);
		throw std::runtime_error(fmt::format("can't create direct input object (error: {:#x})", error));
	}
}

DirectInputControllerProvider::~DirectInputControllerProvider()
{
	if (m_dinput8)
		m_dinput8->Release();
	
	/*if (m_module)
		FreeLibrary(m_module);
	*/
}

std::vector<std::shared_ptr<ControllerBase>> DirectInputControllerProvider::get_controllers()
{
	std::vector<std::shared_ptr<ControllerBase>> result;

	m_dinput8->EnumDevices(DI8DEVCLASS_GAMECTRL,
		[](LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef) -> BOOL
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
	/*if (m_GetdfDIJoystick)
		return m_GetdfDIJoystick();*/

	return GetdfDIJoystick();
}
