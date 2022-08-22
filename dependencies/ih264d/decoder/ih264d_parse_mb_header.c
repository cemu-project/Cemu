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
 * \file ih264d_parse_mb_header.c
 *
 * \brief
 *    This file contains context identifier encoding routines.
 *
 * \date
 *    04/02/2003
 *
 * \author  NS
 ***************************************************************************
 */
#include <string.h>
#include "ih264d_structs.h"
#include "ih264d_bitstrm.h"
#include "ih264d_cabac.h"
#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264d_defs.h"
#include "ih264d_error_handler.h"
#include "ih264d_tables.h"
#include "ih264d_debug.h"
#include "ih264d_defs.h"
#include "ih264d_defs.h"
#include "ih264d_mb_utils.h"
#include "ih264d_parse_mb_header.h"
#include "ih264d_defs.h"

/*! < CtxtInc index 0 - CtxMbTypeI, CtxMbTypeSISuffix
 index 1 - CtxMbTypePSuffix, CtxMbTypeBSuffix
 */



/*!
 **************************************************************************
 * \if Function name : ih264d_parse_mb_type_intra_cabac \endif
 *
 * \brief
 *    This function decodes MB type using CABAC entropy coding mode.
 *
 * \return
 *    MBType.
 *
 **************************************************************************
 */
UWORD8 ih264d_parse_mb_type_intra_cabac(UWORD8 u1_inter,
                                        struct _DecStruct * ps_dec)
{
    decoding_envirnoment_t * ps_cab_env = &ps_dec->s_cab_dec_env;
    dec_bit_stream_t * ps_bitstrm = ps_dec->ps_bitstrm;
    ctxt_inc_mb_info_t * ps_left_ctxt = ps_dec->p_left_ctxt_mb_info;
    ctxt_inc_mb_info_t * ps_top_ctxt = ps_dec->p_top_ctxt_mb_info;
    bin_ctxt_model_t *ps_mb_bin_ctxt = ps_dec->p_mb_type_t;
    WORD8 u1_mb_type, u1_bin;
    UWORD32 u4_cxt_inc;

    u4_cxt_inc = 0;
    if(!u1_inter)
    {
        if(ps_left_ctxt != ps_dec->ps_def_ctxt_mb_info)
            u4_cxt_inc += ((ps_left_ctxt->u1_mb_type != CAB_I4x4) ? 1 : 0);
        if(ps_top_ctxt != ps_dec->ps_def_ctxt_mb_info)
            u4_cxt_inc += ((ps_top_ctxt->u1_mb_type != CAB_I4x4) ? 1 : 0);
    }
    else
    {
        ps_mb_bin_ctxt = ps_mb_bin_ctxt + 3 + (ps_dec->u1_B << 1);
    }

    /* b0 */
    u1_mb_type = (UWORD8)ih264d_decode_bin(u4_cxt_inc, ps_mb_bin_ctxt, ps_bitstrm,
                                          ps_cab_env);
    if(u1_mb_type)
    {
        /* I16x16 or I_PCM mode */
        /* b1 */
        u1_bin = ih264d_decode_terminate(ps_cab_env, ps_bitstrm);
        if(u1_bin == 0)
        {
            /* I16x16 mode */
            /* Read b2 and b3 */
            u4_cxt_inc = (u1_inter) ? 0x021 : 0x043;

            u1_bin = ih264d_decode_bins(2, u4_cxt_inc, ps_mb_bin_ctxt, ps_bitstrm,
                                        ps_cab_env);

            if(u1_bin & 0x01)
                u1_mb_type += 4;

            if(u1_bin & 0x02)
                u1_mb_type += 12;

            if(u1_bin & 0x01)
            {
                /* since b3=1, Read three bins */
                u4_cxt_inc = (u1_inter) ? 0x0332 : 0x0765;
                u1_bin = (UWORD8)ih264d_decode_bins(3, u4_cxt_inc, ps_mb_bin_ctxt,
                                                    ps_bitstrm, ps_cab_env);

            }
            else
            {
                /* Read two bins */
                u4_cxt_inc = (u1_inter) ? 0x033 : 0x076;
                u1_bin = (UWORD8)ih264d_decode_bins(2, u4_cxt_inc, ps_mb_bin_ctxt,
                                                    ps_bitstrm, ps_cab_env);
            }
            u1_mb_type += u1_bin;
        }
        else
        {
            /* I_PCM mode */
            /* b1=1 */
            u1_mb_type = 25;
        }
    }
    return (u1_mb_type);
}

/*!
 **************************************************************************
 * \if Function name : ih264d_parse_mb_type_cabac \endif
 *
 * \brief
 *    This function decodes MB type using CABAC entropy coding mode.
 *
 * \return
 *    MBType.
 *
 **************************************************************************
 */
