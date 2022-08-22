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
 * \file ih264d_process_pslice.c
 *
 * \brief
 *    Contains routines that decode a I slice type
 *
 * Detailed_description
 *
 * \date
 *    21/12/2002
 *
 * \author  NS
 **************************************************************************
 */
#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"

#include <string.h>
#include "ih264d_bitstrm.h"
#include "ih264d_defs.h"
#include "ih264d_debug.h"
#include "ih264d_structs.h"
#include "ih264d_defs.h"
#include "ih264d_parse_cavlc.h"
#include "ih264d_mb_utils.h"
#include "ih264d_deblocking.h"
#include "ih264d_dpb_manager.h"
#include "ih264d_mvpred.h"
#include "ih264d_inter_pred.h"
#include "ih264d_process_pslice.h"
#include "ih264d_error_handler.h"
#include "ih264d_cabac.h"
#include "ih264d_debug.h"
#include "ih264d_tables.h"
#include "ih264d_parse_slice.h"
#include "ih264d_utils.h"
#include "ih264d_parse_islice.h"
#include "ih264d_process_bslice.h"
#include "ih264d_process_intra_mb.h"

void ih264d_init_cabac_contexts(UWORD8 u1_slice_type, dec_struct_t * ps_dec);

void ih264d_insert_pic_in_ref_pic_listx(struct pic_buffer_t *ps_ref_pic_buf_lx,
                                        struct pic_buffer_t *ps_pic)
{
    *ps_ref_pic_buf_lx = *ps_pic;
}

