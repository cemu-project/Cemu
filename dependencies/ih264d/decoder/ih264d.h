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
/*  File Name         : ih264d.h                                             */
/*                                                                           */
/*  Description       : This file contains all the necessary structure and   */
/*                      enumeration definitions needed for the Application   */
/*                      Program Interface(API) of the Ittiam H264 ASP       */
/*                      Decoder on Cortex A8 - Neon platform                 */
/*                                                                           */
/*  List of Functions : ih264d_api_function                              */
/*                                                                           */
/*  Issues / Problems : None                                                 */
/*                                                                           */
/*  Revision History  :                                                      */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         26 08 2010   100239(RCY)     Draft                                */
/*                                                                           */
/*****************************************************************************/

#ifndef _IH264D_H_
#define _IH264D_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "iv.h"
#include "ivd.h"


/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define IVD_ERROR_MASK 0xFF

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/
#define IS_IVD_CONCEALMENT_APPLIED(x)       (x & (1 << IVD_APPLIEDCONCEALMENT))
#define IS_IVD_INSUFFICIENTDATA_ERROR(x)    (x & (1 << IVD_INSUFFICIENTDATA))
#define IS_IVD_CORRUPTEDDATA_ERROR(x)       (x & (1 << IVD_CORRUPTEDDATA))
#define IS_IVD_CORRUPTEDHEADER_ERROR(x)     (x & (1 << IVD_CORRUPTEDHEADER))
#define IS_IVD_UNSUPPORTEDINPUT_ERROR(x)    (x & (1 << IVD_UNSUPPORTEDINPUT))
#define IS_IVD_UNSUPPORTEDPARAM_ERROR(x)    (x & (1 << IVD_UNSUPPORTEDPARAM))
#define IS_IVD_FATAL_ERROR(x)               (x & (1 << IVD_FATALERROR))
#define IS_IVD_INVALID_BITSTREAM_ERROR(x)   (x & (1 << IVD_INVALID_BITSTREAM))
#define IS_IVD_INCOMPLETE_BITSTREAM_ERROR(x) (x & (1 << IVD_INCOMPLETE_BITSTREAM))


/*****************************************************************************/
/* API Function Prototype                                                    */
/*****************************************************************************/
IV_API_CALL_STATUS_T ih264d_api_function(iv_obj_t *ps_handle, void *pv_api_ip,void *pv_api_op);

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/
/* Codec Error codes for H264 ASP Decoder                                   */

typedef enum {

    IH264D_VID_HDR_DEC_NUM_FRM_BUF_NOT_SUFFICIENT   = IVD_DUMMY_ELEMENT_FOR_CODEC_EXTENSIONS + 1,

}IH264D_ERROR_CODES_T;

/*****************************************************************************/
/* Extended Structures                                                       */
/*****************************************************************************/


/*****************************************************************************/
/*  Delete Codec                                                             */
/*****************************************************************************/


typedef struct {
    ivd_delete_ip_t               s_ivd_delete_ip_t;
}ih264d_delete_ip_t;


typedef struct{
    ivd_delete_op_t               s_ivd_delete_op_t;
}ih264d_delete_op_t;


/*****************************************************************************/
/*   Initialize decoder                                                      */
/*****************************************************************************/


typedef struct {
    ivd_create_ip_t                         s_ivd_create_ip_t;
}ih264d_create_ip_t;


typedef struct{
    ivd_create_op_t                         s_ivd_create_op_t;
}ih264d_create_op_t;


/*****************************************************************************/
/*   Video Decode                                                            */
/*****************************************************************************/


typedef struct {
    ivd_video_decode_ip_t                   s_ivd_video_decode_ip_t;
}ih264d_video_decode_ip_t;


typedef struct{
    ivd_video_decode_op_t                   s_ivd_video_decode_op_t;
}ih264d_video_decode_op_t;


/*****************************************************************************/
/*   Get Display Frame                                                       */
/*****************************************************************************/


typedef struct
{
    ivd_get_display_frame_ip_t              s_ivd_get_display_frame_ip_t;
}ih264d_get_display_frame_ip_t;


typedef struct
{
    ivd_get_display_frame_op_t              s_ivd_get_display_frame_op_t;
}ih264d_get_display_frame_op_t;

