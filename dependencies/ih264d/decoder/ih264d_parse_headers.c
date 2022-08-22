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
 * Modified for use with Cemu emulator project
 */
/*!
 **************************************************************************
 * \file ih264d_parse_headers.c
 *
 * \brief
 *    Contains High level syntax[above slice] parsing routines
 *
 * \date
 *    19/12/2002
 *
 * \author  AI
 **************************************************************************
 */
#include <string.h>

#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264_defs.h"
#include "ih264d_bitstrm.h"
#include "ih264d_structs.h"
#include "ih264d_parse_cavlc.h"
#include "ih264d_defs.h"
#include "ih264d_defs.h"
#include "ih264d_defs.h"
#include "ih264d_parse_slice.h"
#include "ih264d_tables.h"
#include "ih264d_utils.h"
#include "ih264d_nal.h"
#include "ih264d_deblocking.h"

#include "ih264d_mem_request.h"
#include "ih264d_debug.h"
#include "ih264d_error_handler.h"
#include "ih264d_mb_utils.h"
#include "ih264d_sei.h"
#include "ih264d_vui.h"
#include "ih264d_thread_parse_decode.h"
#include "ih264d_thread_compute_bs.h"
#include "ih264d_quant_scaling.h"
#include "ih264d_defs.h"
#include "ivd.h"
#include "ih264d.h"

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_get_pre_sei_params                                */
/*                                                                           */
/*  Description   : Gets valid pre-sei params in decoder struct from parse   */
/*                  struct.                                                  */
/*  Inputs        : u1_nal_unit_type slice type                              */
/*                  ps_dec    Decoder parameters                             */
/*  Globals       : None                                                     */
/*  Outputs       : None                                                     */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*                                          Draft                            */
/*                                                                           */
/*****************************************************************************/

void ih264d_get_pre_sei_params(dec_struct_t *ps_dec, UWORD8 u1_nal_unit_type)
{
    if((NULL != ps_dec->ps_sei) &&
        ((0 == ps_dec->ps_sei->s_sei_ccv_params.u1_ccv_cancel_flag) &&
        (0 == ps_dec->ps_sei->s_sei_ccv_params.u1_ccv_persistence_flag)))
    {
        ps_dec->ps_sei->u1_sei_ccv_params_present_flag = 0;
        memset(&ps_dec->ps_sei->s_sei_ccv_params, 0, sizeof(sei_ccv_params_t));
    }

    if((NULL != ps_dec->ps_cur_sps) &&
        ((1 == ps_dec->ps_cur_sps->u1_vui_parameters_present_flag) &&
        ((2 != ps_dec->ps_cur_sps->s_vui.u1_colour_primaries) &&
        (2 != ps_dec->ps_cur_sps->s_vui.u1_matrix_coeffs) &&
        (2 != ps_dec->ps_cur_sps->s_vui.u1_tfr_chars) &&
        (4 != ps_dec->ps_cur_sps->s_vui.u1_tfr_chars) &&
        (5 != ps_dec->ps_cur_sps->s_vui.u1_tfr_chars))))
    {
        if((1 == ps_dec->ps_sei_parse->u1_sei_ccv_params_present_flag) ||
            (IDR_SLICE_NAL == u1_nal_unit_type))
        {
            ps_dec->ps_sei->u1_sei_ccv_params_present_flag =
                        ps_dec->ps_sei_parse->u1_sei_ccv_params_present_flag;
            ps_dec->ps_sei->s_sei_ccv_params = ps_dec->ps_sei_parse->s_sei_ccv_params;
        }
    }
    else
    {
        ps_dec->ps_sei->u1_sei_ccv_params_present_flag = 0;
        memset(&ps_dec->ps_sei->s_sei_ccv_params, 0, sizeof(sei_ccv_params_t));
    }

    if(IDR_SLICE_NAL == u1_nal_unit_type)
    {
        ps_dec->ps_sei->u1_sei_mdcv_params_present_flag =
                        ps_dec->ps_sei_parse->u1_sei_mdcv_params_present_flag;
        ps_dec->ps_sei->s_sei_mdcv_params = ps_dec->ps_sei_parse->s_sei_mdcv_params;
        ps_dec->ps_sei->u1_sei_cll_params_present_flag =
                        ps_dec->ps_sei_parse->u1_sei_cll_params_present_flag;
        ps_dec->ps_sei->s_sei_cll_params = ps_dec->ps_sei_parse->s_sei_cll_params;
        ps_dec->ps_sei->u1_sei_ave_params_present_flag =
                        ps_dec->ps_sei_parse->u1_sei_ave_params_present_flag;
        ps_dec->ps_sei->s_sei_ave_params = ps_dec->ps_sei_parse->s_sei_ave_params;
    }

    ps_dec->ps_sei_parse->u1_sei_mdcv_params_present_flag = 0;
    memset(&ps_dec->ps_sei_parse->s_sei_mdcv_params, 0, sizeof(sei_mdcv_params_t));
    ps_dec->ps_sei_parse->u1_sei_cll_params_present_flag = 0;
    memset(&ps_dec->ps_sei_parse->s_sei_cll_params, 0, sizeof(sei_cll_params_t));
    ps_dec->ps_sei_parse->u1_sei_ave_params_present_flag = 0;
    memset(&ps_dec->ps_sei_parse->s_sei_ave_params, 0, sizeof(sei_ave_params_t));
    ps_dec->ps_sei_parse->u1_sei_ccv_params_present_flag = 0;
    memset(&ps_dec->ps_sei_parse->s_sei_ccv_params, 0, sizeof(sei_ccv_params_t));

}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_parse_slice_partition                                     */
/*                                                                           */
/*  Description   : This function is intended to parse and decode slice part */
/*                  itions. Currently it's not implemented. Decoder will     */
/*                  print a message, skips this NAL and continues            */
/*  Inputs        : ps_dec    Decoder parameters                             */
/*                  ps_bitstrm    Bitstream                                */
/*  Globals       : None                                                     */
/*  Processing    : This functionality needs to be implemented               */
/*  Outputs       : None                                                     */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : Not implemented                                          */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         06 05 2002   NS              Draft                                */
/*                                                                           */
/*****************************************************************************/

