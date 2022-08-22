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

#ifndef _IH264D_MVPRED_H_
#define _IH264D_MVPRED_H_

/**
**************************************************************************
* \file ih264d_mvpred.h
*
* \brief
*    This file contains declarations of functions specific to decoding
*    Motion vector.
*
* Detailed_description
*
* \date
*    10-12-2002
*
* \author  Arvind Raman
**************************************************************************
*/
#include "ih264d_structs.h"
#include "ih264d_defs.h"
//#include "structs.h"

/** Reference number that is not valid */
#define OUT_OF_RANGE_REF  -1

#define ONE_TO_ONE    0
#define FRM_TO_FLD    1
#define FLD_TO_FRM    2

/**
**************************************************************************
*   \brief   POSITION_IN_MVBANK
*
*   a: Pointer to the top left subMb of the MB in the MV bank array
*   b: Horiz posn in terms of subMbs
*   c: Vert posn in terms of subMbs
*   d: subMb number
**************************************************************************
*/
#define POSITION_IN_MVBANK(a, b, c, d) (a) + (c) * (d) + (b)



/**
**************************************************************************
*   \brief   col4x4_t
*
*   Container to return the information related to the co-located 4x4
*   sub-macroblock.
**************************************************************************
*/
typedef struct
{
  mv_pred_t *ps_mv;     /** Ptr to the Mv bank */
  UWORD16 u2_mb_addr_col;       /** Addr of the co-located MB */
  WORD16 i2_mv[2];      /** Mv of the colocated MB */
  WORD8 i1_ref_idx_col;     /** Ref idx of the co-located picture */
  UWORD8 u1_col_pic;        /** Idx of the colocated pic */
  UWORD8 u1_yM;     /** "y" coord of the colocated MB addr */
  UWORD8 u1_vert_mv_scale;      /** as defined in sec 8.4.1.2.1 */
} col4x4_t;





void ih264d_update_nnz_for_skipmb(dec_struct_t * ps_dec,
                                  dec_mb_info_t * ps_cur_mb_info,
                                  UWORD8 u1_entrpy);

void ih264d_get_motion_vector_predictor(mv_pred_t * ps_result,
                                        mv_pred_t **ps_mv_pred,
                                        UWORD8 u1_ref_idx,
                                        UWORD8 u1_B,
                                        const UWORD8 *pu1_mv_pred_condition);
void ih264d_mbaff_mv_pred(mv_pred_t **ps_mv_pred,
                          UWORD8 u1_sub_mb_num,
                          mv_pred_t *ps_mv_nmb,
                          mv_pred_t *ps_mv_ntop,
                          dec_struct_t *ps_dec,
                          UWORD8 uc_mb_part_width,
                          dec_mb_info_t *ps_cur_mb_info,
                          UWORD8* pu0_scale);
void ih264d_non_mbaff_mv_pred(mv_pred_t **ps_mv_pred,
                              UWORD8 u1_sub_mb_num,
                              mv_pred_t *ps_mv_nmb,
                              mv_pred_t *ps_mv_ntop,
                              dec_struct_t *ps_dec,
                              UWORD8 uc_mb_part_width,
                              dec_mb_info_t *ps_cur_mb_info);
UWORD8 ih264d_mvpred_nonmbaff(dec_struct_t *ps_dec,
                              dec_mb_info_t *ps_cur_mb_info,
                              mv_pred_t *ps_mv_nmb,
                              mv_pred_t *ps_mv_ntop,
                              mv_pred_t *ps_mv_final_pred,
                              UWORD8 u1_sub_mb_num,
                              UWORD8 uc_mb_part_width,
                              UWORD8 u1_lx_start,
                              UWORD8 u1_lxend,
                              UWORD8 u1_mb_mc_mode);

UWORD8 ih264d_mvpred_nonmbaffB(dec_struct_t *ps_dec,
                               dec_mb_info_t *ps_cur_mb_info,
                               mv_pred_t *ps_mv_nmb,
                               mv_pred_t *ps_mv_ntop,
                               mv_pred_t *ps_mv_final_pred,
                               UWORD8 u1_sub_mb_num,
                               UWORD8 uc_mb_part_width,
                               UWORD8 u1_lx_start,
                               UWORD8 u1_lxend,
                               UWORD8 u1_mb_mc_mode);

UWORD8 ih264d_mvpred_mbaff(dec_struct_t *ps_dec,
                           dec_mb_info_t *ps_cur_mb_info,
                           mv_pred_t *ps_mv_nmb,
                           mv_pred_t *ps_mv_ntop,
                           mv_pred_t *ps_mv_final_pred,
                           UWORD8 u1_sub_mb_num,
                           UWORD8 uc_mb_part_width,
                           UWORD8 u1_lx_start,
                           UWORD8 u1_lxend,
                           UWORD8 u1_mb_mc_mode);

void ih264d_rep_mv_colz(dec_struct_t *ps_dec,
                        mv_pred_t *ps_mv_pred_src,
                        mv_pred_t *ps_mv_pred_dst,
                        UWORD8 u1_sub_mb_num,
                        UWORD8 u1_colz,
                        UWORD8 u1_ht,
                        UWORD8 u1_wd);

#endif  /* _IH264D_MVPRED_H_ */
