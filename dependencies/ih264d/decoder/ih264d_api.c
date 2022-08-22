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
 * Modified for use with Cemu emulator project
 */

/*****************************************************************************/
/*                                                                           */
/*  File Name         : ih264d_api.c                                         */
/*                                                                           */
/*  Description       : Has all  API related functions                       */
/*                                                                           */
/*                                                                           */
/*  List of Functions : api_check_struct_sanity                              */
/*          ih264d_set_processor                                             */
/*          ih264d_create                                                    */
/*          ih264d_delete                                                    */
/*          ih264d_init                                                      */
/*          ih264d_map_error                                                 */
/*          ih264d_video_decode                                              */
/*          ih264d_get_version                                               */
/*          ih264d_get_display_frame                                         */
/*          ih264d_set_display_frame                                         */
/*          ih264d_set_flush_mode                                            */
/*          ih264d_get_status                                                */
/*          ih264d_get_buf_info                                              */
/*          ih264d_set_params                                                */
/*          ih264d_set_default_params                                        */
/*          ih264d_reset                                                     */
/*          ih264d_ctl                                                       */
/*          ih264d_rel_display_frame                                         */
/*          ih264d_set_degrade                                               */
/*          ih264d_get_frame_dimensions                                      */
/*          ih264d_set_num_cores                                             */
/*          ih264d_fill_output_struct_from_context                           */
/*          ih264d_api_function                                              */
/*                                                                           */
/*  Issues / Problems : None                                                 */
/*                                                                           */
/*  Revision History  :                                                      */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         14 10 2008   100356(SKV)     Draft                                */
/*                                                                           */
/*****************************************************************************/
#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264d_tables.h"
#include "iv.h"
#include "ivd.h"
#include "ih264d.h"
#include "ih264d_defs.h"

#include <string.h>
#include <limits.h>
#include <stddef.h>

#include "ih264d_inter_pred.h"

#include "ih264d_structs.h"
#include "ih264d_nal.h"
#include "ih264d_error_handler.h"

#include "ih264d_defs.h"

#include "ithread.h"
#include "ih264d_parse_slice.h"
#include "ih264d_function_selector.h"
#include "ih264_error.h"
#include "ih264_disp_mgr.h"
#include "ih264_buf_mgr.h"
#include "ih264d_deblocking.h"
#include "ih264d_parse_cavlc.h"
#include "ih264d_parse_cabac.h"
#include "ih264d_utils.h"
#include "ih264d_format_conv.h"
#include "ih264d_parse_headers.h"
#include "ih264d_thread_compute_bs.h"
#include <assert.h>


/*********************/
/* Codec Versioning  */
/*********************/
//Move this to where it is used
#define CODEC_NAME              "H264VDEC"
#define CODEC_RELEASE_TYPE      "production"
#define CODEC_RELEASE_VER       "05.00"
#define CODEC_VENDOR            "ITTIAM"
#define MAXVERSION_STRLEN       511
#ifdef ANDROID
#define VERSION(version_string, codec_name, codec_release_type, codec_release_ver, codec_vendor)    \
    snprintf(version_string, MAXVERSION_STRLEN,                                                     \
             "@(#)Id:%s_%s Ver:%s Released by %s",                                                  \
             codec_name, codec_release_type, codec_release_ver, codec_vendor)
#else
#define VERSION(version_string, codec_name, codec_release_type, codec_release_ver, codec_vendor)    \
    snprintf(version_string, MAXVERSION_STRLEN,                                                     \
             "@(#)Id:%s_%s Ver:%s Released by %s Build: %s @ %s",                                   \
             codec_name, codec_release_type, codec_release_ver, codec_vendor, __DATE__, __TIME__)
#endif


#define MIN_IN_BUFS             1
#define MIN_OUT_BUFS_420        3
#define MIN_OUT_BUFS_422ILE     1
#define MIN_OUT_BUFS_RGB565     1
#define MIN_OUT_BUFS_420SP      2

#define NUM_FRAMES_LIMIT_ENABLED 0

#if NUM_FRAMES_LIMIT_ENABLED
#define NUM_FRAMES_LIMIT 10000
#else
#define NUM_FRAMES_LIMIT 0x7FFFFFFF
#endif


UWORD32 ih264d_get_extra_mem_external(UWORD32 width, UWORD32 height);
WORD32 ih264d_get_frame_dimensions(iv_obj_t *dec_hdl,
                                   void *pv_api_ip,
                                   void *pv_api_op);
WORD32 ih264d_get_vui_params(iv_obj_t *dec_hdl,
                             void *pv_api_ip,
                             void *pv_api_op);

WORD32 ih264d_get_sei_mdcv_params(iv_obj_t *dec_hdl,
                                  void *pv_api_ip,
                                  void *pv_api_op);

WORD32 ih264d_get_sei_cll_params(iv_obj_t *dec_hdl,
                                 void *pv_api_ip,
                                 void *pv_api_op);

WORD32 ih264d_get_sei_ave_params(iv_obj_t *dec_hdl,
                                 void *pv_api_ip,
                                 void *pv_api_op);

WORD32 ih264d_get_sei_ccv_params(iv_obj_t *dec_hdl,
                                 void *pv_api_ip,
                                 void *pv_api_op);

WORD32 ih264d_set_num_cores(iv_obj_t *dec_hdl, void *pv_api_ip, void *pv_api_op);

WORD32 ih264d_deblock_display(dec_struct_t *ps_dec);

void ih264d_signal_decode_thread(dec_struct_t *ps_dec);

void ih264d_signal_bs_deblk_thread(dec_struct_t *ps_dec);
void ih264d_decode_picture_thread(dec_struct_t *ps_dec);

WORD32 ih264d_set_degrade(iv_obj_t *ps_codec_obj,
                          void *pv_api_ip,
                          void *pv_api_op);

void ih264d_fill_output_struct_from_context(dec_struct_t *ps_dec,
                                            ivd_video_decode_op_t *ps_dec_op);

/*!
 **************************************************************************
 * \if Function name : ih264d_export_sei_params \endif
 *
 * \brief
 *    Exports sei params from decoder to application.
 *
 * \return
 *    0 on Success and error code otherwise
 **************************************************************************
 */

void ih264d_export_sei_params(ivd_sei_decode_op_t *ps_sei_decode_op, dec_struct_t *ps_dec)
{
    WORD32 i4_status = IV_SUCCESS;
    sei *ps_sei = (sei *)ps_dec->pv_disp_sei_params;

    i4_status = ih264d_export_sei_mdcv_params(ps_sei_decode_op, ps_sei, &ps_dec->s_sei_export);
    i4_status = ih264d_export_sei_cll_params(ps_sei_decode_op, ps_sei, &ps_dec->s_sei_export);
    i4_status = ih264d_export_sei_ave_params(ps_sei_decode_op, ps_sei, &ps_dec->s_sei_export);
    i4_status = ih264d_export_sei_ccv_params(ps_sei_decode_op, ps_sei, &ps_dec->s_sei_export);

    UNUSED(i4_status);
}

