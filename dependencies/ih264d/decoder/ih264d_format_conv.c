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
/*  File Name         : ih264d_format_conv.c                                */
/*                                                                           */
/*  Description       : Contains functions needed to convert the images in   */
/*                      different color spaces to yuv 422i color space       */
/*                                                                           */
/*                                                                           */
/*  Issues / Problems : None                                                 */
/*                                                                           */
/*  Revision History  :                                                      */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         28 08 2007  Naveen Kumar T        Draft                           */
/*                                                                           */
/*****************************************************************************/
/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

/* System include files */
#include <string.h>
/* User include files */
#include "ih264_typedefs.h"
#include "iv.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264d_structs.h"
#include "ih264d_format_conv.h"
#include "ih264d_defs.h"



#ifdef LOGO_EN
#include "ih264d_ittiam_logo.h"
#define INSERT_LOGO(pu1_buf_y,pu1_buf_u,pu1_buf_v, u4_stride, u4_x_pos, u4_y_pos, u4_yuv_fmt, u4_disp_wd, u4_disp_ht) \
                    ih264d_insert_logo(pu1_buf_y,pu1_buf_u,pu1_buf_v, u4_stride,\
                          u4_x_pos, u4_y_pos, u4_yuv_fmt, u4_disp_wd, u4_disp_ht)
#else
#define INSERT_LOGO(pu1_buf_y,pu1_buf_u,pu1_buf_v, u4_stride, u4_x_pos, u4_y_pos, u4_yuv_fmt, u4_disp_wd, u4_disp_ht)
#endif

/**
 *******************************************************************************
 *
 * @brief Function used from copying a 420SP buffer
 *
 * @par   Description
 * Function used from copying a 420SP buffer
 *
 * @param[in] pu1_y_src
 *   Input Y pointer
 *
 * @param[in] pu1_uv_src
 *   Input UV pointer (UV is interleaved either in UV or VU format)
 *
 * @param[in] pu1_y_dst
 *   Output Y pointer
 *
 * @param[in] pu1_uv_dst
 *   Output UV pointer (UV is interleaved in the same format as that of input)
 *
 * @param[in] wd
 *   Width
 *
 * @param[in] ht
 *   Height
 *
 * @param[in] src_y_strd
 *   Input Y Stride
 *
 * @param[in] src_uv_strd
 *   Input UV stride
 *
 * @param[in] dst_y_strd
 *   Output Y stride
 *
 * @param[in] dst_uv_strd
 *   Output UV stride
 *
 * @returns None
 *
 * @remarks In case there is a need to perform partial frame copy then
 * by passion appropriate source and destination pointers and appropriate
 * values for wd and ht it can be done
 *
 *******************************************************************************
 */
