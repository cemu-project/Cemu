#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/nn_common.h"

namespace nsyskbd
{
	bool IsValidChannel(uint32 channel)
	{
		return channel >= 0 && channel < 4;
	}

	uint32 KBDGetChannelStatus(uint32 channel, uint32be* status)
	{
		static bool loggedError = false;
		if (loggedError == false)
		{
			cemuLog_logDebug(LogType::Force, "KBDGetChannelStatus() placeholder");
			loggedError = true;
		}
		*status = 1; // disconnected

		return 0;
	}

#pragma pack(push, 1)
	struct KeyState
	{
		uint8be channel;
		uint8be ukn1;
		uint8be _padding[2];
		uint32be ukn4;
		uint32be ukn8;
		uint16be uknC;
	};
#pragma pack(pop)
	static_assert(sizeof(KeyState) == 0xE); // actual size might be padded to 0x10?

	uint32 KBDGetKey(uint32 channel, KeyState* keyState)
	{
		// used by MSX VC
		if(!IsValidChannel(channel) || !keyState)
		{
			cemuLog_log(LogType::APIErrors, "KBDGetKey(): Invalid parameter");
			return 0;
		}
		keyState->channel = channel;
		keyState->ukn1 = 0;
		keyState->ukn4 = 0;
		keyState->ukn8 = 0;
		keyState->uknC = 0;
		return 0;
	}

	void nsyskbd_load()
	{
		cafeExportRegister("nsyskbd", KBDGetChannelStatus, LogType::Placeholder);
		cafeExportRegister("nsyskbd", KBDGetKey, LogType::Placeholder);
	}
}
