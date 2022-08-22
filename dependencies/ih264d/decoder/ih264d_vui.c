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
/*  File Name         : ih264d_vui.c                                                */
/*                                                                           */
/*  Description       : This file contains routines to parse VUI NAL's       */
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

#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264d_vui.h"
#include "ih264d_bitstrm.h"
#include "ih264d_parse_cavlc.h"
#include "ih264d_structs.h"
#include "ih264d_error_handler.h"

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_parse_hrd_parametres                                     */
/*                                                                           */
/*  Description   : This function parses hrd_t parametres                      */
/*  Inputs        : ps_hrd          pointer to HRD params                    */
/*                  ps_bitstrm   Bitstream                                */
/*  Globals       : None                                                     */
/*  Processing    : Parses HRD params                                        */
/*  Outputs       : None                                                     */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         06 05 2002   NS              Draft                                */
/*                                                                           */
/*****************************************************************************/

WORD32 ih264d_parse_hrd_parametres(hrd_t *ps_hrd,
                                   dec_bit_stream_t *ps_bitstrm)
{
    UWORD8 u1_index;
    UWORD32 *pu4_bitstrm_ofst = &ps_bitstrm->u4_ofst;
    UWORD32 *pu4_bitstrm_buf = ps_bitstrm->pu4_buffer;

    ps_hrd->u4_cpb_cnt = 1
                    + ih264d_uev(pu4_bitstrm_ofst, pu4_bitstrm_buf);
    if(ps_hrd->u4_cpb_cnt > 31)
        return ERROR_INV_SPS_PPS_T;
    ps_hrd->u1_bit_rate_scale = ih264d_get_bits_h264(ps_bitstrm, 4);
    ps_hrd->u1_cpb_size_scale = ih264d_get_bits_h264(ps_bitstrm, 4);

    for(u1_index = 0; u1_index < (UWORD8)ps_hrd->u4_cpb_cnt; u1_index++)
    {
        ps_hrd->u4_bit_rate[u1_index] = 1
                        + ih264d_uev(pu4_bitstrm_ofst,
                                     pu4_bitstrm_buf);
        ps_hrd->u4_cpb_size[u1_index] = 1
                        + ih264d_uev(pu4_bitstrm_ofst,
                                     pu4_bitstrm_buf);
        ps_hrd->u1_cbr_flag[u1_index] = ih264d_get_bits_h264(ps_bitstrm, 1);
    }

    ps_hrd->u1_initial_cpb_removal_delay = 1
                    + ih264d_get_bits_h264(ps_bitstrm, 5);
    ps_hrd->u1_cpb_removal_delay_length = 1
                    + ih264d_get_bits_h264(ps_bitstrm, 5);
    ps_hrd->u1_dpb_output_delay_length = 1
                    + ih264d_get_bits_h264(ps_bitstrm, 5);
    ps_hrd->u1_time_offset_length = ih264d_get_bits_h264(ps_bitstrm, 5);

    return OK;
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_parse_vui_parametres                                     */
/*                                                                           */
/*  Description   : This function parses VUI NALs.                           */
/*  Inputs        : ps_vu4          pointer to VUI params                    */
/*                  ps_bitstrm   Bitstream                                */
/*  Globals       : None                                                     */
/*  Processing    : Parses VUI NAL's units and stores the info               */
/*  Outputs       : None                                                     */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         06 05 2002   NS              Draft                                */
/*                                                                           */
/*****************************************************************************/

WORD32 ih264d_parse_vui_parametres(vui_t *ps_vu4,
                                   dec_bit_stream_t *ps_bitstrm)
{
    UWORD8 u4_bits;
    UWORD32 *pu4_bitstrm_ofst = &ps_bitstrm->u4_ofst;
    UWORD32 *pu4_bitstrm_buf = ps_bitstrm->pu4_buffer;
    WORD32 ret;

    u4_bits = ih264d_get_bits_h264(ps_bitstrm, 1);
    if(u4_bits)
    {
        u4_bits = ih264d_get_bits_h264(ps_bitstrm, 8);
        ps_vu4->u1_aspect_ratio_idc = (UWORD8)u4_bits;
        if(VUI_EXTENDED_SAR == u4_bits)
        {
            ps_vu4->u2_sar_width = ih264d_get_bits_h264(ps_bitstrm, 16);
            ps_vu4->u2_sar_height = ih264d_get_bits_h264(ps_bitstrm, 16);
        }
    }

    u4_bits = ih264d_get_bits_h264(ps_bitstrm, 1);
    if(u4_bits)
    {
        ps_vu4->u1_overscan_appropriate_flag = ih264d_get_bits_h264(
                        ps_bitstrm, 1);
    }
    u4_bits = ih264d_get_bits_h264(ps_bitstrm, 1);
    /* Initialize to unspecified (5 for video_format and
       2 for colour_primaries, tfr_chars, matrix_coeffs  */
    ps_vu4->u1_video_format = 5;
    ps_vu4->u1_video_full_range_flag = 0;
    ps_vu4->u1_colour_primaries = 2;
    ps_vu4->u1_tfr_chars = 2;
    ps_vu4->u1_matrix_coeffs = 2;

    if(u4_bits)
    {
        ps_vu4->u1_video_format = ih264d_get_bits_h264(ps_bitstrm, 3);
        ps_vu4->u1_video_full_range_flag = ih264d_get_bits_h264(ps_bitstrm,
                                                                1);
        u4_bits = ih264d_get_bits_h264(ps_bitstrm, 1);
        if(u4_bits)
        {
            ps_vu4->u1_colour_primaries = ih264d_get_bits_h264(ps_bitstrm,
                                                               8);
            ps_vu4->u1_tfr_chars = ih264d_get_bits_h264(ps_bitstrm, 8);
            ps_vu4->u1_matrix_coeffs = ih264d_get_bits_h264(ps_bitstrm, 8);
        }
    }

    u4_bits = ih264d_get_bits_h264(ps_bitstrm, 1);
    if(u4_bits)
    {
        ps_vu4->u1_cr_top_field = ih264d_uev(pu4_bitstrm_ofst,
                                             pu4_bitstrm_buf);
        ps_vu4->u1_cr_bottom_field = ih264d_uev(pu4_bitstrm_ofst,
                                                pu4_bitstrm_buf);
    }

    u4_bits = ih264d_get_bits_h264(ps_bitstrm, 1);
    if(u4_bits)
    {
        ps_vu4->u4_num_units_in_tick = ih264d_get_bits_h264(ps_bitstrm, 32);
        ps_vu4->u4_time_scale = ih264d_get_bits_h264(ps_bitstrm, 32);
        ps_vu4->u1_fixed_frame_rate_flag = ih264d_get_bits_h264(ps_bitstrm,
                                                                1);
    }

    u4_bits = ih264d_get_bits_h264(ps_bitstrm, 1);
    ps_vu4->u1_nal_hrd_params_present = u4_bits;
    if(u4_bits)
    {
        ret = ih264d_parse_hrd_parametres(&ps_vu4->s_nal_hrd, ps_bitstrm);
        if(ret != OK)
            return ret;
    }
    u4_bits = ih264d_get_bits_h264(ps_bitstrm, 1);
    ps_vu4->u1_vcl_hrd_params_present = u4_bits;
    if(u4_bits)
    {
        ret = ih264d_parse_hrd_parametres(&ps_vu4->s_vcl_hrd, ps_bitstrm);
        if(ret != OK)
            return ret;
    }

    if(ps_vu4->u1_nal_hrd_params_present || u4_bits)
    {
        ps_vu4->u1_low_delay_hrd_flag = ih264d_get_bits_h264(ps_bitstrm, 1);
    }
    ps_vu4->u1_pic_struct_present_flag = ih264d_get_bits_h264(ps_bitstrm, 1);

    ps_vu4->u1_bitstream_restriction_flag = ih264d_get_bits_h264(ps_bitstrm, 1);

    if(ps_vu4->u1_bitstream_restriction_flag)
    {
        ps_vu4->u1_mv_over_pic_boundaries_flag = ih264d_get_bits_h264(
                        ps_bitstrm, 1);
        ps_vu4->u4_max_bytes_per_pic_denom = ih264d_uev(pu4_bitstrm_ofst,
                                                        pu4_bitstrm_buf);
        ps_vu4->u4_max_bits_per_mb_denom = ih264d_uev(pu4_bitstrm_ofst,
                                                      pu4_bitstrm_buf);
        ps_vu4->u4_log2_max_mv_length_horz = ih264d_uev(pu4_bitstrm_ofst,
                                                        pu4_bitstrm_buf);
        ps_vu4->u4_log2_max_mv_length_vert = ih264d_uev(pu4_bitstrm_ofst,
                                                        pu4_bitstrm_buf);
        ps_vu4->u4_num_reorder_frames = ih264d_uev(pu4_bitstrm_ofst,
                                                   pu4_bitstrm_buf);
        ps_vu4->u4_max_dec_frame_buffering = ih264d_uev(pu4_bitstrm_ofst,
                                                        pu4_bitstrm_buf);
        if((ps_vu4->u4_max_dec_frame_buffering > (H264_MAX_REF_PICS * 2)) ||
           (ps_vu4->u4_num_reorder_frames > ps_vu4->u4_max_dec_frame_buffering))
        {
            return ERROR_INV_SPS_PPS_T;
        }
    }
    else
    {
        /* Setting this to a large value if not present */
        ps_vu4->u4_num_reorder_frames = 64;
        ps_vu4->u4_max_dec_frame_buffering = 64;
    }

    return OK;
}