void ih264d_fmt_conv_420sp_to_rgb565(UWORD8 *pu1_y_src,
                                     UWORD8 *pu1_uv_src,
                                     UWORD16 *pu2_rgb_dst,
                                     WORD32 wd,
                                     WORD32 ht,
                                     WORD32 src_y_strd,
                                     WORD32 src_uv_strd,
                                     WORD32 dst_strd,
                                     WORD32 is_u_first)
{

    WORD16 i2_r, i2_g, i2_b;
    UWORD32 u4_r, u4_g, u4_b;
    WORD16 i2_i, i2_j;
    UWORD8 *pu1_y_src_nxt;
    UWORD16 *pu2_rgb_dst_next_row;

    UWORD8 *pu1_u_src, *pu1_v_src;

    if(is_u_first)
    {
        pu1_u_src = (UWORD8 *)pu1_uv_src;
        pu1_v_src = (UWORD8 *)pu1_uv_src + 1;
    }
    else
    {
        pu1_u_src = (UWORD8 *)pu1_uv_src + 1;
        pu1_v_src = (UWORD8 *)pu1_uv_src;
    }

    pu1_y_src_nxt = pu1_y_src + src_y_strd;
    pu2_rgb_dst_next_row = pu2_rgb_dst + dst_strd;

    for(i2_i = 0; i2_i < (ht >> 1); i2_i++)
    {
        for(i2_j = (wd >> 1); i2_j > 0; i2_j--)
        {
            i2_b = ((*pu1_u_src - 128) * COEFF4 >> 13);
            i2_g = ((*pu1_u_src - 128) * COEFF2 + (*pu1_v_src - 128) * COEFF3)
                            >> 13;
            i2_r = ((*pu1_v_src - 128) * COEFF1) >> 13;

            pu1_u_src += 2;
            pu1_v_src += 2;
            /* pixel 0 */
            /* B */
            u4_b = CLIP_U8(*pu1_y_src + i2_b);
            u4_b >>= 3;
            /* G */
            u4_g = CLIP_U8(*pu1_y_src + i2_g);
            u4_g >>= 2;
            /* R */
            u4_r = CLIP_U8(*pu1_y_src + i2_r);
            u4_r >>= 3;

            pu1_y_src++;
            *pu2_rgb_dst++ = ((u4_r << 11) | (u4_g << 5) | u4_b);

            /* pixel 1 */
            /* B */
            u4_b = CLIP_U8(*pu1_y_src + i2_b);
            u4_b >>= 3;
            /* G */
            u4_g = CLIP_U8(*pu1_y_src + i2_g);
            u4_g >>= 2;
            /* R */
            u4_r = CLIP_U8(*pu1_y_src + i2_r);
            u4_r >>= 3;

            pu1_y_src++;
            *pu2_rgb_dst++ = ((u4_r << 11) | (u4_g << 5) | u4_b);

            /* pixel 2 */
            /* B */
            u4_b = CLIP_U8(*pu1_y_src_nxt + i2_b);
            u4_b >>= 3;
            /* G */
            u4_g = CLIP_U8(*pu1_y_src_nxt + i2_g);
            u4_g >>= 2;
            /* R */
            u4_r = CLIP_U8(*pu1_y_src_nxt + i2_r);
            u4_r >>= 3;

            pu1_y_src_nxt++;
            *pu2_rgb_dst_next_row++ = ((u4_r << 11) | (u4_g << 5) | u4_b);

            /* pixel 3 */
            /* B */
            u4_b = CLIP_U8(*pu1_y_src_nxt + i2_b);
            u4_b >>= 3;
            /* G */
            u4_g = CLIP_U8(*pu1_y_src_nxt + i2_g);
            u4_g >>= 2;
            /* R */
            u4_r = CLIP_U8(*pu1_y_src_nxt + i2_r);
            u4_r >>= 3;

            pu1_y_src_nxt++;
            *pu2_rgb_dst_next_row++ = ((u4_r << 11) | (u4_g << 5) | u4_b);

        }

        pu1_u_src = pu1_u_src + src_uv_strd - wd;
        pu1_v_src = pu1_v_src + src_uv_strd - wd;

        pu1_y_src = pu1_y_src + (src_y_strd << 1) - wd;
        pu1_y_src_nxt = pu1_y_src_nxt + (src_y_strd << 1) - wd;

        pu2_rgb_dst = pu2_rgb_dst_next_row - wd + dst_strd;
        pu2_rgb_dst_next_row = pu2_rgb_dst_next_row + (dst_strd << 1) - wd;
    }

}

