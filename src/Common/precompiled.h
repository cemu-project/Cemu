#pragma once

#include <stdlib.h> // for size_t

#include "version.h"
#include "platform.h"

#define FMT_HEADER_ONLY
#define FMT_USE_GRISU 1
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#if FMT_VERSION > 80000
#include <fmt/xchar.h> // needed for wchar_t support
#endif

#define SDL_MAIN_HANDLED

// #if FMT_VERSION > 80102
// // restore generic formatter for enum (class) to underlying. We currently utilize this for HLE call logging
// template <typename Enum, std::enable_if_t<std::is_enum<Enum>::value, int> = 0>
// constexpr auto format_as(Enum e) noexcept -> std::underlying_type<Enum> {
//   return static_cast<std::underlying_type<Enum>>(e);
// }
// #endif

// c includes
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <cassert>
#include <immintrin.h>

// c++ includes
#include <string>
#include <string_view>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

#include <mutex>
#include <shared_mutex>
#include <thread>
#include <future>
#include <atomic>

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <set>
#include <queue>
#include <stack>
#include <array>
#include <bitset>
#include <bit>

#include <filesystem>
#include <memory>
#include <chrono>
#include <regex>
#include <type_traits>
#include <optional>
#include <span>

#include <boost/predef.h>
#include <boost/nowide/convert.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace fs = std::filesystem;

#include "enumFlags.h"
#include "zstring_view.h"

// base types
using uint64 = uint64_t;
using uint32 = uint32_t;
using uint16 = uint16_t;
using uint8 = uint8_t;

using sint64 = int64_t;
using sint32 = int32_t;
using sint16 = int16_t;
using sint8 = int8_t;

using MPTR = uint32;
using MPTR_UINT8 = uint32;
using MPTR_UINT16 = uint32;
using MPTR_UINT32 = uint32;

// types with explicit big endian order
#include "betype.h"

// types with explicit little endian order (on a big-endian system these would be implemented like betype.h)
using uint64le = uint64_t;
using uint32le = uint32_t;
using uint16le = uint16_t;
using uint8le = uint8_t;

// logging
#include "Cemu/Logging/CemuDebugLogging.h"
#include "Cemu/Logging/CemuLogging.h"

// CPU extensions
extern bool _cpuExtension_SSSE3;
extern bool _cpuExtension_SSE4_1;
extern bool _cpuExtension_AVX2;

// manual endian-swapping

#if _MSC_VER
inline uint64 _swapEndianU64(uint64 v)
{
	return _byteswap_uint64(v);
}

inline uint32 _swapEndianU32(uint32 v)
{
	return _byteswap_ulong(v);
}

inline sint32 _swapEndianS32(sint32 v)
{
	return (sint32)_byteswap_ulong((uint32)v);
}

inline uint16 _swapEndianU16(uint16 v)
{
	return (v >> 8) | (v << 8);
}

inline sint16 _swapEndianS16(sint16 v)
{
	return (sint16)(((uint16)v >> 8) | ((uint16)v << 8));
}
#else
inline uint64 _swapEndianU64(uint64 v)
{
#if BOOST_OS_MACOS
    return OSSwapInt64(v);
#else
    return bswap_64(v);
#endif
}

inline uint32 _swapEndianU32(uint32 v)
{
#if BOOST_OS_MACOS
    return OSSwapInt32(v);
#else
    return bswap_32(v);
#endif
}

inline sint32 _swapEndianS32(sint32 v)
{
#if BOOST_OS_MACOS
    return (sint32)OSSwapInt32((uint32)v);
#else
    return (sint32)bswap_32((uint32)v);
#endif
}

inline uint16 _swapEndianU16(uint16 v)
{
    return (v >> 8) | (v << 8);
}

inline sint16 _swapEndianS16(sint16 v)
{
    return (sint16)(((uint16)v >> 8) | ((uint16)v << 8));
}

inline uint64 _umul128(uint64 multiplier, uint64 multiplicand, uint64 *highProduct) {
    unsigned __int128 x = (unsigned __int128)multiplier * (unsigned __int128)multiplicand;
    *highProduct = (x >> 64);
    return x & 0xFFFFFFFFFFFFFFFF;
}

typedef uint8_t BYTE;
typedef uint32_t DWORD;
typedef int32_t LONG;
typedef int64_t LONGLONG;

