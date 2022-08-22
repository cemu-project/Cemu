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
 * \file ih264d_thread_compute_bs.c
 *
 * \brief
 *    Contains routines that for multi-thread decoder
 *
 * Detailed_description
 *
 * \date
 *    20/02/2012
 *
 * \author  ZR
 **************************************************************************
 */
#include "ih264d_error_handler.h"
#include "ih264d_debug.h"
#include <string.h>
#include "ih264d_defs.h"
#include "ih264d_debug.h"
#include "ih264d_tables.h"
#include "ih264d_structs.h"
#include "ih264d_defs.h"
#include "ih264d_mb_utils.h"

#include "ih264d_thread_compute_bs.h"
#include "ithread.h"
#include "ih264d_deblocking.h"
#include "ih264d_process_pslice.h"
#include "ih264d_process_intra_mb.h"
#include "ih264d_mb_utils.h"
#include "ih264d_tables.h"
#include "ih264d_format_conv.h"
#include "ih264d_defs.h"
UWORD16 ih264d_update_csbp_8x8(UWORD16 u2_luma_csbp);
void ih264d_fill_bs2_horz_vert(UWORD32 *pu4_bs, /* Base pointer of BS table */
                               WORD32 u4_left_mb_csbp, /* csbp of left mb */
                               WORD32 u4_top_mb_csbp, /* csbp of top mb */
                               WORD32 u4_cur_mb_csbp, /* csbp of current mb */
                               const UWORD32 *pu4_packed_bs2, const UWORD16 *pu2_4x4_v2h_reorder);
void ih264d_copy_intra_pred_line(dec_struct_t *ps_dec,
                                 dec_mb_info_t *ps_cur_mb_info,
                                 UWORD32 nmb_index);

#define BS_MB_GROUP 4
#define DEBLK_MB_GROUP 1
#define FORMAT_CONV_MB_GROUP 4

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_compute_bs_non_mbaff_thread                                           */
/*                                                                           */
/*  Description   : This function computes the pointers of left,top & current*/
/*                : Nnz, MvPred & deblk_mb_t and supplies to FillBs function for*/
/*                : Boundary Strength Calculation .this function is used     */
/*                : BS being calculated in separate thread                   */
/*  Inputs        : pointer to decoder context,cur_mb_info,u4_mb_num            */
/*  Processing    :                                                          */
/*                                                                           */
/*  Outputs       : Produces the Boundary Strength for Current Mb            */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*                      ITTIAM                                               */
/*****************************************************************************/

