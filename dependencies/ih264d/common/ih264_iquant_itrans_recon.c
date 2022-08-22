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
 *  ih264_iquant_itrans_recon.c
 *
 * @brief
 *  Contains definition of functions for h264 inverse quantization inverse transformation and recon
 *
 * @author
 *  Ittiam
 *
 *  @par List of Functions:
 *  - ih264_iquant_itrans_recon_4x4()
 *  - ih264_iquant_itrans_recon_8x8()
 *  - ih264_iquant_itrans_recon_4x4_dc()
 *  - ih264_iquant_itrans_recon_8x8_dc()
 *  - ih264_iquant_itrans_recon_chroma_4x4()
 *  -ih264_iquant_itrans_recon_chroma_4x4_dc()
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
#include "ih264_platform_macros.h"
#include "ih264_trans_data.h"
#include "ih264_size_defs.h"
#include "ih264_structs.h"
#include "ih264_trans_quant_itrans_iquant.h"

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
void ih264_iquant_itrans_recon_4x4(WORD16 *pi2_src,
                                   UWORD8 *pu1_pred,
                                   UWORD8 *pu1_out,
                                   WORD32 pred_strd,
                                   WORD32 out_strd,
                                   const UWORD16 *pu2_iscal_mat,
                                   const UWORD16 *pu2_weigh_mat,
                                   UWORD32 u4_qp_div_6,
                                   WORD16 *pi2_tmp,
                                   WORD32 iq_start_idx,
                                   WORD16 *pi2_dc_ld_addr
)
{
    WORD16 *pi2_src_ptr = pi2_src;
    WORD16 *pi2_tmp_ptr = pi2_tmp;
    UWORD8 *pu1_pred_ptr = pu1_pred;
    UWORD8 *pu1_out_ptr = pu1_out;
    WORD16 x0, x1, x2, x3, i;
    WORD32 q0, q1, q2, q3;
    WORD16 i_macro;
    WORD16 rnd_fact = (u4_qp_div_6 < 4) ? 1 << (3 - u4_qp_div_6) : 0;

    /* inverse quant */
    /*horizontal inverse transform */
    for(i = 0; i < SUB_BLK_WIDTH_4x4; i++)
    {
        q0 = pi2_src_ptr[0];
        INV_QUANT(q0, pu2_iscal_mat[0], pu2_weigh_mat[0], u4_qp_div_6, rnd_fact,
                  4);
        if (i==0 && iq_start_idx == 1)
            q0 = pi2_dc_ld_addr[0];     // Restoring dc value for intra case

        q2 = pi2_src_ptr[2];
        INV_QUANT(q2, pu2_iscal_mat[2], pu2_weigh_mat[2], u4_qp_div_6, rnd_fact,
                  4);

        x0 = q0 + q2;
        x1 = q0 - q2;

        q1 = pi2_src_ptr[1];
        INV_QUANT(q1, pu2_iscal_mat[1], pu2_weigh_mat[1], u4_qp_div_6, rnd_fact,
                  4);

        q3 = pi2_src_ptr[3];
        INV_QUANT(q3, pu2_iscal_mat[3], pu2_weigh_mat[3], u4_qp_div_6, rnd_fact,
                  4);

        x2 = (q1 >> 1) - q3;
        x3 = q1 + (q3 >> 1);

        pi2_tmp_ptr[0] = x0 + x3;
        pi2_tmp_ptr[1] = x1 + x2;
        pi2_tmp_ptr[2] = x1 - x2;
        pi2_tmp_ptr[3] = x0 - x3;

        pi2_src_ptr += SUB_BLK_WIDTH_4x4;
        pi2_tmp_ptr += SUB_BLK_WIDTH_4x4;
        pu2_iscal_mat += SUB_BLK_WIDTH_4x4;
        pu2_weigh_mat += SUB_BLK_WIDTH_4x4;
    }

    /* vertical inverse transform */
    pi2_tmp_ptr = pi2_tmp;
    for(i = 0; i < SUB_BLK_WIDTH_4x4; i++)
    {
        pu1_pred_ptr = pu1_pred;
        pu1_out = pu1_out_ptr;

        x0 = (pi2_tmp_ptr[0] + pi2_tmp_ptr[8]);
        x1 = (pi2_tmp_ptr[0] - pi2_tmp_ptr[8]);
        x2 = (pi2_tmp_ptr[4] >> 1) - pi2_tmp_ptr[12];
        x3 = pi2_tmp_ptr[4] + (pi2_tmp_ptr[12] >> 1);

        /* inverse prediction */
        i_macro = x0 + x3;
        i_macro = ((i_macro + 32) >> 6);
        i_macro += *pu1_pred_ptr;
        *pu1_out = CLIP_U8(i_macro);
        pu1_pred_ptr += pred_strd;
        pu1_out += out_strd;

        i_macro = x1 + x2;
        i_macro = ((i_macro + 32) >> 6);
        i_macro += *pu1_pred_ptr;
        *pu1_out = CLIP_U8(i_macro);
        pu1_pred_ptr += pred_strd;
        pu1_out += out_strd;

        i_macro = x1 - x2;
        i_macro = ((i_macro + 32) >> 6);
        i_macro += *pu1_pred_ptr;
        *pu1_out = CLIP_U8(i_macro);
        pu1_pred_ptr += pred_strd;
        pu1_out += out_strd;

        i_macro = x0 - x3;
        i_macro = ((i_macro + 32) >> 6);
        i_macro += *pu1_pred_ptr;
        *pu1_out = CLIP_U8(i_macro);

        pi2_tmp_ptr++;
        pu1_out_ptr++;
        pu1_pred++;
    }

}

