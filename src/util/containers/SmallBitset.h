
// optimized and compact version of std::bitset with no error checking in release mode
// uses a single uint32 to store the bitmask, thus allowing up to 32 bool values

template<size_t N>
class SmallBitset
{
public:
	SmallBitset() = default;
	static_assert(N <= 32);
	
	bool test(size_t index) const
	{
		cemu_assert_debug(index < N);
		return ((m_bits >> index) & 1) != 0;
	}

	void set(size_t index, bool val)
	{
		cemu_assert_debug(index < N);
		m_bits &= ~(1u << index);
		if (val)
			m_bits |= (1u << index);
	}

	void set(size_t index)
	{
		cemu_assert_debug(index < N);
		m_bits |= (1u << index);
	}

private:
	uint32 m_bits{};
};