static IV_API_CALL_STATUS_T api_check_struct_sanity(iv_obj_t *ps_handle,
                                                    void *pv_api_ip,
                                                    void *pv_api_op)
{
    IVD_API_COMMAND_TYPE_T e_cmd;
    UWORD32 *pu4_api_ip;
    UWORD32 *pu4_api_op;
    UWORD32 i, j;

    if(NULL == pv_api_op)
        return (IV_FAIL);

    if(NULL == pv_api_ip)
        return (IV_FAIL);

    pu4_api_ip = (UWORD32 *)pv_api_ip;
    pu4_api_op = (UWORD32 *)pv_api_op;
    e_cmd = *(pu4_api_ip + 1);

    /* error checks on handle */
    switch((WORD32)e_cmd)
    {
        case IVD_CMD_CREATE:
            break;

        case IVD_CMD_REL_DISPLAY_FRAME:
        case IVD_CMD_SET_DISPLAY_FRAME:
        case IVD_CMD_GET_DISPLAY_FRAME:
        case IVD_CMD_VIDEO_DECODE:
        case IVD_CMD_DELETE:
        case IVD_CMD_VIDEO_CTL:
            if(ps_handle == NULL)
            {
                *(pu4_api_op + 1) |= 1 << IVD_UNSUPPORTEDPARAM;
                *(pu4_api_op + 1) |= IVD_HANDLE_NULL;
                return IV_FAIL;
            }

            if(ps_handle->u4_size != sizeof(iv_obj_t))
            {
                *(pu4_api_op + 1) |= 1 << IVD_UNSUPPORTEDPARAM;
                *(pu4_api_op + 1) |= IVD_HANDLE_STRUCT_SIZE_INCORRECT;
                return IV_FAIL;
            }

            if(ps_handle->pv_fxns != ih264d_api_function)
            {
                *(pu4_api_op + 1) |= 1 << IVD_UNSUPPORTEDPARAM;
                *(pu4_api_op + 1) |= IVD_INVALID_HANDLE_NULL;
                return IV_FAIL;
            }

            if(ps_handle->pv_codec_handle == NULL)
            {
                *(pu4_api_op + 1) |= 1 << IVD_UNSUPPORTEDPARAM;
                *(pu4_api_op + 1) |= IVD_INVALID_HANDLE_NULL;
                return IV_FAIL;
            }
            break;
        default:
            *(pu4_api_op + 1) |= 1 << IVD_UNSUPPORTEDPARAM;
            *(pu4_api_op + 1) |= IVD_INVALID_API_CMD;
            return IV_FAIL;
    }

    switch((WORD32)e_cmd)
    {
        case IVD_CMD_CREATE:
        {
            ih264d_create_ip_t *ps_ip = (ih264d_create_ip_t *)pv_api_ip;
            ih264d_create_op_t *ps_op = (ih264d_create_op_t *)pv_api_op;


            ps_op->s_ivd_create_op_t.u4_error_code = 0;

            if((ps_ip->s_ivd_create_ip_t.u4_size > sizeof(ih264d_create_ip_t))
                            || (ps_ip->s_ivd_create_ip_t.u4_size
                                            < sizeof(ivd_create_ip_t)))
            {
                ps_op->s_ivd_create_op_t.u4_error_code |= 1
                                << IVD_UNSUPPORTEDPARAM;
                ps_op->s_ivd_create_op_t.u4_error_code |=
                                IVD_IP_API_STRUCT_SIZE_INCORRECT;
                H264_DEC_DEBUG_PRINT("\n");
                return (IV_FAIL);
            }

            if((ps_op->s_ivd_create_op_t.u4_size != sizeof(ih264d_create_op_t))
                            && (ps_op->s_ivd_create_op_t.u4_size
                                            != sizeof(ivd_create_op_t)))
            {
                ps_op->s_ivd_create_op_t.u4_error_code |= 1
                                << IVD_UNSUPPORTEDPARAM;
                ps_op->s_ivd_create_op_t.u4_error_code |=
                                IVD_OP_API_STRUCT_SIZE_INCORRECT;
                H264_DEC_DEBUG_PRINT("\n");
                return (IV_FAIL);
            }


            if((ps_ip->s_ivd_create_ip_t.e_output_format != IV_YUV_420P)
                            && (ps_ip->s_ivd_create_ip_t.e_output_format
                                            != IV_YUV_422ILE)
                            && (ps_ip->s_ivd_create_ip_t.e_output_format
                                            != IV_RGB_565)
                            && (ps_ip->s_ivd_create_ip_t.e_output_format
                                            != IV_YUV_420SP_UV)
                            && (ps_ip->s_ivd_create_ip_t.e_output_format
                                            != IV_YUV_420SP_VU))
            {
                ps_op->s_ivd_create_op_t.u4_error_code |= 1
                                << IVD_UNSUPPORTEDPARAM;
                ps_op->s_ivd_create_op_t.u4_error_code |=
                                IVD_INIT_DEC_COL_FMT_NOT_SUPPORTED;
                H264_DEC_DEBUG_PRINT("\n");
                return (IV_FAIL);
            }

        }
            break;

        case IVD_CMD_GET_DISPLAY_FRAME:
        {
            ih264d_get_display_frame_ip_t *ps_ip =
                            (ih264d_get_display_frame_ip_t *)pv_api_ip;
            ih264d_get_display_frame_op_t *ps_op =
                            (ih264d_get_display_frame_op_t *)pv_api_op;

            ps_op->s_ivd_get_display_frame_op_t.u4_error_code = 0;

            if((ps_ip->s_ivd_get_display_frame_ip_t.u4_size
                            != sizeof(ih264d_get_display_frame_ip_t))
                            && (ps_ip->s_ivd_get_display_frame_ip_t.u4_size
                                            != sizeof(ivd_get_display_frame_ip_t)))
            {
                ps_op->s_ivd_get_display_frame_op_t.u4_error_code |= 1
                                << IVD_UNSUPPORTEDPARAM;
                ps_op->s_ivd_get_display_frame_op_t.u4_error_code |=
                                IVD_IP_API_STRUCT_SIZE_INCORRECT;
                return (IV_FAIL);
            }

            if((ps_op->s_ivd_get_display_frame_op_t.u4_size
                            != sizeof(ih264d_get_display_frame_op_t))
                            && (ps_op->s_ivd_get_display_frame_op_t.u4_size
                                            != sizeof(ivd_get_display_frame_op_t)))
            {
                ps_op->s_ivd_get_display_frame_op_t.u4_error_code |= 1
                                << IVD_UNSUPPORTEDPARAM;
                ps_op->s_ivd_get_display_frame_op_t.u4_error_code |=
                                IVD_OP_API_STRUCT_SIZE_INCORRECT;
                return (IV_FAIL);
            }
        }
            break;

        case IVD_CMD_REL_DISPLAY_FRAME:
        {
            ih264d_rel_display_frame_ip_t *ps_ip =
                            (ih264d_rel_display_frame_ip_t *)pv_api_ip;
            ih264d_rel_display_frame_op_t *ps_op =
                            (ih264d_rel_display_frame_op_t *)pv_api_op;

            ps_op->s_ivd_rel_display_frame_op_t.u4_error_code = 0;

            if((ps_ip->s_ivd_rel_display_frame_ip_t.u4_size
                            != sizeof(ih264d_rel_display_frame_ip_t))
                            && (ps_ip->s_ivd_rel_display_frame_ip_t.u4_size
                                            != sizeof(ivd_rel_display_frame_ip_t)))
            {
                ps_op->s_ivd_rel_display_frame_op_t.u4_error_code |= 1
                                << IVD_UNSUPPORTEDPARAM;
                ps_op->s_ivd_rel_display_frame_op_t.u4_error_code |=
                                IVD_IP_API_STRUCT_SIZE_INCORRECT;
                return (IV_FAIL);
            }

            if((ps_op->s_ivd_rel_display_frame_op_t.u4_size
                            != sizeof(ih264d_rel_display_frame_op_t))
                            && (ps_op->s_ivd_rel_display_frame_op_t.u4_size
                                            != sizeof(ivd_rel_display_frame_op_t)))
            {
                ps_op->s_ivd_rel_display_frame_op_t.u4_error_code |= 1
                                << IVD_UNSUPPORTEDPARAM;
                ps_op->s_ivd_rel_display_frame_op_t.u4_error_code |=
                                IVD_OP_API_STRUCT_SIZE_INCORRECT;
                return (IV_FAIL);
            }

        }
            break;

        case IVD_CMD_SET_DISPLAY_FRAME:
        {
            ih264d_set_display_frame_ip_t *ps_ip =
                            (ih264d_set_display_frame_ip_t *)pv_api_ip;
            ih264d_set_display_frame_op_t *ps_op =
                            (ih264d_set_display_frame_op_t *)pv_api_op;
            UWORD32 j;

            ps_op->s_ivd_set_display_frame_op_t.u4_error_code = 0;

            if((ps_ip->s_ivd_set_display_frame_ip_t.u4_size
                            != sizeof(ih264d_set_display_frame_ip_t))
                            && (ps_ip->s_ivd_set_display_frame_ip_t.u4_size
                                            != sizeof(ivd_set_display_frame_ip_t)))
            {
                ps_op->s_ivd_set_display_frame_op_t.u4_error_code |= 1
                                << IVD_UNSUPPORTEDPARAM;
                ps_op->s_ivd_set_display_frame_op_t.u4_error_code |=
                                IVD_IP_API_STRUCT_SIZE_INCORRECT;
                return (IV_FAIL);
            }

            if((ps_op->s_ivd_set_display_frame_op_t.u4_size
                            != sizeof(ih264d_set_display_frame_op_t))
                            && (ps_op->s_ivd_set_display_frame_op_t.u4_size
                                            != sizeof(ivd_set_display_frame_op_t)))
            {
                ps_op->s_ivd_set_display_frame_op_t.u4_error_code |= 1
                                << IVD_UNSUPPORTEDPARAM;
                ps_op->s_ivd_set_display_frame_op_t.u4_error_code |=
                                IVD_OP_API_STRUCT_SIZE_INCORRECT;
                return (IV_FAIL);
            }

            if(ps_ip->s_ivd_set_display_frame_ip_t.num_disp_bufs == 0)
            {
                ps_op->s_ivd_set_display_frame_op_t.u4_error_code |= 1
                                << IVD_UNSUPPORTEDPARAM;
                ps_op->s_ivd_set_display_frame_op_t.u4_error_code |=
                                IVD_DISP_FRM_ZERO_OP_BUFS;
                return IV_FAIL;
            }

            for(j = 0; j < ps_ip->s_ivd_set_display_frame_ip_t.num_disp_bufs;
                            j++)
            {
                if(ps_ip->s_ivd_set_display_frame_ip_t.s_disp_buffer[j].u4_num_bufs
                                == 0)
                {
                    ps_op->s_ivd_set_display_frame_op_t.u4_error_code |= 1
                                    << IVD_UNSUPPORTEDPARAM;
                    ps_op->s_ivd_set_display_frame_op_t.u4_error_code |=
                                    IVD_DISP_FRM_ZERO_OP_BUFS;
                    return IV_FAIL;
                }

                for(i = 0;
                                i
                                                < ps_ip->s_ivd_set_display_frame_ip_t.s_disp_buffer[j].u4_num_bufs;
                                i++)
                {
                    if(ps_ip->s_ivd_set_display_frame_ip_t.s_disp_buffer[j].pu1_bufs[i]
                                    == NULL)
                    {
                        ps_op->s_ivd_set_display_frame_op_t.u4_error_code |= 1
                                        << IVD_UNSUPPORTEDPARAM;
                        ps_op->s_ivd_set_display_frame_op_t.u4_error_code |=
                                        IVD_DISP_FRM_OP_BUF_NULL;
                        return IV_FAIL;
                    }

                    if(ps_ip->s_ivd_set_display_frame_ip_t.s_disp_buffer[j].u4_min_out_buf_size[i]
                                    == 0)
                    {
                        ps_op->s_ivd_set_display_frame_op_t.u4_error_code |= 1
                                        << IVD_UNSUPPORTEDPARAM;
                        ps_op->s_ivd_set_display_frame_op_t.u4_error_code |=
                                        IVD_DISP_FRM_ZERO_OP_BUF_SIZE;
                        return IV_FAIL;
                    }
                }
            }
        }
            break;

        case IVD_CMD_VIDEO_DECODE:
        {
            ih264d_video_decode_ip_t *ps_ip =
                            (ih264d_video_decode_ip_t *)pv_api_ip;
            ih264d_video_decode_op_t *ps_op =
                            (ih264d_video_decode_op_t *)pv_api_op;

            H264_DEC_DEBUG_PRINT("The input bytes is: %d",
                                 ps_ip->s_ivd_video_decode_ip_t.u4_num_Bytes);
            ps_op->s_ivd_video_decode_op_t.u4_error_code = 0;

            if(ps_ip->s_ivd_video_decode_ip_t.u4_size
                            != sizeof(ih264d_video_decode_ip_t)&&
                            ps_ip->s_ivd_video_decode_ip_t.u4_size != offsetof(ivd_video_decode_ip_t, s_out_buffer))
            {
                ps_op->s_ivd_video_decode_op_t.u4_error_code |= 1
                                << IVD_UNSUPPORTEDPARAM;
                ps_op->s_ivd_video_decode_op_t.u4_error_code |=
                                IVD_IP_API_STRUCT_SIZE_INCORRECT;
                return (IV_FAIL);
            }

            if(ps_op->s_ivd_video_decode_op_t.u4_size
                            != sizeof(ih264d_video_decode_op_t)&&
                            ps_op->s_ivd_video_decode_op_t.u4_size != offsetof(ivd_video_decode_op_t, u4_output_present))
            {
                ps_op->s_ivd_video_decode_op_t.u4_error_code |= 1
                                << IVD_UNSUPPORTEDPARAM;
                ps_op->s_ivd_video_decode_op_t.u4_error_code |=
                                IVD_OP_API_STRUCT_SIZE_INCORRECT;
                return (IV_FAIL);
            }

        }
            break;

        case IVD_CMD_DELETE:
        {
            ih264d_delete_ip_t *ps_ip =
                            (ih264d_delete_ip_t *)pv_api_ip;
            ih264d_delete_op_t *ps_op =
                            (ih264d_delete_op_t *)pv_api_op;

            ps_op->s_ivd_delete_op_t.u4_error_code = 0;

            if(ps_ip->s_ivd_delete_ip_t.u4_size
                            != sizeof(ih264d_delete_ip_t))
            {
                ps_op->s_ivd_delete_op_t.u4_error_code |= 1
                                << IVD_UNSUPPORTEDPARAM;
                ps_op->s_ivd_delete_op_t.u4_error_code |=
                                IVD_IP_API_STRUCT_SIZE_INCORRECT;
                return (IV_FAIL);
            }

            if(ps_op->s_ivd_delete_op_t.u4_size
                            != sizeof(ih264d_delete_op_t))
            {
                ps_op->s_ivd_delete_op_t.u4_error_code |= 1
                                << IVD_UNSUPPORTEDPARAM;
                ps_op->s_ivd_delete_op_t.u4_error_code |=
                                IVD_OP_API_STRUCT_SIZE_INCORRECT;
                return (IV_FAIL);
            }

        }
            break;

        case IVD_CMD_VIDEO_CTL:
        {
            UWORD32 *pu4_ptr_cmd;
            UWORD32 sub_command;

            pu4_ptr_cmd = (UWORD32 *)pv_api_ip;
            pu4_ptr_cmd += 2;
            sub_command = *pu4_ptr_cmd;

            switch(sub_command)
            {
                case IVD_CMD_CTL_SETPARAMS:
                {
                    ih264d_ctl_set_config_ip_t *ps_ip;
                    ih264d_ctl_set_config_op_t *ps_op;
                    ps_ip = (ih264d_ctl_set_config_ip_t *)pv_api_ip;
                    ps_op = (ih264d_ctl_set_config_op_t *)pv_api_op;

                    if(ps_ip->s_ivd_ctl_set_config_ip_t.u4_size
                                    != sizeof(ih264d_ctl_set_config_ip_t))
                    {
                        ps_op->s_ivd_ctl_set_config_op_t.u4_error_code |= 1
                                        << IVD_UNSUPPORTEDPARAM;
                        ps_op->s_ivd_ctl_set_config_op_t.u4_error_code |=
                                        IVD_IP_API_STRUCT_SIZE_INCORRECT;
                        return IV_FAIL;
                    }
                }
                    //no break; is needed here
                case IVD_CMD_CTL_SETDEFAULT:
                {
                    ih264d_ctl_set_config_op_t *ps_op;
                    ps_op = (ih264d_ctl_set_config_op_t *)pv_api_op;
                    if(ps_op->s_ivd_ctl_set_config_op_t.u4_size
                                    != sizeof(ih264d_ctl_set_config_op_t))
                    {
                        ps_op->s_ivd_ctl_set_config_op_t.u4_error_code |= 1
                                        << IVD_UNSUPPORTEDPARAM;
                        ps_op->s_ivd_ctl_set_config_op_t.u4_error_code |=
                                        IVD_OP_API_STRUCT_SIZE_INCORRECT;
                        return IV_FAIL;
                    }
                }
                    break;

                case IVD_CMD_CTL_GETPARAMS:
                {
                    ih264d_ctl_getstatus_ip_t *ps_ip;
                    ih264d_ctl_getstatus_op_t *ps_op;

                    ps_ip = (ih264d_ctl_getstatus_ip_t *)pv_api_ip;
                    ps_op = (ih264d_ctl_getstatus_op_t *)pv_api_op;
                    if(ps_ip->s_ivd_ctl_getstatus_ip_t.u4_size
                                    != sizeof(ih264d_ctl_getstatus_ip_t))
                    {
                        ps_op->s_ivd_ctl_getstatus_op_t.u4_error_code |= 1
                                        << IVD_UNSUPPORTEDPARAM;
                        ps_op->s_ivd_ctl_getstatus_op_t.u4_error_code |=
                                        IVD_IP_API_STRUCT_SIZE_INCORRECT;
                        return IV_FAIL;
                    }
                    if(ps_op->s_ivd_ctl_getstatus_op_t.u4_size
                                    != sizeof(ih264d_ctl_getstatus_op_t))
                    {
                        ps_op->s_ivd_ctl_getstatus_op_t.u4_error_code |= 1
                                        << IVD_UNSUPPORTEDPARAM;
                        ps_op->s_ivd_ctl_getstatus_op_t.u4_error_code |=
                                        IVD_OP_API_STRUCT_SIZE_INCORRECT;
                        return IV_FAIL;
                    }
                }
                    break;

                case IVD_CMD_CTL_GETBUFINFO:
                {
                    ih264d_ctl_getbufinfo_ip_t *ps_ip;
                    ih264d_ctl_getbufinfo_op_t *ps_op;
                    ps_ip = (ih264d_ctl_getbufinfo_ip_t *)pv_api_ip;
                    ps_op = (ih264d_ctl_getbufinfo_op_t *)pv_api_op;

                    if(ps_ip->s_ivd_ctl_getbufinfo_ip_t.u4_size
                                    != sizeof(ih264d_ctl_getbufinfo_ip_t))
                    {
                        ps_op->s_ivd_ctl_getbufinfo_op_t.u4_error_code |= 1
                                        << IVD_UNSUPPORTEDPARAM;
                        ps_op->s_ivd_ctl_getbufinfo_op_t.u4_error_code |=
                                        IVD_IP_API_STRUCT_SIZE_INCORRECT;
                        return IV_FAIL;
                    }
                    if(ps_op->s_ivd_ctl_getbufinfo_op_t.u4_size
                                    != sizeof(ih264d_ctl_getbufinfo_op_t))
                    {
                        ps_op->s_ivd_ctl_getbufinfo_op_t.u4_error_code |= 1
                                        << IVD_UNSUPPORTEDPARAM;
                        ps_op->s_ivd_ctl_getbufinfo_op_t.u4_error_code |=
                                        IVD_OP_API_STRUCT_SIZE_INCORRECT;
                        return IV_FAIL;
                    }
                }
                    break;

                case IVD_CMD_CTL_GETVERSION:
                {
                    ih264d_ctl_getversioninfo_ip_t *ps_ip;
                    ih264d_ctl_getversioninfo_op_t *ps_op;
                    ps_ip = (ih264d_ctl_getversioninfo_ip_t *)pv_api_ip;
                    ps_op = (ih264d_ctl_getversioninfo_op_t *)pv_api_op;
                    if(ps_ip->s_ivd_ctl_getversioninfo_ip_t.u4_size
                                    != sizeof(ih264d_ctl_getversioninfo_ip_t))
                    {
                        ps_op->s_ivd_ctl_getversioninfo_op_t.u4_error_code |= 1
                                        << IVD_UNSUPPORTEDPARAM;
                        ps_op->s_ivd_ctl_getversioninfo_op_t.u4_error_code |=
                                        IVD_IP_API_STRUCT_SIZE_INCORRECT;
                        return IV_FAIL;
                    }
                    if(ps_op->s_ivd_ctl_getversioninfo_op_t.u4_size
                                    != sizeof(ih264d_ctl_getversioninfo_op_t))
                    {
                        ps_op->s_ivd_ctl_getversioninfo_op_t.u4_error_code |= 1
                                        << IVD_UNSUPPORTEDPARAM;
                        ps_op->s_ivd_ctl_getversioninfo_op_t.u4_error_code |=
                                        IVD_OP_API_STRUCT_SIZE_INCORRECT;
                        return IV_FAIL;
                    }
                }
                    break;

                case IVD_CMD_CTL_FLUSH:
                {
                    ih264d_ctl_flush_ip_t *ps_ip;
                    ih264d_ctl_flush_op_t *ps_op;
                    ps_ip = (ih264d_ctl_flush_ip_t *)pv_api_ip;
                    ps_op = (ih264d_ctl_flush_op_t *)pv_api_op;
                    if(ps_ip->s_ivd_ctl_flush_ip_t.u4_size
                                    != sizeof(ih264d_ctl_flush_ip_t))
                    {
                        ps_op->s_ivd_ctl_flush_op_t.u4_error_code |= 1
                                        << IVD_UNSUPPORTEDPARAM;
                        ps_op->s_ivd_ctl_flush_op_t.u4_error_code |=
                                        IVD_IP_API_STRUCT_SIZE_INCORRECT;
                        return IV_FAIL;
                    }
                    if(ps_op->s_ivd_ctl_flush_op_t.u4_size
                                    != sizeof(ih264d_ctl_flush_op_t))
                    {
                        ps_op->s_ivd_ctl_flush_op_t.u4_error_code |= 1
                                        << IVD_UNSUPPORTEDPARAM;
                        ps_op->s_ivd_ctl_flush_op_t.u4_error_code |=
                                        IVD_OP_API_STRUCT_SIZE_INCORRECT;
                        return IV_FAIL;
                    }
                }
                    break;

                case IVD_CMD_CTL_RESET:
                {
                    ih264d_ctl_reset_ip_t *ps_ip;
                    ih264d_ctl_reset_op_t *ps_op;
                    ps_ip = (ih264d_ctl_reset_ip_t *)pv_api_ip;
                    ps_op = (ih264d_ctl_reset_op_t *)pv_api_op;
                    if(ps_ip->s_ivd_ctl_reset_ip_t.u4_size
                                    != sizeof(ih264d_ctl_reset_ip_t))
                    {
                        ps_op->s_ivd_ctl_reset_op_t.u4_error_code |= 1
                                        << IVD_UNSUPPORTEDPARAM;
                        ps_op->s_ivd_ctl_reset_op_t.u4_error_code |=
                                        IVD_IP_API_STRUCT_SIZE_INCORRECT;
                        return IV_FAIL;
                    }
                    if(ps_op->s_ivd_ctl_reset_op_t.u4_size
                                    != sizeof(ih264d_ctl_reset_op_t))
                    {
                        ps_op->s_ivd_ctl_reset_op_t.u4_error_code |= 1
                                        << IVD_UNSUPPORTEDPARAM;
                        ps_op->s_ivd_ctl_reset_op_t.u4_error_code |=
                                        IVD_OP_API_STRUCT_SIZE_INCORRECT;
                        return IV_FAIL;
                    }
                }
                    break;

                case IH264D_CMD_CTL_DEGRADE:
                {
                    ih264d_ctl_degrade_ip_t *ps_ip;
                    ih264d_ctl_degrade_op_t *ps_op;

                    ps_ip = (ih264d_ctl_degrade_ip_t *)pv_api_ip;
                    ps_op = (ih264d_ctl_degrade_op_t *)pv_api_op;

                    if(ps_ip->u4_size != sizeof(ih264d_ctl_degrade_ip_t))
                    {
                        ps_op->u4_error_code |= 1 << IVD_UNSUPPORTEDPARAM;
                        ps_op->u4_error_code |=
                                        IVD_IP_API_STRUCT_SIZE_INCORRECT;
                        return IV_FAIL;
                    }

                    if(ps_op->u4_size != sizeof(ih264d_ctl_degrade_op_t))
                    {
                        ps_op->u4_error_code |= 1 << IVD_UNSUPPORTEDPARAM;
                        ps_op->u4_error_code |=
                                        IVD_OP_API_STRUCT_SIZE_INCORRECT;
                        return IV_FAIL;
                    }

                    if((ps_ip->i4_degrade_pics < 0)
                                    || (ps_ip->i4_degrade_pics > 4)
                                    || (ps_ip->i4_nondegrade_interval < 0)
                                    || (ps_ip->i4_degrade_type < 0)
                                    || (ps_ip->i4_degrade_type > 15))
                    {
                        ps_op->u4_error_code |= 1 << IVD_UNSUPPORTEDPARAM;
                        return IV_FAIL;
                    }

                    break;
                }

                case IH264D_CMD_CTL_GET_BUFFER_DIMENSIONS:
                {
                    ih264d_ctl_get_frame_dimensions_ip_t *ps_ip;
                    ih264d_ctl_get_frame_dimensions_op_t *ps_op;

                    ps_ip = (ih264d_ctl_get_frame_dimensions_ip_t *)pv_api_ip;
                    ps_op = (ih264d_ctl_get_frame_dimensions_op_t *)pv_api_op;

                    if(ps_ip->u4_size
                                    != sizeof(ih264d_ctl_get_frame_dimensions_ip_t))
                    {
                        ps_op->u4_error_code |= 1 << IVD_UNSUPPORTEDPARAM;
                        ps_op->u4_error_code |=
                                        IVD_IP_API_STRUCT_SIZE_INCORRECT;
                        return IV_FAIL;
                    }

                    if(ps_op->u4_size
                                    != sizeof(ih264d_ctl_get_frame_dimensions_op_t))
                    {
                        ps_op->u4_error_code |= 1 << IVD_UNSUPPORTEDPARAM;
                        ps_op->u4_error_code |=
                                        IVD_OP_API_STRUCT_SIZE_INCORRECT;
                        return IV_FAIL;
                    }

                    break;
                }
                case IH264D_CMD_CTL_GET_VUI_PARAMS:
                {
                    ih264d_ctl_get_vui_params_ip_t *ps_ip;
                    ih264d_ctl_get_vui_params_op_t *ps_op;

                    ps_ip =
                                    (ih264d_ctl_get_vui_params_ip_t *)pv_api_ip;
                    ps_op =
                                    (ih264d_ctl_get_vui_params_op_t *)pv_api_op;

                    if(ps_ip->u4_size
                                    != sizeof(ih264d_ctl_get_vui_params_ip_t))
                    {
                        ps_op->u4_error_code |= 1 << IVD_UNSUPPORTEDPARAM;
                        ps_op->u4_error_code |=
                                        IVD_IP_API_STRUCT_SIZE_INCORRECT;
                        return IV_FAIL;
                    }

                    if(ps_op->u4_size
                                    != sizeof(ih264d_ctl_get_vui_params_op_t))
                    {
                        ps_op->u4_error_code |= 1 << IVD_UNSUPPORTEDPARAM;
                        ps_op->u4_error_code |=
                                        IVD_OP_API_STRUCT_SIZE_INCORRECT;
                        return IV_FAIL;
                    }

                    break;
                }
                case IH264D_CMD_CTL_GET_SEI_MDCV_PARAMS:
                {
                    ih264d_ctl_get_sei_mdcv_params_ip_t *ps_ip;
                    ih264d_ctl_get_sei_mdcv_params_op_t *ps_op;

                    ps_ip = (ih264d_ctl_get_sei_mdcv_params_ip_t *)pv_api_ip;
                    ps_op = (ih264d_ctl_get_sei_mdcv_params_op_t *)pv_api_op;

                    if(ps_ip->u4_size != sizeof(ih264d_ctl_get_sei_mdcv_params_ip_t))
                    {
                        ps_op->u4_error_code |= 1 << IVD_UNSUPPORTEDPARAM;
                        ps_op->u4_error_code |=
                                        IVD_IP_API_STRUCT_SIZE_INCORRECT;
                        return IV_FAIL;
                    }

                    if(ps_op->u4_size != sizeof(ih264d_ctl_get_sei_mdcv_params_op_t))
                    {
                        ps_op->u4_error_code |= 1 << IVD_UNSUPPORTEDPARAM;
                        ps_op->u4_error_code |=
                                        IVD_OP_API_STRUCT_SIZE_INCORRECT;
                        return IV_FAIL;
                    }

                    break;
                }

                case IH264D_CMD_CTL_GET_SEI_CLL_PARAMS:
                {
                    ih264d_ctl_get_sei_cll_params_ip_t *ps_ip;
                    ih264d_ctl_get_sei_cll_params_op_t *ps_op;

                    ps_ip = (ih264d_ctl_get_sei_cll_params_ip_t *)pv_api_ip;
                    ps_op = (ih264d_ctl_get_sei_cll_params_op_t *)pv_api_op;

                    if(ps_ip->u4_size != sizeof(ih264d_ctl_get_sei_cll_params_ip_t))
                    {
                        ps_op->u4_error_code |= 1 << IVD_UNSUPPORTEDPARAM;
                        ps_op->u4_error_code |=
                                        IVD_IP_API_STRUCT_SIZE_INCORRECT;
                        return IV_FAIL;
                    }

                    if(ps_op->u4_size != sizeof(ih264d_ctl_get_sei_cll_params_op_t))
                    {
                        ps_op->u4_error_code |= 1 << IVD_UNSUPPORTEDPARAM;
                        ps_op->u4_error_code |=
                                        IVD_OP_API_STRUCT_SIZE_INCORRECT;
                        return IV_FAIL;
                    }

                    break;
                }

                case IH264D_CMD_CTL_GET_SEI_AVE_PARAMS:
                {
                    ih264d_ctl_get_sei_ave_params_ip_t *ps_ip;
                    ih264d_ctl_get_sei_ave_params_op_t *ps_op;

                    ps_ip = (ih264d_ctl_get_sei_ave_params_ip_t *)pv_api_ip;
                    ps_op = (ih264d_ctl_get_sei_ave_params_op_t *)pv_api_op;

                    if(ps_ip->u4_size != sizeof(ih264d_ctl_get_sei_ave_params_ip_t))
                    {
                        ps_op->u4_error_code |= 1 << IVD_UNSUPPORTEDPARAM;
                        ps_op->u4_error_code |=
                                        IVD_IP_API_STRUCT_SIZE_INCORRECT;
                        return IV_FAIL;
                    }

                    if(ps_op->u4_size != sizeof(ih264d_ctl_get_sei_ave_params_op_t))
                    {
                        ps_op->u4_error_code |= 1 << IVD_UNSUPPORTEDPARAM;
                        ps_op->u4_error_code |=
                                        IVD_OP_API_STRUCT_SIZE_INCORRECT;
                        return IV_FAIL;
                    }

                    break;
                }

                case IH264D_CMD_CTL_GET_SEI_CCV_PARAMS:
                {
                    ih264d_ctl_get_sei_ccv_params_ip_t *ps_ip;
                    ih264d_ctl_get_sei_ccv_params_op_t *ps_op;

                    ps_ip = (ih264d_ctl_get_sei_ccv_params_ip_t *)pv_api_ip;
                    ps_op = (ih264d_ctl_get_sei_ccv_params_op_t *)pv_api_op;

                    if(ps_ip->u4_size != sizeof(ih264d_ctl_get_sei_ccv_params_ip_t))
                    {
                        ps_op->u4_error_code |= 1 << IVD_UNSUPPORTEDPARAM;
                        ps_op->u4_error_code |=
                                        IVD_IP_API_STRUCT_SIZE_INCORRECT;
                        return IV_FAIL;
                    }

                    if(ps_op->u4_size != sizeof(ih264d_ctl_get_sei_ccv_params_op_t))
                    {
                        ps_op->u4_error_code |= 1 << IVD_UNSUPPORTEDPARAM;
                        ps_op->u4_error_code |=
                                        IVD_OP_API_STRUCT_SIZE_INCORRECT;
                        return IV_FAIL;
                    }

                    break;
                }

                case IH264D_CMD_CTL_SET_NUM_CORES:
                {
                    ih264d_ctl_set_num_cores_ip_t *ps_ip;
                    ih264d_ctl_set_num_cores_op_t *ps_op;

                    ps_ip = (ih264d_ctl_set_num_cores_ip_t *)pv_api_ip;
                    ps_op = (ih264d_ctl_set_num_cores_op_t *)pv_api_op;

                    if(ps_ip->u4_size != sizeof(ih264d_ctl_set_num_cores_ip_t))
                    {
                        ps_op->u4_error_code |= 1 << IVD_UNSUPPORTEDPARAM;
                        ps_op->u4_error_code |=
                                        IVD_IP_API_STRUCT_SIZE_INCORRECT;
                        return IV_FAIL;
                    }

                    if(ps_op->u4_size != sizeof(ih264d_ctl_set_num_cores_op_t))
                    {
                        ps_op->u4_error_code |= 1 << IVD_UNSUPPORTEDPARAM;
                        ps_op->u4_error_code |=
                                        IVD_OP_API_STRUCT_SIZE_INCORRECT;
                        return IV_FAIL;
                    }

                    if((ps_ip->u4_num_cores != 1) && (ps_ip->u4_num_cores != 2)
                                    && (ps_ip->u4_num_cores != 3)
                                    && (ps_ip->u4_num_cores != 4))
                    {
                        ps_op->u4_error_code |= 1 << IVD_UNSUPPORTEDPARAM;
                        return IV_FAIL;
                    }
                    break;
                }
                case IH264D_CMD_CTL_SET_PROCESSOR:
                {
                    ih264d_ctl_set_processor_ip_t *ps_ip;
                    ih264d_ctl_set_processor_op_t *ps_op;

                    ps_ip = (ih264d_ctl_set_processor_ip_t *)pv_api_ip;
                    ps_op = (ih264d_ctl_set_processor_op_t *)pv_api_op;

                    if(ps_ip->u4_size != sizeof(ih264d_ctl_set_processor_ip_t))
                    {
                        ps_op->u4_error_code |= 1 << IVD_UNSUPPORTEDPARAM;
                        ps_op->u4_error_code |=
                                        IVD_IP_API_STRUCT_SIZE_INCORRECT;
                        return IV_FAIL;
                    }

                    if(ps_op->u4_size != sizeof(ih264d_ctl_set_processor_op_t))
                    {
                        ps_op->u4_error_code |= 1 << IVD_UNSUPPORTEDPARAM;
                        ps_op->u4_error_code |=
                                        IVD_OP_API_STRUCT_SIZE_INCORRECT;
                        return IV_FAIL;
                    }

                    break;
                }
                default:
                    *(pu4_api_op + 1) |= 1 << IVD_UNSUPPORTEDPARAM;
                    *(pu4_api_op + 1) |= IVD_UNSUPPORTED_API_CMD;
                    return IV_FAIL;
                    break;
            }
        }
            break;
    }

    return IV_SUCCESS;
}


/**
 *******************************************************************************
 *
 * @brief
 *  Sets Processor type
 *
 * @par Description:
 *  Sets Processor type
 *
 * @param[in] ps_codec_obj
 *  Pointer to codec object at API level
 *
 * @param[in] pv_api_ip
 *  Pointer to input argument structure
 *
 * @param[out] pv_api_op
 *  Pointer to output argument structure
 *
 * @returns  Status
 *
 * @remarks
 *
 *
 *******************************************************************************
 */

WORD32 ih264d_set_processor(iv_obj_t *dec_hdl, void *pv_api_ip, void *pv_api_op)
{
    ih264d_ctl_set_processor_ip_t *ps_ip;
    ih264d_ctl_set_processor_op_t *ps_op;
    dec_struct_t *ps_codec = (dec_struct_t *)dec_hdl->pv_codec_handle;

    ps_ip = (ih264d_ctl_set_processor_ip_t *)pv_api_ip;
    ps_op = (ih264d_ctl_set_processor_op_t *)pv_api_op;

    ps_codec->e_processor_arch = (IVD_ARCH_T)ps_ip->u4_arch;
    ps_codec->e_processor_soc = (IVD_SOC_T)ps_ip->u4_soc;

    ih264d_init_function_ptr(ps_codec);

    ps_op->u4_error_code = 0;
    return IV_SUCCESS;
}