void ih264_iquant_itrans_recon_4x4_dc(WORD16 *pi2_src,
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
    UWORD8 *pu1_pred_ptr = pu1_pred;
    UWORD8 *pu1_out_ptr = pu1_out;
    WORD32 q0;
    WORD16 x, i_macro, i;
    WORD16 rnd_fact = (u4_qp_div_6 < 4) ? 1 << (3 - u4_qp_div_6) : 0;
    UNUSED(pi2_tmp);

    if (iq_start_idx == 0)
    {
      q0 = pi2_src[0];
      INV_QUANT(q0, pu2_iscal_mat[0], pu2_weigh_mat[0], u4_qp_div_6, rnd_fact, 4);
    }
    else
    {
      q0 = pi2_dc_ld_addr[0];    // Restoring dc value for intra case3
    }
    i_macro = ((q0 + 32) >> 6);
    for(i = 0; i < SUB_BLK_WIDTH_4x4; i++)
    {
        pu1_pred_ptr = pu1_pred;
        pu1_out = pu1_out_ptr;

        /* inverse prediction */

        x = i_macro + *pu1_pred_ptr;
        *pu1_out = CLIP_U8(x);
        pu1_pred_ptr += pred_strd;
        pu1_out += out_strd;

        x = i_macro + *pu1_pred_ptr;
        *pu1_out = CLIP_U8(x);
        pu1_pred_ptr += pred_strd;
        pu1_out += out_strd;

        x = i_macro + *pu1_pred_ptr;
        *pu1_out = CLIP_U8(x);
        pu1_pred_ptr += pred_strd;
        pu1_out += out_strd;

        x = i_macro + *pu1_pred_ptr;
        *pu1_out = CLIP_U8(x);

        pu1_out_ptr++;
        pu1_pred++;
    }
}

