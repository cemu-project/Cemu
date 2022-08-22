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
/*****************************************************************************/
/*                                                                           */
/*  File Name         : ih264_weighted_pred_intr_sse42.c                     */
/*                                                                           */
/*  Description       : Contains function definitions for weighted           */
/*                      prediction functions in x86 sse4 intrinsics          */
/*                                                                           */
/*  List of Functions : ih264_default_weighted_pred_luma_sse42()             */
/*                      ih264_default_weighted_pred_chroma_sse42()           */
/*                      ih264_weighted_pred_luma_sse42()                     */
/*                      ih264_weighted_pred_chroma_sse42()                   */
/*                      ih264_weighted_bipred_luma_sse42()                   */
/*                      ih264_weighted_bipred_chroma_sse42()                 */
/*                                                                           */
/*  Issues / Problems : None                                                 */
/*                                                                           */
/*  Revision History  :                                                      */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes                              */
/*         30 01 2015   Kaushik         Initial version                      */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

#include <immintrin.h>
#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264_weighted_pred.h"

/*****************************************************************************/
/*  Function definitions .                                                   */
/*****************************************************************************/
/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_default_weighted_pred_luma_sse42                   */
/*                                                                           */
/*  Description   : This function performs the default weighted prediction   */
/*                  as described in sec 8.4.2.3.1 titled "Default weighted   */
/*                  sample prediction process" for luma. The function gets   */
/*                  two ht x wd blocks, calculates their rounded-average and */
/*                  stores it in the destination block. (ht,wd) can be       */
/*                  (4,4), (8,4), (4,8), (8,8), (16,8), (8,16) or (16,16).   */
/*                                                                           */
/*  Inputs        : pu1_src1  - Pointer to source 1                          */
/*                  pu1_src2  - Pointer to source 2                          */
/*                  pu1_dst   - Pointer to destination                       */
/*                  src_strd1 - stride for source 1                          */
/*                  src_strd1 - stride for source 2                          */
/*                  dst_strd  - stride for destination                       */
/*                  ht        - height of the block                          */
/*                  wd        - width of the block                           */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes                              */
/*         04 02 2015   Kaushik         Initial Version                      */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_default_weighted_pred_luma_sse42(UWORD8 *pu1_src1,
                                            UWORD8 *pu1_src2,
                                            UWORD8 *pu1_dst,
                                            WORD32 src_strd1,
                                            WORD32 src_strd2,
                                            WORD32 dst_strd,
                                            WORD32 ht,
                                            WORD32 wd)
{
    __m128i y0_0_16x8b, y0_1_16x8b, y0_2_16x8b, y0_3_16x8b;
    __m128i y1_0_16x8b, y1_1_16x8b, y1_2_16x8b, y1_3_16x8b;

    if(wd == 4)
    {
        do
        {
            y0_0_16x8b = _mm_loadl_epi64((__m128i *)pu1_src1);
            y0_1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src1 + src_strd1));
            y0_2_16x8b = _mm_loadl_epi64(
                            (__m128i *)(pu1_src1 + (src_strd1 << 1)));
            y0_3_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src1 + src_strd1 * 3));

            y1_0_16x8b = _mm_loadl_epi64((__m128i *)pu1_src2);
            y1_1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src2 + src_strd2));
            y1_2_16x8b = _mm_loadl_epi64(
                            (__m128i *)(pu1_src2 + (src_strd2 << 1)));
            y1_3_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src2 + src_strd2 * 3));

            y0_0_16x8b = _mm_avg_epu8(y0_0_16x8b, y1_0_16x8b);
            y0_1_16x8b = _mm_avg_epu8(y0_1_16x8b, y1_1_16x8b);
            y0_2_16x8b = _mm_avg_epu8(y0_2_16x8b, y1_2_16x8b);
            y0_3_16x8b = _mm_avg_epu8(y0_3_16x8b, y1_3_16x8b);

            *((WORD32 *)(pu1_dst)) = _mm_cvtsi128_si32(y0_0_16x8b);
            *((WORD32 *)(pu1_dst + dst_strd)) = _mm_cvtsi128_si32(y0_1_16x8b);
            *((WORD32 *)(pu1_dst + (dst_strd << 1))) = _mm_cvtsi128_si32(y0_2_16x8b);
            *((WORD32 *)(pu1_dst + dst_strd * 3)) = _mm_cvtsi128_si32(y0_3_16x8b);

            ht -= 4;
            pu1_src1 += src_strd1 << 2;
            pu1_src2 += src_strd2 << 2;
            pu1_dst += dst_strd << 2;
        }
        while(ht > 0);
    }
    else if(wd == 8)
    {
        do
        {
            y0_0_16x8b = _mm_loadl_epi64((__m128i *)pu1_src1);
            y0_1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src1 + src_strd1));
            y0_2_16x8b = _mm_loadl_epi64(
                            (__m128i *)(pu1_src1 + (src_strd1 << 1)));
            y0_3_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src1 + src_strd1 * 3));

            y1_0_16x8b = _mm_loadl_epi64((__m128i *)pu1_src2);
            y1_1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src2 + src_strd2));
            y1_2_16x8b = _mm_loadl_epi64(
                            (__m128i *)(pu1_src2 + (src_strd2 << 1)));
            y1_3_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src2 + src_strd2 * 3));

            y0_0_16x8b = _mm_avg_epu8(y0_0_16x8b, y1_0_16x8b);
            y0_1_16x8b = _mm_avg_epu8(y0_1_16x8b, y1_1_16x8b);
            y0_2_16x8b = _mm_avg_epu8(y0_2_16x8b, y1_2_16x8b);
            y0_3_16x8b = _mm_avg_epu8(y0_3_16x8b, y1_3_16x8b);

            _mm_storel_epi64((__m128i *)pu1_dst, y0_0_16x8b);
            _mm_storel_epi64((__m128i *)(pu1_dst + dst_strd), y0_1_16x8b);
            _mm_storel_epi64((__m128i *)(pu1_dst + (dst_strd << 1)), y0_2_16x8b);
            _mm_storel_epi64((__m128i *)(pu1_dst + dst_strd * 3), y0_3_16x8b);

            ht -= 4;
            pu1_src1 += src_strd1 << 2;
            pu1_src2 += src_strd2 << 2;
            pu1_dst += dst_strd << 2;
        }
        while(ht > 0);
    }
    else // wd == 16
    {
        __m128i y0_4_16x8b, y0_5_16x8b, y0_6_16x8b, y0_7_16x8b;
        __m128i y1_4_16x8b, y1_5_16x8b, y1_6_16x8b, y1_7_16x8b;

        do
        {
            y0_0_16x8b = _mm_loadu_si128((__m128i *)pu1_src1);
            y0_1_16x8b = _mm_loadu_si128((__m128i *)(pu1_src1 + src_strd1));
            y0_2_16x8b = _mm_loadu_si128(
                            (__m128i *)(pu1_src1 + (src_strd1 << 1)));
            y0_3_16x8b = _mm_loadu_si128((__m128i *)(pu1_src1 + src_strd1 * 3));
            y0_4_16x8b = _mm_loadu_si128(
                            (__m128i *)(pu1_src1 + (src_strd1 << 2)));
            y0_5_16x8b = _mm_loadu_si128((__m128i *)(pu1_src1 + src_strd1 * 5));
            y0_6_16x8b = _mm_loadu_si128((__m128i *)(pu1_src1 + src_strd1 * 6));
            y0_7_16x8b = _mm_loadu_si128((__m128i *)(pu1_src1 + src_strd1 * 7));

            y1_0_16x8b = _mm_loadu_si128((__m128i *)pu1_src2);
            y1_1_16x8b = _mm_loadu_si128((__m128i *)(pu1_src2 + src_strd2));
            y1_2_16x8b = _mm_loadu_si128(
                            (__m128i *)(pu1_src2 + (src_strd2 << 1)));
            y1_3_16x8b = _mm_loadu_si128((__m128i *)(pu1_src2 + src_strd2 * 3));
            y1_4_16x8b = _mm_loadu_si128(
                            (__m128i *)(pu1_src2 + (src_strd2 << 2)));
            y1_5_16x8b = _mm_loadu_si128((__m128i *)(pu1_src2 + src_strd2 * 5));
            y1_6_16x8b = _mm_loadu_si128((__m128i *)(pu1_src2 + src_strd2 * 6));
            y1_7_16x8b = _mm_loadu_si128((__m128i *)(pu1_src2 + src_strd2 * 7));

            y0_0_16x8b = _mm_avg_epu8(y0_0_16x8b, y1_0_16x8b);
            y0_1_16x8b = _mm_avg_epu8(y0_1_16x8b, y1_1_16x8b);
            y0_2_16x8b = _mm_avg_epu8(y0_2_16x8b, y1_2_16x8b);
            y0_3_16x8b = _mm_avg_epu8(y0_3_16x8b, y1_3_16x8b);
            y0_4_16x8b = _mm_avg_epu8(y0_4_16x8b, y1_4_16x8b);
            y0_5_16x8b = _mm_avg_epu8(y0_5_16x8b, y1_5_16x8b);
            y0_6_16x8b = _mm_avg_epu8(y0_6_16x8b, y1_6_16x8b);
            y0_7_16x8b = _mm_avg_epu8(y0_7_16x8b, y1_7_16x8b);

            _mm_storeu_si128((__m128i *)pu1_dst, y0_0_16x8b);
            _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), y0_1_16x8b);
            _mm_storeu_si128((__m128i *)(pu1_dst + (dst_strd << 1)), y0_2_16x8b);
            _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd * 3), y0_3_16x8b);
            _mm_storeu_si128((__m128i *)(pu1_dst + (dst_strd << 2)), y0_4_16x8b);
            _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd * 5), y0_5_16x8b);
            _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd * 6), y0_6_16x8b);
            _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd * 7), y0_7_16x8b);

            ht -= 8;
            pu1_src1 += src_strd1 << 3;
            pu1_src2 += src_strd2 << 3;
            pu1_dst += dst_strd << 3;
        }
        while(ht > 0);
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_default_weighted_pred_chroma_sse42                 */
/*                                                                           */
/*  Description   : This function performs the default weighted prediction   */
/*                  as described in sec 8.4.2.3.1 titled "Default weighted   */
/*                  sample prediction process" for chroma. The function gets */
/*                  two ht x wd blocks, calculates their rounded-average and */
/*                  stores it in the destination block. (ht,wd) can be       */
/*                  (2,2), (4,2) , (2,4), (4,4), (8,4), (4,8) or (8,8).      */
/*                                                                           */
/*  Inputs        : pu1_src1  - Pointer to source 1                          */
/*                  pu1_src2  - Pointer to source 2                          */
/*                  pu1_dst   - Pointer to destination                       */
/*                  src_strd1 - stride for source 1                          */
/*                  src_strd1 - stride for source 2                          */
/*                  dst_strd  - stride for destination                       */
/*                  ht        - height of the block                          */
/*                  wd        - width of the block                           */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes                              */
/*         04 02 2015   Kaushik         Initial Version                      */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_default_weighted_pred_chroma_sse42(UWORD8 *pu1_src1,
                                              UWORD8 *pu1_src2,
                                              UWORD8 *pu1_dst,
                                              WORD32 src_strd1,
                                              WORD32 src_strd2,
                                              WORD32 dst_strd,
                                              WORD32 ht,
                                              WORD32 wd)
{
    __m128i uv0_0_16x8b, uv0_1_16x8b;
    __m128i uv1_0_16x8b, uv1_1_16x8b;

    if(wd == 2)
    {
        do
        {
            uv0_0_16x8b = _mm_loadl_epi64((__m128i *)pu1_src1);
            uv0_1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src1 + src_strd1));

            uv1_0_16x8b = _mm_loadl_epi64((__m128i *)pu1_src2);
            uv1_1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src2 + src_strd2));

            uv0_0_16x8b = _mm_avg_epu8(uv0_0_16x8b, uv1_0_16x8b);
            uv0_1_16x8b = _mm_avg_epu8(uv0_1_16x8b, uv1_1_16x8b);

            *((WORD32 *)(pu1_dst)) = _mm_cvtsi128_si32(uv0_0_16x8b);
            *((WORD32 *)(pu1_dst + dst_strd)) = _mm_cvtsi128_si32(uv0_1_16x8b);

            ht -= 2;
            pu1_src1 += src_strd1 << 1;
            pu1_src2 += src_strd2 << 1;
            pu1_dst += dst_strd << 1;
        }
        while(ht > 0);
    }
    else if(wd == 4)
    {
        do
        {
            uv0_0_16x8b = _mm_loadl_epi64((__m128i *)pu1_src1);
            uv0_1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src1 + src_strd1));

            uv1_0_16x8b = _mm_loadl_epi64((__m128i *)pu1_src2);
            uv1_1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src2 + src_strd2));

            uv0_0_16x8b = _mm_avg_epu8(uv0_0_16x8b, uv1_0_16x8b);
            uv0_1_16x8b = _mm_avg_epu8(uv0_1_16x8b, uv1_1_16x8b);

            _mm_storel_epi64((__m128i *)pu1_dst, uv0_0_16x8b);
            _mm_storel_epi64((__m128i *)(pu1_dst + dst_strd), uv0_1_16x8b);

            ht -= 2;
            pu1_src1 += src_strd1 << 1;
            pu1_src2 += src_strd2 << 1;
            pu1_dst += dst_strd << 1;
        }
        while(ht > 0);
    }
    else // wd == 8
    {
        __m128i uv0_2_16x8b, uv0_3_16x8b;
        __m128i uv1_2_16x8b, uv1_3_16x8b;

        do
        {
            uv0_0_16x8b = _mm_loadu_si128((__m128i *)pu1_src1);
            uv0_1_16x8b = _mm_loadu_si128((__m128i *)(pu1_src1 + src_strd1));
            uv0_2_16x8b = _mm_loadu_si128(
                            (__m128i *)(pu1_src1 + (src_strd1 << 1)));
            uv0_3_16x8b = _mm_loadu_si128(
                            (__m128i *)(pu1_src1 + src_strd1 * 3));

            uv1_0_16x8b = _mm_loadu_si128((__m128i *)pu1_src2);
            uv1_1_16x8b = _mm_loadu_si128((__m128i *)(pu1_src2 + src_strd2));
            uv1_2_16x8b = _mm_loadu_si128(
                            (__m128i *)(pu1_src2 + (src_strd2 << 1)));
            uv1_3_16x8b = _mm_loadu_si128(
                            (__m128i *)(pu1_src2 + src_strd2 * 3));

            uv0_0_16x8b = _mm_avg_epu8(uv0_0_16x8b, uv1_0_16x8b);
            uv0_1_16x8b = _mm_avg_epu8(uv0_1_16x8b, uv1_1_16x8b);
            uv0_2_16x8b = _mm_avg_epu8(uv0_2_16x8b, uv1_2_16x8b);
            uv0_3_16x8b = _mm_avg_epu8(uv0_3_16x8b, uv1_3_16x8b);

            _mm_storeu_si128((__m128i *)pu1_dst, uv0_0_16x8b);
            _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), uv0_1_16x8b);
            _mm_storeu_si128(
                            (__m128i *)(pu1_dst + (dst_strd << 1)), uv0_2_16x8b);
            _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd * 3), uv0_3_16x8b);

            ht -= 4;
            pu1_src1 += src_strd1 << 2;
            pu1_src2 += src_strd2 << 2;
            pu1_dst += dst_strd << 2;
        }
        while(ht > 0);
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_weighted_pred_luma_sse42                           */
/*                                                                           */
/*  Description   : This function performs the weighted prediction as        */
/*                  described in sec 8.4.2.3.2 titled "Weighted sample       */
/*                  prediction process" for luma. The function gets one      */
/*                  ht x wd block, weights it, rounds it off, offsets it,    */
/*                  saturates it to unsigned 8-bit and stores it in the      */
/*                  destination block. (ht,wd) can be (4,4), (8,4), (4,8),   */
/*                  (8,8), (16,8), (8,16) or (16,16).                        */
/*                                                                           */
/*  Inputs        : pu1_src  - Pointer to source                             */
/*                  pu1_dst  - Pointer to destination                        */
/*                  src_strd - stride for source                             */
/*                  dst_strd - stride for destination                        */
/*                  log_wd   - number of bits to be rounded off              */
/*                  wt       - weight value                                  */
/*                  ofst     - offset value                                  */
/*                  ht       - height of the block                           */
/*                  wd       - width of the block                            */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes                              */
/*         04 02 2015   Kaushik         Initial Version                      */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_weighted_pred_luma_sse42(UWORD8 *pu1_src,
                                    UWORD8 *pu1_dst,
                                    WORD32 src_strd,
                                    WORD32 dst_strd,
                                    WORD32 log_wd,
                                    WORD32 wt,
                                    WORD32 ofst,
                                    WORD32 ht,
                                    WORD32 wd)
{
    __m128i y_0_16x8b, y_1_16x8b, y_2_16x8b, y_3_16x8b;

    __m128i wt_8x16b, round_8x16b, ofst_8x16b;

    WORD32 round_val;

    wt = (WORD16)(wt & 0xffff);
    round_val = 1 << (log_wd - 1);
    ofst = (WORD8)(ofst & 0xff);

    wt_8x16b = _mm_set1_epi16(wt);
    round_8x16b = _mm_set1_epi16(round_val);
    ofst_8x16b = _mm_set1_epi16(ofst);

    if(wd == 4)
    {
        __m128i y_0_8x16b, y_2_8x16b;

        do
        {
            y_0_16x8b = _mm_loadl_epi64((__m128i *)pu1_src);
            y_1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src + src_strd));
            y_2_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src + (src_strd << 1)));
            y_3_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src + src_strd * 3));

            y_0_16x8b = _mm_unpacklo_epi32(y_0_16x8b, y_1_16x8b);
            y_2_16x8b = _mm_unpacklo_epi32(y_2_16x8b, y_3_16x8b);

            y_0_8x16b = _mm_cvtepu8_epi16(y_0_16x8b);
            y_2_8x16b = _mm_cvtepu8_epi16(y_2_16x8b);

            y_0_8x16b = _mm_mullo_epi16(y_0_8x16b, wt_8x16b);
            y_2_8x16b = _mm_mullo_epi16(y_2_8x16b, wt_8x16b);

            y_0_8x16b = _mm_adds_epi16(round_8x16b, y_0_8x16b);
            y_2_8x16b = _mm_adds_epi16(round_8x16b, y_2_8x16b);

            y_0_8x16b = _mm_srai_epi16(y_0_8x16b, log_wd);
            y_2_8x16b = _mm_srai_epi16(y_2_8x16b, log_wd);

            y_0_8x16b = _mm_adds_epi16(ofst_8x16b, y_0_8x16b);
            y_2_8x16b = _mm_adds_epi16(ofst_8x16b, y_2_8x16b);

            y_0_16x8b = _mm_packus_epi16(y_0_8x16b, y_2_8x16b);
            y_1_16x8b = _mm_srli_si128(y_0_16x8b, 4);
            y_2_16x8b = _mm_srli_si128(y_0_16x8b, 8);
            y_3_16x8b = _mm_srli_si128(y_0_16x8b, 12);

            *((WORD32 *)(pu1_dst)) = _mm_cvtsi128_si32(y_0_16x8b);
            *((WORD32 *)(pu1_dst + dst_strd)) = _mm_cvtsi128_si32(y_1_16x8b);
            *((WORD32 *)(pu1_dst + (dst_strd << 1))) = _mm_cvtsi128_si32(y_2_16x8b);
            *((WORD32 *)(pu1_dst + dst_strd * 3)) = _mm_cvtsi128_si32(y_3_16x8b);

            ht -= 4;
            pu1_src += src_strd << 2;
            pu1_dst += dst_strd << 2;
        }
        while(ht > 0);
    }
    else if(wd == 8)
    {
        __m128i y_0_8x16b, y_1_8x16b, y_2_8x16b, y_3_8x16b;

        do
        {
            y_0_16x8b = _mm_loadl_epi64((__m128i *)pu1_src);
            y_1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src + src_strd));
            y_2_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src + (src_strd << 1)));
            y_3_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src + src_strd * 3));

            y_0_8x16b = _mm_cvtepu8_epi16(y_0_16x8b);
            y_1_8x16b = _mm_cvtepu8_epi16(y_1_16x8b);
            y_2_8x16b = _mm_cvtepu8_epi16(y_2_16x8b);
            y_3_8x16b = _mm_cvtepu8_epi16(y_3_16x8b);

            y_0_8x16b = _mm_mullo_epi16(y_0_8x16b, wt_8x16b);
            y_1_8x16b = _mm_mullo_epi16(y_1_8x16b, wt_8x16b);
            y_2_8x16b = _mm_mullo_epi16(y_2_8x16b, wt_8x16b);
            y_3_8x16b = _mm_mullo_epi16(y_3_8x16b, wt_8x16b);

            y_0_8x16b = _mm_adds_epi16(round_8x16b, y_0_8x16b);
            y_1_8x16b = _mm_adds_epi16(round_8x16b, y_1_8x16b);
            y_2_8x16b = _mm_adds_epi16(round_8x16b, y_2_8x16b);
            y_3_8x16b = _mm_adds_epi16(round_8x16b, y_3_8x16b);

            y_0_8x16b = _mm_srai_epi16(y_0_8x16b, log_wd);
            y_1_8x16b = _mm_srai_epi16(y_1_8x16b, log_wd);
            y_2_8x16b = _mm_srai_epi16(y_2_8x16b, log_wd);
            y_3_8x16b = _mm_srai_epi16(y_3_8x16b, log_wd);

            y_0_8x16b = _mm_adds_epi16(ofst_8x16b, y_0_8x16b);
            y_1_8x16b = _mm_adds_epi16(ofst_8x16b, y_1_8x16b);
            y_2_8x16b = _mm_adds_epi16(ofst_8x16b, y_2_8x16b);
            y_3_8x16b = _mm_adds_epi16(ofst_8x16b, y_3_8x16b);

            y_0_16x8b = _mm_packus_epi16(y_0_8x16b, y_1_8x16b);
            y_2_16x8b = _mm_packus_epi16(y_2_8x16b, y_3_8x16b);
            y_1_16x8b = _mm_srli_si128(y_0_16x8b, 8);
            y_3_16x8b = _mm_srli_si128(y_2_16x8b, 8);

            _mm_storel_epi64((__m128i *)pu1_dst, y_0_16x8b);
            _mm_storel_epi64((__m128i *)(pu1_dst + dst_strd), y_1_16x8b);
            _mm_storel_epi64((__m128i *)(pu1_dst + (dst_strd << 1)), y_2_16x8b);
            _mm_storel_epi64((__m128i *)(pu1_dst + dst_strd * 3), y_3_16x8b);

            ht -= 4;
            pu1_src += src_strd << 2;
            pu1_dst += dst_strd << 2;
        }
        while(ht > 0);
    }
    else // wd == 16
    {
        __m128i y_0L_8x16b, y_1L_8x16b, y_2L_8x16b, y_3L_8x16b;
        __m128i y_0H_8x16b, y_1H_8x16b, y_2H_8x16b, y_3H_8x16b;

        __m128i zero_16x8b;
        zero_16x8b = _mm_set1_epi8(0);

        do
        {
            y_0_16x8b = _mm_loadu_si128((__m128i *)pu1_src);
            y_1_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + src_strd));
            y_2_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + (src_strd << 1)));
            y_3_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + src_strd * 3));

            y_0L_8x16b = _mm_cvtepu8_epi16(y_0_16x8b);
            y_0H_8x16b = _mm_unpackhi_epi8(y_0_16x8b, zero_16x8b);
            y_1L_8x16b = _mm_cvtepu8_epi16(y_1_16x8b);
            y_1H_8x16b = _mm_unpackhi_epi8(y_1_16x8b, zero_16x8b);
            y_2L_8x16b = _mm_cvtepu8_epi16(y_2_16x8b);
            y_2H_8x16b = _mm_unpackhi_epi8(y_2_16x8b, zero_16x8b);
            y_3L_8x16b = _mm_cvtepu8_epi16(y_3_16x8b);
            y_3H_8x16b = _mm_unpackhi_epi8(y_3_16x8b, zero_16x8b);

            y_0L_8x16b = _mm_mullo_epi16(y_0L_8x16b, wt_8x16b);
            y_0H_8x16b = _mm_mullo_epi16(y_0H_8x16b, wt_8x16b);
            y_1L_8x16b = _mm_mullo_epi16(y_1L_8x16b, wt_8x16b);
            y_1H_8x16b = _mm_mullo_epi16(y_1H_8x16b, wt_8x16b);
            y_2L_8x16b = _mm_mullo_epi16(y_2L_8x16b, wt_8x16b);
            y_2H_8x16b = _mm_mullo_epi16(y_2H_8x16b, wt_8x16b);
            y_3L_8x16b = _mm_mullo_epi16(y_3L_8x16b, wt_8x16b);
            y_3H_8x16b = _mm_mullo_epi16(y_3H_8x16b, wt_8x16b);

            y_0L_8x16b = _mm_adds_epi16(round_8x16b, y_0L_8x16b);
            y_0H_8x16b = _mm_adds_epi16(round_8x16b, y_0H_8x16b);
            y_1L_8x16b = _mm_adds_epi16(round_8x16b, y_1L_8x16b);
            y_1H_8x16b = _mm_adds_epi16(round_8x16b, y_1H_8x16b);
            y_2L_8x16b = _mm_adds_epi16(round_8x16b, y_2L_8x16b);
            y_2H_8x16b = _mm_adds_epi16(round_8x16b, y_2H_8x16b);
            y_3L_8x16b = _mm_adds_epi16(round_8x16b, y_3L_8x16b);
            y_3H_8x16b = _mm_adds_epi16(round_8x16b, y_3H_8x16b);

            y_0L_8x16b = _mm_srai_epi16(y_0L_8x16b, log_wd);
            y_0H_8x16b = _mm_srai_epi16(y_0H_8x16b, log_wd);
            y_1L_8x16b = _mm_srai_epi16(y_1L_8x16b, log_wd);
            y_1H_8x16b = _mm_srai_epi16(y_1H_8x16b, log_wd);
            y_2L_8x16b = _mm_srai_epi16(y_2L_8x16b, log_wd);
            y_2H_8x16b = _mm_srai_epi16(y_2H_8x16b, log_wd);
            y_3L_8x16b = _mm_srai_epi16(y_3L_8x16b, log_wd);
            y_3H_8x16b = _mm_srai_epi16(y_3H_8x16b, log_wd);

            y_0L_8x16b = _mm_adds_epi16(ofst_8x16b, y_0L_8x16b);
            y_0H_8x16b = _mm_adds_epi16(ofst_8x16b, y_0H_8x16b);
            y_1L_8x16b = _mm_adds_epi16(ofst_8x16b, y_1L_8x16b);
            y_1H_8x16b = _mm_adds_epi16(ofst_8x16b, y_1H_8x16b);
            y_2L_8x16b = _mm_adds_epi16(ofst_8x16b, y_2L_8x16b);
            y_2H_8x16b = _mm_adds_epi16(ofst_8x16b, y_2H_8x16b);
            y_3L_8x16b = _mm_adds_epi16(ofst_8x16b, y_3L_8x16b);
            y_3H_8x16b = _mm_adds_epi16(ofst_8x16b, y_3H_8x16b);

            y_0_16x8b = _mm_packus_epi16(y_0L_8x16b, y_0H_8x16b);
            y_1_16x8b = _mm_packus_epi16(y_1L_8x16b, y_1H_8x16b);
            y_2_16x8b = _mm_packus_epi16(y_2L_8x16b, y_2H_8x16b);
            y_3_16x8b = _mm_packus_epi16(y_3L_8x16b, y_3H_8x16b);

            _mm_storeu_si128((__m128i *)pu1_dst, y_0_16x8b);
            _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), y_1_16x8b);
            _mm_storeu_si128((__m128i *)(pu1_dst + (dst_strd << 1)), y_2_16x8b);
            _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd * 3), y_3_16x8b);

            ht -= 4;
            pu1_src += src_strd << 2;
            pu1_dst += dst_strd << 2;
        }
        while(ht > 0);
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_weighted_pred_chroma_sse42                         */
/*                                                                           */
/*  Description   : This function performs the weighted prediction as        */
/*                  described in sec 8.4.2.3.2 titled "Weighted sample       */
/*                  prediction process" for chroma. The function gets one    */
/*                  ht x wd block, weights it, rounds it off, offsets it,    */
/*                  saturates it to unsigned 8-bit and stores it in the      */
/*                  destination block. (ht,wd) can be (2,2), (4,2), (2,4),   */
/*                  (4,4), (8,4), (4,8) or (8,8).                            */
/*                                                                           */
/*  Inputs        : pu1_src  - Pointer to source                             */
/*                  pu1_dst  - Pointer to destination                        */
/*                  src_strd - stride for source                             */
/*                  dst_strd - stride for destination                        */
/*                  log_wd   - number of bits to be rounded off              */
/*                  wt       - weight values for u and v                     */
/*                  ofst     - offset values for u and v                     */
/*                  ht       - height of the block                           */
/*                  wd       - width of the block                            */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes                              */
/*         04 02 2015   Kaushik         Initial Version                      */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_weighted_pred_chroma_sse42(UWORD8 *pu1_src,
                                      UWORD8 *pu1_dst,
                                      WORD32 src_strd,
                                      WORD32 dst_strd,
                                      WORD32 log_wd,
                                      WORD32 wt,
                                      WORD32 ofst,
                                      WORD32 ht,
                                      WORD32 wd)
{
    __m128i y_0_16x8b, y_1_16x8b;

    __m128i wt_8x16b, round_8x16b, ofst_8x16b;

    WORD32 ofst_u, ofst_v;
    WORD32 round_val;

    ofst_u = (WORD8)(ofst & 0xff);
    ofst_v = (WORD8)(ofst >> 8);
    round_val = 1 << (log_wd - 1);
    ofst = (ofst_u & 0xffff) | (ofst_v << 16);

    wt_8x16b = _mm_set1_epi32(wt);
    round_8x16b = _mm_set1_epi16(round_val);
    ofst_8x16b = _mm_set1_epi32(ofst);

    if(wd == 2)
    {
        __m128i y_0_8x16b;

        do
        {
            y_0_16x8b = _mm_loadl_epi64((__m128i *)pu1_src);
            y_1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src + src_strd));

            y_0_16x8b = _mm_unpacklo_epi32(y_0_16x8b, y_1_16x8b);

            y_0_8x16b = _mm_cvtepu8_epi16(y_0_16x8b);

            y_0_8x16b = _mm_mullo_epi16(y_0_8x16b, wt_8x16b);

            y_0_8x16b = _mm_adds_epi16(round_8x16b, y_0_8x16b);

            y_0_8x16b = _mm_srai_epi16(y_0_8x16b, log_wd);

            y_0_8x16b = _mm_adds_epi16(ofst_8x16b, y_0_8x16b);

            y_0_16x8b = _mm_packus_epi16(y_0_8x16b, y_0_8x16b);
            y_1_16x8b = _mm_srli_si128(y_0_16x8b, 4);

            *((WORD32 *)(pu1_dst)) = _mm_cvtsi128_si32(y_0_16x8b);
            *((WORD32 *)(pu1_dst + dst_strd)) = _mm_cvtsi128_si32(y_1_16x8b);

            ht -= 2;
            pu1_src += src_strd << 1;
            pu1_dst += dst_strd << 1;
        }
        while(ht > 0);
    }
    else if(wd == 4)
    {
        __m128i y_0_8x16b, y_1_8x16b;

        do
        {
            y_0_16x8b = _mm_loadl_epi64((__m128i *)pu1_src);
            y_1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src + src_strd));

            y_0_8x16b = _mm_cvtepu8_epi16(y_0_16x8b);
            y_1_8x16b = _mm_cvtepu8_epi16(y_1_16x8b);

            y_0_8x16b = _mm_mullo_epi16(y_0_8x16b, wt_8x16b);
            y_1_8x16b = _mm_mullo_epi16(y_1_8x16b, wt_8x16b);

            y_0_8x16b = _mm_adds_epi16(round_8x16b, y_0_8x16b);
            y_1_8x16b = _mm_adds_epi16(round_8x16b, y_1_8x16b);

            y_0_8x16b = _mm_srai_epi16(y_0_8x16b, log_wd);
            y_1_8x16b = _mm_srai_epi16(y_1_8x16b, log_wd);

            y_0_8x16b = _mm_adds_epi16(ofst_8x16b, y_0_8x16b);
            y_1_8x16b = _mm_adds_epi16(ofst_8x16b, y_1_8x16b);

            y_0_16x8b = _mm_packus_epi16(y_0_8x16b, y_1_8x16b);
            y_1_16x8b = _mm_srli_si128(y_0_16x8b, 8);

            _mm_storel_epi64((__m128i *)pu1_dst, y_0_16x8b);
            _mm_storel_epi64((__m128i *)(pu1_dst + dst_strd), y_1_16x8b);

            ht -= 2;
            pu1_src += src_strd << 1;
            pu1_dst += dst_strd << 1;
        }
        while(ht > 0);
    }
    else // wd == 16
    {
        __m128i y_2_16x8b, y_3_16x8b;
        __m128i y_0L_8x16b, y_1L_8x16b, y_2L_8x16b, y_3L_8x16b;
        __m128i y_0H_8x16b, y_1H_8x16b, y_2H_8x16b, y_3H_8x16b;

        __m128i zero_16x8b;
        zero_16x8b = _mm_set1_epi8(0);

        do
        {
            y_0_16x8b = _mm_loadu_si128((__m128i *)pu1_src);
            y_1_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + src_strd));
            y_2_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + (src_strd << 1)));
            y_3_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + src_strd * 3));

            y_0L_8x16b = _mm_cvtepu8_epi16(y_0_16x8b);
            y_0H_8x16b = _mm_unpackhi_epi8(y_0_16x8b, zero_16x8b);
            y_1L_8x16b = _mm_cvtepu8_epi16(y_1_16x8b);
            y_1H_8x16b = _mm_unpackhi_epi8(y_1_16x8b, zero_16x8b);
            y_2L_8x16b = _mm_cvtepu8_epi16(y_2_16x8b);
            y_2H_8x16b = _mm_unpackhi_epi8(y_2_16x8b, zero_16x8b);
            y_3L_8x16b = _mm_cvtepu8_epi16(y_3_16x8b);
            y_3H_8x16b = _mm_unpackhi_epi8(y_3_16x8b, zero_16x8b);

            y_0L_8x16b = _mm_mullo_epi16(y_0L_8x16b, wt_8x16b);
            y_0H_8x16b = _mm_mullo_epi16(y_0H_8x16b, wt_8x16b);
            y_1L_8x16b = _mm_mullo_epi16(y_1L_8x16b, wt_8x16b);
            y_1H_8x16b = _mm_mullo_epi16(y_1H_8x16b, wt_8x16b);
            y_2L_8x16b = _mm_mullo_epi16(y_2L_8x16b, wt_8x16b);
            y_2H_8x16b = _mm_mullo_epi16(y_2H_8x16b, wt_8x16b);
            y_3L_8x16b = _mm_mullo_epi16(y_3L_8x16b, wt_8x16b);
            y_3H_8x16b = _mm_mullo_epi16(y_3H_8x16b, wt_8x16b);

            y_0L_8x16b = _mm_adds_epi16(round_8x16b, y_0L_8x16b);
            y_0H_8x16b = _mm_adds_epi16(round_8x16b, y_0H_8x16b);
            y_1L_8x16b = _mm_adds_epi16(round_8x16b, y_1L_8x16b);
            y_1H_8x16b = _mm_adds_epi16(round_8x16b, y_1H_8x16b);
            y_2L_8x16b = _mm_adds_epi16(round_8x16b, y_2L_8x16b);
            y_2H_8x16b = _mm_adds_epi16(round_8x16b, y_2H_8x16b);
            y_3L_8x16b = _mm_adds_epi16(round_8x16b, y_3L_8x16b);
            y_3H_8x16b = _mm_adds_epi16(round_8x16b, y_3H_8x16b);

            y_0L_8x16b = _mm_srai_epi16(y_0L_8x16b, log_wd);
            y_0H_8x16b = _mm_srai_epi16(y_0H_8x16b, log_wd);
            y_1L_8x16b = _mm_srai_epi16(y_1L_8x16b, log_wd);
            y_1H_8x16b = _mm_srai_epi16(y_1H_8x16b, log_wd);
            y_2L_8x16b = _mm_srai_epi16(y_2L_8x16b, log_wd);
            y_2H_8x16b = _mm_srai_epi16(y_2H_8x16b, log_wd);
            y_3L_8x16b = _mm_srai_epi16(y_3L_8x16b, log_wd);
            y_3H_8x16b = _mm_srai_epi16(y_3H_8x16b, log_wd);

            y_0L_8x16b = _mm_adds_epi16(ofst_8x16b, y_0L_8x16b);
            y_0H_8x16b = _mm_adds_epi16(ofst_8x16b, y_0H_8x16b);
            y_1L_8x16b = _mm_adds_epi16(ofst_8x16b, y_1L_8x16b);
            y_1H_8x16b = _mm_adds_epi16(ofst_8x16b, y_1H_8x16b);
            y_2L_8x16b = _mm_adds_epi16(ofst_8x16b, y_2L_8x16b);
            y_2H_8x16b = _mm_adds_epi16(ofst_8x16b, y_2H_8x16b);
            y_3L_8x16b = _mm_adds_epi16(ofst_8x16b, y_3L_8x16b);
            y_3H_8x16b = _mm_adds_epi16(ofst_8x16b, y_3H_8x16b);

            y_0_16x8b = _mm_packus_epi16(y_0L_8x16b, y_0H_8x16b);
            y_1_16x8b = _mm_packus_epi16(y_1L_8x16b, y_1H_8x16b);
            y_2_16x8b = _mm_packus_epi16(y_2L_8x16b, y_2H_8x16b);
            y_3_16x8b = _mm_packus_epi16(y_3L_8x16b, y_3H_8x16b);

            _mm_storeu_si128((__m128i *)pu1_dst, y_0_16x8b);
            _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), y_1_16x8b);
            _mm_storeu_si128((__m128i *)(pu1_dst + (dst_strd << 1)), y_2_16x8b);
            _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd * 3), y_3_16x8b);

            ht -= 4;
            pu1_src += src_strd << 2;
            pu1_dst += dst_strd << 2;
        }
        while(ht > 0);
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_weighted_bi_pred_luma_sse42                        */
/*                                                                           */
/*  Description   : This function performs the weighted biprediction as      */
/*                  described in sec 8.4.2.3.2 titled "Weighted sample       */
/*                  prediction process" for luma. The function gets two      */
/*                  ht x wd blocks, weights them, adds them, rounds off the  */
/*                  sum, offsets it, saturates it to unsigned 8-bit and      */
/*                  stores it in the destination block. (ht,wd) can be       */
/*                  (4,4), (8,4), (4,8), (8,8), (16,8), (8,16) or (16,16).   */
/*                                                                           */
/*  Inputs        : pu1_src1  - Pointer to source 1                          */
/*                  pu1_src2  - Pointer to source 2                          */
/*                  pu1_dst   - Pointer to destination                       */
/*                  src_strd1 - stride for source 1                          */
/*                  src_strd2 - stride for source 2                          */
/*                  dst_strd2 - stride for destination                       */
/*                  log_wd    - number of bits to be rounded off             */
/*                  wt1       - weight value for source 1                    */
/*                  wt2       - weight value for source 2                    */
/*                  ofst1     - offset value for source 1                    */
/*                  ofst2     - offset value for source 2                    */
/*                  ht        - height of the block                          */
/*                  wd        - width of the block                           */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes                              */
/*         04 02 2015   Kaushik         Initial Version                      */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_weighted_bi_pred_luma_sse42(UWORD8 *pu1_src1,
                                       UWORD8 *pu1_src2,
                                       UWORD8 *pu1_dst,
                                       WORD32 src_strd1,
                                       WORD32 src_strd2,
                                       WORD32 dst_strd,
                                       WORD32 log_wd,
                                       WORD32 wt1,
                                       WORD32 wt2,
                                       WORD32 ofst1,
                                       WORD32 ofst2,
                                       WORD32 ht,
                                       WORD32 wd)
{
    __m128i y1_0_16x8b, y1_1_16x8b;
    __m128i y2_0_16x8b, y2_1_16x8b;

    __m128i wt1_8x16b, wt2_8x16b;
    __m128i ofst_8x16b, round_8x16b;

    WORD32 ofst;
    WORD32 round_val, shft;

    wt1 = (WORD16)(wt1 & 0xffff);
    wt2 = (WORD16)(wt2 & 0xffff);
    round_val = 1 << log_wd;
    shft = log_wd + 1;
    ofst1 = (WORD8)(ofst1 & 0xff);
    ofst2 = (WORD8)(ofst2 & 0xff);
    ofst = (ofst1 + ofst2 + 1) >> 1;

    wt1_8x16b = _mm_set1_epi16(wt1);
    wt2_8x16b = _mm_set1_epi16(wt2);
    round_8x16b = _mm_set1_epi16(round_val);
    ofst_8x16b = _mm_set1_epi16(ofst);

    if(wd == 4)
    {
        __m128i y1_2_16x8b, y1_3_16x8b;
        __m128i y2_2_16x8b, y2_3_16x8b;

        __m128i y1_0_8x16b, y1_2_8x16b;
        __m128i y2_0_8x16b, y2_2_8x16b;

        do
        {
            y1_0_16x8b = _mm_loadl_epi64((__m128i *)pu1_src1);
            y1_1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src1 + src_strd1));
            y1_2_16x8b = _mm_loadl_epi64(
                            (__m128i *)(pu1_src1 + (src_strd1 << 1)));
            y1_3_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src1 + src_strd1 * 3));

            y2_0_16x8b = _mm_loadl_epi64((__m128i *)pu1_src2);
            y2_1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src2 + src_strd2));
            y2_2_16x8b = _mm_loadl_epi64(
                            (__m128i *)(pu1_src2 + (src_strd2 << 1)));
            y2_3_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src2 + src_strd2 * 3));

            y1_0_16x8b = _mm_unpacklo_epi32(y1_0_16x8b, y1_1_16x8b);
            y1_2_16x8b = _mm_unpacklo_epi32(y1_2_16x8b, y1_3_16x8b);
            y2_0_16x8b = _mm_unpacklo_epi32(y2_0_16x8b, y2_1_16x8b);
            y2_2_16x8b = _mm_unpacklo_epi32(y2_2_16x8b, y2_3_16x8b);

            y1_0_8x16b = _mm_cvtepu8_epi16(y1_0_16x8b);
            y1_2_8x16b = _mm_cvtepu8_epi16(y1_2_16x8b);
            y2_0_8x16b = _mm_cvtepu8_epi16(y2_0_16x8b);
            y2_2_8x16b = _mm_cvtepu8_epi16(y2_2_16x8b);

            y1_0_8x16b = _mm_mullo_epi16(y1_0_8x16b, wt1_8x16b);
            y2_0_8x16b = _mm_mullo_epi16(y2_0_8x16b, wt2_8x16b);
            y1_2_8x16b = _mm_mullo_epi16(y1_2_8x16b, wt1_8x16b);
            y2_2_8x16b = _mm_mullo_epi16(y2_2_8x16b, wt2_8x16b);

            y1_0_8x16b = _mm_adds_epi16(y1_0_8x16b, y2_0_8x16b);
            y1_2_8x16b = _mm_adds_epi16(y1_2_8x16b, y2_2_8x16b);

            y1_0_8x16b = _mm_adds_epi16(round_8x16b, y1_0_8x16b);
            y1_2_8x16b = _mm_adds_epi16(round_8x16b, y1_2_8x16b);

            y1_0_8x16b = _mm_srai_epi16(y1_0_8x16b, shft);
            y1_2_8x16b = _mm_srai_epi16(y1_2_8x16b, shft);

            y1_0_8x16b = _mm_adds_epi16(ofst_8x16b, y1_0_8x16b);
            y1_2_8x16b = _mm_adds_epi16(ofst_8x16b, y1_2_8x16b);

            y1_0_16x8b = _mm_packus_epi16(y1_0_8x16b, y1_2_8x16b);
            y1_1_16x8b = _mm_srli_si128(y1_0_16x8b, 4);
            y1_2_16x8b = _mm_srli_si128(y1_0_16x8b, 8);
            y1_3_16x8b = _mm_srli_si128(y1_0_16x8b, 12);

            *((WORD32 *)(pu1_dst)) = _mm_cvtsi128_si32(y1_0_16x8b);
            *((WORD32 *)(pu1_dst + dst_strd)) = _mm_cvtsi128_si32(y1_1_16x8b);
            *((WORD32 *)(pu1_dst + (dst_strd << 1))) = _mm_cvtsi128_si32(y1_2_16x8b);
            *((WORD32 *)(pu1_dst + dst_strd * 3)) = _mm_cvtsi128_si32(y1_3_16x8b);


            ht -= 4;
            pu1_src1 += src_strd1 << 2;
            pu1_src2 += src_strd2 << 2;
            pu1_dst += dst_strd << 2;
        }
        while(ht > 0);
    }
    else if(wd == 8)
    {
        __m128i y1_2_16x8b, y1_3_16x8b;
        __m128i y2_2_16x8b, y2_3_16x8b;

        __m128i y1_0_8x16b, y1_1_8x16b, y1_2_8x16b, y1_3_8x16b;
        __m128i y2_0_8x16b, y2_1_8x16b, y2_2_8x16b, y2_3_8x16b;

        do
        {
            y1_0_16x8b = _mm_loadl_epi64((__m128i *)pu1_src1);
            y1_1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src1 + src_strd1));
            y1_2_16x8b = _mm_loadl_epi64(
                            (__m128i *)(pu1_src1 + (src_strd1 << 1)));
            y1_3_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src1 + src_strd1 * 3));

            y2_0_16x8b = _mm_loadl_epi64((__m128i *)pu1_src2);
            y2_1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src2 + src_strd2));
            y2_2_16x8b = _mm_loadl_epi64(
                            (__m128i *)(pu1_src2 + (src_strd2 << 1)));
            y2_3_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src2 + src_strd2 * 3));

            y1_0_8x16b = _mm_cvtepu8_epi16(y1_0_16x8b);
            y1_1_8x16b = _mm_cvtepu8_epi16(y1_1_16x8b);
            y1_2_8x16b = _mm_cvtepu8_epi16(y1_2_16x8b);
            y1_3_8x16b = _mm_cvtepu8_epi16(y1_3_16x8b);

            y2_0_8x16b = _mm_cvtepu8_epi16(y2_0_16x8b);
            y2_1_8x16b = _mm_cvtepu8_epi16(y2_1_16x8b);
            y2_2_8x16b = _mm_cvtepu8_epi16(y2_2_16x8b);
            y2_3_8x16b = _mm_cvtepu8_epi16(y2_3_16x8b);

            y1_0_8x16b = _mm_mullo_epi16(y1_0_8x16b, wt1_8x16b);
            y2_0_8x16b = _mm_mullo_epi16(y2_0_8x16b, wt2_8x16b);
            y1_1_8x16b = _mm_mullo_epi16(y1_1_8x16b, wt1_8x16b);
            y2_1_8x16b = _mm_mullo_epi16(y2_1_8x16b, wt2_8x16b);

            y1_2_8x16b = _mm_mullo_epi16(y1_2_8x16b, wt1_8x16b);
            y2_2_8x16b = _mm_mullo_epi16(y2_2_8x16b, wt2_8x16b);
            y1_3_8x16b = _mm_mullo_epi16(y1_3_8x16b, wt1_8x16b);
            y2_3_8x16b = _mm_mullo_epi16(y2_3_8x16b, wt2_8x16b);

            y1_0_8x16b = _mm_adds_epi16(y1_0_8x16b, y2_0_8x16b);
            y1_1_8x16b = _mm_adds_epi16(y1_1_8x16b, y2_1_8x16b);
            y1_2_8x16b = _mm_adds_epi16(y1_2_8x16b, y2_2_8x16b);
            y1_3_8x16b = _mm_adds_epi16(y1_3_8x16b, y2_3_8x16b);

            y1_0_8x16b = _mm_adds_epi16(round_8x16b, y1_0_8x16b);
            y1_1_8x16b = _mm_adds_epi16(round_8x16b, y1_1_8x16b);
            y1_2_8x16b = _mm_adds_epi16(round_8x16b, y1_2_8x16b);
            y1_3_8x16b = _mm_adds_epi16(round_8x16b, y1_3_8x16b);

            y1_0_8x16b = _mm_srai_epi16(y1_0_8x16b, shft);
            y1_1_8x16b = _mm_srai_epi16(y1_1_8x16b, shft);
            y1_2_8x16b = _mm_srai_epi16(y1_2_8x16b, shft);
            y1_3_8x16b = _mm_srai_epi16(y1_3_8x16b, shft);

            y1_0_8x16b = _mm_adds_epi16(ofst_8x16b, y1_0_8x16b);
            y1_1_8x16b = _mm_adds_epi16(ofst_8x16b, y1_1_8x16b);
            y1_2_8x16b = _mm_adds_epi16(ofst_8x16b, y1_2_8x16b);
            y1_3_8x16b = _mm_adds_epi16(ofst_8x16b, y1_3_8x16b);

            y1_0_16x8b = _mm_packus_epi16(y1_0_8x16b, y1_1_8x16b);
            y1_2_16x8b = _mm_packus_epi16(y1_2_8x16b, y1_3_8x16b);
            y1_1_16x8b = _mm_srli_si128(y1_0_16x8b, 8);
            y1_3_16x8b = _mm_srli_si128(y1_2_16x8b, 8);

            _mm_storel_epi64((__m128i *)pu1_dst, y1_0_16x8b);
            _mm_storel_epi64((__m128i *)(pu1_dst + dst_strd), y1_1_16x8b);
            _mm_storel_epi64((__m128i *)(pu1_dst + (dst_strd << 1)), y1_2_16x8b);
            _mm_storel_epi64((__m128i *)(pu1_dst + dst_strd * 3), y1_3_16x8b);

            ht -= 4;
            pu1_src1 += src_strd1 << 2;
            pu1_src2 += src_strd2 << 2;
            pu1_dst += dst_strd << 2;
        }
        while(ht > 0);
    }
    else // wd == 16
    {
        __m128i y1_0L_8x16b, y1_0H_8x16b, y1_1L_8x16b, y1_1H_8x16b;
        __m128i y2_0L_8x16b, y2_0H_8x16b, y2_1L_8x16b, y2_1H_8x16b;

        __m128i zero_16x8b;
        zero_16x8b = _mm_set1_epi8(0);

        do
        {
            y1_0_16x8b = _mm_loadu_si128((__m128i *)pu1_src1);
            y1_1_16x8b = _mm_loadu_si128((__m128i *)(pu1_src1 + src_strd1));
            y2_0_16x8b = _mm_loadu_si128((__m128i *)pu1_src2);
            y2_1_16x8b = _mm_loadu_si128((__m128i *)(pu1_src2 + src_strd2));

            y1_0L_8x16b = _mm_cvtepu8_epi16(y1_0_16x8b);
            y1_0H_8x16b = _mm_unpackhi_epi8(y1_0_16x8b, zero_16x8b);
            y1_1L_8x16b = _mm_cvtepu8_epi16(y1_1_16x8b);
            y1_1H_8x16b = _mm_unpackhi_epi8(y1_1_16x8b, zero_16x8b);

            y2_0L_8x16b = _mm_cvtepu8_epi16(y2_0_16x8b);
            y2_0H_8x16b = _mm_unpackhi_epi8(y2_0_16x8b, zero_16x8b);
            y2_1L_8x16b = _mm_cvtepu8_epi16(y2_1_16x8b);
            y2_1H_8x16b = _mm_unpackhi_epi8(y2_1_16x8b, zero_16x8b);

            y1_0L_8x16b = _mm_mullo_epi16(y1_0L_8x16b, wt1_8x16b);
            y1_0H_8x16b = _mm_mullo_epi16(y1_0H_8x16b, wt1_8x16b);
            y1_1L_8x16b = _mm_mullo_epi16(y1_1L_8x16b, wt1_8x16b);
            y1_1H_8x16b = _mm_mullo_epi16(y1_1H_8x16b, wt1_8x16b);

            y2_0L_8x16b = _mm_mullo_epi16(y2_0L_8x16b, wt2_8x16b);
            y2_0H_8x16b = _mm_mullo_epi16(y2_0H_8x16b, wt2_8x16b);
            y2_1L_8x16b = _mm_mullo_epi16(y2_1L_8x16b, wt2_8x16b);
            y2_1H_8x16b = _mm_mullo_epi16(y2_1H_8x16b, wt2_8x16b);

            y1_0L_8x16b = _mm_adds_epi16(y1_0L_8x16b, y2_0L_8x16b);
            y1_0H_8x16b = _mm_adds_epi16(y1_0H_8x16b, y2_0H_8x16b);
            y1_1L_8x16b = _mm_adds_epi16(y1_1L_8x16b, y2_1L_8x16b);
            y1_1H_8x16b = _mm_adds_epi16(y1_1H_8x16b, y2_1H_8x16b);

            y1_0L_8x16b = _mm_adds_epi16(round_8x16b, y1_0L_8x16b);
            y1_0H_8x16b = _mm_adds_epi16(round_8x16b, y1_0H_8x16b);
            y1_1L_8x16b = _mm_adds_epi16(round_8x16b, y1_1L_8x16b);
            y1_1H_8x16b = _mm_adds_epi16(round_8x16b, y1_1H_8x16b);

            y1_0L_8x16b = _mm_srai_epi16(y1_0L_8x16b, shft);
            y1_0H_8x16b = _mm_srai_epi16(y1_0H_8x16b, shft);
            y1_1L_8x16b = _mm_srai_epi16(y1_1L_8x16b, shft);
            y1_1H_8x16b = _mm_srai_epi16(y1_1H_8x16b, shft);

            y1_0L_8x16b = _mm_adds_epi16(ofst_8x16b, y1_0L_8x16b);
            y1_0H_8x16b = _mm_adds_epi16(ofst_8x16b, y1_0H_8x16b);
            y1_1L_8x16b = _mm_adds_epi16(ofst_8x16b, y1_1L_8x16b);
            y1_1H_8x16b = _mm_adds_epi16(ofst_8x16b, y1_1H_8x16b);

            y1_0_16x8b = _mm_packus_epi16(y1_0L_8x16b, y1_0H_8x16b);
            y1_1_16x8b = _mm_packus_epi16(y1_1L_8x16b, y1_1H_8x16b);

            _mm_storeu_si128((__m128i *)pu1_dst, y1_0_16x8b);
            _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), y1_1_16x8b);

            ht -= 2;
            pu1_src1 += src_strd1 << 1;
            pu1_src2 += src_strd2 << 1;
            pu1_dst += dst_strd << 1;
        }
        while(ht > 0);
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_weighted_bi_pred_chroma_sse42                      */
/*                                                                           */
/*  Description   : This function performs the weighted biprediction as      */
/*                  described in sec 8.4.2.3.2 titled "Weighted sample       */
/*                  prediction process" for chroma. The function gets two    */
/*                  ht x wd blocks, weights them, adds them, rounds off the  */
/*                  sum, offsets it, saturates it to unsigned 8-bit and      */
/*                  stores it in the destination block. (ht,wd) can be       */
/*                  (2,2), (4,2), (2,4), (4,4), (8,4), (4,8) or (8,8).       */
/*                                                                           */
/*  Inputs        : pu1_src1  - Pointer to source 1                          */
/*                  pu1_src2  - Pointer to source 2                          */
/*                  pu1_dst   - Pointer to destination                       */
/*                  src_strd1 - stride for source 1                          */
/*                  src_strd2 - stride for source 2                          */
/*                  dst_strd2 - stride for destination                       */
/*                  log_wd    - number of bits to be rounded off             */
/*                  wt1       - weight values for u and v in source 1        */
/*                  wt2       - weight values for u and v in source 2        */
/*                  ofst1     - offset value for u and v in source 1         */
/*                  ofst2     - offset value for u and v in source 2         */
/*                  ht        - height of the block                          */
/*                  wd        - width of the block                           */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes                              */
/*         04 02 2015   Kaushik         Initial Version                      */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_weighted_bi_pred_chroma_sse42(UWORD8 *pu1_src1,
                                         UWORD8 *pu1_src2,
                                         UWORD8 *pu1_dst,
                                         WORD32 src_strd1,
                                         WORD32 src_strd2,
                                         WORD32 dst_strd,
                                         WORD32 log_wd,
                                         WORD32 wt1,
                                         WORD32 wt2,
                                         WORD32 ofst1,
                                         WORD32 ofst2,
                                         WORD32 ht,
                                         WORD32 wd)
{
    __m128i y1_0_16x8b, y1_1_16x8b;
    __m128i y2_0_16x8b, y2_1_16x8b;

    __m128i wt1_8x16b, wt2_8x16b;
    __m128i ofst_8x16b, round_8x16b;

    WORD32 ofst1_u, ofst2_u, ofst_u;
    WORD32 ofst1_v, ofst2_v, ofst_v;
    WORD32 round_val, shft, ofst_val;

    round_val = 1 << log_wd;
    shft = log_wd + 1;

    ofst1_u = (WORD8)(ofst1 & 0xff);
    ofst1_v = (WORD8)(ofst1 >> 8);
    ofst2_u = (WORD8)(ofst2 & 0xff);
    ofst2_v = (WORD8)(ofst2 >> 8);

    wt1_8x16b = _mm_set1_epi32(wt1);
    wt2_8x16b = _mm_set1_epi32(wt2);

    ofst_u = (ofst1_u + ofst2_u + 1) >> 1;
    ofst_v = (ofst1_v + ofst2_v + 1) >> 1;
    ofst_val = (ofst_u & 0xffff) | (ofst_v << 16);

    round_8x16b = _mm_set1_epi16(round_val);
    ofst_8x16b = _mm_set1_epi32(ofst_val);

    if(wd == 2)
    {
        __m128i y1_0_8x16b, y2_0_8x16b;

        do
        {
            y1_0_16x8b = _mm_loadl_epi64((__m128i *)pu1_src1);
            y1_1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src1 + src_strd1));

            y2_0_16x8b = _mm_loadl_epi64((__m128i *)pu1_src2);
            y2_1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src2 + src_strd2));

            y1_0_16x8b = _mm_unpacklo_epi32(y1_0_16x8b, y1_1_16x8b);
            y2_0_16x8b = _mm_unpacklo_epi32(y2_0_16x8b, y2_1_16x8b);

            y1_0_8x16b = _mm_cvtepu8_epi16(y1_0_16x8b);
            y2_0_8x16b = _mm_cvtepu8_epi16(y2_0_16x8b);

            y1_0_8x16b = _mm_mullo_epi16(y1_0_8x16b, wt1_8x16b);
            y2_0_8x16b = _mm_mullo_epi16(y2_0_8x16b, wt2_8x16b);

            y1_0_8x16b = _mm_adds_epi16(y1_0_8x16b, y2_0_8x16b);
            y1_0_8x16b = _mm_adds_epi16(round_8x16b, y1_0_8x16b);

            y1_0_8x16b = _mm_srai_epi16(y1_0_8x16b, shft);
            y1_0_8x16b = _mm_adds_epi16(ofst_8x16b, y1_0_8x16b);

            y1_0_16x8b = _mm_packus_epi16(y1_0_8x16b, y1_0_8x16b);
            y1_1_16x8b = _mm_srli_si128(y1_0_16x8b, 4);

            *((WORD32 *)(pu1_dst)) = _mm_cvtsi128_si32(y1_0_16x8b);
            *((WORD32 *)(pu1_dst + dst_strd)) = _mm_cvtsi128_si32(y1_1_16x8b);

            ht -= 2;
            pu1_src1 += src_strd1 << 1;
            pu1_src2 += src_strd2 << 1;
            pu1_dst += dst_strd << 1;
        }
        while(ht > 0);
    }
    else if(wd == 4)
    {
        __m128i y1_0_8x16b, y1_1_8x16b;
        __m128i y2_0_8x16b, y2_1_8x16b;

        do
        {
            y1_0_16x8b = _mm_loadl_epi64((__m128i *)pu1_src1);
            y1_1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src1 + src_strd1));

            y2_0_16x8b = _mm_loadl_epi64((__m128i *)pu1_src2);
            y2_1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src2 + src_strd2));

            y1_0_8x16b = _mm_cvtepu8_epi16(y1_0_16x8b);
            y1_1_8x16b = _mm_cvtepu8_epi16(y1_1_16x8b);

            y2_0_8x16b = _mm_cvtepu8_epi16(y2_0_16x8b);
            y2_1_8x16b = _mm_cvtepu8_epi16(y2_1_16x8b);

            y1_0_8x16b = _mm_mullo_epi16(y1_0_8x16b, wt1_8x16b);
            y2_0_8x16b = _mm_mullo_epi16(y2_0_8x16b, wt2_8x16b);
            y1_1_8x16b = _mm_mullo_epi16(y1_1_8x16b, wt1_8x16b);
            y2_1_8x16b = _mm_mullo_epi16(y2_1_8x16b, wt2_8x16b);

            y1_0_8x16b = _mm_adds_epi16(y1_0_8x16b, y2_0_8x16b);
            y1_1_8x16b = _mm_adds_epi16(y1_1_8x16b, y2_1_8x16b);

            y1_0_8x16b = _mm_adds_epi16(round_8x16b, y1_0_8x16b);
            y1_1_8x16b = _mm_adds_epi16(round_8x16b, y1_1_8x16b);

            y1_0_8x16b = _mm_srai_epi16(y1_0_8x16b, shft);
            y1_1_8x16b = _mm_srai_epi16(y1_1_8x16b, shft);

            y1_0_8x16b = _mm_adds_epi16(ofst_8x16b, y1_0_8x16b);
            y1_1_8x16b = _mm_adds_epi16(ofst_8x16b, y1_1_8x16b);

            y1_0_16x8b = _mm_packus_epi16(y1_0_8x16b, y1_1_8x16b);
            y1_1_16x8b = _mm_srli_si128(y1_0_16x8b, 8);

            _mm_storel_epi64((__m128i *)pu1_dst, y1_0_16x8b);
            _mm_storel_epi64((__m128i *)(pu1_dst + dst_strd), y1_1_16x8b);

            ht -= 2;
            pu1_src1 += src_strd1 << 1;
            pu1_src2 += src_strd2 << 1;
            pu1_dst += dst_strd << 1;
        }
        while(ht > 0);
    }
    else // wd == 8
    {
        __m128i y1_0L_8x16b, y1_0H_8x16b, y1_1L_8x16b, y1_1H_8x16b;
        __m128i y2_0L_8x16b, y2_0H_8x16b, y2_1L_8x16b, y2_1H_8x16b;

        __m128i zero_16x8b;
        zero_16x8b = _mm_set1_epi8(0);

        do
        {
            y1_0_16x8b = _mm_loadu_si128((__m128i *)pu1_src1);
            y1_1_16x8b = _mm_loadu_si128((__m128i *)(pu1_src1 + src_strd1));
            y2_0_16x8b = _mm_loadu_si128((__m128i *)pu1_src2);
            y2_1_16x8b = _mm_loadu_si128((__m128i *)(pu1_src2 + src_strd2));

            y1_0L_8x16b = _mm_cvtepu8_epi16(y1_0_16x8b);
            y1_0H_8x16b = _mm_unpackhi_epi8(y1_0_16x8b, zero_16x8b);
            y1_1L_8x16b = _mm_cvtepu8_epi16(y1_1_16x8b);
            y1_1H_8x16b = _mm_unpackhi_epi8(y1_1_16x8b, zero_16x8b);

            y2_0L_8x16b = _mm_cvtepu8_epi16(y2_0_16x8b);
            y2_0H_8x16b = _mm_unpackhi_epi8(y2_0_16x8b, zero_16x8b);
            y2_1L_8x16b = _mm_cvtepu8_epi16(y2_1_16x8b);
            y2_1H_8x16b = _mm_unpackhi_epi8(y2_1_16x8b, zero_16x8b);

            y1_0L_8x16b = _mm_mullo_epi16(y1_0L_8x16b, wt1_8x16b);
            y1_0H_8x16b = _mm_mullo_epi16(y1_0H_8x16b, wt1_8x16b);
            y1_1L_8x16b = _mm_mullo_epi16(y1_1L_8x16b, wt1_8x16b);
            y1_1H_8x16b = _mm_mullo_epi16(y1_1H_8x16b, wt1_8x16b);

            y2_0L_8x16b = _mm_mullo_epi16(y2_0L_8x16b, wt2_8x16b);
            y2_0H_8x16b = _mm_mullo_epi16(y2_0H_8x16b, wt2_8x16b);
            y2_1L_8x16b = _mm_mullo_epi16(y2_1L_8x16b, wt2_8x16b);
            y2_1H_8x16b = _mm_mullo_epi16(y2_1H_8x16b, wt2_8x16b);

            y1_0L_8x16b = _mm_adds_epi16(y1_0L_8x16b, y2_0L_8x16b);
            y1_0H_8x16b = _mm_adds_epi16(y1_0H_8x16b, y2_0H_8x16b);
            y1_1L_8x16b = _mm_adds_epi16(y1_1L_8x16b, y2_1L_8x16b);
            y1_1H_8x16b = _mm_adds_epi16(y1_1H_8x16b, y2_1H_8x16b);

            y1_0L_8x16b = _mm_adds_epi16(round_8x16b, y1_0L_8x16b);
            y1_0H_8x16b = _mm_adds_epi16(round_8x16b, y1_0H_8x16b);
            y1_1L_8x16b = _mm_adds_epi16(round_8x16b, y1_1L_8x16b);
            y1_1H_8x16b = _mm_adds_epi16(round_8x16b, y1_1H_8x16b);

            y1_0L_8x16b = _mm_srai_epi16(y1_0L_8x16b, shft);
            y1_0H_8x16b = _mm_srai_epi16(y1_0H_8x16b, shft);
            y1_1L_8x16b = _mm_srai_epi16(y1_1L_8x16b, shft);
            y1_1H_8x16b = _mm_srai_epi16(y1_1H_8x16b, shft);

            y1_0L_8x16b = _mm_adds_epi16(ofst_8x16b, y1_0L_8x16b);
            y1_0H_8x16b = _mm_adds_epi16(ofst_8x16b, y1_0H_8x16b);
            y1_1L_8x16b = _mm_adds_epi16(ofst_8x16b, y1_1L_8x16b);
            y1_1H_8x16b = _mm_adds_epi16(ofst_8x16b, y1_1H_8x16b);

            y1_0_16x8b = _mm_packus_epi16(y1_0L_8x16b, y1_0H_8x16b);
            y1_1_16x8b = _mm_packus_epi16(y1_1L_8x16b, y1_1H_8x16b);

            _mm_storeu_si128((__m128i *)pu1_dst, y1_0_16x8b);
            _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), y1_1_16x8b);

            ht -= 2;
            pu1_src1 += src_strd1 << 1;
            pu1_src2 += src_strd2 << 1;
            pu1_dst += dst_strd << 1;
        }
        while(ht > 0);
    }
}
