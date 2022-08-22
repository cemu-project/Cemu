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
*  ivd.h
*
* @brief
*  This file contains all the necessary structure and  enumeration
* definitions needed for the Application  Program Interface(API) of the
* Ittiam Video Decoders
*
* @author
*  100239(RCY)
*
* @remarks
*  None
*
*******************************************************************************
*/

#ifndef _IVD_H
#define _IVD_H

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define IVD_VIDDEC_MAX_IO_BUFFERS 64

/** SEI macros */
/*
 * @brief  specifies the number of colour primary components of the mastering display
 */
#define NUM_SEI_MDCV_PRIMARIES        3

/*
 * @brief  specifies the number of colour primary components of the nominal content colour volume
 */
#define NUM_SEI_CCV_PRIMARIES         3

/*****************************************************************************/
/* Typedefs                                                                  */
/*****************************************************************************/

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/

/* IVD_ARCH_T: Architecture Enumeration                               */
typedef enum
{
    ARCH_NA                 =   0x7FFFFFFF,
    ARCH_ARM_NONEON         =   0x0,
    ARCH_ARM_A9Q,
    ARCH_ARM_A9A,
    ARCH_ARM_A9,
    ARCH_ARM_A7,
    ARCH_ARM_A5,
    ARCH_ARM_A15,
    ARCH_ARM_NEONINTR,
    ARCH_ARMV8_GENERIC,
    ARCH_X86_GENERIC        =   0x100,
    ARCH_X86_SSSE3,
    ARCH_X86_SSE42,
    ARCH_X86_AVX2,
    ARCH_MIPS_GENERIC       =   0x200,
    ARCH_MIPS_32
}IVD_ARCH_T;

/* IVD_SOC_T: SOC Enumeration                               */
typedef enum
{
    SOC_NA                  = 0x7FFFFFFF,
    SOC_GENERIC             = 0x0,
    SOC_HISI_37X            = 0x100,
}IVD_SOC_T;

/* IVD_FRAME_SKIP_MODE_T:Skip mode Enumeration                               */

typedef enum {
    IVD_SKIP_NONE                               = 0x7FFFFFFF,
    IVD_SKIP_P                                  = 0x1,
    IVD_SKIP_B                                  = 0x2,
    IVD_SKIP_I                                  = 0x3,
    IVD_SKIP_IP                                 = 0x4,
    IVD_SKIP_IB                                 = 0x5,
    IVD_SKIP_PB                                 = 0x6,
    IVD_SKIP_IPB                                = 0x7,
    IVD_SKIP_IDR                                = 0x8,
    IVD_SKIP_DEFAULT                            = IVD_SKIP_NONE,
}IVD_FRAME_SKIP_MODE_T;

/* IVD_VIDEO_DECODE_MODE_T: Set decoder to decode either frame worth of data */
/* or only header worth of data                                              */

typedef enum {
    IVD_DECODE_MODE_NA                          = 0x7FFFFFFF,

    /* This enables the codec to process all decodable units */
    IVD_DECODE_FRAME                            = 0x0,

    /* This enables the codec to decode header only */
    IVD_DECODE_HEADER                           = 0x1,



}IVD_VIDEO_DECODE_MODE_T;


/* IVD_DISPLAY_FRAME_OUT_MODE_T: Video Display Frame Output Mode             */

typedef enum {

    IVD_DISPLAY_ORDER_NA                        = 0x7FFFFFFF,
    /* To set codec to fill output buffers in display order */
    IVD_DISPLAY_FRAME_OUT                       = 0x0,

    /* To set codec to fill output buffers in decode order */
    IVD_DECODE_FRAME_OUT                        = 0x1,
}IVD_DISPLAY_FRAME_OUT_MODE_T;


/* IVD_API_COMMAND_TYPE_T:API command type                                   */
typedef enum {
    IVD_CMD_VIDEO_NA                          = 0x7FFFFFFF,
    IVD_CMD_CREATE                            = IV_CMD_DUMMY_ELEMENT + 1,
    IVD_CMD_DELETE,
    IVD_CMD_VIDEO_CTL,
    IVD_CMD_VIDEO_DECODE,
    IVD_CMD_GET_DISPLAY_FRAME,
    IVD_CMD_REL_DISPLAY_FRAME,
    IVD_CMD_SET_DISPLAY_FRAME
}IVD_API_COMMAND_TYPE_T;

