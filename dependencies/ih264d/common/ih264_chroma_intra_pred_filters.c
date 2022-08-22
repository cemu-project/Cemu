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
*  ih264_chroma_intra_pred_filters.c
*
* @brief
*  Contains function definitions for chroma intra prediction  filters
*
* @author
*  Ittiam
*
* @par List of Functions:
*  -ih264_intra_pred_chroma_8x8_mode_dc
*  -ih264_intra_pred_chroma_8x8_mode_horz
*  -ih264_intra_pred_chroma_8x8_mode_vert
*  -ih264_intra_pred_chroma_8x8_mode_plane
*
* @remarks
*  None
*
*******************************************************************************
*/

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

/* System include files */
#include <stdio.h>
#include <stddef.h>
#include <string.h>

/* User include files */
#include "ih264_defs.h"
#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264_intra_pred_filters.h"

/* Global variables used only in assembly files*/
const WORD8  ih264_gai1_intrapred_chroma_plane_coeffs1[] =
{ 0x01,0x00,0x01,0x00,
  0x02,0x00,0x02,0x00,
  0x03,0x00,0x03,0x00,
  0x04,0x00,0x04,0x00
};
 const WORD8  ih264_gai1_intrapred_chroma_plane_coeffs2[] =
 { 0xfd,0xff,0xfe,0xff,
   0xff,0xff,0x00,0x00,
   0x01,0x00,0x02,0x00,
   0x03,0x00,0x04,0x00,
 };

/*****************************************************************************/
/* Chroma Intra prediction 8x8 filters                                       */
/*****************************************************************************/

