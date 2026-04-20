#include "nsyshid.h"
#include "Backend.h"
#include "BackendEmulated.h"
#if HAS_LIBUSB
#include "BackendLibusb.h"
#endif

namespace nsyshid::backend
{
	void AttachDefaultBackends()
	{
	#if HAS_LIBUSB
		// add libusb backend
		{
			auto backendLibusb = std::make_shared<backend::libusb::BackendLibusb>();
			if (backendLibusb->IsInitialisedOk())
			{
				AttachBackend(backendLibusb);
			}
		}
	#endif
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