/* IVD_CONTROL_API_COMMAND_TYPE_T: Video Control API command type            */

typedef enum {
    IVD_CMD_NA                          = 0x7FFFFFFF,
    IVD_CMD_CTL_GETPARAMS               = 0x0,
    IVD_CMD_CTL_SETPARAMS               = 0x1,
    IVD_CMD_CTL_RESET                   = 0x2,
    IVD_CMD_CTL_SETDEFAULT              = 0x3,
    IVD_CMD_CTL_FLUSH                   = 0x4,
    IVD_CMD_CTL_GETBUFINFO              = 0x5,
    IVD_CMD_CTL_GETVERSION              = 0x6,
    IVD_CMD_CTL_CODEC_SUBCMD_START         = 0x7
}IVD_CONTROL_API_COMMAND_TYPE_T;


/* IVD_ERROR_BITS_T: A UWORD32 container will be used for reporting the error*/
/* code to the application. The first 8 bits starting from LSB have been     */
/* reserved for the codec to report internal error details. The rest of the  */
/* bits will be generic for all video decoders and each bit has an associated*/
/* meaning as mentioned below. The unused bit fields are reserved for future */
/* extenstions and will be zero in the current implementation                */

typedef enum {
    /* Bit 8  - Applied concealment.                                         */
    IVD_APPLIEDCONCEALMENT                      = 0x8,
    /* Bit 9 - Insufficient input data.                                     */
    IVD_INSUFFICIENTDATA                        = 0x9,
    /* Bit 10 - Data problem/corruption.                                     */
    IVD_CORRUPTEDDATA                           = 0xa,
    /* Bit 11 - Header problem/corruption.                                   */
    IVD_CORRUPTEDHEADER                         = 0xb,
    /* Bit 12 - Unsupported feature/parameter in input.                      */
    IVD_UNSUPPORTEDINPUT                        = 0xc,
    /* Bit 13 - Unsupported input parameter orconfiguration.                 */
    IVD_UNSUPPORTEDPARAM                        = 0xd,
    /* Bit 14 - Fatal error (stop the codec).If there is an                  */
    /* error and this bit is not set, the error is a recoverable one.        */
    IVD_FATALERROR                              = 0xe,
    /* Bit 15 - Invalid bitstream. Applies when Bitstream/YUV frame          */
    /* buffer for encode/decode call is made with non-valid or zero u4_size  */
    /* data                                                                  */
    IVD_INVALID_BITSTREAM                       = 0xf,
    /* Bit 16          */
    IVD_INCOMPLETE_BITSTREAM                    = 0x10,
    IVD_ERROR_BITS_T_DUMMY_ELEMENT              = 0x7FFFFFFF
}IVD_ERROR_BITS_T;


