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
 * \file ih264d_process_bslice.c
 *
 * \brief
 *    Contains routines that decode B slice type
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
#include "ih264d_structs.h"
#include "ih264d_bitstrm.h"
#include "ih264d_parse_cavlc.h"
#include "ih264d_mb_utils.h"
#include "ih264d_mvpred.h"
#include "ih264d_inter_pred.h"
#include "ih264d_process_pslice.h"
#include "ih264d_error_handler.h"
#include "ih264d_tables.h"
#include "ih264d_parse_slice.h"
#include "ih264d_process_pslice.h"
#include "ih264d_process_bslice.h"
#include "ih264d_tables.h"
#include "ih264d_parse_islice.h"
#include "ih264d_mvpred.h"

void ih264d_init_cabac_contexts(UWORD8 u1_slice_type, dec_struct_t * ps_dec);
//UWORD32 g_hits = 0;
//UWORD32 g_miss = 0;
/*!
 **************************************************************************
 * \if Function name : ih264d_decode_spatial_direct \endif
 *
 * \brief
 *    Decodes spatial direct mode.
 *
 * \return
 *    None.
 *    Arunoday T
 **************************************************************************
 */
WORD32 ih264d_decode_spatial_direct(dec_struct_t * ps_dec,
                                    UWORD8 u1_wd_x,
                                    dec_mb_info_t * ps_cur_mb_info,
                                    UWORD8 u1_mb_num)
{
    mv_pred_t s_mv_pred, *ps_mv;
    UWORD8 u1_col_zero_flag, u1_sub_mb_num, u1_direct_zero_pred_flag = 0;
    UWORD8 u1_mbaff = ps_dec->ps_cur_slice->u1_mbaff_frame_flag;
    mv_pred_t *ps_mv_ntop_start;
    mv_pred_t *ps_mv_nmb_start = ps_dec->ps_mv_cur + (u1_mb_num << 4);
    UWORD8 partition_size, sub_partition, u1_mb_partw, u1_mb_parth;
    UWORD8 i;
    WORD8 i1_pred, i1_ref_frame0, i1_ref_frame1;
    struct pic_buffer_t *ps_ref_frame = NULL, *ps_col_pic, *ps_pic_buff0 = NULL,
                    *ps_pic_buff1 = NULL;

    UWORD8 u1_zero_pred_cond_f, u1_zero_pred_cond_b;
    WORD16 i2_def_mv[2], i2_spat_pred_mv[4], *pi2_final_mv0, *pi2_final_mv1;
    UWORD16 ui2_mask_fwd = 0, ui2_mask_bwd = 0, u2_mask = 0;
    UWORD32 *pui32_weight_ofsts = NULL;
    directmv_t s_mvdirect;
    UWORD8 u1_colz;
    UWORD8 u1_final_ref_idx = 0;
    const UWORD8 *pu1_mb_parth = (const UWORD8 *)gau1_ih264d_mb_parth;
    const UWORD8 *pu1_mb_partw = (const UWORD8 *)gau1_ih264d_mb_partw;
    const UWORD16 sub_mask_table[] =
        { 0x33, 0x3, 0x11, 0x1 };
    const UWORD16 mask_table[] =
        { 0xffff, /*16x16 NA */
          0xff, /* 16x8*/
          0x3333, /* 8x16*/
          0x33 };/* 8x8*/
    mv_pred_t s_temp_mv_pred;
    WORD32 ret = 0;

    /* CHANGED CODE */
    ps_mv_ntop_start = ps_dec->ps_mv_cur + (u1_mb_num << 4)
                    - (ps_dec->u2_frm_wd_in_mbs << (4 + u1_mbaff)) + 12;

    /* assign default values for MotionVector as zero */
    i2_def_mv[0] = 0;
    i2_def_mv[1] = 0;

    u1_direct_zero_pred_flag = ps_dec->pf_mvpred(ps_dec, ps_cur_mb_info, ps_mv_nmb_start,
                                              ps_mv_ntop_start, &s_mv_pred, 0, 4,
                                              0, 1, B_DIRECT_SPATIAL);

    i2_spat_pred_mv[0] = s_mv_pred.i2_mv[0];
    i2_spat_pred_mv[1] = s_mv_pred.i2_mv[1];
    i2_spat_pred_mv[2] = s_mv_pred.i2_mv[2];
    i2_spat_pred_mv[3] = s_mv_pred.i2_mv[3];

    i1_ref_frame0 = s_mv_pred.i1_ref_frame[0];
    i1_ref_frame1 = s_mv_pred.i1_ref_frame[1];

    i1_ref_frame0 = (i1_ref_frame0 < 0) ? -1 : i1_ref_frame0;
    i1_ref_frame1 = (i1_ref_frame1 < 0) ? -1 : i1_ref_frame1;

    i1_pred = 0;

    {
        WORD8 u1_ref_idx, u1_ref_idx1;
        UWORD32 uc_Idx, uc_Idx1;
        UWORD8 u1_scale_ref = (ps_dec->ps_cur_slice->u1_mbaff_frame_flag
                        && ps_cur_mb_info->u1_mb_field_decodingflag);
        u1_final_ref_idx = i1_ref_frame0;
        if(i1_ref_frame0 >= 0)
        {
            /* convert RefIdx if it is MbAff */
            u1_ref_idx = i1_ref_frame0;
            u1_ref_idx1 = i1_ref_frame0;
            if(u1_scale_ref)
            {
                u1_ref_idx1 = u1_ref_idx >> 1;
                if((u1_ref_idx & 0x01) != (1 - ps_cur_mb_info->u1_topmb))
                    u1_ref_idx1 += MAX_REF_BUFS;
            }
            /* If i1_ref_frame0 < 0 then refIdxCol is obtained from ps_pic_buff1 */
            ps_pic_buff0 = ps_dec->ps_ref_pic_buf_lx[0][u1_ref_idx1];
            ps_ref_frame = ps_pic_buff0;
            i1_pred = PRED_L0;
        }

        if(i1_ref_frame1 >= 0)
        {
            /* convert RefIdx if it is MbAff */
            u1_ref_idx = i1_ref_frame1;
            u1_ref_idx1 = i1_ref_frame1;
            if(u1_scale_ref)
            {
                u1_ref_idx1 = u1_ref_idx >> 1;
                if((u1_ref_idx & 0x01) != (1 - ps_cur_mb_info->u1_topmb))
                    u1_ref_idx1 += MAX_REF_BUFS;
            }
            ps_pic_buff1 = ps_dec->ps_ref_pic_buf_lx[1][u1_ref_idx1];
            i1_pred = i1_pred | PRED_L1;
        }
        if(i1_ref_frame0 < 0)
        {
            ps_ref_frame = ps_pic_buff1;
            u1_final_ref_idx = i1_ref_frame1;
        }

        u1_zero_pred_cond_f = (u1_direct_zero_pred_flag) || (i1_ref_frame0 < 0);
        u1_zero_pred_cond_b = (u1_direct_zero_pred_flag) || (i1_ref_frame1 < 0);

        if(ps_dec->ps_cur_pps->u1_wted_bipred_idc)
        {
            uc_Idx = ((i1_ref_frame0 < 1) ? 0 : i1_ref_frame0)
                            * ps_dec->ps_cur_slice->u1_num_ref_idx_lx_active[1];
            if(u1_scale_ref)
                uc_Idx >>= 1;
            uc_Idx1 = (i1_ref_frame1 < 0) ? 0 : i1_ref_frame1;
            uc_Idx += (u1_scale_ref) ? (uc_Idx1 >> 1) : uc_Idx1;
            pui32_weight_ofsts =
                            (UWORD32*)&ps_dec->pu4_wt_ofsts[2 * X3(uc_Idx)];

            if(i1_ref_frame0 < 0)
                pui32_weight_ofsts += 1;

            if(u1_scale_ref && (ps_dec->ps_cur_pps->u1_wted_bipred_idc == 2))
            {
                WORD16 i2_ref_idx;
                i2_ref_idx = MAX(i1_ref_frame0, 0);
                i2_ref_idx *= (ps_dec->ps_cur_slice->u1_num_ref_idx_lx_active[1]
                                << 1);
                i2_ref_idx += MAX(i1_ref_frame1, 0);
                if(!ps_cur_mb_info->u1_topmb)
                    i2_ref_idx +=
                                    (ps_dec->ps_cur_slice->u1_num_ref_idx_lx_active[0]
                                                    << 1)
                                                    * (ps_dec->ps_cur_slice->u1_num_ref_idx_lx_active[1]
                                                                    << 1);
                pui32_weight_ofsts = (UWORD32*)&ps_dec->pu4_mbaff_wt_mat[2
                                * X3(i2_ref_idx)];
            }
        }
    }

    s_temp_mv_pred.i1_ref_frame[0] = i1_ref_frame0;
    s_temp_mv_pred.i1_ref_frame[1] = i1_ref_frame1;
    s_temp_mv_pred.u1_col_ref_pic_idx = ps_ref_frame->u1_mv_buf_id;
    s_temp_mv_pred.u1_pic_type = ps_ref_frame->u1_pic_type;

    /**********************************************************************/
    /* Call the function which gets the number of partitions and          */
    /* partition info of colocated Mb                                     */
    /**********************************************************************/

    ps_dec->pf_parse_mvdirect(ps_dec, ps_dec->ps_col_pic, &s_mvdirect, u1_wd_x,
                           ps_dec->i4_submb_ofst, ps_cur_mb_info);
    ps_col_pic = ps_dec->ps_col_pic;
    if((s_mvdirect.u1_col_zeroflag_change == 0) || u1_direct_zero_pred_flag)
    {
        WORD16 i2_mv_x, i2_mv_y, i2_mvX1, i2_mvY1;
        /* Most probable case */
        u1_col_zero_flag = *(ps_col_pic->pu1_col_zero_flag
                        + s_mvdirect.i4_mv_indices[0]);
        u1_col_zero_flag = u1_col_zero_flag & 0x01;

        if(u1_zero_pred_cond_f || ((i1_ref_frame0 == 0) && (u1_col_zero_flag == 1)))
        {
            i2_mv_x = 0;
            i2_mv_y = 0;
        }
        else
        {
            i2_mv_x = i2_spat_pred_mv[0];
            i2_mv_y = i2_spat_pred_mv[1];

        }

        if(u1_zero_pred_cond_b || ((i1_ref_frame1 == 0) && (u1_col_zero_flag == 1)))
        {
            i2_mvX1 = 0;
            i2_mvY1 = 0;
        }
        else
        {
            i2_mvX1 = i2_spat_pred_mv[2];
            i2_mvY1 = i2_spat_pred_mv[3];
        }

        u1_sub_mb_num = ps_dec->u1_sub_mb_num;
        u1_mb_partw = (u1_wd_x >> 2);


        if(i1_ref_frame0 >= 0)
        {
            {
               pred_info_pkd_t *ps_pred_pkd;
               WORD16 i2_mv[2];
               WORD8 i1_ref_idx= 0;

               i2_mv[0] = i2_mv_x;
               i2_mv[1] = i2_mv_y;

               ps_pred_pkd = ps_dec->ps_pred_pkd + ps_dec->u4_pred_info_pkd_idx;
            ih264d_fill_pred_info(i2_mv,u1_mb_partw,u1_mb_partw,u1_sub_mb_num,i1_pred,
                            ps_pred_pkd,ps_pic_buff0->u1_pic_buf_id,i1_ref_idx,pui32_weight_ofsts,
                            ps_pic_buff0->u1_pic_type);
            ps_dec->u4_pred_info_pkd_idx++;
            ps_cur_mb_info->u1_num_pred_parts++;


            }

        }

        if(i1_ref_frame1 >= 0)
        {
            {
                pred_info_pkd_t *ps_pred_pkd;
               WORD16 i2_mv[2];
               WORD8 i1_ref_idx= 0;

               i2_mv[0] = i2_mvX1;
               i2_mv[1] = i2_mvY1;

               ps_pred_pkd = ps_dec->ps_pred_pkd + ps_dec->u4_pred_info_pkd_idx;
            ih264d_fill_pred_info(i2_mv,u1_mb_partw,u1_mb_partw,u1_sub_mb_num,i1_pred,
                            ps_pred_pkd,ps_pic_buff1->u1_pic_buf_id,i1_ref_idx,pui32_weight_ofsts,
                            ps_pic_buff1->u1_pic_type);
            ps_dec->u4_pred_info_pkd_idx++;
            ps_cur_mb_info->u1_num_pred_parts++;


            }
        }


        /* Replication optimisation */
        s_temp_mv_pred.i2_mv[0] = i2_mv_x;
        s_temp_mv_pred.i2_mv[1] = i2_mv_y;
        s_temp_mv_pred.i2_mv[2] = i2_mvX1;
        s_temp_mv_pred.i2_mv[3] = i2_mvY1;

        /* Calculating colocated zero information */
        {
            /*************************************/
            /* If(bit2 and bit3 set)             */
            /* then                              */
            /*  (bit0 and bit1) => submmbmode    */
            /*  (bit2 and bit3) => mbmode        */
            /* else                              */
            /*  (bit0 and bit1) => mbmode        */
            /*************************************/
            /*UWORD8 u1_packed_mb_sub_mb_mode = sub_partition ?
             (s_mvdirect.i1_partitionsize[0]) : ((s_mvdirect.i1_partitionsize[0]) << 2);*/
            UWORD8 u1_packed_mb_sub_mb_mode = (u1_mb_partw == 2) ? 0x03 : 0;

            if(i1_ref_frame0 < 0)
            {
                i2_mv_x = i2_mvX1;
                i2_mv_y = i2_mvY1;
            }

            /* Change from left shift 4 to 6 - Varun */
            u1_colz = (ps_cur_mb_info->u1_mb_field_decodingflag << 1)
                            | ((u1_final_ref_idx == 0) && (ABS(i2_mv_x) <= 1)
                                            && (ABS(i2_mv_y) <= 1));
            u1_colz |= (u1_packed_mb_sub_mb_mode << 6);
        }
        ps_mv = ps_mv_nmb_start + u1_sub_mb_num;
        ih264d_rep_mv_colz(ps_dec, &s_temp_mv_pred, ps_mv, u1_sub_mb_num, u1_colz,
                           u1_mb_partw, u1_mb_partw);
        if(u1_wd_x == MB_SIZE)
            ps_dec->u1_currB_type = 0;



        return OK;
    }
    /***************************************************************************/
    /* If present MB is 16x16 and the partition of colocated Mb is >= PRED_8x8 */
    /* i.e 8x8 or less than 8x8 partitions then set up DMA for (0,0) and       */
    /* spatially predicted motion vector and do the multiplexing after         */
    /* motion compensation                                                     */
    /***************************************************************************/


    if((u1_wd_x == MB_SIZE) && (s_mvdirect.i1_num_partitions > 2))
    {
        ps_cur_mb_info->u1_Mux = 1;
        if(i1_ref_frame0 >= 0)
        {

            {
                pred_info_pkd_t *ps_pred_pkd;
               WORD8 i1_ref_idx= 0;

               ps_pred_pkd = ps_dec->ps_pred_pkd + ps_dec->u4_pred_info_pkd_idx;
            ih264d_fill_pred_info(&(i2_spat_pred_mv[0]),4,4,0,i1_pred,
                            ps_pred_pkd,ps_pic_buff0->u1_pic_buf_id,i1_ref_idx,pui32_weight_ofsts,
                            ps_pic_buff0->u1_pic_type);
            ps_dec->u4_pred_info_pkd_idx++;
            ps_cur_mb_info->u1_num_pred_parts++;


            }

            /******    (0,0) Motion vectors DMA     *****/
            {
                pred_info_pkd_t *ps_pred_pkd;
               WORD16 i2_mv[2];
               WORD8 i1_ref_idx= 0;

               i2_mv[0] = 0;
               i2_mv[1] = 0;

               ps_pred_pkd = ps_dec->ps_pred_pkd + ps_dec->u4_pred_info_pkd_idx;
            ih264d_fill_pred_info(i2_mv,4,4,0,i1_pred,
                            ps_pred_pkd,ps_pic_buff0->u1_pic_buf_id,i1_ref_idx,pui32_weight_ofsts,
                            ps_pic_buff0->u1_pic_type);
            ps_dec->u4_pred_info_pkd_idx++;
            ps_cur_mb_info->u1_num_pred_parts++;


            }
        }
        if(i1_ref_frame1 >= 0)
        {
            {
                pred_info_pkd_t *ps_pred_pkd;
               WORD16 i2_mv[2];
               WORD8 i1_ref_idx= 0;

               ps_pred_pkd = ps_dec->ps_pred_pkd + ps_dec->u4_pred_info_pkd_idx;
            ih264d_fill_pred_info(&(i2_spat_pred_mv[2]),4,4,0,i1_pred,
                            ps_pred_pkd,ps_pic_buff1->u1_pic_buf_id,i1_ref_idx,pui32_weight_ofsts,
                            ps_pic_buff1->u1_pic_type);
            ps_dec->u4_pred_info_pkd_idx++;
            ps_cur_mb_info->u1_num_pred_parts++;


            }

            /******    (0,0) Motion vectors DMA     *****/

            {
                pred_info_pkd_t *ps_pred_pkd;
               WORD16 i2_mv[2];
               WORD8 i1_ref_idx= 0;

               i2_mv[0] = 0;
               i2_mv[1] = 0;

               ps_pred_pkd = ps_dec->ps_pred_pkd + ps_dec->u4_pred_info_pkd_idx;
            ih264d_fill_pred_info(i2_mv,4,4,0,i1_pred,
                            ps_pred_pkd,ps_pic_buff1->u1_pic_buf_id,i1_ref_idx,pui32_weight_ofsts,
                            ps_pic_buff1->u1_pic_type);
            ps_dec->u4_pred_info_pkd_idx++;
            ps_cur_mb_info->u1_num_pred_parts++;


            }
        }
    }

    /*u1_col = *(ps_col_pic->pu1_col_zero_flag + s_mvdirect.i4_mv_indices[0]);
     u1_col &= 1;
     u1_init = 0;*/

    for(i = 0; i < s_mvdirect.i1_num_partitions; i++)
    {
        partition_size = s_mvdirect.i1_partitionsize[i];
        u1_sub_mb_num = s_mvdirect.i1_submb_num[i];

        sub_partition = partition_size >> 2;
        partition_size &= 0x3;
        u1_mb_partw = pu1_mb_partw[partition_size];
        u1_mb_parth = pu1_mb_parth[partition_size];
        u2_mask = mask_table[partition_size];
        if(sub_partition != 0)
        {
            u1_mb_partw >>= 1;
            u1_mb_parth >>= 1;
            u2_mask = sub_mask_table[partition_size];
        }

        u1_col_zero_flag = *(ps_col_pic->pu1_col_zero_flag
                        + s_mvdirect.i4_mv_indices[i]);
        u1_col_zero_flag = u1_col_zero_flag & 0x01;

        /*if(u1_col != u1_col_zero_flag)
         u1_init = 1;*/

        if(u1_zero_pred_cond_f || ((i1_ref_frame0 == 0) && (u1_col_zero_flag == 1)))
        {
            pi2_final_mv0 = &i2_def_mv[0];
            ui2_mask_fwd |= (u2_mask << u1_sub_mb_num);
        }
        else
            pi2_final_mv0 = &i2_spat_pred_mv[0];

        if(u1_zero_pred_cond_b || ((i1_ref_frame1 == 0) && (u1_col_zero_flag == 1)))
        {
            pi2_final_mv1 = &i2_def_mv[0];
            ui2_mask_bwd |= (u2_mask << u1_sub_mb_num);
        }
        else
            pi2_final_mv1 = &i2_spat_pred_mv[2];

        if(ps_cur_mb_info->u1_Mux != 1)
        {
            /*u1_sub_mb_x = u1_sub_mb_num & 0x03;
             uc_sub_mb_y = (u1_sub_mb_num >> 2);*/
            if(i1_ref_frame0 >= 0)
            {

                {
                    pred_info_pkd_t *ps_pred_pkd;
                   WORD8 i1_ref_idx= 0;

                   ps_pred_pkd = ps_dec->ps_pred_pkd + ps_dec->u4_pred_info_pkd_idx;
                ih264d_fill_pred_info(pi2_final_mv0,u1_mb_partw,u1_mb_parth,u1_sub_mb_num,i1_pred,
                                ps_pred_pkd,ps_pic_buff0->u1_pic_buf_id,i1_ref_idx,pui32_weight_ofsts,
                                ps_pic_buff0->u1_pic_type);
                ps_dec->u4_pred_info_pkd_idx++;
                ps_cur_mb_info->u1_num_pred_parts++;


                }

            }

            if(i1_ref_frame1 >= 0)
            {
                {
                    pred_info_pkd_t *ps_pred_pkd;
                   WORD8 i1_ref_idx= 0;

                   ps_pred_pkd = ps_dec->ps_pred_pkd + ps_dec->u4_pred_info_pkd_idx;
                ih264d_fill_pred_info(pi2_final_mv1,u1_mb_partw,u1_mb_parth,u1_sub_mb_num,i1_pred,
                                ps_pred_pkd,ps_pic_buff1->u1_pic_buf_id,i1_ref_idx,pui32_weight_ofsts,
                                ps_pic_buff1->u1_pic_type);
                ps_dec->u4_pred_info_pkd_idx++;
                ps_cur_mb_info->u1_num_pred_parts++;


                }
            }
        }

        /* Replication optimisation */
        s_temp_mv_pred.i2_mv[0] = pi2_final_mv0[0];
        s_temp_mv_pred.i2_mv[1] = pi2_final_mv0[1];
        s_temp_mv_pred.i2_mv[2] = pi2_final_mv1[0];
        s_temp_mv_pred.i2_mv[3] = pi2_final_mv1[1];

        /* Calculating colocated zero information */
        {
            WORD16 i2_mv_x = 0, i2_mv_y = 0;
            /*************************************/
            /* If(bit2 and bit3 set)             */
            /* then                              */
            /*  (bit0 and bit1) => submmbmode    */
            /*  (bit2 and bit3) => mbmode        */
            /* else                              */
            /*  (bit0 and bit1) => mbmode        */
            /*************************************/
            UWORD8 u1_packed_mb_sub_mb_mode =
                            sub_partition ? (s_mvdirect.i1_partitionsize[i]) : ((s_mvdirect.i1_partitionsize[i])
                                                            << 2);

            if(i1_ref_frame0 >= 0)
            {
                i2_mv_x = pi2_final_mv0[0];
                i2_mv_y = pi2_final_mv0[1];
            }
            else
            {
                i2_mv_x = pi2_final_mv1[0];
                i2_mv_y = pi2_final_mv1[1];
            }

            u1_colz = (ps_cur_mb_info->u1_mb_field_decodingflag << 1)
                            | ((u1_final_ref_idx == 0) && (ABS(i2_mv_x) <= 1)
                                            && (ABS(i2_mv_y) <= 1));
            u1_colz |= (u1_packed_mb_sub_mb_mode << 4);
        }
        ps_mv = ps_mv_nmb_start + u1_sub_mb_num;
        ih264d_rep_mv_colz(ps_dec, &s_temp_mv_pred, ps_mv, u1_sub_mb_num, u1_colz,
                           u1_mb_parth, u1_mb_partw);
    }
    i = 0;
    if(i1_ref_frame0 >= 0)
        ps_cur_mb_info->u2_mask[i++] = ui2_mask_fwd;
    if(i1_ref_frame1 >= 0)
        ps_cur_mb_info->u2_mask[i] = ui2_mask_bwd;

    /*if(u1_init)
     H264_DEC_DEBUG_PRINT("hit\n");
     else
     H264_DEC_DEBUG_PRINT("miss\n");*/

    return OK;
}

