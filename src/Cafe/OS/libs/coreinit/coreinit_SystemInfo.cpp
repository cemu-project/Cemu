#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/coreinit/coreinit_SystemInfo.h"

namespace coreinit
{
	SysAllocator<OSSystemInfo> g_system_info;

	const OSSystemInfo& OSGetSystemInfo()
	{
		return *g_system_info.GetPtr();
	}

	void ci_SystemInfo_Save(MemStreamWriter& s)
	{
		s.writeData("ci_SysInfo_S", 15);

		s.writeBE(g_system_info.GetMPTR());
	}

	void ci_SystemInfo_Restore(MemStreamReader& s)
	{
		char section[16] = { '\0' };
		s.readData(section, 15);
		cemu_assert_debug(strcmp(section, "ci_SysInfo_S") == 0);

		g_system_info = (OSSystemInfo*)memory_getPointerFromVirtualOffset(s.readBE<MPTR>());
	}

	void InitializeSystemInfo()
	{
		cemu_assert(ppcCyclesSince2000 != 0);
		g_system_info->busClock = ESPRESSO_BUS_CLOCK;
		g_system_info->coreClock = ESPRESSO_CORE_CLOCK;
		g_system_info->ticksSince2000 = ESPRESSO_CORE_CLOCK_TO_TIMER_CLOCK(ppcCyclesSince2000);
		g_system_info->l2cacheSize[0] = 512*1024; // 512KB // 512KB
		g_system_info->l2cacheSize[1] = 2*1024*1924; // 2MB
		g_system_info->l2cacheSize[2] = 512*1024; // 512KB
		g_system_info->coreClockToBusClockRatio = 5;
		cafeExportRegister("coreinit", OSGetSystemInfo, LogType::Placeholder);
	}
}
