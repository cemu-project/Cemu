#pragma once


extern "C" void recompiler_fres();
extern "C" void recompiler_frsqrte();
// #if defined(ARCH_X86_64)

// #else
// // stubbed on non-x86 for now
// static void recompiler_frsqrte() 
// {
// 	cemu_assert_unimplemented(); 
// }
// #endif