/*!
 **************************************************************************
 * \if Function name : ih264d_decode_temporal_direct \endif
 *
 * \brief
 *    Decodes temporal direct mode.
 *
 * \return
 *    None.
 *
 **************************************************************************
 */
WORD32 ih264d_decode_temporal_direct(dec_struct_t * ps_dec,
                                     UWORD8 u1_wd_x,
                                     dec_mb_info_t * ps_cur_mb_info,
                                     UWORD8 u1_mb_num)
{
    struct pic_buffer_t *ps_pic_buff0, *ps_pic_buff1, *ps_col_pic;
    mv_pred_t *ps_mv, s_temp_mv_pred;
    UWORD8 u1_sub_mb_num;
    UWORD8 u1_mbaff = ps_dec->ps_cur_slice->u1_mbaff_frame_flag;
    WORD16 i2_mv_x0, i2_mv_y0, i2_mv_x1, i2_mv_y1;
    UWORD8 u1_mb_partw, u1_mb_parth;
    UWORD8 i, partition_size, sub_partition;
    UWORD32 *pui32_weight_ofsts = NULL;
    directmv_t s_mvdirect;
    const UWORD8 *pu1_mb_parth = (const UWORD8 *)gau1_ih264d_mb_parth;
    const UWORD8 *pu1_mb_partw = (const UWORD8 *)gau1_ih264d_mb_partw;
    WORD8 c_refFrm0, c_refFrm1;
    UWORD8 u1_ref_idx0, u1_is_cur_mb_fld;
    WORD32 pic0_poc, pic1_poc, cur_poc;
    WORD32 ret = 0;

    u1_is_cur_mb_fld = ps_cur_mb_info->u1_mb_field_decodingflag;
    ps_pic_buff1 = ps_dec->ps_ref_pic_buf_lx[1][0];

    /**********************************************************************/
    /* Call the function which gets the number of partitions and          */
    /* partition info of colocated Mb                                     */
    /**********************************************************************/
    ps_dec->pf_parse_mvdirect(ps_dec, ps_dec->ps_col_pic, &s_mvdirect, u1_wd_x,
                           ps_dec->i4_submb_ofst, ps_cur_mb_info);
    ps_col_pic = ps_dec->ps_col_pic;

    for(i = 0; i < s_mvdirect.i1_num_partitions; i++)
    {
        UWORD8 u1_colz;
        partition_size = s_mvdirect.i1_partitionsize[i];
        u1_sub_mb_num = s_mvdirect.i1_submb_num[i];
        ps_mv = ps_col_pic->ps_mv + s_mvdirect.i4_mv_indices[i];

        /* This should be removed to catch unitialized memory read */
        u1_ref_idx0 = 0;

        sub_partition = partition_size >> 2;
        partition_size &= 0x3;
        u1_mb_partw = pu1_mb_partw[partition_size];
        u1_mb_parth = pu1_mb_parth[partition_size];
        if(sub_partition != 0)
        {
            u1_mb_partw >>= 1;
            u1_mb_parth >>= 1;
        }
        c_refFrm0 = ps_mv->i1_ref_frame[0];
        c_refFrm1 = ps_mv->i1_ref_frame[1];

        if((c_refFrm0 == -1) && (c_refFrm1 == -1))
        {
            u1_ref_idx0 = 0;
            ps_pic_buff0 = ps_dec->ps_ref_pic_buf_lx[0][0];
            if(u1_mbaff && u1_is_cur_mb_fld)
            {
                if(ps_cur_mb_info->u1_topmb)
                {
                    pic0_poc = ps_pic_buff0->i4_top_field_order_cnt;
                    pic1_poc = ps_pic_buff1->i4_top_field_order_cnt;
                    cur_poc = ps_dec->ps_cur_pic->i4_top_field_order_cnt;
                }
                else
                {
                    pic1_poc = ps_pic_buff1->i4_bottom_field_order_cnt;
                    cur_poc = ps_dec->ps_cur_pic->i4_bottom_field_order_cnt;
                    ps_pic_buff1 = ps_dec->ps_ref_pic_buf_lx[1][MAX_REF_BUFS];
                    pic0_poc = ps_pic_buff0->i4_bottom_field_order_cnt;
                    ps_pic_buff0 = ps_dec->ps_ref_pic_buf_lx[0][MAX_REF_BUFS];
                }
            }
            else
            {
                pic0_poc = ps_pic_buff0->i4_avg_poc;
                pic1_poc = ps_pic_buff1->i4_avg_poc;
                cur_poc = ps_dec->ps_cur_pic->i4_poc;
            }
        }
        else
        {
            UWORD8 uc_i, u1_num_frw_ref_pics;
            UWORD8 buf_id, u1_pic_type;
            buf_id = ps_mv->u1_col_ref_pic_idx;
            u1_pic_type = ps_mv->u1_pic_type;
            if(ps_dec->ps_cur_slice->u1_field_pic_flag)
            {
                if(s_mvdirect.u1_vert_mv_scale == FRM_TO_FLD)
                {
                    u1_pic_type = TOP_FLD;
                    if(ps_dec->ps_cur_slice->u1_bottom_field_flag)
                        u1_pic_type = BOT_FLD;
                }
            }
            u1_num_frw_ref_pics =
                            ps_dec->ps_cur_slice->u1_num_ref_idx_lx_active[0];

            for(uc_i = 0; uc_i < u1_num_frw_ref_pics; uc_i++)
            {
                if(ps_dec->ps_cur_slice->u1_field_pic_flag)
                {
                    if(ps_dec->ps_ref_pic_buf_lx[0][uc_i]->u1_mv_buf_id == buf_id)
                    {
                        if(ps_dec->ps_ref_pic_buf_lx[0][uc_i]->u1_pic_type
                                        == u1_pic_type)
                        {
                            u1_ref_idx0 = uc_i;
                            break;
                        }
                    }
                }
                else
                {
                    if(ps_dec->ps_ref_pic_buf_lx[0][uc_i]->u1_mv_buf_id == buf_id)
                    {
                        u1_ref_idx0 = uc_i;
                        break;
                    }
                }
            }

            ps_pic_buff0 = ps_dec->ps_ref_pic_buf_lx[0][u1_ref_idx0];
            ps_pic_buff1 = ps_dec->ps_ref_pic_buf_lx[1][0];

            if(u1_mbaff && u1_is_cur_mb_fld)
            {
                pic0_poc = ps_pic_buff0->i4_top_field_order_cnt;
                u1_ref_idx0 <<= 1;
                if(s_mvdirect.u1_vert_mv_scale == ONE_TO_ONE)
                {
                    if(u1_pic_type == BOT_FLD)
                    {
                        pic0_poc = ps_pic_buff0->i4_bottom_field_order_cnt;
                        ps_pic_buff0 = ps_dec->ps_ref_pic_buf_lx[0][(u1_ref_idx0
                                        >> 1) + MAX_REF_BUFS];
                        if(ps_cur_mb_info->u1_topmb)
                            u1_ref_idx0++;
                    }
                    else
                    {
                        if(1 - ps_cur_mb_info->u1_topmb)
                            u1_ref_idx0++;
                    }
                }
                if(s_mvdirect.u1_vert_mv_scale == FRM_TO_FLD)
                {
                    if(1 - ps_cur_mb_info->u1_topmb)
                    {
                        pic0_poc = ps_pic_buff0->i4_bottom_field_order_cnt;
                        ps_pic_buff0 = ps_dec->ps_ref_pic_buf_lx[0][(u1_ref_idx0
                                        >> 1) + MAX_REF_BUFS];
                    }
                }
                if(ps_cur_mb_info->u1_topmb)
                {
                    pic1_poc = ps_pic_buff1->i4_top_field_order_cnt;
                    cur_poc = ps_dec->ps_cur_pic->i4_top_field_order_cnt;
                }
                else
                {
                    pic1_poc = ps_pic_buff1->i4_bottom_field_order_cnt;
                    cur_poc = ps_dec->ps_cur_pic->i4_bottom_field_order_cnt;
                    ps_pic_buff1 = ps_dec->ps_ref_pic_buf_lx[1][MAX_REF_BUFS];
                }
            }
            else
            {
                pic0_poc = ps_pic_buff0->i4_avg_poc;
                pic1_poc = ps_pic_buff1->i4_avg_poc;
                cur_poc = ps_dec->ps_cur_pic->i4_poc;
            }
        }
        {
            WORD16 i16_td;
            WORD64 diff;
            if(c_refFrm0 >= 0)
            {
                i2_mv_x0 = ps_mv->i2_mv[0];
                i2_mv_y0 = ps_mv->i2_mv[1];
            }
            else if(c_refFrm1 >= 0)
            {
                i2_mv_x0 = ps_mv->i2_mv[2];
                i2_mv_y0 = ps_mv->i2_mv[3];
            }
            else
            {
                i2_mv_x0 = 0;
                i2_mv_y0 = 0;
            }
            /* If FRM_TO_FLD or FLD_TO_FRM scale the "y" component of the colocated Mv*/
            if(s_mvdirect.u1_vert_mv_scale == FRM_TO_FLD)
            {
                i2_mv_y0 /= 2;
            }
            else if(s_mvdirect.u1_vert_mv_scale == FLD_TO_FRM)
            {
                i2_mv_y0 *= 2;
            }

            diff = (WORD64)pic1_poc - pic0_poc;
            i16_td = CLIP_S8(diff);
            if((ps_pic_buff0->u1_is_short == 0) || (i16_td == 0))
            {
                i2_mv_x1 = 0;
                i2_mv_y1 = 0;
            }
            else
            {
                WORD16 i2_tb, i2_tx, i2_dist_scale_factor, i2_temp;

                diff = (WORD64)cur_poc - pic0_poc;
                i2_tb = CLIP_S8(diff);

                i2_tx = (16384 + ABS(SIGN_POW2_DIV(i16_td, 1))) / i16_td;
                i2_dist_scale_factor = CLIP_S11(
                                            (((i2_tb * i2_tx) + 32) >> 6));
                i2_temp = (i2_mv_x0 * i2_dist_scale_factor + 128) >> 8;
                i2_mv_x1 = i2_temp - i2_mv_x0;
                i2_mv_x0 = i2_temp;

                i2_temp = (i2_mv_y0 * i2_dist_scale_factor + 128) >> 8;
                i2_mv_y1 = i2_temp - i2_mv_y0;
                i2_mv_y0 = i2_temp;
            }
            {
                mv_pred_t *ps_mv;

                /*u1_sub_mb_x = u1_sub_mb_num & 0x03;
                 uc_sub_mb_y = u1_sub_mb_num >> 2;*/
                if(ps_dec->ps_cur_pps->u1_wted_bipred_idc)
                {
                    UWORD8 u1_idx =
                                    u1_ref_idx0
                                                    * ps_dec->ps_cur_slice->u1_num_ref_idx_lx_active[1];
                    UWORD8 u1_scale_ref = u1_mbaff && u1_is_cur_mb_fld;
                    if(u1_scale_ref)
                        u1_idx >>= 1;
                    pui32_weight_ofsts = (UWORD32*)&ps_dec->pu4_wt_ofsts[2
                                    * X3(u1_idx)];
                    if(u1_scale_ref
                                    && (ps_dec->ps_cur_pps->u1_wted_bipred_idc
                                                    == 2))
                    {
                        WORD16 i2_ref_idx;
                        i2_ref_idx = u1_ref_idx0;
                        i2_ref_idx *=
                                        (ps_dec->ps_cur_slice->u1_num_ref_idx_lx_active[1]
                                                        << 1);
                        if(!ps_cur_mb_info->u1_topmb)
                            i2_ref_idx +=
                                            (ps_dec->ps_cur_slice->u1_num_ref_idx_lx_active[0]
                                                            << 1)
                                                            * (ps_dec->ps_cur_slice->u1_num_ref_idx_lx_active[1]
                                                                            << 1);
                        pui32_weight_ofsts =
                                        (UWORD32*)&ps_dec->pu4_mbaff_wt_mat[2
                                                        * X3(i2_ref_idx)];
                    }
                }
                {
                    pred_info_pkd_t *ps_pred_pkd;
                   WORD16 i2_mv[2];
                   WORD8 i1_ref_idx= 0;

                   i2_mv[0] = i2_mv_x0;
                   i2_mv[1] = i2_mv_y0;

                   ps_pred_pkd = ps_dec->ps_pred_pkd + ps_dec->u4_pred_info_pkd_idx;
                ih264d_fill_pred_info(i2_mv,u1_mb_partw,u1_mb_parth,u1_sub_mb_num,PRED_L0 | PRED_L1,
                                ps_pred_pkd,ps_pic_buff0->u1_pic_buf_id,i1_ref_idx,pui32_weight_ofsts,
                                ps_pic_buff0->u1_pic_type);
                ps_dec->u4_pred_info_pkd_idx++;
                ps_cur_mb_info->u1_num_pred_parts++;


                }
                {
                   pred_info_pkd_t *ps_pred_pkd;
                   WORD16 i2_mv[2];
                   WORD8 i1_ref_idx= 0;

                   i2_mv[0] = i2_mv_x1;
                   i2_mv[1] = i2_mv_y1;

                   ps_pred_pkd = ps_dec->ps_pred_pkd + ps_dec->u4_pred_info_pkd_idx;
                ih264d_fill_pred_info(i2_mv,u1_mb_partw,u1_mb_parth,u1_sub_mb_num,PRED_L0 | PRED_L1,
                                ps_pred_pkd,ps_pic_buff1->u1_pic_buf_id,i1_ref_idx,pui32_weight_ofsts,
                                ps_pic_buff1->u1_pic_type);
                ps_dec->u4_pred_info_pkd_idx++;
                ps_cur_mb_info->u1_num_pred_parts++;


                }

                /* Replication optimisation */
                s_temp_mv_pred.i2_mv[0] = i2_mv_x0;
                s_temp_mv_pred.i2_mv[1] = i2_mv_y0;
                s_temp_mv_pred.i2_mv[2] = i2_mv_x1;
                s_temp_mv_pred.i2_mv[3] = i2_mv_y1;
                s_temp_mv_pred.i1_ref_frame[0] = u1_ref_idx0;
                s_temp_mv_pred.i1_ref_frame[1] = 0;
                s_temp_mv_pred.u1_col_ref_pic_idx = ps_pic_buff0->u1_mv_buf_id;
                s_temp_mv_pred.u1_pic_type = ps_pic_buff0->u1_pic_type;
                ps_mv = ps_dec->ps_mv_cur + (u1_mb_num << 4) + u1_sub_mb_num;

                {
                    WORD16 i2_mv_x = 0, i2_mv_y = 0;
                    UWORD8 u1_packed_mb_sub_mb_mode =
                                    sub_partition ? (s_mvdirect.i1_partitionsize[i]) : ((s_mvdirect.i1_partitionsize[i])
                                                                    << 2);

                    if(c_refFrm0 >= 0)
                    {
                        i2_mv_x = i2_mv_x0;
                        i2_mv_y = i2_mv_y0;
                    }
                    else
                    {
                        i2_mv_x = i2_mv_x1;
                        i2_mv_y = i2_mv_y1;
                    }

                    u1_colz =
                                    (ps_cur_mb_info->u1_mb_field_decodingflag << 1)
                                                    | ((u1_ref_idx0 == 0)
                                                                    && (ABS(i2_mv_x)
                                                                                    <= 1)
                                                                    && (ABS(i2_mv_y)
                                                                                    <= 1));
                    u1_colz |= (u1_packed_mb_sub_mb_mode << 4);
                }
                ih264d_rep_mv_colz(ps_dec, &s_temp_mv_pred, ps_mv, u1_sub_mb_num,
                                   u1_colz, u1_mb_parth, u1_mb_partw);
            }
        }
    }
    /* return value set to UWORD8 to make it homogeneous  */
    /* with decodespatialdirect                           */
    return OK;
}

