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
 * \file ih264d_mvpred.c
 *
 * \brief
 *    This file contains function specific to decoding Motion vector.
 *
 * Detailed_description
 *
 * \date
 *    10-12-2002
 *
 * \author  Arvind Raman
 **************************************************************************
 */
#include <string.h>
#include "ih264d_parse_cavlc.h"
#include "ih264d_error_handler.h"
#include "ih264d_structs.h"
#include "ih264d_defs.h"
#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264d_mb_utils.h"
#include "ih264d_defs.h"
#include "ih264d_debug.h"
#include "ih264d_tables.h"
#include "ih264d_process_bslice.h"
#include "ih264d_mvpred.h"
#include "ih264d_inter_pred.h"
#include "ih264d_tables.h"

/*!
 **************************************************************************
 * \if ih264d_get_motion_vector_predictor name : Name \endif
 *
 * \brief
 *    The routine calculates the motion vector predictor for a given block,
 *    given the candidate MV predictors.
 *
 * \param ps_mv_pred: Candidate predictors for the current block
 * \param ps_currMv: Pointer to the left top edge of the current block in
 *     the MV bank
 *
 * \return
 *    _mvPred: The x & y components of the MV predictor.
 *
 * \note
 *    The code implements the logic as described in sec 8.4.1.2.1. Given
 *    the candidate predictors and the pointer to the top left edge of the
 *    block in the MV bank.
 *
 **************************************************************************
 */

void ih264d_get_motion_vector_predictor(mv_pred_t * ps_result,
                                        mv_pred_t **ps_mv_pred,
                                        UWORD8 u1_ref_idx,
                                        UWORD8 u1_B,
                                        const UWORD8 *pu1_mv_pred_condition)
{
    WORD8 c_temp;
    UWORD8 uc_B2 = (u1_B << 1);

    /* If only one of the candidate blocks has a reference frame equal to
     the current block then use the same block as the final predictor */
    c_temp =
                    (ps_mv_pred[LEFT]->i1_ref_frame[u1_B] == u1_ref_idx)
                                    | ((ps_mv_pred[TOP]->i1_ref_frame[u1_B]
                                                    == u1_ref_idx) << 1)
                                    | ((ps_mv_pred[TOP_R]->i1_ref_frame[u1_B]
                                                    == u1_ref_idx) << 2);
    c_temp = pu1_mv_pred_condition[c_temp];

    if(c_temp != -1)
    {
        /* Case when only when one of the cadidate block has the same
         reference frame as the current block */
        ps_result->i2_mv[uc_B2 + 0] = ps_mv_pred[c_temp]->i2_mv[uc_B2 + 0];
        ps_result->i2_mv[uc_B2 + 1] = ps_mv_pred[c_temp]->i2_mv[uc_B2 + 1];
    }
    else
    {
        WORD32 D0, D1;
        D0 = MIN(ps_mv_pred[0]->i2_mv[uc_B2 + 0],
                 ps_mv_pred[1]->i2_mv[uc_B2 + 0]);
        D1 = MAX(ps_mv_pred[0]->i2_mv[uc_B2 + 0],
                 ps_mv_pred[1]->i2_mv[uc_B2 + 0]);
        D1 = MIN(D1, ps_mv_pred[2]->i2_mv[uc_B2 + 0]);
        ps_result->i2_mv[uc_B2 + 0] = (WORD16)(MAX(D0, D1));

        D0 = MIN(ps_mv_pred[0]->i2_mv[uc_B2 + 1],
                 ps_mv_pred[1]->i2_mv[uc_B2 + 1]);
        D1 = MAX(ps_mv_pred[0]->i2_mv[uc_B2 + 1],
                 ps_mv_pred[1]->i2_mv[uc_B2 + 1]);
        D1 = MIN(D1, ps_mv_pred[2]->i2_mv[uc_B2 + 1]);
        ps_result->i2_mv[uc_B2 + 1] = (WORD16)(MAX(D0, D1));

    }
}

/*!
 **************************************************************************
 * \if ih264d_mbaff_mv_pred name : Name \endif
 *
 * \brief
 *    The routine calculates the motion vector predictor for a given block,
 *    given the candidate MV predictors.
 *
 * \param ps_mv_pred: Candidate predictors for the current block
 * \param ps_currMv: Pointer to the left top edge of the current block in
 *     the MV bank
 *
 * \return
 *    _mvPred: The x & y components of the MV predictor.
 *
 * \note
 *    The code implements the logic as described in sec 8.4.1.2.1. Given
 *    the candidate predictors and the pointer to the top left edge of the
 *    block in the MV bank.
 *
 **************************************************************************
 */

