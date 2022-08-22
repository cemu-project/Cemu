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
*  ih264_resi_trans.h
*
* @brief
*  Functions declarations for residue and forward transform
*
* @par List of Functions:
*  - ih264_resi_trans_ft
*  - ih264_resi_trans_4x4
*  - ih264_resi_trans_4x4
*  - ih264_resi_trans_4x4_a9
*  - ih264_resi_trans_4x4_a9
*
* @author
*  Ittiam
*
* @remarks
*  None
*
*******************************************************************************
*/

#ifndef IH264_RESI_TRANS_H_
#define IH264_RESI_TRANS_H_

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/

typedef void ih264_resi_trans_ft(UWORD8 *pu1_src,
                                 UWORD8 *pu1_pred,
                                 WORD32 *pi4_out,
                                 WORD32 src_strd,
                                 WORD32 pred_strd,
                                 WORD32 out_strd);

/*C functions*/

ih264_resi_trans_ft ih264_resi_trans_4x4;

ih264_resi_trans_ft ih264_resi_trans_8x8;

/*A9 functions*/

ih264_resi_trans_ft ih264_resi_trans_4x4_a9;

ih264_resi_trans_ft ih264_resi_trans_8x8_a9;

#endif /* IH264_RESI_TRANS_H_ */
