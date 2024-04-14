#include "Cafe/OS/common/OSCommon.h"
#include "drmapp.h"

namespace drmapp
{
	uint32 NupChkIsFinished(uint32 ukn)
	{
		cemuLog_logDebug(LogType::Force, "drmapp.NupChkIsFinished() - placeholder");
		return 1;
	}

	uint32 PatchChkIsFinished()
	{
		cemuLog_logDebug(LogType::Force, "drmapp.PatchChkIsFinished() - placeholder");
		return 1;
	}

	uint32 AocChkIsFinished()
	{
		cemuLog_logDebug(LogType::Force, "drmapp.AocChkIsFinished() - placeholder");
		return 1;
	}

	uint32 TicketChkIsFinished()
	{
		cemuLog_logDebug(LogType::Force, "drmapp.TicketChkIsFinished__3RplFv() - placeholder");
		return 1;
	}

	void Initialize()
	{
		cafeExportRegisterFunc(NupChkIsFinished, "drmapp", "NupChkIsFinished__3RplFv", LogType::Placeholder);
		cafeExportRegisterFunc(PatchChkIsFinished, "drmapp", "PatchChkIsFinished__3RplFv", LogType::Placeholder);
		cafeExportRegisterFunc(AocChkIsFinished, "drmapp", "AocChkIsFinished__3RplFv", LogType::Placeholder);
		cafeExportRegisterFunc(TicketChkIsFinished, "drmapp", "TicketChkIsFinished__3RplFv", LogType::Placeholder);
	}
} // namespace drmapp