UWORD32 ih264d_parse_mb_type_cabac(struct _DecStruct * ps_dec)
{
    const UWORD8 uc_slice_type = ps_dec->ps_cur_slice->u1_slice_type;
    decoding_envirnoment_t *ps_cab_env = &ps_dec->s_cab_dec_env;
    dec_bit_stream_t *ps_bitstrm = ps_dec->ps_bitstrm;
    ctxt_inc_mb_info_t *ps_left_ctxt = ps_dec->p_left_ctxt_mb_info;
    ctxt_inc_mb_info_t *ps_top_ctxt = ps_dec->p_top_ctxt_mb_info;
    WORD8 c_ctxt_inc;
    bin_ctxt_model_t *ps_mb_bin_ctxt = ps_dec->p_mb_type_t;
    WORD8 u1_mb_type = 0, u1_bin;
    UWORD32 u4_cxt_inc;

    INC_SYM_COUNT(ps_cab_env);

    c_ctxt_inc = 0;

    if(uc_slice_type == SI_SLICE)
    {
        /* b0 */
        if(ps_left_ctxt != ps_dec->ps_def_ctxt_mb_info)
            c_ctxt_inc += ((ps_left_ctxt->u1_mb_type != CAB_SI4x4) ? 1 : 0);
        if(ps_top_ctxt != ps_dec->ps_def_ctxt_mb_info)
            c_ctxt_inc += ((ps_top_ctxt->u1_mb_type != CAB_SI4x4) ? 1 : 0);

        u4_cxt_inc = c_ctxt_inc;
        u1_bin = (UWORD8)ih264d_decode_bin(u4_cxt_inc, ps_mb_bin_ctxt, ps_bitstrm,
                                           ps_cab_env);
        if(u1_bin == 0)
        {
            /* SI MB */
            u1_mb_type = 0;
        }
        else
        {
            u1_mb_type = 1 + ih264d_parse_mb_type_intra_cabac(0, ps_dec);
        }
    }
    else if(uc_slice_type == P_SLICE)
    {
        /* P Slice */
        /* b0 */
        u4_cxt_inc = 0;
        u1_bin = (UWORD8)ih264d_decode_bin(u4_cxt_inc, ps_mb_bin_ctxt, ps_bitstrm,
                                           ps_cab_env);
        if(!u1_bin)
        {
            /* Inter MB types */
            /* b1 */
            u4_cxt_inc = 0x01;
            u1_bin = (UWORD8)ih264d_decode_bin(u4_cxt_inc, ps_mb_bin_ctxt,
                                               ps_bitstrm, ps_cab_env);
            /* b2 */
            u4_cxt_inc = u1_bin + 2;
            u1_mb_type = (UWORD8)ih264d_decode_bin(u4_cxt_inc, ps_mb_bin_ctxt,
                                                  ps_bitstrm, ps_cab_env);
            u1_mb_type = (u1_bin << 1) + u1_mb_type;
            if(u1_mb_type)
                u1_mb_type = 4 - u1_mb_type;
        }
        else
        {
            /* Intra Prefix 1 found */
            /* Intra MB type */
            u1_mb_type = 5 + ih264d_parse_mb_type_intra_cabac(1, ps_dec);
        }
    }
    else if(uc_slice_type == B_SLICE)
    {
        WORD8 a, b;
        /* B Slice */
        /* b0 */
        /* a = b = 0, if B slice and MB is a SKIP or B_DIRECT16x16 */
        a = 0;
        b = 0;
        u1_mb_type = 0;
        if(ps_left_ctxt != ps_dec->ps_def_ctxt_mb_info)
            a = ((ps_left_ctxt->u1_mb_type & CAB_BD16x16_MASK) != CAB_BD16x16);
        if(ps_top_ctxt != ps_dec->ps_def_ctxt_mb_info)
            b = ((ps_top_ctxt->u1_mb_type & CAB_BD16x16_MASK) != CAB_BD16x16);

        u4_cxt_inc = a + b;

        u1_bin = (UWORD8)ih264d_decode_bin(u4_cxt_inc, ps_mb_bin_ctxt, ps_bitstrm,
                                           ps_cab_env);

        if(u1_bin)
        {

            /* b1 */
            u4_cxt_inc = 0x03;
            u1_bin = (UWORD8)ih264d_decode_bin(u4_cxt_inc, ps_mb_bin_ctxt,
                                               ps_bitstrm, ps_cab_env);

            if(!u1_bin)
            {
                /* b2 */
                u4_cxt_inc = 0x05;
                u1_bin = (UWORD8)ih264d_decode_bin(u4_cxt_inc, ps_mb_bin_ctxt,
                                                   ps_bitstrm, ps_cab_env);

                u1_mb_type = u1_bin + 1;
            }
            else
            {
                u1_mb_type = 3;
                /* b2 */
                u4_cxt_inc = 0x04;
                u1_bin = (UWORD8)ih264d_decode_bin(u4_cxt_inc, ps_mb_bin_ctxt,
                                                   ps_bitstrm, ps_cab_env);

                if(u1_bin)
                {
                    u1_mb_type += 8;
                    /* b3 */
                    u4_cxt_inc = 0x05;
                    u1_bin = (UWORD8)ih264d_decode_bin(u4_cxt_inc, ps_mb_bin_ctxt,
                                                       ps_bitstrm, ps_cab_env);

                    if(!u1_bin)
                    {
                        u1_mb_type++;
                        /* b4, b5, b6 */
                        u4_cxt_inc = 0x0555;
                        u1_bin = (UWORD8)ih264d_decode_bins(3, u4_cxt_inc,
                                                            ps_mb_bin_ctxt,
                                                            ps_bitstrm,
                                                            ps_cab_env);



                        u1_mb_type += u1_bin;
                    }
                    else
                    {
                        /* b4 */
                        u4_cxt_inc = 0x05;
                        u1_bin = (UWORD8)ih264d_decode_bin(u4_cxt_inc,
                                                           ps_mb_bin_ctxt,
                                                           ps_bitstrm,
                                                           ps_cab_env);

                        if(u1_bin)
                        {
                            /* b5 */
                            u1_bin = (UWORD8)ih264d_decode_bin(u4_cxt_inc,
                                                               ps_mb_bin_ctxt,
                                                               ps_bitstrm,
                                                               ps_cab_env);

                            u1_mb_type += (u1_bin ? 11 : 0);
                        }
                        else
                        {
                            u1_mb_type = 20;
                            /* b5 */
                            u1_bin = (UWORD8)ih264d_decode_bin(u4_cxt_inc,
                                                               ps_mb_bin_ctxt,
                                                               ps_bitstrm,
                                                               ps_cab_env);

                            if(!u1_bin)
                            {
                                /* b6 */
                                u1_bin = (UWORD8)ih264d_decode_bin(u4_cxt_inc,
                                                                   ps_mb_bin_ctxt,
                                                                   ps_bitstrm,
                                                                   ps_cab_env);

                                u1_mb_type += u1_bin;
                            }
                            else
                            {
                                /* Intra Prefix 111101 found */
                                /* Intra MB type */
                                u1_mb_type =
                                                23
                                                                + ih264d_parse_mb_type_intra_cabac(
                                                                                1,
                                                                                ps_dec);
                            }
                        }
                    }
                }
                else
                {
                    /* b3, b4, b5 */
                    u4_cxt_inc = 0x0555;
                    u1_bin = (UWORD8)ih264d_decode_bins(3, u4_cxt_inc,
                                                        ps_mb_bin_ctxt, ps_bitstrm,
                                                        ps_cab_env);




                    u1_mb_type += u1_bin;
                }
            }
        }
    }
    return ((UWORD32)u1_mb_type);
}

/*!
 **************************************************************************
 * \if Function name : DecSubMBType \endif
 *
 * \brief
 *    This function decodes MB type using CABAC entropy coding mode.
 *
 * \return
 *    MBType.
 *
 **************************************************************************
 */
