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
 *  ih264_deblk_tables.h
 *
 * @brief
 *  This file contains declarations of tables used for deblocking
 *
 * @author
 *  Ittiam
 *
 * @par List of Functions:
 *
 * @remarks
 *  None
 *
 *******************************************************************************
 */

#ifndef IH264_DEBLK_TABLES_H_
#define IH264_DEBLK_TABLES_H_

/*****************************************************************************/
/* Extern global declarations                                                */
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
extern const UWORD8 gu1_ih264_alpha_table[52];

extern const UWORD8 gu1_ih264_beta_table[52];

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
extern const UWORD8 gu1_ih264_clip_table[52][4];

#endif /* IH264_DEBLK_TABLES_H_ */
