#pragma once
#include "boost/nowide/convert.hpp"
#include <charconv>

// Definition for removed templates in Apple Clang 17
#if defined(__apple_build_version__) && (__apple_build_version__ >= 17000000)
namespace std {
	template<>
	struct char_traits<uint16be> {
		using char_type = uint16be;
		using int_type = int;
		using off_type = streamoff;
		using pos_type = streampos;
		using state_type = mbstate_t;

		static inline void constexpr assign(char_type& c1, const char_type& c2) noexcept {
			c1 = c2;
		}

		static inline constexpr bool eq(char_type c1, char_type c2) noexcept {
			return c1 == c2;
		}

		static inline constexpr bool lt(char_type c1, char_type c2) noexcept {
			return c1 < c2;
		}

		static constexpr int compare(const char_type* s1, const char_type* s2, size_t n) {
			for (; n; --n, ++s1, ++s2) {
				if (lt(*s1, *s2)) return -1;
				if (lt(*s2, *s1)) return 1;
			}
			return 0;
		}

		static constexpr size_t length(const char_type* s) {
			size_t len = 0;
			for (; !eq(*s, char_type(0)); ++s) ++len;
			return len;
		}

		static constexpr const char_type* find(const char_type* s, size_t n, const char_type& a) {
			for (; n; --n) {
				if (eq(*s, a))
					return s;
				++s;
			}
			return nullptr;
		}

		static constexpr char_type* move(char_type* s1, const char_type* s2, size_t n) {
			if (n == 0) return s1;
			return static_cast<char_type*>(memmove(s1, s2, n * sizeof(char_type)));
		}

		static constexpr char_type* copy(char_type* s1, const char_type* s2, size_t n) {
			if (n == 0) return s1;
			return static_cast<char_type*>(memcpy(s1, s2, n * sizeof(char_type)));
		}

		static constexpr char_type* assign(char_type* s, size_t n, char_type a) {
			char_type* r = s;
			for (; n; --n, ++s)
				assign(*s, a);
			return r;
		}

		static inline constexpr char_type to_char_type(int_type c) noexcept {
			return char_type(c);
		}

		static inline constexpr int_type to_int_type(char_type c) noexcept {
			return int_type(c);
		}

		static inline constexpr bool eq_int_type(int_type c1, int_type c2) noexcept {
			return c1 == c2;
		}

		static inline constexpr int_type eof() noexcept {
			return static_cast<int_type>(EOF);
		}

		static inline constexpr int_type not_eof(int_type c) noexcept {
			return eq_int_type(c, eof()) ? ~eof() : c;
		}
	};
}
#endif

// todo - move the Cafe/PPC specific parts to CafeString.h eventually
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
	static std::vector<uint16be> FromUtf8(std::string_view str)
	{
		std::vector<uint16be> tmpStr;
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

	class StringLineIterator
	{
	public:
		class Iterator
		{
		public:
			using iterator_category = std::input_iterator_tag;
			using value_type = std::string_view;
			using difference_type = std::ptrdiff_t;
			using pointer = const std::string_view*;
			using reference = const std::string_view&;

			Iterator(std::string_view str, sint32 pos) : m_str(str), m_pos(pos)
			{
				update_line();
			}

			reference operator*() const
			{
				return m_line;
			}

			pointer operator->() const
			{
				return &m_line;
			}

			Iterator& operator++()
			{
				m_pos = m_nextPos;
				update_line();
				return *this;
			}

			friend bool operator==(const Iterator& lhs, const Iterator& rhs)
			{
				return lhs.m_str.data() == rhs.m_str.data() && lhs.m_pos == rhs.m_pos;
			}

			friend bool operator!=(const Iterator& lhs, const Iterator& rhs)
			{
				return !(lhs == rhs);
			}

		private:
			void update_line()
			{
				if (m_pos >= m_str.size())
				{
					m_pos = -1;
					m_line = {};
					return;
				}
				auto pos = m_str.find('\n', m_pos);
				m_nextPos = pos != std::string_view::npos ? pos : -1;
				if(m_nextPos < 0)
					m_line = m_str.substr(m_pos, std::string::npos);
				else
				{
					m_line = m_str.substr(m_pos, m_nextPos - m_pos);
					++m_nextPos; // skip \n
				}
				while (!m_line.empty() && m_line.back() == '\r')
					m_line.remove_suffix(1);
			}

			std::string_view m_str;
			sint32 m_pos;
			sint32 m_nextPos;
			std::string_view m_line;
		};

		StringLineIterator(std::string_view str) : m_str(str) {}
		StringLineIterator(std::span<uint8> str) : m_str((const char*)str.data(), str.size()) {}

		Iterator begin() const
		{
			return Iterator{m_str, 0 };
		}

		Iterator end() const
		{
			return Iterator{m_str, -1 };
		}

	private:
		std::string_view m_str;
	};
};