typedef union _LARGE_INTEGER {
    struct {
        DWORD LowPart;
        LONG  HighPart;
    };
    struct {
        DWORD LowPart;
        LONG  HighPart;
    } u;
    LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

#define DEFINE_ENUM_FLAG_OPERATORS(T)                                                                                                                                            \
    inline T operator~ (T a) { return static_cast<T>( ~static_cast<std::underlying_type<T>::type>(a) ); }                                                                       \
    inline T operator| (T a, T b) { return static_cast<T>( static_cast<std::underlying_type<T>::type>(a) | static_cast<std::underlying_type<T>::type>(b) ); }                   \
    inline T operator& (T a, T b) { return static_cast<T>( static_cast<std::underlying_type<T>::type>(a) & static_cast<std::underlying_type<T>::type>(b) ); }                   \
    inline T operator^ (T a, T b) { return static_cast<T>( static_cast<std::underlying_type<T>::type>(a) ^ static_cast<std::underlying_type<T>::type>(b) ); }                   \
    inline T& operator|= (T& a, T b) { return reinterpret_cast<T&>( reinterpret_cast<std::underlying_type<T>::type&>(a) |= static_cast<std::underlying_type<T>::type>(b) ); }   \
    inline T& operator&= (T& a, T b) { return reinterpret_cast<T&>( reinterpret_cast<std::underlying_type<T>::type&>(a) &= static_cast<std::underlying_type<T>::type>(b) ); }   \
    inline T& operator^= (T& a, T b) { return reinterpret_cast<T&>( reinterpret_cast<std::underlying_type<T>::type&>(a) ^= static_cast<std::underlying_type<T>::type>(b) ); }
#endif

#if !defined(_MSC_VER) || defined(__clang__) // clang-cl does not have built-in _udiv128
inline uint64 _udiv128(uint64 highDividend, uint64 lowDividend, uint64 divisor, uint64 *remainder)
{
    unsigned __int128 dividend = (((unsigned __int128)highDividend) << 64) | ((unsigned __int128)lowDividend);
    *remainder = (uint64)((dividend % divisor) & 0xFFFFFFFFFFFFFFFF);
    return       (uint64)((dividend / divisor) & 0xFFFFFFFFFFFFFFFF);
}
#endif

#if defined(_MSC_VER)
    #define UNREACHABLE __assume(false)
#elif defined(__GNUC__)
    #define UNREACHABLE __builtin_unreachable()
#else
    #define UNREACHABLE
#endif

#if defined(_MSC_VER)
    #define DEBUG_BREAK __debugbreak()
#else
    #include <csignal>
    #define DEBUG_BREAK raise(SIGTRAP) 
#endif

#if defined(_MSC_VER)
    #define DLLEXPORT __declspec(dllexport)
#elif defined(__GNUC__)
    #if BOOST_OS_WINDOWS
        #define DLLEXPORT __attribute__((dllexport))
    #else
        #define DLLEXPORT
    #endif
#else
    #error No definition for DLLEXPORT
#endif

#if BOOST_OS_WINDOWS
	#define NOEXPORT
#elif defined(__GNUC__)
	#define NOEXPORT __attribute__ ((visibility ("hidden")))
#endif

#ifdef __GNUC__
#include <cpuid.h>
#endif

inline void cpuid(int cpuInfo[4], int functionId) {
#if defined(_MSC_VER)
    __cpuid(cpuInfo, functionId);
#elif defined(__GNUC__)
    __cpuid(functionId, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
#else
    #error No definition for cpuid
#endif
}

inline void cpuidex(int cpuInfo[4], int functionId, int subFunctionId) {
#if defined(_MSC_VER)
    __cpuidex(cpuInfo, functionId, subFunctionId);
#elif defined(__GNUC__)
    __cpuid_count(functionId, subFunctionId, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
#else
    #error No definition for cpuidex
#endif
}


// MEMPTR
#include "Common/MemPtr.h"

#define MPTR_NULL	(0)

template <typename T1, typename T2>
constexpr bool HAS_FLAG(T1 flags, T2 test_flag) { return (flags & (T1)test_flag) == (T1)test_flag; }
template <typename T1, typename T2>
constexpr bool HAS_BIT(T1 value, T2 index) { return (value & ((T1)1 << index)) != 0; }
template <typename T>
constexpr void SAFE_RELEASE(T& p) { if (p) { p->Release(); p = nullptr; } }

template <typename T>
constexpr uint32_t ppcsizeof() { return (uint32_t) sizeof(T); }

// common utility functions

template <typename T>
void vectorAppendUnique(std::vector<T>& vec, const T& val) // for cases where a small vector is more efficient than a set
{
    if (std::find(vec.begin(), vec.end(), val) != vec.end())
        return;
    vec.emplace_back(val);
}

template <typename T>
void vectorRemoveByValue(std::vector<T>& vec, const T& val)
{
    vec.erase(std::remove(vec.begin(), vec.end(), val), vec.end());
}

template <typename T>
void vectorRemoveByIndex(std::vector<T>& vec, const size_t index)
{
    vec.erase(vec.begin() + index);
}

template<typename T1, typename T2>
int match_any_of(T1 value, T2 compareTo)
{
    return value == compareTo;
}

template<typename T1, typename T2, typename... Types>
bool match_any_of(T1 value, T2 compareTo, Types&&... others)
{
    return value == compareTo || match_any_of(value, others...);
}

// we cache the frequency in a static variable
[[nodiscard]] static std::chrono::high_resolution_clock::time_point now_cached() noexcept
{
#ifdef _WIN32
    // get current time
	static const long long _Freq = _Query_perf_frequency();	// doesn't change after system boot
	const long long _Ctr = _Query_perf_counter();
	static_assert(std::nano::num == 1, "This assumes period::num == 1.");
	const long long _Whole = (_Ctr / _Freq) * std::nano::den;
	const long long _Part = (_Ctr % _Freq) * std::nano::den / _Freq;
	return (std::chrono::high_resolution_clock::time_point(std::chrono::nanoseconds(_Whole + _Part)));
#else
    return std::chrono::high_resolution_clock::now();
#endif
}


[[nodiscard]] static std::chrono::steady_clock::time_point tick_cached() noexcept
{
#ifdef _WIN32
    // get current time
	static const long long _Freq = _Query_perf_frequency();	// doesn't change after system boot
	const long long _Ctr = _Query_perf_counter();
	static_assert(std::nano::num == 1, "This assumes period::num == 1.");
	const long long _Whole = (_Ctr / _Freq) * std::nano::den;
	const long long _Part = (_Ctr % _Freq) * std::nano::den / _Freq;
	return (std::chrono::steady_clock::time_point(std::chrono::nanoseconds(_Whole + _Part)));
#else
    // todo: Add faster implementation for linux
    return std::chrono::steady_clock::now();
#endif
}

inline void cemu_assert(bool _condition)
{
    if ((_condition) == false)
    {
        DEBUG_BREAK;
    }
}

#ifndef CEMU_DEBUG_ASSERT
//#define cemu_assert_debug(__cond) -> Forcing __cond not to be evaluated currently has unexpected side-effects

inline void cemu_assert_debug(bool _condition)
{
}

inline void cemu_assert_unimplemented()
{
}

inline void cemu_assert_suspicious()
{
}

inline void cemu_assert_error()
{
	DEBUG_BREAK;
}
#else
inline void cemu_assert_debug(bool _condition)
{
    if ((_condition) == false)
        DEBUG_BREAK;
}

inline void cemu_assert_unimplemented()
{
    DEBUG_BREAK;
}

inline void cemu_assert_suspicious()
{
    DEBUG_BREAK;
}

inline void cemu_assert_error()
{
    DEBUG_BREAK;
}
#endif

#define assert_dbg() DEBUG_BREAK // old style unconditional generic assert

// Some string conversion helpers because C++20 std::u8string is too cumbersome to use in practice
// mixing string types generally causes loads of issues and many of the libraries we use dont expose interfaces for u8string

inline const char* _utf8WrapperPtr(const char8_t* input)
{
    // use with care
    return (const char*)input;
}

inline std::string_view _utf8Wrapper(std::u8string_view input)
{
    std::basic_string_view<char> v((char*)input.data(), input.size());
    return v;
}

// convert fs::path to utf8 encoded string
inline std::string _pathToUtf8(const fs::path& path)
{
    std::u8string strU8 = path.generic_u8string();
    std::string v((const char*)strU8.data(), strU8.size());
    return v;
}

// convert utf8 encoded string to fs::path
inline fs::path _utf8ToPath(std::string_view input)
{
    std::basic_string_view<char8_t> v((char8_t*)input.data(), input.size());
    return fs::path(v);
}

class RunAtCemuBoot // -> replaces this with direct function calls. Linkers other than MSVC may optimize way object files entirely if they are not referenced from outside. So a source file self-registering using this would be causing issues
{
public:
    RunAtCemuBoot(void(*f)())
    {
        f();
    }
};

// temporary wrapper until Concurrency TS gives us std::future .is_ready()
template<typename T>
bool future_is_ready(std::future<T>& f)
{
#if defined(__GNUC__)
    return f.wait_for(std::chrono::nanoseconds(0)) == std::future_status::ready;
#else
	return f._Is_ready();
#endif
}

// helper function to cast raw pointers to std::atomic
// this is technically not legal but works on most platforms as long as alignment restrictions are met and the implementation of atomic doesnt come with additional members

template<typename T>
std::atomic<T>* _rawPtrToAtomic(T* ptr)
{
    return reinterpret_cast<std::atomic<T>*>(ptr);
}

#if defined(__GNUC__)
#define ATTR_MS_ABI __attribute__((ms_abi))
#else
#define ATTR_MS_ABI
#endif

#if !defined(TRUE) or !defined(FALSE)
#define TRUE (1)
#define FALSE (0)
#endif

inline uint32 GetTitleIdHigh(uint64 titleId)
{
	return titleId >> 32;
}

inline uint32 GetTitleIdLow(uint64 titleId)
{
	return titleId & 0xFFFFFFFF;
}

#if defined(__GNUC__)
#define memcpy_dwords(__dest, __src, __numDwords) memcpy((__dest), (__src), (__numDwords) * sizeof(uint32))
#define memcpy_qwords(__dest, __src, __numQwords) memcpy((__dest), (__src), (__numQwords) * sizeof(uint64))
#else
#define memcpy_dwords(__dest, __src, __numDwords) __movsd((unsigned long*)(__dest), (const unsigned long*)(__src), __numDwords)
#define memcpy_qwords(__dest, __src, __numQwords) __movsq((unsigned long long*)(__dest), (const unsigned long long*)(__src), __numQwords)
#endif

// PPC context and memory functions
#include "Cafe/HW/MMU/MMU.h"
#include "Cafe/HW/Espresso/PPCState.h"
#include "Cafe/HW/Espresso/PPCCallback.h"
