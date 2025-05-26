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
 *  ih264_trans_quant.h
 *
 * @brief
 *  Contains declarations for forward and inverse transform paths for H264
 *
 * @author
 *  Ittiam
 *
 * @remarks
 *
 *******************************************************************************
 */

#ifndef IH264_TRANS_QUANT_H_
#define IH264_TRANS_QUANT_H_

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/


typedef void ih264_resi_trans_dctrans_quant_ft(UWORD8*pu1_src,
                                       UWORD8 *pu1_pred,
                                       WORD16 *pi2_out,
                                       WORD32 src_strd,
                                       WORD32 pred_strd,
                                       WORD32 dst_strd,
                                       const UWORD16 *pu2_scale_mat,
                                       const UWORD16 *pu2_thresh_mat,
                                       UWORD32 u4_qbit,
                                       UWORD32 u4_round_fact,
                                       UWORD8 *pu1_nnz);

typedef void ih264_idctrans_iquant_itrans_recon_ft(WORD16 *pi2_src,
                                          UWORD8 *pu1_pred,
                                          UWORD8 *pu1_out,
                                          WORD32 src_strd,
                                          WORD32 pred_strd,
                                          WORD32 out_strd,
                                          const UWORD16 *pu2_iscale_mat,
                                          const UWORD16 *pu2_weigh_mat,
                                          UWORD32 qp_div,
                                          UWORD32 pi4_cntrl,
                                          WORD32 *pi4_tmp);


/*Function prototype declarations*/
typedef void ih264_resi_trans_quant_ft(UWORD8*pu1_src,
                                       UWORD8 *pu1_pred,
                                       WORD16 *pi2_out,
                                       WORD32 src_strd,
                                       WORD32 pred_strd,
                                       const UWORD16 *pu2_scale_mat,
                                       const UWORD16 *pu2_thresh_mat,
                                       UWORD32 u4_qbit,
                                       UWORD32 u4_round_fact,
                                       UWORD8 *pu1_nnz,
                                       WORD16 *pi2_alt_dc_addr);

typedef void ih264_luma_16x16_resi_trans_dctrans_quant_ft(UWORD8 *pu1_src,
                                                          UWORD8 *pu1_pred,
                                                          WORD16 *pi2_out,
                                                          WORD32 src_strd,
                                                          WORD32 pred_strd,
                                                          WORD32 dst_strd,
                                                          const UWORD16 *pu2_scale_matrix,
                                                          const UWORD16 *pu2_threshold_matrix,
                                                          UWORD32 u4_qbits,
                                                          UWORD32 u4_round_factor,
                                                          UWORD8 *pu1_nnz,
                                                          UWORD32 u4_dc_flag);

typedef void ih264_chroma_8x8_resi_trans_dctrans_quant_ft(UWORD8 *pu1_src,
                                                          UWORD8 *pu1_pred,
                                                          WORD16 *pi2_out,
                                                          WORD32 src_strd,
                                                          WORD32 pred_strd,
                                                          WORD32 dst_strd,
                                                          const UWORD16 *pu2_scale_matrix,
                                                          const UWORD16 *pu2_threshold_matrix,
                                                          UWORD32 u4_qbits,
                                                          UWORD32 u4_round_factor,
                                                          UWORD8 *pu1_nnz);

typedef void ih264_iquant_itrans_recon_ft(WORD16 *pi2_src,
                                          UWORD8 *pu1_pred,
                                          UWORD8 *pu1_out,
                                          WORD32 pred_strd,
                                          WORD32 out_strd,
                                          const UWORD16 *pu2_iscale_mat,
                                          const UWORD16 *pu2_weigh_mat,
                                          UWORD32 qp_div,
                                          WORD16 *pi2_tmp,
                                          WORD32 iq_start_idx,
                                          WORD16 *pi2_dc_ld_addr);


