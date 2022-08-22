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
#ifndef _IH264D_STRUCTS_H_
#define _IH264D_STRUCTS_H_

#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "iv.h"
#include "ivd.h"

#include "ih264d_transfer_address.h"
#include "ih264d_defs.h"
#include "ih264d_defs.h"
#include "ih264d_bitstrm.h"
#include "ih264d_debug.h"
#include "ih264d_dpb_manager.h"
/* includes for CABAC */
#include "ih264d_cabac.h"
#include "ih264d_dpb_manager.h"

#include "ih264d_vui.h"
#include "ih264d_sei.h"
#include "iv.h"
#include "ivd.h"

#include "ih264_weighted_pred.h"
#include "ih264_trans_quant_itrans_iquant.h"
#include "ih264_inter_pred_filters.h"
#include "ih264_mem_fns.h"
#include "ih264_padding.h"
#include "ih264_intra_pred_filters.h"
#include "ih264_deblk_edge_filters.h"





/** Number of Mb's whoose syntax will be read */
/************************************************************/
/* MB_GROUP should be a multiple of 2                       */
/************************************************************/
#define PARSE_MB_GROUP_4            4

/* MV_SCRATCH_BUFS assumed to be pow(2) */
#define MV_SCRATCH_BUFS             4

#define TOP_FIELD_ONLY      0x02
#define BOT_FIELD_ONLY      0x01

#define MAX_REF_BUF_SIZE       (3776*2*2)

struct _DecStruct;
struct _DecMbInfo;

typedef enum
{
    MB_TYPE_SI_SLICE = 0,
    MB_TYPE_I_SLICE = 3,
    MB_SKIP_FLAG_P_SLICE = 11,
    MB_TYPE_P_SLICE = 14,
    SUB_MB_TYPE_P_SLICE = 21,
    MB_SKIP_FLAG_B_SLICE = 24,
    MB_TYPE_B_SLICE = 27,
    SUB_MB_TYPE_B_SLICE = 36,
    MVD_X = 40,
    MVD_Y = 47,
    REF_IDX = 54,
    MB_QP_DELTA = 60,
    INTRA_CHROMA_PRED_MODE = 64,
    PREV_INTRA4X4_PRED_MODE_FLAG = 68,
    REM_INTRA4X4_PRED_MODE = 69,
    MB_FIELD_DECODING_FLAG = 70,
    CBP_LUMA = 73,
    CBP_CHROMA = 77,
    CBF = 85,
    SIGNIFICANT_COEFF_FLAG_FRAME = 105,
    SIGNIFICANT_COEFF_FLAG_FLD = 277,
    LAST_SIGNIFICANT_COEFF_FLAG_FRAME = 166,
    LAST_SIGNIFICANT_COEFF_FLAG_FLD = 338,
    COEFF_ABS_LEVEL_MINUS1 = 227,

    /* High profile related Syntax element CABAC offsets */
    TRANSFORM_SIZE_8X8_FLAG = 399,
    SIGNIFICANT_COEFF_FLAG_8X8_FRAME = 402,
    LAST_SIGNIFICANT_COEFF_FLAG_8X8_FRAME = 417,
    COEFF_ABS_LEVEL_MINUS1_8X8 = 426,
    SIGNIFICANT_COEFF_FLAG_8X8_FIELD = 436,
    LAST_SIGNIFICANT_COEFF_FLAG_8X8_FIELD = 451

} cabac_table_num_t;

typedef enum
{
    SIG_COEFF_CTXT_CAT_0_OFFSET = 0,
    SIG_COEFF_CTXT_CAT_1_OFFSET = 15,
    SIG_COEFF_CTXT_CAT_2_OFFSET = 29,
    SIG_COEFF_CTXT_CAT_3_OFFSET = 44,
    SIG_COEFF_CTXT_CAT_4_OFFSET = 47,
    SIG_COEFF_CTXT_CAT_5_OFFSET = 0,
    COEFF_ABS_LEVEL_CAT_0_OFFSET = 0,
    COEFF_ABS_LEVEL_CAT_1_OFFSET = 10,
    COEFF_ABS_LEVEL_CAT_2_OFFSET = 20,
    COEFF_ABS_LEVEL_CAT_3_OFFSET = 30,
    COEFF_ABS_LEVEL_CAT_4_OFFSET = 39,
    COEFF_ABS_LEVEL_CAT_5_OFFSET = 0
} cabac_blk_cat_offset_t;

/** Structure for the MV bank */
typedef struct _mv_pred_t
{
    WORD16 i2_mv[4]; /** 0- mvFwdX, 1- mvFwdY, 2- mvBwdX, 3- mvBwdY */
    WORD8 i1_ref_frame[2];

    UWORD8 u1_col_ref_pic_idx; /** Idx into the pic buff array */
    UWORD8 u1_pic_type; /** Idx into the pic buff array */

} mv_pred_t;

typedef struct
{
    WORD32 i4_mv_indices[16];
    WORD8 i1_submb_num[16];
    WORD8 i1_partitionsize[16];
    WORD8 i1_num_partitions;
    WORD8 u1_vert_mv_scale;
    UWORD8 u1_col_zeroflag_change;
} directmv_t;

typedef struct pic_buffer_t
{
    /**Different components of the picture */
    UWORD8 *pu1_buf1;
    UWORD8 *pu1_buf2;
    UWORD8 *pu1_buf3;
    UWORD16 u2_disp_width; /** Width of the display luma frame in pixels */
    UWORD16 u2_disp_height; /** Height of the display luma frame in pixels */
    UWORD32 u4_time_stamp; /** Time at which frame has to be displayed */
    UWORD16 u2_frm_wd_y; /** Width of the luma frame in pixels */
    UWORD16 u2_frm_wd_uv; /** Width of the chroma frame */
    UWORD16 u2_frm_ht_y; /** Height of the luma frame in pixels */
    UWORD16 u2_frm_ht_uv; /** Height of the chroma frame */
    /* Upto this is resembling the structure IH264DEC_DispUnit */

    /* If any member is to be added, add below this            */

    /* u4_ofst from start of picture buffer to display position for Y buffer  */
    UWORD16 u2_crop_offset_y;

    /* u4_ofst from start of picture buffer to display position for UV buffer */
    UWORD16 u2_crop_offset_uv;

    UWORD8 u1_is_short; /** (1: short 0: long) term ref pic */
    UWORD8 u1_pic_type; /** frame / field / complementary field pair */
    UWORD8 u1_pic_buf_id; /** Idx into the picBufAPI array */
    UWORD8 u1_mv_buf_id;
    WORD32 i4_seq;
    UWORD8 *pu1_col_zero_flag;
    mv_pred_t *ps_mv; /** Pointer to the MV bank array */
    WORD32 i4_poc; /** POC */
    WORD32 i4_pic_num;
    WORD32 i4_frame_num;
    WORD32 i4_top_field_order_cnt; /** TopPOC */
    WORD32 i4_bottom_field_order_cnt; /** BottomPOC */
    WORD32 i4_avg_poc; /** minPOC */
    UWORD8 u1_picturetype; /*Same as u1_pic_type..u1_pic_type gets overwritten whereas
     this doesnot get overwritten ...stores the pictype of
     frame/complementary field pair/ mbaff */
    UWORD8 u1_long_term_frm_idx;
    UWORD8 u1_long_term_pic_num;
    UWORD32 u4_pack_slc_typ; /* It will contain information about types of slices */

    /* ! */
    UWORD32 u4_ts;
    UWORD8 u1_pic_struct;/* Refer to SEI table D-1 */
    sei s_sei_pic;

} pic_buffer_t;

typedef struct
{
    void *u4_add[4];
} neighbouradd_t;

typedef struct
{
    const UWORD8 *pu1_inv_scan;
    void *pv_table[6];
} cavlc_cntxt_t;

/**
 ************************************************************************
 * \file ih264d_structs.h
 *
 * \brief
 *    Structures used in the H.264 decoder
 *
 * \date
 *    18/11/2002
 *
 * \author  Sriram Sethuraman
 *
 ************************************************************************
 */

/**
 * Structure to represent a MV Bank buffer and col flag
 */
