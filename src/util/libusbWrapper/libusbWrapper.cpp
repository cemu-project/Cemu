#include "libusbWrapper.h"

/*

#include "config/ActiveSettings.h"

libusbWrapper::libusbWrapper()
{
}

void libusbWrapper::init()
{
#if BOOST_OS_WINDOWS
    if (m_isInitialized)
		return;
	m_isInitialized = true;
	// load module
	m_module = LoadLibraryW(L"libusb-1.0.dll");
	if (!m_module)
	{
		const auto path = ActiveSettings::GetDataPath("resources/libusb-1.0.dll");
		m_module = LoadLibraryW(path.generic_wstring().c_str());
		if (!m_module)
		{
			cemuLog_log(LogType::Force, "libusbWrapper: can't load libusb-1.0.dll");
			return;
		}
	}

	// grab imports
#define FETCH_IMPORT(__NAME__) p_##__NAME__ = (decltype(&__NAME__))GetProcAddress(m_module, #__NAME__)
	FETCH_IMPORT(libusb_init);
	FETCH_IMPORT(libusb_exit);
	FETCH_IMPORT(libusb_interrupt_transfer);
	FETCH_IMPORT(libusb_get_device_list);
	FETCH_IMPORT(libusb_get_device_descriptor);
	FETCH_IMPORT(libusb_open);
	FETCH_IMPORT(libusb_close);
	FETCH_IMPORT(libusb_kernel_driver_active);
	FETCH_IMPORT(libusb_detach_kernel_driver);
	FETCH_IMPORT(libusb_claim_interface);
	FETCH_IMPORT(libusb_free_device_list);
	FETCH_IMPORT(libusb_get_config_descriptor);
	FETCH_IMPORT(libusb_hotplug_register_callback);
	FETCH_IMPORT(libusb_hotplug_deregister_callback);
	FETCH_IMPORT(libusb_has_capability);
	FETCH_IMPORT(libusb_error_name);
	FETCH_IMPORT(libusb_get_string_descriptor);
	FETCH_IMPORT(libusb_get_string_descriptor_ascii);
	FETCH_IMPORT(libusb_free_config_descriptor);
#undef FETCH_IMPORT

	// create default context
	if (p_libusb_init)
		p_libusb_init(nullptr);
#else
	cemuLog_log(LogType::Force, "libusbWrapper: Not supported on this OS");
#endif
}

libusbWrapper::~libusbWrapper()
{
#if BOOST_OS_WINDOWS
	// destroy default context
	if(p_libusb_exit)
		p_libusb_exit(nullptr);

	// unload module
	if(m_module)
		FreeLibrary(m_module);
#endif
}

*/