void ih264d_convert_frm_to_fld_list(struct pic_buffer_t *ps_ref_pic_buf_lx,
                                    UWORD8 *pu1_L0,
                                    dec_struct_t *ps_dec,
                                    UWORD8 u1_num_short_term_bufs)
{
    UWORD8 uc_count = *pu1_L0, i, uc_l1, uc_lx, j;
    struct pic_buffer_t *ps_ref_lx[2], *ps_ref_pic_lx;
    UWORD8 u1_bottom_field_flag;
    dec_slice_params_t *ps_cur_slice;
    UWORD8 u1_ref[2], u1_fld[2], u1_same_fld, u1_op_fld;
    UWORD32 ui_half_num_of_sub_mbs;

    uc_l1 = 0;
    uc_lx = 0;
    ps_cur_slice = ps_dec->ps_cur_slice;
    ps_ref_pic_lx = ps_ref_pic_buf_lx - MAX_REF_BUFS;
    ps_ref_lx[0] = ps_ref_pic_buf_lx;
    ps_ref_lx[1] = ps_ref_pic_buf_lx;
    u1_bottom_field_flag = ps_cur_slice->u1_bottom_field_flag;
    ui_half_num_of_sub_mbs = ((ps_dec->u2_pic_ht * ps_dec->u2_pic_wd) >> 5);
    if(u1_bottom_field_flag)
    {
        u1_ref[0] = BOT_REF;
        u1_ref[1] = TOP_REF;
        u1_fld[0] = BOT_FLD;
        u1_fld[1] = TOP_FLD;
        u1_same_fld = BOT_FLD;
        u1_op_fld = TOP_FLD;
    }
    else
    {
        u1_ref[0] = TOP_REF;
        u1_ref[1] = BOT_REF;
        u1_fld[0] = TOP_FLD;
        u1_fld[1] = BOT_FLD;
        u1_same_fld = TOP_FLD;
        u1_op_fld = BOT_FLD;
    }

    /* Create the field list starting with all the short term     */
    /* frames followed by all the long term frames. No long term  */
    /* reference field should have a list idx less than a short   */
    /* term reference field during initiailization.               */

    for(j = 0; j < 2; j++)
    {
        i = ((j == 0) ? 0 : u1_num_short_term_bufs);
        uc_count = ((j == 0) ? u1_num_short_term_bufs : *pu1_L0);
        for(; i < uc_count; i++, ps_ref_lx[0]++)
        {
            /* Search field of same parity in Frame list */
            if((ps_ref_lx[0]->u1_pic_type & u1_ref[0])) // || ((ps_ref_lx[0]->u1_picturetype & 0x3) == 0))
            {
                /* Insert PIC of same parity in RefPicList */
                ih264d_insert_pic_in_ref_pic_listx(ps_ref_pic_lx, ps_ref_lx[0]);
                ps_ref_pic_lx->i4_pic_num = (ps_ref_pic_lx->i4_pic_num * 2 + 1);
                ps_ref_pic_lx->u1_long_term_pic_num =
                                (ps_ref_pic_lx->u1_long_term_frm_idx * 2 + 1);
                ps_ref_pic_lx->u1_pic_type = u1_same_fld;
                if(u1_fld[0] & BOT_FLD)
                {
                    ps_ref_pic_lx->u1_pic_type = BOT_FLD;
                    ps_ref_pic_lx->pu1_buf1 += ps_ref_pic_lx->u2_frm_wd_y;
                    ps_ref_pic_lx->pu1_buf2 += ps_ref_pic_lx->u2_frm_wd_uv;
                    ps_ref_pic_lx->pu1_buf3 += ps_ref_pic_lx->u2_frm_wd_uv;
                    if(ps_ref_pic_lx->u1_picturetype & 0x3)
                    {
                        ps_ref_pic_lx->pu1_col_zero_flag += ui_half_num_of_sub_mbs;
                        ps_ref_pic_lx->ps_mv += ui_half_num_of_sub_mbs;
                    }
                    ps_ref_pic_lx->i4_poc =
                                    ps_ref_pic_lx->i4_bottom_field_order_cnt;
                    ps_ref_pic_lx->i4_avg_poc =
                                    ps_ref_pic_lx->i4_bottom_field_order_cnt;
                }
                else
                {
                    ps_ref_pic_lx->u1_pic_type = TOP_FLD;
                    ps_ref_pic_lx->i4_poc = ps_ref_pic_lx->i4_top_field_order_cnt;
                    ps_ref_pic_lx->i4_avg_poc =
                                    ps_ref_pic_lx->i4_top_field_order_cnt;
                }

                ps_ref_pic_lx++;
                uc_lx++;
                /* Find field of opposite parity */
                if(uc_l1 < uc_count && ps_ref_lx[1])
                {
                    while(!(ps_ref_lx[1]->u1_pic_type & u1_ref[1]))
                    {
                        ps_ref_lx[1]++;
                        uc_l1++;
                        if(uc_l1 >= uc_count)
                            ps_ref_lx[1] = 0;
                        if(!ps_ref_lx[1])
                            break;
                    }

                    if(ps_ref_lx[1])
                    {
                        uc_l1++;
                        ih264d_insert_pic_in_ref_pic_listx(ps_ref_pic_lx,
                                                           ps_ref_lx[1]);
                        ps_ref_pic_lx->u1_pic_type = u1_op_fld;
                        ps_ref_pic_lx->i4_pic_num = (ps_ref_pic_lx->i4_pic_num * 2);
                        ps_ref_pic_lx->u1_long_term_pic_num =
                                        (ps_ref_pic_lx->u1_long_term_frm_idx * 2);
                        if(u1_fld[1] & BOT_FLD)
                        {
                            ps_ref_pic_lx->u1_pic_type = BOT_FLD;
                            ps_ref_pic_lx->pu1_buf1 += ps_ref_pic_lx->u2_frm_wd_y;
                            ps_ref_pic_lx->pu1_buf2 += ps_ref_pic_lx->u2_frm_wd_uv;
                            ps_ref_pic_lx->pu1_buf3 += ps_ref_pic_lx->u2_frm_wd_uv;
                            if(ps_ref_pic_lx->u1_picturetype & 0x3)
                            {
                                ps_ref_pic_lx->pu1_col_zero_flag +=
                                                ui_half_num_of_sub_mbs;
                                ps_ref_pic_lx->ps_mv += ui_half_num_of_sub_mbs;
                            }
                            ps_ref_pic_lx->i4_poc =
                                            ps_ref_pic_lx->i4_bottom_field_order_cnt;
                            ps_ref_pic_lx->i4_avg_poc =
                                            ps_ref_pic_lx->i4_bottom_field_order_cnt;
                        }
                        else
                        {
                            ps_ref_pic_lx->u1_pic_type = TOP_FLD;
                            ps_ref_pic_lx->i4_poc =
                                            ps_ref_pic_lx->i4_top_field_order_cnt;
                            ps_ref_pic_lx->i4_avg_poc =
                                            ps_ref_pic_lx->i4_top_field_order_cnt;
                        }
                        ps_ref_pic_lx++;
                        uc_lx++;
                        ps_ref_lx[1]++;
                    }
                }
            }
        }

        /* Same parity fields are over, now insert left over opposite parity fields */
        /** Added  if(ps_ref_lx[1]) for error checks */
        if(ps_ref_lx[1])
        {
            for(; uc_l1 < uc_count; uc_l1++)
            {
                if(ps_ref_lx[1]->u1_pic_type & u1_ref[1])
                {
                    /* Insert PIC of opposite parity in RefPicList */
                    ih264d_insert_pic_in_ref_pic_listx(ps_ref_pic_lx,
                                                       ps_ref_lx[1]);
                    ps_ref_pic_lx->u1_pic_type = u1_op_fld;
                    ps_ref_pic_lx->i4_pic_num = (ps_ref_pic_lx->i4_pic_num * 2);
                    ps_ref_pic_lx->u1_long_term_pic_num =
                                    (ps_ref_pic_lx->u1_long_term_frm_idx * 2);
                    if(u1_op_fld == BOT_FLD)
                    {
                        ps_ref_pic_lx->u1_pic_type = BOT_FLD;
                        ps_ref_pic_lx->pu1_buf1 += ps_ref_pic_lx->u2_frm_wd_y;
                        ps_ref_pic_lx->pu1_buf2 += ps_ref_pic_lx->u2_frm_wd_uv;
                        ps_ref_pic_lx->pu1_buf3 += ps_ref_pic_lx->u2_frm_wd_uv;
                        if(ps_ref_pic_lx->u1_picturetype & 0x3)
                        {
                            ps_ref_pic_lx->pu1_col_zero_flag +=
                                            ui_half_num_of_sub_mbs;
                            ps_ref_pic_lx->ps_mv += ui_half_num_of_sub_mbs;
                        }
                        ps_ref_pic_lx->i4_poc =
                                        ps_ref_pic_lx->i4_bottom_field_order_cnt;
                        ps_ref_pic_lx->i4_avg_poc =
                                        ps_ref_pic_lx->i4_bottom_field_order_cnt;
                    }
                    else
                    {
                        ps_ref_pic_lx->i4_poc =
                                        ps_ref_pic_lx->i4_top_field_order_cnt;
                        ps_ref_pic_lx->i4_avg_poc =
                                        ps_ref_pic_lx->i4_top_field_order_cnt;
                    }
                    ps_ref_pic_lx++;
                    uc_lx++;
                    ps_ref_lx[1]++;
                }
            }
        }
    }
    *pu1_L0 = uc_lx;
}

