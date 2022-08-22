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
void ih264d_init_function_ptr_sse42(dec_struct_t *ps_codec)
{
    ps_codec->pf_default_weighted_pred_luma = ih264_default_weighted_pred_luma_sse42;
    ps_codec->pf_default_weighted_pred_chroma = ih264_default_weighted_pred_chroma_sse42;
    ps_codec->pf_weighted_pred_luma = ih264_weighted_pred_luma_sse42;
    ps_codec->pf_weighted_pred_chroma = ih264_weighted_pred_chroma_sse42;
    ps_codec->pf_weighted_bi_pred_luma = ih264_weighted_bi_pred_luma_sse42;
    ps_codec->pf_weighted_bi_pred_chroma = ih264_weighted_bi_pred_chroma_sse42;

    ps_codec->pf_iquant_itrans_recon_luma_4x4 = ih264_iquant_itrans_recon_4x4_sse42;
    ps_codec->pf_iquant_itrans_recon_chroma_4x4 = ih264_iquant_itrans_recon_chroma_4x4_sse42;
    ps_codec->pf_ihadamard_scaling_4x4 = ih264_ihadamard_scaling_4x4_sse42;
    return;
}
