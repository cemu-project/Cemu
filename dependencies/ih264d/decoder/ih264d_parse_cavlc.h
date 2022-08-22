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
#ifndef _IH264D_PARSE_CAVLC_H_
#define _IH264D_PARSE_CAVLC_H_
/*!
 **************************************************************************
 * \file ih264d_parse_cavlc.h
 *
 * \brief
 *    Declaration of UVLC and CAVLC functions
 *
 * \date
 *    18/12/2002
 *
 * \author  AI
 **************************************************************************
 */
#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264d_bitstrm.h"
#include "ih264d_structs.h"
#include "ih264d_cabac.h"

enum cavlcTableNum
{
    tableTotalZeroOffset,
    tableTotalZero,
    tableRunBefore,
    codeGx,
    chromTab,
    offsetNumVlcTab
};

WORD32 ih264d_uvlc(dec_bit_stream_t *ps_bitstrm,
                   UWORD32 u4_range,
                   UWORD32 *pi_bitstrm_ofst,
                   UWORD8 u1_flag,
                   UWORD32 u4_bitstrm_ofst,
                   UWORD32 *pi_bitstrm_buf);

UWORD32 ih264d_uev(UWORD32 *pu4_bitstrm_ofst, UWORD32 *pu4_bitstrm_buf);

WORD32 ih264d_sev(UWORD32 *pu4_bitstrm_ofst, UWORD32 *pu4_bitstrm_buf);

UWORD32 ih264d_tev_range1(UWORD32 *pu4_bitstrm_ofst,
                          UWORD32 *pu4_bitstrm_buf);

UWORD8 RestOfResidualBlockCavlc(WORD16 *pi2_coeff_block,
                                UWORD32 u1_ofst_is_dc_max_coef_scale_fact,
                                UWORD32 u4_total_coeff_trail_one,
                                dec_bit_stream_t *ps_bitstrm,
                                UWORD8 *pu1_invscan);

WORD32 ih264d_cavlc_4x4res_block_totalcoeff_1( UWORD32 u4_isdc,
                                           UWORD32 u4_total_coeff_trail_one,
                                           dec_bit_stream_t *ps_bitstrm);

WORD32 ih264d_cavlc_4x4res_block_totalcoeff_2to10(UWORD32 u4_isdc,
                                               UWORD32 u4_total_coeff_trail_one,
                                               dec_bit_stream_t *ps_bitstrm);

WORD32 ih264d_cavlc_4x4res_block_totalcoeff_11to16(UWORD32 u4_isdc,
                                                UWORD32 u4_total_coeff_trail_one,
                                                dec_bit_stream_t *ps_bitstrm);

WORD32 ih264d_cavlc_parse4x4coeff_n0to7(WORD16 *pi2_coeff_block,
                                        UWORD32 u4_isdc,
                                        WORD32 u4_n,
                                        dec_struct_t *ps_dec,
                                        UWORD32 *pu4_total_coeff);

WORD32 ih264d_cavlc_parse4x4coeff_n8(WORD16 *pi2_coeff_block,
                                      UWORD32 u4_isdc,
                                      WORD32 u4_n,
                                      dec_struct_t *ps_dec,
                                      UWORD32 *pu4_total_coeff);

void ih264d_cavlc_parse_chroma_dc(dec_mb_info_t *ps_cur_mb_info,
                                  WORD16 *pi2_coeff_block,
                                  dec_bit_stream_t *ps_bitstrm,
                                  UWORD32 u4_scale_u,
                                  UWORD32 u4_scale_v,
                                  WORD32 i4_mb_inter_inc);

WORD32 ih264d_cavlc_parse_8x8block_none_available(WORD16 *pi2_coeff_block,
                                                  UWORD32 u4_sub_block_strd,
                                                  UWORD32 u4_isdc,
                                                  dec_struct_t * ps_dec,
                                                  UWORD8 *pu1_top_nnz,
                                                  UWORD8 *pu1_left_nnz,
                                                  UWORD8 u1_tran_form8x8,
                                                  UWORD8 u1_mb_field_decodingflag,
                                                  UWORD32 *pu4_csbp);

WORD32 ih264d_cavlc_parse_8x8block_left_available(WORD16 *pi2_coeff_block,
                                                  UWORD32 u4_sub_block_strd,
                                                  UWORD32 u4_isdc,
                                                  dec_struct_t * ps_dec,
                                                  UWORD8 *pu1_top_nnz,
                                                  UWORD8 *pu1_left_nnz,
                                                  UWORD8 u1_tran_form8x8,
                                                  UWORD8 u1_mb_field_decodingflag,
                                                  UWORD32 *pu4_csbp);

WORD32 ih264d_cavlc_parse_8x8block_top_available(WORD16 *pi2_coeff_block,
                                                 UWORD32 u4_sub_block_strd,
                                                 UWORD32 u4_isdc,
                                                 dec_struct_t * ps_dec,
                                                 UWORD8 *pu1_top_nnz,
                                                 UWORD8 *pu1_left_nnz,
                                                 UWORD8 u1_tran_form8x8,
                                                 UWORD8 u1_mb_field_decodingflag,
                                                 UWORD32 *pu4_csbp);

WORD32 ih264d_cavlc_parse_8x8block_both_available(WORD16 *pi2_coeff_block,
                                                  UWORD32 u4_sub_block_strd,
                                                  UWORD32 u4_isdc,
                                                  dec_struct_t * ps_dec,
                                                  UWORD8 *pu1_top_nnz,
                                                  UWORD8 *pu1_left_nnz,
                                                  UWORD8 u1_tran_form8x8,
                                                  UWORD8 u1_mb_field_decodingflag,
                                                  UWORD32 *pu4_csbp);

WORD8 ResidualBlockChromaDC(WORD16 *pi2_level, dec_bit_stream_t *ps_bitstrm);

void ih264d_parse_pmb_ref_index_cavlc_range1(UWORD32 u4_num_part,
                                             dec_bit_stream_t *ps_bitstrm,
                                             WORD8 *pi1_ref_idx,
                                             UWORD32 u4_num_ref_idx_active_minus1);

WORD32 ih264d_parse_pmb_ref_index_cavlc(UWORD32 u4_num_part,
                                      dec_bit_stream_t *ps_bitstrm,
                                      WORD8 *pi1_ref_idx,
                                      UWORD32 u4_num_ref_idx_active_minus1);

void ih264d_parse_bmb_ref_index_cavlc_range1(UWORD32 u4_num_part,
                                             dec_bit_stream_t *ps_bitstrm,
                                             WORD8 *pi1_ref_idx,
                                             UWORD32 u4_num_ref_idx_active_minus1);

WORD32 ih264d_parse_bmb_ref_index_cavlc(UWORD32 u4_num_part,
                                      dec_bit_stream_t *ps_bitstrm,
                                      WORD8 *pi1_ref_idx,
                                      UWORD32 u4_num_ref_idx_active_minus1);

#endif  /* _IH264D_PARSE_CAVLC_H_ */
