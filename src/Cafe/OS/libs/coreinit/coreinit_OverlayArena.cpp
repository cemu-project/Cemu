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

	void OverlayArena_Save(MemStreamWriter& s)
	{
		s.writeSection("coreinit_OverlayArena");
		s.writeBool(g_coreinitOverlayArena.isEnabled);
	}
	void OverlayArena_Restore(MemStreamReader& s)
	{
		s.readSection("coreinit_OverlayArena");
		s.readBool(g_coreinitOverlayArena.isEnabled);
	}

	void InitializeOverlayArena()
	{
		cafeExportRegister("coreinit", OSIsEnabledOverlayArena, LogType::Placeholder);
		cafeExportRegister("coreinit", OSEnableOverlayArena, LogType::Placeholder);
		g_coreinitOverlayArena.isEnabled = false;
	}
}