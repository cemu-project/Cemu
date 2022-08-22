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
 *  ih264_resi_trans_quant.c
 *
 * @brief
 *  Contains function definitions single stage  forward transform for H.264
 *  It will calculate the residue, do the cf and then do quantization
 *
 * @author
 *  Ittiam
 *
 * @par List of Functions:
 *  - ih264_resi_trans_quant_4x4()
 *  - ih264_resi_trans_quant_chroma_4x4
 *  - ih264_hadamard_quant_4x4
 *  - ih264_hadamard_quant_2x2_uv
 *  - ih264_resi_trans_quant_8x8
 *
 * @remarks
 *******************************************************************************
 */

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

/* System include files */
#include <stddef.h>

/* User include files */
#include "ih264_typedefs.h"
#include "ih264_defs.h"
#include "ih264_size_defs.h"
#include "ih264_macros.h"
#include "ih264_trans_macros.h"
#include "ih264_trans_data.h"
#include "ih264_structs.h"
#include "ih264_trans_quant_itrans_iquant.h"

/**
 *******************************************************************************
 *
 * @brief
 *   This function performs forward transform and quantization on a 4*4 block
 *
 * @par Description:
 *   The function accepts source buffer and estimation buffer. From these, it
 *   computes the residue. This is residue is then transformed and quantized.
 *   The transform and quantization are in placed computed. They use the residue
 *   buffer for this.
 *
 * @param[in] pu1_src
 *   Pointer to source sub-block
 *
 * @param[in] pu1_pred
 *   Pointer to prediction sub-block
 *
 * @param[in] pi2_out
 *   Pointer to residual sub-block
 *
 * @param[in] src_strd
 *   Source stride
 *
 * @param[in] pred_strd
 *   Prediction stride
 *
 * @param[in] dst_strd
 *   Destination stride
 *
 * @param[in] u4_qbits
 *    QP_BITS_h264_4x4 + floor(QP/6)
 *
 * @param[in] pu2_threshold_matrix
 *   Pointer to Forward Quant Threshold Matrix
 *
 * @param[in] pu2_scale_matrix
 *   Pointer to Forward Quant Scale Matrix
 *
 * @param[in] u4_round_factor
 *   Quantization Round factor
 *
 * @param[out] pu1_nnz
 *   Total non-zero coefficients in the current sub-block
 *
 * @returns
 *
 * @remarks
 *   None
 *
 *******************************************************************************
 */
