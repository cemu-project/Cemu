#include "Cafe/OS/libs/cemuextend/CemodPermission.h"

#include <cstdlib>
#include <iostream>

namespace
{
	[[noreturn]] void CheckFailed(const char* expression, int line)
	{
		std::cerr << "CHECK failed at line " << line << ": " << expression << '\n';
		std::abort();
	}

#define CHECK(condition) do { if (!(condition)) CheckFailed(#condition, __LINE__); } while (false)
}

int main()
{
	using cemuextend_hle::NeedsCemodPermissionPrompt;

	CHECK(!NeedsCemodPermissionPrompt(0x1fU, 0, 0, false));
	CHECK(!NeedsCemodPermissionPrompt(0, 0, 0, true));
	CHECK(!NeedsCemodPermissionPrompt(0x1fU, 0x1fU, 0x1fU, true));
	CHECK(NeedsCemodPermissionPrompt(0x1fU, 0x0fU, 0x1fU, true));
	CHECK(NeedsCemodPermissionPrompt(0x1fU, 0x1fU, 0x0fU, true));
	CHECK(!NeedsCemodPermissionPrompt(0x20U, 0, 0, true));
	return 0;
}
