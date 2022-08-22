#pragma once
#include "Cafe/OS/libs/coreinit/coreinit.h"

namespace coreinit
{
	struct OSSystemInfo
	{
		uint32be busClock;
		uint32be coreClock;
		uint64be ticksSince2000;
		uint32be l2cacheSize[PPC_CORE_COUNT];
		uint32be coreClockToBusClockRatio;
	};

	static_assert(sizeof(OSSystemInfo) == 0x20);

	const OSSystemInfo& OSGetSystemInfo();
		
	void InitializeSystemInfo();
};

