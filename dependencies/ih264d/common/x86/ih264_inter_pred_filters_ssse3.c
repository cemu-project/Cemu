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
/*  File Name         : ih264_inter_pred_filters_intr_ssse3.c                */
/*                                                                           */
/*  Description       : Contains function definitions for weighted           */
/*                      prediction functions in x86 sse4 intrinsics          */
/*                                                                           */
/*  List of Functions : ih264_inter_pred_luma_copy_ssse3()                   */
/*                      ih264_inter_pred_luma_horz_ssse3()                   */
/*                      ih264_inter_pred_luma_vert_ssse3()                   */
/*                      ih264_inter_pred_luma_horz_hpel_vert_hpel_ssse3()    */
/*                      ih264_inter_pred_luma_horz_qpel_ssse3()              */
/*                      ih264_inter_pred_luma_vert_qpel_ssse3()              */
/*                      ih264_inter_pred_luma_horz_qpel_vert_qpel_ssse3()    */
/*                      ih264_inter_pred_luma_horz_hpel_vert_qpel_ssse3()    */
/*                      ih264_inter_pred_luma_horz_qpel_vert_hpel_ssse3()    */
/*                      ih264_inter_pred_chroma_ssse3()                      */
/*                                                                           */
/*  Issues / Problems : None                                                 */
/*                                                                           */
/*  Revision History  :                                                      */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes                              */
/*         13 02 2015   Kaushik         Initial version                      */
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
#include "ih264_inter_pred_filters.h"

/*****************************************************************************/
/* Constant Data variables                                                   */
/*****************************************************************************/

