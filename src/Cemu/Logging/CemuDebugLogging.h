#pragma once

// printf-style macros that are only active in non-release builds

#ifdef PUBLIC_RELEASE
#define debug_printf(...)
static void debugBreakpoint() { }
#else
#define debug_printf(...) printf(__VA_ARGS__)
static void debugBreakpoint() {}
#endif