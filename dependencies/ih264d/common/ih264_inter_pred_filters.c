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
 *  ih264_inter_pred_filters.c
 *
 * @brief
 *  Contains function definitions for inter prediction interpolation filters
 *
 * @author
 *  Ittiam
 *
 * @par List of Functions:
 *  - ih264_inter_pred_luma_copy
 *  - ih264_interleave_copy
 *  - ih264_inter_pred_luma_horz
 *  - ih264_inter_pred_luma_vert
 *  - ih264_inter_pred_luma_horz_hpel_vert_hpel
 *  - ih264_inter_pred_luma_horz_qpel
 *  - ih264_inter_pred_luma_vert_qpel
 *  - ih264_inter_pred_luma_horz_qpel_vert_qpel
 *  - ih264_inter_pred_luma_horz_hpel_vert_qpel
 *  - ih264_inter_pred_luma_horz_qpel_vert_hpel
 *  - ih264_inter_pred_luma_bilinear
 *  - ih264_inter_pred_chroma
 *
 * @remarks
 *  None
 *
 *******************************************************************************
 */

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

/* User include files */
#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264_inter_pred_filters.h"


/*****************************************************************************/
/* Constant Data variables                                                   */
/*****************************************************************************/

/* coefficients for 6 tap filtering*/
const WORD32 ih264_g_six_tap[3] ={1,-5,20};


/*****************************************************************************/
/*  Function definitions .                                                   */
/*****************************************************************************/
/**
 *******************************************************************************
 *
 * @brief
 * Interprediction luma function for copy
 *
 * @par Description:
 *    Copies the array of width 'wd' and height 'ht' from the  location pointed
 *    by 'src' to the location pointed by 'dst'
 *
 * @param[in] pu1_src
 *  UWORD8 pointer to the source
 *
 * @param[out] pu1_dst
 *  UWORD8 pointer to the destination
 *
 * @param[in] src_strd
 *  integer source stride
 *
 * @param[in] dst_strd
 *  integer destination stride
 *
 *
 * @param[in] ht
 *  integer height of the array
 *
 * @param[in] wd
 *  integer width of the array
 *
 * @returns
 *
 * @remarks
 *  None
 *
 *******************************************************************************
 */

void ih264_inter_pred_luma_copy(UWORD8 *pu1_src,
                                UWORD8 *pu1_dst,
                                WORD32 src_strd,
                                WORD32 dst_strd,
                                WORD32 ht,
                                WORD32 wd,
                                UWORD8* pu1_tmp,
                                WORD32 dydx)
{
    WORD32 row, col;
    UNUSED(pu1_tmp);
    UNUSED(dydx);
    for(row = 0; row < ht; row++)
    {
        for(col = 0; col < wd; col++)
        {
            pu1_dst[col] = pu1_src[col];
        }

        pu1_src += src_strd;
        pu1_dst += dst_strd;
    }
}

/**
 *******************************************************************************
 *
 * @brief
 * Fucntion for copying to an interleaved destination
 *
 * @par Description:
 *    Copies the array of width 'wd' and height 'ht' from the  location pointed
 *    by 'src' to the location pointed by 'dst'
 *
 * @param[in] pu1_src
 *  UWORD8 pointer to the source
 *
 * @param[out] pu1_dst
 *  UWORD8 pointer to the destination
 *
 * @param[in] src_strd
 *  integer source stride
 *
 * @param[in] dst_strd
 *  integer destination stride
 *
 * @param[in] ht
 *  integer height of the array
 *
 * @param[in] wd
 *  integer width of the array
 *
 * @returns
 *
 * @remarks
 *  The alternate elements of src will be copied to alternate locations in dsr
 *  Other locations are not touched
 *
 *******************************************************************************
 */
void ih264_interleave_copy(UWORD8 *pu1_src,
                           UWORD8 *pu1_dst,
                           WORD32 src_strd,
                           WORD32 dst_strd,
                           WORD32 ht,
                           WORD32 wd)
{
    WORD32 row, col;
    wd *= 2;

    for(row = 0; row < ht; row++)
    {
        for(col = 0; col < wd; col+=2)
        {
            pu1_dst[col] = pu1_src[col];
        }

        pu1_src += src_strd;
        pu1_dst += dst_strd;
    }
}

/**
 *******************************************************************************
 *
 * @brief
 *     Interprediction luma filter for horizontal input
 *
 * @par Description:
 *    Applies a 6 tap horizontal filter .The output is  clipped to 8 bits
 *    sec 8.4.2.2.1 titled "Luma sample interpolation process"
 *
 * @param[in] pu1_src
 *  UWORD8 pointer to the source
 *
 * @param[out] pu1_dst
 *  UWORD8 pointer to the destination
 *
 * @param[in] src_strd
 *  integer source stride
 *
 * @param[in] dst_strd
 *  integer destination stride
 *
 * @param[in] ht
 *  integer height of the array
 *
 * @param[in] wd
 *  integer width of the array
 *
 * @returns
 *
 * @remarks
 *  None
 *
 *******************************************************************************
 */
