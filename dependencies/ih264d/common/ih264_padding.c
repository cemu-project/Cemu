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
*  ih264_padding.c
*
* @brief
*  Contains function definitions for Padding
*
* @author
*  Ittiam
*
* @par List of Functions:
*   - ih264_pad_top()
*   - ih264_pad_bottom()
*   - ih264_pad_left_luma()
*   - ih264_pad_left_chroma()
*   - ih264_pad_right_luma()
*   - ih264_pad_right_chroma()
*
* @remarks
*  None
*
*******************************************************************************
*/

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

/* System include files */
#include <stddef.h>
#include <string.h>

/* User include files */
#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_padding.h"


/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/**
*******************************************************************************
*
* @brief pad at the top of a 2d array
*
* @par Description:
*  The top row of a 2d array is replicated for pad_size times at the top
*
* @param[in] pu1_src
*  UWORD8 pointer to the source
*
* @param[in] src_strd
*  integer source stride
*
* @param[in] wd
*  integer width of the array
*
* @param[in] pad_size
*  integer -padding size of the array
*
* @returns none
*
* @remarks none
*
*******************************************************************************
*/
void ih264_pad_top(UWORD8 *pu1_src,
                   WORD32 src_strd,
                   WORD32 wd,
                   WORD32 pad_size)
{
    WORD32 row;

    for(row = 1; row <= pad_size; row++)
    {
        memcpy(pu1_src - row * src_strd, pu1_src, wd);
    }
}



/**
*******************************************************************************
*
* @brief pad at the bottom of a 2d array
*
* @par Description:
*  The bottom row of a 2d array is replicated for pad_size times at the bottom
*
* @param[in] pu1_src
*  UWORD8 pointer to the source
*
* @param[in] src_strd
*  integer source stride
*
* @param[in] wd
*  integer width of the array
*
* @param[in] pad_size
*  integer -padding size of the array
*
* @returns none
*
* @remarks none
*
*******************************************************************************
*/
void ih264_pad_bottom(UWORD8 *pu1_src,
                      WORD32 src_strd,
                      WORD32 wd,
                      WORD32 pad_size)
{
    WORD32 row;

    for(row = 1; row <= pad_size; row++)
    {
        memcpy(pu1_src + (row - 1) * src_strd, pu1_src - 1 * src_strd, wd);
    }
}

/**
*******************************************************************************
*
* @brief pad (luma block) at the left of a 2d array
*
* @par Description:
*   The left column of a 2d array is replicated for pad_size times to the left
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
* @param[in] pad_size
*  integer -padding size of the array
*
* @returns none
*
* @remarks none
*
*******************************************************************************
 */
void ih264_pad_left_luma(UWORD8 *pu1_src,
                         WORD32 src_strd,
                         WORD32 ht,
                         WORD32 pad_size)
{
    WORD32 row;

    for(row = 0; row < ht; row++)
    {

        memset(pu1_src - pad_size, *pu1_src, pad_size);

        pu1_src += src_strd;
    }
}

/**
*******************************************************************************
*
* @brief pad (chroma block) at the left of a 2d array
*
* @par Description:
*   The left column of a 2d array is replicated for pad_size times to the left
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
* @param[in] pad_size
*  integer -padding size of the array
*
* @returns none
*
* @remarks none
*
*******************************************************************************
*/
void ih264_pad_left_chroma(UWORD8 *pu1_src,
                           WORD32 src_strd,
                           WORD32 ht,
                           WORD32 pad_size)
{
    /* temp var */
    WORD32 row, col;
    UWORD16 u2_uv_val;

    /* pointer to src */
    UWORD16 *pu2_src = (UWORD16 *)pu1_src;

    src_strd >>= 1;
    pad_size >>= 1;

    for(row = 0; row < ht; row++)
    {
        u2_uv_val = pu2_src[0];

        for (col = -pad_size; col < 0; col++)
        {
            pu2_src[col] = u2_uv_val;
        }

        pu2_src += src_strd;
    }
}

/**
*******************************************************************************
*
* @brief pad (luma block) at the right of a 2d array
*
* @par Description:
*  The right column of a 2d array is replicated for pad_size times at the right
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
* @param[in] pad_size
*  integer -padding size of the array
*
* @returns none
*
* @remarks none
*
*******************************************************************************
*/
void ih264_pad_right_luma(UWORD8 *pu1_src,
                          WORD32 src_strd,
                          WORD32 ht,
                          WORD32 pad_size)
{
    WORD32 row;

    for(row = 0; row < ht; row++)
    {
        memset(pu1_src, *(pu1_src -1), pad_size);

        pu1_src += src_strd;
    }
}

/**
*******************************************************************************
*
* @brief pad (chroma block) at the right of a 2d array
*
* @par Description:
*  The right column of a 2d array is replicated for pad_size times at the right
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
* @param[in] pad_size
*  integer -padding size of the array
*
* @returns none
*
* @remarks none
*
*******************************************************************************
*/
void ih264_pad_right_chroma(UWORD8 *pu1_src,
                            WORD32 src_strd,
                            WORD32 ht,
                            WORD32 pad_size)
{
    WORD32 row, col;
    UWORD16 u2_uv_val;
    UWORD16 *pu2_src = (UWORD16 *)pu1_src;

    src_strd >>= 1;
    pad_size >>= 1;

    for(row = 0; row < ht; row++)
    {
        u2_uv_val = pu2_src[-1];

        for (col = 0; col < pad_size; col++)
        {
            pu2_src[col] = u2_uv_val;
        }

        pu2_src += src_strd;
    }
}

