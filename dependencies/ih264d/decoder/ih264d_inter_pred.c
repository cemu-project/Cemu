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
 * \file ih264d_inter_pred.c
 *
 * \brief
 *    This file contains routines to perform MotionCompensation tasks
 *
 * Detailed_description
 *
 * \date
 *    20/11/2002
 *
 * \author  Arvind Raman
 **************************************************************************
 */

#include <string.h>
#include "ih264d_defs.h"
#include "ih264d_mvpred.h"
#include "ih264d_error_handler.h"
#include "ih264d_structs.h"
#include "ih264d_defs.h"
#include "ih264d_inter_pred.h"
#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264d_debug.h"
#include "ih264d_tables.h"
#include "ih264d_mb_utils.h"


void ih264d_pad_on_demand(pred_info_t *ps_pred, UWORD8 lum_chrom_blk);



void ih264d_copy_multiplex_data(UWORD8 *puc_Source,
                                UWORD8 *puc_To,
                                UWORD32 uc_w,
                                UWORD32 uc_h,
                                UWORD32 ui16_sourceWidth,
                                UWORD32 ui16_toWidth)
{
    UWORD8 uc_i, uc_j;

    for(uc_i = 0; uc_i < uc_h; uc_i++)
    {
        memcpy(puc_To, puc_Source, uc_w);
        puc_To += ui16_toWidth;
        puc_Source += ui16_sourceWidth;
    }
}


/*!
 **************************************************************************
 * \if Function name : dma_2d1d \endif
 *
 * \brief
 *    2D -> 1D linear DMA into the reference buffers
 *
 * \return
 *    None
 **************************************************************************
 */
void ih264d_copy_2d1d(UWORD8 *puc_src,
                      UWORD8 *puc_dest,
                      UWORD16 ui16_srcWidth,
                      UWORD16 ui16_widthToFill,
                      UWORD16 ui16_heightToFill)
{
    UWORD32 uc_w, uc_h;
    for(uc_h = ui16_heightToFill; uc_h != 0; uc_h--)
    {
        memcpy(puc_dest, puc_src, ui16_widthToFill);
        puc_dest += ui16_widthToFill;
        puc_src += ui16_srcWidth;
    }
}

/*!
 **************************************************************************
 * \if Function name : ih264d_fill_pred_info \endif
 *
 * \brief
 *    Fills inter prediction related info
 *
 * \return
 *    None
 **************************************************************************
 */
void ih264d_fill_pred_info(WORD16 *pi2_mv,WORD32 part_width,WORD32 part_height, WORD32 sub_mb_num,
                           WORD32 pred_dir,pred_info_pkd_t *ps_pred_pkd,WORD8 i1_buf_id,
                           WORD8 i1_ref_idx,UWORD32 *pu4_wt_offset,UWORD8 u1_pic_type)
{
    WORD32 insert_bits;

    ps_pred_pkd->i2_mv[0] = pi2_mv[0];
    ps_pred_pkd->i2_mv[1] = pi2_mv[1];

    insert_bits = sub_mb_num & 3; /*sub mb x*/
    ps_pred_pkd->i1_size_pos_info = insert_bits;
    insert_bits = sub_mb_num >> 2;/*sub mb y*/
    ps_pred_pkd->i1_size_pos_info |= insert_bits << 2;
    insert_bits = part_width >> 1;
    ps_pred_pkd->i1_size_pos_info |= insert_bits << 4;
    insert_bits = part_height >> 1;
    ps_pred_pkd->i1_size_pos_info |= insert_bits << 6;

    ps_pred_pkd->i1_ref_idx_info = i1_ref_idx;
    ps_pred_pkd->i1_ref_idx_info |= (pred_dir << 6);
    ps_pred_pkd->i1_buf_id = i1_buf_id;
    ps_pred_pkd->pu4_wt_offst = pu4_wt_offset;
    ps_pred_pkd->u1_pic_type = u1_pic_type;


}







/*****************************************************************************/
/* \if Function name : formMbPartInfo \endif                                 */
/*                                                                           */
/* \brief                                                                    */
/*    Form the Mb partition information structure, to be used by the MC      */
/*    routine                                                                */
/*                                                                           */
/* \return                                                                   */
/*    None                                                                   */
/* \note                                                                     */
/*    c_bufx is used to select PredBuffer,                                   */
/*    if it's only Forward/Backward prediction always buffer used is         */
/*    puc_MbLumaPredBuffer[0 to X1],pu1_mb_cb_pred_buffer[0 to X1] and          */
/*    pu1_mb_cr_pred_buffer[0 to X1]                                            */
/*                                                                           */
/*    if it's bidirect for forward ..PredBuffer[0 to X1] buffer is used and  */
/*    ..PredBuffer[X2 to X3] for backward prediction. and                    */
/*                                                                           */
/*    Final predicted samples values are the average of ..PredBuffer[0 to X1]*/
/*    and ..PredBuffer[X2 to X3]                                             */
/*                                                                           */
/*    X1 is 255 for Luma and 63 for Chroma                                   */
/*    X2 is 256 for Luma and 64 for Chroma                                   */
/*    X3 is 511 for Luma and 127 for Chroma                                  */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         11 05 2005   SWRN            Modified to handle pod               */
/*****************************************************************************/

