#pragma once

// staged lookup table suited for cases where the lookup index range can be very large (e.g. memory addresses)
// performs 3 consecutive table lookups, where each table's width is defined by TBitsN
// empty subtables consume no memory beyond the initial two default tables for TBitsY and TBitsZ
template<int TBitsX, int TBitsY, int TBitsZ, typename T>
class LookupTableL3
{
	struct TableZ // z lookup
	{
		T arr[1 << TBitsZ]{};
	};

	struct TableY // y lookup
	{
		TableZ* arr[1 << TBitsY];
	};

	// by generating placeholder tables we can avoid conditionals in the lookup code since no null-pointer checking is necessary
	TableY* m_placeholderTableY{};
	TableZ* m_placeholderTableZ{};


public:
	LookupTableL3()
	{
		// init placeholder table Z
		m_placeholderTableZ = GenerateNewTableZ();
		// init placeholder table Y (all entries point to placeholder table Z)
		m_placeholderTableY = GenerateNewTableY();
		// init x table
		for (auto& itr : m_tableXArr)
			itr = m_placeholderTableY;
	}

	~LookupTableL3()
	{
		delete m_placeholderTableY;
		delete m_placeholderTableZ;
	}

	// lookup
	// only the bottom most N bits bits are used of the offset
	// N = TBitsX + TBitsY + TBitsZ
	// if no match is found a default-constructed object is returned
	T lookup(uint32 offset)
	{
		uint32 indexZ = offset & ((1u << TBitsZ) - 1);
		offset >>= TBitsZ;
		uint32 indexY = offset & ((1u << TBitsY) - 1);
		offset >>= TBitsY;
		uint32 indexX = offset & ((1u << TBitsX) - 1);
		//offset >>= TBitsX;
		return m_tableXArr[indexX]->arr[indexY]->arr[indexZ];
	}

	void store(uint32 offset, T& t)
	{
		uint32 indexZ = offset & ((1u << TBitsZ) - 1);
		offset >>= TBitsZ;
		uint32 indexY = offset & ((1u << TBitsY) - 1);
		offset >>= TBitsY;
		uint32 indexX = offset & ((1u << TBitsX) - 1);
		if (m_tableXArr[indexX] == m_placeholderTableY)
			m_tableXArr[indexX] = GenerateNewTableY();
		TableY* lookupY = m_tableXArr[indexX];
		if (lookupY->arr[indexY] == m_placeholderTableZ)
			lookupY->arr[indexY] = GenerateNewTableZ();
		TableZ* lookupZ = lookupY->arr[indexY];
		lookupZ->arr[indexZ] = t;
	}

private:
	// generate a new Y lookup table which will initially contain only pointers to m_placeholderTableZ
	TableY* GenerateNewTableY()
	{
		TableY* tableY = new TableY();
		for (auto& itr : tableY->arr)
			itr = m_placeholderTableZ;
		return tableY;
	}

	// generate a new Z lookup table which will initially contain only default constructed T
	TableZ* GenerateNewTableZ()
	{
		TableZ* tableZ = new TableZ();
		return tableZ;
	}

	TableY* m_tableXArr[1 << TBitsX]; // x lookup
};
