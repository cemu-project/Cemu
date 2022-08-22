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
 * \file ih264d_thread_parse_decode.c
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
#include "ithread.h"
#include <string.h>
#include "ih264d_defs.h"
#include "ih264d_debug.h"
#include "ih264d_tables.h"
#include "ih264d_structs.h"
#include "ih264d_defs.h"
#include "ih264d_mb_utils.h"
#include "ih264d_thread_parse_decode.h"
#include "ih264d_inter_pred.h"

#include "ih264d_process_pslice.h"
#include "ih264d_process_intra_mb.h"
#include "ih264d_deblocking.h"
#include "ih264d_format_conv.h"

void ih264d_deblock_mb_level(dec_struct_t *ps_dec,
                             dec_mb_info_t *ps_cur_mb_info,
                             UWORD32 nmb_index);

void ih264d_copy_intra_pred_line(dec_struct_t *ps_dec,
                                 dec_mb_info_t *ps_cur_mb_info,
                                 UWORD32 nmb_index);

void ih264d_parse_tfr_nmb(dec_struct_t * ps_dec,
                          UWORD8 u1_mb_idx,
                          UWORD8 u1_num_mbs,
                          UWORD8 u1_num_mbs_next,
                          UWORD8 u1_tfr_n_mb,
                          UWORD8 u1_end_of_row)
{
    WORD32 i, u4_mb_num;

    const UWORD32 u1_mbaff = ps_dec->ps_cur_slice->u1_mbaff_frame_flag;
    UWORD32 u4_n_mb_start;

    UNUSED(u1_mb_idx);
    UNUSED(u1_num_mbs_next);
    if(u1_tfr_n_mb)
    {


        u4_n_mb_start = (ps_dec->u2_cur_mb_addr + 1) - u1_num_mbs;

        // copy into s_frmMbInfo

        u4_mb_num = u4_n_mb_start;
        u4_mb_num = (ps_dec->u2_cur_mb_addr + 1) - u1_num_mbs;

        for(i = 0; i < u1_num_mbs; i++)
        {
            UPDATE_SLICE_NUM_MAP(ps_dec->pu2_slice_num_map, u4_mb_num,
                                 ps_dec->u2_cur_slice_num);
            DATA_SYNC();
            UPDATE_MB_MAP_MBNUM_BYTE(ps_dec->pu1_dec_mb_map, u4_mb_num);

            u4_mb_num++;
        }

        /****************************************************************/
        /* Check for End Of Row in Next iteration                       */
        /****************************************************************/

        /****************************************************************/
        /* Transfer the Following things                                */
        /* N-Mb DeblkParams Data    ( To Ext DeblkParams Buffer )       */
        /* N-Mb Recon Data          ( To Ext Frame Buffer )             */
        /* N-Mb Intrapredline Data  ( Updated Internally)               */
        /* N-Mb MV Data             ( To Ext MV Buffer )                */
        /* N-Mb MVTop/TopRight Data ( To Int MV Top Scratch Buffers)    */
        /****************************************************************/

        /* Swap top and current pointers */

        ps_dec->s_tran_addrecon_parse.pu1_dest_y +=
                        ps_dec->s_tran_addrecon_parse.u4_inc_y[u1_end_of_row];
        ps_dec->s_tran_addrecon_parse.pu1_dest_u +=
                        ps_dec->s_tran_addrecon_parse.u4_inc_uv[u1_end_of_row];
        ps_dec->s_tran_addrecon_parse.pu1_dest_v +=
                        ps_dec->s_tran_addrecon_parse.u4_inc_uv[u1_end_of_row];

        if(u1_end_of_row)
        {
            UWORD16 u2_mb_y;
            UWORD32 u4_frame_stride, y_offset;

            ps_dec->ps_top_mb_row = ps_dec->ps_cur_mb_row;
            ps_dec->ps_cur_mb_row += ((ps_dec->u2_frm_wd_in_mbs) << u1_mbaff);

            u2_mb_y = ps_dec->u2_mby + (1 + u1_mbaff);
            u4_frame_stride = ps_dec->u2_frm_wd_y
                            << ps_dec->ps_cur_slice->u1_field_pic_flag;
            y_offset = (u2_mb_y * u4_frame_stride) << 4;
            ps_dec->s_tran_addrecon_parse.pu1_dest_y =
                            ps_dec->s_cur_pic.pu1_buf1 + y_offset;

            u4_frame_stride = ps_dec->u2_frm_wd_uv
                            << ps_dec->ps_cur_slice->u1_field_pic_flag;
            y_offset = (u2_mb_y * u4_frame_stride) << 3;
            ps_dec->s_tran_addrecon_parse.pu1_dest_u =
                            ps_dec->s_cur_pic.pu1_buf2 + y_offset;
            ps_dec->s_tran_addrecon_parse.pu1_dest_v =
                            ps_dec->s_cur_pic.pu1_buf3 + y_offset;

        }

        ps_dec->ps_deblk_mbn += u1_num_mbs;

        /*
         * The Slice boundary is also a valid condition to transfer. So recalculate
         * the Left increment, in case the number of MBs is lesser than the
         * N MB value. c_numMbs will be equal to N of N MB if the entire N Mb is
         * decoded.
         */
        ps_dec->s_tran_addrecon.u2_mv_left_inc = ((u1_num_mbs >> u1_mbaff) - 1)
                        << (4 + u1_mbaff);
        ps_dec->s_tran_addrecon.u2_mv_top_left_inc = (u1_num_mbs << 2) - 1
                        - (u1_mbaff << 2);

        /* reassign left MV and cur MV pointers */
        ps_dec->ps_mv_left = ps_dec->ps_mv_cur
                        + ps_dec->s_tran_addrecon.u2_mv_left_inc;

        ps_dec->ps_mv_cur += (u1_num_mbs << 4);
        ps_dec->u4_num_mbs_prev_nmb = u1_num_mbs;

    }
}