void ih264_inter_pred_luma_horz(UWORD8 *pu1_src,
                                UWORD8 *pu1_dst,
                                WORD32 src_strd,
                                WORD32 dst_strd,
                                WORD32 ht,
                                WORD32 wd,
                                UWORD8* pu1_tmp,
                                WORD32 dydx)
{
    WORD32 row, col;
    WORD16 i2_tmp;
    UNUSED(pu1_tmp);
    UNUSED(dydx);

    for(row = 0; row < ht; row++)
    {
        for(col = 0; col < wd; col++)
        {
            i2_tmp = 0;/*ih264_g_six_tap[] is the array containing the filter coeffs*/
            i2_tmp = ih264_g_six_tap[0] *
                            (pu1_src[col - 2] + pu1_src[col + 3])
                     + ih264_g_six_tap[1] *
                            (pu1_src[col - 1] + pu1_src[col + 2])
                     + ih264_g_six_tap[2] *
                            (pu1_src[col] + pu1_src[col + 1]);
            i2_tmp = (i2_tmp + 16) >> 5;
            pu1_dst[col] = CLIP_U8(i2_tmp);
        }

        pu1_src += src_strd;
        pu1_dst += dst_strd;
    }

}

/**
 *******************************************************************************
 *
 * @brief
 *    Interprediction luma filter for vertical input
 *
 * @par Description:
 *   Applies a 6 tap vertical filter.The output is  clipped to 8 bits
 *    sec 8.4.2.2.1 titled "Luma sample interpolation process"
 *
 * @param[in] pu1_src
 *  UWORD8 pointer to the source
 *
 * @param[out] pu1_dst
 *  UWORD8 pointer to the destination
 *
 * @param[in] src_strd
 *  integer source stride
 *
 * @param[in] dst_strd
 *  integer destination stride
 *
 * @param[in] ht
 *  integer height of the array
 *
 * @param[in] wd
 *  integer width of the array
 *
 * @returns
 *
 * @remarks
 *  None
 *
 *******************************************************************************
 */
void ih264_inter_pred_luma_vert(UWORD8 *pu1_src,
                                UWORD8 *pu1_dst,
                                WORD32 src_strd,
                                WORD32 dst_strd,
                                WORD32 ht,
                                WORD32 wd,
                                UWORD8* pu1_tmp,
                                WORD32 dydx)
{
    WORD32 row, col;
    WORD16 i2_tmp;
    UNUSED(pu1_tmp);
    UNUSED(dydx);

    for(row = 0; row < ht; row++)
    {
        for(col = 0; col < wd; col++)
        {
            i2_tmp = 0; /*ih264_g_six_tap[] is the array containing the filter coeffs*/
            i2_tmp = ih264_g_six_tap[0] *
                            (pu1_src[col - 2 * src_strd] + pu1_src[col + 3 * src_strd])
                     + ih264_g_six_tap[1] *
                            (pu1_src[col - 1 * src_strd] + pu1_src[col + 2 * src_strd])
                     + ih264_g_six_tap[2] *
                            (pu1_src[col] + pu1_src[col + 1 * src_strd]);
            i2_tmp = (i2_tmp + 16) >> 5;
            pu1_dst[col] = CLIP_U8(i2_tmp);
        }
        pu1_src += src_strd;
        pu1_dst += dst_strd;
    }
}

/*!
 **************************************************************************
 * \if Function name : ih264_inter_pred_luma_horz_hpel_vert_hpel \endif
 *
 * \brief
 *    This function implements a two stage cascaded six tap filter. It
 *    applies the six tap filter in the horizontal direction on the
 *    predictor values, followed by applying the same filter in the
 *    vertical direction on the output of the first stage. The six tap
 *    filtering operation is described in sec 8.4.2.2.1 titled "Luma sample
 *    interpolation process"
 *
 * \param pu1_src: Pointer to the buffer containing the predictor values.
 *     pu1_src could point to the frame buffer or the predictor buffer.
 * \param pu1_dst: Pointer to the destination buffer where the output of
 *     the six tap filter is stored.
 * \param ht: Height of the rectangular pixel grid to be interpolated
 * \param wd: Width of the rectangular pixel grid to be interpolated
 * \param src_strd: Width of the buffer pointed to by pu1_src.
 * \param dst_strd: Width of the destination buffer
 * \param pu1_tmp: temporary buffer.
 * \param dydx: x and y reference offset for qpel calculations: UNUSED in this function.
 *
 * \return
 *    None.
 *
 * \note
 *    This function takes the 8 bit predictor values, applies the six tap
 *    filter in the horizontal direction and outputs the result clipped to
 *    8 bit precision. The input is stored in the buffer pointed to by
 *    pu1_src while the output is stored in the buffer pointed by pu1_dst.
 *    Both pu1_src and pu1_dst could point to the same buffer i.e. the
 *    six tap filter could be done in place.
 *
 **************************************************************************
 */
