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
 *  ih264_structs.h
 *
 * @brief
 *  Structure definitions used in the code
 *
 * @author
 *  Ittiam
 *
 * @par List of Functions:
 *
 * @remarks
 *  None
 *
 *******************************************************************************
 */

#ifndef _IH264_STRUCTS_H_
#define _IH264_STRUCTS_H_

/** MB Type info for Intra MBs */
typedef struct
{
    UWORD32             u4_num_mbpart;
    MBPART_PREDMODE_T   e_mbpart_predmode;
    MBMODES_I16x16      e_intra_predmode;
    UWORD32             u4_cpb_chroma;
    UWORD32             u4_cpb_luma;
}intra_mbtype_info_t;

/** MB Type info for Inter MBs */
typedef struct
{
    UWORD32                 u4_num_mbpart;
    MBPART_PREDMODE_T       e_mbpart_predmode_0;
    MBPART_PREDMODE_T       e_mbpart_predmode_1;
    UWORD32                 u4_mbpart_wd;
    UWORD32                 u4_mbpart_ht;
}inter_mbtype_info_t;


/** Sub MB Type info for Inter MBs */
typedef struct
{
    UWORD32                 u4_num_mbpart;
    MBPART_PREDMODE_T       e_mbpart_predmode;
    UWORD32                 u4_mbpart_wd;
    UWORD32                 u4_mbpart_ht;
}submbtype_info_t;

/**
 * Picture buffer
 */
typedef struct
{
    UWORD8* pu1_luma;
    UWORD8* pu1_chroma;

    WORD32 i4_abs_poc;
    WORD32 i4_poc_lsb;


    /** Lower 32 bit of time stamp */
    UWORD32 u4_timestamp_low;

    /** Upper 32 bit of time stamp */
    UWORD32 u4_timestamp_high;

    WORD32 i4_used_as_ref;

    /**
     * frame_num in the slice header
     */
    WORD32 i4_frame_num;

    /**
     * Long-term frame idx
     * TODO: store in frame_num
     */
    WORD32 i4_long_term_frame_idx;

    /*
     *  0: Top Field
     *  1: Bottom Field
     */
    WORD8   i1_field_type;

    /**
     * buffer ID from frame buffer manager
     */
    WORD32 i4_buf_id;

} pic_buf_t;


/**
 * Reference List
 */
typedef struct
{
    void *pv_pic_buf;

    void *pv_mv_buf;

} ref_list_t;


/**
 * Motion vector
 */
typedef struct
{
    /**
     * Horizontal Motion Vector
     */
    WORD16 i2_mvx;

    /**
     * Vertical Motion Vector
     */
    WORD16 i2_mvy;
} mv_t;

/*****************************************************************************/
/* Following results in packed 48 bit structure. If mv_t included            */
/*  ref_pic_buf_id, then 8 bits will be wasted for each mv for aligning.     */
/*  Also using mv_t as elements directly instead of a pointer to l0 and l1   */
/*  mvs. Since pointer takes 4 bytes and MV itself is 4 bytes. It does not   */
/*  really help using pointers.                                              */
/*****************************************************************************/

/**
 * PU Motion Vector info
 */
typedef struct
{
    /**
     *  L0 Motion Vector
     */
    mv_t s_l0_mv;

    /**
     *  L1 Motion Vector
     */
    mv_t s_l1_mv;

    /**
     *  L0 Ref index
     */
    WORD8   i1_l0_ref_idx;

    /**
     *  L1 Ref index
     */
    WORD8   i1_l1_ref_idx;

    /**
     *  L0 Ref Pic Buf ID
     */
    WORD8 i1_l0_ref_pic_buf_id;

    /**
     *  L1 Ref Pic Buf ID
     */
    WORD8 i1_l1_ref_pic_buf_id;

} pu_mv_t;

/**
 * PU information
 */
typedef struct
{

    /**
     *  Motion Vectors
     */
    pu_mv_t     s_mv;

    /**
     *  PU X position in terms of min PU (4x4) units
     */
    UWORD32     b2_pos_x        : 2;

    /**
     *  PU Y position in terms of min PU (4x4) units
     */
    UWORD32     b2_pos_y        : 2;

    /**
     *  PU width in pixels = (b2_wd + 1) << 2
     */
    UWORD32     b2_wd           : 2;

    /**
     *  PU height in pixels = (b2_ht + 1) << 2
     */
    UWORD32     b2_ht           : 2;

    /**
     *  Intra or Inter flag for each partition - 0 or 1
     */
    UWORD32     b1_intra_flag   : 1;

    /**
     *  PRED_L0, PRED_L1, PRED_BI
     */
    UWORD32     b2_pred_mode    : 2;

} pu_t;


/**
 * MB information to be stored for entire frame
 */
typedef struct
{
    /**
     * Transform sizes 0: 4x4, 1: 8x8,
     */
    UWORD32     b1_trans_size : 1;

    /**
     * CBP - 4 bits for Y, 1 for U and 1 for V
     */
    UWORD32     b6_cbp: 6;

    /**
     * Intra pred sizes  0: 4x4, 1: 8x8, 2: 16x16
     */
    UWORD32     b2_intra_pred_size : 2;

    /**
     * Flag to signal if the current MB is IPCM
     */
    UWORD32     b1_ipcm : 1;

}mb_t;

/*****************************************************************************/
/* Info from last TU row of MB is stored in a row level neighbour buffer    */
/* , which will be used for Boundary Strength computation                    */
/*****************************************************************************/
/**
 *  MB neighbor info
 */
typedef struct
{
    /**
     *  Slice index of the mb
     */
    UWORD16 u2_slice_idx;

    /*************************************************************************/
    /* CBF of bottom TU row (replicated in 4 pixel boundary)                 */
    /* MSB contains CBF of first TU in the last row and LSB contains CBF     */
    /* of last TU in the last row                                            */
    /*************************************************************************/
    /**
     * CBF of bottom TU row
     */
    UWORD16 u2_packed_cbf;

    /*************************************************************************/
    /* QP of bottom TU row (replicated at 8 pixel boundary (Since QP can     */
    /* not change at less than min CU granularity)                           */
    /*************************************************************************/
    /**
     * QP of bottom TU row
     */
    UWORD8 u1_qp;

} mb_top_ny_info_t;

