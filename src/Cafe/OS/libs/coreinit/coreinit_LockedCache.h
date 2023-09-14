#pragma once

namespace coreinit
{
	void ci_LockedCache_Save(MemStreamWriter& s);
	void ci_LockedCache_Restore(MemStreamReader& s);

	void InitializeLC();
}