/*****************************************************************************/
/*   Set Display Frame                                                       */
/*****************************************************************************/


typedef struct
{
    ivd_set_display_frame_ip_t              s_ivd_set_display_frame_ip_t;
}ih264d_set_display_frame_ip_t;


typedef struct
{
    ivd_set_display_frame_op_t              s_ivd_set_display_frame_op_t;
}ih264d_set_display_frame_op_t;

/*****************************************************************************/
/*   Release Display Buffers                                                 */
/*****************************************************************************/


typedef struct
{
    ivd_rel_display_frame_ip_t                  s_ivd_rel_display_frame_ip_t;
}ih264d_rel_display_frame_ip_t;


typedef struct
{
    ivd_rel_display_frame_op_t                  s_ivd_rel_display_frame_op_t;
}ih264d_rel_display_frame_op_t;


typedef enum {
    /** Set number of cores/threads to be used */
    IH264D_CMD_CTL_SET_NUM_CORES         = IVD_CMD_CTL_CODEC_SUBCMD_START,

    /** Set processor details */
    IH264D_CMD_CTL_SET_PROCESSOR         = IVD_CMD_CTL_CODEC_SUBCMD_START + 0x001,

    /** Get display buffer dimensions */
    IH264D_CMD_CTL_GET_BUFFER_DIMENSIONS = IVD_CMD_CTL_CODEC_SUBCMD_START + 0x100,

    /** Get VUI parameters */
    IH264D_CMD_CTL_GET_VUI_PARAMS        = IVD_CMD_CTL_CODEC_SUBCMD_START + 0x101,

    /** Enable/disable GPU, supported on select platforms */
    IH264D_CMD_CTL_GPU_ENABLE_DISABLE    = IVD_CMD_CTL_CODEC_SUBCMD_START + 0x200,

    /** Set degrade level */
    IH264D_CMD_CTL_DEGRADE               = IVD_CMD_CTL_CODEC_SUBCMD_START + 0x300,

    /** Get SEI MDCV parameters */
    IH264D_CMD_CTL_GET_SEI_MDCV_PARAMS   = IVD_CMD_CTL_CODEC_SUBCMD_START + 0x301,

    /** Get SEI CLL parameters */
    IH264D_CMD_CTL_GET_SEI_CLL_PARAMS    = IVD_CMD_CTL_CODEC_SUBCMD_START + 0x302,

    /** Get SEI AVE parameters */
    IH264D_CMD_CTL_GET_SEI_AVE_PARAMS    = IVD_CMD_CTL_CODEC_SUBCMD_START + 0x303,

    /** Get SEI CCV parameters */
    IH264D_CMD_CTL_GET_SEI_CCV_PARAMS    = IVD_CMD_CTL_CODEC_SUBCMD_START + 0x304

}IH264D_CMD_CTL_SUB_CMDS;
/*****************************************************************************/
/*   Video control  Flush                                                    */
/*****************************************************************************/


typedef struct{
    ivd_ctl_flush_ip_t                      s_ivd_ctl_flush_ip_t;
}ih264d_ctl_flush_ip_t;


typedef struct{
    ivd_ctl_flush_op_t                      s_ivd_ctl_flush_op_t;
}ih264d_ctl_flush_op_t;

/*****************************************************************************/
/*   Video control reset                                                     */
/*****************************************************************************/


typedef struct{
    ivd_ctl_reset_ip_t                      s_ivd_ctl_reset_ip_t;
}ih264d_ctl_reset_ip_t;


typedef struct{
    ivd_ctl_reset_op_t                      s_ivd_ctl_reset_op_t;
}ih264d_ctl_reset_op_t;


/*****************************************************************************/
/*   Video control  Set Params                                               */
/*****************************************************************************/


typedef struct {
    ivd_ctl_set_config_ip_t             s_ivd_ctl_set_config_ip_t;
}ih264d_ctl_set_config_ip_t;


typedef struct{
    ivd_ctl_set_config_op_t             s_ivd_ctl_set_config_op_t;
}ih264d_ctl_set_config_op_t;

/*****************************************************************************/
/*   Video control:Get Buf Info                                              */
/*****************************************************************************/


typedef struct{
    ivd_ctl_getbufinfo_ip_t             s_ivd_ctl_getbufinfo_ip_t;
}ih264d_ctl_getbufinfo_ip_t;