WORD32 ih264d_mv_pred_ref_tfr_nby2_pmb(dec_struct_t * ps_dec,
                                     UWORD8 u1_mb_idx,
                                     UWORD8 u1_num_mbs)
{
    parse_pmbarams_t * ps_mb_part_info;
    parse_part_params_t * ps_part;
    mv_pred_t *ps_mv_nmb, *ps_mv_nmb_start, *ps_mv_ntop, *ps_mv_ntop_start;
    UWORD32 i, j;
    const UWORD32 u1_mbaff = ps_dec->ps_cur_slice->u1_mbaff_frame_flag;
    dec_mb_info_t * ps_cur_mb_info;
    WORD32 i2_mv_x, i2_mv_y;
    WORD32 ret;

    ps_dec->i4_submb_ofst -= (u1_num_mbs - u1_mb_idx) << 4;
    ps_mb_part_info = ps_dec->ps_parse_mb_data; // + u1_mb_idx;
    ps_part = ps_dec->ps_parse_part_params; // + u1_mb_idx;

    /* N/2 Mb MvPred and Transfer Setup Loop */
    for(i = u1_mb_idx; i < u1_num_mbs; i++, ps_mb_part_info++)
    {
        UWORD32 u1_colz;
        UWORD32 u1_field;
        mv_pred_t s_mvPred;
        mv_pred_t *ps_mv_pred = &s_mvPred;



        *ps_mv_pred = ps_dec->s_default_mv_pred;

        ps_dec->i4_submb_ofst += SUB_BLK_SIZE;

        /* Restore the slice scratch MbX and MbY context */
        ps_cur_mb_info = ps_dec->ps_nmb_info + i;
        u1_field = ps_cur_mb_info->u1_mb_field_decodingflag;



        ps_mv_nmb_start = ps_dec->ps_mv_cur + (i << 4);
        ps_dec->u2_mbx = ps_cur_mb_info->u2_mbx;
        ps_dec->u2_mby = ps_cur_mb_info->u2_mby;
        ps_dec->u2_mv_2mb[i & 0x1] = 0;

        /* Look for MV Prediction and Reference Transfer in Non-I Mbs */
        if(!ps_mb_part_info->u1_isI_mb)
        {
            UWORD32 u1_blk_no;
            WORD32 i1_ref_idx, i1_ref_idx1;
            UWORD32 u1_sub_mb_x, u1_sub_mb_y, u1_sub_mb_num;
            UWORD32 u1_num_part, u1_num_ref, u1_wd, u1_ht;
            UWORD32 *pu4_wt_offst, **ppu4_wt_ofst;
            UWORD32 u1_scale_ref, u4_bot_mb;
            WORD8 *pi1_ref_idx = ps_mb_part_info->i1_ref_idx[0];
            pic_buffer_t *ps_ref_frame, **pps_ref_frame;
            deblk_mb_t * ps_cur_deblk_mb = ps_dec->ps_deblk_mbn + i;

            /* MB Level initialisations */
            ps_dec->u4_num_pmbair = i >> u1_mbaff;
            ps_dec->u1_mb_idx_mv = i;
            ppu4_wt_ofst = ps_mb_part_info->pu4_wt_offst;
            pps_ref_frame = ps_dec->ps_ref_pic_buf_lx[0];
            /* CHANGED CODE */
            ps_mv_ntop_start = ps_mv_nmb_start
                            - (ps_dec->u2_frm_wd_in_mbs << (4 + u1_mbaff)) + 12;

            u1_num_part = ps_mb_part_info->u1_num_part;
            ps_cur_deblk_mb->u1_mb_type |= (u1_num_part > 1) << 1;
            ps_cur_mb_info->u4_pred_info_pkd_idx = ps_dec->u4_pred_info_pkd_idx;
            ps_cur_mb_info->u1_num_pred_parts = 0;


            /****************************************************/
            /* weighted u4_ofst pointer calculations, this loop  */
            /* runs maximum 4 times, even in direct cases       */
            /****************************************************/
            u1_scale_ref = u1_mbaff & u1_field;

            u4_bot_mb = 1 - ps_cur_mb_info->u1_topmb;
            if(ps_dec->ps_cur_pps->u1_wted_pred_flag)
            {
                u1_num_ref = MIN(u1_num_part, 4);
                for(u1_blk_no = 0; u1_blk_no < u1_num_ref; u1_blk_no++)
                {
                    i1_ref_idx = pi1_ref_idx[u1_blk_no];
                    if(u1_scale_ref)
                        i1_ref_idx >>= 1;
                    pu4_wt_offst = (UWORD32*)&ps_dec->pu4_wt_ofsts[2
                                    * X3(i1_ref_idx)];
                    ppu4_wt_ofst[u1_blk_no] = pu4_wt_offst;
                }
            }
            else
            {
                ppu4_wt_ofst[0] = NULL;
                ppu4_wt_ofst[1] = NULL;
                ppu4_wt_ofst[2] = NULL;
                ppu4_wt_ofst[3] = NULL;
            }

            /**************************************************/
            /* Loop on Partitions                             */
            /**************************************************/
            for(j = 0; j < u1_num_part; j++, ps_part++)
            {

                u1_sub_mb_num = ps_part->u1_sub_mb_num;
                ps_dec->u1_sub_mb_num = u1_sub_mb_num;

                if(PART_NOT_DIRECT != ps_part->u1_is_direct)
                {
                    /* Mb Skip Mode */
                    /* Setting the default and other members of MvPred Structure */
                    s_mvPred.i2_mv[2] = -1;
                    s_mvPred.i2_mv[3] = -1;
                    s_mvPred.i1_ref_frame[0] = 0;
                    i1_ref_idx = (u1_scale_ref && u4_bot_mb) ? MAX_REF_BUFS : 0;
                    ps_ref_frame = pps_ref_frame[i1_ref_idx];
                    s_mvPred.u1_col_ref_pic_idx = ps_ref_frame->u1_mv_buf_id;
                    s_mvPred.u1_pic_type = ps_ref_frame->u1_pic_type;
                    pu4_wt_offst = (UWORD32*)&ps_dec->pu4_wt_ofsts[0];

                    ps_dec->pf_mvpred(ps_dec, ps_cur_mb_info, ps_mv_nmb_start,
                                      ps_mv_ntop_start, &s_mvPred, 0, 4, 0, 1,
                                      MB_SKIP);






                    {
                        pred_info_pkd_t *ps_pred_pkd;
                        ps_pred_pkd = ps_dec->ps_pred_pkd + ps_dec->u4_pred_info_pkd_idx;
                    ih264d_fill_pred_info (s_mvPred.i2_mv,4,4,0,PRED_L0,ps_pred_pkd,ps_ref_frame->u1_pic_buf_id,
                                           (i1_ref_idx >> u1_scale_ref),pu4_wt_offst,
                                           ps_ref_frame->u1_pic_type);


                    ps_dec->u4_pred_info_pkd_idx++;
                    ps_cur_mb_info->u1_num_pred_parts++;
                    }



                    /* Storing colocated zero information */
                    u1_colz = ((ABS(s_mvPred.i2_mv[0]) <= 1)
                                    && (ABS(s_mvPred.i2_mv[1]) <= 1))
                                    + (u1_field << 1);

                    ih264d_rep_mv_colz(ps_dec, &s_mvPred, ps_mv_nmb_start, 0,
                                       u1_colz, 4, 4);
                }
                else
                {
                    u1_sub_mb_x = u1_sub_mb_num & 0x03;
                    u1_sub_mb_y = u1_sub_mb_num >> 2;
                    u1_blk_no =
                                    (u1_num_part < 4) ?
                                                    j :
                                                    (((u1_sub_mb_y >> 1) << 1)
                                                                    + (u1_sub_mb_x
                                                                                    >> 1));

                    ps_mv_ntop = ps_mv_ntop_start + u1_sub_mb_x;
                    ps_mv_nmb = ps_mv_nmb_start + u1_sub_mb_num;

                    u1_wd = ps_part->u1_partwidth;
                    u1_ht = ps_part->u1_partheight;

                    /* Populate the colpic info and reference frames */
                    i1_ref_idx = pi1_ref_idx[u1_blk_no];
                    s_mvPred.i1_ref_frame[0] = i1_ref_idx;

                    /********************************************************/
                    /* Predict Mv                                           */
                    /* Add Mv Residuals and store back                      */
                    /********************************************************/
                    ps_dec->pf_mvpred(ps_dec, ps_cur_mb_info, ps_mv_nmb, ps_mv_ntop,
                                      &s_mvPred, u1_sub_mb_num, u1_wd, 0, 1,
                                      ps_cur_mb_info->u1_mb_mc_mode);
                    i2_mv_x = ps_mv_nmb->i2_mv[0];
                    i2_mv_y = ps_mv_nmb->i2_mv[1];
                    i2_mv_x += s_mvPred.i2_mv[0];
                    i2_mv_y += s_mvPred.i2_mv[1];
                    s_mvPred.i2_mv[0] = i2_mv_x;
                    s_mvPred.i2_mv[1] = i2_mv_y;

                    /********************************************************/
                    /* Transfer setup call                                  */
                    /* convert RefIdx if it is MbAff                        */
                    /* Pass Weight Offset and refFrame                      */
                    /********************************************************/
                    i1_ref_idx1 = i1_ref_idx >> u1_scale_ref;
                    if(u1_scale_ref && ((i1_ref_idx & 0x01) != u4_bot_mb))
                        i1_ref_idx1 += MAX_REF_BUFS;
                    ps_ref_frame = pps_ref_frame[i1_ref_idx1];
                    pu4_wt_offst = ppu4_wt_ofst[u1_blk_no];






                    {
                    pred_info_pkd_t *ps_pred_pkd;
                    ps_pred_pkd = ps_dec->ps_pred_pkd + ps_dec->u4_pred_info_pkd_idx;
                    ih264d_fill_pred_info (s_mvPred.i2_mv,u1_wd,u1_ht,u1_sub_mb_num,PRED_L0,ps_pred_pkd,
                                           ps_ref_frame->u1_pic_buf_id,(i1_ref_idx >> u1_scale_ref),pu4_wt_offst,
                                           ps_ref_frame->u1_pic_type);

                    ps_dec->u4_pred_info_pkd_idx++;
                    ps_cur_mb_info->u1_num_pred_parts++;
                    }



                    /* Fill colocated info in MvPred structure */
                    s_mvPred.u1_col_ref_pic_idx = ps_ref_frame->u1_mv_buf_id;
                    s_mvPred.u1_pic_type = ps_ref_frame->u1_pic_type;

                    /* Calculating colocated zero information */
                    u1_colz =
                                    (u1_field << 1)
                                                    | ((i1_ref_idx == 0)
                                                                    && (ABS(i2_mv_x)
                                                                                    <= 1)
                                                                    && (ABS(i2_mv_y)
                                                                                    <= 1));
                    u1_colz |= ps_mb_part_info->u1_col_info[u1_blk_no];

                    /* Replicate the motion vectors and colzero u4_flag  */
                    /* for all sub-partitions                         */

                    ih264d_rep_mv_colz(ps_dec, &s_mvPred, ps_mv_nmb,
                                       u1_sub_mb_num, u1_colz, u1_ht,
                                       u1_wd);
                }
            }

        }
        else
        {
            /* Storing colocated zero information */
            ih264d_rep_mv_colz(ps_dec, &s_mvPred, ps_mv_nmb_start, 0,
                               (UWORD8)(u1_field << 1), 4, 4);

        }
        /*if num _cores is set to 3,compute bs will be done in another thread*/
        if(ps_dec->u4_num_cores < 3)
        {

            if(ps_dec->u4_app_disable_deblk_frm == 0)
                ps_dec->pf_compute_bs(ps_dec, ps_cur_mb_info,
                                     (UWORD16)(i >> u1_mbaff));
        }
    }



    return OK;
}