/* IVD_CONTROL_API_COMMAND_TYPE_T: Video Control API command type            */
typedef enum {
    IVD_ERROR_NONE                              = 0x0,
    IVD_NUM_MEM_REC_FAILED                      = 0x1,
    IVD_NUM_REC_NOT_SUFFICIENT                  = 0x2,
    IVD_FILL_MEM_REC_FAILED                     = 0x3,
    IVD_REQUESTED_WIDTH_NOT_SUPPPORTED          = 0x4,
    IVD_REQUESTED_HEIGHT_NOT_SUPPPORTED         = 0x5,
    IVD_INIT_DEC_FAILED                         = 0x6,
    IVD_INIT_DEC_NOT_SUFFICIENT                 = 0x7,
    IVD_INIT_DEC_WIDTH_NOT_SUPPPORTED           = 0x8,
    IVD_INIT_DEC_HEIGHT_NOT_SUPPPORTED          = 0x9,
    IVD_INIT_DEC_MEM_NOT_ALIGNED                = 0xa,
    IVD_INIT_DEC_COL_FMT_NOT_SUPPORTED          = 0xb,
    IVD_INIT_DEC_MEM_REC_NOT_SUFFICIENT         = 0xc,
    IVD_GET_VERSION_DATABUFFER_SZ_INSUFFICIENT  = 0xd,
    IVD_BUFFER_SIZE_SET_TO_ZERO                 = 0xe,
    IVD_UNEXPECTED_END_OF_STREAM                = 0xf,
    IVD_SEQUENCE_HEADER_NOT_DECODED             = 0x10,
    IVD_STREAM_WIDTH_HEIGHT_NOT_SUPPORTED       = 0x11,
    IVD_MAX_FRAME_LIMIT_REACHED                 = 0x12,
    IVD_IP_API_STRUCT_SIZE_INCORRECT            = 0x13,
    IVD_OP_API_STRUCT_SIZE_INCORRECT            = 0x14,
    IVD_HANDLE_NULL                             = 0x15,
    IVD_HANDLE_STRUCT_SIZE_INCORRECT            = 0x16,
    IVD_INVALID_HANDLE_NULL                     = 0x17,
    IVD_INVALID_API_CMD                         = 0x18,
    IVD_UNSUPPORTED_API_CMD                     = 0x19,
    IVD_MEM_REC_STRUCT_SIZE_INCORRECT           = 0x1a,
    IVD_DISP_FRM_ZERO_OP_BUFS                   = 0x1b,
    IVD_DISP_FRM_OP_BUF_NULL                    = 0x1c,
    IVD_DISP_FRM_ZERO_OP_BUF_SIZE               = 0x1d,
    IVD_DEC_FRM_BS_BUF_NULL                     = 0x1e,
    IVD_SET_CONFG_INVALID_DEC_MODE              = 0x1f,
    IVD_SET_CONFG_UNSUPPORTED_DISP_WIDTH        = 0x20,
    IVD_RESET_FAILED                            = 0x21,
    IVD_INIT_DEC_MEM_REC_OVERLAP_ERR            = 0x22,
    IVD_INIT_DEC_MEM_REC_BASE_NULL              = 0x23,
    IVD_INIT_DEC_MEM_REC_ALIGNMENT_ERR          = 0x24,
    IVD_INIT_DEC_MEM_REC_INSUFFICIENT_SIZE      = 0x25,
    IVD_INIT_DEC_MEM_REC_INCORRECT_TYPE         = 0x26,
    IVD_DEC_NUMBYTES_INV                        = 0x27,
    IVD_DEC_REF_BUF_NULL                        = 0x28,
    IVD_DEC_FRM_SKIPPED                         = 0x29,
    IVD_RES_CHANGED                             = 0x2a,
    IVD_MEM_ALLOC_FAILED                        = 0x2b,
    IVD_DUMMY_ELEMENT_FOR_CODEC_EXTENSIONS      = 0xD0,
}IVD_ERROR_CODES_T;


/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/
/* structure for passing output buffers to codec during get display buffer   */
/* call                                                                      */
typedef struct {

    /**
     * number of output buffers
     */
    UWORD32             u4_num_bufs;

    /**
     *list of pointers to output buffers
     */
    UWORD8              *pu1_bufs[IVD_VIDDEC_MAX_IO_BUFFERS];

    /**
     * sizes of each output buffer
     */
    UWORD32             u4_min_out_buf_size[IVD_VIDDEC_MAX_IO_BUFFERS];

}ivd_out_bufdesc_t;

/*****************************************************************************/
/*   Create decoder                                                          */
/*****************************************************************************/

/* IVD_API_COMMAND_TYPE_T::e_cmd = IVD_CMD_CREATE                            */


