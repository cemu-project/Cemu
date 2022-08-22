#pragma once
#include "boost/nowide/convert.hpp"
#include <charconv>

namespace StringHelpers
{
	// convert Wii U big-endian wchar_t string to utf8 string
	static std::string ToUtf8(const uint16be* ptr, size_t maxLength)
	{
		std::wstringstream result;
		while (*ptr != 0 && maxLength > 0)
		{
			auto c = (uint16)*ptr;
			result << static_cast<wchar_t>(c);
			ptr++;
			maxLength--;
		}
		return boost::nowide::narrow(result.str());
	}

	static std::string ToUtf8(std::span<uint16be> input)
	{
		return ToUtf8(input.data(), input.size());
	}

	// convert utf8 string to Wii U big-endian wchar_t string
	static std::basic_string<uint16be> FromUtf8(std::string_view str)
	{
		std::basic_string<uint16be> tmpStr;
		std::wstring w = boost::nowide::widen(str.data(), str.size());
		for (auto& c : w)
			tmpStr.push_back((uint16)c);
		return tmpStr;
	}

	static sint32 ToInt(const std::string_view& input, sint32 defaultValue = 0)
	{
		sint32 value = defaultValue;
		if (input.size() >= 2 && (input[0] == '0' && (input[1] == 'x' || input[1] == 'X')))
		{
			// hex number
			const std::from_chars_result result = std::from_chars(input.data() + 2, input.data() + input.size(), value, 16);
			if (result.ec == std::errc::invalid_argument || result.ec == std::errc::result_out_of_range)
				return defaultValue;
		}
		else
		{
			// decimal value
			const std::from_chars_result result = std::from_chars(input.data(), input.data() + input.size(), value);
			if (result.ec == std::errc::invalid_argument || result.ec == std::errc::result_out_of_range)
				return defaultValue;
		}
		return value;
	}

	static sint64 ToInt64(const std::string_view& input, sint64 defaultValue = 0)
	{
		sint64 value = defaultValue;
		if (input.size() >= 2 && (input[0] == '0' && (input[1] == 'x' || input[1] == 'X')))
		{
			// hex number
			const std::from_chars_result result = std::from_chars(input.data() + 2, input.data() + input.size(), value, 16);
			if (result.ec == std::errc::invalid_argument || result.ec == std::errc::result_out_of_range)
				return defaultValue;
		}
		else
		{
			// decimal value
			const std::from_chars_result result = std::from_chars(input.data(), input.data() + input.size(), value);
			if (result.ec == std::errc::invalid_argument || result.ec == std::errc::result_out_of_range)
				return defaultValue;
		}
		return value;
	}

	static size_t ParseHexString(std::string_view input, uint8* output, size_t maxOutputLength)
	{
		size_t parsedLen = 0;
		for (size_t i = 0; i < input.size() - 1; i += 2)
		{
			if (maxOutputLength <= 0)
				break;
			uint8 b = 0;
			uint8 c = input[i + 0];
			// high nibble
			if (c >= '0' && c <= '9')
				b |= ((c - '0') << 4);
			else if (c >= 'a' && c <= 'f')
				b |= ((c - 'a' + 10) << 4);
			else if (c >= 'A' && c <= 'F')
				b |= ((c - 'A' + 10) << 4);
			else
				break;
			// low nibble
			c = input[i + 1];
			if (c >= '0' && c <= '9')
				b |= (c - '0');
			else if (c >= 'a' && c <= 'f')
				b |= (c - 'a' + 10);
			else if (c >= 'A' && c <= 'F')
				b |= (c - 'A' + 10);
			else
				break;
			*output = b;
			output++;
			maxOutputLength--;
			parsedLen++;
		}
		return parsedLen;
	}
};

