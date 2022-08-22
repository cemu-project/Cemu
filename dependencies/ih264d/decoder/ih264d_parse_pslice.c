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
 **************************************************************************
 * \file ih264d_parse_pslice.c
 *
 * \brief
 *    Contains routines that decode a I slice type
 *
 * Detailed_description
 *
 * \date
 *    07/07/2003
 *
 * \author  NS
 **************************************************************************
 */

#include <string.h>
#include "ih264_defs.h"
#include "ih264d_bitstrm.h"
#include "ih264d_defs.h"
#include "ih264d_debug.h"
#include "ih264d_tables.h"
#include "ih264d_structs.h"
#include "ih264d_defs.h"
#include "ih264d_parse_cavlc.h"
#include "ih264d_mb_utils.h"
#include "ih264d_parse_slice.h"
#include "ih264d_mvpred.h"
#include "ih264d_parse_islice.h"
#include "ih264d_process_intra_mb.h"
#include "ih264d_inter_pred.h"
#include "ih264d_process_pslice.h"
#include "ih264d_deblocking.h"
#include "ih264d_cabac.h"
#include "ih264d_parse_mb_header.h"
#include "ih264d_error_handler.h"
#include "ih264d_defs.h"
#include "ih264d_format_conv.h"
#include "ih264d_quant_scaling.h"
#include "ih264d_thread_parse_decode.h"
#include "ih264d_thread_compute_bs.h"
#include "ih264d_process_bslice.h"
#include "ithread.h"
#include "ih264d_utils.h"
#include "ih264d_format_conv.h"

void ih264d_init_cabac_contexts(UWORD8 u1_slice_type, dec_struct_t * ps_dec);
void ih264d_deblock_mb_level(dec_struct_t *ps_dec,
                             dec_mb_info_t *ps_cur_mb_info,
                             UWORD32 nmb_index);

/*!
 **************************************************************************
 * \if Function name : ih264d_parse_pmb_cavlc \endif
 *
 * \brief
 *    This function parses CAVLC syntax of a P MB.
 *
 * \return
 *    0 on Success and Error code otherwise
 **************************************************************************
 */
WORD32 ih264d_parse_pmb_cavlc(dec_struct_t * ps_dec,
                              dec_mb_info_t * ps_cur_mb_info,
                              UWORD8 u1_mb_num,
                              UWORD8 u1_num_mbsNby2)
{
    UWORD32 u1_num_mb_part;
    UWORD32 uc_sub_mb;
    dec_bit_stream_t * const ps_bitstrm = ps_dec->ps_bitstrm;
    UWORD32 * const pu4_bitstrm_buf = ps_bitstrm->pu4_buffer;
    UWORD32 *pu4_bitstrm_ofst = &ps_bitstrm->u4_ofst;

    parse_pmbarams_t * ps_parse_mb_data = ps_dec->ps_parse_mb_data
                    + u1_num_mbsNby2;
    WORD8 * pi1_ref_idx = ps_parse_mb_data->i1_ref_idx[0];
    const UWORD8 u1_mbaff = ps_dec->ps_cur_slice->u1_mbaff_frame_flag;
    const UWORD8 * pu1_num_mb_part = (const UWORD8 *)gau1_ih264d_num_mb_part;
    UWORD8 * pu1_col_info = ps_parse_mb_data->u1_col_info;

    UWORD32 u1_mb_type = ps_cur_mb_info->u1_mb_type;
    UWORD32 u4_sum_mb_mode_pack = 0;
    WORD32 ret;

    UWORD8 u1_no_submb_part_size_lt8x8_flag = 1;
    ps_cur_mb_info->u1_tran_form8x8 = 0;
    ps_cur_mb_info->ps_curmb->u1_tran_form8x8 = 0;

    ps_cur_mb_info->u1_yuv_dc_block_flag = 0;

    ps_cur_mb_info->u1_mb_mc_mode = u1_mb_type;
    uc_sub_mb = ((u1_mb_type == PRED_8x8) | (u1_mb_type == PRED_8x8R0));

    /* Reading the subMB type */
    if(uc_sub_mb)
    {
        WORD32 i;
        UWORD8 u1_colz = (PRED_8x8 << 6);

        for(i = 0; i < 4; i++)
        {
            UWORD32 ui_sub_mb_mode;

            //Inlined ih264d_uev
            UWORD32 u4_bitstream_offset = *pu4_bitstrm_ofst;
            UWORD32 u4_word, u4_ldz;

            /***************************************************************/
            /* Find leading zeros in next 32 bits                          */
            /***************************************************************/
            NEXTBITS_32(u4_word, u4_bitstream_offset, pu4_bitstrm_buf);
            u4_ldz = CLZ(u4_word);
            /* Flush the ps_bitstrm */
            u4_bitstream_offset += (u4_ldz + 1);
            /* Read the suffix from the ps_bitstrm */
            u4_word = 0;
            if(u4_ldz)
                GETBITS(u4_word, u4_bitstream_offset, pu4_bitstrm_buf,
                        u4_ldz);
            *pu4_bitstrm_ofst = u4_bitstream_offset;
            ui_sub_mb_mode = ((1 << u4_ldz) + u4_word - 1);
            //Inlined ih264d_uev

            if(ui_sub_mb_mode > 3)
            {
                return ERROR_SUB_MB_TYPE;
            }
            else
            {
                u4_sum_mb_mode_pack = (u4_sum_mb_mode_pack << 8) | ui_sub_mb_mode;
                /* Storing collocated information */
                *pu1_col_info++ = u1_colz | (UWORD8)(ui_sub_mb_mode << 4);

                COPYTHECONTEXT("sub_mb_type", ui_sub_mb_mode);
            }

            /* check if Motion compensation is done below 8x8 */
            if(ui_sub_mb_mode != P_L0_8x8)
            {
                u1_no_submb_part_size_lt8x8_flag = 0;
            }
        }

        //
        u1_num_mb_part = 4;
    }
    else
    {
        *pu1_col_info++ = (u1_mb_type << 6);
        if(u1_mb_type)
            *pu1_col_info++ = (u1_mb_type << 6);
        u1_num_mb_part = pu1_num_mb_part[u1_mb_type];

    }

    /* Decoding reference index 0: For simple profile the following   */
    /* conditions are always true (mb_field_decoding_flag == 0);      */
    /* (MbPartPredMode != PredL1)                                     */

    {

        UWORD8 uc_field = ps_cur_mb_info->u1_mb_field_decodingflag;
        UWORD8 uc_num_ref_idx_l0_active_minus1 =
                        (ps_dec->ps_cur_slice->u1_num_ref_idx_lx_active[0]
                                        << (u1_mbaff & uc_field)) - 1;

        if((uc_num_ref_idx_l0_active_minus1 > 0) & (u1_mb_type != PRED_8x8R0))
        {
            if(1 == uc_num_ref_idx_l0_active_minus1)
                ih264d_parse_pmb_ref_index_cavlc_range1(
                                u1_num_mb_part, ps_bitstrm, pi1_ref_idx,
                                uc_num_ref_idx_l0_active_minus1);
            else
            {
                ret = ih264d_parse_pmb_ref_index_cavlc(
                                u1_num_mb_part, ps_bitstrm, pi1_ref_idx,
                                uc_num_ref_idx_l0_active_minus1);
                if(ret != OK)
                    return ret;
            }
        }
        else
        {
            /* When there exists only a single frame to predict from */
            UWORD8 uc_i;
            for(uc_i = 0; uc_i < u1_num_mb_part; uc_i++)
                /* Storing Reference Idx Information */
                pi1_ref_idx[uc_i] = 0;
        }
    }

    {
        UWORD8 u1_p_idx, uc_i;
        parse_part_params_t * ps_part = ps_dec->ps_part;
        UWORD8 u1_sub_mb_mode, u1_num_subpart, u1_mb_part_width, u1_mb_part_height;
        UWORD8 u1_sub_mb_num;
        const UWORD8 * pu1_top_left_sub_mb_indx;
        mv_pred_t * ps_mv, *ps_mv_start = ps_dec->ps_mv_cur + (u1_mb_num << 4);
        /* Loading the table pointers */
        const UWORD8 * pu1_mb_partw = (const UWORD8 *)gau1_ih264d_mb_partw;
        const UWORD8 * pu1_mb_parth = (const UWORD8 *)gau1_ih264d_mb_parth;
        const UWORD8 * pu1_sub_mb_indx_mod =
                        (const UWORD8 *)(gau1_ih264d_submb_indx_mod)
                                        + (uc_sub_mb * 6);
        const UWORD8 * pu1_sub_mb_partw = (const UWORD8 *)gau1_ih264d_submb_partw;
        const UWORD8 * pu1_sub_mb_parth = (const UWORD8 *)gau1_ih264d_submb_parth;
        const UWORD8 * pu1_num_sub_mb_part =
                        (const UWORD8 *)gau1_ih264d_num_submb_part;

        UWORD16 u2_sub_mb_num = 0x028A;

        /*********************************************************/
        /* default initialisations for condition (uc_sub_mb == 0) */
        /* i.e. all are subpartitions of 8x8                     */
        /*********************************************************/
        u1_sub_mb_mode = 0;
        u1_num_subpart = 1;
        u1_mb_part_width = pu1_mb_partw[u1_mb_type];
        u1_mb_part_height = pu1_mb_parth[u1_mb_type];
        pu1_top_left_sub_mb_indx = pu1_sub_mb_indx_mod + (u1_mb_type << 1);
        u1_sub_mb_num = 0;

        /* Loop on number of partitions */
        for(uc_i = 0, u1_p_idx = 0; uc_i < u1_num_mb_part; uc_i++)
        {
            UWORD8 uc_j;
            if(uc_sub_mb)
            {
                u1_sub_mb_mode = u4_sum_mb_mode_pack >> 24;
                u1_num_subpart = pu1_num_sub_mb_part[u1_sub_mb_mode];
                u1_mb_part_width = pu1_sub_mb_partw[u1_sub_mb_mode];
                u1_mb_part_height = pu1_sub_mb_parth[u1_sub_mb_mode];
                pu1_top_left_sub_mb_indx = pu1_sub_mb_indx_mod + (u1_sub_mb_mode << 1);
                u1_sub_mb_num = u2_sub_mb_num >> 12;
                u4_sum_mb_mode_pack <<= 8;
                u2_sub_mb_num <<= 4;
            }

            /* Loop on Number of sub-partitions */
            for(uc_j = 0; uc_j < u1_num_subpart; uc_j++, pu1_top_left_sub_mb_indx++)
            {
                WORD16 i2_mvx, i2_mvy;
                u1_sub_mb_num += *pu1_top_left_sub_mb_indx;
                ps_mv = ps_mv_start + u1_sub_mb_num;

                /* Reading the differential Mv from the bitstream */
                //i2_mvx = ih264d_sev(pu4_bitstrm_ofst, pu4_bitstrm_buf);
                //inlining ih264d_sev
                {
                    UWORD32 u4_bitstream_offset = *pu4_bitstrm_ofst;
                    UWORD32 u4_word, u4_ldz, u4_abs_val;

                    /***************************************************************/
                    /* Find leading zeros in next 32 bits                          */
                    /***************************************************************/
                    NEXTBITS_32(u4_word, u4_bitstream_offset,
                                pu4_bitstrm_buf);
                    u4_ldz = CLZ(u4_word);

                    /* Flush the ps_bitstrm */
                    u4_bitstream_offset += (u4_ldz + 1);

                    /* Read the suffix from the ps_bitstrm */
                    u4_word = 0;
                    if(u4_ldz)
                        GETBITS(u4_word, u4_bitstream_offset,
                                pu4_bitstrm_buf, u4_ldz);

                    *pu4_bitstrm_ofst = u4_bitstream_offset;
                    u4_abs_val = ((1 << u4_ldz) + u4_word) >> 1;

                    if(u4_word & 0x1)
                        i2_mvx = (-(WORD32)u4_abs_val);
                    else
                        i2_mvx = (u4_abs_val);
                }
                //inlinined ih264d_sev
                COPYTHECONTEXT("MVD", i2_mvx);
                i2_mvy = ih264d_sev(pu4_bitstrm_ofst,
                                     pu4_bitstrm_buf);
                COPYTHECONTEXT("MVD", i2_mvy);

                /* Storing Info for partitions */
                ps_part->u1_is_direct = PART_NOT_DIRECT;
                ps_part->u1_sub_mb_num = u1_sub_mb_num;
                ps_part->u1_partheight = u1_mb_part_height;
                ps_part->u1_partwidth = u1_mb_part_width;

                /* Storing Mv residuals */
                ps_mv->i2_mv[0] = i2_mvx;
                ps_mv->i2_mv[1] = i2_mvy;

                /* Increment partition Index */
                u1_p_idx++;
                ps_part++;
            }
        }
        ps_parse_mb_data->u1_num_part = u1_p_idx;
        ps_dec->ps_part = ps_part;
    }

    {
        UWORD32 u4_cbp;

        /* Read the Coded block pattern */
        UWORD32 u4_bitstream_offset = *pu4_bitstrm_ofst;
        UWORD32 u4_word, u4_ldz;

        /***************************************************************/
        /* Find leading zeros in next 32 bits                          */
        /***************************************************************/
        NEXTBITS_32(u4_word, u4_bitstream_offset, pu4_bitstrm_buf);
        u4_ldz = CLZ(u4_word);
        /* Flush the ps_bitstrm */
        u4_bitstream_offset += (u4_ldz + 1);
        /* Read the suffix from the ps_bitstrm */
        u4_word = 0;
        if(u4_ldz)
            GETBITS(u4_word, u4_bitstream_offset, pu4_bitstrm_buf, u4_ldz);
        *pu4_bitstrm_ofst = u4_bitstream_offset;
        u4_cbp = ((1 << u4_ldz) + u4_word - 1);

        if(u4_cbp > 47)
            return ERROR_CBP;

        u4_cbp = *((UWORD8*)gau1_ih264d_cbp_inter + u4_cbp);
        COPYTHECONTEXT("coded_block_pattern", u4_cbp);
        ps_cur_mb_info->u1_cbp = u4_cbp;

        /* Read the transform8x8 u4_flag if present */
        if((ps_dec->s_high_profile.u1_transform8x8_present) && (u4_cbp & 0xf)
                        && u1_no_submb_part_size_lt8x8_flag)
        {
            ps_cur_mb_info->u1_tran_form8x8 = ih264d_get_bit_h264(ps_bitstrm);
            COPYTHECONTEXT("transform_size_8x8_flag", ps_cur_mb_info->u1_tran_form8x8);
            ps_cur_mb_info->ps_curmb->u1_tran_form8x8 = ps_cur_mb_info->u1_tran_form8x8;
        }

        /* Read mb_qp_delta */
        if(u4_cbp)
        {
            WORD32 i_temp;

            UWORD32 u4_bitstream_offset = *pu4_bitstrm_ofst;
            UWORD32 u4_word, u4_ldz, u4_abs_val;

            /***************************************************************/
            /* Find leading zeros in next 32 bits                          */
            /***************************************************************/
            NEXTBITS_32(u4_word, u4_bitstream_offset, pu4_bitstrm_buf);
            u4_ldz = CLZ(u4_word);

            /* Flush the ps_bitstrm */
            u4_bitstream_offset += (u4_ldz + 1);

            /* Read the suffix from the ps_bitstrm */
            u4_word = 0;
            if(u4_ldz)
                GETBITS(u4_word, u4_bitstream_offset, pu4_bitstrm_buf,
                        u4_ldz);

            *pu4_bitstrm_ofst = u4_bitstream_offset;
            u4_abs_val = ((1 << u4_ldz) + u4_word) >> 1;

            if(u4_word & 0x1)
                i_temp = (-(WORD32)u4_abs_val);
            else
                i_temp = (u4_abs_val);

            if((i_temp < -26) || (i_temp > 25))
                return ERROR_INV_RANGE_QP_T;
            //inlinined ih264d_sev

            COPYTHECONTEXT("mb_qp_delta", i_temp);
            if(i_temp)
            {
                ret = ih264d_update_qp(ps_dec, (WORD8)i_temp);
                if(ret != OK)
                    return ret;
            }

            ret = ih264d_parse_residual4x4_cavlc(ps_dec, ps_cur_mb_info, 0);
            if(ret != OK)
                return ret;
            if(EXCEED_OFFSET(ps_bitstrm))
                return ERROR_EOB_TERMINATE_T;
        }
        else
        {
            ih264d_update_nnz_for_skipmb(ps_dec, ps_cur_mb_info, CAVLC);
        }



    }

    return OK;
}

