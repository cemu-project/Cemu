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

#ifndef _IH264D_INTER_PRED_H_
#define _IH264D_INTER_PRED_H_

/*!
 **************************************************************************
 * \file ih264d_inter_pred.h
 *
 * \brief
 *    Decalaration for routines defined in MorionCompensate.c
 *
 * Detailed_description
 *
 * \date
 *    creation_date
 *
 * \author  Arvind Raman
 **************************************************************************
 */

#include "ih264d_structs.h"

#define BUFFER_WIDTH        16
/*!
 **************************************************************************
 *   \brief   PRED_BUFFER_WIDTH / HEIGHT
 *
 *   Width and height of the 16 bit (also reused a 2 8 bits buffers). The
 *   required dimensions for these buffers are 21x21, however to align the
 *   start of every row to a WORD aligned boundary the width has been increased
 *   to 24.
 **************************************************************************
 */
//#define PRED_BUFFER_WIDTH   24
//#define PRED_BUFFER_HEIGHT  21
#define PRED_BUFFER_WIDTH   24*2
#define PRED_BUFFER_HEIGHT  24*2

void ih264d_fill_pred_info(WORD16 *pi2_mv,WORD32 part_width,WORD32 part_height, WORD32 sub_mb_num,
                           WORD32 pred_dir,pred_info_pkd_t *ps_pred_pkd,WORD8 i1_buf_id,
                           WORD8 i1_ref_idx,UWORD32 *pu4_wt_offset,UWORD8 u1_pic_type);

WORD32 ih264d_form_mb_part_info_bp(pred_info_pkd_t *ps_pred_pkd,
                                 dec_struct_t * ps_dec,
                                 UWORD16 u2_mb_x,
                                 UWORD16 u2_mb_y,
                                 WORD32 mb_index,
                                 dec_mb_info_t *ps_cur_mb_info);

WORD32 ih264d_form_mb_part_info_mp(pred_info_pkd_t *ps_pred_pkd,
                                 dec_struct_t * ps_dec,
                                 UWORD16 u2_mb_x,
                                 UWORD16 u2_mb_y,
                                 WORD32 mb_index,
                                 dec_mb_info_t *ps_cur_mb_info);


void ih264d_motion_compensate_bp(dec_struct_t * ps_dec, dec_mb_info_t *ps_cur_mb_info);
void ih264d_motion_compensate_mp(dec_struct_t * ps_dec, dec_mb_info_t *ps_cur_mb_info);


void TransferRefBuffs(dec_struct_t *ps_dec);

void ih264d_multiplex_ref_data(dec_struct_t * ps_dec,
                               pred_info_t *ps_pred,
                               UWORD8* pu1_dest_y,
                               UWORD8* pu1_dest_u,
                               dec_mb_info_t *ps_cur_mb_info,
                               UWORD16 u2_dest_wd_y,
                               UWORD16 u2_dest_wd_uv,
                               UWORD8 u1_dir);
#endif /* _IH264D_INTER_PRED_H_ */