WORD32 ih264d_form_mb_part_info_bp(pred_info_pkd_t *ps_pred_pkd,
                                 dec_struct_t * ps_dec,
                                 UWORD16 u2_mb_x,
                                 UWORD16 u2_mb_y,
                                 WORD32 mb_index,
                                 dec_mb_info_t *ps_cur_mb_info)
{
    /* The reference buffer pointer */
    WORD32 i2_frm_x, i2_frm_y;
    WORD32 i2_tmp_mv_x, i2_tmp_mv_y;
    WORD32 i2_rec_x, i2_rec_y;

    WORD32 u2_pic_ht;
    WORD32 u2_frm_wd;
    WORD32 u2_rec_wd;
    UWORD8 u1_sub_x = 0,u1_sub_y=0 ;
    UWORD8  u1_part_wd = 0,u1_part_ht = 0;
    WORD16 i2_mv_x,i2_mv_y;

    /********************************************/
    /* i1_mc_wd       width reqd for mcomp      */
    /* u1_dma_ht      height reqd for mcomp     */
    /* u1_dma_wd      width aligned to 4 bytes  */
    /* u1_dx          fractional part of width  */
    /* u1_dx          fractional part of height */
    /********************************************/
    UWORD32 i1_mc_wd;

    WORD32 u1_dma_ht;

    UWORD32 u1_dma_wd;
    UWORD32 u1_dx;
    UWORD32 u1_dy;
    pred_info_t * ps_pred = ps_dec->ps_pred + ps_dec->u4_pred_info_idx;
    dec_slice_params_t * const ps_cur_slice = ps_dec->ps_cur_slice;
    tfr_ctxt_t *ps_frame_buf;
    struct pic_buffer_t *ps_ref_frm;
    UWORD8 u1_scale_ref,u1_mbaff,u1_field;
    pic_buffer_t  **pps_ref_frame;
    WORD8 i1_size_pos_info,i1_buf_id;

    PROFILE_DISABLE_MB_PART_INFO()

     UNUSED(ps_cur_mb_info);
     i1_size_pos_info = ps_pred_pkd->i1_size_pos_info;
     GET_XPOS_PRED(u1_sub_x,i1_size_pos_info);
     GET_YPOS_PRED(u1_sub_y,i1_size_pos_info);
     GET_WIDTH_PRED(u1_part_wd,i1_size_pos_info);
     GET_HEIGHT_PRED(u1_part_ht,i1_size_pos_info);
     i2_mv_x = ps_pred_pkd->i2_mv[0];
     i2_mv_y = ps_pred_pkd->i2_mv[1];
     i1_buf_id = ps_pred_pkd->i1_buf_id;


     ps_ref_frm = ps_dec->apv_buf_id_pic_buf_map[i1_buf_id];


    {
        ps_frame_buf = &ps_dec->s_tran_addrecon;
    }


    /* Transfer Setup Y */
    {
        UWORD8 *pu1_pred, *pu1_rec;

        /* calculating rounded motion vectors and fractional components */
        i2_tmp_mv_x = i2_mv_x;
        i2_tmp_mv_y = i2_mv_y;
        u1_dx = i2_tmp_mv_x & 0x3;
        u1_dy = i2_tmp_mv_y & 0x3;
        i2_tmp_mv_x >>= 2;
        i2_tmp_mv_y >>= 2;
        i1_mc_wd = u1_part_wd << 2;
        u1_dma_ht = u1_part_ht << 2;
        if(u1_dx)
        {
            i2_tmp_mv_x -= 2;
            i1_mc_wd += 5;
        }
        if(u1_dy)
        {
            i2_tmp_mv_y -= 2;
            u1_dma_ht += 5;
        }

        /********************************************************************/
        /* Calulating the horizontal and the vertical u4_ofst from top left  */
        /* edge of the reference frame, and subsequent clipping             */
        /********************************************************************/
        u2_pic_ht = ps_dec->u2_pic_ht;
        u2_frm_wd = ps_dec->u2_frm_wd_y;
        i2_rec_x = u1_sub_x << 2;
        i2_rec_y = u1_sub_y << 2;

        i2_frm_x = (u2_mb_x << 4) + i2_rec_x + i2_tmp_mv_x;
        i2_frm_y = (u2_mb_y << 4) + i2_rec_y + i2_tmp_mv_y;

        i2_frm_x = CLIP3(MAX_OFFSET_OUTSIDE_X_FRM, (ps_dec->u2_pic_wd - 1),
                         i2_frm_x);
        i2_frm_y = CLIP3(((1 - u1_dma_ht)), (u2_pic_ht - (1)), i2_frm_y);

        pu1_pred = ps_ref_frm->pu1_buf1 + i2_frm_y * u2_frm_wd + i2_frm_x;

        u1_dma_wd = (i1_mc_wd + 3) & 0xFC;

        /********************************************************************/
        /* Calulating the horizontal and the vertical u4_ofst from top left  */
        /* edge of the recon buffer                                         */
        /********************************************************************/
        u2_rec_wd = MB_SIZE;
        {
            u2_rec_wd = ps_dec->u2_frm_wd_y;
            i2_rec_x += (mb_index << 4);
            pu1_rec = ps_frame_buf->pu1_dest_y + i2_rec_y * u2_rec_wd
                            + i2_rec_x;
        }

        /* filling the pred and dma structures for Y */
        u2_frm_wd = ps_dec->u2_frm_wd_y;

        ps_pred->u2_u1_ref_buf_wd = u1_dma_wd;
        ps_pred->i1_dma_ht = u1_dma_ht;
        ps_pred->i1_mc_wd = i1_mc_wd;
        ps_pred->u2_frm_wd = u2_frm_wd;
        ps_pred->pu1_rec_y_u = pu1_rec;
        ps_pred->u2_dst_stride = u2_rec_wd;

        ps_pred->i1_mb_partwidth = u1_part_wd << 2;
        ps_pred->i1_mb_partheight = u1_part_ht << 2;
        ps_pred->u1_dydx = (u1_dy << 2) + u1_dx;

        ps_pred->pu1_y_ref = pu1_pred;

    }

    /* Increment ps_pred index */
    ps_pred++;

    /* Transfer Setup U & V */
    {
        WORD32 i4_ref_offset, i4_rec_offset;
        UWORD8 *pu1_pred_u, *pu1_pred_v;


        /* calculating rounded motion vectors and fractional components */
        i2_tmp_mv_x = i2_mv_x;
        i2_tmp_mv_y = i2_mv_y;

        /************************************************************************/
        /* Table 8-9: Derivation of the vertical component of the chroma vector */
        /* in field coding mode                                                 */
        /************************************************************************/

        /* Eighth sample of the chroma MV */
        u1_dx = i2_tmp_mv_x & 0x7;
        u1_dy = i2_tmp_mv_y & 0x7;

        /********************************************************************/
        /* Calculating the full pel MV for chroma which is 1/2 of the Luma  */
        /* MV in full pel units                                             */
        /********************************************************************/
        i2_mv_x = i2_tmp_mv_x;
        i2_mv_y = i2_tmp_mv_y;
        i2_tmp_mv_x = SIGN_POW2_DIV(i2_tmp_mv_x, 3);
        i2_tmp_mv_y = SIGN_POW2_DIV(i2_tmp_mv_y, 3);
        i1_mc_wd = u1_part_wd << 1;
        u1_dma_ht = u1_part_ht << 1;
        if(u1_dx)
        {
            i2_tmp_mv_x -= (i2_mv_x < 0);
            i1_mc_wd++;
        }
        if(u1_dy != 0)
        {
            i2_tmp_mv_y -= (i2_mv_y < 0);
            u1_dma_ht++;
        }

        /********************************************************************/
        /* Calulating the horizontal and the vertical u4_ofst from top left  */
        /* edge of the reference frame, and subsequent clipping             */
        /********************************************************************/
        u2_pic_ht >>= 1;
        u2_frm_wd = ps_dec->u2_frm_wd_uv;
        i2_rec_x = u1_sub_x << 1;
        i2_rec_y = u1_sub_y << 1;

        i2_frm_x = (u2_mb_x << 3) + i2_rec_x + i2_tmp_mv_x;
        i2_frm_y = (u2_mb_y << 3) + i2_rec_y + i2_tmp_mv_y;

        i2_frm_x = CLIP3(MAX_OFFSET_OUTSIDE_UV_FRM,
                         ((ps_dec->u2_pic_wd >> 1) - 1), i2_frm_x);
        i2_frm_y = CLIP3(((1 - u1_dma_ht)), (u2_pic_ht - (1)), i2_frm_y);

        i4_ref_offset = i2_frm_y * u2_frm_wd + i2_frm_x * YUV420SP_FACTOR;
        u1_dma_wd = (i1_mc_wd + 3) & 0xFC;

        /********************************************************************/
        /* Calulating the horizontal and the vertical u4_ofst from top left  */
        /* edge of the recon buffer                                         */
        /********************************************************************/
        /* CHANGED CODE */
        u2_rec_wd = BLK8x8SIZE * YUV420SP_FACTOR;
        i4_rec_offset = i2_rec_y * u2_rec_wd + i2_rec_x * YUV420SP_FACTOR;

        {
            u2_rec_wd = ps_dec->u2_frm_wd_uv;
            i2_rec_x += (mb_index << 3);
            i4_rec_offset = i2_rec_y * u2_rec_wd + i2_rec_x * YUV420SP_FACTOR;
            ps_pred->pu1_rec_y_u = ps_frame_buf->pu1_dest_u + i4_rec_offset;
            ps_pred->u1_pi1_wt_ofst_rec_v = ps_frame_buf->pu1_dest_v
                            + i4_rec_offset;
        }

        /* CHANGED CODE */

        /* filling the common pred structures for U */
        u2_frm_wd = ps_dec->u2_frm_wd_uv;

        ps_pred->u2_u1_ref_buf_wd = u1_dma_wd;
        ps_pred->i1_dma_ht = u1_dma_ht;
        ps_pred->i1_mc_wd = i1_mc_wd;

        ps_pred->u2_frm_wd = u2_frm_wd;
        ps_pred->u2_dst_stride = u2_rec_wd;

        ps_pred->i1_mb_partwidth = u1_part_wd << 1;
        ps_pred->i1_mb_partheight = u1_part_ht << 1;
        ps_pred->u1_dydx = (u1_dy << 3) + u1_dx;

        pu1_pred_u = ps_ref_frm->pu1_buf2 + i4_ref_offset;
        pu1_pred_v = ps_ref_frm->pu1_buf3 + i4_ref_offset;

        /* Copy U & V partitions */
        ps_pred->pu1_u_ref = pu1_pred_u;

        /* Increment the reference buffer Index */
        ps_pred->pu1_v_ref = pu1_pred_v;
    }

    /* Increment ps_pred index */
    ps_dec->u4_pred_info_idx += 2;

    return OK;

}