void ih264_resi_trans_quant_4x4(UWORD8 *pu1_src,
                                UWORD8 *pu1_pred,
                                WORD16 *pi2_out,
                                WORD32 src_strd,
                                WORD32 pred_strd,
                                const UWORD16 *pu2_scale_matrix,
                                const UWORD16 *pu2_threshold_matrix,
                                UWORD32 u4_qbits,
                                UWORD32 u4_round_factor,
                                UWORD8 *pu1_nnz,
                                WORD16 *pi2_alt_dc_addr)
{
    UWORD32 i;
    WORD32  x0, x1, x2, x3, x4, x5, x6, x7;
    WORD32  i4_value, i4_sign;
    UWORD32 u4_abs_value;
    WORD16  *pi2_out_tmp = pi2_out;
    UWORD32 u4_nonzero_coeff = 0;

    for (i = 0; i < SUB_BLK_WIDTH_4x4; i++)
    {
        /* computing prediction error (residue) */
        x4 = pu1_src[0] - pu1_pred[0];
        x5 = pu1_src[1] - pu1_pred[1];
        x6 = pu1_src[2] - pu1_pred[2];
        x7 = pu1_src[3] - pu1_pred[3];

        /* Horizontal transform */
        x0 = x4 + x7;
        x1 = x5 + x6;
        x2 = x5 - x6;
        x3 = x4 - x7;

        pi2_out_tmp[0] = x0 + x1;
        pi2_out_tmp[1] = (x3 <<1) + x2;
        pi2_out_tmp[2] = x0 - x1;
        pi2_out_tmp[3] = x3 - (x2<<1);

        /* pointing to next row; */
        pu1_src += src_strd;
        pu1_pred += pred_strd;
        pi2_out_tmp += 4;

    }
    pi2_out_tmp = pi2_out;
    for (i = 0; i < SUB_BLK_WIDTH_4x4; i++)
    {

        /* Vertical transform and quantization */
        x4 = pi2_out_tmp[0];
        x5 = pi2_out_tmp[4];
        x6 = pi2_out_tmp[8];
        x7 = pi2_out_tmp[12];


        x0 = x4 + x7;
        x1 = x5 + x6;
        x2 = x5 - x6;
        x3 = x4 - x7;

        /* quantization is done in place */

        i4_value = x0 + x1;

        if(i==0)
        {
          (*pi2_alt_dc_addr) = i4_value;
        }

        FWD_QUANT(i4_value, u4_abs_value, i4_sign, pu2_threshold_matrix[0], pu2_scale_matrix[0], u4_round_factor, u4_qbits, u4_nonzero_coeff);
        pi2_out_tmp[0] = i4_value;


        i4_value = (x3 << 1) + x2;
        FWD_QUANT(i4_value, u4_abs_value, i4_sign, pu2_threshold_matrix[4], pu2_scale_matrix[4], u4_round_factor, u4_qbits, u4_nonzero_coeff);
        pi2_out_tmp[4] = i4_value;


        i4_value = x0 - x1;
        FWD_QUANT(i4_value, u4_abs_value, i4_sign, pu2_threshold_matrix[8], pu2_scale_matrix[8], u4_round_factor, u4_qbits, u4_nonzero_coeff);
        pi2_out_tmp[8] = i4_value;


        i4_value = x3 - (x2 << 1);
        FWD_QUANT(i4_value, u4_abs_value, i4_sign, pu2_threshold_matrix[12], pu2_scale_matrix[12], u4_round_factor, u4_qbits, u4_nonzero_coeff);
        pi2_out_tmp[12] = i4_value;

        pi2_out_tmp ++;
        pu2_scale_matrix++;
        pu2_threshold_matrix++;
    }

    /* Return total nonzero coefficients in the current sub block */
    *pu1_nnz =  u4_nonzero_coeff;
}
/**
 *******************************************************************************
 *
 * @brief
 *   This function performs forward transform and quantization on a 4*4 chroma block
 *   with interleaved values
 *
 * @par Description:
 *   The function accepts source buffer and estimation buffer. From these, it
 *   computes the residue. This is residue is then transformed and quantized.
 *   The transform and quantization are in placed computed. They use the residue
 *   buffer for this.
 *
 * @param[in] pu1_src
 *   Pointer to source sub-block
 *
 * @param[in] pu1_pred
 *   Pointer to prediction sub-block
 *
 * @param[in] pi2_out
 *   Pointer to residual sub-block
 *
 * @param[in] src_strd
 *   Source stride
 *
 * @param[in] pred_strd
 *   Prediction stride
 *
 * @param[in] dst_strd
 *   Destination stride
 *
 * @param[in] u4_qbits
 *    QP_BITS_h264_4x4 + floor(QP/6)
 *
 * @param[in] pu2_threshold_matrix
 *   Pointer to Forward Quant Threshold Matrix
 *
 * @param[in] pu2_scale_matrix
 *   Pointer to Forward Quant Scale Matrix
 *
 * @param[in] u4_round_factor
 *   Quantization Round factor
 *
 * @param[out] pu1_nnz
 *   Total non-zero coefficients in the current sub-block
 *
 * @returns
 *
 * @remarks
 *   None
 *
 *******************************************************************************
 */