void ih264d_fmt_conv_420sp_to_rgba8888(UWORD8 *pu1_y_src,
                                       UWORD8 *pu1_uv_src,
                                       UWORD32 *pu4_rgba_dst,
                                       WORD32 wd,
                                       WORD32 ht,
                                       WORD32 src_y_strd,
                                       WORD32 src_uv_strd,
                                       WORD32 dst_strd,
                                       WORD32 is_u_first)
{

    WORD16 i2_r, i2_g, i2_b;
    UWORD32 u4_r, u4_g, u4_b;
    WORD16 i2_i, i2_j;
    UWORD8 *pu1_y_src_nxt;
    UWORD32 *pu4_rgba_dst_next_row;

    UWORD8 *pu1_u_src, *pu1_v_src;

    if(is_u_first)
    {
        pu1_u_src = (UWORD8 *)pu1_uv_src;
        pu1_v_src = (UWORD8 *)pu1_uv_src + 1;
    }
    else
    {
        pu1_u_src = (UWORD8 *)pu1_uv_src + 1;
        pu1_v_src = (UWORD8 *)pu1_uv_src;
    }

    pu1_y_src_nxt = pu1_y_src + src_y_strd;
    pu4_rgba_dst_next_row = pu4_rgba_dst + dst_strd;

    for(i2_i = 0; i2_i < (ht >> 1); i2_i++)
    {
        for(i2_j = (wd >> 1); i2_j > 0; i2_j--)
        {
            i2_b = ((*pu1_u_src - 128) * COEFF4 >> 13);
            i2_g = ((*pu1_u_src - 128) * COEFF2 + (*pu1_v_src - 128) * COEFF3)
                            >> 13;
            i2_r = ((*pu1_v_src - 128) * COEFF1) >> 13;

            pu1_u_src += 2;
            pu1_v_src += 2;
            /* pixel 0 */
            /* B */
            u4_b = CLIP_U8(*pu1_y_src + i2_b);
            /* G */
            u4_g = CLIP_U8(*pu1_y_src + i2_g);
            /* R */
            u4_r = CLIP_U8(*pu1_y_src + i2_r);

            pu1_y_src++;
            *pu4_rgba_dst++ = ((u4_r << 16) | (u4_g << 8) | (u4_b << 0));

            /* pixel 1 */
            /* B */
            u4_b = CLIP_U8(*pu1_y_src + i2_b);
            /* G */
            u4_g = CLIP_U8(*pu1_y_src + i2_g);
            /* R */
            u4_r = CLIP_U8(*pu1_y_src + i2_r);

            pu1_y_src++;
            *pu4_rgba_dst++ = ((u4_r << 16) | (u4_g << 8) | (u4_b << 0));

            /* pixel 2 */
            /* B */
            u4_b = CLIP_U8(*pu1_y_src_nxt + i2_b);
            /* G */
            u4_g = CLIP_U8(*pu1_y_src_nxt + i2_g);
            /* R */
            u4_r = CLIP_U8(*pu1_y_src_nxt + i2_r);

            pu1_y_src_nxt++;
            *pu4_rgba_dst_next_row++ =
                            ((u4_r << 16) | (u4_g << 8) | (u4_b << 0));

            /* pixel 3 */
            /* B */
            u4_b = CLIP_U8(*pu1_y_src_nxt + i2_b);
            /* G */
            u4_g = CLIP_U8(*pu1_y_src_nxt + i2_g);
            /* R */
            u4_r = CLIP_U8(*pu1_y_src_nxt + i2_r);

            pu1_y_src_nxt++;
            *pu4_rgba_dst_next_row++ =
                            ((u4_r << 16) | (u4_g << 8) | (u4_b << 0));

        }

        pu1_u_src = pu1_u_src + src_uv_strd - wd;
        pu1_v_src = pu1_v_src + src_uv_strd - wd;

        pu1_y_src = pu1_y_src + (src_y_strd << 1) - wd;
        pu1_y_src_nxt = pu1_y_src_nxt + (src_y_strd << 1) - wd;

        pu4_rgba_dst = pu4_rgba_dst_next_row - wd + dst_strd;
        pu4_rgba_dst_next_row = pu4_rgba_dst_next_row + (dst_strd << 1) - wd;
    }

}