/**
*******************************************************************************
*
* ih264_intra_pred_chroma_8x8_mode_dc
*
* @brief
*  Perform Intra prediction for  chroma_8x8 mode:DC
*
* @par Description:
*  Perform Intra prediction for  chroma_8x8 mode:DC ,described in sec 8.3.4.1
*
* @param[in] pu1_src
*  UWORD8 pointer to the source containing alternate U and V samples
*
* @param[out] pu1_dst
*  UWORD8 pointer to the destination with alternate U and V samples
*
* @param[in] src_strd
*  integer source stride
*
* @param[in] dst_strd
*  integer destination stride
*
** @param[in] ngbr_avail
*  availability of neighbouring pixels
*
* @returns
*
* @remarks
*  None
*
******************************************************************************
*/
void ih264_intra_pred_chroma_8x8_mode_dc(UWORD8 *pu1_src,
                                         UWORD8 *pu1_dst,
                                         WORD32 src_strd,
                                         WORD32 dst_strd,
                                         WORD32 ngbr_avail)
{
    WORD32 left_avail, left_avail1, left_avail2; /* availability of left predictors (only for DC) */
    WORD32 top_avail; /* availability of top predictors (only for DC) */
    UWORD8 *pu1_left = NULL; /* Pointer to start of left predictors */
    UWORD8 *pu1_top = NULL; /* Pointer to start of top predictors */

    /* temporary variables to store accumulated first left half,second left half,
     * first top half,second top half of U and  V values*/
    WORD32 val_u_l1 = 0, val_u_l2 = 0, val_u_t1 = 0, val_u_t2 = 0;
    WORD32 val_v_l1 = 0, val_v_l2 = 0, val_v_t1 = 0, val_v_t2 = 0;

    WORD32 val_u1 = 0, val_u2 = 0, val_v1 = 0, val_v2 = 0;

    WORD32 col, row; /*loop variables*/
    UNUSED(src_strd);

    left_avail = ngbr_avail & 0x11;
    left_avail1 = ngbr_avail & 1;
    left_avail2 = (ngbr_avail >> 4) & 1;
    top_avail = (ngbr_avail >> 2) & 1;

    pu1_top = pu1_src + 2 * BLK8x8SIZE + 2;
    pu1_left = pu1_src + 2 * BLK8x8SIZE - 2;

    if(left_avail1)
    { /* First 4x4 block*/
        val_u_l1 += *pu1_left;
        val_v_l1 += *(pu1_left + 1);
        pu1_left -= 2;
        val_u_l1 += *pu1_left;
        val_v_l1 += *(pu1_left + 1);
        pu1_left -= 2;
        val_u_l1 += *pu1_left;
        val_v_l1 += *(pu1_left + 1);
        pu1_left -= 2;
        val_u_l1 += *pu1_left + 2;
        val_v_l1 += *(pu1_left + 1) + 2;
        pu1_left -= 2;
    }
    else
        pu1_left -= 2 * 4;

    if(left_avail2)
    {
        /* Second 4x4 block*/
        val_u_l2 += *pu1_left;
        val_v_l2 += *(pu1_left + 1);
        pu1_left -= 2;
        val_u_l2 += *pu1_left;
        val_v_l2 += *(pu1_left + 1);
        pu1_left -= 2;
        val_u_l2 += *pu1_left;
        val_v_l2 += *(pu1_left + 1);
        pu1_left -= 2;
        val_u_l2 += *pu1_left + 2;
        val_v_l2 += *(pu1_left + 1) + 2;
        pu1_left -= 2;
    }
    else
        pu1_left -= 2 * 4;

    if(top_avail)
    {
        val_u_t1 += *pu1_top + *(pu1_top + 2) + *(pu1_top + 4)
                        + *(pu1_top + 6) + 2;
        val_u_t2 += *(pu1_top + 8) + *(pu1_top + 10) + *(pu1_top + 12)
                        + *(pu1_top + 14) + 2;
        val_v_t1 += *(pu1_top + 1) + *(pu1_top + 3) + *(pu1_top + 5)
                        + *(pu1_top + 7) + 2;
        val_v_t2 += *(pu1_top + 9) + *(pu1_top + 11) + *(pu1_top + 13)
                        + *(pu1_top + 15) + 2;
    }

    if(left_avail + top_avail)
    {
        val_u1 = (left_avail1 + top_avail) ?
                        ((val_u_l1 + val_u_t1)
                                        >> (1 + left_avail1 + top_avail)) :128;
        val_v1 = (left_avail1 + top_avail) ?
                        ((val_v_l1 + val_v_t1)
                                        >> (1 + left_avail1 + top_avail)) :128;
        if(top_avail)
        {
            val_u2 = val_u_t2 >> 2;
            val_v2 = val_v_t2 >> 2;
        }
        else if(left_avail1)
        {
            val_u2 = val_u_l1 >> 2;
            val_v2 = val_v_l1 >> 2;
        }
        else
        {
            val_u2 = val_v2 = 128;
        }

        for(row = 0; row < 4; row++)
        {
            /*top left 4x4 block*/
            for(col = 0; col < 8; col += 2)
            {
                *(pu1_dst + row * dst_strd + col) = val_u1;
                *(pu1_dst + row * dst_strd + col + 1) = val_v1;
            }
            /*top right 4x4 block*/
            for(col = 8; col < 16; col += 2)
            {
                *(pu1_dst + row * dst_strd + col) = val_u2;
                *(pu1_dst + row * dst_strd + col + 1) = val_v2;
            }
        }

        if(left_avail2)
        {
            val_u1 = val_u_l2 >> 2;
            val_v1 = val_v_l2 >> 2;
        }
        else if(top_avail)
        {
            val_u1 = val_u_t1 >> 2;
            val_v1 = val_v_t1 >> 2;
        }
        else
        {
            val_u1 = val_v1 = 128;
        }
        val_u2 = (left_avail2 + top_avail) ?
                        ((val_u_l2 + val_u_t2)
                                        >> (1 + left_avail2 + top_avail)) : 128;
        val_v2 = (left_avail2 + top_avail) ?
                        ((val_v_l2 + val_v_t2)
                                        >> (1 + left_avail2 + top_avail)) :  128;

        for(row = 4; row < 8; row++)
        { /*bottom left 4x4 block*/
            for(col = 0; col < 8; col += 2)
            {
                *(pu1_dst + row * dst_strd + col) = val_u1;
                *(pu1_dst + row * dst_strd + col + 1) = val_v1;
            }
            /*bottom right 4x4 block*/
            for(col = 8; col < 16; col += 2)
            {
                *(pu1_dst + row * dst_strd + col) = val_u2;
                *(pu1_dst + row * dst_strd + col + 1) = val_v2;
            }
        }
    }
    else
    {
        /* Both left and top are unavailable, set the block to 128 */
        for(row = 0; row < 8; row++)
        {
            memset(pu1_dst + row * dst_strd, 128, 8 * sizeof(UWORD16));
        }
    }
}