/**
 *******************************************************************************
 *
 * @brief
 *  This function performs inverse quant and Inverse transform type Ci4 for 8x8 block
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
 * @param[in] src_strd
 *  Input stride
 *
 * @param[in] pred_strd,
 *  Prediction stride
 *
 * @param[in] out_strd
 *  Output Stride
 *
 * @param[in] pi4_tmp
 *  temporary buffer of size 1*16 we dont need a bigger blcok since we reuse
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
void ih264_iquant_itrans_recon_8x8(WORD16 *pi2_src,
                                   UWORD8 *pu1_pred,
                                   UWORD8 *pu1_out,
                                   WORD32 pred_strd,
                                   WORD32 out_strd,
                                   const UWORD16 *pu2_iscale_mat,
                                   const UWORD16 *pu2_weigh_mat,
                                   UWORD32 qp_div,
                                   WORD16 *pi2_tmp,
                                   WORD32 iq_start_idx,
                                   WORD16 *pi2_dc_ld_addr
)
{
    WORD32 i;
    WORD16 *pi2_tmp_ptr = pi2_tmp;
    UWORD8 *pu1_pred_ptr = pu1_pred;
    UWORD8 *pu1_out_ptr = pu1_out;
    WORD16 i_z0, i_z1, i_z2, i_z3, i_z4, i_z5, i_z6, i_z7;
    WORD16 i_y0, i_y1, i_y2, i_y3, i_y4, i_y5, i_y6, i_y7;
    WORD16 i_macro;
    WORD32 q;
    WORD32 rnd_fact = (qp_div < 6) ? (1 << (5 - qp_div)) : 0;
    UNUSED(iq_start_idx);
    UNUSED(pi2_dc_ld_addr);
    /*************************************************************/
    /* De quantization of coefficients. Will be replaced by SIMD */
    /* operations on platform. Note : DC coeff is not scaled     */
    /*************************************************************/
    for(i = 0; i < (SUB_BLK_WIDTH_8x8 * SUB_BLK_WIDTH_8x8); i++)
    {
        q = pi2_src[i];
        INV_QUANT(q, pu2_iscale_mat[i], pu2_weigh_mat[i], qp_div, rnd_fact, 6);
        pi2_tmp_ptr[i] = q;
    }
    /* Perform Inverse transform */
    /*--------------------------------------------------------------------*/
    /* IDCT [ Horizontal transformation ]                                 */
    /*--------------------------------------------------------------------*/
    for(i = 0; i < SUB_BLK_WIDTH_8x8; i++)
    {
        /*------------------------------------------------------------------*/
        /* y0 = w0 + w4                                                     */
        /* y1 = -w3 + w5 - w7 - (w7 >> 1)                                   */
        /* y2 = w0 - w4                                                     */
        /* y3 = w1 + w7 - w3 - (w3 >> 1)                                    */
        /* y4 = (w2 >> 1) - w6                                              */
        /* y5 = -w1 + w7 + w5 + (w5 >> 1)                                   */
        /* y6 = w2 + (w6 >> 1)                                              */
        /* y7 = w3 + w5 + w1 + (w1 >> 1)                                    */
        /*------------------------------------------------------------------*/
        i_y0 = (pi2_tmp_ptr[0] + pi2_tmp_ptr[4] );

        i_y1 = ((WORD32)(-pi2_tmp_ptr[3]) + pi2_tmp_ptr[5] - pi2_tmp_ptr[7]
                        - (pi2_tmp_ptr[7] >> 1));

        i_y2 = (pi2_tmp_ptr[0] - pi2_tmp_ptr[4] );

        i_y3 = ((WORD32)pi2_tmp_ptr[1] + pi2_tmp_ptr[7] - pi2_tmp_ptr[3]
                        - (pi2_tmp_ptr[3] >> 1));

        i_y4 = ((pi2_tmp_ptr[2] >> 1) - pi2_tmp_ptr[6] );

        i_y5 = ((WORD32)(-pi2_tmp_ptr[1]) + pi2_tmp_ptr[7] + pi2_tmp_ptr[5]
                        + (pi2_tmp_ptr[5] >> 1));

        i_y6 = (pi2_tmp_ptr[2] + (pi2_tmp_ptr[6] >> 1));

        i_y7 = ((WORD32)pi2_tmp_ptr[3] + pi2_tmp_ptr[5] + pi2_tmp_ptr[1]
                        + (pi2_tmp_ptr[1] >> 1));

        /*------------------------------------------------------------------*/
        /* z0 = y0 + y6                                                     */
        /* z1 = y1 + (y7 >> 2)                                              */
        /* z2 = y2 + y4                                                     */
        /* z3 = y3 + (y5 >> 2)                                              */
        /* z4 = y2 - y4                                                     */
        /* z5 = (y3 >> 2) - y5                                              */
        /* z6 = y0 - y6                                                     */
        /* z7 = y7 - (y1 >> 2)                                              */
        /*------------------------------------------------------------------*/
        i_z0 = i_y0 + i_y6;
        i_z1 = i_y1 + (i_y7 >> 2);
        i_z2 = i_y2 + i_y4;
        i_z3 = i_y3 + (i_y5 >> 2);
        i_z4 = i_y2 - i_y4;
        i_z5 = (i_y3 >> 2) - i_y5;
        i_z6 = i_y0 - i_y6;
        i_z7 = i_y7 - (i_y1 >> 2);

        /*------------------------------------------------------------------*/
        /* x0 = z0 + z7                                                     */
        /* x1 = z2 + z5                                                     */
        /* x2 = z4 + z3                                                     */
        /* x3 = z6 + z1                                                     */
        /* x4 = z6 - z1                                                     */
        /* x5 = z4 - z3                                                     */
        /* x6 = z2 - z5                                                     */
        /* x7 = z0 - z7                                                     */
        /*------------------------------------------------------------------*/
        pi2_tmp_ptr[0] = i_z0 + i_z7;
        pi2_tmp_ptr[1] = i_z2 + i_z5;
        pi2_tmp_ptr[2] = i_z4 + i_z3;
        pi2_tmp_ptr[3] = i_z6 + i_z1;
        pi2_tmp_ptr[4] = i_z6 - i_z1;
        pi2_tmp_ptr[5] = i_z4 - i_z3;
        pi2_tmp_ptr[6] = i_z2 - i_z5;
        pi2_tmp_ptr[7] = i_z0 - i_z7;

        /* move to the next row */
        //pi2_src_ptr += SUB_BLK_WIDTH_8x8;
        pi2_tmp_ptr += SUB_BLK_WIDTH_8x8;
    }
    /*--------------------------------------------------------------------*/
    /* IDCT [ Vertical transformation] and Xij = (xij + 32)>>6            */
    /*                                                                    */
    /* Add the prediction and store it back to reconstructed frame buffer */
    /* [Prediction buffer itself in this case]                            */
    /*--------------------------------------------------------------------*/

    pi2_tmp_ptr = pi2_tmp;
    for(i = 0; i < SUB_BLK_WIDTH_8x8; i++)
    {
        pu1_pred_ptr = pu1_pred;
        pu1_out = pu1_out_ptr;
        /*------------------------------------------------------------------*/
        /* y0j = w0j + w4j                                                  */
        /* y1j = -w3j + w5j -w7j -(w7j >> 1)                                */
        /* y2j = w0j -w4j                                                   */
        /* y3j = w1j + w7j -w3j -(w3j >> 1)                                 */
        /* y4j = ( w2j >> 1 ) -w6j                                          */
        /* y5j = -w1j + w7j + w5j + (w5j >> 1)                              */
        /* y6j = w2j + ( w6j >> 1 )                                         */
        /* y7j = w3j + w5j + w1j + (w1j >> 1)                               */
        /*------------------------------------------------------------------*/
        i_y0 = pi2_tmp_ptr[0] + pi2_tmp_ptr[32];

        i_y1 = (WORD32)(-pi2_tmp_ptr[24]) + pi2_tmp_ptr[40] - pi2_tmp_ptr[56]
                        - (pi2_tmp_ptr[56] >> 1);

        i_y2 = pi2_tmp_ptr[0] - pi2_tmp_ptr[32];

        i_y3 = (WORD32)pi2_tmp_ptr[8] + pi2_tmp_ptr[56] - pi2_tmp_ptr[24]
                        - (pi2_tmp_ptr[24] >> 1);

        i_y4 = (pi2_tmp_ptr[16] >> 1) - pi2_tmp_ptr[48];

        i_y5 = (WORD32)(-pi2_tmp_ptr[8]) + pi2_tmp_ptr[56] + pi2_tmp_ptr[40]
                        + (pi2_tmp_ptr[40] >> 1);

        i_y6 = pi2_tmp_ptr[16] + (pi2_tmp_ptr[48] >> 1);

        i_y7 = (WORD32)pi2_tmp_ptr[24] + pi2_tmp_ptr[40] + pi2_tmp_ptr[8]
                        + (pi2_tmp_ptr[8] >> 1);

        /*------------------------------------------------------------------*/
        /* z0j = y0j + y6j                                                  */
        /* z1j = y1j + (y7j >> 2)                                           */
        /* z2j = y2j + y4j                                                  */
        /* z3j = y3j + (y5j >> 2)                                           */
        /* z4j = y2j -y4j                                                   */
        /* z5j = (y3j >> 2) -y5j                                            */
        /* z6j = y0j -y6j                                                   */
        /* z7j = y7j -(y1j >> 2)                                            */
        /*------------------------------------------------------------------*/
        i_z0 = i_y0 + i_y6;
        i_z1 = i_y1 + (i_y7 >> 2);
        i_z2 = i_y2 + i_y4;
        i_z3 = i_y3 + (i_y5 >> 2);
        i_z4 = i_y2 - i_y4;
        i_z5 = (i_y3 >> 2) - i_y5;
        i_z6 = i_y0 - i_y6;
        i_z7 = i_y7 - (i_y1 >> 2);

        /*------------------------------------------------------------------*/
        /* x0j = z0j + z7j                                                  */
        /* x1j = z2j + z5j                                                  */
        /* x2j = z4j + z3j                                                  */
        /* x3j = z6j + z1j                                                  */
        /* x4j = z6j -z1j                                                   */
        /* x5j = z4j -z3j                                                   */
        /* x6j = z2j -z5j                                                   */
        /* x7j = z0j -z7j                                                   */
        /*------------------------------------------------------------------*/
        i_macro = ((i_z0 + i_z7 + 32) >> 6) + *pu1_pred_ptr;
        *pu1_out = CLIP_U8(i_macro);
        /* Change uc_recBuffer to Point to next element in the same column*/
        pu1_pred_ptr += pred_strd;
        pu1_out += out_strd;

        i_macro = ((i_z2 + i_z5 + 32) >> 6) + *pu1_pred_ptr;
        *pu1_out = CLIP_U8(i_macro);
        pu1_pred_ptr += pred_strd;
        pu1_out += out_strd;

        i_macro = ((i_z4 + i_z3 + 32) >> 6) + *pu1_pred_ptr;
        *pu1_out = CLIP_U8(i_macro);
        pu1_pred_ptr += pred_strd;
        pu1_out += out_strd;

        i_macro = ((i_z6 + i_z1 + 32) >> 6) + *pu1_pred_ptr;
        *pu1_out = CLIP_U8(i_macro);
        pu1_pred_ptr += pred_strd;
        pu1_out += out_strd;

        i_macro = ((i_z6 - i_z1 + 32) >> 6) + *pu1_pred_ptr;
        *pu1_out = CLIP_U8(i_macro);
        pu1_pred_ptr += pred_strd;
        pu1_out += out_strd;

        i_macro = ((i_z4 - i_z3 + 32) >> 6) + *pu1_pred_ptr;
        *pu1_out = CLIP_U8(i_macro);
        pu1_pred_ptr += pred_strd;
        pu1_out += out_strd;

        i_macro = ((i_z2 - i_z5 + 32) >> 6) + *pu1_pred_ptr;
        *pu1_out = CLIP_U8(i_macro);
        pu1_pred_ptr += pred_strd;
        pu1_out += out_strd;

        i_macro = ((i_z0 - i_z7 + 32) >> 6) + *pu1_pred_ptr;
        *pu1_out = CLIP_U8(i_macro);

        pi2_tmp_ptr++;
        pu1_out_ptr++;
        pu1_pred++;
    }
}