/*****************************************************************************/
/* \if Function name : formMbPartInfo \endif                                 */
/*                                                                           */
/* \brief                                                                    */
/*    Form the Mb partition information structure, to be used by the MC      */
/*    routine                                                                */
/*                                                                           */
/* \return                                                                   */
/*    None                                                                   */
/* \note                                                                     */
/*    c_bufx is used to select PredBuffer,                                   */
/*    if it's only Forward/Backward prediction always buffer used is         */
/*    puc_MbLumaPredBuffer[0 to X1],pu1_mb_cb_pred_buffer[0 to X1] and          */
/*    pu1_mb_cr_pred_buffer[0 to X1]                                            */
/*                                                                           */
/*    if it's bidirect for forward ..PredBuffer[0 to X1] buffer is used and  */
/*    ..PredBuffer[X2 to X3] for backward prediction. and                    */
/*                                                                           */
/*    Final predicted samples values are the average of ..PredBuffer[0 to X1]*/
/*    and ..PredBuffer[X2 to X3]                                             */
/*                                                                           */
/*    X1 is 255 for Luma and 63 for Chroma                                   */
/*    X2 is 256 for Luma and 64 for Chroma                                   */
/*    X3 is 511 for Luma and 127 for Chroma                                  */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         11 05 2005   SWRN            Modified to handle pod               */
/*****************************************************************************/
WORD32 ih264d_form_mb_part_info_mp(pred_info_pkd_t *ps_pred_pkd,
                                 dec_struct_t * ps_dec,
                                 UWORD16 u2_mb_x,
                                 UWORD16 u2_mb_y,
                                 WORD32 mb_index,
                                 dec_mb_info_t *ps_cur_mb_info)
{
    /* The reference buffer pointer */
    UWORD8 *pu1_ref_buf;
    WORD16 i2_frm_x, i2_frm_y, i2_tmp_mv_x, i2_tmp_mv_y, i2_pod_ht;
    WORD16 i2_rec_x, i2_rec_y;
    UWORD16 u2_pic_ht, u2_frm_wd, u2_rec_wd;
    UWORD8 u1_wght_pred_type, u1_wted_bipred_idc;
    UWORD16 u2_tot_ref_scratch_size;
    UWORD8 u1_sub_x = 0;
    UWORD8 u1_sub_y = 0;
    UWORD8 u1_is_bi_dir = 0;

    /********************************************/
    /* i1_mc_wd       width reqd for mcomp      */
    /* u1_dma_ht      height reqd for mcomp     */
    /* u1_dma_wd      width aligned to 4 bytes  */
    /* u1_dx          fractional part of width  */
    /* u1_dx          fractional part of height */
    /********************************************/
    UWORD8 i1_mc_wd, u1_dma_ht, u1_dma_wd, u1_dx, u1_dy;
    pred_info_t * ps_pred ;
    dec_slice_params_t * const ps_cur_slice = ps_dec->ps_cur_slice;
    const UWORD8 u1_slice_type = ps_dec->ps_decode_cur_slice->slice_type;
    UWORD8 u1_pod_bot, u1_pod_top;

    /* load the pictype for pod u4_flag & chroma motion vector derivation */
    UWORD8 u1_ref_pic_type ;

    /* set default value to flags specifying field nature of picture & mb */
    UWORD32 u1_mb_fld = 0, u1_mb_or_pic_fld;
    UWORD32 u1_mb_bot = 0, u1_pic_bot = 0, u1_mb_or_pic_bot;
    tfr_ctxt_t *ps_frame_buf;
    /* calculate flags specifying field nature of picture & mb */
    const UWORD32 u1_pic_fld = ps_cur_slice->u1_field_pic_flag;
    WORD8 i1_pred;
    WORD8 i1_size_pos_info,i1_buf_id,i1_ref_idx;
    UWORD8 u1_part_wd,u1_part_ht;
    WORD16 i2_mv_x,i2_mv_y;
    struct pic_buffer_t *ps_ref_frm;
    UWORD32 *pu4_wt_offset;
    UWORD8 *pu1_buf1,*pu1_buf2,*pu1_buf3;


    PROFILE_DISABLE_MB_PART_INFO()

    ps_pred = ps_dec->ps_pred + ps_dec->u4_pred_info_idx;


     i1_size_pos_info = ps_pred_pkd->i1_size_pos_info;
     GET_XPOS_PRED(u1_sub_x,i1_size_pos_info);
     GET_YPOS_PRED(u1_sub_y,i1_size_pos_info);
     GET_WIDTH_PRED(u1_part_wd,i1_size_pos_info);
     GET_HEIGHT_PRED(u1_part_ht,i1_size_pos_info);
     i2_mv_x = ps_pred_pkd->i2_mv[0];
     i2_mv_y = ps_pred_pkd->i2_mv[1];
     i1_ref_idx = ps_pred_pkd->i1_ref_idx_info & 0x3f;
     i1_buf_id = ps_pred_pkd->i1_buf_id;
     ps_ref_frm = ps_dec->apv_buf_id_pic_buf_map[i1_buf_id];

     i1_pred = (ps_pred_pkd->i1_ref_idx_info & 0xC0) >> 6;
     u1_is_bi_dir = (i1_pred == BI_PRED);


    u1_ref_pic_type = ps_pred_pkd->u1_pic_type & PIC_MASK;

    pu1_buf1  = ps_ref_frm->pu1_buf1;
    pu1_buf2  = ps_ref_frm->pu1_buf2;
    pu1_buf3  = ps_ref_frm->pu1_buf3;

    if(u1_ref_pic_type == BOT_FLD)
    {
        pu1_buf1 += ps_ref_frm->u2_frm_wd_y;
        pu1_buf2 += ps_ref_frm->u2_frm_wd_uv;
        pu1_buf3 += ps_ref_frm->u2_frm_wd_uv;

    }



    if(ps_dec->ps_cur_pps->u1_wted_pred_flag)
    {
            pu4_wt_offset = (UWORD32*)&ps_dec->pu4_wt_ofsts[2
                            * X3(i1_ref_idx)];
    }


    pu4_wt_offset = ps_pred_pkd->pu4_wt_offst;


    /* Pointer to the frame buffer */
    {
        ps_frame_buf = &ps_dec->s_tran_addrecon;
        /* CHANGED CODE */
    }

    if(!u1_pic_fld)
    {
        u1_mb_fld = ps_cur_mb_info->u1_mb_field_decodingflag;
        u1_mb_bot = 1 - ps_cur_mb_info->u1_topmb;
    }
    else
        u1_pic_bot = ps_cur_slice->u1_bottom_field_flag;

    /****************************************************************/
    /* calculating the flags the tell whether to use frame-padding  */
    /* or use software pad-on-demand                                */
    /****************************************************************/
    u1_mb_or_pic_bot = u1_mb_bot | u1_pic_bot;
    u1_mb_or_pic_fld = u1_mb_fld | u1_pic_fld;
    u1_pod_bot = u1_mb_or_pic_fld && (u1_ref_pic_type == TOP_FLD);
    u1_pod_top = u1_mb_or_pic_fld && (u1_ref_pic_type == BOT_FLD);

    /* Weighted Pred additions */
    u1_wted_bipred_idc = ps_dec->ps_cur_pps->u1_wted_bipred_idc;

    if((u1_slice_type == P_SLICE) || (u1_slice_type == SP_SLICE))
    {
        /* P Slice only */
        u1_wght_pred_type = ps_dec->ps_cur_pps->u1_wted_pred_flag;

    }
    else
    {
        /* B Slice only */
        u1_wght_pred_type = 1 + u1_is_bi_dir;
        if(u1_wted_bipred_idc == 0)
            u1_wght_pred_type = 0;
        if((u1_wted_bipred_idc == 2) && (!u1_is_bi_dir))
            u1_wght_pred_type = 0;
    }
    /* load the scratch reference buffer index */
    pu1_ref_buf = ps_dec->pu1_ref_buff + ps_dec->u4_dma_buf_idx;
    u2_tot_ref_scratch_size = 0;


    /* Transfer Setup Y */
    {
        UWORD8 *pu1_pred, *pu1_rec;
        /* calculating rounded motion vectors and fractional components */
        i2_tmp_mv_x = i2_mv_x;
        i2_tmp_mv_y = i2_mv_y;

        u1_dx = i2_tmp_mv_x & 0x3;
        u1_dy = i2_tmp_mv_y & 0x3;
        i2_tmp_mv_x >>= 2;
        i2_tmp_mv_y >>= 2;
        i1_mc_wd = u1_part_wd << 2;
        u1_dma_ht = u1_part_ht << 2;
        if(u1_dx)
        {
            i2_tmp_mv_x -= 2;
            i1_mc_wd += 5;
        }
        if(u1_dy)
        {
            i2_tmp_mv_y -= 2;
            u1_dma_ht += 5;
        }

        /********************************************************************/
        /* Calulating the horizontal and the vertical u4_ofst from top left  */
        /* edge of the reference frame, and subsequent clipping             */
        /********************************************************************/
        u2_pic_ht = ps_dec->u2_pic_ht >> u1_pic_fld;
        u2_frm_wd = ps_dec->u2_frm_wd_y << u1_pic_fld;
        i2_frm_x = (u2_mb_x << 4) + (u1_sub_x << 2) + i2_tmp_mv_x;
        i2_frm_y = ((u2_mb_y + (u1_mb_bot && !u1_mb_fld)) << 4)
                        + (((u1_sub_y << 2) + i2_tmp_mv_y) << u1_mb_fld);

        i2_frm_x = CLIP3(MAX_OFFSET_OUTSIDE_X_FRM, (ps_dec->u2_pic_wd - 1),
                         i2_frm_x);
        i2_frm_y = CLIP3(((1 - u1_dma_ht) << u1_mb_fld),
                         (u2_pic_ht - (1 << u1_mb_fld)), i2_frm_y);

        pu1_pred = pu1_buf1 + i2_frm_y * u2_frm_wd + i2_frm_x;
        u1_dma_wd = (i1_mc_wd + 3) & 0xFC;
        /********************************************************************/
        /* Calulating the horizontal and the vertical u4_ofst from top left  */
        /* edge of the recon buffer                                         */
        /********************************************************************/
        /* CHANGED CODE */
        u2_rec_wd = MB_SIZE;
        i2_rec_x = u1_sub_x << 2;
        i2_rec_y = u1_sub_y << 2;
        {
            u2_rec_wd = ps_dec->u2_frm_wd_y << u1_mb_or_pic_fld;
            i2_rec_x += (mb_index << 4);
            pu1_rec = ps_frame_buf->pu1_dest_y + i2_rec_y * u2_rec_wd
                            + i2_rec_x;
            if(u1_mb_bot)
                pu1_rec += ps_dec->u2_frm_wd_y << ((u1_mb_fld) ? 0 : 4);
        }

        /* CHANGED CODE */

        /* filling the pred and dma structures for Y */
        u2_frm_wd = ps_dec->u2_frm_wd_y << u1_mb_or_pic_fld;

        ps_pred->pu1_dma_dest_addr = pu1_ref_buf;
        ps_pred->u2_u1_ref_buf_wd = u1_dma_wd;
        ps_pred->u2_frm_wd = u2_frm_wd;
        ps_pred->i1_dma_ht = u1_dma_ht;
        ps_pred->i1_mc_wd = i1_mc_wd;
        ps_pred->pu1_rec_y_u = pu1_rec;
        ps_pred->u2_dst_stride = u2_rec_wd;

        ps_pred->i1_mb_partwidth = u1_part_wd << 2;
        ps_pred->i1_mb_partheight = u1_part_ht << 2;
        ps_pred->u1_dydx = (u1_dy << 2) + u1_dx;
        ps_pred->u1_is_bi_direct = u1_is_bi_dir;
        ps_pred->u1_pi1_wt_ofst_rec_v = (UWORD8 *)pu4_wt_offset;
        ps_pred->u1_wght_pred_type = u1_wght_pred_type;
        ps_pred->i1_pod_ht = 0;

        /* Increment the Reference buffer Indices */
        pu1_ref_buf += u1_dma_wd * u1_dma_ht;
        u2_tot_ref_scratch_size += u1_dma_wd * u1_dma_ht;

        /* unrestricted field motion comp for top region outside frame */
        i2_pod_ht = (-i2_frm_y) >> u1_mb_fld;
        if((i2_pod_ht > 0) && u1_pod_top)
        {
            ps_pred->i1_pod_ht = (WORD8)(-i2_pod_ht);
            u1_dma_ht -= i2_pod_ht;
            pu1_pred += i2_pod_ht * u2_frm_wd;
        }
        /* unrestricted field motion comp for bottom region outside frame */
        else if(u1_pod_bot)
        {
            i2_pod_ht = u1_dma_ht + ((i2_frm_y - u2_pic_ht) >> u1_mb_fld);
            if(i2_pod_ht > 0)
            {
                u1_dma_ht -= i2_pod_ht;
                ps_pred->i1_pod_ht = (WORD8)i2_pod_ht;
            }
        }

        /* Copy Y partition */

        /*
         * ps_pred->i1_pod_ht is non zero when MBAFF is present. In case of MBAFF the reference data
         * is copied in the Scrath buffer so that the padding_on_demand doesnot corrupt the frame data
         */
        if(ps_pred->i1_pod_ht)
        {
            ps_pred->pu1_pred = pu1_pred;
            ps_pred->u1_dma_ht_y = u1_dma_ht;
            ps_pred->u1_dma_wd_y = u1_dma_wd;
        }
        ps_pred->pu1_y_ref = pu1_pred;
    }



    /* Increment ps_pred index */
    ps_pred++;

    /* Transfer Setup U & V */
    {
        WORD32 i4_ref_offset, i4_rec_offset;
        UWORD8 *pu1_pred_u, *pu1_pred_v, u1_tmp_dma_ht;
        /* CHANGED CODE */
        UWORD8 u1_chroma_cbp = (UWORD8)(ps_cur_mb_info->u1_cbp >> 4);
        /* CHANGED CODE */

        /* calculating rounded motion vectors and fractional components */
        i2_tmp_mv_x = i2_mv_x;
        i2_tmp_mv_y = i2_mv_y;

        /************************************************************************/
        /* Table 8-9: Derivation of the vertical component of the chroma vector */
        /* in field coding mode                                                 */
        /************************************************************************/
        if(u1_pod_bot && u1_mb_or_pic_bot)
            i2_tmp_mv_y += 2;
        if(u1_pod_top && !u1_mb_or_pic_bot)
            i2_tmp_mv_y -= 2;

        /* Eighth sample of the chroma MV */
        u1_dx = i2_tmp_mv_x & 0x7;
        u1_dy = i2_tmp_mv_y & 0x7;

        /********************************************************************/
        /* Calculating the full pel MV for chroma which is 1/2 of the Luma  */
        /* MV in full pel units                                             */
        /********************************************************************/
        i2_mv_x = i2_tmp_mv_x;
        i2_mv_y = i2_tmp_mv_y;
        i2_tmp_mv_x = SIGN_POW2_DIV(i2_tmp_mv_x, 3);
        i2_tmp_mv_y = SIGN_POW2_DIV(i2_tmp_mv_y, 3);
        i1_mc_wd = u1_part_wd << 1;
        u1_dma_ht = u1_part_ht << 1;
        if(u1_dx)
        {
            if(i2_mv_x < 0)
                i2_tmp_mv_x -= 1;
            i1_mc_wd++;
        }
        if(u1_dy != 0)
        {
            if(i2_mv_y < 0)
                i2_tmp_mv_y -= 1;
            u1_dma_ht++;
        }

        /********************************************************************/
        /* Calulating the horizontal and the vertical u4_ofst from top left  */
        /* edge of the reference frame, and subsequent clipping             */
        /********************************************************************/
        u2_pic_ht >>= 1;
        u2_frm_wd = ps_dec->u2_frm_wd_uv << u1_pic_fld;
        i2_frm_x = (u2_mb_x << 3) + (u1_sub_x << 1) + i2_tmp_mv_x;
        i2_frm_y = ((u2_mb_y + (u1_mb_bot && !u1_mb_fld)) << 3)
                        + (((u1_sub_y << 1) + i2_tmp_mv_y) << u1_mb_fld);

        i2_frm_x = CLIP3(MAX_OFFSET_OUTSIDE_UV_FRM,
                         ((ps_dec->u2_pic_wd >> 1) - 1), i2_frm_x);
        i2_frm_y = CLIP3(((1 - u1_dma_ht) << u1_mb_fld),
                         (u2_pic_ht - (1 << u1_mb_fld)), i2_frm_y);

        i4_ref_offset = i2_frm_y * u2_frm_wd + i2_frm_x * YUV420SP_FACTOR;
        u1_dma_wd = (i1_mc_wd + 3) & 0xFC;

        /********************************************************************/
        /* Calulating the horizontal and the vertical u4_ofst from top left  */
        /* edge of the recon buffer                                         */
        /********************************************************************/
        /* CHANGED CODE */
        u2_rec_wd = BLK8x8SIZE * YUV420SP_FACTOR;
        i2_rec_x = u1_sub_x << 1;
        i2_rec_y = u1_sub_y << 1;
        i4_rec_offset = i2_rec_y * u2_rec_wd + i2_rec_x * YUV420SP_FACTOR;
        {
            u2_rec_wd = ps_dec->u2_frm_wd_uv << u1_mb_or_pic_fld;

            i2_rec_x += (mb_index << 3);
            i4_rec_offset = i2_rec_y * u2_rec_wd + i2_rec_x * YUV420SP_FACTOR;
            if(u1_mb_bot)
                i4_rec_offset += ps_dec->u2_frm_wd_uv << ((u1_mb_fld) ? 0 : 3);
            ps_pred->pu1_rec_y_u = ps_frame_buf->pu1_dest_u + i4_rec_offset;
            ps_pred->u1_pi1_wt_ofst_rec_v = ps_frame_buf->pu1_dest_v
                            + i4_rec_offset;

        }

        /* CHANGED CODE */

        /* filling the common pred structures for U */
        u2_frm_wd = ps_dec->u2_frm_wd_uv << u1_mb_or_pic_fld;
        u1_tmp_dma_ht = u1_dma_ht;
        ps_pred->u2_u1_ref_buf_wd = u1_dma_wd;
        ps_pred->u2_frm_wd = u2_frm_wd;
        ps_pred->i1_dma_ht = u1_dma_ht;
        ps_pred->i1_mc_wd = i1_mc_wd;
        ps_pred->u2_dst_stride = u2_rec_wd;

        ps_pred->i1_mb_partwidth = u1_part_wd << 1;
        ps_pred->i1_mb_partheight = u1_part_ht << 1;
        ps_pred->u1_dydx = (u1_dy << 3) + u1_dx;
        ps_pred->u1_is_bi_direct = u1_is_bi_dir;
        ps_pred->u1_wght_pred_type = u1_wght_pred_type;
        ps_pred->i1_pod_ht = 0;

        ps_pred->pu1_dma_dest_addr = pu1_ref_buf;

        /* unrestricted field motion comp for top region outside frame */
        i2_pod_ht = (-i2_frm_y) >> u1_mb_fld;
        if((i2_pod_ht > 0) && u1_pod_top)
        {
            i4_ref_offset += i2_pod_ht * u2_frm_wd;
            u1_dma_ht -= i2_pod_ht;
            ps_pred->i1_pod_ht = (WORD8)(-i2_pod_ht);
        }
        /* unrestricted field motion comp for bottom region outside frame */
        else if(u1_pod_bot)
        {
            i2_pod_ht = u1_dma_ht + ((i2_frm_y - u2_pic_ht) >> u1_mb_fld);
            if(i2_pod_ht > 0)
            {
                u1_dma_ht -= i2_pod_ht;
                ps_pred->i1_pod_ht = (WORD8)i2_pod_ht;
            }
        }

        pu1_pred_u = pu1_buf2 + i4_ref_offset;
        pu1_pred_v = pu1_buf3 + i4_ref_offset;

        /* Copy U & V partitions */
        if(ps_pred->i1_pod_ht)
        {
            ps_pred->pu1_pred_u = pu1_pred_u;
            ps_pred->u1_dma_ht_uv = u1_dma_ht;
            ps_pred->u1_dma_wd_uv = u1_dma_wd;

        }
        ps_pred->pu1_u_ref = pu1_pred_u;

        /* Increment the reference buffer Index */
        u2_tot_ref_scratch_size += (u1_dma_wd * u1_tmp_dma_ht) << 1;

        if(ps_pred->i1_pod_ht)
        {
            ps_pred->pu1_pred_v = pu1_pred_v;
            ps_pred->u1_dma_ht_uv = u1_dma_ht;
            ps_pred->u1_dma_wd_uv = u1_dma_wd;
        }

        ps_pred->pu1_v_ref = pu1_pred_v;
    }

    /* Increment ps_pred index */
    ps_dec->u4_pred_info_idx += 2;


    /* Increment the reference buffer Index */
    ps_dec->u4_dma_buf_idx += u2_tot_ref_scratch_size;

    if(ps_dec->u4_dma_buf_idx > MAX_REF_BUF_SIZE)
        return ERROR_NUM_MV;

    return OK;



}


