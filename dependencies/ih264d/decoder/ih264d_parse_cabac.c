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
 * \file ih264d_parse_cabac.c
 *
 * \brief
 *    This file contains cabac Residual decoding routines.
 *
 * \date
 *    20/03/2003
 *
 * \author  NS
 ***************************************************************************
 */

#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264d_defs.h"
#include "ih264d_structs.h"

#include "ih264d_cabac.h"
#include "ih264d_bitstrm.h"
#include "ih264d_parse_mb_header.h"
#include "ih264d_debug.h"
#include "ih264d_tables.h"
#include "ih264d_error_handler.h"
#include "ih264d_parse_cabac.h"
#include "ih264d_parse_slice.h"
#include "ih264d_tables.h"
#include "ih264d_mb_utils.h"
#include "ih264d_utils.h"

/*!
 ********************************************************************************
 *   \if Function name : ih264d_read_coeff4x4_cabac \endif
 *
 *   \brief  This function encodes residual_block_cabac as defined in 7.3.5.3.2.
 *
 *   \return
 *       Returns the index of last significant coeff.
 *
 ********************************************************************************
 */

UWORD8 ih264d_read_coeff4x4_cabac(dec_bit_stream_t *ps_bitstrm,
                                  UWORD32 u4_ctxcat,
                                  bin_ctxt_model_t *ps_ctxt_sig_coeff,
                                  dec_struct_t *ps_dec, /*!< pointer to access global variables*/
                                  bin_ctxt_model_t *ps_ctxt_coded)
{

    decoding_envirnoment_t *ps_cab_env = &ps_dec->s_cab_dec_env;
    UWORD32 u4_coded_flag;
    UWORD32 u4_offset, *pu4_buffer;
    UWORD32 u4_code_int_range, u4_code_int_val_ofst;
    tu_sblk4x4_coeff_data_t *ps_tu_4x4;
    WORD16 *pi2_coeff_data;
    WORD32 num_sig_coeffs = 0;

    /*loading from strcuctures*/

    ps_tu_4x4 = (tu_sblk4x4_coeff_data_t *)ps_dec->pv_parse_tu_coeff_data;
    ps_tu_4x4->u2_sig_coeff_map = 0;
    pi2_coeff_data = &ps_tu_4x4->ai2_level[0];

    u4_offset = ps_bitstrm->u4_ofst;
    pu4_buffer = ps_bitstrm->pu4_buffer;

    u4_code_int_range = ps_cab_env->u4_code_int_range;
    u4_code_int_val_ofst = ps_cab_env->u4_code_int_val_ofst;

    {

        /*inilined DecodeDecision_onebin begins*/

        {

            UWORD32 u4_qnt_int_range, u4_int_range_lps;
            UWORD32 u4_symbol, u1_mps_state;

            UWORD32 table_lookup;
            const UWORD32 *pu4_table = (const UWORD32 *)ps_cab_env->cabac_table;
            UWORD32 u4_clz;

            u1_mps_state = (ps_ctxt_coded->u1_mps_state);
            u4_clz = CLZ(u4_code_int_range);
            u4_qnt_int_range = u4_code_int_range << u4_clz;
            u4_qnt_int_range = (u4_qnt_int_range >> 29) & 0x3;
            table_lookup =
                            pu4_table[(u1_mps_state << 2) + u4_qnt_int_range];
            u4_int_range_lps = table_lookup & 0xff;
            u4_int_range_lps = u4_int_range_lps << (23 - u4_clz);
            u4_code_int_range = u4_code_int_range - u4_int_range_lps;
            u4_symbol = ((u1_mps_state >> 6) & 0x1);
            u1_mps_state = (table_lookup >> 8) & 0x7F;

            CHECK_IF_LPS(u4_code_int_range, u4_code_int_val_ofst, u4_symbol,
                         u4_int_range_lps, u1_mps_state, table_lookup)

            if(u4_code_int_range < ONE_RIGHT_SHIFTED_BY_8)
            {

                RENORM_RANGE_OFFSET(u4_code_int_range, u4_code_int_val_ofst,
                                    u4_offset, pu4_buffer)
            }

            ps_ctxt_coded->u1_mps_state = u1_mps_state;
            u4_coded_flag = u4_symbol;

            /*inilined DecodeDecision_onebin ends*/

        }

    }

    if(u4_coded_flag)
    {

        {
            bin_ctxt_model_t *p_binCtxt_last, *p_binCtxt_last_org;
            UWORD32 uc_last_coeff_idx;
            UWORD32 uc_bin;
            UWORD32 i;
            WORD32 first_coeff_offset = 0;

            if((u4_ctxcat == CHROMA_AC_CTXCAT) || (u4_ctxcat == LUMA_AC_CTXCAT))
            {
                first_coeff_offset = 1;
            }

            i = 0;
            if(u4_ctxcat == CHROMA_DC_CTXCAT)
            {
                uc_last_coeff_idx = 3;
            }
            else
            {
                UWORD32 u4_start;
                u4_start = (u4_ctxcat & 1) + (u4_ctxcat >> 2);
                uc_last_coeff_idx = 15 - u4_start;
            }
            p_binCtxt_last_org = ps_ctxt_sig_coeff
                            + LAST_COEFF_CTXT_MINUS_SIG_COEFF_CTXT;

            do
            {

                /*inilined DecodeDecision_onebin begins*/
                {

                    UWORD32 u4_qnt_int_range, u4_int_range_lps;
                    UWORD32 u4_symbol, u1_mps_state;
                    UWORD32 table_lookup;
                    const UWORD32 *pu4_table =
                                    (const UWORD32 *)ps_cab_env->cabac_table;
                    UWORD32 u4_clz;

                    u1_mps_state = (ps_ctxt_sig_coeff->u1_mps_state);

                    u4_clz = CLZ(u4_code_int_range);

                    u4_qnt_int_range = u4_code_int_range << u4_clz;
                    u4_qnt_int_range = (u4_qnt_int_range >> 29) & 0x3;

                    table_lookup = pu4_table[(u1_mps_state << 2)
                                    + u4_qnt_int_range];

                    u4_int_range_lps = table_lookup & 0xff;

                    u4_int_range_lps = u4_int_range_lps << (23 - u4_clz);
                    u4_code_int_range = u4_code_int_range - u4_int_range_lps;
                    u4_symbol = ((u1_mps_state >> 6) & 0x1);
                    u1_mps_state = (table_lookup >> 8) & 0x7F;

                    CHECK_IF_LPS(u4_code_int_range, u4_code_int_val_ofst,
                                 u4_symbol, u4_int_range_lps, u1_mps_state,
                                 table_lookup)

                    if(u4_code_int_range < ONE_RIGHT_SHIFTED_BY_14)
                    {

                        UWORD32 read_bits, u4_clz;
                        u4_clz = CLZ(u4_code_int_range);
                        NEXTBITS(read_bits, (u4_offset + 23), pu4_buffer,
                                 u4_clz)
                        FLUSHBITS(u4_offset, (u4_clz))
                        u4_code_int_range = u4_code_int_range << u4_clz;
                        u4_code_int_val_ofst = (u4_code_int_val_ofst << u4_clz)
                                        | read_bits;
                    }

                    INC_BIN_COUNT(
                                    ps_cab_env)

                    ps_ctxt_sig_coeff->u1_mps_state = u1_mps_state;
                    uc_bin = u4_symbol;

                }
                /*incrementing pointer to point to the context of the next bin*/
                ps_ctxt_sig_coeff++;

                /*inilined DecodeDecision_onebin ends*/

                if(uc_bin)
                {
                    num_sig_coeffs++;
                    SET_BIT(ps_tu_4x4->u2_sig_coeff_map, (i + first_coeff_offset));

                    p_binCtxt_last = p_binCtxt_last_org + i;

                    /*inilined DecodeDecision_onebin begins*/

                    {

                        UWORD32 u4_qnt_int_range, u4_int_range_lps;
                        UWORD32 u4_symbol, u1_mps_state;
                        UWORD32 table_lookup;
                        const UWORD32 *pu4_table =
                                        (const UWORD32 *)ps_cab_env->cabac_table;
                        UWORD32 u4_clz;

                        u1_mps_state = (p_binCtxt_last->u1_mps_state);

                        u4_clz = CLZ(u4_code_int_range);
                        u4_qnt_int_range = u4_code_int_range << u4_clz;
                        u4_qnt_int_range = (u4_qnt_int_range >> 29)
                                        & 0x3;

                        table_lookup = pu4_table[(u1_mps_state << 2)
                                        + u4_qnt_int_range];
                        u4_int_range_lps = table_lookup & 0xff;

                        u4_int_range_lps = u4_int_range_lps
                                        << (23 - u4_clz);

                        u4_code_int_range = u4_code_int_range
                                        - u4_int_range_lps;
                        u4_symbol = ((u1_mps_state >> 6) & 0x1);
                        u1_mps_state = (table_lookup >> 8) & 0x7F;

                        CHECK_IF_LPS(u4_code_int_range, u4_code_int_val_ofst,
                                     u4_symbol, u4_int_range_lps,
                                     u1_mps_state, table_lookup)

                        INC_BIN_COUNT(ps_cab_env)

                        p_binCtxt_last->u1_mps_state = u1_mps_state;
                        uc_bin = u4_symbol;

                    }

                    /*inilined DecodeDecision_onebin ends*/
                    if(uc_bin == 1)
                        goto label_read_levels;

                }

                i = i + 1;

            }
            while(i < uc_last_coeff_idx);

            num_sig_coeffs++;
            SET_BIT(ps_tu_4x4->u2_sig_coeff_map, (i + first_coeff_offset));

            label_read_levels: ;

        }

        /// VALUE of No of Coeff in BLOCK = i + 1 for second case else i;

        /* Decode coeff_abs_level_minus1 and coeff_sign_flag */
        {

            WORD32 i2_abs_lvl;
            UWORD32 u1_abs_level_equal1 = 1, u1_abs_level_gt1 = 0;

            UWORD32 u4_ctx_inc;
            UWORD32 ui_prefix;
        bin_ctxt_model_t *p_ctxt_abs_level;


        p_ctxt_abs_level = ps_dec->p_coeff_abs_level_minus1_t[u4_ctxcat];
        u4_ctx_inc = ((0x51));

        /*****************************************************/
        /* Main Loop runs for no. of Significant coefficient */
        /*****************************************************/


        do
            {

                {
                    INC_SYM_COUNT(&(ps_dec.s_cab_dec_env));

                    /*****************************************************/
                    /* inilining a modified ih264d_decode_bins_unary     */
                    /*****************************************************/

                    {
                        UWORD32 u4_value;
                        UWORD32 u4_symbol;
                        bin_ctxt_model_t *ps_bin_ctxt;
                        UWORD32 u4_ctx_Inc;

                        u4_value = 0;

                        u4_ctx_Inc = u4_ctx_inc & 0xf;
                        ps_bin_ctxt = p_ctxt_abs_level + u4_ctx_Inc;

                        do
                        {

                            {

                                UWORD32 u4_qnt_int_range,
                                                u4_int_range_lps;
                                UWORD32 u1_mps_state;
                                UWORD32 table_lookup;
                                const UWORD32 *pu4_table =
                                                (const UWORD32 *)ps_cab_env->cabac_table;
                                UWORD32 u4_clz;

                                u1_mps_state = (ps_bin_ctxt->u1_mps_state);
                                u4_clz = CLZ(u4_code_int_range);
                                u4_qnt_int_range = u4_code_int_range
                                                << u4_clz;
                                u4_qnt_int_range = (u4_qnt_int_range
                                                >> 29) & 0x3;
                                table_lookup = pu4_table[(u1_mps_state << 2)
                                                + u4_qnt_int_range];
                                u4_int_range_lps = table_lookup & 0xff;

                                u4_int_range_lps = u4_int_range_lps
                                                << (23 - u4_clz);
                                u4_code_int_range = u4_code_int_range
                                                - u4_int_range_lps;
                                u4_symbol = ((u1_mps_state >> 6) & 0x1);
                                u1_mps_state = (table_lookup >> 8) & 0x7F;

                                CHECK_IF_LPS(u4_code_int_range,
                                             u4_code_int_val_ofst, u4_symbol,
                                             u4_int_range_lps, u1_mps_state,
                                             table_lookup)

                                if(u4_code_int_range < ONE_RIGHT_SHIFTED_BY_9)
                                {

                                    RENORM_RANGE_OFFSET(u4_code_int_range,
                                                        u4_code_int_val_ofst,
                                                        u4_offset, pu4_buffer)
                                }

                                INC_BIN_COUNT(ps_cab_env);

                                ps_bin_ctxt->u1_mps_state = u1_mps_state;
                            }

                            INC_BIN_COUNT(ps_cab_env);INC_DECISION_BINS(ps_cab_env);

                            u4_value++;
                            ps_bin_ctxt = p_ctxt_abs_level + (u4_ctx_inc >> 4);

                        }
                        while(u4_symbol && (u4_value < UCOFF_LEVEL));

                        ui_prefix = u4_value - 1 + u4_symbol;

                    }

                    if(ui_prefix == UCOFF_LEVEL)
                    {
                        UWORD32 ui16_sufS = 0;
                        UWORD32 u1_max_bins;
                        UWORD32 u4_value;

                        i2_abs_lvl = UCOFF_LEVEL;
                        /*inlining ih264d_decode_bypass_bins_unary begins*/

                        {
                            UWORD32 uc_bin;
                            UWORD32 bits_to_flush;


                            bits_to_flush = 0;
                            /*renormalize to ensure there 23 bits more in the u4_code_int_val_ofst*/
                            {
                                UWORD32 u4_clz, read_bits;

                                u4_clz = CLZ(u4_code_int_range);
                                FLUSHBITS(u4_offset, u4_clz)
                                NEXTBITS(read_bits, u4_offset, pu4_buffer, CABAC_BITS_TO_READ)
                                u4_code_int_range = u4_code_int_range << u4_clz;
                                u4_code_int_val_ofst = (u4_code_int_val_ofst
                                                << u4_clz) | read_bits;

                            }

                            do
                            {
                                bits_to_flush++;

                                u4_code_int_range = u4_code_int_range >> 1;

                                if(u4_code_int_val_ofst >= u4_code_int_range)
                                {
                                    /* S=1 */
                                    uc_bin = 1;
                                    u4_code_int_val_ofst -= u4_code_int_range;
                                }
                                else
                                {
                                    /* S=0 */
                                    uc_bin = 0;
                                }

                                INC_BIN_COUNT(
                                                ps_cab_env);INC_BYPASS_BINS(ps_cab_env);

                            }
                            while(uc_bin && (bits_to_flush < CABAC_BITS_TO_READ));

                            u4_value = (bits_to_flush - 1);

                        }
                        /*inlining ih264d_decode_bypass_bins_unary ends*/

                        ui16_sufS = (1 << u4_value);
                        u1_max_bins = u4_value;

                        if(u4_value > 0)
                        {

                            /*inline bypassbins_flc begins*/

                            if(u4_value > 10)
                            {
                                UWORD32 u4_clz, read_bits;

                                u4_clz = CLZ(u4_code_int_range);
                                FLUSHBITS(u4_offset, u4_clz)
                                NEXTBITS(read_bits, u4_offset, pu4_buffer, CABAC_BITS_TO_READ)
                                u4_code_int_range = u4_code_int_range << u4_clz;
                                u4_code_int_val_ofst = (u4_code_int_val_ofst
                                                << u4_clz) | read_bits;
                            }

                            {
                                UWORD32 ui_bins;
                                UWORD32 uc_bin;
                                UWORD32 bits_to_flush;

                                ui_bins = 0;
                                bits_to_flush = 0;

                                do
                                {
                                    bits_to_flush++;

                                    u4_code_int_range = u4_code_int_range >> 1;

                                    if(u4_code_int_val_ofst
                                                    >= u4_code_int_range)
                                    {
                                        /* S=1 */
                                        uc_bin = 1;
                                        u4_code_int_val_ofst -=
                                                        u4_code_int_range;
                                    }
                                    else
                                    {
                                        /* S=0 */
                                        uc_bin = 0;
                                    }

                                    INC_BIN_COUNT(
                                                    ps_cab_env);INC_BYPASS_BINS(ps_cab_env);

                                    ui_bins = ((ui_bins << 1) | uc_bin);

                                }
                                while(bits_to_flush < u1_max_bins);

                                u4_value = ui_bins;
                            }

                            /*inline bypassbins_flc ends*/

                        }

                        //Value of K
                        ui16_sufS += u4_value;
                        i2_abs_lvl += ui16_sufS;

                    }
                    else
                        i2_abs_lvl = 1 + ui_prefix;

                    if(i2_abs_lvl > 1)
                    {
                        u1_abs_level_gt1++;
                    }
                    if(!u1_abs_level_gt1)
                    {
                        u1_abs_level_equal1++;
                        u4_ctx_inc = (5 << 4) + MIN(u1_abs_level_equal1, 4);
                    }
                    else
                        u4_ctx_inc = (5 + MIN(u1_abs_level_gt1, 4)) << 4;

                    /*u4_ctx_inc = g_table_temp[u1_abs_level_gt1][u1_abs_level_equal1];*/

                    /* encode coeff_sign_flag[i] */

                    {
                        u4_code_int_range = u4_code_int_range >> 1;

                        if(u4_code_int_val_ofst >= (u4_code_int_range))
                        {
                            /* S=1 */
                            u4_code_int_val_ofst -= u4_code_int_range;
                            i2_abs_lvl = (-i2_abs_lvl);
                        }

                    }
                    num_sig_coeffs--;
                    *pi2_coeff_data++ = i2_abs_lvl;
                }
            }
            while(num_sig_coeffs > 0);
        }
    }

    if(u4_coded_flag)
    {
        WORD32 offset;
        offset = (UWORD8 *)pi2_coeff_data - (UWORD8 *)ps_tu_4x4;
        offset = ALIGN4(offset);
        ps_dec->pv_parse_tu_coeff_data = (void *)((UWORD8 *)ps_dec->pv_parse_tu_coeff_data + offset);
    }


    /*updating structures*/
    ps_cab_env->u4_code_int_val_ofst = u4_code_int_val_ofst;
    ps_cab_env->u4_code_int_range = u4_code_int_range;
    ps_bitstrm->u4_ofst = u4_offset;
    return (u4_coded_flag);
}
/*!
 ********************************************************************************
 *   \if Function name : ih264d_read_coeff8x8_cabac \endif
 *
 *   \brief  This function encodes residual_block_cabac as defined in 7.3.5.3.2.
 when transform_8x8_flag  = 1
 *
 *   \return
 *       Returns the index of last significant coeff.
 *
 ********************************************************************************
 */

