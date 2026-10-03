#pragma once

struct OSThread_t;

namespace coreinit
{
	void PrepareGHSRuntime();
	void __OSTLSFreeForThread(OSThread_t* thread);

	sint32be* __gh_errno_ptr();
	void __gh_set_errno(sint32 errNo);
	sint32 __gh_get_errno();

	void InitializeGHS();
};