/**
 *******************************************************************************
 *
 * @brief Function used from copying a 420SP buffer
 *
 * @par   Description
 * Function used from copying a 420SP buffer
 *
 * @param[in] pu1_y_src
 *   Input Y pointer
 *
 * @param[in] pu1_uv_src
 *   Input UV pointer (UV is interleaved either in UV or VU format)
 *
 * @param[in] pu1_y_dst
 *   Output Y pointer
 *
 * @param[in] pu1_uv_dst
 *   Output UV pointer (UV is interleaved in the same format as that of input)
 *
 * @param[in] wd
 *   Width
 *
 * @param[in] ht
 *   Height
 *
 * @param[in] src_y_strd
 *   Input Y Stride
 *
 * @param[in] src_uv_strd
 *   Input UV stride
 *
 * @param[in] dst_y_strd
 *   Output Y stride
 *
 * @param[in] dst_uv_strd
 *   Output UV stride
 *
 * @returns None
 *
 * @remarks In case there is a need to perform partial frame copy then
 * by passion appropriate source and destination pointers and appropriate
 * values for wd and ht it can be done
 *
 *******************************************************************************
 */

void ih264d_fmt_conv_420sp_to_420sp(UWORD8 *pu1_y_src,
                                    UWORD8 *pu1_uv_src,
                                    UWORD8 *pu1_y_dst,
                                    UWORD8 *pu1_uv_dst,
                                    WORD32 wd,
                                    WORD32 ht,
                                    WORD32 src_y_strd,
                                    WORD32 src_uv_strd,
                                    WORD32 dst_y_strd,
                                    WORD32 dst_uv_strd)
{
    UWORD8 *pu1_src, *pu1_dst;
    WORD32 num_rows, num_cols, src_strd, dst_strd;
    WORD32 i;

    /* copy luma */
    pu1_src = (UWORD8 *)pu1_y_src;
    pu1_dst = (UWORD8 *)pu1_y_dst;

    num_rows = ht;
    num_cols = wd;

    src_strd = src_y_strd;
    dst_strd = dst_y_strd;

    for(i = 0; i < num_rows; i++)
    {
        memcpy(pu1_dst, pu1_src, num_cols);
        pu1_dst += dst_strd;
        pu1_src += src_strd;
    }

    /* copy U and V */
    pu1_src = (UWORD8 *)pu1_uv_src;
    pu1_dst = (UWORD8 *)pu1_uv_dst;

    num_rows = ht >> 1;
    num_cols = wd;

    src_strd = src_uv_strd;
    dst_strd = dst_uv_strd;

    for(i = 0; i < num_rows; i++)
    {
        memcpy(pu1_dst, pu1_src, num_cols);
        pu1_dst += dst_strd;
        pu1_src += src_strd;
    }
    return;
}

/**
 *******************************************************************************
 *
 * @brief Function used from copying a 420SP buffer
 *
 * @par   Description
 * Function used from copying a 420SP buffer
 *
 * @param[in] pu1_y_src
 *   Input Y pointer
 *
 * @param[in] pu1_uv_src
 *   Input UV pointer (UV is interleaved either in UV or VU format)
 *
 * @param[in] pu1_y_dst
 *   Output Y pointer
 *
 * @param[in] pu1_uv_dst
 *   Output UV pointer (UV is interleaved in the same format as that of input)
 *
 * @param[in] wd
 *   Width
 *
 * @param[in] ht
 *   Height
 *
 * @param[in] src_y_strd
 *   Input Y Stride
 *
 * @param[in] src_uv_strd
 *   Input UV stride
 *
 * @param[in] dst_y_strd
 *   Output Y stride
 *
 * @param[in] dst_uv_strd
 *   Output UV stride
 *
 * @returns None
 *
 * @remarks In case there is a need to perform partial frame copy then
 * by passion appropriate source and destination pointers and appropriate
 * values for wd and ht it can be done
 *
 *******************************************************************************
 */
