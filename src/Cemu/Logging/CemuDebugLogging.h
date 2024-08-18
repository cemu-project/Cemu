#pragma once

// printf-style macros that are only active in non-release builds

#ifndef CEMU_DEBUG_ASSERT
#define debug_printf(...)
static void debugBreakpoint() { }
#else
#define debug_printf(...) printf(__VA_ARGS__)
static void debugBreakpoint() {}
#endif