/*!
 **************************************************************************
 * \if Function name : MotionCompensate \endif
 *
 * \brief
 *    The routine forms predictor blocks for the entire MB and stores it in
 *    predictor buffers.This function works only for BASELINE profile
 *
 * \param ps_dec: Pointer to the structure decStruct. This is used to get
 *     pointers to the current and the reference frame and to the MbParams
 *     structure.
 *
 * \return
 *    None
 *
 * \note
 *    The routine forms predictors for all the luma and the chroma MB
 *    partitions.
 **************************************************************************
 */

void ih264d_motion_compensate_bp(dec_struct_t * ps_dec, dec_mb_info_t *ps_cur_mb_info)
{
    pred_info_t *ps_pred ;
    UWORD8 *puc_ref, *pu1_dest_y;
    UWORD8 *pu1_dest_u;
    UWORD32 u2_num_pels, u2_ref_wd_y, u2_ref_wd_uv, u2_dst_wd;

    UWORD32 u4_wd_y, u4_ht_y, u4_wd_uv;
    UWORD32 u4_ht_uv;
    UWORD8 *puc_pred0 = (UWORD8 *)(ps_dec->pi2_pred1);


    PROFILE_DISABLE_INTER_PRED()
    UNUSED(ps_cur_mb_info);
    ps_pred = ps_dec->ps_pred ;

    for(u2_num_pels = 0; u2_num_pels < 256;)
    {
        UWORD32 uc_dx, uc_dy;
        /* Pointer to the destination buffer. If the CBPs of all 8x8 blocks in
         the MB partition are zero then it would be better to copy the
         predictor valus directly to the current frame buffer */
        /*
         * ps_pred->i1_pod_ht is non zero when MBAFF is present. In case of MBAFF the reference data
         * is copied in the Scrath buffer so that the padding_on_demand doesnot corrupt the frame data
         */

        u2_ref_wd_y = ps_pred->u2_frm_wd;
        puc_ref = ps_pred->pu1_y_ref;
        if(ps_pred->u1_dydx & 0x3)
            puc_ref += 2;
        if(ps_pred->u1_dydx >> 2)
            puc_ref += 2 * u2_ref_wd_y;

        u4_wd_y = ps_pred->i1_mb_partwidth;
        u4_ht_y = ps_pred->i1_mb_partheight;
        uc_dx = ps_pred->u1_dydx;
        uc_dy = uc_dx >> 2;
        uc_dx &= 0x3;

        pu1_dest_y = ps_pred->pu1_rec_y_u;
        u2_dst_wd = ps_pred->u2_dst_stride;

        ps_dec->apf_inter_pred_luma[ps_pred->u1_dydx](puc_ref, pu1_dest_y,
                                                      u2_ref_wd_y,
                                                      u2_dst_wd,
                                                      u4_ht_y,
                                                      u4_wd_y, puc_pred0,
                                                      ps_pred->u1_dydx);

        ps_pred++;

        /* Interpolate samples for the chroma components */
        {
            UWORD8 *pu1_ref_u;

            u2_ref_wd_uv = ps_pred->u2_frm_wd;
            pu1_ref_u = ps_pred->pu1_u_ref;

            u4_wd_uv = ps_pred->i1_mb_partwidth;
            u4_ht_uv = ps_pred->i1_mb_partheight;
            uc_dx = ps_pred->u1_dydx; /* 8*dy + dx */
            uc_dy = uc_dx >> 3;
            uc_dx &= 0x7;

            pu1_dest_u = ps_pred->pu1_rec_y_u;
            u2_dst_wd = ps_pred->u2_dst_stride;

            ps_pred++;
            ps_dec->pf_inter_pred_chroma(pu1_ref_u, pu1_dest_u, u2_ref_wd_uv,
                                         u2_dst_wd, uc_dx, uc_dy,
                                         u4_ht_uv, u4_wd_uv);

        }

        u2_num_pels += (UWORD8)u4_wd_y * (UWORD8)u4_ht_y;

    }
}