WORD32 ih264d_parse_slice_partition(dec_struct_t * ps_dec,
                                    dec_bit_stream_t * ps_bitstrm)
{
    H264_DEC_DEBUG_PRINT("\nSlice partition not supported");
    UNUSED(ps_dec);
    UNUSED(ps_bitstrm);
    return (0);
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_parse_sei                                                */
/*                                                                           */
/*  Description   : This function is intended to parse and decode SEI        */
/*                  Currently it's not implemented. Decoder will print a     */
/*                  message, skips this NAL and continues                    */
/*  Inputs        : ps_dec    Decoder parameters                       */
/*                  ps_bitstrm    Bitstream                                */
/*  Globals       : None                                                     */
/*  Processing    : This functionality needs to be implemented               */
/*  Outputs       : None                                                     */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : Not implemented                                          */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         06 05 2002   NS              Draft                                */
/*                                                                           */
/*****************************************************************************/
WORD32 ih264d_parse_sei(dec_struct_t * ps_dec, dec_bit_stream_t * ps_bitstrm)
{
    UNUSED(ps_dec);
    UNUSED(ps_bitstrm);
    return (0);
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_parse_filler_data                                          */
/*                                                                           */
/*  Description   : This function is intended to parse and decode filler     */
/*                  data NAL. Currently it's not implemented. Decoder will   */
/*                  print a message, skips this NAL and continues            */
/*  Inputs        : ps_dec    Decoder parameters                       */
/*                  ps_bitstrm    Bitstream                                */
/*  Globals       : None                                                     */
/*  Processing    : This functionality needs to be implemented               */
/*  Outputs       : None                                                     */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : Not implemented                                          */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         06 05 2002   NS              Draft                                */
/*                                                                           */
/*****************************************************************************/
WORD32 ih264d_parse_filler_data(dec_struct_t * ps_dec,
                                dec_bit_stream_t * ps_bitstrm)
{
    UNUSED(ps_dec);
    UNUSED(ps_bitstrm);
    return (0);
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_parse_end_of_stream                                        */
/*                                                                           */
/*  Description   : This function is intended to parse and decode end of     */
/*                  sequence. Currently it's not implemented. Decoder will   */
/*                  print a message, skips this NAL and continues            */
/*  Inputs        : ps_dec    Decoder parameters                       */
/*  Globals       : None                                                     */
/*  Processing    : This functionality needs to be implemented               */
/*  Outputs       : None                                                     */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : Not implemented                                          */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         06 05 2002   NS              Draft                                */
/*                                                                           */
/*****************************************************************************/
void ih264d_parse_end_of_stream(dec_struct_t * ps_dec)
{
    UNUSED(ps_dec);
    return;
}

/*!
 **************************************************************************
 * \if Function name : ih264d_parse_pps \endif
 *
 * \brief
 *    Decodes Picture Parameter set
 *
 * \return
 *    0 on Success and Error code otherwise
 **************************************************************************
 */
WORD32 ih264d_parse_pps(dec_struct_t * ps_dec, dec_bit_stream_t * ps_bitstrm)
{
    UWORD8 uc_temp;
    dec_seq_params_t * ps_sps = NULL;
    dec_pic_params_t * ps_pps = NULL;
    UWORD32 *pu4_bitstrm_buf = ps_dec->ps_bitstrm->pu4_buffer;
    UWORD32 *pu4_bitstrm_ofst = &ps_dec->ps_bitstrm->u4_ofst;

    /* Variables used for error resilience checks */
    UWORD64 u8_temp;
    UWORD32 u4_temp;
    WORD32 i_temp;

    /* For High profile related syntax elements */
    UWORD8 u1_more_data_flag;
    WORD32 i4_i;

    if(!(ps_dec->i4_header_decoded & 1))
        return ERROR_INV_SPS_PPS_T;

    /*--------------------------------------------------------------------*/
    /* Decode pic_parameter_set_id and find corresponding pic params      */
    /*--------------------------------------------------------------------*/
    u4_temp = ih264d_uev(pu4_bitstrm_ofst, pu4_bitstrm_buf);
    if(u4_temp & MASK_ERR_PIC_SET_ID)
        return ERROR_INV_SPS_PPS_T;
    ps_pps = ps_dec->pv_scratch_sps_pps;
    *ps_pps = ps_dec->ps_pps[u4_temp];
    ps_pps->u1_pic_parameter_set_id = (WORD8)u4_temp;
    COPYTHECONTEXT("PPS: pic_parameter_set_id",ps_pps->u1_pic_parameter_set_id);

    /************************************************/
    /* initilization of High profile syntax element */
    /************************************************/
    ps_pps->i4_transform_8x8_mode_flag = 0;
    ps_pps->i4_pic_scaling_matrix_present_flag = 0;

    /*--------------------------------------------------------------------*/
    /* Decode seq_parameter_set_id and map it to a seq_parameter_set      */
    /*--------------------------------------------------------------------*/
    u4_temp = ih264d_uev(pu4_bitstrm_ofst, pu4_bitstrm_buf);
    if(u4_temp & MASK_ERR_SEQ_SET_ID)
        return ERROR_INV_SPS_PPS_T;
    COPYTHECONTEXT("PPS: seq_parameter_set_id",u4_temp);
    ps_sps = &ps_dec->ps_sps[u4_temp];

    if(FALSE == ps_sps->u1_is_valid)
        return ERROR_INV_SPS_PPS_T;
    ps_pps->ps_sps = ps_sps;

    /*--------------------------------------------------------------------*/
    /* Decode entropy_coding_mode                                         */
    /*--------------------------------------------------------------------*/
    ps_pps->u1_entropy_coding_mode = ih264d_get_bit_h264(ps_bitstrm);
    COPYTHECONTEXT("PPS: entropy_coding_mode_flag",ps_pps->u1_entropy_coding_mode);

    ps_pps->u1_pic_order_present_flag = ih264d_get_bit_h264(ps_bitstrm);
    COPYTHECONTEXT("PPS: pic_order_present_flag",ps_pps->u1_pic_order_present_flag);

    /*--------------------------------------------------------------------*/
    /* Decode num_slice_groups_minus1                                     */
    /*--------------------------------------------------------------------*/
    u8_temp = ih264d_uev(pu4_bitstrm_ofst, pu4_bitstrm_buf) + (UWORD64)1;
    if(u8_temp != 1)
    {
        return ERROR_FEATURE_UNAVAIL;
    }
    ps_pps->u1_num_slice_groups = u8_temp;
    COPYTHECONTEXT("PPS: num_slice_groups_minus1",ps_pps->u1_num_slice_groups -1);

    /*--------------------------------------------------------------------*/
    /* Other parameter set values                                         */
    /*--------------------------------------------------------------------*/
    u8_temp = (UWORD64)1 + ih264d_uev(pu4_bitstrm_ofst, pu4_bitstrm_buf);
    if(u8_temp > H264_MAX_REF_IDX)
        return ERROR_REF_IDX;
    ps_pps->u1_num_ref_idx_lx_active[0] = u8_temp;
    COPYTHECONTEXT("PPS: num_ref_idx_l0_active_minus1",
                    ps_pps->u1_num_ref_idx_lx_active[0] - 1);

    u8_temp = (UWORD64)1 + ih264d_uev(pu4_bitstrm_ofst, pu4_bitstrm_buf);
    if(u8_temp > H264_MAX_REF_IDX)
        return ERROR_REF_IDX;
    ps_pps->u1_num_ref_idx_lx_active[1] = u8_temp;
    COPYTHECONTEXT("PPS: num_ref_idx_l1_active_minus1",
                    ps_pps->u1_num_ref_idx_lx_active[1] - 1);

    ps_pps->u1_wted_pred_flag = ih264d_get_bit_h264(ps_bitstrm);
    COPYTHECONTEXT("PPS: weighted prediction u4_flag",ps_pps->u1_wted_pred_flag);
    uc_temp = ih264d_get_bits_h264(ps_bitstrm, 2);
    COPYTHECONTEXT("PPS: weighted_bipred_idc",uc_temp);
    ps_pps->u1_wted_bipred_idc = uc_temp;

    if(ps_pps->u1_wted_bipred_idc > MAX_WEIGHT_BIPRED_IDC)
        return ERROR_INV_SPS_PPS_T;

    WORD64 i8_temp = (WORD64)26
                        + ih264d_sev(pu4_bitstrm_ofst, pu4_bitstrm_buf);

    if((i8_temp < MIN_H264_QP) || (i8_temp > MAX_H264_QP))
        return ERROR_INV_RANGE_QP_T;

    ps_pps->u1_pic_init_qp = i8_temp;
    COPYTHECONTEXT("PPS: pic_init_qp_minus26",ps_pps->u1_pic_init_qp - 26);

    i8_temp = (WORD64)26 + ih264d_sev(pu4_bitstrm_ofst, pu4_bitstrm_buf);

    if((i8_temp < MIN_H264_QP) || (i8_temp > MAX_H264_QP))
        return ERROR_INV_RANGE_QP_T;

    ps_pps->u1_pic_init_qs = i8_temp;
    COPYTHECONTEXT("PPS: pic_init_qs_minus26",ps_pps->u1_pic_init_qs - 26);

    i_temp = ih264d_sev(pu4_bitstrm_ofst, pu4_bitstrm_buf);
    if((i_temp < -12) || (i_temp > 12))
        return ERROR_INV_RANGE_QP_T;
    ps_pps->i1_chroma_qp_index_offset = i_temp;
    COPYTHECONTEXT("PPS: chroma_qp_index_offset",ps_pps->i1_chroma_qp_index_offset);

    /***************************************************************************/
    /* initialize second_chroma_qp_index_offset to i1_chroma_qp_index_offset if */
    /* second_chroma_qp_index_offset is not present in bit-ps_bitstrm              */
    /***************************************************************************/
    ps_pps->i1_second_chroma_qp_index_offset =
                    ps_pps->i1_chroma_qp_index_offset;

    ps_pps->u1_deblocking_filter_parameters_present_flag = ih264d_get_bit_h264(
                    ps_bitstrm);
    COPYTHECONTEXT("PPS: deblocking_filter_control_present_flag",
                    ps_pps->u1_deblocking_filter_parameters_present_flag);
    ps_pps->u1_constrained_intra_pred_flag = ih264d_get_bit_h264(ps_bitstrm);
    COPYTHECONTEXT("PPS: constrained_intra_pred_flag",
                    ps_pps->u1_constrained_intra_pred_flag);
    ps_pps->u1_redundant_pic_cnt_present_flag = ih264d_get_bit_h264(ps_bitstrm);
    COPYTHECONTEXT("PPS: redundant_pic_cnt_present_flag",
                    ps_pps->u1_redundant_pic_cnt_present_flag);

    /* High profile related syntax elements */
    u1_more_data_flag = MORE_RBSP_DATA(ps_bitstrm);
    if(u1_more_data_flag && (ps_pps->ps_sps->u1_profile_idc == HIGH_PROFILE_IDC))
    {
        /* read transform_8x8_mode_flag  */
        ps_pps->i4_transform_8x8_mode_flag = (WORD32)ih264d_get_bit_h264(
                        ps_bitstrm);

        /* read pic_scaling_matrix_present_flag */
        ps_pps->i4_pic_scaling_matrix_present_flag =
                        (WORD32)ih264d_get_bit_h264(ps_bitstrm);

        if(ps_pps->i4_pic_scaling_matrix_present_flag)
        {
            /* read the scaling matrices */
            for(i4_i = 0; i4_i < (6 + (ps_pps->i4_transform_8x8_mode_flag << 1)); i4_i++)
            {
                ps_pps->u1_pic_scaling_list_present_flag[i4_i] =
                                ih264d_get_bit_h264(ps_bitstrm);

                if(ps_pps->u1_pic_scaling_list_present_flag[i4_i])
                {
                    WORD32 ret;
                    if(i4_i < 6)
                    {
                        ret = ih264d_scaling_list(
                                        ps_pps->i2_pic_scalinglist4x4[i4_i],
                                        16,
                                        &ps_pps->u1_pic_use_default_scaling_matrix_flag[i4_i],
                                        ps_bitstrm);
                    }
                    else
                    {
                        ret = ih264d_scaling_list(
                                        ps_pps->i2_pic_scalinglist8x8[i4_i - 6],
                                        64,
                                        &ps_pps->u1_pic_use_default_scaling_matrix_flag[i4_i],
                                        ps_bitstrm);
                    }

                    if(ret != OK)
                    {
                        return ret;
                    }
                }
            }
        }

        /* read second_chroma_qp_index_offset syntax element */
        i_temp = ih264d_sev(
                        pu4_bitstrm_ofst, pu4_bitstrm_buf);

        if((i_temp < -12) || (i_temp > 12))
            return ERROR_INV_RANGE_QP_T;

        ps_pps->i1_second_chroma_qp_index_offset = i_temp;
    }

    /* In case bitstream read has exceeded the filled size, then
       return an error */
    if(EXCEED_OFFSET(ps_bitstrm))
    {
        return ERROR_INV_SPS_PPS_T;
    }
    ps_pps->u1_is_valid = TRUE;
    ps_dec->ps_pps[ps_pps->u1_pic_parameter_set_id] = *ps_pps;
    return OK;
}

/*!
 **************************************************************************
 * \if Function name : ih264d_parse_sps \endif
 *
 * \brief
 *    Decodes Sequence parameter set from the bitstream
 *
 * \return
 *    0 on Success and Error code otherwise
 **************************************************************************
 */
UWORD32 ih264d_correct_level_idc(UWORD32 u4_level_idc, UWORD32 u4_total_mbs)
{
    UWORD32 u4_max_mbs_allowed;

    switch(u4_level_idc)
    {
        case H264_LEVEL_1_0:
            u4_max_mbs_allowed = MAX_MBS_LEVEL_10;
            break;
        case H264_LEVEL_1_1:
            u4_max_mbs_allowed = MAX_MBS_LEVEL_11;
            break;
        case H264_LEVEL_1_2:
            u4_max_mbs_allowed = MAX_MBS_LEVEL_12;
            break;
        case H264_LEVEL_1_3:
            u4_max_mbs_allowed = MAX_MBS_LEVEL_13;
            break;
        case H264_LEVEL_2_0:
            u4_max_mbs_allowed = MAX_MBS_LEVEL_20;
            break;
        case H264_LEVEL_2_1:
            u4_max_mbs_allowed = MAX_MBS_LEVEL_21;
            break;
        case H264_LEVEL_2_2:
            u4_max_mbs_allowed = MAX_MBS_LEVEL_22;
            break;
        case H264_LEVEL_3_0:
            u4_max_mbs_allowed = MAX_MBS_LEVEL_30;
            break;
        case H264_LEVEL_3_1:
            u4_max_mbs_allowed = MAX_MBS_LEVEL_31;
            break;
        case H264_LEVEL_3_2:
            u4_max_mbs_allowed = MAX_MBS_LEVEL_32;
            break;
        case H264_LEVEL_4_0:
            u4_max_mbs_allowed = MAX_MBS_LEVEL_40;
            break;
        case H264_LEVEL_4_1:
            u4_max_mbs_allowed = MAX_MBS_LEVEL_41;
            break;
        case H264_LEVEL_4_2:
            u4_max_mbs_allowed = MAX_MBS_LEVEL_42;
            break;
        case H264_LEVEL_5_0:
            u4_max_mbs_allowed = MAX_MBS_LEVEL_50;
            break;
        case H264_LEVEL_5_1:
        default:
            u4_max_mbs_allowed = MAX_MBS_LEVEL_51;
            break;

    }

    /*correct of the level is incorrect*/
    if(u4_total_mbs > u4_max_mbs_allowed)
    {
        if(u4_total_mbs > MAX_MBS_LEVEL_50)
            u4_level_idc = H264_LEVEL_5_1;
        else if(u4_total_mbs > MAX_MBS_LEVEL_42)
            u4_level_idc = H264_LEVEL_5_0;
        else if(u4_total_mbs > MAX_MBS_LEVEL_41)
            u4_level_idc = H264_LEVEL_4_2;
        else if(u4_total_mbs > MAX_MBS_LEVEL_40)
            u4_level_idc = H264_LEVEL_4_1;
        else if(u4_total_mbs > MAX_MBS_LEVEL_32)
            u4_level_idc = H264_LEVEL_4_0;
        else if(u4_total_mbs > MAX_MBS_LEVEL_31)
            u4_level_idc = H264_LEVEL_3_2;
        else if(u4_total_mbs > MAX_MBS_LEVEL_30)
            u4_level_idc = H264_LEVEL_3_1;
        else if(u4_total_mbs > MAX_MBS_LEVEL_21)
            u4_level_idc = H264_LEVEL_3_0;
        else if(u4_total_mbs > MAX_MBS_LEVEL_20)
            u4_level_idc = H264_LEVEL_2_1;
        else if(u4_total_mbs > MAX_MBS_LEVEL_10)
            u4_level_idc = H264_LEVEL_2_0;
    }

    return (u4_level_idc);

}
WORD32 ih264d_parse_sps(dec_struct_t *ps_dec, dec_bit_stream_t *ps_bitstrm)
{
    UWORD8 i;
    dec_seq_params_t *ps_seq = NULL;
    UWORD8 u1_profile_idc, u1_level_idc, u1_seq_parameter_set_id, u1_mb_aff_flag = 0;
    UWORD16 i2_max_frm_num;
    UWORD32 *pu4_bitstrm_buf = ps_bitstrm->pu4_buffer;
    UWORD32 *pu4_bitstrm_ofst = &ps_bitstrm->u4_ofst;
    UWORD8 u1_frm, uc_constraint_set0_flag, uc_constraint_set1_flag;
    WORD32 i4_cropped_ht, i4_cropped_wd;
    UWORD32 u4_temp;
    UWORD64 u8_temp;
    UWORD32 u4_pic_height_in_map_units, u4_pic_width_in_mbs;
    UWORD32 u2_pic_wd = 0;
    UWORD32 u2_pic_ht = 0;
    UWORD32 u2_frm_wd_y = 0;
    UWORD32 u2_frm_ht_y = 0;
    UWORD32 u2_frm_wd_uv = 0;
    UWORD32 u2_frm_ht_uv = 0;
    UWORD32 u2_crop_offset_y = 0;
    UWORD32 u2_crop_offset_uv = 0;
    WORD32 ret;
    WORD32 num_reorder_frames;
    /* High profile related syntax element */
    WORD32 i4_i;
    /* G050 */
    UWORD8 u1_frame_cropping_flag, u1_frame_cropping_rect_left_ofst,
                    u1_frame_cropping_rect_right_ofst,
                    u1_frame_cropping_rect_top_ofst,
                    u1_frame_cropping_rect_bottom_ofst;
    /* G050 */
    /*--------------------------------------------------------------------*/
    /* Decode seq_parameter_set_id and profile and level values           */
    /*--------------------------------------------------------------------*/
    SWITCHONTRACE;
    u1_profile_idc = ih264d_get_bits_h264(ps_bitstrm, 8);
    COPYTHECONTEXT("SPS: profile_idc",u1_profile_idc);

    /* G050 */
    uc_constraint_set0_flag = ih264d_get_bit_h264(ps_bitstrm);
    uc_constraint_set1_flag = ih264d_get_bit_h264(ps_bitstrm);
    ih264d_get_bit_h264(ps_bitstrm);

    /*****************************************************/
    /* Read 5 bits for uc_constraint_set3_flag (1 bit)   */
    /* and reserved_zero_4bits (4 bits) - Sushant        */
    /*****************************************************/
    ih264d_get_bits_h264(ps_bitstrm, 5);
    /* G050 */

    /* Check whether particular profile is suported or not */
    /* Check whether particular profile is suported or not */
    if((u1_profile_idc != MAIN_PROFILE_IDC) &&

    (u1_profile_idc != BASE_PROFILE_IDC) &&

    (u1_profile_idc != HIGH_PROFILE_IDC)

    )
    {

        /* Apart from Baseline, main and high profile,
         * only extended profile is supported provided
         * uc_constraint_set0_flag or uc_constraint_set1_flag are set to 1
         */
        if((u1_profile_idc != EXTENDED_PROFILE_IDC) ||
           ((uc_constraint_set1_flag != 1) && (uc_constraint_set0_flag != 1)))
        {
            return (ERROR_FEATURE_UNAVAIL);
        }
    }

    u1_level_idc = ih264d_get_bits_h264(ps_bitstrm, 8);



    COPYTHECONTEXT("SPS: u4_level_idc",u1_level_idc);

    u4_temp = ih264d_uev(pu4_bitstrm_ofst, pu4_bitstrm_buf);
    if(u4_temp & MASK_ERR_SEQ_SET_ID)
        return ERROR_INV_SPS_PPS_T;
    u1_seq_parameter_set_id = u4_temp;
    COPYTHECONTEXT("SPS: seq_parameter_set_id",
                    u1_seq_parameter_set_id);

    /*--------------------------------------------------------------------*/
    /* Find an seq param entry in seqparam array of decStruct             */
    /*--------------------------------------------------------------------*/

    ps_seq = ps_dec->pv_scratch_sps_pps;
    memset(ps_seq, 0, sizeof(dec_seq_params_t));

    if(ps_dec->i4_header_decoded & 1)
    {
        *ps_seq = *ps_dec->ps_cur_sps;
    }


    if((ps_dec->i4_header_decoded & 1) && (ps_seq->u1_profile_idc != u1_profile_idc))
    {
        ps_dec->u1_res_changed = 1;
        return IVD_RES_CHANGED;
    }

    if((ps_dec->i4_header_decoded & 1) && (ps_seq->u1_level_idc != u1_level_idc))
    {
        ps_dec->u1_res_changed = 1;
        return IVD_RES_CHANGED;
    }

    ps_seq->u1_profile_idc = u1_profile_idc;
    ps_seq->u1_level_idc = u1_level_idc;
    ps_seq->u1_seq_parameter_set_id = u1_seq_parameter_set_id;

    /*******************************************************************/
    /* Initializations for high profile - Sushant                      */
    /*******************************************************************/
    ps_seq->i4_chroma_format_idc = 1;
    ps_seq->i4_bit_depth_luma_minus8 = 0;
    ps_seq->i4_bit_depth_chroma_minus8 = 0;
    ps_seq->i4_qpprime_y_zero_transform_bypass_flag = 0;
    ps_seq->i4_seq_scaling_matrix_present_flag = 0;
    if(u1_profile_idc == HIGH_PROFILE_IDC)
    {

        /* reading chroma_format_idc   */
        ps_seq->i4_chroma_format_idc = ih264d_uev(pu4_bitstrm_ofst,
                                                  pu4_bitstrm_buf);

        /* Monochrome is not supported */
        if(ps_seq->i4_chroma_format_idc != 1)
        {
            return ERROR_FEATURE_UNAVAIL;
        }

        /* reading bit_depth_luma_minus8   */
        ps_seq->i4_bit_depth_luma_minus8 = ih264d_uev(pu4_bitstrm_ofst,
                                                      pu4_bitstrm_buf);

        if(ps_seq->i4_bit_depth_luma_minus8 != 0)
        {
            return ERROR_FEATURE_UNAVAIL;
        }

        /* reading bit_depth_chroma_minus8   */
        ps_seq->i4_bit_depth_chroma_minus8 = ih264d_uev(pu4_bitstrm_ofst,
                                                        pu4_bitstrm_buf);

        if(ps_seq->i4_bit_depth_chroma_minus8 != 0)
        {
            return ERROR_FEATURE_UNAVAIL;
        }

        /* reading qpprime_y_zero_transform_bypass_flag   */
        ps_seq->i4_qpprime_y_zero_transform_bypass_flag =
                        (WORD32)ih264d_get_bit_h264(ps_bitstrm);

        if(ps_seq->i4_qpprime_y_zero_transform_bypass_flag != 0)
        {
            return ERROR_INV_SPS_PPS_T;
        }

        /* reading seq_scaling_matrix_present_flag   */
        ps_seq->i4_seq_scaling_matrix_present_flag =
                        (WORD32)ih264d_get_bit_h264(ps_bitstrm);

        if(ps_seq->i4_seq_scaling_matrix_present_flag)
        {
            for(i4_i = 0; i4_i < 8; i4_i++)
            {
                ps_seq->u1_seq_scaling_list_present_flag[i4_i] =
                                ih264d_get_bit_h264(ps_bitstrm);

                /* initialize u1_use_default_scaling_matrix_flag[i4_i] to zero */
                /* before calling scaling list                             */
                ps_seq->u1_use_default_scaling_matrix_flag[i4_i] = 0;

                if(ps_seq->u1_seq_scaling_list_present_flag[i4_i])
                {
                    if(i4_i < 6)
                    {
                        ret = ih264d_scaling_list(
                                        ps_seq->i2_scalinglist4x4[i4_i],
                                        16,
                                        &ps_seq->u1_use_default_scaling_matrix_flag[i4_i],
                                        ps_bitstrm);
                    }
                    else
                    {
                        ret = ih264d_scaling_list(
                                        ps_seq->i2_scalinglist8x8[i4_i - 6],
                                        64,
                                        &ps_seq->u1_use_default_scaling_matrix_flag[i4_i],
                                        ps_bitstrm);
                    }
                    if(ret != OK)
                    {
                        return ret;
                    }
                }
            }
        }
    }
    /*--------------------------------------------------------------------*/
    /* Decode MaxFrameNum                                                 */
    /*--------------------------------------------------------------------*/
    u8_temp = (UWORD64)4 + ih264d_uev(pu4_bitstrm_ofst, pu4_bitstrm_buf);
    if(u8_temp > MAX_BITS_IN_FRAME_NUM)
    {
        return ERROR_INV_SPS_PPS_T;
    }
    ps_seq->u1_bits_in_frm_num = u8_temp;
    COPYTHECONTEXT("SPS: log2_max_frame_num_minus4",
                    (ps_seq->u1_bits_in_frm_num - 4));

    i2_max_frm_num = (1 << (ps_seq->u1_bits_in_frm_num));
    ps_seq->u2_u4_max_pic_num_minus1 = i2_max_frm_num - 1;
    /*--------------------------------------------------------------------*/
    /* Decode picture order count and related values                      */
    /*--------------------------------------------------------------------*/
    u4_temp = ih264d_uev(pu4_bitstrm_ofst, pu4_bitstrm_buf);

    if(u4_temp > MAX_PIC_ORDER_CNT_TYPE)
    {
        return ERROR_INV_POC_TYPE_T;
    }
    ps_seq->u1_pic_order_cnt_type = u4_temp;
    COPYTHECONTEXT("SPS: pic_order_cnt_type",ps_seq->u1_pic_order_cnt_type);

    ps_seq->u1_num_ref_frames_in_pic_order_cnt_cycle = 1;
    if(ps_seq->u1_pic_order_cnt_type == 0)
    {
        u8_temp = (UWORD64)4 + ih264d_uev(pu4_bitstrm_ofst, pu4_bitstrm_buf);
        if(u8_temp > MAX_BITS_IN_POC_LSB)
        {
            return ERROR_INV_SPS_PPS_T;
        }
        ps_seq->u1_log2_max_pic_order_cnt_lsb_minus = u8_temp;
        ps_seq->i4_max_pic_order_cntLsb = (1 << u8_temp);
        COPYTHECONTEXT("SPS: log2_max_pic_order_cnt_lsb_minus4",(u8_temp - 4));
    }
    else if(ps_seq->u1_pic_order_cnt_type == 1)
    {
        ps_seq->u1_delta_pic_order_always_zero_flag = ih264d_get_bit_h264(
                        ps_bitstrm);
        COPYTHECONTEXT("SPS: delta_pic_order_always_zero_flag",
                        ps_seq->u1_delta_pic_order_always_zero_flag);

        ps_seq->i4_ofst_for_non_ref_pic = ih264d_sev(pu4_bitstrm_ofst,
                                                     pu4_bitstrm_buf);
        COPYTHECONTEXT("SPS: offset_for_non_ref_pic",
                        ps_seq->i4_ofst_for_non_ref_pic);

        ps_seq->i4_ofst_for_top_to_bottom_field = ih264d_sev(
                        pu4_bitstrm_ofst, pu4_bitstrm_buf);
        COPYTHECONTEXT("SPS: offset_for_top_to_bottom_field",
                        ps_seq->i4_ofst_for_top_to_bottom_field);

        u4_temp = ih264d_uev(pu4_bitstrm_ofst, pu4_bitstrm_buf);
        if(u4_temp > 255)
            return ERROR_INV_SPS_PPS_T;
        ps_seq->u1_num_ref_frames_in_pic_order_cnt_cycle = u4_temp;
        COPYTHECONTEXT("SPS: num_ref_frames_in_pic_order_cnt_cycle",
                        ps_seq->u1_num_ref_frames_in_pic_order_cnt_cycle);

        for(i = 0; i < ps_seq->u1_num_ref_frames_in_pic_order_cnt_cycle; i++)
        {
            ps_seq->i4_ofst_for_ref_frame[i] = ih264d_sev(
                            pu4_bitstrm_ofst, pu4_bitstrm_buf);
            COPYTHECONTEXT("SPS: offset_for_ref_frame",
                            ps_seq->i4_ofst_for_ref_frame[i]);
        }
    }

    u4_temp = ih264d_uev(pu4_bitstrm_ofst, pu4_bitstrm_buf);

    if((u4_temp > H264_MAX_REF_PICS))
    {
        return ERROR_NUM_REF;
    }

    /* Compare with older num_ref_frames is header is already once */
    if((ps_dec->i4_header_decoded & 1) && (ps_seq->u1_num_ref_frames != u4_temp))
    {
        ps_dec->u1_res_changed = 1;
        return IVD_RES_CHANGED;
    }

    ps_seq->u1_num_ref_frames = u4_temp;
    COPYTHECONTEXT("SPS: num_ref_frames",ps_seq->u1_num_ref_frames);

    ps_seq->u1_gaps_in_frame_num_value_allowed_flag = ih264d_get_bit_h264(
                    ps_bitstrm);
    COPYTHECONTEXT("SPS: gaps_in_frame_num_value_allowed_flag",
                    ps_seq->u1_gaps_in_frame_num_value_allowed_flag);

    /*--------------------------------------------------------------------*/
    /* Decode FrameWidth and FrameHeight and related values               */
    /*--------------------------------------------------------------------*/
    u8_temp = (UWORD64)1 + ih264d_uev(pu4_bitstrm_ofst, pu4_bitstrm_buf);
    /* Check  for unsupported resolutions*/
    if(u8_temp > (H264_MAX_FRAME_WIDTH >> 4))
    {
        return IVD_STREAM_WIDTH_HEIGHT_NOT_SUPPORTED;
    }
    u4_pic_width_in_mbs = u8_temp;
    COPYTHECONTEXT("SPS: pic_width_in_mbs_minus1",
                   u4_pic_width_in_mbs - 1);

    u8_temp = (UWORD64)1 + ih264d_uev(pu4_bitstrm_ofst, pu4_bitstrm_buf);
    if (u8_temp > (H264_MAX_FRAME_HEIGHT >> 4))
    {
        return IVD_STREAM_WIDTH_HEIGHT_NOT_SUPPORTED;
    }
    u4_pic_height_in_map_units = u8_temp;

    ps_seq->u2_frm_wd_in_mbs = u4_pic_width_in_mbs;
    ps_seq->u2_frm_ht_in_mbs = u4_pic_height_in_map_units;

    u2_pic_wd = (u4_pic_width_in_mbs << 4);
    u2_pic_ht = (u4_pic_height_in_map_units << 4);
    /*--------------------------------------------------------------------*/
    /* Get the value of MaxMbAddress and Number of bits needed for it     */
    /*--------------------------------------------------------------------*/
    ps_seq->u2_max_mb_addr = (ps_seq->u2_frm_wd_in_mbs
                    * ps_seq->u2_frm_ht_in_mbs) - 1;

    ps_seq->u2_total_num_of_mbs = ps_seq->u2_max_mb_addr + 1;

    ps_seq->u1_level_idc = ih264d_correct_level_idc(
                    u1_level_idc, ps_seq->u2_total_num_of_mbs);

    u1_frm = ih264d_get_bit_h264(ps_bitstrm);
    if((ps_dec->i4_header_decoded & 1) && (ps_seq->u1_frame_mbs_only_flag != u1_frm))
    {
        ps_dec->u1_res_changed = 1;
        return IVD_RES_CHANGED;
    }

    ps_seq->u1_frame_mbs_only_flag = u1_frm;

    COPYTHECONTEXT("SPS: frame_mbs_only_flag", u1_frm);

    if(!u1_frm)
        u1_mb_aff_flag = ih264d_get_bit_h264(ps_bitstrm);

    if((ps_dec->i4_header_decoded & 1)
                    && (ps_seq->u1_mb_aff_flag != u1_mb_aff_flag))
    {
        ps_dec->u1_res_changed = 1;
        return IVD_RES_CHANGED;
    }

    if(!u1_frm)
    {
        u2_pic_ht <<= 1;
        ps_seq->u1_mb_aff_flag = u1_mb_aff_flag;
        COPYTHECONTEXT("SPS: mb_adaptive_frame_field_flag",
                        ps_seq->u1_mb_aff_flag);

    }
    else
        ps_seq->u1_mb_aff_flag = 0;

    ps_seq->u1_direct_8x8_inference_flag = ih264d_get_bit_h264(ps_bitstrm);

    COPYTHECONTEXT("SPS: direct_8x8_inference_flag",
                    ps_seq->u1_direct_8x8_inference_flag);

    /* G050 */
    u1_frame_cropping_flag = ih264d_get_bit_h264(ps_bitstrm);
    COPYTHECONTEXT("SPS: frame_cropping_flag",u1_frame_cropping_flag);

    if(u1_frame_cropping_flag)
    {
        u1_frame_cropping_rect_left_ofst = ih264d_uev(pu4_bitstrm_ofst,
                                                      pu4_bitstrm_buf);
        COPYTHECONTEXT("SPS: frame_cropping_rect_left_offset",
                        u1_frame_cropping_rect_left_ofst);
        u1_frame_cropping_rect_right_ofst = ih264d_uev(pu4_bitstrm_ofst,
                                                       pu4_bitstrm_buf);
        COPYTHECONTEXT("SPS: frame_cropping_rect_right_offset",
                        u1_frame_cropping_rect_right_ofst);
        u1_frame_cropping_rect_top_ofst = ih264d_uev(pu4_bitstrm_ofst,
                                                     pu4_bitstrm_buf);
        COPYTHECONTEXT("SPS: frame_cropping_rect_top_offset",
                        u1_frame_cropping_rect_top_ofst);
        u1_frame_cropping_rect_bottom_ofst = ih264d_uev(pu4_bitstrm_ofst,
                                                        pu4_bitstrm_buf);
        COPYTHECONTEXT("SPS: frame_cropping_rect_bottom_offset",
                        u1_frame_cropping_rect_bottom_ofst);

		ps_dec->u1_frame_cropping_flag = u1_frame_cropping_flag;
		ps_dec->u1_frame_cropping_rect_left_ofst = u1_frame_cropping_rect_left_ofst;
		ps_dec->u1_frame_cropping_rect_right_ofst = u1_frame_cropping_rect_right_ofst;
		ps_dec->u1_frame_cropping_rect_top_ofst = u1_frame_cropping_rect_top_ofst;
		ps_dec->u1_frame_cropping_rect_bottom_ofst = u1_frame_cropping_rect_bottom_ofst;
    }
    else
    {
        ps_dec->u1_frame_cropping_flag = 0;
		ps_dec->u1_frame_cropping_rect_left_ofst = 0;
		ps_dec->u1_frame_cropping_rect_right_ofst = 0;
		ps_dec->u1_frame_cropping_rect_top_ofst = 0;
		ps_dec->u1_frame_cropping_rect_bottom_ofst = 0;
    }

    /* G050 */

    ps_seq->u1_vui_parameters_present_flag = ih264d_get_bit_h264(ps_bitstrm);
    COPYTHECONTEXT("SPS: vui_parameters_present_flag",
                    ps_seq->u1_vui_parameters_present_flag);

    u2_frm_wd_y = u2_pic_wd + (UWORD8)(PAD_LEN_Y_H << 1);
    if(1 == ps_dec->u4_share_disp_buf)
    {
        if(ps_dec->u4_app_disp_width > u2_frm_wd_y)
            u2_frm_wd_y = ps_dec->u4_app_disp_width;
    }

    u2_frm_ht_y = u2_pic_ht + (UWORD8)(PAD_LEN_Y_V << 2);
    u2_frm_wd_uv = u2_pic_wd + (UWORD8)(PAD_LEN_UV_H << 2);
    u2_frm_wd_uv = MAX(u2_frm_wd_uv, u2_frm_wd_y);

    u2_frm_ht_uv = (u2_pic_ht >> 1) + (UWORD8)(PAD_LEN_UV_V << 2);
    u2_frm_ht_uv = MAX(u2_frm_ht_uv, (u2_frm_ht_y >> 1));


    /* Calculate display picture width, height and start u4_ofst from YUV420 */
    /* pictute buffers as per cropping information parsed above             */
    {
        UWORD16 u2_rgt_ofst = 0;
        UWORD16 u2_lft_ofst = 0;
        UWORD16 u2_top_ofst = 0;
        UWORD16 u2_btm_ofst = 0;
        UWORD8 u1_frm_mbs_flag;
        UWORD8 u1_vert_mult_factor;

        //if(u1_frame_cropping_flag)
        //{
        //    /* Calculate right and left u4_ofst for cropped picture           */
        //    u2_rgt_ofst = u1_frame_cropping_rect_right_ofst << 1;
        //    u2_lft_ofst = u1_frame_cropping_rect_left_ofst << 1;

        //    /* Know frame MBs only u4_flag                                      */
        //    u1_frm_mbs_flag = (1 == ps_seq->u1_frame_mbs_only_flag);

        //    /* Simplify the vertical u4_ofst calculation from field/frame     */
        //    u1_vert_mult_factor = (2 - u1_frm_mbs_flag);

        //    /* Calculate bottom and top u4_ofst for cropped  picture          */
        //    u2_btm_ofst = (u1_frame_cropping_rect_bottom_ofst
        //                    << u1_vert_mult_factor);
        //    u2_top_ofst = (u1_frame_cropping_rect_top_ofst
        //                    << u1_vert_mult_factor);
        //}

        /* Calculate u4_ofst from start of YUV 420 picture buffer to start of*/
        /* cropped picture buffer                                           */
        u2_crop_offset_y = (u2_frm_wd_y * u2_top_ofst) + (u2_lft_ofst);
        u2_crop_offset_uv = (u2_frm_wd_uv * (u2_top_ofst >> 1))
                        + (u2_lft_ofst >> 1) * YUV420SP_FACTOR;
        /* Calculate the display picture width and height based on crop      */
        /* information                                                       */
        i4_cropped_ht = (WORD32)u2_pic_ht - (WORD32)(u2_btm_ofst + u2_top_ofst);
        i4_cropped_wd = (WORD32)u2_pic_wd - (WORD32)(u2_rgt_ofst + u2_lft_ofst);

        if((i4_cropped_ht < MB_SIZE) || (i4_cropped_wd < MB_SIZE))
        {
            return ERROR_INV_SPS_PPS_T;
        }

        if((ps_dec->i4_header_decoded & 1) && (ps_dec->u2_pic_wd != u2_pic_wd))
        {
            ps_dec->u1_res_changed = 1;
            return IVD_RES_CHANGED;
        }

        if((ps_dec->i4_header_decoded & 1) && (ps_dec->u2_disp_width != i4_cropped_wd))
        {
            ps_dec->u1_res_changed = 1;
            return IVD_RES_CHANGED;
        }

        if((ps_dec->i4_header_decoded & 1) && (ps_dec->u2_pic_ht != u2_pic_ht))
        {
            ps_dec->u1_res_changed = 1;
            return IVD_RES_CHANGED;
        }

        if((ps_dec->i4_header_decoded & 1) && (ps_dec->u2_disp_height != i4_cropped_ht))
        {
            ps_dec->u1_res_changed = 1;
            return IVD_RES_CHANGED;
        }

        /* Check again for unsupported resolutions with updated values*/
        if((u2_pic_wd > H264_MAX_FRAME_WIDTH) || (u2_pic_ht > H264_MAX_FRAME_HEIGHT)
                || (u2_pic_wd < H264_MIN_FRAME_WIDTH) || (u2_pic_ht < H264_MIN_FRAME_HEIGHT)
                || (u2_pic_wd * (UWORD32)u2_pic_ht > H264_MAX_FRAME_SIZE))
        {
            return IVD_STREAM_WIDTH_HEIGHT_NOT_SUPPORTED;
        }

        /* If MBAff is enabled, decoder support is limited to streams with
         * width less than half of H264_MAX_FRAME_WIDTH.
         * In case of MBAff decoder processes two rows at a time
         */
        if((u2_pic_wd << ps_seq->u1_mb_aff_flag) > H264_MAX_FRAME_WIDTH)
        {
            return IVD_STREAM_WIDTH_HEIGHT_NOT_SUPPORTED;
        }

    }

    /* Backup num_reorder_frames if header is already decoded */
    if((ps_dec->i4_header_decoded & 1) &&
                    (1 == ps_seq->u1_vui_parameters_present_flag) &&
                    (1 == ps_seq->s_vui.u1_bitstream_restriction_flag))
    {
        num_reorder_frames =  (WORD32)ps_seq->s_vui.u4_num_reorder_frames;
    }
    else
    {
        num_reorder_frames = -1;
    }
    if(1 == ps_seq->u1_vui_parameters_present_flag)
    {
        ret = ih264d_parse_vui_parametres(&ps_seq->s_vui, ps_bitstrm);
        if(ret != OK)
            return ret;
    }

    /* Compare older num_reorder_frames with the new one if header is already decoded */
    if((ps_dec->i4_header_decoded & 1) &&
                    (-1 != num_reorder_frames) &&
                    (1 == ps_seq->u1_vui_parameters_present_flag) &&
                    (1 == ps_seq->s_vui.u1_bitstream_restriction_flag) &&
                    ((WORD32)ps_seq->s_vui.u4_num_reorder_frames != num_reorder_frames))
    {
        ps_dec->u1_res_changed = 1;
        return IVD_RES_CHANGED;
    }

    /* In case bitstream read has exceeded the filled size, then
     return an error */
    if (EXCEED_OFFSET(ps_bitstrm))
    {
        return ERROR_INV_SPS_PPS_T;
    }

    /*--------------------------------------------------------------------*/
    /* All initializations to ps_dec are beyond this point                */
    /*--------------------------------------------------------------------*/
    {
        WORD32 reorder_depth = ih264d_get_dpb_size(ps_seq);
        if((1 == ps_seq->u1_vui_parameters_present_flag) &&
           (1 == ps_seq->s_vui.u1_bitstream_restriction_flag))
        {
            reorder_depth = ps_seq->s_vui.u4_num_reorder_frames + 1;
        }

        if (reorder_depth > H264_MAX_REF_PICS)
        {
            return ERROR_INV_SPS_PPS_T;
        }

        if(ps_seq->u1_frame_mbs_only_flag != 1)
            reorder_depth *= 2;
        ps_dec->i4_reorder_depth = reorder_depth + DISPLAY_LATENCY;
    }
    ps_dec->u2_disp_height = i4_cropped_ht;
    ps_dec->u2_disp_width = i4_cropped_wd;

    ps_dec->u2_pic_wd = u2_pic_wd;
    ps_dec->u2_pic_ht = u2_pic_ht;

    /* Determining the Width and Height of Frame from that of Picture */
    ps_dec->u2_frm_wd_y = u2_frm_wd_y;
    ps_dec->u2_frm_ht_y = u2_frm_ht_y;

    ps_dec->u2_frm_wd_uv = u2_frm_wd_uv;
    ps_dec->u2_frm_ht_uv = u2_frm_ht_uv;
    ps_dec->s_pad_mgr.u1_pad_len_y_v = (UWORD8)(PAD_LEN_Y_V << (1 - u1_frm));
    ps_dec->s_pad_mgr.u1_pad_len_cr_v = (UWORD8)(PAD_LEN_UV_V << (1 - u1_frm));

    ps_dec->u2_frm_wd_in_mbs = ps_seq->u2_frm_wd_in_mbs;
    ps_dec->u2_frm_ht_in_mbs = ps_seq->u2_frm_ht_in_mbs;

    ps_dec->u2_crop_offset_y = u2_crop_offset_y;
    ps_dec->u2_crop_offset_uv = u2_crop_offset_uv;

    ps_seq->u1_is_valid = TRUE;
    ps_dec->ps_sps[u1_seq_parameter_set_id] = *ps_seq;
    ps_dec->ps_cur_sps = &ps_dec->ps_sps[u1_seq_parameter_set_id];

    return OK;
}

/*!
 **************************************************************************
 * \if Function name : ih264d_parse_end_of_sequence \endif
 *
 * \brief
 *    Decodes End of Sequence.
 *
 * \param ps_bitstrm   : Pointer to bit ps_bitstrm containing the NAL unit
 *
 * \return
 *    0 on Success and error code otherwise
 **************************************************************************
 */
WORD32 ih264d_parse_end_of_sequence(dec_struct_t * ps_dec)
{
    WORD32 ret;

    ret = ih264d_end_of_pic_processing(ps_dec);
    return ret;
}

/*!
 **************************************************************************
 * \if Function name : AcessUnitDelimiterRbsp \endif
 *
 * \brief
 *    Decodes AcessUnitDelimiterRbsp.
 *
 * \param ps_bitstrm   : Pointer to bit ps_bitstrm containing the NAL unit
 *
 * \return
 *    0 on Success and error code otherwise
 **************************************************************************
 */

WORD32 ih264d_access_unit_delimiter_rbsp(dec_struct_t * ps_dec)
{
    UWORD8 u1_primary_pic_type;
    u1_primary_pic_type = ih264d_get_bits_h264(ps_dec->ps_bitstrm, 3);
    switch(u1_primary_pic_type)
    {
        case I_PIC:
        case SI_PIC:
        case ISI_PIC:
            ps_dec->ps_dec_err_status->u1_pic_aud_i = PIC_TYPE_I;
            break;
        default:
            ps_dec->ps_dec_err_status->u1_pic_aud_i = PIC_TYPE_UNKNOWN;
    }
    return (0);
}
/*!
 **************************************************************************
 * \if Function name : ih264d_parse_nal_unit \endif
 *
 * \brief
 *    Decodes NAL unit
 *
 * \return
 *    0 on Success and error code otherwise
 **************************************************************************
 */

WORD32 ih264d_parse_nal_unit(iv_obj_t *dec_hdl,
                          ivd_video_decode_op_t *ps_dec_op,
                          UWORD8 *pu1_buf,
                          UWORD32 u4_length)
{

    dec_bit_stream_t *ps_bitstrm;


    dec_struct_t *ps_dec = (dec_struct_t *)dec_hdl->pv_codec_handle;
    ivd_video_decode_ip_t *ps_dec_in =
                    (ivd_video_decode_ip_t *)ps_dec->pv_dec_in;
    dec_slice_params_t * ps_cur_slice = ps_dec->ps_cur_slice;
    UWORD8 u1_first_byte, u1_nal_ref_idc;
    UWORD8 u1_nal_unit_type;
    WORD32 i_status = OK;
    ps_bitstrm = ps_dec->ps_bitstrm;

    if(pu1_buf)
    {
        if(u4_length)
        {
            ps_dec_op->u4_frame_decoded_flag = 0;
            ih264d_process_nal_unit(ps_dec->ps_bitstrm, pu1_buf,
                                    u4_length);

            SWITCHOFFTRACE;
            u1_first_byte = ih264d_get_bits_h264(ps_bitstrm, 8);

            if(NAL_FORBIDDEN_BIT(u1_first_byte))
            {
                H264_DEC_DEBUG_PRINT("\nForbidden bit set in Nal Unit, Let's try\n");
            }
            u1_nal_unit_type = NAL_UNIT_TYPE(u1_first_byte);
            // if any other nal unit other than slice nal is encountered in between a
            // frame break out of loop without consuming header
            if ((ps_dec->u4_slice_start_code_found == 1)
                    && (ps_dec->u1_pic_decode_done != 1)
                    && (u1_nal_unit_type > IDR_SLICE_NAL))
            {
                return ERROR_INCOMPLETE_FRAME;
            }
            ps_dec->u1_nal_unit_type = u1_nal_unit_type;
            u1_nal_ref_idc = (UWORD8)(NAL_REF_IDC(u1_first_byte));
            //Skip all NALUs if SPS and PPS are not decoded
            switch(u1_nal_unit_type)
            {
                case SLICE_DATA_PARTITION_A_NAL:
                case SLICE_DATA_PARTITION_B_NAL:
                case SLICE_DATA_PARTITION_C_NAL:
                    if(!ps_dec->i4_decode_header)
                        ih264d_parse_slice_partition(ps_dec, ps_bitstrm);

                    break;

                case IDR_SLICE_NAL:
                case SLICE_NAL:

                    /* ! */
                    DEBUG_THREADS_PRINTF("Decoding  a slice NAL\n");
                    if(!ps_dec->i4_decode_header)
                    {
                        if(ps_dec->i4_header_decoded == 3)
                        {
                            ih264d_get_pre_sei_params(ps_dec, u1_nal_unit_type);
                            /* ! */
                            ps_dec->u4_slice_start_code_found = 1;

                            ih264d_rbsp_to_sodb(ps_dec->ps_bitstrm);

                            i_status = ih264d_parse_decode_slice(
                                            (UWORD8)(u1_nal_unit_type
                                                            == IDR_SLICE_NAL),
                                            u1_nal_ref_idc, ps_dec);

                            if(i_status != OK)
                            {
                                return i_status;
                            }
                        }
                        else
                        {
                            H264_DEC_DEBUG_PRINT(
                                            "\nSlice NAL Supplied but no header has been supplied\n");
                        }
                    }
                    break;

                case SEI_NAL:
                    if(!ps_dec->i4_decode_header)
                    {
                        ih264d_rbsp_to_sodb(ps_dec->ps_bitstrm);
                        i_status = ih264d_parse_sei_message(ps_dec, ps_bitstrm);
                        if(i_status != OK)
                            return i_status;
                        ih264d_parse_sei(ps_dec, ps_bitstrm);
                    }
                    break;
                case SEQ_PARAM_NAL:
                    /* ! */
                    ih264d_rbsp_to_sodb(ps_dec->ps_bitstrm);
                    i_status = ih264d_parse_sps(ps_dec, ps_bitstrm);
                    ps_dec->u4_sps_cnt_in_process++;
                    /*If a resolution change happens within a process call, due to multiple sps
                     * we will not support it.
                     */
                    if((ps_dec->u4_sps_cnt_in_process > 1 ) &&
                                    (i_status == IVD_RES_CHANGED))
                    {
                        i_status = ERROR_INV_SPS_PPS_T;
                        ps_dec->u1_res_changed = 0;
                    }
                    if(i_status == ERROR_INV_SPS_PPS_T)
                        return i_status;
                    if(!i_status)
                        ps_dec->i4_header_decoded |= 0x1;
                    break;

                case PIC_PARAM_NAL:
                    /* ! */
                    ih264d_rbsp_to_sodb(ps_dec->ps_bitstrm);
                    i_status = ih264d_parse_pps(ps_dec, ps_bitstrm);
                    if(i_status == ERROR_INV_SPS_PPS_T)
                        return i_status;
                    if(!i_status)
                        ps_dec->i4_header_decoded |= 0x2;
                    break;
                case ACCESS_UNIT_DELIMITER_RBSP:
                    if(!ps_dec->i4_decode_header)
                    {
                        ih264d_access_unit_delimiter_rbsp(ps_dec);
                    }
                    break;
                    //Let us ignore the END_OF_SEQ_RBSP NAL and decode even after this NAL
                case END_OF_STREAM_RBSP:
                    if(!ps_dec->i4_decode_header)
                    {
                        ih264d_parse_end_of_stream(ps_dec);
                    }
                    break;
                case FILLER_DATA_NAL:
                    if(!ps_dec->i4_decode_header)
                    {
                        ih264d_parse_filler_data(ps_dec, ps_bitstrm);
                    }
                    break;
                default:
                    H264_DEC_DEBUG_PRINT("\nUnknown NAL type %d\n", u1_nal_unit_type);
                    break;
            }

        }

    }

    return i_status;

}

