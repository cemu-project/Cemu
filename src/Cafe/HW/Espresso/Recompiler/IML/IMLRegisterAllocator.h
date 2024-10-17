#pragma once

// container for storing a set of register indices
// specifically optimized towards storing typical range of physical register indices (expected to be below 64)
class IMLPhysRegisterSet
{
public:
	void SetAvailable(uint32 index)
	{
		cemu_assert_debug(index < 64);
		m_regBitmask |= ((uint64)1 << index);
	}

	void SetReserved(uint32 index)
	{
		cemu_assert_debug(index < 64);
		m_regBitmask &= ~((uint64)1 << index);
	}

	bool IsAvailable(uint32 index) const
	{
		return (m_regBitmask & (1 << index)) != 0;
	}

	IMLPhysRegisterSet& operator&=(const IMLPhysRegisterSet& other)
	{
		this->m_regBitmask &= other.m_regBitmask;
		return *this;
	}

	IMLPhysRegisterSet& operator=(const IMLPhysRegisterSet& other)
	{
		this->m_regBitmask = other.m_regBitmask;
		return *this;
	}

	void RemoveRegisters(const IMLPhysRegisterSet& other)
	{
		this->m_regBitmask &= ~other.m_regBitmask;
	}

	bool HasAnyAvailable() const
	{
		return m_regBitmask != 0;
	}

	bool HasExactlyOneAvailable() const
	{
		return m_regBitmask != 0 && (m_regBitmask & (m_regBitmask - 1)) == 0;
	}

	// returns index of first available register. Do not call when HasAnyAvailable() == false
	uint32 GetFirstAvailableReg()
	{
		cemu_assert_debug(m_regBitmask != 0);
		uint32 regIndex = 0;
		auto tmp = m_regBitmask;
		while ((tmp & 0xFF) == 0)
		{
			regIndex += 8;
			tmp >>= 8;
		}
		while ((tmp & 0x1) == 0)
		{
			regIndex++;
			tmp >>= 1;
		}
		return regIndex;
	}

	// returns index of next available register (search includes any register index >= startIndex)
	// returns -1 if there is no more register
	sint32 GetNextAvailableReg(sint32 startIndex) const
	{
		if (startIndex >= 64)
			return -1;
		uint32 regIndex = startIndex;
		auto tmp = m_regBitmask;
		tmp >>= regIndex;
		if (!tmp)
			return -1;
		while ((tmp & 0xFF) == 0)
		{
			regIndex += 8;
			tmp >>= 8;
		}
		while ((tmp & 0x1) == 0)
		{
			regIndex++;
			tmp >>= 1;
		}
		return regIndex;
	}

	sint32 CountAvailableRegs() const
	{
		return std::popcount(m_regBitmask);
	}

private:
	uint64 m_regBitmask{ 0 };
};


struct IMLRegisterAllocatorParameters
{
	inline IMLPhysRegisterSet& GetPhysRegPool(IMLRegFormat regFormat)
	{
		return perTypePhysPool[stdx::to_underlying(regFormat)];
	}

	IMLPhysRegisterSet perTypePhysPool[stdx::to_underlying(IMLRegFormat::TYPE_COUNT)];
	std::unordered_map<IMLRegID, IMLName> regIdToName;
};

void IMLRegisterAllocator_AllocateRegisters(ppcImlGenContext_t* ppcImlGenContext, IMLRegisterAllocatorParameters& raParam);