void ih264_inter_pred_luma_horz_hpel_vert_hpel(UWORD8 *pu1_src,
                                               UWORD8 *pu1_dst,
                                               WORD32 src_strd,
                                               WORD32 dst_strd,
                                               WORD32 ht,
                                               WORD32 wd,
                                               UWORD8* pu1_tmp,
                                               WORD32 dydx)
{
    WORD32 row, col;
    WORD32 tmp;
    WORD16* pi2_pred1_temp;
    WORD16* pi2_pred1;
    UNUSED(dydx);
    pi2_pred1_temp = (WORD16*)pu1_tmp;
    pi2_pred1_temp += 2;
    pi2_pred1 = pi2_pred1_temp;
    for(row = 0; row < ht; row++)
    {
        for(col = -2; col < wd + 3; col++)
        {
            tmp = 0;/*ih264_g_six_tap[] is the array containing the filter coeffs*/
            tmp = ih264_g_six_tap[0] *
                            (pu1_src[col - 2 * src_strd] + pu1_src[col + 3 * src_strd])
                  + ih264_g_six_tap[1] *
                            (pu1_src[col - 1 * src_strd] + pu1_src[col + 2 * src_strd])
                  + ih264_g_six_tap[2] *
                            (pu1_src[col] + pu1_src[col + 1 * src_strd]);
            pi2_pred1_temp[col] = tmp;
        }
        pu1_src += src_strd;
        pi2_pred1_temp = pi2_pred1_temp + wd + 5;
    }

    for(row = 0; row < ht; row++)
    {
        for(col = 0; col < wd; col++)
        {
            tmp = 0;/*ih264_g_six_tap[] is the array containing the filter coeffs*/
            tmp = ih264_g_six_tap[0] *
                            (pi2_pred1[col - 2] + pi2_pred1[col + 3])
                  + ih264_g_six_tap[1] *
                            (pi2_pred1[col - 1] + pi2_pred1[col + 2])
                  + ih264_g_six_tap[2] * (pi2_pred1[col] + pi2_pred1[col + 1]);
            tmp = (tmp + 512) >> 10;
            pu1_dst[col] = CLIP_U8(tmp);
        }
        pi2_pred1 += (wd + 5);
        pu1_dst += dst_strd;
    }
}

/*!
 **************************************************************************
 * \if Function name : ih264_inter_pred_luma_horz_qpel \endif
 *
 * \brief
 *    This routine applies the six tap filter to the predictors in the
 *    horizontal direction. The six tap filtering operation is described in
 *    sec 8.4.2.2.1 titled "Luma sample interpolation process"
 *
 * \param pu1_src: Pointer to the buffer containing the predictor values.
 *     pu1_src could point to the frame buffer or the predictor buffer.
 * \param pu1_dst: Pointer to the destination buffer where the output of
 *     the six tap filter is stored.
 * \param ht: Height of the rectangular pixel grid to be interpolated
 * \param wd: Width of the rectangular pixel grid to be interpolated
 * \param src_strd: Width of the buffer pointed to by pu1_src.
 * \param dst_strd: Width of the destination buffer
 * \param pu1_tmp: temporary buffer: UNUSED in this function
 * \param dydx: x and y reference offset for qpel calculations.
 *
 * \return
 *    None.
 *
 * \note
 *    This function takes the 8 bit predictor values, applies the six tap
 *    filter in the horizontal direction and outputs the result clipped to
 *    8 bit precision. The input is stored in the buffer pointed to by
 *    pu1_src while the output is stored in the buffer pointed by pu1_dst.
 *    Both pu1_src and pu1_dst could point to the same buffer i.e. the
 *    six tap filter could be done in place.
 *
 **************************************************************************
 */
