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
 *  ih264_ihadamard_scaling_sse42.c
 *
 * @brief
 *  Contains definition of functions for h264 inverse hadamard 4x4 transform and scaling
 *
 * @author
 *  Mohit
 *
 *  @par List of Functions:
 *  - ih264_ihadamard_scaling_4x4_sse42()
 *  - ih264_ihadamard_scaling_2x2_uv_ssse42()
 *
 * @remarks
 *
 *******************************************************************************
 */
/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

/* User include files */
#include "ih264_typedefs.h"
#include "ih264_defs.h"
#include "ih264_trans_macros.h"
#include "ih264_macros.h"
#include "ih264_trans_data.h"
#include "ih264_size_defs.h"
#include "ih264_structs.h"
#include "ih264_trans_quant_itrans_iquant.h"
#include <immintrin.h>
#include <smmintrin.h>

#ifdef __GNUC__
#define ATTRIBUTE_SSE42 __attribute__((target("sse4.2")))
#else
#define ATTRIBUTE_SSE42
#endif

/*
 ********************************************************************************
 *
 * @brief This function performs a 4x4 inverse hadamard transform on the 4x4 DC coefficients
 * of a 16x16 intra prediction macroblock, and then performs scaling.
 * prediction buffer
 *
 * @par Description:
 *  The DC coefficients pass through a 2-stage inverse hadamard transform.
 *  This inverse transformed content is scaled to based on Qp value.
 *
 * @param[in] pi2_src
 *  input 4x4 block of DC coefficients
 *
 * @param[out] pi2_out
 *  output 4x4 block
 *
 * @param[in] pu2_iscal_mat
 *  pointer to scaling list
 *
 * @param[in] pu2_weigh_mat
 *  pointer to weight matrix
 *
 * @param[in] u4_qp_div_6
 *  Floor (qp/6)
 *
 * @param[in] pi4_tmp
 * temporary buffer of size 1*16
 *
 * @returns none
 *
 * @remarks none
 *
 *******************************************************************************
 */
