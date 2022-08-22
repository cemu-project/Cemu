#include "Cafe/OS/common/OSCommon.h"
#include "coreinit_BSP.h"

namespace coreinit
{
	bool bspGetHardwareVersion(uint32be* version)
	{
		uint8 highVersion = 0x11; // anything below 0x11 will be considered as Hollywood
		// todo: Check version returned on console
		uint32 tempVers = highVersion << 24;
		*version = tempVers;
		return true;
	}

	void InitializeBSP()
	{
		cafeExportRegister("coreinit", bspGetHardwareVersion, LogType::Placeholder);
	}
}