void ih264d_fmt_conv_420sp_to_420sp_swap_uv(UWORD8 *pu1_y_src,
                                            UWORD8 *pu1_uv_src,
                                            UWORD8 *pu1_y_dst,
                                            UWORD8 *pu1_uv_dst,
                                            WORD32 wd,
                                            WORD32 ht,
                                            WORD32 src_y_strd,
                                            WORD32 src_uv_strd,
                                            WORD32 dst_y_strd,
                                            WORD32 dst_uv_strd)
{
    UWORD8 *pu1_src, *pu1_dst;
    WORD32 num_rows, num_cols, src_strd, dst_strd;
    WORD32 i;

    /* copy luma */
    pu1_src = (UWORD8 *)pu1_y_src;
    pu1_dst = (UWORD8 *)pu1_y_dst;

    num_rows = ht;
    num_cols = wd;

    src_strd = src_y_strd;
    dst_strd = dst_y_strd;

    for(i = 0; i < num_rows; i++)
    {
        memcpy(pu1_dst, pu1_src, num_cols);
        pu1_dst += dst_strd;
        pu1_src += src_strd;
    }

    /* copy U and V */
    pu1_src = (UWORD8 *)pu1_uv_src;
    pu1_dst = (UWORD8 *)pu1_uv_dst;

    num_rows = ht >> 1;
    num_cols = wd;

    src_strd = src_uv_strd;
    dst_strd = dst_uv_strd;

    for(i = 0; i < num_rows; i++)
    {
        WORD32 j;
        for(j = 0; j < num_cols; j += 2)
        {
            pu1_dst[j + 0] = pu1_src[j + 1];
            pu1_dst[j + 1] = pu1_src[j + 0];
        }
        pu1_dst += dst_strd;
        pu1_src += src_strd;
    }
    return;
}
/**
 *******************************************************************************
 *
 * @brief Function used from copying a 420SP buffer
 *
 * @par   Description
 * Function used from copying a 420SP buffer
 *
 * @param[in] pu1_y_src
 *   Input Y pointer
 *
 * @param[in] pu1_uv_src
 *   Input UV pointer (UV is interleaved either in UV or VU format)
 *
 * @param[in] pu1_y_dst
 *   Output Y pointer
 *
 * @param[in] pu1_u_dst
 *   Output U pointer
 *
 * @param[in] pu1_v_dst
 *   Output V pointer
 *
 * @param[in] wd
 *   Width
 *
 * @param[in] ht
 *   Height
 *
 * @param[in] src_y_strd
 *   Input Y Stride
 *
 * @param[in] src_uv_strd
 *   Input UV stride
 *
 * @param[in] dst_y_strd
 *   Output Y stride
 *
 * @param[in] dst_uv_strd
 *   Output UV stride
 *
 * @param[in] is_u_first
 *   Flag to indicate if U is the first byte in input chroma part
 *
 * @returns none
 *
 * @remarks In case there is a need to perform partial frame copy then
 * by passion appropriate source and destination pointers and appropriate
 * values for wd and ht it can be done
 *
 *******************************************************************************
 */

void ih264d_fmt_conv_420sp_to_420p(UWORD8 *pu1_y_src,
                                   UWORD8 *pu1_uv_src,
                                   UWORD8 *pu1_y_dst,
                                   UWORD8 *pu1_u_dst,
                                   UWORD8 *pu1_v_dst,
                                   WORD32 wd,
                                   WORD32 ht,
                                   WORD32 src_y_strd,
                                   WORD32 src_uv_strd,
                                   WORD32 dst_y_strd,
                                   WORD32 dst_uv_strd,
                                   WORD32 is_u_first,
                                   WORD32 disable_luma_copy)
{
    UWORD8 *pu1_src, *pu1_dst;
    UWORD8 *pu1_u_src, *pu1_v_src;
    WORD32 num_rows, num_cols, src_strd, dst_strd;
    WORD32 i, j;

    if(0 == disable_luma_copy)
    {
        /* copy luma */
        pu1_src = (UWORD8 *)pu1_y_src;
        pu1_dst = (UWORD8 *)pu1_y_dst;

        num_rows = ht;
        num_cols = wd;

        src_strd = src_y_strd;
        dst_strd = dst_y_strd;

        for(i = 0; i < num_rows; i++)
        {
            memcpy(pu1_dst, pu1_src, num_cols);
            pu1_dst += dst_strd;
            pu1_src += src_strd;
        }
    }
    /* de-interleave U and V and copy to destination */
    if(is_u_first)
    {
        pu1_u_src = (UWORD8 *)pu1_uv_src;
        pu1_v_src = (UWORD8 *)pu1_uv_src + 1;
    }
    else
    {
        pu1_u_src = (UWORD8 *)pu1_uv_src + 1;
        pu1_v_src = (UWORD8 *)pu1_uv_src;
    }

    num_rows = ht >> 1;
    num_cols = wd >> 1;

    src_strd = src_uv_strd;
    dst_strd = dst_uv_strd;

    for(i = 0; i < num_rows; i++)
    {
        for(j = 0; j < num_cols; j++)
        {
            pu1_u_dst[j] = pu1_u_src[j * 2];
            pu1_v_dst[j] = pu1_v_src[j * 2];
        }

        pu1_u_dst += dst_strd;
        pu1_v_dst += dst_strd;
        pu1_u_src += src_strd;
        pu1_v_src += src_strd;
    }
    return;
}