WORD32 ih264d_decode_recon_tfr_nmb(dec_struct_t * ps_dec,
                                   UWORD8 u1_mb_idx,
                                   UWORD8 u1_num_mbs,
                                   UWORD8 u1_num_mbs_next,
                                   UWORD8 u1_tfr_n_mb,
                                   UWORD8 u1_end_of_row)
{
    WORD32 i,j;
    UWORD32 u1_end_of_row_next;
    dec_mb_info_t * ps_cur_mb_info;
    UWORD32 u4_update_mbaff = 0;
    WORD32 ret;
    const UWORD32 u1_mbaff = ps_dec->ps_cur_slice->u1_mbaff_frame_flag;
    const UWORD32 u1_slice_type = ps_dec->ps_cur_slice->u1_slice_type;
    const WORD32 u1_skip_th = (
                    (u1_slice_type != I_SLICE) ?
                                    (ps_dec->u1_B ? B_8x8 : PRED_8x8R0) : -1);
    const UWORD32 u1_ipcm_th = (
                    (u1_slice_type != I_SLICE) ? (ps_dec->u1_B ? 23 : 5) : 0);





    /* N Mb MC Loop */
    for(i = u1_mb_idx; i < u1_num_mbs; i++)
    {
        ps_cur_mb_info = ps_dec->ps_nmb_info + i;
        ps_dec->u4_dma_buf_idx = 0;
        ps_dec->u4_pred_info_idx = 0;

        if(ps_cur_mb_info->u1_mb_type <= u1_skip_th)
        {
            {
                WORD32 pred_cnt = 0;
                pred_info_pkd_t *ps_pred_pkd;
                UWORD32 u4_pred_info_pkd_idx;
                WORD8 i1_pred;

                u4_pred_info_pkd_idx = ps_cur_mb_info->u4_pred_info_pkd_idx;

                while(pred_cnt < ps_cur_mb_info->u1_num_pred_parts)
                {

                    ps_pred_pkd = ps_dec->ps_pred_pkd + u4_pred_info_pkd_idx;

                     ps_dec->p_form_mb_part_info(ps_pred_pkd,ps_dec,
                                               ps_cur_mb_info->u2_mbx,ps_cur_mb_info->u2_mby,(i >> u1_mbaff),
                                         ps_cur_mb_info);
                    u4_pred_info_pkd_idx++;
                    pred_cnt++;
                }
            }

            ps_dec->p_motion_compensate(ps_dec, ps_cur_mb_info);

        }
        else if(ps_cur_mb_info->u1_mb_type == MB_SKIP)
        {
            {
                WORD32 pred_cnt = 0;
                pred_info_pkd_t *ps_pred_pkd;
                UWORD32 u4_pred_info_pkd_idx;
                WORD8 i1_pred;

                u4_pred_info_pkd_idx = ps_cur_mb_info->u4_pred_info_pkd_idx;

                while(pred_cnt < ps_cur_mb_info->u1_num_pred_parts)
                {

                    ps_pred_pkd = ps_dec->ps_pred_pkd + u4_pred_info_pkd_idx;

                    ps_dec->p_form_mb_part_info(ps_pred_pkd,ps_dec,
                                               ps_cur_mb_info->u2_mbx,ps_cur_mb_info->u2_mby,(i >> u1_mbaff),
                                         ps_cur_mb_info);

                    u4_pred_info_pkd_idx++;
                    pred_cnt++;
                }
            }
            /* Decode MB skip */
            ps_dec->p_motion_compensate(ps_dec, ps_cur_mb_info);

        }

     }


    /* N Mb IQ IT RECON  Loop */
    for(j = u1_mb_idx; j < i; j++)
    {
        ps_cur_mb_info = ps_dec->ps_nmb_info + j;

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


        if(ps_dec->u4_use_intrapred_line_copy)
        {
            ih264d_copy_intra_pred_line(ps_dec, ps_cur_mb_info, j);
        }

    }

    /*N MB deblocking*/
    if(ps_dec->u4_nmb_deblk == 1)
    {

        UWORD32 u4_cur_mb, u4_right_mb;
        UWORD32 u4_mb_x, u4_mb_y;
        UWORD32 u4_wd_y, u4_wd_uv;
        tfr_ctxt_t *ps_tfr_cxt = &(ps_dec->s_tran_addrecon);
        UWORD8 u1_field_pic_flag = ps_dec->ps_cur_slice->u1_field_pic_flag;
        const WORD32 i4_cb_qp_idx_ofst =
                       ps_dec->ps_cur_pps->i1_chroma_qp_index_offset;
        const WORD32 i4_cr_qp_idx_ofst =
                       ps_dec->ps_cur_pps->i1_second_chroma_qp_index_offset;

        u4_wd_y = ps_dec->u2_frm_wd_y << u1_field_pic_flag;
        u4_wd_uv = ps_dec->u2_frm_wd_uv << u1_field_pic_flag;


        ps_cur_mb_info = ps_dec->ps_nmb_info + u1_mb_idx;

        ps_dec->u4_deblk_mb_x = ps_cur_mb_info->u2_mbx;
        ps_dec->u4_deblk_mb_y = ps_cur_mb_info->u2_mby;

        for(j = u1_mb_idx; j < i; j++)
        {

            ih264d_deblock_mb_nonmbaff(ps_dec, ps_tfr_cxt,
                                       i4_cb_qp_idx_ofst, i4_cr_qp_idx_ofst,
                                        u4_wd_y, u4_wd_uv);


        }



    }



    if(u1_tfr_n_mb)
    {
        /****************************************************************/
        /* Check for End Of Row in Next iteration                       */
        /****************************************************************/
        u1_end_of_row_next =
                        u1_num_mbs_next
                                        && (u1_num_mbs_next
                                                        <= (ps_dec->u1_recon_mb_grp
                                                                        >> u1_mbaff));

        /****************************************************************/
        /* Transfer the Following things                                */
        /* N-Mb DeblkParams Data    ( To Ext DeblkParams Buffer )       */
        /* N-Mb Recon Data          ( To Ext Frame Buffer )             */
        /* N-Mb Intrapredline Data  ( Updated Internally)               */
        /* N-Mb MV Data             ( To Ext MV Buffer )                */
        /* N-Mb MVTop/TopRight Data ( To Int MV Top Scratch Buffers)    */
        /****************************************************************/
        ih264d_transfer_mb_group_data(ps_dec, u1_num_mbs, u1_end_of_row,
                                      u1_end_of_row_next);
        ps_dec->u4_num_mbs_prev_nmb = u1_num_mbs;

        ps_dec->u4_pred_info_idx = 0;
        ps_dec->u4_dma_buf_idx = 0;


    }
    return OK;
}