void ih264d_mbaff_mv_pred(mv_pred_t **ps_mv_pred,
                          UWORD8 u1_sub_mb_num,
                          mv_pred_t *ps_mv_nmb,
                          mv_pred_t *ps_mv_ntop,
                          dec_struct_t *ps_dec,
                          UWORD8 uc_mb_part_width,
                          dec_mb_info_t *ps_cur_mb_info,
                          UWORD8* pu0_scale)
{
    UWORD16 u2_a_in = 0, u2_b_in = 0, u2_c_in = 0, u2_d_in = 0;
    mv_pred_t *ps_mvpred_l, *ps_mvpred_tmp;
    UWORD8 u1_sub_mb_x = (u1_sub_mb_num & 3), uc_sub_mb_y = (u1_sub_mb_num >> 2);
    UWORD8 u1_is_cur_mb_fld, u1_is_left_mb_fld, u1_is_top_mb_fld;
    UWORD8 u1_is_cur_mb_top;

    u1_is_cur_mb_fld = ps_cur_mb_info->u1_mb_field_decodingflag;
    u1_is_cur_mb_top = ps_cur_mb_info->u1_topmb;

    u1_is_left_mb_fld = ps_cur_mb_info->ps_left_mb->u1_mb_fld;
    u1_is_top_mb_fld = ps_cur_mb_info->ps_top_mb->u1_mb_fld;

    /* Checking in the subMB exists, calculating their motion vectors to be
     used as predictors and the reference frames of those subMBs */
    ps_mv_pred[LEFT] = &ps_dec->s_default_mv_pred;
    ps_mv_pred[TOP] = &(ps_dec->s_default_mv_pred);
    ps_mv_pred[TOP_R] = &(ps_dec->s_default_mv_pred);

    /* Check if the left subMb is available */
    if(u1_sub_mb_x)
    {
        u2_a_in = 1;
        ps_mv_pred[LEFT] = (ps_mv_nmb - 1);
    }
    else
    {
        UWORD8 uc_temp;
        u2_a_in = (ps_cur_mb_info->u1_mb_ngbr_availablity & LEFT_MB_AVAILABLE_MASK);
        if(u2_a_in)
        {
            ps_mvpred_l = (ps_dec->u4_num_pmbair) ?
                            ps_mv_nmb :
                            (ps_dec->ps_mv_left + (uc_sub_mb_y << 2) + 48
                                            - (u1_is_cur_mb_top << 4));
            uc_temp = 29;
            if(u1_is_cur_mb_fld ^ u1_is_left_mb_fld)
            {
                if(u1_is_left_mb_fld)
                {
                    uc_temp +=
                                    (((uc_sub_mb_y & 1) << 2)
                                                    + ((uc_sub_mb_y & 2) << 1));
                    uc_temp += ((u1_is_cur_mb_top) ? 0 : 8);
                }
                else
                {
                    uc_temp = uc_temp - (uc_sub_mb_y << 2);
                    uc_temp += ((u1_is_cur_mb_top) ? 0 : 16);
                }
            }
            ps_mv_pred[LEFT] = (ps_mvpred_l - uc_temp);
            pu0_scale[LEFT] = u1_is_cur_mb_fld - u1_is_left_mb_fld;
        }
    }

    /* Check if the top subMB is available */
    if((uc_sub_mb_y > 0) || ((u1_is_cur_mb_top | u1_is_cur_mb_fld) == 0))
    {
        u2_b_in = 1;
        ps_mv_pred[TOP] = ps_mv_nmb - 4;
    }
    else
    {
        u2_b_in = (ps_cur_mb_info->u1_mb_ngbr_availablity & TOP_MB_AVAILABLE_MASK);
        if(u2_b_in)
        {
            /* CHANGED CODE */

            if(u1_is_top_mb_fld && u1_is_cur_mb_fld)
                ps_mvpred_tmp = ps_mv_ntop;
            else
            {
                ps_mvpred_tmp = ps_mv_ntop;
                if(u1_is_cur_mb_top)
                    ps_mvpred_tmp += 16;
            }

            ps_mv_pred[TOP] = ps_mvpred_tmp;
            pu0_scale[TOP] = u1_is_cur_mb_fld - u1_is_top_mb_fld;
        }
    }

    /* Check if the top right subMb is available. The top right subMb is
     defined as the top right subMb at the top right corner of the MB
     partition. The top right subMb index starting from the top left
     corner of the MB partition is given by
     TopRightSubMbIndx = TopLeftSubMbIndx + (WidthOfMbPartition - 6) / 2
     */
    u2_c_in = CHECKBIT(ps_cur_mb_info->u2_top_right_avail_mask,
                        (u1_sub_mb_num + uc_mb_part_width - 1));
    if(u2_c_in)
    {
        ps_mv_pred[TOP_R] = ps_mv_pred[TOP] + uc_mb_part_width;
        pu0_scale[TOP_R] = pu0_scale[TOP];
        if((uc_sub_mb_y == 0) && ((u1_sub_mb_x + uc_mb_part_width) > 3))
        {
            UWORD8 uc_isTopRtMbFld;
            uc_isTopRtMbFld = ps_cur_mb_info->ps_top_right_mb->u1_mb_fld;
            /* CHANGED CODE */
            ps_mvpred_tmp = ps_mv_ntop + uc_mb_part_width + 12;
            ps_mvpred_tmp += (u1_is_cur_mb_top) ? 16 : 0;
            ps_mvpred_tmp += (u1_is_cur_mb_fld && u1_is_cur_mb_top && uc_isTopRtMbFld) ?
                            0 : 16;
            ps_mv_pred[TOP_R] = ps_mvpred_tmp;
            pu0_scale[TOP_R] = u1_is_cur_mb_fld - uc_isTopRtMbFld;
        }
    }
    else
    {
        u2_d_in = CHECKBIT(ps_cur_mb_info->u2_top_left_avail_mask, u1_sub_mb_num);

        /* Check if the the top left subMB is available */
        if(u2_d_in)
        {
            UWORD8 uc_isTopLtMbFld;

            ps_mv_pred[TOP_R] = ps_mv_pred[TOP] - 1;
            pu0_scale[TOP_R] = pu0_scale[TOP];

            if(u1_sub_mb_x == 0)
            {
                if((uc_sub_mb_y > 0) || ((u1_is_cur_mb_top | u1_is_cur_mb_fld) == 0))
                {
                    uc_isTopLtMbFld = u1_is_left_mb_fld;
                    ps_mvpred_tmp = ps_mv_pred[LEFT] - 4;

                    if((u1_is_cur_mb_fld == 0) && uc_isTopLtMbFld)
                    {
                        ps_mvpred_tmp = ps_mv_pred[LEFT] + 16;
                        ps_mvpred_tmp -= (uc_sub_mb_y & 1) ? 0 : 4;
                    }
                }
                else
                {
                    UWORD32 u4_cond = ps_dec->u4_num_pmbair;
                    uc_isTopLtMbFld = ps_cur_mb_info->u1_topleft_mb_fld;

                    /* CHANGED CODE */
                    ps_mvpred_tmp = ps_mv_ntop - 29;
                    ps_mvpred_tmp += (u1_is_cur_mb_top) ? 16 : 0;
                    if(u1_is_cur_mb_fld && u1_is_cur_mb_top)
                        ps_mvpred_tmp -= (uc_isTopLtMbFld) ? 16 : 0;
                }
                ps_mv_pred[TOP_R] = ps_mvpred_tmp;
                pu0_scale[TOP_R] = u1_is_cur_mb_fld - uc_isTopLtMbFld;
            }
        }
        else if(u2_b_in == 0)
        {
            /* If all the subMBs B, C, D are all out of the frame then their MV
             and their reference picture is equal to that of A */
            ps_mv_pred[TOP] = ps_mv_pred[LEFT];
            ps_mv_pred[TOP_R] = ps_mv_pred[LEFT];
            pu0_scale[TOP] = pu0_scale[LEFT];
            pu0_scale[TOP_R] = pu0_scale[LEFT];
        }
    }
}

