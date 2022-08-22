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
*  ih264_deblk_tables.c
*
* @brief
*  Contains tables used for deblocking
*
* @author
*  Ittiam
*
* @par List of Tables:
*  - guc_ih264_qp_scale_cr[]
*  - guc_ih264_alpha_table[]
*  - guc_ih264_beta_table[]
*  - guc_ih264_clip_table[][]
*
* @remarks
*  None
*
*******************************************************************************
*/

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

/* System include files */
#include <stdio.h>

/* User include files */
#include "ih264_typedefs.h"
#include "ih264_deblk_tables.h"

/*****************************************************************************/
/* Extern global definitions                                                 */
/*****************************************************************************/

/**
 ******************************************************************************
 * @brief  alpha & beta tables for deblocking
 * input   : indexA [0-51] & indexB [0-51]
 * output  : alpha & beta
 *
 * @remarks Table 8-16 – in H264 Specification,
 * Derivation of offset dependent threshold variables
 *  alpha and beta from indexA and indexB
 ******************************************************************************
 */
const UWORD8 gu1_ih264_alpha_table[52] =
{
     /* indexA :: 0-51 inclusive */
     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,
     4,     4,     5,     6,     7,     8,     9,    10,
    12,    13,    15,    17,    20,    22,    25,    28,
    32,    36,    40,    45,    50,    56,    63,    71,
    80,    90,   101,   113,   127,   144,   162,   182,
   203,   226,   255,   255,
};

const UWORD8 gu1_ih264_beta_table[52] =
{
     /* indexB :: 0-51 inclusive */
     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,
     2,     2,     2,     3,     3,     3,     3,     4,
     4,     4,     6,     6,     7,     7,     8,     8,
     9,     9,    10,    10,    11,    11,    12,    12,
    13,    13,    14,    14,    15,    15,    16,    16,
    17,    17,    18,    18,
};

/**
 ******************************************************************************
 * @brief  t'C0 table for deblocking
 * input   : indexA [0-51] and bS [1,3]
 * output  : t'C0
 *
 * @remarks Table 8-17 – in H264 Specification,
 * Value of variable t'C0 as a function of indexA and bS
 ******************************************************************************
 */
const UWORD8 gu1_ih264_clip_table[52][4] =
{
    /* indexA :: 0-51 inclusive */
    { 0, 0, 0, 0}, { 0, 0, 0, 0}, { 0, 0, 0, 0}, { 0, 0, 0, 0},
    { 0, 0, 0, 0}, { 0, 0, 0, 0}, { 0, 0, 0, 0}, { 0, 0, 0, 0},
    { 0, 0, 0, 0}, { 0, 0, 0, 0}, { 0, 0, 0, 0}, { 0, 0, 0, 0},
    { 0, 0, 0, 0}, { 0, 0, 0, 0}, { 0, 0, 0, 0}, { 0, 0, 0, 0},
    { 0, 0, 0, 0}, { 0, 0, 0, 1}, { 0, 0, 0, 1}, { 0, 0, 0, 1},
    { 0, 0, 0, 1}, { 0, 0, 1, 1}, { 0, 0, 1, 1}, { 0, 1, 1, 1},
    { 0, 1, 1, 1}, { 0, 1, 1, 1}, { 0, 1, 1, 1}, { 0, 1, 1, 2},
    { 0, 1, 1, 2}, { 0, 1, 1, 2}, { 0, 1, 1, 2}, { 0, 1, 2, 3},
    { 0, 1, 2, 3}, { 0, 2, 2, 3}, { 0, 2, 2, 4}, { 0, 2, 3, 4},
    { 0, 2, 3, 4}, { 0, 3, 3, 5}, { 0, 3, 4, 6}, { 0, 3, 4, 6},
    { 0, 4, 5, 7}, { 0, 4, 5, 8}, { 0, 4, 6, 9}, { 0, 5, 7,10},
    { 0, 6, 8,11}, { 0, 6, 8,13}, { 0, 7,10,14}, { 0, 8,11,16},
    { 0, 9,12,18}, { 0,10,13,20}, { 0,11,15,23}, { 0,13,17,25},
};
