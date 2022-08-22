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
/*  File Name         : ih264d_format_conv.h                                */
/*                                                                           */
/*  Description       : Contains coefficients and constant reqquired for     */
/*                      converting from rgb and gray color spaces to yuv422i */
/*                      color space                                          */
/*                                                                           */
/*  List of Functions : None                                                 */
/*                                                                           */
/*  Issues / Problems : None                                                 */
/*                                                                           */
/*  Revision History  :                                                      */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         27 08 2007  Naveen Kumar T        Draft                           */
/*                                                                           */
/*****************************************************************************/

#ifndef _IH264D_FORMAT_CONV_H_
#define _IH264D_FORMAT_CONV_H_

/*****************************************************************************/
/* Typedefs                                                                  */
/*****************************************************************************/

#define COEFF_0_Y       66
#define COEFF_1_Y       129
#define COEFF_2_Y       25
#define COEFF_0_U       -38
#define COEFF_1_U       -75
#define COEFF_2_U       112
#define COEFF_0_V       112
#define COEFF_1_V       -94
#define COEFF_2_V       -18
#define CONST_RGB_YUV1  4096
#define CONST_RGB_YUV2  32768
#define CONST_GRAY_YUV  128
#define COEF_2_V2_U  0xFFEE0070

#define COF_2Y_0Y          0X00190042
#define COF_1U_0U          0XFFB5FFDA
#define COF_1V_0V          0XFFA20070

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
                                   WORD32 disable_luma_copy);

void ih264d_fmt_conv_420sp_to_420sp_swap_uv(UWORD8 *pu1_y_src,
                                            UWORD8 *pu1_uv_src,
                                            UWORD8 *pu1_y_dst,
                                            UWORD8 *pu1_uv_dst,
                                            WORD32 wd,
                                            WORD32 ht,
                                            WORD32 src_y_strd,
                                            WORD32 src_uv_strd,
                                            WORD32 dst_y_strd,
                                            WORD32 dst_uv_strd);

void ih264d_fmt_conv_420sp_to_420sp(UWORD8 *pu1_y_src,
                                    UWORD8 *pu1_uv_src,
                                    UWORD8 *pu1_y_dst,
                                    UWORD8 *pu1_uv_dst,
                                    WORD32 wd,
                                    WORD32 ht,
                                    WORD32 src_y_strd,
                                    WORD32 src_uv_strd,
                                    WORD32 dst_y_strd,
                                    WORD32 dst_uv_strd);

void ih264d_fmt_conv_420sp_to_rgb565(UWORD8 *pu1_y_src,
                                     UWORD8 *pu1_uv_src,
                                     UWORD16 *pu2_rgb_dst,
                                     WORD32 wd,
                                     WORD32 ht,
                                     WORD32 src_y_strd,
                                     WORD32 src_uv_strd,
                                     WORD32 dst_strd,
                                     WORD32 is_u_first);
#define COEFF1          13073
#define COEFF2          -3207
#define COEFF3          -6664
#define COEFF4          16530

void ih264d_format_convert(dec_struct_t *ps_dec,
                           ivd_get_display_frame_op_t *pv_disp_op,
                           UWORD32 u4_start_y,
                           UWORD32 u4_num_rows_y);


#endif /* _IH264D_FORMAT_CONV_H_ */
