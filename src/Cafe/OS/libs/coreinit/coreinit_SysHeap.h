#pragma once

namespace coreinit
{
	void InitSysHeap();

	void ci_SysHeap_Save(MemStreamWriter& s);
	void ci_SysHeap_Restore(MemStreamReader& s);

	void InitializeSysHeap();
}