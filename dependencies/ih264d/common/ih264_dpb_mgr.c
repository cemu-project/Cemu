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
/**
 *******************************************************************************
 * @file
 *  ih264_dpb_mgr.c
 *
 * @brief
 *  Function definitions used for decoded picture buffer management
 *
 * @author
 *  Srinivas T
 *
 * @par List of Functions:
 *   - ih264_dpb_mgr_init()
 *   - ih264_dpb_mgr_sort_short_term_fields_by_frame_num()
 *   - ih264_dpb_mgr_sort_short_term_fields_by_poc_l0()
 *   - ih264_dpb_mgr_sort_short_term_fields_by_poc_l1()
 *   - ih264_dpb_mgr_sort_long_term_fields_by_frame_idx()
 *   - ih264_dpb_mgr_alternate_ref_fields()
 *   - ih264_dpb_mgr_insert_ref_field()
 *   - ih264_dpb_mgr_insert_ref_frame()
 *   - ih264_dpb_mgr_count_ref_frames()
 *   - ih264_dpb_mgr_delete_ref_frame()
 *   - ih264_dpb_mgr_delete_long_ref_fields_max_frame_idx()
 *   - ih264_dpb_mgr_delete_short_ref_frame()
 *   - ih264_dpb_mgr_delete_all_ref_frames()
 *   - ih264_dpb_mgr_reset()
 *   - ih264_dpb_mgr_release_pics()
 *
 * @remarks
 *  None
 *
 *******************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "ih264_typedefs.h"
#include "ih264_defs.h"
#include "ih264_macros.h"
#include "ih264_error.h"
#include "ih264_structs.h"
#include "ih264_buf_mgr.h"
#include "ih264_dpb_mgr.h"
#include "ih264_debug.h"

/**
 *******************************************************************************
 *
 * @brief
 *  DPB manager initializer
 *
 * @par Description:
 *  Initialises the DPB manager structure
 *
 * @param[in] ps_dpb_mgr
 *  Pointer to the DPB manager structure
 *
 * @returns
 *
 * @remarks
 *
 *
 *******************************************************************************
 */

void ih264_dpb_mgr_init(dpb_mgr_t *ps_dpb_mgr)
{
    UWORD32 i;
    dpb_info_t *ps_dpb_info = ps_dpb_mgr->as_dpb_info;
    for(i = 0; i < MAX_DPB_BUFS; i++)
    {
        ps_dpb_info[i].ps_prev_dpb = NULL;
        ps_dpb_info[i].ps_pic_buf = NULL;
        ps_dpb_mgr->as_top_field_pics[i].i4_used_as_ref    = INVALID;
        ps_dpb_mgr->as_bottom_field_pics[i].i4_used_as_ref = INVALID;
        ps_dpb_mgr->as_top_field_pics[i].i1_field_type     = INVALID;
        ps_dpb_mgr->as_bottom_field_pics[i].i1_field_type  = INVALID;
        ps_dpb_mgr->as_top_field_pics[i].i4_long_term_frame_idx    = -1;
        ps_dpb_mgr->as_bottom_field_pics[i].i4_long_term_frame_idx = -1;
    }

    ps_dpb_mgr->u1_num_short_term_ref_bufs = 0;
    ps_dpb_mgr->u1_num_long_term_ref_bufs = 0;
    ps_dpb_mgr->ps_dpb_short_term_head = NULL;
    ps_dpb_mgr->ps_dpb_long_term_head = NULL;
}

/**
 *******************************************************************************
 *
 * @brief
 *  Function to sort sort term pics by frame_num.
 *
 * @par Description:
 *  Sorts short term fields by frame_num. For 2 fields having same frame_num,
 *  orders them based on requested first field type.
 *
 * @param[in] ps_dpb_mgr
 *  Pointer to the DPB manager structure
 *
 * @param[in] curr_frame_num
 *  frame_num of the current pic
 *
 * @param[in] first_field_type
 *  For complementary fields, required first field
 *
 * @param[in] max_frame_num
 *  Maximum frame_num allowed
 *
 * @returns
 *
 * @remarks
 *
 *
 *******************************************************************************
 */