/*!
 **************************************************************************
 * \if Function name : ih264d_process_inter_mb \endif
 *
 * \brief
 *    This function decodes an Inter MB.
 *
 *
 * \return
 *    0 on Success and Error code otherwise
 **************************************************************************
 */
WORD32 ih264d_process_inter_mb(dec_struct_t * ps_dec,
                               dec_mb_info_t * ps_cur_mb_info,
                               UWORD8 u1_mb_num)
{
    /* CHANGED CODE */
    UWORD8 *pu1_rec_y, *pu1_rec_u, *pu1_rec_v;

    /*CHANGED CODE */
    UWORD32 ui_rec_width, u4_recwidth_cr;
    WORD16 *pi2_y_coeff;
    UWORD32 u1_mb_field_decoding_flag;
    const UWORD8 u1_mbaff = ps_dec->ps_cur_slice->u1_mbaff_frame_flag;
    UWORD32 uc_botMb;
    UWORD32 u4_num_pmbair;
    /* CHANGED CODE */
    tfr_ctxt_t *ps_frame_buf = ps_dec->ps_frame_buf_ip_recon;
    UWORD32 u4_luma_dc_only_csbp = 0;
    UWORD32 u4_luma_dc_only_cbp = 0;
    /* CHANGED CODE */

    uc_botMb = 1 - ps_cur_mb_info->u1_topmb;
    u4_num_pmbair = (u1_mb_num >> u1_mbaff);
    u1_mb_field_decoding_flag = ps_cur_mb_info->u1_mb_field_decodingflag;


    /* CHANGED CODE */
    pu1_rec_y = ps_frame_buf->pu1_dest_y + (u4_num_pmbair << 4);
    pu1_rec_u =
                    ps_frame_buf->pu1_dest_u
                                    + (u4_num_pmbair << 3) * YUV420SP_FACTOR;
    pu1_rec_v = ps_frame_buf->pu1_dest_v + (u4_num_pmbair << 3);
    ui_rec_width = ps_dec->u2_frm_wd_y << u1_mb_field_decoding_flag;
    u4_recwidth_cr = ps_dec->u2_frm_wd_uv << u1_mb_field_decoding_flag;

    /* CHANGED CODE */

    if(u1_mbaff)
    {
        if(uc_botMb)
        {
            pu1_rec_y += (u1_mb_field_decoding_flag ?
                            (ui_rec_width >> 1) : (ui_rec_width << 4));
            pu1_rec_u += (u1_mb_field_decoding_flag ?
                            (u4_recwidth_cr >> 1) : (u4_recwidth_cr << 3));
            pu1_rec_v += (u1_mb_field_decoding_flag ?
                            (u4_recwidth_cr >> 1) : (u4_recwidth_cr << 3));
        }
    }

    if(!ps_cur_mb_info->u1_tran_form8x8)
    {
        u4_luma_dc_only_csbp = ih264d_unpack_luma_coeff4x4_mb(ps_dec,
                                       ps_cur_mb_info,
                                       0);
    }
    else
    {
        if(!ps_dec->ps_cur_pps->u1_entropy_coding_mode)
        {
            u4_luma_dc_only_cbp = ih264d_unpack_luma_coeff4x4_mb(ps_dec,
                                           ps_cur_mb_info,
                                           0);
        }
        else
        {
            u4_luma_dc_only_cbp = ih264d_unpack_luma_coeff8x8_mb(ps_dec,
                                           ps_cur_mb_info);
        }
    }

    pi2_y_coeff = ps_dec->pi2_coeff_data;
    /* Inverse Transform and Reconstruction */
    if(ps_cur_mb_info->u1_cbp & 0x0f)
    {
        /* CHANGED CODE */
        if(!ps_cur_mb_info->u1_tran_form8x8)
        {
            UWORD32 i;
            WORD16 ai2_tmp[16];
            for(i = 0; i < 16; i++)
            {
                if(CHECKBIT(ps_cur_mb_info->u2_luma_csbp, i))
                {
                    WORD16 *pi2_level = pi2_y_coeff + (i << 4);
                    UWORD8 *pu1_pred_sblk = pu1_rec_y + ((i & 0x3) * BLK_SIZE)
                                    + (i >> 2) * (ui_rec_width << 2);
                    PROFILE_DISABLE_IQ_IT_RECON()
                    {
                        if(CHECKBIT(u4_luma_dc_only_csbp, i))
                        {
                            ps_dec->pf_iquant_itrans_recon_luma_4x4_dc(
                                            pi2_level,
                                            pu1_pred_sblk,
                                            pu1_pred_sblk,
                                            ui_rec_width,
                                            ui_rec_width,
                                            gau2_ih264_iquant_scale_4x4[ps_cur_mb_info->u1_qp_rem6],
                                            (UWORD16 *)ps_dec->s_high_profile.i2_scalinglist4x4[3],
                                            ps_cur_mb_info->u1_qp_div6, ai2_tmp, 0,
                                            NULL);
                        }
                        else
                        {
                            ps_dec->pf_iquant_itrans_recon_luma_4x4(
                                            pi2_level,
                                            pu1_pred_sblk,
                                            pu1_pred_sblk,
                                            ui_rec_width,
                                            ui_rec_width,
                                            gau2_ih264_iquant_scale_4x4[ps_cur_mb_info->u1_qp_rem6],
                                            (UWORD16 *)ps_dec->s_high_profile.i2_scalinglist4x4[3],
                                            ps_cur_mb_info->u1_qp_div6, ai2_tmp, 0,
                                            NULL);
                        }
                    }
                }
            }
        }
        else
        {
            WORD16 *pi2_scale_matrix_ptr;
            WORD32 i;

            pi2_scale_matrix_ptr =
                            ps_dec->s_high_profile.i2_scalinglist8x8[1];

            for(i = 0; i < 4; i++)
            {
                WORD16 ai2_tmp[64];
                WORD16 *pi16_levelBlock = pi2_y_coeff + (i << 6); /* move to the next 8x8 adding 64 */

                UWORD8 *pu1_pred_sblk = pu1_rec_y + ((i & 0x1) * BLK8x8SIZE)
                                + (i >> 1) * (ui_rec_width << 3);
                if(CHECKBIT(ps_cur_mb_info->u1_cbp, i))
                {
                    PROFILE_DISABLE_IQ_IT_RECON()
                    {
                        if(CHECKBIT(u4_luma_dc_only_cbp, i))
                        {
                            ps_dec->pf_iquant_itrans_recon_luma_8x8_dc(
                                            pi16_levelBlock,
                                            pu1_pred_sblk,
                                            pu1_pred_sblk,
                                            ui_rec_width,
                                            ui_rec_width,
                                            gau1_ih264d_dequant8x8_cavlc[ps_cur_mb_info->u1_qp_rem6],
                                            (UWORD16 *)pi2_scale_matrix_ptr,
                                            ps_cur_mb_info->u1_qp_div6, ai2_tmp, 0,
                                            NULL);
                        }
                        else
                        {
                            ps_dec->pf_iquant_itrans_recon_luma_8x8(
                                            pi16_levelBlock,
                                            pu1_pred_sblk,
                                            pu1_pred_sblk,
                                            ui_rec_width,
                                            ui_rec_width,
                                            gau1_ih264d_dequant8x8_cavlc[ps_cur_mb_info->u1_qp_rem6],
                                            (UWORD16 *)pi2_scale_matrix_ptr,
                                            ps_cur_mb_info->u1_qp_div6, ai2_tmp, 0,
                                            NULL);
                        }
                    }
                }
            }

        }
    }

    /* Decode Chroma Block */
    ih264d_unpack_chroma_coeff4x4_mb(ps_dec,
                                     ps_cur_mb_info);
    /*--------------------------------------------------------------------*/
    /* Chroma Blocks decoding                                             */
    /*--------------------------------------------------------------------*/
    {
        UWORD8 u1_chroma_cbp = (UWORD8)(ps_cur_mb_info->u1_cbp >> 4);

        if(u1_chroma_cbp != CBPC_ALLZERO)
        {
            UWORD32 u4_scale_u = ps_cur_mb_info->u1_qpc_div6;
            UWORD32 u4_scale_v = ps_cur_mb_info->u1_qpcr_div6;
            UWORD16 u2_chroma_csbp = ps_cur_mb_info->u2_chroma_csbp;

            pi2_y_coeff = ps_dec->pi2_coeff_data;

            {
                UWORD32 i;
                WORD16 ai2_tmp[16];
                for(i = 0; i < 4; i++)
                {
                    WORD16 *pi2_level = pi2_y_coeff + (i << 4);
                    UWORD8 *pu1_pred_sblk = pu1_rec_u
                                    + ((i & 0x1) * BLK_SIZE * YUV420SP_FACTOR)
                                    + (i >> 1) * (u4_recwidth_cr << 2);
                    PROFILE_DISABLE_IQ_IT_RECON()
                    {
                        if(CHECKBIT(u2_chroma_csbp, i))
                        {
                            ps_dec->pf_iquant_itrans_recon_chroma_4x4(
                                            pi2_level,
                                            pu1_pred_sblk,
                                            pu1_pred_sblk,
                                            u4_recwidth_cr,
                                            u4_recwidth_cr,
                                            gau2_ih264_iquant_scale_4x4[ps_cur_mb_info->u1_qpc_rem6],
                                            (UWORD16 *)ps_dec->s_high_profile.i2_scalinglist4x4[4],
                                            u4_scale_u, ai2_tmp, pi2_level);
                        }
                        else if(pi2_level[0] != 0)
                        {
                            ps_dec->pf_iquant_itrans_recon_chroma_4x4_dc(
                                            pi2_level,
                                            pu1_pred_sblk,
                                            pu1_pred_sblk,
                                            u4_recwidth_cr,
                                            u4_recwidth_cr,
                                            gau2_ih264_iquant_scale_4x4[ps_cur_mb_info->u1_qpc_rem6],
                                            (UWORD16 *)ps_dec->s_high_profile.i2_scalinglist4x4[4],
                                            u4_scale_u, ai2_tmp, pi2_level);
                        }
                    }
                }
            }

            pi2_y_coeff += MB_CHROM_SIZE;
            u2_chroma_csbp >>= 4;

            {
                UWORD32 i;
                WORD16 ai2_tmp[16];
                for(i = 0; i < 4; i++)
                {
                    WORD16 *pi2_level = pi2_y_coeff + (i << 4);
                    UWORD8 *pu1_pred_sblk = pu1_rec_u + 1
                                    + ((i & 0x1) * BLK_SIZE * YUV420SP_FACTOR)
                                    + (i >> 1) * (u4_recwidth_cr << 2);
                    PROFILE_DISABLE_IQ_IT_RECON()
                    {
                        if(CHECKBIT(u2_chroma_csbp, i))
                        {
                            ps_dec->pf_iquant_itrans_recon_chroma_4x4(
                                            pi2_level,
                                            pu1_pred_sblk,
                                            pu1_pred_sblk,
                                            u4_recwidth_cr,
                                            u4_recwidth_cr,
                                            gau2_ih264_iquant_scale_4x4[ps_cur_mb_info->u1_qpcr_rem6],
                                            (UWORD16 *)ps_dec->s_high_profile.i2_scalinglist4x4[5],
                                            u4_scale_v, ai2_tmp, pi2_level);
                        }
                        else if(pi2_level[0] != 0)
                        {
                            ps_dec->pf_iquant_itrans_recon_chroma_4x4_dc(
                                            pi2_level,
                                            pu1_pred_sblk,
                                            pu1_pred_sblk,
                                            u4_recwidth_cr,
                                            u4_recwidth_cr,
                                            gau2_ih264_iquant_scale_4x4[ps_cur_mb_info->u1_qpcr_rem6],
                                            (UWORD16 *)ps_dec->s_high_profile.i2_scalinglist4x4[5],
                                            u4_scale_v, ai2_tmp, pi2_level);
                        }
                    }
                }
            }
        }
    }
    return (0);
}

