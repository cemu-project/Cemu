#include "Cafe/OS/common/OSCommon.h"
#include "drmapp.h"

namespace drmapp
{
	uint32 NupChkIsFinished(uint32 ukn)
	{
		forceLogDebug_printf("drmapp.NupChkIsFinished() - placeholder");
		return 1;
	}

	void Initialize()
	{
		cafeExportRegisterFunc(NupChkIsFinished, "drmapp", "NupChkIsFinished__3RplFv", LogType::Placeholder);
	}
}