/**
*******************************************************************************
*
*ih264_intra_pred_chroma_8x8_mode_horz
*
* @brief
*  Perform Intra prediction for  chroma_8x8 mode:Horizontal
*
* @par Description:
*  Perform Intra prediction for  chroma_8x8 mode:Horizontal ,described in sec 8.3.4.2
*
* @param[in] pu1_src
*  UWORD8 pointer to the source containing alternate U and V samples
*
* @param[out] pu1_dst
*  UWORD8 pointer to the destination with alternate U and V samples
*
* @param[in] src_strd
*  integer source stride
*
* @param[in] dst_strd
*  integer destination stride
*
* @param[in] ngbr_avail
* availability of neighbouring pixels(Not used in this function)
*
* @returns
*
* @remarks
*  None
*
******************************************************************************
*/
void ih264_intra_pred_chroma_8x8_mode_horz(UWORD8 *pu1_src,
                                           UWORD8 *pu1_dst,
                                           WORD32 src_strd,
                                           WORD32 dst_strd,
                                           WORD32 ngbr_avail)
{

    UWORD8 *pu1_left = NULL; /* Pointer to start of top predictors */
    WORD32 rows, cols; /* loop variables*/
    UNUSED(src_strd);
    UNUSED(ngbr_avail);
    pu1_left = pu1_src + 2 * BLK8x8SIZE - 2;
    for(rows = 0; rows < 8; rows++)
    {
        for(cols = 0; cols < 16; cols += 2)
        {
            *(pu1_dst + rows * dst_strd + cols) = *pu1_left;

            *(pu1_dst + rows * dst_strd + cols + 1) = *(pu1_left + 1);
        }
        pu1_left -= 2;
    }

}

/**
*******************************************************************************
*
*ih264_intra_pred_chroma_8x8_mode_vert
*
* @brief
*  Perform Intra prediction for  chroma_8x8 mode:vertical
*
* @par Description:
*  Perform Intra prediction for  chroma_8x8 mode:vertical ,described in sec 8.3.4.3
*
* @param[in] pu1_src
*  UWORD8 pointer to the source containing alternate U and V samples
*
* @param[out] pu1_dst
*  UWORD8 pointer to the destination with alternate U and V samples
*
* @param[in] src_strd
*  integer source stride
*
* @param[in] dst_strd
*  integer destination stride
*
* @param[in] ngbr_avail
* availability of neighbouring pixels(Not used in this function)
*
* @returns
*
* @remarks
*  None
*
*******************************************************************************
*/
void ih264_intra_pred_chroma_8x8_mode_vert(UWORD8 *pu1_src,
                                           UWORD8 *pu1_dst,
                                           WORD32 src_strd,
                                           WORD32 dst_strd,
                                           WORD32 ngbr_avail)
{

    UWORD8 *pu1_top = NULL; /* Pointer to start of top predictors */
    WORD32 row;/*loop variable*/
    UNUSED(src_strd);
    UNUSED(ngbr_avail);
    pu1_top = pu1_src + 2 * BLK8x8SIZE + 2;

    /* 8 bytes are copied from src to dst */
    for(row = 0; row < 2; row++)
    {
        memcpy(pu1_dst, pu1_top, 16);

        pu1_dst += dst_strd;
        memcpy(pu1_dst, pu1_top, 16);

        pu1_dst += dst_strd;
        memcpy(pu1_dst, pu1_top, 16);

        pu1_dst += dst_strd;
        memcpy(pu1_dst, pu1_top, 16);

        pu1_dst += dst_strd;
    }
}

