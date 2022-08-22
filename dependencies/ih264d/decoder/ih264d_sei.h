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

/*****************************************************************************/
/*                                                                           */
/*  File Name         : ih264d_sei.h                                                */
/*                                                                           */
/*  Description       : This file contains routines to parse SEI NAL's       */
/*                                                                           */
/*  List of Functions : <List the functions defined in this file>            */
/*                                                                           */
/*  Issues / Problems : None                                                 */
/*                                                                           */
/*  Revision History  :                                                      */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         25 05 2005   NS              Draft                                */
/*                                                                           */
/*****************************************************************************/

#ifndef _IH264D_SEI_H_
#define _IH264D_SEI_H_

#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264d_bitstrm.h"
#include "ih264d_structs.h"
#include "ih264d.h"

#define SEI_BUF_PERIOD      0
#define SEI_PIC_TIMING      1
#define SEI_PAN_SCAN_RECT   2
#define SEI_FILLER          3
#define SEI_UD_REG_T35      4
#define SEI_UD_UN_REG       5
#define SEI_RECOVERY_PT     6
#define SEI_DEC_REF_MARK    7
#define SEI_SPARE_PIC       8
#define SEI_SCENE_INFO      9
#define SEI_SUB_SEQN_INFO   10
#define SEI_SUB_SEQN_LAY_CHAR       11
#define SEI_SUB_SEQN_CHAR   12
#define SEI_FULL_FRAME_FREEZE       13
#define SEI_FULL_FRAME_FREEZE_REL   14
#define SEI_FULL_FRAME_SNAP_SHOT    15
#define SEI_PROG_REF_SEGMENT_START  16
#define SEI_PROG_REF_SEGMENT_END    17
#define SEI_MOT_CON_SLICE_GRP_SET   18
#define SEI_MASTERING_DISP_COL_VOL       137
#define SEI_CONTENT_LIGHT_LEVEL_DATA     144
#define SEI_AMBIENT_VIEWING_ENVIRONMENT  148
#define SEI_CONTENT_COLOR_VOLUME         149

/* Declaration of dec_struct_t to avoid CCS compilation Error */
struct _DecStruct;
WORD32 ih264d_parse_sei_message(struct _DecStruct *ps_dec,
                                dec_bit_stream_t *ps_bitstrm);
typedef struct
{
    UWORD8 u1_seq_parameter_set_id;
    UWORD32 u4_initial_cpb_removal_delay;
    UWORD32 u4_nitial_cpb_removal_delay_offset;

} buf_period_t;

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
 * Structure for Content Light Level Info
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
     * specifies the environmental illuminance of the ambient viewing environment
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

struct _sei
{
    UWORD8 u1_seq_param_set_id;
    buf_period_t s_buf_period;
    UWORD8 u1_pic_struct;
    UWORD16 u2_recovery_frame_cnt;
    UWORD8 u1_exact_match_flag;
    UWORD8 u1_broken_link_flag;
    UWORD8 u1_changing_slice_grp_idc;
    UWORD8 u1_is_valid;

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

};
typedef struct _sei sei;

WORD32 ih264d_export_sei_mdcv_params(ivd_sei_decode_op_t *ps_sei_decode_op,
                                     sei *ps_sei, sei *ps_sei_export);

WORD32 ih264d_export_sei_cll_params(ivd_sei_decode_op_t *ps_sei_decode_op,
                                    sei *ps_sei, sei *ps_sei_export);

WORD32 ih264d_export_sei_ave_params(ivd_sei_decode_op_t *ps_sei_decode_op,
                                    sei *ps_sei, sei *ps_sei_export);

WORD32 ih264d_export_sei_ccv_params(ivd_sei_decode_op_t *ps_sei_decode_op,
                                    sei *ps_sei, sei *ps_sei_export);

#endif /* _IH264D_SEI_H_ */

