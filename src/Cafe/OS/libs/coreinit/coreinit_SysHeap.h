#pragma once

namespace coreinit
{
	void InitSysHeap();

	void SysHeap_Save(MemStreamWriter& s);
	void SysHeap_Restore(MemStreamReader& s);

	void InitializeSysHeap();
}