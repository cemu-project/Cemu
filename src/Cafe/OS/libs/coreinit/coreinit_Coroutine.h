#pragma once

namespace coreinit
{
	struct OSCoroutine
	{
		/* +0x00 */ uint32 lr;
		/* +0x04 */ uint32 cr;
		/* +0x08 */ uint32 gqr1;
		/* +0x0C */ uint32 r1;  // stack pointer
		/* +0x10 */ uint32 r2;
		/* +0x14 */ uint32 r13;
		/* +0x18 */ uint32 gpr[18];
		uint64 fpr[18];
		uint64 psr[18];
	};

	static_assert(sizeof(OSCoroutine) == 0x180);

	void InitializeCoroutine();
}
