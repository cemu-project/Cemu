#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/common/OSCommon.h"
#include "coreinit_OverlayArena.h"

namespace coreinit
{
	struct
	{
		bool isEnabled;
	}g_coreinitOverlayArena = { 0 };

	uint32 OSIsEnabledOverlayArena()
	{
		return g_coreinitOverlayArena.isEnabled ? 1 : 0;
	}

	void OSEnableOverlayArena(uint32 uknParam, uint32be* areaOffset, uint32be* areaSize)
	{
		if (g_coreinitOverlayArena.isEnabled == false)
		{
			memory_enableOverlayArena();
			g_coreinitOverlayArena.isEnabled = true;
		}
		*areaOffset = MEMORY_OVERLAY_AREA_OFFSET;
		*areaSize = MEMORY_OVERLAY_AREA_SIZE;
	}

	void ci_OverlayArena_Save(MemStreamWriter& s)
	{
		s.writeData("ci_OverArea_S", 15);

		s.writeData(&g_coreinitOverlayArena, sizeof(g_coreinitOverlayArena));
	}
	void ci_OverlayArena_Restore(MemStreamReader& s)
	{
		char section[16] = { '\0' };
		s.readData(section, 15);
		cemu_assert_debug(strcmp(section, "ci_OverArea_S") == 0);

		s.readData(&g_coreinitOverlayArena, sizeof(g_coreinitOverlayArena));
	}

	void InitializeOverlayArena()
	{
		cafeExportRegister("coreinit", OSIsEnabledOverlayArena, LogType::Placeholder);
		cafeExportRegister("coreinit", OSEnableOverlayArena, LogType::Placeholder);
		g_coreinitOverlayArena.isEnabled = false;
	}
}