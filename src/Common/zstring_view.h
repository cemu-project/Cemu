#ifndef ASZ_ZSTRING_VIEW_HPP
#define ASZ_ZSTRING_VIEW_HPP

// https://github.com/Aszarsha/zstring_view/blob/master/zstring_view.hpp

#include <string_view>

// Copy interface from cppreference's std::basic_string_view page
//  http://en.cppreference.com/w/cpp/string/basic_string_view
// and add conversion from zstring_view to basic_string_view and CharT const*

template< class CharT
	, class Traits = std::char_traits< CharT >
>
class basic_zstring_view
	: private std::basic_string_view< CharT, Traits > {
private:
	using string_view_base = std::basic_string_view< CharT, Traits >;

public:   /// Types ///
	using typename string_view_base::traits_type;
	using typename string_view_base::value_type;
	using typename string_view_base::pointer;
	using typename string_view_base::const_pointer;
	using typename string_view_base::reference;
	using typename string_view_base::const_reference;
	using typename string_view_base::iterator;
	using typename string_view_base::const_iterator;
	using typename string_view_base::reverse_iterator;
	using typename string_view_base::const_reverse_iterator;
	using typename string_view_base::size_type;
	using typename string_view_base::difference_type;

public:   /// Static members ///
	using string_view_base::npos;

public:   /// Special member functions ///
	constexpr basic_zstring_view() noexcept = default;

	constexpr basic_zstring_view(basic_zstring_view const&) noexcept = default;
	constexpr basic_zstring_view& operator=(basic_zstring_view const&) noexcept = default;

	constexpr basic_zstring_view(basic_zstring_view&&) noexcept = default;
	constexpr basic_zstring_view& operator=(basic_zstring_view&&) noexcept = default;

	basic_zstring_view(CharT const* s) : string_view_base{ s } {   }
	basic_zstring_view(CharT const* s, size_type sz) : string_view_base{ s, sz } {   }

	// Explicitly no convertion from string_view, since null-termination is not guaranteed
	basic_zstring_view(std::basic_string<CharT, std::char_traits<CharT>, std::allocator<CharT>> const& s) : string_view_base{ s } {   }

public:   /// Member functions ///
	//== Iterators ==//
	using string_view_base::begin;
	using string_view_base::cbegin;

	using string_view_base::end;
	using string_view_base::cend;

	using string_view_base::rbegin;
	using string_view_base::crbegin;

	using string_view_base::rend;
	using string_view_base::crend;

	//== Element access ==//
	using string_view_base::operator[];

	using string_view_base::at;

	using string_view_base::front;

	using string_view_base::back;

	using string_view_base::data;

	//== Capacity ==//
	using string_view_base::size;
	using string_view_base::length;

	using string_view_base::max_size;

	using string_view_base::empty;

	//== Modifiers ==//
	using string_view_base::remove_prefix;

	//Explicitly NOT
	//using string_view_base::remove_suffix;

	using string_view_base::swap;

	//== Operations ==//
	using string_view_base::copy;

	//Explicitly NOT
	//using::string_view_base::substr;

	using string_view_base::compare;

	// C++20
	//using string_view_base::starts_with;

	// C++20
	//using string_view_base::ends_with;

	using string_view_base::find;

	using string_view_base::rfind;

	using string_view_base::find_first_of;

	using string_view_base::find_last_of;

	using string_view_base::find_first_not_of;

	using string_view_base::find_last_not_of;

public:   /// Convertions ///
	constexpr operator const_pointer() const noexcept {
		return data();
	}

	// Ideally operator string_view_base()
	// but by language definition (C++17) will never be used:
	// From [class.conv.fct]: A conversion function is never used to convert a (possibly cv-qualified) object to the (possibly cv-qualified) same object type (or a reference to it), to a (possibly cv-qualified) base class of that type (or a reference to it), or to (possibly cv-qualified) void.
	constexpr string_view_base to_string_view() const noexcept {
		return { data(), size() };
	}

public:   /// Misc ///
	friend inline std::ostream& operator<<(std::ostream& o, basic_zstring_view v) {
		o << v.data();
		return o;
	}
};

using zstring_view = basic_zstring_view< char >;
using wzstring_view = basic_zstring_view< wchar_t >;
using u16zstring_view = basic_zstring_view< char16_t >;
using u32zstring_view = basic_zstring_view< char32_t >;

#endif