/*
 **************************************************************************
 * \if Function name : MotionCompensateB \endif
 *
 * \brief
 *    The routine forms predictor blocks for the entire MB and stores it in
 *    predictor buffers.
 *
 * \param ps_dec: Pointer to the structure decStruct. This is used to get
 *     pointers to the current and the reference frame and to the MbParams
 *     structure.
 *
 * \return
 *    None
 *
 * \note
 *    The routine forms predictors for all the luma and the chroma MB
 *    partitions.
 **************************************************************************
 */

void ih264d_motion_compensate_mp(dec_struct_t * ps_dec, dec_mb_info_t *ps_cur_mb_info)
{
    pred_info_t *ps_pred ;
    pred_info_t *ps_pred_y_forw, *ps_pred_y_back, *ps_pred_cr_forw;
    UWORD8 *puc_ref, *pu1_dest_y, *puc_pred0, *puc_pred1;
    UWORD8 *pu1_dest_u, *pu1_dest_v;
    WORD16 *pi16_intm;
    UWORD32 u2_num_pels, u2_ref_wd_y, u2_ref_wd_uv, u2_dst_wd;
    UWORD32 u2_dest_wd_y, u2_dest_wd_uv;
    UWORD32 u2_row_buf_wd_y = 0;
    UWORD32 u2_row_buf_wd_uv = 0;
    UWORD32 u2_log2Y_crwd;
    UWORD32 u4_wd_y, u4_ht_y, u1_dir, u4_wd_uv;
    UWORD32 u4_ht_uv;
    UWORD8 *pu1_temp_mc_buffer = ps_dec->pu1_temp_mc_buffer;
    WORD32 i2_pod_ht;
    UWORD32 u2_pic_ht, u2_frm_wd, u2_rec_wd;
    UWORD32 u1_pod_bot, u1_pod_top;
    UWORD8 *pu1_pred, *pu1_dma_dst;
    UWORD32 u1_dma_wd, u1_dma_ht;

    dec_slice_params_t * const ps_cur_slice = ps_dec->ps_cur_slice;

    /* set default value to flags specifying field nature of picture & mb */
    UWORD32 u1_mb_fld = 0, u1_mb_or_pic_fld;
    UWORD32 u1_mb_or_pic_bot;
    /* calculate flags specifying field nature of picture & mb */
    const UWORD8 u1_pic_fld = ps_cur_slice->u1_field_pic_flag;

    PROFILE_DISABLE_INTER_PRED()
    ps_pred = ps_dec->ps_pred ;
    /* Initialize both ps_pred_y_forw, ps_pred_cr_forw and ps_pred_y_back
     * to avoid static analysis warnings */
    ps_pred_y_forw = ps_pred;
    ps_pred_y_back = ps_pred;
    ps_pred_cr_forw = ps_pred;

    u2_log2Y_crwd = ps_dec->ps_decode_cur_slice->u2_log2Y_crwd;

    if(!u1_pic_fld)
    {
        u1_mb_fld = ps_cur_mb_info->u1_mb_field_decodingflag;
    }

    u1_mb_or_pic_fld = u1_mb_fld | u1_pic_fld;

    pi16_intm = ps_dec->pi2_pred1;
    puc_pred0 = (UWORD8 *)pi16_intm;
    puc_pred1 = puc_pred0 + PRED_BUFFER_WIDTH * PRED_BUFFER_HEIGHT * sizeof(WORD16);

    for(u2_num_pels = 0; u2_num_pels < 256;)
    {
        UWORD8 uc_dx, uc_dy;
        const UWORD8 u1_is_bi_direct = ps_pred->u1_is_bi_direct;
        for(u1_dir = 0; u1_dir <= u1_is_bi_direct; u1_dir++)
        {
            /* Pointer to the destination buffer. If the CBPs of all 8x8 blocks in
             the MB partition are zero then it would be better to copy the
             predictor valus directly to the current frame buffer */
            /*
             * ps_pred->i1_pod_ht is non zero when MBAFF is present. In case of MBAFF the reference data
             * is copied in the Scrath buffer so that the padding_on_demand doesnot corrupt the frame data
             */

            if(ps_pred->i1_pod_ht)
            {
                u2_ref_wd_y = ps_pred->u2_u1_ref_buf_wd;
                puc_ref = ps_pred->pu1_dma_dest_addr;
            }
            else
            {
                u2_ref_wd_y = ps_pred->u2_frm_wd;
                puc_ref = ps_pred->pu1_y_ref;

            }

            if(ps_pred->u1_dydx & 0x3)
                puc_ref += 2;
            if(ps_pred->u1_dydx >> 2)
                puc_ref += 2 * u2_ref_wd_y;
            u4_wd_y = ps_pred->i1_mb_partwidth;
            u4_ht_y = ps_pred->i1_mb_partheight;

            uc_dx = ps_pred->u1_dydx;
            uc_dy = uc_dx >> 2;
            uc_dx &= 0x3;
            if(u1_dir == 0)
            {
                pu1_dest_y = ps_pred->pu1_rec_y_u;
                u2_row_buf_wd_y = ps_pred->u2_dst_stride;
                u2_dst_wd = ps_pred->u2_dst_stride;
                u2_dest_wd_y = u2_dst_wd;
                ps_pred_y_forw = ps_pred;
            }
            else
            {
                pu1_dest_y = pu1_temp_mc_buffer;
                u2_dst_wd = MB_SIZE;
                u2_dest_wd_y = u2_dst_wd;
                ps_pred_y_back = ps_pred;
                ps_pred_y_back->pu1_rec_y_u = pu1_dest_y;
            }

            /* padding on demand (POD) for y done here */

            if(ps_pred->i1_pod_ht)
            {
                pu1_pred = ps_pred->pu1_pred;
                pu1_dma_dst = ps_pred->pu1_dma_dest_addr;
                u1_dma_wd = ps_pred->u1_dma_wd_y;
                u1_dma_ht = ps_pred->u1_dma_ht_y;
                u2_frm_wd = ps_dec->u2_frm_wd_y << u1_mb_or_pic_fld;
                if(ps_pred->i1_pod_ht < 0)
                {
                    pu1_dma_dst = pu1_dma_dst - (ps_pred->i1_pod_ht * ps_pred->u2_u1_ref_buf_wd);
                }
                ih264d_copy_2d1d(pu1_pred, pu1_dma_dst, u2_frm_wd, u1_dma_wd,
                                 u1_dma_ht);
                ih264d_pad_on_demand(ps_pred, LUM_BLK);
            }
            ps_dec->apf_inter_pred_luma[ps_pred->u1_dydx](puc_ref, pu1_dest_y,
                                                          u2_ref_wd_y,
                                                          u2_dst_wd,
                                                          u4_ht_y,
                                                          u4_wd_y,
                                                          puc_pred0,
                                                          ps_pred->u1_dydx);
            ps_pred++;

            /* Interpolate samples for the chroma components */
            {
                UWORD8 *pu1_ref_u;
                UWORD32 u1_dma_ht;

                /* padding on demand (POD) for U and V done here */
                u1_dma_ht = ps_pred->i1_dma_ht;

                if(ps_pred->i1_pod_ht)
                {
                    pu1_pred = ps_pred->pu1_pred_u;
                    pu1_dma_dst = ps_pred->pu1_dma_dest_addr;
                    u1_dma_ht = ps_pred->u1_dma_ht_uv;
                    u1_dma_wd = ps_pred->u1_dma_wd_uv * YUV420SP_FACTOR;
                    u2_frm_wd = ps_dec->u2_frm_wd_uv << u1_mb_or_pic_fld;
                    if(ps_pred->i1_pod_ht < 0)
                    {
                        /*Top POD*/
                        pu1_dma_dst -= (ps_pred->i1_pod_ht
                                        * ps_pred->u2_u1_ref_buf_wd
                                        * YUV420SP_FACTOR);
                    }

                    ih264d_copy_2d1d(pu1_pred, pu1_dma_dst, u2_frm_wd,
                                     u1_dma_wd, u1_dma_ht);

                    pu1_dma_dst += (ps_pred->i1_dma_ht
                                    * ps_pred->u2_u1_ref_buf_wd);
                    pu1_pred = ps_pred->pu1_pred_v;

                    ih264d_pad_on_demand(ps_pred, CHROM_BLK);
                }

                if(ps_pred->i1_pod_ht)
                {
                    pu1_ref_u = ps_pred->pu1_dma_dest_addr;

                    u2_ref_wd_uv = ps_pred->u2_u1_ref_buf_wd
                                    * YUV420SP_FACTOR;
                }
                else
                {
                    u2_ref_wd_uv = ps_pred->u2_frm_wd;
                    pu1_ref_u = ps_pred->pu1_u_ref;

                }

                u4_wd_uv = ps_pred->i1_mb_partwidth;
                u4_ht_uv = ps_pred->i1_mb_partheight;
                uc_dx = ps_pred->u1_dydx; /* 8*dy + dx */
                uc_dy = uc_dx >> 3;
                uc_dx &= 0x7;
                if(u1_dir == 0)
                {
                    pu1_dest_u = ps_pred->pu1_rec_y_u;

                    pu1_dest_v = ps_pred->u1_pi1_wt_ofst_rec_v;
                    u2_row_buf_wd_uv = ps_pred->u2_dst_stride;
                    u2_dst_wd = ps_pred->u2_dst_stride;
                    u2_dest_wd_uv = u2_dst_wd;
                    ps_pred_cr_forw = ps_pred;
                }
                else
                {
                    pu1_dest_u = puc_pred0;

                    pu1_dest_v = puc_pred1;
                    u2_dest_wd_uv = BUFFER_WIDTH;
                    u2_dst_wd = BUFFER_WIDTH;
                    ps_pred->pu1_rec_y_u = pu1_dest_u;
                    ps_pred->u1_pi1_wt_ofst_rec_v = pu1_dest_v;
                }

                ps_pred++;
                ps_dec->pf_inter_pred_chroma(pu1_ref_u, pu1_dest_u,
                                             u2_ref_wd_uv, u2_dst_wd,
                                             uc_dx, uc_dy, u4_ht_uv,
                                             u4_wd_uv);

                if(ps_cur_mb_info->u1_Mux == 1)
                {
                    /******************************************************************/
                    /* padding on demand (POD) for U and V done here                  */
                    /* ps_pred now points to the Y entry of the 0,0 component         */
                    /* Y need not be checked for POD because Y lies within            */
                    /* the picture((0,0) mv for Y doesnot get changed. But (0,0) for  */
                    /* U and V can need POD beacause of cross-field mv adjustments    */
                    /* (Table 8-9 of standard)                                        */
                    /******************************************************************/
                    if((ps_pred + 1)->i1_pod_ht)
                    {
                        pu1_pred = (ps_pred + 1)->pu1_pred_u;
                        pu1_dma_dst = (ps_pred + 1)->pu1_dma_dest_addr;
                        u1_dma_ht = (ps_pred + 1)->u1_dma_ht_uv;
                        u1_dma_wd = (ps_pred + 1)->u1_dma_wd_uv
                                        * YUV420SP_FACTOR;
                        u2_frm_wd = ps_dec->u2_frm_wd_uv << u1_mb_or_pic_fld;
                        if((ps_pred + 1)->i1_pod_ht < 0)
                        {
                            /*Top POD*/
                            pu1_dma_dst -= ((ps_pred + 1)->i1_pod_ht
                                            * (ps_pred + 1)->u2_u1_ref_buf_wd
                                            * YUV420SP_FACTOR);
                        }
                        ih264d_copy_2d1d(pu1_pred, pu1_dma_dst, u2_frm_wd,
                                         u1_dma_wd, u1_dma_ht);
                        pu1_dma_dst += ((ps_pred + 1)->i1_dma_ht
                                        * (ps_pred + 1)->u2_u1_ref_buf_wd); //(u1_dma_ht * u1_dma_wd);//
                        pu1_pred = (ps_pred + 1)->pu1_pred_v;
                        ih264d_pad_on_demand(ps_pred + 1, CHROM_BLK);

                    }

                    ih264d_multiplex_ref_data(ps_dec, ps_pred, pu1_dest_y,
                                              pu1_dest_u, ps_cur_mb_info,
                                              u2_dest_wd_y, u2_dest_wd_uv,
                                              u1_dir);
                    ps_pred += 2;
                }
            }
        }
        if(u1_dir != 0)
            u2_ref_wd_y = MB_SIZE;

        u2_num_pels += u4_wd_y * u4_ht_y;
        /* if BI_DIRECT, average the two pred's, and put in ..PredBuffer[0] */
        if((u1_is_bi_direct != 0) || (ps_pred_y_forw->u1_wght_pred_type != 0))
        {

            switch(ps_pred_y_forw->u1_wght_pred_type)
            {
                case 0:
                    ps_dec->pf_default_weighted_pred_luma(
                                    ps_pred_y_forw->pu1_rec_y_u, pu1_dest_y,
                                    ps_pred_y_forw->pu1_rec_y_u,
                                    u2_row_buf_wd_y, u2_ref_wd_y,
                                    u2_row_buf_wd_y, u4_ht_uv * 2,
                                    u4_wd_uv * 2);

                    ps_dec->pf_default_weighted_pred_chroma(
                                    ps_pred_cr_forw->pu1_rec_y_u, pu1_dest_u,
                                    ps_pred_cr_forw->pu1_rec_y_u,
                                    u2_row_buf_wd_uv, u2_dst_wd,
                                    u2_row_buf_wd_uv, u4_ht_uv,
                                    u4_wd_uv);

                    break;
                case 1:
                {
                    UWORD32 *pu4_weight_ofst =
                                    (UWORD32*)ps_pred_y_forw->u1_pi1_wt_ofst_rec_v;
                    UWORD32 u4_wt_ofst_u, u4_wt_ofst_v;
                    UWORD32 u4_wt_ofst_y =
                                    (UWORD32)(pu4_weight_ofst[0]);
                    WORD32 weight = (WORD16)(u4_wt_ofst_y & 0xffff);
                    WORD32 ofst = (WORD8)(u4_wt_ofst_y >> 16);

                    ps_dec->pf_weighted_pred_luma(ps_pred_y_forw->pu1_rec_y_u,
                                                  ps_pred_y_forw->pu1_rec_y_u,
                                                  u2_row_buf_wd_y,
                                                  u2_row_buf_wd_y,
                                                  (u2_log2Y_crwd & 0x0ff),
                                                  weight, ofst, u4_ht_y,
                                                  u4_wd_y);

                    u4_wt_ofst_u = (UWORD32)(pu4_weight_ofst[2]);
                    u4_wt_ofst_v = (UWORD32)(pu4_weight_ofst[4]);
                    weight = ((u4_wt_ofst_v & 0xffff) << 16)
                                    | (u4_wt_ofst_u & 0xffff);
                    ofst = ((u4_wt_ofst_v >> 16) << 8)
                                    | ((u4_wt_ofst_u >> 16) & 0xFF);

                    ps_dec->pf_weighted_pred_chroma(
                                    ps_pred_cr_forw->pu1_rec_y_u,
                                    ps_pred_cr_forw->pu1_rec_y_u,
                                    u2_row_buf_wd_uv, u2_row_buf_wd_uv,
                                    (u2_log2Y_crwd >> 8), weight, ofst,
                                    u4_ht_y >> 1, u4_wd_y >> 1);
                }

                    break;
                case 2:
                {
                    UWORD32 *pu4_weight_ofst =
                                    (UWORD32*)ps_pred_y_forw->u1_pi1_wt_ofst_rec_v;
                    UWORD32 u4_wt_ofst_u, u4_wt_ofst_v;
                    UWORD32 u4_wt_ofst_y;
                    WORD32 weight1, weight2;
                    WORD32 ofst1, ofst2;

                    u4_wt_ofst_y = (UWORD32)(pu4_weight_ofst[0]);

                    weight1 = (WORD16)(u4_wt_ofst_y & 0xffff);
                    ofst1 = (WORD8)(u4_wt_ofst_y >> 16);

                    u4_wt_ofst_y = (UWORD32)(pu4_weight_ofst[1]);
                    weight2 = (WORD16)(u4_wt_ofst_y & 0xffff);
                    ofst2 = (WORD8)(u4_wt_ofst_y >> 16);

                    ps_dec->pf_weighted_bi_pred_luma(ps_pred_y_forw->pu1_rec_y_u,
                                                     ps_pred_y_back->pu1_rec_y_u,
                                                     ps_pred_y_forw->pu1_rec_y_u,
                                                     u2_row_buf_wd_y,
                                                     u2_ref_wd_y,
                                                     u2_row_buf_wd_y,
                                                     (u2_log2Y_crwd & 0x0ff),
                                                     weight1, weight2, ofst1,
                                                     ofst2, u4_ht_y,
                                                     u4_wd_y);

                    u4_wt_ofst_u = (UWORD32)(pu4_weight_ofst[2]);
                    u4_wt_ofst_v = (UWORD32)(pu4_weight_ofst[4]);
                    weight1 = ((u4_wt_ofst_v & 0xffff) << 16)
                                    | (u4_wt_ofst_u & 0xffff);
                    ofst1 = ((u4_wt_ofst_v >> 16) << 8)
                                    | ((u4_wt_ofst_u >> 16) & 0xFF);

                    u4_wt_ofst_u = (UWORD32)(pu4_weight_ofst[3]);
                    u4_wt_ofst_v = (UWORD32)(pu4_weight_ofst[5]);
                    weight2 = ((u4_wt_ofst_v & 0xffff) << 16)
                                    | (u4_wt_ofst_u & 0xffff);
                    ofst2 = ((u4_wt_ofst_v >> 16) << 8)
                                    | ((u4_wt_ofst_u >> 16) & 0xFF);

                    ps_dec->pf_weighted_bi_pred_chroma(
                                    (ps_pred_y_forw + 1)->pu1_rec_y_u,
                                    (ps_pred_y_back + 1)->pu1_rec_y_u,
                                    (ps_pred_y_forw + 1)->pu1_rec_y_u,
                                    u2_row_buf_wd_uv, u2_dst_wd,
                                    u2_row_buf_wd_uv, (u2_log2Y_crwd >> 8),
                                    weight1, weight2, ofst1, ofst2,
                                    u4_ht_y >> 1, u4_wd_y >> 1);
                }

                    break;
            }

        }
    }
}


