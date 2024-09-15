#pragma once

#include <type_traits>

namespace Latte
{
	class LATTEREG;
};

template<class T, std::size_t... N>
constexpr T bswap_impl(T i, std::index_sequence<N...>)
{
	return ((((i >> (N * CHAR_BIT)) & (T)(uint8_t)(-1)) << ((sizeof(T) - 1 - N) * CHAR_BIT)) | ...);
}

template<class T, class U = std::make_unsigned_t<T>>
constexpr T bswap(T i)
{
	return (T)bswap_impl<U>((U)i, std::make_index_sequence<sizeof(T)>{});
}

template <typename T>
constexpr T SwapEndian(T value)
{
	if constexpr (std::is_integral<T>::value)
	{
#ifdef _MSC_VER
		if constexpr (sizeof(T) == sizeof(uint32_t))
		{
			return (T)_byteswap_ulong(value);
		}
#endif

		return (T)bswap((std::make_unsigned_t<T>)value);
	}
	else if constexpr (std::is_floating_point<T>::value)
	{
		if constexpr (sizeof(T) == sizeof(uint32_t))
		{
			const auto tmp = bswap<uint32_t>(*(uint32_t*)&value);
			return *(T*)&tmp;
		}
		if constexpr (sizeof(T) == sizeof(uint64_t))
		{
			const auto tmp = bswap<uint64_t>(*(uint64_t*)&value);
			return *(T*)&tmp;
		}
	}
	else if constexpr (std::is_enum<T>::value)
	{
		return (T)SwapEndian((std::underlying_type_t<T>)value);
	}
	else if constexpr (std::is_base_of<Latte::LATTEREG, T>::value)
	{
		const auto tmp = bswap<uint32_t>(*(uint32_t*)&value);
		return *(T*)&tmp;
	}
    else
    {
        static_assert(std::is_integral<T>::value, "unsupported betype specialization!");
    }

	return value;
}

#ifndef WIN32
//static_assert(false, "_BE and _LE need to be adjusted");
#endif

// swap if native isn't big endian
template <typename T>
constexpr T _BE(T value)
{
	return SwapEndian(value);
}

// swap if native isn't little endian
template <typename T>
constexpr T _LE(T value)
{
	return value;
}

template <typename T>
class betype
{
public:
	constexpr betype() = default;

	// copy
	constexpr betype(T value)
		: m_value(SwapEndian(value)) {}

	constexpr betype(const betype& value) = default; // required for trivially_copyable
	//constexpr betype(const betype& value)
	//	: m_value(value.m_value) {}

	template <typename U>
	constexpr betype(const betype<U>& value)
		: betype((T)value.value()) {}

	// assigns
	static betype from_bevalue(T value)
	{
		betype result;
		result.m_value = value;
		return result;
	}

	// returns LE value
	constexpr T value() const { return SwapEndian<T>(m_value); }

	// returns BE value
	constexpr T bevalue() const { return m_value; }

	constexpr operator T() const { return value(); }

	betype<T>& operator+=(const betype<T>& v)
	{
		m_value = SwapEndian(T(value() + v.value()));
		return *this;
	}

	betype<T>& operator+=(const T& v) requires std::integral<T>
	{
		m_value = SwapEndian(T(value() + v));
		return *this;
	}

	betype<T>& operator-=(const betype<T>& v)
	{
		m_value = SwapEndian(T(value() - v.value()));
		return *this;
	}

	betype<T>& operator*=(const betype<T>& v)
	{
		m_value = SwapEndian(T(value() * v.value()));
		return *this;
	}

	betype<T>& operator/=(const betype<T>& v)
	{
		m_value = SwapEndian(T(value() / v.value()));
		return *this;
	}

	betype<T>& operator&=(const betype<T>& v) requires (requires (T& x, const T& y) { x &= y; })
	{
		m_value &= v.m_value;
		return *this;
	}

	betype<T>& operator|=(const betype<T>& v) requires (requires (T& x, const T& y) { x |= y; })
	{
		m_value |= v.m_value;
		return *this;
	}

	betype<T>& operator|(const betype<T>& v) const requires (requires (T& x, const T& y) { x | y; })
	{
		betype<T> tmp(*this);
		tmp.m_value = tmp.m_value | v.m_value;
		return tmp;
	}

	betype<T> operator|(const T& v) const requires (requires (T& x, const T& y) { x | y; })
	{
		betype<T> tmp(*this);
		tmp.m_value = tmp.m_value | SwapEndian(v);
		return tmp;
	}

	betype<T>& operator^=(const betype<T>& v) requires std::integral<T>
	{
		m_value ^= v.m_value;
		return *this;
	}

	betype<T>& operator>>=(std::size_t idx) requires std::integral<T>
	{
		m_value = SwapEndian(T(value() >> idx));
		return *this;
	}

	betype<T>& operator<<=(std::size_t idx) requires std::integral<T>
	{
		m_value = SwapEndian(T(value() << idx));
		return *this;
	}

	betype<T> operator~() const requires std::integral<T>
	{
		return from_bevalue(T(~m_value));
	}

	// pre-increment
	betype<T>& operator++() requires std::integral<T>
	{
		m_value = SwapEndian(T(value() + 1));
		return *this;
	}

	// post-increment
	betype<T> operator++(int) requires std::integral<T>
	{
		betype<T> tmp(*this);
		m_value = SwapEndian(T(value() + 1));
		return tmp;
	}

	// pre-decrement
	betype<T>& operator--() requires std::integral<T>
	{
		m_value = SwapEndian(T(value() - 1));
		return *this;
	}

	// post-decrement
	betype<T> operator--(int) requires std::integral<T>
	{
		betype<T> tmp(*this);
		m_value = SwapEndian(T(value() - 1));
		return tmp;
	}

private:
	//T m_value{}; // before 1.26.2
	T m_value;
};

using uint64be = betype<uint64>;
using uint32be = betype<uint32>;
using uint16be = betype<uint16>;
using uint8be = betype<uint8>;

using sint64be = betype<sint64>;
using sint32be = betype<sint32>;
using sint16be = betype<sint16>;
using sint8be = betype<sint8>;

using float32be = betype<float>;
using float64be = betype<double>;

static_assert(sizeof(betype<uint64>) == sizeof(uint64));
static_assert(sizeof(betype<uint32>) == sizeof(uint32));
static_assert(sizeof(betype<uint16>) == sizeof(uint16));
static_assert(sizeof(betype<uint8>) == sizeof(uint8));
static_assert(sizeof(betype<float>) == sizeof(float));
static_assert(sizeof(betype<double>) == sizeof(double));

static_assert(std::is_trivially_copyable_v<uint32be>);
static_assert(std::is_trivially_constructible_v<uint32be>);
static_assert(std::is_copy_constructible_v<uint32be>);
static_assert(std::is_move_constructible_v<uint32be>);
static_assert(std::is_copy_assignable_v<uint32be>);
static_assert(std::is_move_assignable_v<uint32be>);
