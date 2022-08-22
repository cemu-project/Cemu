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

/*****************************************************************************/
/*                                                                           */
/*  File Name         : ih264d_vui.h                                                */
/*                                                                           */
/*  Description       : This file contains routines to parse SEI NAL's       */
/*                                                                           */
/*  List of Functions : <List the functions defined in this file>            */
/*                                                                           */
/*  Issues / Problems : None                                                 */
/*                                                                           */
/*  Revision History  :                                                      */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         25 05 2005   NS              Draft                                */
/*                                                                           */
/*****************************************************************************/

#ifndef _IH264D_VUI_H_
#define _IH264D_VUI_H_

#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264d_bitstrm.h"

#define VUI_EXTENDED_SAR    255

typedef struct
{
    UWORD32 u4_cpb_cnt;
    UWORD8 u1_bit_rate_scale;
    UWORD8 u1_cpb_size_scale;
    UWORD32 u4_bit_rate[32];
    UWORD32 u4_cpb_size[32];
    UWORD8 u1_cbr_flag[32];
    UWORD8 u1_initial_cpb_removal_delay;
    UWORD8 u1_cpb_removal_delay_length;
    UWORD8 u1_dpb_output_delay_length;
    UWORD8 u1_time_offset_length;
} hrd_t;

typedef struct
{
    UWORD8 u1_aspect_ratio_idc;
    UWORD16 u2_sar_width;
    UWORD16 u2_sar_height;
    UWORD8 u1_overscan_appropriate_flag;
    UWORD8 u1_video_format;
    UWORD8 u1_video_full_range_flag;
    UWORD8 u1_colour_primaries;
    UWORD8 u1_tfr_chars;
    UWORD8 u1_matrix_coeffs;
    UWORD8 u1_cr_top_field;
    UWORD8 u1_cr_bottom_field;
    UWORD32 u4_num_units_in_tick;
    UWORD32 u4_time_scale;
    UWORD8 u1_fixed_frame_rate_flag;
    UWORD8 u1_nal_hrd_params_present;
    hrd_t s_nal_hrd;
    UWORD8 u1_vcl_hrd_params_present;
    hrd_t s_vcl_hrd;
    UWORD8 u1_low_delay_hrd_flag;
    UWORD8 u1_pic_struct_present_flag;
    UWORD8 u1_bitstream_restriction_flag;
    UWORD8 u1_mv_over_pic_boundaries_flag;
    UWORD32 u4_max_bytes_per_pic_denom;
    UWORD32 u4_max_bits_per_mb_denom;
    UWORD32 u4_log2_max_mv_length_horz;
    UWORD32 u4_log2_max_mv_length_vert;
    UWORD32 u4_num_reorder_frames;
    UWORD32 u4_max_dec_frame_buffering;
} vui_t;

WORD32 ih264d_parse_vui_parametres(vui_t *ps_vu4,
                                   dec_bit_stream_t *ps_bitstrm);
#endif /* _SEI_H_ */

