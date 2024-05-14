#pragma once
#include "betype.h"
#include "util/helpers/StringHelpers.h"

/* Helper classes to represent CafeOS strings in emulated memory */
template <size_t N>
class CafeString // fixed buffer size, null-terminated, PPC char
{
  public:
	bool assign(std::string_view sv)
	{
		if (sv.size()+1 >= N)
		{
			memcpy(data, sv.data(), sv.size()-1);
			data[sv.size()-1] = '\0';
			return false;
		}
		memcpy(data, sv.data(), sv.size());
		data[sv.size()] = '\0';
		return true;
	}

	const char* c_str()
	{
		return (const char*)data;
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
		std::basic_string<uint16be> beStr = StringHelpers::FromUtf8(sv);
		if(beStr.length() > N-1)
		{
			memcpy(data, beStr.data(), (N-1)*sizeof(uint16be));
			data[N-1] = 0;
			return false;
		}
		memcpy(data, beStr.data(), beStr.length()*sizeof(uint16be));
		data[beStr.length()] = '\0';
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