/*!
 **************************************************************************
 * \if Function name : ih264d_parse_pred_weight_table \endif
 *
 * \brief
 *    Implements pred_weight_table() of 7.3.3.2.
 *
 * \return
 *    None
 *
 **************************************************************************
 */
WORD32 ih264d_parse_pred_weight_table(dec_slice_params_t * ps_cur_slice,
                                      dec_bit_stream_t * ps_bitstrm)
{
    UWORD32 *pu4_bitstrm_buf = ps_bitstrm->pu4_buffer;
    UWORD32 *pu4_bitstrm_ofst = &ps_bitstrm->u4_ofst;
    WORD8 i, cont, lx;
    UWORD8 uc_weight_flag;
    UWORD32 *pui32_weight_offset_lx;
    WORD16 c_weight, c_offset;
    UWORD32 ui32_y_def_weight_ofst, ui32_cr_def_weight_ofst;
    UWORD32 ui32_temp;
    UWORD8 uc_luma_log2_weight_denom;
    UWORD8 uc_chroma_log2_weight_denom;

    /* Variables for error resilience checks */
    UWORD32 u4_temp;
    WORD32 i_temp;

    u4_temp = ih264d_uev(pu4_bitstrm_ofst, pu4_bitstrm_buf);
    if(u4_temp > MAX_LOG2_WEIGHT_DENOM)
    {
        return ERROR_PRED_WEIGHT_TABLE_T;
    }
    uc_luma_log2_weight_denom = u4_temp;
    COPYTHECONTEXT("SH: luma_log2_weight_denom",uc_luma_log2_weight_denom);
    ui32_y_def_weight_ofst = (1 << uc_luma_log2_weight_denom);

    u4_temp = ih264d_uev(pu4_bitstrm_ofst, pu4_bitstrm_buf);
    if(u4_temp > MAX_LOG2_WEIGHT_DENOM)
    {
        return ERROR_PRED_WEIGHT_TABLE_T;
    }
    uc_chroma_log2_weight_denom = u4_temp;
    COPYTHECONTEXT("SH: chroma_log2_weight_denom",uc_chroma_log2_weight_denom);
    ui32_cr_def_weight_ofst = (1 << uc_chroma_log2_weight_denom);

    ps_cur_slice->u2_log2Y_crwd = uc_luma_log2_weight_denom
                    | (uc_chroma_log2_weight_denom << 8);

    cont = (ps_cur_slice->u1_slice_type == B_SLICE);
    lx = 0;
    do
    {
        for(i = 0; i < ps_cur_slice->u1_num_ref_idx_lx_active[lx]; i++)
        {
            pui32_weight_offset_lx = ps_cur_slice->u4_wt_ofst_lx[lx][i];

            uc_weight_flag = ih264d_get_bit_h264(ps_bitstrm);
            pu4_bitstrm_buf = ps_bitstrm->pu4_buffer;
            COPYTHECONTEXT("SH: luma_weight_l0_flag",uc_weight_flag);
            if(uc_weight_flag)
            {
                i_temp = ih264d_sev(pu4_bitstrm_ofst,
                                    pu4_bitstrm_buf);
                if((i_temp < PRED_WEIGHT_MIN) || (i_temp > PRED_WEIGHT_MAX))
                    return ERROR_PRED_WEIGHT_TABLE_T;
                c_weight = i_temp;
                COPYTHECONTEXT("SH: luma_weight_l0",c_weight);

                i_temp = ih264d_sev(pu4_bitstrm_ofst,
                                    pu4_bitstrm_buf);
                if((i_temp < PRED_WEIGHT_MIN) || (i_temp > PRED_WEIGHT_MAX))
                    return ERROR_PRED_WEIGHT_TABLE_T;
                c_offset = i_temp;
                COPYTHECONTEXT("SH: luma_offset_l0",c_offset);

                ui32_temp = (c_offset << 16) | (c_weight & 0xFFFF);
                pui32_weight_offset_lx[0] = ui32_temp;
            }
            else
            {

                pui32_weight_offset_lx[0] = ui32_y_def_weight_ofst;
            }

            {
                WORD8 c_weightCb, c_weightCr, c_offsetCb, c_offsetCr;
                uc_weight_flag = ih264d_get_bit_h264(ps_bitstrm);
                pu4_bitstrm_buf = ps_bitstrm->pu4_buffer;
                COPYTHECONTEXT("SH: chroma_weight_l0_flag",uc_weight_flag);
                if(uc_weight_flag)
                {
                    i_temp = ih264d_sev(pu4_bitstrm_ofst,
                                        pu4_bitstrm_buf);
                    if((i_temp < PRED_WEIGHT_MIN) || (i_temp > PRED_WEIGHT_MAX))
                        return ERROR_PRED_WEIGHT_TABLE_T;
                    c_weightCb = i_temp;
                    COPYTHECONTEXT("SH: chroma_weight_l0",c_weightCb);

                    i_temp = ih264d_sev(pu4_bitstrm_ofst,
                                        pu4_bitstrm_buf);
                    if((i_temp < PRED_WEIGHT_MIN) || (i_temp > PRED_WEIGHT_MAX))
                        return ERROR_PRED_WEIGHT_TABLE_T;
                    c_offsetCb = i_temp;
                    COPYTHECONTEXT("SH: chroma_weight_l0",c_offsetCb);

                    ui32_temp = (c_offsetCb << 16) | (c_weightCb & 0xFFFF);
                    pui32_weight_offset_lx[1] = ui32_temp;

                    i_temp = ih264d_sev(pu4_bitstrm_ofst,
                                        pu4_bitstrm_buf);
                    if((i_temp < PRED_WEIGHT_MIN) || (i_temp > PRED_WEIGHT_MAX))
                        return ERROR_PRED_WEIGHT_TABLE_T;
                    c_weightCr = i_temp;
                    COPYTHECONTEXT("SH: chroma_weight_l0",c_weightCr);

                    i_temp = ih264d_sev(pu4_bitstrm_ofst,
                                        pu4_bitstrm_buf);
                    if((i_temp < PRED_WEIGHT_MIN) || (i_temp > PRED_WEIGHT_MAX))
                        return ERROR_PRED_WEIGHT_TABLE_T;
                    c_offsetCr = i_temp;
                    COPYTHECONTEXT("SH: chroma_weight_l0",c_offsetCr);

                    ui32_temp = (c_offsetCr << 16) | (c_weightCr & 0xFFFF);
                    pui32_weight_offset_lx[2] = ui32_temp;
                }
                else
                {
                    pui32_weight_offset_lx[1] = ui32_cr_def_weight_ofst;
                    pui32_weight_offset_lx[2] = ui32_cr_def_weight_ofst;
                }
            }
        }
        lx++;
    }
    while(cont--);

    return OK;
}