void ih264_iquant_itrans_recon_8x8_dc(WORD16 *pi2_src,
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
    UWORD8 *pu1_pred_ptr = pu1_pred;
    UWORD8 *pu1_out_ptr = pu1_out;
    WORD16 x, i, i_macro;
    WORD32 q;
    WORD32 rnd_fact = (qp_div < 6) ? (1 << (5 - qp_div)) : 0;
    UNUSED(pi2_tmp);
    UNUSED(iq_start_idx);
    UNUSED(pi2_dc_ld_addr);
    /*************************************************************/
    /* Dequantization of coefficients. Will be replaced by SIMD  */
    /* operations on platform. Note : DC coeff is not scaled     */
    /*************************************************************/
    q = pi2_src[0];
    INV_QUANT(q, pu2_iscale_mat[0], pu2_weigh_mat[0], qp_div, rnd_fact, 6);
    i_macro = (q + 32) >> 6;
    /* Perform Inverse transform */
    /*--------------------------------------------------------------------*/
    /* IDCT [ Horizontal transformation ]                                 */
    /*--------------------------------------------------------------------*/
    /*--------------------------------------------------------------------*/
    /* IDCT [ Vertical transformation] and Xij = (xij + 32)>>6            */
    /*                                                                    */
    /* Add the prediction and store it back to reconstructed frame buffer */
    /* [Prediction buffer itself in this case]                            */
    /*--------------------------------------------------------------------*/
    for(i = 0; i < SUB_BLK_WIDTH_8x8; i++)
    {
        pu1_pred_ptr = pu1_pred;
        pu1_out = pu1_out_ptr;

        x = i_macro + *pu1_pred_ptr;
        *pu1_out = CLIP_U8(x);
        /* Change uc_recBuffer to Point to next element in the same column*/
        pu1_pred_ptr += pred_strd;
        pu1_out += out_strd;

        x = i_macro + *pu1_pred_ptr;
        *pu1_out = CLIP_U8(x);
        pu1_pred_ptr += pred_strd;
        pu1_out += out_strd;

        x = i_macro + *pu1_pred_ptr;
        *pu1_out = CLIP_U8(x);
        pu1_pred_ptr += pred_strd;
        pu1_out += out_strd;

        x = i_macro + *pu1_pred_ptr;
        *pu1_out = CLIP_U8(x);
        pu1_pred_ptr += pred_strd;
        pu1_out += out_strd;

        x = i_macro + *pu1_pred_ptr;
        *pu1_out = CLIP_U8(x);
        pu1_pred_ptr += pred_strd;
        pu1_out += out_strd;

        x = i_macro + *pu1_pred_ptr;
        *pu1_out = CLIP_U8(x);
        pu1_pred_ptr += pred_strd;
        pu1_out += out_strd;

        x = i_macro + *pu1_pred_ptr;
        *pu1_out = CLIP_U8(x);
        pu1_pred_ptr += pred_strd;
        pu1_out += out_strd;

        x = i_macro + *pu1_pred_ptr;
        *pu1_out = CLIP_U8(x);

        pu1_out_ptr++;
        pu1_pred++;
    }
}

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
void ih264_iquant_itrans_recon_chroma_4x4(WORD16 *pi2_src,
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
    WORD16 *pi2_src_ptr = pi2_src;
    WORD16 *pi2_tmp_ptr = pi2_tmp;
    UWORD8 *pu1_pred_ptr = pu1_pred;
    UWORD8 *pu1_out_ptr = pu1_out;
    WORD16 x0, x1, x2, x3, i;
    WORD32 q0, q1, q2, q3;
    WORD16 i_macro;
    WORD16 rnd_fact = (u4_qp_div_6 < 4) ? 1 << (3 - u4_qp_div_6) : 0;

    /* inverse quant */
    /*horizontal inverse transform */
    for(i = 0; i < SUB_BLK_WIDTH_4x4; i++)
    {
      if(i==0)
      {
        q0 = pi2_dc_src[0];
      }
      else
      {
        q0 = pi2_src_ptr[0];
        INV_QUANT(q0, pu2_iscal_mat[0], pu2_weigh_mat[0], u4_qp_div_6, rnd_fact, 4);
      }

      q2 = pi2_src_ptr[2];
      INV_QUANT(q2, pu2_iscal_mat[2], pu2_weigh_mat[2], u4_qp_div_6, rnd_fact,
                4);

      x0 = q0 + q2;
      x1 = q0 - q2;

      q1 = pi2_src_ptr[1];
      INV_QUANT(q1, pu2_iscal_mat[1], pu2_weigh_mat[1], u4_qp_div_6, rnd_fact,
                4);

      q3 = pi2_src_ptr[3];
      INV_QUANT(q3, pu2_iscal_mat[3], pu2_weigh_mat[3], u4_qp_div_6, rnd_fact,
                4);

      x2 = (q1 >> 1) - q3;
      x3 = q1 + (q3 >> 1);

      pi2_tmp_ptr[0] = x0 + x3;
      pi2_tmp_ptr[1] = x1 + x2;
      pi2_tmp_ptr[2] = x1 - x2;
      pi2_tmp_ptr[3] = x0 - x3;

      pi2_src_ptr += SUB_BLK_WIDTH_4x4;
      pi2_tmp_ptr += SUB_BLK_WIDTH_4x4;
      pu2_iscal_mat += SUB_BLK_WIDTH_4x4;
      pu2_weigh_mat += SUB_BLK_WIDTH_4x4;
    }

    /* vertical inverse transform */
    pi2_tmp_ptr = pi2_tmp;
    for(i = 0; i < SUB_BLK_WIDTH_4x4; i++)
    {
        pu1_pred_ptr = pu1_pred;
        pu1_out = pu1_out_ptr;

        x0 = (pi2_tmp_ptr[0] + pi2_tmp_ptr[8]);
        x1 = (pi2_tmp_ptr[0] - pi2_tmp_ptr[8]);
        x2 = (pi2_tmp_ptr[4] >> 1) - pi2_tmp_ptr[12];
        x3 =  pi2_tmp_ptr[4] + (pi2_tmp_ptr[12] >> 1);

        /* inverse prediction */
        i_macro = x0 + x3;
        i_macro = ((i_macro + 32) >> 6);
        i_macro += *pu1_pred_ptr;
        *pu1_out = CLIP_U8(i_macro);
        pu1_pred_ptr += pred_strd;
        pu1_out += out_strd;

        i_macro = x1 + x2;
        i_macro = ((i_macro + 32) >> 6);
        i_macro += *pu1_pred_ptr;
        *pu1_out = CLIP_U8(i_macro);
        pu1_pred_ptr += pred_strd;
        pu1_out += out_strd;

        i_macro = x1 - x2;
        i_macro = ((i_macro + 32) >> 6);
        i_macro += *pu1_pred_ptr;
        *pu1_out = CLIP_U8(i_macro);
        pu1_pred_ptr += pred_strd;
        pu1_out += out_strd;

        i_macro = x0 - x3;
        i_macro = ((i_macro + 32) >> 6);
        i_macro += *pu1_pred_ptr;
        *pu1_out = CLIP_U8(i_macro);

        pi2_tmp_ptr++;
        pu1_out_ptr+= 2;    //Interleaved store for output
        pu1_pred+= 2;       //Interleaved load for pred buffer
    }
}