void ih264d_decode_tfr_nmb(dec_struct_t * ps_dec,
                           UWORD8 u1_num_mbs,
                           UWORD8 u1_num_mbs_next,
                           UWORD8 u1_end_of_row)
{

    UWORD32 u1_end_of_row_next;

    const UWORD32 u1_mbaff = ps_dec->ps_cur_slice->u1_mbaff_frame_flag;

    /****************************************************************/
    /* Check for End Of Row in Next iteration                       */
    /****************************************************************/
    u1_end_of_row_next = u1_num_mbs_next &&
                        ((u1_num_mbs_next) <= (ps_dec->u1_recon_mb_grp >> u1_mbaff));

    /****************************************************************/
    /* Transfer the Following things                                */
    /* N-Mb DeblkParams Data    ( To Ext DeblkParams Buffer )       */
    /* N-Mb Recon Data          ( To Ext Frame Buffer )             */
    /* N-Mb Intrapredline Data  ( Updated Internally)               */
    /* N-Mb MV Data             ( To Ext MV Buffer )                */
    /* N-Mb MVTop/TopRight Data ( To Int MV Top Scratch Buffers)    */
    /****************************************************************/
    if(u1_end_of_row)
    {
        ps_dec->i2_dec_thread_mb_y += (1 << u1_mbaff);
    }
    ih264d_transfer_mb_group_data(ps_dec, u1_num_mbs, u1_end_of_row,
                                  u1_end_of_row_next);

}

