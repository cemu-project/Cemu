#pragma once

namespace coreinit
{
	void __OSClearCopyData();
	uint32 __OSGetCopyDataSize();
	uint8* __OSGetCopyDataPtr();
	bool __OSAppendCopyData(uint8* data, sint32 length);
	bool __OSResizeCopyData(sint32 length);

	bool OSGetForegroundBucket(MEMPTR<void>* offset, uint32be* size);
	bool OSGetForegroundBucketFreeArea(MEMPTR<void>* offset, uint32be* size);

	void InitForegroundBucket();

	void InitializeFG();
}