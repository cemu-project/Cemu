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
 *  ih264_iquant_itrans_recon_sse42.c
 *
 * @brief
 *  Contains function definitions for inverse  quantization, inverse
 * transform and reconstruction
 *
 * @author
 *  Mohit [100664]
 *
 * @par List of Functions:
 *  - ih264_iquant_itrans_recon_4x4_sse42()
 *  - ih264_iquant_itrans_recon_chroma_4x4_sse42()
 *
 * @remarks
 *  None
 *
 *******************************************************************************
 */
/* User include files */
#include "ih264_typedefs.h"
#include "ih264_defs.h"
#include "ih264_trans_macros.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264_trans_data.h"
#include "ih264_size_defs.h"
#include "ih264_structs.h"
#include "ih264_trans_quant_itrans_iquant.h"
#include <immintrin.h>

/*
 ********************************************************************************
 *
 * @brief This function reconstructs a 4x4 sub block from quantized resiude and
 * prediction buffer
 *
 * @par Description:
 *  The quantized residue is first inverse quantized, then inverse transformed.
 *  This inverse transformed content is added to the prediction buffer to recon-
 *  struct the end output
 *
 * @param[in] pi2_src
 *  quantized 4x4 block
 *
 * @param[in] pu1_pred
 *  prediction 4x4 block
 *
 * @param[out] pu1_out
 *  reconstructed 4x4 block
 *
 * @param[in] src_strd
 *  quantization buffer stride
 *
 * @param[in] pred_strd,
 *  Prediction buffer stride
 *
 * @param[in] out_strd
 *  recon buffer Stride
 *
 * @param[in] pu2_scaling_list
 *  pointer to scaling list
 *
 * @param[in] pu2_norm_adjust
 *  pointer to inverse scale matrix
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
void ih264_iquant_itrans_recon_4x4_sse42(WORD16 *pi2_src,
                                   UWORD8 *pu1_pred,
                                   UWORD8 *pu1_out,
                                   WORD32 pred_strd,
                                   WORD32 out_strd,
                                   const UWORD16 *pu2_iscal_mat,
                                   const UWORD16 *pu2_weigh_mat,
                                   UWORD32 u4_qp_div_6,
                                   WORD16 *pi2_tmp,
                                   WORD32 iq_start_idx,
                                   WORD16 *pi2_dc_ld_addr)
 {
    UWORD32 *pu4_out = (UWORD32 *) pu1_out;
    __m128i src_r0_r1, src_r2_r3;
    __m128i src_r0, src_r1, src_r2, src_r3;
    __m128i scalemat_r0_r1, scalemat_r2_r3;
    __m128i pred_r0, pred_r1, pred_r2, pred_r3;
    __m128i sign_reg, dequant_r0_r1, dequant_r2_r3;
    __m128i zero_8x16b = _mm_setzero_si128();          // all bits reset to zero
    __m128i temp0, temp1, temp2, temp3, temp4, temp5, temp6, temp7;
    __m128i resq_r0, resq_r1, resq_r2, resq_r3;
    __m128i add_rshift = _mm_set1_epi32((u4_qp_div_6 < 4) ? (1 << (3 - u4_qp_div_6)) : 0);
    __m128i value_32 = _mm_set1_epi32(32);
    UNUSED (pi2_tmp);

    /*************************************************************/
    /* Dequantization of coefficients. Will be replaced by SIMD  */
    /* operations on platform                                    */
    /*************************************************************/
    src_r0_r1 = _mm_loadu_si128((__m128i *) (pi2_src)); //a00 a01 a02 a03 a10 a11 a12 a13 -- the source matrix 0th,1st row
    src_r2_r3 = _mm_loadu_si128((__m128i *) (pi2_src + 8)); //a20 a21 a22 a23 a30 a31 a32 a33 -- the source matrix 2nd,3rd row
    scalemat_r0_r1 = _mm_loadu_si128((__m128i *) (pu2_iscal_mat)); //b00 b01 b02 b03 b10 b11 b12 b13 -- the scaling matrix 0th,1st row
    scalemat_r2_r3 = _mm_loadu_si128((__m128i *) (pu2_iscal_mat + 8)); //b20 b21 b22 b23 b30 b31 b32 b33 -- the scaling matrix 2nd,3rd row
    dequant_r0_r1 = _mm_loadu_si128((__m128i *) (pu2_weigh_mat)); //q00 q01 q02 q03 q10 q11 q12 q13 -- all 16 bits
    dequant_r2_r3 = _mm_loadu_si128((__m128i *) (pu2_weigh_mat + 8)); //q20 q21 q22 q23 q30 q31 q32 q33 -- all 16 bits

    temp0 = _mm_mullo_epi16(scalemat_r0_r1, dequant_r0_r1); //b00*q00 b01*q01 b02*q02 b03*q03 b10*q10 b11*q11 b12*q12 b13*q13 -- 16 bit result
    temp1 = _mm_mullo_epi16(scalemat_r2_r3, dequant_r2_r3); //b00*q00 b01*q01 b02*q02 b03*q03 b10*q10 b11*q11 b12*q12 b13*q13 -- 16 bit result

    temp4 = _mm_unpacklo_epi16(temp0, zero_8x16b); // b00*q00 0 b01*q01 0 b02*q02 0 b03*q03 0 -- 16 bit long
    temp5 = _mm_unpackhi_epi16(temp0, zero_8x16b); // b10*q10 0 b11*q11 0 b12*q12 0 b13*q13 0 -- 16 bit long
    temp6 = _mm_unpacklo_epi16(temp1, zero_8x16b); // b00*q00 0 b01*q01 0 b02*q02 0 b03*q03 0 -- 16 bit long
    temp7 = _mm_unpackhi_epi16(temp1, zero_8x16b); // b10*q10 0 b11*q11 0 b12*q12 0 b13*q13 0 -- 16 bit long

    src_r0 = _mm_unpacklo_epi16(src_r0_r1, zero_8x16b); // a00 0 a01 0 a02 0 a03 0 -- 16 bit long
    src_r1 = _mm_unpackhi_epi16(src_r0_r1, zero_8x16b); // a10 0 a11 0 a12 0 a13 0 -- 16 bit long
    src_r2 = _mm_unpacklo_epi16(src_r2_r3, zero_8x16b); // a20 0 a21 0 a22 0 a23 0 -- 16 bit long
    src_r3 = _mm_unpackhi_epi16(src_r2_r3, zero_8x16b); // a30 0 a31 0 a32 0 a33 0 -- 16 bit long

    temp4 = _mm_madd_epi16(src_r0, temp4); //a00*b00*q00 a10*b10*q10 a20*b20*q20 a30*b30 q30 -- 32 bits long
    temp5 = _mm_madd_epi16(src_r1, temp5);
    temp6 = _mm_madd_epi16(src_r2, temp6);
    temp7 = _mm_madd_epi16(src_r3, temp7);

    if (u4_qp_div_6 >= 4) {
        resq_r0 = _mm_slli_epi32(temp4, u4_qp_div_6 - 4);
        resq_r1 = _mm_slli_epi32(temp5, u4_qp_div_6 - 4);
        resq_r2 = _mm_slli_epi32(temp6, u4_qp_div_6 - 4);
        resq_r3 = _mm_slli_epi32(temp7, u4_qp_div_6 - 4);
    } else {
        temp4 = _mm_add_epi32(temp4, add_rshift);
        temp5 = _mm_add_epi32(temp5, add_rshift);
        temp6 = _mm_add_epi32(temp6, add_rshift);
        temp7 = _mm_add_epi32(temp7, add_rshift);
        resq_r0 = _mm_srai_epi32(temp4, 4 - u4_qp_div_6);
        resq_r1 = _mm_srai_epi32(temp5, 4 - u4_qp_div_6);
        resq_r2 = _mm_srai_epi32(temp6, 4 - u4_qp_div_6);
        resq_r3 = _mm_srai_epi32(temp7, 4 - u4_qp_div_6);
    }

    if (iq_start_idx == 1)
        resq_r0 = _mm_insert_epi32(resq_r0,(WORD32)pi2_dc_ld_addr[0],0);
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
    temp1 = _mm_unpacklo_epi32(resq_r0, resq_r1);                  //a0 b0 a1 b1
    temp3 = _mm_unpacklo_epi32(resq_r2, resq_r3);                  //c0 d0 c1 d1
    temp2 = _mm_unpackhi_epi32(resq_r0, resq_r1);                  //a2 b2 a3 b3
    temp4 = _mm_unpackhi_epi32(resq_r2, resq_r3);                  //c2 d2 c3 d3
    resq_r0 = _mm_unpacklo_epi64(temp1, temp3);                    //a0 b0 c0 d0
    resq_r1 = _mm_unpackhi_epi64(temp1, temp3);                    //a1 b1 c1 d1
    resq_r2 = _mm_unpacklo_epi64(temp2, temp4);                    //a2 b2 c2 d2
    resq_r3 = _mm_unpackhi_epi64(temp2, temp4);                    //a3 b3 c3 d3
    //Transform starts -- horizontal transform
    /*------------------------------------------------------------------*/
    /* z0 = w0 + w2                                             */
    temp0 = _mm_add_epi32(resq_r0, resq_r2);
    /* z1 = w0 - w2                                             */
    temp1 = _mm_sub_epi32(resq_r0, resq_r2);
    /* z2 = (w1 >> 1) - w3                                      */
    temp2 = _mm_srai_epi32(resq_r1, 1);                         //(w1>>1)
    temp2 = _mm_sub_epi32(temp2, resq_r3);                      //(w1>>1) - w3
    /* z3 = w1 + (w3 >> 1)                                      */
    temp3 = _mm_srai_epi32(resq_r3, 1);                         //(w3>>1) + w1
    temp3 = _mm_add_epi32(temp3, resq_r1);
    /*----------------------------------------------------------*/
    /* x0 = z0 + z3                                             */
    resq_r0 = _mm_add_epi32(temp0, temp3);
    /* x1 = z1 + z2                                             */
    resq_r1 = _mm_add_epi32(temp1, temp2);
    /* x2 = z1 - z2                                             */
    resq_r2 = _mm_sub_epi32(temp1, temp2);
    /* x3 = z0 - z3                                             */
    resq_r3 = _mm_sub_epi32(temp0, temp3);
    // Matrix transpose
    /*
     *  a0 b0 c0 d0
     *  a1 b1 c1 d1
     *  a2 b2 c2 d2
     *  a3 b3 c3 d3
     */
    temp1 = _mm_unpacklo_epi32(resq_r0, resq_r1);                  //a0 a1 b0 b1
    temp3 = _mm_unpacklo_epi32(resq_r2, resq_r3);                  //a2 a3 b2 b3
    temp2 = _mm_unpackhi_epi32(resq_r0, resq_r1);                  //c0 c1 d0 d1
    temp4 = _mm_unpackhi_epi32(resq_r2, resq_r3);                  //c2 c3 d2 d3
    resq_r0 = _mm_unpacklo_epi64(temp1, temp3);                    //a0 a1 a2 a3
    resq_r1 = _mm_unpackhi_epi64(temp1, temp3);                    //b0 b1 b2 b3
    resq_r2 = _mm_unpacklo_epi64(temp2, temp4);                    //c0 c1 c2 c3
    resq_r3 = _mm_unpackhi_epi64(temp2, temp4);                    //d0 d1 d2 d3
    //Transform ends -- horizontal transform

    //Load pred buffer
    pred_r0 = _mm_loadl_epi64((__m128i *) (&pu1_pred[0])); //p00 p01 p02 p03 0 0 0 0 0 0 0 0 -- all 8 bits
    pred_r1 = _mm_loadl_epi64((__m128i *) (&pu1_pred[pred_strd])); //p10 p11 p12 p13 0 0 0 0 0 0 0 0 -- all 8 bits
    pred_r2 = _mm_loadl_epi64((__m128i *) (&pu1_pred[2 * pred_strd])); //p20 p21 p22 p23 0 0 0 0 0 0 0 0 -- all 8 bits
    pred_r3 = _mm_loadl_epi64((__m128i *) (&pu1_pred[3 * pred_strd])); //p30 p31 p32 p33 0 0 0 0 0 0 0 0 -- all 8 bits

    pred_r0 = _mm_cvtepu8_epi32(pred_r0); //p00 p01 p02 p03 -- all 32 bits
    pred_r1 = _mm_cvtepu8_epi32(pred_r1); //p10 p11 p12 p13 -- all 32 bits
    pred_r2 = _mm_cvtepu8_epi32(pred_r2); //p20 p21 p22 p23 -- all 32 bits
    pred_r3 = _mm_cvtepu8_epi32(pred_r3); //p30 p31 p32 p33 -- all 32 bits

    /*--------------------------------------------------------------*/
    /* IDCT [ Vertical transformation] and Xij = (xij + 32)>>6      */
    /*                                                              */
    /* Add the prediction and store it back to same buffer          */
    /*--------------------------------------------------------------*/
    /* z0j = y0j + y2j                                                        */
    temp0 = _mm_add_epi32(resq_r0, resq_r2);
    /* z1j = y0j - y2j                                                        */
    temp1 = _mm_sub_epi32(resq_r0, resq_r2);
    /* z2j = (y1j>>1) - y3j                                                        */
    temp2 = _mm_srai_epi32(resq_r1, 1);                             //(y1j>>1)
    temp2 = _mm_sub_epi32(temp2, resq_r3);
    /* z3j = y1j + (y3j>>1)                                                        */
    temp3 = _mm_srai_epi32(resq_r3, 1);                             //(y3j>>1)
    temp3 = _mm_add_epi32(temp3, resq_r1);

    /* x0j = z0j + z3j                                                        */
    temp4 = _mm_add_epi32(temp0, temp3);
    temp4 = _mm_add_epi32(temp4, value_32);
    temp4 = _mm_srai_epi32(temp4, 6);
    temp4 = _mm_add_epi32(temp4, pred_r0);
    /* x1j = z1j + z2j                                                        */
    temp5 = _mm_add_epi32(temp1, temp2);
    temp5 = _mm_add_epi32(temp5, value_32);
    temp5 = _mm_srai_epi32(temp5, 6);
    temp5 = _mm_add_epi32(temp5, pred_r1);
    /* x2j = z1j - z2j                                                        */
    temp6 = _mm_sub_epi32(temp1, temp2);
    temp6 = _mm_add_epi32(temp6, value_32);
    temp6 = _mm_srai_epi32(temp6, 6);
    temp6 = _mm_add_epi32(temp6, pred_r2);
    /* x3j = z0j - z3j                                                        */
    temp7 = _mm_sub_epi32(temp0, temp3);
    temp7 = _mm_add_epi32(temp7, value_32);
    temp7 = _mm_srai_epi32(temp7, 6);
    temp7 = _mm_add_epi32(temp7, pred_r3);

    // 32-bit to 16-bit conversion
    temp0 = _mm_packs_epi32(temp4, temp5);
    temp1 = _mm_packs_epi32(temp6, temp7);
    /*------------------------------------------------------------------*/
    //Clipping the results to 8 bits
    sign_reg = _mm_cmpgt_epi16(temp0, zero_8x16b);      // sign check
    temp0 = _mm_and_si128(temp0, sign_reg);
    sign_reg = _mm_cmpgt_epi16(temp1, zero_8x16b);
    temp1 = _mm_and_si128(temp1, sign_reg);

    resq_r0 = _mm_packus_epi16(temp0, temp1);
    resq_r1 = _mm_srli_si128(resq_r0, 4);
    resq_r2 = _mm_srli_si128(resq_r1, 4);
    resq_r3 = _mm_srli_si128(resq_r2, 4);

    *pu4_out = _mm_cvtsi128_si32(resq_r0);
    pu1_out += out_strd;
    pu4_out = (UWORD32 *) (pu1_out);
    *(pu4_out) = _mm_cvtsi128_si32(resq_r1);
    pu1_out += out_strd;
    pu4_out = (UWORD32 *) (pu1_out);
    *(pu4_out) = _mm_cvtsi128_si32(resq_r2);
    pu1_out += out_strd;
    pu4_out = (UWORD32 *) (pu1_out);
    *(pu4_out) = _mm_cvtsi128_si32(resq_r3);
}

