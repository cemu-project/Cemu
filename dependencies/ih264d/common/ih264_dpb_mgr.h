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
 *  ih264_dpb_mgr.h
 *
 * @brief
 *  Function declarations used for decoded picture buffer management
 *
 * @author
 *  Srinivas T
 *
 *
 * @remarks
 *  None
 *
 *******************************************************************************
 */
#ifndef _IH264_DPB_MGR_H_
#define _IH264_DPB_MGR_H_

/* Temporary definitions. Have to be defined later */

#define MAX_DPB_BUFS                (MAX_DPB_SIZE * 4)

#define MARK_ST_PICNUM_AS_NONREF    1
#define MARK_LT_INDEX_AS_NONREF     2
#define MARK_ST_PICNUM_AS_LT_INDEX  3
#define RESET_REF_PICTURES          5

typedef struct dpb_info_t dpb_info_t;

enum
{
    INVALID = -1,
    UNUSED_FOR_REF = 0  ,
    LONG_TERM_REF       ,
    SHORT_TERM_REF      ,
};
struct dpb_info_t
{
    /**
     * Pointer to picture buffer structure
     */
    pic_buf_t *ps_pic_buf;

    /**
     * Link to the DPB buffer with previous link
     */
    dpb_info_t *ps_prev_dpb;

};

typedef struct
{
    /**
     * Pointer to the most recent pic Num
     */
    dpb_info_t *ps_dpb_short_term_head;

    /**
     * Pointer to the most recent pic Num
     */
    dpb_info_t *ps_dpb_long_term_head;

    /**
     * Physical storage for dpbInfo for ref bufs
     */
    dpb_info_t as_dpb_info[MAX_DPB_BUFS];

    /**
     * Array of structures for bottom field.
     */
    pic_buf_t as_top_field_pics[MAX_DPB_BUFS];

    /**
     * Array of structures for bottom field.
     */
    pic_buf_t as_bottom_field_pics[MAX_DPB_BUFS];

    /**
     * Number of short-term reference buffers
     */
    UWORD8 u1_num_short_term_ref_bufs;

    /**
     * Number of long-term reference buffers
     */
    UWORD8 u1_num_long_term_ref_bufs;

    /**
     * buffer ID current frame
     */
    WORD32 i4_cur_frame_buf_id;

} dpb_mgr_t;

void ih264_dpb_mgr_init(dpb_mgr_t *ps_dpb_mgr);

WORD32 ih264_dpb_mgr_insert_ref_frame(dpb_mgr_t *ps_dpb_mgr,
                                      pic_buf_t *ps_pic_buf,
                                      WORD32 reference_type,
                                      UWORD32 frame_num,
                                      WORD32 long_term_frame_idx);

WORD32 ih264_dpb_mgr_delete_ref_frame(dpb_mgr_t *ps_dpb_mgr,
                                      WORD32 reference_type);

WORD32 ih264_dpb_mgr_delete_all_ref_frames(dpb_mgr_t *ps_dpb_mgr);

WORD32 ih264_dpb_mgr_count_ref_frames(dpb_mgr_t *ps_dpb_mgr,
                                      WORD32 curr_frame_num,
                                      WORD32 max_frame_num);

WORD32 ih264_dpb_mgr_delete_short_ref_frame(dpb_mgr_t *ps_dpb_mgr,
                                            WORD32 curr_frame_num,
                                            WORD32 max_frame_num);

WORD32 ih264_dpb_mgr_insert_ref_field(dpb_mgr_t *ps_dpb_mgr,
                                      pic_buf_t *ps_pic_buf,
                                      WORD32 reference_type,
                                      UWORD32 frame_num,
                                      WORD32 long_term_frame_idx);

WORD32 ih264_dpb_mgr_delete_ref_field(dpb_mgr_t *ps_dpb_mgr,
                                      WORD32 reference_type);

WORD32 ih264_dpb_mgr_alternate_ref_fields(dpb_mgr_t *ps_dpb_mgr,
                                          WORD32 reference_type,
                                          WORD32 first_field_type);

WORD32 ih264_dpb_mgr_sort_short_term_fields_by_frame_num(dpb_mgr_t *ps_dpb_mgr,
                                                         WORD32 curr_frame_num,
                                                         WORD32 first_field_type,
                                                         WORD32 max_frame_num);

WORD32 ih264_dpb_mgr_sort_short_term_fields_by_poc_l0(dpb_mgr_t *ps_dpb_mgr,
                                                      WORD32 curr_poc,
                                                      WORD32 first_field_type);

WORD32 ih264_dpb_mgr_sort_short_term_fields_by_poc_l1(dpb_mgr_t *ps_dpb_mgr,
                                                      WORD32 curr_poc,
                                                      WORD32 first_field_type);

WORD32 ih264_dpb_mgr_sort_long_term_fields_by_frame_idx(dpb_mgr_t *ps_dpb_mgr,
                                                        WORD32 first_field_type);

WORD32 ih264_dpb_mgr_delete_long_ref_fields_max_frame_idx(dpb_mgr_t *ps_dpb_mgr,
                                                          WORD32 max_frame_idx);

void ih264_dpb_mgr_del_ref(dpb_mgr_t *ps_dpb_mgr,
                           buf_mgr_t *ps_buf_mgr,
                           WORD32 u4_abs_poc);

pic_buf_t *ih264_dpb_mgr_get_ref_by_nearest_poc(dpb_mgr_t *ps_dpb_mgr,
                                                WORD32 cur_abs_poc);

pic_buf_t *ih264_dpb_mgr_get_ref_by_poc(dpb_mgr_t *ps_dpb_mgr, WORD32 abs_poc);

pic_buf_t *ih264_dpb_mgr_get_ref_by_poc_lsb(dpb_mgr_t *ps_dpb_mgr,
                                            WORD32 poc_lsb);

void ih264_dpb_mgr_reset(dpb_mgr_t *ps_dpb_mgr, buf_mgr_t *ps_buf_mgr);

void ih264_dpb_mgr_release_pics(buf_mgr_t *ps_buf_mgr, UWORD8 u1_disp_bufs);

#endif /*  _IH264_DPB_MGR_H_ */
