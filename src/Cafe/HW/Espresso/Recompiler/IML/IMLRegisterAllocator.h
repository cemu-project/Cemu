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

	void SetAllAvailable()
	{
		m_regBitmask = ~0ull;
	}

	bool HasAllAvailable() const
	{
		return m_regBitmask == ~0ull;
	}

	bool IsAvailable(uint32 index) const
	{
		return (m_regBitmask & ((uint64)1 << index)) != 0;
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
	IMLPhysReg GetFirstAvailableReg()
	{
		cemu_assert_debug(m_regBitmask != 0);
		return (IMLPhysReg)std::countr_zero(m_regBitmask);
	}

	// returns index of next available register (search includes any register index >= startIndex)
	// returns -1 if there is no more register
	IMLPhysReg GetNextAvailableReg(sint32 startIndex) const
	{
		if (startIndex >= 64)
			return -1;
		uint64 tmp = m_regBitmask >> startIndex;
		if (!tmp)
			return -1;
		return startIndex + (IMLPhysReg)std::countr_zero(tmp);
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