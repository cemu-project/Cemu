#pragma once

namespace coreinit
{
	void LockedCache_Save(MemStreamWriter& s);
	void LockedCache_Restore(MemStreamReader& s);

	void InitializeLC();
}