/*!
 **************************************************************************
 * \if Function name : ih264d_parse_pmb_cabac \endif
 *
 * \brief
 *    This function parses CABAC syntax of a P MB.
 *
 * \return
 *    0 on Success and Error code otherwise
 **************************************************************************
 */
WORD32 ih264d_parse_pmb_cabac(dec_struct_t * ps_dec,
                              dec_mb_info_t * ps_cur_mb_info,
                              UWORD8 u1_mb_num,
                              UWORD8 u1_num_mbsNby2)
{
    UWORD32 u1_num_mb_part;
    UWORD32 uc_sub_mb;
    parse_pmbarams_t * ps_parse_mb_data = ps_dec->ps_parse_mb_data
                    + u1_num_mbsNby2;
    WORD8 * pi1_ref_idx = ps_parse_mb_data->i1_ref_idx[0];
    const UWORD8 * pu1_num_mb_part = (const UWORD8 *)gau1_ih264d_num_mb_part;
    const UWORD32 u1_mb_type = ps_cur_mb_info->u1_mb_type;
    UWORD8 * pu1_col_info = ps_parse_mb_data->u1_col_info;
    UWORD32 u1_mb_mc_mode = u1_mb_type;
    ctxt_inc_mb_info_t * p_curr_ctxt = ps_dec->ps_curr_ctxt_mb_info;
    decoding_envirnoment_t * ps_cab_env = &ps_dec->s_cab_dec_env;
    dec_bit_stream_t * ps_bitstrm = ps_dec->ps_bitstrm;
    UWORD32 u4_sub_mb_pack = 0;
    WORD32 ret;

    UWORD8 u1_no_submb_part_size_lt8x8_flag = 1;
    ps_cur_mb_info->u1_tran_form8x8 = 0;
    ps_cur_mb_info->ps_curmb->u1_tran_form8x8 = 0;

    ps_cur_mb_info->u1_yuv_dc_block_flag = 0;

    p_curr_ctxt->u1_mb_type = CAB_P;
    ps_cur_mb_info->u1_mb_mc_mode = u1_mb_type;
    uc_sub_mb = ((u1_mb_type == PRED_8x8) | (u1_mb_type == PRED_8x8R0));

    /* Reading the subMB type */
    if(uc_sub_mb)
    {

        UWORD8 u1_colz = (PRED_8x8 << 6);
        u1_mb_mc_mode = 0;

        {
            UWORD8 u1_sub_mb_mode;
            u1_sub_mb_mode = ih264d_parse_submb_type_cabac(
                            0, ps_cab_env, ps_bitstrm,
                            ps_dec->p_sub_mb_type_t);
            if(u1_sub_mb_mode > 3)
                return ERROR_SUB_MB_TYPE;

            u4_sub_mb_pack = (u4_sub_mb_pack << 8) | u1_sub_mb_mode;
            /* Storing collocated information */
            *pu1_col_info++ = u1_colz | ((UWORD8)(u1_sub_mb_mode << 4));
            COPYTHECONTEXT("sub_mb_type", u1_sub_mb_mode);
            /* check if Motion compensation is done below 8x8 */
            if(u1_sub_mb_mode != P_L0_8x8)
            {
                u1_no_submb_part_size_lt8x8_flag = 0;
            }
        }
        {
            UWORD8 u1_sub_mb_mode;
            u1_sub_mb_mode = ih264d_parse_submb_type_cabac(
                            0, ps_cab_env, ps_bitstrm,
                            ps_dec->p_sub_mb_type_t);
            if(u1_sub_mb_mode > 3)
                return ERROR_SUB_MB_TYPE;

            u4_sub_mb_pack = (u4_sub_mb_pack << 8) | u1_sub_mb_mode;
            /* Storing collocated information */
            *pu1_col_info++ = u1_colz | ((UWORD8)(u1_sub_mb_mode << 4));
            COPYTHECONTEXT("sub_mb_type", u1_sub_mb_mode);
            /* check if Motion compensation is done below 8x8 */
            if(u1_sub_mb_mode != P_L0_8x8)
            {
                u1_no_submb_part_size_lt8x8_flag = 0;
            }
        }
        {
            UWORD8 u1_sub_mb_mode;
            u1_sub_mb_mode = ih264d_parse_submb_type_cabac(
                            0, ps_cab_env, ps_bitstrm,
                            ps_dec->p_sub_mb_type_t);
            if(u1_sub_mb_mode > 3)
                return ERROR_SUB_MB_TYPE;

            u4_sub_mb_pack = (u4_sub_mb_pack << 8) | u1_sub_mb_mode;
            /* Storing collocated information */
            *pu1_col_info++ = u1_colz | ((UWORD8)(u1_sub_mb_mode << 4));
            COPYTHECONTEXT("sub_mb_type", u1_sub_mb_mode);
            /* check if Motion compensation is done below 8x8 */
            if(u1_sub_mb_mode != P_L0_8x8)
            {
                u1_no_submb_part_size_lt8x8_flag = 0;
            }
        }
        {
            UWORD8 u1_sub_mb_mode;
            u1_sub_mb_mode = ih264d_parse_submb_type_cabac(
                            0, ps_cab_env, ps_bitstrm,
                            ps_dec->p_sub_mb_type_t);
            if(u1_sub_mb_mode > 3)
                return ERROR_SUB_MB_TYPE;

            u4_sub_mb_pack = (u4_sub_mb_pack << 8) | u1_sub_mb_mode;
            /* Storing collocated information */
            *pu1_col_info++ = u1_colz | ((UWORD8)(u1_sub_mb_mode << 4));
            COPYTHECONTEXT("sub_mb_type", u1_sub_mb_mode);
            /* check if Motion compensation is done below 8x8 */
            if(u1_sub_mb_mode != P_L0_8x8)
            {
                u1_no_submb_part_size_lt8x8_flag = 0;
            }
        }
        u1_num_mb_part = 4;
    }
    else
    {
        u1_num_mb_part = pu1_num_mb_part[u1_mb_type];
        /* Storing collocated Mb and SubMb mode information */
        *pu1_col_info++ = (u1_mb_type << 6);
        if(u1_mb_type)
            *pu1_col_info++ = (u1_mb_type << 6);
    }
    /* Decoding reference index 0: For simple profile the following   */
    /* conditions are always true (mb_field_decoding_flag == 0);      */
    /* (MbPartPredMode != PredL1)                                     */
    {
        WORD8 * pi1_top_ref_idx_ctx_inc_arr = p_curr_ctxt->i1_ref_idx;
        WORD8 * pi1_left_ref_idx_ctxt_inc = ps_dec->pi1_left_ref_idx_ctxt_inc;
        UWORD8 u1_mbaff = ps_dec->ps_cur_slice->u1_mbaff_frame_flag;
        UWORD8 uc_field = ps_cur_mb_info->u1_mb_field_decodingflag;
        UWORD8 uc_num_ref_idx_l0_active_minus1 =
                        (ps_dec->ps_cur_slice->u1_num_ref_idx_lx_active[0]
                                        << (u1_mbaff & uc_field)) - 1;

        if((uc_num_ref_idx_l0_active_minus1 > 0) & (u1_mb_type != PRED_8x8R0))
        {
            /* force the routine to decode ref idx for each partition */
            *((UWORD32 *)pi1_ref_idx) = 0x01010101;
            ret = ih264d_parse_ref_idx_cabac(u1_num_mb_part, 0,
                                             uc_num_ref_idx_l0_active_minus1,
                                             u1_mb_mc_mode, pi1_ref_idx,
                                             pi1_left_ref_idx_ctxt_inc,
                                             pi1_top_ref_idx_ctx_inc_arr, ps_cab_env,
                                             ps_bitstrm, ps_dec->p_ref_idx_t);
            if(ret != OK)
                return ret;
        }
        else
        {
            /* When there exists only a single frame to predict from */
            pi1_left_ref_idx_ctxt_inc[0] = 0;
            pi1_left_ref_idx_ctxt_inc[1] = 0;
            pi1_top_ref_idx_ctx_inc_arr[0] = 0;
            pi1_top_ref_idx_ctx_inc_arr[1] = 0;
            *((UWORD32 *)pi1_ref_idx) = 0;
        }
    }

    {
        UWORD8 u1_p_idx, uc_i;
        parse_part_params_t * ps_part = ps_dec->ps_part;
        UWORD8 u1_sub_mb_mode, u1_num_subpart, u1_mb_part_width, u1_mb_part_height;
        UWORD8 u1_sub_mb_num;
        const UWORD8 * pu1_top_left_sub_mb_indx;
        mv_pred_t *ps_mv_start = ps_dec->ps_mv_cur + (u1_mb_num << 4);
        UWORD16 u2_sub_mb_num_pack = 0x028A;

        /* Loading the table pointers */
        const UWORD8 * pu1_mb_partw = (const UWORD8 *)gau1_ih264d_mb_partw;
        const UWORD8 * pu1_mb_parth = (const UWORD8 *)gau1_ih264d_mb_parth;
        const UWORD8 * pu1_sub_mb_indx_mod =
                        (const UWORD8 *)(gau1_ih264d_submb_indx_mod)
                                        + (uc_sub_mb * 6);
        const UWORD8 * pu1_sub_mb_partw = (const UWORD8 *)gau1_ih264d_submb_partw;
        const UWORD8 * pu1_sub_mb_parth = (const UWORD8 *)gau1_ih264d_submb_parth;
        const UWORD8 * pu1_num_sub_mb_part =
                        (const UWORD8 *)gau1_ih264d_num_submb_part;

        /*********************************************************/
        /* default initialisations for condition (uc_sub_mb == 0) */
        /* i.e. all are subpartitions of 8x8                     */
        /*********************************************************/
        u1_sub_mb_mode = 0;
        u1_num_subpart = 1;
        u1_mb_part_width = pu1_mb_partw[u1_mb_type];
        u1_mb_part_height = pu1_mb_parth[u1_mb_type];
        pu1_top_left_sub_mb_indx = pu1_sub_mb_indx_mod + (u1_mb_type << 1);
        u1_sub_mb_num = 0;

        /* Loop on number of partitions */
        for(uc_i = 0, u1_p_idx = 0; uc_i < u1_num_mb_part; uc_i++)
        {
            UWORD8 uc_j;
            if(uc_sub_mb)
            {
                u1_sub_mb_mode = u4_sub_mb_pack >> 24;
                u1_num_subpart = pu1_num_sub_mb_part[u1_sub_mb_mode];
                u1_mb_part_width = pu1_sub_mb_partw[u1_sub_mb_mode];
                u1_mb_part_height = pu1_sub_mb_parth[u1_sub_mb_mode];
                pu1_top_left_sub_mb_indx = pu1_sub_mb_indx_mod + (u1_sub_mb_mode << 1);
                u1_sub_mb_num = u2_sub_mb_num_pack >> 12;
                u4_sub_mb_pack <<= 8;
                u2_sub_mb_num_pack <<= 4;
            }
            /* Loop on Number of sub-partitions */
            for(uc_j = 0; uc_j < u1_num_subpart; uc_j++, pu1_top_left_sub_mb_indx++)
            {
                mv_pred_t * ps_mv;

                u1_sub_mb_num += *pu1_top_left_sub_mb_indx;
                ps_mv = ps_mv_start + u1_sub_mb_num;

                /* Storing Info for partitions */
                ps_part->u1_is_direct = PART_NOT_DIRECT;
                ps_part->u1_sub_mb_num = u1_sub_mb_num;
                ps_part->u1_partheight = u1_mb_part_height;
                ps_part->u1_partwidth = u1_mb_part_width;

                /* Increment partition Index */
                u1_p_idx++;
                ps_part++;

                ih264d_get_mvd_cabac(u1_sub_mb_num, 0, u1_mb_part_width,
                                     u1_mb_part_height, 1, ps_dec, ps_mv);
            }
        }
        ps_parse_mb_data->u1_num_part = u1_p_idx;
        ps_dec->ps_part = ps_part;
    }
    {
        UWORD8 u1_cbp;

        /* Read the Coded block pattern */
        u1_cbp = (WORD8)ih264d_parse_ctx_cbp_cabac(ps_dec);
        COPYTHECONTEXT("coded_block_pattern", u1_cbp);
        ps_cur_mb_info->u1_cbp = u1_cbp;
        p_curr_ctxt->u1_cbp = u1_cbp;
        p_curr_ctxt->u1_intra_chroma_pred_mode = 0;
        p_curr_ctxt->u1_yuv_dc_csbp &= 0xFE;
        ps_dec->pu1_left_yuv_dc_csbp[0] &= 0x6;

        if(u1_cbp > 47)
            return ERROR_CBP;

        ps_cur_mb_info->u1_tran_form8x8 = 0;
        ps_cur_mb_info->ps_curmb->u1_tran_form8x8 = 0;

        /* Read the transform8x8 u4_flag if present */
        if((ps_dec->s_high_profile.u1_transform8x8_present) && (u1_cbp & 0xf)
                        && u1_no_submb_part_size_lt8x8_flag)
        {
            ps_cur_mb_info->u1_tran_form8x8 = ih264d_parse_transform8x8flag_cabac(
                            ps_dec, ps_cur_mb_info);
            COPYTHECONTEXT("transform_size_8x8_flag", ps_cur_mb_info->u1_tran_form8x8);
            p_curr_ctxt->u1_transform8x8_ctxt = ps_cur_mb_info->u1_tran_form8x8;
            ps_cur_mb_info->ps_curmb->u1_tran_form8x8 = ps_cur_mb_info->u1_tran_form8x8;

        }
        else
        {
            p_curr_ctxt->u1_transform8x8_ctxt = 0;
        }

        /* Read mb_qp_delta */
        if(u1_cbp)
        {
            WORD8 c_temp;
            ret = ih264d_parse_mb_qp_delta_cabac(ps_dec, &c_temp);
            if(ret != OK)
                return ret;
            COPYTHECONTEXT("mb_qp_delta", c_temp);
            if(c_temp != 0)
            {
                ret = ih264d_update_qp(ps_dec, c_temp);
                if(ret != OK)
                    return ret;
            }
        }
        else
            ps_dec->i1_prev_mb_qp_delta = 0;



        ih264d_parse_residual4x4_cabac(ps_dec, ps_cur_mb_info, 0);
        if(EXCEED_OFFSET(ps_dec->ps_bitstrm))
            return ERROR_EOB_TERMINATE_T;
    }
    return OK;
}

