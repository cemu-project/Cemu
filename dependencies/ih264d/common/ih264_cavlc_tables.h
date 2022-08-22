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
******************************************************************************
* @file ih264_cavlc_tables.h
*
* @brief
*  This file contains enumerations, macros and extern declarations of H264
*  cavlc tables
*
* @author
*  Ittiam
*
*  @remarks
*   none
******************************************************************************
*/

#ifndef IH264_CAVLC_TABLES_H_
#define IH264_CAVLC_TABLES_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
/**
******************************************************************************
 *  @brief  maximum zeros left
******************************************************************************
 */
#define MAX_ZERO_LEFT 6

/*****************************************************************************/
/* Extern global declarations                                                */
/*****************************************************************************/

/**
 ******************************************************************************
 * @brief  Assignment of cbp to a codenum for intra and inter prediction modes
 * chroma format idc != 0
 * input  : cbp, intra - 0/inter - 1
 * output : codenum
 * @remarks Table 9-4 – Assignment of codeNum to values of coded_block_pattern
 * for macroblock prediction modes in H264 spec
 ******************************************************************************
 */
extern const UWORD8 gu1_cbp_map_tables[48][2];

/**
 ******************************************************************************
 * @brief  total non-zero coefficients and numbers of trailing ones of a residual
 * block are mapped to coefftoken using the tables given below.
 * input  : VLC-Num  | Trailing ones | Total coeffs
 * output : coeff_token (code word, size of the code word)
 * @remarks Table-9-5 coeff_token mapping to TotalCoeff( coeff_token )
 * and TrailingOnes( coeff_token ) in H264 spec
 ******************************************************************************
 */
extern const UWORD8 gu1_code_coeff_token_table[3][4][16];
extern const UWORD8 gu1_size_coeff_token_table[3][4][16];
extern const UWORD8 gu1_code_coeff_token_table_chroma[4][4];
extern const UWORD8 gu1_size_coeff_token_table_chroma[4][4];

/**
 ******************************************************************************
 * @brief  Thresholds for determining whether to increment Level table number.
 * input  : suffix_length
 * output : threshold
 ******************************************************************************
 */
extern const UWORD8 gu1_threshold_vlc_level[6];

/**
 ******************************************************************************
 * @brief  table for encoding total number of zeros
 * input  : coeff_token, total zeros
 * output : code word, size of the code word
 * @remarks Table-9-7, 9-8  total_zeros tables for 4x4 blocks with
 * TotalCoeff( coeff_token ) in H264 spec
 ******************************************************************************
 */
extern const UWORD8 gu1_size_zero_table[135];
extern const UWORD8 gu1_code_zero_table[135];
extern const UWORD8 gu1_size_zero_table_chroma[9];
extern const UWORD8 gu1_code_zero_table_chroma[9];

/**
 ******************************************************************************
 * @brief  index to access zero table (for speed)
 * input  : TotalCoeff( coeff_token )
 * output : index to access zero table
 ******************************************************************************
 */
extern const UWORD8 gu1_index_zero_table[15];

/**
 ******************************************************************************
 * @brief  table for encoding runs of zeros before
 * input  : zeros left, runs of zeros before
 * output : code word, size of the code word
 * @remarks Table-9-10  table for run_before in H264 spec
 ******************************************************************************
 */
extern const UWORD8 gu1_size_run_table[42];
extern const UWORD8 gu1_code_run_table[42];

/**
 ******************************************************************************
 * @brief  index to access run table (look up)
 * input  : zeros left
 * output : index to access run table
 ******************************************************************************
 */
extern const UWORD8 gu1_index_run_table[7];

#endif /* IH264_CAVLC_TABLES_H_ */
