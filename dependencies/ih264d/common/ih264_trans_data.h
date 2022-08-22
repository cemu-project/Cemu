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
 *  ih264_trans_data.h
 *
 * @brief
 *  Contains declaration of global variables for H264 transform , qnat and inverse quant
 *
 * @author
 *  Ittiam
 *
 * @remarks
 *
 *******************************************************************************
 */
#ifndef IH264_GLOBAL_DATA_H_
#define IH264_GLOBAL_DATA_H_

/*****************************************************************************/
/* Extern global declarations                                                */
/*****************************************************************************/

/* Scaling matrices for h264 quantization */
extern const UWORD16 g_scal_coff_h264_4x4[16];
extern const UWORD16 g_scal_coff_h264_8x8[16];


/**
 ******************************************************************************
 * @brief  Scale Table for quantizing 4x4 subblock. To quantize a given 4x4 DCT
 * transformed block, the coefficient at index location (i,j) is scaled by one of
 * the constants in this table and right shift the result by (QP_BITS_h264_4x4 +
 * floor(qp/6)), here qp is the quantization parameter used to quantize the mb.
 *
 * input   : qp%6, index location (i,j)
 * output  : scale constant.
 *
 * @remarks 16 constants for each index position of the subblock and 6 for each
 * qp%6 in the range 0-5 inclusive.
 ******************************************************************************
 */
extern const UWORD16 gu2_quant_scale_matrix_4x4[96];

/**
 ******************************************************************************
 * @brief  Round Factor for quantizing subblock. While quantizing a given 4x4 DCT
 * transformed block, the coefficient at index location (i,j) is scaled by one of
 * the constants in the table gu2_forward_quant_scalar_4x4 and then right shift
 * the result by (QP_BITS_h264_4x4 + floor(qp/6)).
 * Before right shifting a round factor is added.
 * The round factor can be any value [a * (1 << (QP_BITS_h264_4x4 + floor(qp/6)))]
 * for 'a' lies in the range 0-0.5.
 * Here qp is the quantization parameter used to quantize the mb.
 *
 * input   : qp/6
 * output  : round factor.
 *
 * @remarks The round factor is constructed by setting a = 1/3
 ******************************************************************************
 */
extern const UWORD32 gu4_forward_quant_round_factor_4x4[9];

/**
 ******************************************************************************
 * @brief  Threshold Table. Quantizing the given DCT coefficient is done only if
 * it exceeds the threshold value presented in this table.
 *
 * input   : qp/6, qp%6, index location (i,j)
 * output  : Threshold constant.
 *
 * @remarks 16 constants for each index position of the subblock and 6 for each
 * qp%6 in the range 0-5 inclusive and 9 for each qp/6 in the range 0-51.
 ******************************************************************************
 */
extern const UWORD16 gu2_forward_quant_threshold_4x4[96];

/**
 ******************************************************************************
 * @brief  Scale Table for quantizing 8x8 subblock. To quantize a given 8x8 DCT
 * transformed block, the coefficient at index location (i,j) is scaled by one of
 * the constants in this table and right shift the result by (QP_BITS_h264_8x8 +
 * floor(qp/6)), here qp is the quantization parameter used to quantize the mb.
 *
 * input   : qp%6, index location (i,j)
 * output  : scale constant.
 *
 * @remarks 64 constants for each index position of the subblock and 6 for each
 * qp%6 in the range 0-5 inclusive.
 ******************************************************************************
 */
extern const UWORD16 gu2_quant_scale_matrix_8x8 [384];

/**
 ******************************************************************************
 * @brief  Specification of QPc as a function of qPi
 *
 * input   : qp luma
 * output  : qp chroma.
 *
 * @remarks Refer Table 8-15 of h264 specification.
 ******************************************************************************
 */
extern const UWORD8 gu1_qpc_fqpi[52];


#endif /* IH264_GLOBAL_DATA_H_ */
