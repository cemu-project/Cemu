#pragma once

#include "util/helpers/helpers.h"

class SystemException : public std::runtime_error
{
public:
	SystemException()
		: std::runtime_error(GetSystemErrorMessage().c_str()), m_error_code(GetExceptionError())
	{}

	SystemException(const std::exception& ex)
		: std::runtime_error(GetSystemErrorMessage(ex).c_str()), m_error_code(GetExceptionError())
	{}
	
	SystemException(const std::error_code& ec)
		: std::runtime_error(GetSystemErrorMessage(ec).c_str()), m_error_code(GetExceptionError())
	{}

	[[nodiscard]] DWORD GetErrorCode() const { return m_error_code; }
private:
	DWORD m_error_code;
};
