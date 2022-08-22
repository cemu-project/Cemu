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
 * \file ih264d_mb_utils.c
 *
 * \brief
 *    Contains utitlity functions needed for Macroblock decoding
 *
 * \date
 *    18/12/2002
 *
 * \author  AI
 **************************************************************************
 */
#include <string.h>
#include <stdlib.h>
#include "ih264d_bitstrm.h"
#include "ih264d_defs.h"
#include "ih264d_debug.h"
#include "ih264d_structs.h"
#include "ih264d_defs.h"
#include "ih264d_mb_utils.h"
#include "ih264d_parse_slice.h"
#include "ih264d_error_handler.h"
#include "ih264d_parse_mb_header.h"
#include "ih264d_cabac.h"
#include "ih264d_defs.h"
#include "ih264d_tables.h"

/*****************************************************************************/
/*                                                                           */
/*  Function Name : get_mb_info_cavlc                                        */
/*                                                                           */
/*  Description   : This function sets the following information of cur MB   */
/*                  (a) mb_x and mb_y                                        */
/*                  (b) Neighbour availablity                                */
/*                  (c) Macroblock location in the frame buffer              */
/*                  (e) For mbaff predicts field/frame u4_flag for topMb        */
/*                      and sets the field/frame for botMb. This is          */
/*                      written in ps_dec->u1_cur_mb_fld_dec_flag            */
/*                                                                           */
/*  Inputs        : pointer to decstruct                                     */
/*                  pointer to current mb info                               */
/*                  currentMbaddress                                         */
/*                                                                           */
/*  Processing    : leftMb and TopMb params are used by DecMbskip and        */
/*                  DecCtxMbfield  modules so that these modules do not      */
/*                  check for neigbour availability and then find the        */
/*                  neigbours for context increments                         */
/*                                                                           */
/*  Returns       : OK                                                       */
/*                                                                           */
/*  Issues        : <List any issues or problems with this function>         */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         13 07 2002   Jay             Draft                                */
/*                                                                           */
/*****************************************************************************/

UWORD32 ih264d_get_mb_info_cavlc_nonmbaff(dec_struct_t *ps_dec,
                                          const UWORD16 u2_cur_mb_address,
                                          dec_mb_info_t * ps_cur_mb_info,
                                          UWORD32 u4_mbskip_run)
{
    WORD32 mb_x;
    WORD32 mb_y;
    UWORD8 u1_mb_ngbr_avail = 0;
    UWORD16 u2_frm_width_in_mb = ps_dec->u2_frm_wd_in_mbs;
    WORD16 i2_prev_slice_mbx = ps_dec->i2_prev_slice_mbx;
    UWORD16 u2_top_right_mask = TOP_RIGHT_DEFAULT_AVAILABLE;
    UWORD16 u2_top_left_mask = TOP_LEFT_DEFAULT_AVAILABLE;
    UNUSED(u4_mbskip_run);
    /*--------------------------------------------------------------------*/
    /* Calculate values of mb_x and mb_y                                  */
    /*--------------------------------------------------------------------*/
    mb_x = (WORD16)ps_dec->u2_mbx;
    mb_y = (WORD16)ps_dec->u2_mby;

    ps_dec->u2_cur_mb_addr = u2_cur_mb_address;

    mb_x++;

    if(mb_x == u2_frm_width_in_mb)
    {
        mb_x = 0;
        mb_y++;
    }
    if(mb_y > ps_dec->i2_prev_slice_mby)
    {
        /* if not in the immemdiate row of prev slice end then top
         will be available */
        if(mb_y > (ps_dec->i2_prev_slice_mby + 1))
            i2_prev_slice_mbx = -1;

        if(mb_x > i2_prev_slice_mbx)
        {
            u1_mb_ngbr_avail |= TOP_MB_AVAILABLE_MASK;
            u2_top_right_mask |= TOP_RIGHT_TOP_AVAILABLE;
            u2_top_left_mask |= TOP_LEFT_TOP_AVAILABLE;
        }

        if((mb_x > (i2_prev_slice_mbx - 1))
                        && (mb_x != (u2_frm_width_in_mb - 1)))
        {
            u1_mb_ngbr_avail |= TOP_RIGHT_MB_AVAILABLE_MASK;
            u2_top_right_mask |= TOP_RIGHT_TOPR_AVAILABLE;
        }

        if(mb_x > (i2_prev_slice_mbx + 1))
        {
            u1_mb_ngbr_avail |= TOP_LEFT_MB_AVAILABLE_MASK;
            u2_top_left_mask |= TOP_LEFT_TOPL_AVAILABLE;
        }

        /* Next row  Left will be available*/
        i2_prev_slice_mbx = -1;
    }

    /* Same row */
    if(mb_x > (i2_prev_slice_mbx + 1))
    {
        u1_mb_ngbr_avail |= LEFT_MB_AVAILABLE_MASK;
        u2_top_left_mask |= TOP_LEFT_LEFT_AVAILABLE;
    }

    {
        mb_neigbour_params_t *ps_cur_mb_row = ps_dec->ps_cur_mb_row;
        mb_neigbour_params_t *ps_top_mb_row = ps_dec->ps_top_mb_row;

        /* copy the parameters of topleft Mb */
        ps_cur_mb_info->u1_topleft_mbtype = ps_dec->u1_topleft_mbtype;
        /* Neighbour pointer assignments*/
        ps_cur_mb_info->ps_curmb = ps_cur_mb_row + mb_x;
        ps_cur_mb_info->ps_left_mb = ps_cur_mb_row + mb_x - 1;
        ps_cur_mb_info->ps_top_mb = ps_top_mb_row + mb_x;
        ps_cur_mb_info->ps_top_right_mb = ps_top_mb_row + mb_x + 1;

        /* Update the parameters of topleftmb*/
        ps_dec->u1_topleft_mbtype = ps_cur_mb_info->ps_top_mb->u1_mb_type;
    }

    ps_dec->u2_mby = mb_y;
    ps_dec->u2_mbx = mb_x;
    ps_cur_mb_info->u2_mbx = mb_x;
    ps_cur_mb_info->u2_mby = mb_y;
    ps_cur_mb_info->u1_topmb = 1;
    ps_dec->i4_submb_ofst += SUB_BLK_SIZE;
    ps_dec->u1_mb_ngbr_availablity = u1_mb_ngbr_avail;
    ps_cur_mb_info->u1_mb_ngbr_availablity = u1_mb_ngbr_avail;
    ps_cur_mb_info->ps_curmb->u1_mb_fld = ps_dec->u1_cur_mb_fld_dec_flag;
    ps_cur_mb_info->u1_mb_field_decodingflag = ps_dec->u1_cur_mb_fld_dec_flag;
    ps_cur_mb_info->u2_top_left_avail_mask = u2_top_left_mask;
    ps_cur_mb_info->u2_top_right_avail_mask = u2_top_right_mask;
    return (OK);

}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : get_mb_info_cavlc                                        */
/*                                                                           */
/*  Description   : This function sets the following information of cur MB   */
/*                  (a) mb_x and mb_y                                        */
/*                  (b) Neighbour availablity                                */
/*                  (c) Macroblock location in the frame buffer              */
/*                  (e) For mbaff predicts field/frame u4_flag for topMb        */
/*                      and sets the field/frame for botMb. This is          */
/*                      written in ps_dec->u1_cur_mb_fld_dec_flag            */
/*                                                                           */
/*  Inputs        : pointer to decstruct                                     */
/*                  pointer to current mb info                               */
/*                  currentMbaddress                                         */
/*                                                                           */
/*  Processing    : leftMb and TopMb params are used by DecMbskip and        */
/*                  DecCtxMbfield  modules so that these modules do not      */
/*                  check for neigbour availability and then find the        */
/*                  neigbours for context increments                         */
/*                                                                           */
/*  Returns       : OK                                                       */
/*                                                                           */
/*  Issues        : <List any issues or problems with this function>         */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         13 07 2002   Jay             Draft                                */
/*                                                                           */
/*****************************************************************************/

