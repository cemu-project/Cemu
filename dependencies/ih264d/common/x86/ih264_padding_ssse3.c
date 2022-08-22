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
*  ih264_padding_atom_intr.c
*
* @brief
*  Contains function definitions for Padding
*
* @author
*  Srinivas T
*
* @par List of Functions:
*   - ih264_pad_left_luma_ssse3()
*   - ih264_pad_left_chroma_ssse3()
*   - ih264_pad_right_luma_ssse3()
*   - ih264_pad_right_chroma_ssse3()
*
* @remarks
*  None
*
*******************************************************************************
*/

#include <string.h>
#include <assert.h>
#include "ih264_typedefs.h"
#include "ih264_platform_macros.h"
#include "ih264_mem_fns.h"
#include "ih264_debug.h"

#include <immintrin.h>


/**
*******************************************************************************
*
* @brief
*   Padding (luma block) at the left of a 2d array
*
* @par Description:
*   The left column of a 2d array is replicated for pad_size times at the left
*
*
* @param[in] pu1_src
*  UWORD8 pointer to the source
*
* @param[in] src_strd
*  integer source stride
*
* @param[in] ht
*  integer height of the array
*
* @param[in] wd
*  integer width of the array
*
* @param[in] pad_size
*  integer -padding size of the array
*
* @param[in] ht
*  integer height of the array
*
* @param[in] wd
*  integer width of the array
*
* @returns
*
* @remarks
*  None
*
*******************************************************************************
*/

void ih264_pad_left_luma_ssse3(UWORD8 *pu1_src,
                               WORD32 src_strd,
                               WORD32 ht,
                               WORD32 pad_size)
{
    WORD32 row;
    WORD32 i;
    UWORD8 *pu1_dst;

    ASSERT(pad_size % 8 == 0);

    for(row = 0; row < ht; row++)
    {
        __m128i src_temp0_16x8b;

        pu1_dst = pu1_src - pad_size;
        src_temp0_16x8b = _mm_set1_epi8(*pu1_src);
        for(i = 0; i < pad_size; i += 8)
        {
            _mm_storel_epi64((__m128i *)(pu1_dst + i), src_temp0_16x8b);
        }
        pu1_src += src_strd;
    }

}



/**
*******************************************************************************
*
* @brief
*   Padding (chroma block) at the left of a 2d array
*
* @par Description:
*   The left column of a 2d array is replicated for pad_size times at the left
*
*
* @param[in] pu1_src
*  UWORD8 pointer to the source
*
* @param[in] src_strd
*  integer source stride
*
* @param[in] ht
*  integer height of the array
*
* @param[in] wd
*  integer width of the array (each colour component)
*
* @param[in] pad_size
*  integer -padding size of the array
*
* @param[in] ht
*  integer height of the array
*
* @param[in] wd
*  integer width of the array
*
* @returns
*
* @remarks
*  None
*
*******************************************************************************
*/

void ih264_pad_left_chroma_ssse3(UWORD8 *pu1_src,
                                 WORD32 src_strd,
                                 WORD32 ht,
                                 WORD32 pad_size)
{
    WORD32 row;
    WORD32 col;
    UWORD8 *pu1_dst;

    ASSERT(pad_size % 8 == 0);
    for(row = 0; row < ht; row++)
    {
        __m128i src_temp0_16x8b;

        pu1_dst = pu1_src - pad_size;
        src_temp0_16x8b = _mm_set1_epi16(*((UWORD16 *)pu1_src));
        for(col = 0; col < pad_size; col += 8)
        {
            _mm_storel_epi64((__m128i *)(pu1_dst + col), src_temp0_16x8b);
        }
        pu1_src += src_strd;
    }

}



/**
*******************************************************************************
*
* @brief
* Padding (luma block) at the right of a 2d array
*
* @par Description:
* The right column of a 2d array is replicated for pad_size times at the right
*
*
* @param[in] pu1_src
*  UWORD8 pointer to the source
*
* @param[in] src_strd
*  integer source stride
*
* @param[in] ht
*  integer height of the array
*
* @param[in] wd
*  integer width of the array
*
* @param[in] pad_size
*  integer -padding size of the array
*
* @param[in] ht
*  integer height of the array
*
* @param[in] wd
*  integer width of the array
*
* @returns
*
* @remarks
*  None
*
*******************************************************************************
*/

void ih264_pad_right_luma_ssse3(UWORD8 *pu1_src,
                                WORD32 src_strd,
                                WORD32 ht,
                                WORD32 pad_size)
{
    WORD32 row;
    WORD32 col;
    UWORD8 *pu1_dst;

    ASSERT(pad_size % 8 == 0);

    for(row = 0; row < ht; row++)
    {
        __m128i src_temp0_16x8b;

        pu1_dst = pu1_src;
        src_temp0_16x8b = _mm_set1_epi8(*(pu1_src - 1));
        for(col = 0; col < pad_size; col += 8)
        {
            _mm_storel_epi64((__m128i *)(pu1_dst + col), src_temp0_16x8b);
        }
        pu1_src += src_strd;
    }

}



/**
*******************************************************************************
*
* @brief
* Padding (chroma block) at the right of a 2d array
*
* @par Description:
* The right column of a 2d array is replicated for pad_size times at the right
*
*
* @param[in] pu1_src
*  UWORD8 pointer to the source
*
* @param[in] src_strd
*  integer source stride
*
* @param[in] ht
*  integer height of the array
*
* @param[in] wd
*  integer width of the array (each colour component)
*
* @param[in] pad_size
*  integer -padding size of the array
*
* @param[in] ht
*  integer height of the array
*
* @param[in] wd
*  integer width of the array
*
* @returns
*
* @remarks
*  None
*
*******************************************************************************
*/

void ih264_pad_right_chroma_ssse3(UWORD8 *pu1_src,
                                  WORD32 src_strd,
                                  WORD32 ht,
                                  WORD32 pad_size)
{
    WORD32 row;
    WORD32 col;
    UWORD8 *pu1_dst;

    ASSERT(pad_size % 8 == 0);

    for(row = 0; row < ht; row++)
    {
        __m128i src_temp0_16x8b;

        pu1_dst = pu1_src;
        src_temp0_16x8b = _mm_set1_epi16(*((UWORD16 *)(pu1_src - 2)));
        for(col = 0; col < pad_size; col += 8)
        {
            _mm_storel_epi64((__m128i *)(pu1_dst + col), src_temp0_16x8b);
        }

        pu1_src += src_strd;
    }
}

