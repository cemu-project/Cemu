#pragma once

namespace coreinit
{
	void InitSysHeap();

	void* OSAllocFromSystem(uint32 size, uint32 alignment);
	void OSFreeToSystem(void* ptr);

	void SysHeap_Save(MemStreamWriter& s);
	void SysHeap_Restore(MemStreamReader& s);

	void InitializeSysHeap();
}