#include "nsyshid.h"
#include "Backend.h"

#if NSYSHID_ENABLE_BACKEND_LIBUSB

#include "BackendLibusb.h"

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
	}
} // namespace nsyshid::backend
