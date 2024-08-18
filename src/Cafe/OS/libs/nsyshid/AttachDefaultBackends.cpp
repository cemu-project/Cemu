#include "nsyshid.h"
#include "Backend.h"
#include "BackendEmulated.h"

#if NSYSHID_ENABLE_BACKEND_LIBUSB

#include "BackendLibusb.h"

#endif

#if NSYSHID_ENABLE_BACKEND_WINDOWS_HID

#include "BackendWindowsHID.h"

#endif

namespace nsyshid::backend
{
	void AttachDefaultBackends()
	{
#if NSYSHID_ENABLE_BACKEND_LIBUSB
		// add libusb backend
		{
			auto backendLibusb = std::make_shared<backend::libusb::BackendLibusb>();
			if (backendLibusb->IsInitialisedOk())
			{
				AttachBackend(backendLibusb);
			}
		}
#endif // NSYSHID_ENABLE_BACKEND_LIBUSB
#if NSYSHID_ENABLE_BACKEND_WINDOWS_HID
		// add windows hid backend
		{
			auto backendWindowsHID = std::make_shared<backend::windows::BackendWindowsHID>();
			if (backendWindowsHID->IsInitialisedOk())
			{
				AttachBackend(backendWindowsHID);
			}
		}
#endif // NSYSHID_ENABLE_BACKEND_WINDOWS_HID
	   // add emulated backend
		{
			auto backendEmulated = std::make_shared<backend::emulated::BackendEmulated>();
			if (backendEmulated->IsInitialisedOk())
			{
				AttachBackend(backendEmulated);
			}
		}
	}
} // namespace nsyshid::backend
