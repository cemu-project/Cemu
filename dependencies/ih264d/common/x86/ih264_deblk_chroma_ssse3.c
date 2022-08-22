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
/*  File Name         : ih264_deblk_chroma_ssse3.c                           */
/*                                                                           */
/*  Description       : Contains function definitions for deblocking         */
/*                                                                           */
/*  List of Functions : ih264_deblk_chroma_vert_bs4_ssse3()                  */
/*                      ih264_deblk_chroma_horz_bs4_ssse3()                  */
/*                      ih264_deblk_chroma_vert_bslt4_ssse3()                */
/*                      ih264_deblk_chroma_horz_bslt4_ssse3()                */
/*                      ih264_deblk_chroma_vert_bs4_mbaff_ssse3()            */
/*                      ih264_deblk_chroma_vert_bslt4_mbaff_ssse3()          */
/*                                                                           */
/*  Issues / Problems : None                                                 */
/*                                                                           */
/*  Revision History  :                                                      */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         12 02 2015   Naveen Kumar P  Added chrom deblocking ssse3         */
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
/*  Function Name : ih264_deblk_chroma_vert_bs4_ssse3()                      */
/*                                                                           */
/*  Description   : This function performs filtering of a chroma block       */
/*                  vertical edge when the boundary strength is set to 4 in  */
/*                  high profile.                                            */
/*                                                                           */
/*  Inputs        : pu1_src    - pointer to the src sample q0 of U           */
/*                  src_strd   - source stride                               */
/*                  alpha_cb   - alpha value for the boundary in U           */
/*                  beta_cb    - beta value for the boundary in U            */
/*                  alpha_cr   - alpha value for the boundary in V           */
/*                  beta_cr    - beta value for the boundary in V            */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : This operation is described in Sec. 8.7.2.4 under the    */
/*                  title "Filtering process for edges for bS equal to 4" in */
/*                  ITU T Rec H.264 with alpha and beta values different in  */
/*                  U and V.                                                 */
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
void ih264_deblk_chroma_vert_bs4_ssse3(UWORD8 *pu1_src,
                                       WORD32 src_strd,
                                       WORD32 alpha_cb,
                                       WORD32 beta_cb,
                                       WORD32 alpha_cr,
                                       WORD32 beta_cr)
{
    UWORD8 *pu1_src_uv = pu1_src; /* Pointer to the src sample q0 of plane U*/
    WORD32 alpha_cbcr = (alpha_cr << 16) + alpha_cb;
    WORD32 beta_cbcr = (beta_cr << 16) + beta_cb;
    __m128i linea, lineb, linec, lined, linee, linef, lineg, lineh;
    __m128i temp1, temp2, temp3, temp4;

    __m128i q0_uv_16x8, p0_uv_16x8, q1_uv_16x8, p1_uv_16x8;
    __m128i q0_uv_8x16, p0_uv_8x16, q1_uv_8x16, p1_uv_8x16;
    __m128i flag1, flag2;
    __m128i diff, alpha_cbcr_16x8, beta_cbcr_16x8;
    __m128i zero = _mm_setzero_si128();
    __m128i p0_uv_8x16_1, p0_uv_8x16_2, q0_uv_8x16_1, q0_uv_8x16_2;

    /* Load and transpose the pixel values */
    linea = _mm_loadl_epi64((__m128i *)(pu1_src_uv - 4));
    lineb = _mm_loadl_epi64((__m128i *)(pu1_src_uv - 4 + src_strd));
    linec = _mm_loadl_epi64((__m128i *)(pu1_src_uv - 4 + 2 * src_strd));
    lined = _mm_loadl_epi64((__m128i *)(pu1_src_uv - 4 + 3 * src_strd));
    linee = _mm_loadl_epi64((__m128i *)(pu1_src_uv - 4 + 4 * src_strd));
    linef = _mm_loadl_epi64((__m128i *)(pu1_src_uv - 4 + 5 * src_strd));
    lineg = _mm_loadl_epi64((__m128i *)(pu1_src_uv - 4 + 6 * src_strd));
    lineh = _mm_loadl_epi64((__m128i *)(pu1_src_uv - 4 + 7 * src_strd));

    temp1 = _mm_unpacklo_epi16(linea, lineb);
    temp2 = _mm_unpacklo_epi16(linec, lined);
    temp3 = _mm_unpacklo_epi16(linee, linef);
    temp4 = _mm_unpacklo_epi16(lineg, lineh);

    p1_uv_8x16 = _mm_unpacklo_epi32(temp1, temp2);
    p0_uv_8x16 = _mm_unpacklo_epi32(temp3, temp4);
    q0_uv_8x16 = _mm_unpackhi_epi32(temp1, temp2);
    q1_uv_8x16 = _mm_unpackhi_epi32(temp3, temp4);

    p1_uv_16x8 = _mm_unpacklo_epi64(p1_uv_8x16, p0_uv_8x16);
    p0_uv_16x8 = _mm_unpackhi_epi64(p1_uv_8x16, p0_uv_8x16);
    q0_uv_16x8 = _mm_unpacklo_epi64(q0_uv_8x16, q1_uv_8x16);
    q1_uv_16x8 = _mm_unpackhi_epi64(q0_uv_8x16, q1_uv_8x16);
    /* End of transpose */

    q0_uv_8x16 = _mm_unpacklo_epi8(q0_uv_16x8, zero);
    q1_uv_8x16 = _mm_unpacklo_epi8(q1_uv_16x8, zero);
    p1_uv_8x16 = _mm_unpacklo_epi8(p1_uv_16x8, zero);
    p0_uv_8x16 = _mm_unpacklo_epi8(p0_uv_16x8, zero);

    diff = _mm_subs_epi16(p0_uv_8x16, q0_uv_8x16); //Condn 1
    diff = _mm_abs_epi16(diff);
    alpha_cbcr_16x8 = _mm_set1_epi32(alpha_cbcr);
    flag1 = _mm_cmpgt_epi16(alpha_cbcr_16x8, diff);

    diff = _mm_subs_epi16(q1_uv_8x16, q0_uv_8x16); //Condtn 2
    diff = _mm_abs_epi16(diff);
    beta_cbcr_16x8 = _mm_set1_epi32(beta_cbcr);
    flag1 = _mm_and_si128(flag1, _mm_cmpgt_epi16(beta_cbcr_16x8, diff));

    diff = _mm_subs_epi16(p1_uv_8x16, p0_uv_8x16); //Condtn 3
    diff = _mm_abs_epi16(diff);
    flag1 = _mm_and_si128(flag1, _mm_cmpgt_epi16(beta_cbcr_16x8, diff));

    temp1 = _mm_slli_epi16(p1_uv_8x16, 1);
    temp2 = _mm_add_epi16(p0_uv_8x16, q1_uv_8x16);
    temp1 = _mm_add_epi16(temp1, _mm_set1_epi16(2));
    temp1 = _mm_add_epi16(temp1, temp2);
    p0_uv_8x16_1 = _mm_srai_epi16(temp1, 2);

    temp1 = _mm_slli_epi16(q1_uv_8x16, 1);
    temp2 = _mm_add_epi16(p1_uv_8x16, q0_uv_8x16);
    temp1 = _mm_add_epi16(temp1, _mm_set1_epi16(2));
    temp1 = _mm_add_epi16(temp1, temp2);
    q0_uv_8x16_1 = _mm_srai_epi16(temp1, 2);

    q0_uv_8x16 = _mm_unpackhi_epi8(q0_uv_16x8, zero);
    q1_uv_8x16 = _mm_unpackhi_epi8(q1_uv_16x8, zero);
    p1_uv_8x16 = _mm_unpackhi_epi8(p1_uv_16x8, zero);
    p0_uv_8x16 = _mm_unpackhi_epi8(p0_uv_16x8, zero);

    diff = _mm_subs_epi16(p0_uv_8x16, q0_uv_8x16); //Condn 1
    diff = _mm_abs_epi16(diff);
    alpha_cbcr_16x8 = _mm_set1_epi32(alpha_cbcr);
    flag2 = _mm_cmpgt_epi16(alpha_cbcr_16x8, diff);

    diff = _mm_subs_epi16(q1_uv_8x16, q0_uv_8x16); //Condtn 2
    diff = _mm_abs_epi16(diff);
    beta_cbcr_16x8 = _mm_set1_epi32(beta_cbcr);
    flag2 = _mm_and_si128(flag2, _mm_cmpgt_epi16(beta_cbcr_16x8, diff));

    diff = _mm_subs_epi16(p1_uv_8x16, p0_uv_8x16); //Condtn 3
    diff = _mm_abs_epi16(diff);
    flag2 = _mm_and_si128(flag2, _mm_cmpgt_epi16(beta_cbcr_16x8, diff));

    temp1 = _mm_slli_epi16(p1_uv_8x16, 1);
    temp2 = _mm_add_epi16(p0_uv_8x16, q1_uv_8x16);
    temp1 = _mm_add_epi16(temp1, _mm_set1_epi16(2));
    temp1 = _mm_add_epi16(temp1, temp2);
    p0_uv_8x16_2 = _mm_srai_epi16(temp1, 2);

    temp1 = _mm_slli_epi16(q1_uv_8x16, 1);
    temp2 = _mm_add_epi16(p1_uv_8x16, q0_uv_8x16);
    temp1 = _mm_add_epi16(temp1, _mm_set1_epi16(2));
    temp1 = _mm_add_epi16(temp1, temp2);
    q0_uv_8x16_2 = _mm_srai_epi16(temp1, 2);

    p0_uv_8x16_2 = _mm_packus_epi16(p0_uv_8x16_1, p0_uv_8x16_2);
    q0_uv_8x16_2 = _mm_packus_epi16(q0_uv_8x16_1, q0_uv_8x16_2);

    flag1 = _mm_packs_epi16(flag1, flag2);

    p0_uv_8x16_1 = _mm_and_si128(p0_uv_16x8,
                                 _mm_xor_si128(flag1, _mm_set1_epi8(0xFF)));
    p0_uv_8x16_2 = _mm_and_si128(p0_uv_8x16_2, flag1);
    p0_uv_16x8 = _mm_add_epi8(p0_uv_8x16_1, p0_uv_8x16_2);

    q0_uv_8x16_1 = _mm_and_si128(q0_uv_16x8,
                                 _mm_xor_si128(flag1, _mm_set1_epi8(0xFF)));
    q0_uv_8x16_2 = _mm_and_si128(q0_uv_8x16_2, flag1);
    q0_uv_16x8 = _mm_add_epi8(q0_uv_8x16_1, q0_uv_8x16_2);

    /* Inverse-transpose and store back */
    temp1 = _mm_unpacklo_epi16(p1_uv_16x8, p0_uv_16x8);
    temp2 = _mm_unpackhi_epi16(p1_uv_16x8, p0_uv_16x8);
    temp3 = _mm_unpacklo_epi16(q0_uv_16x8, q1_uv_16x8);
    temp4 = _mm_unpackhi_epi16(q0_uv_16x8, q1_uv_16x8);

    linea = _mm_unpacklo_epi32(temp1, temp3);
    lineb = _mm_srli_si128(linea, 8);
    linec = _mm_unpackhi_epi32(temp1, temp3);
    lined = _mm_srli_si128(linec, 8);
    linee = _mm_unpacklo_epi32(temp2, temp4);
    linef = _mm_srli_si128(linee, 8);
    lineg = _mm_unpackhi_epi32(temp2, temp4);
    lineh = _mm_srli_si128(lineg, 8);

    _mm_storel_epi64((__m128i *)(pu1_src_uv - 4), linea);
    _mm_storel_epi64((__m128i *)(pu1_src_uv - 4 + src_strd), lineb);
    _mm_storel_epi64((__m128i *)(pu1_src_uv - 4 + 2 * src_strd), linec);
    _mm_storel_epi64((__m128i *)(pu1_src_uv - 4 + 3 * src_strd), lined);
    _mm_storel_epi64((__m128i *)(pu1_src_uv - 4 + 4 * src_strd), linee);
    _mm_storel_epi64((__m128i *)(pu1_src_uv - 4 + 5 * src_strd), linef);
    _mm_storel_epi64((__m128i *)(pu1_src_uv - 4 + 6 * src_strd), lineg);
    _mm_storel_epi64((__m128i *)(pu1_src_uv - 4 + 7 * src_strd), lineh);

}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_deblk_chroma_horz_bs4_ssse3()                      */
/*                                                                           */
/*  Description   : This function performs filtering of a chroma block       */
/*                  horizontal edge when the boundary strength is set to 4   */
/*                  in high profile.                                         */
/*                                                                           */
/*  Inputs        : pu1_src    - pointer to the src sample q0 of U           */
/*                  src_strd   - source stride                               */
/*                  alpha_cb   - alpha value for the boundary in U           */
/*                  beta_cb    - beta value for the boundary in U            */
/*                  alpha_cr   - alpha value for the boundary in V           */
/*                  beta_cr    - beta value for the boundary in V            */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : This operation is described in Sec. 8.7.2.4 under the    */
/*                  title "Filtering process for edges for bS equal to 4" in */
/*                  ITU T Rec H.264 with alpha and beta values different in  */
/*                  U and V.                                                 */
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
void ih264_deblk_chroma_horz_bs4_ssse3(UWORD8 *pu1_src,
                                       WORD32 src_strd,
                                       WORD32 alpha_cb,
                                       WORD32 beta_cb,
                                       WORD32 alpha_cr,
                                       WORD32 beta_cr)
{
    UWORD8 *pu1_src_uv = pu1_src; /* Pointer to the src sample q0 of plane U*/
    WORD16 i16_posP1, i16_posP0, i16_posQ1;

    UWORD8 *pu1_HorzPixelUV; /*! < Pointer to the first pixel of the boundary */
    WORD32 alpha_cbcr = (alpha_cr << 16) + alpha_cb;
    WORD32 beta_cbcr = (beta_cr << 16) + beta_cb;
    __m128i q0_uv_16x8, p0_uv_16x8, q1_uv_16x8, p1_uv_16x8;
    __m128i q0_uv_8x16, p0_uv_8x16, q1_uv_8x16, p1_uv_8x16;
    __m128i flag1, flag2;
    __m128i diff, alpha_cbcr_16x8, beta_cbcr_16x8;
    __m128i zero = _mm_setzero_si128();
    __m128i p0_uv_8x16_1, p0_uv_8x16_2, q0_uv_8x16_1, q0_uv_8x16_2;
    __m128i temp1, temp2;

    pu1_HorzPixelUV = pu1_src_uv - (src_strd << 1);

    i16_posQ1 = src_strd;
    i16_posP0 = src_strd;
    i16_posP1 = 0;

    q0_uv_16x8 = _mm_loadu_si128((__m128i *)(pu1_src_uv));
    q1_uv_16x8 = _mm_loadu_si128((__m128i *)(pu1_src_uv + i16_posQ1));
    p1_uv_16x8 = _mm_loadu_si128((__m128i *)(pu1_HorzPixelUV + i16_posP1));
    p0_uv_16x8 = _mm_loadu_si128((__m128i *)(pu1_HorzPixelUV + i16_posP0));

    q0_uv_8x16 = _mm_unpacklo_epi8(q0_uv_16x8, zero);
    q1_uv_8x16 = _mm_unpacklo_epi8(q1_uv_16x8, zero);
    p1_uv_8x16 = _mm_unpacklo_epi8(p1_uv_16x8, zero);
    p0_uv_8x16 = _mm_unpacklo_epi8(p0_uv_16x8, zero);

    diff = _mm_subs_epi16(p0_uv_8x16, q0_uv_8x16); //Condn 1
    diff = _mm_abs_epi16(diff);
    alpha_cbcr_16x8 = _mm_set1_epi32(alpha_cbcr);
    flag1 = _mm_cmpgt_epi16(alpha_cbcr_16x8, diff);

    diff = _mm_subs_epi16(q1_uv_8x16, q0_uv_8x16); //Condtn 2
    diff = _mm_abs_epi16(diff);
    beta_cbcr_16x8 = _mm_set1_epi32(beta_cbcr);
    flag1 = _mm_and_si128(flag1, _mm_cmpgt_epi16(beta_cbcr_16x8, diff));

    diff = _mm_subs_epi16(p1_uv_8x16, p0_uv_8x16); //Condtn 3
    diff = _mm_abs_epi16(diff);
    flag1 = _mm_and_si128(flag1, _mm_cmpgt_epi16(beta_cbcr_16x8, diff));

    temp1 = _mm_slli_epi16(p1_uv_8x16, 1);
    temp2 = _mm_add_epi16(p0_uv_8x16, q1_uv_8x16);
    temp1 = _mm_add_epi16(temp1, _mm_set1_epi16(2));
    temp1 = _mm_add_epi16(temp1, temp2);
    p0_uv_8x16_1 = _mm_srai_epi16(temp1, 2);

    temp1 = _mm_slli_epi16(q1_uv_8x16, 1);
    temp2 = _mm_add_epi16(p1_uv_8x16, q0_uv_8x16);
    temp1 = _mm_add_epi16(temp1, _mm_set1_epi16(2));
    temp1 = _mm_add_epi16(temp1, temp2);
    q0_uv_8x16_1 = _mm_srai_epi16(temp1, 2);

    q0_uv_8x16 = _mm_unpackhi_epi8(q0_uv_16x8, zero);
    q1_uv_8x16 = _mm_unpackhi_epi8(q1_uv_16x8, zero);
    p1_uv_8x16 = _mm_unpackhi_epi8(p1_uv_16x8, zero);
    p0_uv_8x16 = _mm_unpackhi_epi8(p0_uv_16x8, zero);

    diff = _mm_subs_epi16(p0_uv_8x16, q0_uv_8x16); //Condn 1
    diff = _mm_abs_epi16(diff);
    alpha_cbcr_16x8 = _mm_set1_epi32(alpha_cbcr);
    flag2 = _mm_cmpgt_epi16(alpha_cbcr_16x8, diff);

    diff = _mm_subs_epi16(q1_uv_8x16, q0_uv_8x16); //Condtn 2
    diff = _mm_abs_epi16(diff);
    beta_cbcr_16x8 = _mm_set1_epi32(beta_cbcr);
    flag2 = _mm_and_si128(flag2, _mm_cmpgt_epi16(beta_cbcr_16x8, diff));

    diff = _mm_subs_epi16(p1_uv_8x16, p0_uv_8x16); //Condtn 3
    diff = _mm_abs_epi16(diff);
    flag2 = _mm_and_si128(flag2, _mm_cmpgt_epi16(beta_cbcr_16x8, diff));

    temp1 = _mm_slli_epi16(p1_uv_8x16, 1);
    temp2 = _mm_add_epi16(p0_uv_8x16, q1_uv_8x16);
    temp1 = _mm_add_epi16(temp1, _mm_set1_epi16(2));
    temp1 = _mm_add_epi16(temp1, temp2);
    p0_uv_8x16_2 = _mm_srai_epi16(temp1, 2);

    temp1 = _mm_slli_epi16(q1_uv_8x16, 1);
    temp2 = _mm_add_epi16(p1_uv_8x16, q0_uv_8x16);
    temp1 = _mm_add_epi16(temp1, _mm_set1_epi16(2));
    temp1 = _mm_add_epi16(temp1, temp2);
    q0_uv_8x16_2 = _mm_srai_epi16(temp1, 2);

    p0_uv_8x16_2 = _mm_packus_epi16(p0_uv_8x16_1, p0_uv_8x16_2);
    q0_uv_8x16_2 = _mm_packus_epi16(q0_uv_8x16_1, q0_uv_8x16_2);

    flag1 = _mm_packs_epi16(flag1, flag2);

    p0_uv_8x16_1 = _mm_and_si128(p0_uv_16x8,
                                 _mm_xor_si128(flag1, _mm_set1_epi8(0xFF)));
    p0_uv_8x16_2 = _mm_and_si128(p0_uv_8x16_2, flag1);
    p0_uv_8x16_1 = _mm_add_epi8(p0_uv_8x16_1, p0_uv_8x16_2);
    _mm_storeu_si128((__m128i *)(pu1_HorzPixelUV + i16_posP0), p0_uv_8x16_1);

    q0_uv_8x16_1 = _mm_and_si128(q0_uv_16x8,
                                 _mm_xor_si128(flag1, _mm_set1_epi8(0xFF)));
    q0_uv_8x16_2 = _mm_and_si128(q0_uv_8x16_2, flag1);
    q0_uv_8x16_1 = _mm_add_epi8(q0_uv_8x16_1, q0_uv_8x16_2);
    _mm_storeu_si128((__m128i *)(pu1_src_uv), q0_uv_8x16_1);

}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_deblk_chroma_vert_bslt4_ssse3()                    */
/*                                                                           */
/*  Description   : This function performs filtering of a chroma block       */
/*                  vertical edge when the boundary strength is less than 4  */
/*                  in high profile.                                         */
/*                                                                           */
/*  Inputs        : pu1_src          - pointer to the src sample q0 of U     */
/*                  src_strd         - source stride                         */
/*                  alpha_cb         - alpha value for the boundary in U     */
/*                  beta_cb          - beta value for the boundary in U      */
/*                  alpha_cr         - alpha value for the boundary in V     */
/*                  beta_cr          - beta value for the boundary in V      */
/*                  u4_bs            - packed Boundary strength array        */
/*                  pu1_cliptab_cb   - tc0_table for U                       */
/*                  pu1_cliptab_cr   - tc0_table for V                       */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : This operation is described in Sec. 8.7.2.3 under the    */
/*                  title "Filtering process for edges for bS less than 4"   */
/*                  in ITU T Rec H.264 with alpha and beta values different  */
/*                  in U and V.                                              */
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
void ih264_deblk_chroma_vert_bslt4_ssse3(UWORD8 *pu1_src,
                                         WORD32 src_strd,
                                         WORD32 alpha_cb,
                                         WORD32 beta_cb,
                                         WORD32 alpha_cr,
                                         WORD32 beta_cr,
                                         UWORD32 u4_bs,
                                         const UWORD8 *pu1_cliptab_cb,
                                         const UWORD8 *pu1_cliptab_cr)
{
    UWORD8 *pu1_src_uv = pu1_src; /* Pointer to the src sample q0 of plane U*/
    UWORD8 u1_Bs0, u1_Bs1, u1_Bs2, u1_Bs3;
    WORD32 alpha_cbcr = (alpha_cr << 16) + alpha_cb;
    WORD32 beta_cbcr = (beta_cr << 16) + beta_cb;
    __m128i linea, lineb, linec, lined, linee, linef, lineg, lineh;
    __m128i temp1, temp2, temp3, temp4;

    __m128i q0_uv_16x8, p0_uv_16x8, q1_uv_16x8, p1_uv_16x8;
    __m128i q0_uv_8x16, p0_uv_8x16, q1_uv_8x16, p1_uv_8x16;
    __m128i flag_bs, flag1, flag2;
    __m128i diff, diff1, alpha_cbcr_16x8, beta_cbcr_16x8, in_macro;
    __m128i zero = _mm_setzero_si128();
    __m128i C0_uv_8x16;
    __m128i p0_uv_8x16_1, p0_uv_8x16_2, q0_uv_8x16_1, q0_uv_8x16_2;

    u1_Bs0 = (u4_bs >> 24) & 0xff;
    u1_Bs1 = (u4_bs >> 16) & 0xff;
    u1_Bs2 = (u4_bs >> 8) & 0xff;
    u1_Bs3 = (u4_bs >> 0) & 0xff;

    flag_bs = _mm_set_epi8(u1_Bs3, u1_Bs3, u1_Bs3, u1_Bs3, u1_Bs2, u1_Bs2,
                           u1_Bs2, u1_Bs2, u1_Bs1, u1_Bs1, u1_Bs1, u1_Bs1,
                           u1_Bs0, u1_Bs0, u1_Bs0, u1_Bs0);
    flag_bs = _mm_cmpeq_epi8(flag_bs, zero); //Set flag to 1s and 0s
    flag_bs = _mm_xor_si128(flag_bs, _mm_set1_epi8(0xFF)); //Invert for required mask

    /* Load and transpose the pixel values */
    linea = _mm_loadl_epi64((__m128i *)(pu1_src_uv - 4));
    lineb = _mm_loadl_epi64((__m128i *)(pu1_src_uv - 4 + src_strd));
    linec = _mm_loadl_epi64((__m128i *)(pu1_src_uv - 4 + 2 * src_strd));
    lined = _mm_loadl_epi64((__m128i *)(pu1_src_uv - 4 + 3 * src_strd));
    linee = _mm_loadl_epi64((__m128i *)(pu1_src_uv - 4 + 4 * src_strd));
    linef = _mm_loadl_epi64((__m128i *)(pu1_src_uv - 4 + 5 * src_strd));
    lineg = _mm_loadl_epi64((__m128i *)(pu1_src_uv - 4 + 6 * src_strd));
    lineh = _mm_loadl_epi64((__m128i *)(pu1_src_uv - 4 + 7 * src_strd));

    temp1 = _mm_unpacklo_epi16(linea, lineb);
    temp2 = _mm_unpacklo_epi16(linec, lined);
    temp3 = _mm_unpacklo_epi16(linee, linef);
    temp4 = _mm_unpacklo_epi16(lineg, lineh);

    p1_uv_8x16 = _mm_unpacklo_epi32(temp1, temp2);
    p0_uv_8x16 = _mm_unpacklo_epi32(temp3, temp4);
    q0_uv_8x16 = _mm_unpackhi_epi32(temp1, temp2);
    q1_uv_8x16 = _mm_unpackhi_epi32(temp3, temp4);

    p1_uv_16x8 = _mm_unpacklo_epi64(p1_uv_8x16, p0_uv_8x16);
    p0_uv_16x8 = _mm_unpackhi_epi64(p1_uv_8x16, p0_uv_8x16);
    q0_uv_16x8 = _mm_unpacklo_epi64(q0_uv_8x16, q1_uv_8x16);
    q1_uv_16x8 = _mm_unpackhi_epi64(q0_uv_8x16, q1_uv_8x16);
    /* End of transpose */

    q0_uv_8x16 = _mm_unpacklo_epi8(q0_uv_16x8, zero);
    q1_uv_8x16 = _mm_unpacklo_epi8(q1_uv_16x8, zero);
    p1_uv_8x16 = _mm_unpacklo_epi8(p1_uv_16x8, zero);
    p0_uv_8x16 = _mm_unpacklo_epi8(p0_uv_16x8, zero);

    diff = _mm_subs_epi16(p0_uv_8x16, q0_uv_8x16); //Condn 1
    diff = _mm_abs_epi16(diff);
    alpha_cbcr_16x8 = _mm_set1_epi32(alpha_cbcr);
    flag1 = _mm_cmpgt_epi16(alpha_cbcr_16x8, diff);

    diff = _mm_subs_epi16(q1_uv_8x16, q0_uv_8x16); //Condtn 2
    diff = _mm_abs_epi16(diff);
    beta_cbcr_16x8 = _mm_set1_epi32(beta_cbcr);
    flag1 = _mm_and_si128(flag1, _mm_cmpgt_epi16(beta_cbcr_16x8, diff));

    diff = _mm_subs_epi16(p1_uv_8x16, p0_uv_8x16); //Condtn 3
    diff = _mm_abs_epi16(diff);
    flag1 = _mm_and_si128(flag1, _mm_cmpgt_epi16(beta_cbcr_16x8, diff));

    diff = _mm_subs_epi16(q0_uv_8x16, p0_uv_8x16);
    diff = _mm_slli_epi16(diff, 2);
    diff1 = _mm_subs_epi16(p1_uv_8x16, q1_uv_8x16);
    diff = _mm_add_epi16(diff, diff1);
    diff = _mm_add_epi16(diff, _mm_set1_epi16(4));
    in_macro = _mm_srai_epi16(diff, 3);

    C0_uv_8x16 = _mm_set_epi16(pu1_cliptab_cr[u1_Bs1], pu1_cliptab_cb[u1_Bs1],
                               pu1_cliptab_cr[u1_Bs1], pu1_cliptab_cb[u1_Bs1],
                               pu1_cliptab_cr[u1_Bs0], pu1_cliptab_cb[u1_Bs0],
                               pu1_cliptab_cr[u1_Bs0], pu1_cliptab_cb[u1_Bs0]);

    C0_uv_8x16 = _mm_add_epi16(C0_uv_8x16, _mm_set1_epi16(1));

    in_macro = _mm_min_epi16(C0_uv_8x16, in_macro); //CLIP3
    C0_uv_8x16 = _mm_subs_epi16(zero, C0_uv_8x16);
    in_macro = _mm_max_epi16(C0_uv_8x16, in_macro);

    p0_uv_8x16_1 = _mm_add_epi16(p0_uv_8x16, in_macro);
    q0_uv_8x16_1 = _mm_sub_epi16(q0_uv_8x16, in_macro);

    q0_uv_8x16 = _mm_unpackhi_epi8(q0_uv_16x8, zero);
    q1_uv_8x16 = _mm_unpackhi_epi8(q1_uv_16x8, zero);
    p1_uv_8x16 = _mm_unpackhi_epi8(p1_uv_16x8, zero);
    p0_uv_8x16 = _mm_unpackhi_epi8(p0_uv_16x8, zero);

    diff = _mm_subs_epi16(p0_uv_8x16, q0_uv_8x16); //Condn 1
    diff = _mm_abs_epi16(diff);
    alpha_cbcr_16x8 = _mm_set1_epi32(alpha_cbcr);
    flag2 = _mm_cmpgt_epi16(alpha_cbcr_16x8, diff);

    diff = _mm_subs_epi16(q1_uv_8x16, q0_uv_8x16); //Condtn 2
    diff = _mm_abs_epi16(diff);
    beta_cbcr_16x8 = _mm_set1_epi32(beta_cbcr);
    flag2 = _mm_and_si128(flag2, _mm_cmpgt_epi16(beta_cbcr_16x8, diff));

    diff = _mm_subs_epi16(p1_uv_8x16, p0_uv_8x16); //Condtn 3
    diff = _mm_abs_epi16(diff);
    flag2 = _mm_and_si128(flag2, _mm_cmpgt_epi16(beta_cbcr_16x8, diff));

    diff = _mm_subs_epi16(q0_uv_8x16, p0_uv_8x16);
    diff = _mm_slli_epi16(diff, 2);
    diff1 = _mm_subs_epi16(p1_uv_8x16, q1_uv_8x16);
    diff = _mm_add_epi16(diff, diff1);
    diff = _mm_add_epi16(diff, _mm_set1_epi16(4));
    in_macro = _mm_srai_epi16(diff, 3);

    C0_uv_8x16 = _mm_set_epi16(pu1_cliptab_cr[u1_Bs3], pu1_cliptab_cb[u1_Bs3],
                               pu1_cliptab_cr[u1_Bs3], pu1_cliptab_cb[u1_Bs3],
                               pu1_cliptab_cr[u1_Bs2], pu1_cliptab_cb[u1_Bs2],
                               pu1_cliptab_cr[u1_Bs2], pu1_cliptab_cb[u1_Bs2]);

    C0_uv_8x16 = _mm_add_epi16(C0_uv_8x16, _mm_set1_epi16(1));

    in_macro = _mm_min_epi16(C0_uv_8x16, in_macro); //CLIP3
    C0_uv_8x16 = _mm_subs_epi16(zero, C0_uv_8x16);
    in_macro = _mm_max_epi16(C0_uv_8x16, in_macro);

    p0_uv_8x16_2 = _mm_add_epi16(p0_uv_8x16, in_macro);
    q0_uv_8x16_2 = _mm_sub_epi16(q0_uv_8x16, in_macro);

    p0_uv_8x16_2 = _mm_packus_epi16(p0_uv_8x16_1, p0_uv_8x16_2);
    q0_uv_8x16_2 = _mm_packus_epi16(q0_uv_8x16_1, q0_uv_8x16_2);

    flag1 = _mm_packs_epi16(flag1, flag2);
    flag1 = _mm_and_si128(flag1, flag_bs); //Final flag (BS condition + other 3 conditions)

    p0_uv_8x16_1 = _mm_and_si128(p0_uv_16x8,
                                 _mm_xor_si128(flag1, _mm_set1_epi8(0xFF)));
    p0_uv_8x16_2 = _mm_and_si128(p0_uv_8x16_2, flag1);
    p0_uv_16x8 = _mm_add_epi8(p0_uv_8x16_1, p0_uv_8x16_2);

    q0_uv_8x16_1 = _mm_and_si128(q0_uv_16x8,
                                 _mm_xor_si128(flag1, _mm_set1_epi8(0xFF)));
    q0_uv_8x16_2 = _mm_and_si128(q0_uv_8x16_2, flag1);
    q0_uv_16x8 = _mm_add_epi8(q0_uv_8x16_1, q0_uv_8x16_2);

    /* Inverse-transpose and store back */
    temp1 = _mm_unpacklo_epi16(p1_uv_16x8, p0_uv_16x8);
    temp2 = _mm_unpackhi_epi16(p1_uv_16x8, p0_uv_16x8);
    temp3 = _mm_unpacklo_epi16(q0_uv_16x8, q1_uv_16x8);
    temp4 = _mm_unpackhi_epi16(q0_uv_16x8, q1_uv_16x8);

    linea = _mm_unpacklo_epi32(temp1, temp3);
    lineb = _mm_srli_si128(linea, 8);
    linec = _mm_unpackhi_epi32(temp1, temp3);
    lined = _mm_srli_si128(linec, 8);
    linee = _mm_unpacklo_epi32(temp2, temp4);
    linef = _mm_srli_si128(linee, 8);
    lineg = _mm_unpackhi_epi32(temp2, temp4);
    lineh = _mm_srli_si128(lineg, 8);

    _mm_storel_epi64((__m128i *)(pu1_src_uv - 4), linea);
    _mm_storel_epi64((__m128i *)(pu1_src_uv - 4 + src_strd), lineb);
    _mm_storel_epi64((__m128i *)(pu1_src_uv - 4 + 2 * src_strd), linec);
    _mm_storel_epi64((__m128i *)(pu1_src_uv - 4 + 3 * src_strd), lined);
    _mm_storel_epi64((__m128i *)(pu1_src_uv - 4 + 4 * src_strd), linee);
    _mm_storel_epi64((__m128i *)(pu1_src_uv - 4 + 5 * src_strd), linef);
    _mm_storel_epi64((__m128i *)(pu1_src_uv - 4 + 6 * src_strd), lineg);
    _mm_storel_epi64((__m128i *)(pu1_src_uv - 4 + 7 * src_strd), lineh);

}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_deblk_chroma_horz_bslt4_ssse3()                    */
/*                                                                           */
/*  Description   : This function performs filtering of a chroma block       */
/*                  horizontal edge when the boundary strength is less than  */
/*                  4 in high profile.                                       */
/*                                                                           */
/*  Inputs        : pu1_src          - pointer to the src sample q0 of U     */
/*                  src_strd         - source stride                         */
/*                  alpha_cb         - alpha value for the boundary in U     */
/*                  beta_cb          - beta value for the boundary in U      */
/*                  alpha_cr         - alpha value for the boundary in V     */
/*                  beta_cr          - beta value for the boundary in V      */
/*                  u4_bs            - packed Boundary strength array        */
/*                  pu1_cliptab_cb   - tc0_table for U                       */
/*                  pu1_cliptab_cr   - tc0_table for V                       */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : This operation is described in Sec. 8.7.2.3 under the    */
/*                  title "Filtering process for edges for bS less than 4"   */
/*                  in ITU T Rec H.264 with alpha and beta values different  */
/*                  in U and V.                                              */
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
void ih264_deblk_chroma_horz_bslt4_ssse3(UWORD8 *pu1_src,
                                         WORD32 src_strd,
                                         WORD32 alpha_cb,
                                         WORD32 beta_cb,
                                         WORD32 alpha_cr,
                                         WORD32 beta_cr,
                                         UWORD32 u4_bs,
                                         const UWORD8 *pu1_cliptab_cb,
                                         const UWORD8 *pu1_cliptab_cr)
{
    UWORD8 *pu1_src_uv = pu1_src; /* Pointer to the src sample q0 of plane U*/
    WORD16 i16_posP1, i16_posP0, i16_posQ1;
    UWORD8 u1_Bs0, u1_Bs1, u1_Bs2, u1_Bs3;

    UWORD8 *pu1_HorzPixelUV; /*! < Pointer to the first pixel of the boundary */
    WORD32 alpha_cbcr = (alpha_cr << 16) + alpha_cb;
    WORD32 beta_cbcr = (beta_cr << 16) + beta_cb;
    __m128i q0_uv_16x8, p0_uv_16x8, q1_uv_16x8, p1_uv_16x8;
    __m128i q0_uv_8x16, p0_uv_8x16, q1_uv_8x16, p1_uv_8x16;
    __m128i flag_bs, flag1, flag2;
    __m128i diff, diff1, alpha_cbcr_16x8, beta_cbcr_16x8, in_macro;
    __m128i zero = _mm_setzero_si128();
    __m128i C0_uv_8x16;
    __m128i p0_uv_8x16_1, p0_uv_8x16_2, q0_uv_8x16_1, q0_uv_8x16_2;

    pu1_HorzPixelUV = pu1_src_uv - (src_strd << 1);

    i16_posQ1 = src_strd;
    i16_posP0 = src_strd;
    i16_posP1 = 0;

    u1_Bs0 = (u4_bs >> 24) & 0xff;
    u1_Bs1 = (u4_bs >> 16) & 0xff;
    u1_Bs2 = (u4_bs >> 8) & 0xff;
    u1_Bs3 = (u4_bs >> 0) & 0xff;

    flag_bs = _mm_set_epi8(u1_Bs3, u1_Bs3, u1_Bs3, u1_Bs3, u1_Bs2, u1_Bs2,
                           u1_Bs2, u1_Bs2, u1_Bs1, u1_Bs1, u1_Bs1, u1_Bs1,
                           u1_Bs0, u1_Bs0, u1_Bs0, u1_Bs0);
    flag_bs = _mm_cmpeq_epi8(flag_bs, zero); //Set flag to 1s and 0s
    flag_bs = _mm_xor_si128(flag_bs, _mm_set1_epi8(0xFF)); //Invert for required mask

    q0_uv_16x8 = _mm_loadu_si128((__m128i *)(pu1_src_uv));
    q1_uv_16x8 = _mm_loadu_si128((__m128i *)(pu1_src_uv + i16_posQ1));
    p1_uv_16x8 = _mm_loadu_si128((__m128i *)(pu1_HorzPixelUV + i16_posP1));
    p0_uv_16x8 = _mm_loadu_si128((__m128i *)(pu1_HorzPixelUV + i16_posP0));

    q0_uv_8x16 = _mm_unpacklo_epi8(q0_uv_16x8, zero);
    q1_uv_8x16 = _mm_unpacklo_epi8(q1_uv_16x8, zero);
    p1_uv_8x16 = _mm_unpacklo_epi8(p1_uv_16x8, zero);
    p0_uv_8x16 = _mm_unpacklo_epi8(p0_uv_16x8, zero);

    diff = _mm_subs_epi16(p0_uv_8x16, q0_uv_8x16); //Condn 1
    diff = _mm_abs_epi16(diff);
    alpha_cbcr_16x8 = _mm_set1_epi32(alpha_cbcr);
    flag1 = _mm_cmpgt_epi16(alpha_cbcr_16x8, diff);

    diff = _mm_subs_epi16(q1_uv_8x16, q0_uv_8x16); //Condtn 2
    diff = _mm_abs_epi16(diff);
    beta_cbcr_16x8 = _mm_set1_epi32(beta_cbcr);
    flag1 = _mm_and_si128(flag1, _mm_cmpgt_epi16(beta_cbcr_16x8, diff));

    diff = _mm_subs_epi16(p1_uv_8x16, p0_uv_8x16); //Condtn 3
    diff = _mm_abs_epi16(diff);
    flag1 = _mm_and_si128(flag1, _mm_cmpgt_epi16(beta_cbcr_16x8, diff));

    diff = _mm_subs_epi16(q0_uv_8x16, p0_uv_8x16);
    diff = _mm_slli_epi16(diff, 2);
    diff1 = _mm_subs_epi16(p1_uv_8x16, q1_uv_8x16);
    diff = _mm_add_epi16(diff, diff1);
    diff = _mm_add_epi16(diff, _mm_set1_epi16(4));
    in_macro = _mm_srai_epi16(diff, 3);

    C0_uv_8x16 = _mm_set_epi16(pu1_cliptab_cr[u1_Bs1], pu1_cliptab_cb[u1_Bs1],
                               pu1_cliptab_cr[u1_Bs1], pu1_cliptab_cb[u1_Bs1],
                               pu1_cliptab_cr[u1_Bs0], pu1_cliptab_cb[u1_Bs0],
                               pu1_cliptab_cr[u1_Bs0], pu1_cliptab_cb[u1_Bs0]);

    C0_uv_8x16 = _mm_add_epi16(C0_uv_8x16, _mm_set1_epi16(1));

    in_macro = _mm_min_epi16(C0_uv_8x16, in_macro); //CLIP3
    C0_uv_8x16 = _mm_subs_epi16(zero, C0_uv_8x16);
    in_macro = _mm_max_epi16(C0_uv_8x16, in_macro);

    p0_uv_8x16_1 = _mm_add_epi16(p0_uv_8x16, in_macro);
    q0_uv_8x16_1 = _mm_sub_epi16(q0_uv_8x16, in_macro);

    q0_uv_8x16 = _mm_unpackhi_epi8(q0_uv_16x8, zero);
    q1_uv_8x16 = _mm_unpackhi_epi8(q1_uv_16x8, zero);
    p1_uv_8x16 = _mm_unpackhi_epi8(p1_uv_16x8, zero);
    p0_uv_8x16 = _mm_unpackhi_epi8(p0_uv_16x8, zero);

    diff = _mm_subs_epi16(p0_uv_8x16, q0_uv_8x16); //Condn 1
    diff = _mm_abs_epi16(diff);
    alpha_cbcr_16x8 = _mm_set1_epi32(alpha_cbcr);
    flag2 = _mm_cmpgt_epi16(alpha_cbcr_16x8, diff);

    diff = _mm_subs_epi16(q1_uv_8x16, q0_uv_8x16); //Condtn 2
    diff = _mm_abs_epi16(diff);
    beta_cbcr_16x8 = _mm_set1_epi32(beta_cbcr);
    flag2 = _mm_and_si128(flag2, _mm_cmpgt_epi16(beta_cbcr_16x8, diff));

    diff = _mm_subs_epi16(p1_uv_8x16, p0_uv_8x16); //Condtn 3
    diff = _mm_abs_epi16(diff);
    flag2 = _mm_and_si128(flag2, _mm_cmpgt_epi16(beta_cbcr_16x8, diff));

    diff = _mm_subs_epi16(q0_uv_8x16, p0_uv_8x16);
    diff = _mm_slli_epi16(diff, 2);
    diff1 = _mm_subs_epi16(p1_uv_8x16, q1_uv_8x16);
    diff = _mm_add_epi16(diff, diff1);
    diff = _mm_add_epi16(diff, _mm_set1_epi16(4));
    in_macro = _mm_srai_epi16(diff, 3);

    C0_uv_8x16 = _mm_set_epi16(pu1_cliptab_cr[u1_Bs3], pu1_cliptab_cb[u1_Bs3],
                               pu1_cliptab_cr[u1_Bs3], pu1_cliptab_cb[u1_Bs3],
                               pu1_cliptab_cr[u1_Bs2], pu1_cliptab_cb[u1_Bs2],
                               pu1_cliptab_cr[u1_Bs2], pu1_cliptab_cb[u1_Bs2]);

    C0_uv_8x16 = _mm_add_epi16(C0_uv_8x16, _mm_set1_epi16(1));

    in_macro = _mm_min_epi16(C0_uv_8x16, in_macro); //CLIP3
    C0_uv_8x16 = _mm_subs_epi16(zero, C0_uv_8x16);
    in_macro = _mm_max_epi16(C0_uv_8x16, in_macro);

    p0_uv_8x16_2 = _mm_add_epi16(p0_uv_8x16, in_macro);
    q0_uv_8x16_2 = _mm_sub_epi16(q0_uv_8x16, in_macro);

    p0_uv_8x16_2 = _mm_packus_epi16(p0_uv_8x16_1, p0_uv_8x16_2);
    q0_uv_8x16_2 = _mm_packus_epi16(q0_uv_8x16_1, q0_uv_8x16_2);

    flag1 = _mm_packs_epi16(flag1, flag2);
    flag1 = _mm_and_si128(flag1, flag_bs); //Final flag (BS condition + other 3 conditions)

    p0_uv_8x16_1 = _mm_and_si128(p0_uv_16x8,
                                 _mm_xor_si128(flag1, _mm_set1_epi8(0xFF)));
    p0_uv_8x16_2 = _mm_and_si128(p0_uv_8x16_2, flag1);
    p0_uv_8x16_1 = _mm_add_epi8(p0_uv_8x16_1, p0_uv_8x16_2);
    _mm_storeu_si128((__m128i *)(pu1_HorzPixelUV + i16_posP0), p0_uv_8x16_1);

    q0_uv_8x16_1 = _mm_and_si128(q0_uv_16x8,
                                 _mm_xor_si128(flag1, _mm_set1_epi8(0xFF)));
    q0_uv_8x16_2 = _mm_and_si128(q0_uv_8x16_2, flag1);
    q0_uv_8x16_1 = _mm_add_epi8(q0_uv_8x16_1, q0_uv_8x16_2);
    _mm_storeu_si128((__m128i *)(pu1_src_uv), q0_uv_8x16_1);

}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_deblk_chroma_vert_bs4_mbaff_ssse3()                */
/*                                                                           */
/*  Description   : This function performs filtering of a chroma block       */
/*                  vertical edge when boundary strength is set to 4 in high */
/*                  profile.                                                 */
/*                                                                           */
/*  Inputs        : pu1_src          - pointer to the src sample q0 of U     */
/*                  src_strd         - source stride                         */
/*                  alpha_cb         - alpha value for the boundary in U     */
/*                  beta_cb          - beta value for the boundary in U      */
/*                  alpha_cr         - alpha value for the boundary in V     */
/*                  beta_cr          - beta value for the boundary in V      */
/*                  u4_bs            - packed Boundary strength array        */
/*                  pu1_cliptab_cb   - tc0_table for U                       */
/*                  pu1_cliptab_cr   - tc0_table for V                       */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : When the function is called twice, this operation is as  */
/*                  described in Sec. 8.7.2.4 under the title "Filtering     */
/*                  process for edges for bS equal to 4" in ITU T Rec H.264  */
/*                  with alpha and beta values different in U and V.         */
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
void ih264_deblk_chroma_vert_bs4_mbaff_ssse3(UWORD8 *pu1_src,
                                             WORD32 src_strd,
                                             WORD32 alpha_cb,
                                             WORD32 beta_cb,
                                             WORD32 alpha_cr,
                                             WORD32 beta_cr)
{
    UWORD8 *pu1_src_uv = pu1_src; /* Pointer to the src sample q0 of plane U*/
    WORD32 alpha_cbcr = (alpha_cr << 16) + alpha_cb;
    WORD32 beta_cbcr = (beta_cr << 16) + beta_cb;
    __m128i linea, lineb, linec, lined;
    __m128i temp1, temp2;

    __m128i q0_uv_16x8, p0_uv_16x8, q1_uv_16x8, p1_uv_16x8;
    __m128i q0_uv_8x16, p0_uv_8x16, q1_uv_8x16, p1_uv_8x16;
    __m128i flag1;
    __m128i diff, alpha_cbcr_16x8, beta_cbcr_16x8;
    __m128i zero = _mm_setzero_si128();
    __m128i p0_uv_8x16_1, p0_uv_8x16_2, q0_uv_8x16_1, q0_uv_8x16_2;

    /* Load and transpose the pixel values */
    linea = _mm_loadl_epi64((__m128i *)(pu1_src_uv - 4));
    lineb = _mm_loadl_epi64((__m128i *)(pu1_src_uv - 4 + src_strd));
    linec = _mm_loadl_epi64((__m128i *)(pu1_src_uv - 4 + 2 * src_strd));
    lined = _mm_loadl_epi64((__m128i *)(pu1_src_uv - 4 + 3 * src_strd));

    temp1 = _mm_unpacklo_epi16(linea, lineb);
    temp2 = _mm_unpacklo_epi16(linec, lined);

    p1_uv_16x8 = _mm_unpacklo_epi32(temp1, temp2);
    p0_uv_16x8 = _mm_srli_si128(p1_uv_16x8, 8);
    q0_uv_16x8 = _mm_unpackhi_epi32(temp1, temp2);
    q1_uv_16x8 = _mm_srli_si128(q0_uv_16x8, 8);
    /* End of transpose */

    q0_uv_8x16 = _mm_unpacklo_epi8(q0_uv_16x8, zero);
    q1_uv_8x16 = _mm_unpacklo_epi8(q1_uv_16x8, zero);
    p1_uv_8x16 = _mm_unpacklo_epi8(p1_uv_16x8, zero);
    p0_uv_8x16 = _mm_unpacklo_epi8(p0_uv_16x8, zero);

    diff = _mm_subs_epi16(p0_uv_8x16, q0_uv_8x16); //Condn 1
    diff = _mm_abs_epi16(diff);
    alpha_cbcr_16x8 = _mm_set1_epi32(alpha_cbcr);
    flag1 = _mm_cmpgt_epi16(alpha_cbcr_16x8, diff);

    diff = _mm_subs_epi16(q1_uv_8x16, q0_uv_8x16); //Condtn 2
    diff = _mm_abs_epi16(diff);
    beta_cbcr_16x8 = _mm_set1_epi32(beta_cbcr);
    flag1 = _mm_and_si128(flag1, _mm_cmpgt_epi16(beta_cbcr_16x8, diff));

    diff = _mm_subs_epi16(p1_uv_8x16, p0_uv_8x16); //Condtn 3
    diff = _mm_abs_epi16(diff);
    flag1 = _mm_and_si128(flag1, _mm_cmpgt_epi16(beta_cbcr_16x8, diff));

    temp1 = _mm_slli_epi16(p1_uv_8x16, 1);
    temp2 = _mm_add_epi16(p0_uv_8x16, q1_uv_8x16);
    temp1 = _mm_add_epi16(temp1, _mm_set1_epi16(2));
    temp1 = _mm_add_epi16(temp1, temp2);
    p0_uv_8x16_1 = _mm_srai_epi16(temp1, 2);

    temp1 = _mm_slli_epi16(q1_uv_8x16, 1);
    temp2 = _mm_add_epi16(p1_uv_8x16, q0_uv_8x16);
    temp1 = _mm_add_epi16(temp1, _mm_set1_epi16(2));
    temp1 = _mm_add_epi16(temp1, temp2);
    q0_uv_8x16_1 = _mm_srai_epi16(temp1, 2);

    p0_uv_8x16_2 = _mm_packus_epi16(p0_uv_8x16_1, p0_uv_8x16_1);
    q0_uv_8x16_2 = _mm_packus_epi16(q0_uv_8x16_1, q0_uv_8x16_1);

    flag1 = _mm_packs_epi16(flag1, flag1);

    p0_uv_8x16_1 = _mm_and_si128(p0_uv_16x8,
                                 _mm_xor_si128(flag1, _mm_set1_epi8(0xFF)));
    p0_uv_8x16_2 = _mm_and_si128(p0_uv_8x16_2, flag1);
    p0_uv_16x8 = _mm_add_epi8(p0_uv_8x16_1, p0_uv_8x16_2);

    q0_uv_8x16_1 = _mm_and_si128(q0_uv_16x8,
                                 _mm_xor_si128(flag1, _mm_set1_epi8(0xFF)));
    q0_uv_8x16_2 = _mm_and_si128(q0_uv_8x16_2, flag1);
    q0_uv_16x8 = _mm_add_epi8(q0_uv_8x16_1, q0_uv_8x16_2);

    /* Inverse-transpose and store back */
    temp1 = _mm_unpacklo_epi16(p1_uv_16x8, p0_uv_16x8);
    temp2 = _mm_unpacklo_epi16(q0_uv_16x8, q1_uv_16x8);

    linea = _mm_unpacklo_epi32(temp1, temp2);
    lineb = _mm_srli_si128(linea, 8);
    linec = _mm_unpackhi_epi32(temp1, temp2);
    lined = _mm_srli_si128(linec, 8);

    _mm_storel_epi64((__m128i *)(pu1_src_uv - 4), linea);
    _mm_storel_epi64((__m128i *)(pu1_src_uv - 4 + src_strd), lineb);
    _mm_storel_epi64((__m128i *)(pu1_src_uv - 4 + 2 * src_strd), linec);
    _mm_storel_epi64((__m128i *)(pu1_src_uv - 4 + 3 * src_strd), lined);

}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_deblk_chroma_vert_bslt4_mbaff_ssse3()              */
/*                                                                           */
/*  Description   : This function performs filtering of a chroma block       */
/*                  vertical edge when boundary strength is less than 4 in   */
/*                  high profile.                                            */
/*                                                                           */
/*  Inputs        : pu1_src          - pointer to the src sample q0 of U     */
/*                  src_strd         - source stride                         */
/*                  alpha_cb         - alpha value for the boundary in U     */
/*                  beta_cb          - beta value for the boundary in U      */
/*                  alpha_cr         - alpha value for the boundary in V     */
/*                  beta_cr          - beta value for the boundary in V      */
/*                  u4_bs            - packed Boundary strength array        */
/*                  pu1_cliptab_cb   - tc0_table for U                       */
/*                  pu1_cliptab_cr   - tc0_table for V                       */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : When the function is called twice, this operation is as  */
/*                  described in Sec. 8.7.2.4 under the title "Filtering     */
/*                  process for edges for bS less than 4" in ITU T Rec H.264 */
/*                  with alpha and beta values different in U and V.         */
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
void ih264_deblk_chroma_vert_bslt4_mbaff_ssse3(UWORD8 *pu1_src,
                                               WORD32 src_strd,
                                               WORD32 alpha_cb,
                                               WORD32 beta_cb,
                                               WORD32 alpha_cr,
                                               WORD32 beta_cr,
                                               UWORD32 u4_bs,
                                               const UWORD8 *pu1_cliptab_cb,
                                               const UWORD8 *pu1_cliptab_cr)
{
    UWORD8 *pu1_src_uv = pu1_src; /* Pointer to the src sample q0 of plane U*/
    UWORD8 u1_Bs0, u1_Bs1, u1_Bs2, u1_Bs3;
    WORD32 alpha_cbcr = (alpha_cr << 16) + alpha_cb;
    WORD32 beta_cbcr = (beta_cr << 16) + beta_cb;
    __m128i linea, lineb, linec, lined;
    __m128i temp1, temp2;

    __m128i q0_uv_16x8, p0_uv_16x8, q1_uv_16x8, p1_uv_16x8;
    __m128i q0_uv_8x16, p0_uv_8x16, q1_uv_8x16, p1_uv_8x16;
    __m128i flag_bs, flag1;
    __m128i diff, diff1, alpha_cbcr_16x8, beta_cbcr_16x8, in_macro;
    __m128i zero = _mm_setzero_si128();
    __m128i C0_uv_8x16;
    __m128i p0_uv_8x16_1, p0_uv_8x16_2, q0_uv_8x16_1, q0_uv_8x16_2;

    u1_Bs0 = (u4_bs >> 24) & 0xff;
    u1_Bs1 = (u4_bs >> 16) & 0xff;
    u1_Bs2 = (u4_bs >> 8) & 0xff;
    u1_Bs3 = (u4_bs >> 0) & 0xff;

    flag_bs = _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0, u1_Bs3, u1_Bs3, u1_Bs2,
                           u1_Bs2, u1_Bs1, u1_Bs1, u1_Bs0, u1_Bs0);
    flag_bs = _mm_cmpeq_epi8(flag_bs, zero); //Set flag to 1s and 0s
    flag_bs = _mm_xor_si128(flag_bs, _mm_set1_epi8(0xFF)); //Invert for required mask

    /* Load and transpose the pixel values */
    linea = _mm_loadl_epi64((__m128i *)(pu1_src_uv - 4));
    lineb = _mm_loadl_epi64((__m128i *)(pu1_src_uv - 4 + src_strd));
    linec = _mm_loadl_epi64((__m128i *)(pu1_src_uv - 4 + 2 * src_strd));
    lined = _mm_loadl_epi64((__m128i *)(pu1_src_uv - 4 + 3 * src_strd));

    temp1 = _mm_unpacklo_epi16(linea, lineb);
    temp2 = _mm_unpacklo_epi16(linec, lined);

    p1_uv_16x8 = _mm_unpacklo_epi32(temp1, temp2);
    p0_uv_16x8 = _mm_srli_si128(p1_uv_16x8, 8);
    q0_uv_16x8 = _mm_unpackhi_epi32(temp1, temp2);
    q1_uv_16x8 = _mm_srli_si128(q0_uv_16x8, 8);
    /* End of transpose */

    q0_uv_8x16 = _mm_unpacklo_epi8(q0_uv_16x8, zero);
    q1_uv_8x16 = _mm_unpacklo_epi8(q1_uv_16x8, zero);
    p1_uv_8x16 = _mm_unpacklo_epi8(p1_uv_16x8, zero);
    p0_uv_8x16 = _mm_unpacklo_epi8(p0_uv_16x8, zero);

    diff = _mm_subs_epi16(p0_uv_8x16, q0_uv_8x16); //Condn 1
    diff = _mm_abs_epi16(diff);
    alpha_cbcr_16x8 = _mm_set1_epi32(alpha_cbcr);
    flag1 = _mm_cmpgt_epi16(alpha_cbcr_16x8, diff);

    diff = _mm_subs_epi16(q1_uv_8x16, q0_uv_8x16); //Condtn 2
    diff = _mm_abs_epi16(diff);
    beta_cbcr_16x8 = _mm_set1_epi32(beta_cbcr);
    flag1 = _mm_and_si128(flag1, _mm_cmpgt_epi16(beta_cbcr_16x8, diff));

    diff = _mm_subs_epi16(p1_uv_8x16, p0_uv_8x16); //Condtn 3
    diff = _mm_abs_epi16(diff);
    flag1 = _mm_and_si128(flag1, _mm_cmpgt_epi16(beta_cbcr_16x8, diff));

    diff = _mm_subs_epi16(q0_uv_8x16, p0_uv_8x16);
    diff = _mm_slli_epi16(diff, 2);
    diff1 = _mm_subs_epi16(p1_uv_8x16, q1_uv_8x16);
    diff = _mm_add_epi16(diff, diff1);
    diff = _mm_add_epi16(diff, _mm_set1_epi16(4));
    in_macro = _mm_srai_epi16(diff, 3);

    C0_uv_8x16 = _mm_set_epi16(pu1_cliptab_cr[u1_Bs3], pu1_cliptab_cb[u1_Bs3],
                               pu1_cliptab_cr[u1_Bs2], pu1_cliptab_cb[u1_Bs2],
                               pu1_cliptab_cr[u1_Bs1], pu1_cliptab_cb[u1_Bs1],
                               pu1_cliptab_cr[u1_Bs0], pu1_cliptab_cb[u1_Bs0]);

    C0_uv_8x16 = _mm_add_epi16(C0_uv_8x16, _mm_set1_epi16(1));

    in_macro = _mm_min_epi16(C0_uv_8x16, in_macro); //CLIP3
    C0_uv_8x16 = _mm_subs_epi16(zero, C0_uv_8x16);
    in_macro = _mm_max_epi16(C0_uv_8x16, in_macro);

    p0_uv_8x16_1 = _mm_add_epi16(p0_uv_8x16, in_macro);
    q0_uv_8x16_1 = _mm_sub_epi16(q0_uv_8x16, in_macro);

    p0_uv_8x16_2 = _mm_packus_epi16(p0_uv_8x16_1, p0_uv_8x16_1);
    q0_uv_8x16_2 = _mm_packus_epi16(q0_uv_8x16_1, q0_uv_8x16_1);

    flag1 = _mm_packs_epi16(flag1, flag1);
    flag1 = _mm_and_si128(flag1, flag_bs); //Final flag (BS condition + other 3 conditions)

    p0_uv_8x16_1 = _mm_and_si128(p0_uv_16x8,
                                 _mm_xor_si128(flag1, _mm_set1_epi8(0xFF)));
    p0_uv_8x16_2 = _mm_and_si128(p0_uv_8x16_2, flag1);
    p0_uv_16x8 = _mm_add_epi8(p0_uv_8x16_1, p0_uv_8x16_2);

    q0_uv_8x16_1 = _mm_and_si128(q0_uv_16x8,
                                 _mm_xor_si128(flag1, _mm_set1_epi8(0xFF)));
    q0_uv_8x16_2 = _mm_and_si128(q0_uv_8x16_2, flag1);
    q0_uv_16x8 = _mm_add_epi8(q0_uv_8x16_1, q0_uv_8x16_2);

    /* Inverse-transpose and store back */
    temp1 = _mm_unpacklo_epi16(p1_uv_16x8, p0_uv_16x8);
    temp2 = _mm_unpacklo_epi16(q0_uv_16x8, q1_uv_16x8);

    linea = _mm_unpacklo_epi32(temp1, temp2);
    lineb = _mm_srli_si128(linea, 8);
    linec = _mm_unpackhi_epi32(temp1, temp2);
    lined = _mm_srli_si128(linec, 8);

    _mm_storel_epi64((__m128i *)(pu1_src_uv - 4), linea);
    _mm_storel_epi64((__m128i *)(pu1_src_uv - 4 + src_strd), lineb);
    _mm_storel_epi64((__m128i *)(pu1_src_uv - 4 + 2 * src_strd), linec);
    _mm_storel_epi64((__m128i *)(pu1_src_uv - 4 + 3 * src_strd), lined);

}