typedef void ih264_iquant_itrans_recon_chroma_ft(WORD16 *pi2_src,
                                                 UWORD8 *pu1_pred,
                                                 UWORD8 *pu1_out,
                                                 WORD32 pred_strd,
                                                 WORD32 out_strd,
                                                 const UWORD16 *pu2_iscal_mat,
                                                 const UWORD16 *pu2_weigh_mat,
                                                 UWORD32 u4_qp_div_6,
                                                 WORD16 *pi2_tmp,
                                                 WORD16 *pi2_dc_src);


typedef void ih264_luma_16x16_idctrans_iquant_itrans_recon_ft(WORD16 *pi2_src,
                                                              UWORD8 *pu1_pred,
                                                              UWORD8 *pu1_out,
                                                              WORD32 src_strd,
                                                              WORD32 pred_strd,
                                                              WORD32 out_strd,
                                                              const UWORD16 *pu2_iscale_mat,
                                                              const UWORD16 *pu2_weigh_mat,
                                                              UWORD32 qp_div,
                                                              UWORD32 pi4_cntrl,
                                                              UWORD32 u4_dc_trans_flag,
                                                              WORD32 *pi4_tmp);

typedef void ih264_chroma_8x8_idctrans_iquant_itrans_recon_ft(WORD16 *pi2_src,
                                                              UWORD8 *pu1_pred,
                                                              UWORD8 *pu1_out,
                                                              WORD32 src_strd,
                                                              WORD32 pred_strd,
                                                              WORD32 out_strd,
                                                              const UWORD16 *pu2_iscale_mat,
                                                              const UWORD16 *pu2_weigh_mat,
                                                              UWORD32 qp_div,
                                                              UWORD32 pi4_cntrl,
                                                              WORD32 *pi4_tmp);

typedef void ih264_ihadamard_scaling_ft(WORD16* pi2_src,
                                        WORD16* pi2_out,
                                        const UWORD16 *pu2_iscal_mat,
                                        const UWORD16 *pu2_weigh_mat,
                                        UWORD32 u4_qp_div_6,
                                        WORD32* pi4_tmp);

typedef void ih264_hadamard_quant_ft(WORD16 *pi2_src, WORD16 *pi2_dst,
                                    const UWORD16 *pu2_scale_matrix,
                                    const UWORD16 *pu2_threshold_matrix, UWORD32 u4_qbits,
                                    UWORD32 u4_round_factor,UWORD8  *pu1_nnz);

ih264_resi_trans_quant_ft ih264_resi_trans_quant_4x4;
ih264_resi_trans_quant_ft ih264_resi_trans_quant_chroma_4x4;
ih264_resi_trans_quant_ft ih264_resi_trans_quant_8x8;
ih264_iquant_itrans_recon_ft ih264_iquant_itrans_recon_4x4;
ih264_iquant_itrans_recon_ft ih264_iquant_itrans_recon_8x8;
ih264_iquant_itrans_recon_ft ih264_iquant_itrans_recon_4x4_dc;
ih264_iquant_itrans_recon_ft ih264_iquant_itrans_recon_8x8_dc;
ih264_iquant_itrans_recon_chroma_ft ih264_iquant_itrans_recon_chroma_4x4;
ih264_iquant_itrans_recon_chroma_ft ih264_iquant_itrans_recon_chroma_4x4_dc;
ih264_ihadamard_scaling_ft ih264_ihadamard_scaling_4x4;
ih264_ihadamard_scaling_ft ih264_ihadamard_scaling_2x2_uv;
ih264_hadamard_quant_ft ih264_hadamard_quant_4x4;
ih264_hadamard_quant_ft ih264_hadamard_quant_2x2_uv;