WORD32 ih264_dpb_mgr_sort_short_term_fields_by_frame_num(dpb_mgr_t *ps_dpb_mgr,
                                                         WORD32 curr_frame_num,
                                                         WORD32 first_field_type,
                                                         WORD32 max_frame_num)
{
    dpb_info_t *ps_dpb_node1 = ps_dpb_mgr->ps_dpb_short_term_head;
    dpb_info_t *ps_dpb_node2;
    WORD32 frame_num_node1;
    WORD32 frame_num_node2;
    pic_buf_t *ps_pic_buf;

    if(ps_dpb_node1 == NULL)
        return -1;

    for (; ps_dpb_node1 != NULL; ps_dpb_node1 = ps_dpb_node1->ps_prev_dpb)
    {
        for (ps_dpb_node2 = ps_dpb_node1->ps_prev_dpb; ps_dpb_node2 != NULL; ps_dpb_node2 = ps_dpb_node2->ps_prev_dpb)
        {
            frame_num_node1 = ps_dpb_node1->ps_pic_buf->i4_frame_num;
            frame_num_node2 = ps_dpb_node2->ps_pic_buf->i4_frame_num;

            if(frame_num_node1 > curr_frame_num)
                frame_num_node1 = frame_num_node1 - max_frame_num;
            if(frame_num_node2 > curr_frame_num)
                frame_num_node2 = frame_num_node2 - max_frame_num;

            if(frame_num_node1 < frame_num_node2)
            {
                ps_pic_buf = ps_dpb_node1->ps_pic_buf;
                ps_dpb_node1->ps_pic_buf = ps_dpb_node2->ps_pic_buf;
                ps_dpb_node2->ps_pic_buf = ps_pic_buf;
            }
        }
    }

    /**
     * For frames and complementary field pairs,
     * ensure first_field_type appears first in the list
     */
    ps_dpb_node1 = ps_dpb_mgr->ps_dpb_short_term_head;
    ps_dpb_node2 = ps_dpb_node1->ps_prev_dpb;
    while(ps_dpb_node2 != NULL)
    {
        pic_buf_t *ps_pic_node1 = ps_dpb_node1->ps_pic_buf;
        pic_buf_t *ps_pic_node2 = ps_dpb_node2->ps_pic_buf;
        frame_num_node1 = ps_pic_node1->i4_frame_num;
        frame_num_node2 = ps_pic_node2->i4_frame_num;
        if(frame_num_node1 == frame_num_node2)
        {
            ASSERT(ps_pic_node1->i1_field_type != ps_pic_node2->i1_field_type);
            if(ps_pic_node1->i1_field_type != first_field_type)
            {
                ps_dpb_node1->ps_pic_buf = ps_pic_node2;
                ps_dpb_node2->ps_pic_buf = ps_pic_node1;
            }
        }
        ps_dpb_node1 = ps_dpb_node2;
        ps_dpb_node2 = ps_dpb_node2->ps_prev_dpb;
    }
    return 0;

}

/**
 *******************************************************************************
 *
 * @brief
 *  Function to sort sort term pics by poc for list 0.
 *
 * @par Description:
 *  Orders all the pocs less than current poc in the descending order.
 *  Then orders all the pocs greater than current poc in the ascending order.
 *
 * @param[in] ps_dpb_mgr
 *  Pointer to the DPB manager structure
 *
 * @param[in] curr_poc
 *  Poc of the current pic
 *
 * @param[in] first_field_type
 *  For complementary fields, required first field
 *
 * @returns
 *
 * @remarks
 *
 *
 *******************************************************************************
 */
WORD32 ih264_dpb_mgr_sort_short_term_fields_by_poc_l0(dpb_mgr_t *ps_dpb_mgr,
                                                      WORD32 curr_poc,
                                                      WORD32 first_field_type)
{
    dpb_info_t *ps_dpb_node1 = ps_dpb_mgr->ps_dpb_short_term_head;
    dpb_info_t *ps_dpb_node2;
    WORD32 poc_node1;
    WORD32 poc_node2;
    WORD32 frame_num_node1;
    WORD32 frame_num_node2;
    pic_buf_t *ps_pic_buf;

    if(ps_dpb_node1 == NULL)
        return -1;

    /**
     * Sort the fields by poc.
     * All POCs less than current poc are first placed in the descending order.
     * Then all POCs greater than current poc are placed in the ascending order.
     */
    for (; ps_dpb_node1 != NULL; ps_dpb_node1 = ps_dpb_node1->ps_prev_dpb)
    {
        for (ps_dpb_node2 = ps_dpb_node1->ps_prev_dpb; ps_dpb_node2 != NULL; ps_dpb_node2 = ps_dpb_node2->ps_prev_dpb)
        {
            poc_node1 = ps_dpb_node1->ps_pic_buf->i4_abs_poc;
            poc_node2 = ps_dpb_node2->ps_pic_buf->i4_abs_poc;
            ASSERT(poc_node1 != curr_poc);
            ASSERT(poc_node2 != curr_poc);
            if(((poc_node1 < curr_poc) && (poc_node2 > curr_poc)) ||
                    ((poc_node1 < curr_poc) && (poc_node2 < curr_poc) && (poc_node1 > poc_node2)) ||
                    ((poc_node1 > curr_poc) && (poc_node2 > curr_poc) && (poc_node1 < poc_node2)))
                    continue;

            ps_pic_buf = ps_dpb_node1->ps_pic_buf;
            ps_dpb_node1->ps_pic_buf = ps_dpb_node2->ps_pic_buf;
            ps_dpb_node2->ps_pic_buf = ps_pic_buf;
        }
    }

    ps_dpb_node1 = ps_dpb_mgr->ps_dpb_short_term_head;
    ps_dpb_node2 = ps_dpb_node1->ps_prev_dpb;
    while(ps_dpb_node2 != NULL)
    {
        pic_buf_t *ps_pic_node1 = ps_dpb_node1->ps_pic_buf;
        pic_buf_t *ps_pic_node2 = ps_dpb_node2->ps_pic_buf;
        frame_num_node1 = ps_pic_node1->i4_frame_num;
        frame_num_node2 = ps_pic_node2->i4_frame_num;
        if(frame_num_node1 == frame_num_node2)
        {
            ASSERT(ps_pic_node1->i1_field_type != ps_pic_node2->i1_field_type);
            if(ps_pic_node1->i1_field_type != first_field_type)
            {
                ps_dpb_node1->ps_pic_buf = ps_pic_node2;
                ps_dpb_node2->ps_pic_buf = ps_pic_node1;
            }
        }
        ps_dpb_node1 = ps_dpb_node2;
        ps_dpb_node2 = ps_dpb_node2->ps_prev_dpb;
    }
    return 0;

}