static int pic_num_compare(const void *pv_pic1, const void *pv_pic2)
{
    struct pic_buffer_t *ps_pic1 = *(struct pic_buffer_t **) pv_pic1;
    struct pic_buffer_t *ps_pic2 = *(struct pic_buffer_t **) pv_pic2;
    if (ps_pic1->i4_pic_num < ps_pic2->i4_pic_num)
    {
        return -1;
    }
    else if (ps_pic1->i4_pic_num > ps_pic2->i4_pic_num)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}
/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_init_ref_idx_lx_p                                        */
/*                                                                           */
/*  Description   : This function initializes the reference picture L0 list  */
/*                  for P slices as per section 8.2.4.2.1 and 8.2.4.2.2.     */
/*                                                                           */
/*  Inputs        : pointer to ps_dec struture                               */
/*  Globals       : NO                                                       */
/*  Processing    : arranges all the short term pictures according to        */
/*                  pic_num in descending order starting from curr pic_num.  */
/*                  and inserts it in L0 list followed by all Long term      */
/*                  pictures in ascending order.                             */
/*                                                                           */
/*  Returns       : void                                                     */
/*                                                                           */
/*  Issues        : <List any issues or problems with this function>         */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         13 07 2002   Jay             Draft                                */
/*                                                                           */
/*****************************************************************************/
void ih264d_init_ref_idx_lx_p(dec_struct_t *ps_dec)
{
    struct pic_buffer_t *ps_ref_pic_buf_lx;
    dpb_manager_t *ps_dpb_mgr;
    struct dpb_info_t *ps_next_dpb;
    WORD8 i, j;
    UWORD8 u1_max_lt_index, u1_min_lt_index;
    UWORD32 u4_lt_index;
    UWORD8 u1_field_pic_flag;
    dec_slice_params_t *ps_cur_slice;
    UWORD8 u1_L0;
    WORD32 i4_cur_pic_num, i4_min_st_pic_num;
    WORD32 i4_temp_pic_num, i4_ref_pic_num;
    UWORD8 u1_num_short_term_bufs;
    UWORD8 u1_max_ref_idx_l0;
    struct pic_buffer_t *aps_st_pic_bufs[2 * MAX_REF_BUFS] = {NULL};

    ps_cur_slice = ps_dec->ps_cur_slice;
    u1_field_pic_flag = ps_cur_slice->u1_field_pic_flag;
    u1_max_ref_idx_l0 = ps_cur_slice->u1_num_ref_idx_lx_active[0]
                    << u1_field_pic_flag;

    ps_dpb_mgr = ps_dec->ps_dpb_mgr;
    /* Get the current frame number */
    i4_cur_pic_num = ps_dec->ps_cur_pic->i4_pic_num;

    /* Get Min pic_num,MinLt */
    i4_min_st_pic_num = i4_cur_pic_num;
    u1_max_lt_index = MAX_REF_BUFS + 1;
    u1_min_lt_index = MAX_REF_BUFS + 1;

    /* Start from ST head */
    ps_next_dpb = ps_dpb_mgr->ps_dpb_st_head;
    for(i = 0; i < ps_dpb_mgr->u1_num_st_ref_bufs; i++)
    {
        i4_ref_pic_num = ps_next_dpb->ps_pic_buf->i4_pic_num;
        if(i4_ref_pic_num < i4_cur_pic_num)
        {
            /* RefPic Buf pic_num is before Current pic_num in decode order */
            i4_min_st_pic_num = MIN(i4_min_st_pic_num, i4_ref_pic_num);
        }

        /* Chase the next link */
        ps_next_dpb = ps_next_dpb->ps_prev_short;
    }

    /* Sort ST ref pocs in ascending order */
    ps_next_dpb = ps_dpb_mgr->ps_dpb_st_head;
    for (j = 0; j < ps_dpb_mgr->u1_num_st_ref_bufs; j++)
    {
        aps_st_pic_bufs[j] = ps_next_dpb->ps_pic_buf;
        ps_next_dpb = ps_next_dpb->ps_prev_short;
    }
    qsort(aps_st_pic_bufs, ps_dpb_mgr->u1_num_st_ref_bufs,
        sizeof(aps_st_pic_bufs[0]), pic_num_compare);

    /* Start from LT head */
    ps_next_dpb = ps_dpb_mgr->ps_dpb_ht_head;
    if(ps_next_dpb)
    {
        u1_max_lt_index = ps_next_dpb->u1_lt_idx;
        u1_min_lt_index = ps_next_dpb->u1_lt_idx;

        for(i = 0; i < ps_dpb_mgr->u1_num_lt_ref_bufs; i++)
        {
            u4_lt_index = ps_next_dpb->u1_lt_idx;
            u1_max_lt_index = (UWORD8)(MAX(u1_max_lt_index, u4_lt_index));
            u1_min_lt_index = (UWORD8)(MIN(u1_min_lt_index, u4_lt_index));

            /* Chase the next link */
            ps_next_dpb = ps_next_dpb->ps_prev_long;
        }
    }
    /* 1. Initialize refIdxL0 */
    u1_L0 = 0;
    if(u1_field_pic_flag)
    {
        ps_ref_pic_buf_lx = ps_dpb_mgr->ps_init_dpb[0][0];
        ps_ref_pic_buf_lx += MAX_REF_BUFS;
        i4_temp_pic_num = i4_cur_pic_num;
    }
    else
    {
        ps_ref_pic_buf_lx = ps_dpb_mgr->ps_init_dpb[0][0];
        i4_temp_pic_num = i4_cur_pic_num;
    }
    /* Arrange all short term buffers in output order as given by pic_num */
    /* Arrange pic_num's less than Curr pic_num in the descending pic_num */
    /* order starting from (Curr pic_num - 1)                             */
    for(j = ps_dpb_mgr->u1_num_st_ref_bufs - 1; j >= 0; j--)
    {
        if(aps_st_pic_bufs[j])
        {
            /* Copy info in pic buffer */
            ih264d_insert_pic_in_ref_pic_listx(ps_ref_pic_buf_lx,
                                               aps_st_pic_bufs[j]);
            ps_ref_pic_buf_lx++;
            u1_L0++;
        }
    }

    /* Arrange all Long term buffers in ascending order, in LongtermIndex */
    /* Start from LT head */
    u1_num_short_term_bufs = u1_L0;
    for(u4_lt_index = u1_min_lt_index; u4_lt_index <= u1_max_lt_index;
                    u4_lt_index++)
    {
        ps_next_dpb = ps_dpb_mgr->ps_dpb_ht_head;
        for(i = 0; i < ps_dpb_mgr->u1_num_lt_ref_bufs; i++)
        {
            if(ps_next_dpb->u1_lt_idx == u4_lt_index)
            {
                ih264d_insert_pic_in_ref_pic_listx(ps_ref_pic_buf_lx,
                                                   ps_next_dpb->ps_pic_buf);

                ps_ref_pic_buf_lx->u1_long_term_pic_num =
                                ps_ref_pic_buf_lx->u1_long_term_frm_idx;
                ps_ref_pic_buf_lx++;
                u1_L0++;
                break;
            }
            ps_next_dpb = ps_next_dpb->ps_prev_long;
        }
    }

    if(u1_field_pic_flag)
    {
        /* Initialize the rest of the entries in the */
        /* reference list to handle of errors        */
        {
            UWORD8 u1_i;
            pic_buffer_t ref_pic;

            ref_pic = *(ps_dpb_mgr->ps_init_dpb[0][0] + MAX_REF_BUFS);

            if(NULL == ref_pic.pu1_buf1)
            {
                ref_pic = *ps_dec->ps_cur_pic;
            }
            for(u1_i = u1_L0; u1_i < u1_max_ref_idx_l0; u1_i++)
            {
                *ps_ref_pic_buf_lx = ref_pic;
                ps_ref_pic_buf_lx++;
            }
        }

        ih264d_convert_frm_to_fld_list(
                        ps_dpb_mgr->ps_init_dpb[0][0] + MAX_REF_BUFS, &u1_L0,
                        ps_dec, u1_num_short_term_bufs);

        ps_ref_pic_buf_lx = ps_dpb_mgr->ps_init_dpb[0][0] + u1_L0;
    }

    /* Initialize the rest of the entries in the */
    /* reference list to handle of errors        */
    {
        UWORD8 u1_i;
        pic_buffer_t ref_pic;

        ref_pic = *(ps_dpb_mgr->ps_init_dpb[0][0]);

        if(NULL == ref_pic.pu1_buf1)
        {
            ref_pic = *ps_dec->ps_cur_pic;
        }
        for(u1_i = u1_L0; u1_i < u1_max_ref_idx_l0; u1_i++)
        {
            *ps_ref_pic_buf_lx = ref_pic;
            ps_ref_pic_buf_lx++;
        }
    }
    ps_dec->ps_cur_slice->u1_initial_list_size[0] = u1_L0;
}

