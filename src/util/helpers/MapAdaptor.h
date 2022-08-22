#pragma once

// https://ideone.com/k0H8Ei

#include <iostream>
#include <map>
#include <boost/iterator/transform_iterator.hpp>

template <typename T, typename F>
struct map_adaptor
{
	map_adaptor(const T& t, const F& f) : _t(t), _f(f) {}
	map_adaptor(map_adaptor& a) = delete;
	map_adaptor(map_adaptor&& a) = default;

	[[nodiscard]] auto begin() { return boost::make_transform_iterator(_t.begin(), _f); }
	[[nodiscard]] auto end() { return boost::make_transform_iterator(_t.end(), _f); }

	[[nodiscard]] auto cbegin() const { return boost::make_transform_iterator(_t.cbegin(), _f); }
	[[nodiscard]] auto cend() const { return boost::make_transform_iterator(_t.cend(), _f); }

protected:
	const T& _t;
	F _f;
};


template <typename T, typename F>
auto get_map_adaptor(const T& t, const F& f) { return map_adaptor<T, F>(t, f); }

template <typename T>
auto get_keys(const T& t) { return get_map_adaptor(t, [](const auto& p) { return p.first; }); }

template <typename T>
auto get_values(const T& t) { return get_map_adaptor(t, [](const auto& p) { return p.second; }); }