void ih264d_convert_frm_mbaff_list(dec_struct_t *ps_dec)
{
    struct pic_buffer_t **ps_ref_pic_lx;
    UWORD8 u1_max_ref_idx, idx;
    UWORD16 u2_frm_wd_y, u2_frm_wd_uv;
    struct pic_buffer_t **ps_ref_pic_buf_lx;
    UWORD32 u4_half_num_of_sub_mbs = ((ps_dec->u2_pic_ht * ps_dec->u2_pic_wd) >> 5);

    ps_ref_pic_buf_lx = ps_dec->ps_ref_pic_buf_lx[0];
    ps_ref_pic_lx = ps_dec->ps_ref_pic_buf_lx[0];
    u1_max_ref_idx = ps_dec->ps_cur_slice->u1_num_ref_idx_lx_active[0];
    for(idx = 0; idx < u1_max_ref_idx; idx++)
    {
        ps_ref_pic_lx[idx]->u1_pic_type = TOP_FLD;
        ps_ref_pic_lx[idx]->i4_poc = ps_ref_pic_lx[idx]->i4_top_field_order_cnt;

    }
    u2_frm_wd_y = ps_dec->u2_frm_wd_y;
    u2_frm_wd_uv = ps_dec->u2_frm_wd_uv;

    for(idx = 0; idx < u1_max_ref_idx; idx++)
    {
        *ps_ref_pic_lx[idx + MAX_REF_BUFS] = *ps_ref_pic_buf_lx[idx];
        ps_ref_pic_lx[idx + MAX_REF_BUFS]->pu1_buf1 =
                        ps_ref_pic_buf_lx[idx]->pu1_buf1 + u2_frm_wd_y;
        ps_ref_pic_lx[idx + MAX_REF_BUFS]->pu1_buf2 =
                        ps_ref_pic_buf_lx[idx]->pu1_buf2 + u2_frm_wd_uv;
        ps_ref_pic_lx[idx + MAX_REF_BUFS]->pu1_buf3 =
                        ps_ref_pic_buf_lx[idx]->pu1_buf3 + u2_frm_wd_uv;

        ps_ref_pic_lx[idx + MAX_REF_BUFS]->u1_pic_type = BOT_FLD;
        ps_ref_pic_lx[idx + MAX_REF_BUFS]->i4_poc =
                        ps_ref_pic_buf_lx[idx]->i4_bottom_field_order_cnt;
        if(ps_ref_pic_buf_lx[idx]->u1_picturetype & 0x3)
        {
            ps_ref_pic_lx[idx + MAX_REF_BUFS]->pu1_col_zero_flag =
                            ps_ref_pic_buf_lx[idx]->pu1_col_zero_flag
                                            + u4_half_num_of_sub_mbs;
            ps_ref_pic_lx[idx + MAX_REF_BUFS]->ps_mv =
                            ps_ref_pic_buf_lx[idx]->ps_mv + u4_half_num_of_sub_mbs;
        }
    }

    if(ps_dec->u1_B)
    {
        ps_ref_pic_buf_lx = ps_dec->ps_ref_pic_buf_lx[1];
        ps_ref_pic_lx = ps_dec->ps_ref_pic_buf_lx[1];
        u1_max_ref_idx = ps_dec->ps_cur_slice->u1_num_ref_idx_lx_active[1];
        for(idx = 0; idx < u1_max_ref_idx; idx++)
        {
            ps_ref_pic_lx[idx]->u1_pic_type = TOP_FLD;
            ps_ref_pic_lx[idx]->i4_poc = ps_ref_pic_lx[idx]->i4_top_field_order_cnt;

        }

        for(idx = 0; idx < u1_max_ref_idx; idx++)
        {
            *ps_ref_pic_lx[idx + MAX_REF_BUFS] = *ps_ref_pic_buf_lx[idx];
            ps_ref_pic_lx[idx + MAX_REF_BUFS]->pu1_buf1 =
                            ps_ref_pic_buf_lx[idx]->pu1_buf1 + u2_frm_wd_y;
            ps_ref_pic_lx[idx + MAX_REF_BUFS]->pu1_buf2 =
                            ps_ref_pic_buf_lx[idx]->pu1_buf2 + u2_frm_wd_uv;
            ps_ref_pic_lx[idx + MAX_REF_BUFS]->pu1_buf3 =
                            ps_ref_pic_buf_lx[idx]->pu1_buf3 + u2_frm_wd_uv;
            ps_ref_pic_lx[idx + MAX_REF_BUFS]->u1_pic_type = BOT_FLD;
            ps_ref_pic_lx[idx + MAX_REF_BUFS]->i4_poc =
                            ps_ref_pic_buf_lx[idx]->i4_bottom_field_order_cnt;

            if(ps_ref_pic_buf_lx[idx]->u1_picturetype & 0x3)
            {
                ps_ref_pic_lx[idx + MAX_REF_BUFS]->pu1_col_zero_flag =
                                ps_ref_pic_buf_lx[idx]->pu1_col_zero_flag
                                                + u4_half_num_of_sub_mbs;
                ps_ref_pic_lx[idx + MAX_REF_BUFS]->ps_mv =
                                ps_ref_pic_buf_lx[idx]->ps_mv
                                                + u4_half_num_of_sub_mbs;
            }
        }
    }
}
static int poc_compare(const void *pv_pic1, const void *pv_pic2)
{
    struct pic_buffer_t *ps_pic1 = *(struct pic_buffer_t **) pv_pic1;
    struct pic_buffer_t *ps_pic2 = *(struct pic_buffer_t **) pv_pic2;
    if (ps_pic1->i4_poc < ps_pic2->i4_poc)
    {
        return -1;
    }
    else if (ps_pic1->i4_poc > ps_pic2->i4_poc)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}