/*!
 **************************************************************************
 * \if Function name : ih264d_multiplex_ref_data \endif
 *
 * \brief
 *    Initializes forward and backward refernce lists for B slice decoding.
 *
 *
 * \return
 *    0 on Success and Error code otherwise
 **************************************************************************
 */

void ih264d_multiplex_ref_data(dec_struct_t * ps_dec,
                               pred_info_t *ps_pred,
                               UWORD8* pu1_dest_y,
                               UWORD8* pu1_dest_u,
                               dec_mb_info_t *ps_cur_mb_info,
                               UWORD16 u2_dest_wd_y,
                               UWORD16 u2_dest_wd_uv,
                               UWORD8 u1_dir)
{
    UWORD16 u2_mask = ps_cur_mb_info->u2_mask[u1_dir];
    UWORD8 *pu1_ref_y, *pu1_ref_u;
    UWORD8 uc_cond, i, j, u1_dydx;
    UWORD16 u2_ref_wd_y, u2_ref_wd_uv;

    PROFILE_DISABLE_INTER_PRED()

    if(ps_pred->i1_pod_ht)
    {
        pu1_ref_y = ps_pred->pu1_dma_dest_addr;

        u2_ref_wd_y = ps_pred->u2_u1_ref_buf_wd;
    }
    else
    {
        pu1_ref_y = ps_pred->pu1_y_ref;
        u2_ref_wd_y = ps_pred->u2_frm_wd;
    }

    ps_pred++;
    if(ps_pred->i1_pod_ht)
    {
        pu1_ref_u = ps_pred->pu1_dma_dest_addr;
        u2_ref_wd_uv = ps_pred->u2_u1_ref_buf_wd * YUV420SP_FACTOR;

    }
    else
    {
        pu1_ref_u = ps_pred->pu1_u_ref;
        u2_ref_wd_uv = ps_pred->u2_frm_wd;

    }

    u1_dydx = ps_pred->u1_dydx;

    {
        UWORD8 uc_dx, uc_dy;
        UWORD8 *pu1_scratch_u;

        uc_dx = u1_dydx & 0x3;
        uc_dy = u1_dydx >> 3;
        if(u1_dydx != 0)
        {
            pred_info_t * ps_prv_pred = ps_pred - 2;
            pu1_scratch_u = ps_prv_pred->pu1_dma_dest_addr;
            ps_dec->pf_inter_pred_chroma(pu1_ref_u, pu1_scratch_u,
                                         u2_ref_wd_uv, 16, uc_dx, uc_dy, 8,
                                         8);

            /* Modify ref pointer and refWidth to point to scratch    */
            /* buffer to be used below in ih264d_copy_multiplex_data functions */
            /* CHANGED CODE */
            pu1_ref_u = pu1_scratch_u;
            u2_ref_wd_uv = 8 * YUV420SP_FACTOR;
        }
    }
    {
        for(i = 0; i < 4; i++)
        {
            for(j = 0; j < 4; j++)
            {
                uc_cond = u2_mask & 1;
                u2_mask >>= 1;
                if(uc_cond)
                {
                    *(UWORD32 *)(pu1_dest_y + u2_dest_wd_y) =
                                    *(UWORD32 *)(pu1_ref_y + u2_ref_wd_y);
                    *(UWORD32 *)(pu1_dest_y + 2 * u2_dest_wd_y) =
                                    *(UWORD32 *)(pu1_ref_y + 2 * u2_ref_wd_y);
                    *(UWORD32 *)(pu1_dest_y + 3 * u2_dest_wd_y) =
                                    *(UWORD32 *)(pu1_ref_y + 3 * u2_ref_wd_y);
                    {
                        UWORD32 *dst, *src;
                        dst = (UWORD32 *)pu1_dest_y;
                        src = (UWORD32 *)pu1_ref_y;
                        *dst = *src;
                        dst++;
                        src++;
                        pu1_dest_y = (UWORD8 *)dst;
                        pu1_ref_y = (UWORD8 *)src;
                    }
                    *(UWORD32 *)(pu1_dest_u + u2_dest_wd_uv) =
                                    *(UWORD32 *)(pu1_ref_u + u2_ref_wd_uv);
                    {
                        UWORD32 *dst, *src;
                        dst = (UWORD32 *)pu1_dest_u;
                        src = (UWORD32 *)pu1_ref_u;
                        *dst = *src;
                        dst++;
                        src++;
                        pu1_dest_u = (UWORD8 *)dst;
                        pu1_ref_u = (UWORD8 *)src;
                    }

                }
                else
                {
                    pu1_dest_y += 4;
                    pu1_ref_y += 4;
                    pu1_dest_u += 2 * YUV420SP_FACTOR;
                    pu1_ref_u += 2 * YUV420SP_FACTOR;
                }
            }
            pu1_ref_y += 4 * (u2_ref_wd_y - 4);
            pu1_ref_u += 2 * (u2_ref_wd_uv - 4 * YUV420SP_FACTOR);
            pu1_dest_y += 4 * (u2_dest_wd_y - 4);
            pu1_dest_u += 2 * (u2_dest_wd_uv - 4 * YUV420SP_FACTOR);
        }
    }
}