UWORD32 ih264d_parse_submb_type_cabac(const UWORD8 u1_slc_type_b,
                                      decoding_envirnoment_t * ps_cab_env,
                                      dec_bit_stream_t * ps_bitstrm,
                                      bin_ctxt_model_t * ps_sub_mb_cxt)
{
    WORD8 u1_sub_mb_type, u1_bin;

    INC_SYM_COUNT(ps_cab_env);

    u1_sub_mb_type = 0;
    u1_bin = (UWORD8)ih264d_decode_bin(0, ps_sub_mb_cxt, ps_bitstrm,
                                       ps_cab_env);

    if(u1_slc_type_b ^ u1_bin)
        return 0;

    if(!u1_slc_type_b)
    {
        /* P Slice */
        u1_sub_mb_type = 1;
        u1_bin = (UWORD8)ih264d_decode_bin(1, ps_sub_mb_cxt, ps_bitstrm,
                                           ps_cab_env);
        if(u1_bin == 1)
        {
            u1_bin = (UWORD8)ih264d_decode_bin(2, ps_sub_mb_cxt, ps_bitstrm,
                                               ps_cab_env);
            u1_sub_mb_type = (2 + (!u1_bin));
        }

        return u1_sub_mb_type;
    }
    else
    {
        /* B Slice */

        /* b1 */
        u1_bin = (UWORD8)ih264d_decode_bin(1, ps_sub_mb_cxt, ps_bitstrm,
                                           ps_cab_env);
        if(u1_bin)
        {
            /* b2 */
            u1_bin = (UWORD8)ih264d_decode_bin(2, ps_sub_mb_cxt, ps_bitstrm,
                                               ps_cab_env);
            if(u1_bin)
            {
                /* b3 */
                u1_sub_mb_type = 7;
                u1_bin = (UWORD8)ih264d_decode_bin(3, ps_sub_mb_cxt, ps_bitstrm,
                                                   ps_cab_env);
                u1_sub_mb_type += u1_bin << 2;
                u1_bin = !u1_bin;
                /* b4 */
                if(u1_bin == 0)
                {
                    u1_bin = ih264d_decode_bin(3, ps_sub_mb_cxt, ps_bitstrm,
                                               ps_cab_env);
                }
                else
                {
                    u1_bin = (UWORD8)ih264d_decode_bins(2, 0x33, ps_sub_mb_cxt,
                                                        ps_bitstrm, ps_cab_env);
                }

                return (u1_sub_mb_type + u1_bin);
            }
            else
            {
                /* b3 */
                u1_bin = (UWORD8)ih264d_decode_bins(2, 0x33, ps_sub_mb_cxt,
                                                    ps_bitstrm, ps_cab_env);
                return (3 + u1_bin);
            }
        }
        else
        {
            /* b2 */
            u1_bin = (UWORD8)ih264d_decode_bin(3, ps_sub_mb_cxt, ps_bitstrm,
                                               ps_cab_env);
            return (1 + u1_bin);
        }
    }
}

/*!
 **************************************************************************
 * \if Function name : ih264d_parse_ref_idx_cabac \endif
 *
 * \brief
 *    This function decodes Reference Index using CABAC entropy coding mode.
 *
 * \return
 *    None
 *
 **************************************************************************
 */
WORD32 ih264d_parse_ref_idx_cabac(const UWORD8 u1_num_part,
                                const UWORD8 u1_b2,
                                const UWORD8 u1_max_ref_minus1,
                                const UWORD8 u1_mb_mode,
                                WORD8 * pi1_ref_idx,
                                WORD8 * const pi1_lft_cxt,
                                WORD8 * const pi1_top_cxt,
                                decoding_envirnoment_t * const ps_cab_env,
                                dec_bit_stream_t * const ps_bitstrm,
                                bin_ctxt_model_t * const ps_ref_cxt)
{
    UWORD8 u1_a, u1_b;
    UWORD32 u4_cxt_inc;
    UWORD8 u1_blk_no, u1_i, u1_idx_lft, u1_idx_top;
    WORD8 i1_ref_idx;

    for(u1_blk_no = 0, u1_i = 0; u1_i < u1_num_part; u1_i++, pi1_ref_idx++)
    {
        u1_idx_lft = ((u1_blk_no & 0x02) >> 1) + u1_b2;
        u1_idx_top = (u1_blk_no & 0x01) + u1_b2;
        i1_ref_idx = *pi1_ref_idx;

        if(i1_ref_idx > 0)
        {
            u1_a = pi1_lft_cxt[u1_idx_lft] > 0;
            u1_b = pi1_top_cxt[u1_idx_top] > 0;

            u4_cxt_inc = u1_a + (u1_b << 1);
            u4_cxt_inc = (u4_cxt_inc | 0x55540);

            i1_ref_idx = (WORD8)ih264d_decode_bins_unary(32, u4_cxt_inc,
                                                         ps_ref_cxt, ps_bitstrm,
                                                         ps_cab_env);

            if((i1_ref_idx > u1_max_ref_minus1) || (i1_ref_idx < 0))
            {
                return ERROR_REF_IDX;
            }

            *pi1_ref_idx = i1_ref_idx;

            INC_SYM_COUNT(ps_cab_env);

        }

        /* Storing Reference Idx Information */
        pi1_lft_cxt[u1_idx_lft] = i1_ref_idx;
        pi1_top_cxt[u1_idx_top] = i1_ref_idx;
        u1_blk_no = u1_blk_no + 1 + (u1_mb_mode & 0x01);
    }
    /* if(!u1_sub_mb) */
    if(u1_num_part != 4)
    {
        pi1_lft_cxt[(!(u1_mb_mode & 0x1)) + u1_b2] = pi1_lft_cxt[u1_b2];
        pi1_top_cxt[(!(u1_mb_mode & 0x2)) + u1_b2] = pi1_top_cxt[u1_b2];
    }
    return OK;
}

/*!
 **************************************************************************
 * \if Function name : ih264d_parse_mb_qp_delta_cabac \endif
 *
 * \brief
 *    This function decodes MB Qp delta using CABAC entropy coding mode.
 *
 * \return
 *    None
 *
 **************************************************************************
 */