void ih264d_compute_bs_non_mbaff_thread(dec_struct_t * ps_dec,
                                        dec_mb_info_t * ps_cur_mb_info,
                                        UWORD32 u4_mb_num)
{
    /* Mvpred and Nnz for top and Courrent */
    mv_pred_t *ps_cur_mv_pred, *ps_top_mv_pred = NULL, *ps_left_mv_pred;
    /* deblk_mb_t Params */
    deblk_mb_t *ps_cur_mb_params; /*< Parameters of current MacroBlock */
    deblkmb_neighbour_t *ps_deblk_top_mb;

    /* Reference Index to POC mapping*/
    void ** apv_map_ref_idx_to_poc;
    UWORD32 u4_leftmbtype;

    UWORD16 u2_left_csbp, u2_top_csbp, u2_cur_csbp;

    /* Set of flags */
    UWORD32 u4_cur_mb_intra, u1_top_mb_typ, u4_cur_mb_fld;
    UWORD32 u1_cur_mb_type;
    UWORD32 * pu4_bs_table;

    /* Neighbour availability */
    /* Initialization */
    const UWORD32 u2_mbx = ps_cur_mb_info->u2_mbx;
    const UWORD32 u2_mby = ps_cur_mb_info->u2_mby;
    const UWORD32 u1_pingpong = u2_mbx & 0x01;

    PROFILE_DISABLE_BOUNDARY_STRENGTH()
    ps_deblk_top_mb = ps_dec->ps_deblk_top_mb + u2_mbx;

    /* Pointer assignment for Current DeblkMB, Current Mv Pred  */
    ps_cur_mb_params = ps_dec->ps_deblk_pic + u4_mb_num;
    ps_cur_mv_pred = ps_dec->s_cur_pic.ps_mv + (u4_mb_num << 4);

    apv_map_ref_idx_to_poc =
                    (void **)ps_dec->ps_computebs_cur_slice->ppv_map_ref_idx_to_poc
                                    + 1;
    u1_cur_mb_type = ps_cur_mb_params->u1_mb_type;
    u1_top_mb_typ = ps_deblk_top_mb->u1_mb_type;
    ps_deblk_top_mb->u1_mb_type = u1_cur_mb_type;

    {
        ps_cur_mb_params->u1_topmb_qp = ps_deblk_top_mb->u1_mb_qp;
        ps_deblk_top_mb->u1_mb_qp = ps_cur_mb_params->u1_mb_qp;

        ps_cur_mb_params->u1_left_mb_qp = ps_dec->deblk_left_mb[1].u1_mb_qp;
        ps_dec->deblk_left_mb[1].u1_mb_qp = ps_cur_mb_params->u1_mb_qp;

    }

    /* if no deblocking required for current Mb then continue */
    /* Check next Mbs   in Mb group                           */
    if(ps_cur_mb_params->u1_deblocking_mode & MB_DISABLE_FILTERING)
    {
        void ** pu4_map_ref_idx_to_poc_l1 = apv_map_ref_idx_to_poc +
        POC_LIST_L0_TO_L1_DIFF;
        {
            /* Store Parameter for Top MvPred refernce frame Address */

            void ** ppv_top_mv_pred_addr = ps_cur_mb_info->ps_curmb->u4_pic_addrress;
            WORD8 * p1_refTop0 = (ps_cur_mv_pred + 12)->i1_ref_frame;
            WORD8 * p1_refTop1 = (ps_cur_mv_pred + 14)->i1_ref_frame;

            /* Store Left addresses for Next Mb   */
            void ** ppv_left_mv_pred_addr =
                            ps_dec->ps_left_mvpred_addr[!u1_pingpong][1].u4_add;
            WORD8 * p1_refleft0 = (ps_cur_mv_pred + 3)->i1_ref_frame;


            ppv_top_mv_pred_addr[0] = apv_map_ref_idx_to_poc[p1_refTop0[0]];
            ppv_top_mv_pred_addr[1] = pu4_map_ref_idx_to_poc_l1[p1_refTop0[1]];

            ppv_left_mv_pred_addr[2] = apv_map_ref_idx_to_poc[p1_refTop1[0]];
            ppv_top_mv_pred_addr[2] = apv_map_ref_idx_to_poc[p1_refTop1[0]];
            ppv_left_mv_pred_addr[3] = pu4_map_ref_idx_to_poc_l1[p1_refTop1[1]];
            ppv_top_mv_pred_addr[3] = pu4_map_ref_idx_to_poc_l1[p1_refTop1[1]];

            ppv_left_mv_pred_addr[0] = apv_map_ref_idx_to_poc[p1_refleft0[0]];
            ppv_left_mv_pred_addr[1] = pu4_map_ref_idx_to_poc_l1[p1_refleft0[1]];
            //}
            /* Storing the leftMbtype for next Mb */
            ps_dec->deblk_left_mb[1].u1_mb_type = ps_cur_mb_params->u1_mb_type;
        }

        return;
    }

    /* Flag for extra left Edge */
    ps_cur_mb_params->u1_single_call = 1;

    /* Update the Left deblk_mb_t and Left MvPred Parameters           */
    if(!u2_mbx)
    {
        u4_leftmbtype = 0;

        /* Initialize the ps_left_mv_pred with Junk but Valid Location */
        /* to avoid invalid memory access                           */
        /* this is read only pointer                                */
        ps_left_mv_pred = ps_cur_mv_pred + 3;
    }
    else
    {
        u4_leftmbtype = ps_dec->deblk_left_mb[1].u1_mb_type;

        /* Come to Left Most Edge of the MB */
        ps_left_mv_pred = ps_cur_mv_pred - (1 << 4) + 3;
    }

    if(!u2_mby)
        u1_top_mb_typ = 0;

    /* MvPred Pointer Calculation */
    /* CHANGED CODE */
    ps_top_mv_pred = ps_cur_mv_pred - (ps_dec->u2_frm_wd_in_mbs << 4) + 12;

    u4_cur_mb_intra = u1_cur_mb_type & D_INTRA_MB;
    u4_cur_mb_fld = !!(u1_cur_mb_type & D_FLD_MB);
    /* Compute BS function */
    pu4_bs_table = ps_cur_mb_params->u4_bs_table;

    u2_cur_csbp = ps_cur_mb_info->ps_curmb->u2_luma_csbp;
    u2_left_csbp = ps_cur_mb_info->ps_left_mb->u2_luma_csbp;
    u2_top_csbp = ps_cur_mb_info->ps_top_mb->u2_luma_csbp;

    /* Compute BS function */
    if(ps_dec->ps_cur_sps->u1_profile_idc == HIGH_PROFILE_IDC)
    {
        if(ps_cur_mb_info->u1_tran_form8x8 == 1)
        {
            u2_cur_csbp = ih264d_update_csbp_8x8(
                            ps_cur_mb_info->ps_curmb->u2_luma_csbp);
        }

        if(ps_cur_mb_info->ps_left_mb->u1_tran_form8x8 == 1)
        {
            u2_left_csbp = ih264d_update_csbp_8x8(
                            ps_cur_mb_info->ps_left_mb->u2_luma_csbp);
        }

        if(ps_cur_mb_info->ps_top_mb->u1_tran_form8x8 == 1)
        {
            u2_top_csbp = ih264d_update_csbp_8x8(
                            ps_cur_mb_info->ps_top_mb->u2_luma_csbp);
        }
    }
    if(u4_cur_mb_intra)
    {

        pu4_bs_table[4] = 0x04040404;
        pu4_bs_table[0] = u4_cur_mb_fld ? 0x03030303 : 0x04040404;
        pu4_bs_table[1] = 0x03030303;
        pu4_bs_table[2] = 0x03030303;
        pu4_bs_table[3] = 0x03030303;
        pu4_bs_table[5] = 0x03030303;
        pu4_bs_table[6] = 0x03030303;
        pu4_bs_table[7] = 0x03030303;
    }
    else
    {
        UWORD32 u4_is_non16x16 = !!(u1_cur_mb_type & D_PRED_NON_16x16);
        UWORD32 u4_is_b =
                        (ps_dec->ps_computebs_cur_slice->slice_type == B_SLICE);






        ih264d_fill_bs2_horz_vert(pu4_bs_table, u2_left_csbp, u2_top_csbp,
                                  u2_cur_csbp, gau4_ih264d_packed_bs2,
                                  gau2_ih264d_4x4_v2h_reorder);

        if(u4_leftmbtype & D_INTRA_MB)
            pu4_bs_table[4] = 0x04040404;

        if(u1_top_mb_typ & D_INTRA_MB)
            pu4_bs_table[0] = u4_cur_mb_fld ? 0x03030303 : 0x04040404;

        ps_dec->pf_fill_bs1[u4_is_b][u4_is_non16x16](
                        ps_cur_mv_pred, ps_top_mv_pred, apv_map_ref_idx_to_poc,
                        pu4_bs_table, ps_left_mv_pred,
                        &(ps_dec->ps_left_mvpred_addr[u1_pingpong][1]),
                        ps_cur_mb_info->ps_top_mb->u4_pic_addrress,
                        (4 >> u4_cur_mb_fld));
    }

    {
        void ** pu4_map_ref_idx_to_poc_l1 = apv_map_ref_idx_to_poc +
        POC_LIST_L0_TO_L1_DIFF;
        {
            /* Store Parameter for Top MvPred refernce frame Address */

            void ** ppv_top_mv_pred_addr = ps_cur_mb_info->ps_curmb->u4_pic_addrress;
            WORD8 * p1_refTop0 = (ps_cur_mv_pred + 12)->i1_ref_frame;
            WORD8 * p1_refTop1 = (ps_cur_mv_pred + 14)->i1_ref_frame;

            /* Store Left addresses for Next Mb   */
            void ** ppv_left_mv_pred_addr =
                            ps_dec->ps_left_mvpred_addr[!u1_pingpong][1].u4_add;
            WORD8 * p1_refleft0 = (ps_cur_mv_pred + 3)->i1_ref_frame;

            ppv_top_mv_pred_addr[0] = apv_map_ref_idx_to_poc[p1_refTop0[0]];
            ppv_top_mv_pred_addr[1] = pu4_map_ref_idx_to_poc_l1[p1_refTop0[1]];

            ppv_left_mv_pred_addr[2] = apv_map_ref_idx_to_poc[p1_refTop1[0]];
            ppv_top_mv_pred_addr[2] = apv_map_ref_idx_to_poc[p1_refTop1[0]];
            ppv_left_mv_pred_addr[3] = pu4_map_ref_idx_to_poc_l1[p1_refTop1[1]];
            ppv_top_mv_pred_addr[3] = pu4_map_ref_idx_to_poc_l1[p1_refTop1[1]];

            ppv_left_mv_pred_addr[0] = apv_map_ref_idx_to_poc[p1_refleft0[0]];
            ppv_left_mv_pred_addr[1] = pu4_map_ref_idx_to_poc_l1[p1_refleft0[1]];

            /* Storing the leftMbtype for next Mb */
            ps_dec->deblk_left_mb[1].u1_mb_type = ps_cur_mb_params->u1_mb_type;

        }
    }

    /* For transform 8x8 disable deblocking of the intrernal edges of a 8x8 block */
    if(ps_cur_mb_info->u1_tran_form8x8)
    {
        pu4_bs_table[1] = 0;
        pu4_bs_table[3] = 0;
        pu4_bs_table[5] = 0;
        pu4_bs_table[7] = 0;
    }
}