/**
 *******************************************************************************
 *
 * @brief
 *  Function to sort sort term pics by poc for list 1.
 *
 * @par Description:
 *  Orders all the pocs greater than current poc in the ascending order.
 *  Then rrders all the pocs less than current poc in the descending order.
 *
 * @param[in] ps_dpb_mgr
 *  Pointer to the DPB manager structure
 *
 * @param[in] curr_poc
 *  Poc of the current pic
 *
 * @param[in] first_field_type
 *  For complementary fields, required first field
 *
 * @returns
 *
 * @remarks
 *
 *
 *******************************************************************************
 */
WORD32 ih264_dpb_mgr_sort_short_term_fields_by_poc_l1(dpb_mgr_t *ps_dpb_mgr,
                                                      WORD32 curr_poc,
                                                      WORD32 first_field_type)
{
    dpb_info_t *ps_dpb_node1 = ps_dpb_mgr->ps_dpb_short_term_head;
    dpb_info_t *ps_dpb_node2;
    WORD32 poc_node1;
    WORD32 poc_node2;
    WORD32 frame_num_node1;
    WORD32 frame_num_node2;
    pic_buf_t *ps_pic_buf;

    if(ps_dpb_node1 == NULL)
        return -1;

    /**
     * Sort the fields by poc.
     * All POCs greater than current poc are first placed in the ascending order.
     * Then all POCs less than current poc are placed in the decending order.
     */
    for (; ps_dpb_node1 != NULL; ps_dpb_node1 = ps_dpb_node1->ps_prev_dpb)
    {
        for (ps_dpb_node2 = ps_dpb_node1->ps_prev_dpb; ps_dpb_node2 != NULL; ps_dpb_node2 = ps_dpb_node2->ps_prev_dpb)
        {
            poc_node1 = ps_dpb_node1->ps_pic_buf->i4_abs_poc;
            poc_node2 = ps_dpb_node2->ps_pic_buf->i4_abs_poc;
            ASSERT(poc_node1 != curr_poc);
            ASSERT(poc_node2 != curr_poc);
            if(((poc_node1 > curr_poc) && (poc_node2 < curr_poc)) ||
                    ((poc_node1 < curr_poc) && (poc_node2 < curr_poc) && (poc_node1 > poc_node2)) ||
                    ((poc_node1 > curr_poc) && (poc_node2 > curr_poc) && (poc_node1 < poc_node2)))
                    continue;

            ps_pic_buf = ps_dpb_node1->ps_pic_buf;
            ps_dpb_node1->ps_pic_buf = ps_dpb_node2->ps_pic_buf;
            ps_dpb_node2->ps_pic_buf = ps_pic_buf;
        }
    }

    ps_dpb_node1 = ps_dpb_mgr->ps_dpb_short_term_head;
    ps_dpb_node2 = ps_dpb_node1->ps_prev_dpb;
    while(ps_dpb_node2 != NULL)
    {
        pic_buf_t *ps_pic_node1 = ps_dpb_node1->ps_pic_buf;
        pic_buf_t *ps_pic_node2 = ps_dpb_node2->ps_pic_buf;
        frame_num_node1 = ps_pic_node1->i4_frame_num;
        frame_num_node2 = ps_pic_node2->i4_frame_num;
        if(frame_num_node1 == frame_num_node2)
        {
            ASSERT(ps_pic_node1->i1_field_type != ps_pic_node2->i1_field_type);
            if(ps_pic_node1->i1_field_type != first_field_type)
            {
                ps_dpb_node1->ps_pic_buf = ps_pic_node2;
                ps_dpb_node2->ps_pic_buf = ps_pic_node1;
            }
        }
        ps_dpb_node1 = ps_dpb_node2;
        ps_dpb_node2 = ps_dpb_node2->ps_prev_dpb;
    }
    return 0;
}
/**
 *******************************************************************************
 *
 * @brief
 *  Function to sort long term pics by long term frame idx.
 *
 * @par Description:
 *  Sorts long term fields by long term frame idx. For 2 fields
 *  having same frame_num, orders them based on requested first field type.
 *
 * @param[in] ps_dpb_mgr
 *  Pointer to the DPB manager structure
 *
 * @param[in] first_field_type
 *  For complementary fields, required first field
 *
 * @returns
 *
 * @remarks
 *
 *
 *******************************************************************************
 */