void ih264d_pad_on_demand(pred_info_t *ps_pred, UWORD8 lum_chrom_blk)
{
    if(CHROM_BLK == lum_chrom_blk)
    {
        UWORD32 *pu4_pod_src_u, *pu4_pod_dst_u;
        UWORD32 *pu4_pod_src_v, *pu4_pod_dst_v;
        WORD32 j, u1_wd_stride;
        WORD32 i, u1_dma_ht, i1_ht;
        UWORD32 u2_dma_size;
        u1_wd_stride = (ps_pred->u2_u1_ref_buf_wd >> 2) * YUV420SP_FACTOR;
        u1_dma_ht = ps_pred->i1_dma_ht;
        u2_dma_size = u1_wd_stride * u1_dma_ht;
        pu4_pod_src_u = (UWORD32 *)ps_pred->pu1_dma_dest_addr;
        pu4_pod_dst_u = pu4_pod_src_u;

        pu4_pod_src_v = pu4_pod_src_u + u2_dma_size;
        pu4_pod_dst_v = pu4_pod_src_v;

        i1_ht = ps_pred->i1_pod_ht;
        pu4_pod_src_u -= u1_wd_stride * i1_ht;
        pu4_pod_src_v -= u1_wd_stride * i1_ht;
        if(i1_ht < 0)
            /* Top POD */
            i1_ht = -i1_ht;
        else
        {
            /* Bottom POD */
            pu4_pod_src_u += (u1_dma_ht - 1) * u1_wd_stride;
            pu4_pod_dst_u += (u1_dma_ht - i1_ht) * u1_wd_stride;
            pu4_pod_src_v += (u1_dma_ht - 1) * u1_wd_stride;
            pu4_pod_dst_v += (u1_dma_ht - i1_ht) * u1_wd_stride;
        }

        for(i = 0; i < i1_ht; i++)
            for(j = 0; j < u1_wd_stride; j++)
            {
                *pu4_pod_dst_u++ = *(pu4_pod_src_u + j);

            }
    }
    else
    {
        UWORD32 *pu4_pod_src, *pu4_pod_dst;
        WORD32 j, u1_wd_stride;
        WORD32 i, i1_ht;
        pu4_pod_src = (UWORD32 *)ps_pred->pu1_dma_dest_addr;
        pu4_pod_dst = pu4_pod_src;
        u1_wd_stride = ps_pred->u2_u1_ref_buf_wd >> 2;
        i1_ht = ps_pred->i1_pod_ht;
        pu4_pod_src -= u1_wd_stride * i1_ht;
        if(i1_ht < 0)
            /* Top POD */
            i1_ht = -i1_ht;
        else
        {
            /* Bottom POD */
            pu4_pod_src += (ps_pred->i1_dma_ht - 1) * u1_wd_stride;
            pu4_pod_dst += (ps_pred->i1_dma_ht - i1_ht) * u1_wd_stride;
        }

        for(i = 0; i < i1_ht; i++)
            for(j = 0; j < u1_wd_stride; j++)
                *pu4_pod_dst++ = *(pu4_pod_src + j);
    }
}