typedef struct
{
    /**
     *  Pointer to buffer that holds col flag.
     */
    void *pv_col_zero_flag;

    /**
     * Pointer to buffer that holds mv_pred
     */
    void *pv_mv;

 }col_mv_buf_t;


typedef struct
{
    UWORD8 u1_dydx; /** 4*dy + dx for Y comp / 8*dy + dx for UV comp */
    UWORD8 u1_is_bi_direct; /** 1: is bi-direct 0: forward / backward only   */
    UWORD8 u1_wght_pred_type; /** 0-default 1-singleWeighted 2-BiWeighted      */
    WORD8 i1_mb_partwidth; /** Width of MB partition                        */
    WORD8 i1_mb_partheight; /** Height of MB partition                       */
    WORD8 i1_mc_wd; /** Number of bytes in a DMA stride              */
    WORD8 i1_dma_ht; /** Number of strides                            */

    WORD8 i1_pod_ht; /** Flag specifying height of pad on demand      */
    /** 0 (No pod) -ve(Top pod) +ve(Bottom pod)      */
    UWORD16 u2_dst_stride; /** Stride value of the destination              */
    UWORD16 u2_u1_ref_buf_wd; /** Width of the ref buffer                      */
    UWORD16 u2_frm_wd;
    UWORD16 u2_dummy;

    UWORD8 *u1_pi1_wt_ofst_rec_v; /** Pointer to packed weight and u4_ofst          */
    UWORD8 *pu1_rec_y_u; /** MB partition address in row buffer           */
    UWORD8 *pu1_dma_dest_addr; /** Destination address for DMA transfer         */
    UWORD8 *pu1_y_ref;
    UWORD8 *pu1_u_ref;
    UWORD8 *pu1_v_ref;

    UWORD8 *pu1_pred;
    UWORD8 *pu1_pred_u;
    UWORD8 *pu1_pred_v;
    UWORD8 u1_dma_wd_y;
    UWORD8 u1_dma_ht_y;
    UWORD8 u1_dma_wd_uv;
    UWORD8 u1_dma_ht_uv;
} pred_info_t;

typedef struct
{
    UWORD32 *pu4_wt_offst;
    WORD16 i2_mv[2];

    /***************************************************/
    /*packing information  i1_size_pos_info             */
    /* bit 1:0 -> X position in terms of  (4x4) units   */
    /* bit 3:2 -> Y position in terms of  (4x4) units   */
    /* bit 5:4 -> PU width 0:4,1:8,2:16                 */
    /* bit 7:6 -> PU height 0:4,1:8,2:16                */
     /***************************************************/
    WORD8 i1_size_pos_info;

    /***************************************************/
    /*packing information ref idx info                 */
    /* bit 5:0 ->ref_idx                              */
    /* bit 6:7   -> 0:l0,1:l1,2:bipred                  */
     /***************************************************/
    WORD8 i1_ref_idx_info;

    WORD8 i1_buf_id;


    UWORD8 u1_pic_type; /** frame /top field/bottom field/mbaff / complementary field pair */

}pred_info_pkd_t;
/*! Sequence level parameters */

typedef struct
{
    UWORD8 u1_seq_parameter_set_id; /** id for the seq par set 0-31 */
    UWORD8 u1_is_valid; /** is Seq Param set valid */

    UWORD16 u2_frm_wd_in_mbs; /** Frame width expressed in MB units */
    UWORD16 u2_frm_ht_in_mbs; /** Frame height expressed in MB units */

    /* Following are derived from the above two */
    UWORD16 u2_fld_ht_in_mbs; /** Field height expressed in MB units */
    UWORD16 u2_max_mb_addr; /** Total number of macroblocks in a coded picture */
    UWORD16 u2_total_num_of_mbs; /** Total number of macroblocks in a coded picture */
    UWORD32 u4_fld_ht; /** field height */
    UWORD32 u4_cwidth; /** chroma width */
    UWORD32 u4_chr_frm_ht; /** chroma height */
    UWORD32 u4_chr_fld_ht; /** chroma field height */
    UWORD8 u1_mb_aff_flag; /** 0 - no mb_aff; 1 - uses mb_aff */

    UWORD8 u1_profile_idc; /** profile value */
    UWORD8 u1_level_idc; /** level value */

    /* high profile related syntax elements   */
    WORD32 i4_chroma_format_idc;
    WORD32 i4_bit_depth_luma_minus8;
    WORD32 i4_bit_depth_chroma_minus8;
    WORD32 i4_qpprime_y_zero_transform_bypass_flag;
    WORD32 i4_seq_scaling_matrix_present_flag;
    UWORD8 u1_seq_scaling_list_present_flag[8];
    UWORD8 u1_use_default_scaling_matrix_flag[8];
    WORD16 i2_scalinglist4x4[6][16];
    WORD16 i2_scalinglist8x8[2][64];
    UWORD8 u1_more_than_one_slice_group_allowed_flag;
    UWORD8 u1_arbitrary_slice_order_allowed_flag;
    UWORD8 u1_redundant_slices_allowed_flag;
    UWORD8 u1_bits_in_frm_num; /** Number of bits in frame num */
    UWORD16 u2_u4_max_pic_num_minus1; /** Maximum frame num minus 1 */
    UWORD8 u1_pic_order_cnt_type; /** 0 - 2 indicates the method to code picture order count */
    UWORD8 u1_log2_max_pic_order_cnt_lsb_minus;
    WORD32 i4_max_pic_order_cntLsb;
    UWORD8 u1_num_ref_frames_in_pic_order_cnt_cycle;
    UWORD8 u1_delta_pic_order_always_zero_flag;
    WORD32 i4_ofst_for_non_ref_pic;
    WORD32 i4_ofst_for_top_to_bottom_field;
    WORD32 i4_ofst_for_ref_frame[MAX_NUM_REF_FRAMES_OFFSET];
    UWORD8 u1_num_ref_frames;
    UWORD8 u1_gaps_in_frame_num_value_allowed_flag;
    UWORD8 u1_frame_mbs_only_flag; /** 1 - frame only; 0 - field/frame pic */
    UWORD8 u1_direct_8x8_inference_flag;
    UWORD8 u1_vui_parameters_present_flag;
    vui_t s_vui;
} dec_seq_params_t;

typedef struct
{
    UWORD16 u2_frm_wd_in_mbs; /** Frame width expressed in MB units    */
    UWORD16 u2_frm_ht_in_mbs; /** Frame height expressed in MB units   */
    UWORD8 u1_frame_mbs_only_flag; /** 1 - frame only; 0 - field/frame pic  */
    UWORD8 u1_profile_idc; /** profile value                        */
    UWORD8 u1_level_idc; /** level value                          */
    UWORD8 u1_direct_8x8_inference_flag;
    UWORD8 u1_eoseq_pending;
} prev_seq_params_t;