typedef struct{
    ivd_ctl_getbufinfo_op_t             s_ivd_ctl_getbufinfo_op_t;
}ih264d_ctl_getbufinfo_op_t;


/*****************************************************************************/
/*   Video control:Getstatus Call                                            */
/*****************************************************************************/


typedef struct{
    ivd_ctl_getstatus_ip_t                  s_ivd_ctl_getstatus_ip_t;
}ih264d_ctl_getstatus_ip_t;



typedef struct{
    ivd_ctl_getstatus_op_t                  s_ivd_ctl_getstatus_op_t;
}ih264d_ctl_getstatus_op_t;


/*****************************************************************************/
/*   Video control:Get Version Info                                          */
/*****************************************************************************/


typedef struct{
    ivd_ctl_getversioninfo_ip_t         s_ivd_ctl_getversioninfo_ip_t;
}ih264d_ctl_getversioninfo_ip_t;



typedef struct{
    ivd_ctl_getversioninfo_op_t         s_ivd_ctl_getversioninfo_op_t;
}ih264d_ctl_getversioninfo_op_t;

typedef struct{

    /**
     * u4_size
     */
    UWORD32                                     u4_size;

    /**
     * cmd
     */
    IVD_API_COMMAND_TYPE_T                      e_cmd;

    /**
     * sub_cmd
     */
    IVD_CONTROL_API_COMMAND_TYPE_T              e_sub_cmd;

    /**
     * Pictures that are are degraded
     * 0 : No degrade
     * 1 : Only on non-reference frames
     * 2 : Use interval specified by u4_nondegrade_interval
     * 3 : All non-key frames
     * 4 : All frames
     */
    WORD32                                     i4_degrade_pics;

    /**
     * Interval for pictures which are completely decoded without any degradation
     */
    WORD32                                     i4_nondegrade_interval;

    /**
     * bit position (lsb is zero): Type of degradation
     * 1 : Disable deblocking
     * 2 : Faster inter prediction filters
     * 3 : Fastest inter prediction filters
     */
    WORD32                                     i4_degrade_type;

}ih264d_ctl_degrade_ip_t;

typedef struct
{
    /**
     * u4_size
     */
    UWORD32                                     u4_size;

    /**
     * error_code
     */
    UWORD32                                     u4_error_code;
}ih264d_ctl_degrade_op_t;

typedef struct{
    UWORD32                                     u4_size;
    IVD_API_COMMAND_TYPE_T                      e_cmd;
    IVD_CONTROL_API_COMMAND_TYPE_T              e_sub_cmd;
    UWORD32                                     u4_disable_deblk_level;
}ih264d_ctl_disable_deblock_ip_t;

typedef struct{
    UWORD32                                     u4_size;
    UWORD32                                     u4_error_code;
}ih264d_ctl_disable_deblock_op_t;


typedef struct{
    UWORD32                                     u4_size;
    IVD_API_COMMAND_TYPE_T                      e_cmd;
    IVD_CONTROL_API_COMMAND_TYPE_T              e_sub_cmd;
    UWORD32                                     u4_num_cores;
}ih264d_ctl_set_num_cores_ip_t;

typedef struct{
    UWORD32                                     u4_size;
    UWORD32                                     u4_error_code;
}ih264d_ctl_set_num_cores_op_t;

typedef struct
{
     /**
      * i4_size
      */
    UWORD32                                     u4_size;
    /**
     * cmd
     */
    IVD_API_COMMAND_TYPE_T                      e_cmd;
    /**
     * sub cmd
     */
    IVD_CONTROL_API_COMMAND_TYPE_T              e_sub_cmd;
    /**
     * Processor type
     */
    UWORD32                                     u4_arch;
    /**
     * SOC type
     */
    UWORD32                                     u4_soc;

    /**
     * num_cores
     */
    UWORD32                                     u4_num_cores;

}ih264d_ctl_set_processor_ip_t;

typedef struct
{
    /**
     * i4_size
     */
    UWORD32                                     u4_size;
    /**
     * error_code
     */
    UWORD32                                     u4_error_code;
}ih264d_ctl_set_processor_op_t;