/*!
 **************************************************************************
 * \if Function name : parsePSliceData \endif
 *
 * \brief
 *    This function parses CAVLC syntax of N MB's of a P slice.
 *    1. After parsing syntax of N MB's, for those N MB's (less than N, incase
 *       of end of slice or end of row), MB is decoded. This process is carried
 *       for one complete MB row or till end of slice.
 *    2. Bottom one row of current MB is copied to IntraPredLine buffers.
 *       IntraPredLine buffers are used for Intra prediction of next row.
 *    3. Current MB row along with previous 4 rows of Luma (and 2 of Chroma) are
 *       deblocked.
 *    4. 4 rows (2 for Chroma) previous row and 12 rows (6 for Chroma) are
 *       DMA'ed to picture buffers.
 *
 * \return
 *    0 on Success and Error code otherwise
 **************************************************************************
 */

/*!
 **************************************************************************
 * \if Function name : ih264d_update_nnz_for_skipmb \endif
 *
 * \brief
 *
 * \return
 *    None
 *
 **************************************************************************
 */
void ih264d_update_nnz_for_skipmb(dec_struct_t * ps_dec,
                                  dec_mb_info_t * ps_cur_mb_info,
                                  UWORD8 u1_entrpy)
{
    UWORD32 *pu4_buf;
    UWORD8 *pu1_buf;
    UNUSED(u1_entrpy);
    pu1_buf = ps_dec->pu1_left_nnz_y;
    pu4_buf = (UWORD32 *)pu1_buf;
    *pu4_buf = 0;
    pu1_buf = ps_dec->pu1_left_nnz_uv;
    pu4_buf = (UWORD32 *)pu1_buf;
    *pu4_buf = 0;
    pu1_buf = ps_cur_mb_info->ps_curmb->pu1_nnz_y;
    pu4_buf = (UWORD32 *)pu1_buf;
    *pu4_buf = 0;
    pu1_buf = ps_cur_mb_info->ps_curmb->pu1_nnz_uv;
    pu4_buf = (UWORD32 *)pu1_buf;
    *pu4_buf = 0;
    ps_cur_mb_info->ps_curmb->u2_luma_csbp = 0;
    ps_cur_mb_info->u2_luma_csbp = 0;
    ps_cur_mb_info->u2_chroma_csbp = 0;
}



