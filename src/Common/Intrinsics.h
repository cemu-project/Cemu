#pragma once
#include <cstdint>

#if defined(_MSC_VER)
    #include <intrin.h>
    #if defined(_M_ARM64)
        #include <arm64intr.h>
        #define BARRIER_FENCE() __dmb(_ARM64_BARRIER_ISH)
        #define READ_TSC()      _ReadStatusReg(ARM64_CNTVCT_EL0)
    #else
        #include <immintrin.h>
        #define BARRIER_FENCE() _mm_mfence()
        #define READ_TSC()      __rdtsc()
    #endif
#else 
    #if defined(__aarch64__)
        #define BARRIER_FENCE() __asm__ __volatile__ ("dmb ish" : : : "memory")
        static inline uint64_t READ_TSC() {
            uint64_t val;
            __asm__ __volatile__("mrs %0, cntvct_el0" : "=r" (val));
            return val;
        }
    #else
        #include <x86intrin.h>
        #define BARRIER_FENCE() __asm__ __volatile__ ("mfence" : : : "memory")
        #define READ_TSC()      __rdtsc()
    #endif
#endif

static inline uint64_t Multiply64to128(uint64_t a, uint64_t b, uint64_t* high) {
#if defined(__SIZEOF_INT128__)
    unsigned __int128 res = (unsigned __int128)a * b;
    *high = (uint64_t)(res >> 64);
    return (uint64_t)res;
#elif defined(_MSC_VER) && defined(_M_X64)
    return _umul128(a, b, high);
#elif defined(_MSC_VER) && defined(_M_ARM64)
    *high = __umulh(a, b);
    return a * b;
#else
    // Generic fallback for other compilers/archs
    uint64_t a_lo = (uint32_t)a, a_hi = a >> 32;
    uint64_t b_lo = (uint32_t)b, b_hi = b >> 32;
    uint64_t p0 = a_lo * b_lo;
    uint64_t p1 = a_lo * b_hi;
    uint64_t p2 = a_hi * b_lo;
    uint64_t p3 = a_hi * b_hi;
    uint64_t cy = (uint32_t)(p0 >> 32) + (uint32_t)p1 + (uint32_t)p2;
    *high = p3 + (p1 >> 32) + (p2 >> 32) + (cy >> 32);
    return (p0 & 0xFFFFFFFF) | (cy << 32);
#endif
}

static inline uint64_t Divide128by64(uint64_t high, uint64_t low, uint64_t divisor, uint64_t* remainder) {
#if defined(__SIZEOF_INT128__)
    unsigned __int128 dividend = ((unsigned __int128)high << 64) | low;
    *remainder = (uint64_t)(dividend % divisor);
    return (uint64_t)(dividend / divisor);
#elif defined(_MSC_VER) && defined(_M_X64)
    return _udiv128(high, low, divisor, remainder);
#else
    // Software fallback for MSVC ARM64 and others
    // This implements long division for 128-bit / 64-bit
    if (high < divisor) {
        // Common case for many emulators: quotient fits in 64 bits
        uint64_t q, r;
        // Schoolbook division: split 128-bit into 4x32-bit if necessary, 
        // but here we use a binary long division for simplicity and correctness.
        uint64_t rem = high;
        uint64_t quot = 0;
        for (int i = 63; i >= 0; i--) {
            rem = (rem << 1) | ((low >> i) & 1);
            if (rem >= divisor) {
                rem -= divisor;
                quot |= (1ULL << i);
            }
        }
        *remainder = rem;
        return quot;
    } else {
        // Quotient overflow: high >= divisor means result > 64 bits.
        // If your code expects a 64-bit return, this is technically an error state.
        // Return max value as a sentinel or handle as needed.
        *remainder = 0; 
        return 0xFFFFFFFFFFFFFFFFULL;
    }
#endif
}