void ih264_resi_trans_quant_chroma_4x4(UWORD8 *pu1_src,
                                       UWORD8 *pu1_pred,
                                       WORD16 *pi2_out,
                                       WORD32 src_strd,
                                       WORD32 pred_strd,
                                       const UWORD16 *pu2_scale_matrix,
                                       const UWORD16 *pu2_threshold_matrix,
                                       UWORD32 u4_qbits,
                                       UWORD32 u4_round_factor,
                                       UWORD8 *pu1_nnz,
                                       WORD16 *pu1_dc_alt_addr)
{
    UWORD32 i;
    WORD32  x0, x1, x2, x3, x4, x5, x6, x7;
    WORD32  i4_value, i4_sign;
    UWORD32 u4_abs_value;
    WORD16  *pi2_out_tmp = pi2_out;
    UWORD32 u4_nonzero_coeff = 0;

    for (i = 0; i < SUB_BLK_WIDTH_4x4; i++)
    {
        /* computing prediction error (residue) */
        x4 = pu1_src[0] - pu1_pred[0];
        x5 = pu1_src[2] - pu1_pred[2];
        x6 = pu1_src[4] - pu1_pred[4];
        x7 = pu1_src[6] - pu1_pred[6];

        /* Horizontal transform */
        x0 = x4 + x7;
        x1 = x5 + x6;
        x2 = x5 - x6;
        x3 = x4 - x7;

        pi2_out_tmp[0] = x0 + x1;
        pi2_out_tmp[1] = (x3 <<1) + x2;
        pi2_out_tmp[2] = x0 - x1;
        pi2_out_tmp[3] = x3 - (x2<<1);

        /* pointing to next row; */
        pu1_src += src_strd;
        pu1_pred += pred_strd;
        pi2_out_tmp += 4;

    }
    pi2_out_tmp = pi2_out;
    for (i = 0; i < SUB_BLK_WIDTH_4x4; i++)
    {

        /* Vertical transform and quantization */
        x4 = pi2_out_tmp[0];
        x5 = pi2_out_tmp[4];
        x6 = pi2_out_tmp[8];
        x7 = pi2_out_tmp[12];


        x0 = x4 + x7;
        x1 = x5 + x6;
        x2 = x5 - x6;
        x3 = x4 - x7;

        /* quantization is done in place */

        i4_value = x0 + x1;

        if(i==0)
        {
          *pu1_dc_alt_addr = i4_value;
        }

        FWD_QUANT(i4_value, u4_abs_value, i4_sign, pu2_threshold_matrix[0],
                  pu2_scale_matrix[0], u4_round_factor, u4_qbits,
                  u4_nonzero_coeff);
        pi2_out_tmp[0] = i4_value;

        i4_value = (x3 << 1) + x2;
        FWD_QUANT(i4_value, u4_abs_value, i4_sign, pu2_threshold_matrix[4],
                  pu2_scale_matrix[4], u4_round_factor, u4_qbits,
                  u4_nonzero_coeff);
        pi2_out_tmp[4] = i4_value;

        i4_value = x0 - x1;
        FWD_QUANT(i4_value, u4_abs_value, i4_sign, pu2_threshold_matrix[8],
                  pu2_scale_matrix[8], u4_round_factor, u4_qbits,
                  u4_nonzero_coeff);
        pi2_out_tmp[8] = i4_value;

        i4_value = x3 - (x2 << 1);
        FWD_QUANT(i4_value, u4_abs_value, i4_sign, pu2_threshold_matrix[12],
                  pu2_scale_matrix[12], u4_round_factor, u4_qbits,
                  u4_nonzero_coeff);
        pi2_out_tmp[12] = i4_value;

        pi2_out_tmp ++;
        pu2_scale_matrix++;
        pu2_threshold_matrix++;
    }

    /* Return total nonzero coefficients in the current sub block */
    *pu1_nnz =  u4_nonzero_coeff;
}