/**
 *  MB level context
 */
typedef struct _mb_ctxt_t
{
    /*************************************************************************/
    /* Tile boundary can be detected by looking at tile start x and tile     */
    /* start y.  And based on the tile, slice and frame boundary the         */
    /* following will be initialized.                                        */
    /*************************************************************************/
    /**
     *  Pointer to left MB
     */
    /*  If not available, this will be set to NULL   */
    struct _mb_ctxt_t *ps_mb_left;

    /**
     *  Pointer to top-left MB
     */
    /* If not available, this will be set to NULL   */
    mb_top_ny_info_t *ps_mb_ny_topleft;

    /**
     *  Pointer to top MB
     */
    /* If not available, this will be set to NULL  */
    mb_top_ny_info_t *ps_mb_ny_top;

    /**
     *  Pointer to top-right MB
     */
    /* If not available, this will be set to NULL */
    mb_top_ny_info_t *ps_mb_ny_topright;

    /*************************************************************************/
    /* Pointer to PU data.                                                   */
    /* This points to a MV Bank stored at frame level. Though this           */
    /* pointer can be derived by reading offset at frame level, it is        */
    /* stored here for faster access. Can be removed if storage of MB       */
    /* structure is critical                                                 */
    /*************************************************************************/
    /**
     * Pointer to PU data
     */
    pu_t *ps_pu;

    /*************************************************************************/
    /* Pointer to a PU map stored at frame level,                            */
    /* Though this pointer can be derived by multiplying MB address with    */
    /* number of minTUs in a MB, it is stored here for faster access.       */
    /* Can be removed if storage of MB structure is critical                */
    /*************************************************************************/
    /**
     * Pointer to a PU map stored at frame level
     */
    UWORD8 *pu1_pu_map;

    /**
     *  Number of TUs filled in as_tu
     */
    /*************************************************************************/
    /* Having the first entry as 32 bit data, helps in keeping each of       */
    /* the structures aligned to 32 bits at MB level                        */
    /*************************************************************************/
    WORD32 i4_tu_cnt;

    /**
     *  Pointer to transform coeff data
     */
    /*************************************************************************/
    /* Following format is repeated for every coded TU                       */
    /* Luma Block                                                            */
    /* num_coeffs      : 16 bits                                             */
    /* zero_cols       : 8 bits ( 1 bit per 4 columns)                       */
    /* sig_coeff_map   : ((TU Size * TU Size) + 31) >> 5 number of WORD32s   */
    /* coeff_data      : Non zero coefficients                               */
    /* Cb Block (only for last TU in 4x4 case else for every luma TU)        */
    /* num_coeffs      : 16 bits                                             */
    /* zero_cols       : 8 bits ( 1 bit per 4 columns)                       */
    /* sig_coeff_map   : ((TU Size * TU Size) + 31) >> 5 number of WORD32s   */
    /* coeff_data      : Non zero coefficients                               */
    /* Cr Block (only for last TU in 4x4 case else for every luma TU)        */
    /* num_coeffs      : 16 bits                                             */
    /* zero_cols       : 8 bits ( 1 bit per 4 columns)                       */
    /* sig_coeff_map   : ((TU Size * TU Size) + 31) >> 5 number of WORD32s   */
    /* coeff_data      : Non zero coefficients                               */
    /*************************************************************************/
    void            *pv_coeff_data;

    /**
     *  Slice to which the MB belongs to
     */
    WORD32 i4_slice_idx;

    /**
     *  MB column position
     */
    WORD32 i4_pos_x;

    /**
     *  MB row position
     */
    WORD32 i4_pos_y;

    /**
     *  Number of PUs filled in ps_pu
     */
    WORD32 i4_pu_cnt;

    /**
     *  Index of current PU being processed in ps_pu
     */
    /*  Scratch variable set to 0 at the start of any PU processing function */
    WORD32 i4_pu_idx;

    /**
     * Vertical Boundary strength
     */
    /* Two bits per edge.
    Stored in format. BS[15] | BS[14] | .. |BS[0]*/
    UWORD32 *pu4_vert_bs;

    /**
     * Horizontal Boundary strength
     */

    /* Two bits per edge.
    Stored in format. BS[15] | BS[14] | .. |BS[0]*/
    UWORD32 *pu4_horz_bs;

    /**
     *  Qp array stored for each 8x8 pixels
     */
    UWORD8 *pu1_qp;

    /**
     *  Pointer to current frame's pu_t array
     */
    pu_t *ps_frm_pu;

    /**
     * Pointer to current frame's pu_t index array, which stores starting index
     * of pu_t for every MB
     */
    UWORD32 *pu4_frm_pu_idx;

    /**
     *  Pointer to current frame's pu map array
     */
    UWORD8 *pu1_frm_pu_map;

    /*************************************************************************/
    /* Need to add encoder specific elements for identifying the order of    */
    /* coding for CU, TU and PU if any                                       */
    /*************************************************************************/
} mb_ctxt_t;

/*************************************************************************/
/* The following describes how each of the CU cases are handled          */
/*************************************************************************/

/*************************************************************************/
/* For SKIP MB                                                           */
/* One Inter PU with appropriate MV                                      */
/* One TU which says CBP is zero and size is 16x16                       */
/*************************************************************************/

/*************************************************************************/
/* For Inter MB                                                          */
/* M Inter PU with appropriate MVs (M between 1 to 4)                    */
/* Number of TUs derived based on transform size                         */
/*************************************************************************/