WORD32 ih264d_parse_mb_qp_delta_cabac(struct _DecStruct * ps_dec,
                                      WORD8 *pi1_mb_qp_delta)
{
    decoding_envirnoment_t * ps_cab_env = &ps_dec->s_cab_dec_env;
    dec_bit_stream_t * ps_bitstrm = ps_dec->ps_bitstrm;

    UWORD8 u1_code_num;
    bin_ctxt_model_t *ps_mb_qp_delta_ctxt = ps_dec->p_mb_qp_delta_t;
    UWORD32 u4_cxt_inc;

    INC_SYM_COUNT(ps_cab_env);

    u4_cxt_inc = (!(!(ps_dec->i1_prev_mb_qp_delta)));

    u1_code_num = 0;
    u4_cxt_inc = (u4_cxt_inc | 0x33320);
    /* max number of bins = 53,
     since Range for MbQpDelta= -26 to +25 inclusive, UNARY code */
    u1_code_num = ih264d_decode_bins_unary(32, u4_cxt_inc, ps_mb_qp_delta_ctxt,
                                          ps_bitstrm, ps_cab_env);
    if(u1_code_num == 32)
    {
        /* Read remaining 21 bins */
        UWORD8 uc_codeNumX;
        u4_cxt_inc = 0x33333;
        uc_codeNumX = ih264d_decode_bins_unary(21, u4_cxt_inc, ps_mb_qp_delta_ctxt,
                                               ps_bitstrm, ps_cab_env);
        u1_code_num = u1_code_num + uc_codeNumX;
    }

    *pi1_mb_qp_delta = (u1_code_num + 1) >> 1;
    /* Table 9.3: If code_num is even Syntax Element has -ve value */
    if(!(u1_code_num & 0x01))
        *pi1_mb_qp_delta = -(*pi1_mb_qp_delta);

    /* Range of MbQpDelta= -26 to +25 inclusive */
    if((*pi1_mb_qp_delta < -26) || (*pi1_mb_qp_delta > 25))
        return ERROR_INV_RANGE_QP_T;
    ps_dec->i1_prev_mb_qp_delta = *pi1_mb_qp_delta;
    return OK;
}
/*!
 **************************************************************************
 * \if Function name : ih264d_parse_chroma_pred_mode_cabac \endif
 *
 * \brief
 *    This function decodes Chroma Pred mode using CABAC entropy coding mode.
 *
 * \return
 *    None
 *
 **************************************************************************
 */
