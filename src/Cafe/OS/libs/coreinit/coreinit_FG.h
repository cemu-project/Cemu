#pragma once

namespace coreinit
{
	void __OSClearCopyData();
	uint32 __OSGetCopyDataSize();
	uint8* __OSGetCopyDataPtr();
	bool __OSAppendCopyData(uint8* data, sint32 length);
	bool __OSResizeCopyData(sint32 length);

	bool OSGetForegroundBucket(MEMPTR<void>* offset, uint32be* size);
	bool OSGetForegroundBucketFreeArea(MPTR* offset, MPTR* size);

	void InitForegroundBucket();

	void FG_Save(MemStreamWriter& s);
	void FG_Restore(MemStreamReader& s);

	void InitializeFG();
}