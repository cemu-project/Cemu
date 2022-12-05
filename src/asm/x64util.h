#pragma once

#if defined(__x86_64__)

extern "C" void recompiler_fres();
extern "C" void recompiler_frsqrte();

#else

// stubbed on non-x86 for now
static void recompiler_fres() {}
static void recompiler_frsqrte() {}

#endif