typedef struct {
    /**
     * u4_size of the structure
     */
    UWORD32                                 u4_size;

    /**
     *  e_cmd
     */
    IVD_API_COMMAND_TYPE_T                  e_cmd;

    /**
     * format in which codec has to give out frame data for display
     */
    IV_COLOR_FORMAT_T                       e_output_format;

    /**
     * Flag to indicate shared display buffer mode
     */
    UWORD32                                 u4_share_disp_buf;

    /**
     * Pointer to a function for aligned allocation.
     */
    void    *(*pf_aligned_alloc)(void *pv_mem_ctxt, WORD32 alignment, WORD32 size);

    /**
     * Pointer to a function for aligned free.
     */
    void   (*pf_aligned_free)(void *pv_mem_ctxt, void *pv_buf);

    /**
     * Pointer to memory context that is needed during alloc/free for custom
     * memory managers. This will be passed as first argument to pf_aligned_alloc and
     * pf_aligned_free.
     * If application is using standard memory functions like
     * malloc/aligned_malloc/memalign/free/aligned_free,
     * then this is not needed and can be set to NULL
     */
    void    *pv_mem_ctxt;

}ivd_create_ip_t;


typedef struct{
    /**
     * u4_size of the structure
     */
    UWORD32                                 u4_size;

    /**
     * u4_error_code
     */
    UWORD32                                 u4_error_code;

    /**
     * Codec Handle
     */
    void                                    *pv_handle;

}ivd_create_op_t;


/*****************************************************************************/
/*  Delete decoder                                                           */
/*****************************************************************************/

/* IVD_API_COMMAND_TYPE_T::e_cmd = IVD_CMD_DELETE                              */



typedef struct {
    /**
     * u4_size of the structure
     */
    UWORD32                                     u4_size;

    /**
     * cmd
     */
    IVD_API_COMMAND_TYPE_T                       e_cmd;

}ivd_delete_ip_t;


typedef struct{
    /**
     * u4_size of the structure
     */
    UWORD32                                     u4_size;

    /**
     * error_code
     */
    UWORD32                                     u4_error_code;

}ivd_delete_op_t;

/*****************************************************************************/
/*   Video Decode                                                            */
/*****************************************************************************/

/* SEI params deocde */
typedef struct {
    UWORD8                                         u1_sei_mdcv_params_present_flag;

    UWORD8                                         u1_sei_cll_params_present_flag;

    UWORD8                                         u1_sei_ave_params_present_flag;

    UWORD8                                         u1_sei_ccv_params_present_flag;

}ivd_sei_decode_op_t;

/* IVD_API_COMMAND_TYPE_T::e_cmd = IVD_CMD_VIDEO_DECODE                      */


typedef struct {
    /**
     * u4_size of the structure
     */
    UWORD32                                 u4_size;

    /**
     * e_cmd
     */
    IVD_API_COMMAND_TYPE_T                  e_cmd;

    /**
     * u4_ts
     */
    UWORD32                                 u4_ts;

    /**
     * u4_num_Bytes
     */
    UWORD32                                 u4_num_Bytes;

    /**
     * pv_stream_buffer
     */
    void                                    *pv_stream_buffer;

    /**
     * output buffer desc
     */
    ivd_out_bufdesc_t                       s_out_buffer;

}ivd_video_decode_ip_t;


typedef struct{
    /**
     * u4_size of the structure
     */
    UWORD32                                 u4_size;

    /**
     * u4_error_code
     */
    UWORD32                                 u4_error_code;

    /**
     * num_bytes_consumed
     */
    UWORD32                                 u4_num_bytes_consumed;

    /**
     * pic_wd
     */
    UWORD32                                 u4_pic_wd;

    /**
     * pic_ht
     */
    UWORD32                                 u4_pic_ht;

    /**
     * pic_type
     */
    IV_PICTURE_CODING_TYPE_T                e_pic_type;

    /**
     * frame_decoded_flag
     */
    UWORD32                                 u4_frame_decoded_flag;

    /**
     * new_seq
     */
    UWORD32                                 u4_new_seq;

    /**
     * output_present
     */
    UWORD32                                 u4_output_present;

    /**
     * progressive_frame_flag
     */
    UWORD32                                 u4_progressive_frame_flag;

    /**
     * is_ref_flag
     */
    UWORD32                                 u4_is_ref_flag;

    /**
     * output_format
     */
    IV_COLOR_FORMAT_T                       e_output_format;

    /**
     * disp_frm_buf
     */
    iv_yuv_buf_t                            s_disp_frm_buf;

    /**
     * sei params o/p struct
     */
    ivd_sei_decode_op_t                     s_sei_decode_op;

    /**
     * fld_type
     */
    IV_FLD_TYPE_T                           e4_fld_type;

    /**
     * ts
     */
    UWORD32                                 u4_ts;

    /**
     * disp_buf_id
     */
    UWORD32                                 u4_disp_buf_id;

    /**
     * reorder_depth
     */
    WORD32                                  i4_reorder_depth;

    /**
     * disp_buf_id
     */
    WORD32                                  i4_display_index;

    /* crop info from SPS */
	UWORD8                                   u1_frame_cropping_flag;
	UWORD8                                   u1_frame_cropping_rect_left_ofst;
	UWORD8                                   u1_frame_cropping_rect_right_ofst;
	UWORD8                                   u1_frame_cropping_rect_top_ofst;
	UWORD8                                   u1_frame_cropping_rect_bottom_ofst;

    /* uncropped image size */
	UWORD32                                  u4_raw_wd;
	UWORD32                                  u4_raw_ht;

}ivd_video_decode_op_t;


