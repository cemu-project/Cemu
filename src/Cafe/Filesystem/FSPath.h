#pragma once
#include "Common/precompiled.h"

class FSPath : public fs::path {
 public:
	FSPath() = default;

	template <class T>
	FSPath(const T& other) : fs::path(other) {};

	template <class T>
	static FSPath Convert(const T& input)
	{
		return FSPath{} / FSPath{input};
	}
#ifdef BOOST_OS_UNIX
	FSPath& operator/= (const FSPath & rhs);
	FSPath& operator/ (const FSPath & rhs);
#endif
};

#ifdef BOOST_OS_UNIX
FSPath operator/ (const FSPath& lhs, const FSPath& rhs);
#endif