/*!
 **************************************************************************
 * \if ih264d_non_mbaff_mv_pred name : Name \endif
 *
 * \brief
 *    The routine calculates the motion vector predictor for a given block,
 *    given the candidate MV predictors.
 *
 * \param ps_mv_pred: Candidate predictors for the current block
 * \param ps_currMv: Pointer to the left top edge of the current block in
 *     the MV bank
 *
 * \return
 *    _mvPred: The x & y components of the MV predictor.
 *
 * \note
 *    The code implements the logic as described in sec 8.4.1.2.1. Given
 *    the candidate predictors and the pointer to the top left edge of the
 *    block in the MV bank.
 *
 **************************************************************************
 */
#if(!MVPRED_NONMBAFF)
void ih264d_non_mbaff_mv_pred(mv_pred_t **ps_mv_pred,
                              UWORD8 u1_sub_mb_num,
                              mv_pred_t *ps_mv_nmb,
                              mv_pred_t *ps_mv_ntop,
                              dec_struct_t *ps_dec,
                              UWORD8 uc_mb_part_width,
                              dec_mb_info_t *ps_cur_mb_info)
{
    UWORD16 u2_b_in = 0, u2_c_in = 0, u2_d_in = 0;
    UWORD8 u1_sub_mb_x = (u1_sub_mb_num & 3), uc_sub_mb_y = (u1_sub_mb_num >> 2);

    /* Checking in the subMB exists, calculating their motion vectors to be
     used as predictors and the reference frames of those subMBs */

    ps_mv_pred[LEFT] = &ps_dec->s_default_mv_pred;
    ps_mv_pred[TOP] = &(ps_dec->s_default_mv_pred);
    ps_mv_pred[TOP_R] = &(ps_dec->s_default_mv_pred);
    /* Check if the left subMb is available */

    if(u1_sub_mb_x)
    {
        ps_mv_pred[LEFT] = (ps_mv_nmb - 1);
    }
    else
    {
        if(ps_cur_mb_info->u1_mb_ngbr_availablity & LEFT_MB_AVAILABLE_MASK)
        {
            ps_mv_pred[LEFT] = (ps_mv_nmb - 13);
        }
    }

    /* Check if the top subMB is available */
    if(uc_sub_mb_y)
    {
        u2_b_in = 1;
        ps_mv_ntop = ps_mv_nmb - 4;
        ps_mv_pred[TOP] = ps_mv_ntop;

    }
    else
    {
        u2_b_in = (ps_cur_mb_info->u1_mb_ngbr_availablity & TOP_MB_AVAILABLE_MASK);
        if(u2_b_in)
        {
            ps_mv_pred[TOP] = ps_mv_ntop;
        }
    }

    /* Check if the top right subMb is available. The top right subMb is
     defined as the top right subMb at the top right corner of the MB
     partition. The top right subMb index starting from the top left
     corner of the MB partition is given by
     TopRightSubMbIndx = TopLeftSubMbIndx + (WidthOfMbPartition - 6) / 2
     */
    u2_c_in = CHECKBIT(ps_cur_mb_info->u2_top_right_avail_mask,
                        (u1_sub_mb_num + uc_mb_part_width - 1));
    if(u2_c_in)
    {
        ps_mv_pred[TOP_R] = (ps_mv_ntop + uc_mb_part_width);

        if(uc_sub_mb_y == 0)
        {
            /* CHANGED CODE */
            if((u1_sub_mb_x + uc_mb_part_width) > 3)
                ps_mv_pred[TOP_R] += 12;
        }
    }
    else
    {
        u2_d_in = CHECKBIT(ps_cur_mb_info->u2_top_left_avail_mask, u1_sub_mb_num);
        /* Check if the the top left subMB is available */
        if(u2_d_in)
        {
            /* CHANGED CODE */
            ps_mv_pred[TOP_R] = (ps_mv_ntop - 1);
            if(u1_sub_mb_x == 0)
            {
                if(uc_sub_mb_y)
                {
                    ps_mv_pred[TOP_R] = (ps_mv_nmb - 17);
                }
                else
                {
                    /* CHANGED CODE */
                    ps_mv_pred[TOP_R] -= 12;
                }
            }
        }
        else if(u2_b_in == 0)
        {
            /* If all the subMBs B, C, D are all out of the frame then their MV
             and their reference picture is equal to that of A */
            ps_mv_pred[TOP] = ps_mv_pred[LEFT];
            ps_mv_pred[TOP_R] = ps_mv_pred[LEFT];
        }
    }
}
#endif

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_mvpred_nonmbaffB                                         */
/*                                                                           */
/*  Description   : This function calculates the motion vector predictor,    */
/*                  for B-Slices                                             */
/*  Inputs        : <What inputs does the function take?>                    */
/*  Globals       : None                                                     */
/*  Processing    : The neighbours A(Left),B(Top),C(TopRight) are calculated */
/*                  and based on the type of Mb the prediction is            */
/*                  appropriately done                                       */
/*  Outputs       : populates ps_mv_final_pred structure                       */
/*  Returns       : u1_direct_zero_pred_flag which is used only in              */
/*                    decodeSpatialdirect()                                  */
/*                                                                           */
/*  Issues        : <List any issues or problems with this function>         */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         03 05 2005   TA              First Draft                          */
/*                                                                           */
/*****************************************************************************/
#if(!MVPRED_NONMBAFF)
UWORD8 ih264d_mvpred_nonmbaffB(dec_struct_t *ps_dec,
                               dec_mb_info_t *ps_cur_mb_info,
                               mv_pred_t *ps_mv_nmb,
                               mv_pred_t *ps_mv_ntop,
                               mv_pred_t *ps_mv_final_pred,
                               UWORD8 u1_sub_mb_num,
                               UWORD8 uc_mb_part_width,
                               UWORD8 u1_lx_start,
                               UWORD8 u1_lxend,
                               UWORD8 u1_mb_mc_mode)
{
    UWORD8 u1_a_in, u1_b_in, uc_temp1, uc_temp2, uc_temp3;
    mv_pred_t *ps_mv_pred[3];
    UWORD8 uc_B2, uc_lx, u1_ref_idx;
    UWORD8 u1_direct_zero_pred_flag = 0;

    ih264d_non_mbaff_mv_pred(ps_mv_pred, u1_sub_mb_num, ps_mv_nmb, ps_mv_ntop,
                             ps_dec, uc_mb_part_width, ps_cur_mb_info);

    for(uc_lx = u1_lx_start; uc_lx < u1_lxend; uc_lx++)
    {
        u1_ref_idx = ps_mv_final_pred->i1_ref_frame[uc_lx];
        uc_B2 = (uc_lx << 1);
        switch(u1_mb_mc_mode)
        {
            case PRED_16x8:
                /* Directional prediction for a 16x8 MB partition */
                if(u1_sub_mb_num == 0)
                {
                    /* Calculating the MV pred for the top 16x8 block */
                    if(ps_mv_pred[TOP]->i1_ref_frame[uc_lx] == u1_ref_idx)
                    {
                        /* If the reference frame used by the top subMB is same as the
                         reference frame used by the current block then MV predictor to
                         be used for the current block is same as the MV of the top
                         subMB */
                        ps_mv_final_pred->i2_mv[uc_B2 + 0] =
                                        ps_mv_pred[TOP]->i2_mv[uc_B2 + 0];
                        ps_mv_final_pred->i2_mv[uc_B2 + 1] =
                                        ps_mv_pred[TOP]->i2_mv[uc_B2 + 1];
                    }
                    else
                    {
                        /* The MV predictor is calculated according to the process
                         defined in 8.4.1.2.1 */
                        ih264d_get_motion_vector_predictor(
                                        ps_mv_final_pred,
                                        ps_mv_pred,
                                        u1_ref_idx,
                                        uc_lx,
                                        (const UWORD8 *)gau1_ih264d_mv_pred_condition);
                    }
                }
                else
                {
                    if(ps_mv_pred[LEFT]->i1_ref_frame[uc_lx] == u1_ref_idx)
                    {
                        /* If the reference frame used by the left subMB is same as the
                         reference frame used by the current block then MV predictor to
                         be used for the current block is same as the MV of the left
                         subMB */
                        ps_mv_final_pred->i2_mv[uc_B2 + 0] =
                                        ps_mv_pred[LEFT]->i2_mv[uc_B2 + 0];
                        ps_mv_final_pred->i2_mv[uc_B2 + 1] =
                                        ps_mv_pred[LEFT]->i2_mv[uc_B2 + 1];
                    }
                    else
                    {
                        /* The MV predictor is calculated according to the process
                         defined in 8.4.1.2.1 */
                        ih264d_get_motion_vector_predictor(
                                        ps_mv_final_pred,
                                        ps_mv_pred,
                                        u1_ref_idx,
                                        uc_lx,
                                        (const UWORD8 *)gau1_ih264d_mv_pred_condition);
                    }
                }
                break;
            case PRED_8x16:
                /* Directional prediction for a 8x16 MB partition */
                if(u1_sub_mb_num == 0)
                {
                    if(ps_mv_pred[LEFT]->i1_ref_frame[uc_lx] == u1_ref_idx)
                    {
                        /* If the reference frame used by the left subMB is same as the
                         reference frame used by the current block then MV predictor to
                         be used for the current block is same as the MV of the left
                         subMB */
                        ps_mv_final_pred->i2_mv[uc_B2 + 0] =
                                        ps_mv_pred[LEFT]->i2_mv[uc_B2 + 0];
                        ps_mv_final_pred->i2_mv[uc_B2 + 1] =
                                        ps_mv_pred[LEFT]->i2_mv[uc_B2 + 1];
                    }
                    else
                    {
                        /* The MV predictor is calculated according to the process
                         defined in 8.4.1.2.1 */
                        ih264d_get_motion_vector_predictor(
                                        ps_mv_final_pred,
                                        ps_mv_pred,
                                        u1_ref_idx,
                                        uc_lx,
                                        (const UWORD8 *)gau1_ih264d_mv_pred_condition);
                    }
                }
                else
                {
                    if(ps_mv_pred[TOP_R]->i1_ref_frame[uc_lx] == u1_ref_idx)
                    {
                        /* If the reference frame used by the top right subMB is same as
                         the reference frame used by the current block then MV
                         predictor to be used for the current block is same as the MV
                         of the left subMB */
                        ps_mv_final_pred->i2_mv[uc_B2 + 0] =
                                        ps_mv_pred[TOP_R]->i2_mv[uc_B2 + 0];
                        ps_mv_final_pred->i2_mv[uc_B2 + 1] =
                                        ps_mv_pred[TOP_R]->i2_mv[uc_B2 + 1];
                    }
                    else
                    {
                        /* The MV predictor is calculated according to the process
                         defined in 8.4.1.2.1 */
                        ih264d_get_motion_vector_predictor(
                                        ps_mv_final_pred,
                                        ps_mv_pred,
                                        u1_ref_idx,
                                        uc_lx,
                                        (const UWORD8 *)gau1_ih264d_mv_pred_condition);
                    }
                }
                break;
            case B_DIRECT_SPATIAL:
                /* Case when the MB has been skipped */
                /* If either of left or the top subMB is not present
                 OR
                 If both the MV components of either the left or the top subMB are
                 zero and their reference frame pointer pointing to 0
                 then MV for the skipped MB is zero
                 else the Median of the mv_pred_t is used */
                uc_temp1 = (UWORD8)ps_mv_pred[LEFT]->i1_ref_frame[0];
                uc_temp2 = (UWORD8)ps_mv_pred[TOP]->i1_ref_frame[0];
                uc_temp3 = (UWORD8)ps_mv_pred[TOP_R]->i1_ref_frame[0];

                ps_mv_final_pred->i1_ref_frame[0] = MIN(uc_temp1,
                                                      MIN(uc_temp2, uc_temp3));

                uc_temp1 = (UWORD8)ps_mv_pred[LEFT]->i1_ref_frame[1];
                uc_temp2 = (UWORD8)ps_mv_pred[TOP]->i1_ref_frame[1];
                uc_temp3 = (UWORD8)ps_mv_pred[TOP_R]->i1_ref_frame[1];

                ps_mv_final_pred->i1_ref_frame[1] = MIN(uc_temp1,
                                                      MIN(uc_temp2, uc_temp3));

                if((ps_mv_final_pred->i1_ref_frame[0] < 0)
                                && (ps_mv_final_pred->i1_ref_frame[1] < 0))
                {
                    u1_direct_zero_pred_flag = 1;
                    ps_mv_final_pred->i1_ref_frame[0] = 0;
                    ps_mv_final_pred->i1_ref_frame[1] = 0;
                }
                ih264d_get_motion_vector_predictor(
                                ps_mv_final_pred, ps_mv_pred,
                                ps_mv_final_pred->i1_ref_frame[0], 0,
                                (const UWORD8 *)gau1_ih264d_mv_pred_condition);

                ih264d_get_motion_vector_predictor(
                                ps_mv_final_pred, ps_mv_pred,
                                ps_mv_final_pred->i1_ref_frame[1], 1,
                                (const UWORD8 *)gau1_ih264d_mv_pred_condition);

                break;
            case MB_SKIP:
                /* Case when the MB has been skipped */
                /* If either of left or the top subMB is not present
                 OR
                 If both the MV components of either the left or the top subMB are
                 zero and their reference frame pointer pointing to 0
                 then MV for the skipped MB is zero
                 else the Median of the mv_pred_t is used */
                u1_a_in = (ps_cur_mb_info->u1_mb_ngbr_availablity &
                LEFT_MB_AVAILABLE_MASK);
                u1_b_in = (ps_cur_mb_info->u1_mb_ngbr_availablity &
                TOP_MB_AVAILABLE_MASK);
                if(((u1_a_in * u1_b_in) == 0)
                                || ((ps_mv_pred[LEFT]->i2_mv[0]
                                                | ps_mv_pred[LEFT]->i2_mv[1]
                                                | ps_mv_pred[LEFT]->i1_ref_frame[0])
                                                == 0)
                                || ((ps_mv_pred[TOP]->i2_mv[0]
                                                | ps_mv_pred[TOP]->i2_mv[1]
                                                | ps_mv_pred[TOP]->i1_ref_frame[0])
                                                == 0))
                {
                    ps_mv_final_pred->i2_mv[0] = 0;
                    ps_mv_final_pred->i2_mv[1] = 0;
                    break;
                }
                /* If the condition above is not true calculate the MV predictor
                 according to the process defined in sec 8.4.1.2.1 */
            default:
                ih264d_get_motion_vector_predictor(
                                ps_mv_final_pred, ps_mv_pred, u1_ref_idx, uc_lx,
                                (const UWORD8 *)gau1_ih264d_mv_pred_condition);
                break;
        }
    }
    return (u1_direct_zero_pred_flag);
}
#endif

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_mvpred_nonmbaff                                          */
/*                                                                           */
/*  Description   : This function calculates the motion vector predictor,    */
/*                  for all the slice types other than B_SLICE               */
/*  Inputs        : <What inputs does the function take?>                    */
/*  Globals       : None                                                     */
/*  Processing    : The neighbours A(Left),B(Top),C(TopRight) are calculated */
/*                  and based on the type of Mb the prediction is            */
/*                  appropriately done                                       */
/*  Outputs       : populates ps_mv_final_pred structure                       */
/*  Returns       : u1_direct_zero_pred_flag which is used only in              */
/*                    decodeSpatialdirect()                                  */
/*                                                                           */
/*  Issues        : <List any issues or problems with this function>         */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         03 05 2005   TA              First Draft                          */
/*                                                                           */
/*****************************************************************************/
#if(!MVPRED_NONMBAFF)
UWORD8 ih264d_mvpred_nonmbaff(dec_struct_t *ps_dec,
                              dec_mb_info_t *ps_cur_mb_info,
                              mv_pred_t *ps_mv_nmb,
                              mv_pred_t *ps_mv_ntop,
                              mv_pred_t *ps_mv_final_pred,
                              UWORD8 u1_sub_mb_num,
                              UWORD8 uc_mb_part_width,
                              UWORD8 u1_lx_start,
                              UWORD8 u1_lxend,
                              UWORD8 u1_mb_mc_mode)
{
    UWORD8 u1_a_in, u1_b_in, uc_temp1, uc_temp2, uc_temp3;
    mv_pred_t *ps_mv_pred[3];
    UWORD8 u1_ref_idx;
    UWORD8 u1_direct_zero_pred_flag = 0;
    UNUSED(u1_lx_start);
    UNUSED(u1_lxend);
    ih264d_non_mbaff_mv_pred(ps_mv_pred, u1_sub_mb_num, ps_mv_nmb, ps_mv_ntop,
                             ps_dec, uc_mb_part_width, ps_cur_mb_info);

    u1_ref_idx = ps_mv_final_pred->i1_ref_frame[0];

    switch(u1_mb_mc_mode)
    {
        case PRED_16x8:
            /* Directional prediction for a 16x8 MB partition */
            if(u1_sub_mb_num == 0)
            {
                /* Calculating the MV pred for the top 16x8 block */
                if(ps_mv_pred[TOP]->i1_ref_frame[0] == u1_ref_idx)
                {
                    /* If the reference frame used by the top subMB is same as the
                     reference frame used by the current block then MV predictor to
                     be used for the current block is same as the MV of the top
                     subMB */

                    ps_mv_final_pred->i2_mv[0] = ps_mv_pred[TOP]->i2_mv[0];
                    ps_mv_final_pred->i2_mv[1] = ps_mv_pred[TOP]->i2_mv[1];
                }
                else
                {
                    /* The MV predictor is calculated according to the process
                     defined in 8.4.1.2.1 */
                    ih264d_get_motion_vector_predictor(
                                    ps_mv_final_pred,
                                    ps_mv_pred,
                                    u1_ref_idx,
                                    0,
                                    (const UWORD8 *)gau1_ih264d_mv_pred_condition);
                }
            }
            else
            {
                if(ps_mv_pred[LEFT]->i1_ref_frame[0] == u1_ref_idx)
                {
                    /* If the reference frame used by the left subMB is same as the
                     reference frame used by the current block then MV predictor to
                     be used for the current block is same as the MV of the left
                     subMB */

                    ps_mv_final_pred->i2_mv[0] = ps_mv_pred[LEFT]->i2_mv[0];
                    ps_mv_final_pred->i2_mv[1] = ps_mv_pred[LEFT]->i2_mv[1];
                }
                else
                {
                    /* The MV predictor is calculated according to the process
                     defined in 8.4.1.2.1 */
                    ih264d_get_motion_vector_predictor(
                                    ps_mv_final_pred,
                                    ps_mv_pred,
                                    u1_ref_idx,
                                    0,
                                    (const UWORD8 *)gau1_ih264d_mv_pred_condition);
                }
            }
            break;
        case PRED_8x16:
            /* Directional prediction for a 8x16 MB partition */
            if(u1_sub_mb_num == 0)
            {
                if(ps_mv_pred[LEFT]->i1_ref_frame[0] == u1_ref_idx)
                {
                    /* If the reference frame used by the left subMB is same as the
                     reference frame used by the current block then MV predictor to
                     be used for the current block is same as the MV of the left
                     subMB */

                    ps_mv_final_pred->i2_mv[0] = ps_mv_pred[LEFT]->i2_mv[0];
                    ps_mv_final_pred->i2_mv[1] = ps_mv_pred[LEFT]->i2_mv[1];
                }
                else
                {
                    /* The MV predictor is calculated according to the process
                     defined in 8.4.1.2.1 */
                    ih264d_get_motion_vector_predictor(
                                    ps_mv_final_pred,
                                    ps_mv_pred,
                                    u1_ref_idx,
                                    0,
                                    (const UWORD8 *)gau1_ih264d_mv_pred_condition);
                }
            }
            else
            {
                if(ps_mv_pred[TOP_R]->i1_ref_frame[0] == u1_ref_idx)
                {
                    /* If the reference frame used by the top right subMB is same as
                     the reference frame used by the current block then MV
                     predictor to be used for the current block is same as the MV
                     of the left subMB */

                    ps_mv_final_pred->i2_mv[0] = ps_mv_pred[TOP_R]->i2_mv[0];
                    ps_mv_final_pred->i2_mv[1] = ps_mv_pred[TOP_R]->i2_mv[1];
                }
                else
                {
                    /* The MV predictor is calculated according to the process
                     defined in 8.4.1.2.1 */
                    ih264d_get_motion_vector_predictor(
                                    ps_mv_final_pred,
                                    ps_mv_pred,
                                    u1_ref_idx,
                                    0,
                                    (const UWORD8 *)gau1_ih264d_mv_pred_condition);
                }
            }
            break;
        case B_DIRECT_SPATIAL:
            /* Case when the MB has been skipped */
            /* If either of left or the top subMB is not present
             OR
             If both the MV components of either the left or the top subMB are
             zero and their reference frame pointer pointing to 0
             then MV for the skipped MB is zero
             else the Median of the mv_pred_t is used */
            uc_temp1 = (UWORD8)ps_mv_pred[LEFT]->i1_ref_frame[0];
            uc_temp2 = (UWORD8)ps_mv_pred[TOP]->i1_ref_frame[0];
            uc_temp3 = (UWORD8)ps_mv_pred[TOP_R]->i1_ref_frame[0];

            ps_mv_final_pred->i1_ref_frame[0] = MIN(uc_temp1,
                                                  MIN(uc_temp2, uc_temp3));

            uc_temp1 = (UWORD8)ps_mv_pred[LEFT]->i1_ref_frame[1];
            uc_temp2 = (UWORD8)ps_mv_pred[TOP]->i1_ref_frame[1];
            uc_temp3 = (UWORD8)ps_mv_pred[TOP_R]->i1_ref_frame[1];

            ps_mv_final_pred->i1_ref_frame[1] = MIN(uc_temp1,
                                                  MIN(uc_temp2, uc_temp3));

            if((ps_mv_final_pred->i1_ref_frame[0] < 0)
                            && (ps_mv_final_pred->i1_ref_frame[1] < 0))
            {
                u1_direct_zero_pred_flag = 1;
                ps_mv_final_pred->i1_ref_frame[0] = 0;
                ps_mv_final_pred->i1_ref_frame[1] = 0;
            }
            ih264d_get_motion_vector_predictor(
                            ps_mv_final_pred, ps_mv_pred,
                            ps_mv_final_pred->i1_ref_frame[0], 0,
                            (const UWORD8 *)gau1_ih264d_mv_pred_condition);

            ih264d_get_motion_vector_predictor(
                            ps_mv_final_pred, ps_mv_pred,
                            ps_mv_final_pred->i1_ref_frame[1], 1,
                            (const UWORD8 *)gau1_ih264d_mv_pred_condition);

            break;
        case MB_SKIP:
            /* Case when the MB has been skipped */
            /* If either of left or the top subMB is not present
             OR
             If both the MV components of either the left or the top subMB are
             zero and their reference frame pointer pointing to 0
             then MV for the skipped MB is zero
             else the Median of the mv_pred_t is used */
            u1_a_in = (ps_cur_mb_info->u1_mb_ngbr_availablity &
            LEFT_MB_AVAILABLE_MASK);
            u1_b_in = (ps_cur_mb_info->u1_mb_ngbr_availablity &
            TOP_MB_AVAILABLE_MASK);
            if(((u1_a_in * u1_b_in) == 0)
                            || ((ps_mv_pred[LEFT]->i2_mv[0]
                                            | ps_mv_pred[LEFT]->i2_mv[1]
                                            | ps_mv_pred[LEFT]->i1_ref_frame[0])
                                            == 0)
                            || ((ps_mv_pred[TOP]->i2_mv[0]
                                            | ps_mv_pred[TOP]->i2_mv[1]
                                            | ps_mv_pred[TOP]->i1_ref_frame[0])
                                            == 0))
            {

                ps_mv_final_pred->i2_mv[0] = 0;
                ps_mv_final_pred->i2_mv[1] = 0;
                break;
            }
            /* If the condition above is not true calculate the MV predictor
             according to the process defined in sec 8.4.1.2.1 */
        default:
            ih264d_get_motion_vector_predictor(
                            ps_mv_final_pred, ps_mv_pred, u1_ref_idx, 0,
                            (const UWORD8 *)gau1_ih264d_mv_pred_condition);
            break;
    }

    return (u1_direct_zero_pred_flag);
}
#endif

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_mvpred_mbaff                                             */
/*                                                                           */
/*  Description   : This function calculates the motion vector predictor,    */
/*  Inputs        : <What inputs does the function take?>                    */
/*  Globals       : None                                                     */
/*  Processing    : The neighbours A(Left),B(Top),C(TopRight) are calculated */
/*                  and based on the type of Mb the prediction is            */
/*                  appropriately done                                       */
/*  Outputs       : populates ps_mv_final_pred structure                       */
/*  Returns       : u1_direct_zero_pred_flag which is used only in              */
/*                    decodeSpatialdirect()                                  */
/*                                                                           */
/*  Issues        : <List any issues or problems with this function>         */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         03 05 2005   TA              First Draft                          */
/*                                                                           */
/*****************************************************************************/

