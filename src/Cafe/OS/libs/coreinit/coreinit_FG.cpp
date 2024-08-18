#include "Cafe/OS/common/OSCommon.h"
#include <memory>

#define FG_BUCKET_AREA_FREE					0	// free area available game
#define FG_BUCKET_AREA_AUDIO_TRANSITION		1	// transition audio buffer
#define FG_BUCKET_AREA2						2	// frame storage?	TV?
#define FG_BUCKET_AREA3						3	// frame storage?	DRC?
#define FG_BUCKET_AREA4						4	// frame storage?	TV?
#define FG_BUCKET_AREA5						5	// frame storage?	DRC?
#define FG_BUCKET_AREA_SAVE					6
#define FG_BUCKET_AREA_COPY					7	// for OS copy data (clipboard and title switch parameters)

#define FG_BUCKET_AREA_FREE_SIZE			0x2800000
#define FG_BUCKET_AREA_SAVE_SIZE			0x1000
#define FG_BUCKET_AREA_COPY_SIZE			0x400000

#define FG_BUCKET_AREA_COUNT	8

namespace coreinit
{
	MEMPTR<void> fgAddr = nullptr; // NULL if not in foreground
	MEMPTR<uint8> fgSaveAreaAddr = nullptr;

	struct
	{
		uint32 id;
		uint32 startOffset;
		uint32 size;
	}fgAreaEntries[FG_BUCKET_AREA_COUNT] =
	{
		{ 0, 0, 0x2800000 },
		{ 7, 0x2800000, 0x400000 },
		{ 1, 0x2C00000, 0x900000 },
		{ 2, 0x3500000, 0x3C0000 },
		{ 3, 0x38C0000, 0x1C0000 },
		{ 4, 0x3A80000, 0x3C0000 },
		{ 5, 0x3E40000, 0x1BF000 },
		{ 6, 0x3FFF000, 0x1000 }
	};

	MEMPTR<uint8> GetFGMemByArea(uint32 areaId)
	{
		if (fgAddr == nullptr)
			return nullptr;
		for (sint32 i = 0; i < FG_BUCKET_AREA_COUNT; i++)
		{
			if (fgAreaEntries[i].id == areaId)
				return MEMPTR<uint8>(fgAddr.GetPtr<uint8>() + fgAreaEntries[i].startOffset);
		}
		return nullptr;
	}


	bool OSGetForegroundBucket(MEMPTR<void>* offset, uint32be* size)
	{
		// return full size of foreground bucket area
		if (offset)
			*offset = { (MPTR)MEMORY_FGBUCKET_AREA_ADDR };
		if (size)
			*size = MEMORY_FGBUCKET_AREA_SIZE;
		// return true if in foreground
		return true;
	}

	bool OSGetForegroundBucketFreeArea(MEMPTR<void>* offset, uint32be* size)
	{
		uint8* freeAreaAddr = GetFGMemByArea(FG_BUCKET_AREA_FREE).GetPtr();
		*offset = freeAreaAddr;
		*size = FG_BUCKET_AREA_FREE_SIZE;
		// return true if in foreground
		return (fgAddr != nullptr);
	}

	void coreinitExport_OSGetForegroundBucket(PPCInterpreter_t* hCPU)
	{
		//debug_printf("OSGetForegroundBucket(0x%x,0x%x)\n", hCPU->gpr[3], hCPU->gpr[4]);
		// returns the whole FG bucket area (if the current process is in the foreground)
		ppcDefineParamMPTR(areaOutput, 0);
		ppcDefineParamMPTR(areaSize, 1);
		bool r = OSGetForegroundBucket((MEMPTR<void>*)memory_getPointerFromVirtualOffsetAllowNull(areaOutput), (uint32be*)memory_getPointerFromVirtualOffsetAllowNull(areaSize));
		osLib_returnFromFunction(hCPU, r ? 1 : 0);
	}

