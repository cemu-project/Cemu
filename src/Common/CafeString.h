#pragma once
#include "betype.h"
#include "util/helpers/StringHelpers.h"

/* Helper classes to represent CafeOS strings in emulated memory */
template <size_t N>
class CafeString // fixed buffer size, null-terminated, PPC char
{
  public:
	constexpr static size_t Size()
	{
		return N;
	}

	// checks whether the string and a null terminator can fit into the buffer
	bool CanHoldString(std::string_view sv) const
	{
		return sv.size() < N;
	}

	bool assign(std::string_view sv)
	{
		if (sv.size() >= N)
		{
			memcpy(data, sv.data(), N-1);
			data[N-1] = '\0';
			return false;
		}
		memcpy(data, sv.data(), sv.size());
		data[sv.size()] = '\0';
		return true;
	}

	void Copy(CafeString<N>& other)
	{
		memcpy(data, other.data, N);
	}

	bool empty() const
	{
		return data[0] == '\0';
	}

	const char* c_str() const
	{
		return (const char*)data;
	}

	void ClearAllBytes()
	{
		memset(data, 0, N);
	}

	auto operator<=>(const CafeString<N>& other) const
	{
		for (size_t i = 0; i < N; i++)
		{
			if (data[i] != other.data[i])
				return data[i] <=> other.data[i];
			if (data[i] == '\0')
				return std::strong_ordering::equal;
		}
		return std::strong_ordering::equal;
	}

	bool operator==(const CafeString<N>& other) const
	{
		for (size_t i = 0; i < N; i++)
		{
			if (data[i] != other.data[i])
				return false;
			if (data[i] == '\0')
				return true;
		}
		return true;
	}

	uint8be data[N];
};

template <size_t N>
class CafeWideString // fixed buffer size, null-terminated, PPC wchar_t (16bit big-endian)
{
  public:
	bool assign(const uint16be* input)
	{
		size_t i = 0;
		while(input[i])
		{
			if(i >= N-1)
			{
				data[N-1] = 0;
				return false;
			}
			data[i] = input[i];
			i++;
		}
		data[i] = 0;
		return true;
	}

	bool assignFromUTF8(std::string_view sv)
	{
		std::vector<uint16be> beStr = StringHelpers::FromUtf8(sv);
		if(beStr.size() > N-1)
		{
			memcpy(data, beStr.data(), (N-1)*sizeof(uint16be));
			data[N-1] = 0;
			return false;
		}
		memcpy(data, beStr.data(), beStr.size()*sizeof(uint16be));
		data[beStr.size()] = '\0';
		return true;
	}

	uint16be data[N];
};

namespace CafeStringHelpers
{
	static uint32 Length(const uint16be* input, uint32 maxLength)
	{
		uint32 i = 0;
		while(input[i] && i < maxLength)
			i++;
		return i;
	}
};