/**
*******************************************************************************
*
* ih264_intra_pred_chroma_8x8_mode_plane
*
* @brief
*  Perform Intra prediction for  chroma_8x8 mode:PLANE
*
* @par Description:
*  Perform Intra prediction for  chroma_8x8 mode:PLANE ,described in sec 8.3.4.4
*
* @param[in] pu1_src
*  UWORD8 pointer to the source containing alternate U and V samples
*
* @param[out] pu1_dst
*  UWORD8 pointer to the destination with alternate U and V samples
*
* @param[in] src_strd
*  integer source stride
*
* @param[in] dst_strd
*  integer destination stride
*
* @param[in] ngbr_avail
* availability of neighbouring pixels(Not used in this function)
*
* @returns
*
* @remarks
*  None
*
******************************************************************************
*/
void ih264_intra_pred_chroma_8x8_mode_plane(UWORD8 *pu1_src,
                                            UWORD8 *pu1_dst,
                                            WORD32 src_strd,
                                            WORD32 dst_strd,
                                            WORD32 ngbr_avail)
{

    UWORD8 *pu1_left = NULL; /* Pointer to start of left predictors */
    UWORD8 *pu1_top = NULL; /* Pointer to start of top predictors */
    WORD32 val = 0;
    WORD32 rows, cols; /* loop variables*/
    WORD32 a_u, b_u, c_u, h_u, v_u; /* Implementing section 8.3.4.4 . The variables represent the corresponding variables in the section*/
    WORD32 a_v, b_v, c_v, h_v, v_v;
    UNUSED(src_strd);
    UNUSED(ngbr_avail);
    a_u = b_u = c_u = h_u = v_u = 0;
    a_v = b_v = c_v = h_v = v_v = 0;
    /* As chroma format 4:2:0 is used,xCF = 4 * ( chroma_format_idc = = 3 ) = 0 and
     yCF = 4 * ( chroma_format_idc != 1  ) = 0   */
    pu1_top = pu1_src + 2 * BLK8x8SIZE + 2;
    pu1_left = pu1_src + 2 * BLK8x8SIZE - 2;
    /* Implementing section 8.3.4.4 */
    for(cols = 0; cols < 4; cols++)
    {
        h_u += (cols + 1) * (pu1_top[8 + 2 * cols] - pu1_top[4 - 2 * cols]);/*section 8.3.4.4   equation (8-144)*/
        h_v += (cols + 1) * (pu1_top[8 + 2 * cols + 1] - pu1_top[4 - 2 * cols+ 1]);

        v_u += (cols + 1) * (pu1_left[(4 + cols) * (-2)] - pu1_left[(2 - cols) * (-2)]);
        v_v += (cols + 1)  * (pu1_left[(4 + cols) * (-2) + 1]  - pu1_left[(2 - cols) * (-2) + 1]);/*section 8.3.4.4   equation (8-145)*/
    }
    a_u = 16 * (pu1_left[7 * (-2)] + pu1_top[14]);
    a_v = 16 * (pu1_left[7 * (-2) + 1] + pu1_top[15]);/*section 8.3.3.4   equation (8-141)*/
    b_u = (34 * h_u + 32) >> 6;/*section 8.3.3.4   equation (8-142)*/
    b_v = (34 * h_v + 32) >> 6;/*section 8.3.3.4   equation (8-142)*/
    c_u = (34 * v_u + 32) >> 6;/*section 8.3.3.4   equation (8-143)*/
    c_v = (34 * v_v + 32) >> 6;/*section 8.3.3.4   equation (8-143)*/

    for(rows = 0; rows < 8; rows++)
    {
        for(cols = 0; cols < 8; cols++)
        {
            val = (a_u + b_u * (cols - 3) + c_u * (rows - 3) );/*section 8.3.4.4   equation (8-140)*/
            val = (val + 16) >> 5;
            *(pu1_dst + rows * dst_strd + 2 * cols) = CLIP_U8(val);
            val = (a_v + b_v * (cols - 3) + c_v * (rows - 3) );/*section 8.3.4.4   equation (8-140)*/
            val = (val + 16) >> 5;
            *(pu1_dst + rows * dst_strd + 2 * cols + 1) = CLIP_U8(val);
        }
    }
}