	void InitForegroundBucket()
	{
		uint32be fgSize;
		OSGetForegroundBucket(&fgAddr, &fgSize);
		uint8* freeAreaPtr = GetFGMemByArea(FG_BUCKET_AREA_FREE).GetPtr();
		memset(freeAreaPtr, 0, FG_BUCKET_AREA_FREE_SIZE);
		uint8* saveAreaPtr = GetFGMemByArea(FG_BUCKET_AREA_SAVE).GetPtr();
		fgSaveAreaAddr = saveAreaPtr;
		if (*(uint32be*)saveAreaPtr != (uint32be)'Save')
		{
			// clear save area
			memset(saveAreaPtr, 0, FG_BUCKET_AREA_SAVE_SIZE);
			// clear copy area
			memset(GetFGMemByArea(FG_BUCKET_AREA_COPY).GetPtr(), 0, FG_BUCKET_AREA_COPY_SIZE);
			// init save area
			*(uint32be*)(saveAreaPtr + 0x00) = 'Save';
			*(uint32be*)(saveAreaPtr + 0x08) |= 0x300;
		}
	}

	void __OSClearCopyData()
	{
		uint8* fgCopyArea = GetFGMemByArea(FG_BUCKET_AREA_COPY).GetPtr();
		if (fgCopyArea)
		{
			*(uint32be*)(fgCopyArea + 0x00) = 0;
		}
	}

	uint32 __OSGetCopyDataSize()
	{
		uint8* fgCopyArea = GetFGMemByArea(FG_BUCKET_AREA_COPY).GetPtr();
		if (fgCopyArea)
		{
			return *(uint32be*)(fgCopyArea + 0x00);
		}
		return 0;
	}

	uint8* __OSGetCopyDataPtr()
	{
		uint8* fgCopyArea = GetFGMemByArea(FG_BUCKET_AREA_COPY).GetPtr();
		if (fgCopyArea)
			return fgCopyArea + 4;
		return nullptr;
	}

	bool __OSAppendCopyData(uint8* data, sint32 length)
	{
		uint8* fgCopyArea = GetFGMemByArea(FG_BUCKET_AREA_COPY).GetPtr();
		if (fgCopyArea == nullptr)
			return false;
		uint32 currentOffset = *(uint32be*)(fgCopyArea + 0x00);
		if ((currentOffset + length) > FG_BUCKET_AREA_COPY_SIZE - 4)
		{
			return false;
		}
		memcpy(fgCopyArea + currentOffset + 4, data, length);
		*(uint32be*)(fgCopyArea + 0x00) += length;
		return true;
	}

	bool __OSResizeCopyData(sint32 length)
	{
		uint8* fgCopyArea = GetFGMemByArea(FG_BUCKET_AREA_COPY).GetPtr();
		if (fgCopyArea == nullptr)
			return false;
		sint32 currentOffset = (sint32) * (uint32be*)(fgCopyArea + 0x00);
		if (length < currentOffset)
		{
			// can only make copy data smaller
			*(uint32be*)(fgCopyArea + 0x00) = length;
			return true;
		}
		return false;
	}

	bool OSCopyFromClipboard(void* data, uint32be* size)
	{
		uint32 cSize = *size;
		if (cSize == 0 && data == nullptr)
			return false;
		if (OSGetForegroundBucket(nullptr, nullptr) == false)
			return false;

		// todo

		*size = 0;
		return true;
	}

	void coreinitExport_OSCopyFromClipboard(PPCInterpreter_t* hCPU)
	{
		cemuLog_logDebug(LogType::Force, "OSCopyFromClipboard(0x{:x},0x{:x})", hCPU->gpr[3], hCPU->gpr[4]);
		ppcDefineParamMEMPTR(buffer, void, 0);
		ppcDefineParamMEMPTR(size, uint32be, 1);
		bool r = OSCopyFromClipboard(buffer.GetPtr(), size.GetPtr());
		osLib_returnFromFunction(hCPU, r ? 1 : 0);
	}

	void InitializeFG()
	{
		osLib_addFunction("coreinit", "OSGetForegroundBucket", coreinitExport_OSGetForegroundBucket);
		cafeExportRegister("coreinit", OSGetForegroundBucket, LogType::CoreinitMem);
		cafeExportRegister("coreinit", OSGetForegroundBucketFreeArea, LogType::CoreinitMem);
		osLib_addFunction("coreinit", "OSCopyFromClipboard", coreinitExport_OSCopyFromClipboard);
	}
}
