#pragma once

namespace coreinit
{
	enum class RplEntryReason
	{
		Loaded = 1,
		Unloaded = 2,
	};

	uint32 OSDynLoad_SetAllocator(MPTR allocFunc, MPTR freeFunc);
	void OSDynLoad_SetTLSAllocator(MPTR allocFunc, MPTR freeFunc);
	uint32 OSDynLoad_GetAllocator(betype<MPTR>* funcAlloc, betype<MPTR>* funcFree);
	void OSDynLoad_GetTLSAllocator(betype<MPTR>* funcAlloc, betype<MPTR>* funcFree);

	void* OSDynLoad_AllocatorAlloc(sint32 size, sint32 alignment);
	void OSDynLoad_AllocatorFree(void* mem);

	uint32 OSDynLoad_Acquire(const char* libName, uint32be* moduleHandleOut);
	void OSDynLoad_Release(uint32 moduleHandle);
	uint32 OSDynLoad_FindExport(uint32 moduleHandle, uint32 isData, const char* exportName, betype<MPTR>* addrOut);

	void InitializeDynLoad();
}