/*****************************************************************************/
/*   Get Display Frame                                                       */
/*****************************************************************************/


/* IVD_API_COMMAND_TYPE_T::e_cmd = IVD_CMD_GET_DISPLAY_FRAME                 */

typedef struct
{
    /**
     * u4_size of the structure
     */
    UWORD32                                 u4_size;

    /**
     * e_cmd
     */
    IVD_API_COMMAND_TYPE_T                  e_cmd;

    /**
     * output buffer desc
     */
    ivd_out_bufdesc_t                       s_out_buffer;

}ivd_get_display_frame_ip_t;


typedef struct
{
    /**
     * u4_size of the structure
     */
    UWORD32                                 u4_size;

    /**
     * error_code
     */
    UWORD32                                 u4_error_code;

    /**
     * progressive_frame_flag
     */
    UWORD32                                 u4_progressive_frame_flag;

    /**
     * pic_type
     */
    IV_PICTURE_CODING_TYPE_T                e_pic_type;

    /**
     * is_ref_flag
     */
    UWORD32                                 u4_is_ref_flag;

    /**
     * output_format
     */
    IV_COLOR_FORMAT_T                       e_output_format;

    /**
     * disp_frm_buf
     */
    iv_yuv_buf_t                            s_disp_frm_buf;

    /**
     * fld_type
     */
    IV_FLD_TYPE_T                           e4_fld_type;

    /**
     * ts
     */
    UWORD32                                 u4_ts;

    /**
     * disp_buf_id
     */
    UWORD32                                 u4_disp_buf_id;
}ivd_get_display_frame_op_t;

/*****************************************************************************/
/*   Set Display Frame                                                       */
/*****************************************************************************/


/* IVD_API_COMMAND_TYPE_T::e_cmd = IVD_CMD_SET_DISPLAY_FRAME                 */

typedef struct
{
    /**
     * u4_size of the structure
     */
    UWORD32                                 u4_size;

    /**
     * cmd
     */
    IVD_API_COMMAND_TYPE_T                  e_cmd;

    /**
     * num_disp_bufs
     */
    UWORD32                                 num_disp_bufs;

    /**
     * output buffer desc
     */
    ivd_out_bufdesc_t                       s_disp_buffer[IVD_VIDDEC_MAX_IO_BUFFERS];

}ivd_set_display_frame_ip_t;


typedef struct
{
    /**
     * u4_size of the structure
     */
    UWORD32                                 u4_size;

    /**
     * error code
     */
    UWORD32                                 u4_error_code;
}ivd_set_display_frame_op_t;


/*****************************************************************************/
/*   Release Display Frame                                                       */
/*****************************************************************************/


/* IVD_API_COMMAND_TYPE_T::e_cmd = IVD_CMD_SET_DISPLAY_FRAME                 */