/*!
 **************************************************************************
 * \if Function name : ih264d_init_ref_idx_lx_b \endif
 *
 * \brief
 *    Initializes forward and backward refernce lists for B slice decoding.
 *
 *
 * \return
 *    0 on Success and Error code otherwise
 **************************************************************************
 */
void ih264d_init_ref_idx_lx_b(dec_struct_t *ps_dec)
{
    struct pic_buffer_t *ps_ref_pic_buf_lx;
    dpb_manager_t *ps_dpb_mgr;
    struct dpb_info_t *ps_next_dpb;
    WORD32 i_cur_poc, i_max_st_poc, i_min_st_poc, i_ref_poc, i_temp_poc;
    WORD8 i, j;
    UWORD8 u1_max_lt_index, u1_min_lt_index;
    UWORD32 u4_lt_index;
    WORD32 i_cur_idx;
    UWORD8 u1_field_pic_flag;
    dec_slice_params_t *ps_cur_slice;
    UWORD8 u1_L0, u1_L1;
    UWORD8 u1_num_short_term_bufs;
    UWORD8 u1_max_ref_idx_l0, u1_max_ref_idx_l1;
    struct pic_buffer_t *aps_st_pic_bufs[2 * MAX_REF_BUFS] = {NULL};
    ps_cur_slice = ps_dec->ps_cur_slice;
    u1_field_pic_flag = ps_cur_slice->u1_field_pic_flag;
    u1_max_ref_idx_l0 = ps_cur_slice->u1_num_ref_idx_lx_active[0]
                    << u1_field_pic_flag;
    u1_max_ref_idx_l1 = ps_cur_slice->u1_num_ref_idx_lx_active[1]
                    << u1_field_pic_flag;

    ps_dpb_mgr = ps_dec->ps_dpb_mgr;
    /* Get the current POC */
    i_cur_poc = ps_dec->ps_cur_pic->i4_poc;

    /* Get MaxStPOC,MinStPOC,MaxLt,MinLt */
    i_max_st_poc = i_cur_poc;
    i_min_st_poc = i_cur_poc;
    u1_max_lt_index = MAX_REF_BUFS + 1;
    u1_min_lt_index = MAX_REF_BUFS + 1;
    /* Start from ST head */
    ps_next_dpb = ps_dpb_mgr->ps_dpb_st_head;
    for(i = 0; i < ps_dpb_mgr->u1_num_st_ref_bufs; i++)
    {
        i_ref_poc = ps_next_dpb->ps_pic_buf->i4_poc;
        if(i_ref_poc < i_cur_poc)
        {
            /* RefPic Buf POC is before Current POC in display order */
            i_min_st_poc = MIN(i_min_st_poc, i_ref_poc);
        }
        else
        {
            /* RefPic Buf POC is after Current POC in display order */
            i_max_st_poc = MAX(i_max_st_poc, i_ref_poc);
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
        sizeof(aps_st_pic_bufs[0]), poc_compare);

    /* Start from LT head */
    ps_next_dpb = ps_dpb_mgr->ps_dpb_ht_head;
    if(ps_next_dpb)
    {
        u1_max_lt_index = ps_next_dpb->u1_lt_idx;
        u1_min_lt_index = ps_next_dpb->u1_lt_idx;
    }
    for(i = 0; i < ps_dpb_mgr->u1_num_lt_ref_bufs; i++)
    {
        u4_lt_index = ps_next_dpb->u1_lt_idx;
        u1_max_lt_index = (UWORD8)(MAX(u1_max_lt_index, u4_lt_index));
        u1_min_lt_index = (UWORD8)(MIN(u1_min_lt_index, u4_lt_index));

        /* Chase the next link */
        ps_next_dpb = ps_next_dpb->ps_prev_long;
    }

    /* 1. Initialize refIdxL0 */
    u1_L0 = 0;
    i_temp_poc = i_cur_poc;
    if(u1_field_pic_flag)
    {
        ps_ref_pic_buf_lx = ps_dpb_mgr->ps_init_dpb[0][0];
        ps_ref_pic_buf_lx += MAX_REF_BUFS;
    }
    else
    {
        ps_ref_pic_buf_lx = ps_dpb_mgr->ps_init_dpb[0][0];
        /* Avoid integer overflow while decrementing by one */
        if (i_temp_poc > INT32_MIN)
            i_temp_poc--;
    }

    i_cur_idx = -1;
    for(j = 0; j < ps_dpb_mgr->u1_num_st_ref_bufs; j++)
    {
        if (NULL == aps_st_pic_bufs[j])
        {
            break;
        }
        if (aps_st_pic_bufs[j]->i4_poc <= i_temp_poc)
        {
            i_cur_idx = j;
        }
    }
    /* Arrange all short term buffers in output order as given by POC */
    /* 1.1 Arrange POC's less than CurrPOC in the descending POC order starting
     from (CurrPOC - 1)*/
    for(j = i_cur_idx; j >= 0; j--)
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

    /* 1.2. Arrange POC's more than CurrPOC in the ascending POC order starting
     from (CurrPOC + 1)*/
    for(j = i_cur_idx + 1; j < ps_dpb_mgr->u1_num_st_ref_bufs; j++)
    {
        if(aps_st_pic_bufs[j])
        {
            ih264d_insert_pic_in_ref_pic_listx(ps_ref_pic_buf_lx,
                                               aps_st_pic_bufs[j]);
            ps_ref_pic_buf_lx++;
            u1_L0++;
        }
    }

    /* 1.3 Arrange all Long term buffers in ascending order, in LongtermIndex */
    /* Start from ST head */

    u1_num_short_term_bufs = u1_L0;
    for(u4_lt_index = u1_min_lt_index; u4_lt_index <= u1_max_lt_index; u4_lt_index++)
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

    ps_dec->ps_cur_slice->u1_initial_list_size[0] = u1_L0;

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
    {
        /* 2. Initialize refIdxL1 */
        u1_L1 = 0;
        if(u1_field_pic_flag)
        {
            ps_ref_pic_buf_lx = ps_dpb_mgr->ps_init_dpb[1][0] + MAX_REF_BUFS;
        }
        else
        {
            ps_ref_pic_buf_lx = ps_dpb_mgr->ps_init_dpb[1][0];
        }

        /* 2.1. Arrange POC's more than CurrPOC in the ascending POC order starting
         from (CurrPOC + 1)*/
        for(j = i_cur_idx + 1; j < ps_dpb_mgr->u1_num_st_ref_bufs; j++)
        {
            if(aps_st_pic_bufs[j])
            {
                /* Start from ST head */
                ih264d_insert_pic_in_ref_pic_listx(ps_ref_pic_buf_lx,
                                                   aps_st_pic_bufs[j]);
                ps_ref_pic_buf_lx++;
                u1_L1++;
            }
        }

        /* Arrange all short term buffers in output order as given by POC */
        /* 2.2 Arrange POC's less than CurrPOC in the descending POC order starting
         from (CurrPOC - 1)*/
        for(j = i_cur_idx; j >= 0; j--)
        {
            if(aps_st_pic_bufs[j])
            {
                ih264d_insert_pic_in_ref_pic_listx(ps_ref_pic_buf_lx,
                                                   aps_st_pic_bufs[j]);
                ps_ref_pic_buf_lx++;
                u1_L1++;
            }
        }

        /* 2.3 Arrange all Long term buffers in ascending order, in LongtermIndex */
        /* Start from ST head */
        u1_num_short_term_bufs = u1_L1;

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
                    u1_L1++;
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
                for(u1_i = u1_L1; u1_i < u1_max_ref_idx_l1; u1_i++)
                {
                    *ps_ref_pic_buf_lx = ref_pic;
                    ps_ref_pic_buf_lx++;
                }
            }

            ih264d_convert_frm_to_fld_list(
                            ps_dpb_mgr->ps_init_dpb[1][0] + MAX_REF_BUFS,
                            &u1_L1, ps_dec, u1_num_short_term_bufs);
            ps_ref_pic_buf_lx = ps_dpb_mgr->ps_init_dpb[1][0] + u1_L1;
        }

        ps_dec->ps_cur_slice->u1_initial_list_size[1] = u1_L1;

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
            for(u1_i = u1_L1; u1_i < u1_max_ref_idx_l1; u1_i++)
            {
                *ps_ref_pic_buf_lx = ref_pic;
                ps_ref_pic_buf_lx++;
            }
        }

        /* If list0 and list 1 ebtries are same then swap the 0th and 1st entry */
        /* of list 1                                                            */
        {
            struct pic_buffer_t *ps_ref_pic1_buf_l0, *ps_ref_pic1_buf_l1;
            struct pic_buffer_t s_ref_pic1_buf_temp;

            ps_ref_pic1_buf_l0 = ps_dpb_mgr->ps_init_dpb[0][0];
            ps_ref_pic1_buf_l1 = ps_dpb_mgr->ps_init_dpb[1][0];

            if((u1_L0 == u1_L1) && (u1_L0 > 1))
            {
                WORD32 i_index, i_swap;

                i_swap = 1;

                for(i_index = 0; i_index < u1_L0; i_index++)
                {
                    if((ps_ref_pic1_buf_l0[i_index]).pu1_buf1
                                    != (ps_ref_pic1_buf_l1[i_index]).pu1_buf1)
                    {
                        i_swap = 0;
                        break;
                    }
                }
                if(1 == i_swap)
                {
                    memcpy(&s_ref_pic1_buf_temp, &ps_ref_pic1_buf_l1[1],
                           sizeof(struct pic_buffer_t));
                    memcpy(&ps_ref_pic1_buf_l1[1], &ps_ref_pic1_buf_l1[0],
                           sizeof(struct pic_buffer_t));
                    memcpy(&ps_ref_pic1_buf_l1[0], &s_ref_pic1_buf_temp,
                           sizeof(struct pic_buffer_t));
                }
            }
        }
    }
}



