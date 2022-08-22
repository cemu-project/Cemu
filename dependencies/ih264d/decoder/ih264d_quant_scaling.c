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
#include "ih264d_tables.h"

#define IDCT_BLOCK_WIDTH8X8  8

WORD32 ih264d_scaling_list(WORD16 *pi2_scaling_list,
                         WORD32 i4_size_of_scalinglist,
                         UWORD8 *pu1_use_default_scaling_matrix_flag,
                         dec_bit_stream_t *ps_bitstrm)
{
    WORD32 i4_j, i4_delta_scale, i4_lastScale = 8, i4_nextScale = 8;
    UWORD32 *pu4_bitstrm_buf = ps_bitstrm->pu4_buffer;
    UWORD32 *pu4_bitstrm_ofst = &ps_bitstrm->u4_ofst;

    *pu1_use_default_scaling_matrix_flag = 0;

    for(i4_j = 0; i4_j < i4_size_of_scalinglist; i4_j++)
    {
        if(i4_nextScale != 0)
        {
            i4_delta_scale = ih264d_sev(pu4_bitstrm_ofst,
                                        pu4_bitstrm_buf);

            if(i4_delta_scale < MIN_H264_DELTA_SCALE ||
                        i4_delta_scale > MAX_H264_DELTA_SCALE)
            {
                return ERROR_INV_RANGE_QP_T;
            }
            i4_nextScale = ((i4_lastScale + i4_delta_scale + 256) & 0xff);

            *pu1_use_default_scaling_matrix_flag = ((i4_j == 0)
                            && (i4_nextScale == 0));

        }
        pi2_scaling_list[i4_j] =
                        (i4_nextScale == 0) ? (i4_lastScale) : (i4_nextScale);
        i4_lastScale = pi2_scaling_list[i4_j];
    }
    return OK;
}

WORD32 ih264d_form_default_scaling_matrix(dec_struct_t *ps_dec)
{

    /*************************************************************************/
    /* perform the inverse scanning for the frame and field scaling matrices */
    /*************************************************************************/
    {
        UWORD8 *pu1_inv_scan;
        WORD32 i4_i, i4_j;

        pu1_inv_scan = (UWORD8 *)gau1_ih264d_inv_scan;

        /* for all 4x4 matrices */
        for(i4_i = 0; i4_i < 6; i4_i++)
        {
            for(i4_j = 0; i4_j < 16; i4_j++)
            {
                ps_dec->s_high_profile.i2_scalinglist4x4[i4_i][pu1_inv_scan[i4_j]] =
                                16;

            }
        }

        /* for all 8x8 matrices */
        for(i4_i = 0; i4_i < 2; i4_i++)
        {
            for(i4_j = 0; i4_j < 64; i4_j++)
            {
                ps_dec->s_high_profile.i2_scalinglist8x8[i4_i][gau1_ih264d_inv_scan_prog8x8_cabac[i4_j]] =
                                16;

            }
        }
    }
    return OK;
}