WORD8 ih264d_parse_chroma_pred_mode_cabac(struct _DecStruct * ps_dec)
{
    decoding_envirnoment_t * ps_cab_env = &ps_dec->s_cab_dec_env;
    dec_bit_stream_t * ps_bitstrm = ps_dec->ps_bitstrm;
    ctxt_inc_mb_info_t * ps_left_ctxt = ps_dec->p_left_ctxt_mb_info;
    ctxt_inc_mb_info_t * ps_top_ctxt = ps_dec->p_top_ctxt_mb_info;
    WORD8 i1_chroma_pred_mode, a, b;
    UWORD32 u4_cxt_inc;

    INC_SYM_COUNT(ps_cab_env);

    /* Binarization is TU and Cmax=3 */
    i1_chroma_pred_mode = 0;
    a = 0;
    b = 0;

    a = ((ps_left_ctxt->u1_intra_chroma_pred_mode != 0) ? 1 : 0);

    b = ((ps_top_ctxt->u1_intra_chroma_pred_mode != 0) ? 1 : 0);
    u4_cxt_inc = a + b;

    u4_cxt_inc = (u4_cxt_inc | 0x330);

    i1_chroma_pred_mode = ih264d_decode_bins_tunary(
                    3, u4_cxt_inc, ps_dec->p_intra_chroma_pred_mode_t,
                    ps_bitstrm, ps_cab_env);

    return (i1_chroma_pred_mode);
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_parse_transform8x8flag_cabac                                     */
/*                                                                           */
/*  Description   :                                                          */
/*  Inputs        :                                                          */
/*                                                                           */
/*                                                                           */
/*  Returns       :                                                          */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*                      Rajasekhar      Creation                             */
/*                                                                           */
/*****************************************************************************/
UWORD8 ih264d_parse_transform8x8flag_cabac(struct _DecStruct * ps_dec,
                                           dec_mb_info_t * ps_cur_mb_info)
{
    decoding_envirnoment_t * ps_cab_env = &ps_dec->s_cab_dec_env;
    dec_bit_stream_t * ps_bitstrm = ps_dec->ps_bitstrm;
    ctxt_inc_mb_info_t * ps_left_ctxt = ps_dec->p_left_ctxt_mb_info;
    ctxt_inc_mb_info_t * ps_top_ctxt = ps_dec->p_top_ctxt_mb_info;
    UWORD8 u1_transform_8x8flag;
    UWORD8 u1_mb_ngbr_avail = ps_cur_mb_info->u1_mb_ngbr_availablity;

    WORD8 a, b;
    UWORD32 u4_cxt_inc;

    /* for calculating the context increment for transform8x8 u4_flag */
    /* it reads transform8x8 u4_flag of the neighbors through */

    /* Binarization is FLC */
    a = 0;
    b = 0;

    if(u1_mb_ngbr_avail & LEFT_MB_AVAILABLE_MASK)
    {
        a = ps_left_ctxt->u1_transform8x8_ctxt;
    }
    if(u1_mb_ngbr_avail & TOP_MB_AVAILABLE_MASK)
    {
        b = ps_top_ctxt->u1_transform8x8_ctxt;

    }

    u4_cxt_inc = a + b;

    u1_transform_8x8flag = ih264d_decode_bin(
                    u4_cxt_inc, ps_dec->s_high_profile.ps_transform8x8_flag,
                    ps_bitstrm, ps_cab_env);

    return (u1_transform_8x8flag);
}

/*!
 **************************************************************************
 * \if Function name : ih264d_read_intra_pred_modes_cabac \endif
 *
 * \brief
 *    Reads the intra pred mode related values of I4x4 MB from bitstream.
 *
 *    This function will read the prev intra pred mode flags and
 *    stores it in pu1_prev_intra4x4_pred_mode_flag. If the u4_flag
 *    indicates that most probable mode is not intra pred mode, then
 *    the rem_intra4x4_pred_mode is read and stored in
 *    pu1_rem_intra4x4_pred_mode array.
 *
 *
 * \return
 *    0 on success and Error code otherwise
 *
 **************************************************************************
 */
WORD32 ih264d_read_intra_pred_modes_cabac(dec_struct_t * ps_dec,
                                          UWORD8 * pu1_prev_intra4x4_pred_mode_flag,
                                          UWORD8 * pu1_rem_intra4x4_pred_mode,
                                          UWORD8 u1_tran_form8x8)
{
    WORD32 i4x4_luma_blk_idx = 0;
    dec_bit_stream_t * ps_bitstrm = ps_dec->ps_bitstrm;
    decoding_envirnoment_t * ps_cab_env = &ps_dec->s_cab_dec_env;
    bin_ctxt_model_t *ps_ctxt_ipred_luma_mpm, *ps_ctx_ipred_luma_rm;
    WORD32 i4_rem_intra4x4_pred_mode;
    UWORD32 u4_prev_intra4x4_pred_mode_flag;
    UWORD32 u4_code_int_range, u4_code_int_val_ofst;
    const UWORD32 *pu4_table = (const UWORD32 *)ps_cab_env->cabac_table;

    ps_ctxt_ipred_luma_mpm = ps_dec->p_prev_intra4x4_pred_mode_flag_t;
    ps_ctx_ipred_luma_rm = ps_dec->p_rem_intra4x4_pred_mode_t;
    SWITCHOFFTRACE;

    i4x4_luma_blk_idx = (0 == u1_tran_form8x8) ? 16 : 4;

    u4_code_int_range = ps_cab_env->u4_code_int_range;
    u4_code_int_val_ofst = ps_cab_env->u4_code_int_val_ofst;

    do
    {

        DECODE_ONE_BIN_MACRO(ps_ctxt_ipred_luma_mpm, u4_code_int_range,
                             u4_code_int_val_ofst, pu4_table, ps_bitstrm,
                             u4_prev_intra4x4_pred_mode_flag)
        *pu1_prev_intra4x4_pred_mode_flag = u4_prev_intra4x4_pred_mode_flag;

        i4_rem_intra4x4_pred_mode = -1;
        if(!u4_prev_intra4x4_pred_mode_flag)
        {

            /*inlining DecodeDecisionBins_FLC*/

            {

                UWORD8 u1_max_bins = 3;
                UWORD32 u4_value;
                UWORD32 u4_symbol, i;

                i = 0;
                u4_value = 0;

                do
                {

                    DECODE_ONE_BIN_MACRO(ps_ctx_ipred_luma_rm, u4_code_int_range,
                                         u4_code_int_val_ofst, pu4_table,
                                         ps_bitstrm, u4_symbol)

                    INC_BIN_COUNT(ps_cab_env);INC_DECISION_BINS(ps_cab_env);

                    u4_value = u4_value | (u4_symbol << i);

                    i++;
                }
                while(i < u1_max_bins);

                i4_rem_intra4x4_pred_mode = (u4_value);

            }

        }

        (*pu1_rem_intra4x4_pred_mode) = i4_rem_intra4x4_pred_mode;

        COPYTHECONTEXT("intra4x4_pred_mode", i4_rem_intra4x4_pred_mode);

        pu1_prev_intra4x4_pred_mode_flag++;
        pu1_rem_intra4x4_pred_mode++;

        i4x4_luma_blk_idx--;
    }
    while(i4x4_luma_blk_idx);

    ps_cab_env->u4_code_int_range = u4_code_int_range;
    ps_cab_env->u4_code_int_val_ofst = u4_code_int_val_ofst;

    return (0);

}

/*!
 **************************************************************************
 * \if Function name : ih264d_parse_ctx_cbp_cabac \endif
 *
 * \brief
 *    This function decodes CtxCbpLuma and CtxCbpChroma (CBP of a Macroblock).
 *    using CABAC entropy coding mode.
 *
 * \return
 *    CBP of a MB.
 *
 **************************************************************************
 */
UWORD32 ih264d_parse_ctx_cbp_cabac(struct _DecStruct * ps_dec)
{

    UWORD32 u4_cxt_inc;
    decoding_envirnoment_t * ps_cab_env = &ps_dec->s_cab_dec_env;
    dec_bit_stream_t * ps_bitstrm = ps_dec->ps_bitstrm;
    ctxt_inc_mb_info_t * ps_left_ctxt = ps_dec->p_left_ctxt_mb_info;
    ctxt_inc_mb_info_t * ps_top_ctxt = ps_dec->p_top_ctxt_mb_info;
    bin_ctxt_model_t *ps_ctxt_cbp_luma = ps_dec->p_cbp_luma_t, *ps_bin_ctxt;
    WORD8 c_Cbp; //,i,j;
    UWORD32 u4_code_int_range, u4_code_int_val_ofst;
    UWORD32 u4_offset, *pu4_buffer;
    const UWORD32 *pu4_table = (const UWORD32 *)ps_cab_env->cabac_table;

    INC_SYM_COUNT(ps_cab_env);



    /* CBP Luma, FL, Cmax = 15, L = 4 */
    u4_cxt_inc = (!((ps_top_ctxt->u1_cbp >> 2) & 0x01)) << 1;
    u4_cxt_inc += !((ps_left_ctxt->u1_cbp >> 1) & 0x01);

    u4_offset = ps_bitstrm->u4_ofst;
    pu4_buffer = ps_bitstrm->pu4_buffer;

    u4_code_int_range = ps_cab_env->u4_code_int_range;
    u4_code_int_val_ofst = ps_cab_env->u4_code_int_val_ofst;
    /*renormalize to ensure there 23 bits more in the u4_code_int_val_ofst*/
    {
        UWORD32 u4_clz, read_bits;

        u4_clz = CLZ(u4_code_int_range);
        FLUSHBITS(u4_offset, u4_clz)
        NEXTBITS(read_bits, u4_offset, pu4_buffer, 23)
        u4_code_int_range = u4_code_int_range << u4_clz;
        u4_code_int_val_ofst = (u4_code_int_val_ofst << u4_clz) | read_bits;
    }

    ps_bin_ctxt = ps_ctxt_cbp_luma + u4_cxt_inc;

    /*inlining DecodeDecision_onebin without renorm*/
    {

        UWORD32 u4_qnt_int_range, u4_int_range_lps;
        UWORD32 u4_symbol, u1_mps_state;
        UWORD32 table_lookup;
        UWORD32 u4_clz;

        u1_mps_state = (ps_bin_ctxt->u1_mps_state);

        u4_clz = CLZ(u4_code_int_range);
        u4_qnt_int_range = u4_code_int_range << u4_clz;
        u4_qnt_int_range = (u4_qnt_int_range >> 29) & 0x3;

        table_lookup = pu4_table[(u1_mps_state << 2) + u4_qnt_int_range];
        u4_int_range_lps = table_lookup & 0xff;

        u4_int_range_lps = u4_int_range_lps << (23 - u4_clz);
        u4_code_int_range = u4_code_int_range - u4_int_range_lps;

        u4_symbol = ((u1_mps_state >> 6) & 0x1);

        /*if mps*/
        u1_mps_state = (table_lookup >> 8) & 0x7F;

        CHECK_IF_LPS(u4_code_int_range, u4_code_int_val_ofst, u4_symbol,
                     u4_int_range_lps, u1_mps_state, table_lookup)

        INC_BIN_COUNT(ps_cab_env);

        ps_bin_ctxt->u1_mps_state = u1_mps_state;

        c_Cbp = u4_symbol;

    }

    u4_cxt_inc = (!((ps_top_ctxt->u1_cbp >> 3) & 0x01)) << 1;
    u4_cxt_inc += !(c_Cbp & 0x01);
    ps_bin_ctxt = ps_ctxt_cbp_luma + u4_cxt_inc;
    /*inlining DecodeDecision_onebin without renorm*/

    {

        UWORD32 u4_qnt_int_range, u4_int_range_lps;
        UWORD32 u4_symbol, u1_mps_state;
        UWORD32 table_lookup;
        UWORD32 u4_clz;

        u1_mps_state = (ps_bin_ctxt->u1_mps_state);

        u4_clz = CLZ(u4_code_int_range);
        u4_qnt_int_range = u4_code_int_range << u4_clz;
        u4_qnt_int_range = (u4_qnt_int_range >> 29) & 0x3;

        table_lookup = pu4_table[(u1_mps_state << 2) + u4_qnt_int_range];
        u4_int_range_lps = table_lookup & 0xff;

        u4_int_range_lps = u4_int_range_lps << (23 - u4_clz);
        u4_code_int_range = u4_code_int_range - u4_int_range_lps;

        u4_symbol = ((u1_mps_state >> 6) & 0x1);

        /*if mps*/
        u1_mps_state = (table_lookup >> 8) & 0x7F;

        CHECK_IF_LPS(u4_code_int_range, u4_code_int_val_ofst, u4_symbol,
                     u4_int_range_lps, u1_mps_state, table_lookup)

        INC_BIN_COUNT(ps_cab_env);

        ps_bin_ctxt->u1_mps_state = u1_mps_state;

        c_Cbp |= u4_symbol << 1;

    }

    u4_cxt_inc = (!(c_Cbp & 0x01)) << 1;
    u4_cxt_inc += !((ps_left_ctxt->u1_cbp >> 3) & 0x01);
    ps_bin_ctxt = ps_ctxt_cbp_luma + u4_cxt_inc;
    /*inlining DecodeDecision_onebin without renorm*/

    {

        UWORD32 u4_qnt_int_range, u4_int_range_lps;
        UWORD32 u4_symbol, u1_mps_state;
        UWORD32 table_lookup;
        UWORD32 u4_clz;

        u1_mps_state = (ps_bin_ctxt->u1_mps_state);

        u4_clz = CLZ(u4_code_int_range);
        u4_qnt_int_range = u4_code_int_range << u4_clz;
        u4_qnt_int_range = (u4_qnt_int_range >> 29) & 0x3;

        table_lookup = pu4_table[(u1_mps_state << 2) + u4_qnt_int_range];
        u4_int_range_lps = table_lookup & 0xff;

        u4_int_range_lps = u4_int_range_lps << (23 - u4_clz);
        u4_code_int_range = u4_code_int_range - u4_int_range_lps;

        u4_symbol = ((u1_mps_state >> 6) & 0x1);

        /*if mps*/
        u1_mps_state = (table_lookup >> 8) & 0x7F;

        CHECK_IF_LPS(u4_code_int_range, u4_code_int_val_ofst, u4_symbol,
                     u4_int_range_lps, u1_mps_state, table_lookup)

        INC_BIN_COUNT(ps_cab_env);

        ps_bin_ctxt->u1_mps_state = u1_mps_state;

        c_Cbp |= u4_symbol << 2;

    }

    u4_cxt_inc = (!((c_Cbp >> 1) & 0x01)) << 1;
    u4_cxt_inc += !((c_Cbp >> 2) & 0x01);
    ps_bin_ctxt = ps_ctxt_cbp_luma + u4_cxt_inc;
    /*inlining DecodeDecision_onebin without renorm*/

    {

        UWORD32 u4_qnt_int_range, u4_int_range_lps;
        UWORD32 u4_symbol, u1_mps_state;
        UWORD32 table_lookup;
        UWORD32 u4_clz;

        u1_mps_state = (ps_bin_ctxt->u1_mps_state);

        u4_clz = CLZ(u4_code_int_range);
        u4_qnt_int_range = u4_code_int_range << u4_clz;
        u4_qnt_int_range = (u4_qnt_int_range >> 29) & 0x3;

        table_lookup = pu4_table[(u1_mps_state << 2) + u4_qnt_int_range];
        u4_int_range_lps = table_lookup & 0xff;

        u4_int_range_lps = u4_int_range_lps << (23 - u4_clz);
        u4_code_int_range = u4_code_int_range - u4_int_range_lps;

        u4_symbol = ((u1_mps_state >> 6) & 0x1);

        /*if mps*/
        u1_mps_state = (table_lookup >> 8) & 0x7F;

        CHECK_IF_LPS(u4_code_int_range, u4_code_int_val_ofst, u4_symbol,
                     u4_int_range_lps, u1_mps_state, table_lookup)

        INC_BIN_COUNT(ps_cab_env);

        ps_bin_ctxt->u1_mps_state = u1_mps_state;

        c_Cbp |= u4_symbol << 3;

    }

    if(u4_code_int_range < ONE_RIGHT_SHIFTED_BY_8)
    {

        RENORM_RANGE_OFFSET(u4_code_int_range, u4_code_int_val_ofst, u4_offset,
                            pu4_buffer)

    }

    {
        UWORD32 u4_cxt_inc;
        WORD8 a, b, c, d;
        bin_ctxt_model_t *p_CtxtCbpChroma = ps_dec->p_cbp_chroma_t;

        /* CBP Chroma, TU, Cmax = 2 */
        a = 0;
        b = 0;
        c = 0;
        d = 0;

        {
            a = (ps_top_ctxt->u1_cbp > 15) ? 2 : 0;
            c = (ps_top_ctxt->u1_cbp > 31) ? 2 : 0;
        }

        {
            b = (ps_left_ctxt->u1_cbp > 15) ? 1 : 0;
            d = (ps_left_ctxt->u1_cbp > 31) ? 1 : 0;
        }
        u4_cxt_inc = a + b;
        u4_cxt_inc = (u4_cxt_inc | ((4 + c + d) << 4));

        /*inlining ih264d_decode_bins_tunary */

        {

            UWORD8 u1_max_bins = 2;
            UWORD32 u4_ctx_inc = u4_cxt_inc;

            UWORD32 u4_value;
            UWORD32 u4_symbol;
            UWORD8 u4_ctx_Inc;
            bin_ctxt_model_t *ps_bin_ctxt;
            u4_value = 0;

            do
            {
                u4_ctx_Inc = u4_ctx_inc & 0xF;
                u4_ctx_inc = u4_ctx_inc >> 4;

                ps_bin_ctxt = p_CtxtCbpChroma + u4_ctx_Inc;
                /*inlining DecodeDecision_onebin*/
                {

                    UWORD32 u4_qnt_int_range, u4_int_range_lps;

                    UWORD32 u1_mps_state;
                    UWORD32 table_lookup;
                    UWORD32 u4_clz;

                    u1_mps_state = (ps_bin_ctxt->u1_mps_state);

                    u4_clz = CLZ(u4_code_int_range);
                    u4_qnt_int_range = u4_code_int_range << u4_clz;
                    u4_qnt_int_range = (u4_qnt_int_range >> 29) & 0x3;

                    table_lookup = pu4_table[(u1_mps_state << 2)
                                    + u4_qnt_int_range];
                    u4_int_range_lps = table_lookup & 0xff;

                    u4_int_range_lps = u4_int_range_lps << (23 - u4_clz);
                    u4_code_int_range = u4_code_int_range - u4_int_range_lps;

                    u4_symbol = ((u1_mps_state >> 6) & 0x1);

                    /*if mps*/
                    u1_mps_state = (table_lookup >> 8) & 0x7F;

                    CHECK_IF_LPS(u4_code_int_range, u4_code_int_val_ofst,
                                 u4_symbol, u4_int_range_lps, u1_mps_state,
                                 table_lookup)

                    if(u4_code_int_range < ONE_RIGHT_SHIFTED_BY_8)
                    {
                        RENORM_RANGE_OFFSET(u4_code_int_range,
                                            u4_code_int_val_ofst, u4_offset,
                                            pu4_buffer)
                    }
                    ps_bin_ctxt->u1_mps_state = u1_mps_state;
                }

                INC_BIN_COUNT(ps_cab_env);INC_DECISION_BINS(
                                ps_cab_env);

                u4_value++;
            }
            while((u4_value < u1_max_bins) & (u4_symbol));

            u4_value = u4_value - 1 + u4_symbol;

            a = (u4_value);

        }

c_Cbp = (c_Cbp | (a << 4));
}

ps_bitstrm->u4_ofst = u4_offset;

ps_cab_env->u4_code_int_range = u4_code_int_range;
ps_cab_env->u4_code_int_val_ofst = u4_code_int_val_ofst;

return (c_Cbp);
}

/*!
 **************************************************************************
 * \if Function name : ih264d_get_mvd_cabac \endif
 *
 * \brief
 *    This function decodes Horz and Vert mvd_l0 and mvd_l1 using CABAC entropy
 *    coding mode as defined in 9.3.2.3.
 *
 * \return
 *    None
 *
 **************************************************************************
 */
void ih264d_get_mvd_cabac(UWORD8 u1_sub_mb,
                          UWORD8 u1_b2,
                          UWORD8 u1_part_wd,
                          UWORD8 u1_part_ht,
                          UWORD8 u1_dec_mvd,
                          dec_struct_t *ps_dec,
                          mv_pred_t *ps_mv)
{
    UWORD8 u1_abs_mvd_x = 0, u1_abs_mvd_y = 0;
    UWORD8 u1_sub_mb_x, u1_sub_mb_y;
    UWORD8 *pu1_top_mv_ctxt, *pu1_lft_mv_ctxt;
    WORD16 *pi2_mv;

    u1_sub_mb_x = (UWORD8)(u1_sub_mb & 0x03);
    u1_sub_mb_y = (UWORD8)(u1_sub_mb >> 2);
    pu1_top_mv_ctxt = &ps_dec->ps_curr_ctxt_mb_info->u1_mv[u1_sub_mb_x][u1_b2];
    pu1_lft_mv_ctxt = &ps_dec->pu1_left_mv_ctxt_inc[u1_sub_mb_y][u1_b2];
    pi2_mv = &ps_mv->i2_mv[u1_b2];

    if(u1_dec_mvd)
    {
        WORD16 i2_mv_x, i2_mv_y;
        WORD32 i2_temp;
        {
            decoding_envirnoment_t * ps_cab_env = &ps_dec->s_cab_dec_env;
            dec_bit_stream_t * ps_bitstrm = ps_dec->ps_bitstrm;
            UWORD16 u2_abs_mvd_x_a, u2_abs_mvd_x_b, u2_abs_mvd_y_a,
                            u2_abs_mvd_y_b;

            u2_abs_mvd_x_b = (UWORD16)pu1_top_mv_ctxt[0];
            u2_abs_mvd_y_b = (UWORD16)pu1_top_mv_ctxt[1];
            u2_abs_mvd_x_a = (UWORD16)pu1_lft_mv_ctxt[0];
            u2_abs_mvd_y_a = (UWORD16)pu1_lft_mv_ctxt[1];

            i2_temp = u2_abs_mvd_x_a + u2_abs_mvd_x_b;

            i2_mv_x = ih264d_parse_mvd_cabac(ps_bitstrm, ps_cab_env,
                                             ps_dec->p_mvd_x_t, i2_temp);

            i2_temp = u2_abs_mvd_y_a + u2_abs_mvd_y_b;

            i2_mv_y = ih264d_parse_mvd_cabac(ps_bitstrm, ps_cab_env,
                                             ps_dec->p_mvd_y_t, i2_temp);
        }

        /***********************************************************************/
        /* Store the abs_mvd_values in cabac contexts                          */
        /* The follownig code can be easily optimzed if mvX, mvY clip values   */
        /* are packed in 16 bits follwed by memcpy                             */
        /***********************************************************************/
        u1_abs_mvd_x = CLIP3(0, 127, ABS(i2_mv_x));
        u1_abs_mvd_y = CLIP3(0, 127, ABS(i2_mv_y));

        COPYTHECONTEXT("MVD", i2_mv_x);COPYTHECONTEXT("MVD", i2_mv_y);

        /* Storing Mv residuals */
        pi2_mv[0] = i2_mv_x;
        pi2_mv[1] = i2_mv_y;
    }

    /***************************************************************/
    /* Store abs_mvd_values cabac contexts                         */
    /***************************************************************/
    {
        UWORD8 u1_i;
        for(u1_i = 0; u1_i < u1_part_wd; u1_i++, pu1_top_mv_ctxt += 4)
        {
            pu1_top_mv_ctxt[0] = u1_abs_mvd_x;
            pu1_top_mv_ctxt[1] = u1_abs_mvd_y;
        }

        for(u1_i = 0; u1_i < u1_part_ht; u1_i++, pu1_lft_mv_ctxt += 4)
        {
            pu1_lft_mv_ctxt[0] = u1_abs_mvd_x;
            pu1_lft_mv_ctxt[1] = u1_abs_mvd_y;
        }
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_parse_mvd_cabac                                                  */
/*                                                                           */
/*  Description   : This cabac function decodes the mvd in a given direction */
/*                  direction ( x or y ) as defined in 9.3.2.3.              */
/*                                                                           */
/*  Inputs        : 1. pointer to Bitstream                                  */
/*                  2. pointer to cabac decoding environmnet                 */
/*                  3. pointer to Mvd context                                */
/*                  4. abs(Top mvd) = u2_abs_mvd_b                           */
/*                  5. abs(left mvd)= u2_abs_mvd_a                           */
/*                                                                           */
/*  Processing    : see section 9.3.2.3 of the standard                      */
/*                                                                           */
/*  Outputs       : i2_mvd                                                   */
/*  Returns       : i2_mvd                                                   */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         16 06 2005   Jay          Draft                                   */
/*                                                                           */
/*****************************************************************************/
WORD16 ih264d_parse_mvd_cabac(dec_bit_stream_t * ps_bitstrm,
                              decoding_envirnoment_t * ps_cab_env,
                              bin_ctxt_model_t * p_ctxt_mvd,
                              UWORD32 i4_temp)

{
    WORD8 k;
    WORD16 i2_suf;
    WORD16 i2_mvd;
    UWORD16 u2_abs_mvd;
    UWORD32 u4_ctx_inc;
    UWORD32 u4_prefix;
    const UWORD32 *pu4_table = (const UWORD32 *)ps_cab_env->cabac_table;
    UWORD32 u4_code_int_range, u4_code_int_val_ofst;

    /*  if mvd < 9                                                  */
    /*  mvd =  Prefix                                                   */
    /*  else                                                            */
    /*  mvd = Prefix + Suffix                                           */
    /*  decode sign bit                                                 */
    /*  Prefix TU decoding Cmax =Ucoff and Suffix 3rd order Exp-Golomb  */

    u2_abs_mvd = (UWORD16)i4_temp;
    u4_ctx_inc = 1;

    if(u2_abs_mvd < 3)
        u4_ctx_inc = 0;
    else if(u2_abs_mvd > 32)
        u4_ctx_inc = 2;

    u4_ctx_inc = (u4_ctx_inc | 0x65430);

    /*inlining modified version of ih264d_decode_bins_unary*/

    {
        UWORD8 u1_max_bins = 9;
        UWORD32 u4_value;
        UWORD32 u4_symbol;
        bin_ctxt_model_t *ps_bin_ctxt;
        UWORD32 u4_ctx_Inc;

        u4_value = 0;
        u4_code_int_range = ps_cab_env->u4_code_int_range;
        u4_code_int_val_ofst = ps_cab_env->u4_code_int_val_ofst;

        do
        {
            u4_ctx_Inc = u4_ctx_inc & 0xf;
            u4_ctx_inc = u4_ctx_inc >> 4;

            ps_bin_ctxt = p_ctxt_mvd + u4_ctx_Inc;

            DECODE_ONE_BIN_MACRO(ps_bin_ctxt, u4_code_int_range,
                                 u4_code_int_val_ofst, pu4_table, ps_bitstrm,
                                 u4_symbol)

            INC_BIN_COUNT(ps_cab_env);INC_DECISION_BINS(ps_cab_env);

            u4_value++;

        }
        while(u4_symbol && u4_value < 5);

        ps_bin_ctxt = p_ctxt_mvd + 6;

        if(u4_symbol && (u4_value < u1_max_bins))
        {

            do
            {

                DECODE_ONE_BIN_MACRO(ps_bin_ctxt, u4_code_int_range,
                                     u4_code_int_val_ofst, pu4_table,
                                     ps_bitstrm, u4_symbol)

                INC_BIN_COUNT(ps_cab_env);INC_DECISION_BINS(ps_cab_env);
                u4_value++;
            }
            while(u4_symbol && (u4_value < u1_max_bins));

        }

        ps_cab_env->u4_code_int_range = u4_code_int_range;
        ps_cab_env->u4_code_int_val_ofst = u4_code_int_val_ofst;
        u4_value = u4_value - 1 + u4_symbol;
        u4_prefix = (u4_value);
    }

    i2_mvd = u4_prefix;

    if(i2_mvd == 9)
    {
        /* Read Suffix */
        k = ih264d_decode_bypass_bins_unary(ps_cab_env, ps_bitstrm);
        i2_suf = (1 << k) - 1;
        k = k + 3;
        i2_suf = (i2_suf << 3);
        i2_mvd += i2_suf;
        i2_suf = ih264d_decode_bypass_bins(ps_cab_env, k, ps_bitstrm);
        i2_mvd += i2_suf;
    }
    /* Read Sign bit */
    if(!i2_mvd)
        return (i2_mvd);

    else
    {
        UWORD32 u4_code_int_val_ofst, u4_code_int_range;

        u4_code_int_val_ofst = ps_cab_env->u4_code_int_val_ofst;
        u4_code_int_range = ps_cab_env->u4_code_int_range;

        if(u4_code_int_range < ONE_RIGHT_SHIFTED_BY_9)
        {
            UWORD32 *pu4_buffer, u4_offset;

            pu4_buffer = ps_bitstrm->pu4_buffer;
            u4_offset = ps_bitstrm->u4_ofst;

            RENORM_RANGE_OFFSET(u4_code_int_range, u4_code_int_val_ofst,
                                u4_offset, pu4_buffer)
            ps_bitstrm->u4_ofst = u4_offset;
        }

        u4_code_int_range = u4_code_int_range >> 1;

        if(u4_code_int_val_ofst >= u4_code_int_range)
        {
            /* S=1 */
            u4_code_int_val_ofst -= u4_code_int_range;
            i2_mvd = (-i2_mvd);
        }

        ps_cab_env->u4_code_int_val_ofst = u4_code_int_val_ofst;
        ps_cab_env->u4_code_int_range = u4_code_int_range;

        return (i2_mvd);

    }
}