typedef struct
{
    /**
     * u4_size of the structure
     */
    UWORD32                                 u4_size;

    /**
     * e_cmd
     */
    IVD_API_COMMAND_TYPE_T                  e_cmd;

    /**
     * disp_buf_id
     */
    UWORD32                                 u4_disp_buf_id;
}ivd_rel_display_frame_ip_t;


typedef struct
{
    /**
     * u4_size of the structure
     */
    UWORD32                                 u4_size;

    /**
     * error code
     */
    UWORD32                                 u4_error_code;
}ivd_rel_display_frame_op_t;

/*****************************************************************************/
/*   Video control  Flush                                                    */
/*****************************************************************************/
/* IVD_API_COMMAND_TYPE_T::e_cmd            = IVD_CMD_VIDEO_CTL              */
/* IVD_CONTROL_API_COMMAND_TYPE_T::e_sub_cmd    = IVD_CMD_ctl_FLUSH          */



typedef struct{
    /**
     * u4_size of the structure
     */
    UWORD32                                 u4_size;

    /**
     * cmd
     */
    IVD_API_COMMAND_TYPE_T                  e_cmd;

    /**
     * sub_cmd
     */
    IVD_CONTROL_API_COMMAND_TYPE_T          e_sub_cmd;
}ivd_ctl_flush_ip_t;


typedef struct{
    /**
     * u4_size of the structure
     */
    UWORD32                                 u4_size;

    /**
     * error code
     */
    UWORD32                                 u4_error_code;
}ivd_ctl_flush_op_t;

/*****************************************************************************/
/*   Video control reset                                                     */
/*****************************************************************************/
/* IVD_API_COMMAND_TYPE_T::e_cmd            = IVD_CMD_VIDEO_CTL              */
/* IVD_CONTROL_API_COMMAND_TYPE_T::e_sub_cmd    = IVD_CMD_ctl_RESET          */


typedef struct{
    /**
     * u4_size of the structure
     */
    UWORD32                                 u4_size;

    /**
     * cmd
     */
    IVD_API_COMMAND_TYPE_T                  e_cmd;

    /**
     * sub_cmd
     */

    IVD_CONTROL_API_COMMAND_TYPE_T          e_sub_cmd;
}ivd_ctl_reset_ip_t;


typedef struct{
    /**
     * u4_size of the structure
     */
    UWORD32                                 u4_size;

    /**
     * error code
     */
    UWORD32                                 u4_error_code;
}ivd_ctl_reset_op_t;


/*****************************************************************************/
/*   Video control  Set Params                                               */
/*****************************************************************************/
/* IVD_API_COMMAND_TYPE_T::e_cmd        = IVD_CMD_VIDEO_CTL                  */
/* IVD_CONTROL_API_COMMAND_TYPE_T::e_sub_cmd=IVD_CMD_ctl_SETPARAMS           */
/* IVD_CONTROL_API_COMMAND_TYPE_T::e_sub_cmd=IVD_CMD_ctl_SETDEFAULT          */



typedef struct {
    /**
     * u4_size of the structure
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
     * vid_dec_mode
     */
    IVD_VIDEO_DECODE_MODE_T                     e_vid_dec_mode;

    /**
     * disp_wd
     */
    UWORD32                                     u4_disp_wd;

    /**
     * frm_skip_mode
     */
    IVD_FRAME_SKIP_MODE_T                       e_frm_skip_mode;

    /**
     * frm_out_mode
     */
    IVD_DISPLAY_FRAME_OUT_MODE_T                e_frm_out_mode;
}ivd_ctl_set_config_ip_t;


typedef struct{
    /**
     * u4_size of the structure
     */
    UWORD32                                     u4_size;

    /**
     * u4_error_code
     */
    UWORD32                                     u4_error_code;
}ivd_ctl_set_config_op_t;

/*****************************************************************************/
/*   Video control:Get Buf Info                                              */
/*****************************************************************************/

/* IVD_API_COMMAND_TYPE_T::e_cmd         = IVD_CMD_VIDEO_CTL                 */
/* IVD_CONTROL_API_COMMAND_TYPE_T::e_sub_cmd=IVD_CMD_ctl_GETBUFINFO          */


