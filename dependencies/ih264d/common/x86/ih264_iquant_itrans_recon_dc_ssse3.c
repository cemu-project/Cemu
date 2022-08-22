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
 *  ih264_iquant_itrans_recon_dc_ssse3.c
 *
 * @brief
 *  Contains function definitions for inverse  quantization, inverse
 * transform and reconstruction
 *
 * @author
 *  Mohit [100664]
 *
 * @par List of Functions:
 *  - ih264_iquant_itrans_recon_4x4_dc_ssse3()
 *  - ih264_iquant_itrans_recon_8x8_dc_ssse3()
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
 * prediction buffer for dc input pattern only, i.e. only the (0,0) element of the input
 * 4x4 block is non-zero. For complete function, refer ih264_iquant_itrans_recon_ssse3.c
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
void ih264_iquant_itrans_recon_4x4_dc_ssse3(WORD16 *pi2_src,
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
    UWORD32 *pu4_out = (UWORD32 *)pu1_out;
    WORD32 q0 = pi2_src[0];
    WORD16 i_macro, rnd_fact = (u4_qp_div_6 < 4) ? 1 << (3 - u4_qp_div_6) : 0;

    __m128i predload_r,pred_r0, pred_r1, pred_r2, pred_r3;
    __m128i sign_reg;
    __m128i zero_8x16b = _mm_setzero_si128();          // all bits reset to zero
    __m128i temp4, temp5, temp6, temp7;
    __m128i value_add;

    UNUSED (pi2_tmp);

    INV_QUANT(q0, pu2_iscal_mat[0], pu2_weigh_mat[0], u4_qp_div_6, rnd_fact, 4);

    if (iq_start_idx != 0 )
        q0 = pi2_dc_ld_addr[0];     // Restoring dc value for intra case

    i_macro = ((q0 + 32) >> 6);

    value_add = _mm_set1_epi16(i_macro);

    zero_8x16b = _mm_setzero_si128();                  // all bits reset to zero
    //Load pred buffer
    predload_r = _mm_loadl_epi64((__m128i *) (&pu1_pred[0])); //p00 p01 p02 p03 0 0 0 0 0 0 0 0 -- all 8 bits
    pred_r0 = _mm_unpacklo_epi8(predload_r, zero_8x16b); //p00 p01 p02 p03 0 0 0 0 -- all 16 bits
    predload_r = _mm_loadl_epi64((__m128i *) (&pu1_pred[pred_strd])); //p10 p11 p12 p13 0 0 0 0 0 0 0 0 -- all 8 bits
    pred_r1 = _mm_unpacklo_epi8(predload_r, zero_8x16b); //p10 p11 p12 p13 0 0 0 0 -- all 16 bits
    predload_r = _mm_loadl_epi64((__m128i *) (&pu1_pred[2*pred_strd])); //p20 p21 p22 p23 0 0 0 0 0 0 0 0 -- all 8 bits
    pred_r2 = _mm_unpacklo_epi8(predload_r, zero_8x16b); //p20 p21 p22 p23 0 0 0 0 -- all 16 bits
    predload_r = _mm_loadl_epi64((__m128i *) (&pu1_pred[3*pred_strd])); //p30 p31 p32 p33 0 0 0 0 0 0 0 0 -- all 8 bits
    pred_r3 = _mm_unpacklo_epi8(predload_r, zero_8x16b); //p30 p31 p32 p33 0 0 0 0 -- all 16 bits

    pred_r0 = _mm_unpacklo_epi64(pred_r0, pred_r1); //p00 p01 p02 p03 p10 p11 p12 p13
    pred_r2 = _mm_unpacklo_epi64(pred_r2, pred_r3); //p20 p21 p22p p23 p30 p31 p32 p33

    temp4 = _mm_add_epi16(value_add, pred_r0);
    temp5 = _mm_add_epi16(value_add, pred_r2);
    /*------------------------------------------------------------------*/
    //Clipping the results to 8 bits
    sign_reg = _mm_cmpgt_epi16(temp4, zero_8x16b);                 // sign check
    temp4 = _mm_and_si128(temp4, sign_reg);
    sign_reg = _mm_cmpgt_epi16(temp5, zero_8x16b);                 // sign check
    temp5 = _mm_and_si128(temp5, sign_reg);

    temp4 = _mm_packus_epi16(temp4,temp5);
    temp5 = _mm_srli_si128(temp4,4);
    temp6 = _mm_srli_si128(temp5,4);
    temp7 = _mm_srli_si128(temp6,4);

    *pu4_out = _mm_cvtsi128_si32(temp4);
    pu1_out += out_strd;
    pu4_out = (UWORD32 *)(pu1_out);
    *(pu4_out) = _mm_cvtsi128_si32(temp5);
    pu1_out += out_strd;
    pu4_out = (UWORD32 *)(pu1_out);
    *(pu4_out) = _mm_cvtsi128_si32(temp6);
    pu1_out += out_strd;
    pu4_out = (UWORD32 *)(pu1_out);
    *(pu4_out) = _mm_cvtsi128_si32(temp7);
}
/**
 *******************************************************************************
 *
 * @brief
 *  This function performs inverse quant and Inverse transform type Ci4 for 8x8 block
 *  for dc input pattern only, i.e. only the (0,0) element of the input 8x8 block is
 *  non-zero. For complete function, refer ih264_iquant_itrans_recon_ssse3.c
 *
 * @par Description:
 *  Performs inverse transform Ci8 and adds the residue to get the
 *  reconstructed block
 *
 * @param[in] pi2_src
 *  Input 8x8coefficients
 *
 * @param[in] pu1_pred
 *  Prediction 8x8 block
 *
 * @param[out] pu1_recon
 *  Output 8x8 block
 *
 * @param[in] q_div
 *  QP/6
 *
 * @param[in] q_rem
 *  QP%6
 *
 * @param[in] q_lev
 *  Quantizer level
 *
 * @param[in] u4_src_stride
 *  Input stride
 *
 * @param[in] u4_pred_stride,
 *  Prediction stride
 *
 * @param[in] u4_out_stride
 *  Output Stride
 *
 * @param[in] pi4_tmp
 *  temporary buffer of size 1*64
 *  the tmp for each block
 *
 * @param[in] pu4_iquant_mat
 *  Pointer to the inverse quantization matrix
 *
 * @returns  Void
 *
 * @remarks
 *  None
 *
 *******************************************************************************
 */

