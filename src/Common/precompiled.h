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

// arch defines

#if defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)
#define ARCH_X86_64
#endif

// c includes
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <cassert>

#if defined(ARCH_X86_64)
#include <immintrin.h>
#endif

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
#include <ctime>
#include <regex>
#include <type_traits>
#include <optional>
#include <span>
#include <ranges>

#include <boost/predef.h>
#include <boost/nowide/convert.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace fs = std::filesystem;

#include "enumFlags.h"

// base types
using uint64 = uint64_t;
using uint32 = uint32_t;
using uint16 = uint16_t;
using uint8 = uint8_t;

using sint64 = int64_t;
using sint32 = int32_t;
using sint16 = int16_t;
using sint8 = int8_t;

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

template<typename T>
inline T GetBits(T value, uint32 index, uint32 numBits)
{
	T mask = (1<<numBits)-1;
	return (value>>index) & mask;
}

template<typename T>
inline void SetBits(T& value, uint32 index, uint32 numBits, uint32 bitValue)
{
	T mask = (1<<numBits)-1;
	value &= ~(mask << index);
	value |= (bitValue << index);
}

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
	#define ASSUME(__cond) __assume(__cond)
	#define TLS_WORKAROUND_NOINLINE // no-op for MSVC as it has a flag for fiber-safe TLS optimizations
#elif defined(__GNUC__) && !defined(__llvm__)
    #define UNREACHABLE __builtin_unreachable()
	#define ASSUME(__cond) __attribute__((assume(__cond)))
	#define TLS_WORKAROUND_NOINLINE __attribute__((noinline))
#elif defined(__clang__)
	#define UNREACHABLE __builtin_unreachable()
	#define ASSUME(__cond) __builtin_assume(__cond)
	#define TLS_WORKAROUND_NOINLINE __attribute__((noinline))
#else
    #error Unknown compiler
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

#if defined(_MSC_VER)
#define FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define FORCE_INLINE inline __attribute__((always_inline))
#else
#define FORCE_INLINE inline
#endif

FORCE_INLINE int BSF(uint32 v) // returns index of first bit set, counting from LSB. If v is 0 then result is undefined
{
#if defined(_MSC_VER)
	return _tzcnt_u32(v); // TZCNT requires BMI1. But if not supported it will execute as BSF
#elif defined(__GNUC__) || defined(__clang__)
	return __builtin_ctz(v);
#else
	return std::countr_zero(v);
#endif
}

// On aarch64 we handle some of the x86 intrinsics by implementing them as wrappers
#if defined(__aarch64__)

inline void _mm_pause()
{
    asm volatile("yield");
}

inline uint64 __rdtsc()
{
    uint64 t;
    asm volatile("mrs %0, cntvct_el0" : "=r" (t));
    return t;
}

inline void _mm_mfence()
{
	asm volatile("" ::: "memory");
	std::atomic_thread_fence(std::memory_order_seq_cst);
}

inline unsigned char _addcarry_u64(unsigned char carry, unsigned long long a, unsigned long long b, unsigned long long *result)
{
    *result = a + b + (unsigned long long)carry;
    if (*result < a)
        return 1;
    return 0;
}

#endif

// asserts


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

// MEMPTR
#include "Common/MemPtr.h"

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

template<typename T1, typename... Types>
bool match_any_of(T1&& value, Types&&... others)
{
    return ((value == others) || ...);
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
#if BOOST_OS_WINDOWS
    // get current time
	static const long long _Freq = _Query_perf_frequency();	// doesn't change after system boot
	const long long _Ctr = _Query_perf_counter();
	static_assert(std::nano::num == 1, "This assumes period::num == 1.");
	const long long _Whole = (_Ctr / _Freq) * std::nano::den;
	const long long _Part = (_Ctr % _Freq) * std::nano::den / _Freq;
	return (std::chrono::steady_clock::time_point(std::chrono::nanoseconds(_Whole + _Part)));
#elif BOOST_OS_LINUX
	struct timespec tp;
	clock_gettime(CLOCK_MONOTONIC_RAW, &tp);
	return std::chrono::steady_clock::time_point(
		std::chrono::seconds(tp.tv_sec) + std::chrono::nanoseconds(tp.tv_nsec));
#elif BOOST_OS_MACOS
	return std::chrono::steady_clock::time_point(
		std::chrono::nanoseconds(clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW)));
#endif
}

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

// locale-independent variant of tolower() which also matches Wii U behavior
inline char _ansiToLower(char c)
{
	if (c >= 'A' && c <= 'Z')
		c -= ('A' - 'a');
	return c;
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
	static_assert(sizeof(T) == sizeof(std::atomic<T>));
	cemu_assert_debug((reinterpret_cast<std::uintptr_t>(ptr) % alignof(std::atomic<T>)) == 0);
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

// PPC stack trace printer
void DebugLogStackTrace(struct OSThread_t* thread, MPTR sp, bool printSymbols = false);

// generic formatter for enums (to underlying)
template <typename Enum>
	requires std::is_enum_v<Enum>
struct fmt::formatter<Enum> : fmt::formatter<underlying_t<Enum>>
{
	auto format(const Enum& e, format_context& ctx) const
	{
		//return fmt::format_to(ctx.out(), "{}", fmt::underlying(e));

		return formatter<underlying_t<Enum>>::format(fmt::underlying(e), ctx);
	}
};

// formatter for betype<T>
template <typename T>
struct fmt::formatter<betype<T>> : fmt::formatter<T>
{
	auto format(const betype<T>& e, format_context& ctx) const
	{
		return formatter<T>::format(static_cast<T>(e), ctx);
	}
};

// useful future C++ stuff
namespace stdx
{
	// std::to_underlying
    template <typename EnumT, typename = std::enable_if_t < std::is_enum<EnumT>{} >>
        constexpr std::underlying_type_t<EnumT> to_underlying(EnumT e) noexcept {
        return static_cast<std::underlying_type_t<EnumT>>(e);
    };

	// std::scope_exit
	template <typename Fn>
	class scope_exit
	{
		Fn m_func;
		bool m_released = false;
	public:
		explicit scope_exit(Fn&& f) noexcept
			: m_func(std::forward<Fn>(f))
		{}
		~scope_exit()
		{
			if (!m_released) m_func();
		}
		scope_exit(scope_exit&& other) noexcept
			: m_func(std::move(other.m_func)), m_released(std::exchange(other.m_released, true))
		{}
		scope_exit(const scope_exit&) = delete;
		scope_exit& operator=(scope_exit) = delete;
		void release() { m_released = true;}
	};
}
