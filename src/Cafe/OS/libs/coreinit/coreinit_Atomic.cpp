#include "Cafe/OS/common/OSCommon.h"
#include <atomic>
#include "coreinit_Atomic.h"

namespace coreinit
{
	/* 32bit atomic operations */

	uint32 OSSwapAtomic(uint32be* mem, uint32 newValue)
	{
		const auto ref = rawPtrToAtomicRef(mem);
		uint32be _newValue = newValue;
		uint32be previousValue = ref.exchange(_newValue);
		return previousValue;
	}

	bool OSCompareAndSwapAtomic(uint32be* mem, uint32 compareValue, uint32 swapValue)
	{
		// seen in GTA3 homebrew port
		const auto ref = rawPtrToAtomicRef(mem);
		uint32be _compareValue = compareValue;
		uint32be _swapValue = swapValue;
		return ref.compare_exchange_strong(_compareValue, _swapValue);
	}

	bool OSCompareAndSwapAtomicEx(uint32be* mem, uint32 compareValue, uint32 swapValue, uint32be* previousValue)
	{
		// seen in GTA3 homebrew port
		const auto ref = rawPtrToAtomicRef(mem);
		uint32be _compareValue = compareValue;
		uint32be _swapValue = swapValue;
		bool r = ref.compare_exchange_strong(_compareValue, _swapValue);
		*previousValue = _compareValue;
		return r;
	}

	uint32 OSAddAtomic(uint32be* mem, uint32 adder)
	{
        // used by SDL Wii U port
		const auto ref = rawPtrToAtomicRef(mem);
		uint32be knownValue;
		while (true)
		{
			knownValue = ref.load();
			uint32be newValue = knownValue + adder;
			if (ref.compare_exchange_strong(knownValue, newValue))
				break;
		}
		return knownValue;
	}

	/* 64bit atomic operations */

	uint64 OSSwapAtomic64(uint64be* mem, uint64 newValue)
	{
		const auto ref = rawPtrToAtomicRef(mem);
		uint64be _newValue = newValue;
		uint64be previousValue = ref.exchange(_newValue);
		return previousValue;
	}

	uint64 OSSetAtomic64(uint64be* mem, uint64 newValue)
	{
		return OSSwapAtomic64(mem, newValue);
	}

	uint64 OSGetAtomic64(uint64be* mem)
	{
		return rawPtrToAtomicRef(mem).load();
	}

	uint64 OSAddAtomic64(uint64be* mem, uint64 adder)
	{
		const auto ref = rawPtrToAtomicRef(mem);
		uint64be knownValue;
		while (true)
		{
			knownValue = ref.load();
			uint64be newValue = knownValue + adder;
			if (ref.compare_exchange_strong(knownValue, newValue))
				break;
		}
		return knownValue;
	}

	uint64 OSAndAtomic64(uint64be* mem, uint64 val)
	{
		const auto ref = rawPtrToAtomicRef(mem);
		uint64be knownValue;
		while (true)
		{
			knownValue = ref.load();
			uint64be newValue = knownValue & val;
			if (ref.compare_exchange_strong(knownValue, newValue))
				break;
		}
		return knownValue;
	}

	uint64 OSOrAtomic64(uint64be* mem, uint64 val)
	{
		const auto ref = rawPtrToAtomicRef(mem);
		uint64be knownValue;
		while (true)
		{
			knownValue = ref.load();
			uint64be newValue = knownValue | val;
			if (ref.compare_exchange_strong(knownValue, newValue))
				break;
		}
		return knownValue;
	}

	bool OSCompareAndSwapAtomic64(uint64be* mem, uint64 compareValue, uint64 swapValue)
	{
		const auto ref = rawPtrToAtomicRef(mem);
		uint64be _compareValue = compareValue;
		uint64be _swapValue = swapValue;
		return ref.compare_exchange_strong(_compareValue, _swapValue);
	}

	bool OSCompareAndSwapAtomicEx64(uint64be* mem, uint64 compareValue, uint64 swapValue, uint64be* previousValue)
	{
		const auto ref = rawPtrToAtomicRef(mem);
		uint64be _compareValue = compareValue;
		uint64be _swapValue = swapValue;
		bool r = ref.compare_exchange_strong(_compareValue, _swapValue);
		*previousValue = _compareValue;
		return r;
	}

	void InitializeAtomic()
	{
		// 32bit atomic operations
		cafeExportRegister("coreinit", OSSwapAtomic, LogType::Placeholder);
		cafeExportRegister("coreinit", OSCompareAndSwapAtomic, LogType::Placeholder);
		cafeExportRegister("coreinit", OSCompareAndSwapAtomicEx, LogType::Placeholder);
		cafeExportRegister("coreinit", OSAddAtomic, LogType::Placeholder);
		
		// 64bit atomic operations
		cafeExportRegister("coreinit", OSSetAtomic64, LogType::Placeholder);
		cafeExportRegister("coreinit", OSGetAtomic64, LogType::Placeholder);
		cafeExportRegister("coreinit", OSSwapAtomic64, LogType::Placeholder);
		cafeExportRegister("coreinit", OSAddAtomic64, LogType::Placeholder);
		cafeExportRegister("coreinit", OSAndAtomic64, LogType::Placeholder);
		cafeExportRegister("coreinit", OSOrAtomic64, LogType::Placeholder);
		cafeExportRegister("coreinit", OSCompareAndSwapAtomic64, LogType::Placeholder);
		cafeExportRegister("coreinit", OSCompareAndSwapAtomicEx64, LogType::Placeholder);
	}
}