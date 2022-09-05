#include "nn_ccr.h"
#include "Cafe/OS/common/OSCommon.h"

namespace nn::ccr
{
sint32 CCRSysCaffeineBootCheck()
{
	forceLogDebug_printf("CCRSysCaffeineBootCheck()");
	return -1;
}

void Initialize()
{
	cafeExportRegister("nn_ccr", CCRSysCaffeineBootCheck, LogType::Placeholder);
}
} // namespace nn::ccr
