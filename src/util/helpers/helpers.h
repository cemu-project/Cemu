#pragma once

#include <charconv>
#include <filesystem>
#include <string_view>

#include "util/math/vector2.h"
#include "util/math/vector3.h"

#ifdef __clang__
#include "Common/unix/fast_float.h"
#endif

template <typename TType>
constexpr auto to_underlying(TType v) noexcept
{
	return static_cast<std::underlying_type_t<TType>>(v);
}

// wrapper to allow reverse iteration with range-based loops before C++20
template<typename T>
class reverse_itr {
private:
	T& iterable_;
public:
	explicit reverse_itr(T& iterable) : iterable_{ iterable } {}
	auto begin() const { return std::rbegin(iterable_); }
	auto end() const { return std::rend(iterable_); }
};

#ifndef M_PI
#define M_PI       3.14159265358979323846
#endif

template<typename T>
T deg2rad(T v) { return v * static_cast<T>(M_PI) / static_cast<T>(180); }
template<typename T>
T rad2deg(T v) { return v * static_cast<T>(180) / static_cast<T>(M_PI); }

template<typename T>
Vector3<T> deg2rad(const Vector3<T>& v) { return { deg2rad(v.x), deg2rad(v.y), deg2rad(v.z) }; }
template<typename T>
Vector3<T> rad2deg(const Vector3<T>& v) { return { rad2deg(v.x), rad2deg(v.y), rad2deg(v.z) }; }

template<typename T>
Vector2<T> deg2rad(const Vector2<T>& v) { return { deg2rad(v.x), deg2rad(v.y) }; }
template<typename T>
Vector2<T> rad2deg(const Vector2<T>& v) { return { rad2deg(v.x), rad2deg(v.y) }; }

uint32_t GetPhysicalCoreCount();

// Creates a temporary file to test for write access
bool TestWriteAccess(const fs::path& p);

fs::path MakeRelativePath(const fs::path& base, const fs::path& path);

#ifdef HAS_DIRECTINPUT
bool GUIDFromString(const char* string, GUID& guid);
std::string StringFromGUID(const GUID& guid);
std::wstring WStringFromGUID(const GUID& guid);
#endif

std::vector<std::string_view> TokenizeView(std::string_view string, char delimiter);
std::vector<std::string> Tokenize(std::string_view string, char delimiter);

std::string ltrim_copy(const std::string& s);
std::string rtrim_copy(const std::string& s);
std::string& ltrim(std::string& str, const std::string& chars = "\t\n\v\f\r ");
std::string& rtrim(std::string& str, const std::string& chars = "\t\n\v\f\r ");
std::string& trim(std::string& str, const std::string& chars = "\t\n\v\f\r ");

std::string_view& ltrim(std::string_view& str, const std::string& chars = "\t\n\v\f\r ");
std::string_view& rtrim(std::string_view& str, const std::string& chars = "\t\n\v\f\r ");
std::string_view& trim(std::string_view& str, const std::string& chars = "\t\n\v\f\r ");

std::string GenerateRandomString(size_t length);
std::string GenerateRandomString(size_t length, std::string_view characters);

std::wstring GetSystemErrorMessageW();
std::wstring GetSystemErrorMessageW(DWORD error_code);
std::string GetSystemErrorMessage();
std::string GetSystemErrorMessage(DWORD error_code);
std::string GetSystemErrorMessage(const std::exception& ex);
std::string GetSystemErrorMessage(const std::error_code& ec);

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...)->overloaded<Ts...>;

template<typename T>
bool equals(T v1, T v2)
{
	/*
	return std::fabs(x-y) <= std::numeric_limits<T>::epsilon() * std::fabs(x+y) * ulp
        // unless the result is subnormal
        || std::fabs(x-y) < std::numeric_limits<T>::min();
	 */
	if constexpr (std::is_floating_point_v<T>)
		return std::abs(v1 - v2) < (T)0.000001;
	else if constexpr (std::is_same_v<T, const char*>)
		return strcmp(v1, v2) == 0;
	else
		return v1 == v2;
}

