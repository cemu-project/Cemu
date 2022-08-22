#pragma once

class StringTokenParser
{
public:
	StringTokenParser() : m_str(nullptr), m_len(0) {};

	StringTokenParser(const char* input, sint32 inputLen) : m_str(input), m_len(inputLen) {};
	StringTokenParser(std::string_view str) : m_str(str.data()), m_len((sint32)str.size()) {};

	// skip whitespaces at current ptr position
	void skipWhitespaces()
	{
		m_str = _skipWhitespaces(m_str, m_len);
	}

	// decrease string length as long as there is a whitespace at the end
	void trimWhitespaces()
	{
		while (m_len > 0)
		{
			const char c = m_str[m_len - 1];
			if (c != ' ' && c != '\t')
				break;
			m_len--;
		}
	}

	bool isEndOfString()
	{
		return m_len <= 0;
	}

	sint32 skipToCharacter(const char c)
	{
		auto str = m_str;
		auto len = m_len;
		sint32 idx = 0;
		while (len > 0)
		{
			if (*str == c)
			{
				m_str = str;
				m_len = len;
				return idx;
			}
			len--;
			str++;
			idx++;
		}
		return -1;
	}

	bool matchWordI(const char* word)
	{
		auto str = m_str;
		auto length = m_len;
		str = _skipWhitespaces(str, length);
		for (sint32 i = 0; i <= length; i++)
		{
			if (word[i] == '\0')
			{
				m_str = str + i;
				m_len = length - i;
				return true;
			}
			if (i == length)
				return false;

			char c1 = str[i];
			char c2 = word[i];
			c1 = _toUpperCase(c1);
			c2 = _toUpperCase(c2);
			if (c1 != c2)
				return false;
		}
		return false;
	}

	bool compareCharacter(sint32 relativeIndex, const char c)
	{
		if (relativeIndex >= m_len)
			return false;
		return m_str[relativeIndex] == c;
	}

	bool compareCharacterI(sint32 relativeIndex, const char c)
	{
		if (relativeIndex >= m_len)
			return false;
		return _toUpperCase(m_str[relativeIndex]) == _toUpperCase(c);
	}

	void skipCharacters(sint32 count)
	{
		if (count > m_len)
			count = m_len;
		m_str += count;
		m_len -= count;
	}

	bool parseU32(uint32& val)
	{
		auto str = m_str;
		auto length = m_len;
		str = _skipWhitespaces(str, length);
		uint32 value = 0;
		sint32 index = 0;
		bool isHex = false;
		if (length <= 0)
			return false;
		if (length >= 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
		{
			isHex = true;
			index += 2;
		}
		else if (str[index] == '0')
		{
			isHex = true;
			index++;
		}
		if (length <= index)
			return false;
		if (isHex)
		{
			sint32 firstDigitIndex = index;
			for (; index < length; index++)
			{
				const char c = str[index];
				if (c >= '0' && c <= '9')
				{
					value *= 0x10;
					value += (c - '0');
				}
				else if (c >= 'a' && c <= 'f')
				{
					value *= 0x10;
					value += (c - 'a' + 10);
				}
				else if (c >= 'A' && c <= 'F')
				{
					value *= 0x10;
					value += (c - 'A' + 10);
				}
				else
					break;
			}
			if (index == firstDigitIndex)
				return false;
			m_str = str + index;
			m_len = length - index;
		}
		else
		{
			sint32 firstDigitIndex = index;
			for (; index < length; index++)
			{
				const char c = str[index];
				if (c >= '0' && c <= '9')
				{
					value *= 10;
					value += (c - '0');
				}
				else
					break;
			}
			if (index == firstDigitIndex)
				return false;
			m_str = str + index;
			m_len = length - index;
		}
		val = value;
		return true;
	}

	bool parseSymbolName(const char*& symbolStr, sint32& symbolLength)
	{
		auto str = m_str;
		auto length = m_len;
		str = _skipWhitespaces(str, length);
		// symbols must start with a letter or _
		if (length <= 0)
			return false;
		if (!(str[0] >= 'a' && str[0] <= 'z') &&
			!(str[0] >= 'A' && str[0] <= 'Z') &&
			!(str[0] == '_'))
			return false;
		sint32 idx = 1;
		while (idx < length)
		{
			const char c = str[idx];
			if (!(c >= 'a' && c <= 'z') &&
				!(c >= 'A' && c <= 'Z') &&
				!(c >= '0' && c <= '9') &&
				!(c == '_') && !(c == '.'))
				break;
			idx++;
		}
		symbolStr = str;
		symbolLength = idx;
		m_str = str + idx;
		m_len = length - idx;
		return true;
	}

	const char* getCurrentPtr()
	{
		return m_str;
	}

	sint32 getCurrentLen()
	{
		return m_len;
	}

	void storeParserState(StringTokenParser* bak)
	{
		bak->m_str = m_str;
		bak->m_len = m_len;
	}

	void restoreParserState(const StringTokenParser* bak)
	{
		m_str = bak->m_str;
		m_len = bak->m_len;
	}

	private:
		const char* _skipWhitespaces(const char* str, sint32& length)
		{
			while (length > 0)
			{
				if (*str != ' ' && *str != '\t')
					break;
				str++;
				length--;
			}
			return str;
		}

		char _toUpperCase(const char c)
		{
			if (c >= 'a' && c <= 'z') 
				return c + ('A' - 'a');
			return c;
		}

	private:
		const char* m_str;
		sint32 m_len;
};