/**
 *******************************************************************************
 *
 * @brief
 *   This function performs forward hadamard transform and quantization on a 4*4 block
 *
 * @par Description:
 *   The function accepts source buffer and estimation buffer. From these, it
 *   computes the residue. This is residue is then transformed and quantized.
 *   The transform and quantization are in placed computed. They use the residue
 *   buffer for this.
 *
 * @param[in] pu1_src
 *   Pointer to source sub-block
 *
 * @param[in] pu1_pred
 *   Pointer to prediction sub-block
 *
 * @param[in] pi2_out
 *   Pointer to residual sub-block
 *
 * @param[in] src_strd
 *   Source stride
 *
 * @param[in] pred_strd
 *   Prediction stride
 *
 * @param[in] dst_strd
 *   Destination stride
 *
 * @param[in] u4_qbits
 *    QP_BITS_h264_4x4 + floor(QP/6)
 *
 * @param[in] pu2_threshold_matrix
 *   Pointer to Forward Quant Threshold Matrix
 *
 * @param[in] pu2_scale_matrix
 *   Pointer to Forward Quant Scale Matrix
 *
 * @param[in] u4_round_factor
 *   Quantization Round factor
 *
 * @param[out] pu1_nnz
 *   Total non-zero coefficients in the current sub-block
 *
 * @returns
 *
 * @remarks
 *   None
 *
 */

void ih264_hadamard_quant_4x4(WORD16 *pi2_src,
                              WORD16 *pi2_dst,
                              const UWORD16 *pu2_scale_matrix,
                              const UWORD16 *pu2_threshold_matrix,
                              UWORD32 u4_qbits,
                              UWORD32 u4_round_factor,
                              UWORD8 *pu1_nnz)
{
  WORD32 i;
  WORD32 x0,x1,x2,x3,x4,x5,x6,x7,i4_value;
  UWORD32 u4_abs_value;
  WORD32 i4_sign;

  *pu1_nnz = 0;

  for (i = 0; i < SUB_BLK_WIDTH_4x4; i++)
    {
        x4 = pi2_src[0];
        x5 = pi2_src[1];
        x6 = pi2_src[2];
        x7 = pi2_src[3];

        x0 = x4 + x7;
        x1 = x5 + x6;
        x2 = x5 - x6;
        x3 = x4 - x7;

        pi2_dst[0] = x0 + x1;
        pi2_dst[1] = x3 + x2;
        pi2_dst[2] = x0 - x1;
        pi2_dst[3] = x3 - x2;

        pi2_src += 4;
        pi2_dst += 4;
    }

    /* Vertical transform and quantization */
    pi2_dst -= SUB_BLK_WIDTH_4x4<<2;

    for (i = 0; i < SUB_BLK_WIDTH_4x4; i++)
    {
        x4 = pi2_dst[0];
        x5 = pi2_dst[4];
        x6 = pi2_dst[8];
        x7 = pi2_dst[12] ;

        x0 = x4 + x7;
        x1 = x5 + x6;
        x2 = x5 - x6;
        x3 = x4 - x7;


        i4_value = (x0 + x1) >> 1;
        FWD_QUANT(i4_value, u4_abs_value, i4_sign, pu2_threshold_matrix[0],
                  pu2_scale_matrix[0], u4_round_factor, u4_qbits, pu1_nnz[0]);
        pi2_dst[0] = i4_value;

        i4_value = (x3 + x2) >> 1;
        FWD_QUANT(i4_value, u4_abs_value, i4_sign, pu2_threshold_matrix[0],
                  pu2_scale_matrix[0], u4_round_factor, u4_qbits, pu1_nnz[0]);
        pi2_dst[4] = i4_value;

        i4_value = (x0 - x1) >> 1;
        FWD_QUANT(i4_value, u4_abs_value, i4_sign, pu2_threshold_matrix[0],
                  pu2_scale_matrix[0], u4_round_factor, u4_qbits, pu1_nnz[0]);
        pi2_dst[8] = i4_value;

        i4_value = (x3 - x2) >> 1;
        FWD_QUANT(i4_value, u4_abs_value, i4_sign, pu2_threshold_matrix[0],
                  pu2_scale_matrix[0], u4_round_factor, u4_qbits, pu1_nnz[0]);
        pi2_dst[12] = i4_value;

        pi2_dst ++;
    }
}

