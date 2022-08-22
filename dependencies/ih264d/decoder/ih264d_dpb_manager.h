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
#ifndef _IH264D_DPB_MANAGER_H_
#define _IH264D_DPB_MANAGER_H_
/*!
***************************************************************************
* \file ih264d_dpb_manager.h
*
* \brief
*    Decoded Picture Buffer Manager Include File
*
* Detailed_description
*
* \date
*    19-12-2002
*
* \author  Sriram Sethuraman
***************************************************************************
*/
#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264d_bitstrm.h"
#include "ih264d_defs.h"

#define END_OF_MMCO                 0
#define MARK_ST_PICNUM_AS_NONREF    1
#define MARK_LT_INDEX_AS_NONREF     2
#define MARK_ST_PICNUM_AS_LT_INDEX  3
#define SET_MAX_LT_INDEX            4
#define RESET_REF_PICTURES          5
#define SET_LT_INDEX                6
#define RESET_NONREF_PICTURES       7
#define RESET_ALL_PICTURES          8

#define NO_LONG_TERM_INDICIES      255
struct field_t
{
    /* picNum of tbe reference field              */
    WORD32 i4_pic_num;

    /*  assigned when used for long term reference */
    /* else MAX_REF_BUFS+1 */
    UWORD8 u1_long_term_frame_idx;

    /* 0 : unused for reference                   */
    /* 1 : used for short term reference          */
    /* 2 : used for long term reference           */
    UWORD8 u1_reference_info;
};


struct dpb_info_t
{
  struct pic_buffer_t *ps_pic_buf;       /** Pointer to picture buffer structure */
  WORD32 i4_frame_num;      /** frame number of picture - unique for each ref*/
  struct dpb_info_t *ps_prev_short;/** Link to the DPB with previous picNum */
  struct dpb_info_t *ps_prev_long;     /** Link to the DPB with previous long term frame*/
  struct field_t s_top_field;       /** Contains information of the top_field
                                     reference info, pic num and longt term frame idx */
  struct field_t s_bot_field;       /** Contains information of the bot_field
                                     reference info, pic num and longt term frame idx */
  UWORD8 u1_buf_id;     /** bufID from bufAPI */
  UWORD8 u1_used_as_ref;        /** whether buffer is used as ref for frame or
                                     complementary reference field pair */
  UWORD8 u1_lt_idx;     /** If buf is assigned long-term index; else MAX_REF_BUFS+1 */

};

typedef struct
{
  struct pic_buffer_t *ps_def_dpb[MAX_REF_BUFS];/** DPB in default index order */
  struct pic_buffer_t *ps_mod_dpb[2][2 * MAX_REF_BUFS];/** DPB in reordered index order, 0-fwd,1-bwd */
  struct pic_buffer_t *ps_init_dpb[2][2 * MAX_REF_BUFS];/** DPB in reordered index order, 0-fwd,1-bwd */
  struct dpb_info_t *ps_dpb_st_head;     /** Pointer to the most recent picNum */
  struct dpb_info_t *ps_dpb_ht_head;     /** Pointer to the smallest LT index */
  struct dpb_info_t as_dpb_info[MAX_REF_BUFS];       /** Physical storage for dpbInfo for ref bufs */
  UWORD8 u1_num_st_ref_bufs;        /** Number of short term ref. buffers */
  UWORD8 u1_num_lt_ref_bufs;        /** Number of long term ref. buffer */
  UWORD8 u1_max_lt_frame_idx;       /** Maximum long term frame index */
  UWORD8 u1_num_gaps;       /** Total number of outstanding gaps */
  void * pv_codec_handle;             /* For Error Handling */
  WORD32 i4_max_frm_num;        /** Max frame number */
  WORD32 ai4_gaps_start_frm_num[MAX_FRAMES];/** start frame number for a gap seqn */
  WORD32 ai4_gaps_end_frm_num[MAX_FRAMES];       /** start frame number for a gap seqn */
  WORD8  ai1_gaps_per_seq[MAX_FRAMES];      /** number of gaps with each gap seqn */
  WORD32 ai4_poc_buf_id_map[MAX_FRAMES][3];
  WORD8 i1_poc_buf_id_entries;
  WORD8 i1_gaps_deleted;
  UWORD16 u2_pic_wd;
  UWORD16 u2_pic_ht;
  UWORD8 u1_mmco_error_in_seq;
}dpb_manager_t;