UWORD32 ih264d_get_mb_info_cavlc_mbaff(dec_struct_t *ps_dec,
                                       const UWORD16 u2_cur_mb_address,
                                       dec_mb_info_t * ps_cur_mb_info,
                                       UWORD32 u4_mbskip_run)
{
    UWORD16 u2_mb_x;
    UWORD16 u2_mb_y;
    UWORD8 u1_mb_ngbr_avail = 0;
    UWORD16 u2_frm_width_in_mb = ps_dec->u2_frm_wd_in_mbs;

    UWORD8 u1_top_mb = 1 - (u2_cur_mb_address & 0x01);
    WORD16 i2_prev_slice_mbx = ps_dec->i2_prev_slice_mbx;
    UWORD8 u1_cur_mb_field = 0;
    UWORD16 u2_top_right_mask = TOP_RIGHT_DEFAULT_AVAILABLE;
    UWORD16 u2_top_left_mask = TOP_LEFT_DEFAULT_AVAILABLE;

    /*--------------------------------------------------------------------*/
    /* Calculate values of mb_x and mb_y                                  */
    /*--------------------------------------------------------------------*/
    u2_mb_x = ps_dec->u2_mbx;
    u2_mb_y = ps_dec->u2_mby;

    ps_dec->u2_cur_mb_addr = u2_cur_mb_address;


    if(u1_top_mb)
    {
        u2_mb_x++;
        if(u2_mb_x == u2_frm_width_in_mb)
        {
            u2_mb_x = 0;
            u2_mb_y += 2;
        }
        if(u2_mb_y > ps_dec->i2_prev_slice_mby)
        {
            /* if not in the immemdiate row of prev slice end then top
             will be available */
            if(u2_mb_y > (ps_dec->i2_prev_slice_mby + 2))
                i2_prev_slice_mbx = -1;
            if(u2_mb_x > i2_prev_slice_mbx)
            {
                u1_mb_ngbr_avail |= TOP_MB_AVAILABLE_MASK;
                u1_cur_mb_field = ps_dec->ps_top_mb_row[u2_mb_x << 1].u1_mb_fld;
                u2_top_right_mask |= TOP_RIGHT_TOP_AVAILABLE;
                u2_top_left_mask |= TOP_LEFT_TOP_AVAILABLE;
            }
            if((u2_mb_x > (i2_prev_slice_mbx - 1))
                            && (u2_mb_x != (u2_frm_width_in_mb - 1)))
            {
                u1_mb_ngbr_avail |= TOP_RIGHT_MB_AVAILABLE_MASK;
                u2_top_right_mask |= TOP_RIGHT_TOPR_AVAILABLE;
            }

            if(u2_mb_x > (i2_prev_slice_mbx + 1))
            {
                u1_mb_ngbr_avail |= TOP_LEFT_MB_AVAILABLE_MASK;
                u2_top_left_mask |= TOP_LEFT_TOPL_AVAILABLE;
            }

            i2_prev_slice_mbx = -1;
        }
        /* Same row */
        if(u2_mb_x > (i2_prev_slice_mbx + 1))
        {
            u1_mb_ngbr_avail |= LEFT_MB_AVAILABLE_MASK;
            u1_cur_mb_field =
                            ps_dec->ps_cur_mb_row[(u2_mb_x << 1) - 1].u1_mb_fld;
            u2_top_left_mask |= TOP_LEFT_LEFT_AVAILABLE;
        }
        /* Read u1_cur_mb_field from the bitstream if u4_mbskip_run <= 1*/
        if(u4_mbskip_run <= 1)
            u1_cur_mb_field = (UWORD8)ih264d_get_bit_h264(ps_dec->ps_bitstrm);

        ps_dec->u1_cur_mb_fld_dec_flag = u1_cur_mb_field;
        ps_dec->u2_top_left_mask = u2_top_left_mask;
        ps_dec->u2_top_right_mask = u2_top_right_mask;
    }
    else
    {
        u1_mb_ngbr_avail = ps_dec->u1_mb_ngbr_availablity;
        u1_cur_mb_field = ps_dec->u1_cur_mb_fld_dec_flag;
        u2_top_left_mask = ps_dec->u2_top_left_mask;
        u2_top_right_mask = ps_dec->u2_top_right_mask;

        if(!u1_cur_mb_field)
        {
            /* Top is available */
            u1_mb_ngbr_avail |= TOP_MB_AVAILABLE_MASK;
            u2_top_right_mask |= TOP_RIGHT_TOP_AVAILABLE;
            u2_top_left_mask |= TOP_LEFT_TOP_AVAILABLE;
            /* Top Right not available */
            u1_mb_ngbr_avail &= TOP_RT_SUBBLOCK_MASK_MOD;
            u2_top_right_mask &= (~TOP_RIGHT_TOPR_AVAILABLE);

            if(u1_mb_ngbr_avail & LEFT_MB_AVAILABLE_MASK)
            {
                u1_mb_ngbr_avail |= TOP_LEFT_MB_AVAILABLE_MASK;
                u2_top_left_mask |= TOP_LEFT_LEFT_AVAILABLE;
                u2_top_left_mask |= TOP_LEFT_TOPL_AVAILABLE;
            }
        }
    }

    ps_dec->u2_mby = u2_mb_y;
    ps_dec->u2_mbx = u2_mb_x;
    ps_cur_mb_info->u2_mbx = u2_mb_x;
    ps_cur_mb_info->u2_mby = u2_mb_y;
    ps_cur_mb_info->u1_topmb = u1_top_mb;
    ps_dec->i4_submb_ofst += SUB_BLK_SIZE;
    ps_dec->u1_mb_ngbr_availablity = u1_mb_ngbr_avail;
    ps_cur_mb_info->u1_mb_ngbr_availablity = u1_mb_ngbr_avail;
    ps_cur_mb_info->u1_mb_field_decodingflag = u1_cur_mb_field;
    ps_cur_mb_info->u2_top_left_avail_mask = u2_top_left_mask;
    ps_cur_mb_info->u2_top_right_avail_mask = u2_top_right_mask;
    ih264d_get_mbaff_neighbours(ps_dec, ps_cur_mb_info, u1_cur_mb_field);
    return (OK);
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : get_mb_info_cabac                                        */
/*                                                                           */
/*  Description   : This function sets the following information of cur MB   */
/*                  (a) mb_x and mb_y                                        */
/*                  (b) Neighbour availablity                                */
/*                  (c) Macroblock location in the frame buffer              */
/*                  (e) leftMb parama and TopMb params of curMB              */
/*                  (f) For Mbaff case leftMb params and TopMb params of     */
/*                      bottomMb are also set if curMB is top                */
/*                  (g) For mbaff predicts field/frame u4_flag for topMb        */
/*                      and sets the field/frame for botMb. This is          */
/*                      written in ps_dec->u1_cur_mb_fld_dec_flag            */
/*                                                                           */
/*  Inputs        : pointer to decstruct                                     */
/*                  pointer to current mb info                               */
/*                  currentMbaddress                                         */
/*                                                                           */
/*  Processing    : leftMb and TopMb params are used by DecMbskip and        */
/*                  DecCtxMbfield  modules so that these modules do not      */
/*                  check for neigbour availability and then find the        */
/*                  neigbours for context increments                         */
/*                                                                           */
/*  Returns       : OK                                                       */
/*                                                                           */
/*  Issues        : <List any issues or problems with this function>         */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         13 07 2002   Jay             Draft                                */
/*                                                                           */
/*****************************************************************************/
UWORD32 ih264d_get_mb_info_cabac_nonmbaff(dec_struct_t *ps_dec,
                                          const UWORD16 u2_cur_mb_address,
                                          dec_mb_info_t * ps_cur_mb_info,
                                          UWORD32 u4_mbskip)
{
    WORD32 mb_x;
    WORD32 mb_y;
    UWORD32 u1_mb_ngbr_avail = 0;
    UWORD32 u2_frm_width_in_mb = ps_dec->u2_frm_wd_in_mbs;
    UWORD32 u1_top_mb = 1;
    WORD32 i2_prev_slice_mbx = ps_dec->i2_prev_slice_mbx;
    UWORD32 u2_top_right_mask = TOP_RIGHT_DEFAULT_AVAILABLE;
    UWORD32 u2_top_left_mask = TOP_LEFT_DEFAULT_AVAILABLE;
    ctxt_inc_mb_info_t * const p_ctx_inc_mb_map = ps_dec->p_ctxt_inc_mb_map;

    /*--------------------------------------------------------------------*/
    /* Calculate values of mb_x and mb_y                                  */
    /*--------------------------------------------------------------------*/
    mb_x = (WORD16)ps_dec->u2_mbx;
    mb_y = (WORD16)ps_dec->u2_mby;

    ps_dec->u2_cur_mb_addr = u2_cur_mb_address;

    mb_x++;
    if((UWORD32)mb_x == u2_frm_width_in_mb)
    {
        mb_x = 0;
        mb_y++;
    }
    /*********************************************************************/
    /* Cabac Context Initialisations                                     */
    /*********************************************************************/
    ps_dec->ps_curr_ctxt_mb_info = p_ctx_inc_mb_map + mb_x;
    ps_dec->p_left_ctxt_mb_info = p_ctx_inc_mb_map - 1;
    ps_dec->p_top_ctxt_mb_info = p_ctx_inc_mb_map - 1;

    /********************************************************************/
    /* neighbour availablility                                          */
    /********************************************************************/
    if(mb_y > ps_dec->i2_prev_slice_mby)
    {
        /* if not in the immemdiate row of prev slice end then top
         will be available */
        if(mb_y > (ps_dec->i2_prev_slice_mby + 1))
            i2_prev_slice_mbx = -1;

        if(mb_x > i2_prev_slice_mbx)
        {
            u1_mb_ngbr_avail |= TOP_MB_AVAILABLE_MASK;
            u2_top_right_mask |= TOP_RIGHT_TOP_AVAILABLE;
            u2_top_left_mask |= TOP_LEFT_TOP_AVAILABLE;
            ps_dec->p_top_ctxt_mb_info = ps_dec->ps_curr_ctxt_mb_info;
        }
        if((mb_x > (i2_prev_slice_mbx - 1))
                        && ((UWORD32)mb_x != (u2_frm_width_in_mb - 1)))
        {
            u1_mb_ngbr_avail |= TOP_RIGHT_MB_AVAILABLE_MASK;
            u2_top_right_mask |= TOP_RIGHT_TOPR_AVAILABLE;
        }

        if(mb_x > (i2_prev_slice_mbx + 1))
        {
            u1_mb_ngbr_avail |= TOP_LEFT_MB_AVAILABLE_MASK;
            u2_top_left_mask |= TOP_LEFT_TOPL_AVAILABLE;
        }
        /* Next row */
        i2_prev_slice_mbx = -1;
    }
    /* Same row */
    if(mb_x > (i2_prev_slice_mbx + 1))
    {
        u1_mb_ngbr_avail |= LEFT_MB_AVAILABLE_MASK;
        u2_top_left_mask |= TOP_LEFT_LEFT_AVAILABLE;
        ps_dec->p_left_ctxt_mb_info = ps_dec->ps_curr_ctxt_mb_info - 1;
    }
    {
        mb_neigbour_params_t *ps_cur_mb_row = ps_dec->ps_cur_mb_row;
        mb_neigbour_params_t *ps_top_mb_row = ps_dec->ps_top_mb_row;
        /* copy the parameters of topleft Mb */
        ps_cur_mb_info->u1_topleft_mbtype = ps_dec->u1_topleft_mbtype;
        /* Neighbour pointer assignments*/
        ps_cur_mb_info->ps_curmb = ps_cur_mb_row + mb_x;
        ps_cur_mb_info->ps_left_mb = ps_cur_mb_row + mb_x - 1;
        ps_cur_mb_info->ps_top_mb = ps_top_mb_row + mb_x;
        ps_cur_mb_info->ps_top_right_mb = ps_top_mb_row + mb_x + 1;

        /* Update the parameters of topleftmb*/
        ps_dec->u1_topleft_mbtype = ps_cur_mb_info->ps_top_mb->u1_mb_type;
    }

    ps_dec->u2_mby = mb_y;
    ps_dec->u2_mbx = mb_x;
    ps_cur_mb_info->u2_mbx = mb_x;
    ps_cur_mb_info->u2_mby = mb_y;
    ps_cur_mb_info->u1_topmb = u1_top_mb;
    ps_dec->i4_submb_ofst += SUB_BLK_SIZE;
    ps_dec->u1_mb_ngbr_availablity = u1_mb_ngbr_avail;
    ps_cur_mb_info->u1_mb_ngbr_availablity = u1_mb_ngbr_avail;
    ps_cur_mb_info->ps_curmb->u1_mb_fld = ps_dec->u1_cur_mb_fld_dec_flag;
    ps_cur_mb_info->u1_mb_field_decodingflag = ps_dec->u1_cur_mb_fld_dec_flag;
    ps_cur_mb_info->u2_top_left_avail_mask = u2_top_left_mask;
    ps_cur_mb_info->u2_top_right_avail_mask = u2_top_right_mask;

    /*********************************************************************/
    /*                  Assign the neigbours                             */
    /*********************************************************************/
    if(u4_mbskip)
    {
        UWORD32 u4_ctx_inc =
                        2
                                        - ((!!(ps_dec->p_top_ctxt_mb_info->u1_mb_type
                                                        & CAB_SKIP_MASK))
                                                        + (!!(ps_dec->p_left_ctxt_mb_info->u1_mb_type
                                                                        & CAB_SKIP_MASK)));

        u4_mbskip = ih264d_decode_bin(u4_ctx_inc, ps_dec->p_mb_skip_flag_t,
                                      ps_dec->ps_bitstrm, &ps_dec->s_cab_dec_env);

        if(!u4_mbskip)
        {
            if(!(u1_mb_ngbr_avail & LEFT_MB_AVAILABLE_MASK))
            {
                UWORD32 *pu4_buf;
                UWORD8 *pu1_buf;

                pu1_buf = ps_dec->pu1_left_nnz_y;
                pu4_buf = (UWORD32 *)pu1_buf;
                *pu4_buf = 0;
                pu1_buf = ps_dec->pu1_left_nnz_uv;
                pu4_buf = (UWORD32 *)pu1_buf;
                *pu4_buf = 0;


                *(ps_dec->pu1_left_yuv_dc_csbp) = 0;
                MEMSET_16BYTES(&ps_dec->pu1_left_mv_ctxt_inc[0][0], 0);
                *(UWORD32 *)ps_dec->pi1_left_ref_idx_ctxt_inc = 0;
            }
            if(!(u1_mb_ngbr_avail & TOP_MB_AVAILABLE_MASK))
            {
                MEMSET_16BYTES(ps_dec->ps_curr_ctxt_mb_info->u1_mv, 0);
                memset(ps_dec->ps_curr_ctxt_mb_info->i1_ref_idx, 0, 4);
            }
        }
    }
    return (u4_mbskip);
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : get_mb_info_cabac                                        */
/*                                                                           */
/*  Description   : This function sets the following information of cur MB   */
/*                  (a) mb_x and mb_y                                        */
/*                  (b) Neighbour availablity                                */
/*                  (c) Macroblock location in the frame buffer              */
/*                  (e) leftMb parama and TopMb params of curMB              */
/*                  (f) For Mbaff case leftMb params and TopMb params of     */
/*                      bottomMb are also set if curMB is top                */
/*                  (g) For mbaff predicts field/frame u4_flag for topMb        */
/*                      and sets the field/frame for botMb. This is          */
/*                      written in ps_dec->u1_cur_mb_fld_dec_flag            */
/*                                                                           */
/*  Inputs        : pointer to decstruct                                     */
/*                  pointer to current mb info                               */
/*                  currentMbaddress                                         */
/*                                                                           */
/*  Processing    : leftMb and TopMb params are used by DecMbskip and        */
/*                  DecCtxMbfield  modules so that these modules do not      */
/*                  check for neigbour availability and then find the        */
/*                  neigbours for context increments                         */
/*                                                                           */
/*  Returns       : OK                                                       */
/*                                                                           */
/*  Issues        : <List any issues or problems with this function>         */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         13 07 2002   Jay             Draft                                */
/*                                                                           */
/*****************************************************************************/

UWORD32 ih264d_get_mb_info_cabac_mbaff(dec_struct_t *ps_dec,
                                       const UWORD16 u2_cur_mb_address,
                                       dec_mb_info_t * ps_cur_mb_info,
                                       UWORD32 u4_mbskip)
{
    WORD32 mb_x;
    WORD32 mb_y;
    UWORD8 u1_mb_ngbr_avail = 0;
    UWORD16 u2_frm_width_in_mb = ps_dec->u2_frm_wd_in_mbs;
    ctxt_inc_mb_info_t * const p_ctx_inc_mb_map = ps_dec->p_ctxt_inc_mb_map;
    ctxt_inc_mb_info_t *ps_curr_ctxt, *ps_top_ctxt, *ps_left_ctxt;
    mb_neigbour_params_t *ps_cur_mb_row = ps_dec->ps_cur_mb_row;
    mb_neigbour_params_t *ps_top_mb_row = ps_dec->ps_top_mb_row;
    UWORD32 u4_left_mb_pair_fld = 0;
    UWORD32 u4_top_mb_pair_fld = 0;
    UWORD8 u1_cur_mb_field = 0;
    UWORD8 u1_top_mb = 1 - (u2_cur_mb_address & 0x01);
    WORD16 i2_prev_slice_mbx = ps_dec->i2_prev_slice_mbx;
    UWORD16 u2_top_right_mask = TOP_RIGHT_DEFAULT_AVAILABLE;
    UWORD16 u2_top_left_mask = TOP_LEFT_DEFAULT_AVAILABLE;

    /*--------------------------------------------------------------------*/
    /* Calculate values of mb_x and mb_y                                  */
    /*--------------------------------------------------------------------*/
    mb_x = (WORD16)ps_dec->u2_mbx;
    mb_y = (WORD16)ps_dec->u2_mby;

    ps_dec->u2_cur_mb_addr = u2_cur_mb_address;

    ps_top_ctxt = ps_left_ctxt = p_ctx_inc_mb_map - 1;

    if(u1_top_mb)
    {
        ctxt_inc_mb_info_t *ps_left_mb_of_bot = ps_left_ctxt;
        ctxt_inc_mb_info_t *ps_top_mb_of_bot = ps_top_ctxt;

        mb_x++;

        if(mb_x == u2_frm_width_in_mb)
        {
            mb_x = 0;
            mb_y += 2;
        }

        ps_curr_ctxt = p_ctx_inc_mb_map + (mb_x << 1);
        if(mb_y > ps_dec->i2_prev_slice_mby)
        {
            UWORD8 u1_cur_mb_fld_flag_known = 0;
            /* Next row */
            if(mb_x > 0)
            {
                /***********************************************************************/
                /*                    Left Mb is avialable                             */
                /***********************************************************************/
                u1_mb_ngbr_avail |= LEFT_MB_AVAILABLE_MASK;
                ps_left_ctxt = ps_curr_ctxt - 2;
                ps_left_mb_of_bot = ps_curr_ctxt - 1;
                u1_cur_mb_field = u4_left_mb_pair_fld = ps_cur_mb_row[(mb_x
                                << 1) - 1].u1_mb_fld;
                u1_cur_mb_fld_flag_known = 1;
                u2_top_left_mask |= TOP_LEFT_LEFT_AVAILABLE;
            }
            /* if not in the immemdiate row of prev slice end then top
             will be available */
            if(mb_y > (ps_dec->i2_prev_slice_mby + 2))
                i2_prev_slice_mbx = -1;
            if(mb_x > i2_prev_slice_mbx)
            {
                /*********************************************************************/
                /*                    Top Mb is avialable                            */
                /*********************************************************************/
                u1_mb_ngbr_avail |= TOP_MB_AVAILABLE_MASK;
                u2_top_right_mask |= TOP_RIGHT_TOP_AVAILABLE;
                u2_top_left_mask |= TOP_LEFT_TOP_AVAILABLE;

                /* point to MbAddrB + 1 */
                ps_top_ctxt = ps_curr_ctxt + 1;
                u4_top_mb_pair_fld = ps_top_mb_row[(mb_x << 1)].u1_mb_fld;

                u1_cur_mb_field =
                                u1_cur_mb_fld_flag_known ?
                                                u1_cur_mb_field :
                                                u4_top_mb_pair_fld;
                ps_top_mb_of_bot = u1_cur_mb_field ? ps_top_ctxt : ps_curr_ctxt;

                /* MbAddrB */
                ps_top_ctxt -= (u1_cur_mb_field && u4_top_mb_pair_fld);
            }

            if((mb_x > (i2_prev_slice_mbx - 1))
                            && (mb_x != (u2_frm_width_in_mb - 1)))
            {
                u1_mb_ngbr_avail |= TOP_RIGHT_MB_AVAILABLE_MASK;
                u2_top_right_mask |= TOP_RIGHT_TOPR_AVAILABLE;
            }

            if(mb_x > (i2_prev_slice_mbx + 1))
            {
                u1_mb_ngbr_avail |= TOP_LEFT_MB_AVAILABLE_MASK;
                u2_top_left_mask |= TOP_LEFT_TOPL_AVAILABLE;
            }
        }
        else
        {
            /* Same row */
            if(mb_x > (i2_prev_slice_mbx + 1))
            {
                /***************************************************************/
                /*                    Left Mb is avialable                     */
                /***************************************************************/
                u1_mb_ngbr_avail |= LEFT_MB_AVAILABLE_MASK;

                u1_cur_mb_field = u4_left_mb_pair_fld = ps_cur_mb_row[(mb_x
                                << 1) - 1].u1_mb_fld;
                ps_left_ctxt = ps_curr_ctxt - 2;
                ps_left_mb_of_bot = ps_curr_ctxt - 1;
                u2_top_left_mask |= TOP_LEFT_LEFT_AVAILABLE;
            }
        }
        /*********************************************************/
        /* Check whether the call is from I slice or Inter slice */
        /*********************************************************/
        if(u4_mbskip)
        {
            UWORD32 u4_ctx_inc = 2
                            - ((!!(ps_top_ctxt->u1_mb_type & CAB_SKIP_MASK))
                                            + (!!(ps_left_ctxt->u1_mb_type
                                                            & CAB_SKIP_MASK)));
            dec_bit_stream_t * const ps_bitstrm = ps_dec->ps_bitstrm;
            decoding_envirnoment_t *ps_cab_dec_env = &ps_dec->s_cab_dec_env;
            bin_ctxt_model_t *p_mb_skip_flag_t = ps_dec->p_mb_skip_flag_t;

            ps_dec->u4_next_mb_skip = 0;
            u4_mbskip = ih264d_decode_bin(u4_ctx_inc, p_mb_skip_flag_t,
                                          ps_bitstrm, ps_cab_dec_env);

            if(u4_mbskip)
            {
                UWORD32 u4_next_mbskip;
                ps_curr_ctxt->u1_mb_type = CAB_SKIP;

                u4_ctx_inc =
                                2
                                                - ((!!(ps_top_mb_of_bot->u1_mb_type
                                                                & CAB_SKIP_MASK))
                                                                + (!!(ps_left_mb_of_bot->u1_mb_type
                                                                                & CAB_SKIP_MASK)));

                /* Decode the skip u4_flag of bottom Mb */
                u4_next_mbskip = ih264d_decode_bin(u4_ctx_inc, p_mb_skip_flag_t,
                                                   ps_bitstrm,
                                                   ps_cab_dec_env);

                ps_dec->u4_next_mb_skip = u4_next_mbskip;

                if(!u4_next_mbskip)
                {
                    u4_ctx_inc = u4_top_mb_pair_fld + u4_left_mb_pair_fld;

                    u1_cur_mb_field = ih264d_decode_bin(
                                    u4_ctx_inc, ps_dec->p_mb_field_dec_flag_t,
                                    ps_bitstrm, ps_cab_dec_env);
                }
            }
        }

        if(!u4_mbskip)
        {
            UWORD32 u4_ctx_inc = u4_top_mb_pair_fld + u4_left_mb_pair_fld;
            u1_cur_mb_field = ih264d_decode_bin(u4_ctx_inc,
                                                ps_dec->p_mb_field_dec_flag_t,
                                                ps_dec->ps_bitstrm,
                                                &ps_dec->s_cab_dec_env);
        }

        ps_dec->u1_cur_mb_fld_dec_flag = u1_cur_mb_field;
        ps_dec->u2_top_left_mask = u2_top_left_mask;
        ps_dec->u2_top_right_mask = u2_top_right_mask;
        ps_dec->u2_mby = mb_y;
        ps_dec->u2_mbx = mb_x;
    }
    else
    {
        u1_cur_mb_field = ps_dec->u1_cur_mb_fld_dec_flag;
        u1_mb_ngbr_avail = ps_dec->u1_mb_ngbr_availablity;
        u2_top_left_mask = ps_dec->u2_top_left_mask;
        u2_top_right_mask = ps_dec->u2_top_right_mask;
        ps_curr_ctxt = p_ctx_inc_mb_map + (mb_x << 1) + 1;

        if(u1_mb_ngbr_avail & LEFT_MB_AVAILABLE_MASK)
        {
            u4_left_mb_pair_fld = ps_cur_mb_row[(mb_x << 1) - 1].u1_mb_fld;

            /* point to A if top else A+1 */
            ps_left_ctxt = ps_curr_ctxt - 2
                            - (u4_left_mb_pair_fld != u1_cur_mb_field);
        }

        if(u1_cur_mb_field)
        {
            if(u1_mb_ngbr_avail & TOP_MB_AVAILABLE_MASK)
            {
                /* point to MbAddrB + 1 */
                ps_top_ctxt = ps_curr_ctxt;
            }
        }
        else
        {
            /* Top is available */
            u1_mb_ngbr_avail |= TOP_MB_AVAILABLE_MASK;
            u2_top_right_mask |= TOP_RIGHT_TOP_AVAILABLE;
            u2_top_left_mask |= TOP_LEFT_TOP_AVAILABLE;
            /* Top Right not available */
            u1_mb_ngbr_avail &= TOP_RT_SUBBLOCK_MASK_MOD;
            u2_top_right_mask &= (~TOP_RIGHT_TOPR_AVAILABLE);

            if(u1_mb_ngbr_avail & LEFT_MB_AVAILABLE_MASK)
            {
                u1_mb_ngbr_avail |= TOP_LEFT_MB_AVAILABLE_MASK;
                u2_top_left_mask |= TOP_LEFT_LEFT_AVAILABLE;
                u2_top_left_mask |= TOP_LEFT_TOPL_AVAILABLE;
            }

            /* CurMbAddr - 1 */
            ps_top_ctxt = ps_curr_ctxt - 1;
        }

        if(u4_mbskip)
        {
            if(ps_curr_ctxt[-1].u1_mb_type & CAB_SKIP_MASK)
            {
                /* If previous mb is skipped, return value of next mb skip */
                u4_mbskip = ps_dec->u4_next_mb_skip;

            }
            else
            {
                /* If previous mb is not skipped then call DecMbSkip */
                UWORD32 u4_ctx_inc =
                                2
                                                - ((!!(ps_top_ctxt->u1_mb_type
                                                                & CAB_SKIP_MASK))
                                                                + (!!(ps_left_ctxt->u1_mb_type
                                                                                & CAB_SKIP_MASK)));

                u4_mbskip = ih264d_decode_bin(u4_ctx_inc,
                                              ps_dec->p_mb_skip_flag_t,
                                              ps_dec->ps_bitstrm,
                                              &ps_dec->s_cab_dec_env);
            }
        }
    }

    ps_cur_mb_info->u2_mbx = mb_x;
    ps_cur_mb_info->u2_mby = mb_y;
    ps_cur_mb_info->u1_topmb = u1_top_mb;
    ps_dec->i4_submb_ofst += SUB_BLK_SIZE;
    ps_dec->u1_mb_ngbr_availablity = u1_mb_ngbr_avail;
    ps_cur_mb_info->u1_mb_ngbr_availablity = u1_mb_ngbr_avail;
    ps_cur_mb_info->u1_mb_field_decodingflag = u1_cur_mb_field;
    ps_cur_mb_info->u2_top_left_avail_mask = u2_top_left_mask;
    ps_cur_mb_info->u2_top_right_avail_mask = u2_top_right_mask;

    ih264d_get_mbaff_neighbours(ps_dec, ps_cur_mb_info, u1_cur_mb_field);
    {
        ih264d_get_cabac_context_mbaff(ps_dec, ps_cur_mb_info, u4_mbskip);
    }

    {
        bin_ctxt_model_t *p_cabac_ctxt_table_t = ps_dec->p_cabac_ctxt_table_t;

        if(u1_cur_mb_field)
        {
            p_cabac_ctxt_table_t += SIGNIFICANT_COEFF_FLAG_FLD;
        }
        else
        {
            p_cabac_ctxt_table_t += SIGNIFICANT_COEFF_FLAG_FRAME;
        }
        {
            bin_ctxt_model_t * * p_significant_coeff_flag_t =
                            ps_dec->p_significant_coeff_flag_t;
            p_significant_coeff_flag_t[0] = p_cabac_ctxt_table_t
                            + SIG_COEFF_CTXT_CAT_0_OFFSET;
            p_significant_coeff_flag_t[1] = p_cabac_ctxt_table_t
                            + SIG_COEFF_CTXT_CAT_1_OFFSET;
            p_significant_coeff_flag_t[2] = p_cabac_ctxt_table_t
                            + SIG_COEFF_CTXT_CAT_2_OFFSET;
            p_significant_coeff_flag_t[3] = p_cabac_ctxt_table_t
                            + SIG_COEFF_CTXT_CAT_3_OFFSET;
            p_significant_coeff_flag_t[4] = p_cabac_ctxt_table_t
                            + SIG_COEFF_CTXT_CAT_4_OFFSET;
            p_significant_coeff_flag_t[5] = p_cabac_ctxt_table_t
                            + SIG_COEFF_CTXT_CAT_5_OFFSET;

        }
    }
    return (u4_mbskip);
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_get_cabac_context_mbaff                                  */
/*                                                                           */
/*  Description   : Gets the current macroblock Cabac Context and sets the   */
/*                  top and left cabac context ptrs in CtxIncMbMap           */
/*                  1. For Coss field left neigbours it alters coded block   */
/*                     u4_flag , motion vectors, reference indices, cbp of      */
/*                     the left neigbours which increases the code i4_size      */
/*                  2. For Coss field top neigbours it alters motion         */
/*                     vectors reference indices of the top neigbours        */
/*                     which further increases the code i4_size                 */
/*                                                                           */
/*  Inputs        : 1. dec_struct_t                                             */
/*                  2. CurMbAddr used for Mbaff (only to see if curMB        */
/*                  is top or bottom)                                        */
/*                  3. uc_curMbFldDecFlag only for Mbaff                     */
/*                                                                           */
/*  Returns       : 0                                                        */
/*                                                                           */
/*  Issues        : code i4_size can be reduced if ui_CodedBlockFlag storage    */
/*                  structure in context is changed. This change however     */
/*                  would break the parseResidual4x4Cabac asm routine.       */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         18 06 2005   Jay                                                  */
/*                                                                           */
/*****************************************************************************/
UWORD32 ih264d_get_cabac_context_mbaff(dec_struct_t * ps_dec,
                                       dec_mb_info_t *ps_cur_mb_info,
                                       UWORD32 u4_mbskip)
{
    const UWORD8 u1_mb_ngbr_availablity = ps_dec->u1_mb_ngbr_availablity;
    ctxt_inc_mb_info_t * const p_ctx_inc_mb_map = ps_dec->p_ctxt_inc_mb_map;

    UWORD8 (*pu1_left_mv_ctxt_inc_2d)[4] = &ps_dec->pu1_left_mv_ctxt_inc[0];
    WORD8 (*pi1_left_ref_idx_ctxt_inc) = ps_dec->pi1_left_ref_idx_ctxt_inc;
    const UWORD8 u1_cur_mb_fld_flag = ps_cur_mb_info->u1_mb_field_decodingflag;
    const UWORD8 u1_topmb = ps_cur_mb_info->u1_topmb;
    const UWORD8 uc_botMb = 1 - ps_cur_mb_info->u1_topmb;

    ctxt_inc_mb_info_t * ps_leftMB;

    ps_dec->ps_curr_ctxt_mb_info = p_ctx_inc_mb_map + (ps_dec->u2_mbx << 1);
    ps_dec->p_top_ctxt_mb_info = ps_dec->ps_curr_ctxt_mb_info;

    if(u1_topmb)
    {
        pu1_left_mv_ctxt_inc_2d = ps_dec->u1_left_mv_ctxt_inc_arr[0];
        pi1_left_ref_idx_ctxt_inc = &ps_dec->i1_left_ref_idx_ctx_inc_arr[0][0];
        ps_dec->pu1_left_yuv_dc_csbp = &ps_dec->u1_yuv_dc_csbp_topmb;
    }
    else
    {
        /* uc_botMb */
        pu1_left_mv_ctxt_inc_2d = ps_dec->u1_left_mv_ctxt_inc_arr[1];
        pi1_left_ref_idx_ctxt_inc = &ps_dec->i1_left_ref_idx_ctx_inc_arr[1][0];
        ps_dec->pu1_left_yuv_dc_csbp = &ps_dec->u1_yuv_dc_csbp_bot_mb;
        ps_dec->ps_curr_ctxt_mb_info += 1;
    }

    ps_dec->pu1_left_mv_ctxt_inc = pu1_left_mv_ctxt_inc_2d;
    ps_dec->pi1_left_ref_idx_ctxt_inc = pi1_left_ref_idx_ctxt_inc;

    if(u1_mb_ngbr_availablity & LEFT_MB_AVAILABLE_MASK)
    {
        const UWORD8 u1_left_mb_fld_flag = ps_cur_mb_info->ps_left_mb->u1_mb_fld;

        ps_leftMB = ps_dec->ps_curr_ctxt_mb_info - 2;
        if(u1_left_mb_fld_flag != u1_cur_mb_fld_flag)
        {
            ctxt_inc_mb_info_t *ps_tempLeft;
            UWORD8 u1_cbp_t, u1_cbp_b;
            UWORD8 u1_cr_cpb;

            ps_leftMB -= uc_botMb;
            ps_tempLeft = ps_dec->ps_left_mb_ctxt_info;
            ps_tempLeft->u1_mb_type = ps_leftMB->u1_mb_type;
            ps_tempLeft->u1_intra_chroma_pred_mode =
                            ps_leftMB->u1_intra_chroma_pred_mode;

            ps_tempLeft->u1_transform8x8_ctxt = ps_leftMB->u1_transform8x8_ctxt;

            u1_cr_cpb = ps_leftMB->u1_cbp;
            /*****************************************************************/
            /* reform RefIdx, CBP, MV and CBF ctxInc taking care of A and A+1*/
            /*****************************************************************/
            if(u1_cur_mb_fld_flag)
            {
                /* current MB is a FLD and left a FRM */
                UWORD8 (* const pu1_left_mv_ctxt_inc_2d_arr_top)[4] =
                                ps_dec->u1_left_mv_ctxt_inc_arr[0];
                UWORD8 (* const pu1_left_mv_ctxt_inc_2d_arr_bot)[4] =
                                ps_dec->u1_left_mv_ctxt_inc_arr[1];
                WORD8 (* const i1_left_ref_idx_ctxt_inc_arr_top) =
                                &ps_dec->i1_left_ref_idx_ctx_inc_arr[0][0];
                WORD8 (* const i1_left_ref_idx_ctxt_inc_arr_bot) =
                                &ps_dec->i1_left_ref_idx_ctx_inc_arr[1][0];

                u1_cbp_t = ps_leftMB->u1_cbp;
                u1_cbp_b = (ps_leftMB + 1)->u1_cbp;
                ps_tempLeft->u1_cbp = (u1_cbp_t & 0x02)
                                | ((u1_cbp_b & 0x02) << 2);

                // set motionvectors as
                // 0T = 0T  0B = 0T
                // 1T = 2T  1B = 2T
                // 2T = 0B  2B = 0B
                // 3T = 2B  3B = 2B
                if(u1_topmb)
                {
                    /********************************************/
                    /*    Bottoms  DC CBF = Top DC CBF          */
                    /********************************************/
                    ps_dec->u1_yuv_dc_csbp_bot_mb =
                                    ps_dec->u1_yuv_dc_csbp_topmb;

                    *(UWORD32 *)pu1_left_mv_ctxt_inc_2d[3] =
                                    *(UWORD32 *)pu1_left_mv_ctxt_inc_2d_arr_bot[2];
                    *(UWORD32 *)pu1_left_mv_ctxt_inc_2d[1] =
                                    *(UWORD32 *)pu1_left_mv_ctxt_inc_2d_arr_top[2];
                    *(UWORD32 *)pu1_left_mv_ctxt_inc_2d[2] =
                                    *(UWORD32 *)pu1_left_mv_ctxt_inc_2d_arr_bot[0];
                    *(UWORD32 *)pu1_left_mv_ctxt_inc_2d[0] =
                                    *(UWORD32 *)pu1_left_mv_ctxt_inc_2d_arr_top[0];

                    i1_left_ref_idx_ctxt_inc_arr_top[1] =
                                    i1_left_ref_idx_ctxt_inc_arr_bot[0];
                    i1_left_ref_idx_ctxt_inc_arr_top[3] =
                                    i1_left_ref_idx_ctxt_inc_arr_bot[2];

                    *(UWORD32 *)(i1_left_ref_idx_ctxt_inc_arr_bot) =
                                    *(UWORD32 *)(i1_left_ref_idx_ctxt_inc_arr_top);

                    memcpy(pu1_left_mv_ctxt_inc_2d_arr_bot,
                           pu1_left_mv_ctxt_inc_2d_arr_top, 16);
                }

                {
                    UWORD8 i;
                    for(i = 0; i < 4; i++)
                    {
                        pu1_left_mv_ctxt_inc_2d[i][1] >>= 1;
                        pu1_left_mv_ctxt_inc_2d[i][3] >>= 1;
                    }
                }
            }
            else
            {
                /* current MB is a FRM and left FLD */
                if(u1_topmb)
                {
                    u1_cbp_t = ps_leftMB->u1_cbp;
                    u1_cbp_t = (u1_cbp_t & 0x02);
                    ps_tempLeft->u1_cbp = (u1_cbp_t | (u1_cbp_t << 2));

                    /********************************************/
                    /*    Bottoms  DC CBF = Top DC CBF          */
                    /********************************************/
                    ps_dec->u1_yuv_dc_csbp_bot_mb =
                                    ps_dec->u1_yuv_dc_csbp_topmb;

                    // set motionvectors as
                    // 3B = 2B = 3T
                    // 1B = 0B = 2T
                    // 3T = 2T = 1T
                    // 1T = 0T = 0T

                    *(UWORD32 *)pu1_left_mv_ctxt_inc_2d[7] =
                                    *(UWORD32 *)pu1_left_mv_ctxt_inc_2d[3];
                    *(UWORD32 *)pu1_left_mv_ctxt_inc_2d[6] =
                                    *(UWORD32 *)pu1_left_mv_ctxt_inc_2d[3];

                    *(UWORD32 *)pu1_left_mv_ctxt_inc_2d[5] =
                                    *(UWORD32 *)pu1_left_mv_ctxt_inc_2d[2];
                    *(UWORD32 *)pu1_left_mv_ctxt_inc_2d[4] =
                                    *(UWORD32 *)pu1_left_mv_ctxt_inc_2d[2];

                    *(UWORD32 *)pu1_left_mv_ctxt_inc_2d[3] =
                                    *(UWORD32 *)pu1_left_mv_ctxt_inc_2d[1];
                    *(UWORD32 *)pu1_left_mv_ctxt_inc_2d[2] =
                                    *(UWORD32 *)pu1_left_mv_ctxt_inc_2d[1];

                    *(UWORD32 *)pu1_left_mv_ctxt_inc_2d[1] =
                                    *(UWORD32 *)pu1_left_mv_ctxt_inc_2d[0];

                    pi1_left_ref_idx_ctxt_inc[7] = (pi1_left_ref_idx_ctxt_inc[3]
                                    - 1);
                    pi1_left_ref_idx_ctxt_inc[6] = (pi1_left_ref_idx_ctxt_inc[3]
                                    - 1);

                    pi1_left_ref_idx_ctxt_inc[5] = (pi1_left_ref_idx_ctxt_inc[1]
                                    - 1);
                    pi1_left_ref_idx_ctxt_inc[4] = (pi1_left_ref_idx_ctxt_inc[1]
                                    - 1);

                    pi1_left_ref_idx_ctxt_inc[3] = (pi1_left_ref_idx_ctxt_inc[2]
                                    - 1);
                    pi1_left_ref_idx_ctxt_inc[2] = (pi1_left_ref_idx_ctxt_inc[2]
                                    - 1);

                    pi1_left_ref_idx_ctxt_inc[1] = (pi1_left_ref_idx_ctxt_inc[0]
                                    - 1);
                    pi1_left_ref_idx_ctxt_inc[0] = (pi1_left_ref_idx_ctxt_inc[0]
                                    - 1);
                }
                else
                {
                    u1_cbp_t = ps_leftMB->u1_cbp;
                    u1_cbp_t = (u1_cbp_t & 0x08);
                    ps_tempLeft->u1_cbp = (u1_cbp_t | (u1_cbp_t >> 2));
                }

                {
                    UWORD8 i;
                    for(i = 0; i < 4; i++)
                    {
                        pu1_left_mv_ctxt_inc_2d[i][1] <<= 1;
                        pu1_left_mv_ctxt_inc_2d[i][3] <<= 1;
                    }
                }

            }

            ps_tempLeft->u1_cbp = ps_tempLeft->u1_cbp + ((u1_cr_cpb >> 4) << 4);
            ps_leftMB = ps_tempLeft;
        }

        ps_dec->p_left_ctxt_mb_info = ps_leftMB;
    }
    else
    {
        ps_dec->p_left_ctxt_mb_info = p_ctx_inc_mb_map - 1;
        if(!u4_mbskip)
        {
            *(ps_dec->pu1_left_yuv_dc_csbp) = 0;

            MEMSET_16BYTES(&pu1_left_mv_ctxt_inc_2d[0][0], 0);
            *(UWORD32 *)pi1_left_ref_idx_ctxt_inc = 0;
        }
    }

    /*************************************************************************/
    /*                Now get the top context mb info                        */
    /*************************************************************************/
    {
        UWORD8 (*u1_top_mv_ctxt_inc_arr_2d)[4] =
                        ps_dec->ps_curr_ctxt_mb_info->u1_mv;
        WORD8 (*pi1_top_ref_idx_ctxt_inc) =
                        ps_dec->ps_curr_ctxt_mb_info->i1_ref_idx;
        UWORD8 uc_topMbFldDecFlag = ps_cur_mb_info->ps_top_mb->u1_mb_fld;

        if(u1_mb_ngbr_availablity & TOP_MB_AVAILABLE_MASK)
        {
            if(ps_cur_mb_info->i1_offset)
                ps_dec->p_top_ctxt_mb_info += 1;

            if(!u4_mbskip)
            {
                memcpy(u1_top_mv_ctxt_inc_arr_2d,
                       &ps_dec->p_top_ctxt_mb_info->u1_mv, 16);
                memcpy(pi1_top_ref_idx_ctxt_inc,
                       &ps_dec->p_top_ctxt_mb_info->i1_ref_idx, 4);
                if(uc_topMbFldDecFlag ^ u1_cur_mb_fld_flag)
                {
                    UWORD8 i;
                    if(u1_cur_mb_fld_flag)
                    {
                        for(i = 0; i < 4; i++)
                        {
                            u1_top_mv_ctxt_inc_arr_2d[i][1] >>= 1;
                            u1_top_mv_ctxt_inc_arr_2d[i][3] >>= 1;
                        }
                    }
                    else
                    {
                        for(i = 0; i < 4; i++)
                        {
                            u1_top_mv_ctxt_inc_arr_2d[i][1] <<= 1;
                            u1_top_mv_ctxt_inc_arr_2d[i][3] <<= 1;
                            pi1_top_ref_idx_ctxt_inc[i] -= 1;
                        }
                    }
                }
            }
        }
        else
        {
            ps_dec->p_top_ctxt_mb_info = p_ctx_inc_mb_map - 1;
            if(!u4_mbskip)
            {

                MEMSET_16BYTES(&u1_top_mv_ctxt_inc_arr_2d[0][0], 0);
                memset(pi1_top_ref_idx_ctxt_inc, 0, 4);
            }
        }
    }

    return OK;
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_update_mbaff_left_nnz                                    */
/*                                                                           */
/*  Description   : This function updates the left luma and chroma nnz for   */
/*                  mbaff cases.                                             */
/*                                                                           */
/*  Inputs        : <What inputs does the function take?>                    */
/*  Globals       : <Does it use any global variables?>                      */
/*  Processing    : <Describe how the function operates - include algorithm  */
/*                  description>                                             */
/*  Outputs       : <What does the function produce?>                        */
/*  Returns       : <What does the function return?>                         */
/*                                                                           */
/*  Issues        : <List any issues or problems with this function>         */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         13 07 2002   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/
void ih264d_update_mbaff_left_nnz(dec_struct_t * ps_dec,
                                  dec_mb_info_t * ps_cur_mb_info)
{
    UWORD32 *pu4_buf;
    UWORD8 *pu1_buf;
    if(ps_cur_mb_info->u1_topmb)
    {
        pu1_buf = ps_dec->pu1_left_nnz_y;
        pu4_buf = (UWORD32 *)pu1_buf;
        ps_dec->u4_n_left_temp_y = *pu4_buf;

        pu1_buf = ps_dec->pu1_left_nnz_uv;
        pu4_buf = (UWORD32 *)pu1_buf;
        ps_dec->u4_n_left_temp_uv = *pu4_buf;
    }
    else
    {

        ps_dec->u4_n_leftY[0] = ps_dec->u4_n_left_temp_y;
        pu1_buf = ps_dec->pu1_left_nnz_y;
        pu4_buf = (UWORD32 *)pu1_buf;
        ps_dec->u4_n_leftY[1] = *pu4_buf;
        ps_dec->u4_n_left_cr[0] = ps_dec->u4_n_left_temp_uv;
        pu1_buf = ps_dec->pu1_left_nnz_uv;
        pu4_buf = (UWORD32 *)pu1_buf;
        ps_dec->u4_n_left_cr[1] = *pu4_buf;

    }
}

/*!
 **************************************************************************
 * \if Function name : ih264d_get_mbaff_neighbours \endif
 *
 * \brief
 *    Gets the neighbors for the current MB if it is of type MB-AFF
 *  frame.
 *
 * \return
 *    None
 *
 **************************************************************************
 */
void ih264d_get_mbaff_neighbours(dec_struct_t * ps_dec,
                                 dec_mb_info_t * ps_cur_mb_info,
                                 UWORD8 uc_curMbFldDecFlag)
{

    mb_neigbour_params_t *ps_left_mb;
    mb_neigbour_params_t *ps_top_mb;
    mb_neigbour_params_t *ps_top_right_mb = NULL;
    mb_neigbour_params_t *ps_curmb;
    const UWORD8 u1_topmb = ps_cur_mb_info->u1_topmb;
    const UWORD8 uc_botMb = 1 - u1_topmb;
    const UWORD32 u4_mb_x = ps_cur_mb_info->u2_mbx;

    /* Current MbParams location in top row buffer */
    ps_curmb = ps_dec->ps_cur_mb_row + (u4_mb_x << 1) + uc_botMb;
    ps_left_mb = ps_curmb - 2;
    /* point to A if top else A+1 */
    if(uc_botMb && (ps_left_mb->u1_mb_fld != uc_curMbFldDecFlag))
    {
        /* move from A + 1 to A */
        ps_left_mb--;
    }
    ps_cur_mb_info->i1_offset = 0;
    if((uc_curMbFldDecFlag == 0) && uc_botMb)
    {
        mb_neigbour_params_t *ps_topleft_mb;
        /* CurMbAddr - 1 */
        ps_top_mb = ps_curmb - 1;

        /* Mark Top right Not available */
        /* point to A */
        ps_topleft_mb = ps_curmb - 3;

        if(ps_topleft_mb->u1_mb_fld)
        {
            /* point to A + 1 */
            ps_topleft_mb++;
        }
        ps_cur_mb_info->u1_topleft_mb_fld = ps_topleft_mb->u1_mb_fld;
        ps_cur_mb_info->u1_topleft_mbtype = ps_topleft_mb->u1_mb_type;
    }
    else
    {
        /* Top = B + 1 */
        ps_top_mb = ps_dec->ps_top_mb_row + (u4_mb_x << 1) + 1;
        ps_top_right_mb = ps_top_mb + 2;
        ps_cur_mb_info->i1_offset = 4;
        /* TopRight =  C + 1 */

        /* TopLeft = D+1 */
        ps_cur_mb_info->u1_topleft_mb_fld = ps_dec->u1_topleft_mb_fld_bot;
        ps_cur_mb_info->u1_topleft_mbtype = ps_dec->u1_topleft_mbtype_bot;

        if(uc_curMbFldDecFlag && u1_topmb)
        {
            if(ps_top_mb->u1_mb_fld)
            {
                /* MbAddrB */
                ps_top_mb--;
                ps_cur_mb_info->i1_offset = 0;
            }
            /* If topright is field then point to C */
            ps_top_right_mb -= ps_top_right_mb->u1_mb_fld ? 1 : 0;
            if(ps_cur_mb_info->u1_topleft_mb_fld)
            {
                /* TopLeft = D */
                ps_cur_mb_info->u1_topleft_mb_fld = ps_dec->u1_topleft_mb_fld;
                ps_cur_mb_info->u1_topleft_mbtype = ps_dec->u1_topleft_mbtype;
            }
        }
    }
    if(u1_topmb)
    {
        /* Update the parameters of topleftmb*/
        ps_dec->u1_topleft_mb_fld = ps_top_mb->u1_mb_fld;
        ps_dec->u1_topleft_mbtype = ps_top_mb->u1_mb_type;
        /* Set invscan and dequantMatrixScan*/
        if(uc_curMbFldDecFlag)
        {
            ps_dec->pu1_inv_scan = (UWORD8 *)gau1_ih264d_inv_scan_fld;
        }
        else
        {
            ps_dec->pu1_inv_scan = (UWORD8 *)gau1_ih264d_inv_scan;
        }
        ps_dec->pu2_quant_scale_y =
                        gau2_ih264_iquant_scale_4x4[ps_dec->u1_qp_y_rem6];
        ps_dec->pu2_quant_scale_u =
                        gau2_ih264_iquant_scale_4x4[ps_dec->u1_qp_u_rem6];
        ps_dec->pu2_quant_scale_v =
                        gau2_ih264_iquant_scale_4x4[ps_dec->u1_qp_v_rem6];

    }
    else
    {
        /* Update the parameters of topleftmb*/
        mb_neigbour_params_t *ps_top_mb_temp = ps_dec->ps_top_mb_row
                        + (u4_mb_x << 1) + 1;
        ps_dec->u1_topleft_mb_fld_bot = ps_top_mb_temp->u1_mb_fld;
        ps_dec->u1_topleft_mbtype_bot = ps_top_mb_temp->u1_mb_type;
    }

    ps_cur_mb_info->ps_left_mb = ps_left_mb;
    ps_cur_mb_info->ps_top_mb = ps_top_mb;
    ps_cur_mb_info->ps_top_right_mb = ps_top_right_mb;
    ps_cur_mb_info->ps_curmb = ps_curmb;
    ps_curmb->u1_mb_fld = uc_curMbFldDecFlag;

    {
        /* Form Left NNZ */
        UWORD8 u1_is_left_mb_fld = ps_left_mb->u1_mb_fld;
        UWORD8 *pu1_left_mb_pair_nnz_y = (UWORD8 *)&ps_dec->u4_n_leftY[0];
        UWORD8 *pu1_left_mb_pair_nnz_uv = (UWORD8 *)&ps_dec->u4_n_left_cr[0];
        UWORD8 *pu1_left_nnz_y = ps_dec->pu1_left_nnz_y;
        UWORD8 *pu1_left_nnz_uv = ps_dec->pu1_left_nnz_uv;

        if(uc_curMbFldDecFlag == u1_is_left_mb_fld)
        {
            *(UWORD32 *)pu1_left_nnz_y = *(UWORD32 *)(pu1_left_mb_pair_nnz_y
                            + (uc_botMb << 2));
            *(UWORD32 *)pu1_left_nnz_uv = *(UWORD32 *)(pu1_left_mb_pair_nnz_uv
                            + (uc_botMb << 2));
        }
        else if((uc_curMbFldDecFlag == 0) && u1_topmb && u1_is_left_mb_fld)
        {
            /* 0 0 1 1 of u4_n_leftY[0], 0 0 2 2 of u4_n_left_cr[0] */
            pu1_left_nnz_y[0] = pu1_left_nnz_y[1] = pu1_left_mb_pair_nnz_y[0];
            pu1_left_nnz_y[2] = pu1_left_nnz_y[3] = pu1_left_mb_pair_nnz_y[1];
            pu1_left_nnz_uv[0] = pu1_left_nnz_uv[1] =
                            pu1_left_mb_pair_nnz_uv[0];
            pu1_left_nnz_uv[2] = pu1_left_nnz_uv[3] =
                            pu1_left_mb_pair_nnz_uv[2];
        }
        else if((uc_curMbFldDecFlag == 0) && uc_botMb && u1_is_left_mb_fld)
        {
            /* 2 2 3 3 of u4_n_leftY[0] , 1 1 3 3 of u4_n_left_cr[0] */
            pu1_left_nnz_y[0] = pu1_left_nnz_y[1] = pu1_left_mb_pair_nnz_y[2];
            pu1_left_nnz_y[2] = pu1_left_nnz_y[3] = pu1_left_mb_pair_nnz_y[3];
            pu1_left_nnz_uv[0] = pu1_left_nnz_uv[1] =
                            pu1_left_mb_pair_nnz_uv[1];
            pu1_left_nnz_uv[2] = pu1_left_nnz_uv[3] =
                            pu1_left_mb_pair_nnz_uv[3];
        }
        else
        {
            /* 0 2 0 2 of u4_n_leftY[0], u4_n_leftY[1] */
            pu1_left_nnz_y[0] = pu1_left_mb_pair_nnz_y[0];
            pu1_left_nnz_y[1] = pu1_left_mb_pair_nnz_y[2];
            pu1_left_nnz_y[2] = pu1_left_mb_pair_nnz_y[4 + 0];
            pu1_left_nnz_y[3] = pu1_left_mb_pair_nnz_y[4 + 2];

            /* 0 of u4_n_left_cr[0] and 0 u4_n_left_cr[1]
             2 of u4_n_left_cr[0] and 2 u4_n_left_cr[1] */
            pu1_left_nnz_uv[0] = pu1_left_mb_pair_nnz_uv[0];
            pu1_left_nnz_uv[1] = pu1_left_mb_pair_nnz_uv[4 + 0];
            pu1_left_nnz_uv[2] = pu1_left_mb_pair_nnz_uv[2];
            pu1_left_nnz_uv[3] = pu1_left_mb_pair_nnz_uv[4 + 2];
        }
    }
}

/*
 **************************************************************************
 * \if Function name : ih264d_transfer_mb_group_data \endif
 *
 * \brief
 *     Transfer the Following things
 *     N-Mb DeblkParams Data    ( To Ext DeblkParams Buffer )
 *     N-Mb Recon Data          ( To Ext Frame Buffer )
 *     N-Mb Intrapredline Data  ( Updated Internally)
 *     N-Mb MV Data             ( To Ext MV Buffer )
 *     N-Mb MVTop/TopRight Data ( To Int MV Top Scratch Buffers)
 *
 * \return
 *    None
 *
 **************************************************************************
 */
void ih264d_transfer_mb_group_data(dec_struct_t * ps_dec,
                                   const UWORD8 u1_num_mbs,
                                   const UWORD8 u1_end_of_row, /* Cur n-Mb End of Row Flag */
                                   const UWORD8 u1_end_of_row_next /* Next n-Mb End of Row Flag */
                                   )
{
    dec_mb_info_t *ps_cur_mb_info = ps_dec->ps_nmb_info;
    tfr_ctxt_t *ps_trns_addr = &ps_dec->s_tran_addrecon;
    UWORD16 u2_mb_y;
    UWORD32 y_offset;
    UWORD32 u4_frame_stride;
    mb_neigbour_params_t *ps_temp;
    const UWORD8 u1_mbaff = ps_dec->ps_cur_slice->u1_mbaff_frame_flag;
    UNUSED(u1_end_of_row_next);

    ps_trns_addr->pu1_dest_y += ps_trns_addr->u4_inc_y[u1_end_of_row];
    ps_trns_addr->pu1_dest_u += ps_trns_addr->u4_inc_uv[u1_end_of_row];
    ps_trns_addr->pu1_dest_v += ps_trns_addr->u4_inc_uv[u1_end_of_row];

    /* Swap top and current pointers */
    if(u1_end_of_row)
    {

        if(ps_dec->u1_separate_parse)
        {
            u2_mb_y = ps_dec->i2_dec_thread_mb_y;
        }
        else
        {
            ps_temp = ps_dec->ps_cur_mb_row;
            ps_dec->ps_cur_mb_row = ps_dec->ps_top_mb_row;
            ps_dec->ps_top_mb_row = ps_temp;

            u2_mb_y = ps_dec->u2_mby + (1 + u1_mbaff);
        }

        u4_frame_stride = ps_dec->u2_frm_wd_y
                        << ps_dec->ps_cur_slice->u1_field_pic_flag;
        y_offset = (u2_mb_y * u4_frame_stride) << 4;
        ps_trns_addr->pu1_dest_y = ps_dec->s_cur_pic.pu1_buf1 + y_offset;

        u4_frame_stride = ps_dec->u2_frm_wd_uv
                        << ps_dec->ps_cur_slice->u1_field_pic_flag;
        y_offset = (u2_mb_y * u4_frame_stride) << 3;
        ps_trns_addr->pu1_dest_u = ps_dec->s_cur_pic.pu1_buf2 + y_offset;
        ps_trns_addr->pu1_dest_v = ps_dec->s_cur_pic.pu1_buf3 + y_offset;

        ps_trns_addr->pu1_mb_y = ps_trns_addr->pu1_dest_y;
        ps_trns_addr->pu1_mb_u = ps_trns_addr->pu1_dest_u;
        ps_trns_addr->pu1_mb_v = ps_trns_addr->pu1_dest_v;
    }

    /*
     * The Slice boundary is also a valid condition to transfer. So recalculate
     * the Left increment, in case the number of MBs is lesser than the
     * N MB value. u1_num_mbs will be equal to N of N MB if the entire N Mb is
     * decoded.
     */
    ps_dec->s_tran_addrecon.u2_mv_left_inc = ((u1_num_mbs >> u1_mbaff) - 1)
                    << (4 + u1_mbaff);
    ps_dec->s_tran_addrecon.u2_mv_top_left_inc = (u1_num_mbs << 2) - 1
                    - (u1_mbaff << 2);

    if(ps_dec->u1_separate_parse == 0)
    {
        /* reassign left MV and cur MV pointers */
        ps_dec->ps_mv_left = ps_dec->ps_mv_cur
                        + ps_dec->s_tran_addrecon.u2_mv_left_inc;

        ps_dec->ps_mv_cur += (u1_num_mbs << 4);
    }

    /* Increment deblock parameters pointer in external memory */

    if(ps_dec->u1_separate_parse == 0)
    {
        ps_dec->ps_deblk_mbn += u1_num_mbs;
    }

}