/*
 ********************************************************************************
 *
 * @brief This function reconstructs a 4x4 sub block from quantized resiude and
 * prediction buffer if only dc value is present for residue
 *
 * @par Description:
 *  The quantized residue is first inverse quantized,
 *  This inverse quantized content is added to the prediction buffer to recon-
 *  struct the end output
 *
 * @param[in] pi2_src
 *  quantized dc coefficient
 *
 * @param[in] pu1_pred
 *  prediction 4x4 block in interleaved format
 *
 * @param[in] pred_strd,
 *  Prediction buffer stride in interleaved format
 *
 * @param[in] out_strd
 *  recon buffer Stride
 *
 * @returns none
 *
 * @remarks none
 *
 *******************************************************************************
 */

void ih264_iquant_itrans_recon_chroma_4x4_dc(WORD16 *pi2_src,
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
    UWORD8 *pu1_pred_ptr = pu1_pred;
    UWORD8 *pu1_out_ptr = pu1_out;
    WORD32 q0;
    WORD16 x, i_macro, i;
    UNUSED(pi2_src);
    UNUSED(pu2_iscal_mat);
    UNUSED(pu2_weigh_mat);
    UNUSED(u4_qp_div_6);
    UNUSED(pi2_tmp);

    q0 = pi2_dc_src[0];    // Restoring dc value for intra case3
    i_macro = ((q0 + 32) >> 6);

    for(i = 0; i < SUB_BLK_WIDTH_4x4; i++)
    {
        pu1_pred_ptr = pu1_pred;
        pu1_out = pu1_out_ptr;

        /* inverse prediction */
        x = i_macro + *pu1_pred_ptr;
        *pu1_out =  CLIP_U8(x);
        pu1_pred_ptr += pred_strd;
        pu1_out += out_strd;

        x = i_macro + *pu1_pred_ptr;
        *pu1_out = CLIP_U8(x);
        pu1_pred_ptr += pred_strd;
        pu1_out += out_strd;

        x = i_macro + *pu1_pred_ptr;
        *pu1_out = CLIP_U8(x);
        pu1_pred_ptr += pred_strd;
        pu1_out += out_strd;

        x = i_macro + *pu1_pred_ptr;
        *pu1_out = CLIP_U8(x);

        pu1_out_ptr+=2;
        pu1_pred+=2;
    }
}
