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
*  ih264e_function_selector_generic.c
*
* @brief
*  Contains functions to initialize function pointers of codec context
*
* @author
*  Ittiam
*
* @par List of Functions:
*  - ih264e_init_function_ptr_generic
*
* @remarks
*  None
*
*******************************************************************************
*/


/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

/* System Include files */
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* User Include files */
#include "ih264_typedefs.h"
#include "iv.h"
#include "ivd.h"
#include "ih264_defs.h"
#include "ih264_size_defs.h"
#include "ih264_error.h"
#include "ih264_trans_quant_itrans_iquant.h"
#include "ih264_inter_pred_filters.h"

#include "ih264d_structs.h"


/**
*******************************************************************************
*
* @brief Initialize the intra/inter/transform/deblk function pointers of
* codec context
*
* @par Description: the current routine initializes the function pointers of
* codec context basing on the architecture in use
*
* @param[in] ps_codec
*  Codec context pointer
*
* @returns  none
*
* @remarks none
*
*******************************************************************************
*/
void ih264d_init_function_ptr_ssse3(dec_struct_t *ps_codec)
{



    /* Init function pointers for intra pred leaf level functions luma
     * Intra 16x16 */
    ps_codec->apf_intra_pred_luma_16x16[0] = ih264_intra_pred_luma_16x16_mode_vert_ssse3;
    ps_codec->apf_intra_pred_luma_16x16[1] = ih264_intra_pred_luma_16x16_mode_horz_ssse3;
    ps_codec->apf_intra_pred_luma_16x16[2] = ih264_intra_pred_luma_16x16_mode_dc_ssse3;
    ps_codec->apf_intra_pred_luma_16x16[3] = ih264_intra_pred_luma_16x16_mode_plane_ssse3;

    /* Init function pointers for intra pred leaf level functions luma
     * Intra 4x4 */
    ps_codec->apf_intra_pred_luma_4x4[0] = ih264_intra_pred_luma_4x4_mode_vert_ssse3;
    ps_codec->apf_intra_pred_luma_4x4[1] = ih264_intra_pred_luma_4x4_mode_horz_ssse3;
    ps_codec->apf_intra_pred_luma_4x4[2] = ih264_intra_pred_luma_4x4_mode_dc_ssse3;
    ps_codec->apf_intra_pred_luma_4x4[3] = ih264_intra_pred_luma_4x4_mode_diag_dl_ssse3;
    ps_codec->apf_intra_pred_luma_4x4[4] = ih264_intra_pred_luma_4x4_mode_diag_dr_ssse3;
    ps_codec->apf_intra_pred_luma_4x4[5] = ih264_intra_pred_luma_4x4_mode_vert_r_ssse3;
    ps_codec->apf_intra_pred_luma_4x4[6] = ih264_intra_pred_luma_4x4_mode_horz_d_ssse3;
    ps_codec->apf_intra_pred_luma_4x4[7] = ih264_intra_pred_luma_4x4_mode_vert_l_ssse3;
    ps_codec->apf_intra_pred_luma_4x4[8] = ih264_intra_pred_luma_4x4_mode_horz_u_ssse3;

    /* Init function pointers for intra pred leaf level functions luma
     * Intra 8x8 */
    ps_codec->apf_intra_pred_luma_8x8[0] = ih264_intra_pred_luma_8x8_mode_vert_ssse3;
    ps_codec->apf_intra_pred_luma_8x8[1] = ih264_intra_pred_luma_8x8_mode_horz_ssse3;
    ps_codec->apf_intra_pred_luma_8x8[2] = ih264_intra_pred_luma_8x8_mode_dc_ssse3;
    ps_codec->apf_intra_pred_luma_8x8[3] = ih264_intra_pred_luma_8x8_mode_diag_dl_ssse3;
    ps_codec->apf_intra_pred_luma_8x8[4] = ih264_intra_pred_luma_8x8_mode_diag_dr_ssse3;
    ps_codec->apf_intra_pred_luma_8x8[5] = ih264_intra_pred_luma_8x8_mode_vert_r_ssse3;
    ps_codec->apf_intra_pred_luma_8x8[6] = ih264_intra_pred_luma_8x8_mode_horz_d_ssse3;
    ps_codec->apf_intra_pred_luma_8x8[7] = ih264_intra_pred_luma_8x8_mode_vert_l_ssse3;
    ps_codec->apf_intra_pred_luma_8x8[8] = ih264_intra_pred_luma_8x8_mode_horz_u_ssse3;

    ps_codec->pf_intra_pred_ref_filtering = ih264_intra_pred_luma_8x8_mode_ref_filtering;

    /* Init function pointers for intra pred leaf level functions chroma
     * Intra 8x8 */
    ps_codec->apf_intra_pred_chroma[0] = ih264_intra_pred_chroma_8x8_mode_vert_ssse3;
    ps_codec->apf_intra_pred_chroma[1] = ih264_intra_pred_chroma_8x8_mode_horz_ssse3;
    ps_codec->apf_intra_pred_chroma[2] = ih264_intra_pred_chroma_8x8_mode_dc;
    ps_codec->apf_intra_pred_chroma[3] = ih264_intra_pred_chroma_8x8_mode_plane_ssse3;


    ps_codec->pf_pad_left_luma = ih264_pad_left_luma_ssse3;
    ps_codec->pf_pad_left_chroma = ih264_pad_left_chroma_ssse3;
    ps_codec->pf_pad_right_luma = ih264_pad_right_luma_ssse3;
    ps_codec->pf_pad_right_chroma = ih264_pad_right_chroma_ssse3;


    ps_codec->pf_iquant_itrans_recon_luma_4x4 = ih264_iquant_itrans_recon_4x4_ssse3;
    ps_codec->pf_iquant_itrans_recon_luma_4x4_dc = ih264_iquant_itrans_recon_4x4_dc_ssse3;
    ps_codec->pf_iquant_itrans_recon_luma_8x8 = ih264_iquant_itrans_recon_8x8_ssse3;
    ps_codec->pf_iquant_itrans_recon_luma_8x8_dc = ih264_iquant_itrans_recon_8x8_dc_ssse3;

    ps_codec->pf_iquant_itrans_recon_chroma_4x4_dc = ih264_iquant_itrans_recon_chroma_4x4_dc_ssse3;

    /* Init fn ptr luma deblocking */
    ps_codec->pf_deblk_luma_vert_bs4 = ih264_deblk_luma_vert_bs4_ssse3;
    ps_codec->pf_deblk_luma_vert_bslt4 = ih264_deblk_luma_vert_bslt4_ssse3;
    ps_codec->pf_deblk_luma_vert_bs4_mbaff = ih264_deblk_luma_vert_bs4_mbaff_ssse3;
    ps_codec->pf_deblk_luma_vert_bslt4_mbaff = ih264_deblk_luma_vert_bslt4_mbaff_ssse3;

    ps_codec->pf_deblk_luma_horz_bs4 = ih264_deblk_luma_horz_bs4_ssse3;
    ps_codec->pf_deblk_luma_horz_bslt4 = ih264_deblk_luma_horz_bslt4_ssse3;

    /* Init fn ptr chroma deblocking */
    ps_codec->pf_deblk_chroma_vert_bs4 = ih264_deblk_chroma_vert_bs4_ssse3;
    ps_codec->pf_deblk_chroma_horz_bs4 = ih264_deblk_chroma_horz_bs4_ssse3;
    ps_codec->pf_deblk_chroma_vert_bs4_mbaff = ih264_deblk_chroma_vert_bs4_mbaff_ssse3;
    ps_codec->pf_deblk_chroma_vert_bslt4 = ih264_deblk_chroma_vert_bslt4_ssse3;
    ps_codec->pf_deblk_chroma_horz_bslt4 = ih264_deblk_chroma_horz_bslt4_ssse3;
    ps_codec->pf_deblk_chroma_vert_bslt4_mbaff = ih264_deblk_chroma_vert_bslt4_mbaff_ssse3;

    /* Inter pred leaf level functions */

    ps_codec->apf_inter_pred_luma[0] = ih264_inter_pred_luma_copy_ssse3;
    ps_codec->apf_inter_pred_luma[1] = ih264_inter_pred_luma_horz_qpel_ssse3;
    ps_codec->apf_inter_pred_luma[2] = ih264_inter_pred_luma_horz_ssse3;
    ps_codec->apf_inter_pred_luma[3] = ih264_inter_pred_luma_horz_qpel_ssse3;
    ps_codec->apf_inter_pred_luma[4] = ih264_inter_pred_luma_vert_qpel_ssse3;
    ps_codec->apf_inter_pred_luma[5] = ih264_inter_pred_luma_horz_qpel_vert_qpel_ssse3;
    ps_codec->apf_inter_pred_luma[6] = ih264_inter_pred_luma_horz_hpel_vert_qpel_ssse3;
    ps_codec->apf_inter_pred_luma[7] = ih264_inter_pred_luma_horz_qpel_vert_qpel_ssse3;
    ps_codec->apf_inter_pred_luma[8] = ih264_inter_pred_luma_vert_ssse3;
    ps_codec->apf_inter_pred_luma[9] = ih264_inter_pred_luma_horz_qpel_vert_hpel_ssse3;
    ps_codec->apf_inter_pred_luma[10] = ih264_inter_pred_luma_horz_hpel_vert_hpel_ssse3;
    ps_codec->apf_inter_pred_luma[11] = ih264_inter_pred_luma_horz_qpel_vert_hpel_ssse3;
    ps_codec->apf_inter_pred_luma[12] = ih264_inter_pred_luma_vert_qpel_ssse3;
    ps_codec->apf_inter_pred_luma[13] = ih264_inter_pred_luma_horz_qpel_vert_qpel_ssse3;
    ps_codec->apf_inter_pred_luma[14] = ih264_inter_pred_luma_horz_hpel_vert_qpel_ssse3;
    ps_codec->apf_inter_pred_luma[15] = ih264_inter_pred_luma_horz_qpel_vert_qpel_ssse3;

    ps_codec->pf_inter_pred_chroma = ih264_inter_pred_chroma_ssse3;


    return;
}