WORD32 ih264_dpb_mgr_sort_long_term_fields_by_frame_idx(dpb_mgr_t *ps_dpb_mgr,
                                                        WORD32 first_field_type)
{
    dpb_info_t *ps_dpb_node1 = ps_dpb_mgr->ps_dpb_long_term_head;
    dpb_info_t *ps_dpb_node2;
    WORD32 frame_idx_node1;
    WORD32 frame_idx_node2;
    pic_buf_t *ps_pic_buf;

    if(ps_dpb_node1 == NULL)
        return -1;

    /* Sort the fields by frame idx */
    for (; ps_dpb_node1 != NULL; ps_dpb_node1 = ps_dpb_node1->ps_prev_dpb)
    {
        for (ps_dpb_node2 = ps_dpb_node1->ps_prev_dpb; ps_dpb_node2 != NULL; ps_dpb_node2 = ps_dpb_node2->ps_prev_dpb)
        {
            frame_idx_node1 = ps_dpb_node1->ps_pic_buf->i4_long_term_frame_idx;
            frame_idx_node2 = ps_dpb_node2->ps_pic_buf->i4_long_term_frame_idx;

            if(frame_idx_node1 > frame_idx_node2)
            {
                ps_pic_buf = ps_dpb_node1->ps_pic_buf;
                ps_dpb_node1->ps_pic_buf = ps_dpb_node2->ps_pic_buf;
                ps_dpb_node2->ps_pic_buf = ps_pic_buf;
            }
        }
    }

    /**
     * For frames and complementary field pairs,
     * ensure first_field_type appears first in the list
     */
    ps_dpb_node1 = ps_dpb_mgr->ps_dpb_long_term_head;
    ps_dpb_node2 = ps_dpb_node1->ps_prev_dpb;
    while(ps_dpb_node2 != NULL)
    {
        pic_buf_t *ps_pic_node1 = ps_dpb_node1->ps_pic_buf;
        pic_buf_t *ps_pic_node2 = ps_dpb_node2->ps_pic_buf;
        frame_idx_node1 = ps_pic_node1->i4_long_term_frame_idx;
        frame_idx_node2 = ps_pic_node2->i4_long_term_frame_idx;
        if(frame_idx_node1 == frame_idx_node2)
        {
            ASSERT(ps_pic_node1->i1_field_type != ps_pic_node2->i1_field_type);
            if(ps_pic_node1->i1_field_type != first_field_type)
            {
                ps_dpb_node1->ps_pic_buf = ps_pic_node2;
                ps_dpb_node2->ps_pic_buf = ps_pic_node1;
            }
        }
        ps_dpb_node1 = ps_dpb_node2;
        ps_dpb_node2 = ps_dpb_node2->ps_prev_dpb;
    }
    return 0;
}

/**
 *******************************************************************************
 *
 * @brief
 *  Function to alternate fields.
 *
 * @par Description:
 *  In the ordered list of fields, alternate fields starting with
 *  first_field_type
 *
 * @param[in] ps_dpb_mgr
 *  Pointer to the DPB manager structure
 *
 * @param[in] reference_type
 *  This is used to select between short-term and long-term linked list.
 *
 * @param[in] first_field_type
 *  For complementary fields, required first field
 *
 * @returns
 *
 * @remarks
 *
 *
 *******************************************************************************
 */
WORD32 ih264_dpb_mgr_alternate_ref_fields(dpb_mgr_t *ps_dpb_mgr,
                                          WORD32 reference_type,
                                          WORD32 first_field_type)
{
    dpb_info_t s_dpb_head;
    dpb_info_t *ps_dpb_head;
    dpb_info_t *ps_dpb_node1;
    dpb_info_t *ps_dpb_node2;
    dpb_info_t *ps_dpb_node3;
    dpb_info_t *ps_dpb_node4;
    WORD32 expected_field;

    expected_field = first_field_type;

    ps_dpb_head = &s_dpb_head;

    ps_dpb_head->ps_prev_dpb = (reference_type == SHORT_TERM_REF) ?
            ps_dpb_mgr->ps_dpb_short_term_head:
            ps_dpb_mgr->ps_dpb_long_term_head;

    ps_dpb_node1 = ps_dpb_head;
    ps_dpb_node2 = ps_dpb_node1->ps_prev_dpb;
    while(ps_dpb_node2 != NULL)
    {
        pic_buf_t *ps_pic_node2 = ps_dpb_node2->ps_pic_buf;
        if(ps_pic_node2->i1_field_type != expected_field)
        {
            /*
             * If it is not expected field, loop over the node till
             * the expected field.
             */
            ps_dpb_node3 = ps_dpb_node2;
            ps_dpb_node4 = ps_dpb_node2->ps_prev_dpb;
            while((ps_dpb_node4 != NULL) &&
                    (ps_dpb_node4->ps_pic_buf->i1_field_type != expected_field))
            {
                ps_dpb_node3 = ps_dpb_node4;
                ps_dpb_node4 = ps_dpb_node4->ps_prev_dpb;
            }
            if(ps_dpb_node4 != NULL)
            {
                ps_dpb_node1->ps_prev_dpb = ps_dpb_node4;
                ps_dpb_node3->ps_prev_dpb = ps_dpb_node4->ps_prev_dpb;
                ps_dpb_node4->ps_prev_dpb = ps_dpb_node2;
            }
            else
            {
                /* node4 null means we have reached the end */
                break;
            }
        }
        ps_dpb_node1 = ps_dpb_node1->ps_prev_dpb;
        ps_dpb_node2 = ps_dpb_node1->ps_prev_dpb;
        expected_field = (ps_dpb_node1->ps_pic_buf->i1_field_type == TOP_FIELD)?
                            BOTTOM_FIELD:TOP_FIELD;
    }

    if(reference_type == SHORT_TERM_REF)
    {
        ps_dpb_mgr->ps_dpb_short_term_head = ps_dpb_head->ps_prev_dpb;
    }
    else
    {
        ps_dpb_mgr->ps_dpb_long_term_head = ps_dpb_head->ps_prev_dpb;
    }

    return 0;
}

