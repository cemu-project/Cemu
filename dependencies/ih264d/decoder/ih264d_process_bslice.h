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
#ifndef _IH264D_PARSE_BSLICE_H_
#define _IH264D_PARSE_BSLICE_H_
/*!
**************************************************************************
* \file ih264d_process_bslice.h
*
* \brief
*    Contains declarations of routines that decode a B slice type
*
* Detailed_description
*
* \date
*    21/12/2002
*
* \author  NS
**************************************************************************
*/
#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264d_structs.h"
WORD32 ih264d_parse_bslice(dec_struct_t * ps_dec,
                            UWORD16 u2_first_mb_in_slice);
WORD32 ih264d_decode_spatial_direct(dec_struct_t * ps_dec,
                                    UWORD8 u1_wd_x,
                                    dec_mb_info_t * ps_cur_mb_info,
                                    UWORD8 u1_mb_num);
WORD32 ih264d_decode_temporal_direct(dec_struct_t * ps_dec,
                                     UWORD8 u1_wd_x,
                                     dec_mb_info_t * ps_cur_mb_info,
                                     UWORD8 u1_mb_num);
WORD32 parseBSliceData(dec_struct_t * ps_dec,
                       dec_slice_params_t * ps_slice,
                       UWORD16 u2_first_mb_in_slice);
WORD32 parseBSliceData(dec_struct_t * ps_dec,
                       dec_slice_params_t * ps_slice,
                       UWORD16 u2_first_mb_in_slice);

void ih264d_init_ref_idx_lx_b(dec_struct_t *ps_dec);

void ih264d_convert_frm_to_fld_list(struct pic_buffer_t *ps_ref_pic_buf_lx,
                                    UWORD8 *pu1_L0,
                                    dec_struct_t *ps_dec,
                                    UWORD8 u1_num_short_term_bufs);

void ih264d_convert_frm_mbaff_list(dec_struct_t *ps_dec);
void ih264d_one_to_one(dec_struct_t *ps_dec,
                       struct pic_buffer_t *ps_col_pic,
                       directmv_t *ps_direct,
                       UWORD8 u1_wd_x,
                       WORD32 u2_sub_mb_ofst,
                       dec_mb_info_t * ps_cur_mb_info);
void ih264d_mbaff_cross_pmbair(dec_struct_t *ps_dec,
                               struct pic_buffer_t *ps_col_pic,
                               directmv_t *ps_direct,
                               UWORD8 u1_wd_x,
                               WORD32 u2_sub_mb_ofst,
                               dec_mb_info_t * ps_cur_mb_info);
void ih264d_frm_to_fld(dec_struct_t *ps_dec,
                       struct pic_buffer_t *ps_col_pic,
                       directmv_t *ps_direct,
                       UWORD8 u1_wd_x,
                       WORD32 u2_sub_mb_ofst,
                       dec_mb_info_t * ps_cur_mb_info);
void ih264d_fld_to_frm(dec_struct_t *ps_dec,
                       struct pic_buffer_t *ps_col_pic,
                       directmv_t *ps_direct,
                       UWORD8 u1_wd_x,
                       WORD32 u2_sub_mb_ofst,
                       dec_mb_info_t * ps_cur_mb_info);
void ih264d_mbaff_to_fld(dec_struct_t *ps_dec,
                         struct pic_buffer_t *ps_col_pic,
                         directmv_t *ps_direct,
                         UWORD8 u1_wd_x,
                         WORD32 u2_sub_mb_ofst,
                         dec_mb_info_t * ps_cur_mb_info);
void ih264d_fld_to_mbaff(dec_struct_t *ps_dec,
                         struct pic_buffer_t *ps_col_pic,
                         directmv_t *ps_direct,
                         UWORD8 u1_wd_x,
                         WORD32 u2_sub_mb_ofst,
                         dec_mb_info_t * ps_cur_mb_info);
WORD32 ih264d_cal_col_pic(dec_struct_t *ps_dec);

WORD32 ih264d_mv_pred_ref_tfr_nby2_bmb(dec_struct_t * ps_dec,
                                     UWORD8 u1_num_mbs,
                                     UWORD8 u1_num_mbsNby2);

#endif /* _IH264D_PARSE_BSLICE_H_ */