/** Picture level parameters */
typedef struct
{
    dec_seq_params_t *ps_sps; /** applicable seq. parameter set */

    /* High profile related syntax elements */
    WORD32 i4_transform_8x8_mode_flag;
    WORD32 i4_pic_scaling_matrix_present_flag;
    UWORD8 u1_pic_scaling_list_present_flag[8];
    UWORD8 u1_pic_use_default_scaling_matrix_flag[8];
    WORD16 i2_pic_scalinglist4x4[6][16];
    WORD16 i2_pic_scalinglist8x8[2][64];
    WORD8 i1_second_chroma_qp_index_offset;

    UWORD32 u4_slice_group_change_rate;
    UWORD8 *pu1_slice_groupmb_map; /** MB map with slice membership labels */
    UWORD8 u1_pic_parameter_set_id; /** id for the picture par set 0-255*/
    UWORD8 u1_entropy_coding_mode; /** Entropy coding : 0-VLC; 1 - CABAC */
    UWORD8 u1_num_slice_groups; /** Number of slice groups */
    UWORD8 u1_pic_init_qp; /** Initial QPY for the picture {-26,25}*/
    WORD8 i1_chroma_qp_index_offset; /** Chroma QP u4_ofst w.r.t QPY {-12,12} */
    UWORD8 u1_dblk_filter_parms_flag; /** Slice layer has deblocking filter parameters */
    UWORD8 u1_constrained_intra_pred_flag; /** Constrained intra prediction u4_flag */
    UWORD8 u1_redundant_pic_cnt_present_flag; /** Redundant_pic_cnt is in slices using this PPS */
    UWORD8 u1_pic_order_present_flag; /** Pic order present u4_flag */
    UWORD8 u1_num_ref_idx_lx_active[2]; /** Maximum reference picture index in the reference list 0 : range [1 - 15] */
    UWORD8 u1_wted_pred_flag;
    UWORD8 u1_wted_bipred_idc;
    UWORD8 u1_pic_init_qs;
    UWORD8 u1_deblocking_filter_parameters_present_flag;
    UWORD8 u1_vui_pic_parameters_flag;
    UWORD8 u1_mb_slice_group_map_type;
    UWORD8 u1_slice_group_change_direction_flag;
    UWORD8 u1_frame_cropping_flag;
    UWORD8 u1_frame_cropping_rect_left_ofst;
    UWORD8 u1_frame_cropping_rect_right_ofst;
    UWORD8 u1_frame_cropping_rect_top_ofst;
    UWORD8 u1_frame_cropping_rect_bottom_ofst;
    void * pv_codec_handle; /* For Error Handling */
    WORD32 i4_top_field_order_cnt;
    WORD32 i4_bottom_field_order_cnt;
    WORD32 i4_avg_poc;
    UWORD8 u1_is_valid; /** is Pic Param set valid */
} dec_pic_params_t;

/** Picture Order Count Paramsters */
typedef struct
{
    WORD32 i4_pic_order_cnt_lsb;
    WORD32 i4_pic_order_cnt_msb;
    WORD32 i4_delta_pic_order_cnt_bottom;
    WORD32 i4_delta_pic_order_cnt[2];
    WORD32 i4_prev_frame_num_ofst;
    UWORD8 u1_mmco_equalto5;
    UWORD8 u1_bot_field;
    UWORD16 u2_frame_num;
    WORD32 i4_top_field_order_count;
    WORD32 i4_bottom_field_order_count;
} pocstruct_t;

/*****************************************************************************/
/* parse_mb_pers_info contains necessary mb info data required persistently  */
/* in the form of top and left neighbours.                                   */
/*****************************************************************************/
typedef struct
{
    void *u4_pic_addrress[4]; /* picture address for BS calc */
    WORD8 pi1_intrapredmodes[4]; /* calc Intra pred modes */
    UWORD8 pu1_nnz_y[4];
    UWORD8 pu1_nnz_uv[4];
    UWORD8 u1_mb_fld;
    UWORD8 u1_mb_type;
    UWORD16 u2_luma_csbp; /* Luma csbp used for BS calc */
    UWORD8 u1_tran_form8x8;
} mb_neigbour_params_t;

/* This info is required for decoding purposes except Deblockng */
typedef struct _DecMbInfo
{
    UWORD8 u1_mb_type; /** macroblock type: I/P/B/SI/SP */
    UWORD8 u1_chroma_pred_mode;
    UWORD8 u1_cbp;
    UWORD8 u1_mb_mc_mode; /** 16x16, 2 16x8, 2 8x16, 4 8x8 */
    UWORD8 u1_topmb; /** top Mb u4_flag */
    UWORD8 u1_mb_ngbr_availablity;
    UWORD8 u1_end_of_slice;
    UWORD8 u1_mb_field_decodingflag;
    UWORD8 u1_topleft_mb_fld;
    UWORD8 u1_topleft_mbtype;
    WORD8 i1_offset;
    UWORD8 u1_Mux;
    UWORD8 u1_qp_div6;
    UWORD8 u1_qp_rem6;
    UWORD8 u1_qpc_div6;
    UWORD8 u1_qpcr_div6;
    UWORD8 u1_qpc_rem6;
    UWORD8 u1_qpcr_rem6;
    UWORD8 u1_tran_form8x8;
    UWORD8 u1_num_pred_parts;
    UWORD8 u1_yuv_dc_block_flag;
    UWORD16 u2_top_right_avail_mask;
    UWORD16 u2_top_left_avail_mask;
    UWORD16 u2_luma_csbp; /** Coded 4x4 Sub Block Pattern */
    UWORD16 u2_chroma_csbp; /** Coded 4x4 Sub Block Pattern */
    UWORD16 u2_mbx;
    UWORD16 u2_mby;
    UWORD16 u2_mask[2];

    UWORD32 u4_pred_info_pkd_idx;

    mb_neigbour_params_t *ps_left_mb;
    mb_neigbour_params_t *ps_top_mb;
    mb_neigbour_params_t *ps_top_right_mb;
    mb_neigbour_params_t *ps_curmb;
} dec_mb_info_t;


/** Slice level parameters */
typedef struct
{
    dec_pic_params_t *ps_pps; /** PPS used */
    WORD32 i4_delta_pic_order_cnt[2];
    WORD32 i4_poc; /** Pic order cnt of picture to which slice belongs*/
    UWORD32 u4_idr_pic_id; /** IDR pic ID */
    UWORD16 u2_first_mb_in_slice; /** Address of first MB in slice*/
    UWORD16 u2_frame_num; /** Frame number from prev IDR pic */

    UWORD8 u1_mbaff_frame_flag; /** Mb adaptive frame field u4_flag */
    UWORD8 u1_field_pic_flag; /** Field picture or not */
    UWORD8 u1_bottom_field_flag; /** If slice belongs to bot field pic */
    UWORD8 u1_slice_type; /** I/P/B/SI/SP */
    WORD32 i4_pic_order_cnt_lsb; /** Picture Order Count */
    UWORD8 u1_slice_qp; /** Add slice_qp_delta to pic_init_QP */
    UWORD8 u1_disable_dblk_filter_idc; /** 0-dblk all edges; 1 - suppress; 2 - suppress only edges */
    WORD8 i1_slice_alpha_c0_offset; /** dblk: alpha and C0 table u4_ofst {-12,12}*/
    WORD8 i1_slice_beta_offset; /** dblk: beta table u4_ofst {-12, 12}*/
    UWORD8 u1_sp_for_switch_flag;
    UWORD8 u1_no_output_of_prior_pics_flag;
    UWORD8 u1_long_term_reference_flag;
    UWORD8 u1_num_ref_idx_lx_active[2];
    UWORD8 u1_cabac_init_idc; /** cabac_init_idc */
    UWORD8 u1_num_ref_idx_active_override_flag;
    UWORD8 u1_direct_spatial_mv_pred_flag;
    WORD32 (*pf_decodeDirect)(struct _DecStruct *ps_dec,
                              UWORD8 u1_wd_x,
                              dec_mb_info_t *ps_cur_mb_info,
                              UWORD8 u1_mb_num);
    UWORD8 u1_redundant_pic_cnt;
    WORD8 i1_slice_qs_delta;
    UWORD8 u1_nal_ref_idc; /** NAL ref idc of the Slice NAL unit */
    UWORD8 u1_nal_unit_type; /** NAL unit type of the Slice NAL */
    UWORD8 u1_direct_8x8_inference_flag;
    UWORD8 u1_mmco_equalto5; /** any of the MMCO command equal to 5 */
    UWORD8 u1_pic_order_cnt_type;
    pocstruct_t s_POC;
    /* DataStructures required for weighted prediction */
    UWORD16 u2_log2Y_crwd; /** Packed luma and chroma log2_weight_denom */
    /* [list0/list1]:[ref pics index]:[0-Y 1-Cb 2-Cr] [weight/u4_ofst],
     weights and offsets are signed numbers, since they are packed, it is defined
     unsigned. LSB byte : weight and MSB byte: u4_ofst */
    UWORD32 u4_wt_ofst_lx[2][MAX_REF_BUFS][3];
    void * pv_codec_handle; /* For Error Handling */

    /*  This is used when reordering is done in Forward or    */
    /*  backward lists. This is because reordering can point  */
    /*  to any valid entry in initial list irrespective of    */
    /*  num_ref_idx_active which could be overwritten using   */
    /*  ref_idx_reorder_flag                                  */
    UWORD8 u1_initial_list_size[2];
    UWORD32 u4_mbs_in_slice;
} dec_slice_params_t;