void ih264d_check_mb_map_deblk(dec_struct_t *ps_dec,
                                    UWORD32 deblk_mb_grp,
                                    tfr_ctxt_t *ps_tfr_cxt,
                                    UWORD32 u4_check_mb_map)
{
    UWORD32 i = 0;
    UWORD32 u4_mb_num;
    UWORD32 u4_cond;
    volatile UWORD8 *mb_map = ps_dec->pu1_recon_mb_map;
    const WORD32 i4_cb_qp_idx_ofst =
                    ps_dec->ps_cur_pps->i1_chroma_qp_index_offset;
    const WORD32 i4_cr_qp_idx_ofst =
                    ps_dec->ps_cur_pps->i1_second_chroma_qp_index_offset;

    UWORD32 u4_wd_y, u4_wd_uv;
    UWORD8 u1_field_pic_flag = ps_dec->ps_cur_slice->u1_field_pic_flag;


    u4_wd_y = ps_dec->u2_frm_wd_y << u1_field_pic_flag;
    u4_wd_uv = ps_dec->u2_frm_wd_uv << u1_field_pic_flag;


    for(i = 0; i < deblk_mb_grp; i++)
    {
        WORD32 nop_cnt = 8*128;
        while(u4_check_mb_map == 1)
        {
            u4_mb_num = ps_dec->u4_cur_deblk_mb_num;
            /*we wait for the right mb because of intra pred data dependency*/
            u4_mb_num = MIN(u4_mb_num + 1, (ps_dec->u4_deblk_mb_y + 1) * ps_dec->u2_frm_wd_in_mbs - 1);
            CHECK_MB_MAP_BYTE(u4_mb_num, mb_map, u4_cond);

            if(u4_cond)
            {
                break;
            }
            else
            {
                if(nop_cnt > 0)
                {
                    nop_cnt -= 128;
                    NOP(128);
                }
                else
                {
                    nop_cnt = 8*128;
                    ithread_yield();
                }
            }
        }

        ih264d_deblock_mb_nonmbaff(ps_dec, ps_tfr_cxt,
                                   i4_cb_qp_idx_ofst, i4_cr_qp_idx_ofst,
                                    u4_wd_y, u4_wd_uv);


    }


}
void ih264d_recon_deblk_slice(dec_struct_t *ps_dec, tfr_ctxt_t *ps_tfr_cxt)
{
    dec_mb_info_t *p_cur_mb;
    UWORD32 u4_max_addr;
    WORD32 i;
    UWORD32 u1_mb_aff;
    UWORD16 u2_slice_num;
    UWORD32 u4_mb_num;
    UWORD16 u2_first_mb_in_slice;
    UWORD32 i2_pic_wdin_mbs;
    UWORD8 u1_num_mbsleft, u1_end_of_row;
    UWORD8 u1_mbaff;
    UWORD16 i16_mb_x, i16_mb_y;
    WORD32 j;
    dec_mb_info_t * ps_cur_mb_info;
    UWORD32 u1_slice_type, u1_B;
    WORD32 u1_skip_th;
    UWORD32 u1_ipcm_th;
    WORD32 ret;
    tfr_ctxt_t *ps_trns_addr;
    UWORD32 u4_frame_stride;
    UWORD32 x_offset, y_offset;
    UWORD32 u4_slice_end;
    pad_mgr_t *ps_pad_mgr ;

    /*check for mb map of first mb in slice to ensure slice header is parsed*/
    while(1)
    {
        UWORD32 u4_mb_num = ps_dec->cur_recon_mb_num;
        UWORD32 u4_cond = 0;
        WORD32 nop_cnt = 8*128;

        CHECK_MB_MAP_BYTE(u4_mb_num, ps_dec->pu1_recon_mb_map, u4_cond);
        if(u4_cond)
        {
            break;
        }
        else
        {
            if(nop_cnt > 0)
            {
                nop_cnt -= 128;
                NOP(128);
            }
            else
            {
                if(ps_dec->u4_output_present &&
                   (ps_dec->u4_fmt_conv_cur_row < ps_dec->s_disp_frame_info.u4_y_ht))
                {
                    ps_dec->u4_fmt_conv_num_rows =
                                    MIN(FMT_CONV_NUM_ROWS,
                                        (ps_dec->s_disp_frame_info.u4_y_ht
                                                        - ps_dec->u4_fmt_conv_cur_row));
                    ih264d_format_convert(ps_dec, &(ps_dec->s_disp_op),
                                          ps_dec->u4_fmt_conv_cur_row,
                                          ps_dec->u4_fmt_conv_num_rows);
                    ps_dec->u4_fmt_conv_cur_row += ps_dec->u4_fmt_conv_num_rows;
                }
                else
                {
                    nop_cnt = 8*128;
                    ithread_yield();
                }
            }
            DEBUG_THREADS_PRINTF("waiting for mb mapcur_dec_mb_num = %d,ps_dec->u2_cur_mb_addr  = %d\n",u2_cur_dec_mb_num,
                            ps_dec->u2_cur_mb_addr);

        }
    }

    u4_max_addr = ps_dec->ps_cur_sps->u2_max_mb_addr;
    u1_mb_aff = ps_dec->ps_cur_slice->u1_mbaff_frame_flag;
    u2_first_mb_in_slice = ps_dec->ps_computebs_cur_slice->u4_first_mb_in_slice;
    i2_pic_wdin_mbs = ps_dec->u2_frm_wd_in_mbs;
    u1_mbaff = ps_dec->ps_cur_slice->u1_mbaff_frame_flag;
    ps_pad_mgr = &ps_dec->s_pad_mgr;

    if(u2_first_mb_in_slice == 0)
    ih264d_init_deblk_tfr_ctxt(ps_dec, ps_pad_mgr, ps_tfr_cxt,
                               ps_dec->u2_frm_wd_in_mbs, 0);


    i16_mb_x = MOD(u2_first_mb_in_slice, i2_pic_wdin_mbs);
    i16_mb_y = DIV(u2_first_mb_in_slice, i2_pic_wdin_mbs);
    i16_mb_y <<= u1_mbaff;
    ps_dec->i2_recon_thread_mb_y = i16_mb_y;
    u4_frame_stride = ps_dec->u2_frm_wd_y
                    << ps_dec->ps_cur_slice->u1_field_pic_flag;

    x_offset = i16_mb_x << 4;
    y_offset = (i16_mb_y * u4_frame_stride) << 4;
    ps_trns_addr = &ps_dec->s_tran_iprecon;

    ps_trns_addr->pu1_dest_y = ps_dec->s_cur_pic.pu1_buf1 + x_offset + y_offset;

    u4_frame_stride = ps_dec->u2_frm_wd_uv
                    << ps_dec->ps_cur_slice->u1_field_pic_flag;
    x_offset >>= 1;
    y_offset = (i16_mb_y * u4_frame_stride) << 3;

    x_offset *= YUV420SP_FACTOR;

    ps_trns_addr->pu1_dest_u = ps_dec->s_cur_pic.pu1_buf2 + x_offset + y_offset;
    ps_trns_addr->pu1_dest_v = ps_dec->s_cur_pic.pu1_buf3 + x_offset + y_offset;

    ps_trns_addr->pu1_mb_y = ps_trns_addr->pu1_dest_y;
    ps_trns_addr->pu1_mb_u = ps_trns_addr->pu1_dest_u;
    ps_trns_addr->pu1_mb_v = ps_trns_addr->pu1_dest_v;

    ps_dec->cur_recon_mb_num = u2_first_mb_in_slice << u1_mbaff;

    u4_slice_end = 0;
    ps_dec->u4_bs_cur_slice_num_mbs = 0;
    ps_dec->u4_cur_bs_mb_num =
                    (ps_dec->ps_computebs_cur_slice->u4_first_mb_in_slice)
                                    << u1_mb_aff;

    if(ps_dec->i1_recon_in_thread3_flag)
    {
        ps_dec->pv_proc_tu_coeff_data =
                (void *) ps_dec->ps_computebs_cur_slice->pv_tu_coeff_data_start;
    }

    u1_slice_type = ps_dec->ps_computebs_cur_slice->slice_type;

    u1_B = (u1_slice_type == B_SLICE);

    u1_skip_th = ((u1_slice_type != I_SLICE) ?
                                    (u1_B ? B_8x8 : PRED_8x8R0) : -1);

    u1_ipcm_th = ((u1_slice_type != I_SLICE) ? (u1_B ? 23 : 5) : 0);



    while(u4_slice_end != 1)
    {
        WORD32 recon_mb_grp,bs_mb_grp;
        WORD32 nop_cnt = 8*128;
        u1_num_mbsleft = ((i2_pic_wdin_mbs - i16_mb_x) << u1_mbaff);
        if(u1_num_mbsleft <= ps_dec->u1_recon_mb_grp)
        {
            recon_mb_grp = u1_num_mbsleft;
            u1_end_of_row = 1;
            i16_mb_x = 0;
        }
        else
        {
            recon_mb_grp = ps_dec->u1_recon_mb_grp;
            u1_end_of_row = 0;
            i16_mb_x += (recon_mb_grp >> u1_mbaff);
        }


        while(1)
        {
            UWORD32 u4_cond = 0;
            UWORD32 u4_mb_num = ps_dec->cur_recon_mb_num + recon_mb_grp - 1;

            /*
             * Wait for one extra mb of MC, because some chroma IQ-IT functions
             * sometimes loads the pixels of the right mb and stores with the loaded
             * values.
             */
            u4_mb_num = MIN(u4_mb_num + 1, (ps_dec->i2_recon_thread_mb_y + 1) * i2_pic_wdin_mbs - 1);

            CHECK_MB_MAP_BYTE(u4_mb_num, ps_dec->pu1_recon_mb_map, u4_cond);
            if(u4_cond)
            {
                break;
            }
            else
            {
                if(nop_cnt > 0)
                {
                    nop_cnt -= 128;
                    NOP(128);
                }
                else
                {
                    if(ps_dec->u4_output_present &&
                       (ps_dec->u4_fmt_conv_cur_row < ps_dec->s_disp_frame_info.u4_y_ht))
                    {
                        ps_dec->u4_fmt_conv_num_rows =
                                        MIN(FMT_CONV_NUM_ROWS,
                                            (ps_dec->s_disp_frame_info.u4_y_ht
                                                            - ps_dec->u4_fmt_conv_cur_row));
                        ih264d_format_convert(ps_dec, &(ps_dec->s_disp_op),
                                              ps_dec->u4_fmt_conv_cur_row,
                                              ps_dec->u4_fmt_conv_num_rows);
                        ps_dec->u4_fmt_conv_cur_row += ps_dec->u4_fmt_conv_num_rows;
                    }
                    else
                    {
                        nop_cnt = 8*128;
                        ithread_yield();
                    }
                }
            }
        }

        for(j = 0; j < recon_mb_grp; j++)
        {
            GET_SLICE_NUM_MAP(ps_dec->pu2_slice_num_map, ps_dec->cur_recon_mb_num,
                              u2_slice_num);

            if(u2_slice_num != ps_dec->u2_cur_slice_num_bs)
            {
                u4_slice_end = 1;
                break;
            }
            if(ps_dec->i1_recon_in_thread3_flag)
            {
                ps_cur_mb_info = &ps_dec->ps_frm_mb_info[ps_dec->cur_recon_mb_num];

                if(ps_cur_mb_info->u1_mb_type <= u1_skip_th)
                {
                    ih264d_process_inter_mb(ps_dec, ps_cur_mb_info, j);
                }
                else if(ps_cur_mb_info->u1_mb_type != MB_SKIP)
                {
                    if((u1_ipcm_th + 25) != ps_cur_mb_info->u1_mb_type)
                    {
                        ps_cur_mb_info->u1_mb_type -= (u1_skip_th + 1);
                        ih264d_process_intra_mb(ps_dec, ps_cur_mb_info, j);
                    }
                }

                ih264d_copy_intra_pred_line(ps_dec, ps_cur_mb_info, j);
            }
            ps_dec->cur_recon_mb_num++;
        }

        if(j != recon_mb_grp)
        {
            u1_end_of_row = 0;
        }

        {
            tfr_ctxt_t *ps_trns_addr = &ps_dec->s_tran_iprecon;
            UWORD16 u2_mb_y;

            ps_trns_addr->pu1_dest_y += ps_trns_addr->u4_inc_y[u1_end_of_row];
            ps_trns_addr->pu1_dest_u += ps_trns_addr->u4_inc_uv[u1_end_of_row];
            ps_trns_addr->pu1_dest_v += ps_trns_addr->u4_inc_uv[u1_end_of_row];

            if(u1_end_of_row)
            {
                ps_dec->i2_recon_thread_mb_y += (1 << u1_mbaff);
                u2_mb_y = ps_dec->i2_recon_thread_mb_y;
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
        }

        bs_mb_grp = j;
        /* Compute BS for NMB group*/
        for(i = 0; i < bs_mb_grp; i++)
        {
            p_cur_mb = &ps_dec->ps_frm_mb_info[ps_dec->u4_cur_bs_mb_num];

            DEBUG_THREADS_PRINTF("ps_dec->u4_cur_bs_mb_num = %d\n",ps_dec->u4_cur_bs_mb_num);
            ih264d_compute_bs_non_mbaff_thread(ps_dec, p_cur_mb,
                                               ps_dec->u4_cur_bs_mb_num);
            ps_dec->u4_cur_bs_mb_num++;
            ps_dec->u4_bs_cur_slice_num_mbs++;

        }

        if(ps_dec->u4_cur_bs_mb_num > u4_max_addr)
        {
            u4_slice_end = 1;
        }

        /*deblock MB group*/
        {
            UWORD32 u4_num_mbs;

            if(ps_dec->u4_cur_bs_mb_num > ps_dec->u4_cur_deblk_mb_num)
            {
                if(u1_end_of_row)
                {
                    u4_num_mbs = ps_dec->u4_cur_bs_mb_num
                                    - ps_dec->u4_cur_deblk_mb_num;
                }
                else
                {
                    u4_num_mbs = ps_dec->u4_cur_bs_mb_num
                                    - ps_dec->u4_cur_deblk_mb_num - 1;
                }
            }
            else
                u4_num_mbs = 0;

            ih264d_check_mb_map_deblk(ps_dec, u4_num_mbs, ps_tfr_cxt,0);
        }

    }
}

void ih264d_recon_deblk_thread(dec_struct_t *ps_dec)
{
    tfr_ctxt_t s_tfr_ctxt;
    tfr_ctxt_t *ps_tfr_cxt = &s_tfr_ctxt; // = &ps_dec->s_tran_addrecon;


    UWORD32 yield_cnt = 0;

    ithread_set_name("ih264d_recon_deblk_thread");

    while(1)
    {

        DEBUG_THREADS_PRINTF(" Entering compute bs slice\n");
        ih264d_recon_deblk_slice(ps_dec, ps_tfr_cxt);

        DEBUG_THREADS_PRINTF(" Exit  compute bs slice \n");

        if(ps_dec->cur_recon_mb_num > ps_dec->ps_cur_sps->u2_max_mb_addr)
        {
                break;
        }
        else
        {
            ps_dec->ps_computebs_cur_slice++;
            ps_dec->u2_cur_slice_num_bs++;
        }
        DEBUG_THREADS_PRINTF("CBS thread:Got next slice/end of frame signal \n ");

    }

    if(ps_dec->u4_output_present &&
       (3 == ps_dec->u4_num_cores) &&
       (ps_dec->u4_fmt_conv_cur_row < ps_dec->s_disp_frame_info.u4_y_ht))
    {
        ps_dec->u4_fmt_conv_num_rows =
                        (ps_dec->s_disp_frame_info.u4_y_ht
                                        - ps_dec->u4_fmt_conv_cur_row);
        ih264d_format_convert(ps_dec, &(ps_dec->s_disp_op),
                              ps_dec->u4_fmt_conv_cur_row,
                              ps_dec->u4_fmt_conv_num_rows);
        ps_dec->u4_fmt_conv_cur_row += ps_dec->u4_fmt_conv_num_rows;

    }



}