void ih264d_read_coeff8x8_cabac(dec_bit_stream_t *ps_bitstrm,
                                dec_struct_t *ps_dec, /*!< pointer to access global variables*/
                                dec_mb_info_t *ps_cur_mb_info)
{
    decoding_envirnoment_t *ps_cab_env = &ps_dec->s_cab_dec_env;
    UWORD32 u4_offset, *pu4_buffer;
    UWORD32 u4_code_int_range, u4_code_int_val_ofst;

    /* High profile related declarations */
    UWORD8 u1_field_coding_flag = ps_cur_mb_info->ps_curmb->u1_mb_fld;
    const UWORD8 *pu1_lastcoeff_context_inc =
                    (UWORD8 *)gau1_ih264d_lastcoeff_context_inc;
    const UWORD8 *pu1_sigcoeff_context_inc;
    bin_ctxt_model_t *ps_ctxt_sig_coeff;
    WORD32 num_sig_coeffs = 0;
    tu_blk8x8_coeff_data_t *ps_tu_8x8;
    WORD16 *pi2_coeff_data;

    /*loading from strcuctures*/

    ps_tu_8x8 = (tu_blk8x8_coeff_data_t *)ps_dec->pv_parse_tu_coeff_data;
    ps_tu_8x8->au4_sig_coeff_map[0] = 0;
    ps_tu_8x8->au4_sig_coeff_map[1] = 0;
    pi2_coeff_data = &ps_tu_8x8->ai2_level[0];


    if(!u1_field_coding_flag)
    {
        pu1_sigcoeff_context_inc =
                        (UWORD8 *)gau1_ih264d_sigcoeff_context_inc_frame;

        /*******************************************************************/
        /* last coefficient context is derived from significant coeff u4_flag */
        /* only significant coefficient matrix need to be initialized      */
        /*******************************************************************/
        ps_ctxt_sig_coeff = ps_dec->s_high_profile.ps_sigcoeff_8x8_frame;
    }
    else
    {
        pu1_sigcoeff_context_inc =
                        (UWORD8 *)gau1_ih264d_sigcoeff_context_inc_field;

        /*******************************************************************/
        /* last coefficient context is derived from significant coeff u4_flag */
        /* only significant coefficient matrix need to be initialized      */
        /*******************************************************************/
        ps_ctxt_sig_coeff = ps_dec->s_high_profile.ps_sigcoeff_8x8_field;
    }

    /*loading from strcuctures*/

    u4_offset = ps_bitstrm->u4_ofst;
    pu4_buffer = ps_bitstrm->pu4_buffer;

    u4_code_int_range = ps_cab_env->u4_code_int_range;
    u4_code_int_val_ofst = ps_cab_env->u4_code_int_val_ofst;

    {
        {
            bin_ctxt_model_t *p_binCtxt_last, *p_binCtxt_last_org,
                            *p_ctxt_sig_coeff_org;
            UWORD32 uc_last_coeff_idx;
            UWORD32 uc_bin;
            UWORD32 i;

            i = 0;

            uc_last_coeff_idx = 63;

            p_binCtxt_last_org = ps_ctxt_sig_coeff
                            + LAST_COEFF_CTXT_MINUS_SIG_COEFF_CTXT_8X8;

            p_ctxt_sig_coeff_org = ps_ctxt_sig_coeff;

            do
            {
                /*inilined DecodeDecision_onebin begins*/
                {
                    UWORD32 u4_qnt_int_range, u4_int_range_lps;
                    UWORD32 u4_symbol, u1_mps_state;
                    UWORD32 table_lookup;
                    const UWORD32 *pu4_table =
                                    (const UWORD32 *)ps_cab_env->cabac_table;
                    UWORD32 u4_clz;

                    u1_mps_state = (ps_ctxt_sig_coeff->u1_mps_state);

                    u4_clz = CLZ(u4_code_int_range);

                    u4_qnt_int_range = u4_code_int_range << u4_clz;
                    u4_qnt_int_range = (u4_qnt_int_range >> 29) & 0x3;

                    table_lookup = pu4_table[(u1_mps_state << 2)
                                    + u4_qnt_int_range];

                    u4_int_range_lps = table_lookup & 0xff;

                    u4_int_range_lps = u4_int_range_lps << (23 - u4_clz);
                    u4_code_int_range = u4_code_int_range - u4_int_range_lps;
                    u4_symbol = ((u1_mps_state >> 6) & 0x1);
                    u1_mps_state = (table_lookup >> 8) & 0x7F;

                    CHECK_IF_LPS(u4_code_int_range, u4_code_int_val_ofst,
                                 u4_symbol, u4_int_range_lps, u1_mps_state,
                                 table_lookup)

                    if(u4_code_int_range < ONE_RIGHT_SHIFTED_BY_14)
                    {
                        UWORD32 read_bits, u4_clz;
                        u4_clz = CLZ(u4_code_int_range);
                        NEXTBITS(read_bits, (u4_offset + 23), pu4_buffer,
                                 u4_clz)
                        FLUSHBITS(u4_offset, (u4_clz))
                        u4_code_int_range = u4_code_int_range << u4_clz;
                        u4_code_int_val_ofst = (u4_code_int_val_ofst << u4_clz)
                                        | read_bits;
                    }

                    ps_ctxt_sig_coeff->u1_mps_state = u1_mps_state;
                    uc_bin = u4_symbol;
                }
                /*incrementing pointer to point to the context of the next bin*/
                ps_ctxt_sig_coeff = p_ctxt_sig_coeff_org
                                + pu1_sigcoeff_context_inc[i + 1];

                /*inilined DecodeDecision_onebin ends*/
                if(uc_bin)
                {
                    num_sig_coeffs++;
                    SET_BIT(ps_tu_8x8->au4_sig_coeff_map[i>31], (i > 31 ? i - 32:i));

                    p_binCtxt_last = p_binCtxt_last_org
                                    + pu1_lastcoeff_context_inc[i];

                    /*inilined DecodeDecision_onebin begins*/

                    {
                        UWORD32 u4_qnt_int_range, u4_int_range_lps;
                        UWORD32 u4_symbol, u1_mps_state;
                        UWORD32 table_lookup;
                        const UWORD32 *pu4_table =
                                        (const UWORD32 *)ps_cab_env->cabac_table;
                        UWORD32 u4_clz;

                        u1_mps_state = (p_binCtxt_last->u1_mps_state);

                        u4_clz = CLZ(u4_code_int_range);
                        u4_qnt_int_range = u4_code_int_range << u4_clz;
                        u4_qnt_int_range = (u4_qnt_int_range >> 29)
                                        & 0x3;

                        table_lookup = pu4_table[(u1_mps_state << 2)
                                        + u4_qnt_int_range];
                        u4_int_range_lps = table_lookup & 0xff;

                        u4_int_range_lps = u4_int_range_lps
                                        << (23 - u4_clz);

                        u4_code_int_range = u4_code_int_range
                                        - u4_int_range_lps;
                        u4_symbol = ((u1_mps_state >> 6) & 0x1);
                        u1_mps_state = (table_lookup >> 8) & 0x7F;

                        CHECK_IF_LPS(u4_code_int_range, u4_code_int_val_ofst,
                                     u4_symbol, u4_int_range_lps,
                                     u1_mps_state, table_lookup)

                        p_binCtxt_last->u1_mps_state = u1_mps_state;
                        uc_bin = u4_symbol;
                    }

                    /*inilined DecodeDecision_onebin ends*/
                    if(uc_bin == 1)
                        goto label_read_levels;

                }

                i = i + 1;

            }
            while(i < uc_last_coeff_idx);

            num_sig_coeffs++;
            SET_BIT(ps_tu_8x8->au4_sig_coeff_map[i>31], (i > 31 ? i - 32:i));

            label_read_levels: ;
        }

        /// VALUE of No of Coeff in BLOCK = i + 1 for second case else i;

        /* Decode coeff_abs_level_minus1 and coeff_sign_flag */
        {
            WORD32 i2_abs_lvl;
            UWORD32 u1_abs_level_equal1 = 1, u1_abs_level_gt1 = 0;

            UWORD32 u4_ctx_inc;
            UWORD32 ui_prefix;
            bin_ctxt_model_t *p_ctxt_abs_level;

            p_ctxt_abs_level =
                            ps_dec->p_coeff_abs_level_minus1_t[LUMA_8X8_CTXCAT];
            u4_ctx_inc = ((0x51));

            /*****************************************************/
            /* Main Loop runs for no. of Significant coefficient */
            /*****************************************************/
            do
            {
                {

                    /*****************************************************/
                    /* inilining a modified ih264d_decode_bins_unary     */
                    /*****************************************************/

                    {
                        UWORD32 u4_value;
                        UWORD32 u4_symbol;
                        bin_ctxt_model_t *ps_bin_ctxt;
                        UWORD32 u4_ctx_Inc;
                        u4_value = 0;

                        u4_ctx_Inc = u4_ctx_inc & 0xf;
                        ps_bin_ctxt = p_ctxt_abs_level + u4_ctx_Inc;

                        do
                        {
                            {
                                UWORD32 u4_qnt_int_range,
                                                u4_int_range_lps;
                                UWORD32 u1_mps_state;
                                UWORD32 table_lookup;
                                const UWORD32 *pu4_table =
                                                (const UWORD32 *)ps_cab_env->cabac_table;
                                UWORD32 u4_clz;

                                u1_mps_state = (ps_bin_ctxt->u1_mps_state);
                                u4_clz = CLZ(u4_code_int_range);
                                u4_qnt_int_range = u4_code_int_range
                                                << u4_clz;
                                u4_qnt_int_range = (u4_qnt_int_range
                                                >> 29) & 0x3;
                                table_lookup = pu4_table[(u1_mps_state << 2)
                                                + u4_qnt_int_range];
                                u4_int_range_lps = table_lookup & 0xff;

                                u4_int_range_lps = u4_int_range_lps
                                                << (23 - u4_clz);
                                u4_code_int_range = u4_code_int_range
                                                - u4_int_range_lps;
                                u4_symbol = ((u1_mps_state >> 6) & 0x1);
                                u1_mps_state = (table_lookup >> 8) & 0x7F;

                                CHECK_IF_LPS(u4_code_int_range,
                                             u4_code_int_val_ofst, u4_symbol,
                                             u4_int_range_lps, u1_mps_state,
                                             table_lookup)

                                if(u4_code_int_range < ONE_RIGHT_SHIFTED_BY_9)
                                {

                                    RENORM_RANGE_OFFSET(u4_code_int_range,
                                                        u4_code_int_val_ofst,
                                                        u4_offset, pu4_buffer)
                                }

                                ps_bin_ctxt->u1_mps_state = u1_mps_state;
                            }

                            u4_value++;
                            ps_bin_ctxt = p_ctxt_abs_level + (u4_ctx_inc >> 4);

                        }
                        while(u4_symbol && (u4_value < UCOFF_LEVEL));

                        ui_prefix = u4_value - 1 + u4_symbol;
                    }

                    if(ui_prefix == UCOFF_LEVEL)
                    {
                        UWORD32 ui16_sufS = 0;
                        UWORD32 u1_max_bins;
                        UWORD32 u4_value;

                        i2_abs_lvl = UCOFF_LEVEL;
                        /*inlining ih264d_decode_bypass_bins_unary begins*/

                        {
                            UWORD32 uc_bin;
                            UWORD32 bits_to_flush;


                            bits_to_flush = 0;
                            /*renormalize to ensure there 23 bits more in the u4_code_int_val_ofst*/
                            {
                                UWORD32 u4_clz, read_bits;

                                u4_clz = CLZ(u4_code_int_range);
                                FLUSHBITS(u4_offset, u4_clz)
                                NEXTBITS(read_bits, u4_offset, pu4_buffer, CABAC_BITS_TO_READ)
                                u4_code_int_range = u4_code_int_range << u4_clz;
                                u4_code_int_val_ofst = (u4_code_int_val_ofst
                                                << u4_clz) | read_bits;
                            }

                            do
                            {
                                bits_to_flush++;

                                u4_code_int_range = u4_code_int_range >> 1;

                                if(u4_code_int_val_ofst >= u4_code_int_range)
                                {
                                    /* S=1 */
                                    uc_bin = 1;
                                    u4_code_int_val_ofst -= u4_code_int_range;
                                }
                                else
                                {
                                    /* S=0 */
                                    uc_bin = 0;
                                }

                            }
                            while(uc_bin && (bits_to_flush < CABAC_BITS_TO_READ));

                            u4_value = (bits_to_flush - 1);
                        }
                        /*inlining ih264d_decode_bypass_bins_unary ends*/

                        ui16_sufS = (1 << u4_value);
                        u1_max_bins = u4_value;

                        if(u4_value > 0)
                        {
                            /*inline bypassbins_flc begins*/

                            if(u4_value > 10)
                            {
                                UWORD32 u4_clz, read_bits;

                                u4_clz = CLZ(u4_code_int_range);
                                FLUSHBITS(u4_offset, u4_clz)
                                NEXTBITS(read_bits, u4_offset, pu4_buffer, CABAC_BITS_TO_READ)
                                u4_code_int_range = u4_code_int_range << u4_clz;
                                u4_code_int_val_ofst = (u4_code_int_val_ofst
                                                << u4_clz) | read_bits;
                            }

                            {
                                UWORD32 ui_bins;
                                UWORD32 uc_bin;
                                UWORD32 bits_to_flush;

                                ui_bins = 0;
                                bits_to_flush = 0;

                                do
                                {
                                    bits_to_flush++;

                                    u4_code_int_range = u4_code_int_range >> 1;

                                    if(u4_code_int_val_ofst
                                                    >= u4_code_int_range)
                                    {
                                        /* S=1 */
                                        uc_bin = 1;
                                        u4_code_int_val_ofst -=
                                                        u4_code_int_range;
                                    }
                                    else
                                    {
                                        /* S=0 */
                                        uc_bin = 0;
                                    }

                                    ui_bins = ((ui_bins << 1) | uc_bin);

                                }
                                while(bits_to_flush < u1_max_bins);

                                u4_value = ui_bins;
                            }
                            /*inline bypassbins_flc ends*/
                        }

                        //Value of K
                        ui16_sufS += u4_value;
                        i2_abs_lvl += (WORD32)ui16_sufS;
                    }
                    else
                    {
                        i2_abs_lvl = 1 + ui_prefix;
                    }

                    if(i2_abs_lvl > 1)
                    {
                        u1_abs_level_gt1++;
                    }
                    if(!u1_abs_level_gt1)
                    {
                        u1_abs_level_equal1++;
                        u4_ctx_inc = (5 << 4) + MIN(u1_abs_level_equal1, 4);
                    }
                    else
                    {
                        u4_ctx_inc = (5 + MIN(u1_abs_level_gt1, 4)) << 4;
                    }

                    /*u4_ctx_inc = g_table_temp[u1_abs_level_gt1][u1_abs_level_equal1];*/

                    /* encode coeff_sign_flag[i] */

                    {
                        u4_code_int_range = u4_code_int_range >> 1;

                        if(u4_code_int_val_ofst >= (u4_code_int_range))
                        {
                            /* S=1 */
                            u4_code_int_val_ofst -= u4_code_int_range;
                            i2_abs_lvl = (-i2_abs_lvl);
                        }
                    }

                    *pi2_coeff_data++ = i2_abs_lvl;
                    num_sig_coeffs--;
                }
            }
            while(num_sig_coeffs > 0);
        }
    }

    {
        WORD32 offset;
        offset = (UWORD8 *)pi2_coeff_data - (UWORD8 *)ps_tu_8x8;
        offset = ALIGN4(offset);
        ps_dec->pv_parse_tu_coeff_data = (void *)((UWORD8 *)ps_dec->pv_parse_tu_coeff_data + offset);
    }

    /*updating structures*/
    ps_cab_env->u4_code_int_val_ofst = u4_code_int_val_ofst;
    ps_cab_env->u4_code_int_range = u4_code_int_range;
    ps_bitstrm->u4_ofst = u4_offset;
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_cabac_parse_8x8block                                     */
/*                                                                           */
/*  Description   : This function does the residual parsing of 4 subblocks   */
/*                  in a 8x8 block.                                          */
/*                                                                           */
/*  Inputs        : pi2_coeff_block : pointer to residual block where        */
/*                  decoded and inverse scan coefficients are updated        */
/*                                                                           */
/*                  u4_sub_block_strd : indicates the number of sublocks    */
/*                  in a row. It is 4 for luma and 2 for chroma.             */
/*                                                                           */
/*                  u4_ctx_cat : inidicates context category for residual    */
/*                  decoding.                                                */
/*                                                                           */
/*                  ps_dec : pointer to Decstruct (decoder context)          */
/*                                                                           */
/*                  pu1_top_nnz : top nnz pointer                            */
/*                                                                           */
/*                  pu1_left_nnz : left nnz pointer                          */
/*                                                                           */
/*  Globals       : No                                                       */
/*  Processing    : Parsing for four subblocks in unrolled, top and left nnz */
/*                  are updated on the fly. csbp is set in accordance to     */
/*                  decoded numcoeff for the subblock index in raster order  */
/*                                                                           */
/*  Outputs       : The updated residue buffer, nnzs and csbp current block  */
/*                                                                           */
/*  Returns       : Returns the coded sub block pattern csbp for the block   */
/*                                                                           */
/*  Issues        : <List any issues or problems with this function>         */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         09 10 2008   Jay          Draft                                   */
/*                                                                           */
/*****************************************************************************/
UWORD32 ih264d_cabac_parse_8x8block(WORD16 *pi2_coeff_block,
                                    UWORD32 u4_sub_block_strd,
                                    UWORD32 u4_ctx_cat,
                                    dec_struct_t * ps_dec,
                                    UWORD8 *pu1_top_nnz,
                                    UWORD8 *pu1_left_nnz)
{
    UWORD32 u4_ctxinc, u4_subblock_coded;
    UWORD32 u4_top0, u4_top1;
    UWORD32 u4_csbp = 0;
    UWORD32 u4_idx = 0;
    dec_bit_stream_t * const ps_bitstrm = ps_dec->ps_bitstrm;
    bin_ctxt_model_t * const ps_cbf = ps_dec->p_cbf_t[u4_ctx_cat];
    bin_ctxt_model_t *ps_src_bin_ctxt;
    bin_ctxt_model_t * const ps_sig_coeff_flag =
                    ps_dec->p_significant_coeff_flag_t[u4_ctx_cat];

    UWORD8 *pu1_inv_scan = ps_dec->pu1_inv_scan;

    /*------------------------------------------------------*/
    /* Residual 4x4 decoding: SubBlock 0                    */
    /*------------------------------------------------------*/
    u4_ctxinc = ((!!pu1_top_nnz[0]) << 1) + (!!pu1_left_nnz[0]);

    ps_src_bin_ctxt = ps_cbf + u4_ctxinc;

    u4_top0 = ih264d_read_coeff4x4_cabac( ps_bitstrm,
                                         u4_ctx_cat, ps_sig_coeff_flag, ps_dec,
                                         ps_src_bin_ctxt);

    INSERT_BIT(u4_csbp, u4_idx, u4_top0);

    /*------------------------------------------------------*/
    /* Residual 4x4 decoding: SubBlock 1                    */
    /*------------------------------------------------------*/
    u4_idx++;
    pi2_coeff_block += NUM_COEFFS_IN_4x4BLK;
    u4_ctxinc = ((!!pu1_top_nnz[1]) << 1) + u4_top0;

    ps_src_bin_ctxt = ps_cbf + u4_ctxinc;

    u4_top1 = ih264d_read_coeff4x4_cabac(ps_bitstrm,
                                         u4_ctx_cat, ps_sig_coeff_flag, ps_dec,
                                         ps_src_bin_ctxt);

    INSERT_BIT(u4_csbp, u4_idx, u4_top1);
    pu1_left_nnz[0] = u4_top1;

    /*------------------------------------------------------*/
    /* Residual 4x4 decoding: SubBlock 2                    */
    /*------------------------------------------------------*/
    u4_idx += (u4_sub_block_strd - 1);
    pi2_coeff_block += ((u4_sub_block_strd - 1) * NUM_COEFFS_IN_4x4BLK);
    u4_ctxinc = (u4_top0 << 1) + (!!pu1_left_nnz[1]);

    ps_src_bin_ctxt = ps_cbf + u4_ctxinc;

    u4_subblock_coded = ih264d_read_coeff4x4_cabac(ps_bitstrm, u4_ctx_cat,
                                                   ps_sig_coeff_flag, ps_dec,
                                                   ps_src_bin_ctxt);

    INSERT_BIT(u4_csbp, u4_idx, u4_subblock_coded);
    pu1_top_nnz[0] = u4_subblock_coded;

    /*------------------------------------------------------*/
    /* Residual 4x4 decoding: SubBlock 3                    */
    /*------------------------------------------------------*/
    u4_idx++;
    pi2_coeff_block += NUM_COEFFS_IN_4x4BLK;
    u4_ctxinc = (u4_top1 << 1) + u4_subblock_coded;

    ps_src_bin_ctxt = ps_cbf + u4_ctxinc;

    u4_subblock_coded = ih264d_read_coeff4x4_cabac(ps_bitstrm, u4_ctx_cat,
                                                   ps_sig_coeff_flag, ps_dec,
                                                   ps_src_bin_ctxt);

    INSERT_BIT(u4_csbp, u4_idx, u4_subblock_coded);
    pu1_top_nnz[1] = pu1_left_nnz[1] = u4_subblock_coded;

    return (u4_csbp);
}

/*!
 **************************************************************************
 * \if Function name : ih264d_parse_residual4x4_cabac \endif
 *
 * \brief
 *    This function parses CABAC syntax of a Luma and Chroma AC Residuals.
 *
 * \return
 *    0 on Success and Error code otherwise
 **************************************************************************
 */

WORD32 ih264d_parse_residual4x4_cabac(dec_struct_t * ps_dec,
                                      dec_mb_info_t *ps_cur_mb_info,
                                      UWORD8 u1_offset)
{
    UWORD8 u1_cbp = ps_cur_mb_info->u1_cbp;
    UWORD16 ui16_csbp = 0;
    WORD16 *pi2_residual_buf;
    UWORD8 uc_ctx_cat;
    UWORD8 *pu1_top_nnz = ps_cur_mb_info->ps_curmb->pu1_nnz_y;
    UWORD8 *pu1_left_nnz = ps_dec->pu1_left_nnz_y;
    UWORD8 *pu1_top_nnz_uv = ps_cur_mb_info->ps_curmb->pu1_nnz_uv;
    ctxt_inc_mb_info_t *p_curr_ctxt = ps_dec->ps_curr_ctxt_mb_info;
    ctxt_inc_mb_info_t *ps_top_ctxt = ps_dec->p_top_ctxt_mb_info;
    dec_bit_stream_t * const ps_bitstrm = ps_dec->ps_bitstrm;
    UWORD32 u4_nbr_avail = ps_dec->u1_mb_ngbr_availablity;
    WORD16 *pi2_coeff_block = NULL;
    bin_ctxt_model_t *ps_src_bin_ctxt;

    UWORD8 u1_top_dc_csbp = (ps_top_ctxt->u1_yuv_dc_csbp) >> 1;
    UWORD8 u1_left_dc_csbp = (ps_dec->pu1_left_yuv_dc_csbp[0]) >> 1;


    if(!(u4_nbr_avail & TOP_MB_AVAILABLE_MASK))
    {
        if(p_curr_ctxt->u1_mb_type & CAB_INTRA_MASK)
        {
            *(UWORD32 *)pu1_top_nnz = 0;
            u1_top_dc_csbp = 0;
            *(UWORD32 *)pu1_top_nnz_uv = 0;
        }
        else
        {
            *(UWORD32 *)pu1_top_nnz = 0x01010101;
            u1_top_dc_csbp = 0x3;
            *(UWORD32 *)pu1_top_nnz_uv = 0x01010101;
        }
    }
    else
    {
        UWORD32 *pu4_buf;
        UWORD8 *pu1_buf;
        pu1_buf = ps_cur_mb_info->ps_top_mb->pu1_nnz_y;
        pu4_buf = (UWORD32 *)pu1_buf;
        *(UWORD32 *)(pu1_top_nnz) = *pu4_buf;

        pu1_buf = ps_cur_mb_info->ps_top_mb->pu1_nnz_uv;
        pu4_buf = (UWORD32 *)pu1_buf;
        *(UWORD32 *)(pu1_top_nnz_uv) = *pu4_buf;

    }

    if(!(u4_nbr_avail & LEFT_MB_AVAILABLE_MASK))
    {
        if(p_curr_ctxt->u1_mb_type & CAB_INTRA_MASK)
        {
            UWORD32 *pu4_buf;
            UWORD8 *pu1_buf;
            *(UWORD32 *)pu1_left_nnz = 0;
            u1_left_dc_csbp = 0;
            pu1_buf = ps_dec->pu1_left_nnz_uv;
            pu4_buf = (UWORD32 *)pu1_buf;
            *pu4_buf = 0;
        }
        else
        {
            UWORD32 *pu4_buf;
            UWORD8 *pu1_buf;
            *(UWORD32 *)pu1_left_nnz = 0x01010101;
            u1_left_dc_csbp = 0x3;
            pu1_buf = ps_dec->pu1_left_nnz_uv;
            pu4_buf = (UWORD32 *)pu1_buf;
            *pu4_buf = 0x01010101;
        }
    }

    uc_ctx_cat = u1_offset ? LUMA_AC_CTXCAT : LUMA_4X4_CTXCAT;

    ps_cur_mb_info->u1_qp_div6 = ps_dec->u1_qp_y_div6;
    ps_cur_mb_info->u1_qpc_div6 = ps_dec->u1_qp_u_div6;
    ps_cur_mb_info->u1_qp_rem6 = ps_dec->u1_qp_y_rem6;
    ps_cur_mb_info->u1_qpc_rem6 = ps_dec->u1_qp_u_rem6;
    // CHECK_THIS
    ps_cur_mb_info->u1_qpcr_div6 = ps_dec->u1_qp_v_div6;
    ps_cur_mb_info->u1_qpcr_rem6 = ps_dec->u1_qp_v_rem6;

    if(u1_cbp & 0x0f)
    {
        if(ps_cur_mb_info->u1_tran_form8x8 == 0)
        {
            /*******************************************************************/
            /* Block 0 residual decoding, check cbp and proceed (subblock = 0) */
            /*******************************************************************/
            if(!(u1_cbp & 0x1))
            {
                *(UWORD16 *)(pu1_top_nnz) = 0;
                *(UWORD16 *)(pu1_left_nnz) = 0;
            }
            else
            {
                ui16_csbp = ih264d_cabac_parse_8x8block(pi2_coeff_block, 4,
                                                        uc_ctx_cat, ps_dec,
                                                        pu1_top_nnz,
                                                        pu1_left_nnz);
            }

            /*******************************************************************/
            /* Block 1 residual decoding, check cbp and proceed (subblock = 2) */
            /*******************************************************************/
            pi2_coeff_block += (2 * NUM_COEFFS_IN_4x4BLK);
            if(!(u1_cbp & 0x2))
            {
                *(UWORD16 *)(pu1_top_nnz + 2) = 0;
                *(UWORD16 *)(pu1_left_nnz) = 0;
            }
            else
            {
                UWORD32 u4_temp = ih264d_cabac_parse_8x8block(pi2_coeff_block,
                                                              4, uc_ctx_cat,
                                                              ps_dec,
                                                              (pu1_top_nnz + 2),
                                                              pu1_left_nnz);
                ui16_csbp |= (u4_temp << 2);
            }

            /*******************************************************************/
            /* Block 2 residual decoding, check cbp and proceed (subblock = 8) */
            /*******************************************************************/
            pi2_coeff_block += (6 * NUM_COEFFS_IN_4x4BLK);
            if(!(u1_cbp & 0x4))
            {
                *(UWORD16 *)(pu1_top_nnz) = 0;
                *(UWORD16 *)(pu1_left_nnz + 2) = 0;
            }
            else
            {
                UWORD32 u4_temp = ih264d_cabac_parse_8x8block(
                                pi2_coeff_block, 4, uc_ctx_cat, ps_dec,
                                pu1_top_nnz, (pu1_left_nnz + 2));
                ui16_csbp |= (u4_temp << 8);
            }

            /*******************************************************************/
            /* Block 3 residual decoding, check cbp and proceed (subblock = 10)*/
            /*******************************************************************/
            pi2_coeff_block += (2 * NUM_COEFFS_IN_4x4BLK);
            if(!(u1_cbp & 0x8))
            {
                *(UWORD16 *)(pu1_top_nnz + 2) = 0;
                *(UWORD16 *)(pu1_left_nnz + 2) = 0;
            }
            else
            {
                UWORD32 u4_temp = ih264d_cabac_parse_8x8block(
                                pi2_coeff_block, 4, uc_ctx_cat, ps_dec,
                                (pu1_top_nnz + 2), (pu1_left_nnz + 2));
                ui16_csbp |= (u4_temp << 10);
            }

        }
        else
        {
            ui16_csbp = 0;

            /*******************************************************************/
            /* Block 0 residual decoding, check cbp and proceed (subblock = 0) */
            /*******************************************************************/
            if(!(u1_cbp & 0x1))
            {
                *(UWORD16 *)(pu1_top_nnz) = 0;
                *(UWORD16 *)(pu1_left_nnz) = 0;
            }
            else
            {

                dec_bit_stream_t * const ps_bitstrm = ps_dec->ps_bitstrm;

                ih264d_read_coeff8x8_cabac( ps_bitstrm,
                                           ps_dec, ps_cur_mb_info);

                pu1_left_nnz[0] = 1;
                pu1_left_nnz[1] = 1;

                pu1_top_nnz[0] = 1;
                pu1_top_nnz[1] = 1;

                /* added to be used by BS computation module */
                ui16_csbp |= 0x0033;
            }

            /*******************************************************************/
            /* Block 1 residual decoding, check cbp and proceed (subblock = 2) */
            /*******************************************************************/
            pi2_coeff_block += 64;

            if(!(u1_cbp & 0x2))
            {
                *(UWORD16 *)(pu1_top_nnz + 2) = 0;
                *(UWORD16 *)(pu1_left_nnz) = 0;
            }
            else
            {


                dec_bit_stream_t * const ps_bitstrm = ps_dec->ps_bitstrm;

                ih264d_read_coeff8x8_cabac(ps_bitstrm,
                                           ps_dec, ps_cur_mb_info);

                pu1_left_nnz[0] = 1;
                pu1_left_nnz[1] = 1;

                pu1_top_nnz[2] = 1;
                pu1_top_nnz[3] = 1;

                /* added to be used by BS computation module */
                ui16_csbp |= 0x00CC;

            }

            /*******************************************************************/
            /* Block 2 residual decoding, check cbp and proceed (subblock = 8) */
            /*******************************************************************/
            pi2_coeff_block += 64;
            if(!(u1_cbp & 0x4))
            {
                *(UWORD16 *)(pu1_top_nnz) = 0;
                *(UWORD16 *)(pu1_left_nnz + 2) = 0;
            }
            else
            {

                dec_bit_stream_t * const ps_bitstrm = ps_dec->ps_bitstrm;

                ih264d_read_coeff8x8_cabac(ps_bitstrm,
                                           ps_dec, ps_cur_mb_info);

                pu1_left_nnz[2] = 1;
                pu1_left_nnz[3] = 1;

                pu1_top_nnz[0] = 1;
                pu1_top_nnz[1] = 1;

                /* added to be used by BS computation module */
                ui16_csbp |= 0x3300;
            }

            /*******************************************************************/
            /* Block 3 residual decoding, check cbp and proceed (subblock = 10)*/
            /*******************************************************************/
            pi2_coeff_block += 64;

            if(!(u1_cbp & 0x8))
            {
                *(UWORD16 *)(pu1_top_nnz + 2) = 0;
                *(UWORD16 *)(pu1_left_nnz + 2) = 0;
            }
            else
            {

                dec_bit_stream_t * const ps_bitstrm = ps_dec->ps_bitstrm;

                ih264d_read_coeff8x8_cabac(ps_bitstrm,
                                           ps_dec, ps_cur_mb_info);

                pu1_left_nnz[2] = 1;
                pu1_left_nnz[3] = 1;

                pu1_top_nnz[2] = 1;
                pu1_top_nnz[3] = 1;

                /* added to be used by BS computation module */
                ui16_csbp |= 0xCC00;
            }
        }
    }
    else
    {
        *(UWORD32 *)(pu1_top_nnz) = 0;
        *(UWORD32 *)(pu1_left_nnz) = 0;
    }
    /*--------------------------------------------------------------------*/
    /* Store the last row of N values to top row                          */
    /*--------------------------------------------------------------------*/
    ps_cur_mb_info->u2_luma_csbp = ui16_csbp;
    ps_cur_mb_info->ps_curmb->u2_luma_csbp = ui16_csbp;
    {
        WORD8 i;
        UWORD16 u2_chroma_csbp = 0;
        ps_cur_mb_info->u2_chroma_csbp = 0;

        u1_cbp >>= 4;
        pu1_top_nnz = pu1_top_nnz_uv;
        pu1_left_nnz = ps_dec->pu1_left_nnz_uv;
        /*--------------------------------------------------------------------*/
        /* if Chroma Component not present OR no ac values present            */
        /* Set the values of N to zero                                        */
        /*--------------------------------------------------------------------*/
        if(u1_cbp == CBPC_ALLZERO)
        {
            ps_dec->pu1_left_yuv_dc_csbp[0] &= 0x1;
            *(UWORD32 *)(pu1_top_nnz) = 0;
            *(UWORD32 *)(pu1_left_nnz) = 0;
            p_curr_ctxt->u1_yuv_dc_csbp &= 0x1;
            return (0);
        }

        /*--------------------------------------------------------------------*/
        /* Decode Chroma DC values                                            */
        /*--------------------------------------------------------------------*/
        for(i = 0; i < 2; i++)
        {
            UWORD8 uc_a = 1, uc_b = 1;
            UWORD32 u4_ctx_inc;
            UWORD8 uc_codedBlockFlag;
            UWORD8 pu1_inv_scan[4] =
                { 0, 1, 2, 3 };
            WORD32 u4_scale;
            WORD32 i4_mb_inter_inc;
            tu_sblk4x4_coeff_data_t *ps_tu_4x4 =
                            (tu_sblk4x4_coeff_data_t *)ps_dec->pv_parse_tu_coeff_data;
            WORD16 *pi2_coeff_data =
                            (WORD16 *)ps_dec->pv_parse_tu_coeff_data;
            WORD16 ai2_dc_coef[4];

            INC_SYM_COUNT(&(ps_dec->s_cab_dec_env));
            u4_scale = (i) ?
                            (ps_dec->pu2_quant_scale_v[0]
                                            << ps_dec->u1_qp_v_div6) :
                            (ps_dec->pu2_quant_scale_u[0]
                                            << ps_dec->u1_qp_u_div6);

            /*--------------------------------------------------------------------*/
            /* Decode Bitstream to get the DC coeff                               */
            /*--------------------------------------------------------------------*/
            uc_a = (u1_left_dc_csbp >> i) & 0x01;
            uc_b = (u1_top_dc_csbp >> i) & 0x01;
            u4_ctx_inc = (uc_a + (uc_b << 1));

            ps_src_bin_ctxt = (ps_dec->p_cbf_t[CHROMA_DC_CTXCAT]) + u4_ctx_inc;

            uc_codedBlockFlag =
                            ih264d_read_coeff4x4_cabac(ps_bitstrm,
                                            CHROMA_DC_CTXCAT,
                                            ps_dec->p_significant_coeff_flag_t[CHROMA_DC_CTXCAT],
                                            ps_dec, ps_src_bin_ctxt);

            i4_mb_inter_inc = (!((ps_cur_mb_info->ps_curmb->u1_mb_type == I_4x4_MB)
                            || (ps_cur_mb_info->ps_curmb->u1_mb_type == I_16x16_MB)))
                            * 3;

            if(ps_dec->s_high_profile.u1_scaling_present)
            {
                u4_scale *=
                                ps_dec->s_high_profile.i2_scalinglist4x4[i4_mb_inter_inc
                                                + 1 + i][0];

            }
            else
            {
                u4_scale <<= 4;
            }

            if(uc_codedBlockFlag)
            {
                WORD32 i_z0, i_z1, i_z2, i_z3;
                WORD32 *pi4_scale;

                SET_BIT(u1_top_dc_csbp, i);
                SET_BIT(u1_left_dc_csbp, i);

                ai2_dc_coef[0] = 0;
                ai2_dc_coef[1] = 0;
                ai2_dc_coef[2] = 0;
                ai2_dc_coef[3] = 0;

                ih264d_unpack_coeff4x4_dc_4x4blk(ps_tu_4x4,
                                                 ai2_dc_coef,
                                                 pu1_inv_scan);
                i_z0 = (ai2_dc_coef[0] + ai2_dc_coef[2]);
                i_z1 = (ai2_dc_coef[0] - ai2_dc_coef[2]);
                i_z2 = (ai2_dc_coef[1] - ai2_dc_coef[3]);
                i_z3 = (ai2_dc_coef[1] + ai2_dc_coef[3]);

                /*-----------------------------------------------------------*/
                /* Scaling and storing the values back                       */
                /*-----------------------------------------------------------*/
                *pi2_coeff_data++ = ((i_z0 + i_z3) * u4_scale) >> 5;
                *pi2_coeff_data++ = ((i_z0 - i_z3) * u4_scale) >> 5;
                *pi2_coeff_data++ = ((i_z1 + i_z2) * u4_scale) >> 5;
                *pi2_coeff_data++ = ((i_z1 - i_z2) * u4_scale) >> 5;

                ps_dec->pv_parse_tu_coeff_data = (void *)pi2_coeff_data;

                SET_BIT(ps_cur_mb_info->u1_yuv_dc_block_flag,(i+1));
            }
            else
            {
                CLEARBIT(u1_top_dc_csbp, i);
                CLEARBIT(u1_left_dc_csbp, i);
            }
        }

        /*********************************************************************/
        /*                   Update the DC csbp                              */
        /*********************************************************************/
        ps_dec->pu1_left_yuv_dc_csbp[0] &= 0x1;
        p_curr_ctxt->u1_yuv_dc_csbp &= 0x1;
        ps_dec->pu1_left_yuv_dc_csbp[0] |= (u1_left_dc_csbp << 1);
        p_curr_ctxt->u1_yuv_dc_csbp |= (u1_top_dc_csbp << 1);
        if(u1_cbp == CBPC_ACZERO)
        {
            *(UWORD32 *)(pu1_top_nnz) = 0;
            *(UWORD32 *)(pu1_left_nnz) = 0;
            return (0);
        }
        /*--------------------------------------------------------------------*/
        /* Decode Chroma AC values                                            */
        /*--------------------------------------------------------------------*/
        {
            UWORD32 u4_temp;
            /*****************************************************************/
            /* U Block  residual decoding, check cbp and proceed (subblock=0)*/
            /*****************************************************************/
            u2_chroma_csbp = ih264d_cabac_parse_8x8block(pi2_coeff_block, 2,
            CHROMA_AC_CTXCAT,
                                                         ps_dec, pu1_top_nnz,
                                                         pu1_left_nnz);

            pi2_coeff_block += MB_CHROM_SIZE;
            /*****************************************************************/
            /* V Block  residual decoding, check cbp and proceed (subblock=1)*/
            /*****************************************************************/
            u4_temp = ih264d_cabac_parse_8x8block(pi2_coeff_block, 2,
            CHROMA_AC_CTXCAT,
                                                  ps_dec, (pu1_top_nnz + 2),
                                                  (pu1_left_nnz + 2));
            u2_chroma_csbp |= (u4_temp << 4);
        }
        /*********************************************************************/
        /*                   Update the AC csbp                              */
        /*********************************************************************/
        ps_cur_mb_info->u2_chroma_csbp = u2_chroma_csbp;
    }

    return (0);
}