typedef struct{
    /**
     * u4_size of the structure
     */
    UWORD32                                     u4_size;

    /**
     *  e_cmd
     */
    IVD_API_COMMAND_TYPE_T                      e_cmd;

    /**
     * sub_cmd
     */
    IVD_CONTROL_API_COMMAND_TYPE_T              e_sub_cmd;
}ivd_ctl_getbufinfo_ip_t;


typedef struct{
    /**
     * u4_size of the structure
     */
    UWORD32                                     u4_size;

    /**
     * error code
     */
    UWORD32                                     u4_error_code;

    /**
     * no of display buffer sets required by codec
     */
    UWORD32                                     u4_num_disp_bufs;

    /**
     * no of input buffers required for codec
     */
    UWORD32                                     u4_min_num_in_bufs;

    /**
     * no of output buffers required for codec
     */
    UWORD32                                     u4_min_num_out_bufs;

    /**
     * sizes of each input buffer required
     */
    UWORD32                                     u4_min_in_buf_size[IVD_VIDDEC_MAX_IO_BUFFERS];

    /**
     * sizes of each output buffer required
     */
    UWORD32                                     u4_min_out_buf_size[IVD_VIDDEC_MAX_IO_BUFFERS];
}ivd_ctl_getbufinfo_op_t;


/*****************************************************************************/
/*   Video control:Getstatus Call                                            */
/*****************************************************************************/


/* IVD_API_COMMAND_TYPE_T::e_cmd        = IVD_CMD_VIDEO_CTL                  */
/* IVD_CONTROL_API_COMMAND_TYPE_T::e_sub_cmd=IVD_CMD_ctl_GETPARAMS           */


typedef struct{
    /**
     * u4_size of the structure
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
}ivd_ctl_getstatus_ip_t;


typedef struct{

     /**
      * u4_size of the structure
      */
    UWORD32                  u4_size;

    /**
      * error code
      */
    UWORD32                  u4_error_code;

    /**
     * no of display buffer sets required by codec
     */
    UWORD32                  u4_num_disp_bufs;

    /**
     * u4_pic_ht
     */
    UWORD32                  u4_pic_ht;

    /**
     * u4_pic_wd
     */
    UWORD32                  u4_pic_wd;

    /**
     * frame_rate
     */
    UWORD32                  u4_frame_rate;

    /**
     * u4_bit_rate
     */
    UWORD32                  u4_bit_rate;

    /**
     * content_type
     */
    IV_CONTENT_TYPE_T        e_content_type;

    /**
     * output_chroma_format
     */
    IV_COLOR_FORMAT_T        e_output_chroma_format;

    /**
     * no of input buffers required for codec
     */
    UWORD32                  u4_min_num_in_bufs;

    /**
     * no of output buffers required for codec
     */
    UWORD32                  u4_min_num_out_bufs;

    /**
     * sizes of each input buffer required
     */
    UWORD32                  u4_min_in_buf_size[IVD_VIDDEC_MAX_IO_BUFFERS];

    /**
     * sizes of each output buffer required
     */
    UWORD32                  u4_min_out_buf_size[IVD_VIDDEC_MAX_IO_BUFFERS];
}ivd_ctl_getstatus_op_t;


/*****************************************************************************/
/*   Video control:Get Version Info                                          */
/*****************************************************************************/

/* IVD_API_COMMAND_TYPE_T::e_cmd        = IVD_CMD_VIDEO_CTL                  */
/* IVD_CONTROL_API_COMMAND_TYPE_T::e_sub_cmd=IVD_CMD_ctl_GETVERSION          */


typedef struct{
    /**
     * u4_size of the structure
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
     * pv_version_buffer
     */
    void                                        *pv_version_buffer;

    /**
     * version_buffer_size
     */
    UWORD32                                     u4_version_buffer_size;
}ivd_ctl_getversioninfo_ip_t;


typedef struct{
    /**
     * u4_size of the structure
     */
    UWORD32                                     u4_size;

    /**
     * error code
     */
    UWORD32                                     u4_error_code;
}ivd_ctl_getversioninfo_op_t;

#endif /* __IVD_H__ */

