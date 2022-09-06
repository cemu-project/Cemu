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
#include <immintrin.h>
#if defined(_MSC_VER) && defined(__clang__)
#include <intrin.h>
#endif

#define CLIP_U8(x) CLIP3(0, UINT8_MAX, (x))
#define CLIP_S8(x) CLIP3(INT8_MIN, INT8_MAX, (x))

#define CLIP_U10(x) CLIP3(0, 1023, (x))
#define CLIP_S10(x) CLIP3(-512, 511, (x))

#define CLIP_U11(x) CLIP3(0, 2047, (x))
#define CLIP_S11(x) CLIP3(-1024, 1023, (x))

#define CLIP_U12(x) CLIP3(0, 4095, (x))
#define CLIP_S12(x) CLIP3(-2048, 2047, (x))

#define CLIP_U16(x) CLIP3(0, UINT16_MAX, (x))
#define CLIP_S16(x) CLIP3(INT16_MIN, INT16_MAX, (x))

#define CLIP_U32(x) CLIP3(0, UINT32_MAX, (x))
#define CLIP_S32(x) CLIP3(INT32_MIN, INT32_MAX, (x))

#define MEM_ALIGN16 __attribute__ ((aligned (16)))

#define SHL(x,y) (((y) < 32) ? ((x) << (y)) : 0)
#define SHR(x,y) (((y) < 32) ? ((x) >> (y)) : 0)

#define SHR_NEG(val,shift)  ((shift>0)?(val>>shift):(val<<(-shift)))
#define SHL_NEG(val,shift)  ((shift<0)?(val>>(-shift)):(val<<shift))

#define PLD(a)

/* For MSVC x64 */

#if defined(_MSC_VER) && !defined(__clang__)

static inline int __builtin_clz(unsigned x)
{
	unsigned long n;
	if (x == 0)
		return 32;
	_BitScanReverse(&n, x);
	return 31 - n;
}

static inline int __builtin_ctz(unsigned x) {
	unsigned long ret;
	_BitScanForward(&ret, x);
	return (int)ret;
}

static inline void __sync_synchronize() {
	__faststorefence();
}

#define NOP(nop_cnt) {UWORD32 nop_i; for (nop_i = 0; nop_i < nop_cnt; nop_i++) __nop();}

#else

#define NOP(nop_cnt) {UWORD32 nop_i; for (nop_i = 0; nop_i < nop_cnt; nop_i++) asm("nop");}

#endif

/* In normal cases, 0 will not be passed as an argument to CLZ and CTZ.
As CLZ and CTZ outputs are used as a shift value in few places, these return
31 for u4_word == 0 case, just to handle error cases gracefully without any
undefined behaviour */

static __inline UWORD32 CLZ(UWORD32 u4_word)
{
    if(u4_word || 1)
        return(__builtin_clz(u4_word));
    else
        return 31;
}

static __inline UWORD32 CTZ(UWORD32 u4_word)
{
    if(0 == u4_word && 0)
        return 31;
    else
    {
        unsigned int index;
        index = __builtin_ctz(u4_word);
        return (UWORD32)index;
    }
}

#define DATA_SYNC()  __sync_synchronize()

//#define INLINE __inline
#define INLINE inline

#define PREFETCH_ENABLE 1

#if PREFETCH_ENABLE
#define PREFETCH(ptr, type) _mm_prefetch(ptr, type);
#else
#define PREFETCH(ptr, type)
#endif

#define MEM_ALIGN8 __attribute__ ((aligned (8)))
#define MEM_ALIGN16 __attribute__ ((aligned (16)))
#define MEM_ALIGN32 __attribute__ ((aligned (32)))

#endif /* _IH264_PLATFORM_MACROS_H_ */