/**
 *******************************************************************************
 *
 * @brief
 *  Add a ref field to short-term or long-term linked list.
 *
 * @par Description:
 *  This function adds a ref field to either short-term or long-term linked
 *  list. It picks up memory for the link from the array of dpb_info in
 *  dpb_mgr. The field is added to the beginning of the linked list and the
 *  head is set the the field.
 *
 * @param[in] ps_dpb_mgr
 *  Pointer to the DPB manager structure
 *
 * @param[in] ps_pic_buf
 *  Pic buf structure for the field being added.
 *
 * @param[in] reference_type
 *  This is used to select between short-term and long-term linked list.
 *
 * @param[in] frame_num
 *  frame_num for the field.
 *
 * @param[in] long_term_frame_idx
 *  If the ref being added is long-term, long_term_frame_idx of the field.
 *  Otherwise invalid.
 *
 * @returns
 *
 * @remarks
 *
 *
 *******************************************************************************
 */
WORD32 ih264_dpb_mgr_insert_ref_field(dpb_mgr_t *ps_dpb_mgr,
                                    pic_buf_t *ps_pic_buf,
                                    WORD32 reference_type,
                                    UWORD32 frame_num,
                                    WORD32 long_term_frame_idx)
{
    WORD32 i;
    dpb_info_t *ps_dpb_info;
    dpb_info_t *ps_dpb_head;

    ps_dpb_info = ps_dpb_mgr->as_dpb_info;

    /* Return error if buffer is already present in the DPB */
    for(i = 0; i < MAX_DPB_BUFS; i++)
    {
        if( (ps_dpb_info[i].ps_pic_buf == ps_pic_buf)
                        && (ps_dpb_info[i].ps_pic_buf->i4_used_as_ref == reference_type) )
        {
            return (-1);
        }
    }

    /* Find an unused DPB location */
    for(i = 0; i < MAX_DPB_BUFS; i++)
    {
        if(NULL == ps_dpb_info[i].ps_pic_buf)
        {
            break;
        }
    }
    if(i == MAX_DPB_BUFS)
    {
        return (-1);
    }

    ps_dpb_head = (reference_type == SHORT_TERM_REF)
                    ?ps_dpb_mgr->ps_dpb_short_term_head
                    :ps_dpb_mgr->ps_dpb_long_term_head;

    if(reference_type == SHORT_TERM_REF)
        long_term_frame_idx = -1;

    /* Create DPB info */
    ps_dpb_info[i].ps_pic_buf = ps_pic_buf;
    ps_dpb_info[i].ps_prev_dpb = ps_dpb_head;
    ps_dpb_info[i].ps_pic_buf->i4_used_as_ref = reference_type;
    ps_dpb_info[i].ps_pic_buf->i4_frame_num = frame_num;
    ps_dpb_info[i].ps_pic_buf->i4_long_term_frame_idx = long_term_frame_idx;

    /* update the head node of linked list to point to the current picture */
    if(reference_type == SHORT_TERM_REF)
    {
        ps_dpb_mgr->ps_dpb_short_term_head = ps_dpb_info + i;

        /* Increment Short term buffer count */
        ps_dpb_mgr->u1_num_short_term_ref_bufs++;

    }
    else
    {
        ps_dpb_mgr->ps_dpb_long_term_head = ps_dpb_info + i;

        /* Increment Long term buffer count */
        ps_dpb_mgr->u1_num_long_term_ref_bufs++;
    }

    return 0;
}

/**
 *******************************************************************************
 *
 * @brief
 *  Add a ref frame to short-term or long-term linked list.
 *
 * @par Description:
 *  This function adds a ref frame to either short-term or long-term linked
 *  list. Internally it calls add ref field twice to add top and bottom field.
 *
 * @param[in] ps_dpb_mgr
 *  Pointer to the DPB manager structure
 *
 * @param[in] ps_pic_buf
 *  Pic buf structure for the field being added.
 *
 * @param[in] reference_type
 *  This is used to select between short-term and long-term linked list.
 *
 * @param[in] frame_num
 *  frame_num for the field.
 *
 * @param[in] long_term_frame_idx
 *  If the ref being added is long-term, long_term_frame_idx of the field.
 *  Otherwise invalid.
 *
 * @returns
 *
 * @remarks
 *
 *
 *******************************************************************************
 */
WORD32 ih264_dpb_mgr_insert_ref_frame(dpb_mgr_t *ps_dpb_mgr,
                                      pic_buf_t *ps_pic_buf,
                                      WORD32 reference_type,
                                      UWORD32 frame_num,
                                      WORD32 long_term_frame_idx)
{
    WORD32 buf_id;
    pic_buf_t *ps_pic_top;
    pic_buf_t *ps_pic_bottom;
    WORD32 ret;

    /*
     * For a frame, since the ps_pic_buf passed to this function is that of top field
     * obtain bottom field using buf_id.
     */
    ps_pic_top = ps_pic_buf;
    buf_id = ps_pic_top->i4_buf_id;
    ps_pic_bottom = &ps_dpb_mgr->as_bottom_field_pics[buf_id];

    /* Insert top field */
    ret = ih264_dpb_mgr_insert_ref_field(ps_dpb_mgr,
                                       ps_pic_top,
                                       reference_type,
                                       frame_num,
                                       long_term_frame_idx);

    if(ret != 0)
        return ret;

    /* Insert bottom field */
    ret = ih264_dpb_mgr_insert_ref_field(ps_dpb_mgr,
                                       ps_pic_bottom,
                                       reference_type,
                                       frame_num,
                                       long_term_frame_idx);

    if(ret != 0)
        return ret;

    return ret;
}

