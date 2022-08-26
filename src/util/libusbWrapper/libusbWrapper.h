#pragma once

// todo - port to cmake build

/*

#include "util/helpers/ClassWrapper.h"

#pragma warning(disable:4200)
#include "libusb-1.0/libusb.h"
#pragma warning(default:4200)

class libusbWrapper : public SingletonRef<libusbWrapper>
{
public:
	libusbWrapper();
	~libusbWrapper();

	void init();
	bool isAvailable() const { return p_libusb_init != nullptr; };
	
	decltype(&libusb_init) p_libusb_init = nullptr;
	decltype(&libusb_exit) p_libusb_exit = nullptr;
	decltype(&libusb_interrupt_transfer) p_libusb_interrupt_transfer;
	decltype(&libusb_get_device_list) p_libusb_get_device_list;
	decltype(&libusb_get_device_descriptor) p_libusb_get_device_descriptor;
	decltype(&libusb_open) p_libusb_open;
	decltype(&libusb_kernel_driver_active) p_libusb_kernel_driver_active;
	decltype(&libusb_detach_kernel_driver) p_libusb_detach_kernel_driver;
	decltype(&libusb_claim_interface) p_libusb_claim_interface;
	decltype(&libusb_free_device_list) p_libusb_free_device_list;
	decltype(&libusb_get_config_descriptor) p_libusb_get_config_descriptor;
	decltype(&libusb_free_config_descriptor) p_libusb_free_config_descriptor;
	decltype(&libusb_close) p_libusb_close;
	decltype(&libusb_hotplug_register_callback) p_libusb_hotplug_register_callback;
	decltype(&libusb_hotplug_deregister_callback) p_libusb_hotplug_deregister_callback;
	decltype(&libusb_has_capability) p_libusb_has_capability;
	decltype(&libusb_error_name) p_libusb_error_name;
	decltype(&libusb_get_string_descriptor) p_libusb_get_string_descriptor;
	decltype(&libusb_get_string_descriptor_ascii) p_libusb_get_string_descriptor_ascii;
	
	
private:
#if BOOST_OS_WINDOWS
	HMODULE m_module = nullptr;
	bool m_isInitialized = false;
#endif
};

*/