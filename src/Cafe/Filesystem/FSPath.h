#pragma once
#include "Common/precompiled.h"

class FSPath : public fs::path {
 public:
	FSPath() = default;

	template <class T>
	FSPath(const T & other) : fs::path(other) {};

	template <class T>
	static FSPath Convert(const T& input) {
		FSPath p;
		p /= input;
		return p;
	}
	FSPath& operator/= (const FSPath & other);
};