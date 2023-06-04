#pragma once

namespace coreinit
{
	struct FIFOEntry_t
	{
		MEMPTR<uint8> p;
	};

	struct IPCFifo_t
	{
		uint32be writeIndex;
		uint32be readIndex;
		uint32be availableEntries; // number of available entries
		uint32be entryCount;
		MEMPTR<FIFOEntry_t> entryArray;
	};

	struct IPCBufPool_t
	{
		/* +0x00 */ uint32be magic;
		/* +0x04 */ MEMPTR<void> fullBufferPtr;
		/* +0x08 */ uint32be fullBufferSize;
		/* +0x0C */ uint32be uknFromParamR7; // boolean?
		/* +0x10 */ uint32be ukn10;			 // set to zero on init
		/* +0x14 */ uint32be entrySize1;
		/* +0x18 */ uint32be entrySize2;	 // set to same value as entrySize1
		/* +0x1C */ uint32be entryCount;	 // actual number of used entries
		/* +0x20 */ MEMPTR<uint8> entryStartPtr;
		/* +0x24 */ uint32be entryCountMul4;
		/* +0x28 */ IPCFifo_t fifo;
		/* +0x3C */ coreinit::OSMutex mutex;
		/* +0x68 */ uint32 ukn68;
		// full size is 0x6C
	};

	static_assert(sizeof(IPCBufPool_t) == 0x6C);

	uint8* IPCBufPoolAllocate(IPCBufPool_t* ipcBufPool, uint32 size);
	IPCBufPool_t* IPCBufPoolCreate(uint8* bufferArea, uint32 bufferSize, uint32 entrySize, uint32be* entryCountOutput, uint32 uknR7);
	sint32 IPCBufPoolFree(IPCBufPool_t* ipcBufPool, uint8* entry);

	void InitializeIPCBuf();
} // namespace coreinit