typedef struct
{
    UWORD8 u1_mb_type; /* Bit representations, X- reserved */
    /** |Field/Frame|X|X|X|X|Bslice u4_flag|PRED_NON_16x16 u4_flag |Intra Mbflag| */
    UWORD8 u1_mb_qp;
    UWORD8 u1_deblocking_mode; /** dblk: Mode [ NO / NO TOP / NO LEFT] filter */
    WORD8 i1_slice_alpha_c0_offset; /** dblk: alpha and C0 table u4_ofst {-12,12}*/
    WORD8 i1_slice_beta_offset; /** dblk: beta table u4_ofst {-12, 12}*/
    UWORD8 u1_single_call;
    UWORD8 u1_topmb_qp;
    UWORD8 u1_left_mb_qp;
    UWORD32 u4_bs_table[10]; /* Boundary strength */

} deblk_mb_t;

typedef struct
{
    UWORD8 u1_mb_type;
    UWORD8 u1_mb_qp;
} deblkmb_neighbour_t;

#define MAX_MV_RESIDUAL_INFO_PER_MB    32
#define MAX_REFIDX_INFO_PER_MB         4
#define PART_NOT_DIRECT                0
#define PART_DIRECT_8x8                1
#define PART_DIRECT_16x16              2
typedef struct
{
    UWORD8 u1_is_direct;
    UWORD8 u1_pred_mode;
    UWORD8 u1_sub_mb_num;
    UWORD8 u1_partheight;
    UWORD8 u1_partwidth;
} parse_part_params_t;

typedef struct
{
    UWORD8 u1_isI_mb;
    UWORD8 u1_num_part;
    UWORD32 *pu4_wt_offst[MAX_REFIDX_INFO_PER_MB];
    WORD8 i1_ref_idx[2][MAX_REFIDX_INFO_PER_MB];
    UWORD8 u1_col_info[MAX_REFIDX_INFO_PER_MB];
} parse_pmbarams_t;

typedef struct
{
    UWORD8 u1_vert_pad_top; /* flip-flop u4_flag remembering pad area (Vert) */
    UWORD8 u1_vert_pad_bot; /* flip-flop u4_flag remembering pad area (Vert) */
    UWORD8 u1_horz_pad; /* flip-flop u4_flag remembering pad area (Vert) */
    UWORD8 u1_pad_len_y_v; /* vertical pad amount for luma               */
    UWORD8 u1_pad_len_cr_v; /* vertical pad amount for chroma             */
} pad_mgr_t;


#define ACCEPT_ALL_PICS   (0x00)
#define REJECT_CUR_PIC    (0x01)
#define REJECT_PB_PICS    (0x02)

#define MASK_REJECT_CUR_PIC (0xFE)
#define MASK_REJECT_PB_PICS (0xFD)

#define PIC_TYPE_UNKNOWN  (0xFF)
#define PIC_TYPE_I        (0x00)
#define SYNC_FRM_DEFAULT  (0xFFFFFFFF)
#define INIT_FRAME        (0xFFFFFF)

typedef struct dec_err_status_t
{
    UWORD8 u1_cur_pic_type;
    UWORD8 u1_pic_aud_i;
    UWORD8 u1_err_flag;
    UWORD32 u4_frm_sei_sync;
    UWORD32 u4_cur_frm;
} dec_err_status_t;

/**************************************************************************/
/* Structure holds information about all high profile toolsets            */
/**************************************************************************/
typedef struct
{
    /*****************************************/
    /* variables required for scaling        */
    /*****************************************/
    UWORD8 u1_scaling_present;
    WORD16 *pi2_scale_mat[8];

    /*************************************************/
    /* scaling matrices for frame macroblocks after  */
    /* inverse scanning                              */
    /*************************************************/
    WORD16 i2_scalinglist4x4[6][16];
    WORD16 i2_scalinglist8x8[2][64];


    /*****************************************/
    /* variables required for transform8x8   */
    /*****************************************/
    UWORD8 u1_transform8x8_present;
    UWORD8 u1_direct_8x8_inference_flag;
    /* temporary variable to get noSubMbPartSizeLessThan8x8Flag from ih264d_parse_bmb_non_direct_cavlc */
    UWORD8 u1_no_submb_part_size_lt8x8_flag;

    /* needed for inverse scanning */
    cavlc_cntxt_t s_cavlc_ctxt;

    /* contexts for the CABAC related parsing */
    bin_ctxt_model_t *ps_transform8x8_flag;
    bin_ctxt_model_t *ps_sigcoeff_8x8_frame;
    bin_ctxt_model_t *ps_last_sigcoeff_8x8_frame;
    bin_ctxt_model_t *ps_coeff_abs_levelminus1;
    bin_ctxt_model_t *ps_sigcoeff_8x8_field;
    bin_ctxt_model_t *ps_last_sigcoeff_8x8_field;

/* variables required for intra8x8 */

/* variables required for handling different Qp for Cb and Cr */

} high_profile_tools_t;

typedef struct
{
    UWORD32 u4_num_bufs; /* Number of buffers in each display frame. 2 for 420SP and 3 for 420P and so on */
    void *buf[3]; /* Pointers to each of the components */
    UWORD32 u4_bufsize[3];
    UWORD32 u4_ofst[3];
} disp_buf_t;
typedef struct _dec_slice_struct
{
    volatile UWORD32 u4_first_mb_in_slice;
    volatile UWORD32 slice_type;
    volatile UWORD16 u2_log2Y_crwd;
    volatile void **ppv_map_ref_idx_to_poc;
    volatile void *pv_tu_coeff_data_start;
} dec_slice_struct_t;

/**
 * Structure to hold coefficient info for a 4x4 transform
 */
typedef struct
{
    /**
     * significant coefficient map
     */
    UWORD16 u2_sig_coeff_map;

    /**
     * holds coefficients
     */
    WORD16  ai2_level[16];
}tu_sblk4x4_coeff_data_t;

/**
 * Structure to hold coefficient info for a 8x8 transform
 */
typedef struct
{

    /**
     * significant coefficient map
     */
    UWORD32 au4_sig_coeff_map[2];

    /**
     * holds coefficients
     */
    WORD16  ai2_level[64];
}tu_blk8x8_coeff_data_t;

