#pragma once
#include "util/helpers/fspinlock.h"

enum class DEBUG_SYMBOL_TYPE
{
	UNDEFINED,
	CODE,
	// big-endian types
	U64,
	U32,
	U16,
	U8,
	S64,
	S32,
	S16,
	S8,
	FLOAT,
	DOUBLE,
};


class DebugSymbolStorage
{
public:
	static void StoreDataType(MPTR address, DEBUG_SYMBOL_TYPE type)
	{
		s_lock.lock();
		s_typeStorage[address] = type;
		s_lock.unlock();
	}

	static DEBUG_SYMBOL_TYPE GetDataType(MPTR address)
	{
		s_lock.lock();
		auto itr = s_typeStorage.find(address);
		if (itr == s_typeStorage.end())
		{
			s_lock.unlock();
			return DEBUG_SYMBOL_TYPE::UNDEFINED;
		}
		DEBUG_SYMBOL_TYPE t = itr->second;
		s_lock.unlock();
		return t;
	}

	static void ClearRange(MPTR address, uint32 length)
	{
		if (length == 0)
			return;
		s_lock.lock();
		for (;;)
		{
			auto itr = s_typeStorage.find(address);
			if (itr != s_typeStorage.end())
				s_typeStorage.erase(itr);
			
			if (length <= 4)
				break;
			address += 4;
			length -= 4;
		}
		s_lock.unlock();
	}

private:
	static FSpinlock s_lock;
	static std::unordered_map<MPTR, DEBUG_SYMBOL_TYPE> s_typeStorage;
};