void ih264_inter_pred_luma_horz_qpel(UWORD8 *pu1_src,
                                     UWORD8 *pu1_dst,
                                     WORD32 src_strd,
                                     WORD32 dst_strd,
                                     WORD32 ht,
                                     WORD32 wd,
                                     UWORD8* pu1_tmp,
                                     WORD32 dydx)
{
    WORD32 row, col;
    UWORD8 *pu1_pred1;
    WORD32 x_offset = dydx & 0x3;
    UNUSED(pu1_tmp);
    pu1_pred1 = pu1_src + (x_offset >> 1);

    for(row = 0; row < ht; row++)
    {
        for(col = 0; col < wd; col++, pu1_src++, pu1_dst++)
        {
            WORD16 i2_temp;
            /* The logic below implements the following equation
             i2_temp = puc_pred[-2] - 5 * (puc_pred[-1] + puc_pred[2]) +
             20 * (puc_pred[0] + puc_pred[1]) + puc_pred[3]; */
            i2_temp = pu1_src[-2] + pu1_src[3]
                      - (pu1_src[-1] + pu1_src[2])
                      + ((pu1_src[0] + pu1_src[1] - pu1_src[-1] - pu1_src[2]) << 2)
                      + ((pu1_src[0] + pu1_src[1]) << 4);
            i2_temp = (i2_temp + 16) >> 5;
            i2_temp = CLIP_U8(i2_temp);
            *pu1_dst = (i2_temp + *pu1_pred1 + 1) >> 1;

            pu1_pred1++;
        }
        pu1_dst += dst_strd - wd;
        pu1_src += src_strd - wd;
        pu1_pred1 += src_strd - wd;
    }
}

/*!
 **************************************************************************
 * \if Function name : ih264_inter_pred_luma_vert_qpel \endif
 *
 * \brief
 *    This routine applies the six tap filter to the predictors in the
 *    vertical direction and interpolates them to obtain pixels at quarter vertical
 *    positions (0, 1/4) and (0, 3/4). The six tap filtering operation is
 *    described in sec 8.4.2.2.1 titled "Luma sample interpolation process"
 *
 * \param pu1_src: Pointer to the buffer containing the predictor values.
 *     pu1_src could point to the frame buffer or the predictor buffer.
 * \param pu1_dst: Pointer to the destination buffer where the output of
 *     the six tap filter is stored.
 * \param ht: Height of the rectangular pixel grid to be interpolated
 * \param wd: Width of the rectangular pixel grid to be interpolated
 * \param src_strd: Width of the buffer pointed to by puc_pred.
 * \param dst_strd: Width of the destination buffer
 * \param pu1_tmp: temporary buffer: UNUSED in this function
 * \param dydx: x and y reference offset for qpel calculations.
 *
 * \return
 *    void
 *
 * \note
 *    This function takes the 8 bit predictor values, applies the six tap
 *    filter in the vertical direction and outputs the result clipped to
 *    8 bit precision. The input is stored in the buffer pointed to by
 *    puc_pred while the output is stored in the buffer pointed by puc_dest.
 *    Both puc_pred and puc_dest could point to the same buffer i.e. the
 *    six tap filter could be done in place.
 *
 * \para <title>
 *    <paragraph>
 *  ...
 **************************************************************************
 */
void ih264_inter_pred_luma_vert_qpel(UWORD8 *pu1_src,
                                     UWORD8 *pu1_dst,
                                     WORD32 src_strd,
                                     WORD32 dst_strd,
                                     WORD32 ht,
                                     WORD32 wd,
                                     UWORD8* pu1_tmp,
                                     WORD32 dydx)
{
    WORD32 row, col;
    WORD32 y_offset = dydx >> 2;
    WORD32 off1, off2, off3;
    UWORD8 *pu1_pred1;
    UNUSED(pu1_tmp);
    y_offset = y_offset & 0x3;

    off1 = src_strd;
    off2 = src_strd << 1;
    off3 = off1 + off2;

    pu1_pred1 = pu1_src + (y_offset >> 1) * src_strd;

    for(row = 0; row < ht; row++)
    {
        for(col = 0; col < wd; col++, pu1_dst++, pu1_src++, pu1_pred1++)
        {
            WORD16 i2_temp;
            /* The logic below implements the following equation
             i16_temp = puc_pred[-2*src_strd] + puc_pred[3*src_strd] -
             5 * (puc_pred[-1*src_strd] + puc_pred[2*src_strd])  +
             20 * (puc_pred[0] + puc_pred[src_strd]); */
            i2_temp = pu1_src[-off2] + pu1_src[off3]
                       - (pu1_src[-off1] + pu1_src[off2])
                       + ((pu1_src[0] + pu1_src[off1] - pu1_src[-off1] - pu1_src[off2]) << 2)
                       + ((pu1_src[0] + pu1_src[off1]) << 4);
            i2_temp = (i2_temp + 16) >> 5;
            i2_temp = CLIP_U8(i2_temp);

            *pu1_dst = (i2_temp + *pu1_pred1 + 1) >> 1;
        }
        pu1_src += src_strd - wd;
        pu1_pred1 += src_strd - wd;
        pu1_dst += dst_strd - wd;
    }
}