WORD32 ih264d_decode_recon_tfr_nmb_thread(dec_struct_t * ps_dec,
                                          UWORD8 u1_num_mbs,
                                          UWORD8 u1_num_mbs_next,
                                          UWORD8 u1_end_of_row)
{
    WORD32 i,j;
    dec_mb_info_t * ps_cur_mb_info;
    UWORD32 u4_update_mbaff = 0;
    const UWORD32 u1_mbaff = ps_dec->ps_cur_slice->u1_mbaff_frame_flag;
    UWORD32 u1_slice_type, u1_B;
    WORD32 u1_skip_th;
    UWORD32 u1_ipcm_th;
    UWORD32 u4_cond;
    UWORD16 u2_slice_num,u2_cur_dec_mb_num;
    WORD32 ret;
    UWORD32 u4_mb_num;
    WORD32 nop_cnt = 8*128;
    u1_slice_type = ps_dec->ps_decode_cur_slice->slice_type;

    u1_B = (u1_slice_type == B_SLICE);

    u1_skip_th = ((u1_slice_type != I_SLICE) ?
                                    (u1_B ? B_8x8 : PRED_8x8R0) : -1);

    u1_ipcm_th = ((u1_slice_type != I_SLICE) ? (u1_B ? 23 : 5) : 0);

    u2_cur_dec_mb_num = ps_dec->cur_dec_mb_num;

    while(1)
    {

        UWORD32 u4_max_mb = (UWORD32)(ps_dec->i2_dec_thread_mb_y + (1 << u1_mbaff)) * ps_dec->u2_frm_wd_in_mbs - 1;
        u4_mb_num = u2_cur_dec_mb_num;
        /*introducing 1 MB delay*/
        u4_mb_num = MIN(u4_mb_num + u1_num_mbs + 1, u4_max_mb);

        CHECK_MB_MAP_BYTE(u4_mb_num, ps_dec->pu1_dec_mb_map, u4_cond);
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
                if(ps_dec->u4_output_present && (2 == ps_dec->u4_num_cores) &&
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
    /* N Mb MC Loop */
    for(i = 0; i < u1_num_mbs; i++)
    {
        u4_mb_num = u2_cur_dec_mb_num;

        GET_SLICE_NUM_MAP(ps_dec->pu2_slice_num_map, u2_cur_dec_mb_num,
                          u2_slice_num);

        if(u2_slice_num != ps_dec->u2_cur_slice_num_dec_thread)
        {
            ps_dec->u4_cur_slice_decode_done = 1;
            break;
        }

        ps_cur_mb_info = &ps_dec->ps_frm_mb_info[u2_cur_dec_mb_num];

        ps_dec->u4_dma_buf_idx = 0;
        ps_dec->u4_pred_info_idx = 0;

        if(ps_cur_mb_info->u1_mb_type <= u1_skip_th)
        {
            WORD32 pred_cnt = 0;
            pred_info_pkd_t *ps_pred_pkd;
            UWORD32 u4_pred_info_pkd_idx;
            WORD8 i1_pred;

            u4_pred_info_pkd_idx = ps_cur_mb_info->u4_pred_info_pkd_idx;

            while(pred_cnt < ps_cur_mb_info->u1_num_pred_parts)
            {
                ps_pred_pkd = ps_dec->ps_pred_pkd + u4_pred_info_pkd_idx;

                ps_dec->p_form_mb_part_info_thread(ps_pred_pkd,ps_dec,
                                                   ps_cur_mb_info->u2_mbx,
                                                   ps_cur_mb_info->u2_mby,
                                                   (i >> u1_mbaff),
                                                   ps_cur_mb_info);

                u4_pred_info_pkd_idx++;
                pred_cnt++;
            }
            ps_dec->p_mc_dec_thread(ps_dec, ps_cur_mb_info);
        }
        else if(ps_cur_mb_info->u1_mb_type == MB_SKIP)
        {
            WORD32 pred_cnt = 0;
            pred_info_pkd_t *ps_pred_pkd;
            UWORD32 u4_pred_info_pkd_idx;
            WORD8 i1_pred;

            u4_pred_info_pkd_idx = ps_cur_mb_info->u4_pred_info_pkd_idx;

            while(pred_cnt < ps_cur_mb_info->u1_num_pred_parts)
            {
                ps_pred_pkd = ps_dec->ps_pred_pkd + u4_pred_info_pkd_idx;

                ps_dec->p_form_mb_part_info_thread(ps_pred_pkd,ps_dec,
                                                   ps_cur_mb_info->u2_mbx,
                                                   ps_cur_mb_info->u2_mby,
                                                   (i >> u1_mbaff),
                                                   ps_cur_mb_info);

                u4_pred_info_pkd_idx++;
                pred_cnt++;
            }
            /* Decode MB skip */
            ps_dec->p_mc_dec_thread(ps_dec, ps_cur_mb_info);
        }

        u2_cur_dec_mb_num++;
    }

    /* N Mb IQ IT RECON  Loop */
    for(j = 0; j < i; j++)
    {
        ps_cur_mb_info = &ps_dec->ps_frm_mb_info[ps_dec->cur_dec_mb_num];

        if((ps_dec->u4_num_cores == 2) || !ps_dec->i1_recon_in_thread3_flag)
        {
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


         if(ps_dec->u4_use_intrapred_line_copy == 1)
                ih264d_copy_intra_pred_line(ps_dec, ps_cur_mb_info, j);
        }

        DATA_SYNC();

        if(u1_mbaff)
        {
            if(u4_update_mbaff)
            {
                UWORD32 u4_mb_num = ps_cur_mb_info->u2_mbx
                                + ps_dec->u2_frm_wd_in_mbs
                                                * (ps_cur_mb_info->u2_mby >> 1);
                UPDATE_MB_MAP_MBNUM_BYTE(ps_dec->pu1_recon_mb_map, u4_mb_num);
                u4_update_mbaff = 0;
            }
            else
            {
                u4_update_mbaff = 1;
            }
        }
        else
        {
            UWORD32 u4_mb_num = ps_cur_mb_info->u2_mbx
                            + ps_dec->u2_frm_wd_in_mbs * ps_cur_mb_info->u2_mby;
            UPDATE_MB_MAP_MBNUM_BYTE(ps_dec->pu1_recon_mb_map, u4_mb_num);
        }
        ps_dec->cur_dec_mb_num++;
     }

    /*N MB deblocking*/
    if(ps_dec->u4_nmb_deblk == 1)
    {
        UWORD32 u4_wd_y, u4_wd_uv;
        tfr_ctxt_t *ps_tfr_cxt = &(ps_dec->s_tran_addrecon);
        UWORD8 u1_field_pic_flag = ps_dec->ps_cur_slice->u1_field_pic_flag;
        const WORD32 i4_cb_qp_idx_ofst =
                       ps_dec->ps_cur_pps->i1_chroma_qp_index_offset;
        const WORD32 i4_cr_qp_idx_ofst =
                       ps_dec->ps_cur_pps->i1_second_chroma_qp_index_offset;

        u4_wd_y = ps_dec->u2_frm_wd_y << u1_field_pic_flag;
        u4_wd_uv = ps_dec->u2_frm_wd_uv << u1_field_pic_flag;

        ps_cur_mb_info = &ps_dec->ps_frm_mb_info[ps_dec->u4_cur_deblk_mb_num];

        ps_dec->u4_deblk_mb_x = ps_cur_mb_info->u2_mbx;
        ps_dec->u4_deblk_mb_y = ps_cur_mb_info->u2_mby;


        for(j = 0; j < i; j++)
        {
            ih264d_deblock_mb_nonmbaff(ps_dec, ps_tfr_cxt,
                                       i4_cb_qp_idx_ofst, i4_cr_qp_idx_ofst,
                                        u4_wd_y, u4_wd_uv);

        }
    }

    /*handle the last mb in picture case*/
    if(ps_dec->cur_dec_mb_num > ps_dec->ps_cur_sps->u2_max_mb_addr)
        ps_dec->u4_cur_slice_decode_done = 1;

    if(i != u1_num_mbs)
    {
        u1_end_of_row = 0;
        /*Number of MB's left in row*/
        u1_num_mbs_next = u1_num_mbs_next + ((u1_num_mbs - i) >> u1_mbaff);
    }

    ih264d_decode_tfr_nmb(ps_dec, (i), u1_num_mbs_next, u1_end_of_row);

    return OK;
}

WORD32 ih264d_decode_slice_thread(dec_struct_t *ps_dec)
{
    UWORD8 u1_num_mbs_next, u1_num_mbsleft, u1_end_of_row = 0;
    const UWORD32 i2_pic_wdin_mbs = ps_dec->u2_frm_wd_in_mbs;
    UWORD8 u1_mbaff, u1_num_mbs;

    UWORD16 u2_first_mb_in_slice;
    UWORD16 i16_mb_x, i16_mb_y;
    UWORD8 u1_field_pic;
    UWORD32 u4_frame_stride, x_offset, y_offset;
    WORD32 ret;

    tfr_ctxt_t *ps_trns_addr;

    /*check for mb map of first mb in slice to ensure slice header is parsed*/
    while(1)
    {
        UWORD32 u4_mb_num = ps_dec->cur_dec_mb_num;
        UWORD32 u4_cond = 0;
        WORD32 nop_cnt = 8 * 128;
        CHECK_MB_MAP_BYTE(u4_mb_num, ps_dec->pu1_dec_mb_map, u4_cond);
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
            else if(ps_dec->u4_output_present && (2 == ps_dec->u4_num_cores) &&
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
            DEBUG_THREADS_PRINTF("waiting for mb mapcur_dec_mb_num = %d,ps_dec->u2_cur_mb_addr  = %d\n",u2_cur_dec_mb_num,
                            ps_dec->u2_cur_mb_addr);

        }
    }



    u1_mbaff = ps_dec->ps_cur_slice->u1_mbaff_frame_flag;

    u2_first_mb_in_slice = ps_dec->ps_decode_cur_slice->u4_first_mb_in_slice;

    i16_mb_x = MOD(u2_first_mb_in_slice, i2_pic_wdin_mbs);
    i16_mb_y = DIV(u2_first_mb_in_slice, i2_pic_wdin_mbs);
    i16_mb_y <<= u1_mbaff;
    ps_dec->i2_dec_thread_mb_y = i16_mb_y;


    ps_dec->cur_dec_mb_num = u2_first_mb_in_slice << u1_mbaff;

    if((ps_dec->u4_num_cores == 2) || !ps_dec->i1_recon_in_thread3_flag)
    {
        ps_dec->pv_proc_tu_coeff_data =
                (void *) ps_dec->ps_decode_cur_slice->pv_tu_coeff_data_start;
    }

    // recalculate recon pointers
    u1_field_pic = ps_dec->ps_cur_slice->u1_field_pic_flag;
    u4_frame_stride = ps_dec->u2_frm_wd_y << u1_field_pic;
    x_offset = i16_mb_x << 4;
    y_offset = (i16_mb_y * u4_frame_stride) << 4;

    ps_trns_addr = &(ps_dec->s_tran_addrecon);

    ps_trns_addr->pu1_dest_y = ps_dec->s_cur_pic.pu1_buf1 + x_offset + y_offset;

    u4_frame_stride = ps_dec->u2_frm_wd_uv << u1_field_pic;
    x_offset >>= 1;
    y_offset = (i16_mb_y * u4_frame_stride) << 3;

    x_offset *= YUV420SP_FACTOR;

    ps_trns_addr->pu1_dest_u = ps_dec->s_cur_pic.pu1_buf2 + x_offset + y_offset;
    ps_trns_addr->pu1_dest_v = ps_dec->s_cur_pic.pu1_buf3 + x_offset + y_offset;

    ps_trns_addr->pu1_mb_y = ps_trns_addr->pu1_dest_y;
    ps_trns_addr->pu1_mb_u = ps_trns_addr->pu1_dest_u;
    ps_trns_addr->pu1_mb_v = ps_trns_addr->pu1_dest_v;


    /* Initialise MC and formMbPartInfo fn ptrs one time based on profile_idc */
    {
        ps_dec->p_mc_dec_thread = ih264d_motion_compensate_bp;
        ps_dec->p_form_mb_part_info_thread = ih264d_form_mb_part_info_bp;
    }
    {
        UWORD8 uc_nofield_nombaff;
        uc_nofield_nombaff = ((ps_dec->ps_cur_slice->u1_field_pic_flag == 0)
                        && (ps_dec->ps_cur_slice->u1_mbaff_frame_flag == 0)
                        && (ps_dec->ps_decode_cur_slice->slice_type != B_SLICE)
                        && (ps_dec->ps_cur_pps->u1_wted_pred_flag == 0));

        if(uc_nofield_nombaff == 0)
        {
            ps_dec->p_mc_dec_thread = ih264d_motion_compensate_mp;
            ps_dec->p_form_mb_part_info_thread = ih264d_form_mb_part_info_mp;
        }

    }

    ps_dec->u4_cur_slice_decode_done = 0;


    while(ps_dec->u4_cur_slice_decode_done != 1)
    {

        u1_num_mbsleft = ((i2_pic_wdin_mbs - i16_mb_x) << u1_mbaff);

        if(u1_num_mbsleft <= ps_dec->u1_recon_mb_grp)
        {
            u1_num_mbs = u1_num_mbsleft;

            /*Indicate number of mb's left in a row*/
            u1_num_mbs_next = 0;
            u1_end_of_row = 1;
            i16_mb_x = 0;
        }
        else
        {
            u1_num_mbs = ps_dec->u1_recon_mb_grp;

            /*Indicate number of mb's left in a row*/
            u1_num_mbs_next = i2_pic_wdin_mbs - i16_mb_x
                            - (ps_dec->u1_recon_mb_grp >> u1_mbaff);
            i16_mb_x += (u1_num_mbs >> u1_mbaff);
            u1_end_of_row = 0;

        }
        ret = ih264d_decode_recon_tfr_nmb_thread(ps_dec, u1_num_mbs, u1_num_mbs_next,
                                           u1_end_of_row);
        if(ret != OK)
            return ret;
    }
    return OK;
}

void ih264d_decode_picture_thread(dec_struct_t *ps_dec )
{
    ithread_set_name("ih264d_decode_picture_thread");
    while(1)
    {
        /*Complete all writes before processing next slice*/

        DEBUG_THREADS_PRINTF(" Entering decode slice\n");

        ih264d_decode_slice_thread(ps_dec);
        DEBUG_THREADS_PRINTF(" Exit  ih264d_decode_slice_thread \n");


        if(ps_dec->cur_dec_mb_num
                        > ps_dec->ps_cur_sps->u2_max_mb_addr)
        {
            /*Last slice in frame*/
            break;
        }
        else
        {
            ps_dec->ps_decode_cur_slice++;
            ps_dec->u2_cur_slice_num_dec_thread++;
        }

    }
    if(ps_dec->u4_output_present && (2 == ps_dec->u4_num_cores) &&
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

void ih264d_signal_decode_thread(dec_struct_t *ps_dec)
{
    if(ps_dec->u4_dec_thread_created == 1)
    {
        ithread_join(ps_dec->pv_dec_thread_handle, NULL);
        ps_dec->u4_dec_thread_created = 0;
    }
}
void ih264d_signal_bs_deblk_thread(dec_struct_t *ps_dec)
{
    if(ps_dec->u4_bs_deblk_thread_created)
    {
        ithread_join(ps_dec->pv_bs_deblk_thread_handle, NULL);
        ps_dec->u4_bs_deblk_thread_created = 0;
    }

}