void ih264d_get_implicit_weights(dec_struct_t *ps_dec);

/*!
 **************************************************************************
 * \if Function name : ih264d_one_to_one \endif
 *
 * \brief
 *    Initializes forward and backward refernce lists for B slice decoding.
 *
 *
 * \return
 *    0 on Success and Error code otherwise
 **************************************************************************
 */
void ih264d_one_to_one(dec_struct_t *ps_dec,
                       struct pic_buffer_t *ps_col_pic,
                       directmv_t *ps_direct,
                       UWORD8 u1_wd_x,
                       WORD32 u2_sub_mb_ofst,
                       dec_mb_info_t * ps_cur_mb_info)
{
    UWORD8 *pu1_col_zero_flag_start, u1_col_mb_pred_mode, u1_num_blks, u1_sub_mb_num;
    UWORD8 u1_init_colzero_flag;
    UNUSED(ps_cur_mb_info);
    pu1_col_zero_flag_start = ps_col_pic->pu1_col_zero_flag + u2_sub_mb_ofst;
    u1_col_mb_pred_mode = pu1_col_zero_flag_start[ps_dec->u1_sub_mb_num];
    u1_init_colzero_flag = u1_col_mb_pred_mode & 1;
    u1_col_mb_pred_mode >>= 6;
    ps_direct->u1_vert_mv_scale = ONE_TO_ONE;
    ps_direct->u1_col_zeroflag_change = 0;

    if(u1_wd_x == MB_SIZE)
    {
        ps_dec->u1_currB_type = (!!u1_col_mb_pred_mode);
        if(u1_col_mb_pred_mode == PRED_16x16)
        {
            ps_direct->i1_num_partitions = 1;
            ps_direct->i4_mv_indices[0] = u2_sub_mb_ofst;
            ps_direct->i1_submb_num[0] = 0;
            ps_direct->i1_partitionsize[0] = PRED_16x16;

            return;
        }
        else if(u1_col_mb_pred_mode < PRED_8x8)
        {
            ps_direct->i1_num_partitions = 2;
            ps_direct->i4_mv_indices[0] = u2_sub_mb_ofst;
            ps_direct->i1_submb_num[0] = 0;
            ps_direct->i1_partitionsize[0] = u1_col_mb_pred_mode;
            u1_sub_mb_num = (u1_col_mb_pred_mode == PRED_16x8) ? 8 : 2;
            ps_direct->i1_submb_num[1] = u1_sub_mb_num;
            ps_direct->i4_mv_indices[1] = u2_sub_mb_ofst
                            + ps_direct->i1_submb_num[1];
            ps_direct->i1_partitionsize[1] = u1_col_mb_pred_mode;
            if((pu1_col_zero_flag_start[u1_sub_mb_num] & 1) != u1_init_colzero_flag)
                ps_direct->u1_col_zeroflag_change = 1;
            return;
        }
        else
        {
            u1_num_blks = 4;
        }
    }
    else
    {
        u1_num_blks = 1;
    }

    {
        const UWORD8 *pu1_top_lt_mb_part_idx;
        UWORD8 u1_col_sub_mb_pred_mode, uc_blk, u1_sub_blk, u1_submb_col = 0;
        UWORD8 u1_num_sub_blks, uc_direct8x8inf, *pu1_col_zero_flag, u1_sub_mb_num;
        const UWORD8 *pu1_num_sub_mb_part =
                        (const UWORD8 *)gau1_ih264d_num_submb_part;
        UWORD8 i1_num_partitions = 0, partition_size;
        WORD32 mv_index;
        const UWORD8 *pu1_top_lt_sub_mb_idx = gau1_ih264d_submb_indx_mod_sp_drct;

        u1_sub_mb_num = ps_dec->u1_sub_mb_num;
        uc_direct8x8inf = ps_dec->ps_cur_slice->u1_direct_8x8_inference_flag;
        pu1_top_lt_mb_part_idx = gau1_ih264d_top_left_mb_part_indx_mod
                        + (PRED_8x8 << 1) + 1;

        for(uc_blk = 0; uc_blk < u1_num_blks; uc_blk++)
        {
            partition_size = PRED_8x8;
            pu1_top_lt_sub_mb_idx = gau1_ih264d_submb_indx_mod_sp_drct;
            if(uc_direct8x8inf == 1)
            {
                u1_submb_col = u1_sub_mb_num | (u1_sub_mb_num >> 1);
                mv_index = u2_sub_mb_ofst + u1_submb_col;
                u1_num_sub_blks = 1;
            }
            else
            {
                /* colMbPart is either 8x8, 8x4, 4x8, 4x4 */
                pu1_col_zero_flag = pu1_col_zero_flag_start + u1_sub_mb_num;
                u1_col_sub_mb_pred_mode = *pu1_col_zero_flag;
                u1_col_sub_mb_pred_mode = (u1_col_sub_mb_pred_mode & 0x30) >> 4;
                partition_size = (UWORD8)((u1_col_sub_mb_pred_mode)
                                | (PRED_8x8 << 2));
                mv_index = u2_sub_mb_ofst + u1_sub_mb_num;
                pu1_top_lt_sub_mb_idx += (u1_col_sub_mb_pred_mode << 1);
                u1_num_sub_blks = pu1_num_sub_mb_part[u1_col_sub_mb_pred_mode];

            }

            for(u1_sub_blk = 0; u1_sub_blk < u1_num_sub_blks;
                            u1_sub_blk++, pu1_top_lt_sub_mb_idx++)
            {
                u1_sub_mb_num += *pu1_top_lt_sub_mb_idx;
                mv_index += *pu1_top_lt_sub_mb_idx;
                ps_direct->i4_mv_indices[i1_num_partitions] = mv_index;
                ps_direct->i1_submb_num[i1_num_partitions] = u1_sub_mb_num;
                ps_direct->i1_partitionsize[i1_num_partitions] = partition_size;
                i1_num_partitions++;
                if(!uc_direct8x8inf)
                    u1_submb_col = u1_sub_mb_num;
                if((pu1_col_zero_flag_start[u1_submb_col] & 1)
                                != u1_init_colzero_flag)
                    ps_direct->u1_col_zeroflag_change = 1;
            }
            u1_sub_mb_num = *pu1_top_lt_mb_part_idx++;
        }
        ps_direct->i1_num_partitions = i1_num_partitions;
    }
}
/*!
 **************************************************************************
 * \if Function name : ih264d_mbaff_cross_pmbair \endif
 *
 * \brief
 *    Initializes forward and backward refernce lists for B slice decoding.
 *
 *
 * \return
 *    0 on Success and Error code otherwise
 **************************************************************************
 */