/*****************************************************************************/
/*  Function Name : ih264d_format_convert                                    */
/*                                                                           */
/*  Description   : Implements format conversion/frame copy                  */
/*  Inputs        : ps_dec - Decoder parameters                              */
/*  Globals       : None                                                     */
/*  Processing    : Refer bumping process in the standard                    */
/*  Outputs       : Assigns display sequence number.                         */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         27 04 2005   NS              Draft                                */
/*                                                                           */
/*****************************************************************************/
void ih264d_format_convert(dec_struct_t *ps_dec,
                           ivd_get_display_frame_op_t *pv_disp_op,
                           UWORD32 u4_start_y,
                           UWORD32 u4_num_rows_y)
{
    UWORD32 convert_uv_only = 0;
    iv_yuv_buf_t *ps_op_frm;
    UWORD8 *pu1_y_src, *pu1_uv_src;
    UWORD32 start_uv = u4_start_y >> 1;

    if(1 == pv_disp_op->u4_error_code)
        return;

    ps_op_frm = &(ps_dec->s_disp_frame_info);

    /* Requires u4_start_y and u4_num_rows_y to be even */
    if(u4_start_y & 1)
    {
        return;
    }

    if((1 == ps_dec->u4_share_disp_buf) &&
       (pv_disp_op->e_output_format == IV_YUV_420SP_UV))
    {
        return;
    }

    pu1_y_src = (UWORD8 *)ps_op_frm->pv_y_buf;
    pu1_y_src += u4_start_y * ps_op_frm->u4_y_strd,

    pu1_uv_src = (UWORD8 *)ps_op_frm->pv_u_buf;
    pu1_uv_src += start_uv * ps_op_frm->u4_u_strd;

    if(pv_disp_op->e_output_format == IV_YUV_420P)
    {
        UWORD8 *pu1_y_dst, *pu1_u_dst, *pu1_v_dst;
        IV_COLOR_FORMAT_T e_output_format = pv_disp_op->e_output_format;

        if(0 == ps_dec->u4_share_disp_buf)
        {
            convert_uv_only = 0;
        }
        else
        {
            convert_uv_only = 1;
        }

        pu1_y_dst = (UWORD8 *)pv_disp_op->s_disp_frm_buf.pv_y_buf;
        pu1_y_dst += u4_start_y * pv_disp_op->s_disp_frm_buf.u4_y_strd;

        pu1_u_dst = (UWORD8 *)pv_disp_op->s_disp_frm_buf.pv_u_buf;
        pu1_u_dst += start_uv * pv_disp_op->s_disp_frm_buf.u4_u_strd;

        pu1_v_dst = (UWORD8 *)pv_disp_op->s_disp_frm_buf.pv_v_buf;
        pu1_v_dst += start_uv * pv_disp_op->s_disp_frm_buf.u4_v_strd;

        ih264d_fmt_conv_420sp_to_420p(pu1_y_src,
                                      pu1_uv_src,
                                      pu1_y_dst,
                                      pu1_u_dst,
                                      pu1_v_dst,
                                      ps_op_frm->u4_y_wd,
                                      u4_num_rows_y,
                                      ps_op_frm->u4_y_strd,
                                      ps_op_frm->u4_u_strd,
                                      pv_disp_op->s_disp_frm_buf.u4_y_strd,
                                      pv_disp_op->s_disp_frm_buf.u4_u_strd,
                                      1,
                                      convert_uv_only);

    }
    else if((pv_disp_op->e_output_format == IV_YUV_420SP_UV) ||
            (pv_disp_op->e_output_format == IV_YUV_420SP_VU))
    {
        UWORD8* pu1_y_dst, *pu1_uv_dst;

        pu1_y_dst = (UWORD8 *)pv_disp_op->s_disp_frm_buf.pv_y_buf;
        pu1_y_dst +=  u4_start_y * pv_disp_op->s_disp_frm_buf.u4_y_strd;

        pu1_uv_dst = (UWORD8 *)pv_disp_op->s_disp_frm_buf.pv_u_buf;
        pu1_uv_dst += start_uv * pv_disp_op->s_disp_frm_buf.u4_u_strd;

        if(pv_disp_op->e_output_format == IV_YUV_420SP_UV)
        {
            ih264d_fmt_conv_420sp_to_420sp(pu1_y_src,
                                           pu1_uv_src,
                                           pu1_y_dst,
                                           pu1_uv_dst,
                                           ps_op_frm->u4_y_wd,
                                           u4_num_rows_y,
                                           ps_op_frm->u4_y_strd,
                                           ps_op_frm->u4_u_strd,
                                           pv_disp_op->s_disp_frm_buf.u4_y_strd,
                                           pv_disp_op->s_disp_frm_buf.u4_u_strd);
        }
        else
        {
            ih264d_fmt_conv_420sp_to_420sp_swap_uv(pu1_y_src,
                                                   pu1_uv_src,
                                                   pu1_y_dst,
                                                   pu1_uv_dst,
                                                   ps_op_frm->u4_y_wd,
                                                   u4_num_rows_y,
                                                   ps_op_frm->u4_y_strd,
                                                   ps_op_frm->u4_u_strd,
                                                   pv_disp_op->s_disp_frm_buf.u4_y_strd,
                                                   pv_disp_op->s_disp_frm_buf.u4_u_strd);
        }
    }
    else if(pv_disp_op->e_output_format == IV_RGB_565)
    {
        UWORD16 *pu2_rgb_dst;

        pu2_rgb_dst = (UWORD16 *)pv_disp_op->s_disp_frm_buf.pv_y_buf;
        pu2_rgb_dst += u4_start_y * pv_disp_op->s_disp_frm_buf.u4_y_strd;

        ih264d_fmt_conv_420sp_to_rgb565(pu1_y_src,
                                        pu1_uv_src,
                                        pu2_rgb_dst,
                                        ps_op_frm->u4_y_wd,
                                        u4_num_rows_y,
                                        ps_op_frm->u4_y_strd,
                                        ps_op_frm->u4_u_strd,
                                        pv_disp_op->s_disp_frm_buf.u4_y_strd,
                                        1);
    }

    if((u4_start_y + u4_num_rows_y) >= ps_dec->s_disp_frame_info.u4_y_ht)
    {

        INSERT_LOGO(pv_disp_op->s_disp_frm_buf.pv_y_buf,
                        pv_disp_op->s_disp_frm_buf.pv_u_buf,
                        pv_disp_op->s_disp_frm_buf.pv_v_buf,
                        pv_disp_op->s_disp_frm_buf.u4_y_strd,
                        ps_dec->u2_disp_width,
                        ps_dec->u2_disp_height,
                        pv_disp_op->e_output_format,
                        ps_op_frm->u4_y_wd,
                        ps_op_frm->u4_y_ht);
    }

    return;
}