/*!
 **************************************************************************
 * \if Function name : ih264_inter_pred_luma_horz_qpel_vert_qpel \endif
 *
 * \brief
 *    This routine applies the six tap filter to the predictors in the
 *    vertical and horizontal direction and averages them to get pixels at locations
 *    (1/4,1/4), (1/4, 3/4), (3/4, 1/4) & (3/4, 3/4). The six tap filtering operation
 *    is described in sec 8.4.2.2.1 titled "Luma sample interpolation process"
 *
 * \param pu1_src: Pointer to the buffer containing the predictor values.
 *     pu1_src could point to the frame buffer or the predictor buffer.
 * \param pu1_dst: Pointer to the destination buffer where the output of
 *     the six tap filter is stored.
 * \param wd: Width of the rectangular pixel grid to be interpolated
 * \param ht: Height of the rectangular pixel grid to be interpolated
 * \param src_strd: Width of the buffer pointed to by puc_pred.
 * \param dst_strd: Width of the destination buffer
 * \param pu1_tmp: temporary buffer, UNUSED in this function
 * \param dydx: x and y reference offset for qpel calculations.
 *
 * \return
 *    void
 *
 * \note
 *    This function takes the 8 bit predictor values, applies the six tap
 *    filter in the vertical direction and outputs the result clipped to
 *    8 bit precision. The input is stored in the buffer pointed to by
 *    puc_pred while the output is stored in the buffer pointed by puc_dest.
 *    Both puc_pred and puc_dest could point to the same buffer i.e. the
 *    six tap filter could be done in place.
 *
 * \para <title>
 *    <paragraph>
 *  ...
 **************************************************************************
 */
void ih264_inter_pred_luma_horz_qpel_vert_qpel(UWORD8 *pu1_src,
                                               UWORD8 *pu1_dst,
                                               WORD32 src_strd,
                                               WORD32 dst_strd,
                                               WORD32 ht,
                                               WORD32 wd,
                                               UWORD8* pu1_tmp,
                                               WORD32 dydx)
{
    WORD32 row, col;
    WORD32 x_offset = dydx & 0x3;
    WORD32 y_offset = dydx >> 2;

    WORD32 off1, off2, off3;
    UWORD8* pu1_pred_vert, *pu1_pred_horz;
    UNUSED(pu1_tmp);
    y_offset = y_offset & 0x3;

    off1 = src_strd;
    off2 = src_strd << 1;
    off3 = off1 + off2;

    pu1_pred_horz = pu1_src + (y_offset >> 1) * src_strd;
    pu1_pred_vert = pu1_src + (x_offset >> 1);

    for(row = 0; row < ht; row++)
    {
        for(col = 0; col < wd;
                        col++, pu1_dst++, pu1_pred_vert++, pu1_pred_horz++)
        {
            WORD16 i2_temp_vert, i2_temp_horz;
            /* The logic below implements the following equation
             i2_temp = puc_pred[-2*src_strd] + puc_pred[3*src_strd] -
             5 * (puc_pred[-1*src_strd] + puc_pred[2*src_strd])  +
             20 * (puc_pred[0] + puc_pred[src_strd]); */
            i2_temp_vert = pu1_pred_vert[-off2] + pu1_pred_vert[off3]
                            - (pu1_pred_vert[-off1] + pu1_pred_vert[off2])
                            + ((pu1_pred_vert[0] + pu1_pred_vert[off1]
                                            - pu1_pred_vert[-off1]
                                            - pu1_pred_vert[off2]) << 2)
                            + ((pu1_pred_vert[0] + pu1_pred_vert[off1]) << 4);
            i2_temp_vert = (i2_temp_vert + 16) >> 5;
            i2_temp_vert = CLIP_U8(i2_temp_vert);

            /* The logic below implements the following equation
             i16_temp = puc_pred[-2] - 5 * (puc_pred[-1] + puc_pred[2]) +
             20 * (puc_pred[0] + puc_pred[1]) + puc_pred[3]; */
            i2_temp_horz = pu1_pred_horz[-2] + pu1_pred_horz[3]
                            - (pu1_pred_horz[-1] + pu1_pred_horz[2])
                            + ((pu1_pred_horz[0] + pu1_pred_horz[1]
                                            - pu1_pred_horz[-1]
                                            - pu1_pred_horz[2]) << 2)
                            + ((pu1_pred_horz[0] + pu1_pred_horz[1]) << 4);
            i2_temp_horz = (i2_temp_horz + 16) >> 5;
            i2_temp_horz = CLIP_U8(i2_temp_horz);
            *pu1_dst = (i2_temp_vert + i2_temp_horz + 1) >> 1;
        }
        pu1_pred_vert += (src_strd - wd);
        pu1_pred_horz += (src_strd - wd);
        pu1_dst += (dst_strd - wd);
    }
}

