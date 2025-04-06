#include "nsyshid.h"
#include "Backend.h"
#include "BackendEmulated.h"
#include "BackendLibusb.h"

namespace nsyshid::backend
{
	void AttachDefaultBackends()
	{
		// add libusb backend
		{
			auto backendLibusb = std::make_shared<backend::libusb::BackendLibusb>();
			if (backendLibusb->IsInitialisedOk())
			{
				AttachBackend(backendLibusb);
			}
		}
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
