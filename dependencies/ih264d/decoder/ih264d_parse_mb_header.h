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
/*!
 ***************************************************************************
 * \file ih264d_parse_mb_header.h
 *
 * \brief
 *    This file contains context identifier decoding routines.
 *
 * \date
 *    04/02/2003
 *
 * \author  NS
 ***************************************************************************
 */
#ifndef _IH264D_PARSE_MB_HEADER_H_
#define _IH264D_PARSE_MB_HEADER_H_

#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264d_structs.h"
#include "ih264d_cabac.h"

WORD32 ih264d_read_intra_pred_modes_cabac(dec_struct_t * ps_dec,
                                          UWORD8 * pu1_prev_intra4x4_pred_mode_flag,
                                          UWORD8 * pu1_rem_intra4x4_pred_mode,
                                          UWORD8 u1_tran_form8x8);

UWORD32 ih264d_parse_mb_type_cabac(struct _DecStruct * ps_dec);
UWORD8 ih264d_parse_mb_type_intra_cabac(UWORD8 u1_inter,
                                        struct _DecStruct * ps_dec);

UWORD32 ih264d_parse_submb_type_cabac(const UWORD8 u1_slc_type_p,
                                      decoding_envirnoment_t * ps_cab_env,
                                      dec_bit_stream_t * ps_bitstrm,
                                      bin_ctxt_model_t * ps_sub_mb_cxt);
WORD32 ih264d_parse_ref_idx_cabac(const UWORD8 u1_num_part,
                                const UWORD8 u1_b2,
                                const UWORD8 u1_max_ref_minus1,
                                const UWORD8 u1_mb_mode,
                                WORD8 * pi1_ref_idx,
                                WORD8 * const pi1_lft_cxt,
                                WORD8 * const pi1_top_cxt,
                                decoding_envirnoment_t * const ps_cab_env,
                                dec_bit_stream_t * const ps_bitstrm,
                                bin_ctxt_model_t * const ps_ref_cxt);

WORD32 ih264d_parse_mb_qp_delta_cabac(struct _DecStruct * ps_dec,
                                      WORD8 *pi1_mb_qp_delta);
WORD8 ih264d_parse_chroma_pred_mode_cabac(struct _DecStruct * ps_dec);

UWORD32 ih264d_parse_ctx_cbp_cabac(struct _DecStruct * ps_dec);

UWORD8 ih264d_parse_transform8x8flag_cabac(struct _DecStruct * ps_dec,
                                           dec_mb_info_t * ps_cur_mb_info);

void ih264d_get_mvd_cabac(UWORD8 u1_sub_mb,
                          UWORD8 u1_b2,
                          UWORD8 u1_part_wd,
                          UWORD8 u1_part_ht,
                          UWORD8 u1_dec_mvd,
                          dec_struct_t *ps_dec,
                          mv_pred_t *ps_mv);

WORD16 ih264d_parse_mvd_cabac(dec_bit_stream_t * ps_bitstrm,
                              decoding_envirnoment_t * ps_cab_env,
                              bin_ctxt_model_t * p_ctxt_mvd,
                              UWORD32 temp);

#endif /* _IH264D_PARSE_MB_HEADER_H_ */
