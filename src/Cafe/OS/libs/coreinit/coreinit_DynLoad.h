#pragma once

namespace coreinit
{
	struct OSDynLoad_NotifyData
	{
		MEMPTR<char> name;

		uint32be textAddr;
		uint32be textOffset;
		uint32be textSize;

		uint32be dataAddr;
		uint32be dataOffset;
		uint32be dataSize;

		uint32be readAddr;
		uint32be readOffset;
		uint32be readSize;
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

	uint32 OSDynLoad_GetModuleName(uint32 moduleHandle, char* nameBuf, sint32be* nameBufSize);
	sint32 OSDynLoad_GetNumberOfRPLs();
	uint32 OSDynLoad_GetRPLInfo(uint32 first, uint32 count, OSDynLoad_NotifyData* outInfos);

	void InitializeDynLoad();
}