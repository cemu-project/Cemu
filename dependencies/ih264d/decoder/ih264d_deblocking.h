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
#ifndef  _IH264D_DEBLOCKING_H_
#define  _IH264D_DEBLOCKING_H_
/*!
 **************************************************************************
 * \file ih264d_deblocking.h
 *
 * \brief
 *    Declarations of deblocking functions
 *
 * \date
 *    23/11/2002
 *
 * \author  AI
 **************************************************************************
 */
#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264d_structs.h"

WORD8 ih264d_set_deblocking_parameters(deblk_mb_t * ps_cur_deblk_mb,
                                       dec_slice_params_t * ps_slice,
                                       UWORD8 u1_mb_ngbr_availablity,
                                       UWORD8 u1_mb_field_decoding_flag);

void ih264d_copy_intra_pred_line(dec_struct_t *ps_dec,
                                 dec_mb_info_t *ps_cur_mb_info,
                                 UWORD32 nmb_index);

void FilterBoundaryLeft(tfr_ctxt_t * const ps_tfr_cxt,
                        const WORD8 i1_cb_qp_idx_ofst,
                        const WORD8 i1_cr_qp_idx_ofst,
                        deblk_mb_t * const ps_cur_mb,
                        UWORD16 u2_strd_y,
                        UWORD16 u2_strd_uv,
                        deblk_mb_t * const ps_left_mb,
                        const UWORD32 pu4_bs_tab[],
                        const UWORD8 u1_cur_fld);
void FilterBoundaryTop(tfr_ctxt_t * const ps_tfr_cxt,
                       const WORD8 i1_cb_qp_idx_ofst,
                       const WORD8 i1_cr_qp_idx_ofst,
                       deblk_mb_t * const ps_cur_mb,
                       const UWORD16 u2_strd_y,
                       const UWORD16 u2_strd_uv,
                       deblk_mb_t * const ps_top_mb,
                       const UWORD32 u4_bs);
void deblock_mb(tfr_ctxt_t * const ps_tfr_cxt,
                const WORD8 i1_cb_qp_idx_ofst,
                const WORD8 i1_cr_qp_idx_ofst,
                deblk_mb_t * const ps_cur_mb,
                WORD32 i4_strd_y,
                WORD32 i4_strd_uv,
                deblk_mb_t * const ps_top_mb,
                deblk_mb_t * const ps_left_mb,
                const UWORD8 u1_cur_fld,
                const UWORD8 u1_extra_top_edge);
void ih264d_deblock_mb_mbaff(dec_struct_t *ps_dec,
                             tfr_ctxt_t * const ps_tfr_cxt,
                             const WORD8 i1_cb_qp_idx_ofst,
                             const WORD8 i1_cr_qp_idx_ofst,
                             deblk_mb_t * const ps_cur_mb,
                             WORD32 i4_strd_y,
                             WORD32 i4_strd_uv,
                             deblk_mb_t * const ps_top_mb,
                             deblk_mb_t * const ps_left_mb,
                             const UWORD8 u1_cur_fld,
                             const UWORD8 u1_extra_top_edge);

void ih264d_deblock_picture_mbaff(dec_struct_t * const ps_dec);

void ih264d_deblock_picture_non_mbaff(dec_struct_t * const ps_dec);

void ih264d_deblock_picture_progressive(dec_struct_t * const ps_dec);

void ih264d_compute_bs_mbaff(dec_struct_t * ps_dec,
                             dec_mb_info_t * ps_cur_mb_info,
                             const UWORD16 u2_mbxn_mb);
void ih264d_compute_bs_non_mbaff(dec_struct_t * ps_dec,
                                 dec_mb_info_t * ps_cur_mb_info,
                                 const UWORD16 u2_mbxn_mb);

void ih264d_fill_bs_mbedge_2(dec_struct_t * ps_dec,
                             dec_mb_info_t * ps_cur_mb_info,
                             const UWORD16 u2_mbxn_mb);

void ih264d_fill_bs_mbedge_4(dec_struct_t * ps_dec,
                             dec_mb_info_t * ps_cur_mb_info,
                             const UWORD16 u2_mbxn_mb);

void ih264d_fill_bs1_16x16mb_pslice(mv_pred_t *ps_cur_mv_pred,
                                    mv_pred_t *ps_top_mv_pred,
                                    void **ppv_map_ref_idx_to_poc,
                                    UWORD32 *pu4_bs_table,
                                    mv_pred_t *ps_leftmost_mv_pred,
                                    neighbouradd_t *ps_left_addr,
                                    void **u4_pic_addrress,
                                    WORD32 i4_ver_mvlimit);

void ih264d_fill_bs1_non16x16mb_pslice(mv_pred_t *ps_cur_mv_pred,
                                       mv_pred_t *ps_top_mv_pred,
                                       void **ppv_map_ref_idx_to_poc,
                                       UWORD32 *pu4_bs_table,
                                       mv_pred_t *ps_leftmost_mv_pred,
                                       neighbouradd_t *ps_left_addr,
                                       void **u4_pic_addrress,
                                       WORD32 i4_ver_mvlimit);

void ih264d_fill_bs1_16x16mb_bslice(mv_pred_t *ps_cur_mv_pred,
                                    mv_pred_t *ps_top_mv_pred,
                                    void **ppv_map_ref_idx_to_poc,
                                    UWORD32 *pu4_bs_table,
                                    mv_pred_t *ps_leftmost_mv_pred,
                                    neighbouradd_t *ps_left_addr,
                                    void **u4_pic_addrress,
                                    WORD32 i4_ver_mvlimit);

void ih264d_fill_bs1_non16x16mb_bslice(mv_pred_t *ps_cur_mv_pred,
                                       mv_pred_t *ps_top_mv_pred,
                                       void **ppv_map_ref_idx_to_poc,
                                       UWORD32 *pu4_bs_table,
                                       mv_pred_t *ps_leftmost_mv_pred,
                                       neighbouradd_t *ps_left_addr,
                                       void **u4_pic_addrress,
                                       WORD32 i4_ver_mvlimit);

void ih264d_fill_bs_xtra_left_edge_cur_fld(UWORD32 *pu4_bs,
                                           WORD32 u4_left_mb_t_csbp,
                                           WORD32 u4_left_mb_b_csbp,
                                           WORD32 u4_cur_mb_csbp,
                                           UWORD32 u4_cur_mb_top);

void ih264d_fill_bs_xtra_left_edge_cur_frm(UWORD32 *pu4_bs,
                                           WORD32 u4_left_mb_t_csbp,
                                           WORD32 u4_left_mb_b_csbp,
                                           WORD32 u4_cur_mb_csbp,
                                           UWORD32 u4_cur_mb_top);

void ih264d_deblock_mb_nonmbaff(dec_struct_t *ps_dec,
                                tfr_ctxt_t * const ps_tfr_cxt,
                                const WORD8 i1_cb_qp_idx_ofst,
                                const WORD8 i1_cr_qp_idx_ofst,
                                WORD32 i4_strd_y,
                                WORD32 i4_strd_uv);

void ih264d_init_deblk_tfr_ctxt(dec_struct_t * ps_dec,
                                pad_mgr_t *ps_pad_mgr,
                                tfr_ctxt_t *ps_tfr_cxt,
                                UWORD16 u2_image_wd_mb,
                                UWORD8 u1_mbaff);

void ih264d_deblock_mb_level(dec_struct_t *ps_dec,
                             dec_mb_info_t *ps_cur_mb_info,
                             UWORD32 nmb_index);

#endif /* _IH264D_DEBLOCKING_H_ */