/**
 *******************************************************************************
 *
 * @brief
 *  Returns the number of ref frames in both the linked list.
 *
 * @par Description:
 *  Returns the count of number of frames, number of complementary field pairs
 *  and number of unpaired fields.
 *
 * @param[in] ps_dpb_mgr
 *  Pointer to the DPB manager structure
 *
 * @param[in] curr_frame_num
 *  frame_num for the field.
 *
 * @param[in] max_frame_num
 *  Maximum frame_num allowed
 *
 * @returns
 *
 * @remarks
 *
 *
 *******************************************************************************
 */
WORD32 ih264_dpb_mgr_count_ref_frames(dpb_mgr_t *ps_dpb_mgr,
                                      WORD32 curr_frame_num,
                                      WORD32 max_frame_num)
{
    WORD32 numShortTerm = 0;
    WORD32 numLongTerm = 0;
    dpb_info_t *ps_dpb_node;
    WORD32 frame_num;
    WORD32 prev_frame_num;

    /*
     * Compute the number of short-term frames/complementary field pairs/
     * unpaired fields
     */
    if(ps_dpb_mgr->ps_dpb_short_term_head != NULL)
    {
        /* Sort the short-term list by frame_num */
        ih264_dpb_mgr_sort_short_term_fields_by_frame_num(ps_dpb_mgr,
                                                        curr_frame_num,
                                                        TOP_FIELD,
                                                        max_frame_num);

        ps_dpb_node = ps_dpb_mgr->ps_dpb_short_term_head;
        if(ps_dpb_node != NULL)
        {
            numShortTerm++;
            prev_frame_num = ps_dpb_node->ps_pic_buf->i4_frame_num;
            ps_dpb_node = ps_dpb_node->ps_prev_dpb;
        }

        while(ps_dpb_node != NULL)
        {
            frame_num = ps_dpb_node->ps_pic_buf->i4_frame_num;
            if(frame_num != prev_frame_num)
                numShortTerm++;
            prev_frame_num = ps_dpb_node->ps_pic_buf->i4_frame_num;
            ps_dpb_node = ps_dpb_node->ps_prev_dpb;
        }
    }

    /*
     * Compute the number of long-term frames/complementary field pairs/
     * unpaired fields
     */
    if(ps_dpb_mgr->ps_dpb_long_term_head != NULL)
    {
        ih264_dpb_mgr_sort_long_term_fields_by_frame_idx(ps_dpb_mgr,
                                                        TOP_FIELD);

        ps_dpb_node = ps_dpb_mgr->ps_dpb_long_term_head;
        if(ps_dpb_node != NULL)
        {
            numLongTerm++;
            prev_frame_num = ps_dpb_node->ps_pic_buf->i4_frame_num;
            ps_dpb_node = ps_dpb_node->ps_prev_dpb;
        }

        while(ps_dpb_node != NULL)
        {
            frame_num = ps_dpb_node->ps_pic_buf->i4_frame_num;
            if(frame_num != prev_frame_num)
                numLongTerm++;
            prev_frame_num = ps_dpb_node->ps_pic_buf->i4_frame_num;
            ps_dpb_node = ps_dpb_node->ps_prev_dpb;
        }
    }
    return (numShortTerm + numLongTerm);
}

/**
 *******************************************************************************
 *
 * @brief
 *  Deletes the ref frame at the end of the linked list.
 *
 * @par Description:
 *  Deletes the ref frame at the end of the linked list. For unpaired fields,
 *  it deletes just the last node. For frame or complementary field pair, it
 *  deletes the last two nodes.
 *
 * @param[in] ps_dpb_mgr
 *  Pointer to the DPB manager structure
 *
 * @param[in] reference_type
 *  This is used to select between short-term and long-term linked list.
 *
 * @returns
 *
 * @remarks
 *
 *
 *******************************************************************************
 */
