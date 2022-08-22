#pragma once

#include <type_traits>

// enum flag helpers
template<typename TEnum>
struct EnableBitMaskOperators
{
	static const bool enable = false;
};

template<typename TEnum>
typename std::enable_if<EnableBitMaskOperators<TEnum>::enable, TEnum>::type
operator &(TEnum lhs, TEnum rhs)
{
	return static_cast<TEnum> (static_cast<typename std::underlying_type<TEnum>::type>(lhs) & static_cast<typename std::underlying_type<TEnum>::type>(rhs));
}

template<typename TEnum>
typename std::enable_if<EnableBitMaskOperators<TEnum>::enable, TEnum>::type
operator |(TEnum lhs, TEnum rhs)
{
	return static_cast<TEnum> (static_cast<typename std::underlying_type<TEnum>::type>(lhs) | static_cast<typename std::underlying_type<TEnum>::type>(rhs));
}

template<typename TEnum>
typename std::enable_if<EnableBitMaskOperators<TEnum>::enable, TEnum>::type
operator ^(TEnum lhs, TEnum rhs)
{
	return static_cast<TEnum> (static_cast<typename std::underlying_type<TEnum>::type>(lhs) ^ static_cast<typename std::underlying_type<TEnum>::type>(rhs));
}

template<typename TEnum>
typename std::enable_if<EnableBitMaskOperators<TEnum>::enable, TEnum>::type
operator ~(TEnum rhs)
{
	return static_cast<TEnum> (~static_cast<typename std::underlying_type<TEnum>::type>(rhs));
}

template<typename TEnum>
typename std::enable_if<EnableBitMaskOperators<TEnum>::enable, TEnum>::type&
operator &=(TEnum& lhs, TEnum rhs)
{
	lhs = static_cast<TEnum> (static_cast<typename std::underlying_type<TEnum>::type>(lhs) & static_cast<typename std::underlying_type<TEnum>::type>(rhs));
	return lhs;
}

template<typename TEnum>
typename std::enable_if<EnableBitMaskOperators<TEnum>::enable, TEnum>::type&
operator |=(TEnum& lhs, TEnum rhs)
{
	lhs = static_cast<TEnum> (static_cast<typename std::underlying_type<TEnum>::type>(lhs) | static_cast<typename std::underlying_type<TEnum>::type>(rhs));
	return lhs;
}

template<typename TEnum>
typename std::enable_if<EnableBitMaskOperators<TEnum>::enable, TEnum>::type&
operator ^=(TEnum& lhs, TEnum rhs)
{
	lhs = static_cast<TEnum> (static_cast<typename std::underlying_type<TEnum>::type>(lhs) ^ static_cast<typename std::underlying_type<TEnum>::type>(rhs));
	return lhs;
}

template<typename TEnum, typename = std::enable_if_t<EnableBitMaskOperators<TEnum>::enable>>
constexpr bool operator==(TEnum lhs, std::underlying_type_t<TEnum> rhs)
{
	return static_cast<std::underlying_type_t<TEnum>>(lhs) == rhs;
}
template<typename TEnum, typename = std::enable_if_t<EnableBitMaskOperators<TEnum>::enable>>
constexpr bool operator!=(TEnum lhs, std::underlying_type_t<TEnum> rhs)
{
	return static_cast<std::underlying_type_t<TEnum>>(lhs) != rhs;
}

#define ENABLE_BITMASK_OPERATORS(x) template<> struct EnableBitMaskOperators<x> { static const bool enable = true; };


template<typename TEnum>
struct EnableEnumIterators
{
	static const bool enable = false;
};

template<typename TEnum>
typename std::enable_if<EnableEnumIterators<TEnum>::enable, TEnum>::type&
operator++(TEnum& lhs) 
{
	lhs = static_cast<TEnum>(static_cast<typename std::underlying_type<TEnum>::type>(lhs) + 1);
	return lhs;
}

template<typename TEnum>
typename std::enable_if<EnableEnumIterators<TEnum>::enable, TEnum>::type
operator*(TEnum rhs) 
{
	return rhs;
}

template<typename TEnum>
typename std::enable_if<EnableEnumIterators<TEnum>::enable, TEnum>::type
begin(TEnum value) 
{
	return EnableEnumIterators<TEnum>::begin;
}

template<typename TEnum>
typename std::enable_if<EnableEnumIterators<TEnum>::enable, TEnum>::type
rbegin(TEnum value) 
{
	return EnableEnumIterators<TEnum>::rbegin;
}

template<typename TEnum>
typename std::enable_if<EnableEnumIterators<TEnum>::enable, TEnum>::type
end(TEnum r) {
	return EnableEnumIterators<TEnum>::end;
}

template<typename TEnum>
typename std::enable_if<EnableEnumIterators<TEnum>::enable, TEnum>::type
rend(TEnum r) {
	return EnableEnumIterators<TEnum>::rend;
}

#define ENABLE_ENUM_ITERATORS(x, first_value, last_value) template<> struct EnableEnumIterators<x> {\
	static const bool enable = true;\
	static const x begin = first_value;\
	static const x rbegin = last_value;\
	static const x end = static_cast<x>(static_cast<typename std::underlying_type<x>::type>(last_value) + 1);\
	static const x rend = static_cast<x>(static_cast<typename std::underlying_type<x>::type>(first_value) - 1);\
};
// todo: rend type must be signed?