template<typename T>
T ConvertString(std::string_view str, sint32 base)
{
	if (str.empty())
		return {};
	
	static_assert(std::is_integral_v<T>);
	T result;
	ltrim(str);

	// from_chars cant deal with hex numbers starting with "0x"
	if (base == 16)
	{
		const sint32 index = str[0] == '-' ? 1 : 0;
		if (str.size() >= 2 && str[index+0] == '0' && tolower(str[index+1]) == 'x')
			str = str.substr(index + 2);

		if (std::from_chars(str.data(), str.data() + str.size(), result, base).ec == std::errc())
		{
			if (index == 1)
			{
				if constexpr(std::is_unsigned_v<T>)
					result = static_cast<T>(-static_cast<std::make_signed_t<T>>(result));
				else
					result = -result;
			}	
			
			return result;
		}

		return {};
	}
	
	if(std::from_chars(str.data(), str.data() + str.size(), result, base).ec == std::errc())
		return result;

	return {};
}

template<typename T>
T ConvertString(std::string_view str)
{
	if (str.empty())
		return {};
	
	T result;
	ltrim(str);
	if constexpr (std::is_same_v<T, bool>)
	{
		return str == "1" || boost::iequals(str, "true");
	}
	else if constexpr(std::is_floating_point_v<T>)
	{
		// from_chars can't deal with float conversation starting with "+"
		ltrim(str, "+");
#ifdef __clang__
		if (fast_float::from_chars(str.data(), str.data() + str.size(), result).ec == std::errc())
			return result;
#else
		if (std::from_chars(str.data(), str.data() + str.size(), result).ec == std::errc())
			return result;
#endif
		return {};
	}
	else if constexpr(std::is_enum_v<T>)
	{
		return (T)ConvertString<std::underlying_type_t<T>>(str);
	}
	else
	{
		const sint32 index = str[0] == '-' ? 1 : 0;
		if (str.size() >= 2 && str[index + 0] == '0' && tolower(str[index + 1]) == 'x')
			result = ConvertString<T>(str, 16);
		else
			result = ConvertString<T>(str, 10);
	}

	return result;
}

template <typename T>
constexpr T DegToRad(T deg) { return (T)((double)deg * M_PI / 180); }
template <typename T>
constexpr T RadToDeg(T rad) { return (T)((double)rad * 180 / M_PI); }

template<typename T>
std::string ToString(T value)
{
	std::ostringstream str;
	str.imbue(std::locale("C"));
	str << value;
	return str.str();
}

template<typename T>
T FromString(std::string value)
{
	std::istringstream str(value);
	str.imbue(std::locale("C"));

	T tmp;
	str >> tmp;
	return tmp;
}

template<typename T>
size_t RemoveDuplicatesKeepOrder(std::vector<T>& vec)
{
	std::set<T> tmp;
	auto new_end = std::remove_if(vec.begin(), vec.end(), [&tmp](const T& value)
		{
			if (tmp.find(value) != std::end(tmp))
				return true;

			tmp.insert(value);
			return false;
		});

	vec.erase(new_end, vec.end());
	return vec.size();
}

void SetThreadName(const char* name);

inline uint64 MakeU64(uint32 high, uint32 low)
{
	return ((uint64)high << 32) | ((uint64)low);
}

static bool IsValidFilename(std::string_view sv)
{
	for (auto& it : sv)
	{
		uint8 c = (uint8)it;
		if (c < 0x20)
			return false;
		if (c == '.' || c == '#' || c == '/' || c == '\\' ||
			c == '<' || c == '>' || c == '|' || c == ':' ||
			c == '\"')
			return false;
	}
	if (!sv.empty())
	{
		if (sv.back() == ' ' || sv.back() == '.')
			return false;
	}
	return true;
}

// MAJOR; MINOR
std::pair<DWORD, DWORD> GetWindowsVersion();
bool IsWindows81OrGreater();
bool IsWindows10OrGreater();

fs::path GetParentProcess();

std::optional<std::vector<uint8>> zlibDecompress(const std::vector<uint8>& compressed, size_t sizeHint = 32*1024);