/*************************************************************************/
/* For Intra MB                                                          */
/* Number of TUs derived based on transform size                         */
/* N Intra Modes are signaled along with coeff data at the start        */
/*************************************************************************/

/*************************************************************************/
/* For Intra PCM MB                                                      */
/* One TU which says ipcm is 1                                           */
/*************************************************************************/



/**
 * Structure to hold quantization parameters of an mb
 */
typedef struct
{

    /*
     * mb qp
     */
    UWORD8 u1_mb_qp;

    /*
     * mb qp / 6
     */
    UWORD8 u1_qp_div;

    /*
     * mb qp mod 6
     */
    UWORD8 u1_qp_rem;

    /*
     * QP bits
     */
    UWORD8  u1_qbits;

    /*
     * forward scale matrix
     */
    const UWORD16 *pu2_scale_mat;

    /*
     * threshold matrix for quantization
     */
    UWORD16 *pu2_thres_mat;

    /*
     * Threshold to compare the sad with
     */
    UWORD16 *pu2_sad_thrsh;

    /*
     * qp dependent rounding constant
     */
    UWORD32 u4_dead_zone;

    /*
     *  inverse scale matrix
     */
    const UWORD16 *pu2_iscale_mat;

    /*
     * Weight matrix in iquant
     */
    UWORD16 *pu2_weigh_mat;

}quant_params_t;

/**
 * Structure to hold Profile tier level info for a given layer
 */

typedef struct
{
    /**
     *  NAL unit type
     */
    WORD8 i1_nal_unit_type;

    /**
     * NAL ref idc
     */
    WORD8 i1_nal_ref_idc;


} nal_header_t;

/**
 * HRD parameters Info
 */
typedef struct
{
    /**
     * Specifies the number of alternative CPB specifications in the
     * bitstream
     */
    UWORD8 u1_cpb_cnt_minus1;

    /**
    * (together with bit_rate_value_minus1) specifies the
    * maximum input bit rate of the i-th CPB
    */
    UWORD32 u4_bit_rate_scale;

    /**
    * (together with cpb_size_du_value_minus1) specifies
    * CPB size of the i-th CPB when the CPB operates
    * at the access unit level
    */
    UWORD32 u4_cpb_size_scale;

    /**
     *  (together with bit_rate_scale) specifies the
     *  maximum input bit rate for the i-th CPB
     */
    UWORD32 au4_bit_rate_value_minus1[32];
    /**
     *  together with cpb_size_scale to specify the
     *  CPB size when the CPB operates at the access unit level.
     */
    UWORD32 au4_cpb_size_value_minus1[32];

    /**
     * if 1, specifies that the HSS operates in a constant bit rate (CBR) mode
     * if 0, specifies that the HSS operates in a intermittent bit rate (CBR) mode
     */
    UWORD8  au1_cbr_flag[32];


    /**
    * specifies the length, in bits for initial cpb delay (nal/vcl)syntax in bp sei
    */
    UWORD8  u1_initial_cpb_removal_delay_length_minus1;

    /**
    * specifies the length, in bits for the cpb delay syntax in pt_sei
    */
    UWORD8  u1_cpb_removal_delay_length_minus1;

    /**
    * specifies the length, in bits, of the pic_dpb_output_delay syntax element in the pt SEI message
    */
    UWORD8  u1_dpb_output_delay_length_minus1;

    /**
     * Specifies length of the time offset parameter
     */
    UWORD8  u1_time_offset_length;

}hrd_params_t;


/**
 * Structure to hold VUI parameters Info
 */
typedef struct
{
    /**
    *  indicates the presence of aspect_ratio
    */
    UWORD8 u1_aspect_ratio_info_present_flag;

    /**
    *  specifies the aspect ratio of the luma samples
    */
    UWORD8 u1_aspect_ratio_idc;

    /**
    *  width of the luma samples. user dependent
    */
    UWORD16 u2_sar_width;

    /**
    *  Height of the luma samples. user dependent
    */
    UWORD16 u2_sar_height;

    /**
    * if 1, specifies that the overscan_appropriate_flag is present
    * if 0, the preferred display method for the video signal is unspecified
    */
    UWORD8 u1_overscan_info_present_flag;

    /**
    * if 1,indicates that the cropped decoded pictures output
    * are suitable for display using overscan
    */
    UWORD8 u1_overscan_appropriate_flag;

    /**
    * if 1 specifies that video_format, video_full_range_flag and
    * colour_description_present_flag are present
    */
    UWORD8 u1_video_signal_type_present_flag;

    /**
    * pal, secam, ntsc, ...
    */
    UWORD8 u1_video_format;

    /**
    * indicates the black level and range of the luma and chroma signals
    */
    UWORD8 u1_video_full_range_flag;

    /**
    * if 1,to 1 specifies that colour_primaries, transfer_characteristics
    * and matrix_coefficients are present
    */
    UWORD8 u1_colour_description_present_flag;

    /**
    * indicates the chromaticity coordinates of the source primaries
    */
    UWORD8 u1_colour_primaries;

    /**
    * indicates the opto-electronic transfer characteristic of the source picture
    */
    UWORD8 u1_transfer_characteristics;

    /**
    * the matrix coefficients used in deriving luma and chroma signals
    * from the green, blue, and red primaries
    */
    UWORD8 u1_matrix_coefficients;

    /**
    * if 1, specifies that chroma_sample_loc_type_top_field and
    * chroma_sample_loc_type_bottom_field are present
    */
    UWORD8 u1_chroma_loc_info_present_flag;

    /**
    * location of chroma samples
    */
    UWORD8 u1_chroma_sample_loc_type_top_field;

    UWORD8 u1_chroma_sample_loc_type_bottom_field;

    /**
    *   Indicates the presence of the
    *   num_units_in_ticks, time_scale flag
    */
    UWORD8 u1_vui_timing_info_present_flag;

    /**
    *   Number of units that
    *   correspond to one increment of the
    *   clock. Indicates the  resolution
    */
    UWORD32 u4_vui_num_units_in_tick;

    /**
    *   The number of time units that pass in one second
    */
    UWORD32 u4_vui_time_scale;

    /**
     *   Flag indicating that time difference between two frames is a constant
     */
    UWORD8 u1_fixed_frame_rate_flag;

    /**
     *   Indicates the presence of NAL HRD parameters
     */
    UWORD8 u1_nal_hrd_parameters_present_flag;

    /**
     *   NAL level HRD parameters
     */
    hrd_params_t s_nal_hrd_parameters;

    /**
     *   Indicates the presence of VCL HRD parameters
     */
    UWORD8 u1_vcl_hrd_parameters_present_flag;

    /**
     *   VCL level HRD parameters
     */
    hrd_params_t s_vcl_hrd_parameters;

    /**
     *  Specifies the HRD operational mode
     */
    UWORD8 u1_low_delay_hrd_flag;

    /**
     * Indicates presence of SEI messages which include pic_struct syntax element
     */
    UWORD8 u1_pic_struct_present_flag;

    /**
    * 1, specifies that the following cvs bitstream restriction parameters are present
    */
    UWORD8 u1_bitstream_restriction_flag;

    /**
    * if 0, indicates that no pel outside the pic boundaries and
    * no sub-pels derived using pels outside the pic boundaries is used for inter prediction
    */
    UWORD8 u1_motion_vectors_over_pic_boundaries_flag;

    /**
    * Indicates a number of bytes not exceeded by the sum of the sizes of the VCL NAL units
    * associated with any coded picture
    */
    UWORD8 u1_max_bytes_per_pic_denom;

    /**
    *  Indicates an upper bound for the number of bits of coding_unit() data
    */
    UWORD8 u1_max_bits_per_mb_denom;

    /**
    * Indicate the maximum absolute value of a decoded horizontal MV component
    * in quarter-pel luma units
    */
    UWORD8 u1_log2_max_mv_length_horizontal;

    /**
    * Indicate the maximum absolute value of a decoded vertical MV component
    * in quarter-pel luma units
    */
    UWORD8 u1_log2_max_mv_length_vertical;

    /**
     *  Max number of frames that are not synchronized in display and decode order
     */
    UWORD8 u1_num_reorder_frames;

    /**
     * specifies required size of the HRD DPB in units of frame buffers.
     */
     UWORD8 u1_max_dec_frame_buffering;

} vui_t;