/**************************************************************************
 * \if Function name : ih264d_init_decoder \endif
 *
 *
 * \brief
 *    Initializes the decoder
 *
 * \param apiVersion               : Version of the api being used.
 * \param errorHandlingMechanism   : Mechanism to be used for errror handling.
 * \param postFilteringType: Type of post filtering operation to be used.
 * \param uc_outputFormat: Format of the decoded picture [default 4:2:0].
 * \param uc_dispBufs: Number of Display Buffers.
 * \param p_NALBufAPI: Pointer to NAL Buffer API.
 * \param p_DispBufAPI: Pointer to Display Buffer API.
 * \param ih264d_dec_mem_manager  :Pointer to the function that will be called by decoder
 *                        for memory allocation and freeing.
 *
 * \return
 *    0 on Success and -1 on error
 *
 **************************************************************************
 */
void ih264d_init_decoder(void * ps_dec_params)
{
    dec_struct_t * ps_dec = (dec_struct_t *)ps_dec_params;
    dec_slice_params_t *ps_cur_slice;
    pocstruct_t *ps_prev_poc, *ps_cur_poc;
    WORD32 size;

    size = sizeof(pred_info_t) * 2 * 32;
    memset(ps_dec->ps_pred, 0 , size);

    size = sizeof(disp_mgr_t);
    memset(ps_dec->pv_disp_buf_mgr, 0 , size);

    size = sizeof(buf_mgr_t) + ithread_get_mutex_lock_size();
    memset(ps_dec->pv_pic_buf_mgr, 0, size);

    size = sizeof(dec_err_status_t);
    memset(ps_dec->ps_dec_err_status, 0, size);

    size = sizeof(sei);
    memset(ps_dec->ps_sei, 0, size);

    size = sizeof(sei);
    memset(ps_dec->ps_sei_parse, 0, size);

    size = sizeof(dpb_commands_t);
    memset(ps_dec->ps_dpb_cmds, 0, size);

    size = sizeof(dec_bit_stream_t);
    memset(ps_dec->ps_bitstrm, 0, size);

    size = sizeof(dec_slice_params_t);
    memset(ps_dec->ps_cur_slice, 0, size);

    size = MAX(sizeof(dec_seq_params_t), sizeof(dec_pic_params_t));
    memset(ps_dec->pv_scratch_sps_pps, 0, size);

    size = sizeof(ctxt_inc_mb_info_t);
    memset(ps_dec->ps_left_mb_ctxt_info, 0, size);

    size = (sizeof(neighbouradd_t) << 2);
    memset(ps_dec->ps_left_mvpred_addr, 0 ,size);

    size = sizeof(buf_mgr_t) + ithread_get_mutex_lock_size();
    memset(ps_dec->pv_mv_buf_mgr, 0, size);

    /* Free any dynamic buffers that are allocated */
    ih264d_free_dynamic_bufs(ps_dec);

    {
        UWORD8 i;
        struct pic_buffer_t *ps_init_dpb;
        ps_init_dpb = ps_dec->ps_dpb_mgr->ps_init_dpb[0][0];
        for(i = 0; i < 2 * MAX_REF_BUFS; i++)
        {
            ps_init_dpb->pu1_buf1 = NULL;
            ps_init_dpb->u1_long_term_frm_idx = MAX_REF_BUFS + 1;
            ps_dec->ps_dpb_mgr->ps_init_dpb[0][i] = ps_init_dpb;
            ps_dec->ps_dpb_mgr->ps_mod_dpb[0][i] = ps_init_dpb;
            ps_init_dpb++;
        }

        ps_init_dpb = ps_dec->ps_dpb_mgr->ps_init_dpb[1][0];
        for(i = 0; i < 2 * MAX_REF_BUFS; i++)
        {
            ps_init_dpb->pu1_buf1 = NULL;
            ps_init_dpb->u1_long_term_frm_idx = MAX_REF_BUFS + 1;
            ps_dec->ps_dpb_mgr->ps_init_dpb[1][i] = ps_init_dpb;
            ps_dec->ps_dpb_mgr->ps_mod_dpb[1][i] = ps_init_dpb;
            ps_init_dpb++;
        }
    }

    ps_cur_slice = ps_dec->ps_cur_slice;
    ps_dec->init_done = 0;

    ps_dec->u4_num_cores = 1;

    ps_dec->u2_pic_ht = ps_dec->u2_pic_wd = 0;

    ps_dec->u1_separate_parse = DEFAULT_SEPARATE_PARSE;
    ps_dec->u4_app_disable_deblk_frm = 0;
    ps_dec->i4_degrade_type = 0;
    ps_dec->i4_degrade_pics = 0;

    memset(ps_dec->ps_pps, 0,
           ((sizeof(dec_pic_params_t)) * MAX_NUM_PIC_PARAMS));
    memset(ps_dec->ps_sps, 0,
           ((sizeof(dec_seq_params_t)) * MAX_NUM_SEQ_PARAMS));

    /* Initialization of function pointers ih264d_deblock_picture function*/

    ps_dec->p_DeblockPicture[0] = ih264d_deblock_picture_non_mbaff;
    ps_dec->p_DeblockPicture[1] = ih264d_deblock_picture_mbaff;

    ps_dec->s_cab_dec_env.pv_codec_handle = ps_dec;

    ps_dec->u4_num_fld_in_frm = 0;

    ps_dec->ps_dpb_mgr->pv_codec_handle = ps_dec;

    /* Initialize the sei validity u4_flag with zero indiacting sei is not valid*/
    ps_dec->ps_sei->u1_is_valid = 0;

    /* decParams Initializations */
    ps_dec->ps_cur_pps = NULL;
    ps_dec->ps_cur_sps = NULL;
    ps_dec->u1_init_dec_flag = 0;
    ps_dec->u1_first_slice_in_stream = 1;
    ps_dec->u1_last_pic_not_decoded = 0;
    ps_dec->u4_app_disp_width = 0;
    ps_dec->i4_header_decoded = 0;
    ps_dec->u4_total_frames_decoded = 0;

    ps_dec->i4_error_code = 0;
    ps_dec->i4_content_type = IV_CONTENTTYPE_NA;
    ps_dec->ps_cur_slice->u1_mbaff_frame_flag = 0;

    ps_dec->ps_dec_err_status->u1_err_flag = ACCEPT_ALL_PICS; //REJECT_PB_PICS;
    ps_dec->ps_dec_err_status->u1_cur_pic_type = PIC_TYPE_UNKNOWN;
    ps_dec->ps_dec_err_status->u4_frm_sei_sync = SYNC_FRM_DEFAULT;
    ps_dec->ps_dec_err_status->u4_cur_frm = INIT_FRAME;
    ps_dec->ps_dec_err_status->u1_pic_aud_i = PIC_TYPE_UNKNOWN;

    ps_dec->u1_pr_sl_type = 0xFF;
    ps_dec->u2_mbx = 0xffff;
    ps_dec->u2_mby = 0;
    ps_dec->u2_total_mbs_coded = 0;

    /* POC initializations */
    ps_prev_poc = &ps_dec->s_prev_pic_poc;
    ps_cur_poc = &ps_dec->s_cur_pic_poc;
    ps_prev_poc->i4_pic_order_cnt_lsb = ps_cur_poc->i4_pic_order_cnt_lsb = 0;
    ps_prev_poc->i4_pic_order_cnt_msb = ps_cur_poc->i4_pic_order_cnt_msb = 0;
    ps_prev_poc->i4_delta_pic_order_cnt_bottom =
                    ps_cur_poc->i4_delta_pic_order_cnt_bottom = 0;
    ps_prev_poc->i4_delta_pic_order_cnt[0] =
                    ps_cur_poc->i4_delta_pic_order_cnt[0] = 0;
    ps_prev_poc->i4_delta_pic_order_cnt[1] =
                    ps_cur_poc->i4_delta_pic_order_cnt[1] = 0;
    ps_prev_poc->u1_mmco_equalto5 = ps_cur_poc->u1_mmco_equalto5 = 0;
    ps_prev_poc->i4_top_field_order_count = ps_cur_poc->i4_top_field_order_count =
                    0;
    ps_prev_poc->i4_bottom_field_order_count =
                    ps_cur_poc->i4_bottom_field_order_count = 0;
    ps_prev_poc->u1_bot_field = ps_cur_poc->u1_bot_field = 0;
    ps_prev_poc->u1_mmco_equalto5 = ps_cur_poc->u1_mmco_equalto5 = 0;
    ps_prev_poc->i4_prev_frame_num_ofst = ps_cur_poc->i4_prev_frame_num_ofst = 0;
    ps_cur_slice->u1_mmco_equalto5 = 0;
    ps_cur_slice->u2_frame_num = 0;

    ps_dec->i4_max_poc = 0;
    ps_dec->i4_prev_max_display_seq = 0;
    ps_dec->u1_recon_mb_grp = 4;
    ps_dec->i4_reorder_depth = -1;

    /* Field PIC initializations */
    ps_dec->u1_second_field = 0;
    ps_dec->s_prev_seq_params.u1_eoseq_pending = 0;

    /* Set the cropping parameters as zero */
    ps_dec->u2_crop_offset_y = 0;
    ps_dec->u2_crop_offset_uv = 0;

	ps_dec->u1_frame_cropping_flag = 0;
	ps_dec->u1_frame_cropping_rect_left_ofst = 0;
	ps_dec->u1_frame_cropping_rect_right_ofst = 0;
	ps_dec->u1_frame_cropping_rect_top_ofst = 0;
	ps_dec->u1_frame_cropping_rect_bottom_ofst = 0;

    /* The Initial Frame Rate Info is not Present */
    ps_dec->i4_vui_frame_rate = -1;
    ps_dec->i4_pic_type = NA_SLICE;
    ps_dec->i4_frametype = IV_NA_FRAME;
    ps_dec->i4_content_type = IV_CONTENTTYPE_NA;

    ps_dec->u1_res_changed = 0;


    ps_dec->u1_frame_decoded_flag = 0;

    /* Set the default frame seek mask mode */
    ps_dec->u4_skip_frm_mask = SKIP_NONE;

    /********************************************************/
    /* Initialize CAVLC residual decoding function pointers */
    /********************************************************/
    ps_dec->pf_cavlc_4x4res_block[0] = ih264d_cavlc_4x4res_block_totalcoeff_1;
    ps_dec->pf_cavlc_4x4res_block[1] =
                    ih264d_cavlc_4x4res_block_totalcoeff_2to10;
    ps_dec->pf_cavlc_4x4res_block[2] =
                    ih264d_cavlc_4x4res_block_totalcoeff_11to16;

    ps_dec->pf_cavlc_parse4x4coeff[0] = ih264d_cavlc_parse4x4coeff_n0to7;
    ps_dec->pf_cavlc_parse4x4coeff[1] = ih264d_cavlc_parse4x4coeff_n8;

    ps_dec->pf_cavlc_parse_8x8block[0] =
                    ih264d_cavlc_parse_8x8block_none_available;
    ps_dec->pf_cavlc_parse_8x8block[1] =
                    ih264d_cavlc_parse_8x8block_left_available;
    ps_dec->pf_cavlc_parse_8x8block[2] =
                    ih264d_cavlc_parse_8x8block_top_available;
    ps_dec->pf_cavlc_parse_8x8block[3] =
                    ih264d_cavlc_parse_8x8block_both_available;

    /***************************************************************************/
    /* Initialize Bs calculation function pointers for P and B, 16x16/non16x16 */
    /***************************************************************************/
    ps_dec->pf_fill_bs1[0][0] = ih264d_fill_bs1_16x16mb_pslice;
    ps_dec->pf_fill_bs1[0][1] = ih264d_fill_bs1_non16x16mb_pslice;

    ps_dec->pf_fill_bs1[1][0] = ih264d_fill_bs1_16x16mb_bslice;
    ps_dec->pf_fill_bs1[1][1] = ih264d_fill_bs1_non16x16mb_bslice;

    ps_dec->pf_fill_bs_xtra_left_edge[0] =
                    ih264d_fill_bs_xtra_left_edge_cur_frm;
    ps_dec->pf_fill_bs_xtra_left_edge[1] =
                    ih264d_fill_bs_xtra_left_edge_cur_fld;

    /* Initialize Reference Pic Buffers */
    ih264d_init_ref_bufs(ps_dec->ps_dpb_mgr);

    ps_dec->u2_prv_frame_num = 0;
    ps_dec->u1_top_bottom_decoded = 0;
    ps_dec->u1_dangling_field = 0;

    ps_dec->s_cab_dec_env.cabac_table = gau4_ih264d_cabac_table;

    ps_dec->pu1_left_mv_ctxt_inc = ps_dec->u1_left_mv_ctxt_inc_arr[0];
    ps_dec->pi1_left_ref_idx_ctxt_inc =
                    &ps_dec->i1_left_ref_idx_ctx_inc_arr[0][0];
    ps_dec->pu1_left_yuv_dc_csbp = &ps_dec->u1_yuv_dc_csbp_topmb;

    /* ! */
    /* Initializing flush frame u4_flag */
    ps_dec->u1_flushfrm = 0;

    {
        ps_dec->s_cab_dec_env.pv_codec_handle = (void*)ps_dec;
        ps_dec->ps_bitstrm->pv_codec_handle = (void*)ps_dec;
        ps_dec->ps_cur_slice->pv_codec_handle = (void*)ps_dec;
        ps_dec->ps_dpb_mgr->pv_codec_handle = (void*)ps_dec;
    }

    memset(ps_dec->disp_bufs, 0, (MAX_DISP_BUFS_NEW) * sizeof(disp_buf_t));
    memset(ps_dec->u4_disp_buf_mapping, 0,
           (MAX_DISP_BUFS_NEW) * sizeof(UWORD32));
    memset(ps_dec->u4_disp_buf_to_be_freed, 0,
           (MAX_DISP_BUFS_NEW) * sizeof(UWORD32));
    memset(ps_dec->ps_cur_slice, 0, sizeof(dec_slice_params_t));

    ih264d_init_arch(ps_dec);
    ih264d_init_function_ptr(ps_dec);
    ps_dec->e_frm_out_mode = IVD_DISPLAY_FRAME_OUT;
    ps_dec->init_done = 1;

}
WORD32 ih264d_free_static_bufs(iv_obj_t *dec_hdl)
{
    dec_struct_t *ps_dec;

    void (*pf_aligned_free)(void *pv_mem_ctxt, void *pv_buf);
    void *pv_mem_ctxt;

    ps_dec = (dec_struct_t *)dec_hdl->pv_codec_handle;
    pf_aligned_free = ps_dec->pf_aligned_free;
    pv_mem_ctxt = ps_dec->pv_mem_ctxt;

    PS_DEC_ALIGNED_FREE(ps_dec, ps_dec->ps_sps);
    PS_DEC_ALIGNED_FREE(ps_dec, ps_dec->ps_pps);
    PS_DEC_ALIGNED_FREE(ps_dec, ps_dec->pv_dec_thread_handle);
    PS_DEC_ALIGNED_FREE(ps_dec, ps_dec->pv_bs_deblk_thread_handle);
    PS_DEC_ALIGNED_FREE(ps_dec, ps_dec->ps_dpb_mgr);
    PS_DEC_ALIGNED_FREE(ps_dec, ps_dec->ps_pred);
    PS_DEC_ALIGNED_FREE(ps_dec, ps_dec->pv_disp_buf_mgr);
    PS_DEC_ALIGNED_FREE(ps_dec, ps_dec->pv_pic_buf_mgr);
    PS_DEC_ALIGNED_FREE(ps_dec, ps_dec->ps_pic_buf_base);
    PS_DEC_ALIGNED_FREE(ps_dec, ps_dec->ps_dec_err_status);
    PS_DEC_ALIGNED_FREE(ps_dec, ps_dec->ps_sei);
    PS_DEC_ALIGNED_FREE(ps_dec, ps_dec->ps_sei_parse);
    PS_DEC_ALIGNED_FREE(ps_dec, ps_dec->ps_dpb_cmds);
    PS_DEC_ALIGNED_FREE(ps_dec, ps_dec->ps_bitstrm);
    PS_DEC_ALIGNED_FREE(ps_dec, ps_dec->ps_cur_slice);
    PS_DEC_ALIGNED_FREE(ps_dec, ps_dec->pv_scratch_sps_pps);
    PS_DEC_ALIGNED_FREE(ps_dec, ps_dec->pu1_bits_buf_static);
    PS_DEC_ALIGNED_FREE(ps_dec, ps_dec->ppv_map_ref_idx_to_poc_base);
    PS_DEC_ALIGNED_FREE(ps_dec, ps_dec->p_cabac_ctxt_table_t);
    PS_DEC_ALIGNED_FREE(ps_dec, ps_dec->ps_left_mb_ctxt_info);
    PS_DEC_ALIGNED_FREE(ps_dec, ps_dec->pu1_ref_buff_base);
    PS_DEC_ALIGNED_FREE(ps_dec, ps_dec->pi2_pred1);
    PS_DEC_ALIGNED_FREE(ps_dec, ps_dec->pu1_temp_mc_buffer);
    PS_DEC_ALIGNED_FREE(ps_dec, ps_dec->pu1_init_dpb_base);
    PS_DEC_ALIGNED_FREE(ps_dec, ps_dec->pu4_mbaff_wt_mat);
    PS_DEC_ALIGNED_FREE(ps_dec, ps_dec->pu4_wts_ofsts_mat);
    PS_DEC_ALIGNED_FREE(ps_dec, ps_dec->ps_left_mvpred_addr);
    PS_DEC_ALIGNED_FREE(ps_dec, ps_dec->pv_mv_buf_mgr);
    PS_DEC_ALIGNED_FREE(ps_dec, ps_dec->ps_col_mv_base);
    PS_DEC_ALIGNED_FREE(ps_dec, dec_hdl->pv_codec_handle);

    if(dec_hdl)
    {
        pf_aligned_free(pv_mem_ctxt, dec_hdl);
    }
    return IV_SUCCESS;
}
/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_create                                              */
/*                                                                           */
/*  Description   : creates decoder                                          */
/*                                                                           */
/*  Inputs        :iv_obj_t decoder handle                                   */
/*                :pv_api_ip pointer to input structure                      */
/*                :pv_api_op pointer to output structure                     */
/*  Outputs       :                                                          */
/*  Returns       : void                                                     */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         22 10 2008    100356         Draft                                */
/*                                                                           */
/*****************************************************************************/
WORD32 ih264d_allocate_static_bufs(iv_obj_t **dec_hdl, void *pv_api_ip, void *pv_api_op)
{
    ih264d_create_ip_t *ps_create_ip;
    ih264d_create_op_t *ps_create_op;
    void *pv_buf;
    UWORD8 *pu1_buf;
    dec_struct_t *ps_dec;
    void *(*pf_aligned_alloc)(void *pv_mem_ctxt, WORD32 alignment, WORD32 size);
    void (*pf_aligned_free)(void *pv_mem_ctxt, void *pv_buf);
    void *pv_mem_ctxt;
    WORD32 size;

    ps_create_ip = (ih264d_create_ip_t *)pv_api_ip;
    ps_create_op = (ih264d_create_op_t *)pv_api_op;

    ps_create_op->s_ivd_create_op_t.u4_error_code = 0;

    pf_aligned_alloc = ps_create_ip->s_ivd_create_ip_t.pf_aligned_alloc;
    pf_aligned_free = ps_create_ip->s_ivd_create_ip_t.pf_aligned_free;
    pv_mem_ctxt  = ps_create_ip->s_ivd_create_ip_t.pv_mem_ctxt;

    /* Initialize return handle to NULL */
    ps_create_op->s_ivd_create_op_t.pv_handle = NULL;
    pv_buf = pf_aligned_alloc(pv_mem_ctxt, 128, sizeof(iv_obj_t));
    RETURN_IF((NULL == pv_buf), IV_FAIL);
    memset(pv_buf, 0, sizeof(iv_obj_t));
    *dec_hdl = (iv_obj_t *)pv_buf;
    ps_create_op->s_ivd_create_op_t.pv_handle = *dec_hdl;

    (*dec_hdl)->pv_codec_handle = NULL;
    pv_buf = pf_aligned_alloc(pv_mem_ctxt, 128, sizeof(dec_struct_t));
    RETURN_IF((NULL == pv_buf), IV_FAIL);
    (*dec_hdl)->pv_codec_handle = (dec_struct_t *)pv_buf;
    ps_dec = (dec_struct_t *)pv_buf;

    memset(ps_dec, 0, sizeof(dec_struct_t));

#ifndef LOGO_EN
    ps_dec->u4_share_disp_buf = ps_create_ip->s_ivd_create_ip_t.u4_share_disp_buf;
#else
    ps_dec->u4_share_disp_buf = 0;
#endif

    ps_dec->u1_chroma_format =
                    (UWORD8)(ps_create_ip->s_ivd_create_ip_t.e_output_format);

    if((ps_dec->u1_chroma_format != IV_YUV_420P)
                    && (ps_dec->u1_chroma_format
                                    != IV_YUV_420SP_UV)
                    && (ps_dec->u1_chroma_format
                                    != IV_YUV_420SP_VU))
    {
        ps_dec->u4_share_disp_buf = 0;
    }

    ps_dec->pf_aligned_alloc = pf_aligned_alloc;
    ps_dec->pf_aligned_free = pf_aligned_free;
    ps_dec->pv_mem_ctxt = pv_mem_ctxt;


    size = ((sizeof(dec_seq_params_t)) * MAX_NUM_SEQ_PARAMS);
    pv_buf = pf_aligned_alloc(pv_mem_ctxt, 128, size);
    RETURN_IF((NULL == pv_buf), IV_FAIL);
    memset(pv_buf, 0, size);
    ps_dec->ps_sps = pv_buf;

    size = (sizeof(dec_pic_params_t)) * MAX_NUM_PIC_PARAMS;
    pv_buf = pf_aligned_alloc(pv_mem_ctxt, 128, size);
    RETURN_IF((NULL == pv_buf), IV_FAIL);
    memset(pv_buf, 0, size);
    ps_dec->ps_pps = pv_buf;

    size = ithread_get_handle_size();
    pv_buf = pf_aligned_alloc(pv_mem_ctxt, 128, size);
    RETURN_IF((NULL == pv_buf), IV_FAIL);
    memset(pv_buf, 0, size);
    ps_dec->pv_dec_thread_handle = pv_buf;

    size = ithread_get_handle_size();
    pv_buf = pf_aligned_alloc(pv_mem_ctxt, 128, size);
    RETURN_IF((NULL == pv_buf), IV_FAIL);
    memset(pv_buf, 0, size);
    ps_dec->pv_bs_deblk_thread_handle = pv_buf;

    size = sizeof(dpb_manager_t);
    pv_buf = pf_aligned_alloc(pv_mem_ctxt, 128, size);
    RETURN_IF((NULL == pv_buf), IV_FAIL);
    memset(pv_buf, 0, size);
    ps_dec->ps_dpb_mgr = pv_buf;

    size = sizeof(pred_info_t) * 2 * 32;
    pv_buf = pf_aligned_alloc(pv_mem_ctxt, 128, size);
    RETURN_IF((NULL == pv_buf), IV_FAIL);
    memset(pv_buf, 0, size);
    ps_dec->ps_pred = pv_buf;

    size = sizeof(disp_mgr_t);
    pv_buf = pf_aligned_alloc(pv_mem_ctxt, 128, size);
    RETURN_IF((NULL == pv_buf), IV_FAIL);
    memset(pv_buf, 0, size);
    ps_dec->pv_disp_buf_mgr = pv_buf;

    size = sizeof(buf_mgr_t) + ithread_get_mutex_lock_size();
    pv_buf = pf_aligned_alloc(pv_mem_ctxt, 128, size);
    RETURN_IF((NULL == pv_buf), IV_FAIL);
    memset(pv_buf, 0, size);
    ps_dec->pv_pic_buf_mgr = pv_buf;

    size = sizeof(struct pic_buffer_t) * (H264_MAX_REF_PICS * 2);
    pv_buf = pf_aligned_alloc(pv_mem_ctxt, 128, size);
    RETURN_IF((NULL == pv_buf), IV_FAIL);
    memset(pv_buf, 0, size);
    ps_dec->ps_pic_buf_base = pv_buf;

    size = sizeof(dec_err_status_t);
    pv_buf = pf_aligned_alloc(pv_mem_ctxt, 128, size);
    RETURN_IF((NULL == pv_buf), IV_FAIL);
    memset(pv_buf, 0, size);
    ps_dec->ps_dec_err_status = (dec_err_status_t *)pv_buf;

    size = sizeof(sei);
    pv_buf = pf_aligned_alloc(pv_mem_ctxt, 128, size);
    RETURN_IF((NULL == pv_buf), IV_FAIL);
    memset(pv_buf, 0, size);
    ps_dec->ps_sei = (sei *)pv_buf;

    size = sizeof(sei);
    pv_buf = pf_aligned_alloc(pv_mem_ctxt, 128, size);
    RETURN_IF((NULL == pv_buf), IV_FAIL);
    memset(pv_buf, 0, size);
    ps_dec->ps_sei_parse = (sei *)pv_buf;

    size = sizeof(dpb_commands_t);
    pv_buf = pf_aligned_alloc(pv_mem_ctxt, 128, size);
    RETURN_IF((NULL == pv_buf), IV_FAIL);
    memset(pv_buf, 0, size);
    ps_dec->ps_dpb_cmds = (dpb_commands_t *)pv_buf;

    size = sizeof(dec_bit_stream_t);
    pv_buf = pf_aligned_alloc(pv_mem_ctxt, 128, size);
    RETURN_IF((NULL == pv_buf), IV_FAIL);
    memset(pv_buf, 0, size);
    ps_dec->ps_bitstrm = (dec_bit_stream_t *)pv_buf;

    size = sizeof(dec_slice_params_t);
    pv_buf = pf_aligned_alloc(pv_mem_ctxt, 128, size);
    RETURN_IF((NULL == pv_buf), IV_FAIL);
    memset(pv_buf, 0, size);
    ps_dec->ps_cur_slice = (dec_slice_params_t *)pv_buf;

    size = MAX(sizeof(dec_seq_params_t), sizeof(dec_pic_params_t));
    pv_buf = pf_aligned_alloc(pv_mem_ctxt, 128, size);
    RETURN_IF((NULL == pv_buf), IV_FAIL);
    memset(pv_buf, 0, size);
    ps_dec->pv_scratch_sps_pps = pv_buf;


    ps_dec->u4_static_bits_buf_size = 256000;
    pv_buf = pf_aligned_alloc(pv_mem_ctxt, 128, ps_dec->u4_static_bits_buf_size);
    RETURN_IF((NULL == pv_buf), IV_FAIL);
    memset(pv_buf, 0, ps_dec->u4_static_bits_buf_size);
    ps_dec->pu1_bits_buf_static = pv_buf;


    size = ((TOTAL_LIST_ENTRIES + PAD_MAP_IDX_POC)
                        * sizeof(void *));
    pv_buf = pf_aligned_alloc(pv_mem_ctxt, 128, size);
    RETURN_IF((NULL == pv_buf), IV_FAIL);
    ps_dec->ppv_map_ref_idx_to_poc_base = pv_buf;
    memset(ps_dec->ppv_map_ref_idx_to_poc_base, 0, size);

    ps_dec->ppv_map_ref_idx_to_poc = ps_dec->ppv_map_ref_idx_to_poc_base + OFFSET_MAP_IDX_POC;


    size = (sizeof(bin_ctxt_model_t) * NUM_CABAC_CTXTS);
    pv_buf = pf_aligned_alloc(pv_mem_ctxt, 128, size);
    RETURN_IF((NULL == pv_buf), IV_FAIL);
    memset(pv_buf, 0, size);
    ps_dec->p_cabac_ctxt_table_t = pv_buf;



    size = sizeof(ctxt_inc_mb_info_t);
    pv_buf = pf_aligned_alloc(pv_mem_ctxt, 128, size);
    RETURN_IF((NULL == pv_buf), IV_FAIL);
    memset(pv_buf, 0, size);
    ps_dec->ps_left_mb_ctxt_info = pv_buf;



    size = MAX_REF_BUF_SIZE * 2;
    pv_buf = pf_aligned_alloc(pv_mem_ctxt, 128, size);
    RETURN_IF((NULL == pv_buf), IV_FAIL);
    memset(pv_buf, 0, size);
    ps_dec->pu1_ref_buff_base = pv_buf;
    ps_dec->pu1_ref_buff = ps_dec->pu1_ref_buff_base + MAX_REF_BUF_SIZE;


    size = ((sizeof(WORD16)) * PRED_BUFFER_WIDTH
                        * PRED_BUFFER_HEIGHT * 2);
    pv_buf = pf_aligned_alloc(pv_mem_ctxt, 128, size);
    RETURN_IF((NULL == pv_buf), IV_FAIL);
    memset(pv_buf, 0, size);
    ps_dec->pi2_pred1 = pv_buf;


    size = sizeof(UWORD8) * (MB_LUM_SIZE);
    pv_buf = pf_aligned_alloc(pv_mem_ctxt, 128, size);
    RETURN_IF((NULL == pv_buf), IV_FAIL);
    memset(pv_buf, 0, size);
    ps_dec->pu1_temp_mc_buffer = pv_buf;




    size = 8 * MAX_REF_BUFS * sizeof(struct pic_buffer_t);
    pv_buf = pf_aligned_alloc(pv_mem_ctxt, 128, size);
    RETURN_IF((NULL == pv_buf), IV_FAIL);
    memset(pv_buf, 0, size);

    ps_dec->pu1_init_dpb_base = pv_buf;
    pu1_buf = pv_buf;
    ps_dec->ps_dpb_mgr->ps_init_dpb[0][0] = (struct pic_buffer_t *)pu1_buf;

    pu1_buf += size / 2;
    ps_dec->ps_dpb_mgr->ps_init_dpb[1][0] = (struct pic_buffer_t *)pu1_buf;

    size = (sizeof(UWORD32) * 2 * 3
                        * ((MAX_FRAMES << 1) * (MAX_FRAMES << 1)) * 2);
    pv_buf = pf_aligned_alloc(pv_mem_ctxt, 128, size);
    RETURN_IF((NULL == pv_buf), IV_FAIL);
    memset(pv_buf, 0, size);
    ps_dec->pu4_mbaff_wt_mat = pv_buf;

    size = sizeof(UWORD32) * 2 * 3
                        * ((MAX_FRAMES << 1) * (MAX_FRAMES << 1));
    pv_buf = pf_aligned_alloc(pv_mem_ctxt, 128, size);
    RETURN_IF((NULL == pv_buf), IV_FAIL);
    memset(pv_buf, 0, size);
    ps_dec->pu4_wts_ofsts_mat = pv_buf;


    size = (sizeof(neighbouradd_t) << 2);
    pv_buf = pf_aligned_alloc(pv_mem_ctxt, 128, size);
    RETURN_IF((NULL == pv_buf), IV_FAIL);
    memset(pv_buf, 0, size);
    ps_dec->ps_left_mvpred_addr = pv_buf;


    size = sizeof(buf_mgr_t) + ithread_get_mutex_lock_size();
    pv_buf = pf_aligned_alloc(pv_mem_ctxt, 128, size);
    RETURN_IF((NULL == pv_buf), IV_FAIL);
    memset(pv_buf, 0, size);
    ps_dec->pv_mv_buf_mgr = pv_buf;


    size =  sizeof(col_mv_buf_t) * (H264_MAX_REF_PICS * 2);
    pv_buf = pf_aligned_alloc(pv_mem_ctxt, 128, size);
    RETURN_IF((NULL == pv_buf), IV_FAIL);
    ps_dec->ps_col_mv_base = pv_buf;
    memset(ps_dec->ps_col_mv_base, 0, size);

    ih264d_init_decoder(ps_dec);

    return IV_SUCCESS;
}