void ih264d_mbaff_cross_pmbair(dec_struct_t *ps_dec,
                               struct pic_buffer_t *ps_col_pic,
                               directmv_t *ps_direct,
                               UWORD8 u1_wd_x,
                               WORD32 u2_sub_mb_ofst,
                               dec_mb_info_t * ps_cur_mb_info)
{
    UWORD8 *pu1_col_zero_flag_start, *pu1_col_zero_flag, u1_sub_mb_num,
                    uc_sub_mb_num_col;
    UWORD8 *pu1_col_zero_flag_right_half;
    WORD32 i4_force_8X8;
    UWORD8 u1_num_blks, u1_col_mb_pred_mode, uc_blk, u1_col_sub_mb_pred_mode,
                    u1_col_sub_mb_pred_mode_rt;
    UWORD8 i1_num_partitions = 0, partition_size;

    WORD32 mv_index;

    UWORD8 u1_num_sub_blks;
    UWORD8 u1_is_cur_mb_fld, i;
    UWORD8 u1_init_colzero_flag;

    u1_is_cur_mb_fld = ps_cur_mb_info->u1_mb_field_decodingflag;
    u1_sub_mb_num = ps_dec->u1_sub_mb_num;
    ps_direct->u1_col_zeroflag_change = 0;
    /*pu1_col_zero_flag_start = ps_col_pic->pu1_col_zero_flag + u2_sub_mb_ofst;
     u1_col_mb_pred_mode = pu1_col_zero_flag_start[u1_sub_mb_num];
     u1_init_colzero_flag = u1_col_mb_pred_mode & 1;
     u1_col_mb_pred_mode >>= 6; */
    if(0 == u1_is_cur_mb_fld)
    {
        ps_direct->u1_vert_mv_scale = FLD_TO_FRM;
        if(u1_wd_x == MB_SIZE)
        {
            pu1_col_zero_flag_start = ps_col_pic->pu1_col_zero_flag
                            + u2_sub_mb_ofst;
            u1_col_mb_pred_mode = pu1_col_zero_flag_start[0];
            u1_init_colzero_flag = u1_col_mb_pred_mode & 1;
            u1_col_mb_pred_mode >>= 6;


            if(u1_col_mb_pred_mode & 0x2)
            {
                ps_dec->u1_currB_type = 1;
                if(u1_col_mb_pred_mode == PRED_8x16)
                {
                    ps_direct->i1_num_partitions = 2;
                    ps_direct->i4_mv_indices[0] = u2_sub_mb_ofst;
                    ps_direct->i1_submb_num[0] = 0;
                    ps_direct->i1_partitionsize[0] = PRED_8x16;
                    ps_direct->i4_mv_indices[1] = u2_sub_mb_ofst + 2;
                    ps_direct->i1_submb_num[1] = 2;
                    ps_direct->i1_partitionsize[1] = PRED_8x16;
                    if((pu1_col_zero_flag_start[2] & 1) != u1_init_colzero_flag)
                        ps_direct->u1_col_zeroflag_change = 1;
                }
                else
                {
                    pu1_col_zero_flag = pu1_col_zero_flag_start + u1_sub_mb_num;
                    u1_col_sub_mb_pred_mode = (*pu1_col_zero_flag & 0x10);/* 8x4 or 4x4 mode */

                    pu1_col_zero_flag_right_half = pu1_col_zero_flag_start
                                    + u1_sub_mb_num + 2;
                    u1_col_sub_mb_pred_mode_rt =
                                    (*pu1_col_zero_flag_right_half & 0x10);/* 8x4 or 4x4 mode */

                    i4_force_8X8 = (u1_col_sub_mb_pred_mode)
                                    || (u1_col_sub_mb_pred_mode_rt);
                    if(i4_force_8X8)
                    {
                        u1_num_sub_blks = 2;
                        partition_size = PRED_8x8;
                    }
                    else
                    {
                        partition_size = PRED_8x16;
                        u1_num_sub_blks = 1;
                    }

                    for(i = 0; i < 2; i++)
                    {
                        for(uc_blk = 0; uc_blk < u1_num_sub_blks; uc_blk++)
                        {
                            uc_sub_mb_num_col = u1_sub_mb_num | (u1_sub_mb_num >> 1);
                            uc_sub_mb_num_col &= 0x7;
                            mv_index = u2_sub_mb_ofst + uc_sub_mb_num_col;

                            ps_direct->i4_mv_indices[i1_num_partitions] =
                                            mv_index;
                            ps_direct->i1_submb_num[i1_num_partitions] =
                                            u1_sub_mb_num;
                            ps_direct->i1_partitionsize[i1_num_partitions] =
                                            partition_size;
                            i1_num_partitions++;
                            if((pu1_col_zero_flag_start[uc_sub_mb_num_col] & 1)
                                            != u1_init_colzero_flag)
                                ps_direct->u1_col_zeroflag_change = 1;
                            u1_sub_mb_num += 8;
                        }
                        u1_sub_mb_num = 2; /* move to second half of Cur MB */
                    }
                    ps_direct->i1_num_partitions = i1_num_partitions;
                    return;
                }
            }
            else
            {
                ps_direct->i1_num_partitions = 1;
                ps_direct->i4_mv_indices[0] = u2_sub_mb_ofst;
                ps_direct->i1_submb_num[0] = 0;
                ps_direct->i1_partitionsize[0] = PRED_16x16;
                ps_dec->u1_currB_type = 0;
                return;
            }
        }
        else
        {
            uc_sub_mb_num_col = u1_sub_mb_num | (u1_sub_mb_num >> 1);
            uc_sub_mb_num_col &= 0x7;

            ps_direct->i4_mv_indices[0] = u2_sub_mb_ofst + uc_sub_mb_num_col;
            ps_direct->i1_submb_num[0] = u1_sub_mb_num;
            ps_direct->i1_partitionsize[0] = PRED_8x8;
            ps_direct->i1_num_partitions = 1;
        }
    }
    else
    {
        ps_direct->u1_vert_mv_scale = FRM_TO_FLD;
        pu1_col_zero_flag_start = ps_col_pic->pu1_col_zero_flag + u2_sub_mb_ofst;
        u1_init_colzero_flag = pu1_col_zero_flag_start[0] & 1;

        if(u1_wd_x == MB_SIZE)
        {
            UWORD8 u1_submb_col;
            UWORD8 *puc_colZeroFlagStart_bot_mb, uc_colMbPredMode_bot_mb;

            pu1_col_zero_flag_start = ps_col_pic->pu1_col_zero_flag
                            + u2_sub_mb_ofst;
            u1_col_mb_pred_mode = pu1_col_zero_flag_start[u1_sub_mb_num] >> 6;

            puc_colZeroFlagStart_bot_mb = ps_col_pic->pu1_col_zero_flag
                            + u2_sub_mb_ofst + 16;
            uc_colMbPredMode_bot_mb = puc_colZeroFlagStart_bot_mb[8] >> 6;

            i4_force_8X8 = (u1_col_mb_pred_mode & 0x2)
                            || (uc_colMbPredMode_bot_mb & 0x2);
            if(i4_force_8X8)
            {
                u1_num_blks = 2;
                partition_size = PRED_8x8;
            }
            else
            {
                u1_num_blks = 1;
                partition_size = PRED_16x8;
            }

            ps_dec->u1_currB_type = 1;
            /*As this mb is derived from 2 Mbs min no of partitions = 2*/
            for(i = 0; i < 2; i++)
            {

                pu1_col_zero_flag_start = ps_col_pic->pu1_col_zero_flag
                                + u2_sub_mb_ofst;
                u1_col_mb_pred_mode = pu1_col_zero_flag_start[u1_sub_mb_num] >> 6;

                for(uc_blk = 0; uc_blk < u1_num_blks; uc_blk++)
                {
                    u1_submb_col = (u1_sub_mb_num & 0x7) ? 1 : 0;
                    u1_submb_col += u1_sub_mb_num;
                    mv_index = u2_sub_mb_ofst + u1_submb_col;


                    ps_direct->i4_mv_indices[i1_num_partitions] = mv_index;
                    ps_direct->i1_submb_num[i1_num_partitions] = u1_sub_mb_num;
                    ps_direct->i1_partitionsize[i1_num_partitions] =
                                    partition_size;
                    i1_num_partitions++;
                    if((pu1_col_zero_flag_start[u1_submb_col] & 1)
                                    != u1_init_colzero_flag)
                        ps_direct->u1_col_zeroflag_change = 1;
                    u1_sub_mb_num += 2;
                }
                u1_sub_mb_num = 8; /* move to second half of Cur MB */
                u2_sub_mb_ofst += 16;/* move to next Colocated MB */
            }
            ps_direct->i1_num_partitions = i1_num_partitions;
            return;
        }
        else
        {
            uc_sub_mb_num_col = u1_sub_mb_num | (u1_sub_mb_num >> 1);
            uc_sub_mb_num_col &= 0xb;
            u2_sub_mb_ofst += (u1_sub_mb_num >> 3) ? 16 : 0;

            ps_direct->i4_mv_indices[0] = u2_sub_mb_ofst + uc_sub_mb_num_col;
            ps_direct->i1_submb_num[0] = u1_sub_mb_num;
            ps_direct->i1_partitionsize[0] = PRED_8x8;
            ps_direct->i1_num_partitions = 1;
            return;
        }
    }
}
/*!
 **************************************************************************
 * \if Function name : ih264d_cal_col_pic \endif
 *
 * \brief
 *    Finds the colocated picture.
 *
 *
 * \return
 *    0 on Success and Error code otherwise
 **************************************************************************
 */
WORD32 ih264d_cal_col_pic(dec_struct_t *ps_dec)
{
    struct pic_buffer_t* ps_col_pic = ps_dec->ps_col_pic;
    UWORD8 uc_curpictype, uc_colpictype;
    ps_col_pic = ps_dec->ps_ref_pic_buf_lx[1][0];
    uc_curpictype = (ps_dec->ps_cur_pic->u1_picturetype & 0x7);
    uc_colpictype = (ps_col_pic->u1_picturetype & 0x7);
    if(uc_curpictype == FRM_PIC)
    {
        if(uc_colpictype == FRM_PIC)
            ps_dec->pf_parse_mvdirect = ih264d_one_to_one;
        else if(uc_colpictype == COMP_FLD_PAIR)
        {
            ps_dec->pf_parse_mvdirect = ih264d_fld_to_frm;
            if(ps_col_pic->i4_top_field_order_cnt
                            >= ps_col_pic->i4_bottom_field_order_cnt)
            {
                struct pic_buffer_t* ps_tempPic = ps_col_pic;
                UWORD32 ui_half_num_of_sub_mbs = ((ps_dec->u2_pic_ht
                                * ps_dec->u2_pic_wd) >> 5);
                ps_col_pic = ps_dec->ps_ref_pic_buf_lx[1][MAX_REF_BUFS];
                /* memcpy ps_tempPic to ps_col_pic */
                *ps_col_pic = *ps_tempPic;
                ps_col_pic->pu1_buf1 = ps_tempPic->pu1_buf1
                                + ps_tempPic->u2_frm_wd_y;
                ps_col_pic->pu1_buf2 = ps_tempPic->pu1_buf2
                                + ps_tempPic->u2_frm_wd_uv;
                ps_col_pic->pu1_buf3 = ps_tempPic->pu1_buf3
                                + ps_tempPic->u2_frm_wd_uv;
                ps_col_pic->pu1_col_zero_flag = ps_tempPic->pu1_col_zero_flag
                                + ui_half_num_of_sub_mbs;
                ps_col_pic->ps_mv = ps_tempPic->ps_mv + ui_half_num_of_sub_mbs;


                ps_col_pic->u1_pic_type = 0;/*complementary reference field pair-refering as frame */



            }
        }
        else
        {
            UWORD32 i4_error_code;
            i4_error_code = ERROR_DBP_MANAGER_T;
//          i4_error_code |= 1<<IVD_CORRUPTEDDATA;
            return i4_error_code;
        }
    }
    else if(uc_curpictype == AFRM_PIC)
    {
        ps_dec->pf_parse_mvdirect = ih264d_fld_to_mbaff;
    }
    else /* must be a field*/
    {
        if(uc_colpictype == FRM_PIC)
            ps_dec->pf_parse_mvdirect = ih264d_frm_to_fld;
        else if(uc_colpictype == AFRM_PIC)
            ps_dec->pf_parse_mvdirect = ih264d_mbaff_to_fld;
        else
            ps_dec->pf_parse_mvdirect = ih264d_one_to_one;
    }
    ps_dec->ps_col_pic = ps_col_pic;
    return OK;
}

/*!
 **************************************************************************
 * \if Function name : ih264d_frm_to_fld \endif
 *
 * \brief
 *    Initializes forward and backward refernce lists for B slice decoding.
 *
 *
 * \return
 *    0 on Success and Error code otherwise
 **************************************************************************
 */
void ih264d_frm_to_fld(dec_struct_t *ps_dec,
                       struct pic_buffer_t *ps_col_pic,
                       directmv_t *ps_direct,
                       UWORD8 u1_wd_x,
                       WORD32 u2_sub_mb_ofst,
                       dec_mb_info_t * ps_cur_mb_info)
{
    UWORD8 *pu1_col_zero_flag_start, u1_sub_mb_num;
    UWORD8 u1_num_blks, u1_col_mb_pred_mode, uc_blk;
    UWORD8 i1_num_partitions = 0, partition_size, i;
    WORD32 mv_index;
    UWORD32 increment;
    WORD32 i4_force_8X8;
    UNUSED(ps_cur_mb_info);
    ps_direct->u1_col_zeroflag_change = 1;
    ps_direct->u1_vert_mv_scale = FRM_TO_FLD;
    u1_sub_mb_num = ps_dec->u1_sub_mb_num;

    /* new calculation specific to this function */
    if((ps_col_pic->u1_picturetype & 0x7) == FRM_PIC)
    {
        UWORD16 u2_frm_wd_in_mbs = ps_dec->u2_frm_wd_in_mbs;
        increment = (u2_frm_wd_in_mbs << 4);
        /*mbAddrCol = mbAddrCol1 */
        u2_sub_mb_ofst = (ps_dec->u2_mbx
                        + (2 * ps_dec->u2_mby * u2_frm_wd_in_mbs)) << 4;
    }
    else
        increment = 16;

    if(u1_wd_x == MB_SIZE)
    {
        ps_dec->u1_currB_type = 1;

        {
            UWORD8 *puc_colZeroFlagStart_bot_mb, uc_colMbPredMode_bot_mb;

            pu1_col_zero_flag_start = ps_col_pic->pu1_col_zero_flag
                            + u2_sub_mb_ofst;
            u1_col_mb_pred_mode = (*pu1_col_zero_flag_start >> 6);

            puc_colZeroFlagStart_bot_mb = ps_col_pic->pu1_col_zero_flag
                            + u2_sub_mb_ofst + increment;
            uc_colMbPredMode_bot_mb = (*puc_colZeroFlagStart_bot_mb >> 6);

            i4_force_8X8 = (u1_col_mb_pred_mode & 0x2)
                            || (uc_colMbPredMode_bot_mb & 0x2);

            if(i4_force_8X8)
            {
                u1_num_blks = 2;
                partition_size = PRED_8x8;
            }
            else
            {
                partition_size = PRED_16x8;
                u1_num_blks = 1;
            }
        }

        /*As this mb is derived from 2 Mbs, min no of partitions = 2*/
        for(i = 0; i < 2; i++)
        {
            for(uc_blk = 0; uc_blk < u1_num_blks; uc_blk++)
            {
                mv_index = u2_sub_mb_ofst + u1_sub_mb_num;
                mv_index += (u1_sub_mb_num & 0x7) ? 1 : 0;

                ps_direct->i4_mv_indices[i1_num_partitions] = mv_index;
                ps_direct->i1_submb_num[i1_num_partitions] = u1_sub_mb_num;
                ps_direct->i1_partitionsize[i1_num_partitions] = partition_size;
                i1_num_partitions++;

                u1_sub_mb_num += 2;
            }
            u1_sub_mb_num = 8; /* move to second half of Cur MB */
            u2_sub_mb_ofst += increment;/* move to next Colocated MB */
        }
        ps_direct->i1_num_partitions = i1_num_partitions;
        return;
    }
    else
    {
        UWORD8 u1_sub_mb_num_col;
        u1_sub_mb_num_col = u1_sub_mb_num | (u1_sub_mb_num >> 1);
        u1_sub_mb_num_col &= 0xb;
        u2_sub_mb_ofst += (u1_sub_mb_num >> 3) ? increment : 0;

        ps_direct->i4_mv_indices[0] = u2_sub_mb_ofst + u1_sub_mb_num_col;
        ps_direct->i1_submb_num[0] = u1_sub_mb_num;
        ps_direct->i1_partitionsize[0] = PRED_8x8;
        ps_direct->i1_num_partitions = 1;
        return;
    }
}
/*!
 **************************************************************************
 * \if Function name : ih264d_fld_to_frm \endif
 *
 * \brief
 *    Initializes forward and backward refernce lists for B slice decoding.
 *
 *
 * \return
 *    0 on Success and Error code otherwise
 **************************************************************************
 */