/**
 *******************************************************************************
 *
 * @brief
 *   This function performs forward hadamard transform and quantization on a 2*2 block
 *   for both U and V planes
 *
 * @par Description:
 *   The function accepts source buffer and estimation buffer. From these, it
 *   computes the residue. This is residue is then transformed and quantized.
 *   The transform and quantization are in placed computed. They use the residue
 *   buffer for this.
 *
 * @param[in] pu1_src
 *   Pointer to source sub-block
 *
 * @param[in] pu1_pred
 *   Pointer to prediction sub-block
 *
 * @param[in] pi2_out
 *   Pointer to residual sub-block
 *
 * @param[in] src_strd
 *   Source stride
 *
 * @param[in] pred_strd
 *   Prediction stride
 *
 * @param[in] dst_strd
 *   Destination stride
 *
 * @param[in] u4_qbits
 *    QP_BITS_h264_4x4 + floor(QP/6)
 *
 * @param[in] pu2_threshold_matrix
 *   Pointer to Forward Quant Threshold Matrix
 *
 * @param[in] pu2_scale_matrix
 *   Pointer to Forward Quant Scale Matrix
 *
 * @param[in] u4_round_factor
 *   Quantization Round factor
 *
 * @param[out] pu1_nnz
 *   Total non-zero coefficients in the current sub-block
 *
 * @returns
 *
 * @remarks
 *   NNZ for dc is populated at 0 and 5th position of pu1_nnz
 *
 */

void ih264_hadamard_quant_2x2_uv(WORD16 *pi2_src,
                                 WORD16 *pi2_dst,
                                 const UWORD16 *pu2_scale_matrix,
                                 const UWORD16 *pu2_threshold_matrix,
                                 UWORD32 u4_qbits,
                                 UWORD32 u4_round_factor,
                                 UWORD8 *pu1_nnz)
{
    WORD32 x0, x1, x2, x3, x4, x5, x6, x7;
    WORD32 i4_value, i4_sign, plane;
    UWORD32 u4_abs_value;

    for(plane = 0; plane < 2; plane++)
    {
        pu1_nnz[plane] = 0;

        /* Horizontal transform */
        x4 = pi2_src[0];
        x5 = pi2_src[1];
        x6 = pi2_src[2];
        x7 = pi2_src[3];

        x0 = x4 + x5;
        x1 = x4 - x5;
        x2 = x6 + x7;
        x3 = x6 - x7;

        /* Vertical transform and quantization */
        i4_value = (x0 + x2);
        FWD_QUANT(i4_value, u4_abs_value, i4_sign, pu2_threshold_matrix[0],
                  pu2_scale_matrix[0], u4_round_factor, u4_qbits,
                  pu1_nnz[plane]);
        pi2_dst[0] = i4_value;

        i4_value = (x0 - x2);
        FWD_QUANT(i4_value, u4_abs_value, i4_sign, pu2_threshold_matrix[0],
                  pu2_scale_matrix[0], u4_round_factor, u4_qbits,
                  pu1_nnz[plane]);
        pi2_dst[2] = i4_value;

        i4_value = (x1 - x3);
        FWD_QUANT(i4_value, u4_abs_value, i4_sign, pu2_threshold_matrix[0],
                  pu2_scale_matrix[0], u4_round_factor, u4_qbits,
                  pu1_nnz[plane]);
        pi2_dst[3] = i4_value;

        i4_value = (x1 + x3);
        FWD_QUANT(i4_value, u4_abs_value, i4_sign, pu2_threshold_matrix[0],
                  pu2_scale_matrix[0], u4_round_factor, u4_qbits,
                  pu1_nnz[plane]);
        pi2_dst[1] = i4_value;

        pi2_dst += 4;
        pi2_src += 4;

    }
}