/*!
 **************************************************************************
 * \if Function name : ih264_inter_pred_luma_horz_qpel_vert_hpel \endif
 *
 * \brief
 *    This routine applies the six tap filter to the predictors in the vertical
 *    and horizontal direction to obtain the pixel at (1/2,1/2). It then interpolates
 *    pixel at (0,1/2) and (1/2,1/2) to obtain pixel at (1/4,1/2). Similarly for (3/4,1/2).
 *    The six tap filtering operation is described in sec 8.4.2.2.1 titled
 *    "Luma sample interpolation process"
 *
 * \param pu1_src: Pointer to the buffer containing the predictor values.
 *     pu1_src could point to the frame buffer or the predictor buffer.
 * \param pu1_dst: Pointer to the destination buffer where the output of
 *     the six tap filter followed by interpolation is stored.
 * \param wd: Width of the rectangular pixel grid to be interpolated
 * \param ht: Height of the rectangular pixel grid to be interpolated
 * \param src_strd: Width of the buffer pointed to by puc_pred.
 * \param dst_strd: Width of the destination buffer
 * \param pu1_tmp: buffer to store temporary output after 1st 6-tap filter.
 * \param dydx: x and y reference offset for qpel calculations.
 *
 * \return
 *    void
 *
 * \note
 *    This function takes the 8 bit predictor values, applies the six tap
 *    filter in the vertical direction and outputs the result clipped to
 *    8 bit precision. The input is stored in the buffer pointed to by
 *    puc_pred while the output is stored in the buffer pointed by puc_dest.
 *    Both puc_pred and puc_dest could point to the same buffer i.e. the
 *    six tap filter could be done in place.
 *
 * \para <title>
 *    <paragraph>
 *  ...
 **************************************************************************
 */
void ih264_inter_pred_luma_horz_qpel_vert_hpel(UWORD8 *pu1_src,
                                               UWORD8 *pu1_dst,
                                               WORD32 src_strd,
                                               WORD32 dst_strd,
                                               WORD32 ht,
                                               WORD32 wd,
                                               UWORD8* pu1_tmp,
                                               WORD32 dydx)
{
    WORD32 row, col;
    WORD32 tmp;
    WORD16* pi2_pred1_temp, *pi2_pred1;
    UWORD8* pu1_dst_tmp;
    WORD32 x_offset = dydx & 0x3;
    WORD16 i2_macro;

    pi2_pred1_temp = (WORD16*)pu1_tmp;
    pi2_pred1_temp += 2;
    pi2_pred1 = pi2_pred1_temp;
    pu1_dst_tmp = pu1_dst;

    for(row = 0; row < ht; row++)
    {
        for(col = -2; col < wd + 3; col++)
        {
            tmp = 0;/*ih264_g_six_tap[] is the array containing the filter coeffs*/
            tmp = ih264_g_six_tap[0] *
                            (pu1_src[col - 2 * src_strd] + pu1_src[col + 3 * src_strd])
                  + ih264_g_six_tap[1] *
                            (pu1_src[col - 1 * src_strd] + pu1_src[col + 2 * src_strd])
                  + ih264_g_six_tap[2] *
                            (pu1_src[col] + pu1_src[col + 1 * src_strd]);
            pi2_pred1_temp[col] = tmp;
        }

        pu1_src += src_strd;
        pi2_pred1_temp = pi2_pred1_temp + wd + 5;
    }

    pi2_pred1_temp = pi2_pred1;
    for(row = 0; row < ht; row++)
    {
        for(col = 0; col < wd; col++)
        {
            tmp = 0;/*ih264_g_six_tap[] is the array containing the filter coeffs*/
            tmp = ih264_g_six_tap[0] *
                            (pi2_pred1[col - 2] + pi2_pred1[col + 3])
                  + ih264_g_six_tap[1] *
                            (pi2_pred1[col - 1] + pi2_pred1[col + 2])
                  + ih264_g_six_tap[2] *
                            (pi2_pred1[col] + pi2_pred1[col + 1]);
            tmp = (tmp + 512) >> 10;
            pu1_dst[col] = CLIP_U8(tmp);
        }
        pi2_pred1 += (wd + 5);
        pu1_dst += dst_strd;
    }

    pu1_dst = pu1_dst_tmp;
    pi2_pred1_temp += (x_offset >> 1);
    for(row = ht; row != 0; row--)
    {
        for(col = wd; col != 0; col--, pu1_dst++, pi2_pred1_temp++)
        {
            UWORD8 uc_temp;
            /* Clipping the output of the six tap filter obtained from the
             first stage of the 2d filter stage */
            *pi2_pred1_temp = (*pi2_pred1_temp + 16) >> 5;
            i2_macro = (*pi2_pred1_temp);
            uc_temp = CLIP_U8(i2_macro);
            *pu1_dst = (*pu1_dst + uc_temp + 1) >> 1;
        }
        pi2_pred1_temp += 5;
        pu1_dst += dst_strd - wd;
    }
}

