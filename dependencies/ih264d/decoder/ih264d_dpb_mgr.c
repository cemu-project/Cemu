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
#ifdef __ANDROID__
#include <log/log.h>
#endif
#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "iv.h"
#include "ih264d_dpb_manager.h"
#include "ih264d_bitstrm.h"
#include "ih264d_parse_cavlc.h"
#include "ih264d_defs.h"
#include "ih264d_structs.h"
#include "ih264d_process_bslice.h"
#include "ih264d_debug.h"
#include "ih264d_tables.h"
#include "ih264d_error_handler.h"
#include "string.h"
#include "ih264d_defs.h"
#include "ih264_error.h"
#include "ih264_buf_mgr.h"
#include "assert.h"

/*!
 ***************************************************************************
 * \file ih264d_dpb_mgr.c
 *
 * \brief
 *    Functions for managing the decoded picture buffer
 *
 * Detailed_description
 *
 * \date
 *    19-12-2002
 *
 * \author  Sriram Sethuraman
 ***************************************************************************
 */

/*!
 **************************************************************************
 * \if Function name : ih264d_init_ref_bufs \endif
 *
 * \brief
 *    Called at the start for initialization.
 *
 * \return
 *    none
 **************************************************************************
 */
void ih264d_init_ref_bufs(dpb_manager_t *ps_dpb_mgr)
{
    UWORD32 i;
    struct dpb_info_t *ps_dpb_info = ps_dpb_mgr->as_dpb_info;
    for(i = 0; i < MAX_REF_BUFS; i++)
    {
        ps_dpb_info[i].u1_used_as_ref = UNUSED_FOR_REF;
        ps_dpb_info[i].u1_lt_idx = MAX_REF_BUFS + 1;
        ps_dpb_info[i].ps_prev_short = NULL;
        ps_dpb_info[i].ps_prev_long = NULL;
        ps_dpb_info[i].ps_pic_buf = NULL;
        ps_dpb_info[i].s_top_field.u1_reference_info = UNUSED_FOR_REF;
        ps_dpb_info[i].s_bot_field.u1_reference_info = UNUSED_FOR_REF;
        ps_dpb_info[i].s_top_field.u1_long_term_frame_idx = MAX_REF_BUFS + 1;
        ps_dpb_info[i].s_bot_field.u1_long_term_frame_idx = MAX_REF_BUFS + 1;

    }
    ps_dpb_mgr->u1_num_st_ref_bufs = ps_dpb_mgr->u1_num_lt_ref_bufs = 0;
    ps_dpb_mgr->ps_dpb_st_head = NULL;
    ps_dpb_mgr->ps_dpb_ht_head = NULL;
    ps_dpb_mgr->i1_gaps_deleted = 0;
    ps_dpb_mgr->i1_poc_buf_id_entries = 0;
    ps_dpb_mgr->u1_mmco_error_in_seq = 0;

    ps_dpb_mgr->u1_num_gaps = 0;
    for(i = 0; i < MAX_FRAMES; i++)
    {
        ps_dpb_mgr->ai4_gaps_start_frm_num[i] = INVALID_FRAME_NUM;
        ps_dpb_mgr->ai4_gaps_end_frm_num[i] = 0;
        ps_dpb_mgr->ai1_gaps_per_seq[i] = 0;
        ps_dpb_mgr->ai4_poc_buf_id_map[i][0] = -1;
        ps_dpb_mgr->ai4_poc_buf_id_map[i][1] = 0x7fffffff;
        ps_dpb_mgr->ai4_poc_buf_id_map[i][2] = 0;
    }

}

void ih264d_free_ref_pic_mv_bufs(void* pv_dec, UWORD8 pic_buf_id)
{
    dec_struct_t *ps_dec = (dec_struct_t *)pv_dec;

    if((pic_buf_id == ps_dec->u1_pic_buf_id) &&
                    ps_dec->ps_cur_slice->u1_field_pic_flag &&
                    (ps_dec->u1_top_bottom_decoded == 0))
    {
        return;
    }

    ih264_buf_mgr_release((buf_mgr_t *)ps_dec->pv_pic_buf_mgr,
                          pic_buf_id,
                          BUF_MGR_REF);
    ih264_buf_mgr_release((buf_mgr_t *)ps_dec->pv_mv_buf_mgr,
                          ps_dec->au1_pic_buf_id_mv_buf_id_map[pic_buf_id],
                          BUF_MGR_REF);
}
/*!
 **************************************************************************
 * \if Function name : ih264d_delete_lt_node \endif
 *
 * \brief
 *    Delete a buffer with a long term index from the LT linked list
 *
 * \return
 *    none
 **************************************************************************
 */
WORD32 ih264d_delete_lt_node(dpb_manager_t *ps_dpb_mgr,
                             UWORD32 u4_lt_idx,
                             UWORD8 u1_fld_pic_flag,
                             struct dpb_info_t *ps_lt_node_to_insert,
                             WORD32 *pi4_status)
{
    *pi4_status = 0;
    if(ps_dpb_mgr->u1_num_lt_ref_bufs > 0)
    {
        WORD32 i;
        struct dpb_info_t *ps_next_dpb;
        /* ps_unmark_node points to the node to be removed */
        /* from long term list.                            */
        struct dpb_info_t *ps_unmark_node;
        //Find the node with matching LTIndex
        ps_next_dpb = ps_dpb_mgr->ps_dpb_ht_head;
        if(ps_next_dpb->u1_lt_idx == u4_lt_idx)
        {
            ps_unmark_node = ps_next_dpb;
        }
        else
        {
            for(i = 1; i < ps_dpb_mgr->u1_num_lt_ref_bufs; i++)
            {
                if(ps_next_dpb->ps_prev_long->u1_lt_idx == u4_lt_idx)
                    break;
                ps_next_dpb = ps_next_dpb->ps_prev_long;
            }
            if(i == ps_dpb_mgr->u1_num_lt_ref_bufs)
                *pi4_status = 1;
            else
                ps_unmark_node = ps_next_dpb->ps_prev_long;
        }

        if(*pi4_status == 0)
        {
            if(u1_fld_pic_flag)
            {
                if(ps_lt_node_to_insert != ps_unmark_node)
                {
                    UWORD8 u1_deleted = 0;
                    /* for the ps_unmark_node mark the corresponding field */
                    /* field as unused for reference                       */

                    if(ps_unmark_node->s_top_field.u1_long_term_frame_idx
                                    == u4_lt_idx)
                    {
                        ps_unmark_node->s_top_field.u1_reference_info =
                                        UNUSED_FOR_REF;
                        ps_unmark_node->s_top_field.u1_long_term_frame_idx =
                        MAX_REF_BUFS + 1;
                        u1_deleted = 1;
                    }
                    if(ps_unmark_node->s_bot_field.u1_long_term_frame_idx
                                    == u4_lt_idx)
                    {
                        ps_unmark_node->s_bot_field.u1_reference_info =
                                        UNUSED_FOR_REF;
                        ps_unmark_node->s_bot_field.u1_long_term_frame_idx =
                        MAX_REF_BUFS + 1;
                        u1_deleted = 1;
                    }

                    if(!u1_deleted)
                    {

                        UWORD32 i4_error_code;
                        i4_error_code = ERROR_DBP_MANAGER_T;

                        return i4_error_code;
                    }
                }

                ps_unmark_node->u1_used_as_ref =
                                ps_unmark_node->s_top_field.u1_reference_info
                                                | ps_unmark_node->s_bot_field.u1_reference_info;
            }
            else
                ps_unmark_node->u1_used_as_ref = UNUSED_FOR_REF;

            if(UNUSED_FOR_REF == ps_unmark_node->u1_used_as_ref)
            {
                if(ps_unmark_node == ps_dpb_mgr->ps_dpb_ht_head)
                    ps_dpb_mgr->ps_dpb_ht_head = ps_next_dpb->ps_prev_long;

                ps_unmark_node->u1_lt_idx = MAX_REF_BUFS + 1;
                ps_unmark_node->s_top_field.u1_reference_info =
                UNUSED_FOR_REF;
                ps_unmark_node->s_bot_field.u1_reference_info =
                UNUSED_FOR_REF;
                // Release the physical buffer
                ih264d_free_ref_pic_mv_bufs(ps_dpb_mgr->pv_codec_handle,
                                            ps_unmark_node->u1_buf_id);
                ps_next_dpb->ps_prev_long = ps_unmark_node->ps_prev_long; //update link
                ps_unmark_node->ps_prev_long = NULL;
                ps_dpb_mgr->u1_num_lt_ref_bufs--; //decrement LT buf count
            }
        }
    }
    return OK;
}

/*!
 **************************************************************************
 * \if Function name : ih264d_insert_lt_node \endif
 *
 * \brief
 *    Insert a buffer into the LT linked list at a given LT index
 *
 * \return
 *    none
 **************************************************************************
 */