/*
 *******************************************************************************
 *
 * @brief
 *  This function performs Single stage forward transform CF8 and quantization on 8*8 blocks
 *  for h.264
 *
 * @par Description:
 *  Performs single stage 8x8 forward transform CF8 after calculating the residue
 *  The result is then quantized
 *
 * @param[in] pu1_src
 *  Input 8x8 pixels
 *
 * @param[in] pu1_pred
 *  Input 8x8 pixels
 *
 * @param[in] pi1_out
 * Output 8x8 pixels
 *
 * @param[in] u4_thresh
 *  Threshold under which the coeffs are not quantized
 *
 *  @param[in] u4_qp_div
 *  QP/6
 *
 *  @param[in] u4_qp_rem
 *  QP%6
 *
 * @param[in] u2_src_stride
 *  Source stride
 *
 * @param[in] pred_strd
 * stride for prediciton buffer
 *
 *  @param[in] dst_strd
 *  stride for destination buffer
 *
 *  @param[in] pu4_quant_mat
 *  Pointer to the 4x4 quantization matrix
 *
 * @returns  Void
 *
 *
 *******************************************************************************
 */
void ih264_resi_trans_quant_8x8(UWORD8 *pu1_src,
                                UWORD8 *pu1_pred,
                                WORD16 *pi2_out,
                                WORD32 src_strd,
                                WORD32 pred_strd,
                                const UWORD16 *pu2_scale_matrix,
                                const UWORD16 *pu2_threshold_matrix,
                                UWORD32 u4_qbits,
                                UWORD32 u4_round_factor,
                                UWORD8 *pu1_nnz,
                                WORD16 *pu1_dc_alt_addr)