/**
 * Structure to hold SPS info
 */
typedef struct
{
    /**
     *  profile_idc
     */
    UWORD8 u1_profile_idc;

    /** constraint_set0_flag */
    UWORD8 u1_constraint_set0_flag;

    /** constraint_set1_flag */
    UWORD8 u1_constraint_set1_flag;

    /** constraint_set2_flag */
    UWORD8 u1_constraint_set2_flag;

    /** constraint_set3_flag */
    UWORD8 u1_constraint_set3_flag;

    /**
     *  level_idc
     */
    UWORD8 u1_level_idc;

    /**
     *  seq_parameter_set_id
     */
    UWORD8 u1_sps_id;


    /**
     *  chroma_format_idc
     */
    UWORD8 u1_chroma_format_idc;

    /**
     *  residual_colour_transform_flag
     */
    WORD8 i1_residual_colour_transform_flag;

    /**
     *  bit_depth_luma_minus8
     */
    WORD8 i1_bit_depth_luma;

    /**
     *  bit_depth_chroma_minus8
     */
    WORD8 i1_bit_depth_chroma;

    /**
     *  qpprime_y_zero_transform_bypass_flag
     */
    WORD8 i1_qpprime_y_zero_transform_bypass_flag;

    /**
     *  seq_scaling_matrix_present_flag
     */
    WORD8 i1_seq_scaling_matrix_present_flag;

    /**
     *  seq_scaling_list_present_flag
     */
    WORD8 ai1_seq_scaling_list_present_flag[8];

    /**
     *  log2_max_frame_num_minus4
     */
    WORD8 i1_log2_max_frame_num;

    /**
     *  MaxFrameNum in the standard
     *  1 << i1_log2_max_frame_num
     */
    WORD32 i4_max_frame_num;

    /**
     *  pic_order_cnt_type
     */
    WORD8 i1_pic_order_cnt_type;

    /**
     *  log2_max_pic_order_cnt_lsb_minus4
     */
    WORD8 i1_log2_max_pic_order_cnt_lsb;

    /**
     * MaxPicOrderCntLsb in the standard.
     * 1 << log2_max_pic_order_cnt_lsb_minus4
     */
    WORD32 i4_max_pic_order_cnt_lsb;

    /**
     *  delta_pic_order_always_zero_flag
     */
    WORD8 i1_delta_pic_order_always_zero_flag;

    /**
     *  offset_for_non_ref_pic
     */
    WORD32 i4_offset_for_non_ref_pic;

    /**
     *  offset_for_top_to_bottom_field
     */
    WORD32 i4_offset_for_top_to_bottom_field;

    /**
     *  num_ref_frames_in_pic_order_cnt_cycle
     */
    UWORD8 u1_num_ref_frames_in_pic_order_cnt_cycle;

    /**
     * Offset_for_ref_frame
     */
    WORD32 ai4_offset_for_ref_frame[256];

    /**
     *  max_num_ref_frames
     */
    UWORD8 u1_max_num_ref_frames;

    /**
     *  gaps_in_frame_num_value_allowed_flag
     */
    WORD8 i1_gaps_in_frame_num_value_allowed_flag;

    /**
     *  pic_width_in_mbs_minus1
     */
    WORD16 i2_pic_width_in_mbs_minus1;

    /**
     *  pic_height_in_map_units_minus1
     */
    WORD16 i2_pic_height_in_map_units_minus1;

    /**
     *  frame_mbs_only_flag
     */
    WORD8 i1_frame_mbs_only_flag;

    /**
     *  mb_adaptive_frame_field_flag
     */
    WORD8 i1_mb_adaptive_frame_field_flag;

    /**
     *  direct_8x8_inference_flag
     */
    WORD8 i1_direct_8x8_inference_flag;

    /**
     *  frame_cropping_flag
     */
    WORD8 i1_frame_cropping_flag;

    /**
     *  frame_crop_left_offset
     */
    WORD16 i2_frame_crop_left_offset;

    /**
     *  frame_crop_right_offset
     */
    WORD16 i2_frame_crop_right_offset;

    /**
     *  frame_crop_top_offset
     */
    WORD16 i2_frame_crop_top_offset;

    /**
     *  frame_crop_bottom_offset
     */
    WORD16 i2_frame_crop_bottom_offset;

    /**
     *  vui_parameters_present_flag
     */
    WORD8 i1_vui_parameters_present_flag;

    /**
     * vui_parameters_Structure_info
     */
    vui_t s_vui_parameters;

    /**
     * Flag to give status of SPS structure
     */
    WORD8 i1_sps_valid;

    /**
     * Coded Picture width
     */
    WORD32 i2_pic_wd;

    /**
     * Coded Picture height
     */
    WORD32 i2_pic_ht;

    /**
     *  Picture width in MB units
     */

    WORD16 i2_pic_wd_in_mb;

    /**
     *  Picture height in MB units
     */

    WORD16 i2_pic_ht_in_mb;

    /**
     * useDefaultScalingMatrixFlag
     */
    WORD8 ai1_use_default_scaling_matrix_flag[8];

    /**
     * 4x4 Scaling lists after inverse zig zag scan
     */
    UWORD16 au2_4x4_weight_scale[6][16];

    /**
     * 4x4 Scaling lists after inverse zig zag scan
     */
    UWORD16 au2_8x8_weight_scale[2][64];

} sps_t;