WORD32 ih264d_insert_lt_node(dpb_manager_t *ps_dpb_mgr,
                           struct dpb_info_t *ps_mov_node,
                           UWORD32 u4_lt_idx,
                           UWORD8 u1_fld_pic_flag)
{
    UWORD8 u1_mark_top_field_long_term = 0;
    UWORD8 u1_mark_bot_field_long_term = 0;

    {
        if(u1_fld_pic_flag)
        {
            /* Assign corresponding field (top or bottom) long_term_frame_idx */

            if((ps_mov_node->s_top_field.u1_reference_info == IS_LONG_TERM)
                            && (ps_mov_node->s_bot_field.u1_reference_info
                                            == IS_LONG_TERM))
            {
                if(ps_mov_node->u1_lt_idx == u4_lt_idx)
                    u1_mark_bot_field_long_term = 1;
                else
                {

                    UWORD32 i4_error_code;
                    i4_error_code = ERROR_DBP_MANAGER_T;

                    return i4_error_code;

                }
            }
            else if(ps_mov_node->s_top_field.u1_reference_info == IS_LONG_TERM)
            {
                u1_mark_top_field_long_term = 1;
            }

            if(!(u1_mark_top_field_long_term || u1_mark_bot_field_long_term))
            {
                UWORD32 i4_error_code;
                i4_error_code = ERROR_DBP_MANAGER_T;
                return i4_error_code;
            }
        }
        else
        {
            ps_mov_node->s_top_field.u1_reference_info = IS_LONG_TERM;
            ps_mov_node->s_bot_field.u1_reference_info = IS_LONG_TERM;
            ps_mov_node->s_top_field.u1_long_term_frame_idx = u4_lt_idx;
            ps_mov_node->s_bot_field.u1_long_term_frame_idx = u4_lt_idx;
            u1_mark_bot_field_long_term = 1;
            u1_mark_top_field_long_term = 1;
        }

        ps_mov_node->u1_lt_idx = u4_lt_idx; //Assign the LT index to the node
        ps_mov_node->ps_pic_buf->u1_long_term_frm_idx = u4_lt_idx;
        ps_mov_node->u1_used_as_ref = IS_LONG_TERM;

        /* Insert the new long term in the LT list with  u4_lt_idx    */
        /* in ascending order.                                         */
        if(ps_dpb_mgr->u1_num_lt_ref_bufs > 0)
        {
            struct dpb_info_t *ps_next_dpb = ps_dpb_mgr->ps_dpb_ht_head;
            if(u4_lt_idx < ps_next_dpb->u1_lt_idx)
            {
                //LTIndex to be inserted is the smallest LT index
                //Update head and point prev to the next higher index
                ps_mov_node->ps_prev_long = ps_next_dpb;
                ps_dpb_mgr->ps_dpb_ht_head = ps_mov_node;
            }
            else
            {
                WORD32 i;
                struct dpb_info_t *ps_nxtDPB = ps_next_dpb;
                ps_next_dpb = ps_next_dpb->ps_prev_long;
                for(i = 1; i < ps_dpb_mgr->u1_num_lt_ref_bufs; i++)
                {
                    if(ps_next_dpb->u1_lt_idx > u4_lt_idx)
                        break;
                    ps_nxtDPB = ps_next_dpb;
                    ps_next_dpb = ps_next_dpb->ps_prev_long;
                }

                ps_nxtDPB->ps_prev_long = ps_mov_node;
                ps_mov_node->ps_prev_long = ps_next_dpb;
            }
        }
        else
        {
            ps_dpb_mgr->ps_dpb_ht_head = ps_mov_node;
            ps_mov_node->ps_prev_long = NULL;
        }
        /* Identify the picture buffer as a long term picture buffer */
        ps_mov_node->ps_pic_buf->u1_is_short = 0;

        /* Increment LT buf count only if new LT node inserted    */
        /* If Increment during top_field is done, don't increment */
        /* for bottom field, as both them are part of same pic.   */
        if(u1_mark_bot_field_long_term)
            ps_dpb_mgr->u1_num_lt_ref_bufs++;

    }
    return OK;
}

/*!
 **************************************************************************
 * \if Function name : ih264d_insert_st_node \endif
 *
 * \brief
 *    Adds a short term reference picture into the ST linked list
 *
 * \return
 *    None
 *
 * \note
 *    Called only for a new coded picture with nal_ref_idc!=0
 **************************************************************************
 */
WORD32 ih264d_insert_st_node(dpb_manager_t *ps_dpb_mgr,
                          struct pic_buffer_t *ps_pic_buf,
                          UWORD8 u1_buf_id,
                          UWORD32 u4_cur_pic_num)
{
    WORD32 i;
    struct dpb_info_t *ps_dpb_info = ps_dpb_mgr->as_dpb_info;
    UWORD8 u1_picture_type = ps_pic_buf->u1_picturetype;
    /* Find an unused dpb location */
    for(i = 0; i < MAX_REF_BUFS; i++)
    {
        if((ps_dpb_info[i].ps_pic_buf == ps_pic_buf)
                        && ps_dpb_info[i].u1_used_as_ref)
        {
            /*signal an error in the case of frame pic*/
            if(ps_dpb_info[i].ps_pic_buf->u1_pic_type == FRM_PIC)
            {
                return ERROR_DBP_MANAGER_T;
            }
            else
            {
                /* Can occur only for field bottom pictures */
                ps_dpb_info[i].s_bot_field.u1_reference_info = IS_SHORT_TERM;
                return OK;
            }
        }

        if((ps_dpb_info[i].u1_used_as_ref == UNUSED_FOR_REF)
                        && (ps_dpb_info[i].s_top_field.u1_reference_info
                                        == UNUSED_FOR_REF)
                        && (ps_dpb_info[i].s_bot_field.u1_reference_info
                                        == UNUSED_FOR_REF))
            break;
    }
    if(i == MAX_REF_BUFS)
    {
        UWORD32 i4_error_code;
        i4_error_code = ERROR_DBP_MANAGER_T;
        return i4_error_code;
    }

    /* Create dpb info */
    ps_dpb_info[i].ps_pic_buf = ps_pic_buf;
    ps_dpb_info[i].ps_prev_short = ps_dpb_mgr->ps_dpb_st_head;
    ps_dpb_info[i].u1_buf_id = u1_buf_id;
    ps_dpb_info[i].u1_used_as_ref = TRUE;
    ps_dpb_info[i].u1_lt_idx = MAX_REF_BUFS + 1;
    ps_dpb_info[i].i4_frame_num = u4_cur_pic_num;
    ps_dpb_info[i].ps_pic_buf->i4_frame_num = u4_cur_pic_num;

    /* update the head node of linked list to point to the cur Pic */
    ps_dpb_mgr->ps_dpb_st_head = ps_dpb_info + i;

    // Increment Short term bufCount
    ps_dpb_mgr->u1_num_st_ref_bufs++;
    /* Identify the picture as a short term picture buffer */
    ps_pic_buf->u1_is_short = IS_SHORT_TERM;

    if((u1_picture_type & 0x03) == FRM_PIC)
    {
        ps_dpb_info[i].u1_used_as_ref = IS_SHORT_TERM;
        ps_dpb_info[i].s_top_field.u1_reference_info = IS_SHORT_TERM;
        ps_dpb_info[i].s_bot_field.u1_reference_info = IS_SHORT_TERM;
    }

    if((u1_picture_type & 0x03) == TOP_FLD)
        ps_dpb_info[i].s_top_field.u1_reference_info = IS_SHORT_TERM;

    if((u1_picture_type & 0x03) == BOT_FLD)
        ps_dpb_info[i].s_bot_field.u1_reference_info = IS_SHORT_TERM;

    return OK;
}

/*!
 **************************************************************************
 * \if Function name : ih264d_delete_st_node_or_make_lt \endif
 *
 * \brief
 *    Delete short term ref with a given picNum from the ST linked list or
 *     make it an LT node
 *
 * \return
 *    0 - if successful; -1 - otherwise
 *
 * \note
 *    Common parts to MMCO==1 and MMCO==3 have been combined here
 **************************************************************************
 */