typedef struct{
    UWORD32                                     u4_size;
    IVD_API_COMMAND_TYPE_T                      e_cmd;
    IVD_CONTROL_API_COMMAND_TYPE_T              e_sub_cmd;
}ih264d_ctl_get_frame_dimensions_ip_t;


typedef struct{
    UWORD32                                     u4_size;
    UWORD32                                     u4_error_code;
    UWORD32                                     u4_x_offset[3];
    UWORD32                                     u4_y_offset[3];
    UWORD32                                     u4_disp_wd[3];
    UWORD32                                     u4_disp_ht[3];
    UWORD32                                     u4_buffer_wd[3];
    UWORD32                                     u4_buffer_ht[3];
}ih264d_ctl_get_frame_dimensions_op_t;

typedef struct
{
    UWORD32                                     u4_size;
    IVD_API_COMMAND_TYPE_T                      e_cmd;
    IVD_CONTROL_API_COMMAND_TYPE_T              e_sub_cmd;
}ih264d_ctl_get_vui_params_ip_t;

typedef struct
{
    UWORD32                                     u4_size;
    UWORD32                                     u4_error_code;
    UWORD8                                      u1_aspect_ratio_idc;
    UWORD16                                     u2_sar_width;
    UWORD16                                     u2_sar_height;
    UWORD8                                      u1_overscan_appropriate_flag;
    UWORD8                                      u1_video_format;
    UWORD8                                      u1_video_full_range_flag;
    UWORD8                                      u1_colour_primaries;
    UWORD8                                      u1_tfr_chars;
    UWORD8                                      u1_matrix_coeffs;
    UWORD8                                      u1_cr_top_field;
    UWORD8                                      u1_cr_bottom_field;
    UWORD32                                     u4_num_units_in_tick;
    UWORD32                                     u4_time_scale;
    UWORD8                                      u1_fixed_frame_rate_flag;
    UWORD8                                      u1_nal_hrd_params_present;
    UWORD8                                      u1_vcl_hrd_params_present;
    UWORD8                                      u1_low_delay_hrd_flag;
    UWORD8                                      u1_pic_struct_present_flag;
    UWORD8                                      u1_bitstream_restriction_flag;
    UWORD8                                      u1_mv_over_pic_boundaries_flag;
    UWORD32                                     u4_max_bytes_per_pic_denom;
    UWORD32                                     u4_max_bits_per_mb_denom;
    UWORD32                                     u4_log2_max_mv_length_horz;
    UWORD32                                     u4_log2_max_mv_length_vert;
    UWORD32                                     u4_num_reorder_frames;
    UWORD32                                     u4_max_dec_frame_buffering;
}ih264d_ctl_get_vui_params_op_t;


typedef struct
{
    /**
     * u4_size
     */
    UWORD32                                     u4_size;

    /**
     * cmd
     */
    IVD_API_COMMAND_TYPE_T                      e_cmd;

    /**
     * sub_cmd
     */
    IVD_CONTROL_API_COMMAND_TYPE_T              e_sub_cmd;
}ih264d_ctl_get_sei_mdcv_params_ip_t;

typedef struct
{
    /**
     * u4_size
     */
    UWORD32                                     u4_size;

    /**
     * error_code
     */
    UWORD32                                     u4_error_code;

    /**
     * Array to store the display_primaries_x values
     */
    UWORD16                                     au2_display_primaries_x[NUM_SEI_MDCV_PRIMARIES];

    /**
     * Array to store the display_primaries_y values
     */
    UWORD16                                     au2_display_primaries_y[NUM_SEI_MDCV_PRIMARIES];

    /**
     * Variable to store the white point x value
     */
    UWORD16                                     u2_white_point_x;

    /**
     * Variable to store the white point y value
     */
    UWORD16                                     u2_white_point_y;

    /**
     * Variable to store the max display mastering luminance value
     */
    UWORD32                                     u4_max_display_mastering_luminance;

    /**
     * Variable to store the min display mastering luminance value
     */
    UWORD32                                     u4_min_display_mastering_luminance;
}ih264d_ctl_get_sei_mdcv_params_op_t;

typedef struct
{
    /**
     * u4_size
     */
    UWORD32                                     u4_size;

    /**
     * cmd
     */
    IVD_API_COMMAND_TYPE_T                      e_cmd;

    /**
     * sub_cmd
     */
    IVD_CONTROL_API_COMMAND_TYPE_T              e_sub_cmd;
}ih264d_ctl_get_sei_cll_params_ip_t;