{
    WORD16 *pi2_out_tmp = pi2_out;
    UWORD32 i;
    WORD32 a0, a1, a2, a3, a4, a5, a6, a7;
    WORD32 r0, r1, r2, r3, r4, r5, r6, r7;
    WORD32 i4_sign;
    UWORD32 u4_abs_value;
    UWORD32 u4_nonzero_coeff = 0;

    UNUSED(pu1_dc_alt_addr);

    /*Horizontal transform */
    /* we are going to use the a's and r's in a twisted way since */
    /*i dont want to declare more variables */
    for(i = 0; i < SUB_BLK_WIDTH_8x8; ++i)
    {
        r0 = pu1_src[0];
        r0 -= pu1_pred[0];
        r1 = pu1_src[1];
        r1 -= pu1_pred[1];
        r2 = pu1_src[2];r2 -= pu1_pred[2];
        r3 = pu1_src[3];r3 -= pu1_pred[3];
        r4 = pu1_src[4];r4 -= pu1_pred[4];
        r5 = pu1_src[5];r5 -= pu1_pred[5];
        r6 = pu1_src[6];r6 -= pu1_pred[6];
        r7 = pu1_src[7];r7 -= pu1_pred[7];


        a0 = r0 + r7;
        a1 = r1 + r6;
        a2 = r2 + r5;
        a3 = r3 + r4;

        a4 = a0 + a3;
        a5 = a1 + a2;
        a6 = a0 - a3;
        a7 = a1 - a2;

        pi2_out_tmp[0] = a4 + a5;

        pi2_out_tmp[2] = a6 + (a7>>1);
        pi2_out_tmp[4] = a4 - a5;
        pi2_out_tmp[6] = (a6>>1) - a7;

        a0 = r0 - r7;
        a1 = r1 - r6;
        a2 = r2 - r5;
        a3 = r3 - r4;

        a4 = a1 + a2 + ((a0>>1) + a0);
        a5 = a0 - a3 - ((a2>>1) + a2);
        a6 = a0 + a3 - ((a1>>1) + a1);
        a7 = a1 - a2 + ((a3>>1) + a3);

        pi2_out_tmp[1] = a4 + (a7>>2);
        pi2_out_tmp[3] = a5 + (a6>>2);
        pi2_out_tmp[5] = a6 - (a5>>2);
        pi2_out_tmp[7] = (a4>>2) - a7;

        pu1_src += src_strd;
        pu1_pred += pred_strd;
        pi2_out_tmp += 8;
    }

    /*vertical transform and quant */

    pi2_out_tmp = pi2_out;

    for (i = 0; i < SUB_BLK_WIDTH_8x8; ++i)
    {

        r0 = pi2_out_tmp[0];
        r1 = pi2_out_tmp[8];
        r2 = pi2_out_tmp[16];
        r3 = pi2_out_tmp[24];
        r4 = pi2_out_tmp[32];
        r5 = pi2_out_tmp[40];
        r6 = pi2_out_tmp[48];
        r7 = pi2_out_tmp[56];

        a0 = r0 + r7;
        a1 = r1 + r6;
        a2 = r2 + r5;
        a3 = r3 + r4;

        a4 = a0 + a3;
        a5 = a1 + a2;
        a6 = a0 - a3;
        a7 = a1 - a2;

        a0 = r0 - r7;
        a1 = r1 - r6;
        a2 = r2 - r5;
        a3 = r3 - r4;

        r0 = a4 + a5;
        r2 = a6 + (a7>>1);
        r4 = a4 - a5;
        r6 = (a6>>1) - a7;

        a4 = a1 + a2 + ((a0>>1) + a0);
        a5 = a0 - a3 - ((a2>>1) + a2);
        a6 = a0 + a3 - ((a1>>1) + a1);
        a7 = a1 - a2 + ((a3>>1) + a3);

        r1 = a4 + (a7>>2);
        r3 = a5 + (a6>>2);
        r5 = a6 - (a5>>2);
        r7 = (a4>>2) - a7;

        FWD_QUANT(r0, u4_abs_value, i4_sign, pu2_threshold_matrix[0],
                  pu2_scale_matrix[0], u4_round_factor, u4_qbits,
                  u4_nonzero_coeff);
        pi2_out_tmp[0] = r0;

        FWD_QUANT(r1, u4_abs_value, i4_sign, pu2_threshold_matrix[8],
                  pu2_scale_matrix[8], u4_round_factor, u4_qbits,
                  u4_nonzero_coeff);
        pi2_out_tmp[8] = r1;

        FWD_QUANT(r2, u4_abs_value, i4_sign, pu2_threshold_matrix[16],
                  pu2_scale_matrix[16], u4_round_factor, u4_qbits,
                  u4_nonzero_coeff);
        pi2_out_tmp[16] = r2;

        FWD_QUANT(r3, u4_abs_value, i4_sign, pu2_threshold_matrix[24],
                  pu2_scale_matrix[24], u4_round_factor, u4_qbits,
                  u4_nonzero_coeff);
        pi2_out_tmp[24] = r3;

        FWD_QUANT(r4, u4_abs_value, i4_sign, pu2_threshold_matrix[32],
                  pu2_scale_matrix[32], u4_round_factor, u4_qbits,
                  u4_nonzero_coeff);
        pi2_out_tmp[32] = r4;

        FWD_QUANT(r5, u4_abs_value, i4_sign, pu2_threshold_matrix[40],
                  pu2_scale_matrix[40], u4_round_factor, u4_qbits,
                  u4_nonzero_coeff);
        pi2_out_tmp[40] = r5;

        FWD_QUANT(r6, u4_abs_value, i4_sign, pu2_threshold_matrix[48],
                  pu2_scale_matrix[48], u4_round_factor, u4_qbits,
                  u4_nonzero_coeff);
        pi2_out_tmp[48] = r6;

        FWD_QUANT(r7, u4_abs_value, i4_sign, pu2_threshold_matrix[56],
                  pu2_scale_matrix[56], u4_round_factor, u4_qbits,
                  u4_nonzero_coeff);
        pi2_out_tmp[56] = r7;

        pi2_out_tmp++;
        pu2_scale_matrix++;
        pu2_threshold_matrix++;
    }
       /* Return total nonzero coefficients in the current sub block */
        *pu1_nnz =  u4_nonzero_coeff;
}
