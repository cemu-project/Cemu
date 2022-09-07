#pragma once
#include "Common/precompiled.h"

class FSPath : public fs::path {
 public:
	FSPath() = default;

	template <class T>
	FSPath(const T& other) : fs::path(other) {};

#ifdef BOOST_OS_UNIX
	template <class T>
	static FSPath Convert(const T& input)
	{
		return FSPath{} / FSPath{input};
	}
	FSPath& operator/= (const FSPath & other);
	FSPath& operator/ (const FSPath & other);
#endif
};