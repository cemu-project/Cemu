#pragma once

namespace coreinit
{
	void InitializeMemory();

	void OSGetMemBound(sint32 memType, MPTR* offsetOutput, uint32* sizeOutput);

	void* OSBlockMove(MEMPTR<void> dst, MEMPTR<void> src, uint32 size, bool flushDC);
	void* OSBlockSet(MEMPTR<void> dst, uint32 value, uint32 size);

	void OSMemoryBarrier();
}