/*!
 **************************************************************************
 * \if Function name : ih264_inter_pred_luma_horz_hpel_vert_qpel \endif
 *
 * \brief
 *    This routine applies the six tap filter to the predictors in the horizontal
 *    and vertical direction to obtain the pixel at (1/2,1/2). It then interpolates
 *    pixel at (1/2,0) and (1/2,1/2) to obtain pixel at (1/2,1/4). Similarly for (1/2,3/4).
 *    The six tap filtering operation is described in sec 8.4.2.2.1 titled
 *    "Luma sample interpolation process"
 *
 * \param pu1_src: Pointer to the buffer containing the predictor values.
 *     pu1_src could point to the frame buffer or the predictor buffer.
 * \param pu1_dst: Pointer to the destination buffer where the output of
 *     the six tap filter followed by interpolation is stored.
 * \param wd: Width of the rectangular pixel grid to be interpolated
 * \param ht: Height of the rectangular pixel grid to be interpolated
 * \param src_strd: Width of the buffer pointed to by puc_pred.
 * \param dst_strd: Width of the destination buffer
 * \param pu1_tmp: buffer to store temporary output after 1st 6-tap filter.
 * \param dydx: x and y reference offset for qpel calculations.
 *
 * \return
 *    void
 *
 * \note
 *    This function takes the 8 bit predictor values, applies the six tap
 *    filter in the vertical direction and outputs the result clipped to
 *    8 bit precision. The input is stored in the buffer pointed to by
 *    puc_pred while the output is stored in the buffer pointed by puc_dest.
 *    Both puc_pred and puc_dest could point to the same buffer i.e. the
 *    six tap filter could be done in place.
 *
 * \para <title>
 *    <paragraph>
 *  ...
 **************************************************************************
 */
void ih264_inter_pred_luma_horz_hpel_vert_qpel(UWORD8 *pu1_src,
                                               UWORD8 *pu1_dst,
                                               WORD32 src_strd,
                                               WORD32 dst_strd,
                                               WORD32 ht,
                                               WORD32 wd,
                                               UWORD8* pu1_tmp,
                                               WORD32 dydx)
{

    WORD32 row, col;
    WORD32 tmp;
    WORD32 y_offset = dydx >> 2;
    WORD16* pi2_pred1_temp, *pi2_pred1;
    UWORD8* pu1_dst_tmp;
    //WORD32 x_offset = dydx & 0x3;
    WORD16 i2_macro;

    y_offset = y_offset & 0x3;

    pi2_pred1_temp = (WORD16*)pu1_tmp;
    pi2_pred1_temp += 2 * wd;
    pi2_pred1 = pi2_pred1_temp;
    pu1_dst_tmp = pu1_dst;
    pu1_src -= 2 * src_strd;
    for(row = -2; row < ht + 3; row++)
    {
        for(col = 0; col < wd; col++)
        {
            tmp = 0;/*ih264_g_six_tap[] is the array containing the filter coeffs*/
            tmp = ih264_g_six_tap[0] * (pu1_src[col - 2] + pu1_src[col + 3])
                  + ih264_g_six_tap[1] * (pu1_src[col - 1] + pu1_src[col + 2])
                  + ih264_g_six_tap[2] * (pu1_src[col] + pu1_src[col + 1]);
            pi2_pred1_temp[col - 2 * wd] = tmp;
        }

        pu1_src += src_strd;
        pi2_pred1_temp += wd;
    }
    pi2_pred1_temp = pi2_pred1;
    for(row = 0; row < ht; row++)
    {
        for(col = 0; col < wd; col++)
        {
            tmp = 0;/*ih264_g_six_tap[] is the array containing the filter coeffs*/
            tmp = ih264_g_six_tap[0] * (pi2_pred1[col - 2 * wd] + pi2_pred1[col + 3 * wd])
                  + ih264_g_six_tap[1] * (pi2_pred1[col - 1 * wd] + pi2_pred1[col + 2 * wd])
                  + ih264_g_six_tap[2] * (pi2_pred1[col] + pi2_pred1[col + 1 * wd]);
            tmp = (tmp + 512) >> 10;
            pu1_dst[col] = CLIP_U8(tmp);
        }
        pi2_pred1 += wd;
        pu1_dst += dst_strd;
    }
    pu1_dst = pu1_dst_tmp;
    pi2_pred1_temp += (y_offset >> 1) * wd;
    for(row = ht; row != 0; row--)

    {
        for(col = wd; col != 0; col--, pu1_dst++, pi2_pred1_temp++)
        {
            UWORD8 u1_temp;
            /* Clipping the output of the six tap filter obtained from the
             first stage of the 2d filter stage */
            *pi2_pred1_temp = (*pi2_pred1_temp + 16) >> 5;
            i2_macro = (*pi2_pred1_temp);
            u1_temp = CLIP_U8(i2_macro);
            *pu1_dst = (*pu1_dst + u1_temp + 1) >> 1;
        }
        //pi16_pred1_temp += wd;
        pu1_dst += dst_strd - wd;
    }
}