void ih264d_fld_to_frm(dec_struct_t *ps_dec,
                       struct pic_buffer_t *ps_col_pic,
                       directmv_t *ps_direct,
                       UWORD8 u1_wd_x,
                       WORD32 u2_sub_mb_ofst,
                       dec_mb_info_t * ps_cur_mb_info)
{
    UWORD8 *pu1_col_zero_flag_start, *pu1_col_zero_flag,
                    *pu1_col_zero_flag_right_half, u1_sub_mb_num, uc_sub_mb_num_col;
    UWORD8 u1_col_mb_pred_mode, uc_blk;
    WORD32 i4_force_8X8;

    UNUSED(ps_cur_mb_info);
    ps_direct->u1_vert_mv_scale = FLD_TO_FRM;
    ps_direct->u1_col_zeroflag_change = 1;
    /* new calculation specific to this function for u2_sub_mb_ofst*/
    u2_sub_mb_ofst = (ps_dec->u2_mbx
                    + ((ps_dec->u2_mby >> 1) * ps_dec->u2_frm_wd_in_mbs)) << 4;
    u2_sub_mb_ofst += ((ps_dec->u2_mby & 1) << 3);

    if(u1_wd_x == MB_SIZE)
    {
        pu1_col_zero_flag_start = ps_col_pic->pu1_col_zero_flag + u2_sub_mb_ofst;
        u1_col_mb_pred_mode = (*pu1_col_zero_flag_start >> 6);
        ps_dec->u1_currB_type = (!!u1_col_mb_pred_mode);

        if(u1_col_mb_pred_mode & 0x2)
        {
            if(u1_col_mb_pred_mode == PRED_8x16)
            {
                ps_direct->i1_num_partitions = 2;
                ps_direct->i4_mv_indices[0] = u2_sub_mb_ofst;
                ps_direct->i1_submb_num[0] = 0;
                ps_direct->i1_partitionsize[0] = PRED_8x16;
                ps_direct->i4_mv_indices[1] = u2_sub_mb_ofst + 2;
                ps_direct->i1_submb_num[1] = 2;
                ps_direct->i1_partitionsize[1] = PRED_8x16;
            }
            else
            {
                UWORD8 i1_num_partitions = 0, partition_size;
                UWORD32 mv_index;
                UWORD8 u1_num_sub_blks, i, u1_col_sub_mb_pred_mode,
                                u1_col_sub_mb_pred_mode_rt;

                u1_sub_mb_num = ps_dec->u1_sub_mb_num;

                pu1_col_zero_flag = pu1_col_zero_flag_start + u1_sub_mb_num;
                u1_col_sub_mb_pred_mode = (*pu1_col_zero_flag & 0x10);/* 8x4 or 4x4 mode */

                pu1_col_zero_flag_right_half = pu1_col_zero_flag_start + u1_sub_mb_num
                                + 2;
                u1_col_sub_mb_pred_mode_rt = (*pu1_col_zero_flag_right_half
                                & 0x10);/* 8x4 or 4x4 mode */

                i4_force_8X8 = (u1_col_sub_mb_pred_mode)
                                || (u1_col_sub_mb_pred_mode_rt);
                if(i4_force_8X8)
                {
                    u1_num_sub_blks = 2;
                    partition_size = PRED_8x8;
                }
                else
                {
                    partition_size = PRED_8x16;
                    u1_num_sub_blks = 1;
                }

                for(i = 0; i < 2; i++)
                {
                    for(uc_blk = 0; uc_blk < u1_num_sub_blks; uc_blk++)
                    {
                        uc_sub_mb_num_col = u1_sub_mb_num | (u1_sub_mb_num >> 1);
                        uc_sub_mb_num_col &= 0x7;
                        mv_index = u2_sub_mb_ofst + uc_sub_mb_num_col;

                        ps_direct->i4_mv_indices[i1_num_partitions] = mv_index;
                        ps_direct->i1_submb_num[i1_num_partitions] =
                                        u1_sub_mb_num;
                        ps_direct->i1_partitionsize[i1_num_partitions] =
                                        partition_size;
                        i1_num_partitions++;
                        u1_sub_mb_num += 8;
                    }

                    u1_sub_mb_num = 2; /* move to second half of Cur MB */

                }
                ps_direct->i1_num_partitions = i1_num_partitions;
                return;
            }
        }
        else
        {
            ps_direct->i1_num_partitions = 1;
            ps_direct->i4_mv_indices[0] = u2_sub_mb_ofst;
            ps_direct->i1_submb_num[0] = 0;
            ps_direct->i1_partitionsize[0] = PRED_16x16;
            return;
        }
    }
    else
    {
        u1_sub_mb_num = ps_dec->u1_sub_mb_num;
        uc_sub_mb_num_col = u1_sub_mb_num | (u1_sub_mb_num >> 1);
        uc_sub_mb_num_col &= 0x7;

        ps_direct->i4_mv_indices[0] = u2_sub_mb_ofst + uc_sub_mb_num_col;
        ps_direct->i1_submb_num[0] = u1_sub_mb_num;
        ps_direct->i1_partitionsize[0] = PRED_8x8;
        ps_direct->i1_num_partitions = 1;
    }
}
/*!
 **************************************************************************
 * \if Function name : ih264d_one_to_one \endif
 *
 * \brief
 *    Initializes forward and backward refernce lists for B slice decoding.
 *
 *
 * \return
 *    0 on Success and Error code otherwise
 **************************************************************************
 */
void ih264d_mbaff_to_fld(dec_struct_t *ps_dec,
                         struct pic_buffer_t *ps_col_pic,
                         directmv_t *ps_direct,
                         UWORD8 u1_wd_x,
                         WORD32 u2_sub_mb_ofst,
                         dec_mb_info_t * ps_cur_mb_info)
{
    UWORD8* pu1_col_zero_flag, u1_iscol_mb_fld;
    u2_sub_mb_ofst <<= 1;
    pu1_col_zero_flag = ps_col_pic->pu1_col_zero_flag + u2_sub_mb_ofst;
    u1_iscol_mb_fld = (*pu1_col_zero_flag & 0x2) >> 1;
    if(u1_iscol_mb_fld)
    {
        u2_sub_mb_ofst += (ps_dec->ps_cur_slice->u1_bottom_field_flag << 4);
        ih264d_one_to_one(ps_dec, ps_col_pic, ps_direct, u1_wd_x,
                          u2_sub_mb_ofst, ps_cur_mb_info);
    }
    else
        ih264d_frm_to_fld(ps_dec, ps_col_pic, ps_direct, u1_wd_x,
                          u2_sub_mb_ofst, ps_cur_mb_info);
}
/*!
 **************************************************************************
 * \if Function name : ih264d_one_to_one \endif
 *
 * \brief
 *    Initializes forward and backward refernce lists for B slice decoding.
 *
 *
 * \return
 *    0 on Success and Error code otherwise
 **************************************************************************
 */
void ih264d_fld_to_mbaff(dec_struct_t *ps_dec,
                         struct pic_buffer_t *ps_col_pic,
                         directmv_t *ps_direct,
                         UWORD8 u1_wd_x,
                         WORD32 u2_sub_mb_ofst,
                         dec_mb_info_t * ps_cur_mb_info)
{
    if((ps_col_pic->u1_picturetype & 0x7) == COMP_FLD_PAIR)
    {
        /* first calculate the colocated picture which varies with Mb */
        UWORD8 u1_is_cur_mb_fld;
        u1_is_cur_mb_fld = ps_cur_mb_info->u1_mb_field_decodingflag;
        u2_sub_mb_ofst = (u2_sub_mb_ofst & 0xffe0); /* mbaddrCol5 = curmbaddr/2;*/
        u2_sub_mb_ofst >>= 1;

        ps_col_pic = ps_dec->ps_ref_pic_buf_lx[1][0];
        if(u1_is_cur_mb_fld)
        {
            if(1 - ps_cur_mb_info->u1_topmb)
                ps_col_pic = ps_dec->ps_ref_pic_buf_lx[1][MAX_REF_BUFS];

            ih264d_one_to_one(ps_dec, ps_col_pic, ps_direct, u1_wd_x,
                              u2_sub_mb_ofst, ps_cur_mb_info);
        }
        else
        {

            if(ABS(ps_col_pic->i4_top_field_order_cnt
                            - (WORD64)ps_dec->ps_cur_pic->i4_poc) >=
                            ABS((WORD64)ps_dec->ps_cur_pic->i4_poc
                                                - ps_col_pic->i4_bottom_field_order_cnt))
            {
                ps_col_pic = ps_dec->ps_ref_pic_buf_lx[1][MAX_REF_BUFS];
            }

            if(ps_cur_mb_info->u1_topmb == 0)
                u2_sub_mb_ofst += 8;
            ih264d_mbaff_cross_pmbair(ps_dec, ps_col_pic, ps_direct, u1_wd_x,
                                      u2_sub_mb_ofst, ps_cur_mb_info);
        }
        ps_dec->ps_col_pic = ps_col_pic;
    }
    else
    {
        UWORD8* pu1_col_zero_flag = ps_col_pic->pu1_col_zero_flag
                        + u2_sub_mb_ofst;
        UWORD8 temp, u1_is_cur_mb_fld, u1_iscol_mb_fld;

        u1_iscol_mb_fld = (*pu1_col_zero_flag & 0x2) >> 1;
        u1_is_cur_mb_fld = ps_cur_mb_info->u1_mb_field_decodingflag;
        temp = (u1_iscol_mb_fld ^ u1_is_cur_mb_fld);

        if(temp == 0)
            ih264d_one_to_one(ps_dec, ps_col_pic, ps_direct, u1_wd_x,
                              u2_sub_mb_ofst, ps_cur_mb_info);
        else
        {
            u2_sub_mb_ofst &= 0xffef;
            if(u1_is_cur_mb_fld == 0)
            {
                if(ABS(ps_col_pic->i4_top_field_order_cnt
                                - (WORD64)ps_dec->ps_cur_pic->i4_poc) >=
                                ABS((WORD64)ps_dec->ps_cur_pic->i4_poc
                                                    - ps_col_pic->i4_bottom_field_order_cnt))
                {
                    u2_sub_mb_ofst += 0x10;
                }
                if(ps_cur_mb_info->u1_topmb == 0)
                    u2_sub_mb_ofst += 8;
            }
            ih264d_mbaff_cross_pmbair(ps_dec, ps_col_pic, ps_direct, u1_wd_x,
                                      u2_sub_mb_ofst, ps_cur_mb_info);
        }
    }
}