/*A9 Declarations*/
ih264_resi_trans_quant_ft ih264_resi_trans_quant_4x4_a9;
ih264_resi_trans_quant_ft ih264_resi_trans_quant_chroma_4x4_a9;
ih264_iquant_itrans_recon_ft ih264_iquant_itrans_recon_4x4_a9;
ih264_iquant_itrans_recon_ft ih264_iquant_itrans_recon_8x8_a9;
ih264_iquant_itrans_recon_ft ih264_iquant_itrans_recon_4x4_dc_a9;
ih264_iquant_itrans_recon_ft ih264_iquant_itrans_recon_8x8_dc_a9;
ih264_iquant_itrans_recon_chroma_ft ih264_iquant_itrans_recon_chroma_4x4_a9;
ih264_iquant_itrans_recon_chroma_ft ih264_iquant_itrans_recon_chroma_4x4_dc_a9;
ih264_luma_16x16_resi_trans_dctrans_quant_ft ih264_luma_16x16_resi_trans_dctrans_quant_a9;
ih264_chroma_8x8_resi_trans_dctrans_quant_ft ih264_chroma_8x8_resi_trans_dctrans_quant_a9;
ih264_luma_16x16_idctrans_iquant_itrans_recon_ft ih264_luma_16x16_idctrans_iquant_itrans_recon_a9;
ih264_chroma_8x8_idctrans_iquant_itrans_recon_ft ih264_chroma_8x8_idctrans_iquant_itrans_recon_a9;
ih264_ihadamard_scaling_ft ih264_ihadamard_scaling_4x4_a9;
ih264_ihadamard_scaling_ft ih264_ihadamard_scaling_2x2_uv_a9;
ih264_hadamard_quant_ft ih264_hadamard_quant_4x4_a9;
ih264_hadamard_quant_ft ih264_hadamard_quant_2x2_uv_a9;

/*Av8 Declarations*/
ih264_resi_trans_quant_ft ih264_resi_trans_quant_4x4_av8;
ih264_resi_trans_quant_ft ih264_resi_trans_quant_chroma_4x4_av8;
ih264_iquant_itrans_recon_ft ih264_iquant_itrans_recon_4x4_av8;
ih264_iquant_itrans_recon_ft ih264_iquant_itrans_recon_8x8_av8;
ih264_iquant_itrans_recon_ft ih264_iquant_itrans_recon_4x4_dc_av8;
ih264_iquant_itrans_recon_ft ih264_iquant_itrans_recon_8x8_dc_av8;
ih264_iquant_itrans_recon_chroma_ft ih264_iquant_itrans_recon_chroma_4x4_av8;
ih264_iquant_itrans_recon_chroma_ft ih264_iquant_itrans_recon_chroma_4x4_dc_av8;
ih264_ihadamard_scaling_ft ih264_ihadamard_scaling_4x4_av8;
ih264_ihadamard_scaling_ft ih264_ihadamard_scaling_2x2_uv_av8;
ih264_hadamard_quant_ft ih264_hadamard_quant_4x4_av8;
ih264_hadamard_quant_ft ih264_hadamard_quant_2x2_uv_av8;

/*SSSE3 Declarations*/
ih264_iquant_itrans_recon_ft ih264_iquant_itrans_recon_4x4_ssse3;
ih264_iquant_itrans_recon_ft ih264_iquant_itrans_recon_8x8_ssse3;
ih264_iquant_itrans_recon_ft ih264_iquant_itrans_recon_4x4_dc_ssse3;
ih264_iquant_itrans_recon_ft ih264_iquant_itrans_recon_8x8_dc_ssse3;
ih264_iquant_itrans_recon_chroma_ft ih264_iquant_itrans_recon_chroma_4x4_dc_ssse3;
ih264_ihadamard_scaling_ft ih264_ihadamard_scaling_4x4_ssse3;
ih264_ihadamard_scaling_ft ih264_ihadamard_scaling_2x2_uv_ssse3;
/*SSSE42 Declarations*/
ih264_resi_trans_quant_ft ih264_resi_trans_quant_4x4_sse42;
ih264_resi_trans_quant_ft ih264_resi_trans_quant_chroma_4x4_sse42;
ih264_iquant_itrans_recon_ft ih264_iquant_itrans_recon_4x4_sse42;
ih264_iquant_itrans_recon_chroma_ft ih264_iquant_itrans_recon_chroma_4x4_sse42;
ih264_ihadamard_scaling_ft ih264_ihadamard_scaling_4x4_sse42;
ih264_hadamard_quant_ft ih264_hadamard_quant_4x4_sse42;
ih264_hadamard_quant_ft ih264_hadamard_quant_2x2_uv_sse42;

#endif /* IH264_TRANS_QUANT_H_ */
