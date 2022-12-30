#pragma once

#if defined(ARCH_X86_64)

extern "C" void recompiler_fres();
extern "C" void recompiler_frsqrte();

#else

// stubbed on non-x86 for now
static void recompiler_fres() 
{
	cemu_assert_unimplemented();
}
static void recompiler_frsqrte() 
{
	cemu_assert_unimplemented(); 
}

#endif
