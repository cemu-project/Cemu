#pragma once

extern "C" uint64 ATTR_MS_ABI udiv128(uint64 low, uint64 hi, uint64 divisor, uint64* remainder);
extern "C" void recompiler_fres();
extern "C" void recompiler_frsqrte();