typedef struct
{
    /**
     * u4_size
     */
    UWORD32                                     u4_size;

    /**
     * error_code
     */
    UWORD32                                     u4_error_code;

    /**
     * The maximum pixel intensity of all samples
     */
    UWORD16                                     u2_max_content_light_level;

    /**
     * The average pixel intensity of all samples
     */
    UWORD16                                     u2_max_pic_average_light_level;
} ih264d_ctl_get_sei_cll_params_op_t;

typedef struct
{
    /**
     * u4_size
     */
    UWORD32                                     u4_size;

    /**
     * cmd
     */
    IVD_API_COMMAND_TYPE_T                      e_cmd;

    /**
     * sub_cmd
     */
    IVD_CONTROL_API_COMMAND_TYPE_T              e_sub_cmd;
}ih264d_ctl_get_sei_ave_params_ip_t;

typedef struct
{
    /**
     * u4_size
     */
    UWORD32                                     u4_size;

    /**
     * error_code
     */
    UWORD32                                     u4_error_code;

    /**
     * specifies the environmental illluminance of the ambient viewing environment
     */
    UWORD32                                     u4_ambient_illuminance;

    /*
     * specify the normalized x chromaticity coordinates of the
     * environmental ambient light in the nominal viewing environment
     */
    UWORD16                                     u2_ambient_light_x;

    /*
     * specify the normalized y chromaticity coordinates of the
     * environmental ambient light in the nominal viewing environment
     */
    UWORD16                                     u2_ambient_light_y;
} ih264d_ctl_get_sei_ave_params_op_t;

typedef struct
{
    /**
     * u4_size
     */
    UWORD32                                     u4_size;

    /**
     * cmd
     */
    IVD_API_COMMAND_TYPE_T                      e_cmd;

    /**
     * sub_cmd
     */
    IVD_CONTROL_API_COMMAND_TYPE_T              e_sub_cmd;
}ih264d_ctl_get_sei_ccv_params_ip_t;

typedef struct
{
    /**
     * u4_size
     */
    UWORD32                                     u4_size;

    /**
     * error_code
     */
    UWORD32                                     u4_error_code;

    /*
     * Flag used to control persistence of CCV SEI messages
     */
    UWORD8                                      u1_ccv_cancel_flag;

    /*
     * specifies the persistence of the CCV SEI message for the current layer
     */
    UWORD8                                      u1_ccv_persistence_flag;

    /*
     * specifies the presence of syntax elements ccv_primaries_x and ccv_primaries_y
     */
    UWORD8                                      u1_ccv_primaries_present_flag;

    /*
     * specifies that the syntax element ccv_min_luminance_value is present
     */
    UWORD8                                      u1_ccv_min_luminance_value_present_flag;

    /*
     * specifies that the syntax element ccv_max_luminance_value is present
     */
    UWORD8                                      u1_ccv_max_luminance_value_present_flag;

    /*
     * specifies that the syntax element ccv_avg_luminance_value is present
     */
    UWORD8                                      u1_ccv_avg_luminance_value_present_flag;

    /*
     * shall be equal to 0 in bitstreams conforming to this version. Other values
     * for reserved_zero_2bits are reserved for future use
     */
    UWORD8                                      u1_ccv_reserved_zero_2bits;

    /*
     * specify the normalized x chromaticity coordinates of the colour
     * primary component c of the nominal content colour volume
     */
    WORD32                                      ai4_ccv_primaries_x[NUM_SEI_CCV_PRIMARIES];

    /*
     * specify the normalized y chromaticity coordinates of the colour
     * primary component c of the nominal content colour volume
     */
    WORD32                                      ai4_ccv_primaries_y[NUM_SEI_CCV_PRIMARIES];

    /*
     * specifies the normalized minimum luminance value
     */
    UWORD32                                     u4_ccv_min_luminance_value;

    /*
     * specifies the normalized maximum luminance value
     */
    UWORD32                                     u4_ccv_max_luminance_value;

    /*
     * specifies the normalized average luminance value
     */
    UWORD32                                     u4_ccv_avg_luminance_value;
} ih264d_ctl_get_sei_ccv_params_op_t;


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif
#endif /* _IH264D_H_ */