UWORD8 ih264d_mvpred_mbaff(dec_struct_t *ps_dec,
                           dec_mb_info_t *ps_cur_mb_info,
                           mv_pred_t *ps_mv_nmb,
                           mv_pred_t *ps_mv_ntop,
                           mv_pred_t *ps_mv_final_pred,
                           UWORD8 u1_sub_mb_num,
                           UWORD8 uc_mb_part_width,
                           UWORD8 u1_lx_start,
                           UWORD8 u1_lxend,
                           UWORD8 u1_mb_mc_mode)
{
    UWORD8 u1_a_in, u1_b_in, uc_temp1, uc_temp2, uc_temp3;
    mv_pred_t *ps_mv_pred[3], s_mvPred[3];
    UWORD8 uc_B2, pu0_scale[3], i, uc_lx, u1_ref_idx;
    UWORD8 u1_direct_zero_pred_flag = 0;

    pu0_scale[0] = pu0_scale[1] = pu0_scale[2] = 0;
    ih264d_mbaff_mv_pred(ps_mv_pred, u1_sub_mb_num, ps_mv_nmb, ps_mv_ntop, ps_dec,
                         uc_mb_part_width, ps_cur_mb_info, pu0_scale);
    for(i = 0; i < 3; i++)
    {
        if(pu0_scale[i] != 0)
        {
            memcpy(&s_mvPred[i], ps_mv_pred[i], sizeof(mv_pred_t));
            if(pu0_scale[i] == 1)
            {
                s_mvPred[i].i1_ref_frame[0] = s_mvPred[i].i1_ref_frame[0] << 1;
                s_mvPred[i].i1_ref_frame[1] = s_mvPred[i].i1_ref_frame[1] << 1;
                s_mvPred[i].i2_mv[1] = SIGN_POW2_DIV(s_mvPred[i].i2_mv[1], 1);
                s_mvPred[i].i2_mv[3] = SIGN_POW2_DIV(s_mvPred[i].i2_mv[3], 1);
            }
            else
            {
                s_mvPred[i].i1_ref_frame[0] = s_mvPred[i].i1_ref_frame[0] >> 1;
                s_mvPred[i].i1_ref_frame[1] = s_mvPred[i].i1_ref_frame[1] >> 1;
                s_mvPred[i].i2_mv[1] = s_mvPred[i].i2_mv[1] << 1;
                s_mvPred[i].i2_mv[3] = s_mvPred[i].i2_mv[3] << 1;
            }
            ps_mv_pred[i] = &s_mvPred[i];
        }
    }

    for(uc_lx = u1_lx_start; uc_lx < u1_lxend; uc_lx++)
    {
        u1_ref_idx = ps_mv_final_pred->i1_ref_frame[uc_lx];
        uc_B2 = (uc_lx << 1);
        switch(u1_mb_mc_mode)
        {
            case PRED_16x8:
                /* Directional prediction for a 16x8 MB partition */
                if(u1_sub_mb_num == 0)
                {
                    /* Calculating the MV pred for the top 16x8 block */
                    if(ps_mv_pred[TOP]->i1_ref_frame[uc_lx] == u1_ref_idx)
                    {
                        /* If the reference frame used by the top subMB is same as the
                         reference frame used by the current block then MV predictor to
                         be used for the current block is same as the MV of the top
                         subMB */
                        ps_mv_final_pred->i2_mv[uc_B2 + 0] =
                                        ps_mv_pred[TOP]->i2_mv[uc_B2 + 0];
                        ps_mv_final_pred->i2_mv[uc_B2 + 1] =
                                        ps_mv_pred[TOP]->i2_mv[uc_B2 + 1];
                    }
                    else
                    {
                        /* The MV predictor is calculated according to the process
                         defined in 8.4.1.2.1 */
                        ih264d_get_motion_vector_predictor(
                                        ps_mv_final_pred,
                                        ps_mv_pred,
                                        u1_ref_idx,
                                        uc_lx,
                                        (const UWORD8 *)gau1_ih264d_mv_pred_condition);
                    }
                }
                else
                {
                    if(ps_mv_pred[LEFT]->i1_ref_frame[uc_lx] == u1_ref_idx)
                    {
                        /* If the reference frame used by the left subMB is same as the
                         reference frame used by the current block then MV predictor to
                         be used for the current block is same as the MV of the left
                         subMB */
                        ps_mv_final_pred->i2_mv[uc_B2 + 0] =
                                        ps_mv_pred[LEFT]->i2_mv[uc_B2 + 0];
                        ps_mv_final_pred->i2_mv[uc_B2 + 1] =
                                        ps_mv_pred[LEFT]->i2_mv[uc_B2 + 1];
                    }
                    else
                    {
                        /* The MV predictor is calculated according to the process
                         defined in 8.4.1.2.1 */
                        ih264d_get_motion_vector_predictor(
                                        ps_mv_final_pred,
                                        ps_mv_pred,
                                        u1_ref_idx,
                                        uc_lx,
                                        (const UWORD8 *)gau1_ih264d_mv_pred_condition);
                    }
                }
                break;
            case PRED_8x16:
                /* Directional prediction for a 8x16 MB partition */
                if(u1_sub_mb_num == 0)
                {
                    if(ps_mv_pred[LEFT]->i1_ref_frame[uc_lx] == u1_ref_idx)
                    {
                        /* If the reference frame used by the left subMB is same as the
                         reference frame used by the current block then MV predictor to
                         be used for the current block is same as the MV of the left
                         subMB */
                        ps_mv_final_pred->i2_mv[uc_B2 + 0] =
                                        ps_mv_pred[LEFT]->i2_mv[uc_B2 + 0];
                        ps_mv_final_pred->i2_mv[uc_B2 + 1] =
                                        ps_mv_pred[LEFT]->i2_mv[uc_B2 + 1];
                    }
                    else
                    {
                        /* The MV predictor is calculated according to the process
                         defined in 8.4.1.2.1 */
                        ih264d_get_motion_vector_predictor(
                                        ps_mv_final_pred,
                                        ps_mv_pred,
                                        u1_ref_idx,
                                        uc_lx,
                                        (const UWORD8 *)gau1_ih264d_mv_pred_condition);
                    }
                }
                else
                {
                    if(ps_mv_pred[TOP_R]->i1_ref_frame[uc_lx] == u1_ref_idx)
                    {
                        /* If the reference frame used by the top right subMB is same as
                         the reference frame used by the current block then MV
                         predictor to be used for the current block is same as the MV
                         of the left subMB */
                        ps_mv_final_pred->i2_mv[uc_B2 + 0] =
                                        ps_mv_pred[TOP_R]->i2_mv[uc_B2 + 0];
                        ps_mv_final_pred->i2_mv[uc_B2 + 1] =
                                        ps_mv_pred[TOP_R]->i2_mv[uc_B2 + 1];
                    }
                    else
                    {
                        /* The MV predictor is calculated according to the process
                         defined in 8.4.1.2.1 */
                        ih264d_get_motion_vector_predictor(
                                        ps_mv_final_pred,
                                        ps_mv_pred,
                                        u1_ref_idx,
                                        uc_lx,
                                        (const UWORD8 *)gau1_ih264d_mv_pred_condition);
                    }
                }
                break;
            case B_DIRECT_SPATIAL:
                /* Case when the MB has been skipped */
                /* If either of left or the top subMB is not present
                 OR
                 If both the MV components of either the left or the top subMB are
                 zero and their reference frame pointer pointing to 0
                 then MV for the skipped MB is zero
                 else the Median of the mv_pred_t is used */
                uc_temp1 = (UWORD8)ps_mv_pred[LEFT]->i1_ref_frame[0];
                uc_temp2 = (UWORD8)ps_mv_pred[TOP]->i1_ref_frame[0];
                uc_temp3 = (UWORD8)ps_mv_pred[TOP_R]->i1_ref_frame[0];

                ps_mv_final_pred->i1_ref_frame[0] = MIN(uc_temp1,
                                                      MIN(uc_temp2, uc_temp3));

                uc_temp1 = (UWORD8)ps_mv_pred[LEFT]->i1_ref_frame[1];
                uc_temp2 = (UWORD8)ps_mv_pred[TOP]->i1_ref_frame[1];
                uc_temp3 = (UWORD8)ps_mv_pred[TOP_R]->i1_ref_frame[1];

                ps_mv_final_pred->i1_ref_frame[1] = MIN(uc_temp1,
                                                      MIN(uc_temp2, uc_temp3));

                /* If the reference indices are negative clip the scaled reference indices to -1 */
                /* i.e invalid reference index */

                /*if(ps_mv_final_pred->i1_ref_frame[0] < 0)
                 ps_mv_final_pred->i1_ref_frame[0] = -1;

                 if(ps_mv_final_pred->i1_ref_frame[1] < 0)
                 ps_mv_final_pred->i1_ref_frame[1] = -1; */

                if((ps_mv_final_pred->i1_ref_frame[0] < 0)
                                && (ps_mv_final_pred->i1_ref_frame[1] < 0))
                {
                    u1_direct_zero_pred_flag = 1;
                    ps_mv_final_pred->i1_ref_frame[0] = 0;
                    ps_mv_final_pred->i1_ref_frame[1] = 0;
                }
                ih264d_get_motion_vector_predictor(
                                ps_mv_final_pred, ps_mv_pred,
                                ps_mv_final_pred->i1_ref_frame[0], 0,
                                (const UWORD8 *)gau1_ih264d_mv_pred_condition);

                ih264d_get_motion_vector_predictor(
                                ps_mv_final_pred, ps_mv_pred,
                                ps_mv_final_pred->i1_ref_frame[1], 1,
                                (const UWORD8 *)gau1_ih264d_mv_pred_condition);

                break;
            case MB_SKIP:
                /* Case when the MB has been skipped */
                /* If either of left or the top subMB is not present
                 OR
                 If both the MV components of either the left or the top subMB are
                 zero and their reference frame pointer pointing to 0
                 then MV for the skipped MB is zero
                 else the Median of the mv_pred_t is used */
                u1_a_in = (ps_cur_mb_info->u1_mb_ngbr_availablity &
                LEFT_MB_AVAILABLE_MASK);
                u1_b_in = (ps_cur_mb_info->u1_mb_ngbr_availablity &
                TOP_MB_AVAILABLE_MASK);
                if(((u1_a_in * u1_b_in) == 0)
                                || ((ps_mv_pred[LEFT]->i2_mv[0]
                                                | ps_mv_pred[LEFT]->i2_mv[1]
                                                | ps_mv_pred[LEFT]->i1_ref_frame[0])
                                                == 0)
                                || ((ps_mv_pred[TOP]->i2_mv[0]
                                                | ps_mv_pred[TOP]->i2_mv[1]
                                                | ps_mv_pred[TOP]->i1_ref_frame[0])
                                                == 0))
                {
                    ps_mv_final_pred->i2_mv[0] = 0;
                    ps_mv_final_pred->i2_mv[1] = 0;
                    break;
                }
                /* If the condition above is not true calculate the MV predictor
                 according to the process defined in sec 8.4.1.2.1 */
            default:
                ih264d_get_motion_vector_predictor(
                                ps_mv_final_pred, ps_mv_pred, u1_ref_idx, uc_lx,
                                (const UWORD8 *)gau1_ih264d_mv_pred_condition);
                break;
        }
    }
    return (u1_direct_zero_pred_flag);
}




void ih264d_rep_mv_colz(dec_struct_t *ps_dec,
                        mv_pred_t *ps_mv_pred_src,
                        mv_pred_t *ps_mv_pred_dst,
                        UWORD8 u1_sub_mb_num,
                        UWORD8 u1_colz,
                        UWORD8 u1_ht,
                        UWORD8 u1_wd)
{

    UWORD8 k, m;
    UWORD8 *pu1_colz = ps_dec->pu1_col_zero_flag + ps_dec->i4_submb_ofst
                    + u1_sub_mb_num;

    for(k = 0; k < u1_ht; k++)
    {
        for(m = 0; m < u1_wd; m++)
        {
            *(ps_mv_pred_dst + m) = *(ps_mv_pred_src);
            *(pu1_colz + m) = u1_colz;

        }
        pu1_colz += SUB_BLK_WIDTH;
        ps_mv_pred_dst += SUB_BLK_WIDTH;
    }
}

