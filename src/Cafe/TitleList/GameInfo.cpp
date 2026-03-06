#include "GameInfo.h"
#include "config/ActiveSettings.h"

fs::path GameInfo2::GetSaveFolder()
{
	return ActiveSettings::GetMlcPath(fmt::format("usr/save/{:08x}/{:08x}", (GetBaseTitleId() >> 32), GetBaseTitleId() & 0xFFFFFFFF));
}