ATTRIBUTE_SSE42
void ih264_ihadamard_scaling_4x4_sse42(WORD16* pi2_src,
                                       WORD16* pi2_out,
                                       const UWORD16 *pu2_iscal_mat,
                                       const UWORD16 *pu2_weigh_mat,
                                       UWORD32 u4_qp_div_6,
                                       WORD32* pi4_tmp)
{
    __m128i src_r0_r1, src_r2_r3;
    __m128i src_r0, src_r1, src_r2, src_r3;
    __m128i temp0, temp1, temp2, temp3;
    __m128i add_rshift = _mm_set1_epi32((u4_qp_div_6 < 6) ? (1 << (5 - u4_qp_div_6)) : 0);
    __m128i mult_val = _mm_set1_epi32(pu2_iscal_mat[0] * pu2_weigh_mat[0]);
    UNUSED (pi4_tmp);

    src_r0_r1 = _mm_loadu_si128((__m128i *) (pi2_src)); //a00 a01 a02 a03 a10 a11 a12 a13 -- the source matrix 0th,1st row
    src_r2_r3 = _mm_loadu_si128((__m128i *) (pi2_src + 8)); //a20 a21 a22 a23 a30 a31 a32 a33 -- the source matrix 2nd,3rd row
    //sign_reg = _mm_cmpgt_epi16(zero_8x16b, src_r0_r1);
    src_r0 = _mm_cvtepi16_epi32(src_r0_r1);
    src_r0_r1 = _mm_srli_si128(src_r0_r1, 8);
    src_r1 = _mm_cvtepi16_epi32(src_r0_r1);

    src_r2 = _mm_cvtepi16_epi32(src_r2_r3);
    src_r2_r3 = _mm_srli_si128(src_r2_r3, 8);
    src_r3 = _mm_cvtepi16_epi32(src_r2_r3);

    /* Perform Inverse transform */
    /*-------------------------------------------------------------*/
    /* IDCT [ Horizontal transformation ]                          */
    /*-------------------------------------------------------------*/
    // Matrix transpose
    /*
     *  a0 a1 a2 a3
     *  b0 b1 b2 b3
     *  c0 c1 c2 c3
     *  d0 d1 d2 d3
     */
    temp0 = _mm_unpacklo_epi32(src_r0, src_r1);                  //a0 b0 a1 b1
    temp2 = _mm_unpacklo_epi32(src_r2, src_r3);                  //c0 d0 c1 d1
    temp1 = _mm_unpackhi_epi32(src_r0, src_r1);                  //a2 b2 a3 b3
    temp3 = _mm_unpackhi_epi32(src_r2, src_r3);                  //c2 d2 c3 d3
    src_r0 = _mm_unpacklo_epi64(temp0, temp2);                    //a0 b0 c0 d0
    src_r1 = _mm_unpackhi_epi64(temp0, temp2);                    //a1 b1 c1 d1
    src_r2 = _mm_unpacklo_epi64(temp1, temp3);                    //a2 b2 c2 d2
    src_r3 = _mm_unpackhi_epi64(temp1, temp3);                    //a3 b3 c3 d3

    temp0 = _mm_add_epi32(src_r0, src_r3);
    temp1 = _mm_add_epi32(src_r1, src_r2);
    temp2 = _mm_sub_epi32(src_r1, src_r2);
    temp3 = _mm_sub_epi32(src_r0, src_r3);

    src_r0 = _mm_add_epi32(temp0, temp1);
    src_r1 = _mm_add_epi32(temp2, temp3);
    src_r2 = _mm_sub_epi32(temp0, temp1);
    src_r3 = _mm_sub_epi32(temp3, temp2);

    /*-------------------------------------------------------------*/
    /* IDCT [ Vertical transformation ]                          */
    /*-------------------------------------------------------------*/
    // Matrix transpose
    /*
     *  a0 b0 c0 d0
     *  a1 b1 c1 d1
     *  a2 b2 c2 d2
     *  a3 b3 c3 d3
     */
    temp0 = _mm_unpacklo_epi32(src_r0, src_r1);                  //a0 a1 b0 b1
    temp2 = _mm_unpacklo_epi32(src_r2, src_r3);                  //a2 a3 b2 b3
    temp1 = _mm_unpackhi_epi32(src_r0, src_r1);                  //c0 c1 d0 d1
    temp3 = _mm_unpackhi_epi32(src_r2, src_r3);                  //c2 c3 d2 d3
    src_r0 = _mm_unpacklo_epi64(temp0, temp2);                   //a0 a1 a2 a3
    src_r1 = _mm_unpackhi_epi64(temp0, temp2);                   //b0 b1 b2 b3
    src_r2 = _mm_unpacklo_epi64(temp1, temp3);                   //c0 c1 c2 c3
    src_r3 = _mm_unpackhi_epi64(temp1, temp3);                   //d0 d1 d2 d3

    temp0 = _mm_add_epi32(src_r0, src_r3);
    temp1 = _mm_add_epi32(src_r1, src_r2);
    temp2 = _mm_sub_epi32(src_r1, src_r2);
    temp3 = _mm_sub_epi32(src_r0, src_r3);

    src_r0 = _mm_add_epi32(temp0, temp1);
    src_r1 = _mm_add_epi32(temp2, temp3);
    src_r2 = _mm_sub_epi32(temp0, temp1);
    src_r3 = _mm_sub_epi32(temp3, temp2);

    src_r0 = _mm_mullo_epi32(src_r0, mult_val);
    src_r1 = _mm_mullo_epi32(src_r1, mult_val);
    src_r2 = _mm_mullo_epi32(src_r2, mult_val);
    src_r3 = _mm_mullo_epi32(src_r3, mult_val);

    //Scaling
    if(u4_qp_div_6 >= 6)
    {
        src_r0 = _mm_slli_epi32(src_r0, u4_qp_div_6 - 6);
        src_r1 = _mm_slli_epi32(src_r1, u4_qp_div_6 - 6);
        src_r2 = _mm_slli_epi32(src_r2, u4_qp_div_6 - 6);
        src_r3 = _mm_slli_epi32(src_r3, u4_qp_div_6 - 6);
    }
    else
    {
        temp0 = _mm_add_epi32(src_r0, add_rshift);
        temp1 = _mm_add_epi32(src_r1, add_rshift);
        temp2 = _mm_add_epi32(src_r2, add_rshift);
        temp3 = _mm_add_epi32(src_r3, add_rshift);
        src_r0 = _mm_srai_epi32(temp0, 6 - u4_qp_div_6);
        src_r1 = _mm_srai_epi32(temp1, 6 - u4_qp_div_6);
        src_r2 = _mm_srai_epi32(temp2, 6 - u4_qp_div_6);
        src_r3 = _mm_srai_epi32(temp3, 6 - u4_qp_div_6);
    }
    src_r0_r1 = _mm_packs_epi32(src_r0, src_r1);
    src_r2_r3 = _mm_packs_epi32(src_r2, src_r3);

    _mm_storeu_si128((__m128i *) (&pi2_out[0]), src_r0_r1);
    _mm_storeu_si128((__m128i *) (&pi2_out[8]), src_r2_r3);
}

