#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/nn_common.h"

namespace nsyskbd
{
	uint32 KBDGetChannelStatus(uint32 channel, uint32be* status)
	{
		static bool loggedError = false;
		if (loggedError == false)
		{
			forceLogDebug_printf("KBDGetChannelStatus() placeholder");
			loggedError = true;
		}
		*status = 1; // disconnected

		return 0;
	}

	void nsyskbd_load()
	{
		cafeExportRegister("nsyskbd", KBDGetChannelStatus, LogType::Placeholder);
	}
}