/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_parse_inter_slice_data_cabac                             */
/*                                                                           */
/*  Description   : This function parses cabac syntax of a inter slice on    */
/*                  N MB basis.                                              */
/*                                                                           */
/*  Inputs        : ps_dec                                                   */
/*                  sliceparams                                              */
/*                  firstMbInSlice                                           */
/*                                                                           */
/*  Processing    : 1. After parsing syntax for N MBs those N MBs are        */
/*                     decoded till the end of slice.                        */
/*                  2. MV prediction and DMA happens on a N/2 MB basis.      */
/*                                                                           */
/*  Returns       : 0                                                        */
/*                                                                           */
/*  Issues        : <List any issues or problems with this function>         */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         13 07 2002   Jay             Draft                                */
/*                                                                           */
/*****************************************************************************/
WORD32 ih264d_parse_inter_slice_data_cabac(dec_struct_t * ps_dec,
                                           dec_slice_params_t * ps_slice,
                                           UWORD16 u2_first_mb_in_slice)
{
    UWORD32 uc_more_data_flag;
    WORD32 i2_cur_mb_addr;
    UWORD32 u1_num_mbs, u1_num_mbsNby2, u1_mb_idx;
    UWORD32 u1_mbaff;
    UWORD32 u1_num_mbs_next, u1_end_of_row;
    const UWORD16 i2_pic_wdin_mbs = ps_dec->u2_frm_wd_in_mbs;
    UWORD32 u1_slice_end = 0;
    UWORD32 u1_tfr_n_mb = 0;
    UWORD32 u1_decode_nmb = 0;


    deblk_mb_t *ps_cur_deblk_mb;
    dec_mb_info_t *ps_cur_mb_info;

    parse_pmbarams_t *ps_parse_mb_data = ps_dec->ps_parse_mb_data;
    UWORD32 u1_inter_mb_skip_type;
    UWORD32 u1_inter_mb_type;
    UWORD32 u1_deblk_mb_type;
    UWORD32 u1_mb_threshold;
    dec_bit_stream_t * const ps_bitstrm = ps_dec->ps_bitstrm;
    WORD32 ret = OK;

    /******************************************************/
    /* Initialisations specific to B or P slice           */
    /******************************************************/
    if(ps_slice->u1_slice_type == P_SLICE)
    {
        u1_inter_mb_skip_type = CAB_P_SKIP;
        u1_inter_mb_type = P_MB;
        u1_deblk_mb_type = D_INTER_MB;
        u1_mb_threshold = 5;
    }
    else // B_SLICE
    {
        u1_inter_mb_skip_type = CAB_B_SKIP;
        u1_inter_mb_type = B_MB;
        u1_deblk_mb_type = D_B_SLICE;
        u1_mb_threshold = 23;
    }

    /******************************************************/
    /* Slice Level Initialisations                        */
    /******************************************************/
    i2_cur_mb_addr = u2_first_mb_in_slice;
    ps_dec->u1_qp = ps_slice->u1_slice_qp;
    ih264d_update_qp(ps_dec, 0);
    u1_mb_idx = ps_dec->u1_mb_idx;
    u1_num_mbs = u1_mb_idx;
    u1_num_mbsNby2 = 0;
    u1_mbaff = ps_slice->u1_mbaff_frame_flag;
    i2_cur_mb_addr = u2_first_mb_in_slice << u1_mbaff;
    uc_more_data_flag = 1;

    /* Initialisations specific to cabac */
    if(ps_bitstrm->u4_ofst & 0x07)
    {
        ps_bitstrm->u4_ofst += 8;
        ps_bitstrm->u4_ofst &= 0xFFFFFFF8;
    }

    ret = ih264d_init_cabac_dec_envirnoment(&(ps_dec->s_cab_dec_env), ps_bitstrm);
    if(ret != OK)
        return ret;

    ps_dec->i1_prev_mb_qp_delta = 0;

    while(!u1_slice_end)
    {
        UWORD8 u1_mb_type;
        UWORD32 u4_mb_skip;

        ps_dec->pv_prev_mb_parse_tu_coeff_data = ps_dec->pv_parse_tu_coeff_data;

        if(i2_cur_mb_addr > ps_dec->ps_cur_sps->u2_max_mb_addr)
        {
            break;
        }

        ps_cur_mb_info = ps_dec->ps_nmb_info + u1_num_mbs;
        ps_dec->u4_num_mbs_cur_nmb = u1_num_mbs;

        ps_cur_mb_info->u1_Mux = 0;
        ps_dec->u4_num_pmbair = (u1_num_mbs >> u1_mbaff);
        ps_cur_deblk_mb = ps_dec->ps_deblk_mbn + u1_num_mbs;

        ps_cur_mb_info->u1_end_of_slice = 0;

        /* Storing Default partition info */
        ps_parse_mb_data->u1_num_part = 1;
        ps_parse_mb_data->u1_isI_mb = 0;

        /***************************************************************/
        /* Get the required information for decoding of MB             */
        /* mb_x, mb_y , neighbour availablity,                         */
        /***************************************************************/
        u4_mb_skip = ps_dec->pf_get_mb_info(ps_dec, i2_cur_mb_addr, ps_cur_mb_info, 1);

        /*********************************************************************/
        /* initialize u1_tran_form8x8 to zero to aviod uninitialized accesses */
        /*********************************************************************/
        ps_cur_mb_info->u1_tran_form8x8 = 0;
        ps_cur_mb_info->ps_curmb->u1_tran_form8x8 = 0;

        /***************************************************************/
        /* Set the deblocking parameters for this MB                   */
        /***************************************************************/
        if(ps_dec->u4_app_disable_deblk_frm == 0)
            ih264d_set_deblocking_parameters(ps_cur_deblk_mb, ps_slice,
                                             ps_dec->u1_mb_ngbr_availablity,
                                             ps_dec->u1_cur_mb_fld_dec_flag);

        if(u4_mb_skip)
        {

            /* Set appropriate flags in ps_cur_mb_info and ps_dec */
            memset(ps_dec->ps_curr_ctxt_mb_info, 0, sizeof(ctxt_inc_mb_info_t));
            ps_dec->ps_curr_ctxt_mb_info->u1_mb_type = u1_inter_mb_skip_type;

            MEMSET_16BYTES(&ps_dec->pu1_left_mv_ctxt_inc[0][0], 0);

            *((UWORD32 *)ps_dec->pi1_left_ref_idx_ctxt_inc) = 0;
            *(ps_dec->pu1_left_yuv_dc_csbp) = 0;

            ps_dec->i1_prev_mb_qp_delta = 0;
            ps_cur_mb_info->u1_mb_type = MB_SKIP;
            ps_cur_mb_info->u1_cbp = 0;

            {
                /* Storing Skip partition info */
                parse_part_params_t *ps_part_info = ps_dec->ps_part;
                ps_part_info->u1_is_direct = PART_DIRECT_16x16;
                ps_part_info->u1_sub_mb_num = 0;
                ps_dec->ps_part++;
            }

            /* Update Nnzs */
            ih264d_update_nnz_for_skipmb(ps_dec, ps_cur_mb_info, CABAC);

            ps_cur_mb_info->ps_curmb->u1_mb_type = u1_inter_mb_type;
            ps_cur_deblk_mb->u1_mb_type |= u1_deblk_mb_type;
            ps_cur_deblk_mb->u1_mb_qp = ps_dec->u1_qp;

        }
        else
        {

            /* Macroblock Layer Begins */
            /* Decode the u1_mb_type */
            u1_mb_type = ih264d_parse_mb_type_cabac(ps_dec);
            ps_cur_mb_info->u1_mb_type = u1_mb_type;
            if(u1_mb_type > (25 + u1_mb_threshold))
                return ERROR_MB_TYPE;

            /* Parse Macroblock Data */
            if(u1_mb_type < u1_mb_threshold)
            {
                ps_cur_mb_info->ps_curmb->u1_mb_type = u1_inter_mb_type;
                *(ps_dec->pu1_left_yuv_dc_csbp) &= 0x6;

                ret = ps_dec->pf_parse_inter_mb(ps_dec, ps_cur_mb_info, u1_num_mbs,
                                          u1_num_mbsNby2);
                if(ret != OK)
                    return ret;
                ps_cur_deblk_mb->u1_mb_qp = ps_dec->u1_qp;
                ps_cur_deblk_mb->u1_mb_type |= u1_deblk_mb_type;
            }
            else
            {
                /* Storing Intra partition info */
                ps_parse_mb_data->u1_num_part = 0;
                ps_parse_mb_data->u1_isI_mb = 1;

                if((25 + u1_mb_threshold) == u1_mb_type)
                {
                    /* I_PCM_MB */
                    ps_cur_mb_info->ps_curmb->u1_mb_type = I_PCM_MB;
                    ret = ih264d_parse_ipcm_mb(ps_dec, ps_cur_mb_info, u1_num_mbs);
                    if(ret != OK)
                        return ret;
                    ps_cur_deblk_mb->u1_mb_qp = 0;
                }
                else
                {
                    if(u1_mb_type == u1_mb_threshold)
                        ps_cur_mb_info->ps_curmb->u1_mb_type = I_4x4_MB;
                    else
                        ps_cur_mb_info->ps_curmb->u1_mb_type = I_16x16_MB;

                    ret = ih264d_parse_imb_cabac(
                                    ps_dec, ps_cur_mb_info,
                                    (UWORD8)(u1_mb_type - u1_mb_threshold));
                    if(ret != OK)
                        return ret;
                    ps_cur_deblk_mb->u1_mb_qp = ps_dec->u1_qp;
                }
                ps_cur_deblk_mb->u1_mb_type |= D_INTRA_MB;

            }

        }

        if(u1_mbaff)
        {
            ih264d_update_mbaff_left_nnz(ps_dec, ps_cur_mb_info);
        }


        if(ps_cur_mb_info->u1_topmb && u1_mbaff)
            uc_more_data_flag = 1;
        else
        {
            uc_more_data_flag = ih264d_decode_terminate(&ps_dec->s_cab_dec_env,
                                                      ps_bitstrm);
            uc_more_data_flag = !uc_more_data_flag;
            COPYTHECONTEXT("Decode Sliceterm",!uc_more_data_flag);
        }

        if(u1_mbaff)
        {
            if(!uc_more_data_flag && (0 == (i2_cur_mb_addr & 1)))
            {
                return ERROR_EOB_FLUSHBITS_T;
            }
        }
        /* Next macroblock information */
        i2_cur_mb_addr++;
        u1_num_mbs++;
        u1_num_mbsNby2++;
        ps_parse_mb_data++;

        /****************************************************************/
        /* Check for End Of Row and other flags that determine when to  */
        /* do DMA setup for N/2-Mb, Decode for N-Mb, and Transfer for   */
        /* N-Mb                                                         */
        /****************************************************************/
        u1_num_mbs_next = i2_pic_wdin_mbs - ps_dec->u2_mbx - 1;
        u1_end_of_row = (!u1_num_mbs_next) && (!(u1_mbaff && (u1_num_mbs & 0x01)));
        u1_slice_end = !uc_more_data_flag;
        u1_tfr_n_mb = (u1_num_mbs == ps_dec->u1_recon_mb_grp) || u1_end_of_row
                        || u1_slice_end;
        u1_decode_nmb = u1_tfr_n_mb || u1_slice_end;
        ps_cur_mb_info->u1_end_of_slice = u1_slice_end;
        /*u1_dma_nby2mb   = u1_decode_nmb ||
         (u1_num_mbsNby2 == ps_dec->u1_recon_mb_grp_pair);*/

//if(u1_dma_nby2mb)
        if(u1_decode_nmb)
        {

            ps_dec->pf_mvpred_ref_tfr_nby2mb(ps_dec, u1_mb_idx, u1_num_mbs);
            u1_num_mbsNby2 = 0;

            {
                ps_parse_mb_data = ps_dec->ps_parse_mb_data;
                ps_dec->ps_part = ps_dec->ps_parse_part_params;
            }
        }

        /*H264_DEC_DEBUG_PRINT("Pic: %d Mb_X=%d Mb_Y=%d",
         ps_slice->i4_poc >> ps_slice->u1_field_pic_flag,
         ps_dec->u2_mbx,ps_dec->u2_mby + (1 - ps_cur_mb_info->u1_topmb));
         H264_DEC_DEBUG_PRINT("u1_decode_nmb: %d, u1_num_mbs: %d", u1_decode_nmb, u1_num_mbs);*/
        if(u1_decode_nmb)
        {

            if(ps_dec->u1_separate_parse)
            {
                ih264d_parse_tfr_nmb(ps_dec, u1_mb_idx, u1_num_mbs,
                                     u1_num_mbs_next, u1_tfr_n_mb, u1_end_of_row);
                ps_dec->ps_nmb_info +=  u1_num_mbs;
            }
            else
            {
                ih264d_decode_recon_tfr_nmb(ps_dec, u1_mb_idx, u1_num_mbs,
                                            u1_num_mbs_next, u1_tfr_n_mb,
                                            u1_end_of_row);
            }
            ps_dec->u2_total_mbs_coded += u1_num_mbs;
            if(u1_tfr_n_mb)
                u1_num_mbs = 0;
            u1_mb_idx = u1_num_mbs;
            ps_dec->u1_mb_idx = u1_num_mbs;

        }
    }


    ps_dec->u4_num_mbs_cur_nmb = 0;
    ps_dec->ps_cur_slice->u4_mbs_in_slice = i2_cur_mb_addr

                        - (u2_first_mb_in_slice << u1_mbaff);

    return ret;
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_parse_inter_slice_data_cavlc                             */
/*                                                                           */
/*  Description   : This function parses cavlc syntax of a inter slice on    */
/*                  N MB basis.                                              */
/*                                                                           */
/*  Inputs        : ps_dec                                                   */
/*                  sliceparams                                              */
/*                  firstMbInSlice                                           */
/*                                                                           */
/*  Processing    : 1. After parsing syntax for N MBs those N MBs are        */
/*                     decoded till the end of slice.                        */
/*                  2. MV prediction and DMA happens on a N/2 MB basis.      */
/*                                                                           */
/*  Returns       : 0                                                        */
/*                                                                           */
/*  Issues        : <List any issues or problems with this function>         */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         13 07 2002   Jay             Draft                                */
/*                                                                           */
/*****************************************************************************/

WORD32 ih264d_parse_inter_slice_data_cavlc(dec_struct_t * ps_dec,
                                           dec_slice_params_t * ps_slice,
                                           UWORD16 u2_first_mb_in_slice)
{
    UWORD32 uc_more_data_flag;
    WORD32 i2_cur_mb_addr;
    UWORD32 u1_num_mbs, u1_num_mbsNby2, u1_mb_idx;
    UWORD32 i2_mb_skip_run;
    UWORD32 u1_read_mb_type;

    UWORD32 u1_mbaff;
    UWORD32 u1_num_mbs_next, u1_end_of_row;
    const UWORD32 i2_pic_wdin_mbs = ps_dec->u2_frm_wd_in_mbs;
    UWORD32 u1_slice_end = 0;
    UWORD32 u1_tfr_n_mb = 0;
    UWORD32 u1_decode_nmb = 0;

    dec_bit_stream_t * const ps_bitstrm = ps_dec->ps_bitstrm;
    UWORD32 *pu4_bitstrm_buf = ps_bitstrm->pu4_buffer;
    UWORD32 *pu4_bitstrm_ofst = &ps_bitstrm->u4_ofst;
    deblk_mb_t *ps_cur_deblk_mb;
    dec_mb_info_t *ps_cur_mb_info;
    parse_pmbarams_t *ps_parse_mb_data = ps_dec->ps_parse_mb_data;
    UWORD32 u1_inter_mb_type;
    UWORD32 u1_deblk_mb_type;
    UWORD32 u1_mb_threshold;
    WORD32 ret = OK;

    /******************************************************/
    /* Initialisations specific to B or P slice           */
    /******************************************************/

    if(ps_slice->u1_slice_type == P_SLICE)
    {
        u1_inter_mb_type = P_MB;
        u1_deblk_mb_type = D_INTER_MB;
        u1_mb_threshold = 5;
    }
    else // B_SLICE
    {
        u1_inter_mb_type = B_MB;
        u1_deblk_mb_type = D_B_SLICE;
        u1_mb_threshold = 23;
    }
    /******************************************************/
    /* Slice Level Initialisations                        */
    /******************************************************/
    ps_dec->u1_qp = ps_slice->u1_slice_qp;
    ih264d_update_qp(ps_dec, 0);
    u1_mb_idx = ps_dec->u1_mb_idx;
    u1_num_mbs = u1_mb_idx;

    u1_num_mbsNby2 = 0;
    u1_mbaff = ps_slice->u1_mbaff_frame_flag;
    i2_cur_mb_addr = u2_first_mb_in_slice << u1_mbaff;
    i2_mb_skip_run = 0;
    uc_more_data_flag = 1;
    u1_read_mb_type = 0;

    while(!u1_slice_end)
    {
        UWORD8 u1_mb_type;

        ps_dec->pv_prev_mb_parse_tu_coeff_data = ps_dec->pv_parse_tu_coeff_data;

        if(i2_cur_mb_addr > ps_dec->ps_cur_sps->u2_max_mb_addr)
        {
            break;
        }


        ps_cur_mb_info = ps_dec->ps_nmb_info + u1_num_mbs;
        ps_dec->u4_num_mbs_cur_nmb = u1_num_mbs;

        ps_cur_mb_info->u1_Mux = 0;
        ps_dec->u4_num_pmbair = (u1_num_mbs >> u1_mbaff);
        ps_cur_deblk_mb = ps_dec->ps_deblk_mbn + u1_num_mbs;

        ps_cur_mb_info->u1_end_of_slice = 0;

        /* Storing Default partition info */
        ps_parse_mb_data->u1_num_part = 1;
        ps_parse_mb_data->u1_isI_mb = 0;

        if((!i2_mb_skip_run) && (!u1_read_mb_type))
        {

            //Inlined ih264d_uev
            UWORD32 u4_bitstream_offset = *pu4_bitstrm_ofst;
            UWORD32 u4_word, u4_ldz;

            /***************************************************************/
            /* Find leading zeros in next 32 bits                          */
            /***************************************************************/
            NEXTBITS_32(u4_word, u4_bitstream_offset, pu4_bitstrm_buf);

            u4_ldz = CLZ(u4_word);

            /* Flush the ps_bitstrm */
            u4_bitstream_offset += (u4_ldz + 1);
            /* Read the suffix from the ps_bitstrm */
            u4_word = 0;
            if(u4_ldz)
            {
                GETBITS(u4_word, u4_bitstream_offset, pu4_bitstrm_buf,
                        u4_ldz);
            }
            *pu4_bitstrm_ofst = u4_bitstream_offset;
            i2_mb_skip_run = ((1 << u4_ldz) + u4_word - 1);
            //Inlined ih264d_uev
            COPYTHECONTEXT("mb_skip_run", i2_mb_skip_run);
            uc_more_data_flag = MORE_RBSP_DATA(ps_bitstrm);
            u1_read_mb_type = uc_more_data_flag;
        }

        /***************************************************************/
        /* Get the required information for decoding of MB                  */
        /* mb_x, mb_y , neighbour availablity,                              */
        /***************************************************************/
        ps_dec->pf_get_mb_info(ps_dec, i2_cur_mb_addr, ps_cur_mb_info, i2_mb_skip_run);

        /***************************************************************/
        /* Set the deblocking parameters for this MB                   */
        /***************************************************************/
        if(ps_dec->u4_app_disable_deblk_frm == 0)
            ih264d_set_deblocking_parameters(ps_cur_deblk_mb, ps_slice,
                                             ps_dec->u1_mb_ngbr_availablity,
                                             ps_dec->u1_cur_mb_fld_dec_flag);

        if(i2_mb_skip_run)
        {
            /* Set appropriate flags in ps_cur_mb_info and ps_dec */
            ps_dec->i1_prev_mb_qp_delta = 0;
            ps_dec->u1_sub_mb_num = 0;
            ps_cur_mb_info->u1_mb_type = MB_SKIP;
            ps_cur_mb_info->u1_mb_mc_mode = PRED_16x16;
            ps_cur_mb_info->u1_cbp = 0;

            {
                /* Storing Skip partition info */
                parse_part_params_t *ps_part_info = ps_dec->ps_part;
                ps_part_info->u1_is_direct = PART_DIRECT_16x16;
                ps_part_info->u1_sub_mb_num = 0;
                ps_dec->ps_part++;
            }

            /* Update Nnzs */
            ih264d_update_nnz_for_skipmb(ps_dec, ps_cur_mb_info, CAVLC);

            ps_cur_mb_info->ps_curmb->u1_mb_type = u1_inter_mb_type;
            ps_cur_deblk_mb->u1_mb_type |= u1_deblk_mb_type;

            i2_mb_skip_run--;
        }
        else
        {
            u1_read_mb_type = 0;
            /**************************************************************/
            /* Macroblock Layer Begins, Decode the u1_mb_type                */
            /**************************************************************/
            {
                UWORD32 u4_bitstream_offset = *pu4_bitstrm_ofst;
                UWORD32 u4_word, u4_ldz, u4_temp;


                //Inlined ih264d_uev
                /***************************************************************/
                /* Find leading zeros in next 32 bits                          */
                /***************************************************************/
                NEXTBITS_32(u4_word, u4_bitstream_offset, pu4_bitstrm_buf);
                u4_ldz = CLZ(u4_word);
                /* Flush the ps_bitstrm */
                u4_bitstream_offset += (u4_ldz + 1);
                /* Read the suffix from the ps_bitstrm */
                u4_word = 0;
                if(u4_ldz)
                    GETBITS(u4_word, u4_bitstream_offset, pu4_bitstrm_buf,
                            u4_ldz);
                *pu4_bitstrm_ofst = u4_bitstream_offset;
                u4_temp = ((1 << u4_ldz) + u4_word - 1);
                //Inlined ih264d_uev
                if(u4_temp > (UWORD32)(25 + u1_mb_threshold))
                    return ERROR_MB_TYPE;
                u1_mb_type = u4_temp;
                COPYTHECONTEXT("u1_mb_type", u1_mb_type);
            }
            ps_cur_mb_info->u1_mb_type = u1_mb_type;

            /**************************************************************/
            /* Parse Macroblock data                                      */
            /**************************************************************/
            if(u1_mb_type < u1_mb_threshold)
            {
                ps_cur_mb_info->ps_curmb->u1_mb_type = u1_inter_mb_type;

                ret = ps_dec->pf_parse_inter_mb(ps_dec, ps_cur_mb_info, u1_num_mbs,
                                          u1_num_mbsNby2);
                if(ret != OK)
                    return ret;
                ps_cur_deblk_mb->u1_mb_type |= u1_deblk_mb_type;
            }
            else
            {
                /* Storing Intra partition info */
                ps_parse_mb_data->u1_num_part = 0;
                ps_parse_mb_data->u1_isI_mb = 1;

                if((25 + u1_mb_threshold) == u1_mb_type)
                {
                    /* I_PCM_MB */
                    ps_cur_mb_info->ps_curmb->u1_mb_type = I_PCM_MB;
                    ret = ih264d_parse_ipcm_mb(ps_dec, ps_cur_mb_info, u1_num_mbs);
                    if(ret != OK)
                         return ret;
                    ps_dec->u1_qp = 0;
                }
                else
                {
                    ret = ih264d_parse_imb_cavlc(
                                    ps_dec, ps_cur_mb_info, u1_num_mbs,
                                    (UWORD8)(u1_mb_type - u1_mb_threshold));
                    if(ret != OK)
                        return ret;
                }

                ps_cur_deblk_mb->u1_mb_type |= D_INTRA_MB;
            }
            uc_more_data_flag = MORE_RBSP_DATA(ps_bitstrm);
        }
        ps_cur_deblk_mb->u1_mb_qp = ps_dec->u1_qp;

        if(u1_mbaff)
        {
            ih264d_update_mbaff_left_nnz(ps_dec, ps_cur_mb_info);
            if(!uc_more_data_flag && !i2_mb_skip_run && (0 == (i2_cur_mb_addr & 1)))
            {
                return ERROR_EOB_FLUSHBITS_T;
            }
        }
        /**************************************************************/
        /* Get next Macroblock address                                */
        /**************************************************************/
        i2_cur_mb_addr++;

        u1_num_mbs++;
        u1_num_mbsNby2++;
        ps_parse_mb_data++;

        /****************************************************************/
        /* Check for End Of Row and other flags that determine when to  */
        /* do DMA setup for N/2-Mb, Decode for N-Mb, and Transfer for   */
        /* N-Mb                                                         */
        /****************************************************************/
        u1_num_mbs_next = i2_pic_wdin_mbs - ps_dec->u2_mbx - 1;
        u1_end_of_row = (!u1_num_mbs_next) && (!(u1_mbaff && (u1_num_mbs & 0x01)));
        u1_slice_end = (!(uc_more_data_flag || i2_mb_skip_run));
        u1_tfr_n_mb = (u1_num_mbs == ps_dec->u1_recon_mb_grp) || u1_end_of_row
                        || u1_slice_end;
        u1_decode_nmb = u1_tfr_n_mb || u1_slice_end;
        ps_cur_mb_info->u1_end_of_slice = u1_slice_end;

        /*u1_dma_nby2mb   = u1_decode_nmb ||
         (u1_num_mbsNby2 == ps_dec->u1_recon_mb_grp_pair);*/

//if(u1_dma_nby2mb)
        if(u1_decode_nmb)
        {
            ps_dec->pf_mvpred_ref_tfr_nby2mb(ps_dec, u1_mb_idx, u1_num_mbs);
            u1_num_mbsNby2 = 0;

            {
                ps_parse_mb_data = ps_dec->ps_parse_mb_data;
                ps_dec->ps_part = ps_dec->ps_parse_part_params;
            }
        }

        /*H264_DEC_DEBUG_PRINT("Pic: %d Mb_X=%d Mb_Y=%d",
         ps_slice->i4_poc >> ps_slice->u1_field_pic_flag,
         ps_dec->u2_mbx,ps_dec->u2_mby + (1 - ps_cur_mb_info->u1_topmb));
         H264_DEC_DEBUG_PRINT("u1_decode_nmb: %d", u1_decode_nmb);*/
        if(u1_decode_nmb)
        {



            if(ps_dec->u1_separate_parse)
            {
                ih264d_parse_tfr_nmb(ps_dec, u1_mb_idx, u1_num_mbs,
                                     u1_num_mbs_next, u1_tfr_n_mb, u1_end_of_row);
                ps_dec->ps_nmb_info +=  u1_num_mbs;
            }
            else
            {
                ih264d_decode_recon_tfr_nmb(ps_dec, u1_mb_idx, u1_num_mbs,
                                            u1_num_mbs_next, u1_tfr_n_mb,
                                            u1_end_of_row);
            }
            ps_dec->u2_total_mbs_coded += u1_num_mbs;
            if(u1_tfr_n_mb)
                u1_num_mbs = 0;
            u1_mb_idx = u1_num_mbs;
            ps_dec->u1_mb_idx = u1_num_mbs;

        }
//ps_dec->ps_pred++;
    }

    ps_dec->u4_num_mbs_cur_nmb = 0;
    ps_dec->ps_cur_slice->u4_mbs_in_slice = i2_cur_mb_addr
                        - (u2_first_mb_in_slice << u1_mbaff);


    return ret;
}

WORD32 ih264d_mark_err_slice_skip(dec_struct_t * ps_dec,
                                WORD32 num_mb_skip,
                                UWORD8 u1_is_idr_slice,
                                UWORD16 u2_frame_num,
                                pocstruct_t *ps_cur_poc,
                                WORD32 prev_slice_err)
{
    WORD32 i2_cur_mb_addr;
    UWORD32 u1_num_mbs, u1_num_mbsNby2;
    UWORD32 u1_mb_idx = ps_dec->u1_mb_idx;
    UWORD32 i2_mb_skip_run;

    UWORD32 u1_num_mbs_next, u1_end_of_row;
    const UWORD32 i2_pic_wdin_mbs = ps_dec->u2_frm_wd_in_mbs;
    UWORD32 u1_slice_end;
    UWORD32 u1_tfr_n_mb;
    UWORD32 u1_decode_nmb;
    dec_bit_stream_t * const ps_bitstrm = ps_dec->ps_bitstrm;
    dec_slice_params_t * ps_slice = ps_dec->ps_cur_slice;
    UWORD32 *pu4_bitstrm_buf = ps_bitstrm->pu4_buffer;
    UWORD32 *pu4_bitstrm_ofst = &ps_bitstrm->u4_ofst;
    deblk_mb_t *ps_cur_deblk_mb;
    dec_mb_info_t *ps_cur_mb_info;
    parse_pmbarams_t *ps_parse_mb_data;
    UWORD32 u1_inter_mb_type;
    UWORD32 u1_deblk_mb_type;
    UWORD16 u2_total_mbs_coded;
    UWORD32 u1_mbaff;
    parse_part_params_t *ps_part_info;
    WORD32 ret;
    UNUSED(u1_is_idr_slice);

    if(ps_dec->ps_dec_err_status->u1_err_flag & REJECT_CUR_PIC)
    {
        ih264d_err_pic_dispbuf_mgr(ps_dec);
        return 0;
    }

    if(ps_dec->ps_cur_slice->u1_mbaff_frame_flag && (num_mb_skip & 1))
    {
        num_mb_skip++;
    }
    ps_dec->ps_dpb_cmds->u1_long_term_reference_flag = 0;
    if(prev_slice_err == 1)
    {
        /* first slice - missing/header corruption */
        ps_dec->ps_cur_slice->u2_frame_num = u2_frame_num;
        {
            WORD32 i, j, poc = 0;

            ps_dec->ps_cur_slice->u2_first_mb_in_slice = 0;

            ps_dec->pf_mvpred = ih264d_mvpred_nonmbaff;
            ps_dec->p_form_mb_part_info = ih264d_form_mb_part_info_bp;
            ps_dec->p_motion_compensate = ih264d_motion_compensate_bp;

            if(ps_dec->ps_cur_pic != NULL)
            {
                poc = ps_dec->ps_cur_pic->i4_poc;
                if (poc <= INT32_MAX - 2)
                    poc += 2;
            }

            j = -1;
            for(i = 0; i < MAX_NUM_PIC_PARAMS; i++)
            {
                   if(ps_dec->ps_pps[i].u1_is_valid == TRUE)
                   {
                       if(ps_dec->ps_pps[i].ps_sps->u1_is_valid == TRUE)
                       {
                           j = i;
                           break;
                       }
                   }
            }

            //if valid SPS PPS is not found return error
            if(j == -1)
            {
                return ERROR_INV_SLICE_HDR_T;
            }

            /* call ih264d_start_of_pic only if it was not called earlier*/
            if(ps_dec->u4_pic_buf_got == 0)
            {
                //initialize slice params required by ih264d_start_of_pic to valid values
                ps_dec->ps_cur_slice->u1_slice_type = P_SLICE;
                ps_dec->ps_cur_slice->u1_nal_ref_idc = 1;
                ps_dec->ps_cur_slice->u1_nal_unit_type = 1;
                ret = ih264d_start_of_pic(ps_dec, poc, ps_cur_poc,
                        ps_dec->ps_cur_slice->u2_frame_num,
                        &ps_dec->ps_pps[j]);

                if(ret != OK)
                {
                    return ret;
                }
            }

            ps_dec->ps_ref_pic_buf_lx[0][0]->u1_pic_buf_id = 0;

            ps_dec->u4_output_present = 0;

            {
                ih264d_get_next_display_field(ps_dec,
                                              ps_dec->ps_out_buffer,
                                              &(ps_dec->s_disp_op));
                /* If error code is non-zero then there is no buffer available for display,
                 hence avoid format conversion */

                if(0 != ps_dec->s_disp_op.u4_error_code)
                {
                    ps_dec->u4_fmt_conv_cur_row = ps_dec->s_disp_frame_info.u4_y_ht;
                }
                else
                    ps_dec->u4_output_present = 1;
            }

            if(ps_dec->u1_separate_parse == 1)
            {
                if(ps_dec->u4_dec_thread_created == 0)
                {
                    ithread_create(ps_dec->pv_dec_thread_handle, NULL,
                                   (void *)ih264d_decode_picture_thread,
                                   (void *)ps_dec);

                    ps_dec->u4_dec_thread_created = 1;
                }

                if((ps_dec->u4_num_cores == 3) &&
                                ((ps_dec->u4_app_disable_deblk_frm == 0) || ps_dec->i1_recon_in_thread3_flag)
                                && (ps_dec->u4_bs_deblk_thread_created == 0))
                {
                    ps_dec->u4_start_recon_deblk = 0;
                    ithread_create(ps_dec->pv_bs_deblk_thread_handle, NULL,
                                   (void *)ih264d_recon_deblk_thread,
                                   (void *)ps_dec);
                    ps_dec->u4_bs_deblk_thread_created = 1;
                }
            }
        }
    }
    else
    {
        // Middle / last slice

        dec_slice_struct_t *ps_parse_cur_slice;
        ps_parse_cur_slice = ps_dec->ps_dec_slice_buf + ps_dec->u2_cur_slice_num;

        if(ps_dec->u1_slice_header_done
            && ps_parse_cur_slice == ps_dec->ps_parse_cur_slice)
        {
            // Slice data corrupted
            // in the case of mbaff, conceal from the even mb.
            if((ps_dec->ps_cur_slice->u1_mbaff_frame_flag) && (ps_dec->u4_num_mbs_cur_nmb & 1))
            {
                ps_dec->u4_num_mbs_cur_nmb = ps_dec->u4_num_mbs_cur_nmb - 1;
                ps_dec->u2_cur_mb_addr--;
            }

            u1_num_mbs = ps_dec->u4_num_mbs_cur_nmb;
            if(u1_num_mbs)
            {
                ps_cur_mb_info = ps_dec->ps_nmb_info + u1_num_mbs - 1;
            }
            else
            {
                if(ps_dec->u1_separate_parse)
                {
                    ps_cur_mb_info = ps_dec->ps_nmb_info;
                }
                else
                {
                    ps_cur_mb_info = ps_dec->ps_nmb_info
                            + ps_dec->u4_num_mbs_prev_nmb - 1;
                }
            }

            ps_dec->u2_mby = ps_cur_mb_info->u2_mby;
            ps_dec->u2_mbx = ps_cur_mb_info->u2_mbx;

            ps_dec->u1_mb_ngbr_availablity =
                    ps_cur_mb_info->u1_mb_ngbr_availablity;

            if(u1_num_mbs)
            {
                // Going back 1 mb
                ps_dec->pv_parse_tu_coeff_data = ps_dec->pv_prev_mb_parse_tu_coeff_data;
                ps_dec->u2_cur_mb_addr--;
                ps_dec->i4_submb_ofst -= SUB_BLK_SIZE;

                // Parse/decode N-MB left unparsed
                if (ps_dec->u1_pr_sl_type == P_SLICE
                        || ps_dec->u1_pr_sl_type == B_SLICE)
                {
                    ps_dec->pf_mvpred_ref_tfr_nby2mb(ps_dec, u1_mb_idx,    u1_num_mbs);
                    ps_dec->ps_part = ps_dec->ps_parse_part_params;
                }

                u1_num_mbs_next = i2_pic_wdin_mbs - ps_dec->u2_mbx - 1;
                u1_end_of_row = (!u1_num_mbs_next)
                        && (!(ps_dec->ps_cur_slice->u1_mbaff_frame_flag && (u1_num_mbs & 0x01)));
                u1_slice_end = 1;
                u1_tfr_n_mb = 1;
                ps_cur_mb_info->u1_end_of_slice = u1_slice_end;

                if(ps_dec->u1_separate_parse)
                {
                    ih264d_parse_tfr_nmb(ps_dec, u1_mb_idx, u1_num_mbs,
                            u1_num_mbs_next, u1_tfr_n_mb, u1_end_of_row);
                    ps_dec->ps_nmb_info += u1_num_mbs;
                }
                else
                {
                    ih264d_decode_recon_tfr_nmb(ps_dec, u1_mb_idx, u1_num_mbs,
                            u1_num_mbs_next, u1_tfr_n_mb, u1_end_of_row);
                }
                ps_dec->u2_total_mbs_coded += u1_num_mbs;
                ps_dec->u1_mb_idx = 0;
                ps_dec->u4_num_mbs_cur_nmb = 0;
            }

            if(ps_dec->u2_total_mbs_coded
                    >= ps_dec->u2_frm_ht_in_mbs * ps_dec->u2_frm_wd_in_mbs)
            {
                ps_dec->u1_pic_decode_done = 1;
                return 0;
            }

            /* Inserting new slice only if the current slice has atleast 1 MB*/
            if(ps_dec->ps_parse_cur_slice->u4_first_mb_in_slice <
                    (UWORD32)(ps_dec->u2_total_mbs_coded >> ps_slice->u1_mbaff_frame_flag))
            {
                ps_dec->i2_prev_slice_mbx = ps_dec->u2_mbx;
                ps_dec->i2_prev_slice_mby = ps_dec->u2_mby;
                ps_dec->u2_cur_slice_num++;
                ps_dec->ps_parse_cur_slice++;
            }

        }
        else
        {
            // Slice missing / header corrupted
            ps_dec->ps_parse_cur_slice = ps_dec->ps_dec_slice_buf
                                            + ps_dec->u2_cur_slice_num;
        }
    }

    /******************************************************/
    /* Initializations to new slice                       */
    /******************************************************/
    {
        WORD32 num_entries;
        WORD32 size;
        UWORD8 *pu1_buf;

        num_entries = MAX_FRAMES;
        if((1 >= ps_dec->ps_cur_sps->u1_num_ref_frames) &&
            (0 == ps_dec->i4_display_delay))
        {
            num_entries = 1;
        }
        num_entries = ((2 * num_entries) + 1);
        num_entries *= 2;

        size = num_entries * sizeof(void *);
        size += PAD_MAP_IDX_POC * sizeof(void *);

        pu1_buf = (UWORD8 *)ps_dec->pv_map_ref_idx_to_poc_buf;
        pu1_buf += size * ps_dec->u2_cur_slice_num;
        ps_dec->ps_parse_cur_slice->ppv_map_ref_idx_to_poc = (volatile void **)pu1_buf;
    }
    u1_mbaff = ps_slice->u1_mbaff_frame_flag;
    ps_dec->ps_cur_slice->u2_first_mb_in_slice = ps_dec->u2_total_mbs_coded >> u1_mbaff;
    ps_dec->ps_cur_slice->i1_slice_alpha_c0_offset = 0;
    ps_dec->ps_cur_slice->i1_slice_beta_offset = 0;

    if(ps_dec->ps_cur_slice->u1_field_pic_flag)
        ps_dec->u2_prv_frame_num = ps_dec->ps_cur_slice->u2_frame_num;

    ps_dec->ps_parse_cur_slice->u4_first_mb_in_slice = ps_dec->u2_total_mbs_coded >> u1_mbaff;
    ps_dec->ps_parse_cur_slice->u2_log2Y_crwd =    ps_dec->ps_cur_slice->u2_log2Y_crwd;


    if(ps_dec->u1_separate_parse)
    {
        ps_dec->ps_parse_cur_slice->pv_tu_coeff_data_start = ps_dec->pv_parse_tu_coeff_data;
    }
    else
    {
        ps_dec->pv_proc_tu_coeff_data = ps_dec->pv_parse_tu_coeff_data;
    }

    /******************************************************/
    /* Initializations specific to P slice                */
    /******************************************************/
    u1_inter_mb_type = P_MB;
    u1_deblk_mb_type = D_INTER_MB;

    ps_dec->ps_cur_slice->u1_slice_type = P_SLICE;
    ps_dec->ps_parse_cur_slice->slice_type = P_SLICE;
    ps_dec->pf_mvpred_ref_tfr_nby2mb = ih264d_mv_pred_ref_tfr_nby2_pmb;
    ps_dec->ps_part = ps_dec->ps_parse_part_params;
    ps_dec->u2_mbx =
                    (MOD(ps_dec->ps_cur_slice->u2_first_mb_in_slice - 1, ps_dec->u2_frm_wd_in_mbs));
    ps_dec->u2_mby =
                    (DIV(ps_dec->ps_cur_slice->u2_first_mb_in_slice - 1, ps_dec->u2_frm_wd_in_mbs));
    ps_dec->u2_mby <<= u1_mbaff;

    /******************************************************/
    /* Parsing / decoding the slice                       */
    /******************************************************/
    ps_dec->u1_slice_header_done = 2;
    ps_dec->u1_qp = ps_slice->u1_slice_qp;
    ih264d_update_qp(ps_dec, 0);
    u1_mb_idx = ps_dec->u1_mb_idx;
    ps_parse_mb_data = ps_dec->ps_parse_mb_data;
    u1_num_mbs = u1_mb_idx;

    u1_slice_end = 0;
    u1_tfr_n_mb = 0;
    u1_decode_nmb = 0;
    u1_num_mbsNby2 = 0;
    i2_cur_mb_addr = ps_dec->u2_total_mbs_coded;
    i2_mb_skip_run = num_mb_skip;

    while(!u1_slice_end)
    {
        UWORD8 u1_mb_type;

        if(i2_cur_mb_addr > ps_dec->ps_cur_sps->u2_max_mb_addr)
            break;

        ps_cur_mb_info = ps_dec->ps_nmb_info + u1_num_mbs;
        ps_dec->u4_num_mbs_cur_nmb = u1_num_mbs;

        ps_cur_mb_info->u1_Mux = 0;
        ps_dec->u4_num_pmbair = (u1_num_mbs >> u1_mbaff);
        ps_cur_deblk_mb = ps_dec->ps_deblk_mbn + u1_num_mbs;

        ps_cur_mb_info->u1_end_of_slice = 0;

        /* Storing Default partition info */
        ps_parse_mb_data->u1_num_part = 1;
        ps_parse_mb_data->u1_isI_mb = 0;

        /**************************************************************/
        /* Get the required information for decoding of MB            */
        /**************************************************************/
        /* mb_x, mb_y, neighbor availablity, */
        if (u1_mbaff)
            ih264d_get_mb_info_cavlc_mbaff(ps_dec, i2_cur_mb_addr, ps_cur_mb_info, i2_mb_skip_run);
        else
            ih264d_get_mb_info_cavlc_nonmbaff(ps_dec, i2_cur_mb_addr, ps_cur_mb_info, i2_mb_skip_run);

        /* Set the deblocking parameters for this MB */
        if(ps_dec->u4_app_disable_deblk_frm == 0)
        {
            ih264d_set_deblocking_parameters(ps_cur_deblk_mb, ps_slice,
                                             ps_dec->u1_mb_ngbr_availablity,
                                             ps_dec->u1_cur_mb_fld_dec_flag);
        }

        /* Set appropriate flags in ps_cur_mb_info and ps_dec */
        ps_dec->i1_prev_mb_qp_delta = 0;
        ps_dec->u1_sub_mb_num = 0;
        ps_cur_mb_info->u1_mb_type = MB_SKIP;
        ps_cur_mb_info->u1_mb_mc_mode = PRED_16x16;
        ps_cur_mb_info->u1_cbp = 0;

        /* Storing Skip partition info */
        ps_part_info = ps_dec->ps_part;
        ps_part_info->u1_is_direct = PART_DIRECT_16x16;
        ps_part_info->u1_sub_mb_num = 0;
        ps_dec->ps_part++;

        /* Update Nnzs */
        ih264d_update_nnz_for_skipmb(ps_dec, ps_cur_mb_info, CAVLC);

        ps_cur_mb_info->ps_curmb->u1_mb_type = u1_inter_mb_type;
        ps_cur_deblk_mb->u1_mb_type |= u1_deblk_mb_type;

        i2_mb_skip_run--;

        ps_cur_deblk_mb->u1_mb_qp = ps_dec->u1_qp;

        if (u1_mbaff)
        {
            ih264d_update_mbaff_left_nnz(ps_dec, ps_cur_mb_info);
        }

        /**************************************************************/
        /* Get next Macroblock address                                */
        /**************************************************************/
        i2_cur_mb_addr++;

        u1_num_mbs++;
        u1_num_mbsNby2++;
        ps_parse_mb_data++;

        /****************************************************************/
        /* Check for End Of Row and other flags that determine when to  */
        /* do DMA setup for N/2-Mb, Decode for N-Mb, and Transfer for   */
        /* N-Mb                                                         */
        /****************************************************************/
        u1_num_mbs_next = i2_pic_wdin_mbs - ps_dec->u2_mbx - 1;
        u1_end_of_row = (!u1_num_mbs_next) && (!(u1_mbaff && (u1_num_mbs & 0x01)));
        u1_slice_end = !i2_mb_skip_run;
        u1_tfr_n_mb = (u1_num_mbs == ps_dec->u1_recon_mb_grp) || u1_end_of_row
                        || u1_slice_end;
        u1_decode_nmb = u1_tfr_n_mb || u1_slice_end;
        ps_cur_mb_info->u1_end_of_slice = u1_slice_end;

        if(u1_decode_nmb)
        {
            ps_dec->pf_mvpred_ref_tfr_nby2mb(ps_dec, u1_mb_idx, u1_num_mbs);
            u1_num_mbsNby2 = 0;

            ps_parse_mb_data = ps_dec->ps_parse_mb_data;
            ps_dec->ps_part = ps_dec->ps_parse_part_params;

            if(ps_dec->u1_separate_parse)
            {
                ih264d_parse_tfr_nmb(ps_dec, u1_mb_idx, u1_num_mbs,
                                     u1_num_mbs_next, u1_tfr_n_mb, u1_end_of_row);
                ps_dec->ps_nmb_info +=  u1_num_mbs;
            }
            else
            {
                ih264d_decode_recon_tfr_nmb(ps_dec, u1_mb_idx, u1_num_mbs, u1_num_mbs_next,
                                            u1_tfr_n_mb, u1_end_of_row);
            }
            ps_dec->u2_total_mbs_coded += u1_num_mbs;
            if(u1_tfr_n_mb)
                u1_num_mbs = 0;
            u1_mb_idx = u1_num_mbs;
            ps_dec->u1_mb_idx = u1_num_mbs;
        }
    }

    ps_dec->u4_num_mbs_cur_nmb = 0;
    ps_dec->ps_cur_slice->u4_mbs_in_slice = i2_cur_mb_addr
                        - ps_dec->ps_parse_cur_slice->u4_first_mb_in_slice;

    H264_DEC_DEBUG_PRINT("Mbs in slice: %d\n", ps_dec->ps_cur_slice->u4_mbs_in_slice);


    /* incremented here only if first slice is inserted */
    if(ps_dec->u4_first_slice_in_pic != 0)
    {
        ps_dec->ps_parse_cur_slice++;
        ps_dec->u2_cur_slice_num++;
    }

    ps_dec->i2_prev_slice_mbx = ps_dec->u2_mbx;
    ps_dec->i2_prev_slice_mby = ps_dec->u2_mby;

    if(ps_dec->u2_total_mbs_coded
            >= ps_dec->u2_frm_ht_in_mbs * ps_dec->u2_frm_wd_in_mbs)
    {
        ps_dec->u1_pic_decode_done = 1;
    }

    return 0;

}

/*!
 **************************************************************************
 * \if Function name : ih264d_decode_pslice \endif
 *
 * \brief
 *    Decodes a P Slice
 *
 *
 * \return
 *    0 on Success and Error code otherwise
 **************************************************************************
 */
WORD32 ih264d_parse_pslice(dec_struct_t *ps_dec, UWORD16 u2_first_mb_in_slice)
{
    dec_pic_params_t * ps_pps = ps_dec->ps_cur_pps;
    dec_slice_params_t * ps_cur_slice = ps_dec->ps_cur_slice;
    dec_bit_stream_t *ps_bitstrm = ps_dec->ps_bitstrm;
    UWORD32 *pu4_bitstrm_buf = ps_bitstrm->pu4_buffer;
    UWORD32 *pu4_bitstrm_ofst = &ps_bitstrm->u4_ofst;
    UWORD8 u1_mbaff = ps_dec->ps_cur_slice->u1_mbaff_frame_flag; //ps_dec->ps_cur_sps->u1_mb_aff_flag;
    UWORD8 u1_field_pic_flag = ps_cur_slice->u1_field_pic_flag;

    UWORD64 u8_ref_idx_l0;
    UWORD32 u4_temp;
    WORD32 i_temp;
    WORD32 ret;

    /*--------------------------------------------------------------------*/
    /* Read remaining contents of the slice header                        */
    /*--------------------------------------------------------------------*/
    {
        WORD8 *pi1_buf;
        WORD16 *pi2_mv = ps_dec->s_default_mv_pred.i2_mv;
        WORD32 *pi4_mv = (WORD32*)pi2_mv;
        WORD16 *pi16_refFrame;

        pi1_buf = ps_dec->s_default_mv_pred.i1_ref_frame;
        pi16_refFrame = (WORD16*)pi1_buf;
        *pi4_mv = 0;
        *(pi4_mv + 1) = 0;
        *pi16_refFrame = OUT_OF_RANGE_REF;
        ps_dec->s_default_mv_pred.u1_col_ref_pic_idx = (UWORD8)-1;
        ps_dec->s_default_mv_pred.u1_pic_type = (UWORD8)-1;
    }

    ps_cur_slice->u1_num_ref_idx_active_override_flag = ih264d_get_bit_h264(
                    ps_bitstrm);

    COPYTHECONTEXT("SH: num_ref_idx_override_flag",
                    ps_cur_slice->u1_num_ref_idx_active_override_flag);

    u8_ref_idx_l0 = ps_dec->ps_cur_pps->u1_num_ref_idx_lx_active[0];
    if(ps_cur_slice->u1_num_ref_idx_active_override_flag)
    {
        u8_ref_idx_l0 = ih264d_uev(pu4_bitstrm_ofst, pu4_bitstrm_buf) + (UWORD64)1;
    }

    {
        UWORD8 u1_max_ref_idx = H264_MAX_REF_PICS << u1_field_pic_flag;
        if(u8_ref_idx_l0 > u1_max_ref_idx)
        {
            return ERROR_NUM_REF;
        }
        ps_cur_slice->u1_num_ref_idx_lx_active[0] = u8_ref_idx_l0;
        COPYTHECONTEXT("SH: num_ref_idx_l0_active_minus1",
                        ps_cur_slice->u1_num_ref_idx_lx_active[0] - 1);

    }

    {
        UWORD8 uc_refIdxReFlagL0 = ih264d_get_bit_h264(ps_bitstrm);
        COPYTHECONTEXT("SH: ref_pic_list_reordering_flag_l0",uc_refIdxReFlagL0);

        ih264d_init_ref_idx_lx_p(ps_dec);
        /* Store the value for future slices in the same picture */
        ps_dec->u1_num_ref_idx_lx_active_prev =
                        ps_cur_slice->u1_num_ref_idx_lx_active[0];

        /* Modified temporarily */
        if(uc_refIdxReFlagL0)
        {
            WORD8 ret;
            ps_dec->ps_ref_pic_buf_lx[0] = ps_dec->ps_dpb_mgr->ps_mod_dpb[0];
            ret = ih264d_ref_idx_reordering(ps_dec, 0);
            if(ret == -1)
                return ERROR_REFIDX_ORDER_T;
            ps_dec->ps_ref_pic_buf_lx[0] = ps_dec->ps_dpb_mgr->ps_mod_dpb[0];
        }
        else
            ps_dec->ps_ref_pic_buf_lx[0] =
                            ps_dec->ps_dpb_mgr->ps_init_dpb[0];
    }
    /* Create refIdx to POC mapping */
    {
        void **pui_map_ref_idx_to_poc_lx0, **pui_map_ref_idx_to_poc_lx1;
        WORD8 idx;
        struct pic_buffer_t *ps_pic;

        pui_map_ref_idx_to_poc_lx0 = ps_dec->ppv_map_ref_idx_to_poc + FRM_LIST_L0;
        pui_map_ref_idx_to_poc_lx0[0] = 0; //For ref_idx = -1
        pui_map_ref_idx_to_poc_lx0++;
        for(idx = 0; idx < ps_cur_slice->u1_num_ref_idx_lx_active[0]; idx++)
        {
            ps_pic = ps_dec->ps_ref_pic_buf_lx[0][idx];
            pui_map_ref_idx_to_poc_lx0[idx] = (ps_pic->pu1_buf1);
        }

        /* Bug Fix Deblocking */
        pui_map_ref_idx_to_poc_lx1 = ps_dec->ppv_map_ref_idx_to_poc + FRM_LIST_L1;
        pui_map_ref_idx_to_poc_lx1[0] = 0;

        if(u1_mbaff)
        {
            void **ppv_map_ref_idx_to_poc_lx_t, **ppv_map_ref_idx_to_poc_lx_b;
            void **ppv_map_ref_idx_to_poc_lx_t1, **ppv_map_ref_idx_to_poc_lx_b1;
            ppv_map_ref_idx_to_poc_lx_t = ps_dec->ppv_map_ref_idx_to_poc
                            + TOP_LIST_FLD_L0;
            ppv_map_ref_idx_to_poc_lx_b = ps_dec->ppv_map_ref_idx_to_poc
                            + BOT_LIST_FLD_L0;

            ppv_map_ref_idx_to_poc_lx_t[0] = 0; //  For ref_idx = -1
            ppv_map_ref_idx_to_poc_lx_t++;
            ppv_map_ref_idx_to_poc_lx_b[0] = 0; // For ref_idx = -1
            ppv_map_ref_idx_to_poc_lx_b++;

            idx = 0;
            for(idx = 0; idx < ps_cur_slice->u1_num_ref_idx_lx_active[0]; idx++)
            {
                ps_pic = ps_dec->ps_ref_pic_buf_lx[0][idx];
                ppv_map_ref_idx_to_poc_lx_t[0] = (ps_pic->pu1_buf1);
                ppv_map_ref_idx_to_poc_lx_b[1] = (ps_pic->pu1_buf1);

                ppv_map_ref_idx_to_poc_lx_b[0] = (ps_pic->pu1_buf1) + 1;
                ppv_map_ref_idx_to_poc_lx_t[1] = (ps_pic->pu1_buf1) + 1;

                ppv_map_ref_idx_to_poc_lx_t += 2;
                ppv_map_ref_idx_to_poc_lx_b += 2;
            }
            ppv_map_ref_idx_to_poc_lx_t1 = ps_dec->ppv_map_ref_idx_to_poc
                            + TOP_LIST_FLD_L1;
            ppv_map_ref_idx_to_poc_lx_t1[0] = 0;
            ppv_map_ref_idx_to_poc_lx_b1 = ps_dec->ppv_map_ref_idx_to_poc
                            + BOT_LIST_FLD_L1;
            ppv_map_ref_idx_to_poc_lx_b1[0] = 0;

        }

        if(ps_dec->u4_num_cores >= 3)
        {
            WORD32 num_entries;
            WORD32 size;

            num_entries = MAX_FRAMES;
            if((1 >= ps_dec->ps_cur_sps->u1_num_ref_frames) &&
                (0 == ps_dec->i4_display_delay))
            {
                num_entries = 1;
            }
            num_entries = ((2 * num_entries) + 1);
            num_entries *= 2;

            size = num_entries * sizeof(void *);
            size += PAD_MAP_IDX_POC * sizeof(void *);

            memcpy((void *)ps_dec->ps_parse_cur_slice->ppv_map_ref_idx_to_poc,
                   ps_dec->ppv_map_ref_idx_to_poc,
                   size);
        }


    }
    if(ps_pps->u1_wted_pred_flag)
    {
        ret = ih264d_parse_pred_weight_table(ps_cur_slice, ps_bitstrm);
        if(ret != OK)
            return ret;
        ih264d_form_pred_weight_matrix(ps_dec);
        ps_dec->pu4_wt_ofsts = ps_dec->pu4_wts_ofsts_mat;
    }
    else
    {
        ps_dec->ps_cur_slice->u2_log2Y_crwd = 0;
        ps_dec->pu4_wt_ofsts = ps_dec->pu4_wts_ofsts_mat;
    }

    ps_dec->ps_parse_cur_slice->u2_log2Y_crwd =
                    ps_dec->ps_cur_slice->u2_log2Y_crwd;

    if(u1_mbaff && (u1_field_pic_flag == 0))
    {
        ih264d_convert_frm_mbaff_list(ps_dec);
    }

    /* G050 */
    if(ps_cur_slice->u1_nal_ref_idc != 0)
    {
        if(!ps_dec->ps_dpb_cmds->u1_dpb_commands_read)
        {
            i_temp = ih264d_read_mmco_commands(ps_dec);
            if (i_temp < 0)
            {
                return ERROR_DBP_MANAGER_T;
            }
            ps_dec->u4_bitoffset = i_temp;
        }
        else
            ps_bitstrm->u4_ofst += ps_dec->u4_bitoffset;

    }
    /* G050 */

    if(ps_pps->u1_entropy_coding_mode == CABAC)
    {
        u4_temp = ih264d_uev(pu4_bitstrm_ofst, pu4_bitstrm_buf);

        if(u4_temp > MAX_CABAC_INIT_IDC)
        {
            return ERROR_INV_SLICE_HDR_T;
        }
        ps_cur_slice->u1_cabac_init_idc = u4_temp;
        COPYTHECONTEXT("SH: cabac_init_idc",ps_cur_slice->u1_cabac_init_idc);
    }

    /* Read slice_qp_delta */
    WORD64 i8_temp = (WORD64)ps_pps->u1_pic_init_qp
                        + ih264d_sev(pu4_bitstrm_ofst, pu4_bitstrm_buf);
    if((i8_temp < MIN_H264_QP) || (i8_temp > MAX_H264_QP))
    {
        return ERROR_INV_RANGE_QP_T;
    }
    ps_cur_slice->u1_slice_qp = i8_temp;
    COPYTHECONTEXT("SH: slice_qp_delta",
                    (WORD8)(ps_cur_slice->u1_slice_qp - ps_pps->u1_pic_init_qp));

    if(ps_pps->u1_deblocking_filter_parameters_present_flag == 1)
    {
        u4_temp = ih264d_uev(pu4_bitstrm_ofst, pu4_bitstrm_buf);
        if(u4_temp > SLICE_BOUNDARY_DBLK_DISABLED)
        {
            return ERROR_INV_SLICE_HDR_T;
        }

        COPYTHECONTEXT("SH: disable_deblocking_filter_idc", u4_temp);
        ps_cur_slice->u1_disable_dblk_filter_idc = u4_temp;
        if(u4_temp != 1)
        {
            i_temp = ih264d_sev(pu4_bitstrm_ofst, pu4_bitstrm_buf)
                            << 1;
            if((MIN_DBLK_FIL_OFF > i_temp) || (i_temp > MAX_DBLK_FIL_OFF))
            {
                return ERROR_INV_SLICE_HDR_T;
            }
            ps_cur_slice->i1_slice_alpha_c0_offset = i_temp;
            COPYTHECONTEXT("SH: slice_alpha_c0_offset_div2",
                            ps_cur_slice->i1_slice_alpha_c0_offset >> 1);

            i_temp = ih264d_sev(pu4_bitstrm_ofst, pu4_bitstrm_buf)
                            << 1;
            if((MIN_DBLK_FIL_OFF > i_temp) || (i_temp > MAX_DBLK_FIL_OFF))
            {
                return ERROR_INV_SLICE_HDR_T;
            }
            ps_cur_slice->i1_slice_beta_offset = i_temp;
            COPYTHECONTEXT("SH: slice_beta_offset_div2",
                            ps_cur_slice->i1_slice_beta_offset >> 1);
        }
        else
        {
            ps_cur_slice->i1_slice_alpha_c0_offset = 0;
            ps_cur_slice->i1_slice_beta_offset = 0;
        }
    }
    else
    {
        ps_cur_slice->u1_disable_dblk_filter_idc = 0;
        ps_cur_slice->i1_slice_alpha_c0_offset = 0;
        ps_cur_slice->i1_slice_beta_offset = 0;
    }

    ps_dec->u1_slice_header_done = 2;

    if(ps_pps->u1_entropy_coding_mode)
    {
        SWITCHOFFTRACE; SWITCHONTRACECABAC;
        ps_dec->pf_parse_inter_slice = ih264d_parse_inter_slice_data_cabac;
        ps_dec->pf_parse_inter_mb = ih264d_parse_pmb_cabac;
        ih264d_init_cabac_contexts(P_SLICE, ps_dec);

        if(ps_dec->ps_cur_slice->u1_mbaff_frame_flag)
            ps_dec->pf_get_mb_info = ih264d_get_mb_info_cabac_mbaff;
        else
            ps_dec->pf_get_mb_info = ih264d_get_mb_info_cabac_nonmbaff;
    }
    else
    {
        SWITCHONTRACE; SWITCHOFFTRACECABAC;
        ps_dec->pf_parse_inter_slice = ih264d_parse_inter_slice_data_cavlc;
        ps_dec->pf_parse_inter_mb = ih264d_parse_pmb_cavlc;
        if(ps_dec->ps_cur_slice->u1_mbaff_frame_flag)
        {
            ps_dec->pf_get_mb_info = ih264d_get_mb_info_cavlc_mbaff;
        }
        else
            ps_dec->pf_get_mb_info = ih264d_get_mb_info_cavlc_nonmbaff;
    }

    ps_dec->u1_B = 0;
    ps_dec->pf_mvpred_ref_tfr_nby2mb = ih264d_mv_pred_ref_tfr_nby2_pmb;
    ret = ps_dec->pf_parse_inter_slice(ps_dec, ps_cur_slice, u2_first_mb_in_slice);
    if(ret != OK)
        return ret;
//    ps_dec->curr_slice_in_error = 0 ;
    return OK;
}