ATTRIBUTE_SSE42
void ih264_ihadamard_scaling_2x2_uv_sse42(WORD16* pi2_src,
                                          WORD16* pi2_out,
                                          const UWORD16 *pu2_iscal_mat,
                                          const UWORD16 *pu2_weigh_mat,
                                          UWORD32 u4_qp_div_6,
                                          WORD32* pi4_tmp)
{
    __m128i src, plane_0, plane_1, temp0, temp1, sign_reg;
    __m128i zero_8x16b = _mm_setzero_si128();
    __m128i scale_val = _mm_set1_epi32((WORD32)(pu2_iscal_mat[0] * pu2_weigh_mat[0]));
    UNUSED(pi4_tmp);

    src = _mm_loadu_si128((__m128i *) pi2_src);         //a0 a1 a2 a3 b0 b1 b2 b3
    sign_reg = _mm_cmpgt_epi16(zero_8x16b, src);
    plane_0 = _mm_unpacklo_epi16(src, sign_reg);        //a0 a1 a2 a3 -- 32 bits
    plane_1 = _mm_unpackhi_epi16(src, sign_reg);        //b0 b1 b2 b3 -- 32 bits

    temp0 = _mm_hadd_epi32(plane_0, plane_1);           //a0+a1 a2+a3 b0+b1 b2+b3
    temp1 = _mm_hsub_epi32(plane_0, plane_1);           //a0-a1 a2-a3 b0-b1 b2-b3
    plane_0 = _mm_hadd_epi32(temp0, temp1);             //a0+a1+a2+a3 b0+b1+b2+b3 a0-a1+a2-a3 b0-b1+b2-b3
    plane_1 = _mm_hsub_epi32(temp0, temp1);             //a0+a1-a2-a3 b0+b1-b2-b3 a0-a1-a2+a3 b0-b1-b2+b3
    temp0 = _mm_unpacklo_epi32(plane_0, plane_1);       //a0+a1+a2+a3 a0+a1-a2-a3 b0+b1+b2+b3 b0+b1-b2-b3
    temp1 = _mm_unpackhi_epi32(plane_0, plane_1);       //a0-a1+a2-a3 a0-a1-a2+a3 b0-b1+b2-b3 b0-b1-b2+b3

    plane_0 = _mm_unpacklo_epi64(temp0, temp1);         //a0+a1+a2+a3 a0+a1-a2-a3 a0-a1+a2-a3 a0-a1-a2+a3
    plane_1 = _mm_unpackhi_epi64(temp0, temp1);         //b0+b1+b2+b3 b0+b1-b2-b3 b0-b1+b2-b3 b0-b1-b2+b3

    plane_0 = _mm_shuffle_epi32(plane_0, 0xd8);         //a0+a1+a2+a3 a0-a1+a2-a3 a0+a1-a2-a3 a0-a1-a2+a3
    plane_1 = _mm_shuffle_epi32(plane_1, 0xd8);         //b0+b1+b2+b3 b0-b1+b2-b3 b0+b1-b2-b3 b0-b1-b2+b3

    temp0 = _mm_mullo_epi32(scale_val, plane_0);        //multiply by pu2_iscal_mat[0] * pu2_weigh_mat[0]
    temp1 = _mm_mullo_epi32(scale_val, plane_1);        //multiply by pu2_iscal_mat[0] * pu2_weigh_mat[0]

    temp0 = _mm_slli_epi32(temp0, u4_qp_div_6);
    temp1 = _mm_slli_epi32(temp1, u4_qp_div_6);

    temp0 = _mm_srai_epi32(temp0, 5);
    temp1 = _mm_srai_epi32(temp1, 5);

    temp0 = _mm_packs_epi32(temp0, temp1);              //Final values are 16-bits only.

    _mm_storeu_si128((__m128i *) (&pi2_out[0]), temp0);

}
