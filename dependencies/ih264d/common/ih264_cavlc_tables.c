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
* @file
*  ih264_cavlc_tables.c
*
* @brief
*  This file contains H264 cavlc tables for encoding coeff_tokens, levels, total
*  zeros and runs before zeros
*
* @author
*   Ittiam
*
* @par List of Tables
*   - gu1_code_coeff_token_table
*   - gu1_size_coeff_token_table
*   - gu1_code_coeff_token_table_chroma
*   - gu1_size_coeff_token_table_chroma
*   - gu1_threshold_vlc_level
*   - gu1_size_zero_table
*   - gu1_code_zero_table
*   - gu1_size_zero_table_chroma
*   - gu1_code_zero_table_chroma
*   - gu1_index_zero_table
*   - gu1_size_run_table
*   - gu1_code_run_table
*   - gu4_codeword_level_tables
*   - gu1_codesize_level_tables
*
* @remarks
*  none
*
******************************************************************************
*/

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

/* User include files */
#include "ih264_typedefs.h"
#include "ih264_cavlc_tables.h"


/*****************************************************************************/
/* Extern global definitions                                                 */
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
const UWORD8 gu1_cbp_map_tables[48][2]=
{
    { 3,  0},   {29,  2},   {30,  3},   {17,  7},   {31,  4},   {18,  8},   {37, 17},   { 8, 13},
    {32,  5},   {38, 18},   {19,  9},   { 9, 14},   {20, 10},   {10, 15},   {11, 16},   { 2, 11},
    {16,  1},   {33, 32},   {34, 33},   {21, 36},   {35, 34},   {22, 37},   {39, 44},   { 4, 40},
    {36, 35},   {40, 45},   {23, 38},   { 5, 41},   {24, 39},   { 6, 42},   { 7, 43},   { 1, 19},
    {41,  6},   {42, 24},   {43, 25},   {25, 20},   {44, 26},   {26, 21},   {46, 46},   {12, 28},
    {45, 27},   {47, 47},   {27, 22},   {13, 29},   {28, 23},   {14, 30},   {15, 31},   { 0, 12},
};


/**
 ******************************************************************************
 * @brief  total non-zero coefficients and numbers of trailing ones of a residual
 * block are mapped to coeff_token using the tables given below.
 * input  : VLC-Num  | Trailing ones | Total coeffs
 * output : coeff_token (code word, size of the code word)
 * @remarks Table-9-5 coeff_token mapping to TotalCoeff( coeff_token )
 * and TrailingOnes( coeff_token ) in H264 spec
 ******************************************************************************
 */
const UWORD8 gu1_code_coeff_token_table[3][4][16] =
{
    {
        { 5,  7,  7,  7,  7, 15, 11,  8, 15, 11, 15, 11, 15, 11,  7,  4, },
        { 1,  4,  6,  6,  6,  6, 14, 10, 14, 10, 14, 10,  1, 14, 10,  6, },
        { 0,  1,  5,  5,  5,  5,  5, 13,  9, 13,  9, 13,  9, 13,  9,  5, },
        { 0,  0,  3,  3,  4,  4,  4,  4,  4, 12, 12,  8, 12,  8, 12,  8, },
    },
    {
        {11,  7,  7,  7,  4,  7, 15, 11, 15, 11,  8, 15, 11,  7,  9,  7, },
        { 2,  7, 10,  6,  6,  6,  6, 14, 10, 14, 10, 14, 10, 11,  8,  6, },
        { 0,  3,  9,  5,  5,  5,  5, 13,  9, 13,  9, 13,  9,  6, 10,  5, },
        { 0,  0,  5,  4,  6,  8,  4,  4,  4, 12,  8, 12, 12,  8,  1,  4, },
    },
    {
        {15, 11,  8, 15, 11,  9,  8, 15, 11, 15, 11,  8, 13,  9,  5,  1, },
        {14, 15, 12, 10,  8, 14, 10, 14, 14, 10, 14, 10,  7, 12,  8,  4, },
        { 0, 13, 14, 11,  9, 13,  9, 13, 10, 13,  9, 13,  9, 11,  7,  3, },
        { 0,  0, 12, 11, 10,  9,  8, 13, 12, 12, 12,  8, 12, 10,  6,  2, },
    },
};

const UWORD8 gu1_size_coeff_token_table[3][4][16] =
{
    {
        { 6,  8,  9, 10, 11, 13, 13, 13, 14, 14, 15, 15, 16, 16, 16, 16, },
        { 2,  6,  8,  9, 10, 11, 13, 13, 14, 14, 15, 15, 15, 16, 16, 16, },
        { 0,  3,  7,  8,  9, 10, 11, 13, 13, 14, 14, 15, 15, 16, 16, 16, },
        { 0,  0,  5,  6,  7,  8,  9, 10, 11, 13, 14, 14, 15, 15, 16, 16, },
    },
    {
        { 6,  6,  7,  8,  8,  9, 11, 11, 12, 12, 12, 13, 13, 13, 14, 14, },
        { 2,  5,  6,  6,  7,  8,  9, 11, 11, 12, 12, 13, 13, 14, 14, 14, },
        { 0,  3,  6,  6,  7,  8,  9, 11, 11, 12, 12, 13, 13, 13, 14, 14, },
        { 0,  0,  4,  4,  5,  6,  6,  7,  9, 11, 11, 12, 13, 13, 13, 14, },
    },
    {
        { 6,  6,  6,  7,  7,  7,  7,  8,  8,  9,  9,  9, 10, 10, 10, 10, },
        { 4,  5,  5,  5,  5,  6,  6,  7,  8,  8,  9,  9,  9, 10, 10, 10, },
        { 0,  4,  5,  5,  5,  6,  6,  7,  7,  8,  8,  9,  9, 10, 10, 10, },
        { 0,  0,  4,  4,  4,  4,  4,  5,  6,  7,  8,  8,  9, 10, 10, 10, },
    },
};
const UWORD8 gu1_code_coeff_token_table_chroma[4][4] =
{
    { 7,  4,  3,  2, },
    { 1,  6,  3,  3, },
    { 0,  1,  2,  2, },
    { 0,  0,  5,  0, },
};

