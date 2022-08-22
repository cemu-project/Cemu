#include "Cafe/OS/common/OSCommon.h"
#include "avm.h"

namespace avm
{
	bool AVMIsHDCPAvailable()
	{
		return true;
	}

	bool AVMIsHDCPOn()
	{
		return true;
	}

	bool AVMGetAnalogContentsProtectionEnable(uint32be* isEnable)
	{
		*isEnable = 1;
		return false;
	}

	bool AVMIsAnalogContentsProtectionOn()
	{
		return true;
	}

	bool AVMSetAnalogContentsProtectionEnable(sint32 newState)
	{
		return true;  // returns 1 (true) if new state was applied successfully?
	}

	void Initialize()
	{
		cafeExportRegister("avm", AVMIsHDCPAvailable, LogType::Placeholder);
		cafeExportRegister("avm", AVMIsHDCPOn, LogType::Placeholder);
		cafeExportRegister("avm", AVMGetAnalogContentsProtectionEnable, LogType::Placeholder);
		cafeExportRegister("avm", AVMIsAnalogContentsProtectionOn, LogType::Placeholder);
		cafeExportRegister("avm", AVMSetAnalogContentsProtectionEnable, LogType::Placeholder);
	}
}