/** Aggregating structure that is globally available */
typedef struct _DecStruct
{

    /* Add below all other static memory allocations and pointers to items
     that are dynamically allocated once per session */
    dec_bit_stream_t *ps_bitstrm;
    dec_seq_params_t *ps_cur_sps;
    dec_pic_params_t *ps_cur_pps;
    dec_slice_params_t *ps_cur_slice;

    dec_pic_params_t *ps_pps;
    dec_seq_params_t *ps_sps;
    const UWORD16 *pu2_quant_scale_y;
    const UWORD16 *pu2_quant_scale_u;
    const UWORD16 *pu2_quant_scale_v;
    UWORD16 u2_mbx;
    UWORD16 u2_mby;

    UWORD16 u2_frm_wd_y; /** Width for luma buff */
    UWORD16 u2_frm_ht_y; /** Height for luma buff */
    UWORD16 u2_frm_wd_uv; /** Width for chroma buff */
    UWORD16 u2_frm_ht_uv; /** Height for chroma buff */
    UWORD16 u2_frm_wd_in_mbs; /** Frame width expressed in MB units */
    UWORD16 u2_frm_ht_in_mbs; /** Frame height expressed in MB units */
    WORD32 i4_submb_ofst; /** Offset in subMbs from the top left edge */
    /* Pointer to colocated Zero frame Image, will be used in B_DIRECT mode */
    /* colZeroFlag | // 0th bit
     field_flag  | // 1st bit
     XX          | // 2:3 bit don't cares
     subMbMode   | // 4:5 bit
     MbMode      | // 6:7 bit */

    UWORD8 *pu1_col_zero_flag;

    UWORD16 u2_pic_wd; /** Width of the picture being decoded */
    UWORD16 u2_pic_ht; /** Height of the picture being decoded */

    UWORD8 u1_first_slice_in_stream;
    UWORD8 u1_mb_ngbr_availablity;
    UWORD8 u1_ref_idxl0_active_minus1;
    UWORD8 u1_qp;
    UWORD8 u1_qp_y_div6;
    UWORD8 u1_qp_u_div6;
    UWORD8 u1_qp_y_rem6;
    UWORD8 u1_qp_u_rem6;

    /*********************************/
    /* configurable mb-group numbers */
    /* very critical to the decoder  */
    /*********************************/
    /************************************************************/
    /* MB_GROUP should be a multiple of 2                       */
    /************************************************************/
    UWORD8 u1_recon_mb_grp;
    UWORD8 u1_recon_mb_grp_pair;
    /* Variables to handle Cabac */
    decoding_envirnoment_t s_cab_dec_env; /* < Structure for decoding_envirnoment_t */
    /* These things need to be updated at each MbLevel */
    WORD8 i1_next_ctxt_idx; /* < next Ctxt Index */
    UWORD8 u1_currB_type;
    WORD8 i1_prev_mb_qp_delta; /* Prev MbQpDelta */
    UWORD8 u1_nal_unit_type;

    ctxt_inc_mb_info_t *p_ctxt_inc_mb_map; /* Pointer to ctxt_inc_mb_info_t map */
    ctxt_inc_mb_info_t *p_left_ctxt_mb_info; /* Pointer to left ctxt_inc_mb_info_t */
    ctxt_inc_mb_info_t *p_top_ctxt_mb_info; /* Pointer to top ctxt_inc_mb_info_t */
    ctxt_inc_mb_info_t *ps_curr_ctxt_mb_info; /* Pointer to current ctxt_inc_mb_info_t */
    ctxt_inc_mb_info_t *ps_def_ctxt_mb_info; /* Pointer to default ctxt_inc_mb_info_t */

    /* mv contexts for mv decoding using cabac */
    //UWORD8   u1_top_mv_ctxt_inc[4][4];
    /* Dimensions for u1_left_mv_ctxt_inc_arr is [2][4][4] for Mbaff case */
    UWORD8 u1_left_mv_ctxt_inc_arr[2][4][4];
    UWORD8 (*pu1_left_mv_ctxt_inc)[4];

    UWORD8 u1_sub_mb_num;
    UWORD8 u1_B; /** if B slice u1_B = 1 else 0 */
    WORD16 i2_only_backwarddma_info_idx;
    mv_pred_t *ps_mv; /** Pointer to the MV bank array */
    mv_pred_t *ps_mv_bank_cur; /** Pointer to the MV bank array */
    mv_pred_t s_default_mv_pred; /** Structure containing the default values
     for MV predictor */

    pred_info_t *ps_pred; /** Stores info to cfg MC */
    pred_info_t *ps_pred_start;

    UWORD32 u4_pred_info_idx;
    pred_info_pkd_t *ps_pred_pkd;
    pred_info_pkd_t *ps_pred_pkd_start;
    UWORD32 u4_pred_info_pkd_idx;
    UWORD8 *pu1_ref_buff; /** Destination buffer for DMAs */
    UWORD32 u4_dma_buf_idx;

    UWORD8 *pu1_y;
    UWORD8 *pu1_u;
    UWORD8 *pu1_v;

    WORD16 *pi2_y_coeff;
    UWORD8 *pu1_inv_scan;

    /**
     * Pointer frame level TU subblock coeff data
     */
    void *pv_pic_tu_coeff_data;

    /**
     * Pointer to TU subblock coeff data and number of subblocks and scan idx
     * Incremented each time a coded subblock is processed
     *
     */
    void *pv_parse_tu_coeff_data;
    void *pv_prev_mb_parse_tu_coeff_data;

    void *pv_proc_tu_coeff_data;

    WORD16 *pi2_coeff_data;

    cavlc_cntxt_t s_cavlc_ctxt;

    UWORD32 u4_n_leftY[2];
    UWORD32 u4_n_left_cr[2];
    UWORD32 u4_n_left_temp_y;

    UWORD8 pu1_left_nnz_y[4];
    UWORD8 pu1_left_nnz_uv[4];
    UWORD32 u4_n_left_temp_uv;
    /***************************************************************************/
    /*          Base pointer to all the cabac contexts                         */
    /***************************************************************************/
    bin_ctxt_model_t *p_cabac_ctxt_table_t;

    /***************************************************************************/
    /* cabac context pointers for every SE mapped into in p_cabac_ctxt_table_t */
    /***************************************************************************/
    bin_ctxt_model_t *p_mb_type_t;
    bin_ctxt_model_t *p_mb_skip_flag_t;
    bin_ctxt_model_t *p_sub_mb_type_t;
    bin_ctxt_model_t *p_mvd_x_t;
    bin_ctxt_model_t *p_mvd_y_t;
    bin_ctxt_model_t *p_ref_idx_t;
    bin_ctxt_model_t *p_mb_qp_delta_t;
    bin_ctxt_model_t *p_intra_chroma_pred_mode_t;
    bin_ctxt_model_t *p_prev_intra4x4_pred_mode_flag_t;
    bin_ctxt_model_t *p_rem_intra4x4_pred_mode_t;
    bin_ctxt_model_t *p_mb_field_dec_flag_t;
    bin_ctxt_model_t *p_cbp_luma_t;
    bin_ctxt_model_t *p_cbp_chroma_t;
    bin_ctxt_model_t *p_cbf_t[NUM_CTX_CAT];
    bin_ctxt_model_t *p_significant_coeff_flag_t[NUM_CTX_CAT];
    bin_ctxt_model_t *p_coeff_abs_level_minus1_t[NUM_CTX_CAT];

    UWORD32 u4_num_pmbair; /** MB pair number */
    mv_pred_t *ps_mv_left; /** Pointer to left motion vector bank */
    mv_pred_t *ps_mv_top_left; /** Pointer to top left motion vector bank */
    mv_pred_t *ps_mv_top_right; /** Pointer to top right motion vector bank */

    UWORD8 *pu1_left_yuv_dc_csbp;


    deblkmb_neighbour_t deblk_left_mb[2];
    deblkmb_neighbour_t *ps_deblk_top_mb;
    neighbouradd_t (*ps_left_mvpred_addr)[2]; /* Left MvPred Address Ping Pong*/

    /***************************************************************************/
    /*       Ref_idx contexts  are stored in the following way                 */
    /*  Array Idx 0,1 for reference indices in Forward direction               */
    /*  Array Idx 2,3 for reference indices in backward direction              */
    /***************************************************************************/

    /* Dimensions for u1_left_ref_ctxt_inc_arr is [2][4] for Mbaff:Top and Bot */
    WORD8 i1_left_ref_idx_ctx_inc_arr[2][4];
    WORD8 *pi1_left_ref_idx_ctxt_inc;

    /*************************************************************************/
    /*               Arrangnment of DC CSBP                                  */
    /*        bits:  b7  b6  b5  b4  b3  b2  b1  b0                          */
    /*        CSBP:   x   x   x   x   x  Vdc Udc Ydc                         */
    /*************************************************************************/
    /*************************************************************************/
    /*  Points either to u1_yuv_dc_csbp_topmb or  u1_yuv_dc_csbp_bot_mb     */
    /*************************************************************************/
    UWORD8 u1_yuv_dc_csbp_topmb;
    UWORD8 u1_yuv_dc_csbp_bot_mb;

    /* DMA SETUP */
    tfr_ctxt_t s_tran_addrecon_parse;
    tfr_ctxt_t s_tran_addrecon;
    tfr_ctxt_t s_tran_iprecon;
    tfr_ctxt_t *ps_frame_buf_ip_recon;
    WORD8 i1_recon_in_thread3_flag;

    /* slice Header Simplification */
    UWORD8 u1_pr_sl_type;
    WORD32 i4_frametype;
    UWORD32 u4_app_disp_width;
    WORD32 i4_error_code;
    UWORD32 u4_bitoffset;

    /* Variables added to handle field pics */

    UWORD8 u1_second_field;
    WORD32 i4_pic_type;
    WORD32 i4_content_type;
    WORD32 i4_decode_header;
    WORD32 i4_header_decoded;
    UWORD32 u4_total_frames_decoded;

    ctxt_inc_mb_info_t *ps_left_mb_ctxt_info; /* structure containing the left MB's
     context info, incase of Mbaff */
    pocstruct_t s_prev_pic_poc;
    pocstruct_t s_cur_pic_poc;
    WORD32 i4_cur_display_seq;
    WORD32 i4_prev_max_display_seq;
    WORD32 i4_max_poc;
    deblk_mb_t *ps_cur_deblk_mb;

    /* Pointers to local scratch buffers */
    deblk_mb_t *ps_deblk_pic;

    /* Pointers to Picture Buffers (Given by BufAPI Lib) */
    struct pic_buffer_t *ps_cur_pic; /** Pointer to Current picture buffer */

    /* Scratch Picture Buffers (Given by BufAPI Lib) */
    struct pic_buffer_t s_cur_pic;

    /* Current Slice related information */
    volatile UWORD16 u2_cur_slice_num;
    volatile UWORD16 u2_cur_slice_num_dec_thread;

    /* Variables needed for Buffer API handling */
    UWORD8 u1_nal_buf_id;
    UWORD8 u1_pic_buf_id;
    UWORD8 u1_pic_bufs;

    WORD16 *pi2_pred1; //[441];  /** Temp predictor buffer for MC */
    /* Pointer to refernce Pic buffers list, 0:fwd, 1:bwd */
    pic_buffer_t **ps_ref_pic_buf_lx[2];
    /* refIdx to POC mapping */
    void **ppv_map_ref_idx_to_poc;
    void **ppv_map_ref_idx_to_poc_base;
    UWORD32 *pu4_wts_ofsts_mat;
    UWORD32 *pu4_wt_ofsts;
    UWORD32 *pu4_mbaff_wt_mat;
    /* Function pointers to read Params common to CAVLC and CABAC */
    WORD32 (*pf_parse_inter_mb)(struct _DecStruct * ps_dec,
                                dec_mb_info_t * ps_cur_mb_info,
                                UWORD8 u1_mb_num,
                                UWORD8 u1_num_mbsNby2);
    WORD32 (*pf_mvpred_ref_tfr_nby2mb)(struct _DecStruct * ps_dec,
                                     UWORD8 u1_num_mbs,
                                     UWORD8 u1_num_mbsNby2);

    WORD32 (*pf_parse_inter_slice)(struct _DecStruct * ps_dec,
                                   dec_slice_params_t * ps_slice,
                                   UWORD16 u2_first_mb_in_slice);

    UWORD32 (*pf_get_mb_info)(struct _DecStruct * ps_dec,
                              const UWORD16 u2_cur_mb_address,
                              dec_mb_info_t * ps_cur_mb_info,
                              UWORD32 u4_mbskip_run);

    /* Variables for Decode Buffer Management */
    dpb_manager_t *ps_dpb_mgr;
    dpb_commands_t *ps_dpb_cmds;
    dpb_commands_t s_dpb_cmds_scratch;

    /* Variables Required for N MB design */
    dec_mb_info_t *ps_nmb_info;

    UWORD8 *pu1_y_intra_pred_line;
    UWORD8 *pu1_u_intra_pred_line;
    UWORD8 *pu1_v_intra_pred_line;

    UWORD8 *pu1_cur_y_intra_pred_line;
    UWORD8 *pu1_cur_u_intra_pred_line;
    UWORD8 *pu1_cur_v_intra_pred_line;

    UWORD8 *pu1_cur_y_intra_pred_line_base;
    UWORD8 *pu1_cur_u_intra_pred_line_base;
    UWORD8 *pu1_cur_v_intra_pred_line_base;

    UWORD8 *pu1_prev_y_intra_pred_line;
    UWORD8 *pu1_prev_u_intra_pred_line;
    UWORD8 *pu1_prev_v_intra_pred_line;

    UWORD32 u4_intra_pred_line_ofst;

    UWORD8 u1_res_changed;

    mv_pred_t *ps_mv_cur; /** pointer to current motion vector bank */
    mv_pred_t *ps_mv_top; /** pointer to top motion vector bank */
    mv_pred_t *ps_mv_top_right2;/** Pointer to top right motion vector bank */
    mv_pred_t *ps_mv_p[2]; /** Scratch ping motion vector bank */
    mv_pred_t *ps_mv_top_p[MV_SCRATCH_BUFS]; /** Scratch top pong motion vector bank */
    UWORD8 u1_mv_top_p;

    deblk_mb_t *ps_deblk_mbn;

    UWORD8 *pu1_temp_mc_buffer;

    struct _sei *ps_sei;
    struct _sei *ps_sei_parse;
    struct _sei s_sei_export;

    void *pv_disp_sei_params;

    UWORD8 u1_pic_struct_copy;
    /* Variables required for cropping */
    UWORD16 u2_disp_width;
    UWORD16 u2_disp_height;
    UWORD16 u2_crop_offset_y;
    UWORD16 u2_crop_offset_uv;

    /* Crop info from SPS */
	UWORD8 u1_frame_cropping_flag;
	UWORD8 u1_frame_cropping_rect_left_ofst;
	UWORD8 u1_frame_cropping_rect_right_ofst;
	UWORD8 u1_frame_cropping_rect_top_ofst;
	UWORD8 u1_frame_cropping_rect_bottom_ofst;

    /* Variable required to get presentation time stamp through application */
    UWORD32 u4_pts;

    /* Variables used for gaps in frame number */
    UWORD16 u2_prev_ref_frame_num;

    UWORD8 u1_mb_idx;
    struct pic_buffer_t *ps_col_pic;
    void (*pf_parse_mvdirect)(struct _DecStruct*,
                           struct pic_buffer_t*,
                           directmv_t*,
                           UWORD8,
                           WORD32,
                           dec_mb_info_t *);
    void *pv_dec_out;
    void *pv_dec_in;
    void *pv_scratch_sps_pps; /*used temeporarily store sps/ spps while parsing*/

    /* state pointers to mb and partition information */
    parse_pmbarams_t *ps_parse_mb_data;
    parse_part_params_t *ps_parse_part_params;

    /* scratch pointers to mb and partition information */
    parse_part_params_t *ps_part;

    UWORD8 u1_max_dec_frame_buffering;
    pad_mgr_t s_pad_mgr;
    UWORD8 (*pf_mvpred)(struct _DecStruct *ps_dec,
                        struct _DecMbInfo *ps_cur_mb_info,
                        mv_pred_t *ps_mv_pred,
                        mv_pred_t *ps_mv_nmb,
                        mv_pred_t *ps_mv_ntop,
                        UWORD8 u1_sub_mb_num,
                        UWORD8 uc_mb_part_width,
                        UWORD8 uc_lxstart,
                        UWORD8 uc_lxend,
                        UWORD8 u1_mb_mc_mode);
    void (*pf_compute_bs)(struct _DecStruct * ps_dec,
                         struct _DecMbInfo * ps_cur_mb_info,
                         const UWORD16 u2_mbxn_mb);
    UWORD8 u1_init_dec_flag;
    WORD32 i4_reorder_depth;
    prev_seq_params_t s_prev_seq_params;
    UWORD8 u1_cur_mb_fld_dec_flag; /* current Mb fld or Frm */

    UWORD8 u1_topleft_mb_fld;
    UWORD8 u1_topleft_mbtype;
    UWORD8 u1_topleft_mb_fld_bot;
    UWORD8 u1_topleft_mbtype_bot;
    WORD16 i2_prev_slice_mbx;
    WORD16 i2_prev_slice_mby;
    UWORD16 u2_top_left_mask;
    UWORD16 u2_top_right_mask;
    dec_err_status_t * ps_dec_err_status;
    /* Ensure pi1_left_pred_mode is aligned to 4 byte boundary,
    by declaring this after a pointer or an integer */
    WORD8 pi1_left_pred_mode[8];

    UWORD8 u1_mb_idx_mv;
    UWORD16 u2_mv_2mb[2];
    UWORD32 u4_skip_frm_mask;

    /* variable for finding the no.of mbs decoded in the current picture */
    UWORD16 u2_total_mbs_coded;
    /* member added for supporting fragmented annex - B */
//  frg_annex_read_t s_frag_annex_read;
    /* added for vui_t, sei support*/
    WORD32 i4_vui_frame_rate;
    /* To Store the value of ref_idx_active for previous slice */
    /* useful in error handling                                */
    UWORD8 u1_num_ref_idx_lx_active_prev;
    /* Flag added to come out of process call in annex-b if&if frame is decoded */
    /* presence of access unit delimters and pps and sps                        */
    UWORD8 u1_frame_decoded_flag;

    /* To keep track of whether the last picture was decoded or not */
    /* in case of skip mode set by the application                  */
    UWORD8 u1_last_pic_not_decoded;

    WORD32 e_dec_status;
    UWORD32 u4_num_fld_in_frm;

    /* Function pointer for 4x4 residual cavlc parsing based on total coeff */
    WORD32 (*pf_cavlc_4x4res_block[3])(UWORD32 u4_isdc,
                                    UWORD32 u4_total_coeff_trail_one, /**TotalCoefficients<<16+trailingones*/
                                    dec_bit_stream_t *ps_bitstrm);

    /* Function pointer array for interpolate functions in called from motion compensattion module */
    void (*p_mc_interpolate_x_y[16][3])(UWORD8*,
                                        UWORD8*,
                                        UWORD8*,
                                        UWORD8,
                                        UWORD16,
                                        UWORD16,
                                        UWORD8);

    /**************************************************************************/
    /* Function pointer for 4x4 totalcoeff, trlone and residual cavlc parsing */
    /* based on u4_n (neigbourinng nnz average)                               */
    /* These point to two functions depending on (u4_n > 7) and (u4_n <= 7)   */
    /**************************************************************************/
    WORD32 (*pf_cavlc_parse4x4coeff[2])(WORD16 *pi2_coeff_block,
                                        UWORD32 u4_isdc, /* is it a DC block */
                                        WORD32 u4_n,
                                        struct _DecStruct *ps_dec, /** Decoder Parameters */
                                        UWORD32 *pu4_total_coeff);

    /**************************************************************************/
    /* Function pointer for luma 8x8block cavlc parsing based on top and left */
    /* neigbour availability.                                                 */
    /**************************************************************************/
    WORD32 (*pf_cavlc_parse_8x8block[4])(WORD16 *pi2_coeff_block,
                                         UWORD32 u4_sub_block_strd,
                                         UWORD32 u4_isdc,
                                         struct _DecStruct *ps_dec,
                                         UWORD8 *pu1_top_nnz,
                                         UWORD8 *pu1_left_nnz,
                                         UWORD8 u1_tran_form8x8,
                                         UWORD8 u1_mb_field_decodingflag,
                                         UWORD32 *pu4_csbp);

    /**************************************************************************/
    /* Ping pong top and current rows of mb neigbour_params                   */
    /**************************************************************************/
    mb_neigbour_params_t *ps_nbr_mb_row;
    mb_neigbour_params_t *ps_cur_mb_row;
    mb_neigbour_params_t *ps_top_mb_row;

    /**************************************************************************/
    /* Function pointer for 16x16 and non16x16 Bs1 calculations depending on   */
    /* P and B slice.                                                          */
    /***************************************************************************/
    void (*pf_fill_bs1[2][2])(mv_pred_t *ps_cur_mv_pred,
                              mv_pred_t *ps_top_mv_pred,
                              void **ppv_map_ref_idx_to_poc,
                              UWORD32 *pu4_bs_table, /* pointer to the BsTable array */
                              mv_pred_t *ps_leftmost_mv_pred,
                              neighbouradd_t *ps_left_addr,
                              void **u4_pic_addrress,
                              WORD32 i4_ver_mvlimit);

    void (*pf_fill_bs_xtra_left_edge[2])(UWORD32 *pu4_bs, /* Base pointer of BS table */
                                         WORD32 u4_left_mb_t_csbp, /* left mbpair's top csbp   */
                                         WORD32 u4_left_mb_b_csbp, /* left mbpair's bottom csbp*/
                                         WORD32 u4_cur_mb_csbp, /* csbp of current mb */
                                         UWORD32 u4_cur_mb_bot /* is top or bottom mb */

                                         );
    /* Function pointer array for BP and MP functions for MC*/
    void (*p_motion_compensate)(struct _DecStruct * ps_dec,
                               dec_mb_info_t *ps_cur_mb_info);


    void (*p_mc_dec_thread)(struct _DecStruct * ps_dec, dec_mb_info_t *ps_cur_mb_info);

    /* Function pointer array for BP and MP functions for formMbPartInfo*/

    WORD32 (*p_form_mb_part_info)(pred_info_pkd_t *ps_pred_pkd,
                                struct _DecStruct * ps_dec,
                                     UWORD16 u2_mb_x,
                                     UWORD16 u2_mb_y,
                                     WORD32 mb_index,
                                     dec_mb_info_t *ps_cur_mb_info);

    WORD32 (*p_form_mb_part_info_thread)(pred_info_pkd_t *ps_pred_pkd,
                                struct _DecStruct * ps_dec,
                                     UWORD16 u2_mb_x,
                                     UWORD16 u2_mb_y,
                                     WORD32 mb_index,
                                     dec_mb_info_t *ps_cur_mb_info);


    /* Required for cabac mbaff bottom mb */
    UWORD32 u4_next_mb_skip;

    void (*p_DeblockPicture[2])(struct _DecStruct *);

    /* ! */
    UWORD32 u4_ts;
    UWORD8 u1_flushfrm;

    /* Output format sent by the application */
    UWORD8 u1_chroma_format;
    UWORD8 u1_pic_decode_done;
    UWORD8 u1_slice_header_done;
    WORD32 init_done;

    /******************************************/
    /* For the high profile related variables */
    /******************************************/
    high_profile_tools_t s_high_profile;
    /* CBCR */
    UWORD8 u1_qp_v_div6;
    UWORD8 u1_qp_v_rem6;
    /*
     * TO help solve the dangling field case.
     * Check for the previous frame number and the current frame number.
     */
    UWORD16 u2_prv_frame_num;
    UWORD8 u1_top_bottom_decoded;
    UWORD8 u1_dangling_field;

    IVD_DISPLAY_FRAME_OUT_MODE_T                e_frm_out_mode;

    UWORD8 *pu1_bits_buf_static;
    UWORD8 *pu1_bits_buf_dynamic;

    UWORD32 u4_static_bits_buf_size;
    UWORD32 u4_dynamic_bits_buf_size;

    UWORD32 u4_num_disp_bufs_requested;
    WORD32 i4_display_delay;
    UWORD32 u4_slice_start_code_found;

    UWORD32 u4_nmb_deblk;
    UWORD32 u4_use_intrapred_line_copy;
    UWORD32 u4_num_mbs_prev_nmb;
    UWORD32 u4_num_mbs_cur_nmb;
    UWORD32 u4_app_deblk_disable_level;
    UWORD32 u4_app_disable_deblk_frm;
    WORD32 i4_mv_frac_mask;

    disp_buf_t disp_bufs[MAX_DISP_BUFS_NEW];
    UWORD32 u4_disp_buf_mapping[MAX_DISP_BUFS_NEW];
    UWORD32 u4_disp_buf_to_be_freed[MAX_DISP_BUFS_NEW];
    UWORD32 u4_share_disp_buf;
    UWORD32 u4_num_disp_bufs;

    UWORD32 u4_bs_deblk_thread_created;
    volatile UWORD32 u4_start_recon_deblk;
    void *pv_bs_deblk_thread_handle;

    UWORD32 u4_cur_bs_mb_num;
    UWORD32 u4_bs_cur_slice_num_mbs;
    UWORD32 u4_cur_deblk_mb_num;
    UWORD32 u4_sps_cnt_in_process;
    volatile UWORD16 u2_cur_slice_num_bs;

    UWORD32 u4_deblk_mb_x;
    UWORD32 u4_deblk_mb_y;



    iv_yuv_buf_t s_disp_frame_info;
    UWORD32 u4_fmt_conv_num_rows;
    UWORD32 u4_fmt_conv_cur_row;
    ivd_out_bufdesc_t *ps_out_buffer;
    ivd_get_display_frame_op_t s_disp_op;
    UWORD32 u4_output_present;

    volatile UWORD16 cur_dec_mb_num;
    volatile UWORD16 cur_recon_mb_num;
    volatile UWORD16 u2_cur_mb_addr;
    WORD16 i2_dec_thread_mb_y;
    WORD16 i2_recon_thread_mb_y;

    UWORD8 u1_separate_parse;
    UWORD32 u4_dec_thread_created;
    void *pv_dec_thread_handle;
    volatile UWORD8 *pu1_dec_mb_map;
    volatile UWORD8 *pu1_recon_mb_map;
    volatile UWORD16 *pu2_slice_num_map;
    dec_slice_struct_t *ps_dec_slice_buf;
    void *pv_map_ref_idx_to_poc_buf;
    dec_mb_info_t *ps_frm_mb_info;
    volatile dec_slice_struct_t * volatile ps_parse_cur_slice;
    volatile dec_slice_struct_t * volatile ps_decode_cur_slice;
    volatile dec_slice_struct_t * volatile ps_computebs_cur_slice;
    UWORD32 u4_cur_slice_decode_done;
    UWORD32 u4_extra_mem_used;

    /* 2 first slice not parsed , 1 :first slice parsed , 0 :first valid slice header parsed*/
    UWORD32 u4_first_slice_in_pic;
    UWORD32 u4_num_cores;
    IVD_ARCH_T e_processor_arch;
    IVD_SOC_T e_processor_soc;

    /**
     * Pictures that are are degraded
     * 0 : No degrade
     * 1 : Only on non-reference frames
     * 2 : Use interval specified by u4_nondegrade_interval
     * 3 : All non-key frames
     * 4 : All frames
     */
    WORD32 i4_degrade_pics;

    /**
     * Interval for pictures which are completely decoded without any degradation
     */
    WORD32 i4_nondegrade_interval;

    /**
     * bit position (lsb is zero): Type of degradation
     * 1 : Disable deblocking
     * 2 : Faster inter prediction filters
     * 3 : Fastest inter prediction filters
     */
    WORD32 i4_degrade_type;

    /** Degrade pic count, Used to maintain the interval between non-degraded pics
     *
     */
    WORD32 i4_degrade_pic_cnt;
    WORD32 i4_display_index;
    UWORD32 u4_pic_buf_got;

    /**
     * Col flag and mv pred buffer manager
     */
    void *pv_mv_buf_mgr;

    /**
     * Picture buffer manager
     */
    void *pv_pic_buf_mgr;

    /**
     * Display buffer manager
     */
    void *pv_disp_buf_mgr;

    void *apv_buf_id_pic_buf_map[MAX_DISP_BUFS_NEW];

    UWORD8 au1_pic_buf_id_mv_buf_id_map[MAX_DISP_BUFS_NEW];

    UWORD8 au1_pic_buf_ref_flag[MAX_DISP_BUFS_NEW];

    struct pic_buffer_t *ps_pic_buf_base;

    UWORD8 *pu1_ref_buff_base;
    col_mv_buf_t *ps_col_mv_base;
    void *(*pf_aligned_alloc)(void *pv_mem_ctxt, WORD32 alignment, WORD32 size);
    void (*pf_aligned_free)(void *pv_mem_ctxt, void *pv_buf);
    void *pv_mem_ctxt;

    UWORD8 *pu1_pic_buf_base;
    UWORD8 *pu1_mv_bank_buf_base;
    UWORD8 *pu1_init_dpb_base;

    ih264_default_weighted_pred_ft *pf_default_weighted_pred_luma;

    ih264_default_weighted_pred_ft *pf_default_weighted_pred_chroma;

    ih264_weighted_pred_ft *pf_weighted_pred_luma;

    ih264_weighted_pred_ft *pf_weighted_pred_chroma;

    ih264_weighted_bi_pred_ft *pf_weighted_bi_pred_luma;

    ih264_weighted_bi_pred_ft *pf_weighted_bi_pred_chroma;

    ih264_pad *pf_pad_top;
    ih264_pad *pf_pad_bottom;
    ih264_pad *pf_pad_left_luma;
    ih264_pad *pf_pad_left_chroma;
    ih264_pad *pf_pad_right_luma;
    ih264_pad *pf_pad_right_chroma;

    ih264_inter_pred_chroma_ft *pf_inter_pred_chroma;

    ih264_inter_pred_luma_ft *apf_inter_pred_luma[16];

    ih264_intra_pred_luma_ft *apf_intra_pred_luma_16x16[4];

    ih264_intra_pred_luma_ft *apf_intra_pred_luma_8x8[9];

    ih264_intra_pred_luma_ft *apf_intra_pred_luma_4x4[9];

    ih264_intra_pred_ref_filtering_ft *pf_intra_pred_ref_filtering;

    ih264_intra_pred_chroma_ft *apf_intra_pred_chroma[4];

    ih264_iquant_itrans_recon_ft *pf_iquant_itrans_recon_luma_4x4;

    ih264_iquant_itrans_recon_ft *pf_iquant_itrans_recon_luma_4x4_dc;

    ih264_iquant_itrans_recon_ft *pf_iquant_itrans_recon_luma_8x8;

    ih264_iquant_itrans_recon_ft *pf_iquant_itrans_recon_luma_8x8_dc;

    ih264_iquant_itrans_recon_chroma_ft *pf_iquant_itrans_recon_chroma_4x4;

    ih264_iquant_itrans_recon_chroma_ft *pf_iquant_itrans_recon_chroma_4x4_dc;

    ih264_ihadamard_scaling_ft *pf_ihadamard_scaling_4x4;

    /**
     * deblock vertical luma edge with blocking strength 4
     */
    ih264_deblk_edge_bs4_ft *pf_deblk_luma_vert_bs4;

    /**
     * deblock vertical luma edge with blocking strength less than 4
     */
    ih264_deblk_edge_bslt4_ft *pf_deblk_luma_vert_bslt4;

    /**
     * deblock vertical luma edge with blocking strength 4 for mbaff
     */
    ih264_deblk_edge_bs4_ft *pf_deblk_luma_vert_bs4_mbaff;

    /**
     * deblock vertical luma edge with blocking strength less than 4 for mbaff
     */
    ih264_deblk_edge_bslt4_ft *pf_deblk_luma_vert_bslt4_mbaff;

    /**
     * deblock vertical chroma edge with blocking strength 4
     */
    ih264_deblk_chroma_edge_bs4_ft *pf_deblk_chroma_vert_bs4;

    /**
     * deblock vertical chroma edge with blocking strength less than 4
     */
    ih264_deblk_chroma_edge_bslt4_ft *pf_deblk_chroma_vert_bslt4;

    /**
     * deblock vertical chroma edge with blocking strength 4 for mbaff
     */
    ih264_deblk_chroma_edge_bs4_ft *pf_deblk_chroma_vert_bs4_mbaff;

    /**
     * deblock vertical chroma edge with blocking strength less than 4 for mbaff
     */
    ih264_deblk_chroma_edge_bslt4_ft *pf_deblk_chroma_vert_bslt4_mbaff;

    /**
     * deblock horizontal luma edge with blocking strength 4
     */
    ih264_deblk_edge_bs4_ft *pf_deblk_luma_horz_bs4;

    /**
     * deblock horizontal luma edge with blocking strength less than 4
     */
    ih264_deblk_edge_bslt4_ft *pf_deblk_luma_horz_bslt4;

    /**
     * deblock horizontal chroma edge with blocking strength 4
     */
    ih264_deblk_chroma_edge_bs4_ft *pf_deblk_chroma_horz_bs4;

    /**
     * deblock horizontal chroma edge with blocking strength less than 4
     */
    ih264_deblk_chroma_edge_bslt4_ft *pf_deblk_chroma_horz_bslt4;


} dec_struct_t;

#endif /* _H264_DEC_STRUCTS_H */
