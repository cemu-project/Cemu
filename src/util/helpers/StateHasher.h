#pragma once

// hashes two separate streams at once for better parallelism, combined into one final uint64 hash at the end
// inspired by xxHash and MurmurHash3
class DualStateHasher
{
public:
	FORCEINLINE DualStateHasher()
	{
		m_h0 = 0x9E3779B97F4A7C15;
		m_h1 = 0xC2B2AE3D27D4EB4F;
	};

	FORCEINLINE void MixIn(uint64 a, uint64 b)
	{
		uint64 tmp = m_h1;
		m_h1 = (m_h0 ^ a) * 0x85EBCA77C2B2AE63ULL;
		m_h0 = (tmp ^ b) * 0x165667B19E3779F9ULL;
	}

	FORCEINLINE void MixInSingle(uint64 a)
	{
		uint64 tmp = m_h1;
		m_h1 = (m_h0 ^ a) * 0x85EBCA77C2B2AE63ULL;
		m_h0 = tmp;
	}

	FORCEINLINE uint64 Finish()
	{
		uint64 combined = m_h0 ^ std::rotl(m_h1, 31);
		combined ^= combined >> 33;
		combined *= 0xff51afd7ed558ccdULL;
		combined ^= combined >> 33;
		combined *= 0xc4ceb9fe1a85ec53ULL;
		combined ^= combined >> 33;
		return combined;
	}

private:
	uint64 m_h0;
	uint64 m_h1;
};