/**
 *******************************************************************************
 *  function:ih264_inter_pred_luma_bilinear
 *
 * @brief
 *    This routine applies the bilinear filter to the predictors .
 *    The  filtering operation is described in
 *    sec 8.4.2.2.1 titled "Luma sample interpolation process"
 *
 * @par Description:
\note
 *     This function is called to obtain pixels lying at the following
 *    locations (1/4,1), (3/4,1),(1,1/4), (1,3/4) ,(1/4,1/2), (3/4,1/2),(1/2,1/4), (1/2,3/4),(3/4,1/4),(1/4,3/4),(3/4,3/4)&& (1/4,1/4) .
 *    The function averages the two adjacent values from the two input arrays in horizontal direction.
 *
 *
 * @param[in] pu1_src1:
 *  UWORD8 Pointer to the buffer containing the first input array.
 *
 * @param[in] pu1_src2:
 *  UWORD8 Pointer to the buffer containing the second input array.
 *
 * @param[out] pu1_dst
 *  UWORD8 pointer to the destination where the output of bilinear filter is stored.
 *
 * @param[in] src_strd1
 *  Stride of the first input buffer
 *
 * @param[in] src_strd2
 *  Stride of the second input buffer
 *
 * @param[in] dst_strd
 *  integer destination stride of pu1_dst
 *
 * @param[in] ht
 *  integer height of the array
 *
 * @param[in] wd
 *  integer width of the array
 *
 * @returns
 *
 * @remarks
 *  None
 *
 *******************************************************************************
 */
void ih264_inter_pred_luma_bilinear(UWORD8 *pu1_src1,
                                    UWORD8 *pu1_src2,
                                    UWORD8 *pu1_dst,
                                    WORD32 src_strd1,
                                    WORD32 src_strd2,
                                    WORD32 dst_strd,
                                    WORD32 ht,
                                    WORD32 wd)
{
    WORD32 row, col;
    WORD16 i2_tmp;

    for(row = 0; row < ht; row++)
    {
        for(col = 0; col < wd; col++)
        {
            i2_tmp = pu1_src1[col] + pu1_src2[col];
            i2_tmp = (i2_tmp + 1) >> 1;
            pu1_dst[col] = CLIP_U8(i2_tmp);
        }
        pu1_src1 += src_strd1;
        pu1_src2 += src_strd2;
        pu1_dst += dst_strd;
    }

}

/**
 *******************************************************************************
 *
 * @brief
 *    Interprediction chroma filter
 *
 * @par Description:
 *   Applies filtering to chroma samples as mentioned in
 *    sec 8.4.2.2.2 titled "chroma sample interpolation process"
 *
 * @param[in] pu1_src
 *  UWORD8 pointer to the source containing alternate U and V samples
 *
 * @param[out] pu1_dst
 *  UWORD8 pointer to the destination
 *
 * @param[in] src_strd
 *  integer source stride
 *
 * @param[in] dst_strd
 *  integer destination stride
 *
 * @param[in] u1_dx
 *  dx value where the sample is to be produced(refer sec 8.4.2.2.2 )
 *
 * @param[in] u1_dy
 *  dy value where the sample is to be produced(refer sec 8.4.2.2.2 )
 *
 * @param[in] ht
 *  integer height of the array
 *
 * @param[in] wd
 *  integer width of the array
 *
 * @returns
 *
 * @remarks
 *  None
 *
 *******************************************************************************
 */
void ih264_inter_pred_chroma(UWORD8 *pu1_src,
                             UWORD8 *pu1_dst,
                             WORD32 src_strd,
                             WORD32 dst_strd,
                             WORD32 dx,
                             WORD32 dy,
                             WORD32 ht,
                             WORD32 wd)
{
    WORD32 row, col;
    WORD16 i2_tmp;

    for(row = 0; row < ht; row++)
    {
        for(col = 0; col < 2 * wd; col++)
        {
            i2_tmp = 0; /* applies equation (8-266) in section 8.4.2.2.2 */
            i2_tmp = (8 - dx) * (8 - dy) * pu1_src[col]
                     + (dx) * (8 - dy) * pu1_src[col + 2]
                     + (8 - dx) * (dy) * (pu1_src + src_strd)[col]
                     + (dx) * (dy) * (pu1_src + src_strd)[col + 2];
            i2_tmp = (i2_tmp + 32) >> 6;
            pu1_dst[col] = CLIP_U8(i2_tmp);
        }
        pu1_src += src_strd;
        pu1_dst += dst_strd;
    }
}