WORD32 ih264d_form_scaling_matrix_picture(dec_seq_params_t *ps_seq,
                                          dec_pic_params_t *ps_pic,
                                          dec_struct_t *ps_dec)
{
    /* default scaling matrices */
    WORD32 i4_i;

    /* check the SPS first */
    if(ps_seq->i4_seq_scaling_matrix_present_flag)
    {
        for(i4_i = 0; i4_i < 8; i4_i++)
        {
            if(i4_i < 6)
            {
                /* fall-back rule A */
                if(!ps_seq->u1_seq_scaling_list_present_flag[i4_i])
                {
                    if((i4_i == 0) || (i4_i == 3))
                    {
                        ps_dec->s_high_profile.pi2_scale_mat[i4_i] =
                                        (i4_i == 0) ? (WORD16 *)(gai2_ih264d_default_intra4x4) : (WORD16 *)(gai2_ih264d_default_inter4x4);
                    }
                    else
                    {
                        ps_dec->s_high_profile.pi2_scale_mat[i4_i] =
                                        ps_dec->s_high_profile.pi2_scale_mat[i4_i
                                                        - 1];
                    }
                }
                else
                {
                    if(ps_seq->u1_use_default_scaling_matrix_flag[i4_i])
                    {
                        ps_dec->s_high_profile.pi2_scale_mat[i4_i] =
                                        (i4_i < 3) ? (WORD16 *)(gai2_ih264d_default_intra4x4) : (WORD16 *)(gai2_ih264d_default_inter4x4);
                    }
                    else
                    {
                        ps_dec->s_high_profile.pi2_scale_mat[i4_i] =
                                        ps_seq->i2_scalinglist4x4[i4_i];
                    }
                }

            }
            else
            {
                /* fall-back rule A */
                if((!ps_seq->u1_seq_scaling_list_present_flag[i4_i])
                                || (ps_seq->u1_use_default_scaling_matrix_flag[i4_i]))
                {
                    ps_dec->s_high_profile.pi2_scale_mat[i4_i] =
                                    (i4_i == 6) ? ((WORD16*)gai2_ih264d_default_intra8x8) : ((WORD16*)gai2_ih264d_default_inter8x8);
                }
                else
                {
                    ps_dec->s_high_profile.pi2_scale_mat[i4_i] =
                                    ps_seq->i2_scalinglist8x8[i4_i - 6];
                }
            }
        }
    }

    /* checking for the PPS */

    if(ps_pic->i4_pic_scaling_matrix_present_flag)
    {
        for(i4_i = 0; i4_i < 8; i4_i++)
        {
            if(i4_i < 6)
            {
                /* fall back rule B */
                if(!ps_pic->u1_pic_scaling_list_present_flag[i4_i])
                {
                    if((i4_i == 0) || (i4_i == 3))
                    {
                        if(!ps_seq->i4_seq_scaling_matrix_present_flag)
                        {
                            ps_dec->s_high_profile.pi2_scale_mat[i4_i] =
                                            (i4_i == 0) ? (WORD16 *)(gai2_ih264d_default_intra4x4) : (WORD16 *)(gai2_ih264d_default_inter4x4);
                        }
                    }
                    else
                    {
                        ps_dec->s_high_profile.pi2_scale_mat[i4_i] =
                                        ps_dec->s_high_profile.pi2_scale_mat[i4_i
                                                        - 1];
                    }
                }
                else
                {
                    if(ps_pic->u1_pic_use_default_scaling_matrix_flag[i4_i])
                    {
                        ps_dec->s_high_profile.pi2_scale_mat[i4_i] =
                                        (i4_i < 3) ? (WORD16 *)(gai2_ih264d_default_intra4x4) : (WORD16 *)(gai2_ih264d_default_inter4x4);
                    }
                    else
                    {
                        ps_dec->s_high_profile.pi2_scale_mat[i4_i] =
                                        ps_pic->i2_pic_scalinglist4x4[i4_i];
                    }
                }
            }
            else
            {
                if(!ps_pic->u1_pic_scaling_list_present_flag[i4_i])
                {
                    if(!ps_seq->i4_seq_scaling_matrix_present_flag)
                    {
                        ps_dec->s_high_profile.pi2_scale_mat[i4_i] =
                                        (i4_i == 6) ? ((WORD16*)gai2_ih264d_default_intra8x8) : ((WORD16*)gai2_ih264d_default_inter8x8);
                    }
                }
                else
                {
                    if(ps_pic->u1_pic_use_default_scaling_matrix_flag[i4_i])
                    {
                        ps_dec->s_high_profile.pi2_scale_mat[i4_i] =
                                        (i4_i == 6) ? (WORD16 *)(gai2_ih264d_default_intra8x8) : (WORD16 *)(gai2_ih264d_default_inter8x8);
                    }
                    else
                    {
                        ps_dec->s_high_profile.pi2_scale_mat[i4_i] =
                                        ps_pic->i2_pic_scalinglist8x8[i4_i - 6];
                    }
                }
            }
        }
    }

    /*************************************************************************/
    /* perform the inverse scanning for the frame and field scaling matrices */
    /*************************************************************************/
    {
        UWORD8 *pu1_inv_scan_4x4;
        WORD32 i4_i, i4_j;

        pu1_inv_scan_4x4 = (UWORD8 *)gau1_ih264d_inv_scan;

        /* for all 4x4 matrices */
        for(i4_i = 0; i4_i < 6; i4_i++)
        {
            if(ps_dec->s_high_profile.pi2_scale_mat[i4_i] == NULL)
                return ERROR_CORRUPTED_SLICE;

            for(i4_j = 0; i4_j < 16; i4_j++)
            {
                ps_dec->s_high_profile.i2_scalinglist4x4[i4_i][pu1_inv_scan_4x4[i4_j]] =
                                ps_dec->s_high_profile.pi2_scale_mat[i4_i][i4_j];

            }
        }

        /* for all 8x8 matrices */
        for(i4_i = 0; i4_i < 2; i4_i++)
        {
            if(ps_dec->s_high_profile.pi2_scale_mat[i4_i + 6] == NULL)
                return ERROR_CORRUPTED_SLICE;

            for(i4_j = 0; i4_j < 64; i4_j++)
            {
                ps_dec->s_high_profile.i2_scalinglist8x8[i4_i][gau1_ih264d_inv_scan_prog8x8_cabac[i4_j]] =
                                ps_dec->s_high_profile.pi2_scale_mat[i4_i + 6][i4_j];

            }
        }
    }
    return OK;
}