/* coefficients for 6 tap filtering*/
//const WORD32 ih264_g_six_tap[3] ={1,-5,20};
/*****************************************************************************/
/*  Function definitions .                                                   */
/*****************************************************************************/
/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_inter_pred_luma_copy_ssse3                         */
/*                                                                           */
/*  Description   : This function copies the contents of ht x wd block from  */
/*                  source to destination. (ht,wd) can be (4,4), (8,4),      */
/*                  (4,8), (8,8), (16,8), (8,16) or (16,16).                 */
/*                                                                           */
/*  Inputs        : puc_src  - pointer to source                             */
/*                  puc_dst  - pointer to destination                        */
/*                  src_strd - stride for source                             */
/*                  dst_strd - stride for destination                        */
/*                  ht       - height of the block                           */
/*                  wd       - width of the block                            */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes                              */
/*         13 02 2015   Kaushik         Initial Version                      */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_inter_pred_luma_copy_ssse3(UWORD8 *pu1_src,
                                      UWORD8 *pu1_dst,
                                      WORD32 src_strd,
                                      WORD32 dst_strd,
                                      WORD32 ht,
                                      WORD32 wd,
                                      UWORD8* pu1_tmp,
                                      WORD32 dydx)
{
    __m128i y_0_16x8b, y_1_16x8b, y_2_16x8b, y_3_16x8b;

    WORD32 src_strd2, src_strd3, src_strd4, dst_strd2, dst_strd3, dst_strd4;
    UNUSED(pu1_tmp);
    UNUSED(dydx);

    src_strd2 = src_strd << 1;
    dst_strd2 = dst_strd << 1;
    src_strd4 = src_strd << 2;
    dst_strd4 = dst_strd << 2;
    src_strd3 = src_strd2 + src_strd;
    dst_strd3 = dst_strd2 + dst_strd;

    if(wd == 4)
    {
        do
        {
            *((WORD32 *)(pu1_dst)) =  *((WORD32 *)(pu1_src));
            *((WORD32 *)(pu1_dst + dst_strd)) = *((WORD32 *)(pu1_src + src_strd));
            *((WORD32 *)(pu1_dst + dst_strd2)) = *((WORD32 *)(pu1_src + src_strd2));
            *((WORD32 *)(pu1_dst + dst_strd3)) = *((WORD32 *)(pu1_src + src_strd3));

            ht -= 4;
            pu1_src += src_strd4;
            pu1_dst += dst_strd4;
        }
        while(ht > 0);
    }
    else if(wd == 8)
    {
        do
        {
            y_0_16x8b = _mm_loadl_epi64((__m128i *)pu1_src);
            y_1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src + src_strd));
            y_2_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src + src_strd2));
            y_3_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src + src_strd3));

            _mm_storel_epi64((__m128i *)pu1_dst, y_0_16x8b);
            _mm_storel_epi64((__m128i *)(pu1_dst + dst_strd), y_1_16x8b);
            _mm_storel_epi64((__m128i *)(pu1_dst + dst_strd2), y_2_16x8b);
            _mm_storel_epi64((__m128i *)(pu1_dst + dst_strd3), y_3_16x8b);

            ht -= 4;
            pu1_src += src_strd4;
            pu1_dst += dst_strd4;
        }
        while(ht > 0);
    }
    else // wd == 16
    {
        WORD32 src_strd5, src_strd6, src_strd7, src_strd8;
        WORD32 dst_strd5, dst_strd6, dst_strd7, dst_strd8;

        __m128i y_4_16x8b, y_5_16x8b, y_6_16x8b, y_7_16x8b;

        src_strd5 = src_strd2 + src_strd3;
        dst_strd5 = dst_strd2 + dst_strd3;
        src_strd6 = src_strd3 << 1;
        dst_strd6 = dst_strd3 << 1;
        src_strd7 = src_strd3 + src_strd4;
        dst_strd7 = dst_strd3 + dst_strd4;
        src_strd8 = src_strd << 3;
        dst_strd8 = dst_strd << 3;

        do
        {
            y_0_16x8b = _mm_loadu_si128((__m128i *)pu1_src);
            y_1_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + src_strd));
            y_2_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + src_strd2));
            y_3_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + src_strd3));
            y_4_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + src_strd4));
            y_5_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + src_strd5));
            y_6_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + src_strd6));
            y_7_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + src_strd7));

            _mm_storeu_si128((__m128i *)pu1_dst, y_0_16x8b);
            _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), y_1_16x8b);
            _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd2), y_2_16x8b);
            _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd3), y_3_16x8b);
            _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd4), y_4_16x8b);
            _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd5), y_5_16x8b);
            _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd6), y_6_16x8b);
            _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd7), y_7_16x8b);

            ht -= 8;
            pu1_src += src_strd8;
            pu1_dst += dst_strd8;
        }
        while(ht > 0);
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_inter_pred_luma_horz_ssse3                         */
/*                                                                           */
/*  Description   : This function applies a horizontal 6-tap filter on       */
/*                  ht x wd block as mentioned in sec. 8.4.2.2.1 titled      */
/*                  "Luma sample interpolation process". (ht,wd) can be      */
/*                  (4,4), (8,4), (4,8), (8,8), (16,8), (8,16) or (16,16).   */
/*                                                                           */
/*  Inputs        : puc_src  - pointer to source                             */
/*                  puc_dst  - pointer to destination                        */
/*                  src_strd - stride for source                             */
/*                  dst_strd - stride for destination                        */
/*                  ht       - height of the block                           */
/*                  wd       - width of the block                            */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes                              */
/*         13 02 2015   Kaushik         Initial Version                      */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_inter_pred_luma_horz_ssse3(UWORD8 *pu1_src,
                                      UWORD8 *pu1_dst,
                                      WORD32 src_strd,
                                      WORD32 dst_strd,
                                      WORD32 ht,
                                      WORD32 wd,
                                      UWORD8* pu1_tmp,
                                      WORD32 dydx)
{
    __m128i coeff0_1_16x8b, coeff2_3_16x8b, coeff4_5_16x8b;
    __m128i const_val16_8x16b;

    UNUSED(pu1_tmp);
    UNUSED(dydx);

    pu1_src -= 2; // the filter input starts from x[-2] (till x[3])

    coeff0_1_16x8b = _mm_set1_epi32(0xFB01FB01); //c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1
    coeff2_3_16x8b = _mm_set1_epi32(0x14141414); //c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3
    coeff4_5_16x8b = _mm_set1_epi32(0x01FB01FB); //c4 c5 c4 c5 c4 c5 c4 c5 c4 c5 c4 c5 c4 c5 c4 c5
                                                 //c0 = c5 = 1, c1 = c4 = -5, c2 = c3 = 20
    const_val16_8x16b = _mm_set1_epi16(16);

    if(wd == 4)
    {
        __m128i src_r0_16x8b, src_r1_16x8b, src_r0r1_16x8b;
        __m128i src_r0_sht_16x8b, src_r1_sht_16x8b;

        __m128i res_r0r1_t1_8x16b, res_r0r1_t2_8x16b, res_r0r1_t3_8x16b;
        __m128i res_r0r1_16x8b;

        //Row0 : a0 a1 a2 a3 a4 a5 a6 a7 a8 a9.....
        //Row1 : b0 b1 b2 b3 b4 b5 b6 b7 b8 b9.....

        do
        {
            src_r0_16x8b = _mm_loadu_si128((__m128i *)pu1_src);                     //a0 a1 a2 a3 a4 a5 a6 a7 a8 a9....a15
            src_r1_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + src_strd));        //b0 b1 b2 b3 b4 b5 b6 b7 b8 b9....b15

            src_r0_sht_16x8b = _mm_srli_si128(src_r0_16x8b, 1);                     //a1 a2 a3 a4 a5 a6 a7 a8 a9....a15 0
            src_r1_sht_16x8b = _mm_srli_si128(src_r1_16x8b, 1);                     //b1 b2 b3 b4 b5 b6 b7 b8 b9....b15 0

            src_r0_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);       //a0 a1 a1 a2 a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8
            src_r1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);       //b0 b1 b1 b2 b2 b3 b3 b4 b4 b5 b5 b6 b6 b7 b7 b8

            src_r0r1_16x8b = _mm_unpacklo_epi64(src_r0_16x8b, src_r1_16x8b);        //a0 a1 a1 a2 a2 a3 a3 a4 b0 b1 b1 b2 b2 b3 b3 b4
            res_r0r1_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);  //a0*c0+a1*c1 a1*c0+a2*c1 a2*c0+a3*c1 a3*c0+a4*c1
                                                                                    //b0*c0+b1*c1 b1*c0+b2*c1 b2*c0+b3*c1 b3*c0+b4*c1

            src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 4);                         //a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8  0  0  0  0
            src_r1_16x8b = _mm_srli_si128(src_r1_16x8b, 4);                         //b2 b3 b3 b4 b4 b5 b5 b6 b6 b7 b7 b8  0  0  0  0

            src_r0r1_16x8b = _mm_unpacklo_epi64(src_r0_16x8b, src_r1_16x8b);        //a2 a3 a3 a4 a4 a5 a5 a6 b2 b3 b3 b4 b4 b5 b5 b6
            res_r0r1_t2_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff2_3_16x8b);  //a2*c2+a3*c3 a3*c2+a4*c3 a4*c2+a5*c3 a5*c2+a6*c3
                                                                                    //b2*c2+b3*c3 b3*c2+b4*c3 b4*c2+b5*c3 b5*c2+b6*c3

            src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 4);                         //a4 a5 a5 a6 a6 a7 a7 a8  0  0  0  0  0  0  0  0
            src_r1_16x8b = _mm_srli_si128(src_r1_16x8b, 4);                         //b4 b5 b5 b6 b6 b7 b7 b8  0  0  0  0  0  0  0  0

            src_r0r1_16x8b = _mm_unpacklo_epi64(src_r0_16x8b, src_r1_16x8b);        //a4 a5 a5 a6 a6 a7 a7 a8 b4 b5 b5 b6 b6 b7 b7 b8
            res_r0r1_t3_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff4_5_16x8b);  //a4*c4+a5*c5 a5*c4+a6*c5 a6*c4+a7*c5 a7*c4+a8*c5
                                                                                    //b4*c4+b5*c5 b5*c4+b6*c5 b4*c6+b7*c5 b7*c4+b8*c5

            res_r0r1_t1_8x16b = _mm_add_epi16(res_r0r1_t1_8x16b, res_r0r1_t2_8x16b);
            res_r0r1_t3_8x16b = _mm_add_epi16(const_val16_8x16b, res_r0r1_t3_8x16b);
            res_r0r1_t1_8x16b = _mm_add_epi16(res_r0r1_t1_8x16b, res_r0r1_t3_8x16b); //a0*c0+a1*c1+a2*c2+a3*c3+a4*a4+a5*c5 + 16;
                                                                                     //a1*c0+a2*c1+a2*c2+a3*c3+a5*a4+a6*c5 + 16;
                                                                                     //a2*c0+a3*c1+a4*c2+a5*c3+a6*a4+a7*c5 + 16;
                                                                                     //a3*c0+a4*c1+a5*c2+a6*c3+a6*a4+a8*c5 + 16;
                                                                                     //b0*c0+b1*c1+b2*c2+b3*c3+b4*b4+b5*c5 + 16;
                                                                                     //b1*c0+b2*c1+b2*c2+b3*c3+b5*b4+b6*c5 + 16;
                                                                                     //b2*c0+b3*c1+b4*c2+b5*c3+b6*b4+b7*c5 + 16;
                                                                                     //b3*c0+b4*c1+b5*c2+b6*c3+b6*b4+b8*c5 + 16;

            res_r0r1_t1_8x16b = _mm_srai_epi16(res_r0r1_t1_8x16b, 5);                //shifting right by 5 bits.

            res_r0r1_16x8b = _mm_packus_epi16(res_r0r1_t1_8x16b, res_r0r1_t1_8x16b);

            *((WORD32 *)(pu1_dst)) = _mm_cvtsi128_si32(res_r0r1_16x8b);
            res_r0r1_16x8b = _mm_srli_si128(res_r0r1_16x8b, 4);
            *((WORD32 *)(pu1_dst + dst_strd)) = _mm_cvtsi128_si32(res_r0r1_16x8b);

            ht -= 2;
            pu1_src += src_strd << 1;
            pu1_dst += dst_strd << 1;
        }
        while(ht > 0);
    }
    else if(wd == 8)
    {
        __m128i src_r0_16x8b, src_r1_16x8b, src_r0_sht_16x8b, src_r1_sht_16x8b;
        __m128i src_r0_t1_16x8b, src_r1_t1_16x8b;

        __m128i res_r0_t1_8x16b, res_r0_t2_8x16b, res_r0_t3_8x16b;
        __m128i res_r1_t1_8x16b, res_r1_t2_8x16b, res_r1_t3_8x16b;

        //Row0 : a0 a1 a2 a3 a4 a5 a6 a7 a8 a9.....
        //Row1 : b0 b1 b2 b3 b4 b5 b6 b7 b8 b9.....

        do
        {
            src_r0_16x8b = _mm_loadu_si128((__m128i *)pu1_src);                   //a0 a1 a2 a3 a4 a5 a6 a7 a8 a9....a15
            src_r1_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + src_strd));      //b0 b1 b2 b3 b4 b5 b6 b7 b8 b9....b15

            src_r0_sht_16x8b = _mm_srli_si128(src_r0_16x8b, 1);                   //a1 a2 a3 a4 a5 a6 a7 a8 a9....a15 0
            src_r1_sht_16x8b = _mm_srli_si128(src_r1_16x8b, 1);                   //b1 b2 b3 b4 b5 b6 b7 b8 b9....b15 0

            src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);  //a0 a1 a1 a2 a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8
            src_r1_t1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);  //b0 b1 b1 b2 b2 b3 b3 b4 b4 b5 b5 b6 b6 b7 b7 b8

            res_r0_t1_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff0_1_16x8b); //a0*c0+a1*c1 a1*c0+a2*c1 a2*c0+a3*c1 a3*c0+a4*c1
                                                                                  //a4*c0+a5*c1 a5*c0+a6*c1 a6*c0+a7*c1 a7*c0+a8*c1
            res_r1_t1_8x16b = _mm_maddubs_epi16(src_r1_t1_16x8b, coeff0_1_16x8b); //b0*c0+b1*c1 b1*c0+b2*c1 b2*c0+b3*c1 b3*c0+b4*c1
                                                                                  //b4*c0+b5*c1 b5*c0+b6*c1 b6*c0+b7*c1 b7*c0+b8*c1

            src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 2);                       //a2 a3 a4 a5 a6 a7 a8 a9....a15 0 0
            src_r1_16x8b = _mm_srli_si128(src_r1_16x8b, 2);                       //b2 b3 b4 b5 b6 b7 b8 b9....b15 0 0

            src_r0_sht_16x8b = _mm_srli_si128(src_r0_sht_16x8b, 2);               //a3 a4 a5 a6 a7 a8 a9....a15 0  0  0
            src_r1_sht_16x8b = _mm_srli_si128(src_r1_sht_16x8b, 2);               //b3 b4 b5 b6 b7 b8 b9....b15 0  0  0

            src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);  //a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8 a8 a9 a9 a10
            src_r1_t1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);  //b2 b3 b3 b4 b4 b5 b5 b6 b6 b7 b7 b8 a8 a9 a9 a10

            res_r0_t2_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff2_3_16x8b); //a2*c2+a3*c3 a3*c2+a4*c3 a4*c2+a5*c3 a5*c2+a6*c3
                                                                                  //a6*c2+a7*c3 a7*c2+a8*c3 a8*c2+a9*c3 a9*c2+a10*c3
            res_r1_t2_8x16b = _mm_maddubs_epi16(src_r1_t1_16x8b, coeff2_3_16x8b); //b2*c2+b3*c3 b3*c2+b4*c3 b2*c4+b5*c3 b5*c2+b6*c3
                                                                                  //b6*c2+b7*c3 b7*c2+b8*c3 b8*c2+b9*c3 b9*c2+b10*c3

            src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 2);                       //a4 a5 a6 a7 a8 a9....a15 0  0  0  0
            src_r1_16x8b = _mm_srli_si128(src_r1_16x8b, 2);                       //b4 b5 b6 b7 b8 b9....b15 0  0  0  0

            src_r0_sht_16x8b = _mm_srli_si128(src_r0_sht_16x8b, 2);               //a5 a6 a7 a8 a9....a15 0  0  0  0  0
            src_r1_sht_16x8b = _mm_srli_si128(src_r1_sht_16x8b, 2);               //b5 b6 b7 b8 b9....b15 0  0  0  0  0

            src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);  //a4 a5 a5 a6 a6 a7 a7 a8 a8 a9 a9 a10 a10 a11 a11 a12
            src_r1_t1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);  //b4 b5 b5 b6 b6 b7 b7 b8 b8 b9 b9 b10 b10 b11 b11 b12

            res_r0_t3_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff4_5_16x8b); //a4*c4+a5*c5 a5*c4+a6*c5 a6*c4+a7*c5 a7*c4+a8*c5
                                                                                  //a8*c4+a9*c5 a9*c4+a10*c5 a10*c4+a11*c5 a11*c4+a12*c5
            res_r1_t3_8x16b = _mm_maddubs_epi16(src_r1_t1_16x8b, coeff4_5_16x8b); //b4*c4+b5*c5 b5*c4+b6*c5 b6*c4+b7*c5 b7*c4+b8*c5
                                                                                  //b8*c4+b9*c5 b9*c4+b10*c5 b10*c4+b11*c5 b11*c4+b12*c5
            res_r0_t1_8x16b = _mm_add_epi16(res_r0_t1_8x16b, res_r0_t2_8x16b);
            res_r1_t1_8x16b = _mm_add_epi16(res_r1_t1_8x16b, res_r1_t2_8x16b);
            res_r0_t3_8x16b = _mm_add_epi16(res_r0_t3_8x16b, const_val16_8x16b);
            res_r1_t3_8x16b = _mm_add_epi16(res_r1_t3_8x16b, const_val16_8x16b);
            res_r0_t1_8x16b = _mm_add_epi16(res_r0_t1_8x16b, res_r0_t3_8x16b);
            res_r1_t1_8x16b = _mm_add_epi16(res_r1_t1_8x16b, res_r1_t3_8x16b);

            res_r0_t1_8x16b = _mm_srai_epi16(res_r0_t1_8x16b, 5);                 //shifting right by 5 bits.
            res_r1_t1_8x16b = _mm_srai_epi16(res_r1_t1_8x16b, 5);

            src_r0_16x8b = _mm_packus_epi16(res_r0_t1_8x16b, res_r0_t1_8x16b);
            src_r1_16x8b = _mm_packus_epi16(res_r1_t1_8x16b, res_r1_t1_8x16b);

            _mm_storel_epi64((__m128i *)pu1_dst, src_r0_16x8b);
            _mm_storel_epi64((__m128i *)(pu1_dst + dst_strd), src_r1_16x8b);

            ht -= 2;
            pu1_src += src_strd << 1;
            pu1_dst += dst_strd << 1;
        }
        while(ht > 0);
    }
    else // wd == 16
    {
        __m128i src_r0_16x8b, src_r1_16x8b, src_r0_sht_16x8b, src_r1_sht_16x8b;
        __m128i src_r0_t1_16x8b, src_r1_t1_16x8b;

        __m128i res_r0_t1_8x16b, res_r0_t2_8x16b, res_r0_t3_8x16b;
        __m128i res_r1_t1_8x16b, res_r1_t2_8x16b, res_r1_t3_8x16b;

        //Row0 : a0 a1 a2 a3 a4 a5 a6 a7 a8 a9.....
        //Row0 :                         b0 b1 b2 b3 b4 b5 b6 b7 b8 b9.....
        //b0 is same a8. Similarly other bn pixels are same as a(n+8) pixels.

        do
        {
            src_r0_16x8b = _mm_loadu_si128((__m128i *)pu1_src);                  //a0 a1 a2 a3 a4 a5 a6 a7 a8 a9....a15
            src_r1_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + 8));            //b0 b1 b2 b3 b4 b5 b6 b7 b8 b9....b15

            src_r0_sht_16x8b = _mm_srli_si128(src_r0_16x8b, 1);                   //a1 a2 a3 a4 a5 a6 a7 a8 a9....a15 0
            src_r1_sht_16x8b = _mm_srli_si128(src_r1_16x8b, 1);                   //b1 b2 b3 b4 b5 b6 b7 b8 b9....b15 0

            src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);  //a0 a1 a1 a2 a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8
            src_r1_t1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);  //b0 b1 b1 b2 b2 b3 b3 b4 b4 b5 b5 b6 b6 b7 b7 b8

            res_r0_t1_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff0_1_16x8b); //a0*c0+a1*c1 a1*c0+a2*c1 a2*c0+a3*c1 a3*c0+a4*c1
                                                                                  //a4*c0+a5*c1 a5*c0+a6*c1 a6*c0+a7*c1 a7*c0+a8*c1
            res_r1_t1_8x16b = _mm_maddubs_epi16(src_r1_t1_16x8b, coeff0_1_16x8b); //b0*c0+b1*c1 b1*c0+b2*c1 b2*c0+b3*c1 b3*c0+b4*c1
                                                                                  //b4*c0+b5*c1 b5*c0+b6*c1 b6*c0+b7*c1 b7*c0+b8*c1

            src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 2);                       //a2 a3 a4 a5 a6 a7 a8 a9....a15 0 0
            src_r1_16x8b = _mm_srli_si128(src_r1_16x8b, 2);                       //b2 b3 b4 b5 b6 b7 b8 b9....b15 0 0

            src_r0_sht_16x8b = _mm_srli_si128(src_r0_sht_16x8b, 2);               //a3 a4 a5 a6 a7 a8 a9....a15 0  0  0
            src_r1_sht_16x8b = _mm_srli_si128(src_r1_sht_16x8b, 2);               //b3 b4 b5 b6 b7 b8 b9....b15 0  0  0

            src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);  //a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8 a8 a9 a9 a10
            src_r1_t1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);  //b2 b3 b3 b4 b4 b5 b5 b6 b6 b7 b7 b8 a8 a9 a9 a10

            res_r0_t2_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff2_3_16x8b); //a2*c2+a3*c3 a3*c2+a4*c3 a4*c2+a5*c3 a5*c2+a6*c3
                                                                                  //a6*c2+a7*c3 a7*c2+a8*c3 a8*c2+a9*c3 a9*c2+a10*c3
            res_r1_t2_8x16b = _mm_maddubs_epi16(src_r1_t1_16x8b, coeff2_3_16x8b); //b2*c2+b3*c3 b3*c2+b4*c3 b2*c4+b5*c3 b5*c2+b6*c3
                                                                                  //b6*c2+b7*c3 b7*c2+b8*c3 b8*c2+b9*c3 b9*c2+b10*c3

            src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 2);                       //a4 a5 a6 a7 a8 a9....a15 0  0  0  0
            src_r1_16x8b = _mm_srli_si128(src_r1_16x8b, 2);                       //b4 b5 b6 b7 b8 b9....b15 0  0  0  0

            src_r0_sht_16x8b = _mm_srli_si128(src_r0_sht_16x8b, 2);               //a5 a6 a7 a8 a9....a15 0  0  0  0  0
            src_r1_sht_16x8b = _mm_srli_si128(src_r1_sht_16x8b, 2);               //b5 b6 b7 b8 b9....b15 0  0  0  0  0

            src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);  //a4 a5 a5 a6 a6 a7 a7 a8 a8 a9 a9 a10 a10 a11 a11 a12
            src_r1_t1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);  //b4 b5 b5 b6 b6 b7 b7 b8 b8 b9 b9 b10 b10 b11 b11 b12

            res_r0_t3_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff4_5_16x8b); //a4*c4+a5*c5 a5*c4+a6*c5 a6*c4+a7*c5 a7*c4+a8*c5
                                                                                  //a8*c4+a9*c5 a9*c4+a10*c5 a10*c4+a11*c5 a11*c4+a12*c5
            res_r1_t3_8x16b = _mm_maddubs_epi16(src_r1_t1_16x8b, coeff4_5_16x8b); //b4*c4+b5*c5 b5*c4+b6*c5 b6*c4+b7*c5 b7*c4+b8*c5
                                                                                  //b8*c4+b9*c5 b9*c4+b10*c5 b10*c4+b11*c5 b11*c4+b12*c5
            res_r0_t1_8x16b = _mm_add_epi16(res_r0_t1_8x16b, res_r0_t2_8x16b);
            res_r1_t1_8x16b = _mm_add_epi16(res_r1_t1_8x16b, res_r1_t2_8x16b);
            res_r0_t3_8x16b = _mm_add_epi16(res_r0_t3_8x16b, const_val16_8x16b);
            res_r1_t3_8x16b = _mm_add_epi16(res_r1_t3_8x16b, const_val16_8x16b);
            res_r0_t1_8x16b = _mm_add_epi16(res_r0_t1_8x16b, res_r0_t3_8x16b);
            res_r1_t1_8x16b = _mm_add_epi16(res_r1_t1_8x16b, res_r1_t3_8x16b);

            res_r0_t1_8x16b = _mm_srai_epi16(res_r0_t1_8x16b, 5);                 //shifting right by 5 bits.
            res_r1_t1_8x16b = _mm_srai_epi16(res_r1_t1_8x16b, 5);

            src_r0_16x8b = _mm_packus_epi16(res_r0_t1_8x16b, res_r1_t1_8x16b);
            _mm_storeu_si128((__m128i *)pu1_dst, src_r0_16x8b);

            ht--;
            pu1_src += src_strd;
            pu1_dst += dst_strd;
        }
        while(ht > 0);
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_inter_pred_luma_vert_ssse3                         */
/*                                                                           */
/*  Description   : This function applies a vertical 6-tap filter on         */
/*                  ht x wd block as mentioned in sec. 8.4.2.2.1 titled      */
/*                  "Luma sample interpolation process". (ht,wd) can be      */
/*                  (4,4), (8,4), (4,8), (8,8), (16,8), (8,16) or (16,16).   */
/*                                                                           */
/*  Inputs        : puc_src  - pointer to source                             */
/*                  puc_dst  - pointer to destination                        */
/*                  src_strd - stride for source                             */
/*                  dst_strd - stride for destination                        */
/*                  ht       - height of the block                           */
/*                  wd       - width of the block                            */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes                              */
/*         13 02 2015   Kaushik         Initial Version                      */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_inter_pred_luma_vert_ssse3(UWORD8 *pu1_src,
                                      UWORD8 *pu1_dst,
                                      WORD32 src_strd,
                                      WORD32 dst_strd,
                                      WORD32 ht,
                                      WORD32 wd,
                                      UWORD8* pu1_tmp,
                                      WORD32 dydx)
{
    __m128i src_r0_16x8b, src_r1_16x8b, src_r2_16x8b, src_r3_16x8b, src_r4_16x8b;
    __m128i src_r5_16x8b, src_r6_16x8b;
    __m128i src_r0r1_16x8b, src_r2r3_16x8b, src_r4r5_16x8b;

    __m128i res_16x8b, res_t1_8x16b, res_t2_8x16b, res_t3_8x16b;

    __m128i coeff0_1_16x8b, coeff2_3_16x8b, coeff4_5_16x8b;
    __m128i const_val16_8x16b;

    UNUSED(pu1_tmp);
    UNUSED(dydx);

    coeff0_1_16x8b = _mm_set1_epi32(0xFB01FB01); //c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1
    coeff2_3_16x8b = _mm_set1_epi32(0x14141414); //c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3
    coeff4_5_16x8b = _mm_set1_epi32(0x01FB01FB); //c4 c5 c4 c5 c4 c5 c4 c5 c4 c5 c4 c5 c4 c5 c4 c5
                                                 //c0 = c5 = 1, c1 = c4 = -5, c2 = c3 = 20
    const_val16_8x16b = _mm_set1_epi16(16);

    pu1_src -= src_strd << 1; // the filter input starts from x[-2] (till x[3])

    if(wd == 4)
    {
        //Epilogue: Load all the pred rows except sixth and seventh row
        //          for the first and second row processing.
        src_r0_16x8b = _mm_loadl_epi64((__m128i *)pu1_src);
        pu1_src += src_strd;
        src_r1_16x8b = _mm_loadl_epi64((__m128i *)pu1_src);
        pu1_src += src_strd;
        src_r2_16x8b = _mm_loadl_epi64((__m128i *)pu1_src);
        pu1_src += src_strd;
        src_r3_16x8b = _mm_loadl_epi64((__m128i *)pu1_src);
        pu1_src += src_strd;
        src_r4_16x8b = _mm_loadl_epi64((__m128i *)pu1_src);
        pu1_src += src_strd;

        src_r0_16x8b = _mm_unpacklo_epi32(src_r0_16x8b, src_r1_16x8b);
        src_r1_16x8b = _mm_unpacklo_epi32(src_r1_16x8b, src_r2_16x8b);
        src_r2_16x8b = _mm_unpacklo_epi32(src_r2_16x8b, src_r3_16x8b);
        src_r3_16x8b = _mm_unpacklo_epi32(src_r3_16x8b, src_r4_16x8b);

        do
        {
            src_r5_16x8b = _mm_loadl_epi64((__m128i *)pu1_src);
            src_r6_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src + src_strd));

            src_r4_16x8b = _mm_unpacklo_epi32(src_r4_16x8b, src_r5_16x8b);
            src_r5_16x8b = _mm_unpacklo_epi32(src_r5_16x8b, src_r6_16x8b);

            src_r0r1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r1_16x8b);
            src_r2r3_16x8b = _mm_unpacklo_epi8(src_r2_16x8b, src_r3_16x8b);
            src_r4r5_16x8b = _mm_unpacklo_epi8(src_r4_16x8b, src_r5_16x8b);

            res_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);
            res_t2_8x16b = _mm_maddubs_epi16(src_r2r3_16x8b, coeff2_3_16x8b);
            res_t3_8x16b = _mm_maddubs_epi16(src_r4r5_16x8b, coeff4_5_16x8b);

            res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t2_8x16b);
            res_t3_8x16b = _mm_add_epi16(res_t3_8x16b, const_val16_8x16b);
            res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t3_8x16b);

            res_t1_8x16b = _mm_srai_epi16(res_t1_8x16b, 5); //shifting right by 5 bits.
            res_16x8b = _mm_packus_epi16(res_t1_8x16b, res_t1_8x16b);

            *((WORD32 *)(pu1_dst)) = _mm_cvtsi128_si32(res_16x8b);
            res_16x8b = _mm_srli_si128(res_16x8b, 4);
            *((WORD32 *)(pu1_dst + dst_strd)) = _mm_cvtsi128_si32(res_16x8b);

            src_r0_16x8b = src_r2_16x8b;
            src_r1_16x8b = src_r3_16x8b;
            src_r2_16x8b = src_r4_16x8b;
            src_r3_16x8b = src_r5_16x8b;
            src_r4_16x8b = src_r6_16x8b;

            ht -= 2;
            pu1_src += src_strd << 1;
            pu1_dst += dst_strd << 1;
        }
        while(ht > 0);
    }

    else if(wd == 8)
    {
        //Epilogue: Load all the pred rows except sixth and seventh row
        //          for the first and second row processing.
        src_r0_16x8b = _mm_loadl_epi64((__m128i *)pu1_src);
        pu1_src += src_strd;
        src_r1_16x8b = _mm_loadl_epi64((__m128i *)pu1_src);
        pu1_src += src_strd;
        src_r2_16x8b = _mm_loadl_epi64((__m128i *)pu1_src);
        pu1_src += src_strd;
        src_r3_16x8b = _mm_loadl_epi64((__m128i *)pu1_src);
        pu1_src += src_strd;
        src_r4_16x8b = _mm_loadl_epi64((__m128i *)pu1_src);
        pu1_src += src_strd;

        src_r0_16x8b = _mm_unpacklo_epi64(src_r0_16x8b, src_r1_16x8b);
        src_r1_16x8b = _mm_unpacklo_epi64(src_r1_16x8b, src_r2_16x8b);
        src_r2_16x8b = _mm_unpacklo_epi64(src_r2_16x8b, src_r3_16x8b);
        src_r3_16x8b = _mm_unpacklo_epi64(src_r3_16x8b, src_r4_16x8b);

        do
        {
            src_r5_16x8b = _mm_loadl_epi64((__m128i *)pu1_src);
            src_r6_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src + src_strd));

            src_r4_16x8b = _mm_unpacklo_epi64(src_r4_16x8b, src_r5_16x8b);
            src_r5_16x8b = _mm_unpacklo_epi64(src_r5_16x8b, src_r6_16x8b);

            src_r0r1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r1_16x8b);
            src_r2r3_16x8b = _mm_unpacklo_epi8(src_r2_16x8b, src_r3_16x8b);
            src_r4r5_16x8b = _mm_unpacklo_epi8(src_r4_16x8b, src_r5_16x8b);

            res_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);
            res_t2_8x16b = _mm_maddubs_epi16(src_r2r3_16x8b, coeff2_3_16x8b);
            res_t3_8x16b = _mm_maddubs_epi16(src_r4r5_16x8b, coeff4_5_16x8b);

            res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t2_8x16b);
            res_t3_8x16b = _mm_add_epi16(res_t3_8x16b, const_val16_8x16b);
            res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t3_8x16b);

            res_t1_8x16b = _mm_srai_epi16(res_t1_8x16b, 5); //shifting right by 5 bits.
            res_16x8b = _mm_packus_epi16(res_t1_8x16b, res_t1_8x16b);

            _mm_storel_epi64((__m128i *)pu1_dst, res_16x8b);

            src_r0r1_16x8b = _mm_unpackhi_epi8(src_r0_16x8b, src_r1_16x8b);
            src_r2r3_16x8b = _mm_unpackhi_epi8(src_r2_16x8b, src_r3_16x8b);
            src_r4r5_16x8b = _mm_unpackhi_epi8(src_r4_16x8b, src_r5_16x8b);

            res_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);
            res_t2_8x16b = _mm_maddubs_epi16(src_r2r3_16x8b, coeff2_3_16x8b);
            res_t3_8x16b = _mm_maddubs_epi16(src_r4r5_16x8b, coeff4_5_16x8b);

            res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t2_8x16b);
            res_t3_8x16b = _mm_add_epi16(res_t3_8x16b, const_val16_8x16b);
            res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t3_8x16b);

            res_t1_8x16b = _mm_srai_epi16(res_t1_8x16b, 5); //shifting right by 5 bits.
            res_16x8b = _mm_packus_epi16(res_t1_8x16b, res_t1_8x16b);

            _mm_storel_epi64((__m128i *)(pu1_dst + dst_strd), res_16x8b);

            src_r0_16x8b = src_r2_16x8b;
            src_r1_16x8b = src_r3_16x8b;
            src_r2_16x8b = src_r4_16x8b;
            src_r3_16x8b = src_r5_16x8b;
            src_r4_16x8b = src_r6_16x8b;

            ht -= 2;
            pu1_src += src_strd << 1;
            pu1_dst += dst_strd << 1;
        }
        while(ht > 0);
    }
    else // wd == 16
    {
        __m128i res_t0_8x16b;

        //Epilogue: Load all the pred rows except sixth and seventh row
        //          for the first and second row processing.
        src_r0_16x8b = _mm_loadu_si128((__m128i *)pu1_src);
        pu1_src += src_strd;
        src_r1_16x8b = _mm_loadu_si128((__m128i *)pu1_src);
        pu1_src += src_strd;
        src_r2_16x8b = _mm_loadu_si128((__m128i *)pu1_src);
        pu1_src += src_strd;
        src_r3_16x8b = _mm_loadu_si128((__m128i *)pu1_src);
        pu1_src += src_strd;
        src_r4_16x8b = _mm_loadu_si128((__m128i *)pu1_src);
        pu1_src += src_strd;

        do
        {
            src_r5_16x8b  = _mm_loadu_si128((__m128i *)pu1_src);
            src_r6_16x8b  = _mm_loadu_si128((__m128i *)(pu1_src + src_strd));

            src_r0r1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r1_16x8b);
            src_r2r3_16x8b = _mm_unpacklo_epi8(src_r2_16x8b, src_r3_16x8b);
            src_r4r5_16x8b = _mm_unpacklo_epi8(src_r4_16x8b, src_r5_16x8b);

            res_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);
            res_t2_8x16b = _mm_maddubs_epi16(src_r2r3_16x8b, coeff2_3_16x8b);
            res_t3_8x16b = _mm_maddubs_epi16(src_r4r5_16x8b, coeff4_5_16x8b);

            res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t2_8x16b);
            res_t3_8x16b = _mm_add_epi16(res_t3_8x16b, const_val16_8x16b);
            res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t3_8x16b);
            res_t0_8x16b = _mm_srai_epi16(res_t1_8x16b, 5); //shifting right by 5 bits.

            src_r0r1_16x8b = _mm_unpackhi_epi8(src_r0_16x8b, src_r1_16x8b);
            src_r2r3_16x8b = _mm_unpackhi_epi8(src_r2_16x8b, src_r3_16x8b);
            src_r4r5_16x8b = _mm_unpackhi_epi8(src_r4_16x8b, src_r5_16x8b);

            res_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);
            res_t2_8x16b = _mm_maddubs_epi16(src_r2r3_16x8b, coeff2_3_16x8b);
            res_t3_8x16b = _mm_maddubs_epi16(src_r4r5_16x8b, coeff4_5_16x8b);

            res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t2_8x16b);
            res_t3_8x16b = _mm_add_epi16(res_t3_8x16b, const_val16_8x16b);
            res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t3_8x16b);
            res_t1_8x16b = _mm_srai_epi16(res_t1_8x16b, 5); //shifting right by 5 bits.

            res_16x8b = _mm_packus_epi16(res_t0_8x16b, res_t1_8x16b);

            _mm_storeu_si128((__m128i *)pu1_dst, res_16x8b);

            src_r0r1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r2_16x8b);
            src_r2r3_16x8b = _mm_unpacklo_epi8(src_r3_16x8b, src_r4_16x8b);
            src_r4r5_16x8b = _mm_unpacklo_epi8(src_r5_16x8b, src_r6_16x8b);

            res_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);
            res_t2_8x16b = _mm_maddubs_epi16(src_r2r3_16x8b, coeff2_3_16x8b);
            res_t3_8x16b = _mm_maddubs_epi16(src_r4r5_16x8b, coeff4_5_16x8b);

            res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t2_8x16b);
            res_t3_8x16b = _mm_add_epi16(res_t3_8x16b, const_val16_8x16b);
            res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t3_8x16b);
            res_t0_8x16b = _mm_srai_epi16(res_t1_8x16b, 5); //shifting right by 5 bits.

            src_r0r1_16x8b = _mm_unpackhi_epi8(src_r1_16x8b, src_r2_16x8b);
            src_r2r3_16x8b = _mm_unpackhi_epi8(src_r3_16x8b, src_r4_16x8b);
            src_r4r5_16x8b = _mm_unpackhi_epi8(src_r5_16x8b, src_r6_16x8b);

            res_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);
            res_t2_8x16b = _mm_maddubs_epi16(src_r2r3_16x8b, coeff2_3_16x8b);
            res_t3_8x16b = _mm_maddubs_epi16(src_r4r5_16x8b, coeff4_5_16x8b);

            res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t2_8x16b);
            res_t3_8x16b = _mm_add_epi16(res_t3_8x16b, const_val16_8x16b);
            res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t3_8x16b);
            res_t1_8x16b = _mm_srai_epi16(res_t1_8x16b, 5); //shifting right by 5 bits.

            res_16x8b = _mm_packus_epi16(res_t0_8x16b, res_t1_8x16b);

            _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), res_16x8b);

            src_r0_16x8b = src_r2_16x8b;
            src_r1_16x8b = src_r3_16x8b;
            src_r2_16x8b = src_r4_16x8b;
            src_r3_16x8b = src_r5_16x8b;
            src_r4_16x8b = src_r6_16x8b;

            ht -= 2;
            pu1_src += src_strd << 1;
            pu1_dst += dst_strd << 1;
        }
        while(ht > 0);
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_inter_pred_luma_horz_hpel_vert_hpel_ssse3          */
/*                                                                           */
/*  Description   : This function implements a two stage cascaded six tap    */
/*                  filter, horizontally and then vertically on ht x wd      */
/*                  block as mentioned in sec. 8.4.2.2.1 titled "Luma sample */
/*                  interpolation process". (ht,wd) can be (4,4), (8,4),     */
/*                  (4,8), (8,8), (16,8), (8,16) or (16,16).                 */
/*                                                                           */
/*  Inputs        : puc_src  - pointer to source                             */
/*                  puc_dst  - pointer to destination                        */
/*                  src_strd - stride for source                             */
/*                  dst_strd - stride for destination                        */
/*                  ht       - height of the block                           */
/*                  wd       - width of the block                            */
/*                  pu1_tmp  - pointer to temporary buffer                   */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes                              */
/*         13 02 2015   Kaushik         Initial Version                      */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_inter_pred_luma_horz_hpel_vert_hpel_ssse3(UWORD8 *pu1_src,
                                                     UWORD8 *pu1_dst,
                                                     WORD32 src_strd,
                                                     WORD32 dst_strd,
                                                     WORD32 ht,
                                                     WORD32 wd,
                                                     UWORD8* pu1_tmp,
                                                     WORD32 dydx)
{
    UNUSED(dydx);

    if(wd == 4)
    {
        WORD16 *pi2_temp;

        pu1_tmp += 4;
        pu1_src -= src_strd << 1;
        pi2_temp = (WORD16 *)pu1_tmp;
        pu1_src -= 2; // the filter input starts from x[-2] (till x[3])

        // Horizontal 6-tap filtering
        {
            WORD32 ht_tmp = ht + 4;

            __m128i src_r0_16x8b, src_r1_16x8b;
            __m128i src_r0_sht_16x8b, src_r1_sht_16x8b;
            __m128i src_r0r1_t1_16x8b;
            __m128i res_r0r1_t1_8x16b, res_r0r1_t2_8x16b, res_r0r1_t3_8x16b;
            __m128i coeff0_1_16x8b, coeff2_3_16x8b, coeff4_5_16x8b;

            coeff0_1_16x8b = _mm_set1_epi32(0xFB01FB01);  //c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1
            coeff2_3_16x8b = _mm_set1_epi32(0x14141414);  //c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3
            coeff4_5_16x8b = _mm_set1_epi32(0x01FB01FB);  //c4 c5 c5 c5 c4 c5 c5 c5 c4 c5 c5 c5 c4 c5 c5 c5
                                                          //c0 = c5 = 1, c1 = c4 = -5, c2 = c3 = 20
            //Row0 : a0 a1 a2 a3 a4 a5 a6 a7 a8 a9.....
            //Row1 : b0 b1 b2 b3 b4 b5 b6 b7 b8 b9.....

            do
            {
                src_r0_16x8b = _mm_loadu_si128((__m128i *)pu1_src);                       //a0 a1 a2 a3 a4 a5 a6 a7 a8 a9....a15
                src_r1_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + src_strd));          //b0 b1 b2 b3 b4 b5 b6 b7 b8 b9....b15

                src_r0_sht_16x8b = _mm_srli_si128(src_r0_16x8b, 1);                       //a1 a2 a3 a4 a5 a6 a7 a8 a9....a15 0
                src_r1_sht_16x8b = _mm_srli_si128(src_r1_16x8b, 1);                       //b1 b2 b3 b4 b5 b6 b7 b8 b9....b15 0

                src_r0_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);         //a0 a1 a1 a2 a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8
                src_r1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);         //b0 b1 b1 b2 b2 b3 b3 b4 b4 b5 b5 b6 b6 b7 b7 b8

                src_r0r1_t1_16x8b = _mm_unpacklo_epi64(src_r0_16x8b, src_r1_16x8b);       //a0 a1 a1 a2 a2 a3 a3 a4 b0 b1 b1 b2 b2 b3 b3 b4
                res_r0r1_t1_8x16b = _mm_maddubs_epi16(src_r0r1_t1_16x8b, coeff0_1_16x8b); //a0*c0+a1*c1 a1*c0+a2*c1 a2*c0+a3*c1 a3*c0+a4*c1
                                                                                          //b0*c0+b1*c1 b1*c0+b2*c1 b2*c0+b3*c1 b3*c0+b4*c1

                src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 4);                           //a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8  0  0  0  0
                src_r1_16x8b = _mm_srli_si128(src_r1_16x8b, 4);                           //b2 b3 b3 b4 b4 b5 b5 b6 b6 b7 b7 b8  0  0  0  0

                src_r0r1_t1_16x8b = _mm_unpacklo_epi64(src_r0_16x8b, src_r1_16x8b);       //a2 a3 a3 a4 a4 a5 a5 a6 b2 b3 b3 b4 b4 b5 b5 b6
                res_r0r1_t2_8x16b = _mm_maddubs_epi16(src_r0r1_t1_16x8b, coeff2_3_16x8b); //a2*c2+a3*c3 a3*c2+a4*c3 a4*c2+a5*c3 a5*c2+a6*c3
                                                                                          //b2*c2+b3*c3 b3*c2+b4*c3 b4*c2+b5*c3 b5*c2+b6*c3

                src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 4);                           //a4 a5 a5 a6 a6 a7 a7 a8  0  0  0  0  0  0  0  0
                src_r1_16x8b = _mm_srli_si128(src_r1_16x8b, 4);                           //b4 b5 b5 b6 b6 b7 b7 b8  0  0  0  0  0  0  0  0

                src_r0r1_t1_16x8b = _mm_unpacklo_epi64(src_r0_16x8b, src_r1_16x8b);       //a4 a5 a5 a6 a6 a7 a7 a8 b4 b5 b5 b6 b6 b7 b7 b8
                res_r0r1_t3_8x16b = _mm_maddubs_epi16(src_r0r1_t1_16x8b, coeff4_5_16x8b); //a4*c4+a5*c5 a5*c4+a6*c5 a6*c4+a7*c5 a7*c4+a8*c5
                                                                                          //b4*c4+b5*c5 b5*c4+b6*c5 b4*c6+b7*c5 b7*c4+b8*c5
                res_r0r1_t1_8x16b = _mm_add_epi16(res_r0r1_t1_8x16b, res_r0r1_t2_8x16b);
                res_r0r1_t1_8x16b = _mm_add_epi16(res_r0r1_t3_8x16b, res_r0r1_t1_8x16b);

                _mm_storeu_si128((__m128i *)pi2_temp, res_r0r1_t1_8x16b);

                ht_tmp -= 2;
                pu1_src += src_strd << 1;
                pi2_temp += 8;
            }
            while(ht_tmp > 0);

            src_r0_16x8b = _mm_loadu_si128((__m128i *)pu1_src);                           //a0 a1 a2 a3 a4 a5 a6 a7 a8 a9....a15
            src_r0_sht_16x8b = _mm_srli_si128(src_r0_16x8b, 1);                           //a1 a2 a3 a4 a5 a6 a7 a8 a9....a15 0

            src_r0_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);             //a0 a1 a1 a2 a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8
            res_r0r1_t1_8x16b = _mm_maddubs_epi16(src_r0_16x8b, coeff0_1_16x8b);          //a0*c0+a1*c1 a1*c0+a2*c1 a2*c0+a3*c1 a3*c0+a4*c1

            src_r0_16x8b = _mm_srli_si128(src_r0_16x8b,4);                                //a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8  0  0  0  0
            res_r0r1_t2_8x16b = _mm_maddubs_epi16(src_r0_16x8b, coeff2_3_16x8b);          //a2*c2+a3*c3 a3*c2+a4*c3 a4*c2+a5*c3 a5*c2+a6*c3

            src_r0_16x8b = _mm_srli_si128(src_r0_16x8b,4);                                //a4 a5 a5 a6 a6 a7 a7 a8  0  0  0  0  0  0  0  0
            res_r0r1_t3_8x16b = _mm_maddubs_epi16(src_r0_16x8b, coeff4_5_16x8b);          //a4*c4+a5*c5 a5*c4+a6*c5 a6*c4+a7*c5 a7*c4+a8*c5

            res_r0r1_t1_8x16b = _mm_add_epi16(res_r0r1_t1_8x16b, res_r0r1_t2_8x16b);
            res_r0r1_t1_8x16b = _mm_add_epi16(res_r0r1_t3_8x16b, res_r0r1_t1_8x16b);

            _mm_storel_epi64((__m128i *)pi2_temp, res_r0r1_t1_8x16b);
        }

        pi2_temp = (WORD16 *)pu1_tmp;

        // Vertical 6-tap filtering
        {
            __m128i src_r0_8x16b, src_r1_8x16b, src_r2_8x16b, src_r3_8x16b,
                            src_r4_8x16b;
            __m128i src_r5_8x16b, src_r6_8x16b;
            __m128i src_t1_8x16b, src_t2_8x16b;

            __m128i res_t0_4x32b, res_t1_4x32b, res_t2_4x32b, res_t3_4x32b;
            __m128i res_8x16b, res_16x8b;

            __m128i coeff0_1_8x16b, coeff2_3_8x16b, coeff4_5_8x16b;
            __m128i const_val512_4x32b;

            coeff0_1_8x16b = _mm_set1_epi32(0xFFFB0001);
            coeff2_3_8x16b = _mm_set1_epi32(0x00140014);
            coeff4_5_8x16b = _mm_set1_epi32(0x0001FFFB);

            const_val512_4x32b = _mm_set1_epi32(512);

            src_r0_8x16b = _mm_loadl_epi64((__m128i *)(pi2_temp));
            src_r1_8x16b = _mm_loadl_epi64((__m128i *)(pi2_temp + 4));
            src_r2_8x16b = _mm_loadl_epi64((__m128i *)(pi2_temp + 8));
            src_r3_8x16b = _mm_loadl_epi64((__m128i *)(pi2_temp + 12));
            src_r4_8x16b = _mm_loadl_epi64((__m128i *)(pi2_temp + 16));
            pi2_temp += 20;

            do
            {
                src_r5_8x16b = _mm_loadl_epi64((__m128i *)pi2_temp);
                src_r6_8x16b = _mm_loadl_epi64((__m128i *)(pi2_temp + 4));

                src_r0_8x16b = _mm_unpacklo_epi16(src_r0_8x16b, src_r1_8x16b);
                src_t1_8x16b = _mm_unpacklo_epi16(src_r2_8x16b, src_r3_8x16b);
                src_t2_8x16b = _mm_unpacklo_epi16(src_r4_8x16b, src_r5_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_t1_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_t2_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(res_t3_4x32b, const_val512_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);

                res_t0_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                src_r1_8x16b = _mm_unpacklo_epi16(src_r1_8x16b, src_r2_8x16b);
                src_t1_8x16b = _mm_unpacklo_epi16(src_r3_8x16b, src_r4_8x16b);
                src_t2_8x16b = _mm_unpacklo_epi16(src_r5_8x16b, src_r6_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r1_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_t1_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_t2_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(res_t3_4x32b, const_val512_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);

                res_t1_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                res_8x16b = _mm_packs_epi32(res_t0_4x32b, res_t1_4x32b);
                res_16x8b = _mm_packus_epi16(res_8x16b, res_8x16b);

                *((WORD32 *)(pu1_dst)) = _mm_cvtsi128_si32(res_16x8b);
                res_16x8b = _mm_srli_si128(res_16x8b, 4);
                *((WORD32 *)(pu1_dst + dst_strd)) = _mm_cvtsi128_si32(res_16x8b);

                src_r0_8x16b = src_r2_8x16b;
                src_r1_8x16b = src_r3_8x16b;
                src_r2_8x16b = src_r4_8x16b;
                src_r3_8x16b = src_r5_8x16b;
                src_r4_8x16b = src_r6_8x16b;

                ht -= 2;
                pi2_temp += 8;
                pu1_dst += dst_strd << 1;
            }
            while(ht > 0);
        }
    }
    else if(wd == 8)
    {
        WORD16 *pi2_temp;

        pu1_tmp += 4;
        pu1_src -= src_strd << 1;
        pi2_temp = (WORD16 *)pu1_tmp;
        pu1_src -= 2; // the filter input starts from x[-2] (till x[3])

        // Horizontal 6-tap filtering
        {
            WORD32 ht_tmp = ht + 4;

            __m128i src_r0_16x8b, src_r1_16x8b;
            __m128i src_r0_sht_16x8b, src_r1_sht_16x8b;
            __m128i src_r0_t1_16x8b, src_r1_t1_16x8b;
            __m128i res_r0_t1_8x16b, res_r0_t2_8x16b, res_r0_t3_8x16b;
            __m128i res_r1_t1_8x16b, res_r1_t2_8x16b, res_r1_t3_8x16b;
            __m128i coeff0_1_16x8b, coeff2_3_16x8b, coeff4_5_16x8b;

            coeff0_1_16x8b = _mm_set1_epi32(0xFB01FB01);  //c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1
            coeff2_3_16x8b = _mm_set1_epi32(0x14141414);  //c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3
            coeff4_5_16x8b = _mm_set1_epi32(0x01FB01FB);  //c4 c5 c5 c5 c4 c5 c5 c5 c4 c5 c5 c5 c4 c5 c5 c5
                                                          //c0 = c5 = 1, c1 = c4 = -5, c2 = c3 = 20
            //Row0 : a0 a1 a2 a3 a4 a5 a6 a7 a8 a9.....
            //Row1 : b0 b1 b2 b3 b4 b5 b6 b7 b8 b9.....

            do
            {
                src_r0_16x8b = _mm_loadu_si128((__m128i *)pu1_src);                      //a0 a1 a2 a3 a4 a5 a6 a7 a8 a9 a10 a11 a12 a13 a14 a15
                src_r1_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + src_strd));         //b0 b1 b2 b3 b4 b5 b6 b7 b8 b9 b10 b11 b12 b13 b14 b15

                src_r0_sht_16x8b = _mm_srli_si128(src_r0_16x8b, 1);                      //a1 a2 a3 a4 a5 a6 a7 a8 a9....a15 0
                src_r1_sht_16x8b = _mm_srli_si128(src_r1_16x8b, 1);                      //b1 b2 b3 b4 b5 b6 b7 b8 b9....b15 0

                src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);     //a0 a1 a1 a2 a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8
                src_r1_t1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);     //b0 b1 b1 b2 b2 b3 b3 b4 b4 b5 b5 b6 b6 b7 b7 b8

                res_r0_t1_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff0_1_16x8b);    //a0*c0+a1*c1 a1*c0+a2*c1 a2*c0+a3*c1 a3*c0+a4*c1
                                                                                         //a4*c0+a5*c1 a5*c0+a6*c1 a6*c0+a7*c1 a7*c0+a8*c1
                res_r1_t1_8x16b = _mm_maddubs_epi16(src_r1_t1_16x8b, coeff0_1_16x8b);    //b0*c0+b1*c1 b1*c0+b2*c1 b2*c0+b3*c1 b3*c0+b4*c1
                                                                                         //b4*c0+b5*c1 b5*c0+b6*c1 b6*c0+b7*c1 b7*c0+b8*c1

                src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 2);                          //a2 a3 a4 a5 a6 a7 a8 a9....a15 0 0
                src_r1_16x8b = _mm_srli_si128(src_r1_16x8b, 2);                          //b2 b3 b4 b5 b6 b7 b8 b9....b15 0 0

                src_r0_sht_16x8b = _mm_srli_si128(src_r0_sht_16x8b, 2);                  //a3 a4 a5 a6 a7 a8 a9....a15 0  0  0
                src_r1_sht_16x8b = _mm_srli_si128(src_r1_sht_16x8b, 2);                  //b3 b4 b5 b6 b7 b8 b9....b15 0  0  0

                src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);     //a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8 a8 a9 a9 a10
                src_r1_t1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);     //b2 b3 b3 b4 b4 b5 b5 b6 b6 b7 b7 b8 a8 a9 a9 a10

                res_r0_t2_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff2_3_16x8b);    //a2*c2+a3*c3 a3*c2+a4*c3 a4*c2+a5*c3 a5*c2+a6*c3
                                                                                         //a6*c2+a7*c3 a7*c2+a8*c3 a8*c2+a9*c3 a9*c2+a10*c3
                res_r1_t2_8x16b = _mm_maddubs_epi16(src_r1_t1_16x8b, coeff2_3_16x8b);    //b2*c2+b3*c3 b3*c2+b4*c3 b2*c4+b5*c3 b5*c2+b6*c3
                                                                                         //b6*c2+b7*c3 b7*c2+b8*c3 b8*c2+b9*c3 b9*c2+b10*c3

                src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 2);                          //a4 a5 a6 a7 a8 a9....a15 0  0  0  0
                src_r1_16x8b = _mm_srli_si128(src_r1_16x8b, 2);                          //b4 b5 b6 b7 b8 b9....b15 0  0  0  0

                src_r0_sht_16x8b = _mm_srli_si128(src_r0_sht_16x8b, 2);                  //a5 a6 a7 a8 a9....a15 0  0  0  0  0
                src_r1_sht_16x8b = _mm_srli_si128(src_r1_sht_16x8b, 2);                  //b5 b6 b7 b8 b9....b15 0  0  0  0  0

                src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);     //a4 a5 a5 a6 a6 a7 a7 a8 a8 a9 a9 a10 a10 a11 a11 a12
                src_r1_t1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);     //b4 b5 b5 b6 b6 b7 b7 b8 b8 b9 b9 b10 b10 b11 b11 b12

                res_r0_t3_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff4_5_16x8b);    //a4*c4+a5*c5 a5*c4+a6*c5 a6*c4+a7*c5 a7*c4+a8*c5
                                                                                         //a8*c4+a9*c5 a9*c4+a10*c5 a10*c4+a11*c5 a11*c4+a12*c5
                res_r1_t3_8x16b = _mm_maddubs_epi16(src_r1_t1_16x8b, coeff4_5_16x8b);    //b4*c4+b5*c5 b5*c4+b6*c5 b6*c4+b7*c5 b7*c4+b8*c5
                                                                                         //b8*c4+b9*c5 b9*c4+b10*c5 b10*c4+b11*c5 b11*c4+b12*c5
                res_r0_t1_8x16b = _mm_add_epi16(res_r0_t1_8x16b, res_r0_t2_8x16b);
                res_r0_t1_8x16b = _mm_add_epi16(res_r0_t1_8x16b, res_r0_t3_8x16b);

                res_r1_t1_8x16b = _mm_add_epi16(res_r1_t1_8x16b, res_r1_t2_8x16b);
                res_r1_t1_8x16b = _mm_add_epi16(res_r1_t1_8x16b, res_r1_t3_8x16b);

                _mm_storeu_si128((__m128i *)pi2_temp, res_r0_t1_8x16b);
                _mm_storeu_si128((__m128i *)(pi2_temp + 8), res_r1_t1_8x16b);

                ht_tmp -= 2;
                pu1_src += src_strd << 1;
                pi2_temp += 16;
            }
            while(ht_tmp > 0);

            src_r0_16x8b = _mm_loadu_si128((__m128i *)pu1_src);                          //a0 a1 a2 a3 a4 a5 a6 a7 a8 a9 a10 a11 a12 a13 a14 a15
            src_r0_sht_16x8b = _mm_srli_si128(src_r0_16x8b, 1);                          //a1 a2 a3 a4 a5 a6 a7 a8 a9....a15 0

            src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b,src_r0_sht_16x8b);          //a0 a1 a1 a2 a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8
            res_r0_t1_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b,coeff0_1_16x8b);         //a0*c0+a1*c1 a1*c0+a2*c1 a2*c0+a3*c1 a3*c0+a4*c1
                                                                                         //a4*c0+a5*c1 a5*c0+a6*c1 a6*c0+a7*c1 a7*c0+a8*c1

            src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 2);                              //a2 a3 a4 a5 a6 a7 a8 a9....a15 0 0
            src_r0_sht_16x8b = _mm_srli_si128(src_r0_sht_16x8b, 2);                      //a3 a4 a5 a6 a7 a8 a9....a15 0  0  0

            src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);         //a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8 a8 a9 a9 a10
            res_r0_t2_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff2_3_16x8b);        //a2*c2+a3*c3 a3*c2+a4*c3 a4*c2+a5*c3 a5*c2+a6*c3
                                                                                         //a6*c2+a7*c3 a7*c2+a8*c3 a8*c2+a9*c3 a9*c2+a10*c3

            src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 2);                              //a4 a5 a6 a7 a8 a9....a15 0  0  0  0
            src_r0_sht_16x8b = _mm_srli_si128(src_r0_sht_16x8b, 2);                      //a5 a6 a7 a8 a9....a15 0  0  0  0  0

            src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);         //a4 a5 a5 a6 a6 a7 a7 a8 a8 a9 a9 a10 a10 a11 a11 a12
            res_r0_t3_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff4_5_16x8b);        //a4*c4+a5*c5 a5*c4+a6*c5 a6*c4+a7*c5 a7*c4+a8*c5
                                                                                         //a8*c4+a9*c5 a9*c4+a10*c5 a10*c4+a11*c5 a11*c4+a12*c5
            res_r0_t1_8x16b = _mm_add_epi16(res_r0_t1_8x16b, res_r0_t2_8x16b);
            res_r0_t1_8x16b = _mm_add_epi16(res_r0_t1_8x16b, res_r0_t3_8x16b);

            _mm_storeu_si128((__m128i *)pi2_temp, res_r0_t1_8x16b);
        }

        pi2_temp = (WORD16 *)pu1_tmp;

        // Vertical 6-tap filtering
        {
            __m128i src_r0_8x16b, src_r1_8x16b, src_r2_8x16b, src_r3_8x16b,
                            src_r4_8x16b;
            __m128i src_r5_8x16b, src_r6_8x16b;
            __m128i src_r0r1_8x16b, src_r2r3_8x16b, src_r4r5_8x16b;

            __m128i res_t1_4x32b, res_t2_4x32b, res_t3_4x32b;
            __m128i res_c0_4x32b, res_c1_4x32b;
            __m128i res_8x16b, res_16x8b;

            __m128i coeff0_1_8x16b, coeff2_3_8x16b, coeff4_5_8x16b;
            __m128i const_val512_4x32b;

            coeff0_1_8x16b = _mm_set1_epi32(0xFFFB0001);
            coeff2_3_8x16b = _mm_set1_epi32(0x00140014);
            coeff4_5_8x16b = _mm_set1_epi32(0x0001FFFB);

            const_val512_4x32b = _mm_set1_epi32(512);

            src_r0_8x16b = _mm_loadu_si128((__m128i *)pi2_temp);
            src_r1_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp + 8));
            src_r2_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp + 16));
            src_r3_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp + 24));
            src_r4_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp + 32));
            pi2_temp += 40;

            do
            {
                src_r5_8x16b = _mm_loadu_si128((__m128i *)pi2_temp);
                src_r6_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp + 8));

                src_r0r1_8x16b = _mm_unpacklo_epi16(src_r0_8x16b, src_r1_8x16b);
                src_r2r3_8x16b = _mm_unpacklo_epi16(src_r2_8x16b, src_r3_8x16b);
                src_r4r5_8x16b = _mm_unpacklo_epi16(src_r4_8x16b, src_r5_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(res_t3_4x32b, const_val512_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);
                res_c0_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                src_r0r1_8x16b = _mm_unpackhi_epi16(src_r0_8x16b, src_r1_8x16b);
                src_r2r3_8x16b = _mm_unpackhi_epi16(src_r2_8x16b, src_r3_8x16b);
                src_r4r5_8x16b = _mm_unpackhi_epi16(src_r4_8x16b, src_r5_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(res_t3_4x32b, const_val512_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);
                res_c1_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                res_8x16b = _mm_packs_epi32(res_c0_4x32b, res_c1_4x32b);
                res_16x8b = _mm_packus_epi16(res_8x16b, res_8x16b);

                _mm_storel_epi64((__m128i *)pu1_dst, res_16x8b);

                src_r0r1_8x16b = _mm_unpacklo_epi16(src_r1_8x16b, src_r2_8x16b);
                src_r2r3_8x16b = _mm_unpacklo_epi16(src_r3_8x16b, src_r4_8x16b);
                src_r4r5_8x16b = _mm_unpacklo_epi16(src_r5_8x16b, src_r6_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(res_t3_4x32b, const_val512_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);
                res_c0_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                src_r0r1_8x16b = _mm_unpackhi_epi16(src_r1_8x16b, src_r2_8x16b);
                src_r2r3_8x16b = _mm_unpackhi_epi16(src_r3_8x16b, src_r4_8x16b);
                src_r4r5_8x16b = _mm_unpackhi_epi16(src_r5_8x16b, src_r6_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(res_t3_4x32b, const_val512_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);
                res_c1_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                res_8x16b = _mm_packs_epi32(res_c0_4x32b, res_c1_4x32b);
                res_16x8b = _mm_packus_epi16(res_8x16b, res_8x16b);

                _mm_storel_epi64((__m128i *)(pu1_dst + dst_strd), res_16x8b);

                src_r0_8x16b = src_r2_8x16b;
                src_r1_8x16b = src_r3_8x16b;
                src_r2_8x16b = src_r4_8x16b;
                src_r3_8x16b = src_r5_8x16b;
                src_r4_8x16b = src_r6_8x16b;

                ht -= 2;
                pi2_temp += 16;
                pu1_dst += dst_strd << 1;
            }
            while(ht > 0);
        }
    }
    else // wd == 16
    {
        WORD16 *pi2_temp;
        WORD32 ht_tmp;

        pu1_tmp += 4;
        pu1_src -= src_strd << 1;
        pi2_temp = (WORD16 *)pu1_tmp;
        pu1_src -= 2; // the filter input starts from x[-2] (till x[3])

        // Horizontal 6-tap filtering
        {
            __m128i src_r0_16x8b, src_r1_16x8b, src_r0_sht_16x8b, src_r1_sht_16x8b;
            __m128i src_r0_t1_16x8b, src_r1_t1_16x8b;

            __m128i res_r0_t1_8x16b, res_r0_t2_8x16b, res_r0_t3_8x16b;
            __m128i res_r1_t1_8x16b, res_r1_t2_8x16b, res_r1_t3_8x16b;

            __m128i coeff0_1_16x8b, coeff2_3_16x8b, coeff4_5_16x8b;

            ht_tmp = ht + 5;

            coeff0_1_16x8b = _mm_set1_epi32(0xFB01FB01);  //c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1
            coeff2_3_16x8b = _mm_set1_epi32(0x14141414);  //c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3
            coeff4_5_16x8b = _mm_set1_epi32(0x01FB01FB);  //c4 c5 c5 c5 c4 c5 c5 c5 c4 c5 c5 c5 c4 c5 c5 c5
                                                          //c0 = c5 = 1, c1 = c4 = -5, c2 = c3 = 20
            //Row0 : a0 a1 a2 a3 a4 a5 a6 a7 a8 a9.....
            //Row0 :                         b0 b1 b2 b3 b4 b5 b6 b7 b8 b9.....
            //b0 is same a8. Similarly other bn pixels are same as a(n+8) pixels.

            do
            {
                src_r0_16x8b = _mm_loadu_si128((__m128i *)pu1_src);                      //a0 a1 a2 a3 a4 a5 a6 a7 a8 a9....a15
                src_r1_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + 8));                //b0 b1 b2 b3 b4 b5 b6 b7 b8 b9....b15

                src_r0_sht_16x8b = _mm_srli_si128(src_r0_16x8b, 1);                      //a1 a2 a3 a4 a5 a6 a7 a8 a9....a15 0
                src_r1_sht_16x8b = _mm_srli_si128(src_r1_16x8b, 1);                      //b1 b2 b3 b4 b5 b6 b7 b8 b9....b15 0

                src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);     //a0 a1 a1 a2 a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8
                src_r1_t1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);     //b0 b1 b1 b2 b2 b3 b3 b4 b4 b5 b5 b6 b6 b7 b7 b8

                res_r0_t1_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff0_1_16x8b);    //a0*c0+a1*c1 a1*c0+a2*c1 a2*c0+a3*c1 a3*c0+a4*c1
                                                                                         //a4*c0+a5*c1 a5*c0+a6*c1 a6*c0+a7*c1 a7*c0+a8*c1
                res_r1_t1_8x16b = _mm_maddubs_epi16(src_r1_t1_16x8b, coeff0_1_16x8b);    //b0*c0+b1*c1 b1*c0+b2*c1 b2*c0+b3*c1 b3*c0+b4*c1
                                                                                         //b4*c0+b5*c1 b5*c0+b6*c1 b6*c0+b7*c1 b7*c0+b8*c1

                src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 2);                          //a2 a3 a4 a5 a6 a7 a8 a9....a15 0 0
                src_r1_16x8b = _mm_srli_si128(src_r1_16x8b, 2);                          //b2 b3 b4 b5 b6 b7 b8 b9....b15 0 0

                src_r0_sht_16x8b = _mm_srli_si128(src_r0_sht_16x8b, 2);                  //a3 a4 a5 a6 a7 a8 a9....a15 0  0  0
                src_r1_sht_16x8b = _mm_srli_si128(src_r1_sht_16x8b, 2);                  //b3 b4 b5 b6 b7 b8 b9....b15 0  0  0

                src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);     //a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8 a8 a9 a9 a10
                src_r1_t1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);     //b2 b3 b3 b4 b4 b5 b5 b6 b6 b7 b7 b8 a8 a9 a9 a10

                res_r0_t2_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff2_3_16x8b);    //a2*c2+a3*c3 a3*c2+a4*c3 a4*c2+a5*c3 a5*c2+a6*c3
                                                                                         //a6*c2+a7*c3 a7*c2+a8*c3 a8*c2+a9*c3 a9*c2+a10*c3
                res_r1_t2_8x16b = _mm_maddubs_epi16(src_r1_t1_16x8b, coeff2_3_16x8b);    //b2*c2+b3*c3 b3*c2+b4*c3 b2*c4+b5*c3 b5*c2+b6*c3
                                                                                         //b6*c2+b7*c3 b7*c2+b8*c3 b8*c2+b9*c3 b9*c2+b10*c3

                src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 2);                          //a4 a5 a6 a7 a8 a9....a15 0  0  0  0
                src_r1_16x8b = _mm_srli_si128(src_r1_16x8b, 2);                          //b4 b5 b6 b7 b8 b9....b15 0  0  0  0

                src_r0_sht_16x8b = _mm_srli_si128(src_r0_sht_16x8b, 2);                  //a5 a6 a7 a8 a9....a15 0  0  0  0  0
                src_r1_sht_16x8b = _mm_srli_si128(src_r1_sht_16x8b, 2);                  //b5 b6 b7 b8 b9....b15 0  0  0  0  0

                src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);     //a4 a5 a5 a6 a6 a7 a7 a8 a8 a9 a9 a10 a10 a11 a11 a12
                src_r1_t1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);     //b4 b5 b5 b6 b6 b7 b7 b8 b8 b9 b9 b10 b10 b11 b11 b12

                res_r0_t3_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff4_5_16x8b);    //a4*c4+a5*c5 a5*c4+a6*c5 a6*c4+a7*c5 a7*c4+a8*c5
                                                                                         //a8*c4+a9*c5 a9*c4+a10*c5 a10*c4+a11*c5 a11*c4+a12*c5
                res_r1_t3_8x16b = _mm_maddubs_epi16(src_r1_t1_16x8b, coeff4_5_16x8b);    //b4*c4+b5*c5 b5*c4+b6*c5 b6*c4+b7*c5 b7*c4+b8*c5
                                                                                         //b8*c4+b9*c5 b9*c4+b10*c5 b10*c4+b11*c5 b11*c4+b12*c5
                res_r0_t1_8x16b = _mm_add_epi16(res_r0_t1_8x16b, res_r0_t2_8x16b);
                res_r0_t1_8x16b = _mm_add_epi16(res_r0_t1_8x16b, res_r0_t3_8x16b);

                res_r1_t1_8x16b = _mm_add_epi16(res_r1_t1_8x16b, res_r1_t2_8x16b);
                res_r1_t1_8x16b = _mm_add_epi16(res_r1_t1_8x16b, res_r1_t3_8x16b);

                _mm_storeu_si128((__m128i *)pi2_temp, res_r0_t1_8x16b);
                _mm_storeu_si128((__m128i *)(pi2_temp + 8), res_r1_t1_8x16b);

                ht_tmp--;
                pu1_src += src_strd;
                pi2_temp += 16;
            }
            while(ht_tmp > 0);
        }

        pi2_temp = (WORD16 *)pu1_tmp;

        // Vertical 6-tap filtering
        {
            WORD16 *pi2_temp2;
            UWORD8 *pu1_dst2;
            WORD32 ht_tmp;

            __m128i src_r0_8x16b, src_r1_8x16b, src_r2_8x16b, src_r3_8x16b, src_r4_8x16b;
            __m128i src_r5_8x16b, src_r6_8x16b;
            __m128i src_r0r1_8x16b, src_r2r3_8x16b, src_r4r5_8x16b;

            __m128i res_t1_4x32b, res_t2_4x32b, res_t3_4x32b;
            __m128i res_c0_4x32b, res_c1_4x32b;
            __m128i res_8x16b, res_16x8b;

            __m128i coeff0_1_8x16b, coeff2_3_8x16b, coeff4_5_8x16b;
            __m128i const_val512_4x32b;

            coeff0_1_8x16b = _mm_set1_epi32(0xFFFB0001);
            coeff2_3_8x16b = _mm_set1_epi32(0x00140014);
            coeff4_5_8x16b = _mm_set1_epi32(0x0001FFFB);

            const_val512_4x32b = _mm_set1_epi32(512);

            pi2_temp2 = pi2_temp + 8;
            pu1_dst2 = pu1_dst + 8;
            ht_tmp = ht;

            /**********************************************************/
            /*     Do first height x 8 block                          */
            /**********************************************************/
            src_r0_8x16b = _mm_loadu_si128((__m128i *)pi2_temp);
            src_r1_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp + 16));
            src_r2_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp + 32));
            src_r3_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp + 48));
            src_r4_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp + 64));
            pi2_temp += 80;

            do
            {
                src_r5_8x16b = _mm_loadu_si128((__m128i *)pi2_temp);
                src_r6_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp + 16));

                src_r0r1_8x16b = _mm_unpacklo_epi16(src_r0_8x16b, src_r1_8x16b);
                src_r2r3_8x16b = _mm_unpacklo_epi16(src_r2_8x16b, src_r3_8x16b);
                src_r4r5_8x16b = _mm_unpacklo_epi16(src_r4_8x16b, src_r5_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(res_t3_4x32b, const_val512_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);
                res_c0_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                src_r0r1_8x16b = _mm_unpackhi_epi16(src_r0_8x16b, src_r1_8x16b);
                src_r2r3_8x16b = _mm_unpackhi_epi16(src_r2_8x16b, src_r3_8x16b);
                src_r4r5_8x16b = _mm_unpackhi_epi16(src_r4_8x16b, src_r5_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(res_t3_4x32b, const_val512_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);
                res_c1_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                res_8x16b = _mm_packs_epi32(res_c0_4x32b, res_c1_4x32b);
                res_16x8b = _mm_packus_epi16(res_8x16b, res_8x16b);

                _mm_storel_epi64((__m128i *)pu1_dst, res_16x8b);

                src_r0r1_8x16b = _mm_unpacklo_epi16(src_r1_8x16b, src_r2_8x16b);
                src_r2r3_8x16b = _mm_unpacklo_epi16(src_r3_8x16b, src_r4_8x16b);
                src_r4r5_8x16b = _mm_unpacklo_epi16(src_r5_8x16b, src_r6_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(res_t3_4x32b, const_val512_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);
                res_c0_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                src_r0r1_8x16b = _mm_unpackhi_epi16(src_r1_8x16b, src_r2_8x16b);
                src_r2r3_8x16b = _mm_unpackhi_epi16(src_r3_8x16b, src_r4_8x16b);
                src_r4r5_8x16b = _mm_unpackhi_epi16(src_r5_8x16b, src_r6_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(res_t3_4x32b, const_val512_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);
                res_c1_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                res_8x16b = _mm_packs_epi32(res_c0_4x32b, res_c1_4x32b);
                res_16x8b = _mm_packus_epi16(res_8x16b, res_8x16b);

                _mm_storel_epi64((__m128i *)(pu1_dst + dst_strd), res_16x8b);

                src_r0_8x16b = src_r2_8x16b;
                src_r1_8x16b = src_r3_8x16b;
                src_r2_8x16b = src_r4_8x16b;
                src_r3_8x16b = src_r5_8x16b;
                src_r4_8x16b = src_r6_8x16b;

                ht_tmp -= 2;
                pi2_temp += 32;
                pu1_dst += dst_strd << 1;
            }
            while(ht_tmp > 0);

            /**********************************************************/
            /*     Do second ht x 8 block                          */
            /**********************************************************/
            src_r0_8x16b = _mm_loadu_si128((__m128i *)pi2_temp2);
            src_r1_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 16));
            src_r2_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 32));
            src_r3_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 48));
            src_r4_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 64));
            pi2_temp2 += 80;

            do
            {
                src_r5_8x16b = _mm_loadu_si128((__m128i *)pi2_temp2);
                src_r6_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 16));

                src_r0r1_8x16b = _mm_unpacklo_epi16(src_r0_8x16b, src_r1_8x16b);
                src_r2r3_8x16b = _mm_unpacklo_epi16(src_r2_8x16b, src_r3_8x16b);
                src_r4r5_8x16b = _mm_unpacklo_epi16(src_r4_8x16b, src_r5_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(res_t3_4x32b, const_val512_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);
                res_c0_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                src_r0r1_8x16b = _mm_unpackhi_epi16(src_r0_8x16b, src_r1_8x16b);
                src_r2r3_8x16b = _mm_unpackhi_epi16(src_r2_8x16b, src_r3_8x16b);
                src_r4r5_8x16b = _mm_unpackhi_epi16(src_r4_8x16b, src_r5_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(res_t3_4x32b, const_val512_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);
                res_c1_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                res_8x16b = _mm_packs_epi32(res_c0_4x32b, res_c1_4x32b);
                res_16x8b = _mm_packus_epi16(res_8x16b, res_8x16b);

                _mm_storel_epi64((__m128i *)pu1_dst2, res_16x8b);

                src_r0r1_8x16b = _mm_unpacklo_epi16(src_r1_8x16b, src_r2_8x16b);
                src_r2r3_8x16b = _mm_unpacklo_epi16(src_r3_8x16b, src_r4_8x16b);
                src_r4r5_8x16b = _mm_unpacklo_epi16(src_r5_8x16b, src_r6_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(res_t3_4x32b, const_val512_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);
                res_c0_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                src_r0r1_8x16b = _mm_unpackhi_epi16(src_r1_8x16b, src_r2_8x16b);
                src_r2r3_8x16b = _mm_unpackhi_epi16(src_r3_8x16b, src_r4_8x16b);
                src_r4r5_8x16b = _mm_unpackhi_epi16(src_r5_8x16b, src_r6_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(res_t3_4x32b, const_val512_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);
                res_c1_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                res_8x16b = _mm_packs_epi32(res_c0_4x32b, res_c1_4x32b);
                res_16x8b = _mm_packus_epi16(res_8x16b, res_8x16b);

                _mm_storel_epi64((__m128i *)(pu1_dst2 + dst_strd), res_16x8b);

                src_r0_8x16b = src_r2_8x16b;
                src_r1_8x16b = src_r3_8x16b;
                src_r2_8x16b = src_r4_8x16b;
                src_r3_8x16b = src_r5_8x16b;
                src_r4_8x16b = src_r6_8x16b;

                ht -= 2;
                pi2_temp2 += 32;
                pu1_dst2 += dst_strd << 1;
            }
            while(ht > 0);
        }
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_inter_pred_luma_horz_qpel_ssse3                    */
/*                                                                           */
/*  Description   : This function implements a six-tap filter horizontally   */
/*                  on ht x wd block and averages the values with the source */
/*                  pixels to calculate horizontal quarter-pel as mentioned  */
/*                  in sec. 8.4.2.2.1 titled "Luma sample interpolation      */
/*                  process". (ht,wd) can be (4,4), (8,4), (4,8), (8,8),     */
/*                  (16,8), (8,16) or (16,16).                               */
/*                                                                           */
/*  Inputs        : puc_src  - pointer to source                             */
/*                  puc_dst  - pointer to destination                        */
/*                  src_strd - stride for source                             */
/*                  dst_strd - stride for destination                        */
/*                  ht       - height of the block                           */
/*                  wd       - width of the block                            */
/*                  pu1_tmp  - pointer to temporary buffer                   */
/*                  dydx     - x and y reference offset for q-pel            */
/*                             calculations                                  */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes                              */
/*         13 02 2015   Kaushik         Initial Version                      */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_inter_pred_luma_horz_qpel_ssse3(UWORD8 *pu1_src,
                                           UWORD8 *pu1_dst,
                                           WORD32 src_strd,
                                           WORD32 dst_strd,
                                           WORD32 ht,
                                           WORD32 wd,
                                           UWORD8* pu1_tmp,
                                           WORD32 dydx)
{
    WORD32 x_offset;
    UWORD8 *pu1_pred1;

    __m128i src_r0_16x8b, src_r1_16x8b;
    __m128i src_r0_sht_16x8b, src_r1_sht_16x8b;
    __m128i coeff0_1_16x8b, coeff2_3_16x8b, coeff4_5_16x8b;
    __m128i const_val16_8x16b;

    UNUSED(pu1_tmp);

    x_offset = dydx & 3;

    coeff0_1_16x8b = _mm_set1_epi32(0xFB01FB01); //c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1
    coeff2_3_16x8b = _mm_set1_epi32(0x14141414); //c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3
    coeff4_5_16x8b = _mm_set1_epi32(0x01FB01FB); //c4 c5 c4 c5 c4 c5 c4 c5 c4 c5 c4 c5 c4 c5 c4 c5
                                                 //c0 = c5 = 1, c1 = c4 = -5, c2 = c3 = 20
    pu1_pred1 = pu1_src + (x_offset >> 1);

    const_val16_8x16b = _mm_set1_epi16(16);

    pu1_src -= 2; // the filter input starts from x[-2] (till x[3])

    if(wd == 4)
    {
        __m128i src_r0r1_16x8b;

        __m128i res_r0r1_t1_8x16b, res_r0r1_t2_8x16b, res_r0r1_t3_8x16b;
        __m128i res_r0r1_16x8b;

        //Row0 : a0 a1 a2 a3 a4 a5 a6 a7 a8 a9.....
        //Row1 : b0 b1 b2 b3 b4 b5 b6 b7 b8 b9.....

        do
        {
            src_r0_16x8b = _mm_loadu_si128((__m128i *)pu1_src);                         //a0 a1 a2 a3 a4 a5 a6 a7 a8 a9....a15
            src_r1_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + src_strd));            //b0 b1 b2 b3 b4 b5 b6 b7 b8 b9....b15

            src_r0_sht_16x8b = _mm_srli_si128(src_r0_16x8b, 1);                         //a1 a2 a3 a4 a5 a6 a7 a8 a9....a15 0
            src_r1_sht_16x8b = _mm_srli_si128(src_r1_16x8b, 1);                         //b1 b2 b3 b4 b5 b6 b7 b8 b9....b15 0

            src_r0_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);           //a0 a1 a1 a2 a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8
            src_r1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);           //b0 b1 b1 b2 b2 b3 b3 b4 b4 b5 b5 b6 b6 b7 b7 b8

            src_r0r1_16x8b = _mm_unpacklo_epi64(src_r0_16x8b, src_r1_16x8b);            //a0 a1 a1 a2 a2 a3 a3 a4 b0 b1 b1 b2 b2 b3 b3 b4
            res_r0r1_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);      //a0*c0+a1*c1 a1*c0+a2*c1 a2*c0+a3*c1 a3*c0+a4*c1
                                                                                        //b0*c0+b1*c1 b1*c0+b2*c1 b2*c0+b3*c1 b3*c0+b4*c1

            src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 4);                             //a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8  0  0  0  0
            src_r1_16x8b = _mm_srli_si128(src_r1_16x8b, 4);                             //b2 b3 b3 b4 b4 b5 b5 b6 b6 b7 b7 b8  0  0  0  0

            src_r0r1_16x8b = _mm_unpacklo_epi64(src_r0_16x8b, src_r1_16x8b);            //a2 a3 a3 a4 a4 a5 a5 a6 b2 b3 b3 b4 b4 b5 b5 b6
            res_r0r1_t2_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff2_3_16x8b);      //a2*c2+a3*c3 a3*c2+a4*c3 a4*c2+a5*c3 a5*c2+a6*c3
                                                                                        //b2*c2+b3*c3 b3*c2+b4*c3 b4*c2+b5*c3 b5*c2+b6*c3

            src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 4);                             //a4 a5 a5 a6 a6 a7 a7 a8  0  0  0  0  0  0  0  0
            src_r1_16x8b = _mm_srli_si128(src_r1_16x8b, 4);                             //b4 b5 b5 b6 b6 b7 b7 b8  0  0  0  0  0  0  0  0

            src_r0r1_16x8b = _mm_unpacklo_epi64(src_r0_16x8b, src_r1_16x8b);            //a4 a5 a5 a6 a6 a7 a7 a8 b4 b5 b5 b6 b6 b7 b7 b8
            res_r0r1_t3_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff4_5_16x8b);      //a4*c4+a5*c5 a5*c4+a6*c5 a6*c4+a7*c5 a7*c4+a8*c5
                                                                                        //b4*c4+b5*c5 b5*c4+b6*c5 b4*c6+b7*c5 b7*c4+b8*c5
            src_r0_16x8b = _mm_loadl_epi64((__m128i *)pu1_pred1);
            src_r1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_pred1 + src_strd));

            res_r0r1_t1_8x16b = _mm_add_epi16(res_r0r1_t1_8x16b, res_r0r1_t2_8x16b);
            res_r0r1_t3_8x16b = _mm_add_epi16(const_val16_8x16b, res_r0r1_t3_8x16b);
            res_r0r1_t1_8x16b = _mm_add_epi16(res_r0r1_t1_8x16b, res_r0r1_t3_8x16b);    //a0*c0+a1*c1+a2*c2+a3*c3+a4*a4+a5*c5 + 16;
                                                                                        //a1*c0+a2*c1+a2*c2+a3*c3+a5*a4+a6*c5 + 16;
                                                                                        //a2*c0+a3*c1+a4*c2+a5*c3+a6*a4+a7*c5 + 16;
                                                                                        //a3*c0+a4*c1+a5*c2+a6*c3+a6*a4+a8*c5 + 16;
                                                                                        //b0*c0+b1*c1+b2*c2+b3*c3+b4*b4+b5*c5 + 16;
                                                                                        //b1*c0+b2*c1+b2*c2+b3*c3+b5*b4+b6*c5 + 16;
                                                                                        //b2*c0+b3*c1+b4*c2+b5*c3+b6*b4+b7*c5 + 16;
                                                                                        //b3*c0+b4*c1+b5*c2+b6*c3+b6*b4+b8*c5 + 16;
            src_r0r1_16x8b = _mm_unpacklo_epi32(src_r0_16x8b,src_r1_16x8b);

            res_r0r1_t1_8x16b = _mm_srai_epi16(res_r0r1_t1_8x16b, 5);                   //shifting right by 5 bits.

            res_r0r1_16x8b = _mm_packus_epi16(res_r0r1_t1_8x16b, res_r0r1_t1_8x16b);
            res_r0r1_16x8b = _mm_avg_epu8(src_r0r1_16x8b, res_r0r1_16x8b);              //computing q-pel

            *((WORD32 *)(pu1_dst)) = _mm_cvtsi128_si32(res_r0r1_16x8b);
            res_r0r1_16x8b = _mm_srli_si128(res_r0r1_16x8b, 4);
            *((WORD32 *)(pu1_dst + dst_strd)) = _mm_cvtsi128_si32(res_r0r1_16x8b);

            ht -= 2;
            pu1_src += src_strd << 1;
            pu1_pred1 += src_strd << 1;
            pu1_dst += dst_strd << 1;
        }
        while(ht > 0);
    }
    else if(wd == 8)
    {
        __m128i src_r0_t1_16x8b, src_r1_t1_16x8b;

        __m128i res_r0_t1_8x16b, res_r0_t2_8x16b, res_r0_t3_8x16b;
        __m128i res_r1_t1_8x16b, res_r1_t2_8x16b, res_r1_t3_8x16b;
        __m128i res_r0_16x8b, res_r1_16x8b;

        //Row0 : a0 a1 a2 a3 a4 a5 a6 a7 a8 a9.....
        //Row1 : b0 b1 b2 b3 b4 b5 b6 b7 b8 b9.....

        do
        {
            src_r0_16x8b = _mm_loadu_si128((__m128i *)pu1_src);                      //a0 a1 a2 a3 a4 a5 a6 a7 a8 a9....a15
            src_r1_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + src_strd));         //b0 b1 b2 b3 b4 b5 b6 b7 b8 b9....b15

            src_r0_sht_16x8b = _mm_srli_si128(src_r0_16x8b, 1);                      //a1 a2 a3 a4 a5 a6 a7 a8 a9....a15 0
            src_r1_sht_16x8b = _mm_srli_si128(src_r1_16x8b, 1);                      //b1 b2 b3 b4 b5 b6 b7 b8 b9....b15 0

            src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);     //a0 a1 a1 a2 a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8
            src_r1_t1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);     //b0 b1 b1 b2 b2 b3 b3 b4 b4 b5 b5 b6 b6 b7 b7 b8

            res_r0_t1_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff0_1_16x8b);    //a0*c0+a1*c1 a1*c0+a2*c1 a2*c0+a3*c1 a3*c0+a4*c1
                                                                                     //a4*c0+a5*c1 a5*c0+a6*c1 a6*c0+a7*c1 a7*c0+a8*c1
            res_r1_t1_8x16b = _mm_maddubs_epi16(src_r1_t1_16x8b, coeff0_1_16x8b);    //b0*c0+b1*c1 b1*c0+b2*c1 b2*c0+b3*c1 b3*c0+b4*c1
                                                                                     //b4*c0+b5*c1 b5*c0+b6*c1 b6*c0+b7*c1 b7*c0+b8*c1

            src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 2);                          //a2 a3 a4 a5 a6 a7 a8 a9....a15 0 0
            src_r1_16x8b = _mm_srli_si128(src_r1_16x8b, 2);                          //b2 b3 b4 b5 b6 b7 b8 b9....b15 0 0

            src_r0_sht_16x8b = _mm_srli_si128(src_r0_sht_16x8b, 2);                  //a3 a4 a5 a6 a7 a8 a9....a15 0  0  0
            src_r1_sht_16x8b = _mm_srli_si128(src_r1_sht_16x8b, 2);                  //b3 b4 b5 b6 b7 b8 b9....b15 0  0  0

            src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);     //a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8 a8 a9 a9 a10
            src_r1_t1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);     //b2 b3 b3 b4 b4 b5 b5 b6 b6 b7 b7 b8 a8 a9 a9 a10

            res_r0_t2_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff2_3_16x8b);    //a2*c2+a3*c3 a3*c2+a4*c3 a4*c2+a5*c3 a5*c2+a6*c3
                                                                                     //a6*c2+a7*c3 a7*c2+a8*c3 a8*c2+a9*c3 a9*c2+a10*c3
            res_r1_t2_8x16b = _mm_maddubs_epi16(src_r1_t1_16x8b, coeff2_3_16x8b);    //b2*c2+b3*c3 b3*c2+b4*c3 b2*c4+b5*c3 b5*c2+b6*c3
                                                                                     //b6*c2+b7*c3 b7*c2+b8*c3 b8*c2+b9*c3 b9*c2+b10*c3

            src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 2);                          //a4 a5 a6 a7 a8 a9....a15 0  0  0  0
            src_r1_16x8b = _mm_srli_si128(src_r1_16x8b, 2);                          //b4 b5 b6 b7 b8 b9....b15 0  0  0  0

            src_r0_sht_16x8b = _mm_srli_si128(src_r0_sht_16x8b, 2);                  //a5 a6 a7 a8 a9....a15 0  0  0  0  0
            src_r1_sht_16x8b = _mm_srli_si128(src_r1_sht_16x8b, 2);                  //b5 b6 b7 b8 b9....b15 0  0  0  0  0

            src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);     //a4 a5 a5 a6 a6 a7 a7 a8 a8 a9 a9 a10 a10 a11 a11 a12
            src_r1_t1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);     //b4 b5 b5 b6 b6 b7 b7 b8 b8 b9 b9 b10 b10 b11 b11 b12

            res_r0_t3_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff4_5_16x8b);    //a4*c4+a5*c5 a5*c4+a6*c5 a6*c4+a7*c5 a7*c4+a8*c5
                                                                                     //a8*c4+a9*c5 a9*c4+a10*c5 a10*c4+a11*c5 a11*c4+a12*c5
            res_r1_t3_8x16b = _mm_maddubs_epi16(src_r1_t1_16x8b, coeff4_5_16x8b);    //b4*c4+b5*c5 b5*c4+b6*c5 b6*c4+b7*c5 b7*c4+b8*c5
                                                                                     //b8*c4+b9*c5 b9*c4+b10*c5 b10*c4+b11*c5 b11*c4+b12*c5
            src_r0_16x8b = _mm_loadl_epi64((__m128i *)pu1_pred1);
            src_r1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_pred1 + src_strd));

            res_r0_t1_8x16b = _mm_add_epi16(res_r0_t1_8x16b, res_r0_t2_8x16b);
            res_r1_t1_8x16b = _mm_add_epi16(res_r1_t1_8x16b, res_r1_t2_8x16b);
            res_r0_t3_8x16b = _mm_add_epi16(const_val16_8x16b, res_r0_t3_8x16b);
            res_r1_t3_8x16b = _mm_add_epi16(const_val16_8x16b, res_r1_t3_8x16b);
            res_r0_t1_8x16b = _mm_add_epi16(res_r0_t1_8x16b, res_r0_t3_8x16b);
            res_r1_t1_8x16b = _mm_add_epi16(res_r1_t1_8x16b, res_r1_t3_8x16b);

            res_r0_t1_8x16b = _mm_srai_epi16(res_r0_t1_8x16b, 5);
            res_r1_t1_8x16b = _mm_srai_epi16(res_r1_t1_8x16b, 5);                    //shifting right by 5 bits.

            res_r0_16x8b = _mm_packus_epi16(res_r0_t1_8x16b, res_r0_t1_8x16b);
            res_r1_16x8b = _mm_packus_epi16(res_r1_t1_8x16b, res_r1_t1_8x16b);

            res_r0_16x8b = _mm_avg_epu8(src_r0_16x8b, res_r0_16x8b);
            res_r1_16x8b = _mm_avg_epu8(src_r1_16x8b, res_r1_16x8b);                 //computing q-pel

            _mm_storel_epi64((__m128i *)pu1_dst, res_r0_16x8b);
            _mm_storel_epi64((__m128i *)(pu1_dst + dst_strd), res_r1_16x8b);

            ht -= 2;
            pu1_src += src_strd << 1;
            pu1_pred1 += src_strd << 1;
            pu1_dst += dst_strd << 1;
        }
        while(ht > 0);
    }
    else // wd == 16
    {
        __m128i src_r0_t1_16x8b, src_r1_t1_16x8b;

        __m128i res_r0_t1_8x16b, res_r0_t2_8x16b, res_r0_t3_8x16b;
        __m128i res_r1_t1_8x16b, res_r1_t2_8x16b, res_r1_t3_8x16b;
        __m128i res_16x8b;

        //Row0 : a0 a1 a2 a3 a4 a5 a6 a7 a8 a9.....
        //Row0 :                         b0 b1 b2 b3 b4 b5 b6 b7 b8 b9.....
        //b0 is same a8. Similarly other bn pixels are same as a(n+8) pixels.

        do
        {
            src_r0_16x8b = _mm_loadu_si128((__m128i *)pu1_src);                      //a0 a1 a2 a3 a4 a5 a6 a7 a8 a9....a15
            src_r1_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + 8));                //b0 b1 b2 b3 b4 b5 b6 b7 b8 b9....b15

            src_r0_sht_16x8b = _mm_srli_si128(src_r0_16x8b, 1);                      //a1 a2 a3 a4 a5 a6 a7 a8 a9....a15 0
            src_r1_sht_16x8b = _mm_srli_si128(src_r1_16x8b, 1);                      //b1 b2 b3 b4 b5 b6 b7 b8 b9....b15 0

            src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);     //a0 a1 a1 a2 a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8
            src_r1_t1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);     //b0 b1 b1 b2 b2 b3 b3 b4 b4 b5 b5 b6 b6 b7 b7 b8

            res_r0_t1_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff0_1_16x8b);    //a0*c0+a1*c1 a1*c0+a2*c1 a2*c0+a3*c1 a3*c0+a4*c1
                                                                                     //a4*c0+a5*c1 a5*c0+a6*c1 a6*c0+a7*c1 a7*c0+a8*c1
            res_r1_t1_8x16b = _mm_maddubs_epi16(src_r1_t1_16x8b, coeff0_1_16x8b);    //b0*c0+b1*c1 b1*c0+b2*c1 b2*c0+b3*c1 b3*c0+b4*c1
                                                                                     //b4*c0+b5*c1 b5*c0+b6*c1 b6*c0+b7*c1 b7*c0+b8*c1

            src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 2);                          //a2 a3 a4 a5 a6 a7 a8 a9....a15 0 0
            src_r1_16x8b = _mm_srli_si128(src_r1_16x8b, 2);                          //b2 b3 b4 b5 b6 b7 b8 b9....b15 0 0

            src_r0_sht_16x8b = _mm_srli_si128(src_r0_sht_16x8b, 2);                  //a3 a4 a5 a6 a7 a8 a9....a15 0  0  0
            src_r1_sht_16x8b = _mm_srli_si128(src_r1_sht_16x8b, 2);                  //b3 b4 b5 b6 b7 b8 b9....b15 0  0  0

            src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);     //a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8 a8 a9 a9 a10
            src_r1_t1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);     //b2 b3 b3 b4 b4 b5 b5 b6 b6 b7 b7 b8 a8 a9 a9 a10

            res_r0_t2_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff2_3_16x8b);    //a2*c2+a3*c3 a3*c2+a4*c3 a4*c2+a5*c3 a5*c2+a6*c3
                                                                                     //a6*c2+a7*c3 a7*c2+a8*c3 a8*c2+a9*c3 a9*c2+a10*c3
            res_r1_t2_8x16b = _mm_maddubs_epi16(src_r1_t1_16x8b, coeff2_3_16x8b);    //b2*c2+b3*c3 b3*c2+b4*c3 b2*c4+b5*c3 b5*c2+b6*c3
                                                                                     //b6*c2+b7*c3 b7*c2+b8*c3 b8*c2+b9*c3 b9*c2+b10*c3

            src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 2);                          //a4 a5 a6 a7 a8 a9....a15 0  0  0  0
            src_r1_16x8b = _mm_srli_si128(src_r1_16x8b, 2);                          //b4 b5 b6 b7 b8 b9....b15 0  0  0  0

            src_r0_sht_16x8b = _mm_srli_si128(src_r0_sht_16x8b, 2);                  //a5 a6 a7 a8 a9....a15 0  0  0  0  0
            src_r1_sht_16x8b = _mm_srli_si128(src_r1_sht_16x8b, 2);                  //b5 b6 b7 b8 b9....b15 0  0  0  0  0

            src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);     //a4 a5 a5 a6 a6 a7 a7 a8 a8 a9 a9 a10 a10 a11 a11 a12
            src_r1_t1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);     //b4 b5 b5 b6 b6 b7 b7 b8 b8 b9 b9 b10 b10 b11 b11 b12

            res_r0_t3_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff4_5_16x8b);    //a4*c4+a5*c5 a5*c4+a6*c5 a6*c4+a7*c5 a7*c4+a8*c5
                                                                                     //a8*c4+a9*c5 a9*c4+a10*c5 a10*c4+a11*c5 a11*c4+a12*c5
            res_r1_t3_8x16b = _mm_maddubs_epi16(src_r1_t1_16x8b, coeff4_5_16x8b);    //b4*c4+b5*c5 b5*c4+b6*c5 b6*c4+b7*c5 b7*c4+b8*c5
                                                                                     //b8*c4+b9*c5 b9*c4+b10*c5 b10*c4+b11*c5 b11*c4+b12*c5
            src_r0_16x8b = _mm_loadu_si128((__m128i *)pu1_pred1);

            res_r0_t1_8x16b = _mm_add_epi16(res_r0_t1_8x16b, res_r0_t2_8x16b);
            res_r1_t1_8x16b = _mm_add_epi16(res_r1_t1_8x16b, res_r1_t2_8x16b);
            res_r0_t3_8x16b = _mm_add_epi16(const_val16_8x16b, res_r0_t3_8x16b);
            res_r1_t3_8x16b = _mm_add_epi16(const_val16_8x16b, res_r1_t3_8x16b);
            res_r0_t1_8x16b = _mm_add_epi16(res_r0_t1_8x16b, res_r0_t3_8x16b);
            res_r1_t1_8x16b = _mm_add_epi16(res_r1_t1_8x16b, res_r1_t3_8x16b);

            res_r0_t1_8x16b = _mm_srai_epi16(res_r0_t1_8x16b, 5);
            res_r1_t1_8x16b = _mm_srai_epi16(res_r1_t1_8x16b, 5);                    //shifting right by 5 bits

            res_16x8b = _mm_packus_epi16(res_r0_t1_8x16b, res_r1_t1_8x16b);
            res_16x8b = _mm_avg_epu8(src_r0_16x8b, res_16x8b);                       //computing q-pel

            _mm_storeu_si128((__m128i *)pu1_dst, res_16x8b);

            ht--;
            pu1_src += src_strd;
            pu1_pred1 += src_strd;
            pu1_dst += dst_strd;
        }
        while(ht > 0);
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_inter_pred_luma_vert_qpel_ssse3                    */
/*                                                                           */
/*  Description   : This function implements a six-tap filter vertically on  */
/*                  ht x wd block and averages the values with the source    */
/*                  pixels to calculate vertical quarter-pel as mentioned in */
/*                  sec. 8.4.2.2.1 titled "Luma sample interpolation         */
/*                  process". (ht,wd) can be (4,4), (8,4), (4,8), (8,8),     */
/*                  (16,8), (8,16) or (16,16).                               */
/*                                                                           */
/*  Inputs        : puc_src  - pointer to source                             */
/*                  puc_dst  - pointer to destination                        */
/*                  src_strd - stride for source                             */
/*                  dst_strd - stride for destination                        */
/*                  ht       - height of the block                           */
/*                  wd       - width of the block                            */
/*                  pu1_tmp  - pointer to temporary buffer                   */
/*                  dydx     - x and y reference offset for q-pel            */
/*                             calculations                                  */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes                              */
/*         13 02 2015   Kaushik         Initial Version                      */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_inter_pred_luma_vert_qpel_ssse3(UWORD8 *pu1_src,
                                           UWORD8 *pu1_dst,
                                           WORD32 src_strd,
                                           WORD32 dst_strd,
                                           WORD32 ht,
                                           WORD32 wd,
                                           UWORD8* pu1_tmp,
                                           WORD32 dydx)
{
    WORD32 y_offset;
    UWORD8 *pu1_pred1;


    __m128i src_r0_16x8b, src_r1_16x8b, src_r2_16x8b, src_r3_16x8b, src_r4_16x8b;
    __m128i src_r5_16x8b, src_r6_16x8b;
    __m128i src_r0r1_16x8b, src_r2r3_16x8b, src_r4r5_16x8b;
    __m128i res_16x8b, res_t1_8x16b, res_t2_8x16b, res_t3_8x16b;

    __m128i coeff0_1_16x8b, coeff2_3_16x8b, coeff4_5_16x8b;
    __m128i const_val16_8x16b;

    UNUSED(pu1_tmp);
    y_offset = dydx & 0xf;

    coeff0_1_16x8b = _mm_set1_epi32(0xFB01FB01); //c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1
    coeff2_3_16x8b = _mm_set1_epi32(0x14141414); //c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3
    coeff4_5_16x8b = _mm_set1_epi32(0x01FB01FB); //c4 c5 c4 c5 c4 c5 c4 c5 c4 c5 c4 c5 c4 c5 c4 c5
                                                 //c0 = c5 = 1, c1 = c4 = -5, c2 = c3 = 20

    pu1_pred1 = pu1_src + (y_offset >> 3) * src_strd;

    const_val16_8x16b = _mm_set1_epi16(16);

    pu1_src -= src_strd << 1; // the filter input starts from x[-2] (till x[3])

    if(wd == 4)
    {
        //Epilogue: Load all the pred rows except sixth and seventh row
        //          for the first and second row processing.
        src_r0_16x8b = _mm_loadl_epi64((__m128i *)pu1_src);
        pu1_src += src_strd;
        src_r1_16x8b = _mm_loadl_epi64((__m128i *)pu1_src);
        pu1_src += src_strd;
        src_r2_16x8b = _mm_loadl_epi64((__m128i *)pu1_src);
        pu1_src += src_strd;
        src_r3_16x8b = _mm_loadl_epi64((__m128i *)pu1_src);
        pu1_src += src_strd;
        src_r4_16x8b = _mm_loadl_epi64((__m128i *)pu1_src);
        pu1_src += src_strd;

        src_r0_16x8b = _mm_unpacklo_epi32(src_r0_16x8b, src_r1_16x8b);
        src_r1_16x8b = _mm_unpacklo_epi32(src_r1_16x8b, src_r2_16x8b);
        src_r2_16x8b = _mm_unpacklo_epi32(src_r2_16x8b, src_r3_16x8b);
        src_r3_16x8b = _mm_unpacklo_epi32(src_r3_16x8b, src_r4_16x8b);

        do
        {
            src_r5_16x8b = _mm_loadl_epi64((__m128i *)pu1_src);
            src_r6_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src + src_strd));

            src_r4_16x8b = _mm_unpacklo_epi32(src_r4_16x8b, src_r5_16x8b);
            src_r5_16x8b = _mm_unpacklo_epi32(src_r5_16x8b, src_r6_16x8b);

            src_r0r1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r1_16x8b);
            src_r2r3_16x8b = _mm_unpacklo_epi8(src_r2_16x8b, src_r3_16x8b);
            src_r4r5_16x8b = _mm_unpacklo_epi8(src_r4_16x8b, src_r5_16x8b);

            res_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);
            res_t2_8x16b = _mm_maddubs_epi16(src_r2r3_16x8b, coeff2_3_16x8b);
            res_t3_8x16b = _mm_maddubs_epi16(src_r4r5_16x8b, coeff4_5_16x8b);

            src_r0_16x8b = _mm_loadl_epi64((__m128i *)pu1_pred1);
            src_r1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_pred1 + src_strd));

            res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t2_8x16b);
            res_t3_8x16b = _mm_add_epi16(const_val16_8x16b, res_t3_8x16b);
            res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t3_8x16b);

            src_r0r1_16x8b = _mm_unpacklo_epi32(src_r0_16x8b,src_r1_16x8b);

            res_t1_8x16b = _mm_srai_epi16(res_t1_8x16b, 5); //shifting right by 5 bits.

            res_16x8b = _mm_packus_epi16(res_t1_8x16b, res_t1_8x16b);

            res_16x8b = _mm_avg_epu8(src_r0r1_16x8b, res_16x8b); //computing q-pel

            *((WORD32 *)(pu1_dst)) = _mm_cvtsi128_si32(res_16x8b);
            res_16x8b = _mm_srli_si128(res_16x8b, 4);
            *((WORD32 *)(pu1_dst + dst_strd)) = _mm_cvtsi128_si32(res_16x8b);

            src_r0_16x8b = src_r2_16x8b;
            src_r1_16x8b = src_r3_16x8b;
            src_r2_16x8b = src_r4_16x8b;
            src_r3_16x8b = src_r5_16x8b;
            src_r4_16x8b = src_r6_16x8b;

            ht -= 2;
            pu1_src += src_strd << 1;
            pu1_pred1 += src_strd << 1;
            pu1_dst += dst_strd << 1;
        }
        while(ht > 0);
    }

    else if(wd == 8)
    {
        //Epilogue: Load all the pred rows except sixth and seventh row
        //          for the first and second row processing.
        src_r0_16x8b = _mm_loadl_epi64((__m128i *)pu1_src);
        pu1_src += src_strd;
        src_r1_16x8b = _mm_loadl_epi64((__m128i *)pu1_src);
        pu1_src += src_strd;
        src_r2_16x8b = _mm_loadl_epi64((__m128i *)pu1_src);
        pu1_src += src_strd;
        src_r3_16x8b = _mm_loadl_epi64((__m128i *)pu1_src);
        pu1_src += src_strd;
        src_r4_16x8b = _mm_loadl_epi64((__m128i *)pu1_src);
        pu1_src += src_strd;

        src_r0_16x8b = _mm_unpacklo_epi64(src_r0_16x8b, src_r1_16x8b);
        src_r1_16x8b = _mm_unpacklo_epi64(src_r1_16x8b, src_r2_16x8b);
        src_r2_16x8b = _mm_unpacklo_epi64(src_r2_16x8b, src_r3_16x8b);
        src_r3_16x8b = _mm_unpacklo_epi64(src_r3_16x8b, src_r4_16x8b);

        do
        {
            src_r5_16x8b = _mm_loadl_epi64((__m128i *)pu1_src);
            src_r6_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src + src_strd));

            src_r4_16x8b = _mm_unpacklo_epi64(src_r4_16x8b, src_r5_16x8b);
            src_r5_16x8b = _mm_unpacklo_epi64(src_r5_16x8b, src_r6_16x8b);

            src_r0r1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r1_16x8b);
            src_r2r3_16x8b = _mm_unpacklo_epi8(src_r2_16x8b, src_r3_16x8b);
            src_r4r5_16x8b = _mm_unpacklo_epi8(src_r4_16x8b, src_r5_16x8b);

            res_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);
            res_t2_8x16b = _mm_maddubs_epi16(src_r2r3_16x8b, coeff2_3_16x8b);
            res_t3_8x16b = _mm_maddubs_epi16(src_r4r5_16x8b, coeff4_5_16x8b);

            src_r0r1_16x8b = _mm_loadl_epi64((__m128i *)pu1_pred1);

            res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t2_8x16b);
            res_t3_8x16b = _mm_add_epi16(const_val16_8x16b, res_t3_8x16b);
            res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t3_8x16b);

            res_t1_8x16b = _mm_srai_epi16(res_t1_8x16b, 5); //shifting right by 5 bits.

            res_16x8b = _mm_packus_epi16(res_t1_8x16b, res_t1_8x16b);
            res_16x8b = _mm_avg_epu8(src_r0r1_16x8b, res_16x8b); //computing q-pel

            _mm_storel_epi64((__m128i *)pu1_dst, res_16x8b);

            src_r0r1_16x8b = _mm_unpackhi_epi8(src_r0_16x8b, src_r1_16x8b);
            src_r2r3_16x8b = _mm_unpackhi_epi8(src_r2_16x8b, src_r3_16x8b);
            src_r4r5_16x8b = _mm_unpackhi_epi8(src_r4_16x8b, src_r5_16x8b);

            res_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);
            res_t2_8x16b = _mm_maddubs_epi16(src_r2r3_16x8b, coeff2_3_16x8b);
            res_t3_8x16b = _mm_maddubs_epi16(src_r4r5_16x8b, coeff4_5_16x8b);

            src_r0r1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_pred1 + src_strd));

            res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t2_8x16b);
            res_t3_8x16b = _mm_add_epi16(const_val16_8x16b, res_t3_8x16b);
            res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t3_8x16b);

            res_t1_8x16b = _mm_srai_epi16(res_t1_8x16b, 5); //shifting right by 5 bits.

            res_16x8b = _mm_packus_epi16(res_t1_8x16b, res_t1_8x16b);
            res_16x8b = _mm_avg_epu8(src_r0r1_16x8b, res_16x8b); //computing q-pel

            _mm_storel_epi64((__m128i *)(pu1_dst + dst_strd), res_16x8b);

            src_r0_16x8b = src_r2_16x8b;
            src_r1_16x8b = src_r3_16x8b;
            src_r2_16x8b = src_r4_16x8b;
            src_r3_16x8b = src_r5_16x8b;
            src_r4_16x8b = src_r6_16x8b;

            ht -= 2;
            pu1_src += src_strd << 1;
            pu1_pred1 += src_strd << 1;
            pu1_dst += dst_strd << 1;
        }
        while(ht > 0);
    }
    else // wd == 16
    {
        __m128i res_t0_8x16b;

        //Epilogue: Load all the pred rows except sixth and seventh row
        //          for the first and second row processing.
        src_r0_16x8b = _mm_loadu_si128((__m128i *)pu1_src);
        pu1_src += src_strd;
        src_r1_16x8b = _mm_loadu_si128((__m128i *)pu1_src);
        pu1_src += src_strd;
        src_r2_16x8b = _mm_loadu_si128((__m128i *)pu1_src);
        pu1_src += src_strd;
        src_r3_16x8b = _mm_loadu_si128((__m128i *)pu1_src);
        pu1_src += src_strd;
        src_r4_16x8b = _mm_loadu_si128((__m128i *)pu1_src);
        pu1_src += src_strd;

        do
        {
            src_r5_16x8b  = _mm_loadu_si128((__m128i *)pu1_src);
            src_r6_16x8b  = _mm_loadu_si128((__m128i *)(pu1_src + src_strd));

            src_r0r1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r1_16x8b);
            src_r2r3_16x8b = _mm_unpacklo_epi8(src_r2_16x8b, src_r3_16x8b);
            src_r4r5_16x8b = _mm_unpacklo_epi8(src_r4_16x8b, src_r5_16x8b);

            res_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);
            res_t2_8x16b = _mm_maddubs_epi16(src_r2r3_16x8b, coeff2_3_16x8b);
            res_t3_8x16b = _mm_maddubs_epi16(src_r4r5_16x8b, coeff4_5_16x8b);

            res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t2_8x16b);
            res_t3_8x16b = _mm_add_epi16(const_val16_8x16b, res_t3_8x16b);
            res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t3_8x16b);

            res_t0_8x16b = _mm_srai_epi16(res_t1_8x16b, 5); //shifting right by 5 bits.

            src_r0r1_16x8b = _mm_unpackhi_epi8(src_r0_16x8b, src_r1_16x8b);
            src_r2r3_16x8b = _mm_unpackhi_epi8(src_r2_16x8b, src_r3_16x8b);
            src_r4r5_16x8b = _mm_unpackhi_epi8(src_r4_16x8b, src_r5_16x8b);

            res_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);
            res_t2_8x16b = _mm_maddubs_epi16(src_r2r3_16x8b, coeff2_3_16x8b);
            res_t3_8x16b = _mm_maddubs_epi16(src_r4r5_16x8b, coeff4_5_16x8b);

            src_r0r1_16x8b = _mm_loadu_si128((__m128i *)pu1_pred1);

            res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t2_8x16b);
            res_t3_8x16b = _mm_add_epi16(const_val16_8x16b, res_t3_8x16b);
            res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t3_8x16b);

            res_t1_8x16b = _mm_srai_epi16(res_t1_8x16b, 5); //shifting right by 5 bits.

            res_16x8b = _mm_packus_epi16(res_t0_8x16b, res_t1_8x16b);
            res_16x8b = _mm_avg_epu8(src_r0r1_16x8b, res_16x8b); //computing q-pel

            _mm_storeu_si128((__m128i *)pu1_dst, res_16x8b);

            src_r0r1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r2_16x8b);
            src_r2r3_16x8b = _mm_unpacklo_epi8(src_r3_16x8b, src_r4_16x8b);
            src_r4r5_16x8b = _mm_unpacklo_epi8(src_r5_16x8b, src_r6_16x8b);

            res_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);
            res_t2_8x16b = _mm_maddubs_epi16(src_r2r3_16x8b, coeff2_3_16x8b);
            res_t3_8x16b = _mm_maddubs_epi16(src_r4r5_16x8b, coeff4_5_16x8b);

            res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t2_8x16b);
            res_t3_8x16b = _mm_add_epi16(const_val16_8x16b, res_t3_8x16b);
            res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t3_8x16b);

            res_t0_8x16b = _mm_srai_epi16(res_t1_8x16b, 5); //shifting right by 5 bits.

            src_r0r1_16x8b = _mm_unpackhi_epi8(src_r1_16x8b, src_r2_16x8b);
            src_r2r3_16x8b = _mm_unpackhi_epi8(src_r3_16x8b, src_r4_16x8b);
            src_r4r5_16x8b = _mm_unpackhi_epi8(src_r5_16x8b, src_r6_16x8b);

            res_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);
            res_t2_8x16b = _mm_maddubs_epi16(src_r2r3_16x8b, coeff2_3_16x8b);
            res_t3_8x16b = _mm_maddubs_epi16(src_r4r5_16x8b, coeff4_5_16x8b);

            src_r0r1_16x8b = _mm_loadu_si128((__m128i *)(pu1_pred1 + src_strd));

            res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t2_8x16b);
            res_t3_8x16b = _mm_add_epi16(const_val16_8x16b, res_t3_8x16b);
            res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t3_8x16b);

            res_t1_8x16b = _mm_srai_epi16(res_t1_8x16b, 5); //shifting right by 5 bits.

            res_16x8b = _mm_packus_epi16(res_t0_8x16b, res_t1_8x16b);
            res_16x8b = _mm_avg_epu8(src_r0r1_16x8b, res_16x8b); //computing q-pel

            _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), res_16x8b);

            src_r0_16x8b = src_r2_16x8b;
            src_r1_16x8b = src_r3_16x8b;
            src_r2_16x8b = src_r4_16x8b;
            src_r3_16x8b = src_r5_16x8b;
            src_r4_16x8b = src_r6_16x8b;

            ht -= 2;
            pu1_src += src_strd << 1;
            pu1_pred1 += src_strd << 1;
            pu1_dst += dst_strd << 1;
        }
        while(ht > 0);
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_inter_pred_luma_horz_qpel_vert_qpel_ssse3          */
/*                                                                           */
/*  Description   : This function implements a six-tap filter vertically and */
/*                  horizontally on ht x wd block separately and averages    */
/*                  the two sets of values to calculate values at (1/4,1/4), */
/*                  (1/4, 3/4), (3/4, 1/4) or (3/4, 3/4) as mentioned in     */
/*                  sec. 8.4.2.2.1 titled "Luma sample interpolation         */
/*                  process". (ht,wd) can be (4,4), (8,4), (4,8), (8,8),     */
/*                  (16,8), (8,16) or (16,16).                               */
/*                                                                           */
/*  Inputs        : puc_src  - pointer to source                             */
/*                  puc_dst  - pointer to destination                        */
/*                  src_strd - stride for source                             */
/*                  dst_strd - stride for destination                        */
/*                  ht       - height of the block                           */
/*                  wd       - width of the block                            */
/*                  pu1_tmp  - pointer to temporary buffer                   */
/*                  dydx     - x and y reference offset for q-pel            */
/*                             calculations                                  */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes                              */
/*         13 02 2015   Kaushik         Initial Version                      */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_inter_pred_luma_horz_qpel_vert_qpel_ssse3(UWORD8 *pu1_src,
                                                     UWORD8 *pu1_dst,
                                                     WORD32 src_strd,
                                                     WORD32 dst_strd,
                                                     WORD32 ht,
                                                     WORD32 wd,
                                                     UWORD8* pu1_tmp,
                                                     WORD32 dydx)
{
    WORD32 ht_temp;
    UWORD8 *pu1_pred_vert,*pu1_pred_horiz;
    UWORD8 *pu1_tmp1, *pu1_tmp2;
    WORD32 x_offset, y_offset;

    __m128i coeff0_1_16x8b, coeff2_3_16x8b, coeff4_5_16x8b;
    __m128i const_val16_8x16b;

    pu1_tmp1 = pu1_tmp;

    dydx &= 0xf;
    ht_temp = ht;
    x_offset = dydx & 0x3;
    y_offset = dydx >> 2;
    pu1_tmp2 = pu1_tmp1;

    pu1_pred_vert  = pu1_src + (x_offset >> 1) - 2*src_strd;
    pu1_pred_horiz = pu1_src + (y_offset >> 1) * src_strd - 2;
    //the filter input starts from x[-2] (till x[3])

    coeff0_1_16x8b = _mm_set1_epi32(0xFB01FB01);  //c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1
    coeff2_3_16x8b = _mm_set1_epi32(0x14141414);  //c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3
    coeff4_5_16x8b = _mm_set1_epi32(0x01FB01FB);  //c4 c5 c5 c5 c4 c5 c5 c5 c4 c5 c5 c5 c4 c5 c5 c5
                                                  //c0 = c5 = 1, c1 = c4 = -5, c2 = c3 = 20
    const_val16_8x16b = _mm_set1_epi16(16);

    if(wd == 4)
    {
        //vertical q-pel filter
        {
            __m128i src_r0_16x8b, src_r1_16x8b, src_r2_16x8b, src_r3_16x8b, src_r4_16x8b;
            __m128i src_r5_16x8b, src_r6_16x8b;
            __m128i src_r0r1_16x8b, src_r2r3_16x8b, src_r4r5_16x8b;

            __m128i res_r0r1_16x8b, res_t1_8x16b, res_t2_8x16b, res_t3_8x16b;

            //epilogue: Load all the pred rows except sixth  and seventh row for the
            //first and second row processing.
            src_r0_16x8b = _mm_loadl_epi64((__m128i *)(pu1_pred_vert));
            pu1_pred_vert = pu1_pred_vert + src_strd;

            src_r1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_pred_vert));
            pu1_pred_vert = pu1_pred_vert + src_strd;
            src_r0_16x8b = _mm_unpacklo_epi32(src_r0_16x8b, src_r1_16x8b);

            src_r2_16x8b = _mm_loadl_epi64((__m128i *)(pu1_pred_vert));
            pu1_pred_vert = pu1_pred_vert + src_strd;
            src_r1_16x8b = _mm_unpacklo_epi32(src_r1_16x8b, src_r2_16x8b);

            src_r3_16x8b = _mm_loadl_epi64((__m128i *)(pu1_pred_vert));
            pu1_pred_vert = pu1_pred_vert + src_strd;
            src_r2_16x8b = _mm_unpacklo_epi32(src_r2_16x8b, src_r3_16x8b);

            src_r4_16x8b = _mm_loadl_epi64((__m128i *)(pu1_pred_vert));
            pu1_pred_vert = pu1_pred_vert + src_strd;
            src_r3_16x8b = _mm_unpacklo_epi32(src_r3_16x8b, src_r4_16x8b);

            //Core Loop: Process all the rows.
            do
            {
                src_r5_16x8b = _mm_loadl_epi64((__m128i *)(pu1_pred_vert));
                src_r4_16x8b = _mm_unpacklo_epi32(src_r4_16x8b, src_r5_16x8b);

                src_r6_16x8b = _mm_loadl_epi64((__m128i *)(pu1_pred_vert + src_strd));
                src_r5_16x8b = _mm_unpacklo_epi32(src_r5_16x8b, src_r6_16x8b);

                src_r0r1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r1_16x8b);
                src_r2r3_16x8b = _mm_unpacklo_epi8(src_r2_16x8b, src_r3_16x8b);
                src_r4r5_16x8b = _mm_unpacklo_epi8(src_r4_16x8b, src_r5_16x8b);

                res_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);
                res_t2_8x16b = _mm_maddubs_epi16(src_r2r3_16x8b, coeff2_3_16x8b);
                res_t3_8x16b = _mm_maddubs_epi16(src_r4r5_16x8b, coeff4_5_16x8b);

                res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t2_8x16b);
                res_t3_8x16b = _mm_add_epi16(const_val16_8x16b, res_t3_8x16b);
                res_t1_8x16b = _mm_add_epi16(res_t3_8x16b, res_t1_8x16b);

                res_t1_8x16b = _mm_srai_epi16(res_t1_8x16b, 5); //shifting right by 5 bits.
                res_r0r1_16x8b = _mm_packus_epi16(res_t1_8x16b, res_t1_8x16b);

                _mm_storel_epi64((__m128i *)pu1_tmp1, res_r0r1_16x8b);

                src_r0_16x8b = src_r2_16x8b;
                src_r1_16x8b = src_r3_16x8b;
                src_r2_16x8b = src_r4_16x8b;
                src_r3_16x8b = src_r5_16x8b;
                src_r4_16x8b = src_r6_16x8b;

                ht_temp -= 2;
                pu1_pred_vert += src_strd << 1;
                pu1_tmp1 += 8;
            }
            while(ht_temp > 0);
        }

        //horizontal q-pel filter
        {
            __m128i src_r0_16x8b, src_r1_16x8b;
            __m128i src_r0_sht_16x8b, src_r1_sht_16x8b;
            __m128i src_r0r1_vpel_16x8b, src_r0r1_t1_16x8b;

            __m128i res_r0r1_t1_8x16b, res_r0r1_t2_8x16b, res_r0r1_t3_8x16b;
            __m128i res_r0r1_16x8b;

            //Row0 : a0 a1 a2 a3 a4 a5 a6 a7 a8 a9.....
            //Row1 : b0 b1 b2 b3 b4 b5 b6 b7 b8 b9.....

            do
            {
                src_r0_16x8b = _mm_loadu_si128((__m128i *)pu1_pred_horiz);                  //a0 a1 a2 a3 a4 a5 a6 a7 a8 a9....a15
                src_r1_16x8b = _mm_loadu_si128((__m128i *)(pu1_pred_horiz + src_strd));     //b0 b1 b2 b3 b4 b5 b6 b7 b8 b9....b15

                src_r0r1_vpel_16x8b = _mm_loadl_epi64((__m128i *)pu1_tmp2);

                src_r0_sht_16x8b = _mm_srli_si128(src_r0_16x8b, 1);                          //a1 a2 a3 a4 a5 a6 a7 a8 a9....a15 0
                src_r1_sht_16x8b = _mm_srli_si128(src_r1_16x8b, 1);                          //b1 b2 b3 b4 b5 b6 b7 b8 b9....b15 0

                src_r0_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);            //a0 a1 a1 a2 a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8
                src_r1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);            //b0 b1 b1 b2 b2 b3 b3 b4 b4 b5 b5 b6 b6 b7 b7 b8

                src_r0r1_t1_16x8b = _mm_unpacklo_epi64(src_r0_16x8b, src_r1_16x8b);          //a0 a1 a1 a2 a2 a3 a3 a4 b0 b1 b1 b2 b2 b3 b3 b4
                res_r0r1_t1_8x16b = _mm_maddubs_epi16(src_r0r1_t1_16x8b, coeff0_1_16x8b);    //a0*c0+a1*c1 a1*c0+a2*c1 a2*c0+a3*c1 a3*c0+a4*c1
                                                                                             //b0*c0+b1*c1 b1*c0+b2*c1 b2*c0+b3*c1 b3*c0+b4*c1

                src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 4);                              //a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8  0  0  0  0
                src_r1_16x8b = _mm_srli_si128(src_r1_16x8b, 4);                              //b2 b3 b3 b4 b4 b5 b5 b6 b6 b7 b7 b8  0  0  0  0

                src_r0r1_t1_16x8b = _mm_unpacklo_epi64(src_r0_16x8b, src_r1_16x8b);          //a2 a3 a3 a4 a4 a5 a5 a6 b2 b3 b3 b4 b4 b5 b5 b6
                res_r0r1_t2_8x16b = _mm_maddubs_epi16(src_r0r1_t1_16x8b, coeff2_3_16x8b);    //a2*c2+a3*c3 a3*c2+a4*c3 a4*c2+a5*c3 a5*c2+a6*c3
                                                                                             //b2*c2+b3*c3 b3*c2+b4*c3 b4*c2+b5*c3 b5*c2+b6*c3

                src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 4);                              //a4 a5 a5 a6 a6 a7 a7 a8  0  0  0  0  0  0  0  0
                src_r1_16x8b = _mm_srli_si128(src_r1_16x8b, 4);                              //b4 b5 b5 b6 b6 b7 b7 b8  0  0  0  0  0  0  0  0

                src_r0r1_t1_16x8b = _mm_unpacklo_epi64(src_r0_16x8b, src_r1_16x8b);          //a4 a5 a5 a6 a6 a7 a7 a8 b4 b5 b5 b6 b6 b7 b7 b8
                res_r0r1_t3_8x16b = _mm_maddubs_epi16(src_r0r1_t1_16x8b, coeff4_5_16x8b);    //a4*c4+a5*c5 a5*c4+a6*c5 a6*c4+a7*c5 a7*c4+a8*c5
                                                                                             //b4*c4+b5*c5 b5*c4+b6*c5 b4*c6+b7*c5 b7*c4+b8*c5

                res_r0r1_t1_8x16b = _mm_add_epi16(res_r0r1_t1_8x16b, res_r0r1_t2_8x16b);
                res_r0r1_t3_8x16b = _mm_add_epi16(const_val16_8x16b, res_r0r1_t3_8x16b);
                res_r0r1_t1_8x16b = _mm_add_epi16(res_r0r1_t1_8x16b, res_r0r1_t3_8x16b);     //a0*c0+a1*c1+a2*c2+a3*c3+a4*a4+a5*c5 + 15;
                                                                                             //a1*c0+a2*c1+a2*c2+a3*c3+a5*a4+a6*c5 + 15;
                                                                                             //a2*c0+a3*c1+a4*c2+a5*c3+a6*a4+a7*c5 + 15;
                                                                                             //a3*c0+a4*c1+a5*c2+a6*c3+a6*a4+a8*c5 + 15;
                                                                                             //b0*c0+b1*c1+b2*c2+b3*c3+b4*b4+b5*c5 + 15;
                                                                                             //b1*c0+b2*c1+b2*c2+b3*c3+b5*b4+b6*c5 + 15;
                                                                                             //b2*c0+b3*c1+b4*c2+b5*c3+b6*b4+b7*c5 + 15;
                                                                                             //b3*c0+b4*c1+b5*c2+b6*c3+b6*b4+b8*c5 + 15;

                res_r0r1_t1_8x16b = _mm_srai_epi16(res_r0r1_t1_8x16b, 5);                    //shifting right by 5 bits.

                res_r0r1_16x8b = _mm_packus_epi16(res_r0r1_t1_8x16b,res_r0r1_t1_8x16b);

                res_r0r1_16x8b = _mm_avg_epu8(res_r0r1_16x8b,src_r0r1_vpel_16x8b);

                *((WORD32 *)(pu1_dst)) = _mm_cvtsi128_si32(res_r0r1_16x8b);
                res_r0r1_16x8b = _mm_srli_si128(res_r0r1_16x8b, 4);
                *((WORD32 *)(pu1_dst + dst_strd)) = _mm_cvtsi128_si32(res_r0r1_16x8b);

                ht -= 2;
                pu1_pred_horiz += src_strd << 1;
                pu1_tmp2 += 8;
                pu1_dst += dst_strd << 1;
            }
            while(ht > 0);
        }
    }
    else if(wd == 8)
    {
        //vertical q-pel filter
        {
            __m128i src_r0_16x8b, src_r1_16x8b, src_r2_16x8b, src_r3_16x8b;
            __m128i src_r4_16x8b, src_r5_16x8b, src_r6_16x8b;
            __m128i src_r0r1_16x8b, src_r2r3_16x8b, src_r4r5_16x8b;

            __m128i res_16x8b, res_t1_8x16b, res_t2_8x16b, res_t3_8x16b;

            //epilogue: Load all the pred rows except sixth  and seventh row for the
            //first and second row processing.
            src_r0_16x8b = _mm_loadl_epi64((__m128i *)(pu1_pred_vert));
            pu1_pred_vert = pu1_pred_vert + src_strd;

            src_r1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_pred_vert));
            pu1_pred_vert = pu1_pred_vert + src_strd;
            src_r0_16x8b = _mm_unpacklo_epi64(src_r0_16x8b, src_r1_16x8b);

            src_r2_16x8b = _mm_loadl_epi64((__m128i *)(pu1_pred_vert));
            pu1_pred_vert = pu1_pred_vert + src_strd;
            src_r1_16x8b = _mm_unpacklo_epi64(src_r1_16x8b, src_r2_16x8b);

            src_r3_16x8b = _mm_loadl_epi64((__m128i *)(pu1_pred_vert));
            pu1_pred_vert = pu1_pred_vert + src_strd;
            src_r2_16x8b = _mm_unpacklo_epi64(src_r2_16x8b, src_r3_16x8b);

            src_r4_16x8b = _mm_loadl_epi64((__m128i *)(pu1_pred_vert));
            pu1_pred_vert = pu1_pred_vert + src_strd;
            src_r3_16x8b = _mm_unpacklo_epi64(src_r3_16x8b, src_r4_16x8b);

            //Core Loop: Process all the rows.
            do
            {
                src_r5_16x8b = _mm_loadl_epi64((__m128i *)(pu1_pred_vert));
                src_r4_16x8b = _mm_unpacklo_epi64(src_r4_16x8b, src_r5_16x8b);

                src_r6_16x8b = _mm_loadl_epi64((__m128i *)(pu1_pred_vert + src_strd));
                src_r5_16x8b = _mm_unpacklo_epi64(src_r5_16x8b, src_r6_16x8b);

                src_r0r1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r1_16x8b);
                src_r2r3_16x8b = _mm_unpacklo_epi8(src_r2_16x8b, src_r3_16x8b);
                src_r4r5_16x8b = _mm_unpacklo_epi8(src_r4_16x8b, src_r5_16x8b);

                res_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);
                res_t2_8x16b = _mm_maddubs_epi16(src_r2r3_16x8b, coeff2_3_16x8b);
                res_t3_8x16b = _mm_maddubs_epi16(src_r4r5_16x8b, coeff4_5_16x8b);

                res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t2_8x16b);
                res_t3_8x16b = _mm_add_epi16(const_val16_8x16b, res_t3_8x16b);
                res_t1_8x16b = _mm_add_epi16(res_t3_8x16b, res_t1_8x16b);

                res_t1_8x16b = _mm_srai_epi16(res_t1_8x16b, 5); //shifting right by 5 bits.
                res_16x8b = _mm_packus_epi16(res_t1_8x16b, res_t1_8x16b);

                _mm_storel_epi64((__m128i *)(pu1_tmp1), res_16x8b);

                src_r0r1_16x8b = _mm_unpackhi_epi8(src_r0_16x8b, src_r1_16x8b);
                src_r2r3_16x8b = _mm_unpackhi_epi8(src_r2_16x8b, src_r3_16x8b);
                src_r4r5_16x8b = _mm_unpackhi_epi8(src_r4_16x8b, src_r5_16x8b);

                res_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);
                res_t2_8x16b = _mm_maddubs_epi16(src_r2r3_16x8b, coeff2_3_16x8b);
                res_t3_8x16b = _mm_maddubs_epi16(src_r4r5_16x8b, coeff4_5_16x8b);

                res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t2_8x16b);
                res_t3_8x16b = _mm_add_epi16(const_val16_8x16b, res_t3_8x16b);
                res_t1_8x16b = _mm_add_epi16(res_t3_8x16b, res_t1_8x16b);

                res_t1_8x16b = _mm_srai_epi16(res_t1_8x16b, 5); //shifting right by 5 bits.
                res_16x8b = _mm_packus_epi16(res_t1_8x16b, res_t1_8x16b);

                _mm_storel_epi64((__m128i *)(pu1_tmp1 + 8), res_16x8b);

                src_r0_16x8b = src_r2_16x8b;
                src_r1_16x8b = src_r3_16x8b;
                src_r2_16x8b = src_r4_16x8b;
                src_r3_16x8b = src_r5_16x8b;
                src_r4_16x8b = src_r6_16x8b;

                ht_temp -= 2;
                pu1_pred_vert += src_strd << 1;
                pu1_tmp1 += 16;
            }
            while(ht_temp > 0);
        }

        //horizontal q-pel filter
        {
            __m128i src_r0_16x8b, src_r1_16x8b, src_r0_sht_16x8b, src_r1_sht_16x8b;
            __m128i src_r0_t1_16x8b, src_r1_t1_16x8b;
            __m128i src_r0_vpel_16x8b, src_r1_vpel_16x8b;

            __m128i res_r0_t1_8x16b, res_r0_t2_8x16b, res_r0_t3_8x16b;
            __m128i res_r1_t1_8x16b, res_r1_t2_8x16b, res_r1_t3_8x16b, res_16x8b;

            //Row0 : a0 a1 a2 a3 a4 a5 a6 a7 a8 a9.....
            //Row1 : b0 b1 b2 b3 b4 b5 b6 b7 b8 b9.....

            do
            {
                src_r0_16x8b = _mm_loadu_si128((__m128i *)(pu1_pred_horiz));               //a0 a1 a2 a3 a4 a5 a6 a7 a8 a9....a15
                src_r1_16x8b = _mm_loadu_si128((__m128i *)(pu1_pred_horiz + src_strd));    //b0 b1 b2 b3 b4 b5 b6 b7 b8 b9....b15

                src_r0_vpel_16x8b = _mm_loadl_epi64((__m128i *)(pu1_tmp2));                //a2 a3 a4 a5 a6 a7 a8....a15 0 or
                                                                                           //a3 a4 a5 a6 a7 a8 a9....a15 0
                src_r1_vpel_16x8b = _mm_loadl_epi64((__m128i *)(pu1_tmp2 + 8));
                                                                                           //b2 b3 b4 b5 b6 b7 b8....b15 0 or
                                                                                           //b3 b4 b5 b6 b7 b8 b9....b15 0

                src_r0_sht_16x8b = _mm_srli_si128(src_r0_16x8b, 1);                        //a1 a2 a3 a4 a5 a6 a7 a8 a9....a15 0
                src_r1_sht_16x8b = _mm_srli_si128(src_r1_16x8b, 1);                        //b1 b2 b3 b4 b5 b6 b7 b8 b9....b15 0

                src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);       //a0 a1 a1 a2 a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8
                src_r1_t1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);       //b0 b1 b1 b2 b2 b3 b3 b4 b4 b5 b5 b6 b6 b7 b7 b8

                res_r0_t1_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff0_1_16x8b);      //a0*c0+a1*c1 a1*c0+a2*c1 a2*c0+a3*c1 a3*c0+a4*c1
                                                                                           //a4*c0+a5*c1 a5*c0+a6*c1 a6*c0+a7*c1 a7*c0+a8*c1
                res_r1_t1_8x16b = _mm_maddubs_epi16(src_r1_t1_16x8b, coeff0_1_16x8b);      //b0*c0+b1*c1 b1*c0+b2*c1 b2*c0+b3*c1 b3*c0+b4*c1
                                                                                           //b4*c0+b5*c1 b5*c0+b6*c1 b6*c0+b7*c1 b7*c0+b8*c1

                src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 2);                            //a2 a3 a4 a5 a6 a7 a8 a9....a15 0 0
                src_r1_16x8b = _mm_srli_si128(src_r1_16x8b, 2);                            //b2 b3 b4 b5 b6 b7 b8 b9....b15 0 0

                src_r0_sht_16x8b = _mm_srli_si128(src_r0_sht_16x8b, 2);                    //a3 a4 a5 a6 a7 a8 a9....a15 0  0  0
                src_r1_sht_16x8b = _mm_srli_si128(src_r1_sht_16x8b, 2);                    //b3 b4 b5 b6 b7 b8 b9....b15 0  0  0

                src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);       //a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8 a8 a9 a9 a10
                src_r1_t1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);       //b2 b3 b3 b4 b4 b5 b5 b6 b6 b7 b7 b8 a8 a9 a9 a10

                res_r0_t2_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff2_3_16x8b);      //a2*c2+a3*c3 a3*c2+a4*c3 a4*c2+a5*c3 a5*c2+a6*c3
                                                                                           //a6*c2+a7*c3 a7*c2+a8*c3 a8*c2+a9*c3 a9*c2+a10*c3
                res_r1_t2_8x16b = _mm_maddubs_epi16(src_r1_t1_16x8b, coeff2_3_16x8b);      //b2*c2+b3*c3 b3*c2+b4*c3 b2*c4+b5*c3 b5*c2+b6*c3
                                                                                           //b6*c2+b7*c3 b7*c2+b8*c3 b8*c2+b9*c3 b9*c2+b10*c3

                src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 2);                            //a4 a5 a6 a7 a8 a9....a15 0  0  0  0
                src_r1_16x8b = _mm_srli_si128(src_r1_16x8b, 2);                            //b4 b5 b6 b7 b8 b9....b15 0  0  0  0

                src_r0_sht_16x8b = _mm_srli_si128(src_r0_sht_16x8b, 2);                    //a5 a6 a7 a8 a9....a15 0  0  0  0  0
                src_r1_sht_16x8b = _mm_srli_si128(src_r1_sht_16x8b, 2);                    //b5 b6 b7 b8 b9....b15 0  0  0  0  0

                src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);       //a4 a5 a5 a6 a6 a7 a7 a8 a8 a9 a9 a10 a10 a11 a11 a12
                src_r1_t1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);       //b4 b5 b5 b6 b6 b7 b7 b8 b8 b9 b9 b10 b10 b11 b11 b12

                res_r0_t3_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff4_5_16x8b);      //a4*c4+a5*c5 a5*c4+a6*c5 a6*c4+a7*c5 a7*c4+a8*c5
                                                                                           //a8*c4+a9*c5 a9*c4+a10*c5 a10*c4+a11*c5 a11*c4+a12*c5
                res_r1_t3_8x16b = _mm_maddubs_epi16(src_r1_t1_16x8b, coeff4_5_16x8b);      //b4*c4+b5*c5 b5*c4+b6*c5 b6*c4+b7*c5 b7*c4+b8*c5
                                                                                           //b8*c4+b9*c5 b9*c4+b10*c5 b10*c4+b11*c5 b11*c4+b12*c5
                res_r0_t1_8x16b = _mm_add_epi16(res_r0_t1_8x16b, res_r0_t2_8x16b);
                res_r0_t3_8x16b = _mm_add_epi16(const_val16_8x16b, res_r0_t3_8x16b);
                res_r0_t1_8x16b = _mm_add_epi16(res_r0_t1_8x16b, res_r0_t3_8x16b);
                res_r0_t1_8x16b = _mm_srai_epi16(res_r0_t1_8x16b, 5);                      //shifting right by 5 bits.

                res_16x8b = _mm_packus_epi16(res_r0_t1_8x16b, res_r0_t1_8x16b);
                res_16x8b = _mm_avg_epu8(res_16x8b, src_r0_vpel_16x8b);

                _mm_storel_epi64((__m128i *)(pu1_dst), res_16x8b);

                res_r1_t1_8x16b = _mm_add_epi16(res_r1_t1_8x16b, res_r1_t2_8x16b);
                res_r1_t3_8x16b = _mm_add_epi16(const_val16_8x16b, res_r1_t3_8x16b);
                res_r1_t1_8x16b = _mm_add_epi16(res_r1_t1_8x16b, res_r1_t3_8x16b);
                res_r1_t1_8x16b = _mm_srai_epi16(res_r1_t1_8x16b, 5);                      //shifting right by 5 bits.

                res_16x8b = _mm_packus_epi16(res_r1_t1_8x16b, res_r1_t1_8x16b);
                res_16x8b = _mm_avg_epu8(res_16x8b,src_r1_vpel_16x8b);

                _mm_storel_epi64((__m128i *)(pu1_dst + dst_strd), res_16x8b);

                ht -= 2;
                pu1_pred_horiz += src_strd << 1;
                pu1_dst += dst_strd << 1;
                pu1_tmp2 += 16;
            }
            while(ht > 0);
        }
    }
    else // wd == 16
    {
        //vertical q-pel filter
        {
            __m128i src_r0_16x8b, src_r1_16x8b, src_r2_16x8b, src_r3_16x8b;
            __m128i src_r4_16x8b, src_r5_16x8b, src_r6_16x8b;
            __m128i src_r0r1_16x8b, src_r2r3_16x8b, src_r4r5_16x8b;

            __m128i res_t0_8x16b, res_t1_8x16b, res_t2_8x16b, res_t3_8x16b;
            __m128i res_16x8b;

            //epilogue: Load all the pred rows except sixth  and seventh row for the
            //first and second row processing.
            src_r0_16x8b = _mm_loadu_si128((__m128i *)(pu1_pred_vert));
            pu1_pred_vert =  pu1_pred_vert + src_strd;
            src_r1_16x8b = _mm_loadu_si128((__m128i *)(pu1_pred_vert));
            pu1_pred_vert =  pu1_pred_vert + src_strd;
            src_r2_16x8b = _mm_loadu_si128((__m128i *)(pu1_pred_vert));
            pu1_pred_vert =  pu1_pred_vert + src_strd;
            src_r3_16x8b = _mm_loadu_si128((__m128i *)(pu1_pred_vert));
            pu1_pred_vert =  pu1_pred_vert + src_strd;
            src_r4_16x8b = _mm_loadu_si128((__m128i *)(pu1_pred_vert));
            pu1_pred_vert =  pu1_pred_vert + src_strd;

            //Core Loop: Process all the rows.
            do
            {
                src_r5_16x8b = _mm_loadu_si128((__m128i *)(pu1_pred_vert));
                src_r6_16x8b = _mm_loadu_si128((__m128i *)(pu1_pred_vert + src_strd));

                src_r0r1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r1_16x8b);
                src_r2r3_16x8b = _mm_unpacklo_epi8(src_r2_16x8b, src_r3_16x8b);
                src_r4r5_16x8b = _mm_unpacklo_epi8(src_r4_16x8b, src_r5_16x8b);

                res_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);
                res_t2_8x16b = _mm_maddubs_epi16(src_r2r3_16x8b, coeff2_3_16x8b);
                res_t3_8x16b = _mm_maddubs_epi16(src_r4r5_16x8b, coeff4_5_16x8b);

                res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t2_8x16b);
                res_t3_8x16b = _mm_add_epi16(const_val16_8x16b, res_t3_8x16b);
                res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t3_8x16b);
                res_t0_8x16b = _mm_srai_epi16(res_t1_8x16b, 5); //shifting right by 5 bits.

                src_r0r1_16x8b = _mm_unpackhi_epi8(src_r0_16x8b, src_r1_16x8b);
                src_r2r3_16x8b = _mm_unpackhi_epi8(src_r2_16x8b, src_r3_16x8b);
                src_r4r5_16x8b = _mm_unpackhi_epi8(src_r4_16x8b, src_r5_16x8b);

                res_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);
                res_t2_8x16b = _mm_maddubs_epi16(src_r2r3_16x8b, coeff2_3_16x8b);
                res_t3_8x16b = _mm_maddubs_epi16(src_r4r5_16x8b, coeff4_5_16x8b);

                res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t2_8x16b);
                res_t3_8x16b = _mm_add_epi16(const_val16_8x16b, res_t3_8x16b);
                res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t3_8x16b);
                res_t1_8x16b = _mm_srai_epi16(res_t1_8x16b, 5); //shifting right by 5 bits.

                res_16x8b = _mm_packus_epi16(res_t0_8x16b, res_t1_8x16b);

                _mm_storeu_si128((__m128i *)(pu1_tmp1), res_16x8b);

                src_r0r1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r2_16x8b);
                src_r2r3_16x8b = _mm_unpacklo_epi8(src_r3_16x8b, src_r4_16x8b);
                src_r4r5_16x8b = _mm_unpacklo_epi8(src_r5_16x8b, src_r6_16x8b);

                res_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);
                res_t2_8x16b = _mm_maddubs_epi16(src_r2r3_16x8b, coeff2_3_16x8b);
                res_t3_8x16b = _mm_maddubs_epi16(src_r4r5_16x8b, coeff4_5_16x8b);

                res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t2_8x16b);
                res_t3_8x16b = _mm_add_epi16(const_val16_8x16b, res_t3_8x16b);
                res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t3_8x16b);
                res_t0_8x16b = _mm_srai_epi16(res_t1_8x16b, 5); //shifting right by 5 bits.

                src_r0r1_16x8b = _mm_unpackhi_epi8(src_r1_16x8b, src_r2_16x8b);
                src_r2r3_16x8b = _mm_unpackhi_epi8(src_r3_16x8b, src_r4_16x8b);
                src_r4r5_16x8b = _mm_unpackhi_epi8(src_r5_16x8b, src_r6_16x8b);

                res_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);
                res_t2_8x16b = _mm_maddubs_epi16(src_r2r3_16x8b, coeff2_3_16x8b);
                res_t3_8x16b = _mm_maddubs_epi16(src_r4r5_16x8b, coeff4_5_16x8b);

                res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t2_8x16b);
                res_t3_8x16b = _mm_add_epi16(const_val16_8x16b, res_t3_8x16b);
                res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t3_8x16b);
                res_t1_8x16b = _mm_srai_epi16(res_t1_8x16b, 5); //shifting right by 5 bits.

                res_16x8b = _mm_packus_epi16(res_t0_8x16b, res_t1_8x16b);

                _mm_storeu_si128((__m128i *)(pu1_tmp1 + 16), res_16x8b);

                src_r0_16x8b = src_r2_16x8b;
                src_r1_16x8b = src_r3_16x8b;
                src_r2_16x8b = src_r4_16x8b;
                src_r3_16x8b = src_r5_16x8b;
                src_r4_16x8b = src_r6_16x8b;

                ht_temp -= 2;
                pu1_pred_vert += src_strd << 1;
                pu1_tmp1 += 32;
            }
            while(ht_temp > 0);
        }
        //horizontal q-pel filter
        {
            __m128i src_r0_16x8b, src_r1_16x8b, src_r0_sht_16x8b, src_r1_sht_16x8b;
            __m128i src_r0_t1_16x8b, src_r1_t1_16x8b;
            __m128i src_vpel_16x8b;

            __m128i res_r0_t1_8x16b, res_r0_t2_8x16b, res_r0_t3_8x16b;
            __m128i res_r1_t1_8x16b, res_r1_t2_8x16b, res_r1_t3_8x16b;
            __m128i res_16x8b;

            //Row0 : a0 a1 a2 a3 a4 a5 a6 a7 a8 a9.....
            //Row0 :                         b0 b1 b2 b3 b4 b5 b6 b7 b8 b9.....
            //b0 is same a8. Similarly other bn pixels are same as a(n+8) pixels.

            do
            {
                src_r0_16x8b = _mm_loadu_si128((__m128i *)(pu1_pred_horiz));             //a0 a1 a2 a3 a4 a5 a6 a7 a8 a9....a15
                src_r1_16x8b = _mm_loadu_si128((__m128i *)(pu1_pred_horiz + 8));         //b0 b1 b2 b3 b4 b5 b6 b7 b8 b9....b15
                src_vpel_16x8b = _mm_loadu_si128((__m128i *)(pu1_tmp2));

                src_r0_sht_16x8b = _mm_srli_si128(src_r0_16x8b, 1);                      //a1 a2 a3 a4 a5 a6 a7 a8 a9....a15 0
                src_r1_sht_16x8b = _mm_srli_si128(src_r1_16x8b, 1);                      //b1 b2 b3 b4 b5 b6 b7 b8 b9....b15 0

                src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);     //a0 a1 a1 a2 a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8
                src_r1_t1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);     //b0 b1 b1 b2 b2 b3 b3 b4 b4 b5 b5 b6 b6 b7 b7 b8

                res_r0_t1_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff0_1_16x8b);    //a0*c0+a1*c1 a1*c0+a2*c1 a2*c0+a3*c1 a3*c0+a4*c1
                                                                                         //a4*c0+a5*c1 a5*c0+a6*c1 a6*c0+a7*c1 a7*c0+a8*c1
                res_r1_t1_8x16b = _mm_maddubs_epi16(src_r1_t1_16x8b, coeff0_1_16x8b);    //b0*c0+b1*c1 b1*c0+b2*c1 b2*c0+b3*c1 b3*c0+b4*c1
                                                                                         //b4*c0+b5*c1 b5*c0+b6*c1 b6*c0+b7*c1 b7*c0+b8*c1

                src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 2);                          //a2 a3 a4 a5 a6 a7 a8 a9....a15 0 0
                src_r1_16x8b = _mm_srli_si128(src_r1_16x8b, 2);                          //b2 b3 b4 b5 b6 b7 b8 b9....b15 0 0

                src_r0_sht_16x8b = _mm_srli_si128(src_r0_sht_16x8b, 2);                  //a3 a4 a5 a6 a7 a8 a9....a15 0  0  0
                src_r1_sht_16x8b = _mm_srli_si128(src_r1_sht_16x8b, 2);                  //b3 b4 b5 b6 b7 b8 b9....b15 0  0  0

                src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);     //a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8 a8 a9 a9 a10
                src_r1_t1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);     //b2 b3 b3 b4 b4 b5 b5 b6 b6 b7 b7 b8 a8 a9 a9 a10

                res_r0_t2_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff2_3_16x8b);    //a2*c2+a3*c3 a3*c2+a4*c3 a4*c2+a5*c3 a5*c2+a6*c3
                                                                                         //a6*c2+a7*c3 a7*c2+a8*c3 a8*c2+a9*c3 a9*c2+a10*c3
                res_r1_t2_8x16b = _mm_maddubs_epi16(src_r1_t1_16x8b, coeff2_3_16x8b);    //b2*c2+b3*c3 b3*c2+b4*c3 b2*c4+b5*c3 b5*c2+b6*c3
                                                                                         //b6*c2+b7*c3 b7*c2+b8*c3 b8*c2+b9*c3 b9*c2+b10*c3

                src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 2);                          //a4 a5 a6 a7 a8 a9....a15 0  0  0  0
                src_r1_16x8b = _mm_srli_si128(src_r1_16x8b, 2);                          //b4 b5 b6 b7 b8 b9....b15 0  0  0  0

                src_r0_sht_16x8b = _mm_srli_si128(src_r0_sht_16x8b, 2);                  //a5 a6 a7 a8 a9....a15 0  0  0  0  0
                src_r1_sht_16x8b = _mm_srli_si128(src_r1_sht_16x8b, 2);                  //b5 b6 b7 b8 b9....b15 0  0  0  0  0

                src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);     //a4 a5 a5 a6 a6 a7 a7 a8 a8 a9 a9 a10 a10 a11 a11 a12
                src_r1_t1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);     //b4 b5 b5 b6 b6 b7 b7 b8 b8 b9 b9 b10 b10 b11 b11 b12

                res_r0_t3_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff4_5_16x8b);    //a4*c4+a5*c5 a5*c4+a6*c5 a6*c4+a7*c5 a7*c4+a8*c5
                                                                                         //a8*c4+a9*c5 a9*c4+a10*c5 a10*c4+a11*c5 a11*c4+a12*c5
                res_r1_t3_8x16b = _mm_maddubs_epi16(src_r1_t1_16x8b, coeff4_5_16x8b);    //b4*c4+b5*c5 b5*c4+b6*c5 b6*c4+b7*c5 b7*c4+b8*c5
                                                                                         //b8*c4+b9*c5 b9*c4+b10*c5 b10*c4+b11*c5 b11*c4+b12*c5
                res_r0_t1_8x16b = _mm_add_epi16(res_r0_t1_8x16b, res_r0_t2_8x16b);
                res_r0_t3_8x16b = _mm_add_epi16(const_val16_8x16b, res_r0_t3_8x16b);
                res_r0_t1_8x16b = _mm_add_epi16(res_r0_t1_8x16b, res_r0_t3_8x16b);
                res_r0_t1_8x16b = _mm_srai_epi16(res_r0_t1_8x16b, 5);                    //shifting right by 5 bits.

                res_r1_t1_8x16b = _mm_add_epi16(res_r1_t1_8x16b, res_r1_t2_8x16b);
                res_r1_t1_8x16b = _mm_add_epi16(res_r1_t1_8x16b, res_r1_t3_8x16b);
                res_r1_t1_8x16b = _mm_add_epi16(res_r1_t1_8x16b, const_val16_8x16b);
                res_r1_t1_8x16b = _mm_srai_epi16(res_r1_t1_8x16b, 5);                    //shifting right by 5 bits.

                res_16x8b = _mm_packus_epi16(res_r0_t1_8x16b, res_r1_t1_8x16b);

                res_16x8b = _mm_avg_epu8(res_16x8b, src_vpel_16x8b);
                _mm_storeu_si128((__m128i *)(pu1_dst), res_16x8b);

                ht --;
                pu1_pred_horiz  += src_strd;
                pu1_dst += dst_strd;
                pu1_tmp2 += 16;
            }
            while(ht > 0);
        }
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_inter_pred_luma_horz_qpel_vert_hpel_ssse3          */
/*                                                                           */
/*  Description   : This function implements a six-tap filter vertically and */
/*                  horizontally on ht x wd block separately and averages    */
/*                  the two sets of values to calculate values at (1/4,1/2), */
/*                  or (3/4, 1/2) as mentioned in sec. 8.4.2.2.1 titled      */
/*                  "Luma sample interpolation process". (ht,wd) can be      */
/*                  (4,4), (8,4), (4,8), (8,8), (16,8), (8,16) or (16,16).   */
/*                                                                           */
/*  Inputs        : puc_src  - pointer to source                             */
/*                  puc_dst  - pointer to destination                        */
/*                  src_strd - stride for source                             */
/*                  dst_strd - stride for destination                        */
/*                  ht       - height of the block                           */
/*                  wd       - width of the block                            */
/*                  pu1_tmp  - pointer to temporary buffer                   */
/*                  dydx     - x and y reference offset for q-pel            */
/*                             calculations                                  */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes                              */
/*         13 02 2015   Kaushik         Initial Version                      */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_inter_pred_luma_horz_qpel_vert_hpel_ssse3(UWORD8 *pu1_src,
                                                     UWORD8 *pu1_dst,
                                                     WORD32 src_strd,
                                                     WORD32 dst_strd,
                                                     WORD32 ht,
                                                     WORD32 wd,
                                                     UWORD8* pu1_tmp,
                                                     WORD32 dydx)
{
    WORD32 ht_temp;
    WORD32 x_offset;
    WORD32 off0,off1, off2, off3, off4, off5;
    WORD16 *pi2_temp1,*pi2_temp2,*pi2_temp3;

    ht_temp = ht;
    x_offset = dydx & 0x3;
    pi2_temp1 = (WORD16 *)pu1_tmp;
    pi2_temp2 = pi2_temp1;
    pi2_temp3 = pi2_temp1 + (x_offset >> 1);

    pu1_src -= 2 * src_strd;
    pu1_src -= 2;
    pi2_temp3 += 2;
    //the filter input starts from x[-2] (till x[3])

    if(wd == 4)
    {
        //vertical half-pel
        {
            __m128i src_r0_16x8b, src_r1_16x8b, src_r2_16x8b, src_r3_16x8b, src_r4_16x8b;
            __m128i src_r5_16x8b, src_r6_16x8b;
            __m128i src_r0r1_16x8b, src_r2r3_16x8b, src_r4r5_16x8b;

            __m128i res_t1_8x16b, res_t2_8x16b, res_t3_8x16b;

            __m128i coeff0_1_16x8b, coeff2_3_16x8b, coeff4_5_16x8b;

            coeff0_1_16x8b = _mm_set1_epi32(0xFB01FB01);  //c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1
            coeff2_3_16x8b = _mm_set1_epi32(0x14141414);  //c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3
            coeff4_5_16x8b = _mm_set1_epi32(0x01FB01FB);  //c4 c5 c5 c5 c4 c5 c5 c5 c4 c5 c5 c5 c4 c5 c5 c5
                                                          //c0 = c5 = 1, c1 = c4 = -5, c2 = c3 = 20
            off0 = -((src_strd << 2) + src_strd) + 8;
            off1 = -(src_strd << 2) + 8;
            off2 = -((src_strd << 1) + src_strd) + 8;
            off3 = -(src_strd << 1) + 8;
            off4 = -src_strd + 8;
            off5 = 8;

            //epilogue: Load all the pred rows except sixth  and seventh row for the
            //first and second row processing.
            src_r0_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src));
            pu1_src =  pu1_src + src_strd;

            src_r1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src));
            pu1_src =  pu1_src + src_strd;

            src_r2_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src));
            pu1_src =  pu1_src + src_strd;

            src_r3_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src));
            pu1_src =  pu1_src + src_strd;

            src_r4_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src));
            pu1_src =  pu1_src + src_strd;

            //Core Loop: Process all the rows.
            do
            {
                src_r5_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src));

                src_r0r1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r1_16x8b);
                src_r2r3_16x8b = _mm_unpacklo_epi8(src_r2_16x8b, src_r3_16x8b);
                src_r4r5_16x8b = _mm_unpacklo_epi8(src_r4_16x8b, src_r5_16x8b);

                res_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);
                res_t2_8x16b = _mm_maddubs_epi16(src_r2r3_16x8b, coeff2_3_16x8b);
                res_t3_8x16b = _mm_maddubs_epi16(src_r4r5_16x8b, coeff4_5_16x8b);

                res_t1_8x16b = _mm_add_epi16(res_t2_8x16b, res_t1_8x16b);
                res_t1_8x16b = _mm_add_epi16(res_t3_8x16b, res_t1_8x16b);

                _mm_storeu_si128((__m128i *)(pi2_temp1), res_t1_8x16b);

                pi2_temp1[8] = pu1_src[off0] + pu1_src[off5]
                                   - (pu1_src[off1] + pu1_src[off4])
                                   + ((pu1_src[off2] + pu1_src[off3] - pu1_src[off1] - pu1_src[off4]) << 2)
                                   + ((pu1_src[off2] + pu1_src[off3]) << 4);

                pu1_src = pu1_src + src_strd;
                pi2_temp1 = pi2_temp1 + 9;

                src_r6_16x8b = _mm_loadl_epi64((__m128i *)(pu1_src));

                src_r0r1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r2_16x8b);
                src_r2r3_16x8b = _mm_unpacklo_epi8(src_r3_16x8b, src_r4_16x8b);
                src_r4r5_16x8b = _mm_unpacklo_epi8(src_r5_16x8b, src_r6_16x8b);

                res_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);
                res_t2_8x16b = _mm_maddubs_epi16(src_r2r3_16x8b, coeff2_3_16x8b);
                res_t3_8x16b = _mm_maddubs_epi16(src_r4r5_16x8b, coeff4_5_16x8b);

                res_t1_8x16b = _mm_add_epi16(res_t2_8x16b, res_t1_8x16b);
                res_t1_8x16b = _mm_add_epi16(res_t3_8x16b, res_t1_8x16b);

                _mm_storeu_si128((__m128i *)(pi2_temp1), res_t1_8x16b);

                pi2_temp1[8] = pu1_src[off0] + pu1_src[off5]
                                   - (pu1_src[off1] + pu1_src[off4])
                                   + ((pu1_src[off2] + pu1_src[off3] - pu1_src[off1] - pu1_src[off4]) << 2)
                                   + ((pu1_src[off2] + pu1_src[off3]) << 4);

                ht_temp -= 2;
                pu1_src = pu1_src + src_strd;
                pi2_temp1 = pi2_temp1 + 9;

                src_r0_16x8b = src_r2_16x8b;
                src_r1_16x8b = src_r3_16x8b;
                src_r2_16x8b = src_r4_16x8b;
                src_r3_16x8b = src_r5_16x8b;
                src_r4_16x8b = src_r6_16x8b;
            }
            while(ht_temp > 0);
        }

        //horizontal q-pel
        {
            __m128i src_r0_8x16b, src_r1_8x16b, src_r2_8x16b;
            __m128i src_r3_8x16b, src_r4_8x16b, src_r5_8x16b;
            __m128i src_r0r1_c0_8x16b, src_r2r3_c0_8x16b, src_r4r5_c0_8x16b;
            __m128i src_hpel_16x8b, src_hpel_8x16b;

            __m128i res_t1_4x32b, res_t2_4x32b, res_t3_4x32b;
            __m128i res_8x16b, res_16x8b;

            __m128i coeff0_1_8x16b, coeff2_3_8x16b, coeff4_5_8x16b;
            __m128i const_val512_4x32b, const_val16_8x16b;

            coeff0_1_8x16b = _mm_set1_epi32(0xFFFB0001);
            coeff2_3_8x16b = _mm_set1_epi32(0x00140014);
            coeff4_5_8x16b = _mm_set1_epi32(0x0001FFFB);

            const_val512_4x32b = _mm_set1_epi32(512);
            const_val16_8x16b = _mm_set1_epi16(16);

            do
            {
                src_r0_8x16b = _mm_loadl_epi64((__m128i *)(pi2_temp2));
                src_r1_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 1));
                src_r2_8x16b = _mm_srli_si128(src_r1_8x16b, 2);
                src_r3_8x16b = _mm_srli_si128(src_r1_8x16b, 4);
                src_r4_8x16b = _mm_srli_si128(src_r1_8x16b, 6);
                src_r5_8x16b = _mm_srli_si128(src_r1_8x16b, 8);

                src_r0r1_c0_8x16b = _mm_unpacklo_epi16(src_r0_8x16b, src_r1_8x16b);
                src_r2r3_c0_8x16b = _mm_unpacklo_epi16(src_r2_8x16b, src_r3_8x16b);
                src_r4r5_c0_8x16b = _mm_unpacklo_epi16(src_r4_8x16b, src_r5_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_c0_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_c0_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_c0_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(const_val512_4x32b, res_t3_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);
                res_t1_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                res_8x16b = _mm_packs_epi32(res_t1_4x32b, res_t1_4x32b);
                res_16x8b = _mm_packus_epi16(res_8x16b, res_8x16b);

                src_hpel_8x16b = _mm_loadl_epi64((__m128i *)(pi2_temp3));
                src_hpel_8x16b = _mm_add_epi16(src_hpel_8x16b, const_val16_8x16b);
                src_hpel_8x16b = _mm_srai_epi16(src_hpel_8x16b, 5); //shifting right by 5 bits.
                src_hpel_16x8b = _mm_packus_epi16(src_hpel_8x16b, src_hpel_8x16b);

                res_16x8b = _mm_avg_epu8(res_16x8b, src_hpel_16x8b);

                *((WORD32 *)(pu1_dst)) = _mm_cvtsi128_si32(res_16x8b);

                ht--;
                pi2_temp2 = pi2_temp2 + 4 + 5;
                pi2_temp3 = pi2_temp3 + 4 + 5;
                pu1_dst = pu1_dst + dst_strd;
            }
            while(ht > 0);
        }
    }
    else if(wd == 8)
    {
        // vertical half-pel
        {
            __m128i src_r0_16x8b, src_r1_16x8b, src_r2_16x8b, src_r3_16x8b, src_r4_16x8b;
            __m128i src_r5_16x8b, src_r6_16x8b;
            __m128i src_r0r1_16x8b, src_r2r3_16x8b, src_r4r5_16x8b;

            __m128i res_t1_8x16b, res_t2_8x16b, res_t3_8x16b;

            __m128i coeff0_1_16x8b, coeff2_3_16x8b, coeff4_5_16x8b;

            coeff0_1_16x8b = _mm_set1_epi32(0xFB01FB01);  //c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1
            coeff2_3_16x8b = _mm_set1_epi32(0x14141414);  //c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3
            coeff4_5_16x8b = _mm_set1_epi32(0x01FB01FB);  //c4 c5 c5 c5 c4 c5 c5 c5 c4 c5 c5 c5 c4 c5 c5 c5

            //epilogue: Load all the pred rows except sixth  and seventh row for the
            //first and second row processing.
            src_r0_16x8b = _mm_loadu_si128((__m128i *)(pu1_src));
            pu1_src =  pu1_src + src_strd;

            src_r1_16x8b = _mm_loadu_si128((__m128i *)(pu1_src));
            pu1_src =  pu1_src + src_strd;

            src_r2_16x8b = _mm_loadu_si128((__m128i *)(pu1_src));
            pu1_src =  pu1_src + src_strd;

            src_r3_16x8b = _mm_loadu_si128((__m128i *)(pu1_src));
            pu1_src =  pu1_src + src_strd;

            src_r4_16x8b = _mm_loadu_si128((__m128i *)(pu1_src));
            pu1_src =  pu1_src + src_strd;

            //Core Loop: Process all the rows.
            do
            {
                src_r5_16x8b = _mm_loadu_si128((__m128i *)(pu1_src));
                src_r6_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + src_strd));

                src_r0r1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r1_16x8b);
                src_r2r3_16x8b = _mm_unpacklo_epi8(src_r2_16x8b, src_r3_16x8b);
                src_r4r5_16x8b = _mm_unpacklo_epi8(src_r4_16x8b, src_r5_16x8b);

                res_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);
                res_t2_8x16b = _mm_maddubs_epi16(src_r2r3_16x8b, coeff2_3_16x8b);
                res_t3_8x16b = _mm_maddubs_epi16(src_r4r5_16x8b, coeff4_5_16x8b);

                res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t2_8x16b);
                res_t1_8x16b = _mm_add_epi16(res_t3_8x16b, res_t1_8x16b);

                _mm_storeu_si128((__m128i *)(pi2_temp1), res_t1_8x16b);

                src_r0r1_16x8b = _mm_unpackhi_epi8(src_r0_16x8b, src_r1_16x8b);
                src_r2r3_16x8b = _mm_unpackhi_epi8(src_r2_16x8b, src_r3_16x8b);
                src_r4r5_16x8b = _mm_unpackhi_epi8(src_r4_16x8b, src_r5_16x8b);

                res_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);
                res_t2_8x16b = _mm_maddubs_epi16(src_r2r3_16x8b, coeff2_3_16x8b);
                res_t3_8x16b = _mm_maddubs_epi16(src_r4r5_16x8b, coeff4_5_16x8b);

                res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t2_8x16b);
                res_t1_8x16b = _mm_add_epi16(res_t3_8x16b, res_t1_8x16b);

                _mm_storeu_si128((__m128i *)(pi2_temp1 + 8), res_t1_8x16b);

                src_r0r1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r2_16x8b);
                src_r2r3_16x8b = _mm_unpacklo_epi8(src_r3_16x8b, src_r4_16x8b);
                src_r4r5_16x8b = _mm_unpacklo_epi8(src_r5_16x8b, src_r6_16x8b);

                res_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);
                res_t2_8x16b = _mm_maddubs_epi16(src_r2r3_16x8b, coeff2_3_16x8b);
                res_t3_8x16b = _mm_maddubs_epi16(src_r4r5_16x8b, coeff4_5_16x8b);

                res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t2_8x16b);
                res_t1_8x16b = _mm_add_epi16(res_t3_8x16b, res_t1_8x16b);

                _mm_storeu_si128((__m128i *)(pi2_temp1 + 8 + 5), res_t1_8x16b);

                src_r0r1_16x8b = _mm_unpackhi_epi8(src_r1_16x8b, src_r2_16x8b);
                src_r2r3_16x8b = _mm_unpackhi_epi8(src_r3_16x8b, src_r4_16x8b);
                src_r4r5_16x8b = _mm_unpackhi_epi8(src_r5_16x8b, src_r6_16x8b);

                res_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);
                res_t2_8x16b = _mm_maddubs_epi16(src_r2r3_16x8b, coeff2_3_16x8b);
                res_t3_8x16b = _mm_maddubs_epi16(src_r4r5_16x8b, coeff4_5_16x8b);

                res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t2_8x16b);
                res_t1_8x16b = _mm_add_epi16(res_t3_8x16b, res_t1_8x16b);

                _mm_storeu_si128((__m128i *)(pi2_temp1 + 8 + 5 + 8), res_t1_8x16b);

                src_r0_16x8b = src_r2_16x8b;
                src_r1_16x8b = src_r3_16x8b;
                src_r2_16x8b = src_r4_16x8b;
                src_r3_16x8b = src_r5_16x8b;
                src_r4_16x8b = src_r6_16x8b;

                ht_temp -= 2;
                pu1_src =  pu1_src + (src_strd << 1);
                pi2_temp1 = pi2_temp1 + (13 << 1);
            }
            while(ht_temp > 0);
        }
        // horizontal q-pel
        {
            __m128i src_r0_8x16b, src_r1_8x16b, src_r2_8x16b, src_r3_8x16b;
            __m128i src_r4_8x16b, src_r5_8x16b;
            __m128i src_r0r1_c0_8x16b, src_r2r3_c0_8x16b, src_r4r5_c0_8x16b;
            __m128i src_r0r1_c1_8x16b, src_r2r3_c1_8x16b, src_r4r5_c1_8x16b;
            __m128i src_hpel_8x16b, src_hpel_16x8b;

            __m128i res_t0_4x32b, res_t1_4x32b, res_t2_4x32b, res_t3_4x32b;
            __m128i res_8x16b, res_16x8b;

            __m128i coeff0_1_8x16b, coeff2_3_8x16b, coeff4_5_8x16b;
            __m128i const_val512_4x32b, const_val16_8x16b;

            coeff0_1_8x16b = _mm_set1_epi32(0xFFFB0001);
            coeff2_3_8x16b = _mm_set1_epi32(0x00140014);
            coeff4_5_8x16b = _mm_set1_epi32(0x0001FFFB);

            const_val512_4x32b = _mm_set1_epi32(512);
            const_val16_8x16b = _mm_set1_epi16(16);

            do
            {
                src_r0_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2));
                src_r1_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 1));
                src_r2_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 2));
                src_r3_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 3));
                src_r4_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 4));
                src_r5_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 5));

                src_r0r1_c0_8x16b = _mm_unpacklo_epi16(src_r0_8x16b, src_r1_8x16b);
                src_r2r3_c0_8x16b = _mm_unpacklo_epi16(src_r2_8x16b, src_r3_8x16b);
                src_r4r5_c0_8x16b = _mm_unpacklo_epi16(src_r4_8x16b, src_r5_8x16b);

                src_r0r1_c1_8x16b = _mm_unpackhi_epi16(src_r0_8x16b, src_r1_8x16b);
                src_r2r3_c1_8x16b = _mm_unpackhi_epi16(src_r2_8x16b, src_r3_8x16b);
                src_r4r5_c1_8x16b = _mm_unpackhi_epi16(src_r4_8x16b, src_r5_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_c0_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_c0_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_c0_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(res_t3_4x32b, const_val512_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);

                res_t0_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_c1_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_c1_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_c1_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(res_t3_4x32b, const_val512_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);

                res_t1_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                res_8x16b = _mm_packs_epi32(res_t0_4x32b, res_t1_4x32b);
                res_16x8b = _mm_packus_epi16(res_8x16b, res_8x16b);

                src_hpel_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp3));
                src_hpel_8x16b = _mm_add_epi16(src_hpel_8x16b, const_val16_8x16b);
                src_hpel_8x16b = _mm_srai_epi16(src_hpel_8x16b, 5); //shifting right by 5 bits.
                src_hpel_16x8b = _mm_packus_epi16(src_hpel_8x16b, src_hpel_8x16b);

                res_16x8b = _mm_avg_epu8(res_16x8b, src_hpel_16x8b);

                _mm_storel_epi64((__m128i *)(pu1_dst), res_16x8b);

                ht--;
                pi2_temp2 = pi2_temp2 + 8 + 5;
                pi2_temp3 = pi2_temp3 + 8 + 5;
                pu1_dst = pu1_dst + dst_strd;
            }
            while(ht > 0);
        }
    }
    else // wd == 16
    {
        // vertical half-pel
        {
            __m128i src_r0_16x8b, src_r1_16x8b, src_r2_16x8b, src_r3_16x8b;
            __m128i src_r4_16x8b, src_r5_16x8b;
            __m128i src_r0_c2_16x8b, src_r1_c2_16x8b, src_r2_c2_16x8b, src_r3_c2_16x8b;
            __m128i src_r4_c2_16x8b, src_r5_c2_16x8b;
            __m128i src_r0r1_16x8b, src_r2r3_16x8b, src_r4r5_16x8b;

            __m128i res_t1_8x16b, res_t2_8x16b, res_t3_8x16b;

            __m128i coeff0_1_16x8b,coeff2_3_16x8b,coeff4_5_16x8b;

            coeff0_1_16x8b = _mm_set1_epi32(0xFB01FB01);  //c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1
            coeff2_3_16x8b = _mm_set1_epi32(0x14141414);  //c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3
            coeff4_5_16x8b = _mm_set1_epi32(0x01FB01FB);  //c4 c5 c5 c5 c4 c5 c5 c5 c4 c5 c5 c5 c4 c5 c5 c5

            src_r0_16x8b = _mm_loadu_si128((__m128i *)(pu1_src));
            src_r0_c2_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + 16));
            pu1_src =  pu1_src + src_strd;
            src_r1_16x8b = _mm_loadu_si128((__m128i *)(pu1_src));
            src_r1_c2_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + 16));
            pu1_src =  pu1_src + src_strd;
            src_r2_16x8b = _mm_loadu_si128((__m128i *)(pu1_src));
            src_r2_c2_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + 16));
            pu1_src =  pu1_src + src_strd;
            src_r3_16x8b = _mm_loadu_si128((__m128i *)(pu1_src));
            src_r3_c2_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + 16));
            pu1_src =  pu1_src + src_strd;
            src_r4_16x8b = _mm_loadu_si128((__m128i *)(pu1_src));
            src_r4_c2_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + 16));
            pu1_src =  pu1_src + src_strd;

            //Core Loop: Process all the rows.
            do
            {
                src_r5_16x8b  = _mm_loadu_si128((__m128i *)(pu1_src));
                src_r5_c2_16x8b  = _mm_loadu_si128((__m128i *)(pu1_src + 16));

                src_r0r1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r1_16x8b);
                src_r2r3_16x8b = _mm_unpacklo_epi8(src_r2_16x8b, src_r3_16x8b);
                src_r4r5_16x8b = _mm_unpacklo_epi8(src_r4_16x8b, src_r5_16x8b);

                res_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);
                res_t2_8x16b = _mm_maddubs_epi16(src_r2r3_16x8b, coeff2_3_16x8b);
                res_t3_8x16b = _mm_maddubs_epi16(src_r4r5_16x8b, coeff4_5_16x8b);

                res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t2_8x16b);
                res_t1_8x16b = _mm_add_epi16(res_t3_8x16b, res_t1_8x16b);

                _mm_storeu_si128((__m128i *)(pi2_temp1), res_t1_8x16b);

                src_r0r1_16x8b = _mm_unpackhi_epi8(src_r0_16x8b, src_r1_16x8b);
                src_r2r3_16x8b = _mm_unpackhi_epi8(src_r2_16x8b, src_r3_16x8b);
                src_r4r5_16x8b = _mm_unpackhi_epi8(src_r4_16x8b, src_r5_16x8b);

                res_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);
                res_t2_8x16b = _mm_maddubs_epi16(src_r2r3_16x8b, coeff2_3_16x8b);
                res_t3_8x16b = _mm_maddubs_epi16(src_r4r5_16x8b, coeff4_5_16x8b);

                res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t2_8x16b);
                res_t1_8x16b = _mm_add_epi16(res_t3_8x16b, res_t1_8x16b);

                _mm_storeu_si128((__m128i *)(pi2_temp1 + 8), res_t1_8x16b);

                src_r0r1_16x8b = _mm_unpacklo_epi8(src_r0_c2_16x8b, src_r1_c2_16x8b);
                src_r2r3_16x8b = _mm_unpacklo_epi8(src_r2_c2_16x8b, src_r3_c2_16x8b);
                src_r4r5_16x8b = _mm_unpacklo_epi8(src_r4_c2_16x8b, src_r5_c2_16x8b);

                res_t1_8x16b = _mm_maddubs_epi16(src_r0r1_16x8b, coeff0_1_16x8b);
                res_t2_8x16b = _mm_maddubs_epi16(src_r2r3_16x8b, coeff2_3_16x8b);
                res_t3_8x16b = _mm_maddubs_epi16(src_r4r5_16x8b, coeff4_5_16x8b);

                res_t1_8x16b = _mm_add_epi16(res_t1_8x16b, res_t2_8x16b);
                res_t1_8x16b = _mm_add_epi16(res_t3_8x16b, res_t1_8x16b);

                _mm_storeu_si128((__m128i *)(pi2_temp1 + 16), res_t1_8x16b);

                src_r0_16x8b = src_r1_16x8b;
                src_r1_16x8b = src_r2_16x8b;
                src_r2_16x8b = src_r3_16x8b;
                src_r3_16x8b = src_r4_16x8b;
                src_r4_16x8b = src_r5_16x8b;

                src_r0_c2_16x8b = src_r1_c2_16x8b;
                src_r1_c2_16x8b = src_r2_c2_16x8b;
                src_r2_c2_16x8b = src_r3_c2_16x8b;
                src_r3_c2_16x8b = src_r4_c2_16x8b;
                src_r4_c2_16x8b = src_r5_c2_16x8b;

                ht_temp--;
                pu1_src =  pu1_src + src_strd;
                pi2_temp1 =  pi2_temp1 + 16 + 5;
            }
            while(ht_temp > 0);
        }
        // horizontal q-pel
        {
            __m128i src_r0_8x16b, src_r1_8x16b, src_r2_8x16b, src_r3_8x16b;
            __m128i src_r4_8x16b, src_r5_8x16b;
            __m128i src_r0r1_8x16b, src_r2r3_8x16b, src_r4r5_8x16b;
            __m128i src_hpel1_8x16b, src_hpel2_8x16b, src_hpel_16x8b;

            __m128i res_t0_4x32b, res_t1_4x32b, res_t2_4x32b, res_t3_4x32b;
            __m128i res_c0_8x16b, res_c1_8x16b, res_16x8b;

            __m128i coeff0_1_8x16b, coeff2_3_8x16b, coeff4_5_8x16b;
            __m128i const_val512_4x32b, const_val16_8x16b;

            coeff0_1_8x16b = _mm_set1_epi32(0xFFFB0001);
            coeff2_3_8x16b = _mm_set1_epi32(0x00140014);
            coeff4_5_8x16b = _mm_set1_epi32(0x0001FFFB);

            const_val512_4x32b = _mm_set1_epi32(512);
            const_val16_8x16b = _mm_set1_epi16(16);

            do
            {
                src_r0_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2));
                src_r1_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 1));
                src_r2_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 2));
                src_r3_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 3));
                src_r4_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 4));
                src_r5_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 5));

                src_r0r1_8x16b = _mm_unpacklo_epi16(src_r0_8x16b, src_r1_8x16b);
                src_r2r3_8x16b = _mm_unpacklo_epi16(src_r2_8x16b, src_r3_8x16b);
                src_r4r5_8x16b = _mm_unpacklo_epi16(src_r4_8x16b, src_r5_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(res_t3_4x32b, const_val512_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);
                res_t0_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                src_r0r1_8x16b = _mm_unpackhi_epi16(src_r0_8x16b, src_r1_8x16b);
                src_r2r3_8x16b = _mm_unpackhi_epi16(src_r2_8x16b, src_r3_8x16b);
                src_r4r5_8x16b = _mm_unpackhi_epi16(src_r4_8x16b, src_r5_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(const_val512_4x32b, res_t3_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);
                res_t1_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                res_c0_8x16b = _mm_packs_epi32(res_t0_4x32b, res_t1_4x32b);

                src_r0_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 8));
                src_r1_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 8 + 1));
                src_r2_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 8 + 2));
                src_r3_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 8 + 3));
                src_r4_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 8 + 4));
                src_r5_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 8 + 5));

                src_r0r1_8x16b = _mm_unpacklo_epi16(src_r0_8x16b, src_r1_8x16b);
                src_r2r3_8x16b = _mm_unpacklo_epi16(src_r2_8x16b, src_r3_8x16b);
                src_r4r5_8x16b = _mm_unpacklo_epi16(src_r4_8x16b, src_r5_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(const_val512_4x32b, res_t3_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);
                res_t0_4x32b = _mm_srai_epi32(res_t1_4x32b ,10);

                src_r0r1_8x16b = _mm_unpackhi_epi16(src_r0_8x16b, src_r1_8x16b);
                src_r2r3_8x16b = _mm_unpackhi_epi16(src_r2_8x16b, src_r3_8x16b);
                src_r4r5_8x16b = _mm_unpackhi_epi16(src_r4_8x16b, src_r5_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(const_val512_4x32b, res_t3_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);
                res_t1_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                res_c1_8x16b = _mm_packs_epi32(res_t0_4x32b, res_t1_4x32b);
                res_16x8b = _mm_packus_epi16(res_c0_8x16b, res_c1_8x16b);

                src_hpel1_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp3));
                src_hpel1_8x16b = _mm_add_epi16(src_hpel1_8x16b, const_val16_8x16b);
                src_hpel1_8x16b = _mm_srai_epi16(src_hpel1_8x16b, 5); //shifting right by 5 bits.

                src_hpel2_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp3 + 8));
                src_hpel2_8x16b = _mm_add_epi16(src_hpel2_8x16b, const_val16_8x16b);
                src_hpel2_8x16b = _mm_srai_epi16(src_hpel2_8x16b, 5); //shifting right by 5 bits.

                src_hpel_16x8b = _mm_packus_epi16(src_hpel1_8x16b, src_hpel2_8x16b);
                res_16x8b = _mm_avg_epu8(res_16x8b, src_hpel_16x8b);

                _mm_storeu_si128((__m128i *)(pu1_dst), res_16x8b);

                ht--;
                pi2_temp2 = pi2_temp2 + 16 + 5;
                pi2_temp3 = pi2_temp3 + 16 + 5;
                pu1_dst = pu1_dst + dst_strd;
            }
            while(ht > 0);
        }
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_inter_pred_luma_horz_hpel_vert_qpel_ssse3          */
/*                                                                           */
/*  Description   : This function implements a six-tap filter vertically and */
/*                  horizontally on ht x wd block separately and averages    */
/*                  the two sets of values to calculate values at (1/2,1/4), */
/*                  or (1/2, 3/4) as mentioned in sec. 8.4.2.2.1 titled      */
/*                  "Luma sample interpolation process". (ht,wd) can be      */
/*                  (4,4), (8,4), (4,8), (8,8), (16,8), (8,16) or (16,16).   */
/*                                                                           */
/*  Inputs        : puc_src  - pointer to source                             */
/*                  puc_dst  - pointer to destination                        */
/*                  src_strd - stride for source                             */
/*                  dst_strd - stride for destination                        */
/*                  ht       - height of the block                           */
/*                  wd       - width of the block                            */
/*                  pu1_tmp  - pointer to temporary buffer                   */
/*                  dydx     - x and y reference offset for q-pel            */
/*                             calculations                                  */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes                              */
/*         13 02 2015   Kaushik         Initial Version                      */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_inter_pred_luma_horz_hpel_vert_qpel_ssse3(UWORD8 *pu1_src,
                                                     UWORD8 *pu1_dst,
                                                     WORD32 src_strd,
                                                     WORD32 dst_strd,
                                                     WORD32 ht,
                                                     WORD32 wd,
                                                     UWORD8* pu1_tmp,
                                                     WORD32 dydx)
{
    WORD32 ht_temp;
    WORD32 y_offset;
    WORD16 *pi2_temp1,*pi2_temp2,*pi2_temp3;

    y_offset = (dydx & 0xf) >> 2;
    pi2_temp1 = (WORD16 *)pu1_tmp;
    pi2_temp2 = pi2_temp1;
    pi2_temp3 = pi2_temp1 + (y_offset >> 1) * wd;

    ht_temp = ht + 5;
    pu1_src -= src_strd << 1;
    pu1_src -= 2;
    pi2_temp3 += wd << 1;
    //the filter input starts from x[-2] (till x[3])

    if(wd == 4)
    {
        // horizontal half-pel
        {
            __m128i src_r0_16x8b, src_r1_16x8b, src_r0r1_t1_16x8b;
            __m128i src_r0_sht_16x8b, src_r1_sht_16x8b;
            __m128i res_r0r1_t1_8x16b, res_r0r1_t2_8x16b, res_r0r1_t3_8x16b;
            __m128i coeff0_1_16x8b, coeff2_3_16x8b, coeff4_5_16x8b;

            coeff0_1_16x8b = _mm_set1_epi32(0xFB01FB01);  //c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1
            coeff2_3_16x8b = _mm_set1_epi32(0x14141414);  //c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3
            coeff4_5_16x8b = _mm_set1_epi32(0x01FB01FB);  //c4 c5 c5 c5 c4 c5 c5 c5 c4 c5 c5 c5 c4 c5 c5 c5

            //Row0 : a0 a1 a2 a3 a4 a5 a6 a7 a8 a9.....
            //Row1 : b0 b1 b2 b3 b4 b5 b6 b7 b8 b9.....

            do
            {
                src_r0_16x8b = _mm_loadu_si128((__m128i *)pu1_src);                         //a0 a1 a2 a3 a4 a5 a6 a7 a8 a9....a15
                src_r1_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + src_strd));            //b0 b1 b2 b3 b4 b5 b6 b7 b8 b9....b15

                src_r0_sht_16x8b = _mm_srli_si128(src_r0_16x8b, 1);                         //a1 a2 a3 a4 a5 a6 a7 a8 a9....a15 0
                src_r1_sht_16x8b = _mm_srli_si128(src_r1_16x8b, 1);                         //b1 b2 b3 b4 b5 b6 b7 b8 b9....b15 0

                src_r0_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);           //a0 a1 a1 a2 a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8
                src_r1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);           //b0 b1 b1 b2 b2 b3 b3 b4 b4 b5 b5 b6 b6 b7 b7 b8

                src_r0r1_t1_16x8b = _mm_unpacklo_epi64(src_r0_16x8b, src_r1_16x8b);         //a0 a1 a1 a2 a2 a3 a3 a4 b0 b1 b1 b2 b2 b3 b3 b4
                res_r0r1_t1_8x16b = _mm_maddubs_epi16(src_r0r1_t1_16x8b, coeff0_1_16x8b);   //a0*c0+a1*c1 a1*c0+a2*c1 a2*c0+a3*c1 a3*c0+a4*c1
                                                                                            //b0*c0+b1*c1 b1*c0+b2*c1 b2*c0+b3*c1 b3*c0+b4*c1

                src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 4);                             //a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8  0  0  0  0
                src_r1_16x8b = _mm_srli_si128(src_r1_16x8b, 4);                             //b2 b3 b3 b4 b4 b5 b5 b6 b6 b7 b7 b8  0  0  0  0

                src_r0r1_t1_16x8b = _mm_unpacklo_epi64(src_r0_16x8b, src_r1_16x8b);         //a2 a3 a3 a4 a4 a5 a5 a6 b2 b3 b3 b4 b4 b5 b5 b6
                res_r0r1_t2_8x16b = _mm_maddubs_epi16(src_r0r1_t1_16x8b, coeff2_3_16x8b);   //a2*c2+a3*c3 a3*c2+a4*c3 a4*c2+a5*c3 a5*c2+a6*c3
                                                                                            //b2*c2+b3*c3 b3*c2+b4*c3 b4*c2+b5*c3 b5*c2+b6*c3

                src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 4);                             //a4 a5 a5 a6 a6 a7 a7 a8  0  0  0  0  0  0  0  0
                src_r1_16x8b = _mm_srli_si128(src_r1_16x8b, 4);                             //b4 b5 b5 b6 b6 b7 b7 b8  0  0  0  0  0  0  0  0

                src_r0r1_t1_16x8b = _mm_unpacklo_epi64(src_r0_16x8b, src_r1_16x8b);         //a4 a5 a5 a6 a6 a7 a7 a8 b4 b5 b5 b6 b6 b7 b7 b8
                res_r0r1_t3_8x16b = _mm_maddubs_epi16(src_r0r1_t1_16x8b, coeff4_5_16x8b);   //a4*c4+a5*c5 a5*c4+a6*c5 a6*c4+a7*c5 a7*c4+a8*c5
                                                                                            //b4*c4+b5*c5 b5*c4+b6*c5 b4*c6+b7*c5 b7*c4+b8*c5

                res_r0r1_t1_8x16b = _mm_add_epi16(res_r0r1_t1_8x16b, res_r0r1_t2_8x16b);
                res_r0r1_t1_8x16b = _mm_add_epi16(res_r0r1_t1_8x16b, res_r0r1_t3_8x16b);


                _mm_storeu_si128((__m128i *)(pi2_temp1), res_r0r1_t1_8x16b);

                ht_temp -= 2;
                pu1_src =  pu1_src + (src_strd << 1);
                pi2_temp1 =  pi2_temp1 + (4 << 1);
            }
            while(ht_temp > 0);
        }
        // vertical q-pel
        {
            __m128i src_r0_8x16b, src_r1_8x16b, src_r2_8x16b, src_r3_8x16b;
            __m128i src_r4_8x16b, src_r5_8x16b, src_r6_8x16b;
            __m128i src_r0r1_c0_8x16b, src_r2r3_c0_8x16b, src_r4r5_c0_8x16b;
            __m128i src_hpel_16x8b, src_hpel_8x16b;

            __m128i res_t0_4x32b, res_t1_4x32b, res_t2_4x32b, res_t3_4x32b;
            __m128i res_8x16b, res_16x8b;

            __m128i coeff0_1_8x16b, coeff2_3_8x16b, coeff4_5_8x16b;
            __m128i const_val512_4x32b, const_val16_8x16b;

            const_val512_4x32b = _mm_set1_epi32(512);
            const_val16_8x16b = _mm_set1_epi16(16);

            coeff0_1_8x16b = _mm_set1_epi32(0xFFFB0001);
            coeff2_3_8x16b = _mm_set1_epi32(0x00140014);
            coeff4_5_8x16b = _mm_set1_epi32(0x0001FFFB);

            src_r0_8x16b = _mm_loadl_epi64((__m128i *)(pi2_temp2));
            src_r1_8x16b = _mm_loadl_epi64((__m128i *)(pi2_temp2 + 4));
            src_r2_8x16b = _mm_loadl_epi64((__m128i *)(pi2_temp2 + 8));
            src_r3_8x16b = _mm_loadl_epi64((__m128i *)(pi2_temp2 + 12));
            src_r4_8x16b = _mm_loadl_epi64((__m128i *)(pi2_temp2 + 16));
            pi2_temp2 += 20;

            do
            {
                src_r5_8x16b = _mm_loadl_epi64((__m128i *)(pi2_temp2));
                src_r6_8x16b = _mm_loadl_epi64((__m128i *)(pi2_temp2 + 4));

                src_r0r1_c0_8x16b = _mm_unpacklo_epi16(src_r0_8x16b, src_r1_8x16b);
                src_r2r3_c0_8x16b = _mm_unpacklo_epi16(src_r2_8x16b, src_r3_8x16b);
                src_r4r5_c0_8x16b = _mm_unpacklo_epi16(src_r4_8x16b, src_r5_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_c0_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_c0_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_c0_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(res_t3_4x32b, const_val512_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);
                res_t0_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                src_r0r1_c0_8x16b = _mm_unpacklo_epi16(src_r1_8x16b, src_r2_8x16b);
                src_r2r3_c0_8x16b = _mm_unpacklo_epi16(src_r3_8x16b, src_r4_8x16b);
                src_r4r5_c0_8x16b = _mm_unpacklo_epi16(src_r5_8x16b, src_r6_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_c0_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_c0_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_c0_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(res_t3_4x32b, const_val512_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);
                res_t1_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                res_8x16b = _mm_packs_epi32(res_t0_4x32b, res_t1_4x32b);
                res_16x8b = _mm_packus_epi16(res_8x16b, res_8x16b);

                src_hpel_8x16b = _mm_loadu_si128((__m128i *)pi2_temp3);
                src_hpel_8x16b = _mm_add_epi16(src_hpel_8x16b, const_val16_8x16b);
                src_hpel_8x16b = _mm_srai_epi16(src_hpel_8x16b, 5); //shifting right by 5 bits.
                src_hpel_16x8b = _mm_packus_epi16(src_hpel_8x16b, src_hpel_8x16b);

                res_16x8b = _mm_avg_epu8(res_16x8b, src_hpel_16x8b);

                *((WORD32 *)(pu1_dst)) = _mm_cvtsi128_si32(res_16x8b);
                res_16x8b = _mm_srli_si128(res_16x8b, 4);
                *((WORD32 *)(pu1_dst + dst_strd)) = _mm_cvtsi128_si32(res_16x8b);

                src_r0_8x16b = src_r2_8x16b;
                src_r1_8x16b = src_r3_8x16b;
                src_r2_8x16b = src_r4_8x16b;
                src_r3_8x16b = src_r5_8x16b;
                src_r4_8x16b = src_r6_8x16b;

                ht -= 2;
                pi2_temp2 =  pi2_temp2 + (4 << 1);
                pi2_temp3 =  pi2_temp3 + (4 << 1);
                pu1_dst = pu1_dst + (dst_strd << 1);
            }
            while(ht > 0);
        }
    }
    else if(wd == 8)
    {
        // horizontal half-pel
        {
            __m128i src_r0_16x8b, src_r1_16x8b, src_r0_sht_16x8b, src_r1_sht_16x8b;
            __m128i src_r0_t1_16x8b, src_r1_t1_16x8b;

            __m128i res_r0_t1_8x16b, res_r0_t2_8x16b, res_r0_t3_8x16b;
            __m128i res_r1_t1_8x16b, res_r1_t2_8x16b, res_r1_t3_8x16b;

            __m128i coeff0_1_16x8b, coeff2_3_16x8b, coeff4_5_16x8b;

            coeff0_1_16x8b = _mm_set1_epi32(0xFB01FB01);  //c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1
            coeff2_3_16x8b = _mm_set1_epi32(0x14141414);  //c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3
            coeff4_5_16x8b = _mm_set1_epi32(0x01FB01FB);  //c4 c5 c5 c5 c4 c5 c5 c5 c4 c5 c5 c5 c4 c5 c5 c5

            //Row0 : a0 a1 a2 a3 a4 a5 a6 a7 a8 a9.....
            //Row1 : b0 b1 b2 b3 b4 b5 b6 b7 b8 b9.....

            do
            {
                src_r0_16x8b = _mm_loadu_si128((__m128i *)(pu1_src));                   //a0 a1 a2 a3 a4 a5 a6 a7 a8 a9....a15
                src_r1_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + src_strd));        //b0 b1 b2 b3 b4 b5 b6 b7 b8 b9....b15

                src_r0_sht_16x8b = _mm_srli_si128(src_r0_16x8b, 1);                     //a1 a2 a3 a4 a5 a6 a7 a8 a9....a15 0
                src_r1_sht_16x8b = _mm_srli_si128(src_r1_16x8b, 1);                     //b1 b2 b3 b4 b5 b6 b7 b8 b9....b15 0

                src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);    //a0 a1 a1 a2 a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8
                src_r1_t1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);    //b0 b1 b1 b2 b2 b3 b3 b4 b4 b5 b5 b6 b6 b7 b7 b8

                res_r0_t1_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff0_1_16x8b);   //a0*c0+a1*c1 a1*c0+a2*c1 a2*c0+a3*c1 a3*c0+a4*c1
                                                                                        //a4*c0+a5*c1 a5*c0+a6*c1 a6*c0+a7*c1 a7*c0+a8*c1
                res_r1_t1_8x16b = _mm_maddubs_epi16(src_r1_t1_16x8b, coeff0_1_16x8b);   //b0*c0+b1*c1 b1*c0+b2*c1 b2*c0+b3*c1 b3*c0+b4*c1
                                                                                        //b4*c0+b5*c1 b5*c0+b6*c1 b6*c0+b7*c1 b7*c0+b8*c1

                src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 2);                         //a2 a3 a4 a5 a6 a7 a8 a9....a15 0 0
                src_r1_16x8b = _mm_srli_si128(src_r1_16x8b, 2);                         //b2 b3 b4 b5 b6 b7 b8 b9....b15 0 0

                src_r0_sht_16x8b = _mm_srli_si128(src_r0_sht_16x8b, 2);                 //a3 a4 a5 a6 a7 a8 a9....a15 0  0  0
                src_r1_sht_16x8b = _mm_srli_si128(src_r1_sht_16x8b, 2);                 //b3 b4 b5 b6 b7 b8 b9....b15 0  0  0

                src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);    //a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8 a8 a9 a9 a10
                src_r1_t1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);    //b2 b3 b3 b4 b4 b5 b5 b6 b6 b7 b7 b8 a8 a9 a9 a10

                res_r0_t2_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff2_3_16x8b);   //a2*c2+a3*c3 a3*c2+a4*c3 a4*c2+a5*c3 a5*c2+a6*c3
                                                                                        //a6*c2+a7*c3 a7*c2+a8*c3 a8*c2+a9*c3 a9*c2+a10*c3
                res_r1_t2_8x16b = _mm_maddubs_epi16(src_r1_t1_16x8b, coeff2_3_16x8b);   //b2*c2+b3*c3 b3*c2+b4*c3 b2*c4+b5*c3 b5*c2+b6*c3
                                                                                        //b6*c2+b7*c3 b7*c2+b8*c3 b8*c2+b9*c3 b9*c2+b10*c3

                src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 2);                         //a4 a5 a6 a7 a8 a9....a15 0  0  0  0
                src_r1_16x8b = _mm_srli_si128(src_r1_16x8b, 2);                         //b4 b5 b6 b7 b8 b9....b15 0  0  0  0

                src_r0_sht_16x8b = _mm_srli_si128(src_r0_sht_16x8b, 2);                 //a5 a6 a7 a8 a9....a15 0  0  0  0  0
                src_r1_sht_16x8b = _mm_srli_si128(src_r1_sht_16x8b, 2);                 //b5 b6 b7 b8 b9....b15 0  0  0  0  0

                src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);    //a4 a5 a5 a6 a6 a7 a7 a8 a8 a9 a9 a10 a10 a11 a11 a12
                src_r1_t1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);    //b4 b5 b5 b6 b6 b7 b7 b8 b8 b9 b9 b10 b10 b11 b11 b12

                res_r0_t3_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff4_5_16x8b);   //a4*c4+a5*c5 a5*c4+a6*c5 a6*c4+a7*c5 a7*c4+a8*c5
                                                                                        //a8*c4+a9*c5 a9*c4+a10*c5 a10*c4+a11*c5 a11*c4+a12*c5
                res_r1_t3_8x16b = _mm_maddubs_epi16(src_r1_t1_16x8b, coeff4_5_16x8b);   //b4*c4+b5*c5 b5*c4+b6*c5 b6*c4+b7*c5 b7*c4+b8*c5
                                                                                        //b8*c4+b9*c5 b9*c4+b10*c5 b10*c4+b11*c5 b11*c4+b12*c5
                res_r0_t1_8x16b = _mm_add_epi16(res_r0_t1_8x16b, res_r0_t2_8x16b);
                res_r0_t1_8x16b = _mm_add_epi16(res_r0_t1_8x16b, res_r0_t3_8x16b);

                res_r1_t1_8x16b = _mm_add_epi16(res_r1_t1_8x16b, res_r1_t2_8x16b);
                res_r1_t1_8x16b = _mm_add_epi16(res_r1_t1_8x16b, res_r1_t3_8x16b);

                _mm_storeu_si128((__m128i *)(pi2_temp1), res_r0_t1_8x16b);
                _mm_storeu_si128((__m128i *)(pi2_temp1 + 8), res_r1_t1_8x16b);

                ht_temp -= 2;
                pu1_src =  pu1_src + (src_strd << 1);
                pi2_temp1 =  pi2_temp1 + (8 << 1);
            }
            while(ht_temp > 0);
        }
        // vertical q-pel
        {
            __m128i src_r0_8x16b, src_r1_8x16b, src_r2_8x16b, src_r3_8x16b;
            __m128i src_r4_8x16b, src_r5_8x16b, src_r6_8x16b;
            __m128i src_r0r1_8x16b, src_r2r3_8x16b, src_r4r5_8x16b;
            __m128i src_hpel_8x16b, src_hpel_16x8b;

            __m128i res_t0_4x32b, res_t1_4x32b, res_t2_4x32b, res_t3_4x32b;
            __m128i res_8x16b, res_16x8b;

            __m128i coeff0_1_8x16b, coeff2_3_8x16b, coeff4_5_8x16b;
            __m128i const_val512_4x32b, const_val16_8x16b;

            coeff0_1_8x16b = _mm_set1_epi32(0xFFFB0001);
            coeff2_3_8x16b = _mm_set1_epi32(0x00140014);
            coeff4_5_8x16b = _mm_set1_epi32(0x0001FFFB);

            const_val512_4x32b = _mm_set1_epi32(512);
            const_val16_8x16b = _mm_set1_epi16(16);

            src_r0_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2));
            src_r1_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 8));
            src_r2_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 16));
            src_r3_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 24));
            src_r4_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 32));
            pi2_temp2 += 40;

            do
            {
                src_r5_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2));
                src_r6_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 8));

                src_r0r1_8x16b = _mm_unpacklo_epi16(src_r0_8x16b, src_r1_8x16b);
                src_r2r3_8x16b = _mm_unpacklo_epi16(src_r2_8x16b, src_r3_8x16b);
                src_r4r5_8x16b = _mm_unpacklo_epi16(src_r4_8x16b, src_r5_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(res_t3_4x32b, const_val512_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);
                res_t0_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                src_r0r1_8x16b = _mm_unpackhi_epi16(src_r0_8x16b, src_r1_8x16b);
                src_r2r3_8x16b = _mm_unpackhi_epi16(src_r2_8x16b, src_r3_8x16b);
                src_r4r5_8x16b = _mm_unpackhi_epi16(src_r4_8x16b, src_r5_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(res_t3_4x32b, const_val512_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);
                res_t1_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                res_8x16b = _mm_packs_epi32(res_t0_4x32b, res_t1_4x32b);
                res_16x8b = _mm_packus_epi16(res_8x16b, res_8x16b);

                src_hpel_8x16b = _mm_loadu_si128((__m128i *)pi2_temp3);
                src_hpel_8x16b = _mm_add_epi16(const_val16_8x16b, src_hpel_8x16b);
                src_hpel_8x16b = _mm_srai_epi16(src_hpel_8x16b, 5); //shifting right by 5 bits.
                src_hpel_16x8b = _mm_packus_epi16(src_hpel_8x16b, src_hpel_8x16b);

                res_16x8b = _mm_avg_epu8(res_16x8b, src_hpel_16x8b);

                _mm_storel_epi64((__m128i *)(pu1_dst), res_16x8b);

                src_r0r1_8x16b = _mm_unpacklo_epi16(src_r1_8x16b, src_r2_8x16b);
                src_r2r3_8x16b = _mm_unpacklo_epi16(src_r3_8x16b, src_r4_8x16b);
                src_r4r5_8x16b = _mm_unpacklo_epi16(src_r5_8x16b, src_r6_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(res_t3_4x32b, const_val512_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);
                res_t0_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                src_r0r1_8x16b = _mm_unpackhi_epi16(src_r1_8x16b, src_r2_8x16b);
                src_r2r3_8x16b = _mm_unpackhi_epi16(src_r3_8x16b, src_r4_8x16b);
                src_r4r5_8x16b = _mm_unpackhi_epi16(src_r5_8x16b, src_r6_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(res_t3_4x32b, const_val512_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);
                res_t1_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                res_8x16b = _mm_packs_epi32(res_t0_4x32b, res_t1_4x32b);
                res_16x8b = _mm_packus_epi16(res_8x16b, res_8x16b);

                src_hpel_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp3 + 8));
                src_hpel_8x16b = _mm_add_epi16(const_val16_8x16b, src_hpel_8x16b);
                src_hpel_8x16b = _mm_srai_epi16(src_hpel_8x16b, 5); //shifting right by 5 bits.
                src_hpel_16x8b = _mm_packus_epi16(src_hpel_8x16b, src_hpel_8x16b);

                res_16x8b = _mm_avg_epu8(res_16x8b, src_hpel_16x8b);

                _mm_storel_epi64((__m128i *)(pu1_dst + dst_strd), res_16x8b);

                src_r0_8x16b = src_r2_8x16b;
                src_r1_8x16b = src_r3_8x16b;
                src_r2_8x16b = src_r4_8x16b;
                src_r3_8x16b = src_r5_8x16b;
                src_r4_8x16b = src_r6_8x16b;

                ht -= 2;
                pi2_temp2 = pi2_temp2 + (8 << 1);
                pi2_temp3 = pi2_temp3 + (8 << 1);
                pu1_dst = pu1_dst + (dst_strd << 1);
            }
            while(ht > 0);
        }
    }
    else // wd == 16
    {
        UWORD8 *pu1_dst1;
        WORD16 *pi2_temp4,*pi2_temp5;

        pu1_dst1 = pu1_dst + 8;
        pi2_temp4 = pi2_temp2 + 8;
        pi2_temp5 = pi2_temp3 + 8;

        // horizontal half-pel
        {
            __m128i src_r0_16x8b, src_r1_16x8b, src_r0_sht_16x8b, src_r1_sht_16x8b;
            __m128i src_r0_t1_16x8b, src_r1_t1_16x8b;

            __m128i res_r0_t1_8x16b, res_r0_t2_8x16b, res_r0_t3_8x16b;
            __m128i res_r1_t1_8x16b, res_r1_t2_8x16b, res_r1_t3_8x16b;

            __m128i coeff0_1_16x8b, coeff2_3_16x8b, coeff4_5_16x8b;

            coeff0_1_16x8b = _mm_set1_epi32(0xFB01FB01);  //c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1 c0 c1
            coeff2_3_16x8b = _mm_set1_epi32(0x14141414);  //c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3 c2 c3
            coeff4_5_16x8b = _mm_set1_epi32(0x01FB01FB);  //c4 c5 c5 c5 c4 c5 c5 c5 c4 c5 c5 c5 c4 c5 c5 c5

            //Row0 : a0 a1 a2 a3 a4 a5 a6 a7 a8 a9.....
            //Row0 :                         b0 b1 b2 b3 b4 b5 b6 b7 b8 b9.....
            //b0 is same a8. Similarly other bn pixels are same as a(n+8) pixels.

            do
            {
                src_r0_16x8b = _mm_loadu_si128((__m128i *)(pu1_src));                  //a0 a1 a2 a3 a4 a5 a6 a7 a8 a9....a15
                src_r1_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + 8));              //b0 b1 b2 b3 b4 b5 b6 b7 b8 b9....b15

                src_r0_sht_16x8b = _mm_srli_si128(src_r0_16x8b, 1);                    //a1 a2 a3 a4 a5 a6 a7 a8 a9....a15 0
                src_r1_sht_16x8b = _mm_srli_si128(src_r1_16x8b, 1);                    //b1 b2 b3 b4 b5 b6 b7 b8 b9....b15 0

                src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);   //a0 a1 a1 a2 a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8
                src_r1_t1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);   //b0 b1 b1 b2 b2 b3 b3 b4 b4 b5 b5 b6 b6 b7 b7 b8

                res_r0_t1_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff0_1_16x8b);   //a0*c0+a1*c1 a1*c0+a2*c1 a2*c0+a3*c1 a3*c0+a4*c1
                                                                                        //a4*c0+a5*c1 a5*c0+a6*c1 a6*c0+a7*c1 a7*c0+a8*c1
                res_r1_t1_8x16b = _mm_maddubs_epi16(src_r1_t1_16x8b, coeff0_1_16x8b);   //b0*c0+b1*c1 b1*c0+b2*c1 b2*c0+b3*c1 b3*c0+b4*c1
                                                                                        //b4*c0+b5*c1 b5*c0+b6*c1 b6*c0+b7*c1 b7*c0+b8*c1

                src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 2);                         //a2 a3 a4 a5 a6 a7 a8 a9....a15 0 0
                src_r1_16x8b = _mm_srli_si128(src_r1_16x8b, 2);                         //b2 b3 b4 b5 b6 b7 b8 b9....b15 0 0

                src_r0_sht_16x8b = _mm_srli_si128(src_r0_sht_16x8b, 2);                 //a3 a4 a5 a6 a7 a8 a9....a15 0  0  0
                src_r1_sht_16x8b = _mm_srli_si128(src_r1_sht_16x8b, 2);                 //b3 b4 b5 b6 b7 b8 b9....b15 0  0  0

                src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);    //a2 a3 a3 a4 a4 a5 a5 a6 a6 a7 a7 a8 a8 a9 a9 a10
                src_r1_t1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);    //b2 b3 b3 b4 b4 b5 b5 b6 b6 b7 b7 b8 a8 a9 a9 a10

                res_r0_t2_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff2_3_16x8b);   //a2*c2+a3*c3 a3*c2+a4*c3 a4*c2+a5*c3 a5*c2+a6*c3
                                                                                        //a6*c2+a7*c3 a7*c2+a8*c3 a8*c2+a9*c3 a9*c2+a10*c3
                res_r1_t2_8x16b = _mm_maddubs_epi16(src_r1_t1_16x8b, coeff2_3_16x8b);   //b2*c2+b3*c3 b3*c2+b4*c3 b2*c4+b5*c3 b5*c2+b6*c3
                                                                                        //b6*c2+b7*c3 b7*c2+b8*c3 b8*c2+b9*c3 b9*c2+b10*c3

                src_r0_16x8b = _mm_srli_si128(src_r0_16x8b, 2);                         //a4 a5 a6 a7 a8 a9....a15 0  0  0  0
                src_r1_16x8b = _mm_srli_si128(src_r1_16x8b, 2);                         //b4 b5 b6 b7 b8 b9....b15 0  0  0  0

                src_r0_sht_16x8b = _mm_srli_si128(src_r0_sht_16x8b, 2);                 //a5 a6 a7 a8 a9....a15 0  0  0  0  0
                src_r1_sht_16x8b = _mm_srli_si128(src_r1_sht_16x8b, 2);                 //b5 b6 b7 b8 b9....b15 0  0  0  0  0

                src_r0_t1_16x8b = _mm_unpacklo_epi8(src_r0_16x8b, src_r0_sht_16x8b);    //a4 a5 a5 a6 a6 a7 a7 a8 a8 a9 a9 a10 a10 a11 a11 a12
                src_r1_t1_16x8b = _mm_unpacklo_epi8(src_r1_16x8b, src_r1_sht_16x8b);    //b4 b5 b5 b6 b6 b7 b7 b8 b8 b9 b9 b10 b10 b11 b11 b12

                res_r0_t3_8x16b = _mm_maddubs_epi16(src_r0_t1_16x8b, coeff4_5_16x8b);   //a4*c4+a5*c5 a5*c4+a6*c5 a6*c4+a7*c5 a7*c4+a8*c5
                                                                                        //a8*c4+a9*c5 a9*c4+a10*c5 a10*c4+a11*c5 a11*c4+a12*c5
                res_r1_t3_8x16b = _mm_maddubs_epi16(src_r1_t1_16x8b, coeff4_5_16x8b);   //b4*c4+b5*c5 b5*c4+b6*c5 b6*c4+b7*c5 b7*c4+b8*c5
                                                                                        //b8*c4+b9*c5 b9*c4+b10*c5 b10*c4+b11*c5 b11*c4+b12*c5
                res_r0_t1_8x16b = _mm_add_epi16(res_r0_t1_8x16b, res_r0_t2_8x16b);
                res_r0_t1_8x16b = _mm_add_epi16(res_r0_t1_8x16b, res_r0_t3_8x16b);

                res_r1_t1_8x16b = _mm_add_epi16(res_r1_t1_8x16b, res_r1_t2_8x16b);
                res_r1_t1_8x16b = _mm_add_epi16(res_r1_t1_8x16b, res_r1_t3_8x16b);

                _mm_storeu_si128((__m128i *)(pi2_temp1), res_r0_t1_8x16b);
                _mm_storeu_si128((__m128i *)(pi2_temp1 + 8), res_r1_t1_8x16b);

                ht_temp--;
                pu1_src =  pu1_src + src_strd;
                pi2_temp1 =  pi2_temp1 + 16;
            }
            while(ht_temp > 0);
        }
        // vertical q-pel
        {
            __m128i src_r0_8x16b, src_r1_8x16b, src_r2_8x16b, src_r3_8x16b, src_r4_8x16b;
            __m128i src_r5_8x16b, src_r6_8x16b;
            __m128i src_r0r1_8x16b, src_r2r3_8x16b, src_r4r5_8x16b;
            __m128i src_hpel_8x16b, src_hpel_16x8b;

            __m128i res_t0_4x32b, res_t1_4x32b, res_t2_4x32b, res_t3_4x32b;
            __m128i res_8x16b, res_16x8b;

            __m128i coeff0_1_8x16b, coeff2_3_8x16b, coeff4_5_8x16b;
            __m128i const_val512_4x32b, const_val16_8x16b;

            coeff0_1_8x16b = _mm_set1_epi32(0xFFFB0001);
            coeff2_3_8x16b = _mm_set1_epi32(0x00140014);
            coeff4_5_8x16b = _mm_set1_epi32(0x0001FFFB);

            const_val512_4x32b = _mm_set1_epi32(512);
            const_val16_8x16b = _mm_set1_epi16(16);

            /**********************************************************/
            /*     Do first height x 8 block                          */
            /**********************************************************/
            src_r0_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2));
            src_r1_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 16));
            src_r2_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 32));
            src_r3_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 48));
            src_r4_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 64));
            pi2_temp2 += 80;

            ht_temp = ht;
            do
            {
                src_r5_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2));
                src_r6_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp2 + 16));

                src_r0r1_8x16b = _mm_unpacklo_epi16(src_r0_8x16b, src_r1_8x16b);
                src_r2r3_8x16b = _mm_unpacklo_epi16(src_r2_8x16b, src_r3_8x16b);
                src_r4r5_8x16b = _mm_unpacklo_epi16(src_r4_8x16b, src_r5_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(res_t3_4x32b, const_val512_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);
                res_t0_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                src_r0r1_8x16b = _mm_unpackhi_epi16(src_r0_8x16b, src_r1_8x16b);
                src_r2r3_8x16b = _mm_unpackhi_epi16(src_r2_8x16b, src_r3_8x16b);
                src_r4r5_8x16b = _mm_unpackhi_epi16(src_r4_8x16b, src_r5_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(res_t3_4x32b, const_val512_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);
                res_t1_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                res_8x16b = _mm_packs_epi32(res_t0_4x32b, res_t1_4x32b);
                res_16x8b = _mm_packus_epi16(res_8x16b, res_8x16b);

                src_hpel_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp3));
                src_hpel_8x16b = _mm_add_epi16(src_hpel_8x16b, const_val16_8x16b);
                src_hpel_8x16b = _mm_srai_epi16(src_hpel_8x16b, 5); //shifting right by 5 bits.
                src_hpel_16x8b = _mm_packus_epi16(src_hpel_8x16b, src_hpel_8x16b);

                res_16x8b = _mm_avg_epu8(res_16x8b, src_hpel_16x8b);
                _mm_storel_epi64((__m128i *)(pu1_dst), res_16x8b);

                src_r0r1_8x16b = _mm_unpacklo_epi16(src_r1_8x16b, src_r2_8x16b);
                src_r2r3_8x16b = _mm_unpacklo_epi16(src_r3_8x16b, src_r4_8x16b);
                src_r4r5_8x16b = _mm_unpacklo_epi16(src_r5_8x16b, src_r6_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(res_t3_4x32b, const_val512_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);
                res_t0_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                src_r0r1_8x16b = _mm_unpackhi_epi16(src_r1_8x16b, src_r2_8x16b);
                src_r2r3_8x16b = _mm_unpackhi_epi16(src_r3_8x16b, src_r4_8x16b);
                src_r4r5_8x16b = _mm_unpackhi_epi16(src_r5_8x16b, src_r6_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(res_t3_4x32b, const_val512_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);
                res_t1_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                res_8x16b = _mm_packs_epi32(res_t0_4x32b, res_t1_4x32b);
                res_16x8b = _mm_packus_epi16(res_8x16b, res_8x16b);

                src_hpel_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp3 + 16));
                src_hpel_8x16b = _mm_add_epi16(src_hpel_8x16b, const_val16_8x16b);
                src_hpel_8x16b = _mm_srai_epi16(src_hpel_8x16b, 5); //shifting right by 5 bits.
                src_hpel_16x8b = _mm_packus_epi16(src_hpel_8x16b, src_hpel_8x16b);

                res_16x8b = _mm_avg_epu8(res_16x8b, src_hpel_16x8b);
                _mm_storel_epi64((__m128i *)(pu1_dst + dst_strd), res_16x8b);

                src_r0_8x16b = src_r2_8x16b;
                src_r1_8x16b = src_r3_8x16b;
                src_r2_8x16b = src_r4_8x16b;
                src_r3_8x16b = src_r5_8x16b;
                src_r4_8x16b = src_r6_8x16b;

                ht_temp -= 2;
                pi2_temp3 = pi2_temp3 + (16 << 1);
                pi2_temp2 = pi2_temp2 + (16 << 1);
                pu1_dst = pu1_dst + (dst_strd << 1);
            }
            while(ht_temp > 0);

            /**********************************************************/
            /*     Do second height * 8 block                         */
            /**********************************************************/
            src_r0_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp4));
            src_r1_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp4 + 16));
            src_r2_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp4 + 32));
            src_r3_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp4 + 48));
            src_r4_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp4 + 64));
            pi2_temp4 += 80;

            do
            {
                src_r5_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp4));
                src_r6_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp4 + 16));

                src_r0r1_8x16b = _mm_unpacklo_epi16(src_r0_8x16b, src_r1_8x16b);
                src_r2r3_8x16b = _mm_unpacklo_epi16(src_r2_8x16b, src_r3_8x16b);
                src_r4r5_8x16b = _mm_unpacklo_epi16(src_r4_8x16b, src_r5_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(res_t3_4x32b, const_val512_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);
                res_t0_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                src_r0r1_8x16b = _mm_unpackhi_epi16(src_r0_8x16b, src_r1_8x16b);
                src_r2r3_8x16b = _mm_unpackhi_epi16(src_r2_8x16b, src_r3_8x16b);
                src_r4r5_8x16b = _mm_unpackhi_epi16(src_r4_8x16b, src_r5_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(res_t3_4x32b, const_val512_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);
                res_t1_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                res_8x16b = _mm_packs_epi32(res_t0_4x32b, res_t1_4x32b);
                res_16x8b = _mm_packus_epi16(res_8x16b, res_8x16b);

                src_hpel_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp5));
                src_hpel_8x16b = _mm_add_epi16(src_hpel_8x16b, const_val16_8x16b);
                src_hpel_8x16b = _mm_srai_epi16(src_hpel_8x16b, 5); //shifting right by 5 bits.
                src_hpel_16x8b = _mm_packus_epi16(src_hpel_8x16b, src_hpel_8x16b);

                res_16x8b = _mm_avg_epu8(res_16x8b, src_hpel_16x8b);
                _mm_storel_epi64((__m128i *)(pu1_dst1), res_16x8b);

                src_r0r1_8x16b = _mm_unpacklo_epi16(src_r1_8x16b, src_r2_8x16b);
                src_r2r3_8x16b = _mm_unpacklo_epi16(src_r3_8x16b, src_r4_8x16b);
                src_r4r5_8x16b = _mm_unpacklo_epi16(src_r5_8x16b, src_r6_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(res_t3_4x32b, const_val512_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);
                res_t0_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                src_r0r1_8x16b = _mm_unpackhi_epi16(src_r1_8x16b, src_r2_8x16b);
                src_r2r3_8x16b = _mm_unpackhi_epi16(src_r3_8x16b, src_r4_8x16b);
                src_r4r5_8x16b = _mm_unpackhi_epi16(src_r5_8x16b, src_r6_8x16b);

                res_t1_4x32b = _mm_madd_epi16(src_r0r1_8x16b, coeff0_1_8x16b);
                res_t2_4x32b = _mm_madd_epi16(src_r2r3_8x16b, coeff2_3_8x16b);
                res_t3_4x32b = _mm_madd_epi16(src_r4r5_8x16b, coeff4_5_8x16b);

                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t2_4x32b);
                res_t3_4x32b = _mm_add_epi32(res_t3_4x32b, const_val512_4x32b);
                res_t1_4x32b = _mm_add_epi32(res_t1_4x32b, res_t3_4x32b);
                res_t1_4x32b = _mm_srai_epi32(res_t1_4x32b, 10);

                res_8x16b = _mm_packs_epi32(res_t0_4x32b, res_t1_4x32b);
                res_16x8b = _mm_packus_epi16(res_8x16b, res_8x16b);

                src_hpel_8x16b = _mm_loadu_si128((__m128i *)(pi2_temp5 + 16));
                src_hpel_8x16b = _mm_add_epi16(src_hpel_8x16b, const_val16_8x16b);
                src_hpel_8x16b = _mm_srai_epi16(src_hpel_8x16b, 5); //shifting right by 5 bits.
                src_hpel_16x8b = _mm_packus_epi16(src_hpel_8x16b, src_hpel_8x16b);

                res_16x8b = _mm_avg_epu8(res_16x8b, src_hpel_16x8b);
                _mm_storel_epi64((__m128i *)(pu1_dst1 + dst_strd), res_16x8b);

                src_r0_8x16b = src_r2_8x16b;
                src_r1_8x16b = src_r3_8x16b;
                src_r2_8x16b = src_r4_8x16b;
                src_r3_8x16b = src_r5_8x16b;
                src_r4_8x16b = src_r6_8x16b;

                ht -= 2;
                pi2_temp5 = pi2_temp5 + (16 << 1);
                pi2_temp4 = pi2_temp4 + (16 << 1);
                pu1_dst1 = pu1_dst1 + (dst_strd << 1);
            }
            while(ht > 0);
        }
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_inter_pred_chroma_ssse3                            */
/*                                                                           */
/*  Description   : This function implements a four-tap 2D filter as         */
/*                  mentioned in sec. 8.4.2.2.2 titled "Chroma sample        */
/*                  "interpolation process". (ht,wd) can be (2,2), (4,2),    */
/*                  (2,4), (4,4), (8,4), (4,8) or (8,8).                     */
/*                                                                           */
/*  Inputs        : puc_src  - pointer to source                             */
/*                  puc_dst  - pointer to destination                        */
/*                  src_strd - stride for source                             */
/*                  dst_strd - stride for destination                        */
/*                  dx       - x position of destination value               */
/*                  dy       - y position of destination value               */
/*                  ht       - height of the block                           */
/*                  wd       - width of the block                            */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes                              */
/*         13 02 2015   Kaushik         Initial Version                      */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_inter_pred_chroma_ssse3(UWORD8 *pu1_src,
                                   UWORD8 *pu1_dst,
                                   WORD32 src_strd,
                                   WORD32 dst_strd,
                                   WORD32 dx,
                                   WORD32 dy,
                                   WORD32 ht,
                                   WORD32 wd)
{
    WORD32 i, j, A, B, C, D;

    i = 8 - dx;
    j = 8 - dy;

    A = i * j;
    B = dx * j;
    C = i * dy;
    D = dx * dy;

    if(wd == 2)
    {
        WORD32 tmp1, tmp2, tmp3, tmp4;

        do
        {
            //U
            tmp1 = A * pu1_src[0] + B * pu1_src[2] + C * pu1_src[src_strd] + D * pu1_src[src_strd + 2];
            tmp2 = A * pu1_src[2] + B * pu1_src[4] + C * pu1_src[src_strd + 2] + D * pu1_src[src_strd + 4];
            //V
            tmp3 = A * pu1_src[1] + B * pu1_src[3] + C * pu1_src[src_strd + 1] + D * pu1_src[src_strd + 3];
            tmp4 = A * pu1_src[3] + B * pu1_src[5] + C * pu1_src[src_strd + 3] + D * pu1_src[src_strd + 5];

            tmp1 = (tmp1 + 32) >> 6;
            tmp2 = (tmp2 + 32) >> 6;
            tmp3 = (tmp3 + 32) >> 6;
            tmp4 = (tmp4 + 32) >> 6;

            pu1_dst[0] = CLIP_U8(tmp1);
            pu1_dst[2] = CLIP_U8(tmp2);
            pu1_dst[1] = CLIP_U8(tmp3);
            pu1_dst[3] = CLIP_U8(tmp4);

            pu1_src += src_strd;
            pu1_dst += dst_strd;

            tmp1 = A * pu1_src[0] + B * pu1_src[2] + C * pu1_src[src_strd] + D * pu1_src[src_strd + 2];
            tmp2 = A * pu1_src[2] + B * pu1_src[4] + C * pu1_src[src_strd + 2] + D * pu1_src[src_strd + 4];
            tmp3 = A * pu1_src[1] + B * pu1_src[3] + C * pu1_src[src_strd + 1] + D * pu1_src[src_strd + 3];
            tmp4 = A * pu1_src[3] + B * pu1_src[5] + C * pu1_src[src_strd + 3] + D * pu1_src[src_strd + 5];

            tmp1 = (tmp1 + 32) >> 6;
            tmp2 = (tmp2 + 32) >> 6;
            tmp3 = (tmp3 + 32) >> 6;
            tmp4 = (tmp4 + 32) >> 6;

            pu1_dst[0] = CLIP_U8(tmp1);
            pu1_dst[2] = CLIP_U8(tmp2);
            pu1_dst[1] = CLIP_U8(tmp3);
            pu1_dst[3] = CLIP_U8(tmp4);

            ht -= 2;
            pu1_src += src_strd;
            pu1_dst += dst_strd;
        }
        while(ht > 0);

    }
    else if(wd == 4)
    {
        WORD32 AB, CD;

        __m128i src_r1_16x8b, src_r2_16x8b, src_r3_16x8b;
        __m128i res1_AB_8x16b, res1_CD_8x16b, res1_8x16b, res1_16x8b;
        __m128i res2_AB_8x16b, res2_CD_8x16b, res2_8x16b, res2_16x8b;

        __m128i coeffAB_16x8b, coeffCD_16x8b, round_add32_8x16b;
        __m128i const_shuff_16x8b;

        AB = (B << 8) + A;
        CD = (D << 8) + C;

        coeffAB_16x8b = _mm_set1_epi16(AB);
        coeffCD_16x8b = _mm_set1_epi16(CD);

        round_add32_8x16b = _mm_set1_epi16(32);

        const_shuff_16x8b = _mm_setr_epi32(0x03010200, 0x05030402, 0x07050604, 0x09070806);

        src_r1_16x8b = _mm_loadu_si128((__m128i *)pu1_src);
        src_r1_16x8b = _mm_shuffle_epi8(src_r1_16x8b, const_shuff_16x8b);
        pu1_src += src_strd;

        do
        {
            src_r2_16x8b = _mm_loadu_si128((__m128i *)pu1_src);
            src_r3_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + src_strd));

            src_r2_16x8b = _mm_shuffle_epi8(src_r2_16x8b, const_shuff_16x8b);
            src_r3_16x8b = _mm_shuffle_epi8(src_r3_16x8b, const_shuff_16x8b);

            res1_AB_8x16b = _mm_maddubs_epi16(src_r1_16x8b, coeffAB_16x8b);
            res1_CD_8x16b = _mm_maddubs_epi16(src_r2_16x8b, coeffCD_16x8b);
            res2_AB_8x16b = _mm_maddubs_epi16(src_r2_16x8b, coeffAB_16x8b);
            res2_CD_8x16b = _mm_maddubs_epi16(src_r3_16x8b, coeffCD_16x8b);

            res1_8x16b = _mm_add_epi16(res1_AB_8x16b, res1_CD_8x16b);
            res2_8x16b = _mm_add_epi16(res2_AB_8x16b, res2_CD_8x16b);
            res1_8x16b = _mm_add_epi16(res1_8x16b, round_add32_8x16b);
            res2_8x16b = _mm_add_epi16(res2_8x16b, round_add32_8x16b);

            res1_8x16b = _mm_srai_epi16(res1_8x16b, 6);
            res2_8x16b = _mm_srai_epi16(res2_8x16b, 6);

            res1_16x8b = _mm_packus_epi16(res1_8x16b, res1_8x16b);
            res2_16x8b = _mm_packus_epi16(res2_8x16b, res2_8x16b);

            _mm_storel_epi64((__m128i *)pu1_dst, res1_16x8b);
            _mm_storel_epi64((__m128i *)(pu1_dst + dst_strd), res2_16x8b);

            src_r1_16x8b = src_r3_16x8b;

            ht -= 2;
            pu1_src += src_strd << 1;
            pu1_dst += dst_strd << 1;
        }
        while(ht > 0);
    }
    else // wd == 8
    {
        WORD32 AB, CD;

        __m128i src_r1l_16x8b, src_r2l_16x8b;
        __m128i src_r1h_16x8b, src_r2h_16x8b;

        __m128i res_l_AB_8x16b, res_l_CD_8x16b;
        __m128i res_h_AB_8x16b, res_h_CD_8x16b;
        __m128i res_l_8x16b, res_h_8x16b, res_16x8b;

        __m128i coeffAB_16x8b, coeffCD_16x8b, round_add32_8x16b;
        __m128i const_shuff_16x8b;

        AB = (B << 8) + A;
        CD = (D << 8) + C;

        coeffAB_16x8b = _mm_set1_epi16(AB);
        coeffCD_16x8b = _mm_set1_epi16(CD);

        round_add32_8x16b = _mm_set1_epi16(32);

        const_shuff_16x8b = _mm_setr_epi32(0x03010200, 0x05030402, 0x07050604, 0x09070806);

        src_r1l_16x8b = _mm_loadu_si128((__m128i *)pu1_src);
        src_r1h_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + 8));

        src_r1l_16x8b = _mm_shuffle_epi8(src_r1l_16x8b, const_shuff_16x8b);
        src_r1h_16x8b = _mm_shuffle_epi8(src_r1h_16x8b, const_shuff_16x8b);

        pu1_src += src_strd;

        do
        {
            //row 1
            src_r2l_16x8b = _mm_loadu_si128((__m128i *)pu1_src);
            src_r2h_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + 8));

            src_r2l_16x8b = _mm_shuffle_epi8(src_r2l_16x8b, const_shuff_16x8b);
            src_r2h_16x8b = _mm_shuffle_epi8(src_r2h_16x8b, const_shuff_16x8b);

            res_l_AB_8x16b = _mm_maddubs_epi16(src_r1l_16x8b, coeffAB_16x8b);
            res_h_AB_8x16b = _mm_maddubs_epi16(src_r1h_16x8b, coeffAB_16x8b);
            res_l_CD_8x16b = _mm_maddubs_epi16(src_r2l_16x8b, coeffCD_16x8b);
            res_h_CD_8x16b = _mm_maddubs_epi16(src_r2h_16x8b, coeffCD_16x8b);

            res_l_8x16b = _mm_add_epi16(res_l_AB_8x16b, round_add32_8x16b);
            res_h_8x16b = _mm_add_epi16(res_h_AB_8x16b, round_add32_8x16b);
            res_l_8x16b = _mm_add_epi16(res_l_8x16b, res_l_CD_8x16b);
            res_h_8x16b = _mm_add_epi16(res_h_8x16b, res_h_CD_8x16b);

            res_l_8x16b = _mm_srai_epi16(res_l_8x16b, 6);
            res_h_8x16b = _mm_srai_epi16(res_h_8x16b, 6);

            res_16x8b = _mm_packus_epi16(res_l_8x16b, res_h_8x16b);

            _mm_storeu_si128((__m128i *)pu1_dst, res_16x8b);

            pu1_src += src_strd;
            pu1_dst += dst_strd;

            //row 2
            src_r1l_16x8b = _mm_loadu_si128((__m128i *)pu1_src);
            src_r1h_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + 8));

            src_r1l_16x8b = _mm_shuffle_epi8(src_r1l_16x8b, const_shuff_16x8b);
            src_r1h_16x8b = _mm_shuffle_epi8(src_r1h_16x8b, const_shuff_16x8b);

            res_l_AB_8x16b = _mm_maddubs_epi16(src_r2l_16x8b, coeffAB_16x8b);
            res_h_AB_8x16b = _mm_maddubs_epi16(src_r2h_16x8b, coeffAB_16x8b);
            res_l_CD_8x16b = _mm_maddubs_epi16(src_r1l_16x8b, coeffCD_16x8b);
            res_h_CD_8x16b = _mm_maddubs_epi16(src_r1h_16x8b, coeffCD_16x8b);

            res_l_8x16b = _mm_add_epi16(res_l_AB_8x16b, round_add32_8x16b);
            res_h_8x16b = _mm_add_epi16(res_h_AB_8x16b, round_add32_8x16b);
            res_l_8x16b = _mm_add_epi16(res_l_8x16b, res_l_CD_8x16b);
            res_h_8x16b = _mm_add_epi16(res_h_8x16b, res_h_CD_8x16b);

            res_l_8x16b = _mm_srai_epi16(res_l_8x16b, 6);
            res_h_8x16b = _mm_srai_epi16(res_h_8x16b, 6);

            res_16x8b = _mm_packus_epi16(res_l_8x16b, res_h_8x16b);

            _mm_storeu_si128((__m128i *)pu1_dst, res_16x8b);

            pu1_src += src_strd;
            pu1_dst += dst_strd;

            //row 3
            src_r2l_16x8b = _mm_loadu_si128((__m128i *)pu1_src);
            src_r2h_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + 8));

            src_r2l_16x8b = _mm_shuffle_epi8(src_r2l_16x8b, const_shuff_16x8b);
            src_r2h_16x8b = _mm_shuffle_epi8(src_r2h_16x8b, const_shuff_16x8b);

            res_l_AB_8x16b = _mm_maddubs_epi16(src_r1l_16x8b, coeffAB_16x8b);
            res_h_AB_8x16b = _mm_maddubs_epi16(src_r1h_16x8b, coeffAB_16x8b);
            res_l_CD_8x16b = _mm_maddubs_epi16(src_r2l_16x8b, coeffCD_16x8b);
            res_h_CD_8x16b = _mm_maddubs_epi16(src_r2h_16x8b, coeffCD_16x8b);

            res_l_8x16b = _mm_add_epi16(res_l_AB_8x16b, round_add32_8x16b);
            res_h_8x16b = _mm_add_epi16(res_h_AB_8x16b, round_add32_8x16b);
            res_l_8x16b = _mm_add_epi16(res_l_8x16b, res_l_CD_8x16b);
            res_h_8x16b = _mm_add_epi16(res_h_8x16b, res_h_CD_8x16b);

            res_l_8x16b = _mm_srai_epi16(res_l_8x16b, 6);
            res_h_8x16b = _mm_srai_epi16(res_h_8x16b, 6);

            res_16x8b = _mm_packus_epi16(res_l_8x16b, res_h_8x16b);

            _mm_storeu_si128((__m128i *)pu1_dst, res_16x8b);

            pu1_src += src_strd;
            pu1_dst += dst_strd;

            //row 1
            src_r1l_16x8b = _mm_loadu_si128((__m128i *)pu1_src);
            src_r1h_16x8b = _mm_loadu_si128((__m128i *)(pu1_src + 8));

            src_r1l_16x8b = _mm_shuffle_epi8(src_r1l_16x8b, const_shuff_16x8b);
            src_r1h_16x8b = _mm_shuffle_epi8(src_r1h_16x8b, const_shuff_16x8b);

            res_l_AB_8x16b = _mm_maddubs_epi16(src_r2l_16x8b, coeffAB_16x8b);
            res_h_AB_8x16b = _mm_maddubs_epi16(src_r2h_16x8b, coeffAB_16x8b);
            res_l_CD_8x16b = _mm_maddubs_epi16(src_r1l_16x8b, coeffCD_16x8b);
            res_h_CD_8x16b = _mm_maddubs_epi16(src_r1h_16x8b, coeffCD_16x8b);

            res_l_8x16b = _mm_add_epi16(res_l_AB_8x16b, round_add32_8x16b);
            res_h_8x16b = _mm_add_epi16(res_h_AB_8x16b, round_add32_8x16b);
            res_l_8x16b = _mm_add_epi16(res_l_8x16b, res_l_CD_8x16b);
            res_h_8x16b = _mm_add_epi16(res_h_8x16b, res_h_CD_8x16b);

            res_l_8x16b = _mm_srai_epi16(res_l_8x16b, 6);
            res_h_8x16b = _mm_srai_epi16(res_h_8x16b, 6);

            res_16x8b = _mm_packus_epi16(res_l_8x16b, res_h_8x16b);

            _mm_storeu_si128((__m128i *)pu1_dst, res_16x8b);

            ht -= 4;
            pu1_src += src_strd;
            pu1_dst += dst_strd;
        }
        while(ht > 0);
    }
}
