#pragma once

// printf-style macros that are only active in non-release builds

#ifdef PUBLIC_RELEASE
#define debug_printf //
#define debug_puts //
static void debugBreakpoint() { }
#else
#define debug_printf printf
#define debug_puts puts
static void debugBreakpoint() {}
#endif