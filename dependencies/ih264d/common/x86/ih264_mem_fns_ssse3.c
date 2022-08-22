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
 *  ih264_mem_fns_atom_intr.c
 *
 * @brief
 *  Functions used for memory operations
 *
 * @author
 *  Ittiam
 *
 * @par List of Functions:
 *
 * @remarks
 *  None
 *
 *******************************************************************************
 */

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "ih264_typedefs.h"
#include "ih264_mem_fns.h"

#include <immintrin.h>

/**
 *******************************************************************************
 *
 * @brief
 *   memcpy of a 8,16 or 32 bytes
 *
 * @par Description:
 *   Does memcpy of 8bit data from source to destination for 8,16 or 32 number of bytes
 *
 * @param[in] pu1_dst
 *  UWORD8 pointer to the destination
 *
 * @param[in] pu1_src
 *  UWORD8 pointer to the source
 *
 * @param[in] num_bytes
 *  number of bytes to copy
 * @returns
 *
 * @remarks
 *  None
 *
 *******************************************************************************
 */




void ih264_memcpy_mul_8_ssse3(UWORD8 *pu1_dst, UWORD8 *pu1_src, UWORD32 num_bytes)
{
    int col;
    for(col = num_bytes; col >= 8; col -= 8)
    {
        __m128i src_temp16x8b;
        src_temp16x8b = _mm_loadl_epi64((__m128i *)(pu1_src));
        pu1_src += 8;
        _mm_storel_epi64((__m128i *)(pu1_dst), src_temp16x8b);
        pu1_dst += 8;
    }
}

/**
 *******************************************************************************
 *
 * @brief
 *   memset of a 8,16 or 32 bytes
 *
 * @par Description:
 *   Does memset of 8bit data for 8,16 or 32 number of bytes
 *
 * @param[in] pu1_dst
 *  UWORD8 pointer to the destination
 *
 * @param[in] value
 *  UWORD8 value used for memset
 *
 * @param[in] num_bytes
 *  number of bytes to set
 * @returns
 *
 * @remarks
 *  None
 *
 *******************************************************************************
 */


void ih264_memset_mul_8_ssse3(UWORD8 *pu1_dst, UWORD8 value, UWORD32 num_bytes)
{
    int col;
    __m128i src_temp16x8b;
    src_temp16x8b = _mm_set1_epi8(value);
    for(col = num_bytes; col >= 8; col -= 8)
    {
        _mm_storel_epi64((__m128i *)(pu1_dst), src_temp16x8b);
        pu1_dst += 8;
    }
}

/**
 *******************************************************************************
 *
 * @brief
 *   memset of 16bit data of a 8,16 or 32 bytes
 *
 * @par Description:
 *   Does memset of 16bit data for 8,16 or 32 number of bytes
 *
 * @param[in] pu2_dst
 *  UWORD8 pointer to the destination
 *
 * @param[in] value
 *  UWORD16 value used for memset
 *
 * @param[in] num_words
 *  number of words to set
 * @returns
 *
 * @remarks
 *  None
 *
 *******************************************************************************
 */


void ih264_memset_16bit_mul_8_ssse3(UWORD16 *pu2_dst, UWORD16 value, UWORD32 num_words)
{
    int col;
    __m128i src_temp16x8b;
    src_temp16x8b = _mm_set1_epi16(value);
    for(col = num_words; col >= 8; col -= 8)
    {
        _mm_storeu_si128((__m128i *)(pu2_dst), src_temp16x8b);
        pu2_dst += 8;
    }
}