void ih264_iquant_itrans_recon_8x8_dc_ssse3 (WORD16 *pi2_src,
                                         UWORD8 *pu1_pred,
                                         UWORD8 *pu1_out,
                                         WORD32 pred_strd,
                                         WORD32 out_strd,
                                         const UWORD16 *pu2_iscale_mat,
                                         const UWORD16 *pu2_weigh_mat,
                                         UWORD32 qp_div,
                                         WORD16 *pi2_tmp,
                                         WORD32 iq_start_idx,
                                         WORD16 *pi2_dc_ld_addr)
{
    WORD32 q0 = pi2_src[0];
    WORD16 i_macro, rnd_fact = (qp_div < 6) ? 1 << (5 - qp_div) : 0;

    __m128i predload_r,pred_r0, pred_r1, pred_r2, pred_r3,pred_r4,pred_r5,pred_r6,pred_r7;
    __m128i sign_reg;
    __m128i zero_8x16b = _mm_setzero_si128();          // all bits reset to zero
    __m128i temp1,temp2,temp3,temp4, temp5, temp6, temp7,temp8;
    __m128i value_add;

    UNUSED (pi2_tmp);
    UNUSED (iq_start_idx);
    UNUSED (pi2_dc_ld_addr);

    INV_QUANT(q0, pu2_iscale_mat[0], pu2_weigh_mat[0], qp_div, rnd_fact, 6);
    i_macro = ((q0 + 32) >> 6);

    value_add = _mm_set1_epi16(i_macro);

    //Load pred buffer row 0
    predload_r = _mm_loadl_epi64((__m128i *)(&pu1_pred[0])); //p0 p1 p2 p3 p4 p5 p6 p7 0 0 0 0 0 0 0 0 -- all 8 bits
    pred_r0 = _mm_unpacklo_epi8(predload_r, zero_8x16b); //p0 p1 p2 p3 p4 p5 p6 p7 -- all 16 bits
    //Load pred buffer row 1
    predload_r = _mm_loadl_epi64((__m128i *)(&pu1_pred[pred_strd])); //p0 p1 p2 p3 p4 p5 p6 p7 0 0 0 0 0 0 0 0 -- all 8 bits
    pred_r1 = _mm_unpacklo_epi8(predload_r, zero_8x16b); //p0 p1 p2 p3 p4 p5 p6 p7 -- all 16 bits
    //Load pred buffer row 2
    predload_r = _mm_loadl_epi64(
                    (__m128i *)(&pu1_pred[2 * pred_strd])); //p0 p1 p2 p3 p4 p5 p6 p7 0 0 0 0 0 0 0 0 -- all 8 bits
    pred_r2 = _mm_unpacklo_epi8(predload_r, zero_8x16b); //p0 p1 p2 p3 p4 p5 p6 p7 -- all 16 bits
    //Load pred buffer row 3
    predload_r = _mm_loadl_epi64(
                    (__m128i *)(&pu1_pred[3 * pred_strd])); //p0 p1 p2 p3 p4 p5 p6 p7 0 0 0 0 0 0 0 0 -- all 8 bits
    pred_r3 = _mm_unpacklo_epi8(predload_r, zero_8x16b); //p0 p1 p2 p3 p4 p5 p6 p7 -- all 16 bits
    //Load pred buffer row 4
    predload_r = _mm_loadl_epi64(
                    (__m128i *)(&pu1_pred[4 * pred_strd])); //p0 p1 p2 p3 p4 p5 p6 p7 0 0 0 0 0 0 0 0 -- all 8 bits
    pred_r4 = _mm_unpacklo_epi8(predload_r, zero_8x16b); //p0 p1 p2 p3 p4 p5 p6 p7 -- all 16 bits
    //Load pred buffer row 5
    predload_r = _mm_loadl_epi64(
                    (__m128i *)(&pu1_pred[5 * pred_strd])); //p0 p1 p2 p3 p4 p5 p6 p7 0 0 0 0 0 0 0 0 -- all 8 bit
    pred_r5 = _mm_unpacklo_epi8(predload_r, zero_8x16b); //p0 p1 p2 p3 p4 p5 p6 p7 -- all 16 bits
    //Load pred buffer row 6
    predload_r = _mm_loadl_epi64(
                    (__m128i *)(&pu1_pred[6 * pred_strd])); //p0 p1 p2 p3 p4 p5 p6 p7 0 0 0 0 0 0 0 0 -- all 8 bits
    pred_r6 = _mm_unpacklo_epi8(predload_r, zero_8x16b); //p0 p1 p2 p3 p4 p5 p6 p7 -- all 16 bits
    //Load pred buffer row 7
    predload_r = _mm_loadl_epi64(
                    (__m128i *)(&pu1_pred[7 * pred_strd])); //p0 p1 p2 p3 p4 p5 p6 p7 0 0 0 0 0 0 0 0 -- all 8 bits
    pred_r7 = _mm_unpacklo_epi8(predload_r, zero_8x16b); //p0 p1 p2 p3 p4 p5 p6 p7 -- all 16 bits

    temp1 = _mm_add_epi16(value_add, pred_r0);

    temp2 = _mm_add_epi16(value_add, pred_r1);

    temp3 = _mm_add_epi16(value_add, pred_r2);

    temp4 = _mm_add_epi16(value_add, pred_r3);

    temp5 = _mm_add_epi16(value_add, pred_r4);

    temp6 = _mm_add_epi16(value_add, pred_r5);

    temp7 = _mm_add_epi16(value_add, pred_r6);

    temp8 = _mm_add_epi16(value_add, pred_r7);
    /*------------------------------------------------------------------*/
    //Clipping the results to 8 bits
    sign_reg = _mm_cmpgt_epi16(temp1, zero_8x16b); // sign check
    temp1 = _mm_and_si128(temp1, sign_reg);
    sign_reg = _mm_cmpgt_epi16(temp2, zero_8x16b); // sign check
    temp2 = _mm_and_si128(temp2, sign_reg);
    sign_reg = _mm_cmpgt_epi16(temp3, zero_8x16b); // sign check
    temp3 = _mm_and_si128(temp3, sign_reg);
    sign_reg = _mm_cmpgt_epi16(temp4, zero_8x16b); // sign check
    temp4 = _mm_and_si128(temp4, sign_reg);
    sign_reg = _mm_cmpgt_epi16(temp5, zero_8x16b); // sign check
    temp5 = _mm_and_si128(temp5, sign_reg);
    sign_reg = _mm_cmpgt_epi16(temp6, zero_8x16b); // sign check
    temp6 = _mm_and_si128(temp6, sign_reg);
    sign_reg = _mm_cmpgt_epi16(temp7, zero_8x16b); // sign check
    temp7 = _mm_and_si128(temp7, sign_reg);
    sign_reg = _mm_cmpgt_epi16(temp8, zero_8x16b); // sign check
    temp8 = _mm_and_si128(temp8, sign_reg);

    temp1 = _mm_packus_epi16(temp1, zero_8x16b);
    temp2 = _mm_packus_epi16(temp2, zero_8x16b);
    temp3 = _mm_packus_epi16(temp3, zero_8x16b);
    temp4 = _mm_packus_epi16(temp4, zero_8x16b);
    temp5 = _mm_packus_epi16(temp5, zero_8x16b);
    temp6 = _mm_packus_epi16(temp6, zero_8x16b);
    temp7 = _mm_packus_epi16(temp7, zero_8x16b);
    temp8 = _mm_packus_epi16(temp8, zero_8x16b);

    _mm_storel_epi64((__m128i *)(&pu1_out[0]), temp1);
    _mm_storel_epi64((__m128i *)(&pu1_out[out_strd]), temp2);
    _mm_storel_epi64((__m128i *)(&pu1_out[2 * out_strd]), temp3);
    _mm_storel_epi64((__m128i *)(&pu1_out[3 * out_strd]), temp4);
    _mm_storel_epi64((__m128i *)(&pu1_out[4 * out_strd]), temp5);
    _mm_storel_epi64((__m128i *)(&pu1_out[5 * out_strd]), temp6);
    _mm_storel_epi64((__m128i *)(&pu1_out[6 * out_strd]), temp7);
    _mm_storel_epi64((__m128i *)(&pu1_out[7 * out_strd]), temp8);
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
void ih264_iquant_itrans_recon_chroma_4x4_dc_ssse3(WORD16 *pi2_src,
                                   UWORD8 *pu1_pred,
                                   UWORD8 *pu1_out,
                                   WORD32 pred_strd,
                                   WORD32 out_strd,
                                   const UWORD16 *pu2_iscal_mat,
                                   const UWORD16 *pu2_weigh_mat,
                                   UWORD32 u4_qp_div_6,
                                   WORD16 *pi2_tmp,
                                   WORD16 *pi2_dc_src)
 {
    WORD16 q0 = pi2_dc_src[0];      // DC value won't be dequantized for chroma inverse transform
    WORD16 i_macro = ((q0 + 32) >> 6);

    __m128i pred_r0, pred_r1, pred_r2, pred_r3, sign_reg;
    __m128i zero_8x16b = _mm_setzero_si128();          // all bits reset to zero
    __m128i chroma_mask = _mm_set1_epi16 (0xFF);
    __m128i value_add = _mm_set1_epi16(i_macro);
    __m128i out_r0, out_r1, out_r2, out_r3;

    UNUSED (pi2_src);
    UNUSED (pu2_iscal_mat);
    UNUSED (pu2_weigh_mat);
    UNUSED (u4_qp_div_6);
    UNUSED (pi2_tmp);

    //Load pred buffer
    pred_r0 = _mm_loadl_epi64((__m128i *) (&pu1_pred[0])); //p00 p01 p02 p03 0 0 0 0 0 0 0 0 -- all 8 bits
    pred_r1 = _mm_loadl_epi64((__m128i *) (&pu1_pred[pred_strd])); //p10 p11 p12 p13 0 0 0 0 0 0 0 0 -- all 8 bits
    pred_r2 = _mm_loadl_epi64((__m128i *) (&pu1_pred[2 * pred_strd])); //p20 p21 p22 p23 0 0 0 0 0 0 0 0 -- all 8 bits
    pred_r3 = _mm_loadl_epi64((__m128i *) (&pu1_pred[3 * pred_strd])); //p30 p31 p32 p33 0 0 0 0 0 0 0 0 -- all 8 bits

    pred_r0 = _mm_and_si128(pred_r0, chroma_mask);
    pred_r1 = _mm_and_si128(pred_r1, chroma_mask);
    pred_r2 = _mm_and_si128(pred_r2, chroma_mask);
    pred_r3 = _mm_and_si128(pred_r3, chroma_mask);

    pred_r0 = _mm_unpacklo_epi64(pred_r0, pred_r1); //p00 p01 p02 p03 p10 p11 p12 p13
    pred_r2 = _mm_unpacklo_epi64(pred_r2, pred_r3); //p20 p21 p22p p23 p30 p31 p32 p33

    pred_r0 = _mm_add_epi16(value_add, pred_r0);
    pred_r2 = _mm_add_epi16(value_add, pred_r2);

    /*------------------------------------------------------------------*/
    //Clipping the results to 8 bits
    sign_reg = _mm_cmpgt_epi16(pred_r0, zero_8x16b);        // sign check
    pred_r0 = _mm_and_si128(pred_r0, sign_reg);
    sign_reg = _mm_cmpgt_epi16(pred_r2, zero_8x16b);
    pred_r2 = _mm_and_si128(pred_r2, sign_reg);

    pred_r0 = _mm_packus_epi16(pred_r0, pred_r2);
    pred_r1 = _mm_srli_si128(pred_r0, 4);
    pred_r2 = _mm_srli_si128(pred_r1, 4);
    pred_r3 = _mm_srli_si128(pred_r2, 4);

    pred_r0 = _mm_unpacklo_epi8(pred_r0, zero_8x16b); //p00 p01 p02 p03 -- all 16 bits
    pred_r1 = _mm_unpacklo_epi8(pred_r1, zero_8x16b); //p10 p11 p12 p13 -- all 16 bits
    pred_r2 = _mm_unpacklo_epi8(pred_r2, zero_8x16b); //p20 p21 p22 p23 -- all 16 bits
    pred_r3 = _mm_unpacklo_epi8(pred_r3, zero_8x16b); //p30 p31 p32 p33 -- all 16 bits

    chroma_mask = _mm_set1_epi16 (0xFF00);
    out_r0 = _mm_loadl_epi64((__m128i *) (&pu1_out[0]));
    out_r1 = _mm_loadl_epi64((__m128i *) (&pu1_out[out_strd]));
    out_r2 = _mm_loadl_epi64((__m128i *) (&pu1_out[2 * out_strd]));
    out_r3 = _mm_loadl_epi64((__m128i *) (&pu1_out[3 * out_strd]));

    out_r0 = _mm_and_si128(out_r0, chroma_mask);
    out_r1 = _mm_and_si128(out_r1, chroma_mask);
    out_r2 = _mm_and_si128(out_r2, chroma_mask);
    out_r3 = _mm_and_si128(out_r3, chroma_mask);

    out_r0 = _mm_add_epi8(out_r0, pred_r0);
    out_r1 = _mm_add_epi8(out_r1, pred_r1);
    out_r2 = _mm_add_epi8(out_r2, pred_r2);
    out_r3 = _mm_add_epi8(out_r3, pred_r3);

    _mm_storel_epi64((__m128i *)(&pu1_out[0]), out_r0);
    _mm_storel_epi64((__m128i *)(&pu1_out[out_strd]), out_r1);
    _mm_storel_epi64((__m128i *)(&pu1_out[2 * out_strd]), out_r2);
    _mm_storel_epi64((__m128i *)(&pu1_out[3 * out_strd]), out_r3);
}


