#include "BackendEmulated.h"
#include "Infinity.h"
#include "Skylander.h"
#include "config/CemuConfig.h"

namespace nsyshid::backend::emulated
{
	BackendEmulated::BackendEmulated()
	{
		cemuLog_logDebug(LogType::Force, "nsyshid::BackendEmulated: emulated backend initialised");
	}

	BackendEmulated::~BackendEmulated() = default;

	bool BackendEmulated::IsInitialisedOk()
	{
		return true;
	}

	void BackendEmulated::AttachVisibleDevices()
	{
		if (GetConfig().emulated_usb_devices.emulate_skylander_portal && !FindDeviceById(0x1430, 0x0150))
		{
			cemuLog_logDebug(LogType::Force, "Attaching Emulated Portal");
			// Add Skylander Portal
			auto device = std::make_shared<SkylanderPortalDevice>();
			AttachDevice(device);
		}
		if (GetConfig().emulated_usb_devices.emulate_infinity_base && !FindDeviceById(0x0E6F, 0x0129))
		{
			cemuLog_logDebug(LogType::Force, "Attaching Emulated Base");
			// Add Infinity Base
			auto device = std::make_shared<InfinityBaseDevice>();
			AttachDevice(device);
		}
	}
} // namespace nsyshid::backend::emulated