WORD32 ih264_dpb_mgr_delete_ref_frame(dpb_mgr_t *ps_dpb_mgr,
                                      WORD32 reference_type)
{
    dpb_info_t *ps_dpb_node1;
    dpb_info_t *ps_dpb_node2;
    dpb_info_t *ps_dpb_node3;

    /*
     * Assumption: The nodes sorted for frame num.
     */


    /* Select bw short-term and long-term list. */
    ps_dpb_node1 = (reference_type == SHORT_TERM_REF)
                    ?ps_dpb_mgr->ps_dpb_short_term_head
                    :ps_dpb_mgr->ps_dpb_long_term_head;
    /* If null, no entries in the list. Hence return. */
    if(ps_dpb_node1 == NULL)
        return 0;

    /* If only one node in the list, set as unsed for refer and return. */
    if(ps_dpb_node1->ps_prev_dpb == NULL)
    {
        /* Set the picture as unused for reference */
        ps_dpb_node1->ps_pic_buf->i4_used_as_ref = UNUSED_FOR_REF;
        ps_dpb_node1->ps_pic_buf = NULL;

        if(reference_type == SHORT_TERM_REF)
        {
            ps_dpb_mgr->ps_dpb_short_term_head = NULL;

            /* Increment Short term buffer count */
            ps_dpb_mgr->u1_num_short_term_ref_bufs = 0;

        }
        else
        {
            ps_dpb_mgr->ps_dpb_long_term_head = NULL;

            /* Increment Long term buffer count */
            ps_dpb_mgr->u1_num_long_term_ref_bufs = 0;

        }
        return 0;
    }

    /**
     * If there are only 2 nodes in the list, set second node as unused for reference.
     * If the frame_num of second node and first node is same, set first node also as
     * unused for reference and set the corresponding head to NULL.
     */
    ps_dpb_node2 = ps_dpb_node1->ps_prev_dpb;
    if(ps_dpb_node2->ps_prev_dpb == NULL)
    {
        /* Set the picture as unused for reference */
        if(ps_dpb_node2->ps_pic_buf->i4_frame_num == ps_dpb_node1->ps_pic_buf->i4_frame_num)
        {
            /* Set the picture as unused for reference */
            ps_dpb_node1->ps_pic_buf->i4_used_as_ref = UNUSED_FOR_REF;
            ps_dpb_node1->ps_pic_buf = NULL;
            if(reference_type == SHORT_TERM_REF)
            {
                ps_dpb_mgr->ps_dpb_short_term_head = NULL;

                /* Increment Short term buffer count */
                ps_dpb_mgr->u1_num_short_term_ref_bufs = 0;

            }
            else
            {
                ps_dpb_mgr->ps_dpb_long_term_head = NULL;

                /* Increment Long term buffer count */
                ps_dpb_mgr->u1_num_long_term_ref_bufs = 0;

            }

        }
        ps_dpb_node2->ps_pic_buf->i4_used_as_ref = UNUSED_FOR_REF;
        ps_dpb_node2->ps_pic_buf = NULL;
        ps_dpb_node1->ps_prev_dpb = NULL;
        return 0;
    }
    /*
     * If there are more than 2 nodes, run a loop to get the last 3 nodes.
     */
    ps_dpb_node3 = ps_dpb_node2->ps_prev_dpb;
    while(ps_dpb_node3->ps_prev_dpb != NULL)
    {
        ps_dpb_node1 = ps_dpb_node2;
        ps_dpb_node2 = ps_dpb_node3;
        ps_dpb_node3 = ps_dpb_node3->ps_prev_dpb;
    }
    /*
     * If node 2 and node 3 frame_nums are same, set node 2 also as unsed for
     * reference and del reference from node1.
     */
    if(ps_dpb_node2->ps_pic_buf->i4_frame_num == ps_dpb_node3->ps_pic_buf->i4_frame_num)
    {
        ps_dpb_node2->ps_pic_buf->i4_used_as_ref = UNUSED_FOR_REF;
        ps_dpb_node2->ps_pic_buf = NULL;
        ps_dpb_node1->ps_prev_dpb = NULL;

    }
    /* Set the third node as unused for reference */
    ps_dpb_node3->ps_pic_buf->i4_used_as_ref = UNUSED_FOR_REF;
    ps_dpb_node3->ps_pic_buf = NULL;
    ps_dpb_node2->ps_prev_dpb = NULL;

    return 0;
}
/**
 *******************************************************************************
 *
 * @brief
 *  Delete long-term ref fields above max frame idx.
 *
 * @par Description:
 *  Deletes all the long-term ref fields having idx greater than max_frame_idx
 *
 * @param[in] ps_dpb_mgr
 *  Pointer to the DPB manager structure
 *
 * @param[in] max_frame_idx
 *  Max long-term frame idx allowed.
 *
 * @returns
 *
 * @remarks
 *
 *
 *******************************************************************************
 */
WORD32 ih264_dpb_mgr_delete_long_ref_fields_max_frame_idx(dpb_mgr_t *ps_dpb_mgr,
                                                          WORD32 max_frame_idx)
{
    dpb_info_t *ps_dpb_node1;
    dpb_info_t *ps_dpb_node2;
    /*
     * Loop until there is node which isn't to be deleted is encountered.
     */
    while(ps_dpb_mgr->ps_dpb_long_term_head != NULL)
    {
        if(ps_dpb_mgr->ps_dpb_long_term_head->ps_pic_buf->i4_long_term_frame_idx
                        <= max_frame_idx)
        {
            break;
        }
        ps_dpb_mgr->ps_dpb_long_term_head->ps_pic_buf->i4_used_as_ref = UNUSED_FOR_REF;
        ps_dpb_mgr->ps_dpb_long_term_head->ps_pic_buf = NULL;
        ps_dpb_mgr->ps_dpb_long_term_head = ps_dpb_mgr->ps_dpb_long_term_head->ps_prev_dpb;
    }

    ps_dpb_node1 = ps_dpb_mgr->ps_dpb_long_term_head;
    if(ps_dpb_node1 == NULL)
        return 0;
    /*
     * With the node that isn't to be deleted as head, loop until the end.
     */
    ps_dpb_node2 = ps_dpb_node1->ps_prev_dpb;
    while(ps_dpb_node2 != NULL)
    {
        if(ps_dpb_node2->ps_pic_buf->i4_long_term_frame_idx > max_frame_idx)
        {
            ps_dpb_node2->ps_pic_buf->i4_used_as_ref = UNUSED_FOR_REF;
            ps_dpb_node2->ps_pic_buf = NULL;
            ps_dpb_node1->ps_prev_dpb = ps_dpb_node2->ps_prev_dpb;
        }
        ps_dpb_node1 = ps_dpb_node1->ps_prev_dpb;
        if(ps_dpb_node1 == NULL)
            break;
        ps_dpb_node2 = ps_dpb_node1->ps_prev_dpb;
    }
    return 0;
}

