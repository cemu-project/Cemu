#pragma once
#include <atomic>

namespace coreinit
{
	uint32 OSSwapAtomic(uint32be* mem, uint32 newValue);
	bool OSCompareAndSwapAtomic(uint32be* mem, uint32 compareValue, uint32 swapValue);
	bool OSCompareAndSwapAtomicEx(uint32be* mem, uint32 compareValue, uint32 swapValue, uint32be* previousValue);
	uint32 OSAddAtomic(uint32be* mem, uint32 adder);
	
	uint64 OSSwapAtomic64(uint64be* mem, uint64 newValue);
	uint64 OSSetAtomic64(uint64be* mem, uint64 newValue);
	uint64 OSGetAtomic64(uint64be* mem);
	uint64 OSAddAtomic64(uint64be* mem, uint64 adder);
	uint64 OSAndAtomic64(uint64be* mem, uint64 val);
	uint64 OSOrAtomic64(uint64be* mem, uint64 val);
	bool OSCompareAndSwapAtomic64(uint64be* mem, uint64 compareValue, uint64 swapValue);
	bool OSCompareAndSwapAtomicEx64(uint64be* mem, uint64 compareValue, uint64 swapValue, uint64be* previousValue);

	void InitializeAtomic();
}