WORD32 ih264d_delete_st_node_or_make_lt(dpb_manager_t *ps_dpb_mgr,
                                      WORD32 i4_pic_num,
                                      UWORD32 u4_lt_idx,
                                      UWORD8 u1_fld_pic_flag)
{
    WORD32 i;
    struct dpb_info_t *ps_next_dpb;
    WORD32 i4_frame_num = i4_pic_num;
    struct dpb_info_t *ps_unmark_node = NULL;
    UWORD8 u1_del_node = 0, u1_del_st = 0;
    UWORD8 u1_reference_type = UNUSED_FOR_REF;
    WORD32 ret;

    if(u1_fld_pic_flag)
    {
        i4_frame_num = i4_frame_num >> 1;

        if(u4_lt_idx == (MAX_REF_BUFS + 1))
            u1_reference_type = UNUSED_FOR_REF;
        else
            u1_reference_type = IS_LONG_TERM;
    }

    //Find the node with matching picNum
    ps_next_dpb = ps_dpb_mgr->ps_dpb_st_head;
    if((WORD32)ps_next_dpb->i4_frame_num == i4_frame_num)
    {
        ps_unmark_node = ps_next_dpb;
    }
    else
    {
        for(i = 1; i < ps_dpb_mgr->u1_num_st_ref_bufs; i++)
        {
            if((WORD32)ps_next_dpb->ps_prev_short->i4_frame_num == i4_frame_num)
                break;
            ps_next_dpb = ps_next_dpb->ps_prev_short;
        }

        if(i == ps_dpb_mgr->u1_num_st_ref_bufs)
        {
            if(ps_dpb_mgr->u1_num_gaps)
            {
                ret = ih264d_delete_gap_frm_mmco(ps_dpb_mgr, i4_frame_num, &u1_del_st);
                if(ret != OK)
                    return ret;
            }
            else
            {
                UWORD32 i4_error_code;
                i4_error_code = ERROR_DBP_MANAGER_T;

                return i4_error_code;
            }

            if(u1_del_st)
            {
                UWORD32 i4_error_code;
                i4_error_code = ERROR_DBP_MANAGER_T;
                return i4_error_code;
            }
            else
            {
                return 0;
            }
        }
        else
            ps_unmark_node = ps_next_dpb->ps_prev_short;
    }

    if(u1_fld_pic_flag)
    {
        /* Mark the corresponding field ( top or bot) as  */
        /* UNUSED_FOR_REF or IS_LONG_TERM depending on    */
        /* u1_reference_type.                             */
        if(ps_unmark_node->s_top_field.i4_pic_num == i4_pic_num)
        {
            ps_unmark_node->s_top_field.u1_reference_info = u1_reference_type;
            ps_unmark_node->s_top_field.u1_long_term_frame_idx = u4_lt_idx;
            {
                UWORD8 *pu1_src = ps_unmark_node->ps_pic_buf->pu1_col_zero_flag;
                WORD32 i4_size = ((ps_dpb_mgr->u2_pic_wd
                                * ps_dpb_mgr->u2_pic_ht) >> 5);
                /* memset the colocated zero u4_flag buffer */
                memset(pu1_src, 0, i4_size);
            }
        }

        else if(ps_unmark_node->s_bot_field.i4_pic_num == i4_pic_num)
        {

            ps_unmark_node->s_bot_field.u1_reference_info = u1_reference_type;
            ps_unmark_node->s_bot_field.u1_long_term_frame_idx = u4_lt_idx;
            {
                UWORD8 *pu1_src =
                                ps_unmark_node->ps_pic_buf->pu1_col_zero_flag
                                                + ((ps_dpb_mgr->u2_pic_wd
                                                                * ps_dpb_mgr->u2_pic_ht)
                                                                >> 5);
                WORD32 i4_size = ((ps_dpb_mgr->u2_pic_wd
                                * ps_dpb_mgr->u2_pic_ht) >> 5);
                /* memset the colocated zero u4_flag buffer */
                memset(pu1_src, 0, i4_size);
            }
        }
        ps_unmark_node->u1_used_as_ref =
                        ps_unmark_node->s_top_field.u1_reference_info
                                        | ps_unmark_node->s_bot_field.u1_reference_info;
    }
    else
    {
        ps_unmark_node->u1_used_as_ref = UNUSED_FOR_REF;
        ps_unmark_node->s_top_field.u1_reference_info = UNUSED_FOR_REF;
        ps_unmark_node->s_bot_field.u1_reference_info = UNUSED_FOR_REF;

        {
            UWORD8 *pu1_src = ps_unmark_node->ps_pic_buf->pu1_col_zero_flag;

            WORD32 i4_size = ((ps_dpb_mgr->u2_pic_wd
                            * ps_dpb_mgr->u2_pic_ht) >> 4);
            /* memset the colocated zero u4_flag buffer */
            memset(pu1_src, 0, i4_size);
        }
    }

    if(!(ps_unmark_node->u1_used_as_ref & IS_SHORT_TERM))
    {
        if(ps_unmark_node == ps_dpb_mgr->ps_dpb_st_head)
            ps_dpb_mgr->ps_dpb_st_head = ps_next_dpb->ps_prev_short;
        else
            ps_next_dpb->ps_prev_short = ps_unmark_node->ps_prev_short; //update link
        ps_dpb_mgr->u1_num_st_ref_bufs--; //decrement ST buf count
        u1_del_node = 1;
    }

    if(u4_lt_idx == MAX_REF_BUFS + 1)
    {
        if(u1_del_node)
        {
            // Release the physical buffer
            ih264d_free_ref_pic_mv_bufs(ps_dpb_mgr->pv_codec_handle,
                                        ps_unmark_node->u1_buf_id);
            ps_unmark_node->ps_prev_short = NULL;
        }
    }
    else
    {
        WORD32 i4_status;
        //If another node has the same LT index, delete that node
        ret = ih264d_delete_lt_node(ps_dpb_mgr, u4_lt_idx,
                              u1_fld_pic_flag, ps_unmark_node, &i4_status);
        if(ret != OK)
            return ret;
        // Now insert the short term node as a long term node
        ret = ih264d_insert_lt_node(ps_dpb_mgr, ps_unmark_node, u4_lt_idx,
                              u1_fld_pic_flag);
        if(ret != OK)
            return ret;
    }
    return OK;
}
/*!
 **************************************************************************
 * \if Function name : ih264d_reset_ref_bufs \endif
 *
 * \brief
 *    Called if MMCO==5/7 or on the first slice of an IDR picture
 *
 * \return
 *    none
 **************************************************************************
 */
void ih264d_reset_ref_bufs(dpb_manager_t *ps_dpb_mgr)
{
    WORD32 i;
    struct dpb_info_t *ps_dpb_info = ps_dpb_mgr->as_dpb_info;

    for(i = 0; i < MAX_REF_BUFS; i++)
    {
        if(ps_dpb_info[i].u1_used_as_ref)
        {
            ps_dpb_info[i].u1_used_as_ref = UNUSED_FOR_REF;
            ps_dpb_info[i].u1_lt_idx = MAX_REF_BUFS + 1;
            ps_dpb_info[i].ps_prev_short = NULL;
            ps_dpb_info[i].ps_prev_long = NULL;
            ps_dpb_info[i].ps_pic_buf = NULL;
            ps_dpb_info[i].s_top_field.u1_reference_info = UNUSED_FOR_REF;
            ps_dpb_info[i].s_bot_field.u1_reference_info = UNUSED_FOR_REF;
            ps_dpb_info[i].s_top_field.u1_long_term_frame_idx = MAX_REF_BUFS + 1;
            ps_dpb_info[i].s_bot_field.u1_long_term_frame_idx = MAX_REF_BUFS + 1;

            //Release physical buffer
            ih264d_free_ref_pic_mv_bufs(ps_dpb_mgr->pv_codec_handle,
                                        ps_dpb_info[i].u1_buf_id);
        }
    }
    ps_dpb_mgr->u1_num_st_ref_bufs = ps_dpb_mgr->u1_num_lt_ref_bufs = 0;
    ps_dpb_mgr->ps_dpb_st_head = NULL;
    ps_dpb_mgr->ps_dpb_ht_head = NULL;
    ps_dpb_mgr->u1_mmco_error_in_seq = 0;

    /* release all gaps */
    ps_dpb_mgr->u1_num_gaps = 0;
    for(i = 0; i < MAX_FRAMES; i++)
    {
        ps_dpb_mgr->ai4_gaps_start_frm_num[i] = INVALID_FRAME_NUM;
        ps_dpb_mgr->ai4_gaps_end_frm_num[i] = 0;
        ps_dpb_mgr->ai1_gaps_per_seq[i] = 0;
    }
}

/*!
 **************************************************************************
 * \if Function name : Name \endif
 *
 * \brief
 *     create the default index list after an MMCO
 *
 * \return
 *    0 - if no_error; -1 - error
 *
 **************************************************************************
 */
WORD32 ih264d_update_default_index_list(dpb_manager_t *ps_dpb_mgr)
{
    WORD32 i;
    struct dpb_info_t *ps_next_dpb = ps_dpb_mgr->ps_dpb_st_head;

    for(i = 0; i < ps_dpb_mgr->u1_num_st_ref_bufs; i++)
    {
        ps_dpb_mgr->ps_def_dpb[i] = ps_next_dpb->ps_pic_buf;
        ps_next_dpb = ps_next_dpb->ps_prev_short;
    }

    ps_next_dpb = ps_dpb_mgr->ps_dpb_ht_head;
    for(;i< ps_dpb_mgr->u1_num_st_ref_bufs + ps_dpb_mgr->u1_num_lt_ref_bufs; i++)
    {
        ps_dpb_mgr->ps_def_dpb[i] = ps_next_dpb->ps_pic_buf;
        ps_next_dpb = ps_next_dpb->ps_prev_long;
    }
    return 0;
}

/*!
 **************************************************************************
 * \if Function name : ref_idx_reordering \endif
 *
 * \brief
 *     Parse the bitstream and reorder indices for the current slice
 *
 * \return
 *    0 - if no_error; -1 - error
 *
 * \note
 *    Called only if ref_idx_reordering_flag_l0 is decoded as 1
 *    Remove error checking for unmatching picNum or LTIndex later (if not needed)
 * \para
 *    This section implements 7.3.3.1 and 8.2.6.4
 *    Uses the default index list as the starting point and
 *    remaps the picNums sent to the next higher index in the
 *    modified list. The unmodified ones are copied from the
 *    default to modified list retaining their order in the default list.
 *
 **************************************************************************
 */