const UWORD8 gu1_size_coeff_token_table_chroma[4][4] =
{
    { 6,  6,  6,  6, },
    { 1,  6,  7,  8, },
    { 0,  3,  7,  8, },
    { 0,  0,  6,  7, },
};

/**
 ******************************************************************************
 * @brief  After encoding the current Level, to encode the next level, the choice
 * of VLC table needs to be updated. The update is carried basing on a set of thresholds.
 * These thresholds are listed in the table below for lookup.
 * input  : suffix_length
 * output : threshold
 ******************************************************************************
 */
const UWORD8 gu1_threshold_vlc_level[6] =
{
    0, 3, 6, 12, 24, 48
};


/**
 ******************************************************************************
 * @brief  table for encoding total number of zeros
 * input  : coeff_token, total zeros
 * output : code word, size of the code word
 * @remarks Table-9-7, 9-8  total_zeros tables for 4x4 blocks with
 * TotalCoeff( coeff_token ) in H264 spec
 ******************************************************************************
 */
const UWORD8 gu1_size_zero_table[135] =
{
     1, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 9,
     3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 6, 6, 6, 6,
     4, 3, 3, 3, 4, 4, 3, 3, 4, 5, 5, 6, 5, 6,
     5, 3, 4, 4, 3, 3, 3, 4, 3, 4, 5, 5, 5,
     4, 4, 4, 3, 3, 3, 3, 3, 4, 5, 4, 5,
     6, 5, 3, 3, 3, 3, 3, 3, 4, 3, 6,
     6, 5, 3, 3, 3, 2, 3, 4, 3, 6,
     6, 4, 5, 3, 2, 2, 3, 3, 6,
     6, 6, 4, 2, 2, 3, 2, 5,
     5, 5, 3, 2, 2, 2, 4,
     4, 4, 3, 3, 1, 3,
     4, 4, 2, 1, 3,
     3, 3, 1, 2,
     2, 2, 1,
     1, 1,
};
const UWORD8 gu1_code_zero_table[135] =
{
     1, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 1,
     7, 6, 5, 4, 3, 5, 4, 3, 2, 3, 2, 3, 2, 1, 0,
     5, 7, 6, 5, 4, 3, 4, 3, 2, 3, 2, 1, 1, 0,
     3, 7, 5, 4, 6, 5, 4, 3, 3, 2, 2, 1, 0,
     5, 4, 3, 7, 6, 5, 4, 3, 2, 1, 1, 0,
     1, 1, 7, 6, 5, 4, 3, 2, 1, 1, 0,
     1, 1, 5, 4, 3, 3, 2, 1, 1, 0,
     1, 1, 1, 3, 3, 2, 2, 1, 0,
     1, 0, 1, 3, 2, 1, 1, 1,
     1, 0, 1, 3, 2, 1, 1,
     0, 1, 1, 2, 1, 3,
     0, 1, 1, 1, 1,
     0, 1, 1, 1,
     0, 1, 1,
     0, 1,
};
const UWORD8 gu1_size_zero_table_chroma[9] =
{
     1, 2, 3, 3,
     1, 2, 2,
     1, 1,
};
const UWORD8 gu1_code_zero_table_chroma[9] =
{
     1, 1, 1, 0,
     1, 1, 0,
     1, 0,
};

/**
 ******************************************************************************
 * @brief  index to access zero table (look up)
 * input  : TotalCoeff( coeff_token )
 * output : index to access zero table
 ******************************************************************************
 */
const UWORD8 gu1_index_zero_table[15] =
{
    0,  16,  31,  45,  58,  70,  81,  91, 100, 108, 115, 121, 126, 130, 133,
};

/**
 ******************************************************************************
 * @brief  table for encoding runs of zeros before
 * input  : zeros left, runs of zeros before
 * output : code word, size of the code word
 * @remarks Table-9-10  table for run_before in H264 spec
 ******************************************************************************
 */
const UWORD8 gu1_size_run_table[42] =
{
      1,  1,
      1,  2,  2,
      2,  2,  2,  2,
      2,  2,  2,  3,  3,
      2,  2,  3,  3,  3,  3,
      2,  3,  3,  3,  3,  3,  3,
      3,  3,  3,  3,  3,  3,  3,  4,  5,  6,  7,  8,  9, 10, 11,
};
const UWORD8 gu1_code_run_table[42] =
{
      1,  0,
      1,  1,  0,
      3,  2,  1,  0,
      3,  2,  1,  1,  0,
      3,  2,  3,  2,  1,  0,
      3,  0,  1,  3,  2,  5,  4,
      7,  6,  5,  4,  3,  2,  1,  1,  1,  1,  1,  1,  1,  1,  1,
};
/**
 ******************************************************************************
 * @brief  index to access zero table (look up)
 * input  : TotalCoeff( coeff_token )
 * output : index to access zero table
 ******************************************************************************
 */
const UWORD8 gu1_index_run_table[7] =
{
    0,  2,  5,  9,  14,  20,  27,
};