/*
 ********************************************************************************
 *
 * @brief This function reconstructs a 4x4 sub block from quantized chroma resiude and
 * prediction buffer
 *
 * @par Description:
 *  The quantized residue is first inverse quantized, then inverse transformed.
 *  This inverse transformed content is added to the prediction buffer to recon-
 *  struct the end output
 *
 * @param[in] pi2_src
 *  quantized 4x4 block
 *
 * @param[in] pu1_pred
 *  prediction 4x4 block
 *
 * @param[out] pu1_out
 *  reconstructed 4x4 block
 *
 * @param[in] src_strd
 *  quantization buffer stride
 *
 * @param[in] pred_strd,
 *  Prediction buffer stride
 *
 * @param[in] out_strd
 *  recon buffer Stride
 *
 * @param[in] pu2_scaling_list
 *  pointer to scaling list
 *
 * @param[in] pu2_norm_adjust
 *  pointer to inverse scale matrix
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
void ih264_iquant_itrans_recon_chroma_4x4_sse42(WORD16 *pi2_src,
                                   UWORD8 *pu1_pred,
                                   UWORD8 *pu1_out,
                                   WORD32 pred_strd,
                                   WORD32 out_strd,
                                   const UWORD16 *pu2_iscal_mat,
                                   const UWORD16 *pu2_weigh_mat,
                                   UWORD32 u4_qp_div_6,
                                   WORD16 *pi2_tmp,
                                   WORD16 *pi2_dc_ld_addr)
 {
    __m128i src_r0_r1, src_r2_r3;
    __m128i src_r0, src_r1, src_r2, src_r3;
    __m128i scalemat_r0_r1, scalemat_r2_r3;
    __m128i pred_r0, pred_r1, pred_r2, pred_r3;
    __m128i sign_reg, dequant_r0_r1, dequant_r2_r3;
    __m128i zero_8x16b = _mm_setzero_si128();          // all bits reset to zero
    __m128i temp0, temp1, temp2, temp3, temp4, temp5, temp6, temp7;
    __m128i resq_r0, resq_r1, resq_r2, resq_r3;
    __m128i add_rshift = _mm_set1_epi32((u4_qp_div_6 < 4) ? (1 << (3 - u4_qp_div_6)) : 0);
    __m128i value_32 = _mm_set1_epi32(32);
    __m128i chroma_mask = _mm_set1_epi16 (0xFF);
    __m128i out_r0, out_r1, out_r2, out_r3;
    UNUSED (pi2_tmp);

    /*************************************************************/
    /* Dequantization of coefficients. Will be replaced by SIMD  */
    /* operations on platform                                    */
    /*************************************************************/
    src_r0_r1 = _mm_loadu_si128((__m128i *) (pi2_src)); //a00 a01 a02 a03 a10 a11 a12 a13 -- the source matrix 0th,1st row
    src_r2_r3 = _mm_loadu_si128((__m128i *) (pi2_src + 8)); //a20 a21 a22 a23 a30 a31 a32 a33 -- the source matrix 2nd,3rd row
    scalemat_r0_r1 = _mm_loadu_si128((__m128i *) (pu2_iscal_mat)); //b00 b01 b02 b03 b10 b11 b12 b13 -- the scaling matrix 0th,1st row
    scalemat_r2_r3 = _mm_loadu_si128((__m128i *) (pu2_iscal_mat + 8)); //b20 b21 b22 b23 b30 b31 b32 b33 -- the scaling matrix 2nd,3rd row
    dequant_r0_r1 = _mm_loadu_si128((__m128i *) (pu2_weigh_mat)); //q00 q01 q02 q03 q10 q11 q12 q13 -- all 16 bits
    dequant_r2_r3 = _mm_loadu_si128((__m128i *) (pu2_weigh_mat + 8)); //q20 q21 q22 q23 q30 q31 q32 q33 -- all 16 bits

    temp0 = _mm_mullo_epi16(scalemat_r0_r1, dequant_r0_r1); //b00*q00 b01*q01 b02*q02 b03*q03 b10*q10 b11*q11 b12*q12 b13*q13 -- 16 bit result
    temp1 = _mm_mullo_epi16(scalemat_r2_r3, dequant_r2_r3); //b00*q00 b01*q01 b02*q02 b03*q03 b10*q10 b11*q11 b12*q12 b13*q13 -- 16 bit result

    temp4 = _mm_unpacklo_epi16(temp0, zero_8x16b); // b00*q00 0 b01*q01 0 b02*q02 0 b03*q03 0 -- 16 bit long
    temp5 = _mm_unpackhi_epi16(temp0, zero_8x16b); // b10*q10 0 b11*q11 0 b12*q12 0 b13*q13 0 -- 16 bit long
    temp6 = _mm_unpacklo_epi16(temp1, zero_8x16b); // b00*q00 0 b01*q01 0 b02*q02 0 b03*q03 0 -- 16 bit long
    temp7 = _mm_unpackhi_epi16(temp1, zero_8x16b); // b10*q10 0 b11*q11 0 b12*q12 0 b13*q13 0 -- 16 bit long

    src_r0 = _mm_unpacklo_epi16(src_r0_r1, zero_8x16b); // a00 0 a01 0 a02 0 a03 0 -- 16 bit long
    src_r1 = _mm_unpackhi_epi16(src_r0_r1, zero_8x16b); // a10 0 a11 0 a12 0 a13 0 -- 16 bit long
    src_r2 = _mm_unpacklo_epi16(src_r2_r3, zero_8x16b); // a20 0 a21 0 a22 0 a23 0 -- 16 bit long
    src_r3 = _mm_unpackhi_epi16(src_r2_r3, zero_8x16b); // a30 0 a31 0 a32 0 a33 0 -- 16 bit long

    temp4 = _mm_madd_epi16(src_r0, temp4); //a00*b00*q00 a10*b10*q10 a20*b20*q20 a30*b30 q30 -- 32 bits long
    temp5 = _mm_madd_epi16(src_r1, temp5);
    temp6 = _mm_madd_epi16(src_r2, temp6);
    temp7 = _mm_madd_epi16(src_r3, temp7);

    if (u4_qp_div_6 >= 4) {
        resq_r0 = _mm_slli_epi32(temp4, u4_qp_div_6 - 4);
        resq_r1 = _mm_slli_epi32(temp5, u4_qp_div_6 - 4);
        resq_r2 = _mm_slli_epi32(temp6, u4_qp_div_6 - 4);
        resq_r3 = _mm_slli_epi32(temp7, u4_qp_div_6 - 4);
    } else {
        temp4 = _mm_add_epi32(temp4, add_rshift);
        temp5 = _mm_add_epi32(temp5, add_rshift);
        temp6 = _mm_add_epi32(temp6, add_rshift);
        temp7 = _mm_add_epi32(temp7, add_rshift);
        resq_r0 = _mm_srai_epi32(temp4, 4 - u4_qp_div_6);
        resq_r1 = _mm_srai_epi32(temp5, 4 - u4_qp_div_6);
        resq_r2 = _mm_srai_epi32(temp6, 4 - u4_qp_div_6);
        resq_r3 = _mm_srai_epi32(temp7, 4 - u4_qp_div_6);
    }

    resq_r0 = _mm_insert_epi32(resq_r0,(WORD32)pi2_dc_ld_addr[0],0);
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
    temp1 = _mm_unpacklo_epi32(resq_r0, resq_r1);                  //a0 b0 a1 b1
    temp3 = _mm_unpacklo_epi32(resq_r2, resq_r3);                  //c0 d0 c1 d1
    temp2 = _mm_unpackhi_epi32(resq_r0, resq_r1);                  //a2 b2 a3 b3
    temp4 = _mm_unpackhi_epi32(resq_r2, resq_r3);                  //c2 d2 c3 d3
    resq_r0 = _mm_unpacklo_epi64(temp1, temp3);                    //a0 b0 c0 d0
    resq_r1 = _mm_unpackhi_epi64(temp1, temp3);                    //a1 b1 c1 d1
    resq_r2 = _mm_unpacklo_epi64(temp2, temp4);                    //a2 b2 c2 d2
    resq_r3 = _mm_unpackhi_epi64(temp2, temp4);                    //a3 b3 c3 d3
    //Transform starts -- horizontal transform
    /*------------------------------------------------------------------*/
    /* z0 = w0 + w2                                             */
    temp0 = _mm_add_epi32(resq_r0, resq_r2);
    /* z1 = w0 - w2                                             */
    temp1 = _mm_sub_epi32(resq_r0, resq_r2);
    /* z2 = (w1 >> 1) - w3                                      */
    temp2 = _mm_srai_epi32(resq_r1, 1);                         //(w1>>1)
    temp2 = _mm_sub_epi32(temp2, resq_r3);                      //(w1>>1) - w3
    /* z3 = w1 + (w3 >> 1)                                      */
    temp3 = _mm_srai_epi32(resq_r3, 1);                         //(w3>>1) + w1
    temp3 = _mm_add_epi32(temp3, resq_r1);
    /*----------------------------------------------------------*/
    /* x0 = z0 + z3                                             */
    resq_r0 = _mm_add_epi32(temp0, temp3);
    /* x1 = z1 + z2                                             */
    resq_r1 = _mm_add_epi32(temp1, temp2);
    /* x2 = z1 - z2                                             */
    resq_r2 = _mm_sub_epi32(temp1, temp2);
    /* x3 = z0 - z3                                             */
    resq_r3 = _mm_sub_epi32(temp0, temp3);
    // Matrix transpose
    /*
     *  a0 b0 c0 d0
     *  a1 b1 c1 d1
     *  a2 b2 c2 d2
     *  a3 b3 c3 d3
     */
    temp1 = _mm_unpacklo_epi32(resq_r0, resq_r1);                  //a0 a1 b0 b1
    temp3 = _mm_unpacklo_epi32(resq_r2, resq_r3);                  //a2 a3 b2 b3
    temp2 = _mm_unpackhi_epi32(resq_r0, resq_r1);                  //c0 c1 d0 d1
    temp4 = _mm_unpackhi_epi32(resq_r2, resq_r3);                  //c2 c3 d2 d3
    resq_r0 = _mm_unpacklo_epi64(temp1, temp3);                    //a0 a1 a2 a3
    resq_r1 = _mm_unpackhi_epi64(temp1, temp3);                    //b0 b1 b2 b3
    resq_r2 = _mm_unpacklo_epi64(temp2, temp4);                    //c0 c1 c2 c3
    resq_r3 = _mm_unpackhi_epi64(temp2, temp4);                    //d0 d1 d2 d3
    //Transform ends -- horizontal transform

    //Load pred buffer
    pred_r0 = _mm_loadl_epi64((__m128i *) (&pu1_pred[0])); //p00 p01 p02 p03 0 0 0 0 0 0 0 0 -- all 8 bits
    pred_r1 = _mm_loadl_epi64((__m128i *) (&pu1_pred[pred_strd])); //p10 p11 p12 p13 0 0 0 0 0 0 0 0 -- all 8 bits
    pred_r2 = _mm_loadl_epi64((__m128i *) (&pu1_pred[2 * pred_strd])); //p20 p21 p22 p23 0 0 0 0 0 0 0 0 -- all 8 bits
    pred_r3 = _mm_loadl_epi64((__m128i *) (&pu1_pred[3 * pred_strd])); //p30 p31 p32 p33 0 0 0 0 0 0 0 0 -- all 8 bits

    pred_r0 = _mm_and_si128(pred_r0, chroma_mask);
    pred_r1 = _mm_and_si128(pred_r1, chroma_mask);
    pred_r2 = _mm_and_si128(pred_r2, chroma_mask);
    pred_r3 = _mm_and_si128(pred_r3, chroma_mask);

    pred_r0 = _mm_cvtepu16_epi32(pred_r0); //p00 p01 p02 p03 -- all 32 bits
    pred_r1 = _mm_cvtepu16_epi32(pred_r1); //p10 p11 p12 p13 -- all 32 bits
    pred_r2 = _mm_cvtepu16_epi32(pred_r2); //p20 p21 p22 p23 -- all 32 bits
    pred_r3 = _mm_cvtepu16_epi32(pred_r3); //p30 p31 p32 p33 -- all 32 bits

    /*--------------------------------------------------------------*/
    /* IDCT [ Vertical transformation] and Xij = (xij + 32)>>6      */
    /*                                                              */
    /* Add the prediction and store it back to same buffer          */
    /*--------------------------------------------------------------*/
    /* z0j = y0j + y2j                                                        */
    temp0 = _mm_add_epi32(resq_r0, resq_r2);
    /* z1j = y0j - y2j                                                        */
    temp1 = _mm_sub_epi32(resq_r0, resq_r2);
    /* z2j = (y1j>>1) - y3j                                                        */
    temp2 = _mm_srai_epi32(resq_r1, 1);                             //(y1j>>1)
    temp2 = _mm_sub_epi32(temp2, resq_r3);
    /* z3j = y1j + (y3j>>1)                                                        */
    temp3 = _mm_srai_epi32(resq_r3, 1);                             //(y3j>>1)
    temp3 = _mm_add_epi32(temp3, resq_r1);

    /* x0j = z0j + z3j                                                        */
    temp4 = _mm_add_epi32(temp0, temp3);
    temp4 = _mm_add_epi32(temp4, value_32);
    temp4 = _mm_srai_epi32(temp4, 6);
    temp4 = _mm_add_epi32(temp4, pred_r0);
    /* x1j = z1j + z2j                                                        */
    temp5 = _mm_add_epi32(temp1, temp2);
    temp5 = _mm_add_epi32(temp5, value_32);
    temp5 = _mm_srai_epi32(temp5, 6);
    temp5 = _mm_add_epi32(temp5, pred_r1);
    /* x2j = z1j - z2j                                                        */
    temp6 = _mm_sub_epi32(temp1, temp2);
    temp6 = _mm_add_epi32(temp6, value_32);
    temp6 = _mm_srai_epi32(temp6, 6);
    temp6 = _mm_add_epi32(temp6, pred_r2);
    /* x3j = z0j - z3j                                                        */
    temp7 = _mm_sub_epi32(temp0, temp3);
    temp7 = _mm_add_epi32(temp7, value_32);
    temp7 = _mm_srai_epi32(temp7, 6);
    temp7 = _mm_add_epi32(temp7, pred_r3);

    // 32-bit to 16-bit conversion
    temp0 = _mm_packs_epi32(temp4, temp5);
    temp1 = _mm_packs_epi32(temp6, temp7);
    /*------------------------------------------------------------------*/
    //Clipping the results to 8 bits
    sign_reg = _mm_cmpgt_epi16(temp0, zero_8x16b);      // sign check
    temp0 = _mm_and_si128(temp0, sign_reg);
    sign_reg = _mm_cmpgt_epi16(temp1, zero_8x16b);
    temp1 = _mm_and_si128(temp1, sign_reg);

    resq_r0 = _mm_packus_epi16(temp0, temp1);
    resq_r1 = _mm_srli_si128(resq_r0, 4);
    resq_r2 = _mm_srli_si128(resq_r1, 4);
    resq_r3 = _mm_srli_si128(resq_r2, 4);

    resq_r0 = _mm_cvtepu8_epi16(resq_r0); //p00 p01 p02 p03 -- all 16 bits
    resq_r1 = _mm_cvtepu8_epi16(resq_r1); //p10 p11 p12 p13 -- all 16 bits
    resq_r2 = _mm_cvtepu8_epi16(resq_r2); //p20 p21 p22 p23 -- all 16 bits
    resq_r3 = _mm_cvtepu8_epi16(resq_r3); //p30 p31 p32 p33 -- all 16 bits

    chroma_mask = _mm_set1_epi16 (0xFF00);
    out_r0 = _mm_loadl_epi64((__m128i *) (&pu1_out[0]));
    out_r1 = _mm_loadl_epi64((__m128i *) (&pu1_out[out_strd]));
    out_r2 = _mm_loadl_epi64((__m128i *) (&pu1_out[2 * out_strd]));
    out_r3 = _mm_loadl_epi64((__m128i *) (&pu1_out[3 * out_strd]));

    out_r0 = _mm_and_si128(out_r0, chroma_mask);
    out_r1 = _mm_and_si128(out_r1, chroma_mask);
    out_r2 = _mm_and_si128(out_r2, chroma_mask);
    out_r3 = _mm_and_si128(out_r3, chroma_mask);

    out_r0 = _mm_add_epi8(out_r0, resq_r0);
    out_r1 = _mm_add_epi8(out_r1, resq_r1);
    out_r2 = _mm_add_epi8(out_r2, resq_r2);
    out_r3 = _mm_add_epi8(out_r3, resq_r3);

    _mm_storel_epi64((__m128i *)(&pu1_out[0]), out_r0);
    _mm_storel_epi64((__m128i *)(&pu1_out[out_strd]), out_r1);
    _mm_storel_epi64((__m128i *)(&pu1_out[2 * out_strd]), out_r2);
    _mm_storel_epi64((__m128i *)(&pu1_out[3 * out_strd]), out_r3);
}