/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_create                                              */
/*                                                                           */
/*  Description   : creates decoder                                          */
/*                                                                           */
/*  Inputs        :iv_obj_t decoder handle                                   */
/*                :pv_api_ip pointer to input structure                      */
/*                :pv_api_op pointer to output structure                     */
/*  Outputs       :                                                          */
/*  Returns       : void                                                     */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         22 10 2008    100356         Draft                                */
/*                                                                           */
/*****************************************************************************/
WORD32 ih264d_create(iv_obj_t *dec_hdl, void *pv_api_ip, void *pv_api_op)
{
    ih264d_create_ip_t *ps_create_ip;
    ih264d_create_op_t *ps_create_op;

    WORD32 ret;

    ps_create_ip = (ih264d_create_ip_t *)pv_api_ip;
    ps_create_op = (ih264d_create_op_t *)pv_api_op;

    ps_create_op->s_ivd_create_op_t.u4_error_code = 0;
    dec_hdl = NULL;
    ret = ih264d_allocate_static_bufs(&dec_hdl, pv_api_ip, pv_api_op);

    /* If allocation of some buffer fails, then free buffers allocated till then */
    if(IV_FAIL == ret)
    {
        if(dec_hdl)
        {
            if(dec_hdl->pv_codec_handle)
            {
                ih264d_free_static_bufs(dec_hdl);
            }
            else
            {
                void (*pf_aligned_free)(void *pv_mem_ctxt, void *pv_buf);
                void *pv_mem_ctxt;

                pf_aligned_free = ps_create_ip->s_ivd_create_ip_t.pf_aligned_free;
                pv_mem_ctxt  = ps_create_ip->s_ivd_create_ip_t.pv_mem_ctxt;
                pf_aligned_free(pv_mem_ctxt, dec_hdl);
            }
        }
        ps_create_op->s_ivd_create_op_t.u4_error_code = IVD_MEM_ALLOC_FAILED;
        ps_create_op->s_ivd_create_op_t.u4_error_code |= 1 << IVD_FATALERROR;

        return IV_FAIL;
    }

    return IV_SUCCESS;
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name :  ih264d_map_error                                        */
/*                                                                           */
/*  Description   :  Maps error codes to IVD error groups                    */
/*                                                                           */
/*  Inputs        :                                                          */
/*  Globals       : <Does it use any global variables?>                      */
/*  Outputs       :                                                          */
/*  Returns       : void                                                     */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         22 10 2008    100356         Draft                                */
/*                                                                           */
/*****************************************************************************/
UWORD32 ih264d_map_error(UWORD32 i4_err_status)
{
    UWORD32 temp = 0;

    switch(i4_err_status)
    {
        case ERROR_MEM_ALLOC_ISRAM_T:
        case ERROR_MEM_ALLOC_SDRAM_T:
        case ERROR_BUF_MGR:
        case ERROR_MB_GROUP_ASSGN_T:
        case ERROR_FRAME_LIMIT_OVER:
        case ERROR_ACTUAL_RESOLUTION_GREATER_THAN_INIT:
        case ERROR_PROFILE_NOT_SUPPORTED:
        case ERROR_INIT_NOT_DONE:
        case IVD_MEM_ALLOC_FAILED:
        case ERROR_FEATURE_UNAVAIL:
        case IVD_STREAM_WIDTH_HEIGHT_NOT_SUPPORTED:
            temp = 1 << IVD_FATALERROR;
            H264_DEC_DEBUG_PRINT("\nFatal Error\n");
            break;

        case ERROR_DBP_MANAGER_T:
        case ERROR_GAPS_IN_FRM_NUM:
        case ERROR_UNKNOWN_NAL:
        case ERROR_INV_MB_SLC_GRP_T:
        case ERROR_MULTIPLE_SLC_GRP_T:
        case ERROR_UNKNOWN_LEVEL:
        case ERROR_UNAVAIL_PICBUF_T:
        case ERROR_UNAVAIL_MVBUF_T:
        case ERROR_UNAVAIL_DISPBUF_T:
        case ERROR_NUM_REF:
        case ERROR_REFIDX_ORDER_T:
        case ERROR_PIC0_NOT_FOUND_T:
        case ERROR_MB_TYPE:
        case ERROR_SUB_MB_TYPE:
        case ERROR_CBP:
        case ERROR_REF_IDX:
        case ERROR_NUM_MV:
        case ERROR_CHROMA_PRED_MODE:
        case ERROR_INTRAPRED:
        case ERROR_NEXT_MB_ADDRESS_T:
        case ERROR_MB_ADDRESS_T:
        case ERROR_PIC1_NOT_FOUND_T:
        case ERROR_CAVLC_NUM_COEFF_T:
        case ERROR_CAVLC_SCAN_POS_T:
        case ERROR_PRED_WEIGHT_TABLE_T:
        case ERROR_CORRUPTED_SLICE:
            temp = 1 << IVD_CORRUPTEDDATA;
            break;

        case ERROR_NOT_SUPP_RESOLUTION:
        case ERROR_ACTUAL_LEVEL_GREATER_THAN_INIT:
            temp = 1 << IVD_UNSUPPORTEDINPUT;
            break;

        case ERROR_INVALID_PIC_PARAM:
        case ERROR_INVALID_SEQ_PARAM:
        case ERROR_EGC_EXCEED_32_1_T:
        case ERROR_EGC_EXCEED_32_2_T:
        case ERROR_INV_RANGE_TEV_T:
        case ERROR_INV_SLC_TYPE_T:
        case ERROR_INV_POC_TYPE_T:
        case ERROR_INV_RANGE_QP_T:
        case ERROR_INV_SPS_PPS_T:
        case ERROR_INV_SLICE_HDR_T:
        case ERROR_INV_SEI_MDCV_PARAMS:
        case ERROR_INV_SEI_CLL_PARAMS:
        case ERROR_INV_SEI_AVE_PARAMS:
        case ERROR_INV_SEI_CCV_PARAMS:
            temp = 1 << IVD_CORRUPTEDHEADER;
            break;

        case ERROR_EOB_FLUSHBITS_T:
        case ERROR_EOB_GETBITS_T:
        case ERROR_EOB_GETBIT_T:
        case ERROR_EOB_BYPASS_T:
        case ERROR_EOB_DECISION_T:
        case ERROR_EOB_TERMINATE_T:
        case ERROR_EOB_READCOEFF4X4CAB_T:
            temp = 1 << IVD_INSUFFICIENTDATA;
            break;
        case ERROR_DYNAMIC_RESOLUTION_NOT_SUPPORTED:
        case ERROR_DISP_WIDTH_RESET_TO_PIC_WIDTH:
            temp = 1 << IVD_UNSUPPORTEDPARAM | 1 << IVD_FATALERROR;
            break;

        case ERROR_DANGLING_FIELD_IN_PIC:
            temp = 1 << IVD_APPLIEDCONCEALMENT;
            break;

    }

    return temp;

}

UWORD32 ih264d_get_outbuf_size(WORD32 pic_wd,
                               UWORD32 pic_ht,
                               UWORD8 u1_chroma_format,
                               UWORD32 *p_buf_size)
{
    UWORD32 u4_min_num_out_bufs = 0;

    if(u1_chroma_format == IV_YUV_420P)
        u4_min_num_out_bufs = MIN_OUT_BUFS_420;
    else if(u1_chroma_format == IV_YUV_422ILE)
        u4_min_num_out_bufs = MIN_OUT_BUFS_422ILE;
    else if(u1_chroma_format == IV_RGB_565)
        u4_min_num_out_bufs = MIN_OUT_BUFS_RGB565;
    else if((u1_chroma_format == IV_YUV_420SP_UV)
                    || (u1_chroma_format == IV_YUV_420SP_VU))
        u4_min_num_out_bufs = MIN_OUT_BUFS_420SP;

    if(u1_chroma_format == IV_YUV_420P)
    {
        p_buf_size[0] = (pic_wd * pic_ht);
        p_buf_size[1] = (pic_wd * pic_ht) >> 2;
        p_buf_size[2] = (pic_wd * pic_ht) >> 2;
    }
    else if(u1_chroma_format == IV_YUV_422ILE)
    {
        p_buf_size[0] = (pic_wd * pic_ht) * 2;
        p_buf_size[1] = p_buf_size[2] = 0;
    }
    else if(u1_chroma_format == IV_RGB_565)
    {
        p_buf_size[0] = (pic_wd * pic_ht) * 2;
        p_buf_size[1] = p_buf_size[2] = 0;
    }
    else if((u1_chroma_format == IV_YUV_420SP_UV)
                    || (u1_chroma_format == IV_YUV_420SP_VU))
    {
        p_buf_size[0] = (pic_wd * pic_ht);
        p_buf_size[1] = (pic_wd * pic_ht) >> 1;
        p_buf_size[2] = 0;
    }

    return u4_min_num_out_bufs;
}

WORD32 check_app_out_buf_size(dec_struct_t *ps_dec)
{
    UWORD32 au4_min_out_buf_size[IVD_VIDDEC_MAX_IO_BUFFERS];
    UWORD32 u4_min_num_out_bufs, i;
    UWORD32 pic_wd, pic_ht;

    if(0 == ps_dec->u4_share_disp_buf)
    {
        pic_wd = ps_dec->u2_disp_width;
        pic_ht = ps_dec->u2_disp_height;

    }
    else
    {
        pic_wd = ps_dec->u2_frm_wd_y;
        pic_ht = ps_dec->u2_frm_ht_y;
    }

    if(ps_dec->u4_app_disp_width > pic_wd)
        pic_wd = ps_dec->u4_app_disp_width;

    u4_min_num_out_bufs = ih264d_get_outbuf_size(pic_wd, pic_ht,
                                                 ps_dec->u1_chroma_format,
                                                 &au4_min_out_buf_size[0]);


    if(0 == ps_dec->u4_share_disp_buf)
    {
        if(ps_dec->ps_out_buffer->u4_num_bufs < u4_min_num_out_bufs)
            return IV_FAIL;

        for(i = 0; i < u4_min_num_out_bufs; i++)
        {
            if(ps_dec->ps_out_buffer->u4_min_out_buf_size[i]
                            < au4_min_out_buf_size[i])
                return (IV_FAIL);
        }
    }
    else
    {
        if(ps_dec->disp_bufs[0].u4_num_bufs < u4_min_num_out_bufs)
            return IV_FAIL;

        for(i = 0; i < u4_min_num_out_bufs; i++)
        {
            /* We need to check only with the disp_buffer[0], because we have
             * already ensured that all the buffers are of the same size in
             * ih264d_set_display_frame.
             */
            if(ps_dec->disp_bufs[0].u4_bufsize[i] < au4_min_out_buf_size[i])
                return (IV_FAIL);
        }

    }

    return (IV_SUCCESS);
}


/*****************************************************************************/
/*                                                                           */
/*  Function Name :  ih264d_video_decode                                     */
/*                                                                           */
/*  Description   :  handle video decode API command                         */
/*                                                                           */
/*  Inputs        :iv_obj_t decoder handle                                   */
/*                :pv_api_ip pointer to input structure                      */
/*                :pv_api_op pointer to output structure                     */
/*  Outputs       :                                                          */
/*  Returns       : void                                                     */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         22 10 2008    100356         Draft                                */
/*                                                                           */
/*****************************************************************************/

WORD32 ih264d_video_decode(iv_obj_t *dec_hdl, void *pv_api_ip, void *pv_api_op)
{
    /* ! */

    dec_struct_t * ps_dec = (dec_struct_t *)(dec_hdl->pv_codec_handle);

    WORD32 i4_err_status = 0;
    UWORD8 *pu1_buf = NULL;
    WORD32 buflen;
    UWORD32 u4_max_ofst, u4_length_of_start_code = 0;

    UWORD32 bytes_consumed = 0;
    UWORD32 cur_slice_is_nonref = 0;
    UWORD32 u4_next_is_aud;
    UWORD32 u4_first_start_code_found = 0;
    WORD32 ret = 0,api_ret_value = IV_SUCCESS;
    WORD32 header_data_left = 0,frame_data_left = 0;
    UWORD8 *pu1_bitstrm_buf;
    ih264d_video_decode_ip_t *ps_h264d_dec_ip;
    ih264d_video_decode_op_t *ps_h264d_dec_op;
    ivd_video_decode_ip_t *ps_dec_ip;
    ivd_video_decode_op_t *ps_dec_op;

    ithread_set_name((void*)"Parse_thread");

    ps_h264d_dec_ip = (ih264d_video_decode_ip_t *)pv_api_ip;
    ps_h264d_dec_op = (ih264d_video_decode_op_t *)pv_api_op;
    ps_dec_ip = &ps_h264d_dec_ip->s_ivd_video_decode_ip_t;
    ps_dec_op = &ps_h264d_dec_op->s_ivd_video_decode_op_t;

    {
        UWORD32 u4_size;
        u4_size = ps_dec_op->u4_size;
        memset(ps_h264d_dec_op, 0, sizeof(ih264d_video_decode_op_t));
        ps_dec_op->u4_size = u4_size;
    }

    ps_dec->pv_dec_out = ps_dec_op;
    if(ps_dec->init_done != 1)
    {
        return IV_FAIL;
    }

    /*Data memory barries instruction,so that bitstream write by the application is complete*/
    DATA_SYNC();

    if(0 == ps_dec->u1_flushfrm)
    {
        if(ps_dec_ip->pv_stream_buffer == NULL)
        {
            ps_dec_op->u4_error_code |= 1 << IVD_UNSUPPORTEDPARAM;
            ps_dec_op->u4_error_code |= IVD_DEC_FRM_BS_BUF_NULL;
            return IV_FAIL;
        }
        if(ps_dec_ip->u4_num_Bytes <= 0)
        {
            ps_dec_op->u4_error_code |= 1 << IVD_UNSUPPORTEDPARAM;
            ps_dec_op->u4_error_code |= IVD_DEC_NUMBYTES_INV;
            return IV_FAIL;

        }
    }
    ps_dec->u1_pic_decode_done = 0;

    ps_dec_op->u4_num_bytes_consumed = 0;
    ps_dec_op->i4_reorder_depth = -1;
    ps_dec_op->i4_display_index = DEFAULT_POC;
    ps_dec->ps_out_buffer = NULL;

    if(ps_dec_ip->u4_size
                    >= offsetof(ivd_video_decode_ip_t, s_out_buffer))
        ps_dec->ps_out_buffer = &ps_dec_ip->s_out_buffer;

    ps_dec->u4_fmt_conv_cur_row = 0;

    ps_dec->u4_output_present = 0;
    ps_dec->s_disp_op.u4_error_code = 1;
    ps_dec->u4_fmt_conv_num_rows = FMT_CONV_NUM_ROWS;
    if(0 == ps_dec->u4_share_disp_buf
                    && ps_dec->i4_decode_header == 0)
    {
        UWORD32 i;
        if((ps_dec->ps_out_buffer->u4_num_bufs == 0) ||
           (ps_dec->ps_out_buffer->u4_num_bufs > IVD_VIDDEC_MAX_IO_BUFFERS))
        {
            ps_dec_op->u4_error_code |= 1 << IVD_UNSUPPORTEDPARAM;
            ps_dec_op->u4_error_code |= IVD_DISP_FRM_ZERO_OP_BUFS;
            return IV_FAIL;
        }

        for(i = 0; i < ps_dec->ps_out_buffer->u4_num_bufs; i++)
        {
            if(ps_dec->ps_out_buffer->pu1_bufs[i] == NULL)
            {
                ps_dec_op->u4_error_code |= 1 << IVD_UNSUPPORTEDPARAM;
                ps_dec_op->u4_error_code |= IVD_DISP_FRM_OP_BUF_NULL;
                return IV_FAIL;
            }

            if(ps_dec->ps_out_buffer->u4_min_out_buf_size[i] == 0)
            {
                ps_dec_op->u4_error_code |= 1 << IVD_UNSUPPORTEDPARAM;
                ps_dec_op->u4_error_code |=
                                IVD_DISP_FRM_ZERO_OP_BUF_SIZE;
                return IV_FAIL;
            }
        }
    }

    if(ps_dec->u4_total_frames_decoded >= NUM_FRAMES_LIMIT)
    {
        ps_dec_op->u4_error_code = ERROR_FRAME_LIMIT_OVER;
        return IV_FAIL;
    }

    /* ! */
    ps_dec->u4_ts = ps_dec_ip->u4_ts;

    ps_dec_op->u4_error_code = 0;
    ps_dec_op->e_pic_type = IV_NA_FRAME;
    ps_dec_op->u4_output_present = 0;
    ps_dec_op->u4_frame_decoded_flag = 0;

    ps_dec->i4_frametype = IV_NA_FRAME;
    ps_dec->i4_content_type = IV_CONTENTTYPE_NA;

    ps_dec->u4_slice_start_code_found = 0;

    /* In case the deocder is not in flush mode(in shared mode),
     then decoder has to pick up a buffer to write current frame.
     Check if a frame is available in such cases */

    if(ps_dec->u1_init_dec_flag == 1 && ps_dec->u4_share_disp_buf == 1
                    && ps_dec->u1_flushfrm == 0)
    {
        UWORD32 i;

        WORD32 disp_avail = 0, free_id;

        /* Check if at least one buffer is available with the codec */
        /* If not then return to application with error */
        for(i = 0; i < ps_dec->u1_pic_bufs; i++)
        {
            if(0 == ps_dec->u4_disp_buf_mapping[i]
                            || 1 == ps_dec->u4_disp_buf_to_be_freed[i])
            {
                disp_avail = 1;
                break;
            }

        }

        if(0 == disp_avail)
        {
            /* If something is queued for display wait for that buffer to be returned */

            ps_dec_op->u4_error_code = IVD_DEC_REF_BUF_NULL;
            ps_dec_op->u4_error_code |= (1 << IVD_UNSUPPORTEDPARAM);
            return (IV_FAIL);
        }

        while(1)
        {
            pic_buffer_t *ps_pic_buf;
            ps_pic_buf = (pic_buffer_t *)ih264_buf_mgr_get_next_free(
                            (buf_mgr_t *)ps_dec->pv_pic_buf_mgr, &free_id);

            if(ps_pic_buf == NULL)
            {
                UWORD32 i, display_queued = 0;

                /* check if any buffer was given for display which is not returned yet */
                for(i = 0; i < (MAX_DISP_BUFS_NEW); i++)
                {
                    if(0 != ps_dec->u4_disp_buf_mapping[i])
                    {
                        display_queued = 1;
                        break;
                    }
                }
                /* If some buffer is queued for display, then codec has to singal an error and wait
                 for that buffer to be returned.
                 If nothing is queued for display then codec has ownership of all display buffers
                 and it can reuse any of the existing buffers and continue decoding */

                if(1 == display_queued)
                {
                    /* If something is queued for display wait for that buffer to be returned */
                    ps_dec_op->u4_error_code = IVD_DEC_REF_BUF_NULL;
                    ps_dec_op->u4_error_code |= (1
                                    << IVD_UNSUPPORTEDPARAM);
                    return (IV_FAIL);
                }
            }
            else
            {
                /* If the buffer is with display, then mark it as in use and then look for a buffer again */
                if(1 == ps_dec->u4_disp_buf_mapping[free_id])
                {
                    ih264_buf_mgr_set_status(
                                    (buf_mgr_t *)ps_dec->pv_pic_buf_mgr,
                                    free_id,
                                    BUF_MGR_IO);
                }
                else
                {
                    /**
                     *  Found a free buffer for present call. Release it now.
                     *  Will be again obtained later.
                     */
                    ih264_buf_mgr_release((buf_mgr_t *)ps_dec->pv_pic_buf_mgr,
                                          free_id,
                                          BUF_MGR_IO);
                    break;
                }
            }
        }

    }

    if(ps_dec->u1_flushfrm)
    {
        if(ps_dec->u1_init_dec_flag == 0)
        {
            /*Come out of flush mode and return*/
            ps_dec->u1_flushfrm = 0;
            return (IV_FAIL);
        }



        ih264d_get_next_display_field(ps_dec, ps_dec->ps_out_buffer,
                                      &(ps_dec->s_disp_op));
        if(0 == ps_dec->s_disp_op.u4_error_code)
        {
            /* check output buffer size given by the application */
            if(check_app_out_buf_size(ps_dec) != IV_SUCCESS)
            {
                ps_dec_op->u4_error_code= IVD_DISP_FRM_ZERO_OP_BUF_SIZE;
                return (IV_FAIL);
            }

            ps_dec->u4_fmt_conv_cur_row = 0;
            ps_dec->u4_fmt_conv_num_rows = ps_dec->s_disp_frame_info.u4_y_ht;
            ih264d_format_convert(ps_dec, &(ps_dec->s_disp_op),
                                  ps_dec->u4_fmt_conv_cur_row,
                                  ps_dec->u4_fmt_conv_num_rows);
            ps_dec->u4_fmt_conv_cur_row += ps_dec->u4_fmt_conv_num_rows;
            ps_dec->u4_output_present = 1;

        }
        ih264d_export_sei_params(&ps_dec_op->s_sei_decode_op, ps_dec);

        ih264d_release_display_field(ps_dec, &(ps_dec->s_disp_op));

        ps_dec_op->u4_pic_wd = (UWORD32)ps_dec->u2_disp_width;
        ps_dec_op->u4_pic_ht = (UWORD32)ps_dec->u2_disp_height;
        ps_dec_op->i4_reorder_depth = ps_dec->i4_reorder_depth;
        ps_dec_op->i4_display_index = ps_dec->i4_display_index;

        ps_dec_op->u4_new_seq = 0;

        ps_dec_op->u4_output_present = ps_dec->u4_output_present;
        ps_dec_op->u4_progressive_frame_flag =
                        ps_dec->s_disp_op.u4_progressive_frame_flag;
        ps_dec_op->e_output_format =
                        ps_dec->s_disp_op.e_output_format;
        ps_dec_op->s_disp_frm_buf = ps_dec->s_disp_op.s_disp_frm_buf;
        ps_dec_op->e4_fld_type = ps_dec->s_disp_op.e4_fld_type;
        ps_dec_op->u4_ts = ps_dec->s_disp_op.u4_ts;
        ps_dec_op->u4_disp_buf_id = ps_dec->s_disp_op.u4_disp_buf_id;

        /*In the case of flush ,since no frame is decoded set pic type as invalid*/
        ps_dec_op->u4_is_ref_flag = -1;
        ps_dec_op->e_pic_type = IV_NA_FRAME;
        ps_dec_op->u4_frame_decoded_flag = 0;

        if(0 == ps_dec->s_disp_op.u4_error_code)
        {
            return (IV_SUCCESS);
        }
        else
            return (IV_FAIL);

    }
    if(ps_dec->u1_res_changed == 1)
    {
        /*if resolution has changed and all buffers have been flushed, reset decoder*/
        ih264d_init_decoder(ps_dec);
    }

    ps_dec->u2_cur_mb_addr = 0;
    ps_dec->u2_total_mbs_coded = 0;
    ps_dec->u2_cur_slice_num = 0;
    ps_dec->cur_dec_mb_num = 0;
    ps_dec->cur_recon_mb_num = 0;
    ps_dec->u4_first_slice_in_pic = 1;
    ps_dec->u1_slice_header_done = 0;
    ps_dec->u1_dangling_field = 0;

    ps_dec->u4_dec_thread_created = 0;
    ps_dec->u4_bs_deblk_thread_created = 0;
    ps_dec->u4_cur_bs_mb_num = 0;
    ps_dec->u4_start_recon_deblk  = 0;
    ps_dec->u4_sps_cnt_in_process = 0;

    DEBUG_THREADS_PRINTF(" Starting process call\n");


    ps_dec->u4_pic_buf_got = 0;

    do
    {
        WORD32 buf_size;

        pu1_buf = (UWORD8*)ps_dec_ip->pv_stream_buffer
                        + ps_dec_op->u4_num_bytes_consumed;

        u4_max_ofst = ps_dec_ip->u4_num_Bytes
                        - ps_dec_op->u4_num_bytes_consumed;

        /* If dynamic bitstream buffer is not allocated and
         * header decode is done, then allocate dynamic bitstream buffer
         */
        if((NULL == ps_dec->pu1_bits_buf_dynamic) &&
           (ps_dec->i4_header_decoded & 1))
        {
            WORD32 size;

            void *pv_buf;
            void *pv_mem_ctxt = ps_dec->pv_mem_ctxt;
            size = MAX(256000, ps_dec->u2_pic_wd * ps_dec->u2_pic_ht * 3 / 2);
            pv_buf = ps_dec->pf_aligned_alloc(pv_mem_ctxt, 128,
                                              size + EXTRA_BS_OFFSET);
            RETURN_IF((NULL == pv_buf), IV_FAIL);
            memset(pv_buf, 0, size + EXTRA_BS_OFFSET);
            ps_dec->pu1_bits_buf_dynamic = pv_buf;
            ps_dec->u4_dynamic_bits_buf_size = size;
        }

        if(ps_dec->pu1_bits_buf_dynamic)
        {
            pu1_bitstrm_buf = ps_dec->pu1_bits_buf_dynamic;
            buf_size = ps_dec->u4_dynamic_bits_buf_size;
        }
        else
        {
            pu1_bitstrm_buf = ps_dec->pu1_bits_buf_static;
            buf_size = ps_dec->u4_static_bits_buf_size;
        }

        u4_next_is_aud = 0;

        buflen = ih264d_find_start_code(pu1_buf, 0, u4_max_ofst,
                                               &u4_length_of_start_code,
                                               &u4_next_is_aud);

        if(buflen == -1)
            buflen = 0;
        /* Ignore bytes beyond the allocated size of intermediate buffer */
        /* Since 8 bytes are read ahead, ensure 8 bytes are free at the
        end of the buffer, which will be memset to 0 after emulation prevention */
        buflen = MIN(buflen, buf_size - 8);

        bytes_consumed = buflen + u4_length_of_start_code;
        ps_dec_op->u4_num_bytes_consumed += bytes_consumed;

        if(buflen)
        {
            memcpy(pu1_bitstrm_buf, pu1_buf + u4_length_of_start_code,
                   buflen);
            /* Decoder may read extra 8 bytes near end of the frame */
            if((buflen + 8) < buf_size)
            {
                memset(pu1_bitstrm_buf + buflen, 0, 8);
            }
            u4_first_start_code_found = 1;

        }
        else
        {
            /*start code not found*/

            if(u4_first_start_code_found == 0)
            {
                /*no start codes found in current process call*/

                ps_dec->i4_error_code = ERROR_START_CODE_NOT_FOUND;
                ps_dec_op->u4_error_code |= 1 << IVD_INSUFFICIENTDATA;

                if(ps_dec->u4_pic_buf_got == 0)
                {

                    ih264d_fill_output_struct_from_context(ps_dec,
                                                           ps_dec_op);

                    ps_dec_op->u4_error_code = ps_dec->i4_error_code;
                    ps_dec_op->u4_frame_decoded_flag = 0;

                    return (IV_FAIL);
                }
                else
                {
                    ps_dec->u1_pic_decode_done = 1;
                    continue;
                }
            }
            else
            {
                /* a start code has already been found earlier in the same process call*/
                frame_data_left = 0;
                header_data_left = 0;
                continue;
            }

        }

        ret = ih264d_parse_nal_unit(dec_hdl, ps_dec_op,
                              pu1_bitstrm_buf, buflen);
        if(ret != OK)
        {
            UWORD32 error =  ih264d_map_error(ret);
            ps_dec_op->u4_error_code = error | ret;
            api_ret_value = IV_FAIL;

            if((ret == IVD_RES_CHANGED)
                            || (ret == IVD_MEM_ALLOC_FAILED)
                            || (ret == ERROR_UNAVAIL_PICBUF_T)
                            || (ret == ERROR_UNAVAIL_MVBUF_T)
                            || (ret == ERROR_INV_SPS_PPS_T)
                            || (ret == ERROR_FEATURE_UNAVAIL)
                            || (ret == IVD_STREAM_WIDTH_HEIGHT_NOT_SUPPORTED)
                            || (ret == IVD_DISP_FRM_ZERO_OP_BUF_SIZE))
            {
                ps_dec->u4_slice_start_code_found = 0;
                break;
            }

            if((ret == ERROR_INCOMPLETE_FRAME) || (ret == ERROR_DANGLING_FIELD_IN_PIC))
            {
                ps_dec_op->u4_num_bytes_consumed -= bytes_consumed;
                api_ret_value = IV_FAIL;
                break;
            }

            if(ret == ERROR_IN_LAST_SLICE_OF_PIC)
            {
                api_ret_value = IV_FAIL;
                break;
            }

        }

        header_data_left = ((ps_dec->i4_decode_header == 1)
                        && (ps_dec->i4_header_decoded != 3)
                        && (ps_dec_op->u4_num_bytes_consumed
                                        < ps_dec_ip->u4_num_Bytes));
        frame_data_left = (((ps_dec->i4_decode_header == 0)
                        && ((ps_dec->u1_pic_decode_done == 0)
                                        || (u4_next_is_aud == 1)))
                        && (ps_dec_op->u4_num_bytes_consumed
                                        < ps_dec_ip->u4_num_Bytes));
    }
    while(( header_data_left == 1)||(frame_data_left == 1));

    if((ps_dec->u4_pic_buf_got == 1)
            && (ret != IVD_MEM_ALLOC_FAILED)
            && ps_dec->u2_total_mbs_coded < ps_dec->u2_frm_ht_in_mbs * ps_dec->u2_frm_wd_in_mbs)
    {
        // last slice - missing/corruption
        WORD32 num_mb_skipped;
        WORD32 prev_slice_err;
        pocstruct_t temp_poc;
        WORD32 ret1;
        WORD32 ht_in_mbs;
        ht_in_mbs = ps_dec->u2_pic_ht >> (4 + ps_dec->ps_cur_slice->u1_field_pic_flag);
        num_mb_skipped = (ht_in_mbs * ps_dec->u2_frm_wd_in_mbs)
                            - ps_dec->u2_total_mbs_coded;

        if(ps_dec->u4_first_slice_in_pic && (ps_dec->u4_pic_buf_got == 0))
            prev_slice_err = 1;
        else
            prev_slice_err = 2;

        if(ps_dec->u4_first_slice_in_pic && (ps_dec->u2_total_mbs_coded == 0))
            prev_slice_err = 1;

        ret1 = ih264d_mark_err_slice_skip(ps_dec, num_mb_skipped, ps_dec->u1_nal_unit_type == IDR_SLICE_NAL, ps_dec->ps_cur_slice->u2_frame_num,
                                   &temp_poc, prev_slice_err);

        if((ret1 == ERROR_UNAVAIL_PICBUF_T) || (ret1 == ERROR_UNAVAIL_MVBUF_T) ||
                       (ret1 == ERROR_INV_SPS_PPS_T))
        {
            ret = ret1;
        }
    }

    if((ret == IVD_RES_CHANGED)
                    || (ret == IVD_MEM_ALLOC_FAILED)
                    || (ret == ERROR_UNAVAIL_PICBUF_T)
                    || (ret == ERROR_UNAVAIL_MVBUF_T)
                    || (ret == ERROR_INV_SPS_PPS_T))
    {

        /* signal the decode thread */
        ih264d_signal_decode_thread(ps_dec);
        /* close deblock thread if it is not closed yet */
        if(ps_dec->u4_num_cores == 3)
        {
            ih264d_signal_bs_deblk_thread(ps_dec);
        }
        /* dont consume bitstream for change in resolution case */
        if(ret == IVD_RES_CHANGED)
        {
            ps_dec_op->u4_num_bytes_consumed -= bytes_consumed;
        }
        return IV_FAIL;
    }

	/* Mirror some raw state info to output */
	ps_dec_op->u1_frame_cropping_flag = ps_dec->u1_frame_cropping_flag;
	ps_dec_op->u1_frame_cropping_rect_left_ofst = ps_dec->u1_frame_cropping_rect_left_ofst;
	ps_dec_op->u1_frame_cropping_rect_right_ofst = ps_dec->u1_frame_cropping_rect_right_ofst;
	ps_dec_op->u1_frame_cropping_rect_top_ofst = ps_dec->u1_frame_cropping_rect_top_ofst;
	ps_dec_op->u1_frame_cropping_rect_bottom_ofst = ps_dec->u1_frame_cropping_rect_bottom_ofst;

	ps_dec_op->u4_raw_wd = ps_dec->u2_pic_wd;
	ps_dec_op->u4_raw_ht = ps_dec->u2_pic_ht;


    if(ps_dec->u1_separate_parse)
    {
        /* If Format conversion is not complete,
         complete it here */
        if(ps_dec->u4_num_cores == 2)
        {

            /*do deblocking of all mbs*/
            if((ps_dec->u4_nmb_deblk == 0) &&(ps_dec->u4_start_recon_deblk == 1) && (ps_dec->ps_cur_sps->u1_mb_aff_flag == 0))
            {
                UWORD32 u4_num_mbs,u4_max_addr;
                tfr_ctxt_t s_tfr_ctxt;
                tfr_ctxt_t *ps_tfr_cxt = &s_tfr_ctxt;
                pad_mgr_t *ps_pad_mgr = &ps_dec->s_pad_mgr;

                /*BS is done for all mbs while parsing*/
                u4_max_addr = (ps_dec->u2_frm_wd_in_mbs * ps_dec->u2_frm_ht_in_mbs) - 1;
                ps_dec->u4_cur_bs_mb_num = u4_max_addr + 1;


                ih264d_init_deblk_tfr_ctxt(ps_dec, ps_pad_mgr, ps_tfr_cxt,
                                           ps_dec->u2_frm_wd_in_mbs, 0);


                u4_num_mbs = u4_max_addr
                                - ps_dec->u4_cur_deblk_mb_num + 1;

                DEBUG_PERF_PRINTF("mbs left for deblocking= %d \n",u4_num_mbs);

                if(u4_num_mbs != 0)
                    ih264d_check_mb_map_deblk(ps_dec, u4_num_mbs,
                                                   ps_tfr_cxt,1);

                ps_dec->u4_start_recon_deblk  = 0;

            }

        }

        /*signal the decode thread*/
        ih264d_signal_decode_thread(ps_dec);
        /* close deblock thread if it is not closed yet*/
        if(ps_dec->u4_num_cores == 3)
        {
            ih264d_signal_bs_deblk_thread(ps_dec);
        }
    }


    DATA_SYNC();


    if((ps_dec_op->u4_error_code & 0xff)
                    != ERROR_DYNAMIC_RESOLUTION_NOT_SUPPORTED)
    {
        ps_dec_op->u4_pic_wd = (UWORD32)ps_dec->u2_disp_width;
        ps_dec_op->u4_pic_ht = (UWORD32)ps_dec->u2_disp_height;
        ps_dec_op->i4_reorder_depth = ps_dec->i4_reorder_depth;
    }

//Report if header (sps and pps) has not been decoded yet
    if(ps_dec->i4_decode_header == 1 && ps_dec->i4_header_decoded != 3)
    {
        ps_dec_op->u4_error_code |= (1 << IVD_INSUFFICIENTDATA);
        api_ret_value = IV_FAIL;
    }

    if((ps_dec->u4_pic_buf_got == 1)
                    && (ERROR_DANGLING_FIELD_IN_PIC != i4_err_status))
    {
        /*
         * For field pictures, set the bottom and top picture decoded u4_flag correctly.
         */

        if(ps_dec->ps_cur_slice->u1_field_pic_flag)
        {
            if(1 == ps_dec->ps_cur_slice->u1_bottom_field_flag)
            {
                ps_dec->u1_top_bottom_decoded |= BOT_FIELD_ONLY;
            }
            else
            {
                ps_dec->u1_top_bottom_decoded |= TOP_FIELD_ONLY;
            }
        }
        else
        {
                ps_dec->u1_top_bottom_decoded = TOP_FIELD_ONLY | BOT_FIELD_ONLY;
        }

        /* if new frame in not found (if we are still getting slices from previous frame)
         * ih264d_deblock_display is not called. Such frames will not be added to reference /display
         */
        if ((ps_dec->ps_dec_err_status->u1_err_flag & REJECT_CUR_PIC) == 0)
        {
            /* Calling Function to deblock Picture and Display */
            ret = ih264d_deblock_display(ps_dec);
        }


        /*set to complete ,as we dont support partial frame decode*/
        if(ps_dec->i4_header_decoded == 3)
        {
            ps_dec->u2_total_mbs_coded = ps_dec->ps_cur_sps->u2_max_mb_addr + 1;
        }

        /*Update the i4_frametype at the end of picture*/
        if(ps_dec->ps_cur_slice->u1_nal_unit_type == IDR_SLICE_NAL)
        {
            ps_dec->i4_frametype = IV_IDR_FRAME;
        }
        else if(ps_dec->i4_pic_type == B_SLICE)
        {
            ps_dec->i4_frametype = IV_B_FRAME;
        }
        else if(ps_dec->i4_pic_type == P_SLICE)
        {
            ps_dec->i4_frametype = IV_P_FRAME;
        }
        else if(ps_dec->i4_pic_type == I_SLICE)
        {
            ps_dec->i4_frametype = IV_I_FRAME;
        }
        else
        {
            H264_DEC_DEBUG_PRINT("Shouldn't come here\n");
        }

        //Update the content type
        ps_dec->i4_content_type = ps_dec->ps_cur_slice->u1_field_pic_flag;

        ps_dec->u4_total_frames_decoded = ps_dec->u4_total_frames_decoded + 2;
        ps_dec->u4_total_frames_decoded = ps_dec->u4_total_frames_decoded
                        - ps_dec->ps_cur_slice->u1_field_pic_flag;

    }

    /* close deblock thread if it is not closed yet*/
    if(ps_dec->u4_num_cores == 3)
    {
        ih264d_signal_bs_deblk_thread(ps_dec);
    }


    {
        /* In case the decoder is configured to run in low delay mode,
         * then get display buffer and then format convert.
         * Note in this mode, format conversion does not run paralelly in a thread and adds to the codec cycles
         */

        if((IVD_DECODE_FRAME_OUT == ps_dec->e_frm_out_mode)
                        && ps_dec->u1_init_dec_flag)
        {

            ih264d_get_next_display_field(ps_dec, ps_dec->ps_out_buffer,
                                          &(ps_dec->s_disp_op));
            if(0 == ps_dec->s_disp_op.u4_error_code)
            {
                ps_dec->u4_fmt_conv_cur_row = 0;
                ps_dec->u4_output_present = 1;
            }
        }

        ih264d_fill_output_struct_from_context(ps_dec, ps_dec_op);

        /* If Format conversion is not complete,
         complete it here */
        if(ps_dec->u4_output_present &&
          (ps_dec->u4_fmt_conv_cur_row < ps_dec->s_disp_frame_info.u4_y_ht))
        {
            ps_dec->u4_fmt_conv_num_rows = ps_dec->s_disp_frame_info.u4_y_ht
                            - ps_dec->u4_fmt_conv_cur_row;
            ih264d_format_convert(ps_dec, &(ps_dec->s_disp_op),
                                  ps_dec->u4_fmt_conv_cur_row,
                                  ps_dec->u4_fmt_conv_num_rows);
            ps_dec->u4_fmt_conv_cur_row += ps_dec->u4_fmt_conv_num_rows;
        }

        ih264d_release_display_field(ps_dec, &(ps_dec->s_disp_op));
    }

    if(ps_dec->i4_decode_header == 1 && (ps_dec->i4_header_decoded & 1) == 1)
    {
        ps_dec_op->u4_progressive_frame_flag = 1;
        if((NULL != ps_dec->ps_cur_sps) && (1 == (ps_dec->ps_cur_sps->u1_is_valid)))
        {
            if((0 == ps_dec->ps_sps->u1_frame_mbs_only_flag)
                            && (0 == ps_dec->ps_sps->u1_mb_aff_flag))
                ps_dec_op->u4_progressive_frame_flag = 0;

        }
    }

    if((TOP_FIELD_ONLY | BOT_FIELD_ONLY) == ps_dec->u1_top_bottom_decoded)
    {
        ps_dec->u1_top_bottom_decoded = 0;
    }
    /*--------------------------------------------------------------------*/
    /* Do End of Pic processing.                                          */
    /* Should be called only if frame was decoded in previous process call*/
    /*--------------------------------------------------------------------*/
    if(ps_dec->u4_pic_buf_got == 1)
    {
        if(1 == ps_dec->u1_last_pic_not_decoded)
        {
            ret = ih264d_end_of_pic_dispbuf_mgr(ps_dec);

            if(ret != OK)
                return ret;

            ret = ih264d_end_of_pic(ps_dec);
            if(ret != OK)
                return ret;
        }
        else
        {
            ret = ih264d_end_of_pic(ps_dec);
            if(ret != OK)
                return ret;
        }

    }


    /*Data memory barrier instruction,so that yuv write by the library is complete*/
    DATA_SYNC();

    H264_DEC_DEBUG_PRINT("The num bytes consumed: %d\n",
                         ps_dec_op->u4_num_bytes_consumed);
    return api_ret_value;
}

WORD32 ih264d_get_version(iv_obj_t *dec_hdl, void *pv_api_ip, void *pv_api_op)
{
    char version_string[MAXVERSION_STRLEN + 1];
    UWORD32 version_string_len;

    ivd_ctl_getversioninfo_ip_t *ps_ip;
    ivd_ctl_getversioninfo_op_t *ps_op;

    ps_ip = (ivd_ctl_getversioninfo_ip_t *)pv_api_ip;
    ps_op = (ivd_ctl_getversioninfo_op_t *)pv_api_op;
    UNUSED(dec_hdl);
    ps_op->u4_error_code = IV_SUCCESS;

    VERSION(version_string, CODEC_NAME, CODEC_RELEASE_TYPE, CODEC_RELEASE_VER,
            CODEC_VENDOR);

    if((WORD32)ps_ip->u4_version_buffer_size <= 0)
    {
        ps_op->u4_error_code = IH264D_VERS_BUF_INSUFFICIENT;
        return (IV_FAIL);
    }

    version_string_len = strnlen(version_string, MAXVERSION_STRLEN) + 1;

    if(ps_ip->u4_version_buffer_size >= version_string_len) //(WORD32)sizeof(sizeof(version_string)))
    {
        memcpy(ps_ip->pv_version_buffer, version_string, version_string_len);
        ps_op->u4_error_code = IV_SUCCESS;
    }
    else
    {
        ps_op->u4_error_code = IH264D_VERS_BUF_INSUFFICIENT;
        return IV_FAIL;
    }
    return (IV_SUCCESS);
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name :   ih264d_get_display_frame                               */
/*                                                                           */
/*  Description   :                                                          */
/*  Inputs        :iv_obj_t decoder handle                                   */
/*                :pv_api_ip pointer to input structure                      */
/*                :pv_api_op pointer to output structure                     */
/*  Outputs       :                                                          */
/*  Returns       : void                                                     */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         22 10 2008    100356         Draft                                */
/*                                                                           */
/*****************************************************************************/
WORD32 ih264d_get_display_frame(iv_obj_t *dec_hdl,
                                void *pv_api_ip,
                                void *pv_api_op)
{

    UNUSED(dec_hdl);
    UNUSED(pv_api_ip);
    UNUSED(pv_api_op);
    // This function is no longer needed, output is returned in the process()
    return IV_FAIL;
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name :  ih264d_set_display_frame                                */
/*                                                                           */
/*  Description   :                                                          */
/*                                                                           */
/*  Inputs        :iv_obj_t decoder handle                                   */
/*                :pv_api_ip pointer to input structure                      */
/*                :pv_api_op pointer to output structure                     */
/*  Outputs       :                                                          */
/*  Returns       : void                                                     */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         22 10 2008    100356         Draft                                */
/*                                                                           */
/*****************************************************************************/
WORD32 ih264d_set_display_frame(iv_obj_t *dec_hdl,
                                void *pv_api_ip,
                                void *pv_api_op)
{

    UWORD32 u4_disp_buf_size[3], u4_num_disp_bufs;
    ivd_set_display_frame_ip_t *dec_disp_ip;
    ivd_set_display_frame_op_t *dec_disp_op;

    UWORD32 i;
    dec_struct_t * ps_dec = (dec_struct_t *)(dec_hdl->pv_codec_handle);

    dec_disp_ip = (ivd_set_display_frame_ip_t *)pv_api_ip;
    dec_disp_op = (ivd_set_display_frame_op_t *)pv_api_op;
    dec_disp_op->u4_error_code = 0;


    ps_dec->u4_num_disp_bufs = 0;
    if(ps_dec->u4_share_disp_buf)
    {
        UWORD32 u4_num_bufs = dec_disp_ip->num_disp_bufs;

        u4_num_bufs = MIN(u4_num_bufs, MAX_DISP_BUFS_NEW);

        ps_dec->u4_num_disp_bufs = u4_num_bufs;

        /* Get the number and sizes of the first buffer. Compare this with the
         * rest to make sure all the buffers are of the same size.
         */
        u4_num_disp_bufs = dec_disp_ip->s_disp_buffer[0].u4_num_bufs;

        u4_disp_buf_size[0] =
          dec_disp_ip->s_disp_buffer[0].u4_min_out_buf_size[0];
        u4_disp_buf_size[1] =
          dec_disp_ip->s_disp_buffer[0].u4_min_out_buf_size[1];
        u4_disp_buf_size[2] =
          dec_disp_ip->s_disp_buffer[0].u4_min_out_buf_size[2];

        for(i = 0; i < u4_num_bufs; i++)
        {
            if(dec_disp_ip->s_disp_buffer[i].u4_num_bufs != u4_num_disp_bufs)
            {
                return IV_FAIL;
            }

            if((dec_disp_ip->s_disp_buffer[i].u4_min_out_buf_size[0]
                != u4_disp_buf_size[0])
                || (dec_disp_ip->s_disp_buffer[i].u4_min_out_buf_size[1]
                    != u4_disp_buf_size[1])
                || (dec_disp_ip->s_disp_buffer[i].u4_min_out_buf_size[2]
                    != u4_disp_buf_size[2]))
            {
                return IV_FAIL;
            }

            ps_dec->disp_bufs[i].u4_num_bufs =
                            dec_disp_ip->s_disp_buffer[i].u4_num_bufs;

            ps_dec->disp_bufs[i].buf[0] =
                            dec_disp_ip->s_disp_buffer[i].pu1_bufs[0];
            ps_dec->disp_bufs[i].buf[1] =
                            dec_disp_ip->s_disp_buffer[i].pu1_bufs[1];
            ps_dec->disp_bufs[i].buf[2] =
                            dec_disp_ip->s_disp_buffer[i].pu1_bufs[2];

            ps_dec->disp_bufs[i].u4_bufsize[0] =
                            dec_disp_ip->s_disp_buffer[i].u4_min_out_buf_size[0];
            ps_dec->disp_bufs[i].u4_bufsize[1] =
                            dec_disp_ip->s_disp_buffer[i].u4_min_out_buf_size[1];
            ps_dec->disp_bufs[i].u4_bufsize[2] =
                            dec_disp_ip->s_disp_buffer[i].u4_min_out_buf_size[2];

        }
    }
    return IV_SUCCESS;

}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_set_flush_mode                                    */
/*                                                                           */
/*  Description   :                                                          */
/*                                                                           */
/*  Inputs        :iv_obj_t decoder handle                                   */
/*                :pv_api_ip pointer to input structure                      */
/*                :pv_api_op pointer to output structure                     */
/*  Globals       : <Does it use any global variables?>                      */
/*  Outputs       :                                                          */
/*  Returns       : void                                                     */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         22 10 2008    100356         Draft                                */
/*                                                                           */
/*****************************************************************************/
WORD32 ih264d_set_flush_mode(iv_obj_t *dec_hdl, void *pv_api_ip, void *pv_api_op)
{
    dec_struct_t * ps_dec;
    ivd_ctl_flush_op_t *ps_ctl_op = (ivd_ctl_flush_op_t*)pv_api_op;
    ps_ctl_op->u4_error_code = 0;

    ps_dec = (dec_struct_t *)(dec_hdl->pv_codec_handle);
    UNUSED(pv_api_ip);
    /* ! */
    /* Signal flush frame control call */
    ps_dec->u1_flushfrm = 1;

    if(ps_dec->u1_init_dec_flag == 1)
    {
        ih264d_release_pics_in_dpb((void *)ps_dec, ps_dec->u1_pic_bufs);
        ih264d_release_display_bufs(ps_dec);
    }

    ps_ctl_op->u4_error_code = 0;

    /* Ignore dangling fields during flush */
    ps_dec->u1_top_bottom_decoded = 0;

    return IV_SUCCESS;
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_get_status                                        */
/*                                                                           */
/*  Description   :                                                          */
/*                                                                           */
/*  Inputs        :iv_obj_t decoder handle                                   */
/*                :pv_api_ip pointer to input structure                      */
/*                :pv_api_op pointer to output structure                     */
/*  Globals       : <Does it use any global variables?>                      */
/*  Outputs       :                                                          */
/*  Returns       : void                                                     */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         22 10 2008    100356         Draft                                */
/*                                                                           */
/*****************************************************************************/

WORD32 ih264d_get_status(iv_obj_t *dec_hdl, void *pv_api_ip, void *pv_api_op)
{

    UWORD32 i;
    dec_struct_t * ps_dec;
    UWORD32 pic_wd, pic_ht;
    ivd_ctl_getstatus_op_t *ps_ctl_op = (ivd_ctl_getstatus_op_t*)pv_api_op;
    UNUSED(pv_api_ip);
    ps_ctl_op->u4_error_code = 0;

    ps_dec = (dec_struct_t *)(dec_hdl->pv_codec_handle);


    if((NULL != ps_dec->ps_cur_sps) && (1 == (ps_dec->ps_cur_sps->u1_is_valid)))
    {
        ps_ctl_op->u4_pic_ht = ps_dec->u2_disp_height;
        ps_ctl_op->u4_pic_wd = ps_dec->u2_disp_width;

        if(0 == ps_dec->u4_share_disp_buf)
        {
            pic_wd = ps_dec->u2_disp_width;
            pic_ht = ps_dec->u2_disp_height;

        }
        else
        {
            pic_wd = ps_dec->u2_frm_wd_y;
            pic_ht = ps_dec->u2_frm_ht_y;
        }
    }
    else
    {
        pic_wd = 0;
        pic_ht = 0;

        ps_ctl_op->u4_pic_ht = pic_wd;
        ps_ctl_op->u4_pic_wd = pic_ht;

        if(1 == ps_dec->u4_share_disp_buf)
        {
            pic_wd += (PAD_LEN_Y_H << 1);
            pic_ht += (PAD_LEN_Y_V << 2);

        }

    }

    if(ps_dec->u4_app_disp_width > pic_wd)
        pic_wd = ps_dec->u4_app_disp_width;
    if(0 == ps_dec->u4_share_disp_buf)
        ps_ctl_op->u4_num_disp_bufs = 1;
    else
    {
        if((NULL != ps_dec->ps_cur_sps) && (1 == (ps_dec->ps_cur_sps->u1_is_valid)))
        {
            if((ps_dec->ps_cur_sps->u1_vui_parameters_present_flag == 1) &&
               (1 == ps_dec->ps_cur_sps->s_vui.u1_bitstream_restriction_flag))
            {
                ps_ctl_op->u4_num_disp_bufs =
                                ps_dec->ps_cur_sps->s_vui.u4_num_reorder_frames + 1;
            }
            else
            {
                /*if VUI is not present assume maximum possible refrence frames for the level,
                 * as max reorder frames*/
                ps_ctl_op->u4_num_disp_bufs = ih264d_get_dpb_size(ps_dec->ps_cur_sps);
            }

            ps_ctl_op->u4_num_disp_bufs +=
                            ps_dec->ps_cur_sps->u1_num_ref_frames + 1;
        }
        else
        {
            ps_ctl_op->u4_num_disp_bufs = 32;
        }
        ps_ctl_op->u4_num_disp_bufs = MAX(
                        ps_ctl_op->u4_num_disp_bufs, 6);
        ps_ctl_op->u4_num_disp_bufs = MIN(
                        ps_ctl_op->u4_num_disp_bufs, 32);
    }

    ps_ctl_op->u4_error_code = ps_dec->i4_error_code;

    ps_ctl_op->u4_frame_rate = 0; //make it proper
    ps_ctl_op->u4_bit_rate = 0; //make it proper
    ps_ctl_op->e_content_type = ps_dec->i4_content_type;
    ps_ctl_op->e_output_chroma_format = ps_dec->u1_chroma_format;
    ps_ctl_op->u4_min_num_in_bufs = MIN_IN_BUFS;

    if(ps_dec->u1_chroma_format == IV_YUV_420P)
    {
        ps_ctl_op->u4_min_num_out_bufs = MIN_OUT_BUFS_420;
    }
    else if(ps_dec->u1_chroma_format == IV_YUV_422ILE)
    {
        ps_ctl_op->u4_min_num_out_bufs = MIN_OUT_BUFS_422ILE;
    }
    else if(ps_dec->u1_chroma_format == IV_RGB_565)
    {
        ps_ctl_op->u4_min_num_out_bufs = MIN_OUT_BUFS_RGB565;
    }
    else if((ps_dec->u1_chroma_format == IV_YUV_420SP_UV)
                    || (ps_dec->u1_chroma_format == IV_YUV_420SP_VU))
    {
        ps_ctl_op->u4_min_num_out_bufs = MIN_OUT_BUFS_420SP;
    }

    else
    {
        //Invalid chroma format; Error code may be updated, verify in testing if needed
        ps_ctl_op->u4_error_code = ERROR_FEATURE_UNAVAIL;
        return IV_FAIL;
    }

    for(i = 0; i < ps_ctl_op->u4_min_num_in_bufs; i++)
    {
        ps_ctl_op->u4_min_in_buf_size[i] = MAX(256000, pic_wd * pic_ht * 3 / 2);
    }

    /*!*/
    if(ps_dec->u1_chroma_format == IV_YUV_420P)
    {
        ps_ctl_op->u4_min_out_buf_size[0] = (pic_wd * pic_ht);
        ps_ctl_op->u4_min_out_buf_size[1] = (pic_wd * pic_ht)
                        >> 2;
        ps_ctl_op->u4_min_out_buf_size[2] = (pic_wd * pic_ht)
                        >> 2;
    }
    else if(ps_dec->u1_chroma_format == IV_YUV_422ILE)
    {
        ps_ctl_op->u4_min_out_buf_size[0] = (pic_wd * pic_ht)
                        * 2;
        ps_ctl_op->u4_min_out_buf_size[1] =
                        ps_ctl_op->u4_min_out_buf_size[2] = 0;
    }
    else if(ps_dec->u1_chroma_format == IV_RGB_565)
    {
        ps_ctl_op->u4_min_out_buf_size[0] = (pic_wd * pic_ht)
                        * 2;
        ps_ctl_op->u4_min_out_buf_size[1] =
                        ps_ctl_op->u4_min_out_buf_size[2] = 0;
    }
    else if((ps_dec->u1_chroma_format == IV_YUV_420SP_UV)
                    || (ps_dec->u1_chroma_format == IV_YUV_420SP_VU))
    {
        ps_ctl_op->u4_min_out_buf_size[0] = (pic_wd * pic_ht);
        ps_ctl_op->u4_min_out_buf_size[1] = (pic_wd * pic_ht)
                        >> 1;
        ps_ctl_op->u4_min_out_buf_size[2] = 0;
    }

    ps_dec->u4_num_disp_bufs_requested = ps_ctl_op->u4_num_disp_bufs;
    return IV_SUCCESS;
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name :    ih264d_get_buf_info                                   */
/*                                                                           */
/*  Description   :                                                          */
/*                                                                           */
/*  Inputs        :iv_obj_t decoder handle                                   */
/*                :pv_api_ip pointer to input structure                      */
/*                :pv_api_op pointer to output structure                     */
/*  Globals       : <Does it use any global variables?>                      */
/*  Outputs       :                                                          */
/*  Returns       : void                                                     */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         22 10 2008    100356         Draft                                */
/*                                                                           */
/*****************************************************************************/
WORD32 ih264d_get_buf_info(iv_obj_t *dec_hdl, void *pv_api_ip, void *pv_api_op)
{

    dec_struct_t * ps_dec;
    UWORD8 i = 0; // Default for 420P format
    UWORD16 pic_wd, pic_ht;
    ivd_ctl_getbufinfo_op_t *ps_ctl_op =
                    (ivd_ctl_getbufinfo_op_t*)pv_api_op;
    UWORD32 au4_min_out_buf_size[IVD_VIDDEC_MAX_IO_BUFFERS];
    UNUSED(pv_api_ip);

    ps_ctl_op->u4_error_code = 0;

    ps_dec = (dec_struct_t *)(dec_hdl->pv_codec_handle);

    ps_ctl_op->u4_min_num_in_bufs = MIN_IN_BUFS;


    ps_ctl_op->u4_num_disp_bufs = 1;


    pic_wd = 0;
    pic_ht = 0;

    if(ps_dec->i4_header_decoded == 3)
    {

        if(0 == ps_dec->u4_share_disp_buf)
        {
            pic_wd = ps_dec->u2_disp_width;
            pic_ht = ps_dec->u2_disp_height;

        }
        else
        {
            pic_wd = ps_dec->u2_frm_wd_y;
            pic_ht = ps_dec->u2_frm_ht_y;
        }

    }

    for(i = 0; i < ps_ctl_op->u4_min_num_in_bufs; i++)
    {
        ps_ctl_op->u4_min_in_buf_size[i] = MAX(256000, pic_wd * pic_ht * 3 / 2);
    }
    if((WORD32)ps_dec->u4_app_disp_width > pic_wd)
        pic_wd = ps_dec->u4_app_disp_width;

    if(0 == ps_dec->u4_share_disp_buf)
        ps_ctl_op->u4_num_disp_bufs = 1;
    else
    {
        if((NULL != ps_dec->ps_cur_sps) && (1 == (ps_dec->ps_cur_sps->u1_is_valid)))
        {
            if((ps_dec->ps_cur_sps->u1_vui_parameters_present_flag == 1) &&
               (1 == ps_dec->ps_cur_sps->s_vui.u1_bitstream_restriction_flag))
            {
                ps_ctl_op->u4_num_disp_bufs =
                                ps_dec->ps_cur_sps->s_vui.u4_num_reorder_frames + 1;
            }
            else
            {
                /*if VUI is not present assume maximum possible refrence frames for the level,
                 * as max reorder frames*/
                ps_ctl_op->u4_num_disp_bufs = ih264d_get_dpb_size(ps_dec->ps_cur_sps);
            }

            ps_ctl_op->u4_num_disp_bufs +=
                            ps_dec->ps_cur_sps->u1_num_ref_frames + 1;

        }
        else
        {
            ps_ctl_op->u4_num_disp_bufs = 32;

        }

        ps_ctl_op->u4_num_disp_bufs = MAX(
                        ps_ctl_op->u4_num_disp_bufs, 6);
        ps_ctl_op->u4_num_disp_bufs = MIN(
                        ps_ctl_op->u4_num_disp_bufs, 32);
    }

    ps_ctl_op->u4_min_num_out_bufs = ih264d_get_outbuf_size(
                    pic_wd, pic_ht, ps_dec->u1_chroma_format,
                    &au4_min_out_buf_size[0]);

    for(i = 0; i < ps_ctl_op->u4_min_num_out_bufs; i++)
    {
        ps_ctl_op->u4_min_out_buf_size[i] = au4_min_out_buf_size[i];
    }

    ps_dec->u4_num_disp_bufs_requested = ps_ctl_op->u4_num_disp_bufs;

    return IV_SUCCESS;
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_set_params                                        */
/*                                                                           */
/*  Description   :                                                          */
/*                                                                           */
/*  Inputs        :iv_obj_t decoder handle                                   */
/*                :pv_api_ip pointer to input structure                      */
/*                :pv_api_op pointer to output structure                     */
/*  Outputs       :                                                          */
/*  Returns       : void                                                     */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         22 10 2008    100356         Draft                                */
/*                                                                           */
/*****************************************************************************/
WORD32 ih264d_set_params(iv_obj_t *dec_hdl, void *pv_api_ip, void *pv_api_op)
{

    dec_struct_t * ps_dec;
    WORD32 ret = IV_SUCCESS;

    ih264d_ctl_set_config_ip_t *ps_h264d_ctl_ip =
                    (ih264d_ctl_set_config_ip_t *)pv_api_ip;
    ih264d_ctl_set_config_op_t *ps_h264d_ctl_op =
                    (ih264d_ctl_set_config_op_t *)pv_api_op;;
    ivd_ctl_set_config_ip_t *ps_ctl_ip =
                    &ps_h264d_ctl_ip->s_ivd_ctl_set_config_ip_t;
    ivd_ctl_set_config_op_t *ps_ctl_op =
                    &ps_h264d_ctl_op->s_ivd_ctl_set_config_op_t;

    ps_dec = (dec_struct_t *)(dec_hdl->pv_codec_handle);

    ps_dec->u4_skip_frm_mask = 0;

    ps_ctl_op->u4_error_code = 0;

    if(ps_ctl_ip->e_frm_skip_mode != IVD_SKIP_NONE)
    {
        ps_ctl_op->u4_error_code = (1 << IVD_UNSUPPORTEDPARAM);
        ret = IV_FAIL;
    }

    if(ps_ctl_ip->u4_disp_wd >= ps_dec->u2_disp_width)
    {
        ps_dec->u4_app_disp_width = ps_ctl_ip->u4_disp_wd;
    }
    else if(0 == ps_dec->i4_header_decoded)
    {
        ps_dec->u4_app_disp_width = ps_ctl_ip->u4_disp_wd;
    }
    else if(ps_ctl_ip->u4_disp_wd == 0)
    {
        ps_dec->u4_app_disp_width = 0;
    }
    else
    {
        /*
         * Set the display width to zero. This will ensure that the wrong value we had stored (0xFFFFFFFF)
         * does not propogate.
         */
        ps_dec->u4_app_disp_width = 0;
        ps_ctl_op->u4_error_code |= (1 << IVD_UNSUPPORTEDPARAM);
        ps_ctl_op->u4_error_code |= ERROR_DISP_WIDTH_INVALID;
        ret = IV_FAIL;
    }

    if(ps_ctl_ip->e_vid_dec_mode == IVD_DECODE_FRAME)
        ps_dec->i4_decode_header = 0;
    else if(ps_ctl_ip->e_vid_dec_mode == IVD_DECODE_HEADER)
        ps_dec->i4_decode_header = 1;
    else
    {
        ps_ctl_op->u4_error_code = (1 << IVD_UNSUPPORTEDPARAM);
        ps_dec->i4_decode_header = 1;
        ret = IV_FAIL;
    }
    ps_dec->e_frm_out_mode = IVD_DISPLAY_FRAME_OUT;

    if((ps_ctl_ip->e_frm_out_mode != IVD_DECODE_FRAME_OUT) &&
       (ps_ctl_ip->e_frm_out_mode != IVD_DISPLAY_FRAME_OUT))
    {
        ps_ctl_op->u4_error_code = (1 << IVD_UNSUPPORTEDPARAM);
        ret = IV_FAIL;
    }
    ps_dec->e_frm_out_mode = ps_ctl_ip->e_frm_out_mode;
    return ret;

}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_set_default_params                                */
/*                                                                           */
/*  Description   :                                                          */
/*                                                                           */
/*  Inputs        :iv_obj_t decoder handle                                   */
/*                :pv_api_ip pointer to input structure                      */
/*                :pv_api_op pointer to output structure                     */
/*  Outputs       :                                                          */
/*  Returns       : void                                                     */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         08 08 2011   100421          Copied from set_params               */
/*                                                                           */
/*****************************************************************************/
WORD32 ih264d_set_default_params(iv_obj_t *dec_hdl,
                                 void *pv_api_ip,
                                 void *pv_api_op)
{

    dec_struct_t * ps_dec;
    WORD32 ret = IV_SUCCESS;

    ivd_ctl_set_config_op_t *ps_ctl_op =
                    (ivd_ctl_set_config_op_t *)pv_api_op;
    ps_dec = (dec_struct_t *)(dec_hdl->pv_codec_handle);
    UNUSED(pv_api_ip);


    {
        ps_dec->u4_app_disp_width = 0;
        ps_dec->u4_skip_frm_mask = 0;
        ps_dec->i4_decode_header = 1;

        ps_ctl_op->u4_error_code = 0;
    }


    return ret;
}
/*****************************************************************************/
/*                                                                           */
/*  Function Name :  ih264d_reset                                            */
/*                                                                           */
/*  Description   :                                                          */
/*                                                                           */
/*  Inputs        :iv_obj_t decoder handle                                   */
/*                :pv_api_ip pointer to input structure                      */
/*                :pv_api_op pointer to output structure                     */
/*  Globals       : <Does it use any global variables?>                      */
/*  Outputs       :                                                          */
/*  Returns       : void                                                     */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         22 10 2008    100356         Draft                                */
/*                                                                           */
/*****************************************************************************/
WORD32 ih264d_delete(iv_obj_t *dec_hdl, void *pv_api_ip, void *pv_api_op)
{
    dec_struct_t *ps_dec;
    ih264d_delete_ip_t *ps_ip = (ih264d_delete_ip_t *)pv_api_ip;
    ih264d_delete_op_t *ps_op = (ih264d_delete_op_t *)pv_api_op;

    ps_dec = (dec_struct_t *)(dec_hdl->pv_codec_handle);
    UNUSED(ps_ip);
    ps_op->s_ivd_delete_op_t.u4_error_code = 0;
    ih264d_free_dynamic_bufs(ps_dec);
    ih264d_free_static_bufs(dec_hdl);
    return IV_SUCCESS;
}
/*****************************************************************************/
/*                                                                           */
/*  Function Name :  ih264d_reset                                            */
/*                                                                           */
/*  Description   :                                                          */
/*                                                                           */
/*  Inputs        :iv_obj_t decoder handle                                   */
/*                :pv_api_ip pointer to input structure                      */
/*                :pv_api_op pointer to output structure                     */
/*  Globals       : <Does it use any global variables?>                      */
/*  Outputs       :                                                          */
/*  Returns       : void                                                     */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         22 10 2008    100356         Draft                                */
/*                                                                           */
/*****************************************************************************/
WORD32 ih264d_reset(iv_obj_t *dec_hdl, void *pv_api_ip, void *pv_api_op)
{
    dec_struct_t * ps_dec;
    ivd_ctl_reset_op_t *ps_ctl_op = (ivd_ctl_reset_op_t *)pv_api_op;
    UNUSED(pv_api_ip);
    ps_ctl_op->u4_error_code = 0;

    ps_dec = (dec_struct_t *)(dec_hdl->pv_codec_handle);

    if(ps_dec != NULL)
    {
        ih264d_init_decoder(ps_dec);
    }
    else
    {
        H264_DEC_DEBUG_PRINT(
                        "\nReset called without Initializing the decoder\n");
        ps_ctl_op->u4_error_code = ERROR_INIT_NOT_DONE;
    }

    return IV_SUCCESS;
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name :  ih264d_ctl                                              */
/*                                                                           */
/*  Description   :                                                          */
/*                                                                           */
/*  Inputs        :iv_obj_t decoder handle                                   */
/*                :pv_api_ip pointer to input structure                      */
/*                :pv_api_op pointer to output structure                     */
/*  Outputs       :                                                          */
/*  Returns       : void                                                     */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         22 10 2008    100356         Draft                                */
/*                                                                           */
/*****************************************************************************/
WORD32 ih264d_ctl(iv_obj_t *dec_hdl, void *pv_api_ip, void *pv_api_op)
{
    ivd_ctl_set_config_ip_t *ps_ctl_ip;
    ivd_ctl_set_config_op_t *ps_ctl_op;
    WORD32 ret = IV_SUCCESS;
    UWORD32 subcommand;
    dec_struct_t *ps_dec = dec_hdl->pv_codec_handle;

    if(ps_dec->init_done != 1)
    {
        //Return proper Error Code
        return IV_FAIL;
    }
    ps_ctl_ip = (ivd_ctl_set_config_ip_t*)pv_api_ip;
    ps_ctl_op = (ivd_ctl_set_config_op_t*)pv_api_op;
    ps_ctl_op->u4_error_code = 0;
    subcommand = ps_ctl_ip->e_sub_cmd;

    switch(subcommand)
    {
        case IVD_CMD_CTL_GETPARAMS:
            ret = ih264d_get_status(dec_hdl, (void *)pv_api_ip,
                                    (void *)pv_api_op);
            break;
        case IVD_CMD_CTL_SETPARAMS:
            ret = ih264d_set_params(dec_hdl, (void *)pv_api_ip,
                                    (void *)pv_api_op);
            break;
        case IVD_CMD_CTL_RESET:
            ret = ih264d_reset(dec_hdl, (void *)pv_api_ip, (void *)pv_api_op);
            break;
        case IVD_CMD_CTL_SETDEFAULT:
            ret = ih264d_set_default_params(dec_hdl, (void *)pv_api_ip,
                                            (void *)pv_api_op);
            break;
        case IVD_CMD_CTL_FLUSH:
            ret = ih264d_set_flush_mode(dec_hdl, (void *)pv_api_ip,
                                        (void *)pv_api_op);
            break;
        case IVD_CMD_CTL_GETBUFINFO:
            ret = ih264d_get_buf_info(dec_hdl, (void *)pv_api_ip,
                                      (void *)pv_api_op);
            break;
        case IVD_CMD_CTL_GETVERSION:
            ret = ih264d_get_version(dec_hdl, (void *)pv_api_ip,
                                     (void *)pv_api_op);
            break;
        case IH264D_CMD_CTL_DEGRADE:
            ret = ih264d_set_degrade(dec_hdl, (void *)pv_api_ip,
                                     (void *)pv_api_op);
            break;

        case IH264D_CMD_CTL_SET_NUM_CORES:
            ret = ih264d_set_num_cores(dec_hdl, (void *)pv_api_ip,
                                       (void *)pv_api_op);
            break;
        case IH264D_CMD_CTL_GET_BUFFER_DIMENSIONS:
            ret = ih264d_get_frame_dimensions(dec_hdl, (void *)pv_api_ip,
                                              (void *)pv_api_op);
            break;
        case IH264D_CMD_CTL_GET_VUI_PARAMS:
            ret = ih264d_get_vui_params(dec_hdl, (void *)pv_api_ip,
                                        (void *)pv_api_op);
            break;
        case IH264D_CMD_CTL_GET_SEI_MDCV_PARAMS:
            ret = ih264d_get_sei_mdcv_params(dec_hdl, (void *)pv_api_ip,
                                             (void *)pv_api_op);
            break;
        case IH264D_CMD_CTL_GET_SEI_CLL_PARAMS:
            ret = ih264d_get_sei_cll_params(dec_hdl, (void *)pv_api_ip,
                                            (void *)pv_api_op);
            break;
        case IH264D_CMD_CTL_GET_SEI_AVE_PARAMS:
            ret = ih264d_get_sei_ave_params(dec_hdl, (void *)pv_api_ip,
                                            (void *)pv_api_op);
            break;
        case IH264D_CMD_CTL_GET_SEI_CCV_PARAMS:
            ret = ih264d_get_sei_ccv_params(dec_hdl, (void *)pv_api_ip,
                                            (void *)pv_api_op);
            break;
        case IH264D_CMD_CTL_SET_PROCESSOR:
            ret = ih264d_set_processor(dec_hdl, (void *)pv_api_ip,
                                       (void *)pv_api_op);
            break;
        default:
            H264_DEC_DEBUG_PRINT("\ndo nothing\n")
            ;
            break;
    }

    return ret;
}
/*****************************************************************************/
/*                                                                           */
/*  Function Name :   ih264d_rel_display_frame                               */
/*                                                                           */
/*  Description   :                                                          */
/*                                                                           */
/*  Inputs        :iv_obj_t decoder handle                                   */
/*                :pv_api_ip pointer to input structure                      */
/*                :pv_api_op pointer to output structure                     */
/*  Outputs       :                                                          */
/*  Returns       : void                                                     */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         22 10 2008    100356         Draft                                */
/*                                                                           */
/*****************************************************************************/
WORD32 ih264d_rel_display_frame(iv_obj_t *dec_hdl,
                                void *pv_api_ip,
                                void *pv_api_op)
{

    ivd_rel_display_frame_ip_t *ps_rel_ip;
    ivd_rel_display_frame_op_t *ps_rel_op;
    UWORD32 buf_released = 0;

    UWORD32 u4_ts = 0;
    dec_struct_t *ps_dec = dec_hdl->pv_codec_handle;

    ps_rel_ip = (ivd_rel_display_frame_ip_t *)pv_api_ip;
    ps_rel_op = (ivd_rel_display_frame_op_t *)pv_api_op;
    ps_rel_op->u4_error_code = 0;
    u4_ts = ps_rel_ip->u4_disp_buf_id;

    if(0 == ps_dec->u4_share_disp_buf)
    {
        ps_dec->u4_disp_buf_mapping[u4_ts] = 0;
        ps_dec->u4_disp_buf_to_be_freed[u4_ts] = 0;
        return IV_SUCCESS;
    }

    if(ps_dec->pv_pic_buf_mgr != NULL)
    {
        if(1 == ps_dec->u4_disp_buf_mapping[u4_ts])
        {
            ih264_buf_mgr_release((buf_mgr_t *)ps_dec->pv_pic_buf_mgr,
                                  ps_rel_ip->u4_disp_buf_id,
                                  BUF_MGR_IO);
            ps_dec->u4_disp_buf_mapping[u4_ts] = 0;
            buf_released = 1;
        }
    }

    if((1 == ps_dec->u4_share_disp_buf) && (0 == buf_released))
        ps_dec->u4_disp_buf_to_be_freed[u4_ts] = 1;

    return IV_SUCCESS;
}

/**
 *******************************************************************************
 *
 * @brief
 *  Sets degrade params
 *
 * @par Description:
 *  Sets degrade params.
 *  Refer to ih264d_ctl_degrade_ip_t definition for details
 *
 * @param[in] ps_codec_obj
 *  Pointer to codec object at API level
 *
 * @param[in] pv_api_ip
 *  Pointer to input argument structure
 *
 * @param[out] pv_api_op
 *  Pointer to output argument structure
 *
 * @returns  Status
 *
 * @remarks
 *
 *
 *******************************************************************************
 */

WORD32 ih264d_set_degrade(iv_obj_t *ps_codec_obj,
                          void *pv_api_ip,
                          void *pv_api_op)
{
    ih264d_ctl_degrade_ip_t *ps_ip;
    ih264d_ctl_degrade_op_t *ps_op;
    dec_struct_t *ps_codec = (dec_struct_t *)ps_codec_obj->pv_codec_handle;

    ps_ip = (ih264d_ctl_degrade_ip_t *)pv_api_ip;
    ps_op = (ih264d_ctl_degrade_op_t *)pv_api_op;

    ps_codec->i4_degrade_type = ps_ip->i4_degrade_type;
    ps_codec->i4_nondegrade_interval = ps_ip->i4_nondegrade_interval;
    ps_codec->i4_degrade_pics = ps_ip->i4_degrade_pics;

    ps_op->u4_error_code = 0;
    ps_codec->i4_degrade_pic_cnt = 0;

    return IV_SUCCESS;
}

WORD32 ih264d_get_frame_dimensions(iv_obj_t *dec_hdl,
                                   void *pv_api_ip,
                                   void *pv_api_op)
{
    ih264d_ctl_get_frame_dimensions_ip_t *ps_ip;
    ih264d_ctl_get_frame_dimensions_op_t *ps_op;
    dec_struct_t *ps_dec = dec_hdl->pv_codec_handle;
    UWORD32 disp_wd, disp_ht, buffer_wd, buffer_ht, x_offset, y_offset;

    ps_ip = (ih264d_ctl_get_frame_dimensions_ip_t *)pv_api_ip;

    ps_op = (ih264d_ctl_get_frame_dimensions_op_t *)pv_api_op;
    UNUSED(ps_ip);
    if((NULL != ps_dec->ps_cur_sps) && (1 == (ps_dec->ps_cur_sps->u1_is_valid)))
    {
        disp_wd = ps_dec->u2_disp_width;
        disp_ht = ps_dec->u2_disp_height;

        if(0 == ps_dec->u4_share_disp_buf)
        {
            buffer_wd = disp_wd;
            buffer_ht = disp_ht;
        }
        else
        {
            buffer_wd = ps_dec->u2_frm_wd_y;
            buffer_ht = ps_dec->u2_frm_ht_y;
        }
    }
    else
    {
        disp_wd = 0;
        disp_ht = 0;

        if(0 == ps_dec->u4_share_disp_buf)
        {
            buffer_wd = disp_wd;
            buffer_ht = disp_ht;
        }
        else
        {
            buffer_wd = ALIGN16(disp_wd) + (PAD_LEN_Y_H << 1);
            buffer_ht = ALIGN16(disp_ht) + (PAD_LEN_Y_V << 2);
        }
    }
    if(ps_dec->u4_app_disp_width > buffer_wd)
        buffer_wd = ps_dec->u4_app_disp_width;

    if(0 == ps_dec->u4_share_disp_buf)
    {
        x_offset = 0;
        y_offset = 0;
    }
    else
    {
        y_offset = (PAD_LEN_Y_V << 1);
        x_offset = PAD_LEN_Y_H;

        if((NULL != ps_dec->ps_sps) && (1 == (ps_dec->ps_sps->u1_is_valid))
                        && (0 != ps_dec->u2_crop_offset_y))
        {
            y_offset += ps_dec->u2_crop_offset_y / ps_dec->u2_frm_wd_y;
            x_offset += ps_dec->u2_crop_offset_y % ps_dec->u2_frm_wd_y;
        }
    }

    ps_op->u4_disp_wd[0] = disp_wd;
    ps_op->u4_disp_ht[0] = disp_ht;
    ps_op->u4_buffer_wd[0] = buffer_wd;
    ps_op->u4_buffer_ht[0] = buffer_ht;
    ps_op->u4_x_offset[0] = x_offset;
    ps_op->u4_y_offset[0] = y_offset;

    ps_op->u4_disp_wd[1] = ps_op->u4_disp_wd[2] = ((ps_op->u4_disp_wd[0] + 1)
                    >> 1);
    ps_op->u4_disp_ht[1] = ps_op->u4_disp_ht[2] = ((ps_op->u4_disp_ht[0] + 1)
                    >> 1);
    ps_op->u4_buffer_wd[1] = ps_op->u4_buffer_wd[2] = (ps_op->u4_buffer_wd[0]
                    >> 1);
    ps_op->u4_buffer_ht[1] = ps_op->u4_buffer_ht[2] = (ps_op->u4_buffer_ht[0]
                    >> 1);
    ps_op->u4_x_offset[1] = ps_op->u4_x_offset[2] =
                    (ps_op->u4_x_offset[0] >> 1);
    ps_op->u4_y_offset[1] = ps_op->u4_y_offset[2] =
                    (ps_op->u4_y_offset[0] >> 1);

    if((ps_dec->u1_chroma_format == IV_YUV_420SP_UV)
                    || (ps_dec->u1_chroma_format == IV_YUV_420SP_VU))
    {
        ps_op->u4_disp_wd[2] = 0;
        ps_op->u4_disp_ht[2] = 0;
        ps_op->u4_buffer_wd[2] = 0;
        ps_op->u4_buffer_ht[2] = 0;
        ps_op->u4_x_offset[2] = 0;
        ps_op->u4_y_offset[2] = 0;

        ps_op->u4_disp_wd[1] <<= 1;
        ps_op->u4_buffer_wd[1] <<= 1;
        ps_op->u4_x_offset[1] <<= 1;
    }

    return IV_SUCCESS;

}

WORD32 ih264d_get_vui_params(iv_obj_t *dec_hdl,
                             void *pv_api_ip,
                             void *pv_api_op)
{
    ih264d_ctl_get_vui_params_ip_t *ps_ip;
    ih264d_ctl_get_vui_params_op_t *ps_op;
    dec_struct_t *ps_dec = dec_hdl->pv_codec_handle;
    dec_seq_params_t *ps_sps;
    vui_t *ps_vui;
    WORD32 i;
    UWORD32 u4_size;

    ps_ip = (ih264d_ctl_get_vui_params_ip_t *)pv_api_ip;
    ps_op = (ih264d_ctl_get_vui_params_op_t *)pv_api_op;
    UNUSED(ps_ip);

    u4_size = ps_op->u4_size;
    memset(ps_op, 0, sizeof(ih264d_ctl_get_vui_params_op_t));
    ps_op->u4_size = u4_size;

    if(NULL == ps_dec->ps_cur_sps)
    {
        ps_op->u4_error_code = ERROR_VUI_PARAMS_NOT_FOUND;
        return IV_FAIL;
    }

    ps_sps = ps_dec->ps_cur_sps;
    if((0 == ps_sps->u1_is_valid)
                    || (0 == ps_sps->u1_vui_parameters_present_flag))
    {
        ps_op->u4_error_code = ERROR_VUI_PARAMS_NOT_FOUND;
        return IV_FAIL;
    }

    ps_vui = &ps_sps->s_vui;

    ps_op->u1_aspect_ratio_idc              = ps_vui->u1_aspect_ratio_idc;
    ps_op->u2_sar_width                     = ps_vui->u2_sar_width;
    ps_op->u2_sar_height                    = ps_vui->u2_sar_height;
    ps_op->u1_overscan_appropriate_flag     = ps_vui->u1_overscan_appropriate_flag;
    ps_op->u1_video_format                  = ps_vui->u1_video_format;
    ps_op->u1_video_full_range_flag         = ps_vui->u1_video_full_range_flag;
    ps_op->u1_colour_primaries              = ps_vui->u1_colour_primaries;
    ps_op->u1_tfr_chars                     = ps_vui->u1_tfr_chars;
    ps_op->u1_matrix_coeffs                 = ps_vui->u1_matrix_coeffs;
    ps_op->u1_cr_top_field                  = ps_vui->u1_cr_top_field;
    ps_op->u1_cr_bottom_field               = ps_vui->u1_cr_bottom_field;
    ps_op->u4_num_units_in_tick             = ps_vui->u4_num_units_in_tick;
    ps_op->u4_time_scale                    = ps_vui->u4_time_scale;
    ps_op->u1_fixed_frame_rate_flag         = ps_vui->u1_fixed_frame_rate_flag;
    ps_op->u1_nal_hrd_params_present        = ps_vui->u1_nal_hrd_params_present;
    ps_op->u1_vcl_hrd_params_present        = ps_vui->u1_vcl_hrd_params_present;
    ps_op->u1_low_delay_hrd_flag            = ps_vui->u1_low_delay_hrd_flag;
    ps_op->u1_pic_struct_present_flag       = ps_vui->u1_pic_struct_present_flag;
    ps_op->u1_bitstream_restriction_flag    = ps_vui->u1_bitstream_restriction_flag;
    ps_op->u1_mv_over_pic_boundaries_flag   = ps_vui->u1_mv_over_pic_boundaries_flag;
    ps_op->u4_max_bytes_per_pic_denom       = ps_vui->u4_max_bytes_per_pic_denom;
    ps_op->u4_max_bits_per_mb_denom         = ps_vui->u4_max_bits_per_mb_denom;
    ps_op->u4_log2_max_mv_length_horz       = ps_vui->u4_log2_max_mv_length_horz;
    ps_op->u4_log2_max_mv_length_vert       = ps_vui->u4_log2_max_mv_length_vert;
    ps_op->u4_num_reorder_frames            = ps_vui->u4_num_reorder_frames;
    ps_op->u4_max_dec_frame_buffering       = ps_vui->u4_max_dec_frame_buffering;

    return IV_SUCCESS;
}
/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_get_sei_mdcv_params                               */
/*                                                                           */
/*  Description   : This function populates SEI mdcv message in              */
/*                     output structure                                      */
/*  Inputs        : iv_obj_t decoder handle                                  */
/*                : pv_api_ip pointer to input structure                     */
/*                : pv_api_op pointer to output structure                    */
/*  Outputs       :                                                          */
/*  Returns       : returns 0; 1 with error code when MDCV is not present    */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*                                                                           */
/*                                                                           */
/*****************************************************************************/
WORD32 ih264d_get_sei_mdcv_params(iv_obj_t *dec_hdl,
                                  void *pv_api_ip,
                                  void *pv_api_op)
{
    ih264d_ctl_get_sei_mdcv_params_ip_t *ps_ip;
    ih264d_ctl_get_sei_mdcv_params_op_t *ps_op;
    dec_struct_t *ps_dec = dec_hdl->pv_codec_handle;
    sei_mdcv_params_t *ps_sei_mdcv;
    WORD32 i4_count;

    ps_ip = (ih264d_ctl_get_sei_mdcv_params_ip_t *)pv_api_ip;
    ps_op = (ih264d_ctl_get_sei_mdcv_params_op_t *)pv_api_op;
    UNUSED(ps_ip);

    if(0 == ps_dec->s_sei_export.u1_sei_mdcv_params_present_flag)
    {
        ps_op->u4_error_code = ERROR_SEI_MDCV_PARAMS_NOT_FOUND;
        return IV_FAIL;
    }

    ps_sei_mdcv = &ps_dec->s_sei_export.s_sei_mdcv_params;

    for(i4_count = 0; i4_count < NUM_SEI_MDCV_PRIMARIES; i4_count++)
    {
        ps_op->au2_display_primaries_x[i4_count] = ps_sei_mdcv->au2_display_primaries_x[i4_count];
        ps_op->au2_display_primaries_y[i4_count] = ps_sei_mdcv->au2_display_primaries_y[i4_count];
    }

    ps_op->u2_white_point_x = ps_sei_mdcv->u2_white_point_x;
    ps_op->u2_white_point_y = ps_sei_mdcv->u2_white_point_y;
    ps_op->u4_max_display_mastering_luminance = ps_sei_mdcv->u4_max_display_mastering_luminance;
    ps_op->u4_min_display_mastering_luminance = ps_sei_mdcv->u4_min_display_mastering_luminance;

    return IV_SUCCESS;
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_get_sei_cll_params                                */
/*                                                                           */
/*  Description   : This function populates SEI cll message in               */
/*                     output structure                                      */
/*  Inputs        : iv_obj_t decoder handle                                  */
/*                : pv_api_ip pointer to input structure                     */
/*                : pv_api_op pointer to output structure                    */
/*  Outputs       :                                                          */
/*  Returns       : returns 0; 1 with error code when CLL is not present     */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*                                                                           */
/*                                                                           */
/*****************************************************************************/
WORD32 ih264d_get_sei_cll_params(iv_obj_t *dec_hdl,
                                 void *pv_api_ip,
                                 void *pv_api_op)
{
    ih264d_ctl_get_sei_cll_params_ip_t *ps_ip;
    ih264d_ctl_get_sei_cll_params_op_t *ps_op;
    dec_struct_t *ps_dec = dec_hdl->pv_codec_handle;
    sei_cll_params_t *ps_sei_cll;

    ps_ip = (ih264d_ctl_get_sei_cll_params_ip_t *)pv_api_ip;
    ps_op = (ih264d_ctl_get_sei_cll_params_op_t *)pv_api_op;
    UNUSED(ps_ip);

    if(0 == ps_dec->s_sei_export.u1_sei_cll_params_present_flag)
    {
        ps_op->u4_error_code = ERROR_SEI_CLL_PARAMS_NOT_FOUND;
        return IV_FAIL;
    }

    ps_sei_cll = &ps_dec->s_sei_export.s_sei_cll_params;

    ps_op->u2_max_content_light_level = ps_sei_cll->u2_max_content_light_level;
    ps_op->u2_max_pic_average_light_level = ps_sei_cll->u2_max_pic_average_light_level;

    return IV_SUCCESS;
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_get_sei_ave_params                                */
/*                                                                           */
/*  Description   : This function populates SEI ave message in               */
/*                     output structure                                      */
/*  Inputs        : iv_obj_t decoder handle                                  */
/*                : pv_api_ip pointer to input structure                     */
/*                : pv_api_op pointer to output structure                    */
/*  Outputs       :                                                          */
/*  Returns       : returns 0; 1 with error code when AVE is not present     */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*                                                                           */
/*                                                                           */
/*****************************************************************************/
WORD32 ih264d_get_sei_ave_params(iv_obj_t *dec_hdl,
                                 void *pv_api_ip,
                                 void *pv_api_op)
{
    ih264d_ctl_get_sei_ave_params_ip_t *ps_ip;
    ih264d_ctl_get_sei_ave_params_op_t *ps_op;
    dec_struct_t *ps_dec = dec_hdl->pv_codec_handle;
    sei_ave_params_t *ps_sei_ave;

    ps_ip = (ih264d_ctl_get_sei_ave_params_ip_t *)pv_api_ip;
    ps_op = (ih264d_ctl_get_sei_ave_params_op_t *)pv_api_op;
    UNUSED(ps_ip);

    if(0 == ps_dec->s_sei_export.u1_sei_ave_params_present_flag)
    {
        ps_op->u4_error_code = ERROR_SEI_AVE_PARAMS_NOT_FOUND;
        return IV_FAIL;
    }

    ps_sei_ave = &ps_dec->s_sei_export.s_sei_ave_params;

    ps_op->u4_ambient_illuminance = ps_sei_ave->u4_ambient_illuminance;
    ps_op->u2_ambient_light_x = ps_sei_ave->u2_ambient_light_x;
    ps_op->u2_ambient_light_y = ps_sei_ave->u2_ambient_light_y;

    return IV_SUCCESS;
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_get_sei_ccv_params                                */
/*                                                                           */
/*  Description   : This function populates SEI mdcv message in              */
/*                     output structure                                      */
/*  Inputs        : iv_obj_t decoder handle                                  */
/*                : pv_api_ip pointer to input structure                     */
/*                : pv_api_op pointer to output structure                    */
/*  Outputs       :                                                          */
/*  Returns       : returns 0; 1 with error code when CCV is not present    */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*                                                                           */
/*                                                                           */
/*****************************************************************************/
WORD32 ih264d_get_sei_ccv_params(iv_obj_t *dec_hdl,
                                 void *pv_api_ip,
                                 void *pv_api_op)
{
    ih264d_ctl_get_sei_ccv_params_ip_t *ps_ip;
    ih264d_ctl_get_sei_ccv_params_op_t *ps_op;
    dec_struct_t *ps_dec = dec_hdl->pv_codec_handle;
    sei_ccv_params_t *ps_sei_ccv;
    WORD32 i4_count;

    ps_ip = (ih264d_ctl_get_sei_ccv_params_ip_t *)pv_api_ip;
    ps_op = (ih264d_ctl_get_sei_ccv_params_op_t *)pv_api_op;
    UNUSED(ps_ip);

    if(0 == ps_dec->s_sei_export.u1_sei_ccv_params_present_flag)
    {
        ps_op->u4_error_code = ERROR_SEI_CCV_PARAMS_NOT_FOUND;
        return IV_FAIL;
    }

    ps_sei_ccv = &ps_dec->s_sei_export.s_sei_ccv_params;

    ps_op->u1_ccv_cancel_flag = ps_sei_ccv->u1_ccv_cancel_flag;

    if(0 == ps_op->u1_ccv_cancel_flag)
    {
        ps_op->u1_ccv_persistence_flag = ps_sei_ccv->u1_ccv_persistence_flag;
        ps_op->u1_ccv_primaries_present_flag = ps_sei_ccv->u1_ccv_primaries_present_flag;
        ps_op->u1_ccv_min_luminance_value_present_flag =
                    ps_sei_ccv->u1_ccv_min_luminance_value_present_flag;
        ps_op->u1_ccv_max_luminance_value_present_flag =
                    ps_sei_ccv->u1_ccv_max_luminance_value_present_flag;
        ps_op->u1_ccv_avg_luminance_value_present_flag =
                    ps_sei_ccv->u1_ccv_avg_luminance_value_present_flag;
        ps_op->u1_ccv_reserved_zero_2bits = ps_sei_ccv->u1_ccv_reserved_zero_2bits;

        if(1 == ps_sei_ccv->u1_ccv_primaries_present_flag)
        {
            for(i4_count = 0; i4_count < NUM_SEI_CCV_PRIMARIES; i4_count++)
            {
                ps_op->ai4_ccv_primaries_x[i4_count] = ps_sei_ccv->ai4_ccv_primaries_x[i4_count];
                ps_op->ai4_ccv_primaries_y[i4_count] = ps_sei_ccv->ai4_ccv_primaries_y[i4_count];
            }
        }

        if(1 == ps_sei_ccv->u1_ccv_min_luminance_value_present_flag)
        {
            ps_op->u4_ccv_min_luminance_value = ps_sei_ccv->u4_ccv_min_luminance_value;
        }
        if(1 == ps_sei_ccv->u1_ccv_max_luminance_value_present_flag)
        {
            ps_op->u4_ccv_max_luminance_value = ps_sei_ccv->u4_ccv_max_luminance_value;
        }
        if(1 == ps_sei_ccv->u1_ccv_avg_luminance_value_present_flag)
        {
            ps_op->u4_ccv_avg_luminance_value = ps_sei_ccv->u4_ccv_avg_luminance_value;
        }
    }

    return IV_SUCCESS;
}

WORD32 ih264d_set_num_cores(iv_obj_t *dec_hdl, void *pv_api_ip, void *pv_api_op)
{
    ih264d_ctl_set_num_cores_ip_t *ps_ip;
    ih264d_ctl_set_num_cores_op_t *ps_op;
    dec_struct_t *ps_dec = dec_hdl->pv_codec_handle;

    ps_ip = (ih264d_ctl_set_num_cores_ip_t *)pv_api_ip;
    ps_op = (ih264d_ctl_set_num_cores_op_t *)pv_api_op;
    ps_op->u4_error_code = 0;
    ps_dec->u4_num_cores = ps_ip->u4_num_cores;
    if(ps_dec->u4_num_cores == 1)
    {
        ps_dec->u1_separate_parse = 0;
    }
    else
    {
        ps_dec->u1_separate_parse = 1;
    }

    /*using only upto three threads currently*/
    if(ps_dec->u4_num_cores > 3)
        ps_dec->u4_num_cores = 3;

    return IV_SUCCESS;
}

void ih264d_fill_output_struct_from_context(dec_struct_t *ps_dec,
                                            ivd_video_decode_op_t *ps_dec_op)
{
    if((ps_dec_op->u4_error_code & 0xff)
                    != ERROR_DYNAMIC_RESOLUTION_NOT_SUPPORTED)
    {
        ps_dec_op->u4_pic_wd = (UWORD32)ps_dec->u2_disp_width;
        ps_dec_op->u4_pic_ht = (UWORD32)ps_dec->u2_disp_height;
    }
    ps_dec_op->i4_reorder_depth = ps_dec->i4_reorder_depth;
    ps_dec_op->i4_display_index = ps_dec->i4_display_index;
    ps_dec_op->e_pic_type = ps_dec->i4_frametype;

    ps_dec_op->u4_new_seq = 0;
    ps_dec_op->u4_output_present = ps_dec->u4_output_present;
    ps_dec_op->u4_progressive_frame_flag =
                    ps_dec->s_disp_op.u4_progressive_frame_flag;

    ps_dec_op->u4_is_ref_flag = 1;
    if(ps_dec_op->u4_frame_decoded_flag)
    {
        if(ps_dec->ps_cur_slice->u1_nal_ref_idc == 0)
            ps_dec_op->u4_is_ref_flag = 0;
    }

    ps_dec_op->e_output_format = ps_dec->s_disp_op.e_output_format;
    ps_dec_op->s_disp_frm_buf = ps_dec->s_disp_op.s_disp_frm_buf;
    ps_dec_op->e4_fld_type = ps_dec->s_disp_op.e4_fld_type;
    ps_dec_op->u4_ts = ps_dec->s_disp_op.u4_ts;
    ps_dec_op->u4_disp_buf_id = ps_dec->s_disp_op.u4_disp_buf_id;

    ih264d_export_sei_params(&ps_dec_op->s_sei_decode_op, ps_dec);
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_api_function                                      */
/*                                                                           */
/*  Description   :                                                          */
/*                                                                           */
/*  Inputs        :iv_obj_t decoder handle                                   */
/*                :pv_api_ip pointer to input structure                      */
/*                :pv_api_op pointer to output structure                     */
/*  Outputs       :                                                          */
/*  Returns       : void                                                     */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         22 10 2008    100356         Draft                                */
/*                                                                           */
/*****************************************************************************/
IV_API_CALL_STATUS_T ih264d_api_function(iv_obj_t *dec_hdl,
                                              void *pv_api_ip,
                                              void *pv_api_op)
{
    UWORD32 command;
    UWORD32 *pu2_ptr_cmd;
    UWORD32 u4_api_ret;
    IV_API_CALL_STATUS_T e_status;
    e_status = api_check_struct_sanity(dec_hdl, pv_api_ip, pv_api_op);

    if(e_status != IV_SUCCESS)
    {
        UWORD32 *ptr_err;

        ptr_err = (UWORD32 *)pv_api_op;
        UNUSED(ptr_err);
        H264_DEC_DEBUG_PRINT("error code = %d\n", *(ptr_err + 1));
        return IV_FAIL;
    }

    pu2_ptr_cmd = (UWORD32 *)pv_api_ip;
    pu2_ptr_cmd++;

    command = *pu2_ptr_cmd;
//    H264_DEC_DEBUG_PRINT("inside lib = %d\n",command);
    switch(command)
    {

        case IVD_CMD_CREATE:
            u4_api_ret = ih264d_create(dec_hdl, (void *)pv_api_ip,
                                     (void *)pv_api_op);
            break;
        case IVD_CMD_DELETE:
            u4_api_ret = ih264d_delete(dec_hdl, (void *)pv_api_ip,
                                     (void *)pv_api_op);
            break;

        case IVD_CMD_VIDEO_DECODE:
            u4_api_ret = ih264d_video_decode(dec_hdl, (void *)pv_api_ip,
                                             (void *)pv_api_op);
            break;

        case IVD_CMD_GET_DISPLAY_FRAME:
            u4_api_ret = ih264d_get_display_frame(dec_hdl, (void *)pv_api_ip,
                                                  (void *)pv_api_op);

            break;

        case IVD_CMD_SET_DISPLAY_FRAME:
            u4_api_ret = ih264d_set_display_frame(dec_hdl, (void *)pv_api_ip,
                                                  (void *)pv_api_op);

            break;

        case IVD_CMD_REL_DISPLAY_FRAME:
            u4_api_ret = ih264d_rel_display_frame(dec_hdl, (void *)pv_api_ip,
                                                  (void *)pv_api_op);
            break;

        case IVD_CMD_VIDEO_CTL:
            u4_api_ret = ih264d_ctl(dec_hdl, (void *)pv_api_ip,
                                    (void *)pv_api_op);
            break;
        default:
            u4_api_ret = IV_FAIL;
            break;
    }

    return u4_api_ret;
}
