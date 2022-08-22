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
#ifndef  _IH264D_UTILS_H_
#define  _IH264D_UTILS_H_
/*!
**************************************************************************
* \file ih264d_utils.h
*
* \brief
*    Contains declaration of routines
*    that handle of start and end of pic processing
*
* \date
*    19/12/2002
*
* \author  AI
**************************************************************************
*/
#include "ih264d_defs.h"
#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264d_structs.h"
#include "ih264d_parse_cavlc.h"

#define PS_DEC_ALIGNED_FREE(ps_dec, y) \
if(y) {ps_dec->pf_aligned_free(ps_dec->pv_mem_ctxt, ((void *)y)); (y) = NULL;}
void pad_frm_buff_vert(dec_struct_t *ps_dec);

UWORD8 ih264d_is_end_of_pic(UWORD16 u2_frame_num,
                            UWORD8 u1_nal_ref_idc,
                            pocstruct_t *ps_cur_poc,
                            pocstruct_t *ps_prev_poc,
                            dec_slice_params_t * ps_prev_slice,
                            UWORD8 u1_pic_order_cnt_type,
                            UWORD8 u1_nal_unit_type,
                            UWORD32 u4_idr_pic_id,
                            UWORD8 u1_field_pic_flag,
                            UWORD8 u1_bottom_field_flag);

WORD32 ih264d_end_of_pic_processing(dec_struct_t * ps_dec);

WORD32 ih264d_init_pic(dec_struct_t *ps_dec,
                       UWORD16 u2_frame_num,
                       WORD32 i4_poc,
                       dec_pic_params_t * ps_pps);

WORD32 ih264d_end_of_pic_processing(dec_struct_t * ps_dec);
WORD32 ih264d_decode_pic_order_cnt(UWORD8 u1_is_idr_slice,
                                   UWORD32 u2_frame_num,
                                   pocstruct_t *ps_prev_poc,
                                   pocstruct_t *ps_cur_poc,
                                   dec_slice_params_t *ps_cur_slice,
                                   dec_pic_params_t * ps_pps,
                                   UWORD8 u1_nal_ref_idc,
                                   UWORD8 u1_bottom_field_flag,
                                   UWORD8 u1_field_pic_flag,
                                   WORD32 *pi4_poc);
void ih264d_release_display_bufs(dec_struct_t *ps_dec);
WORD32 ih264d_assign_display_seq(dec_struct_t *ps_dec);
void ih264d_assign_pic_num(dec_struct_t *ps_dec);

void ih264d_unpack_coeff4x4_dc_4x4blk(tu_sblk4x4_coeff_data_t *ps_tu_4x4,
                                      WORD16 *pi2_out_coeff_data,
                                      UWORD8 *pu1_inv_scan);

WORD32 ih264d_update_qp(dec_struct_t * ps_dec, const WORD8 i1_qp);
WORD32 ih264d_decode_gaps_in_frame_num(dec_struct_t *ps_dec,
                                       UWORD16 u2_frame_num);

WORD32 ih264d_get_next_display_field(dec_struct_t * ps_dec,
                                  ivd_out_bufdesc_t *ps_out_buffer,
                                  ivd_get_display_frame_op_t *pv_disp_op);

void ih264d_release_display_field(dec_struct_t *ps_dec,
                                  ivd_get_display_frame_op_t *pv_disp_op);
void ih264d_close_video_decoder(iv_obj_t *iv_obj_t);
WORD32 ih264d_get_dpb_size(dec_seq_params_t *ps_seq);
WORD32 ih264d_get_next_nal_unit(UWORD8 *pu1_buf,
                                UWORD32 u4_cur_pos,
                                UWORD32 u4_max_ofst,
                                UWORD32 *pu4_length_of_start_code);

WORD16 ih264d_free_dynamic_bufs(dec_struct_t * ps_dec);
#endif /* _IH264D_UTILS_H_ */
