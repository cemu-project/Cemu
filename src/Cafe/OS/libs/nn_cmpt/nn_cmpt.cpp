#include "Cafe/OS/common/OSCommon.h"
#include "nn_cmpt.h"

namespace nn::cmpt
{
	uint32 CMPTAcctGetPcConf(uint32be* pcConf)
	{
		cemuLog_logDebug(LogType::Force, "CMPTAcctGetPcConf() - todo");
		pcConf[0] = 0;
		pcConf[1] = 0;
		pcConf[2] = 0;
		return 0;
	}

	uint32 CMPTGetDataSize(uint32be* sizeOut)
	{
		*sizeOut = 0xC0000;
		return 0;
	}

	void Initialize()
	{
		cafeExportRegister("nn_cmpt", CMPTAcctGetPcConf, LogType::Placeholder);
		cafeExportRegister("nn_cmpt", CMPTGetDataSize, LogType::Placeholder);
	}
}