/**
 * Structure to hold PPS info
 */
typedef struct
{
    /**
     *  pic_parameter_set_id
     */
    UWORD8 u1_pps_id;

    /**
     *  seq_parameter_set_id
     */
    UWORD8 u1_sps_id;

    /**
     *   Entropy coding : 0-VLC; 1 - CABAC
     */
    UWORD8 u1_entropy_coding_mode_flag;

    /*
     * Pic order present flag
     */
    UWORD8 u1_pic_order_present_flag;

    /*
     * Number of slice groups
     */
    UWORD8 u1_num_slice_groups;

    /*
     * Slice group map type
     */
    UWORD8 u1_slice_group_map_type;

    /*
     * Maximum reference picture index in the reference list 0 : range [0 - 31]
     */
    WORD8 i1_num_ref_idx_l0_default_active;

    /*
     * Maximum reference picture index in the reference list 1 : range [0 - 31]
     */
    WORD8 i1_num_ref_idx_l1_default_active;

    /**
     *  weighted_pred_flag
     */
    WORD8 i1_weighted_pred_flag;

    /**
     *  weighted_bipred_flag
     */
    WORD8 i1_weighted_bipred_idc;

    /**
     *  pic_init_qp_minus26
     */
    WORD8 i1_pic_init_qp;

    /**
     *  pic_init_qs_minus26
     */
    WORD8 i1_pic_init_qs;

    /*
     * Chroma QP offset w.r.t QPY {-12,12}
     */
    WORD8  i1_chroma_qp_index_offset;

    /**
     *  deblocking_filter_control_present_flag
     */
    WORD8 i1_deblocking_filter_control_present_flag;

    /**
     *  constrained_intra_pred_flag
     */
    WORD8 i1_constrained_intra_pred_flag;

    /**
     *  redundant_pic_cnt_present_flag
     */
    WORD8 i1_redundant_pic_cnt_present_flag;

    /**
     *  transform_8x8_mode_flag
     */
    WORD8 i1_transform_8x8_mode_flag;

    /**
     *  pic_scaling_matrix_present_flag
     */
    WORD8 i1_pic_scaling_matrix_present_flag;

    /*
     *  Second chroma QP offset
     */
    WORD8  i1_second_chroma_qp_index_offset;


    /**
     * useDefaultScalingMatrixFlag
     */
    WORD8 ai1_use_default_scaling_matrix_flag[8];

    /**
     * 4x4 Scaling lists after inverse zig zag scan
     */
    UWORD16 au2_4x4_weight_scale[6][16];

    /**
     * 4x4 Scaling lists after inverse zig zag scan
     */
    UWORD16 au2_8x8_weight_scale[2][64];


    /**
     *  pic_scaling_list_present_flag
     */
    WORD8 ai1_pic_scaling_list_present_flag[8];

    /**
     * Flag to give status of PPS structure
     */
    WORD8   i1_pps_valid;


} pps_t;

/**
 * MMCO commands and params.
 */
typedef struct
{
    /* memory management control operation command */
    UWORD8 u1_memory_management_control_operation;

    /*
     * Contains difference of pic nums of short-term pic/frame
     * 1. To signal it as "unused for reference" if mmco = 1
     * 2. To signal it as "used for long-term reference" if mmco = 3
     */
    UWORD32 u4_difference_of_pic_nums_minus1;

    /* Long-term pic num to be set as "unused for reference" */
    UWORD8 u1_long_term_pic_num;

    /*
     * Assign a long-term idx to a picture as follows
     * 1. Assign to a short-term pic if mmco = 3
     * 2. Assign to the current pic if mmco = 6
     */
    UWORD8 u1_long_term_frame_idx;

    /*
     * The max long-term idx. The long-term pics having idx above
     * are set as "unused for reference
     */
    UWORD8 u1_max_long_term_frame_idx_plus1;

}mmco_prms_t;

/**
 * Structure to hold Reference picture list modification info
 */