/**
 *******************************************************************************
 *
 * @brief
 *  Deletes the short-term with least frame_num
 *
 * @par Description:
 *  Deletes the short-term with least frame_num. It sorts the function the
 *  short-term linked list by frame-num and the function that deletes the last
 *  frame in the linked list.
 *
 * @param[in] ps_dpb_mgr
 *  Pointer to the DPB manager structure
 *
 * @param[in] curr_frame_num
 *  frame_num of the current pic
 *
 * @param[in] max_frame_num
 *  Maximum frame_num allowed
 *
 * @returns
 *
 * @remarks
 *
 *
 *******************************************************************************
 */
WORD32 ih264_dpb_mgr_delete_short_ref_frame(dpb_mgr_t *ps_dpb_mgr,
                                            WORD32 curr_frame_num,
                                            WORD32 max_frame_num)
{
    WORD32 ret;
    /* Sort the short-term list by frame_num */
    ret = ih264_dpb_mgr_sort_short_term_fields_by_frame_num(ps_dpb_mgr,
                                                          curr_frame_num,
                                                          TOP_FIELD,
                                                          max_frame_num);

    /* Delete the last reference frame or field */
    ret = ih264_dpb_mgr_delete_ref_frame(ps_dpb_mgr,SHORT_TERM_REF);

    if(ret != 0)
    {
        ASSERT(0);
    }

    return ret;
}
/**
 *******************************************************************************
 *
 * @brief
 *  Deletes all the ref frames.
 *
 * @par Description:
 *  Deletes all of the ref frames/fields in the short-term and long-term linked
 *  list.
 *
 * @param[in] ps_dpb_mgr
 *  Pointer to the DPB manager structure
 *
 * @returns
 *
 * @remarks
 *
 *
 *******************************************************************************
 */
WORD32 ih264_dpb_mgr_delete_all_ref_frames(dpb_mgr_t *ps_dpb_mgr)
{
    /* Loop over short-term linked list. */
    while(ps_dpb_mgr->ps_dpb_short_term_head != NULL)
    {
        ih264_dpb_mgr_delete_ref_frame(ps_dpb_mgr,SHORT_TERM_REF);
    }

    /* Loop over long-term linked list. */
    while(ps_dpb_mgr->ps_dpb_long_term_head != NULL)
    {
        ih264_dpb_mgr_delete_ref_frame(ps_dpb_mgr,LONG_TERM_REF);
    }
    return 0;
}


void ih264_dpb_mgr_reset(dpb_mgr_t *ps_dpb_mgr, buf_mgr_t *ps_buf_mgr)
{
    WORD32 i;
    dpb_info_t *ps_dpb_info;
    ASSERT(0);


    ps_dpb_info = ps_dpb_mgr->as_dpb_info;

    for(i = 0; i < MAX_DPB_BUFS; i++)
    {
        if(ps_dpb_info[i].ps_pic_buf->i4_used_as_ref)
        {
            ps_dpb_info[i].ps_pic_buf->i4_used_as_ref = UNUSED_FOR_REF;
            ps_dpb_info[i].ps_prev_dpb = NULL;
            //Release physical buffer
            ih264_buf_mgr_release(ps_buf_mgr, ps_dpb_info[i].ps_pic_buf->i4_buf_id,
                                  BUF_MGR_REF);

            ps_dpb_info[i].ps_pic_buf = NULL;
        }
    }
    ps_dpb_mgr->u1_num_short_term_ref_bufs = 0;
    ps_dpb_mgr->u1_num_long_term_ref_bufs  = 0;
    ps_dpb_mgr->ps_dpb_short_term_head = NULL;
    ps_dpb_mgr->ps_dpb_long_term_head  = NULL;

}

/**
 *******************************************************************************
 *
 * @brief
 *  deletes all pictures from DPB
 *
 * @par Description:
 *  Deletes all pictures present in the DPB manager
 *
 * @param[in] ps_buf_mgr
 *  Pointer to buffer manager structure
 *
 * @param[in] u1_disp_bufs
 *  Number of buffers to be deleted
 *
 * @returns
 *
 * @remarks
 *
 *
 *******************************************************************************
 */

void ih264_dpb_mgr_release_pics(buf_mgr_t *ps_buf_mgr, UWORD8 u1_disp_bufs)
{
    WORD8 i;
    UWORD32 buf_status;
    ASSERT(0);

    for(i = 0; i < u1_disp_bufs; i++)
    {
        buf_status = ih264_buf_mgr_get_status(ps_buf_mgr, i);
        if(0 != buf_status)
        {
            ih264_buf_mgr_release((buf_mgr_t *)ps_buf_mgr, i, BUF_MGR_REF);
        }
    }
}
