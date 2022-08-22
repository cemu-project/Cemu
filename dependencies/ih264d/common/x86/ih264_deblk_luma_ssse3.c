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
/*  File Name         : ih264_deblk_luma_ssse3.c                             */
/*                                                                           */
/*  Description       : Contains function definitions for deblocking         */
/*                                                                           */
/*  List of Functions : ih264_deblk_luma_vert_bs4_ssse3()                    */
/*                      ih264_deblk_luma_horz_bs4_ssse3()                    */
/*                      ih264_deblk_luma_vert_bslt4_ssse3()                  */
/*                      ih264_deblk_luma_horz_bslt4_ssse3()                  */
/*                      ih264_deblk_luma_vert_bs4_mbaff_ssse3()              */
/*                      ih264_deblk_luma_vert_bslt4_mbaff_ssse3()            */
/*                                                                           */
/*  Issues / Problems : None                                                 */
/*                                                                           */
/*  Revision History  :                                                      */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         12 02 2015   Naveen Kumar P  Added luma deblocking ssse3          */
/*                                      intrinsics                           */
/*                                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

/* System include files */
#include <stdio.h>

/* User include files */
#include "ih264_typedefs.h"
#include "ih264_platform_macros.h"
#include "ih264_deblk_edge_filters.h"
#include "ih264_macros.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_deblk_luma_vert_bs4_ssse3()                        */
/*                                                                           */
/*  Description   : This function performs filtering of a luma block         */
/*                  vertical edge when the boundary strength is set to 4.    */
/*                                                                           */
/*  Inputs        : pu1_src    - pointer to the src sample q0                */
/*                  src_strd   - source stride                               */
/*                  alpha      - alpha value for the boundary                */
/*                  beta       - beta value for the boundary                 */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : This operation is described in Sec. 8.7.2.4 under the    */
/*                  title "Filtering process for edges for bS equal to 4" in */
/*                  ITU T Rec H.264.                                         */
/*                                                                           */
/*  Outputs       : None                                                     */
/*                                                                           */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         12 02 2015   Naveen Kumar P  Initial version                      */
/*                                                                           */
/*****************************************************************************/
void ih264_deblk_luma_vert_bs4_ssse3(UWORD8 *pu1_src,
                                     WORD32 src_strd,
                                     WORD32 alpha,
                                     WORD32 beta)
{
    __m128i zero = _mm_setzero_si128();
    __m128i q0_16x8, q1_16x8, q2_16x8, q3_16x8;
    __m128i p0_16x8, p1_16x8, p2_16x8, p3_16x8;
    __m128i q0_8x16, q1_8x16, q2_8x16, q3_8x16;
    __m128i p0_8x16, p1_8x16, p2_8x16, p3_8x16;
    __m128i q0_16x8_1;
    __m128i p0_16x8_1;
    __m128i q0_16x8_2, q1_16x8_2, q2_16x8_2;
    __m128i p0_16x8_2, p1_16x8_2, p2_16x8_2;
    __m128i temp1, temp2, temp3, temp4, temp5, temp6;
    __m128i Alpha_8x16, Beta_8x16;
    __m128i flag1_16x8, flag2_16x8, flag3_16x8, flag4_16x8;
    __m128i const_val2_16x8 = _mm_set1_epi16(2);
    __m128i line1, line2, line3, line4, line5, line6, line7, line8;

    Alpha_8x16 = _mm_set1_epi16(alpha);
    Beta_8x16 = _mm_set1_epi16(beta);

    line1 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 0 * src_strd));
    line2 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 1 * src_strd));
    line3 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 2 * src_strd));
    line4 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 3 * src_strd));
    line5 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 4 * src_strd));
    line6 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 5 * src_strd));
    line7 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 6 * src_strd));
    line8 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 7 * src_strd));

    temp1 = _mm_unpacklo_epi8(line1, line2);
    temp2 = _mm_unpacklo_epi8(line3, line4);
    temp3 = _mm_unpacklo_epi8(line5, line6);
    temp4 = _mm_unpacklo_epi8(line7, line8);

    line1 = _mm_unpacklo_epi16(temp1, temp2);
    line2 = _mm_unpackhi_epi16(temp1, temp2);
    line3 = _mm_unpacklo_epi16(temp3, temp4);
    line4 = _mm_unpackhi_epi16(temp3, temp4);

    p1_8x16 = _mm_unpacklo_epi32(line1, line3);
    p0_8x16 = _mm_unpackhi_epi32(line1, line3);
    q0_8x16 = _mm_unpacklo_epi32(line2, line4);
    q1_8x16 = _mm_unpackhi_epi32(line2, line4);

    line1 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 8 * src_strd));
    line2 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 9 * src_strd));
    line3 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 10 * src_strd));
    line4 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 11 * src_strd));
    line5 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 12 * src_strd));
    line6 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 13 * src_strd));
    line7 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 14 * src_strd));
    line8 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 15 * src_strd));

    temp1 = _mm_unpacklo_epi8(line1, line2);
    temp2 = _mm_unpacklo_epi8(line3, line4);
    temp3 = _mm_unpacklo_epi8(line5, line6);
    temp4 = _mm_unpacklo_epi8(line7, line8);

    line1 = _mm_unpacklo_epi16(temp1, temp2);
    line2 = _mm_unpackhi_epi16(temp1, temp2);
    line3 = _mm_unpacklo_epi16(temp3, temp4);
    line4 = _mm_unpackhi_epi16(temp3, temp4);

    temp1 = _mm_unpacklo_epi32(line1, line3);
    temp2 = _mm_unpackhi_epi32(line1, line3);
    temp3 = _mm_unpacklo_epi32(line2, line4);
    temp4 = _mm_unpackhi_epi32(line2, line4);

    p3_16x8 = _mm_unpacklo_epi64(p1_8x16, temp1);
    p2_16x8 = _mm_unpackhi_epi64(p1_8x16, temp1);
    q2_16x8 = _mm_unpacklo_epi64(q1_8x16, temp4);
    q3_16x8 = _mm_unpackhi_epi64(q1_8x16, temp4);
    p1_16x8 = _mm_unpacklo_epi64(p0_8x16, temp2);
    p0_16x8 = _mm_unpackhi_epi64(p0_8x16, temp2);
    q0_16x8 = _mm_unpacklo_epi64(q0_8x16, temp3);
    q1_16x8 = _mm_unpackhi_epi64(q0_8x16, temp3);

    //Cond1 (ABS(p0 - q0) < alpha)
    temp1 = _mm_subs_epu8(q0_16x8, p0_16x8);
    temp2 = _mm_subs_epu8(p0_16x8, q0_16x8);
    temp1 = _mm_add_epi8(temp1, temp2);

    temp2 = _mm_unpacklo_epi8(temp1, zero);
    temp1 = _mm_unpackhi_epi8(temp1, zero);

    temp2 = _mm_cmpgt_epi16(Alpha_8x16, temp2);
    temp1 = _mm_cmpgt_epi16(Alpha_8x16, temp1);

    flag1_16x8 = _mm_packs_epi16(temp2, temp1);

    //Cond2 (ABS(q1 - q0) < beta)
    temp1 = _mm_subs_epu8(q0_16x8, q1_16x8);
    temp2 = _mm_subs_epu8(q1_16x8, q0_16x8);
    temp1 = _mm_add_epi8(temp1, temp2);

    temp2 = _mm_unpacklo_epi8(temp1, zero);
    temp1 = _mm_unpackhi_epi8(temp1, zero);

    temp2 = _mm_cmpgt_epi16(Beta_8x16, temp2);
    temp1 = _mm_cmpgt_epi16(Beta_8x16, temp1);

    flag2_16x8 = _mm_packs_epi16(temp2, temp1);

    flag1_16x8 = _mm_and_si128(flag1_16x8, flag2_16x8);

    //Cond3 (ABS(p1 - p0) < beta)
    temp1 = _mm_subs_epu8(p0_16x8, p1_16x8);
    temp2 = _mm_subs_epu8(p1_16x8, p0_16x8);
    temp1 = _mm_add_epi8(temp1, temp2);

    temp2 = _mm_unpacklo_epi8(temp1, zero);
    temp1 = _mm_unpackhi_epi8(temp1, zero);

    temp2 = _mm_cmpgt_epi16(Beta_8x16, temp2);
    temp1 = _mm_cmpgt_epi16(Beta_8x16, temp1);

    flag2_16x8 = _mm_packs_epi16(temp2, temp1);

    // !((ABS(p0 - q0) < alpha) || (ABS(q1 - q0) < beta) || (ABS(p1 - p0) < beta))
    flag1_16x8 = _mm_and_si128(flag1_16x8, flag2_16x8);

    // (ABS(p0 - q0) < ((alpha >> 2) + 2))
    temp1 = _mm_subs_epu8(p0_16x8, q0_16x8);
    temp2 = _mm_subs_epu8(q0_16x8, p0_16x8);
    temp1 = _mm_add_epi8(temp1, temp2);
    Alpha_8x16 = _mm_srai_epi16(Alpha_8x16, 2);
    Alpha_8x16 = _mm_add_epi16(Alpha_8x16, const_val2_16x8);

    temp2 = _mm_unpacklo_epi8(temp1, zero);
    temp1 = _mm_unpackhi_epi8(temp1, zero);
    temp2 = _mm_cmpgt_epi16(Alpha_8x16, temp2);
    temp1 = _mm_cmpgt_epi16(Alpha_8x16, temp1);

    flag2_16x8 = _mm_packs_epi16(temp2, temp1);
    flag2_16x8 = _mm_and_si128(flag1_16x8, flag2_16x8);

    // (ABS(p2 - p0) < beta)
    temp1 = _mm_subs_epu8(p0_16x8, p2_16x8);
    temp2 = _mm_subs_epu8(p2_16x8, p0_16x8);
    temp1 = _mm_add_epi8(temp1, temp2);

    temp2 = _mm_unpacklo_epi8(temp1, zero);
    temp1 = _mm_unpackhi_epi8(temp1, zero);
    temp2 = _mm_cmpgt_epi16(Beta_8x16, temp2);
    temp1 = _mm_cmpgt_epi16(Beta_8x16, temp1);

    flag3_16x8 = _mm_packs_epi16(temp2, temp1);
    flag3_16x8 = _mm_and_si128(flag3_16x8, flag2_16x8);

    // (ABS(q2 - q0) < beta)
    temp1 = _mm_subs_epu8(q0_16x8, q2_16x8);
    temp2 = _mm_subs_epu8(q2_16x8, q0_16x8);
    temp1 = _mm_add_epi8(temp1, temp2);

    temp2 = _mm_unpacklo_epi8(temp1, zero);
    temp1 = _mm_unpackhi_epi8(temp1, zero);
    temp2 = _mm_cmpgt_epi16(Beta_8x16, temp2);
    temp1 = _mm_cmpgt_epi16(Beta_8x16, temp1);

    flag4_16x8 = _mm_packs_epi16(temp2, temp1);
    flag4_16x8 = _mm_and_si128(flag4_16x8, flag2_16x8);

    // First 8 pixels
    p3_8x16 = _mm_unpacklo_epi8(p3_16x8, zero);
    p2_8x16 = _mm_unpacklo_epi8(p2_16x8, zero);
    p1_8x16 = _mm_unpacklo_epi8(p1_16x8, zero);
    p0_8x16 = _mm_unpacklo_epi8(p0_16x8, zero);
    q0_8x16 = _mm_unpacklo_epi8(q0_16x8, zero);
    q1_8x16 = _mm_unpacklo_epi8(q1_16x8, zero);
    q2_8x16 = _mm_unpacklo_epi8(q2_16x8, zero);
    q3_8x16 = _mm_unpacklo_epi8(q3_16x8, zero);

    // p0_1 and q0_1
    temp1 = _mm_add_epi16(p0_8x16, q1_8x16);
    temp2 = _mm_add_epi16(p1_8x16, q0_8x16);
    temp5 = _mm_add_epi16(temp1, const_val2_16x8);
    temp6 = _mm_add_epi16(temp2, const_val2_16x8);
    temp3 = _mm_slli_epi16(p1_8x16, 1);
    temp4 = _mm_slli_epi16(q1_8x16, 1);
    temp1 = _mm_add_epi16(temp5, temp3);
    temp2 = _mm_add_epi16(temp6, temp4);
    p0_16x8_1 = _mm_srai_epi16(temp1, 2);
    q0_16x8_1 = _mm_srai_epi16(temp2, 2);

    // p1_2 and q1_2
    temp6 = _mm_add_epi16(temp6, p0_8x16);
    temp5 = _mm_add_epi16(temp5, q0_8x16);
    temp1 = _mm_add_epi16(temp6, p2_8x16);
    temp2 = _mm_add_epi16(temp5, q2_8x16);
    p1_16x8_2 = _mm_srai_epi16(temp1, 2);
    q1_16x8_2 = _mm_srai_epi16(temp2, 2);

    // p0_2 and q0_2
    temp1 = _mm_add_epi16(temp3, p2_8x16);
    temp2 = _mm_add_epi16(temp4, q2_8x16);
    temp1 = _mm_add_epi16(temp1, q1_8x16);
    temp2 = _mm_add_epi16(temp2, p1_8x16);
    temp3 = _mm_add_epi16(p0_8x16, q0_8x16);
    temp3 = _mm_slli_epi16(temp3, 1);
    temp1 = _mm_add_epi16(temp1, temp3);
    temp2 = _mm_add_epi16(temp2, temp3);
    temp1 = _mm_add_epi16(temp1, _mm_set1_epi16(4));
    temp2 = _mm_add_epi16(temp2, _mm_set1_epi16(4));
    p0_16x8_2 = _mm_srai_epi16(temp1, 3);
    q0_16x8_2 = _mm_srai_epi16(temp2, 3);

    // p2_2 and q2_2
    temp1 = _mm_add_epi16(temp6, const_val2_16x8);
    temp2 = _mm_add_epi16(temp5, const_val2_16x8);
    temp3 = _mm_slli_epi16(p2_8x16, 1);
    temp4 = _mm_slli_epi16(q2_8x16, 1);
    temp3 = _mm_add_epi16(p2_8x16, temp3);
    temp4 = _mm_add_epi16(q2_8x16, temp4);
    temp5 = _mm_slli_epi16(p3_8x16, 1);
    temp6 = _mm_slli_epi16(q3_8x16, 1);
    temp1 = _mm_add_epi16(temp1, temp3);
    temp2 = _mm_add_epi16(temp2, temp4);
    temp1 = _mm_add_epi16(temp1, temp5);
    temp2 = _mm_add_epi16(temp2, temp6);
    p2_16x8_2 = _mm_srai_epi16(temp1, 3);
    q2_16x8_2 = _mm_srai_epi16(temp2, 3);

    // Second 8 pixels and packing with first 8 pixels
    p3_8x16 = _mm_unpackhi_epi8(p3_16x8, zero);
    p2_8x16 = _mm_unpackhi_epi8(p2_16x8, zero);
    p1_8x16 = _mm_unpackhi_epi8(p1_16x8, zero);
    p0_8x16 = _mm_unpackhi_epi8(p0_16x8, zero);
    q0_8x16 = _mm_unpackhi_epi8(q0_16x8, zero);
    q1_8x16 = _mm_unpackhi_epi8(q1_16x8, zero);
    q2_8x16 = _mm_unpackhi_epi8(q2_16x8, zero);
    q3_8x16 = _mm_unpackhi_epi8(q3_16x8, zero);

    // p0_1 and q0_1
    temp1 = _mm_add_epi16(p0_8x16, q1_8x16);
    temp2 = _mm_add_epi16(p1_8x16, q0_8x16);
    temp5 = _mm_add_epi16(temp1, const_val2_16x8);
    temp6 = _mm_add_epi16(temp2, const_val2_16x8);
    temp3 = _mm_slli_epi16(p1_8x16, 1);
    temp4 = _mm_slli_epi16(q1_8x16, 1);
    temp1 = _mm_add_epi16(temp5, temp3);
    temp2 = _mm_add_epi16(temp6, temp4);
    temp1 = _mm_srai_epi16(temp1, 2);
    temp2 = _mm_srai_epi16(temp2, 2);
    p0_16x8_1 = _mm_packus_epi16(p0_16x8_1, temp1);
    q0_16x8_1 = _mm_packus_epi16(q0_16x8_1, temp2);

    // p1_2 and q1_2
    temp6 = _mm_add_epi16(temp6, p0_8x16);
    temp5 = _mm_add_epi16(temp5, q0_8x16);
    temp1 = _mm_add_epi16(temp6, p2_8x16);
    temp2 = _mm_add_epi16(temp5, q2_8x16);
    temp1 = _mm_srai_epi16(temp1, 2);
    temp2 = _mm_srai_epi16(temp2, 2);
    p1_16x8_2 = _mm_packus_epi16(p1_16x8_2, temp1);
    q1_16x8_2 = _mm_packus_epi16(q1_16x8_2, temp2);

    // p0_2 and q0_2
    temp1 = _mm_add_epi16(temp3, p2_8x16);
    temp2 = _mm_add_epi16(temp4, q2_8x16);
    temp1 = _mm_add_epi16(temp1, q1_8x16);
    temp2 = _mm_add_epi16(temp2, p1_8x16);
    temp3 = _mm_add_epi16(p0_8x16, q0_8x16);
    temp3 = _mm_slli_epi16(temp3, 1);
    temp1 = _mm_add_epi16(temp1, temp3);
    temp2 = _mm_add_epi16(temp2, temp3);
    temp1 = _mm_add_epi16(temp1, _mm_set1_epi16(4));
    temp2 = _mm_add_epi16(temp2, _mm_set1_epi16(4));
    temp1 = _mm_srai_epi16(temp1, 3);
    temp2 = _mm_srai_epi16(temp2, 3);
    p0_16x8_2 = _mm_packus_epi16(p0_16x8_2, temp1);
    q0_16x8_2 = _mm_packus_epi16(q0_16x8_2, temp2);

    // p2_2 and q2_2
    temp1 = _mm_add_epi16(temp6, const_val2_16x8);
    temp2 = _mm_add_epi16(temp5, const_val2_16x8);
    temp3 = _mm_slli_epi16(p2_8x16, 1);
    temp4 = _mm_slli_epi16(q2_8x16, 1);
    temp3 = _mm_add_epi16(p2_8x16, temp3);
    temp4 = _mm_add_epi16(q2_8x16, temp4);
    temp5 = _mm_slli_epi16(p3_8x16, 1);
    temp6 = _mm_slli_epi16(q3_8x16, 1);
    temp1 = _mm_add_epi16(temp1, temp3);
    temp2 = _mm_add_epi16(temp2, temp4);
    temp1 = _mm_add_epi16(temp1, temp5);
    temp2 = _mm_add_epi16(temp2, temp6);
    temp1 = _mm_srai_epi16(temp1, 3);
    temp2 = _mm_srai_epi16(temp2, 3);
    p2_16x8_2 = _mm_packus_epi16(p2_16x8_2, temp1);
    q2_16x8_2 = _mm_packus_epi16(q2_16x8_2, temp2);

    // p0 and q0
    p0_16x8 = _mm_and_si128(p0_16x8,
                            _mm_xor_si128(flag1_16x8, _mm_set1_epi8(0xFF)));
    p0_16x8_1 = _mm_and_si128(p0_16x8_1, flag1_16x8);
    p0_16x8 = _mm_add_epi8(p0_16x8, p0_16x8_1);
    q0_16x8 = _mm_and_si128(q0_16x8,
                            _mm_xor_si128(flag1_16x8, _mm_set1_epi8(0xFF)));
    q0_16x8_1 = _mm_and_si128(q0_16x8_1, flag1_16x8);
    q0_16x8 = _mm_add_epi8(q0_16x8, q0_16x8_1);

    // p0 and q0
    p0_16x8 = _mm_and_si128(p0_16x8,
                            _mm_xor_si128(flag3_16x8, _mm_set1_epi8(0xFF)));
    p0_16x8_2 = _mm_and_si128(p0_16x8_2, flag3_16x8);
    p0_16x8 = _mm_add_epi8(p0_16x8, p0_16x8_2);
    q0_16x8 = _mm_and_si128(q0_16x8,
                            _mm_xor_si128(flag4_16x8, _mm_set1_epi8(0xFF)));
    q0_16x8_2 = _mm_and_si128(q0_16x8_2, flag4_16x8);
    q0_16x8 = _mm_add_epi8(q0_16x8, q0_16x8_2);

    // p1 and q1
    p1_16x8 = _mm_and_si128(p1_16x8,
                            _mm_xor_si128(flag3_16x8, _mm_set1_epi8(0xFF)));
    p1_16x8_2 = _mm_and_si128(p1_16x8_2, flag3_16x8);
    p1_16x8 = _mm_add_epi8(p1_16x8, p1_16x8_2);
    q1_16x8 = _mm_and_si128(q1_16x8,
                            _mm_xor_si128(flag4_16x8, _mm_set1_epi8(0xFF)));
    q1_16x8_2 = _mm_and_si128(q1_16x8_2, flag4_16x8);
    q1_16x8 = _mm_add_epi8(q1_16x8, q1_16x8_2);

    // p2 and q2
    p2_16x8 = _mm_and_si128(p2_16x8,
                            _mm_xor_si128(flag3_16x8, _mm_set1_epi8(0xFF)));
    p2_16x8_2 = _mm_and_si128(p2_16x8_2, flag3_16x8);
    p2_16x8 = _mm_add_epi8(p2_16x8, p2_16x8_2);
    q2_16x8 = _mm_and_si128(q2_16x8,
                            _mm_xor_si128(flag4_16x8, _mm_set1_epi8(0xFF)));
    q2_16x8_2 = _mm_and_si128(q2_16x8_2, flag4_16x8);
    q2_16x8 = _mm_add_epi8(q2_16x8, q2_16x8_2);

    temp1 = _mm_unpacklo_epi8(p3_16x8, p2_16x8);
    temp2 = _mm_unpacklo_epi8(p1_16x8, p0_16x8);
    temp3 = _mm_unpacklo_epi8(q0_16x8, q1_16x8);
    temp4 = _mm_unpacklo_epi8(q2_16x8, q3_16x8);

    p3_8x16 = _mm_unpacklo_epi16(temp1, temp2);
    p2_8x16 = _mm_unpackhi_epi16(temp1, temp2);
    q2_8x16 = _mm_unpacklo_epi16(temp3, temp4);
    q3_8x16 = _mm_unpackhi_epi16(temp3, temp4);

    line1 = _mm_unpacklo_epi32(p3_8x16, q2_8x16);
    line2 = _mm_srli_si128(line1, 8);
    line3 = _mm_unpackhi_epi32(p3_8x16, q2_8x16);
    line4 = _mm_srli_si128(line3, 8);
    line5 = _mm_unpacklo_epi32(p2_8x16, q3_8x16);
    line6 = _mm_srli_si128(line5, 8);
    line7 = _mm_unpackhi_epi32(p2_8x16, q3_8x16);
    line8 = _mm_srli_si128(line7, 8);

    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 0 * src_strd), line1);
    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 1 * src_strd), line2);
    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 2 * src_strd), line3);
    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 3 * src_strd), line4);
    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 4 * src_strd), line5);
    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 5 * src_strd), line6);
    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 6 * src_strd), line7);
    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 7 * src_strd), line8);

    temp1 = _mm_unpackhi_epi8(p3_16x8, p2_16x8);
    temp2 = _mm_unpackhi_epi8(p1_16x8, p0_16x8);
    temp3 = _mm_unpackhi_epi8(q0_16x8, q1_16x8);
    temp4 = _mm_unpackhi_epi8(q2_16x8, q3_16x8);

    p3_8x16 = _mm_unpacklo_epi16(temp1, temp2);
    p2_8x16 = _mm_unpackhi_epi16(temp1, temp2);
    q2_8x16 = _mm_unpacklo_epi16(temp3, temp4);
    q3_8x16 = _mm_unpackhi_epi16(temp3, temp4);

    line1 = _mm_unpacklo_epi32(p3_8x16, q2_8x16);
    line2 = _mm_srli_si128(line1, 8);
    line3 = _mm_unpackhi_epi32(p3_8x16, q2_8x16);
    line4 = _mm_srli_si128(line3, 8);
    line5 = _mm_unpacklo_epi32(p2_8x16, q3_8x16);
    line6 = _mm_srli_si128(line5, 8);
    line7 = _mm_unpackhi_epi32(p2_8x16, q3_8x16);
    line8 = _mm_srli_si128(line7, 8);

    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 8 * src_strd), line1);
    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 9 * src_strd), line2);
    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 10 * src_strd), line3);
    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 11 * src_strd), line4);
    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 12 * src_strd), line5);
    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 13 * src_strd), line6);
    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 14 * src_strd), line7);
    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 15 * src_strd), line8);

}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_deblk_luma_horz_bs4_ssse3()                        */
/*                                                                           */
/*  Description   : This function performs filtering of a luma block         */
/*                  horizontal edge when the boundary strength is set to 4.  */
/*                                                                           */
/*  Inputs        : pu1_src    - pointer to the src sample q0                */
/*                  src_strd   - source stride                               */
/*                  alpha      - alpha value for the boundary                */
/*                  beta       - beta value for the boundary                 */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : This operation is described in Sec. 8.7.2.4 under the    */
/*                  title "Filtering process for edges for bS equal to 4" in */
/*                  ITU T Rec H.264.                                         */
/*                                                                           */
/*  Outputs       : None                                                     */
/*                                                                           */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         12 02 2015   Naveen Kumar P  Initial version                      */
/*                                                                           */
/*****************************************************************************/
void ih264_deblk_luma_horz_bs4_ssse3(UWORD8 *pu1_src,
                                     WORD32 src_strd,
                                     WORD32 alpha,
                                     WORD32 beta)
{
    WORD16 i16_posP3, i16_posP2, i16_posP1, i16_posP0;
    WORD16 i16_posQ1, i16_posQ2, i16_posQ3;
    UWORD8 *pu1_HorzPixel;
    __m128i zero = _mm_setzero_si128();
    __m128i q0_16x8, q1_16x8, q2_16x8, q3_16x8;
    __m128i p0_16x8, p1_16x8, p2_16x8, p3_16x8;
    __m128i q0_8x16, q1_8x16, q2_8x16, q3_8x16;
    __m128i p0_8x16, p1_8x16, p2_8x16, p3_8x16;
    __m128i q0_16x8_1;
    __m128i p0_16x8_1;
    __m128i q0_16x8_2, q1_16x8_2, q2_16x8_2;
    __m128i p0_16x8_2, p1_16x8_2, p2_16x8_2;
    __m128i temp1, temp2, temp3, temp4, temp5, temp6;
    __m128i Alpha_8x16, Beta_8x16;
    __m128i flag1_16x8, flag2_16x8, flag3_16x8, flag4_16x8;
    __m128i const_val2_16x8 = _mm_set1_epi16(2);

    pu1_HorzPixel = pu1_src - (src_strd << 2);

    i16_posQ1 = src_strd;
    i16_posQ2 = X2(src_strd);
    i16_posQ3 = X3(src_strd);
    i16_posP0 = X3(src_strd);
    i16_posP1 = X2(src_strd);
    i16_posP2 = src_strd;
    i16_posP3 = 0;

    Alpha_8x16 = _mm_set1_epi16(alpha);
    Beta_8x16 = _mm_set1_epi16(beta);

    p3_16x8 = _mm_loadu_si128((__m128i *)(pu1_HorzPixel + i16_posP3));
    p2_16x8 = _mm_loadu_si128((__m128i *)(pu1_HorzPixel + i16_posP2));
    p1_16x8 = _mm_loadu_si128((__m128i *)(pu1_HorzPixel + i16_posP1));
    p0_16x8 = _mm_loadu_si128((__m128i *)(pu1_HorzPixel + i16_posP0));
    q0_16x8 = _mm_loadu_si128((__m128i *)(pu1_src));
    q1_16x8 = _mm_loadu_si128((__m128i *)(pu1_src + i16_posQ1));
    q2_16x8 = _mm_loadu_si128((__m128i *)(pu1_src + i16_posQ2));
    q3_16x8 = _mm_loadu_si128((__m128i *)(pu1_src + i16_posQ3));

    //Cond1 (ABS(p0 - q0) < alpha)
    temp1 = _mm_subs_epu8(q0_16x8, p0_16x8);
    temp2 = _mm_subs_epu8(p0_16x8, q0_16x8);
    temp1 = _mm_add_epi8(temp1, temp2);

    temp2 = _mm_unpacklo_epi8(temp1, zero);
    temp1 = _mm_unpackhi_epi8(temp1, zero);

    temp2 = _mm_cmpgt_epi16(Alpha_8x16, temp2);
    temp1 = _mm_cmpgt_epi16(Alpha_8x16, temp1);

    flag1_16x8 = _mm_packs_epi16(temp2, temp1);

    //Cond2 (ABS(q1 - q0) < beta)
    temp1 = _mm_subs_epu8(q0_16x8, q1_16x8);
    temp2 = _mm_subs_epu8(q1_16x8, q0_16x8);
    temp1 = _mm_add_epi8(temp1, temp2);

    temp2 = _mm_unpacklo_epi8(temp1, zero);
    temp1 = _mm_unpackhi_epi8(temp1, zero);

    temp2 = _mm_cmpgt_epi16(Beta_8x16, temp2);
    temp1 = _mm_cmpgt_epi16(Beta_8x16, temp1);

    flag2_16x8 = _mm_packs_epi16(temp2, temp1);

    flag1_16x8 = _mm_and_si128(flag1_16x8, flag2_16x8);

    //Cond3 (ABS(p1 - p0) < beta)
    temp1 = _mm_subs_epu8(p0_16x8, p1_16x8);
    temp2 = _mm_subs_epu8(p1_16x8, p0_16x8);
    temp1 = _mm_add_epi8(temp1, temp2);

    temp2 = _mm_unpacklo_epi8(temp1, zero);
    temp1 = _mm_unpackhi_epi8(temp1, zero);

    temp2 = _mm_cmpgt_epi16(Beta_8x16, temp2);
    temp1 = _mm_cmpgt_epi16(Beta_8x16, temp1);

    flag2_16x8 = _mm_packs_epi16(temp2, temp1);

    // !((ABS(p0 - q0) < alpha) || (ABS(q1 - q0) < beta) || (ABS(p1 - p0) < beta))
    flag1_16x8 = _mm_and_si128(flag1_16x8, flag2_16x8);

    // (ABS(p0 - q0) < ((alpha >> 2) + 2))
    temp1 = _mm_subs_epu8(p0_16x8, q0_16x8);
    temp2 = _mm_subs_epu8(q0_16x8, p0_16x8);
    temp1 = _mm_add_epi8(temp1, temp2);
    Alpha_8x16 = _mm_srai_epi16(Alpha_8x16, 2);
    Alpha_8x16 = _mm_add_epi16(Alpha_8x16, const_val2_16x8);

    temp2 = _mm_unpacklo_epi8(temp1, zero);
    temp1 = _mm_unpackhi_epi8(temp1, zero);
    temp2 = _mm_cmpgt_epi16(Alpha_8x16, temp2);
    temp1 = _mm_cmpgt_epi16(Alpha_8x16, temp1);

    flag2_16x8 = _mm_packs_epi16(temp2, temp1);
    flag2_16x8 = _mm_and_si128(flag1_16x8, flag2_16x8);

    // (ABS(p2 - p0) < beta)
    temp1 = _mm_subs_epu8(p0_16x8, p2_16x8);
    temp2 = _mm_subs_epu8(p2_16x8, p0_16x8);
    temp1 = _mm_add_epi8(temp1, temp2);

    temp2 = _mm_unpacklo_epi8(temp1, zero);
    temp1 = _mm_unpackhi_epi8(temp1, zero);
    temp2 = _mm_cmpgt_epi16(Beta_8x16, temp2);
    temp1 = _mm_cmpgt_epi16(Beta_8x16, temp1);

    flag3_16x8 = _mm_packs_epi16(temp2, temp1);
    flag3_16x8 = _mm_and_si128(flag3_16x8, flag2_16x8);

    // (ABS(q2 - q0) < beta)
    temp1 = _mm_subs_epu8(q0_16x8, q2_16x8);
    temp2 = _mm_subs_epu8(q2_16x8, q0_16x8);
    temp1 = _mm_add_epi8(temp1, temp2);

    temp2 = _mm_unpacklo_epi8(temp1, zero);
    temp1 = _mm_unpackhi_epi8(temp1, zero);
    temp2 = _mm_cmpgt_epi16(Beta_8x16, temp2);
    temp1 = _mm_cmpgt_epi16(Beta_8x16, temp1);

    flag4_16x8 = _mm_packs_epi16(temp2, temp1);
    flag4_16x8 = _mm_and_si128(flag4_16x8, flag2_16x8);

    // First 8 pixels
    p3_8x16 = _mm_unpacklo_epi8(p3_16x8, zero);
    p2_8x16 = _mm_unpacklo_epi8(p2_16x8, zero);
    p1_8x16 = _mm_unpacklo_epi8(p1_16x8, zero);
    p0_8x16 = _mm_unpacklo_epi8(p0_16x8, zero);
    q0_8x16 = _mm_unpacklo_epi8(q0_16x8, zero);
    q1_8x16 = _mm_unpacklo_epi8(q1_16x8, zero);
    q2_8x16 = _mm_unpacklo_epi8(q2_16x8, zero);
    q3_8x16 = _mm_unpacklo_epi8(q3_16x8, zero);

    // p0_1 and q0_1
    temp1 = _mm_add_epi16(p0_8x16, q1_8x16);
    temp2 = _mm_add_epi16(p1_8x16, q0_8x16);
    temp5 = _mm_add_epi16(temp1, const_val2_16x8);
    temp6 = _mm_add_epi16(temp2, const_val2_16x8);
    temp3 = _mm_slli_epi16(p1_8x16, 1);
    temp4 = _mm_slli_epi16(q1_8x16, 1);
    temp1 = _mm_add_epi16(temp5, temp3);
    temp2 = _mm_add_epi16(temp6, temp4);
    p0_16x8_1 = _mm_srai_epi16(temp1, 2);
    q0_16x8_1 = _mm_srai_epi16(temp2, 2);

    // p1_2 and q1_2
    temp6 = _mm_add_epi16(temp6, p0_8x16);
    temp5 = _mm_add_epi16(temp5, q0_8x16);
    temp1 = _mm_add_epi16(temp6, p2_8x16);
    temp2 = _mm_add_epi16(temp5, q2_8x16);
    p1_16x8_2 = _mm_srai_epi16(temp1, 2);
    q1_16x8_2 = _mm_srai_epi16(temp2, 2);

    // p0_2 and q0_2
    temp1 = _mm_add_epi16(temp3, p2_8x16);
    temp2 = _mm_add_epi16(temp4, q2_8x16);
    temp1 = _mm_add_epi16(temp1, q1_8x16);
    temp2 = _mm_add_epi16(temp2, p1_8x16);
    temp3 = _mm_add_epi16(p0_8x16, q0_8x16);
    temp3 = _mm_slli_epi16(temp3, 1);
    temp1 = _mm_add_epi16(temp1, temp3);
    temp2 = _mm_add_epi16(temp2, temp3);
    temp1 = _mm_add_epi16(temp1, _mm_set1_epi16(4));
    temp2 = _mm_add_epi16(temp2, _mm_set1_epi16(4));
    p0_16x8_2 = _mm_srai_epi16(temp1, 3);
    q0_16x8_2 = _mm_srai_epi16(temp2, 3);

    // p2_2 and q2_2
    temp1 = _mm_add_epi16(temp6, const_val2_16x8);
    temp2 = _mm_add_epi16(temp5, const_val2_16x8);
    temp3 = _mm_slli_epi16(p2_8x16, 1);
    temp4 = _mm_slli_epi16(q2_8x16, 1);
    temp3 = _mm_add_epi16(p2_8x16, temp3);
    temp4 = _mm_add_epi16(q2_8x16, temp4);
    temp5 = _mm_slli_epi16(p3_8x16, 1);
    temp6 = _mm_slli_epi16(q3_8x16, 1);
    temp1 = _mm_add_epi16(temp1, temp3);
    temp2 = _mm_add_epi16(temp2, temp4);
    temp1 = _mm_add_epi16(temp1, temp5);
    temp2 = _mm_add_epi16(temp2, temp6);
    p2_16x8_2 = _mm_srai_epi16(temp1, 3);
    q2_16x8_2 = _mm_srai_epi16(temp2, 3);

    // Second 8 pixels and packing with first 8 pixels
    p3_8x16 = _mm_unpackhi_epi8(p3_16x8, zero);
    p2_8x16 = _mm_unpackhi_epi8(p2_16x8, zero);
    p1_8x16 = _mm_unpackhi_epi8(p1_16x8, zero);
    p0_8x16 = _mm_unpackhi_epi8(p0_16x8, zero);
    q0_8x16 = _mm_unpackhi_epi8(q0_16x8, zero);
    q1_8x16 = _mm_unpackhi_epi8(q1_16x8, zero);
    q2_8x16 = _mm_unpackhi_epi8(q2_16x8, zero);
    q3_8x16 = _mm_unpackhi_epi8(q3_16x8, zero);

    // p0_1 and q0_1
    temp1 = _mm_add_epi16(p0_8x16, q1_8x16);
    temp2 = _mm_add_epi16(p1_8x16, q0_8x16);
    temp5 = _mm_add_epi16(temp1, const_val2_16x8);
    temp6 = _mm_add_epi16(temp2, const_val2_16x8);
    temp3 = _mm_slli_epi16(p1_8x16, 1);
    temp4 = _mm_slli_epi16(q1_8x16, 1);
    temp1 = _mm_add_epi16(temp5, temp3);
    temp2 = _mm_add_epi16(temp6, temp4);
    temp1 = _mm_srai_epi16(temp1, 2);
    temp2 = _mm_srai_epi16(temp2, 2);
    p0_16x8_1 = _mm_packus_epi16(p0_16x8_1, temp1);
    q0_16x8_1 = _mm_packus_epi16(q0_16x8_1, temp2);

    // p1_2 and q1_2
    temp6 = _mm_add_epi16(temp6, p0_8x16);
    temp5 = _mm_add_epi16(temp5, q0_8x16);
    temp1 = _mm_add_epi16(temp6, p2_8x16);
    temp2 = _mm_add_epi16(temp5, q2_8x16);
    temp1 = _mm_srai_epi16(temp1, 2);
    temp2 = _mm_srai_epi16(temp2, 2);
    p1_16x8_2 = _mm_packus_epi16(p1_16x8_2, temp1);
    q1_16x8_2 = _mm_packus_epi16(q1_16x8_2, temp2);

    // p0_2 and q0_2
    temp1 = _mm_add_epi16(temp3, p2_8x16);
    temp2 = _mm_add_epi16(temp4, q2_8x16);
    temp1 = _mm_add_epi16(temp1, q1_8x16);
    temp2 = _mm_add_epi16(temp2, p1_8x16);
    temp3 = _mm_add_epi16(p0_8x16, q0_8x16);
    temp3 = _mm_slli_epi16(temp3, 1);
    temp1 = _mm_add_epi16(temp1, temp3);
    temp2 = _mm_add_epi16(temp2, temp3);
    temp1 = _mm_add_epi16(temp1, _mm_set1_epi16(4));
    temp2 = _mm_add_epi16(temp2, _mm_set1_epi16(4));
    temp1 = _mm_srai_epi16(temp1, 3);
    temp2 = _mm_srai_epi16(temp2, 3);
    p0_16x8_2 = _mm_packus_epi16(p0_16x8_2, temp1);
    q0_16x8_2 = _mm_packus_epi16(q0_16x8_2, temp2);

    // p2_2 and q2_2
    temp1 = _mm_add_epi16(temp6, const_val2_16x8);
    temp2 = _mm_add_epi16(temp5, const_val2_16x8);
    temp3 = _mm_slli_epi16(p2_8x16, 1);
    temp4 = _mm_slli_epi16(q2_8x16, 1);
    temp3 = _mm_add_epi16(p2_8x16, temp3);
    temp4 = _mm_add_epi16(q2_8x16, temp4);
    temp5 = _mm_slli_epi16(p3_8x16, 1);
    temp6 = _mm_slli_epi16(q3_8x16, 1);
    temp1 = _mm_add_epi16(temp1, temp3);
    temp2 = _mm_add_epi16(temp2, temp4);
    temp1 = _mm_add_epi16(temp1, temp5);
    temp2 = _mm_add_epi16(temp2, temp6);
    temp1 = _mm_srai_epi16(temp1, 3);
    temp2 = _mm_srai_epi16(temp2, 3);
    p2_16x8_2 = _mm_packus_epi16(p2_16x8_2, temp1);
    q2_16x8_2 = _mm_packus_epi16(q2_16x8_2, temp2);

    // p0 and q0
    p0_16x8 = _mm_and_si128(p0_16x8,
                            _mm_xor_si128(flag1_16x8, _mm_set1_epi8(0xFF)));
    p0_16x8_1 = _mm_and_si128(p0_16x8_1, flag1_16x8);
    p0_16x8 = _mm_add_epi8(p0_16x8, p0_16x8_1);
    q0_16x8 = _mm_and_si128(q0_16x8,
                            _mm_xor_si128(flag1_16x8, _mm_set1_epi8(0xFF)));
    q0_16x8_1 = _mm_and_si128(q0_16x8_1, flag1_16x8);
    q0_16x8 = _mm_add_epi8(q0_16x8, q0_16x8_1);

    // p0 and q0
    p0_16x8 = _mm_and_si128(p0_16x8,
                            _mm_xor_si128(flag3_16x8, _mm_set1_epi8(0xFF)));
    p0_16x8_2 = _mm_and_si128(p0_16x8_2, flag3_16x8);
    p0_16x8 = _mm_add_epi8(p0_16x8, p0_16x8_2);
    q0_16x8 = _mm_and_si128(q0_16x8,
                            _mm_xor_si128(flag4_16x8, _mm_set1_epi8(0xFF)));
    q0_16x8_2 = _mm_and_si128(q0_16x8_2, flag4_16x8);
    q0_16x8 = _mm_add_epi8(q0_16x8, q0_16x8_2);

    // p1 and q1
    p1_16x8 = _mm_and_si128(p1_16x8,
                            _mm_xor_si128(flag3_16x8, _mm_set1_epi8(0xFF)));
    p1_16x8_2 = _mm_and_si128(p1_16x8_2, flag3_16x8);
    p1_16x8 = _mm_add_epi8(p1_16x8, p1_16x8_2);
    q1_16x8 = _mm_and_si128(q1_16x8,
                            _mm_xor_si128(flag4_16x8, _mm_set1_epi8(0xFF)));
    q1_16x8_2 = _mm_and_si128(q1_16x8_2, flag4_16x8);
    q1_16x8 = _mm_add_epi8(q1_16x8, q1_16x8_2);

    // p2 and q2
    p2_16x8 = _mm_and_si128(p2_16x8,
                            _mm_xor_si128(flag3_16x8, _mm_set1_epi8(0xFF)));
    p2_16x8_2 = _mm_and_si128(p2_16x8_2, flag3_16x8);
    p2_16x8 = _mm_add_epi8(p2_16x8, p2_16x8_2);
    q2_16x8 = _mm_and_si128(q2_16x8,
                            _mm_xor_si128(flag4_16x8, _mm_set1_epi8(0xFF)));
    q2_16x8_2 = _mm_and_si128(q2_16x8_2, flag4_16x8);
    q2_16x8 = _mm_add_epi8(q2_16x8, q2_16x8_2);

    _mm_storeu_si128((__m128i *)(pu1_HorzPixel + i16_posP2), p2_16x8);
    _mm_storeu_si128((__m128i *)(pu1_HorzPixel + i16_posP1), p1_16x8);
    _mm_storeu_si128((__m128i *)(pu1_HorzPixel + i16_posP0), p0_16x8);

    _mm_storeu_si128((__m128i *)(pu1_src), q0_16x8);
    _mm_storeu_si128((__m128i *)(pu1_src + i16_posQ1), q1_16x8);
    _mm_storeu_si128((__m128i *)(pu1_src + i16_posQ2), q2_16x8);

}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_deblk_luma_vert_bslt4_ssse3()                      */
/*                                                                           */
/*  Description   : This function performs filtering of a luma block         */
/*                  vertical edge when the boundary strength is less than 4. */
/*                                                                           */
/*  Inputs        : pu1_src       - pointer to the src sample q0             */
/*                  src_strd      - source stride                            */
/*                  alpha         - alpha value for the boundary             */
/*                  beta          - beta value for the boundary              */
/*                  u4_bs         - packed Boundary strength array           */
/*                  pu1_cliptab   - tc0_table                                */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : This operation is described in Sec. 8.7.2.3 under the    */
/*                  title "Filtering process for edges for bS less than 4"   */
/*                  in ITU T Rec H.264.                                      */
/*                                                                           */
/*  Outputs       : None                                                     */
/*                                                                           */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         12 02 2015   Naveen Kumar P  Initial version                      */
/*                                                                           */
/*****************************************************************************/
void ih264_deblk_luma_vert_bslt4_ssse3(UWORD8 *pu1_src,
                                       WORD32 src_strd,
                                       WORD32 alpha,
                                       WORD32 beta,
                                       UWORD32 u4_bs,
                                       const UWORD8 *pu1_cliptab)
{
    UWORD8 u1_Bs, u1_Bs1;

    WORD32 j = 0;

    __m128i linea, lineb, linec, lined, linee, linef, lineg, lineh;
    __m128i int1, int2, int3, int4, high1, high2;
    __m128i flag, flag1, i_C, i_C0;
    __m128i i_Ap, i_Aq, diff, const1, const2, in_macro, in_macrotemp, temp,
                    temp1;
    __m128i zero = _mm_setzero_si128();

    for(j = 0; j <= 8 * src_strd; j += 8 * src_strd)
    {
        //Transpose
        linea = _mm_loadl_epi64((__m128i *)(pu1_src - 3 + j));
        lineb = _mm_loadl_epi64((__m128i *)(pu1_src - 3 + src_strd + j));
        linec = _mm_loadl_epi64((__m128i *)(pu1_src - 3 + 2 * src_strd + j));
        lined = _mm_loadl_epi64((__m128i *)(pu1_src - 3 + 3 * src_strd + j));

        linea = _mm_unpacklo_epi8(linea, zero);
        lineb = _mm_unpacklo_epi8(lineb, zero);
        linec = _mm_unpacklo_epi8(linec, zero);
        lined = _mm_unpacklo_epi8(lined, zero);

        int1 = _mm_unpacklo_epi16(linea, lineb);
        lineb = _mm_unpackhi_epi16(linea, lineb);

        int2 = _mm_unpacklo_epi16(linec, lined);
        lined = _mm_unpackhi_epi16(linec, lined);

        linea = _mm_unpacklo_epi16(int1, int2);
        int1 = _mm_unpackhi_epi16(int1, int2);

        linec = _mm_unpacklo_epi16(lineb, lined);
        high1 = _mm_unpackhi_epi16(lineb, lined);

        linee = _mm_loadl_epi64((__m128i *)(pu1_src - 3 + 4 * src_strd + j));
        linef = _mm_loadl_epi64((__m128i *)(pu1_src - 3 + 5 * src_strd + j));
        lineg = _mm_loadl_epi64((__m128i *)(pu1_src - 3 + 6 * src_strd + j));
        lineh = _mm_loadl_epi64((__m128i *)(pu1_src - 3 + 7 * src_strd + j));

        linee = _mm_unpacklo_epi8(linee, zero);
        linef = _mm_unpacklo_epi8(linef, zero);
        lineg = _mm_unpacklo_epi8(lineg, zero);
        lineh = _mm_unpacklo_epi8(lineh, zero);

        int2 = _mm_unpacklo_epi16(linee, linef);
        linef = _mm_unpackhi_epi16(linee, linef);

        int3 = _mm_unpacklo_epi16(lineg, lineh);
        lineh = _mm_unpackhi_epi16(lineg, lineh);

        linee = _mm_unpacklo_epi16(int2, int3);
        int2 = _mm_unpackhi_epi16(int2, int3);

        lineg = _mm_unpacklo_epi16(linef, lineh);
        high2 = _mm_unpackhi_epi16(linef, lineh);

        int4 = _mm_unpacklo_epi16(linea, linee);
        lineb = _mm_unpackhi_epi16(linea, linee);

        int3 = _mm_unpacklo_epi16(int1, int2);
        lined = _mm_unpackhi_epi16(int1, int2);

        int2 = _mm_unpacklo_epi16(linec, lineg);
        linef = _mm_unpackhi_epi16(linec, lineg);

        linea = int4;
        linec = int3;
        linee = int2;

        lineg = _mm_unpacklo_epi16(high1, high2);
        lineh = _mm_unpackhi_epi16(high1, high2);

        //end of transpose

        u1_Bs = (u4_bs >> 24) & 0xff;
        u1_Bs1 = (u4_bs >> 16) & 0xff;
        u4_bs <<= 16;

        flag1 = _mm_set_epi16(u1_Bs1, u1_Bs, u1_Bs1, u1_Bs, u1_Bs1, u1_Bs,
                              u1_Bs1, u1_Bs);
        flag1 = _mm_cmpeq_epi16(flag1, zero); //Set flag to 1s and 0s
        flag1 = _mm_xor_si128(flag1, _mm_set1_epi16(0xFFFF)); //Invert for required mask

        i_C0 = _mm_set_epi16(pu1_cliptab[u1_Bs1], pu1_cliptab[u1_Bs],
                             pu1_cliptab[u1_Bs1], pu1_cliptab[u1_Bs],
                             pu1_cliptab[u1_Bs1], pu1_cliptab[u1_Bs],
                             pu1_cliptab[u1_Bs1], pu1_cliptab[u1_Bs]);

        diff = _mm_subs_epi16(linec, lined); //Condn 1
        diff = _mm_abs_epi16(diff);
        const1 = _mm_set1_epi16(alpha);
        flag = _mm_cmpgt_epi16(const1, diff);

        diff = _mm_subs_epi16(linee, lined); //Condtn 2
        diff = _mm_abs_epi16(diff);
        const1 = _mm_set1_epi16(beta);
        flag = _mm_and_si128(flag, _mm_cmpgt_epi16(const1, diff));

        diff = _mm_subs_epi16(lineb, linec); //Condtn 3
        diff = _mm_abs_epi16(diff);
        flag = _mm_and_si128(flag, _mm_cmpgt_epi16(const1, diff)); //Const 1= Beta from now on

        flag = _mm_and_si128(flag, flag1); //Final flag (ui_B condition + other 3 conditions)

        //Adding Ap<Beta and Aq<Beta
        i_Ap = _mm_subs_epi16(linea, linec);
        i_Ap = _mm_abs_epi16(i_Ap);
        const2 = _mm_cmpgt_epi16(const1, i_Ap);
        const2 = _mm_subs_epi16(zero, const2); //Make FFFF=1 and 0000=0
        i_C = _mm_add_epi16(i_C0, const2);

        i_Aq = _mm_subs_epi16(linef, lined);
        i_Aq = _mm_abs_epi16(i_Aq);
        const2 = _mm_cmpgt_epi16(const1, i_Aq);
        const2 = _mm_subs_epi16(zero, const2);
        i_C = _mm_add_epi16(i_C, const2);

        //Calculate in_macro
        diff = _mm_subs_epi16(lined, linec);
        diff = _mm_slli_epi16(diff, 2);
        const2 = _mm_subs_epi16(lineb, linee);
        diff = _mm_add_epi16(diff, const2);
        const2 = _mm_set1_epi16(4);
        diff = _mm_add_epi16(diff, const2);
        in_macro = _mm_srai_epi16(diff, 3);

        in_macro = _mm_min_epi16(i_C, in_macro); //CLIP3
        i_C = _mm_subs_epi16(zero, i_C);
        in_macro = _mm_max_epi16(i_C, in_macro);

        //Compute and store
        in_macrotemp = _mm_add_epi16(linec, in_macro);
        in_macrotemp = _mm_and_si128(in_macrotemp, flag);
        temp = _mm_and_si128(linec,
                             _mm_xor_si128(flag, _mm_set1_epi16(0xFFFF)));
        temp = _mm_add_epi16(temp, in_macrotemp);
        //temp= _mm_packus_epi16 (temp, zero);
        //_mm_storel_epi64(uc_HorzPixel+i16_posP0+i, in_macrotemp);

        in_macrotemp = _mm_subs_epi16(lined, in_macro);
        in_macrotemp = _mm_and_si128(in_macrotemp, flag);
        temp1 = _mm_and_si128(lined,
                              _mm_xor_si128(flag, _mm_set1_epi16(0xFFFF)));
        temp1 = _mm_add_epi16(temp1, in_macrotemp);
        //temp1= _mm_packus_epi16 (temp1, zero);
        //_mm_storel_epi64(pu1_src+i, in_macrotemp);

        //If Ap<Beta
        flag1 = _mm_cmpgt_epi16(const1, i_Ap);
        flag1 = _mm_and_si128(flag, flag1);
        in_macrotemp = _mm_add_epi16(linec, lined);
        in_macrotemp = _mm_add_epi16(in_macrotemp, _mm_set1_epi16(1));
        in_macrotemp = _mm_srai_epi16(in_macrotemp, 1);
        in_macro = _mm_add_epi16(in_macrotemp, linea);
        in_macro = _mm_subs_epi16(in_macro, _mm_slli_epi16(lineb, 1));
        in_macro = _mm_srai_epi16(in_macro, 1);

        in_macro = _mm_min_epi16(i_C0, in_macro); //CLIP3
        i_C0 = _mm_subs_epi16(zero, i_C0);
        in_macro = _mm_max_epi16(i_C0, in_macro);

        in_macro = _mm_and_si128(in_macro, flag1);
        lineb = _mm_add_epi16(lineb, in_macro);
        //in_macro= _mm_packus_epi16 (i_p1, zero);
        //_mm_storel_epi64(uc_HorzPixel+i16_posP1+i, in_macro);

        flag1 = _mm_cmpgt_epi16(const1, i_Aq);
        flag1 = _mm_and_si128(flag, flag1);
        in_macro = _mm_add_epi16(in_macrotemp, linef);
        in_macro = _mm_subs_epi16(in_macro, _mm_slli_epi16(linee, 1));
        in_macro = _mm_srai_epi16(in_macro, 1);

        i_C0 = _mm_abs_epi16(i_C0);
        in_macro = _mm_min_epi16(i_C0, in_macro); //CLIP3
        i_C0 = _mm_subs_epi16(zero, i_C0);
        in_macro = _mm_max_epi16(i_C0, in_macro);

        in_macro = _mm_and_si128(in_macro, flag1);
        linee = _mm_add_epi16(linee, in_macro);
        //in_macro= _mm_packus_epi16 (i_q1, zero);
        //_mm_storel_epi64(pu1_src+i16_posQ1+i, in_macro);
        linec = temp;
        lined = temp1;
        //End of filtering

        int1 = _mm_unpacklo_epi16(linea, linee);
        linee = _mm_unpackhi_epi16(linea, linee);

        int2 = _mm_unpacklo_epi16(linec, lineg);
        lineg = _mm_unpackhi_epi16(linec, lineg);

        linea = _mm_unpacklo_epi16(int1, int2);
        int3 = _mm_unpackhi_epi16(int1, int2);

        linec = _mm_unpacklo_epi16(linee, lineg);
        lineg = _mm_unpackhi_epi16(linee, lineg);

        int1 = _mm_unpacklo_epi16(lineb, linef);
        linef = _mm_unpackhi_epi16(lineb, linef);

        int2 = _mm_unpacklo_epi16(lined, lineh);
        lineh = _mm_unpackhi_epi16(lined, lineh);

        lineb = _mm_unpacklo_epi16(int1, int2);
        int4 = _mm_unpackhi_epi16(int1, int2);

        lined = _mm_unpacklo_epi16(linef, lineh);
        lineh = _mm_unpackhi_epi16(linef, lineh);

        int1 = _mm_unpackhi_epi16(linea, lineb);
        linea = _mm_unpacklo_epi16(linea, lineb);

        int2 = _mm_unpacklo_epi16(int3, int4);
        high1 = _mm_unpackhi_epi16(int3, int4);

        lineb = _mm_unpacklo_epi16(linec, lined);
        linef = _mm_unpackhi_epi16(linec, lined);

        lined = _mm_unpacklo_epi16(lineg, lineh);
        lineh = _mm_unpackhi_epi16(lineg, lineh);

        linee = int1;
        lineg = high1;
        linec = int2;
        //End of inverse transpose

        //Packs and stores
        linea = _mm_packus_epi16(linea, zero);
        _mm_storel_epi64((__m128i *)(pu1_src - 3 + j), linea);

        lineb = _mm_packus_epi16(lineb, zero);
        _mm_storel_epi64((__m128i *)(pu1_src - 3 + src_strd + j), lineb);

        linec = _mm_packus_epi16(linec, zero);
        _mm_storel_epi64((__m128i *)(pu1_src - 3 + 2 * src_strd + j), linec);

        lined = _mm_packus_epi16(lined, zero);
        _mm_storel_epi64((__m128i *)(pu1_src - 3 + 3 * src_strd + j), lined);

        linee = _mm_packus_epi16(linee, zero);
        _mm_storel_epi64((__m128i *)(pu1_src - 3 + 4 * src_strd + j), linee);

        linef = _mm_packus_epi16(linef, zero);
        _mm_storel_epi64((__m128i *)(pu1_src - 3 + 5 * src_strd + j), linef);

        lineg = _mm_packus_epi16(lineg, zero);
        _mm_storel_epi64((__m128i *)(pu1_src - 3 + 6 * src_strd + j), lineg);

        lineh = _mm_packus_epi16(lineh, zero);
        _mm_storel_epi64((__m128i *)(pu1_src - 3 + 7 * src_strd + j), lineh);

    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_deblk_luma_horz_bslt4_ssse3()                      */
/*                                                                           */
/*  Description   : This function performs filtering of a luma block         */
/*                  horizontal edge when boundary strength is less than 4.   */
/*                                                                           */
/*  Inputs        : pu1_src       - pointer to the src sample q0             */
/*                  src_strd      - source stride                            */
/*                  alpha         - alpha value for the boundary             */
/*                  beta          - beta value for the boundary              */
/*                  u4_bs         - packed Boundary strength array           */
/*                  pu1_cliptab   - tc0_table                                */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : This operation is described in Sec. 8.7.2.3 under the    */
/*                  title "Filtering process for edges for bS less than 4"   */
/*                  in ITU T Rec H.264.                                      */
/*                                                                           */
/*  Outputs       : None                                                     */
/*                                                                           */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         12 02 2015   Naveen Kumar P  Initial version                      */
/*                                                                           */
/*****************************************************************************/
void ih264_deblk_luma_horz_bslt4_ssse3(UWORD8 *pu1_src,
                                       WORD32 src_strd,
                                       WORD32 alpha,
                                       WORD32 beta,
                                       UWORD32 u4_bs,
                                       const UWORD8 *pu1_cliptab)
{
    WORD16 i16_posP2, i16_posP1, i16_posP0, i16_posQ1, i16_posQ2;
    UWORD8 *pu1_HorzPixel;
    __m128i zero = _mm_setzero_si128();
    __m128i bs_flag_16x8b, C0_16x8, C0_8x16, C0_hi_8x16, C_8x16, C_hi_8x16;
    __m128i q0_16x8, q1_16x8, q2_16x8, p0_16x8, p1_16x8, p2_16x8;
    __m128i temp1, temp2;
    __m128i Alpha_8x16, Beta_8x16, flag1_16x8, flag2_16x8, flag3_16x8;
    __m128i in_macro_16x8, in_macro_hi_16x8;
    __m128i const_val4_8x16;
    UWORD8 u1_Bs0, u1_Bs1, u1_Bs2, u1_Bs3;
    UWORD8 clip0, clip1, clip2, clip3;

    pu1_HorzPixel = pu1_src - (src_strd << 2);

    i16_posQ1 = src_strd;
    i16_posQ2 = X2(src_strd);
    i16_posP0 = X3(src_strd);
    i16_posP1 = X2(src_strd);
    i16_posP2 = src_strd;

    q0_16x8 = _mm_loadu_si128((__m128i *)(pu1_src));
    q1_16x8 = _mm_loadu_si128((__m128i *)(pu1_src + i16_posQ1));

    u1_Bs0 = (u4_bs >> 24) & 0xff;
    u1_Bs1 = (u4_bs >> 16) & 0xff;
    u1_Bs2 = (u4_bs >> 8) & 0xff;
    u1_Bs3 = (u4_bs >> 0) & 0xff;
    clip0 = pu1_cliptab[u1_Bs0];
    clip1 = pu1_cliptab[u1_Bs1];
    clip2 = pu1_cliptab[u1_Bs2];
    clip3 = pu1_cliptab[u1_Bs3];

    Alpha_8x16 = _mm_set1_epi16(alpha);
    Beta_8x16 = _mm_set1_epi16(beta);

    bs_flag_16x8b = _mm_set_epi8(u1_Bs3, u1_Bs3, u1_Bs3, u1_Bs3, u1_Bs2, u1_Bs2,
                                 u1_Bs2, u1_Bs2, u1_Bs1, u1_Bs1, u1_Bs1, u1_Bs1,
                                 u1_Bs0, u1_Bs0, u1_Bs0, u1_Bs0);

    C0_16x8 = _mm_set_epi8(clip3, clip3, clip3, clip3, clip2, clip2, clip2,
                           clip2, clip1, clip1, clip1, clip1, clip0, clip0,
                           clip0, clip0);

    bs_flag_16x8b = _mm_cmpeq_epi8(bs_flag_16x8b, zero);
    bs_flag_16x8b = _mm_xor_si128(bs_flag_16x8b, _mm_set1_epi8(0xFF)); //Invert for required mask
    C0_8x16 = _mm_unpacklo_epi8(C0_16x8, zero);
    C0_hi_8x16 = _mm_unpackhi_epi8(C0_16x8, zero);

    p1_16x8 = _mm_loadu_si128((__m128i *)(pu1_HorzPixel + i16_posP1));
    p0_16x8 = _mm_loadu_si128((__m128i *)(pu1_HorzPixel + i16_posP0));
    p2_16x8 = _mm_loadu_si128((__m128i *)(pu1_HorzPixel + i16_posP2));
    q2_16x8 = _mm_loadu_si128((__m128i *)(pu1_src + i16_posQ2));

    //Cond1 (ABS(p0 - q0) < alpha)
    temp1 = _mm_subs_epu8(q0_16x8, p0_16x8);
    temp2 = _mm_subs_epu8(p0_16x8, q0_16x8);
    temp1 = _mm_add_epi8(temp1, temp2);

    temp2 = _mm_unpacklo_epi8(temp1, zero);
    temp1 = _mm_unpackhi_epi8(temp1, zero);

    temp2 = _mm_cmpgt_epi16(Alpha_8x16, temp2);
    temp1 = _mm_cmpgt_epi16(Alpha_8x16, temp1);

    flag1_16x8 = _mm_packs_epi16(temp2, temp1);
    flag1_16x8 = _mm_and_si128(flag1_16x8, bs_flag_16x8b);

    //Cond2 (ABS(q1 - q0) < beta)
    temp1 = _mm_subs_epu8(q0_16x8, q1_16x8);
    temp2 = _mm_subs_epu8(q1_16x8, q0_16x8);
    temp1 = _mm_add_epi8(temp1, temp2);

    temp2 = _mm_unpacklo_epi8(temp1, zero);
    temp1 = _mm_unpackhi_epi8(temp1, zero);

    temp2 = _mm_cmpgt_epi16(Beta_8x16, temp2);
    temp1 = _mm_cmpgt_epi16(Beta_8x16, temp1);

    flag2_16x8 = _mm_packs_epi16(temp2, temp1);

    flag1_16x8 = _mm_and_si128(flag1_16x8, flag2_16x8);

    //Cond3 (ABS(p1 - p0) < beta)
    temp1 = _mm_subs_epu8(p0_16x8, p1_16x8);
    temp2 = _mm_subs_epu8(p1_16x8, p0_16x8);
    temp1 = _mm_add_epi8(temp1, temp2);

    temp2 = _mm_unpacklo_epi8(temp1, zero);
    temp1 = _mm_unpackhi_epi8(temp1, zero);

    temp2 = _mm_cmpgt_epi16(Beta_8x16, temp2);
    temp1 = _mm_cmpgt_epi16(Beta_8x16, temp1);

    flag2_16x8 = _mm_packs_epi16(temp2, temp1);

    // !((ABS(p0 - q0) < alpha) || (ABS(q1 - q0) < beta) || (ABS(p1 - p0) < beta))
    flag1_16x8 = _mm_and_si128(flag1_16x8, flag2_16x8);

    // (ABS(p2 - p0) < beta)
    temp1 = _mm_subs_epu8(p0_16x8, p2_16x8);
    temp2 = _mm_subs_epu8(p2_16x8, p0_16x8);
    temp1 = _mm_add_epi8(temp1, temp2);

    temp2 = _mm_unpacklo_epi8(temp1, zero);
    temp1 = _mm_unpackhi_epi8(temp1, zero);
    temp2 = _mm_cmpgt_epi16(Beta_8x16, temp2);
    temp1 = _mm_cmpgt_epi16(Beta_8x16, temp1);

    flag2_16x8 = _mm_packs_epi16(temp2, temp1);
    flag2_16x8 = _mm_and_si128(flag1_16x8, flag2_16x8);

    temp2 = _mm_subs_epi16(zero, temp2);
    temp1 = _mm_subs_epi16(zero, temp1);

    C_8x16 = _mm_add_epi16(C0_8x16, temp2);
    C_hi_8x16 = _mm_add_epi16(C0_hi_8x16, temp1);

    // (ABS(q2 - q0) < beta)
    temp1 = _mm_subs_epu8(q0_16x8, q2_16x8);
    temp2 = _mm_subs_epu8(q2_16x8, q0_16x8);
    temp1 = _mm_add_epi8(temp1, temp2);

    temp2 = _mm_unpacklo_epi8(temp1, zero);
    temp1 = _mm_unpackhi_epi8(temp1, zero);
    temp2 = _mm_cmpgt_epi16(Beta_8x16, temp2);
    temp1 = _mm_cmpgt_epi16(Beta_8x16, temp1);

    flag3_16x8 = _mm_packs_epi16(temp2, temp1);
    flag3_16x8 = _mm_and_si128(flag1_16x8, flag3_16x8);

    temp2 = _mm_subs_epi16(zero, temp2);
    temp1 = _mm_subs_epi16(zero, temp1);

    C_8x16 = _mm_add_epi16(C_8x16, temp2);
    C_hi_8x16 = _mm_add_epi16(C_hi_8x16, temp1);

    const_val4_8x16 = _mm_set1_epi16(4);
    temp1 = _mm_subs_epi16(_mm_unpacklo_epi8(q0_16x8, zero),
                           _mm_unpacklo_epi8(p0_16x8, zero));
    temp2 = _mm_subs_epi16(_mm_unpacklo_epi8(p1_16x8, zero),
                           _mm_unpacklo_epi8(q1_16x8, zero));
    temp1 = _mm_slli_epi16(temp1, 2);
    temp1 = _mm_add_epi16(temp1, temp2);
    temp1 = _mm_add_epi16(temp1, const_val4_8x16);
    in_macro_16x8 = _mm_srai_epi16(temp1, 3);

    temp1 = _mm_subs_epi16(_mm_unpackhi_epi8(q0_16x8, zero),
                           _mm_unpackhi_epi8(p0_16x8, zero));
    temp2 = _mm_subs_epi16(_mm_unpackhi_epi8(p1_16x8, zero),
                           _mm_unpackhi_epi8(q1_16x8, zero));
    temp1 = _mm_slli_epi16(temp1, 2);
    temp1 = _mm_add_epi16(temp1, temp2);
    temp1 = _mm_add_epi16(temp1, const_val4_8x16);
    in_macro_hi_16x8 = _mm_srai_epi16(temp1, 3);

    in_macro_16x8 = _mm_min_epi16(C_8x16, in_macro_16x8); //CLIP3
    in_macro_hi_16x8 = _mm_min_epi16(C_hi_8x16, in_macro_hi_16x8); //CLIP3
    C_8x16 = _mm_subs_epi16(zero, C_8x16);
    C_hi_8x16 = _mm_subs_epi16(zero, C_hi_8x16);
    in_macro_16x8 = _mm_max_epi16(C_8x16, in_macro_16x8); //CLIP3
    in_macro_hi_16x8 = _mm_max_epi16(C_hi_8x16, in_macro_hi_16x8); //CLIP3

    temp1 = _mm_add_epi16(_mm_unpacklo_epi8(p0_16x8, zero), in_macro_16x8);
    temp2 = _mm_add_epi16(_mm_unpackhi_epi8(p0_16x8, zero), in_macro_hi_16x8);

    temp1 = _mm_packus_epi16(temp1, temp2);

    temp1 = _mm_and_si128(temp1, flag1_16x8);
    temp2 = _mm_and_si128(p0_16x8,
                          _mm_xor_si128(flag1_16x8, _mm_set1_epi16(0xFFFF)));

    temp1 = _mm_add_epi8(temp1, temp2);

    _mm_storeu_si128((__m128i *)(pu1_HorzPixel + i16_posP0), temp1);

    temp1 = _mm_sub_epi16(_mm_unpacklo_epi8(q0_16x8, zero), in_macro_16x8);
    temp2 = _mm_sub_epi16(_mm_unpackhi_epi8(q0_16x8, zero), in_macro_hi_16x8);

    temp1 = _mm_packus_epi16(temp1, temp2);

    temp1 = _mm_and_si128(temp1, flag1_16x8);
    temp2 = _mm_and_si128(q0_16x8,
                          _mm_xor_si128(flag1_16x8, _mm_set1_epi16(0xFFFF)));

    temp1 = _mm_add_epi8(temp1, temp2);
    _mm_storeu_si128((__m128i *)(pu1_src), temp1);

    //if(Ap < Beta)
    temp1 = _mm_avg_epu16(_mm_unpacklo_epi8(q0_16x8, zero),
                          _mm_unpacklo_epi8(p0_16x8, zero));
    temp2 = _mm_slli_epi16(_mm_unpacklo_epi8(p1_16x8, zero), 1);
    //temp2 = _mm_subs_epi16(zero,temp2);
    temp2 = _mm_subs_epi16(_mm_unpacklo_epi8(p2_16x8, zero), temp2);
    temp2 = _mm_add_epi16(temp1, temp2);
    in_macro_16x8 = _mm_srai_epi16(temp2, 1);

    temp1 = _mm_avg_epu16(_mm_unpackhi_epi8(q0_16x8, zero),
                          _mm_unpackhi_epi8(p0_16x8, zero));
    temp2 = _mm_slli_epi16(_mm_unpackhi_epi8(p1_16x8, zero), 1);
    //temp2 = _mm_subs_epi16(zero,temp2);
    temp2 = _mm_subs_epi16(_mm_unpackhi_epi8(p2_16x8, zero), temp2);
    temp2 = _mm_add_epi16(temp1, temp2);
    in_macro_hi_16x8 = _mm_srai_epi16(temp2, 1);

    in_macro_16x8 = _mm_min_epi16(C0_8x16, in_macro_16x8); //CLIP3
    in_macro_hi_16x8 = _mm_min_epi16(C0_hi_8x16, in_macro_hi_16x8); //CLIP3
    C0_8x16 = _mm_subs_epi16(zero, C0_8x16);
    C0_hi_8x16 = _mm_subs_epi16(zero, C0_hi_8x16);
    in_macro_16x8 = _mm_max_epi16(C0_8x16, in_macro_16x8); //CLIP3
    in_macro_hi_16x8 = _mm_max_epi16(C0_hi_8x16, in_macro_hi_16x8); //CLIP3

    temp1 = _mm_add_epi16(_mm_unpacklo_epi8(p1_16x8, zero), in_macro_16x8);
    temp2 = _mm_add_epi16(_mm_unpackhi_epi8(p1_16x8, zero), in_macro_hi_16x8);

    temp1 = _mm_packus_epi16(temp1, temp2);

    temp1 = _mm_and_si128(temp1, flag2_16x8);
    temp2 = _mm_and_si128(p1_16x8,
                          _mm_xor_si128(flag2_16x8, _mm_set1_epi16(0xFFFF)));
    temp1 = _mm_add_epi8(temp1, temp2);
    _mm_storeu_si128((__m128i *)(pu1_HorzPixel + i16_posP1), temp1);

    //if(Aq < Beta)
    temp1 = _mm_avg_epu16(_mm_unpacklo_epi8(q0_16x8, zero),
                          _mm_unpacklo_epi8(p0_16x8, zero));
    temp2 = _mm_slli_epi16(_mm_unpacklo_epi8(q1_16x8, zero), 1);
    //temp2 = _mm_slli_epi16 (temp2, 1);
    temp2 = _mm_subs_epi16(_mm_unpacklo_epi8(q2_16x8, zero), temp2);
    temp2 = _mm_add_epi16(temp1, temp2);
    in_macro_16x8 = _mm_srai_epi16(temp2, 1);

    temp1 = _mm_avg_epu16(_mm_unpackhi_epi8(q0_16x8, zero),
                          _mm_unpackhi_epi8(p0_16x8, zero));
    temp2 = _mm_slli_epi16(_mm_unpackhi_epi8(q1_16x8, zero), 1);
    //temp2 = _mm_slli_epi16 (temp2, 1);
    temp2 = _mm_subs_epi16(_mm_unpackhi_epi8(q2_16x8, zero), temp2);
    temp2 = _mm_add_epi16(temp1, temp2);
    in_macro_hi_16x8 = _mm_srai_epi16(temp2, 1);

    in_macro_16x8 = _mm_max_epi16(C0_8x16, in_macro_16x8); //CLIP3
    in_macro_hi_16x8 = _mm_max_epi16(C0_hi_8x16, in_macro_hi_16x8); //CLIP3
    C0_8x16 = _mm_subs_epi16(zero, C0_8x16);
    C0_hi_8x16 = _mm_subs_epi16(zero, C0_hi_8x16);
    in_macro_16x8 = _mm_min_epi16(C0_8x16, in_macro_16x8); //CLIP3
    in_macro_hi_16x8 = _mm_min_epi16(C0_hi_8x16, in_macro_hi_16x8); //CLIP3

    temp1 = _mm_add_epi16(_mm_unpacklo_epi8(q1_16x8, zero), in_macro_16x8);
    temp2 = _mm_add_epi16(_mm_unpackhi_epi8(q1_16x8, zero), in_macro_hi_16x8);

    temp1 = _mm_packus_epi16(temp1, temp2);

    temp1 = _mm_and_si128(temp1, flag3_16x8);
    temp2 = _mm_and_si128(q1_16x8,
                          _mm_xor_si128(flag3_16x8, _mm_set1_epi16(0xFFFF)));
    temp1 = _mm_add_epi8(temp1, temp2);

    _mm_storeu_si128((__m128i *)(pu1_src + i16_posQ1), temp1);

}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_deblk_luma_vert_bs4_mbaff_ssse3()                  */
/*                                                                           */
/*  Description   : This function performs filtering of a luma block         */
/*                  vertical edge when boundary strength is set to 4.        */
/*                                                                           */
/*  Inputs        : pu1_src       - pointer to the src sample q0             */
/*                  src_strd      - source stride                            */
/*                  alpha         - alpha value for the boundary             */
/*                  beta          - beta value for the boundary              */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : When the function is called twice, this operation is as  */
/*                  described in Sec. 8.7.2.3 under the title "Filtering     */
/*                  process for edges for bS equal to 4" in ITU T Rec H.264. */
/*                                                                           */
/*  Outputs       : None                                                     */
/*                                                                           */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         12 02 2015   Naveen Kumar P  Initial version                      */
/*                                                                           */
/*****************************************************************************/
void ih264_deblk_luma_vert_bs4_mbaff_ssse3(UWORD8 *pu1_src,
                                           WORD32 src_strd,
                                           WORD32 alpha,
                                           WORD32 beta)
{
    __m128i zero = _mm_setzero_si128();
    __m128i q0_16x8, q1_16x8, q2_16x8, q3_16x8;
    __m128i p0_16x8, p1_16x8, p2_16x8, p3_16x8;
    __m128i q0_8x16, q1_8x16, q2_8x16, q3_8x16;
    __m128i p0_8x16, p1_8x16, p2_8x16, p3_8x16;
    __m128i q0_16x8_1;
    __m128i p0_16x8_1;
    __m128i q0_16x8_2, q1_16x8_2, q2_16x8_2;
    __m128i p0_16x8_2, p1_16x8_2, p2_16x8_2;
    __m128i temp1, temp2, temp3, temp4, temp5, temp6;
    __m128i Alpha_8x16, Beta_8x16;
    __m128i flag1_16x8, flag2_16x8, flag3_16x8, flag4_16x8;
    __m128i const_val2_16x8 = _mm_set1_epi16(2);
    __m128i line1, line2, line3, line4, line5, line6, line7, line8;

    Alpha_8x16 = _mm_set1_epi16(alpha);
    Beta_8x16 = _mm_set1_epi16(beta);

    line1 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 0 * src_strd));
    line2 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 1 * src_strd));
    line3 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 2 * src_strd));
    line4 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 3 * src_strd));
    line5 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 4 * src_strd));
    line6 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 5 * src_strd));
    line7 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 6 * src_strd));
    line8 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 7 * src_strd));

    temp1 = _mm_unpacklo_epi8(line1, line2);
    temp2 = _mm_unpacklo_epi8(line3, line4);
    temp3 = _mm_unpacklo_epi8(line5, line6);
    temp4 = _mm_unpacklo_epi8(line7, line8);

    line1 = _mm_unpacklo_epi16(temp1, temp2);
    line2 = _mm_unpackhi_epi16(temp1, temp2);
    line3 = _mm_unpacklo_epi16(temp3, temp4);
    line4 = _mm_unpackhi_epi16(temp3, temp4);

    p1_8x16 = _mm_unpacklo_epi32(line1, line3);
    p0_8x16 = _mm_unpackhi_epi32(line1, line3);
    q0_8x16 = _mm_unpacklo_epi32(line2, line4);
    q1_8x16 = _mm_unpackhi_epi32(line2, line4);

    p3_16x8 = _mm_unpacklo_epi64(p1_8x16, zero);
    p2_16x8 = _mm_unpackhi_epi64(p1_8x16, zero);
    q2_16x8 = _mm_unpacklo_epi64(q1_8x16, zero);
    q3_16x8 = _mm_unpackhi_epi64(q1_8x16, zero);
    p1_16x8 = _mm_unpacklo_epi64(p0_8x16, zero);
    p0_16x8 = _mm_unpackhi_epi64(p0_8x16, zero);
    q0_16x8 = _mm_unpacklo_epi64(q0_8x16, zero);
    q1_16x8 = _mm_unpackhi_epi64(q0_8x16, zero);

    //Cond1 (ABS(p0 - q0) < alpha)
    temp1 = _mm_subs_epu8(q0_16x8, p0_16x8);
    temp2 = _mm_subs_epu8(p0_16x8, q0_16x8);
    temp1 = _mm_add_epi8(temp1, temp2);

    temp2 = _mm_unpacklo_epi8(temp1, zero);
    temp1 = _mm_unpackhi_epi8(temp1, zero);

    temp2 = _mm_cmpgt_epi16(Alpha_8x16, temp2);
    temp1 = _mm_cmpgt_epi16(Alpha_8x16, temp1);

    flag1_16x8 = _mm_packs_epi16(temp2, temp1);

    //Cond2 (ABS(q1 - q0) < beta)
    temp1 = _mm_subs_epu8(q0_16x8, q1_16x8);
    temp2 = _mm_subs_epu8(q1_16x8, q0_16x8);
    temp1 = _mm_add_epi8(temp1, temp2);

    temp2 = _mm_unpacklo_epi8(temp1, zero);
    temp1 = _mm_unpackhi_epi8(temp1, zero);

    temp2 = _mm_cmpgt_epi16(Beta_8x16, temp2);
    temp1 = _mm_cmpgt_epi16(Beta_8x16, temp1);

    flag2_16x8 = _mm_packs_epi16(temp2, temp1);

    flag1_16x8 = _mm_and_si128(flag1_16x8, flag2_16x8);

    //Cond3 (ABS(p1 - p0) < beta)
    temp1 = _mm_subs_epu8(p0_16x8, p1_16x8);
    temp2 = _mm_subs_epu8(p1_16x8, p0_16x8);
    temp1 = _mm_add_epi8(temp1, temp2);

    temp2 = _mm_unpacklo_epi8(temp1, zero);
    temp1 = _mm_unpackhi_epi8(temp1, zero);

    temp2 = _mm_cmpgt_epi16(Beta_8x16, temp2);
    temp1 = _mm_cmpgt_epi16(Beta_8x16, temp1);

    flag2_16x8 = _mm_packs_epi16(temp2, temp1);

    // !((ABS(p0 - q0) < alpha) || (ABS(q1 - q0) < beta) || (ABS(p1 - p0) < beta))
    flag1_16x8 = _mm_and_si128(flag1_16x8, flag2_16x8);

    // (ABS(p0 - q0) < ((alpha >> 2) + 2))
    temp1 = _mm_subs_epu8(p0_16x8, q0_16x8);
    temp2 = _mm_subs_epu8(q0_16x8, p0_16x8);
    temp1 = _mm_add_epi8(temp1, temp2);
    Alpha_8x16 = _mm_srai_epi16(Alpha_8x16, 2);
    Alpha_8x16 = _mm_add_epi16(Alpha_8x16, const_val2_16x8);

    temp2 = _mm_unpacklo_epi8(temp1, zero);
    temp1 = _mm_unpackhi_epi8(temp1, zero);
    temp2 = _mm_cmpgt_epi16(Alpha_8x16, temp2);
    temp1 = _mm_cmpgt_epi16(Alpha_8x16, temp1);

    flag2_16x8 = _mm_packs_epi16(temp2, temp1);
    flag2_16x8 = _mm_and_si128(flag1_16x8, flag2_16x8);

    // (ABS(p2 - p0) < beta)
    temp1 = _mm_subs_epu8(p0_16x8, p2_16x8);
    temp2 = _mm_subs_epu8(p2_16x8, p0_16x8);
    temp1 = _mm_add_epi8(temp1, temp2);

    temp2 = _mm_unpacklo_epi8(temp1, zero);
    temp1 = _mm_unpackhi_epi8(temp1, zero);
    temp2 = _mm_cmpgt_epi16(Beta_8x16, temp2);
    temp1 = _mm_cmpgt_epi16(Beta_8x16, temp1);

    flag3_16x8 = _mm_packs_epi16(temp2, temp1);
    flag3_16x8 = _mm_and_si128(flag3_16x8, flag2_16x8);

    // (ABS(q2 - q0) < beta)
    temp1 = _mm_subs_epu8(q0_16x8, q2_16x8);
    temp2 = _mm_subs_epu8(q2_16x8, q0_16x8);
    temp1 = _mm_add_epi8(temp1, temp2);

    temp2 = _mm_unpacklo_epi8(temp1, zero);
    temp1 = _mm_unpackhi_epi8(temp1, zero);
    temp2 = _mm_cmpgt_epi16(Beta_8x16, temp2);
    temp1 = _mm_cmpgt_epi16(Beta_8x16, temp1);

    flag4_16x8 = _mm_packs_epi16(temp2, temp1);
    flag4_16x8 = _mm_and_si128(flag4_16x8, flag2_16x8);

    // First 8 pixels
    p3_8x16 = _mm_unpacklo_epi8(p3_16x8, zero);
    p2_8x16 = _mm_unpacklo_epi8(p2_16x8, zero);
    p1_8x16 = _mm_unpacklo_epi8(p1_16x8, zero);
    p0_8x16 = _mm_unpacklo_epi8(p0_16x8, zero);
    q0_8x16 = _mm_unpacklo_epi8(q0_16x8, zero);
    q1_8x16 = _mm_unpacklo_epi8(q1_16x8, zero);
    q2_8x16 = _mm_unpacklo_epi8(q2_16x8, zero);
    q3_8x16 = _mm_unpacklo_epi8(q3_16x8, zero);

    // p0_1 and q0_1
    temp1 = _mm_add_epi16(p0_8x16, q1_8x16);
    temp2 = _mm_add_epi16(p1_8x16, q0_8x16);
    temp5 = _mm_add_epi16(temp1, const_val2_16x8);
    temp6 = _mm_add_epi16(temp2, const_val2_16x8);
    temp3 = _mm_slli_epi16(p1_8x16, 1);
    temp4 = _mm_slli_epi16(q1_8x16, 1);
    temp1 = _mm_add_epi16(temp5, temp3);
    temp2 = _mm_add_epi16(temp6, temp4);
    p0_16x8_1 = _mm_srai_epi16(temp1, 2);
    q0_16x8_1 = _mm_srai_epi16(temp2, 2);

    // p1_2 and q1_2
    temp6 = _mm_add_epi16(temp6, p0_8x16);
    temp5 = _mm_add_epi16(temp5, q0_8x16);
    temp1 = _mm_add_epi16(temp6, p2_8x16);
    temp2 = _mm_add_epi16(temp5, q2_8x16);
    p1_16x8_2 = _mm_srai_epi16(temp1, 2);
    q1_16x8_2 = _mm_srai_epi16(temp2, 2);

    // p0_2 and q0_2
    temp1 = _mm_add_epi16(temp3, p2_8x16);
    temp2 = _mm_add_epi16(temp4, q2_8x16);
    temp1 = _mm_add_epi16(temp1, q1_8x16);
    temp2 = _mm_add_epi16(temp2, p1_8x16);
    temp3 = _mm_add_epi16(p0_8x16, q0_8x16);
    temp3 = _mm_slli_epi16(temp3, 1);
    temp1 = _mm_add_epi16(temp1, temp3);
    temp2 = _mm_add_epi16(temp2, temp3);
    temp1 = _mm_add_epi16(temp1, _mm_set1_epi16(4));
    temp2 = _mm_add_epi16(temp2, _mm_set1_epi16(4));
    p0_16x8_2 = _mm_srai_epi16(temp1, 3);
    q0_16x8_2 = _mm_srai_epi16(temp2, 3);

    // p2_2 and q2_2
    temp1 = _mm_add_epi16(temp6, const_val2_16x8);
    temp2 = _mm_add_epi16(temp5, const_val2_16x8);
    temp3 = _mm_slli_epi16(p2_8x16, 1);
    temp4 = _mm_slli_epi16(q2_8x16, 1);
    temp3 = _mm_add_epi16(p2_8x16, temp3);
    temp4 = _mm_add_epi16(q2_8x16, temp4);
    temp5 = _mm_slli_epi16(p3_8x16, 1);
    temp6 = _mm_slli_epi16(q3_8x16, 1);
    temp1 = _mm_add_epi16(temp1, temp3);
    temp2 = _mm_add_epi16(temp2, temp4);
    temp1 = _mm_add_epi16(temp1, temp5);
    temp2 = _mm_add_epi16(temp2, temp6);
    p2_16x8_2 = _mm_srai_epi16(temp1, 3);
    q2_16x8_2 = _mm_srai_epi16(temp2, 3);

    // p0_1 and q0_1
    p0_16x8_1 = _mm_packus_epi16(p0_16x8_1, zero);
    q0_16x8_1 = _mm_packus_epi16(q0_16x8_1, zero);

    // p1_2 and q1_2
    p1_16x8_2 = _mm_packus_epi16(p1_16x8_2, zero);
    q1_16x8_2 = _mm_packus_epi16(q1_16x8_2, zero);

    // p0_2 and q0_2
    p0_16x8_2 = _mm_packus_epi16(p0_16x8_2, zero);
    q0_16x8_2 = _mm_packus_epi16(q0_16x8_2, zero);

    // p2_2 and q2_2
    p2_16x8_2 = _mm_packus_epi16(p2_16x8_2, zero);
    q2_16x8_2 = _mm_packus_epi16(q2_16x8_2, zero);

    // p0 and q0
    p0_16x8 = _mm_and_si128(p0_16x8,
                            _mm_xor_si128(flag1_16x8, _mm_set1_epi8(0xFF)));
    p0_16x8_1 = _mm_and_si128(p0_16x8_1, flag1_16x8);
    p0_16x8 = _mm_add_epi8(p0_16x8, p0_16x8_1);
    q0_16x8 = _mm_and_si128(q0_16x8,
                            _mm_xor_si128(flag1_16x8, _mm_set1_epi8(0xFF)));
    q0_16x8_1 = _mm_and_si128(q0_16x8_1, flag1_16x8);
    q0_16x8 = _mm_add_epi8(q0_16x8, q0_16x8_1);

    // p0 and q0
    p0_16x8 = _mm_and_si128(p0_16x8,
                            _mm_xor_si128(flag3_16x8, _mm_set1_epi8(0xFF)));
    p0_16x8_2 = _mm_and_si128(p0_16x8_2, flag3_16x8);
    p0_16x8 = _mm_add_epi8(p0_16x8, p0_16x8_2);
    q0_16x8 = _mm_and_si128(q0_16x8,
                            _mm_xor_si128(flag4_16x8, _mm_set1_epi8(0xFF)));
    q0_16x8_2 = _mm_and_si128(q0_16x8_2, flag4_16x8);
    q0_16x8 = _mm_add_epi8(q0_16x8, q0_16x8_2);

    // p1 and q1
    p1_16x8 = _mm_and_si128(p1_16x8,
                            _mm_xor_si128(flag3_16x8, _mm_set1_epi8(0xFF)));
    p1_16x8_2 = _mm_and_si128(p1_16x8_2, flag3_16x8);
    p1_16x8 = _mm_add_epi8(p1_16x8, p1_16x8_2);
    q1_16x8 = _mm_and_si128(q1_16x8,
                            _mm_xor_si128(flag4_16x8, _mm_set1_epi8(0xFF)));
    q1_16x8_2 = _mm_and_si128(q1_16x8_2, flag4_16x8);
    q1_16x8 = _mm_add_epi8(q1_16x8, q1_16x8_2);

    // p2 and q2
    p2_16x8 = _mm_and_si128(p2_16x8,
                            _mm_xor_si128(flag3_16x8, _mm_set1_epi8(0xFF)));
    p2_16x8_2 = _mm_and_si128(p2_16x8_2, flag3_16x8);
    p2_16x8 = _mm_add_epi8(p2_16x8, p2_16x8_2);
    q2_16x8 = _mm_and_si128(q2_16x8,
                            _mm_xor_si128(flag4_16x8, _mm_set1_epi8(0xFF)));
    q2_16x8_2 = _mm_and_si128(q2_16x8_2, flag4_16x8);
    q2_16x8 = _mm_add_epi8(q2_16x8, q2_16x8_2);

    temp1 = _mm_unpacklo_epi8(p3_16x8, p2_16x8);
    temp2 = _mm_unpacklo_epi8(p1_16x8, p0_16x8);
    temp3 = _mm_unpacklo_epi8(q0_16x8, q1_16x8);
    temp4 = _mm_unpacklo_epi8(q2_16x8, q3_16x8);

    p3_8x16 = _mm_unpacklo_epi16(temp1, temp2);
    p2_8x16 = _mm_unpackhi_epi16(temp1, temp2);
    q2_8x16 = _mm_unpacklo_epi16(temp3, temp4);
    q3_8x16 = _mm_unpackhi_epi16(temp3, temp4);

    line1 = _mm_unpacklo_epi32(p3_8x16, q2_8x16);
    line2 = _mm_srli_si128(line1, 8);
    line3 = _mm_unpackhi_epi32(p3_8x16, q2_8x16);
    line4 = _mm_srli_si128(line3, 8);
    line5 = _mm_unpacklo_epi32(p2_8x16, q3_8x16);
    line6 = _mm_srli_si128(line5, 8);
    line7 = _mm_unpackhi_epi32(p2_8x16, q3_8x16);
    line8 = _mm_srli_si128(line7, 8);

    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 0 * src_strd), line1);
    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 1 * src_strd), line2);
    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 2 * src_strd), line3);
    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 3 * src_strd), line4);
    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 4 * src_strd), line5);
    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 5 * src_strd), line6);
    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 6 * src_strd), line7);
    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 7 * src_strd), line8);

}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_deblk_luma_vert_bslt4_mbaff_ssse3()                */
/*                                                                           */
/*  Description   : This function performs filtering of a luma block         */
/*                  vertical edge when boundary strength is less than 4.     */
/*                                                                           */
/*  Inputs        : pu1_src       - pointer to the src sample q0             */
/*                  src_strd      - source stride                            */
/*                  alpha         - alpha value for the boundary             */
/*                  beta          - beta value for the boundary              */
/*                  u4_bs         - packed Boundary strength array           */
/*                  pu1_cliptab   - tc0_table                                */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : When the function is called twice, this operation is as  */
/*                  described in Sec. 8.7.2.3 under the title "Filtering     */
/*                  process for edges for bS less than 4" in ITU T Rec H.264.*/
/*                                                                           */
/*  Outputs       : None                                                     */
/*                                                                           */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         12 02 2015   Naveen Kumar P  Initial version                      */
/*                                                                           */
/*****************************************************************************/
void ih264_deblk_luma_vert_bslt4_mbaff_ssse3(UWORD8 *pu1_src,
                                             WORD32 src_strd,
                                             WORD32 alpha,
                                             WORD32 beta,
                                             UWORD32 u4_bs,
                                             const UWORD8 *pu1_cliptab)
{
    __m128i zero = _mm_setzero_si128();
    __m128i bs_flag_16x8b, C0_16x8, C0_8x16, C_8x16;
    __m128i q0_16x8, q1_16x8, q2_16x8, q3_16x8;
    __m128i p0_16x8, p1_16x8, p2_16x8, p3_16x8;
    __m128i temp1, temp2, temp3, temp4;
    __m128i Alpha_8x16, Beta_8x16, flag1_16x8, flag2_16x8, flag3_16x8;
    __m128i in_macro_16x8;
    __m128i const_val4_8x16;
    UWORD8 u1_Bs0, u1_Bs1, u1_Bs2, u1_Bs3;
    UWORD8 clip0, clip1, clip2, clip3;
    __m128i line1, line2, line3, line4, line5, line6, line7, line8;
    __m128i q0_16x8_1, q1_16x8_1, q0_16x8_2;
    __m128i p0_16x8_1, p1_16x8_1, p0_16x8_2;

    line1 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 0 * src_strd));
    line2 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 1 * src_strd));
    line3 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 2 * src_strd));
    line4 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 3 * src_strd));
    line5 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 4 * src_strd));
    line6 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 5 * src_strd));
    line7 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 6 * src_strd));
    line8 = _mm_loadl_epi64((__m128i *)(pu1_src - 4 + 7 * src_strd));

    temp1 = _mm_unpacklo_epi8(line1, line2);
    temp2 = _mm_unpacklo_epi8(line3, line4);
    temp3 = _mm_unpacklo_epi8(line5, line6);
    temp4 = _mm_unpacklo_epi8(line7, line8);

    line1 = _mm_unpacklo_epi16(temp1, temp2);
    line2 = _mm_unpackhi_epi16(temp1, temp2);
    line3 = _mm_unpacklo_epi16(temp3, temp4);
    line4 = _mm_unpackhi_epi16(temp3, temp4);

    temp1 = _mm_unpacklo_epi32(line1, line3);
    temp2 = _mm_unpackhi_epi32(line1, line3);
    temp3 = _mm_unpacklo_epi32(line2, line4);
    temp4 = _mm_unpackhi_epi32(line2, line4);

    p3_16x8 = _mm_unpacklo_epi64(temp1, zero);
    p2_16x8 = _mm_unpackhi_epi64(temp1, zero);
    q2_16x8 = _mm_unpacklo_epi64(temp4, zero);
    q3_16x8 = _mm_unpackhi_epi64(temp4, zero);
    p1_16x8 = _mm_unpacklo_epi64(temp2, zero);
    p0_16x8 = _mm_unpackhi_epi64(temp2, zero);
    q0_16x8 = _mm_unpacklo_epi64(temp3, zero);
    q1_16x8 = _mm_unpackhi_epi64(temp3, zero);

    u1_Bs0 = (u4_bs >> 24) & 0xff;
    u1_Bs1 = (u4_bs >> 16) & 0xff;
    u1_Bs2 = (u4_bs >> 8) & 0xff;
    u1_Bs3 = (u4_bs >> 0) & 0xff;
    clip0 = pu1_cliptab[u1_Bs0];
    clip1 = pu1_cliptab[u1_Bs1];
    clip2 = pu1_cliptab[u1_Bs2];
    clip3 = pu1_cliptab[u1_Bs3];

    Alpha_8x16 = _mm_set1_epi16(alpha);
    Beta_8x16 = _mm_set1_epi16(beta);

    bs_flag_16x8b = _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0, u1_Bs3, u1_Bs3, u1_Bs2,
                                 u1_Bs2, u1_Bs1, u1_Bs1, u1_Bs0, u1_Bs0);

    C0_16x8 = _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0, clip3, clip3, clip2, clip2,
                           clip1, clip1, clip0, clip0);

    bs_flag_16x8b = _mm_cmpeq_epi8(bs_flag_16x8b, zero);
    bs_flag_16x8b = _mm_xor_si128(bs_flag_16x8b, _mm_set1_epi8(0xFF)); //Invert for required mask
    C0_8x16 = _mm_unpacklo_epi8(C0_16x8, zero);

    //Cond1 (ABS(p0 - q0) < alpha)
    temp1 = _mm_subs_epu8(q0_16x8, p0_16x8);
    temp2 = _mm_subs_epu8(p0_16x8, q0_16x8);
    temp1 = _mm_add_epi8(temp1, temp2);

    temp2 = _mm_unpacklo_epi8(temp1, zero);
    temp2 = _mm_cmpgt_epi16(Alpha_8x16, temp2);

    flag1_16x8 = _mm_packs_epi16(temp2, zero);
    flag1_16x8 = _mm_and_si128(flag1_16x8, bs_flag_16x8b);

    //Cond2 (ABS(q1 - q0) < beta)
    temp1 = _mm_subs_epu8(q0_16x8, q1_16x8);
    temp2 = _mm_subs_epu8(q1_16x8, q0_16x8);
    temp1 = _mm_add_epi8(temp1, temp2);

    temp2 = _mm_unpacklo_epi8(temp1, zero);
    temp2 = _mm_cmpgt_epi16(Beta_8x16, temp2);

    flag2_16x8 = _mm_packs_epi16(temp2, zero);
    flag1_16x8 = _mm_and_si128(flag1_16x8, flag2_16x8);

    //Cond3 (ABS(p1 - p0) < beta)
    temp1 = _mm_subs_epu8(p0_16x8, p1_16x8);
    temp2 = _mm_subs_epu8(p1_16x8, p0_16x8);
    temp1 = _mm_add_epi8(temp1, temp2);

    temp2 = _mm_unpacklo_epi8(temp1, zero);
    temp2 = _mm_cmpgt_epi16(Beta_8x16, temp2);

    flag2_16x8 = _mm_packs_epi16(temp2, zero);

    // !((ABS(p0 - q0) < alpha) || (ABS(q1 - q0) < beta) || (ABS(p1 - p0) < beta))
    flag1_16x8 = _mm_and_si128(flag1_16x8, flag2_16x8);

    // (ABS(p2 - p0) < beta)
    temp1 = _mm_subs_epu8(p0_16x8, p2_16x8);
    temp2 = _mm_subs_epu8(p2_16x8, p0_16x8);
    temp1 = _mm_add_epi8(temp1, temp2);

    temp2 = _mm_unpacklo_epi8(temp1, zero);
    temp2 = _mm_cmpgt_epi16(Beta_8x16, temp2);

    flag2_16x8 = _mm_packs_epi16(temp2, zero);
    flag2_16x8 = _mm_and_si128(flag1_16x8, flag2_16x8);

    temp2 = _mm_subs_epi16(zero, temp2);

    C_8x16 = _mm_add_epi16(C0_8x16, temp2);

    // (ABS(q2 - q0) < beta)
    temp1 = _mm_subs_epu8(q0_16x8, q2_16x8);
    temp2 = _mm_subs_epu8(q2_16x8, q0_16x8);
    temp1 = _mm_add_epi8(temp1, temp2);

    temp2 = _mm_unpacklo_epi8(temp1, zero);
    temp2 = _mm_cmpgt_epi16(Beta_8x16, temp2);

    flag3_16x8 = _mm_packs_epi16(temp2, zero);
    flag3_16x8 = _mm_and_si128(flag1_16x8, flag3_16x8);

    temp2 = _mm_subs_epi16(zero, temp2);

    C_8x16 = _mm_add_epi16(C_8x16, temp2);

    const_val4_8x16 = _mm_set1_epi16(4);
    temp1 = _mm_subs_epi16(_mm_unpacklo_epi8(q0_16x8, zero),
                           _mm_unpacklo_epi8(p0_16x8, zero));
    temp2 = _mm_subs_epi16(_mm_unpacklo_epi8(p1_16x8, zero),
                           _mm_unpacklo_epi8(q1_16x8, zero));
    temp1 = _mm_slli_epi16(temp1, 2);
    temp1 = _mm_add_epi16(temp1, temp2);
    temp1 = _mm_add_epi16(temp1, const_val4_8x16);
    in_macro_16x8 = _mm_srai_epi16(temp1, 3);

    in_macro_16x8 = _mm_min_epi16(C_8x16, in_macro_16x8); //CLIP3
    C_8x16 = _mm_subs_epi16(zero, C_8x16);
    in_macro_16x8 = _mm_max_epi16(C_8x16, in_macro_16x8); //CLIP3

    // p0
    temp1 = _mm_add_epi16(_mm_unpacklo_epi8(p0_16x8, zero), in_macro_16x8);

    temp1 = _mm_packus_epi16(temp1, zero);

    p0_16x8_1 = _mm_and_si128(temp1, flag1_16x8);
    p0_16x8_2 = _mm_and_si128(
                    p0_16x8, _mm_xor_si128(flag1_16x8, _mm_set1_epi16(0xFFFF)));

    p0_16x8_1 = _mm_add_epi8(p0_16x8_1, p0_16x8_2);

    // q0
    temp1 = _mm_sub_epi16(_mm_unpacklo_epi8(q0_16x8, zero), in_macro_16x8);

    temp1 = _mm_packus_epi16(temp1, zero);

    q0_16x8_1 = _mm_and_si128(temp1, flag1_16x8);
    q0_16x8_2 = _mm_and_si128(
                    q0_16x8, _mm_xor_si128(flag1_16x8, _mm_set1_epi16(0xFFFF)));

    q0_16x8_1 = _mm_add_epi8(q0_16x8_1, q0_16x8_2);

    //if(Ap < Beta)
    temp1 = _mm_avg_epu16(_mm_unpacklo_epi8(q0_16x8, zero),
                          _mm_unpacklo_epi8(p0_16x8, zero));
    temp2 = _mm_slli_epi16(_mm_unpacklo_epi8(p1_16x8, zero), 1);
    //temp2 = _mm_subs_epi16(zero,temp2);
    temp2 = _mm_subs_epi16(_mm_unpacklo_epi8(p2_16x8, zero), temp2);
    temp2 = _mm_add_epi16(temp1, temp2);
    in_macro_16x8 = _mm_srai_epi16(temp2, 1);

    in_macro_16x8 = _mm_min_epi16(C0_8x16, in_macro_16x8); //CLIP3
    C0_8x16 = _mm_subs_epi16(zero, C0_8x16);
    in_macro_16x8 = _mm_max_epi16(C0_8x16, in_macro_16x8); //CLIP3

    // p1
    temp1 = _mm_add_epi16(_mm_unpacklo_epi8(p1_16x8, zero), in_macro_16x8);

    temp1 = _mm_packus_epi16(temp1, zero);

    p1_16x8_1 = _mm_and_si128(temp1, flag2_16x8);
    p1_16x8 = _mm_and_si128(p1_16x8,
                            _mm_xor_si128(flag2_16x8, _mm_set1_epi16(0xFFFF)));
    p1_16x8 = _mm_add_epi8(p1_16x8, p1_16x8_1);

    //if(Aq < Beta)
    temp1 = _mm_avg_epu16(_mm_unpacklo_epi8(q0_16x8, zero),
                          _mm_unpacklo_epi8(p0_16x8, zero));
    temp2 = _mm_slli_epi16(_mm_unpacklo_epi8(q1_16x8, zero), 1);
    //temp2 = _mm_slli_epi16 (temp2, 1);
    temp2 = _mm_subs_epi16(_mm_unpacklo_epi8(q2_16x8, zero), temp2);
    temp2 = _mm_add_epi16(temp1, temp2);
    in_macro_16x8 = _mm_srai_epi16(temp2, 1);

    in_macro_16x8 = _mm_max_epi16(C0_8x16, in_macro_16x8); //CLIP3
    C0_8x16 = _mm_subs_epi16(zero, C0_8x16);
    in_macro_16x8 = _mm_min_epi16(C0_8x16, in_macro_16x8); //CLIP3

    temp1 = _mm_add_epi16(_mm_unpacklo_epi8(q1_16x8, zero), in_macro_16x8);

    // q1
    temp1 = _mm_packus_epi16(temp1, zero);

    q1_16x8_1 = _mm_and_si128(temp1, flag3_16x8);
    q1_16x8 = _mm_and_si128(q1_16x8,
                            _mm_xor_si128(flag3_16x8, _mm_set1_epi16(0xFFFF)));
    q1_16x8 = _mm_add_epi8(q1_16x8, q1_16x8_1);

    temp1 = _mm_unpacklo_epi8(p3_16x8, p2_16x8);
    temp2 = _mm_unpacklo_epi8(p1_16x8, p0_16x8_1);
    temp3 = _mm_unpacklo_epi8(q0_16x8_1, q1_16x8);
    temp4 = _mm_unpacklo_epi8(q2_16x8, q3_16x8);

    line7 = _mm_unpacklo_epi16(temp1, temp2);
    temp1 = _mm_unpackhi_epi16(temp1, temp2);
    line8 = _mm_unpacklo_epi16(temp3, temp4);
    temp2 = _mm_unpackhi_epi16(temp3, temp4);

    line1 = _mm_unpacklo_epi32(line7, line8);
    line2 = _mm_srli_si128(line1, 8);
    line3 = _mm_unpackhi_epi32(line7, line8);
    line4 = _mm_srli_si128(line3, 8);
    line5 = _mm_unpacklo_epi32(temp1, temp2);
    line6 = _mm_srli_si128(line5, 8);
    line7 = _mm_unpackhi_epi32(temp1, temp2);
    line8 = _mm_srli_si128(line7, 8);

    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 0 * src_strd), line1);
    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 1 * src_strd), line2);
    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 2 * src_strd), line3);
    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 3 * src_strd), line4);
    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 4 * src_strd), line5);
    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 5 * src_strd), line6);
    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 6 * src_strd), line7);
    _mm_storel_epi64((__m128i *)(pu1_src - 4 + 7 * src_strd), line8);
}

