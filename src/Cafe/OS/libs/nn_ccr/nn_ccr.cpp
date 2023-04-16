#include "Cafe/OS/common/OSCommon.h"
#include "nn_ccr.h"

namespace nn::ccr
{
	sint32 CCRSysCaffeineBootCheck()
	{
		cemuLog_logDebug(LogType::Force, "CCRSysCaffeineBootCheck()");
		return -1;
	}

	void Initialize()
	{
		cafeExportRegister("nn_ccr", CCRSysCaffeineBootCheck, LogType::Placeholder);
	}
}
