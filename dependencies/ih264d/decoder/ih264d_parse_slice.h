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
#ifndef _IH264D_PARSE_SLICE_H_
#define _IH264D_PARSE_SLICE_H_
/*!
 **************************************************************************
 * \file ih264d_parse_slice.h
 *
 * \brief
 *    Contains routines that decodes a slice NAL unit
 *
 * \date
 *    19/12/2002
 *
 * \author  AI
 **************************************************************************
 */
#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264d_structs.h"
#include "ih264d_error_handler.h"
WORD32 ih264d_fix_error_in_dpb(dec_struct_t * ps_dec);
WORD32 ih264d_parse_decode_slice(UWORD8 u1_is_idr_slice,
                                 UWORD8 u1_nal_ref_idc,
                                 dec_struct_t * ps_dec );

WORD32 ih264d_end_of_pic(dec_struct_t *ps_dec);
WORD32 ih264d_start_of_pic(dec_struct_t *ps_dec,
                           WORD32 i4_poc,
                           pocstruct_t *ps_temp_poc,
                           UWORD16 u2_frame_num,
                           dec_pic_params_t *ps_pps);

WORD32 ih264d_ref_idx_reordering(dec_struct_t * ps_dec, UWORD8 u1_isB);
WORD32 ih264d_read_mmco_commands(dec_struct_t * ps_dec);
void ih264d_form_pred_weight_matrix(dec_struct_t *ps_dec);
WORD32 ih264d_end_of_pic_dispbuf_mgr(dec_struct_t * ps_dec);
#endif /* _IH264D_PARSE_SLICE_H_ */