typedef struct
{
    /* ref_pic_list_modification_flag_l0 */
    WORD8 i1_ref_pic_list_modification_flag_l0;

    /* Modification required in list0 */
    WORD8 i1_modification_of_pic_nums_idc_l0[MAX_MODICATION_IDC];

    /*
     * The absolute difference between the picture number of
     * the picture being moved to the current index in
     * list0 and the picture number prediction value
     */
    UWORD32 u4_abs_diff_pic_num_minus1_l0[MAX_MODICATION_IDC];

    /*
     * The long-term picture number of the picture being moved
     * to the current index in list0
     */
    UWORD8 u1_long_term_pic_num_l0[MAX_MODICATION_IDC];

    /* ref_pic_list_modification_flag_l1 */
    WORD8 i1_ref_pic_list_modification_flag_l1;

    /* Modification required in list1 */
    WORD8 i1_modification_of_pic_nums_idc_l1[MAX_MODICATION_IDC];

    /*
     * The absolute difference between the picture number of
     * the picture being moved to the current index in
     * list1 and the picture number prediction value
     */
    UWORD32 u4_abs_diff_pic_num_minus1_l1[MAX_MODICATION_IDC];

    /*
     * The long-term picture number of the picture being moved
     * to the current index in list1
     */
   UWORD8 u1_long_term_pic_num_l1[MAX_MODICATION_IDC];
}rplm_t;

/**
 * Structure to hold Slice Header info
 */
typedef struct
{

    /*
     *  nal_unit_type
     */
    WORD8  i1_nal_unit_type;

    /*
     *  nal_unit_idc
     */
    WORD8  i1_nal_unit_idc;

    /*
     *  first_mb_in_slice
     */
    UWORD16   u2_first_mb_in_slice;

    /*
     *  slice_type
     */
    UWORD8   u1_slice_type;

    /*
     *  pic_parameter_set_id
     */
    UWORD8   u1_pps_id;

    /*
     *  frame_num
     */
    WORD32 i4_frame_num;

    /*
     *  field_pic_flag
     */
    WORD8   i1_field_pic_flag;

    /*
     *  bottom_field_flag
     */
    WORD8   i1_bottom_field_flag;

    /*
     *  second_field
     */
    WORD8   i1_second_field_flag;

    /*
     *  idr_pic_id
     */
    UWORD16 u2_idr_pic_id ;

    /*
     *  pic_order_cnt_lsb
     */
    UWORD16 i4_pic_order_cnt_lsb;

    /*
     *  delta_pic_order_cnt_bottom
     */
    WORD32  i4_delta_pic_order_cnt_bottom;

    /*
     *  delta_pic_order_cnt
     */
    WORD32   ai4_delta_pic_order_cnt[2];

    /*
     *  redundant_pic_cnt
     */
    UWORD8   u1_redundant_pic_cnt;

    /*
     *  direct_spatial_mv_pred_flag
     */
    UWORD8   u1_direct_spatial_mv_pred_flag;

    /*
     *  num_ref_idx_active_override_flag
     */
    UWORD8   u1_num_ref_idx_active_override_flag;

    /*
     *  num_ref_idx_l0_active
     */
    WORD8   i1_num_ref_idx_l0_active;

    /*
     *  num_ref_idx_l1_active_minus1
     */
    WORD8   i1_num_ref_idx_l1_active;

    /*
     * ref_pic_list_reordering_flag_l0
     */
    UWORD8  u1_ref_idx_reordering_flag_l0;

    /*
     * ref_pic_list_reordering_flag_l1
     */
    UWORD8  u1_ref_idx_reordering_flag_l1;

    /**
     *  Reference prediction list modification
     */
    rplm_t s_rplm;

    /**
     * L0 Reference pic lists
     */
    ref_list_t as_ref_pic_list0[MAX_DPB_SIZE];

    /**
     * L1 Reference pic lists
     */
    ref_list_t as_ref_pic_list1[MAX_DPB_SIZE];

    /*
     * no_output_of_prior_pics_flag
     */
    UWORD8   u1_no_output_of_prior_pics_flag;

    /*
     * long_term_reference_flag
     */
    UWORD8   u1_long_term_reference_flag;

    /*
     * adaptive_ref_pic_marking_mode_flag
     */
    UWORD8   u1_adaptive_ref_pic_marking_mode_flag;

    /*
     * Array to structures to store mmco commands
     * and parameters.
     */
    mmco_prms_t as_mmco_prms[MAX_MMCO_COMMANDS];

    /*
     *  entropy_coding_mode_flag
     */
    WORD8   u1_entropy_coding_mode_flag;

    /*
     *  cabac_init_idc
     */
    WORD8   i1_cabac_init_idc;

    /*
     *  i1_slice_qp
     */
    WORD8   i1_slice_qp;

    /*
     *  sp_for_switch_flag
     */
    UWORD8   u1_sp_for_switch_flag;

    /*
     *  slice_qs_delta
     */
    UWORD8   u1_slice_qs;

    /*
     *  disable_deblocking_filter_idc
     */
    WORD8   u1_disable_deblocking_filter_idc;

    /*
     *  slice_alpha_c0_offset_div2
     */
    WORD8   i1_slice_alpha_c0_offset_div2;

    /*
     *  slice_beta_offset_div2
     */
    WORD8   i1_slice_beta_offset_div2;

    /*
     *  num_slice_groups_minus1
     */
    WORD8   u1_num_slice_groups_minus1;

    /*
     *  slice_group_change_cycle
     */
    WORD8   u1_slice_group_change_cycle;

    /**
     * Start MB X
     */
    UWORD16 i2_mb_x;

    /**
     * Start MB Y
     */
    UWORD16 i2_mb_y;

    /**
     * Absolute POC. Contains minimum of top and bottom POC.
     */
    WORD32 i4_abs_pic_order_cnt;

    /**
     *  Absolute top POC. Contains top poc for frame or top
     *  field. Invalid for bottom field.
     */
    WORD32 i4_abs_top_pic_order_cnt;

    /**
     *  Absolute top POC. Contains bottom poc for frame or bottom
     *  field. Invalid for top field.
     */
    WORD32 i4_abs_bottom_pic_order_cnt;

    /** Flag signaling if the current slice is ref slice */
    UWORD8 i1_nal_ref_idc;

    /** Flag to indicate if the current slice is MBAFF Frame */
    UWORD8 u1_mbaff_frame_flag;

    /** luma_log2_weight_denom */
    UWORD8 u1_luma_log2_weight_denom;

    /** chroma_log2_weight_denom */
    UWORD8 u1_chroma_log2_weight_denom;

    /** luma_weight_l0_flag */
    UWORD8 au1_luma_weight_l0_flag[MAX_DPB_SIZE];

    /** luma_weight_l0 : (-128, 127 )is the range of weights
     * when weighted pred is enabled, 128 is default value */
    WORD16 ai2_luma_weight_l0[MAX_DPB_SIZE];

    /** luma_offset_l0 : (-128, 127 )is the range of offset
     * when weighted pred is enabled, 0 is default value */
    WORD8 ai1_luma_offset_l0[MAX_DPB_SIZE];

    /** chroma_weight_l0_flag */
    UWORD8 au1_chroma_weight_l0_flag[MAX_DPB_SIZE];

    /** chroma_weight_l0 : (-128, 127 )is the range of weights
     * when weighted pred is enabled, 128 is default value*/
    WORD16 ai2_chroma_weight_l0[MAX_DPB_SIZE][2];

    /** chroma_offset_l0 : (-128, 127 )is the range of offset
     * when weighted pred is enabled, 0 is default value*/
    WORD8 ai1_chroma_offset_l0[MAX_DPB_SIZE][2];

    /** luma_weight_l0_flag */
    UWORD8 au1_luma_weight_l1_flag[MAX_DPB_SIZE];

    /** luma_weight_l1 : (-128, 127 )is the range of weights
     * when weighted pred is enabled, 128 is default value */
    WORD16 ai2_luma_weight_l1[MAX_DPB_SIZE];

    /** luma_offset_l1 : (-128, 127 )is the range of offset
     * when weighted pred is enabled, 0 is default value */
    WORD8 ai1_luma_offset_l1[MAX_DPB_SIZE];

    /** chroma_weight_l1_flag */
    UWORD8 au1_chroma_weight_l1_flag[MAX_DPB_SIZE];

    /** chroma_weight_l1 : (-128, 127 )is the range of weights
     * when weighted pred is enabled, 128 is default value */
    WORD16 ai2_chroma_weight_l1[MAX_DPB_SIZE][2];

    /** chroma_offset_l1 :(-128, 127 )is the range of offset
     * when weighted pred is enabled, 0 is default value */
    WORD8 ai1_chroma_offset_l1[MAX_DPB_SIZE][2];
}slice_header_t;