WORD32 ih264d_ref_idx_reordering(dec_struct_t *ps_dec, UWORD8 uc_lx)
{
    dpb_manager_t *ps_dpb_mgr = ps_dec->ps_dpb_mgr;
    UWORD16 u4_cur_pic_num = ps_dec->ps_cur_slice->u2_frame_num;
    /*< Maximum Picture Number Minus 1 */
    UWORD16 ui_max_frame_num =
                    ps_dec->ps_cur_sps->u2_u4_max_pic_num_minus1 + 1;

    WORD32 i, count = 0;
    UWORD32 ui_remapIdc, ui_nextUev;
    WORD16 u2_pred_frame_num = u4_cur_pic_num;
    WORD32 i_temp;
    UWORD16 u2_def_mod_flag = 0; /* Flag to keep track of which indices have been remapped */
    UWORD8 modCount = 0;
    UWORD32 *pu4_bitstrm_buf = ps_dec->ps_bitstrm->pu4_buffer;
    UWORD32 *pu4_bitstrm_ofst = &ps_dec->ps_bitstrm->u4_ofst;
    dec_slice_params_t *ps_cur_slice = ps_dec->ps_cur_slice;
    UWORD8 u1_field_pic_flag = ps_cur_slice->u1_field_pic_flag;

    if(u1_field_pic_flag)
    {
        u4_cur_pic_num = u4_cur_pic_num * 2 + 1;
        ui_max_frame_num = ui_max_frame_num * 2;
    }

    u2_pred_frame_num = u4_cur_pic_num;

    ui_remapIdc = ih264d_uev(pu4_bitstrm_ofst, pu4_bitstrm_buf);

    while((ui_remapIdc != 3)
                    && (count < ps_cur_slice->u1_num_ref_idx_lx_active[uc_lx]))
    {
        ui_nextUev = ih264d_uev(pu4_bitstrm_ofst, pu4_bitstrm_buf);
        if(ui_remapIdc != 2)
        {
            if(ui_nextUev > ui_max_frame_num)
                return ERROR_DBP_MANAGER_T;

            ui_nextUev = ui_nextUev + 1;

            if(ui_remapIdc == 0)
            {
                // diffPicNum is -ve
                i_temp = (WORD32)u2_pred_frame_num - (WORD32)ui_nextUev;
                if(i_temp < 0)
                    i_temp += ui_max_frame_num;
            }
            else
            {
                // diffPicNum is +ve
                i_temp = (WORD32)u2_pred_frame_num + (WORD32)ui_nextUev;
                if(i_temp >= ui_max_frame_num)
                    i_temp -= ui_max_frame_num;
            }
            /* Find the dpb with the matching picNum (picNum==frameNum for framePic) */

            if(i_temp > u4_cur_pic_num)
                i_temp = i_temp - ui_max_frame_num;

            for(i = 0; i < (ps_cur_slice->u1_initial_list_size[uc_lx]); i++)
            {
                if(ps_dpb_mgr->ps_init_dpb[uc_lx][i]->i4_pic_num == i_temp)
                    break;
            }
            if(i == (ps_cur_slice->u1_initial_list_size[uc_lx]))
            {
                UWORD32 i4_error_code;
                i4_error_code = ERROR_DBP_MANAGER_T;
                return i4_error_code;
            }

            u2_def_mod_flag |= (1 << i);
            ps_dpb_mgr->ps_mod_dpb[uc_lx][modCount++] =
                            ps_dpb_mgr->ps_init_dpb[uc_lx][i];
            u2_pred_frame_num = i_temp; //update predictor to be the picNum just obtained
        }
        else //2
        {
            UWORD8 u1_lt_idx;

            if(ui_nextUev > (MAX_REF_BUFS + 1))
                return ERROR_DBP_MANAGER_T;

            u1_lt_idx = (UWORD8)ui_nextUev;

            for(i = 0; i < (ps_cur_slice->u1_initial_list_size[uc_lx]); i++)
            {
                if(!ps_dpb_mgr->ps_init_dpb[uc_lx][i]->u1_is_short)
                {
                    if(ps_dpb_mgr->ps_init_dpb[uc_lx][i]->u1_long_term_pic_num
                                    == u1_lt_idx)
                        break;
                }
            }
            if(i == (ps_cur_slice->u1_initial_list_size[uc_lx]))
            {
                UWORD32 i4_error_code;
                i4_error_code = ERROR_DBP_MANAGER_T;
                return i4_error_code;
            }

            u2_def_mod_flag |= (1 << i);
            ps_dpb_mgr->ps_mod_dpb[uc_lx][modCount++] =
                            ps_dpb_mgr->ps_init_dpb[uc_lx][i];
        }

        ui_remapIdc = ih264d_uev(pu4_bitstrm_ofst, pu4_bitstrm_buf);
        /* Get the remapping_idc - 0/1/2/3 */
        count++;
    }

    //Handle the ref indices that were not remapped
    for(i = 0; i < (ps_cur_slice->u1_num_ref_idx_lx_active[uc_lx]); i++)
    {
        if(!(u2_def_mod_flag & (1 << i)))
            ps_dpb_mgr->ps_mod_dpb[uc_lx][modCount++] =
                            ps_dpb_mgr->ps_init_dpb[uc_lx][i];
    }
    return OK;
}
/*!
 **************************************************************************
 * \if Function name : ih264d_read_mmco_commands \endif
 *
 * \brief
 *    Parses MMCO commands and stores them in a structure for later use.
 *
 * \return
 *    0 - No error; -1 - Error
 *
 * \note
 *    This function stores MMCO commands in structure only for the first time.
 *    In case of MMCO commands being issued for same Picture Number, they are
 *    just parsed and not stored them in the structure.
 *
 **************************************************************************
 */
WORD32 ih264d_read_mmco_commands(struct _DecStruct * ps_dec)
{
    dec_pic_params_t *ps_pps = ps_dec->ps_cur_pps;
    dec_seq_params_t *ps_sps = ps_pps->ps_sps;
    dec_bit_stream_t *ps_bitstrm = ps_dec->ps_bitstrm;
    dpb_commands_t *ps_dpb_cmds = &(ps_dec->s_dpb_cmds_scratch);
    dec_slice_params_t * ps_slice = ps_dec->ps_cur_slice;
    WORD32 j;
    UWORD8 u1_buf_mode;
    struct MMCParams *ps_mmc_params;
    UWORD32 *pu4_bitstrm_buf = ps_dec->ps_bitstrm->pu4_buffer;
    UWORD32 *pu4_bitstrm_ofst = &ps_bitstrm->u4_ofst;
    UWORD32 u4_bit_ofst = ps_dec->ps_bitstrm->u4_ofst;

    ps_slice->u1_mmco_equalto5 = 0;
    {
        if(ps_dec->u1_nal_unit_type == IDR_SLICE_NAL)
        {
            ps_slice->u1_no_output_of_prior_pics_flag =
                            ih264d_get_bit_h264(ps_bitstrm);
            COPYTHECONTEXT("SH: no_output_of_prior_pics_flag",
                            ps_slice->u1_no_output_of_prior_pics_flag);
            ps_slice->u1_long_term_reference_flag = ih264d_get_bit_h264(
                            ps_bitstrm);
            COPYTHECONTEXT("SH: long_term_reference_flag",
                            ps_slice->u1_long_term_reference_flag);
            ps_dpb_cmds->u1_idr_pic = 1;
            ps_dpb_cmds->u1_no_output_of_prior_pics_flag =
                            ps_slice->u1_no_output_of_prior_pics_flag;
            ps_dpb_cmds->u1_long_term_reference_flag =
                            ps_slice->u1_long_term_reference_flag;
        }
        else
        {
            u1_buf_mode = ih264d_get_bit_h264(ps_bitstrm); //0 - sliding window; 1 - arbitrary
            COPYTHECONTEXT("SH: adaptive_ref_pic_buffering_flag", u1_buf_mode);
            ps_dpb_cmds->u1_buf_mode = u1_buf_mode;
            j = 0;

            if(u1_buf_mode == 1)
            {
                UWORD32 u4_mmco;
                UWORD32 u4_diff_pic_num;
                UWORD32 u4_lt_idx, u4_max_lt_idx_plus1;

                u4_mmco = ih264d_uev(pu4_bitstrm_ofst,
                                     pu4_bitstrm_buf);
                while(u4_mmco != END_OF_MMCO)
                {
                    if (j >= MAX_REF_BUFS)
                    {
#ifdef __ANDROID__
                        ALOGE("b/25818142");
                        android_errorWriteLog(0x534e4554, "25818142");
#endif
                        ps_dpb_cmds->u1_num_of_commands = 0;
                        return -1;
                    }
                    ps_mmc_params = &ps_dpb_cmds->as_mmc_params[j];
                    ps_mmc_params->u4_mmco = u4_mmco;
                    switch(u4_mmco)
                    {
                        case MARK_ST_PICNUM_AS_NONREF:
                            u4_diff_pic_num = ih264d_uev(pu4_bitstrm_ofst,
                                                         pu4_bitstrm_buf);
                            //Get absDiffPicnumMinus1
                            ps_mmc_params->u4_diff_pic_num = u4_diff_pic_num;
                            break;

                        case MARK_LT_INDEX_AS_NONREF:
                            u4_lt_idx = ih264d_uev(pu4_bitstrm_ofst,
                                                   pu4_bitstrm_buf);
                            ps_mmc_params->u4_lt_idx = u4_lt_idx;
                            break;

                        case MARK_ST_PICNUM_AS_LT_INDEX:
                            u4_diff_pic_num = ih264d_uev(pu4_bitstrm_ofst,
                                                         pu4_bitstrm_buf);
                            ps_mmc_params->u4_diff_pic_num = u4_diff_pic_num;
                            u4_lt_idx = ih264d_uev(pu4_bitstrm_ofst,
                                                   pu4_bitstrm_buf);
                            ps_mmc_params->u4_lt_idx = u4_lt_idx;
                            break;

                        case SET_MAX_LT_INDEX:
                        {
                            u4_max_lt_idx_plus1 = ih264d_uev(pu4_bitstrm_ofst,
                                                             pu4_bitstrm_buf);
                            if (u4_max_lt_idx_plus1 > ps_sps->u1_num_ref_frames)
                            {
                                /* Invalid max LT ref index */
                                return -1;
                            }
                            ps_mmc_params->u4_max_lt_idx_plus1 = u4_max_lt_idx_plus1;
                            break;
                        }
                        case RESET_REF_PICTURES:
                        {
                            ps_slice->u1_mmco_equalto5 = 1;
                            break;
                        }

                        case SET_LT_INDEX:
                            u4_lt_idx = ih264d_uev(pu4_bitstrm_ofst,
                                                   pu4_bitstrm_buf);
                            ps_mmc_params->u4_lt_idx = u4_lt_idx;
                            break;

                        default:
                            break;
                    }
                    u4_mmco = ih264d_uev(pu4_bitstrm_ofst,
                                         pu4_bitstrm_buf);

                    j++;
                }
                ps_dpb_cmds->u1_num_of_commands = j;
            }
        }
        ps_dpb_cmds->u1_dpb_commands_read = 1;
        ps_dpb_cmds->u1_dpb_commands_read_slc = 1;

    }
    u4_bit_ofst = ps_dec->ps_bitstrm->u4_ofst - u4_bit_ofst;
    return u4_bit_ofst;
}

/*!
 **************************************************************************
 * \if Function name : ih264d_do_mmco_buffer \endif
 *
 * \brief
 *    Perform decoded picture buffer memory management control operations
 *
 * \return
 *    0 - No error; -1 - Error
 *
 * \note
 *    Bitstream is also parsed here to get the MMCOs
 *
 **************************************************************************
 */
