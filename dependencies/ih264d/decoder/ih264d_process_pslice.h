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
#ifndef _IH264D_PROCESS_PSLICE_H_
#define _IH264D_PROCESS_PSLICE_H_
/*!
**************************************************************************
* \file ih264d_process_pslice.h
*
* \brief
*    Contains declarations of routines that decode a P slice type
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
WORD32 ih264d_parse_pslice(dec_struct_t *ps_dec,
                            UWORD16 u2_first_mb_in_slice);
WORD32 ih264d_parse_pred_weight_table(dec_slice_params_t * ps_cur_slice,
                                    dec_bit_stream_t * ps_bitstrm);

WORD32 parsePSliceData(dec_struct_t * ps_dec,
                       dec_slice_params_t * ps_slice,
                       UWORD16 u2_first_mb_in_slice);

WORD32 ih264d_process_inter_mb(dec_struct_t * ps_dec,
                               dec_mb_info_t * ps_cur_mb_info,
                               UWORD8 u1_mb_num);

void ih264d_init_ref_idx_lx_p(dec_struct_t *ps_dec);

WORD32 ih264d_mv_pred_ref_tfr_nby2_pmb(dec_struct_t * ps_dec,
                                     UWORD8 u1_num_mbs,
                                     UWORD8 u1_num_mbsNby2);

WORD32 ih264d_decode_recon_tfr_nmb(dec_struct_t * ps_dec,
                                 UWORD8 u1_mb_idx,
                                 UWORD8 u1_num_mbs,
                                 UWORD8 u1_num_mbs_next,
                                 UWORD8 u1_tfr_n_mb,
                                 UWORD8 u1_end_of_row);

void ih264d_insert_pic_in_ref_pic_listx(struct pic_buffer_t *ps_ref_pic_buf_lx,
                                        struct pic_buffer_t *ps_pic);
#endif /* _IH264D_PROCESS_PSLICE_H_ */