/*****************************************************************************/
/* The following can be used to type cast coefficient data that is stored    */
/*  per subblock. Note that though i2_level is shown as an array that        */
/*  holds 16 coefficients, only the first few entries will be valid. Next    */
/*  subblocks data starts after the valid number of coefficients. Number     */
/*  of non-zero coefficients will be derived using number of non-zero bits   */
/*  in sig coeff map                                                         */
/*****************************************************************************/

/**
 * Structure to hold coefficient info for a 2x2 chroma DC transform
 */
typedef struct
{
    /**
     * significant coefficient map
     */
    UWORD8 u1_sig_coeff_map;

    /**
     * sub block position
     */
    UWORD8 u1_subblk_pos;

    /**
     * holds coefficients
     */
    WORD16  ai2_level[2 * 2];
}tu_sblk2x2_coeff_data_t;

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
     * sub block position
     */
    UWORD16 u2_subblk_pos;

    /**
     * holds coefficients
     */
    WORD16  ai2_level[SUBBLK_COEFF_CNT];
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
     * sub block position
     */
    UWORD16 u2_subblk_pos;

    /**
     * holds coefficients
     */
    WORD16  ai2_level[TRANS_SIZE_8 * TRANS_SIZE_8];
}tu_blk8x8_coeff_data_t;


/**
 * Structure to hold coefficient info for a 16x16 IPCM MB
 */
typedef struct
{
    /**
     * holds coefficients
     */
    UWORD8  au1_level[MB_SIZE * MB_SIZE * 3 / 2];
}tu_ipcm_coeff_data_t;


typedef struct
{
    /**
     * Transform sizes 0: 4x4, 1: 8x8,
     */
    UWORD32     b1_trans_size : 1;

    /**
     * Flag to signal if the current MB is IPCM
     */
    UWORD32     b1_ipcm : 1;

    /**
     * Intra pred sizes  0: 4x4, 1: 8x8, 2: 16x16
     */
    UWORD32     b2_intra_pred_size : 2;

    /**
     * Chroma intra mode
     */
    UWORD32     b2_intra_chroma_pred_mode: 2;

    /**
     * Number of coded subblocks in the current MB, for which
     * tu data is sent. Maximum of 27 subblocks in the following
     * order.
     * 1 4x4 luma DC(for intra16x16),
     * 16 4x4 luma,
     * 2 2x2 chroma DC,
     * 8 4x4 chroma,
     */
    WORD32      b5_num_coded_sblks: 5;

    /**
     * Flag to signal if 4x4 subblock for DC values (in INTRA 16x16 MB)
     * is coded
     */
    UWORD32     b1_luma_dc_coded: 1;

    /**
     * Flag to signal if 4x4 subblock for DC values (in INTRA 16x16 MB)
     * is coded
     */
    UWORD32     b1_chroma_dc_coded: 1;

    /**
     * CSBP - 16 bits, 1 bit for each 4x4
     * for intra16x16 mb_type only ac coefficients are
     */
    UWORD32     b16_luma_csbp: 16;

    /**
     * CSBP - 16 bits, 1 bit for each 4x4
     * for intra16x16 mb_type only ac coefficients are
     */
    UWORD32     b8_chroma_csbp: 8;

    /**
     * Luma Intra pred modes,
     * Based on intra pred size either 16, 4 or 1 entry will be
     * populated below.
     */
    UWORD8     au1_luma_intra_modes[16];

}intra_mb_t;