WORD32 ih264d_do_mmco_buffer(dpb_commands_t *ps_dpb_cmds,
                          dpb_manager_t *ps_dpb_mgr,
                          UWORD8 u1_numRef_frames_for_seq, /*!< num_ref_frames from active SeqParSet*/
                          UWORD32 u4_cur_pic_num,
                          UWORD32 u2_u4_max_pic_num_minus1,
                          UWORD8 u1_nal_unit_type,
                          struct pic_buffer_t *ps_pic_buf,
                          UWORD8 u1_buf_id,
                          UWORD8 u1_fld_pic_flag,
                          UWORD8 u1_curr_pic_in_err)
{
    WORD32 i;
    UWORD8 u1_buf_mode, u1_marked_lt;
    struct dpb_info_t *ps_next_dpb;
    UWORD8 u1_num_gaps;
    UWORD8 u1_del_node = 1;
    UWORD8 u1_insert_st_pic = 1;
    WORD32 ret;
    UNUSED(u1_nal_unit_type);
    UNUSED(u2_u4_max_pic_num_minus1);
    u1_buf_mode = ps_dpb_cmds->u1_buf_mode; //0 - sliding window; 1 - Adaptive
    u1_marked_lt = 0;
    u1_num_gaps = ps_dpb_mgr->u1_num_gaps;

    if(!u1_buf_mode)
    {
        //Sliding window - implements 8.2.5.3
        if((ps_dpb_mgr->u1_num_st_ref_bufs
                        + ps_dpb_mgr->u1_num_lt_ref_bufs + u1_num_gaps)
                        == u1_numRef_frames_for_seq)
        {
            UWORD8 u1_new_node_flag = 1;
            if((0 == ps_dpb_mgr->u1_num_st_ref_bufs) && (0 == u1_num_gaps))
            {
                UWORD32 i4_error_code;
                i4_error_code = ERROR_DBP_MANAGER_T;
                return i4_error_code;
            }

            // Chase the links to reach the last but one picNum, if available
            ps_next_dpb = ps_dpb_mgr->ps_dpb_st_head;

            if(ps_dpb_mgr->u1_num_st_ref_bufs > 1)
            {
                if(ps_next_dpb->i4_frame_num == (WORD32)u4_cur_pic_num)
                {
                    /* Incase of  filed pictures top_field has been allocated   */
                    /* picture buffer and complementary bottom field pair comes */
                    /* then the sliding window mechanism should not allocate a  */
                    /* new node                                                 */
                    u1_new_node_flag = 0;
                }

                for(i = 1; i < (ps_dpb_mgr->u1_num_st_ref_bufs - 1); i++)
                {
                    if(ps_next_dpb == NULL)
                    {
                        UWORD32 i4_error_code;
                        i4_error_code = ERROR_DBP_MANAGER_T;
                        return i4_error_code;
                    }
                    if(ps_next_dpb->i4_frame_num == (WORD32)u4_cur_pic_num)
                    {
                        /* Incase of  field pictures top_field has been allocated   */
                        /* picture buffer and complementary bottom field pair comes */
                        /* then the sliding window mechanism should not allocate a  */
                        /* new node                                                 */
                        u1_new_node_flag = 0;
                    }
                    ps_next_dpb = ps_next_dpb->ps_prev_short;
                }

                if(ps_next_dpb->ps_prev_short->ps_prev_short != NULL)
                {
                    UWORD32 i4_error_code;
                    i4_error_code = ERROR_DBP_MANAGER_T;
                    return i4_error_code;
                }

                if(u1_new_node_flag)
                {
                    if(u1_num_gaps)
                    {
                        ret = ih264d_delete_gap_frm_sliding(ps_dpb_mgr,
                                                            ps_next_dpb->ps_prev_short->i4_frame_num,
                                                            &u1_del_node);
                        if(ret != OK)
                            return ret;
                    }

                    if(u1_del_node)
                    {
                        ps_dpb_mgr->u1_num_st_ref_bufs--;
                        ps_next_dpb->ps_prev_short->u1_used_as_ref =
                                        UNUSED_FOR_REF;
                        ps_next_dpb->ps_prev_short->s_top_field.u1_reference_info =
                                        UNUSED_FOR_REF;
                        ps_next_dpb->ps_prev_short->s_bot_field.u1_reference_info =
                                        UNUSED_FOR_REF;
                        ih264d_free_ref_pic_mv_bufs(ps_dpb_mgr->pv_codec_handle,
                                                    ps_next_dpb->ps_prev_short->u1_buf_id);
                        ps_next_dpb->ps_prev_short->ps_pic_buf = NULL;
                        ps_next_dpb->ps_prev_short = NULL;
                    }
                }
            }
            else
            {
                if(ps_dpb_mgr->u1_num_st_ref_bufs)
                {
                    ret = ih264d_delete_gap_frm_sliding(ps_dpb_mgr,
                                                       ps_next_dpb->i4_frame_num,
                                                       &u1_del_node);
                    if(ret != OK)
                        return ret;
                    if((ps_next_dpb->i4_frame_num != (WORD32)u4_cur_pic_num)
                                    && u1_del_node)
                    {
                        ps_dpb_mgr->u1_num_st_ref_bufs--;
                        ps_next_dpb->u1_used_as_ref = FALSE;
                        ps_next_dpb->s_top_field.u1_reference_info =
                                        UNUSED_FOR_REF;
                        ps_next_dpb->s_bot_field.u1_reference_info =
                                        UNUSED_FOR_REF;
                        ih264d_free_ref_pic_mv_bufs(ps_dpb_mgr->pv_codec_handle,
                                                    ps_next_dpb->u1_buf_id);
                        ps_next_dpb->ps_pic_buf = NULL;
                        ps_next_dpb->ps_prev_short = NULL;
                        ps_dpb_mgr->ps_dpb_st_head = NULL;
                        ps_next_dpb = NULL;
                    }
                    else if(ps_next_dpb->i4_frame_num == (WORD32)u4_cur_pic_num)
                    {
                        if(u1_curr_pic_in_err)
                        {
                            u1_insert_st_pic = 0;
                        }
                        else if(ps_dpb_mgr->u1_num_st_ref_bufs > 0)
                        {
                            ps_dpb_mgr->u1_num_st_ref_bufs--;
                            ps_next_dpb->u1_used_as_ref = FALSE;
                            ps_next_dpb->s_top_field.u1_reference_info =
                                            UNUSED_FOR_REF;
                            ps_next_dpb->s_bot_field.u1_reference_info =
                                            UNUSED_FOR_REF;
                            ih264d_free_ref_pic_mv_bufs(ps_dpb_mgr->pv_codec_handle,
                                                        ps_next_dpb->u1_buf_id);
                            ps_next_dpb->ps_pic_buf = NULL;
                            ps_next_dpb = NULL;
                        }
                    }
                }
                else
                {
                    ret = ih264d_delete_gap_frm_sliding(ps_dpb_mgr,
                                                        INVALID_FRAME_NUM,
                                                        &u1_del_node);
                    if(ret != OK)
                        return ret;
                    if(u1_del_node)
                    {
                        UWORD32 i4_error_code;
                        i4_error_code = ERROR_DBP_MANAGER_T;
                        return i4_error_code;
                    }
                }
            }
        }
    }
    else
    {
        //Adaptive memory control - implements 8.2.5.4
        UWORD32 u4_mmco;
        UWORD32 u4_diff_pic_num;
        WORD32 i4_pic_num;
        UWORD32 u4_lt_idx;
        WORD32 j;
        struct MMCParams *ps_mmc_params;

        for(j = 0; j < ps_dpb_cmds->u1_num_of_commands; j++)
        {
            ps_mmc_params = &ps_dpb_cmds->as_mmc_params[j];
            u4_mmco = ps_mmc_params->u4_mmco; //Get MMCO

            switch(u4_mmco)
            {
                case MARK_ST_PICNUM_AS_NONREF:
                {

                    {
                        UWORD32 i4_cur_pic_num = u4_cur_pic_num;
                        WORD64 i8_pic_num;
                        u4_diff_pic_num = ps_mmc_params->u4_diff_pic_num; //Get absDiffPicnumMinus1
                        if(u1_fld_pic_flag)
                            i4_cur_pic_num = i4_cur_pic_num * 2 + 1;
                        i8_pic_num = ((WORD64)i4_cur_pic_num - ((WORD64)u4_diff_pic_num + 1));
                        if(IS_OUT_OF_RANGE_S32(i8_pic_num))
                        {
                            return ERROR_DBP_MANAGER_T;
                        }
                        i4_pic_num = i8_pic_num;
                    }

                    if(ps_dpb_mgr->u1_num_st_ref_bufs > 0)
                    {
                        ret = ih264d_delete_st_node_or_make_lt(ps_dpb_mgr,
                                                               i4_pic_num,
                                                               MAX_REF_BUFS + 1,
                                                               u1_fld_pic_flag);
                        if(ret != OK)
                            return ret;
                    }
                    else
                    {
                        UWORD8 u1_dummy;
                        ret = ih264d_delete_gap_frm_mmco(ps_dpb_mgr, i4_pic_num, &u1_dummy);
                        if(ret != OK)
                            return ret;
                    }
                    break;
                }
                case MARK_LT_INDEX_AS_NONREF:
                {
                    WORD32 i4_status;
                    u4_lt_idx = ps_mmc_params->u4_lt_idx; //Get long term index
                    ret = ih264d_delete_lt_node(ps_dpb_mgr,
                                                u4_lt_idx,
                                                u1_fld_pic_flag,
                                                0, &i4_status);
                    if(ret != OK)
                        return ret;
                    if(i4_status)
                    {
                        UWORD32 i4_error_code;
                        i4_error_code = ERROR_DBP_MANAGER_T;
                        return i4_error_code;
                    }
                    break;
                }

                case MARK_ST_PICNUM_AS_LT_INDEX:
                {
                    {
                        UWORD32 i4_cur_pic_num = u4_cur_pic_num;
                        WORD64 i8_pic_num;
                        u4_diff_pic_num = ps_mmc_params->u4_diff_pic_num; //Get absDiffPicnumMinus1
                        if(u1_fld_pic_flag)
                            i4_cur_pic_num = i4_cur_pic_num * 2 + 1;

                        i8_pic_num = (WORD64)i4_cur_pic_num - ((WORD64)u4_diff_pic_num + 1);
                        if(IS_OUT_OF_RANGE_S32(i8_pic_num))
                        {
                            return ERROR_DBP_MANAGER_T;
                        }
                        i4_pic_num = i8_pic_num;
                    }

                    u4_lt_idx = ps_mmc_params->u4_lt_idx; //Get long term index

                    if((ps_dpb_mgr->u1_max_lt_frame_idx == NO_LONG_TERM_INDICIES) ||
                        (u4_lt_idx > ps_dpb_mgr->u1_max_lt_frame_idx))
                    {
                        return ERROR_DBP_MANAGER_T;
                    }

                    if(ps_dpb_mgr->u1_num_st_ref_bufs > 0)
                    {
                        ret = ih264d_delete_st_node_or_make_lt(ps_dpb_mgr,
                                                               i4_pic_num, u4_lt_idx,
                                                               u1_fld_pic_flag);
                        if(ret != OK)
                            return ret;
                    }
                    break;
                }
                case SET_MAX_LT_INDEX:
                {
                    UWORD8 uc_numLT = ps_dpb_mgr->u1_num_lt_ref_bufs;
                    u4_lt_idx = ps_mmc_params->u4_max_lt_idx_plus1; //Get Max_long_term_index_plus1
                    if(u4_lt_idx <= ps_dpb_mgr->u1_max_lt_frame_idx
                                    && uc_numLT > 0)
                    {
                        struct dpb_info_t *ps_nxtDPB;
                        //Set all LT buffers with index >= u4_lt_idx to nonreference
                        ps_nxtDPB = ps_dpb_mgr->ps_dpb_ht_head;
                        ps_next_dpb = ps_nxtDPB->ps_prev_long;
                        if(ps_nxtDPB->u1_lt_idx >= u4_lt_idx)
                        {
                            i = 0;
                            ps_dpb_mgr->ps_dpb_ht_head = NULL;
                        }
                        else
                        {
                            for(i = 1; i < uc_numLT; i++)
                            {
                                if(ps_next_dpb->u1_lt_idx >= u4_lt_idx)
                                    break;
                                ps_nxtDPB = ps_next_dpb;
                                ps_next_dpb = ps_next_dpb->ps_prev_long;
                            }
                            ps_nxtDPB->ps_prev_long = NULL; //Terminate the link of the closest LTIndex that is <=Max
                        }
                        ps_dpb_mgr->u1_num_lt_ref_bufs = i;
                        if(i == 0)
                            ps_next_dpb = ps_nxtDPB;

                        for(; i < uc_numLT; i++)
                        {
                            ps_nxtDPB = ps_next_dpb;
                            ps_nxtDPB->u1_lt_idx = MAX_REF_BUFS + 1;
                            ps_nxtDPB->u1_used_as_ref = UNUSED_FOR_REF;
                            ps_nxtDPB->s_top_field.u1_reference_info =
                                            UNUSED_FOR_REF;
                            ps_nxtDPB->s_bot_field.u1_reference_info =
                                            UNUSED_FOR_REF;

                            ps_nxtDPB->ps_pic_buf = NULL;
                            //Release buffer
                            ih264d_free_ref_pic_mv_bufs(ps_dpb_mgr->pv_codec_handle,
                                                        ps_nxtDPB->u1_buf_id);
                            ps_next_dpb = ps_nxtDPB->ps_prev_long;
                            ps_nxtDPB->ps_prev_long = NULL;
                        }
                    }
                    if(u4_lt_idx == 0)
                    {
                        ps_dpb_mgr->u1_max_lt_frame_idx = NO_LONG_TERM_INDICIES;
                    }
                    else
                    {
                        ps_dpb_mgr->u1_max_lt_frame_idx = u4_lt_idx - 1;
                    }

                    break;
                }
                case SET_LT_INDEX:
                {
                    u4_lt_idx = ps_mmc_params->u4_lt_idx; //Get long term index
                    if((ps_dpb_mgr->u1_max_lt_frame_idx == NO_LONG_TERM_INDICIES) ||
                        (u4_lt_idx > ps_dpb_mgr->u1_max_lt_frame_idx))
                    {
                        return ERROR_DBP_MANAGER_T;
                    }
                    ret = ih264d_insert_st_node(ps_dpb_mgr, ps_pic_buf, u1_buf_id,
                                          u4_cur_pic_num);
                    if(ret != OK)
                        return ret;

                    if(ps_dpb_mgr->u1_num_st_ref_bufs > 0)

                    {
                        ret = ih264d_delete_st_node_or_make_lt(ps_dpb_mgr,
                                                               u4_cur_pic_num,
                                                               u4_lt_idx,
                                                               u1_fld_pic_flag);
                        if(ret != OK)
                            return ret;
                    }
                    else
                    {
                        return ERROR_DBP_MANAGER_T;
                    }

                    u1_marked_lt = 1;
                    break;
                }

                default:
                    break;
            }
            if(u4_mmco == RESET_REF_PICTURES || u4_mmco == RESET_ALL_PICTURES)
            {
                ih264d_reset_ref_bufs(ps_dpb_mgr);
                u4_cur_pic_num = 0;
            }
        }
    }
    if(!u1_marked_lt && u1_insert_st_pic)
    {
        ret = ih264d_insert_st_node(ps_dpb_mgr, ps_pic_buf, u1_buf_id,
                              u4_cur_pic_num);
        if(ret != OK)
            return ret;
    }
    return OK;
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_release_pics_in_dpb                                         */
/*                                                                           */
/*  Description   : This function deletes all pictures from DPB              */
/*                                                                           */
/*  Inputs        : h_pic_buf_api: pointer to picture buffer API               */
/*                  u1_disp_bufs: number pictures ready for display           */
/*                                                                           */
/*  Globals       : None                                                     */
/*  Outputs       : None                                                     */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         22 06 2005   NS              Draft                                */
/*                                                                           */
/*****************************************************************************/
void ih264d_release_pics_in_dpb(void *pv_dec,
                                UWORD8 u1_disp_bufs)
{
    WORD8 i;
    dec_struct_t *ps_dec = (dec_struct_t *)pv_dec;

    for(i = 0; i < u1_disp_bufs; i++)
    {
        ih264_buf_mgr_release((buf_mgr_t *)ps_dec->pv_pic_buf_mgr,
                              i,
                              BUF_MGR_REF);
        ih264_buf_mgr_release((buf_mgr_t *)ps_dec->pv_mv_buf_mgr,
                              ps_dec->au1_pic_buf_id_mv_buf_id_map[i],
                              BUF_MGR_REF);
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_delete_gap_frm_sliding                            */
/*                                                                           */
/*  Description   : This function deletes a picture from the list of gaps,   */
/*                  if the frame number of gap frame is lesser than the one  */
/*                  to be deleted by sliding window                          */
/*  Inputs        : ps_dpb_mgr: pointer to dpb manager                       */
/*                  i4_frame_num:  frame number of picture that's going to   */
/*                  be deleted by sliding window                             */
/*                  pu1_del_node: holds 0 if a gap is deleted else 1         */
/*  Globals       : None                                                     */
/*  Processing    : Function searches for frame number lesser than           */
/*                  i4_frame_num in the gaps list                            */
/*  Outputs       : None                                                     */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         22 06 2005   NS              Draft                                */
/*                                                                           */
/*****************************************************************************/
WORD32 ih264d_delete_gap_frm_sliding(dpb_manager_t *ps_dpb_mgr,
                                    WORD32 i4_frame_num,
                                    UWORD8 *pu1_del_node)
{
    WORD8 i1_gap_idx, i, j, j_min;
    WORD32 *pi4_gaps_start_frm_num, *pi4_gaps_end_frm_num, i4_gap_frame_num;
    WORD32 i4_start_frm_num, i4_end_frm_num;
    WORD32 i4_max_frm_num;
    WORD32 i4_frm_num, i4_gap_frm_num_min;

    /* find the least frame num from gaps and current DPB node    */
    /* Delete the least one                                       */
    *pu1_del_node = 1;
    if(0 == ps_dpb_mgr->u1_num_gaps)
        return OK;
    pi4_gaps_start_frm_num = ps_dpb_mgr->ai4_gaps_start_frm_num;
    pi4_gaps_end_frm_num = ps_dpb_mgr->ai4_gaps_end_frm_num;
    i4_gap_frame_num = INVALID_FRAME_NUM;
    i4_max_frm_num = ps_dpb_mgr->i4_max_frm_num;

    i1_gap_idx = -1;
    if(INVALID_FRAME_NUM != i4_frame_num)
    {
        i4_gap_frame_num = i4_frame_num;
        for(i = 0; i < MAX_FRAMES; i++)
        {
            i4_start_frm_num = pi4_gaps_start_frm_num[i];
            if(INVALID_FRAME_NUM != i4_start_frm_num)
            {
                i4_end_frm_num = pi4_gaps_end_frm_num[i];
                if(i4_end_frm_num < i4_max_frm_num)
                {
                    if(i4_start_frm_num <= i4_gap_frame_num)
                    {
                        i4_gap_frame_num = i4_start_frm_num;
                        i1_gap_idx = i;
                    }
                }
                else
                {
                    if(((i4_start_frm_num <= i4_gap_frame_num)
                                    && (i4_gap_frame_num <= i4_max_frm_num))
                                    || ((i4_start_frm_num >= i4_gap_frame_num)
                                                    && ((i4_gap_frame_num
                                                                    + i4_max_frm_num)
                                                                    >= i4_end_frm_num)))
                    {
                        i4_gap_frame_num = i4_start_frm_num;
                        i1_gap_idx = i;
                    }
                }
            }
        }
    }
    else
    {
        /* no valid short term buffers, delete one gap from the least start */
        /* of gap sequence                                                  */
        i4_gap_frame_num = pi4_gaps_start_frm_num[0];
        i1_gap_idx = 0;
        for(i = 1; i < MAX_FRAMES; i++)
        {
            if(INVALID_FRAME_NUM != pi4_gaps_start_frm_num[i])
            {
                if(pi4_gaps_start_frm_num[i] < i4_gap_frame_num)
                {
                    i4_gap_frame_num = pi4_gaps_start_frm_num[i];
                    i1_gap_idx = i;
                }
            }
        }
        if(INVALID_FRAME_NUM == i4_gap_frame_num)
        {
            UWORD32 i4_error_code;
            i4_error_code = ERROR_DBP_MANAGER_T;
            return i4_error_code;
        }
    }

    if(-1 != i1_gap_idx)
    {
        /* find least frame_num in the poc_map, which is in this range */
        i4_start_frm_num = pi4_gaps_start_frm_num[i1_gap_idx];
        if(i4_start_frm_num < 0)
            i4_start_frm_num += i4_max_frm_num;
        i4_end_frm_num = pi4_gaps_end_frm_num[i1_gap_idx];
        if(i4_end_frm_num < 0)
            i4_end_frm_num += i4_max_frm_num;

        i4_gap_frm_num_min = 0xfffffff;
        j_min = MAX_FRAMES;
        for(j = 0; j < MAX_FRAMES; j++)
        {
            i4_frm_num = ps_dpb_mgr->ai4_poc_buf_id_map[j][2];
            if((i4_start_frm_num <= i4_frm_num)
                            && (i4_end_frm_num >= i4_frm_num))
            {
                if(i4_frm_num < i4_gap_frm_num_min)
                {
                    j_min = j;
                    i4_gap_frm_num_min = i4_frm_num;
                }
            }
        }

        if(j_min != MAX_FRAMES)
        {

            ps_dpb_mgr->ai4_poc_buf_id_map[j_min][0] = -1;
            ps_dpb_mgr->ai4_poc_buf_id_map[j_min][1] = 0x7fffffff;
            ps_dpb_mgr->ai4_poc_buf_id_map[j_min][2] = GAP_FRAME_NUM;
            ps_dpb_mgr->i1_gaps_deleted++;

            ps_dpb_mgr->ai1_gaps_per_seq[i1_gap_idx]--;
            ps_dpb_mgr->u1_num_gaps--;
            *pu1_del_node = 0;
            if(0 == ps_dpb_mgr->ai1_gaps_per_seq[i1_gap_idx])
            {
                ps_dpb_mgr->ai4_gaps_start_frm_num[i1_gap_idx] =
                INVALID_FRAME_NUM;
                ps_dpb_mgr->ai4_gaps_end_frm_num[i1_gap_idx] = 0;
            }
        }
    }

    return OK;
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_delete_gap_frm_mmco                               */
/*                                                                           */
/*  Description   : This function deletes a picture from the list of gaps,   */
/*                  if the frame number (specified by mmco commands) to be   */
/*                  deleted is in the range by gap sequence.                 */
/*                                                                           */
/*  Inputs        : ps_dpb_mgr: pointer to dpb manager                       */
/*                  i4_frame_num:  frame number of picture that's going to   */
/*                  be deleted by mmco                                       */
/*                  pu1_del_node: holds 0 if a gap is deleted else 1         */
/*  Globals       : None                                                     */
/*  Processing    : Function searches for frame number lesser in the range   */
/*                  specified by gap sequence                                */
/*  Outputs       : None                                                     */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         22 06 2005   NS              Draft                                */
/*                                                                           */
/*****************************************************************************/
WORD32 ih264d_delete_gap_frm_mmco(dpb_manager_t *ps_dpb_mgr,
                                  WORD32 i4_frame_num,
                                  UWORD8 *pu1_del_node)
{
    WORD8 i, j;
    WORD32 *pi4_start, *pi4_end;
    WORD32 i4_start_frm_num, i4_end_frm_num, i4_max_frm_num;

    /* find the least frame num from gaps and current DPB node    */
    /* Delete the gaps                                            */
    *pu1_del_node = 1;
    pi4_start = ps_dpb_mgr->ai4_gaps_start_frm_num;
    pi4_end = ps_dpb_mgr->ai4_gaps_end_frm_num;
    i4_max_frm_num = ps_dpb_mgr->i4_max_frm_num;

    if(0 == ps_dpb_mgr->u1_num_gaps)
        return OK;

    if(i4_frame_num < 0)
        i4_frame_num += i4_max_frm_num;
    for(i = 0; i < MAX_FRAMES; i++)
    {
        i4_start_frm_num = pi4_start[i];
        if(i4_start_frm_num < 0)
            i4_start_frm_num += i4_max_frm_num;
        if(INVALID_FRAME_NUM != i4_start_frm_num)
        {
            i4_end_frm_num = pi4_end[i];
            if(i4_end_frm_num < 0)
                i4_end_frm_num += i4_max_frm_num;

            if((i4_frame_num >= i4_start_frm_num)
                            && (i4_frame_num <= i4_end_frm_num))
            {
                break;
            }
            else
            {
                if(((i4_frame_num + i4_max_frm_num) >= i4_start_frm_num)
                                && ((i4_frame_num + i4_max_frm_num)
                                                <= i4_end_frm_num))
                {
                    UWORD32 i4_error_code;
                    i4_error_code = ERROR_DBP_MANAGER_T;
                    return i4_error_code;
                }
            }
        }
    }

    /* find frame_num index, in the poc_map which needs to be deleted */
    for(j = 0; j < MAX_FRAMES; j++)
    {
        if(i4_frame_num == ps_dpb_mgr->ai4_poc_buf_id_map[j][2])
            break;
    }

    if(MAX_FRAMES != i)
    {
        if(j == MAX_FRAMES)
        {
            UWORD32 i4_error_code;
            i4_error_code = ERROR_DBP_MANAGER_T;
            return i4_error_code;
        }

        ps_dpb_mgr->ai4_poc_buf_id_map[j][0] = -1;
        ps_dpb_mgr->ai4_poc_buf_id_map[j][1] = 0x7fffffff;
        ps_dpb_mgr->ai4_poc_buf_id_map[j][2] = GAP_FRAME_NUM;
        ps_dpb_mgr->i1_gaps_deleted++;

        ps_dpb_mgr->ai1_gaps_per_seq[i]--;
        ps_dpb_mgr->u1_num_gaps--;
        *pu1_del_node = 0;
        if(0 == ps_dpb_mgr->ai1_gaps_per_seq[i])
        {
            ps_dpb_mgr->ai4_gaps_start_frm_num[i] = INVALID_FRAME_NUM;
            ps_dpb_mgr->ai4_gaps_end_frm_num[i] = 0;
        }
    }
    else
    {
        UWORD32 i4_error_code;
        i4_error_code = ERROR_DBP_MANAGER_T;
        return i4_error_code;
    }

    return OK;
}

/*!
 **************************************************************************
 * \if Function name : ih264d_do_mmco_for_gaps \endif
 *
 * \brief
 *    Perform decoded picture buffer memory management control operations
 *
 * \return
 *    0 - No error; -1 - Error
 *
 * \note
 *    Bitstream is also parsed here to get the MMCOs
 *
 **************************************************************************
 */
WORD32 ih264d_do_mmco_for_gaps(dpb_manager_t *ps_dpb_mgr,
                             UWORD8 u1_num_ref_frames /*!< num_ref_frames from active SeqParSet*/
                             )
{
    struct dpb_info_t *ps_next_dpb;
    UWORD8 u1_num_gaps;
    UWORD8 u1_st_ref_bufs, u1_lt_ref_bufs, u1_del_node;
    WORD8 i;
    WORD32 i4_frame_gaps = 1;
    WORD32 ret;

    //Sliding window - implements 8.2.5.3, flush out buffers
    u1_st_ref_bufs = ps_dpb_mgr->u1_num_st_ref_bufs;
    u1_lt_ref_bufs = ps_dpb_mgr->u1_num_lt_ref_bufs;

    while(1)
    {
        u1_num_gaps = ps_dpb_mgr->u1_num_gaps;
        if((u1_st_ref_bufs + u1_lt_ref_bufs + u1_num_gaps + i4_frame_gaps)
                        > u1_num_ref_frames)
        {
            if(0 == (u1_st_ref_bufs + u1_num_gaps))
            {
                i4_frame_gaps = 0;
                ps_dpb_mgr->u1_num_gaps = (u1_num_ref_frames
                                - u1_lt_ref_bufs);
            }
            else
            {
                u1_del_node = 1;
                ps_next_dpb = ps_dpb_mgr->ps_dpb_st_head;

                if(u1_st_ref_bufs > 1)
                {
                    for(i = 1; i < (u1_st_ref_bufs - 1); i++)
                    {
                        if(ps_next_dpb == NULL)
                        {
                            UWORD32 i4_error_code;
                            i4_error_code = ERROR_DBP_MANAGER_T;
                            return i4_error_code;
                        }
                        ps_next_dpb = ps_next_dpb->ps_prev_short;
                    }

                    if(ps_next_dpb->ps_prev_short->ps_prev_short != NULL)
                    {
                        return ERROR_DBP_MANAGER_T;
                    }

                    if(u1_num_gaps)
                    {
                        ret = ih264d_delete_gap_frm_sliding(ps_dpb_mgr,
                                                            ps_next_dpb->ps_prev_short->i4_frame_num,
                                                            &u1_del_node);
                        if(ret != OK)
                            return ret;
                    }

                    if(u1_del_node)
                    {
                        u1_st_ref_bufs--;
                        ps_next_dpb->ps_prev_short->u1_used_as_ref =
                                        UNUSED_FOR_REF;
                        ps_next_dpb->ps_prev_short->s_top_field.u1_reference_info =
                                        UNUSED_FOR_REF;
                        ps_next_dpb->ps_prev_short->s_bot_field.u1_reference_info =
                                        UNUSED_FOR_REF;
                        ih264d_free_ref_pic_mv_bufs(ps_dpb_mgr->pv_codec_handle,
                                                    ps_next_dpb->ps_prev_short->u1_buf_id);
                        ps_next_dpb->ps_prev_short->ps_pic_buf = NULL;
                        ps_next_dpb->ps_prev_short = NULL;
                    }
                }
                else
                {
                    if(u1_st_ref_bufs)
                    {
                        if(u1_num_gaps)
                        {
                            ret = ih264d_delete_gap_frm_sliding(ps_dpb_mgr,
                                                                ps_next_dpb->i4_frame_num,
                                                                &u1_del_node);
                            if(ret != OK)
                                return ret;
                        }

                        if(u1_del_node)
                        {
                            u1_st_ref_bufs--;
                            ps_next_dpb->u1_used_as_ref = FALSE;
                            ps_next_dpb->s_top_field.u1_reference_info =
                                            UNUSED_FOR_REF;
                            ps_next_dpb->s_bot_field.u1_reference_info =
                                            UNUSED_FOR_REF;
                            ih264d_free_ref_pic_mv_bufs(ps_dpb_mgr->pv_codec_handle,
                                                        ps_next_dpb->u1_buf_id);
                            ps_next_dpb->ps_pic_buf = NULL;
                            ps_next_dpb = NULL;
                            ps_dpb_mgr->ps_dpb_st_head = NULL;
                            ps_dpb_mgr->u1_num_st_ref_bufs = u1_st_ref_bufs;
                        }
                    }
                    else
                    {
                        ret = ih264d_delete_gap_frm_sliding(ps_dpb_mgr,
                                                            INVALID_FRAME_NUM,
                                                            &u1_del_node);
                        if(ret != OK)
                            return ret;
                        if(u1_del_node)
                        {
                            return ERROR_DBP_MANAGER_T;
                        }
                    }
                }
            }
        }
        else
        {
            ps_dpb_mgr->u1_num_gaps += i4_frame_gaps;
            break;
        }
    }

    ps_dpb_mgr->u1_num_st_ref_bufs = u1_st_ref_bufs;

    return OK;
}
/****************************************************************************/
/*                                                                          */
/* Function Name  : ih264d_free_node_from_dpb                                      */
/*                                                                          */
/* Description    :                                                         */
/*                                                                          */
/* Inputs         :                                                         */
/*                                                                          */
/* Globals        :                                                         */
/*                                                                          */
/* Processing     :                                                         */
/*                                                                          */
/* Outputs        :                                                         */
/*                                                                          */
/* Returns        :                                                         */
/*                                                                          */
/* Known Issues   :                                                         */
/*                                                                          */
/* Revision History                                                         */
/*                                                                          */
/*      DD MM YY            Author        Changes                           */
/*                          Sarat                                           */
/****************************************************************************/
/**** Function Added for Error Resilience *****/
WORD32 ih264d_free_node_from_dpb(dpb_manager_t *ps_dpb_mgr,
                               UWORD32 u4_cur_pic_num,
                               UWORD8 u1_numRef_frames_for_seq)
{
    WORD32 i;
    UWORD8 u1_num_gaps = ps_dpb_mgr->u1_num_gaps;
    struct dpb_info_t *ps_next_dpb;
    UWORD8 u1_del_node = 1;
    WORD32 ret;

    //Sliding window - implements 8.2.5.3
    if((ps_dpb_mgr->u1_num_st_ref_bufs + ps_dpb_mgr->u1_num_lt_ref_bufs
                    + u1_num_gaps) == u1_numRef_frames_for_seq)
    {
        UWORD8 u1_new_node_flag = 1;
        if((0 == ps_dpb_mgr->u1_num_st_ref_bufs) && (0 == u1_num_gaps))
        {
            return ERROR_DBP_MANAGER_T;
        }

        // Chase the links to reach the last but one picNum, if available
        ps_next_dpb = ps_dpb_mgr->ps_dpb_st_head;

        if(ps_dpb_mgr->u1_num_st_ref_bufs > 1)
        {
            if(ps_next_dpb->i4_frame_num == (WORD32)u4_cur_pic_num)
            {
                /* Incase of  filed pictures top_field has been allocated   */
                /* picture buffer and complementary bottom field pair comes */
                /* then the sliding window mechanism should not allocate a  */
                /* new node                                                 */
                u1_new_node_flag = 0;
            }

            for(i = 1; i < (ps_dpb_mgr->u1_num_st_ref_bufs - 1); i++)
            {
                if(ps_next_dpb == NULL)
                    return ERROR_DBP_MANAGER_T;

                if(ps_next_dpb->i4_frame_num == (WORD32)u4_cur_pic_num)
                {
                    /* Incase of  field pictures top_field has been allocated   */
                    /* picture buffer and complementary bottom field pair comes */
                    /* then the sliding window mechanism should not allocate a  */
                    /* new node                                                 */
                    u1_new_node_flag = 0;
                }
                ps_next_dpb = ps_next_dpb->ps_prev_short;
            }

            if(ps_next_dpb->ps_prev_short->ps_prev_short != NULL)
                return ERROR_DBP_MANAGER_T;

            if(u1_new_node_flag)
            {
                if(u1_num_gaps)
                {
                    ret = ih264d_delete_gap_frm_sliding(ps_dpb_mgr,
                                                        ps_next_dpb->ps_prev_short->i4_frame_num,
                                                        &u1_del_node);
                    if(ret != OK)
                        return ret;
                }

                if(u1_del_node)
                {
                    ps_dpb_mgr->u1_num_st_ref_bufs--;
                    ps_next_dpb->ps_prev_short->u1_used_as_ref = UNUSED_FOR_REF;
                    ps_next_dpb->ps_prev_short->s_top_field.u1_reference_info =
                                    UNUSED_FOR_REF;
                    ps_next_dpb->ps_prev_short->s_bot_field.u1_reference_info =
                                    UNUSED_FOR_REF;
                    ih264d_free_ref_pic_mv_bufs(ps_dpb_mgr->pv_codec_handle,
                                                ps_next_dpb->ps_prev_short->u1_buf_id);
                    ps_next_dpb->ps_prev_short->ps_pic_buf = NULL;
                    ps_next_dpb->ps_prev_short = NULL;
                }
            }
        }
        else
        {
            if(ps_dpb_mgr->u1_num_st_ref_bufs)
            {
                ret = ih264d_delete_gap_frm_sliding(ps_dpb_mgr,
                                                    ps_next_dpb->i4_frame_num,
                                                    &u1_del_node);
                if(ret != OK)
                    return ret;
                if((ps_next_dpb->i4_frame_num != (WORD32)u4_cur_pic_num)
                                && u1_del_node)
                {
                    ps_dpb_mgr->u1_num_st_ref_bufs--;
                    ps_next_dpb->u1_used_as_ref = FALSE;
                    ps_next_dpb->s_top_field.u1_reference_info = UNUSED_FOR_REF;
                    ps_next_dpb->s_bot_field.u1_reference_info = UNUSED_FOR_REF;
                    ih264d_free_ref_pic_mv_bufs(ps_dpb_mgr->pv_codec_handle,
                                                ps_next_dpb->u1_buf_id);
                    ps_next_dpb->ps_pic_buf = NULL;
                    ps_next_dpb = NULL;
                }
            }
            else
            {
                ret = ih264d_delete_gap_frm_sliding(ps_dpb_mgr, INVALID_FRAME_NUM, &u1_del_node);
                if(ret != OK)
                    return ret;
                if(u1_del_node)
                    return ERROR_DBP_MANAGER_T;
            }
        }
    }
    return OK;
}
/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_delete_nonref_nondisplay_pics                            */
/*                                                                           */
/*  Description   :                                                          */
/*                                                                           */
/*                                                                           */
/*  Inputs        :                                                          */
/*  Globals       :                                                          */
/*  Processing    :                                                          */
/*                                                                           */
/*  Outputs       :                                                          */
/*  Returns       :                                                          */
/*                                                                           */
/*  Issues        :                                                          */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         05 06 2007   Varun           Draft                                */
/*                                                                           */
/*****************************************************************************/

void ih264d_delete_nonref_nondisplay_pics(dpb_manager_t *ps_dpb_mgr)
{
    WORD8 i;
    WORD32 (*i4_poc_buf_id_map)[3] = ps_dpb_mgr->ai4_poc_buf_id_map;

    /* remove all gaps marked as unused for ref */
    for(i = 0; (i < MAX_FRAMES) && ps_dpb_mgr->i1_gaps_deleted; i++)
    {
        if(GAP_FRAME_NUM == i4_poc_buf_id_map[i][2])
        {
            ps_dpb_mgr->i1_gaps_deleted--;
            ps_dpb_mgr->i1_poc_buf_id_entries--;
            i4_poc_buf_id_map[i][0] = -1;
            i4_poc_buf_id_map[i][1] = 0x7fffffff;
            i4_poc_buf_id_map[i][2] = 0;
        }
    }
}
/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_insert_pic_in_display_list                               */
/*                                                                           */
/*  Description   :                                                          */
/*                                                                           */
/*                                                                           */
/*  Inputs        :                                                          */
/*  Globals       :                                                          */
/*  Processing    :                                                          */
/*                                                                           */
/*  Outputs       :                                                          */
/*  Returns       :                                                          */
/*                                                                           */
/*  Issues        :                                                          */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         05 06 2007   Varun           Draft                                */
/*                                                                           */
/*****************************************************************************/

WORD32 ih264d_insert_pic_in_display_list(dpb_manager_t *ps_dpb_mgr,
                                         UWORD8 u1_buf_id,
                                         WORD32 i4_display_poc,
                                         UWORD32 u4_frame_num)
{
    WORD8 i;
    WORD32 (*i4_poc_buf_id_map)[3] = ps_dpb_mgr->ai4_poc_buf_id_map;

    for(i = 0; i < MAX_FRAMES; i++)
    {
        /* Find an empty slot */
        if(i4_poc_buf_id_map[i][0] == -1)
        {
            if(GAP_FRAME_NUM == i4_poc_buf_id_map[i][2])
                ps_dpb_mgr->i1_gaps_deleted--;
            else
                ps_dpb_mgr->i1_poc_buf_id_entries++;

            i4_poc_buf_id_map[i][0] = u1_buf_id;
            i4_poc_buf_id_map[i][1] = i4_display_poc;
            i4_poc_buf_id_map[i][2] = u4_frame_num;

            break;
        }
    }

    if(MAX_FRAMES == i)
    {

        UWORD32 i4_error_code;
        i4_error_code = ERROR_GAPS_IN_FRM_NUM;
        return i4_error_code;
    }
    return OK;
}

