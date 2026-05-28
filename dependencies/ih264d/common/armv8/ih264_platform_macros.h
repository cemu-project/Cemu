/******************************************************************************
 *
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *****************************************************************************
 * Originally developed and contributed by Ittiam Systems Pvt. Ltd, Bangalore
*/
/**
*******************************************************************************
* @file
*  ih264_platform_macros.h
*
* @brief
*  Platform specific Macro definitions used in the codec
*
* @author
*  Ittiam
*
* @remarks
*  None
*
*******************************************************************************
*/
#ifndef _IH264_PLATFORM_MACROS_H_
#define _IH264_PLATFORM_MACROS_H_

#include <stdint.h>

#ifndef WORD32
typedef int32_t  WORD32;
#endif
#ifndef UWORD32
typedef uint32_t UWORD32;
#endif

#include <arm_neon.h>

#ifdef _MSC_VER
    #include <intrin.h>
    
    #if defined(_M_ARM64) && !defined(__arm64_intrinsics_h__)
        #ifndef _ARM64_BARRIER_ISH
            #define _ARM64_BARRIER_ISH 0xB 
        #endif
    #endif

    #define INLINE      __inline
    #define MEM_ALIGN8  __declspec(align(8))
    #define MEM_ALIGN16 __declspec(align(16))
    #define MEM_ALIGN32 __declspec(align(32))
    
    #define DATA_SYNC() __dmb(_ARM64_BARRIER_ISH)
    
    #define NOP(nop_cnt) { for (int i = 0; i < (int)nop_cnt; i++) __nop(); }
    
    #define ITT_BIG_ENDIAN(x) _byteswap_ulong(x)
#else
    #define INLINE      inline
    #define MEM_ALIGN8  __attribute__ ((aligned (8)))
    #define MEM_ALIGN16 __attribute__ ((aligned (16)))
    #define MEM_ALIGN32 __attribute__ ((aligned (32)))
    
    #define DATA_SYNC() __sync_synchronize()
    
    #define NOP(nop_cnt)                                                \
    {                                                                   \
        uint32_t nop_i;                                                 \
        for (nop_i = 0; nop_i < nop_cnt; nop_i++)                       \
            __asm__ __volatile__("mov x0, x0");                         \
    }

    static INLINE uint32_t ITT_BIG_ENDIAN(uint32_t x) {
        return __builtin_bswap32(x);
    }
#endif

#ifndef CLIP3
#define CLIP3(min_val, max_val, x) (((x) < (min_val)) ? (min_val) : (((x) > (max_val)) ? (max_val) : (x)))
#endif

#define CLIP_U8(x)  CLIP3(0, 255, (x))
#define CLIP_S8(x)  CLIP3(-128, 127, (x))
#define CLIP_U10(x) CLIP3(0, 1023, (x))
#define CLIP_S10(x) CLIP3(-512, 511, (x))
#define CLIP_U11(x) CLIP3(0, 2047, (x))
#define CLIP_S11(x) CLIP3(-1024, 1023, (x))
#define CLIP_U12(x) CLIP3(0, 4095, (x))
#define CLIP_S12(x) CLIP3(-2048, 2047, (x))
#define CLIP_U16(x) CLIP3(0, 65535, (x))
#define CLIP_S16(x) CLIP3(-32768, 32767, (x))
#define CLIP_U32(x) CLIP3(0, 0xFFFFFFFF, (x))
#define CLIP_S32(x) CLIP3(-2147483647-1, 2147483647, (x))

static INLINE uint32_t CLZ(uint32_t u4_word) {
    if (u4_word == 0) return 31;
#ifdef _MSC_VER
    return (uint32_t)_CountLeadingZeros(u4_word);
#else
    return (uint32_t)__builtin_clz(u4_word);
#endif
}

static INLINE uint32_t CTZ(uint32_t u4_word) {
    if (u4_word == 0) return 31;
#ifdef _MSC_VER
    unsigned long index;
    _BitScanForward(&index, u4_word);
    return (uint32_t)index;
#else
    return (uint32_t)__builtin_ctz(u4_word);
#endif
}

#define SHL(x,y) (((y) < 32) ? ((uint32_t)(x) << (y)) : 0)
#define SHR(x,y) (((y) < 32) ? ((uint32_t)(x) >> (y)) : 0)

#define SHR_NEG(val,shift)  ((shift>0)?(val>>shift):(val<<(-shift)))
#define SHL_NEG(val,shift)  ((shift<0)?(val>>(-shift)):(val<<shift))

#endif /* _IH264_PLATFORM_MACROS_H_ */