typedef struct
{
    /**
     * Transform sizes 0: 4x4, 1: 8x8,
     */
    UWORD8     b1_trans_size : 1;


    /**
     * Skip flag
     */
    UWORD8     b1_skip : 1;


    /**
     * Number of coded subblocks in the current MB, for which
     * tu data is sent. Maximum of 26 subblocks in the following
     * order.
     * 16 4x4 luma,
     * 2 2x2 chroma DC,
     * 8 4x4 chroma,
     */
    WORD32      b5_num_coded_sblks: 5;

    /**
     * CSBP - 16 bits, 1 bit for each 4x4
     * for intra16x16 mb_type only ac coefficients are
     */
    UWORD32     b16_luma_csbp: 16;

    /**
     * CSBP - 16 bits, 1 bit for each 4x4
     * for intra16x16 mb_type only ac coefficients are
     */
    UWORD32     b16_chroma_csbp: 8;
}inter_mb_t;

/**
 * Structure to hold Mastering Display Color Volume SEI
 */
typedef struct
{
    /**
     * Array to store the display_primaries_x values
     */
    UWORD16 au2_display_primaries_x[NUM_SEI_MDCV_PRIMARIES];

    /**
     * Array to store the display_primaries_y values
     */
    UWORD16 au2_display_primaries_y[NUM_SEI_MDCV_PRIMARIES];

    /**
     * Variable to store the white point x value
     */
    UWORD16 u2_white_point_x;

    /**
     * Variable to store the white point y value
     */
    UWORD16 u2_white_point_y;

    /**
     * Variable to store the max display mastering luminance value
     */
    UWORD32 u4_max_display_mastering_luminance;

    /**
     * Variable to store the min display mastering luminance value
     */
    UWORD32 u4_min_display_mastering_luminance;
}sei_mdcv_params_t;


/**
 *  Structure for Content Light Level Info
 *
 */
typedef struct
{
    /**
     * The maximum pixel intensity of all samples
     */
    UWORD16 u2_max_content_light_level;

    /**
     * The average pixel intensity of all samples
     */
    UWORD16 u2_max_pic_average_light_level;
}sei_cll_params_t;


/**
 * Structure to hold Ambient viewing environment SEI
 */
typedef struct
{
    /**
     * specifies the environmental illluminance of the ambient viewing environment
     */
    UWORD32 u4_ambient_illuminance;

    /*
     * specify the normalized x chromaticity coordinates of the
     * environmental ambient light in the nominal viewing environment
     */
    UWORD16 u2_ambient_light_x;

    /*
    * specify the normalized y chromaticity coordinates of the
    * environmental ambient light in the nominal viewing environment
    */
    UWORD16 u2_ambient_light_y;
}sei_ave_params_t;


/**
 * Structure to hold Content color volume SEI
 */
typedef struct
{
    /*
     * Flag used to control persistence of CCV SEI messages
     */
    UWORD8 u1_ccv_cancel_flag;

    /*
     * specifies the persistence of the CCV SEI message for the current layer
     */
    UWORD8 u1_ccv_persistence_flag;

    /*
     * specifies the presence of syntax elements ccv_primaries_x and ccv_primaries_y
     */
    UWORD8 u1_ccv_primaries_present_flag;

    /*
     * specifies that the syntax element ccv_min_luminance_value is present
     */
    UWORD8 u1_ccv_min_luminance_value_present_flag;

    /*
     * specifies that the syntax element ccv_max_luminance_value is present
     */
    UWORD8 u1_ccv_max_luminance_value_present_flag;

    /*
     * specifies that the syntax element ccv_avg_luminance_value is present
     */
    UWORD8 u1_ccv_avg_luminance_value_present_flag;

    /*
     * shall be equal to 0 in bitstreams conforming to this version. Other values
     * for reserved_zero_2bits are reserved for future use
     */
    UWORD8 u1_ccv_reserved_zero_2bits;

    /*
     * specify the normalized x chromaticity coordinates of the colour
     * primary component c of the nominal content colour volume
     */
    WORD32 ai4_ccv_primaries_x[NUM_SEI_CCV_PRIMARIES];

    /*
     * specify the normalized y chromaticity coordinates of the colour
     * primary component c of the nominal content colour volume
     */
    WORD32 ai4_ccv_primaries_y[NUM_SEI_CCV_PRIMARIES];

    /*
     * specifies the normalized minimum luminance value
     */
    UWORD32 u4_ccv_min_luminance_value;

    /*
     * specifies the normalized maximum luminance value
     */
    UWORD32 u4_ccv_max_luminance_value;

    /*
     * specifies the normalized average luminance value
     */
    UWORD32 u4_ccv_avg_luminance_value;
}sei_ccv_params_t;


/**
 * Structure to hold SEI parameters Info
 */
typedef struct
{
    /**
     *  mastering display color volume info present flag
     */
    UWORD8 u1_sei_mdcv_params_present_flag;

    /*
     * MDCV parameters
     */
    sei_mdcv_params_t s_sei_mdcv_params;

    /**
     * content light level info present flag
     */
    UWORD8 u1_sei_cll_params_present_flag;

    /*
     * CLL parameters
     */
    sei_cll_params_t s_sei_cll_params;

    /**
     * ambient viewing environment info present flag
     */
    UWORD8 u1_sei_ave_params_present_flag;

    /*
     * AVE parameters
     */
    sei_ave_params_t s_sei_ave_params;

    /**
     * content color volume info present flag
     */
    UWORD8 u1_sei_ccv_params_present_flag;

    /*
     * CCV parameters
     */
    sei_ccv_params_t s_sei_ccv_params;
} sei_params_t;


#endif /* _IH264_STRUCTS_H_ */