/** Structure store the MMC Commands */
struct MMCParams
{
  UWORD32 u4_mmco;      /** memory managemet control operation */
  UWORD32 u4_diff_pic_num;      /** diff Of Pic Nums Minus1 */
  UWORD32 u4_lt_idx;        /** Long Term Pic Idx */
  UWORD32 u4_max_lt_idx_plus1;      /** MaxLongTermPicIdxPlus1 */
};

typedef struct
{
  UWORD8  u1_dpb_commands_read;     /** Flag to indicate that DBP commands are read */
  UWORD8  u1_buf_mode;      /** decoder Pic bugffering mode*/
  UWORD8  u1_num_of_commands;       /** Number of MMC commands */
  /* These variables are ised in case of IDR pictures only */
  UWORD8  u1_idr_pic;       /** = 1 ,IDR pic */
  UWORD8  u1_no_output_of_prior_pics_flag;
  UWORD8  u1_long_term_reference_flag;
  struct MMCParams  as_mmc_params[MAX_REF_BUFS];      /* < Buffer to store MMC commands */
  UWORD8 u1_dpb_commands_read_slc;
}dpb_commands_t;

void ih264d_init_ref_bufs(dpb_manager_t *ps_dpb_mgr);

WORD32 ih264d_insert_st_node(dpb_manager_t *ps_dpb_mgr,
                          struct pic_buffer_t *ps_pic_buf,
                          UWORD8 u1_buf_id,
                          UWORD32 u2_cur_pic_num);
WORD32 ih264d_update_default_index_list(dpb_manager_t *ps_dpb_mgr);
WORD32 ih264d_do_mmco_buffer(dpb_commands_t *ps_dpb_cmds,
                          dpb_manager_t *ps_dpb_mgr,
                          UWORD8 u1_numRef_frames_for_seq,
                          UWORD32 u4_cur_pic_num,
                          UWORD32 u2_u4_max_pic_num_minus1,
                          UWORD8 u1_nal_unit_type,
                          struct pic_buffer_t *ps_pic_buf,
                          UWORD8 u1_buf_id,
                          UWORD8 u1_fld_pic_flag,
                          UWORD8 u1_curr_pic_in_err);
void ih264d_release_pics_in_dpb(void *pv_dec,
                                UWORD8 u1_disp_bufs);
void ih264d_reset_ref_bufs(dpb_manager_t *ps_dpb_mgr);
WORD32 ih264d_delete_st_node_or_make_lt(dpb_manager_t *ps_dpb_mgr,
                                      WORD32 u4_pic_num,
                                      UWORD32 u4_lt_idx,
                                      UWORD8 u1_fld_pic_flag);

WORD32 ih264d_delete_gap_frm_mmco(dpb_manager_t *ps_dpb_mgr,
                                  WORD32 i4_frame_num,
                                  UWORD8 *pu1_del_node);

WORD32 ih264d_delete_gap_frm_sliding(dpb_manager_t *ps_dpb_mgr,
                                     WORD32 i4_frame_num,
                                     UWORD8 *pu1_del_node);

WORD32 ih264d_do_mmco_for_gaps(dpb_manager_t *ps_dpb_mgr,
                             UWORD8 u1_num_ref_frames);

WORD32 ih264d_insert_pic_in_display_list(dpb_manager_t *ps_dpb_mgr,
                                         UWORD8 u1_buf_id,
                                         WORD32 i4_display_poc,
                                         UWORD32 u4_frame_num);
void ih264d_delete_nonref_nondisplay_pics(dpb_manager_t *ps_dpb_mgr);
#endif /*  _IH264D_DPB_MANAGER_H_ */
