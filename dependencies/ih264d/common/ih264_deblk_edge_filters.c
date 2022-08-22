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
/**************************************************************************** */
/*                                                                            */
/*  File Name         : ih264_deblk_edge_filters.c                            */
/*                                                                            */
/*  Description       : Contains function definitions for deblocking          */
/*                                                                            */
/*  List of Functions : ih264_deblk_luma_vert_bs4()                           */
/*                      ih264_deblk_luma_horz_bs4()                           */
/*                      ih264_deblk_luma_vert_bslt4()                         */
/*                      ih264_deblk_luma_horz_bslt4()                         */
/*                      ih264_deblk_luma_vert_bs4_mbaff()                     */
/*                      ih264_deblk_luma_vert_bslt4_mbaff()                   */
/*                      ih264_deblk_chroma_vert_bs4_bp()                      */
/*                      ih264_deblk_chroma_horz_bs4_bp()                      */
/*                      ih264_deblk_chroma_vert_bslt4_bp()                    */
/*                      ih264_deblk_chroma_horz_bslt4_bp()                    */
/*                      ih264_deblk_chroma_vert_bs4_mbaff_bp()                */
/*                      ih264_deblk_chroma_vert_bslt4_mbaff_bp()              */
/*                      ih264_deblk_chroma_vert_bs4()                         */
/*                      ih264_deblk_chroma_horz_bs4()                         */
/*                      ih264_deblk_chroma_vert_bslt4()                       */
/*                      ih264_deblk_chroma_horz_bslt4()                       */
/*                      ih264_deblk_chroma_vert_bs4_mbaff()                   */
/*                      ih264_deblk_chroma_vert_bslt4_mbaff()                 */
/*                                                                            */
/*  Issues / Problems : None                                                  */
/*                                                                            */
/*  Revision History  :                                                       */
/*                                                                            */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)   */
/*         28 11 2013   Ittiam          Draft                                 */
/*         29 12 2014   Kaushik         Added double-call vertical            */
/*                      Senthoor        deblocking and high profile           */
/*                                      deblocking functions                  */
/*                                                                            */
/******************************************************************************/

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

/* System include files */
#include <stdio.h>

/* User include files */
#include "ih264_typedefs.h"
#include "ih264_platform_macros.h"
#include "ih264_deblk_edge_filters.h"
#include "ih264_macros.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_deblk_luma_vert_bs4()                              */
/*                                                                           */
/*  Description   : This function performs filtering of a luma block         */
/*                  vertical edge when the boundary strength is set to 4.    */
/*                                                                           */
/*  Inputs        : pu1_src    - pointer to the src sample q0                */
/*                  src_strd   - source stride                               */
/*                  alpha      - alpha value for the boundary                */
/*                  beta       - beta value for the boundary                 */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : This operation is described in Sec. 8.7.2.4 under the    */
/*                  title "Filtering process for edges for bS equal to 4" in */
/*                  ITU T Rec H.264.                                         */
/*                                                                           */
/*  Outputs       : None                                                     */
/*                                                                           */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         28 11 2013   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/
void ih264_deblk_luma_vert_bs4(UWORD8 *pu1_src,
                               WORD32 src_strd,
                               WORD32 alpha,
                               WORD32 beta)
{
    UWORD8 p3, p2, p1, p0, q0, q1, q2, q3;
    WORD32 pos_p3, pos_p2, pos_p1, pos_p0;
    WORD32 pos_q0, pos_q1, pos_q2,pos_q3;
    UWORD8 a_p, a_q; /* threshold variables */
    WORD32 blk_strd = src_strd << 2; /* block_increment = src_strd * 4 */
    UWORD8 *pu1_src_temp;
    WORD8 i = 0, edge;

    pos_q0 = 0;
    pos_q1 = 1;
    pos_q2 = 2;
    pos_q3 = 3;
    pos_p0 = -1;
    pos_p1 = -2;
    pos_p2 = -3;
    pos_p3 = -4;

    for(edge = 0; edge < 4; edge++, pu1_src += blk_strd)
    {
        pu1_src_temp = pu1_src;
        for(i = 0; i < 4; ++i, pu1_src_temp += src_strd)
        {
            q0 = pu1_src_temp[pos_q0];
            q1 = pu1_src_temp[pos_q1];
            p0 = pu1_src_temp[pos_p0];
            p1 = pu1_src_temp[pos_p1];

            /* Filter Decision */
            if((ABS(p0 - q0) >= alpha) ||
               (ABS(q1 - q0) >= beta)  ||
               (ABS(p1 - p0) >= beta))
                continue;

            p2 = pu1_src_temp[pos_p2];
            p3 = pu1_src_temp[pos_p3];
            q2 = pu1_src_temp[pos_q2];
            q3 = pu1_src_temp[pos_q3];

            if(ABS(p0 - q0) < ((alpha >> 2) + 2))
            {
                /* Threshold Variables */
                a_p = (UWORD8)ABS(p2 - p0);
                a_q = (UWORD8)ABS(q2 - q0);

                if(a_p < beta)
                {
                    /* p0', p1', p2' */
                    pu1_src_temp[pos_p0] = ((p2 + X2(p1) + X2(p0) + X2(q0) + q1
                                    + 4) >> 3);
                    pu1_src_temp[pos_p1] = ((p2 + p1 + p0 + q0 + 2) >> 2);
                    pu1_src_temp[pos_p2] =
                                    ((X2(p3) + X3(p2) + p1 + p0 + q0
                                                    + 4) >> 3);
                }
                else
                {
                    /* p0'*/
                    pu1_src_temp[pos_p0] = ((X2(p1) + p0 + q1 + 2) >> 2);
                }

                if(a_q < beta)
                {
                    /* q0', q1', q2' */
                    pu1_src_temp[pos_q0] = (p1 + X2(p0) + X2(q0) + X2(q1) + q2
                                    + 4) >> 3;
                    pu1_src_temp[pos_q1] = (p0 + q0 + q1 + q2 + 2) >> 2;
                    pu1_src_temp[pos_q2] = (X2(q3) + X3(q2) + q1 + q0 + p0 + 4)
                                    >> 3;
                }
                else
                {
                    /* q0'*/
                    pu1_src_temp[pos_q0] = (X2(q1) + q0 + p1 + 2) >> 2;
                }
            }
            else
            {
                /* p0', q0'*/
                pu1_src_temp[pos_p0] = ((X2(p1) + p0 + q1 + 2) >> 2);
                pu1_src_temp[pos_q0] = (X2(q1) + q0 + p1 + 2) >> 2;
            }
        }
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_deblk_luma_horz_bs4()                              */
/*                                                                           */
/*  Description   : This function performs filtering of a luma block         */
/*                  horizontal edge when the boundary strength is set to 4.  */
/*                                                                           */
/*  Inputs        : pu1_src    - pointer to the src sample q0                */
/*                  src_strd   - source stride                               */
/*                  alpha      - alpha value for the boundary                */
/*                  beta       - beta value for the boundary                 */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : This operation is described in Sec. 8.7.2.4 under the    */
/*                  title "Filtering process for edges for bS equal to 4" in */
/*                  ITU T Rec H.264.                                         */
/*                                                                           */
/*  Outputs       : None                                                     */
/*                                                                           */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         28 11 2013   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/
void ih264_deblk_luma_horz_bs4(UWORD8 *pu1_src,
                               WORD32 src_strd,
                               WORD32 alpha,
                               WORD32 beta)
{
    UWORD8 p3, p2, p1, p0, q0, q1, q2, q3;
    WORD32 pos_p3, pos_p2, pos_p1, pos_p0, pos_q0, pos_q1,
                    pos_q2, pos_q3;
    UWORD8 a_p, a_q; /* threshold variables */
    UWORD8 *pu1_p3; /* pointer to the src sample p3 */
    UWORD8 *pu1_p3_temp;
    UWORD8 *pu1_src_temp;
    WORD8 i = 0, edge;

    pu1_p3 = pu1_src - (src_strd << 2);
    pos_q0 = 0;
    pos_q1 = src_strd;
    pos_q2 = X2(src_strd);
    pos_q3 = X3(src_strd);
    pos_p0 = X3(src_strd);
    pos_p1 = X2(src_strd);
    pos_p2 = src_strd;
    pos_p3 = 0;

    for(edge = 0; edge < 4; edge++, pu1_src += 4, pu1_p3 += 4)
    {
        pu1_src_temp = pu1_src;
        pu1_p3_temp = pu1_p3;
        for(i = 0; i < 4; ++i, pu1_src_temp++, pu1_p3_temp++)
        {
            q0 = pu1_src_temp[pos_q0];
            q1 = pu1_src_temp[pos_q1];
            p0 = pu1_p3_temp[pos_p0];
            p1 = pu1_p3_temp[pos_p1];

            /* Filter Decision */
            if((ABS(p0 - q0) >= alpha) ||
               (ABS(q1 - q0) >= beta) ||
               (ABS(p1 - p0) >= beta))
                continue;

            p2 = pu1_p3_temp[pos_p2];
            p3 = pu1_p3_temp[pos_p3];
            q2 = pu1_src_temp[pos_q2];
            q3 = pu1_src_temp[pos_q3];

            if(ABS(p0 - q0) < ((alpha >> 2) + 2))
            {
                /* Threshold Variables */
                a_p = ABS(p2 - p0);
                a_q = ABS(q2 - q0);

                if((a_p < beta))
                {
                    /* p0', p1', p2' */
                    pu1_p3_temp[pos_p0] = (p2 + X2(p1) + X2(p0) + X2(q0) + q1
                                    + 4) >> 3;
                    pu1_p3_temp[pos_p1] = (p2 + p1 + p0 + q0 + 2) >> 2;
                    pu1_p3_temp[pos_p2] =
                                    (X2(p3) + X3(p2) + p1 + p0 + q0
                                                    + 4) >> 3;
                }
                else
                {
                    /* p0'*/
                    pu1_p3_temp[pos_p0] = (X2(p1) + p0 + q1 + 2) >> 2;
                }

                if(a_q < beta)
                {
                    /* q0', q1', q2' */
                    pu1_src_temp[pos_q0] = (p1 + X2(p0) + X2(q0) + X2(q1)
                                    + q2 + 4) >> 3;
                    pu1_src_temp[pos_q1] = (p0 + q0 + q1 + q2 + 2) >> 2;
                    pu1_src_temp[pos_q2] = (X2(q3) + X3(q2) + q1 + q0 + p0
                                    + 4) >> 3;
                }
                else
                {
                    /* q0'*/
                    pu1_src_temp[pos_q0] = (X2(q1) + q0 + p1 + 2) >> 2;
                }
            }
            else
            {
                /* p0', q0'*/
                pu1_p3_temp[pos_p0] = (X2(p1) + p0 + q1 + 2) >> 2;
                pu1_src_temp[pos_q0] = (X2(q1) + q0 + p1 + 2) >> 2;
            }
        }
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_deblk_chroma_vert_bs4_bp()                         */
/*                                                                           */
/*  Description   : This function performs filtering of a chroma block       */
/*                  vertical edge when the boundary strength is set to 4.    */
/*                                                                           */
/*  Inputs        : pu1_src    - pointer to the src sample q0 of U           */
/*                  src_strd   - source stride                               */
/*                  alpha      - alpha value for the boundary                */
/*                  beta       - beta value for the boundary                 */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : This operation is described in Sec. 8.7.2.4 under the    */
/*                  title "Filtering process for edges for bS equal to 4" in */
/*                  ITU T Rec H.264.                                         */
/*                                                                           */
/*  Outputs       : None                                                     */
/*                                                                           */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         28 11 2013   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/
void ih264_deblk_chroma_vert_bs4_bp(UWORD8 *pu1_src,
                                    WORD32 src_strd,
                                    WORD32 alpha,
                                    WORD32 beta)
{
    UWORD8 *pu1_src_u = pu1_src; /* pointer to the src sample q0 of U */
    UWORD8 *pu1_src_v = pu1_src + 1; /* pointer to the src sample q0 of V */
    UWORD8 p1_u, p0_u, q0_u, q1_u, p1_v, p0_v, q0_v, q1_v;
    WORD32 blk_strd = src_strd << 1; /* block_increment = src_strd * 2 */
    WORD32 pos_p1, pos_p0, pos_q0, pos_q1;
    UWORD8 *pu1_src_temp_u, *pu1_src_temp_v;
    WORD8 i = 0, edge;

    pos_q0 = 0;
    pos_q1 = 2;
    pos_p0 = -2;
    pos_p1 = -4;

    for(edge = 0; edge < 4;
                    edge++, pu1_src_u += blk_strd, pu1_src_v += blk_strd)
    {
        pu1_src_temp_u = pu1_src_u;
        pu1_src_temp_v = pu1_src_v;
        for(i = 0; i < 2; ++i, pu1_src_temp_u += src_strd, pu1_src_temp_v +=
                        src_strd)
        {
            q0_u = pu1_src_temp_u[pos_q0];
            q1_u = pu1_src_temp_u[pos_q1];
            p0_u = pu1_src_temp_u[pos_p0];
            p1_u = pu1_src_temp_u[pos_p1];
            q0_v = pu1_src_temp_v[pos_q0];
            q1_v = pu1_src_temp_v[pos_q1];
            p0_v = pu1_src_temp_v[pos_p0];
            p1_v = pu1_src_temp_v[pos_p1];

            /* Filter Decision */
            if((ABS(p0_u - q0_u) < alpha) &&
               (ABS(q1_u - q0_u) < beta) &&
               (ABS(p1_u - p0_u) < beta))
            {
                /* p0' */
                pu1_src_temp_u[pos_p0] = ((X2(p1_u) + p0_u + q1_u + 2) >> 2);
                /* q0' */
                pu1_src_temp_u[pos_q0] = (X2(q1_u) + q0_u + p1_u + 2) >> 2;
            }

            /* Filter Decision */
            if((ABS(p0_v - q0_v) < alpha) &&
               (ABS(q1_v - q0_v) < beta) &&
               (ABS(p1_v - p0_v) < beta))
            {
                /* p0' */
                pu1_src_temp_v[pos_p0] = ((X2(p1_v) + p0_v + q1_v + 2) >> 2);
                /* q0' */
                pu1_src_temp_v[pos_q0] = (X2(q1_v) + q0_v + p1_v + 2) >> 2;
            }
        }
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_deblk_chroma_horz_bs4_bp()                         */
/*                                                                           */
/*  Description   : This function performs filtering of a chroma block       */
/*                  horizontal edge when the boundary strength is set to 4.  */
/*                                                                           */
/*  Inputs        : pu1_src    - pointer to the src sample q0 of U           */
/*                  src_strd   - source stride                               */
/*                  alpha      - alpha value for the boundary                */
/*                  beta       - beta value for the boundary                 */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : This operation is described in Sec. 8.7.2.4 under the    */
/*                  title "Filtering process for edges for bS equal to 4" in */
/*                  ITU T Rec H.264.                                         */
/*                                                                           */
/*  Outputs       : None                                                     */
/*                                                                           */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         28 11 2013   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/
void ih264_deblk_chroma_horz_bs4_bp(UWORD8 *pu1_src,
                                    WORD32 src_strd,
                                    WORD32 alpha,
                                    WORD32 beta)
{
    UWORD8 *pu1_src_u = pu1_src; /* pointer to the src sample q0 of U */
    UWORD8 *pu1_src_v = pu1_src + 1; /* pointer to the src sample q0 of V */
    UWORD8 p1_u, p0_u, q0_u, q1_u, p1_v, p0_v, q0_v, q1_v;
    WORD32 pos_p1, pos_p0, pos_q0, pos_q1;
    UWORD8 *pu1_src_temp_u, *pu1_src_temp_v;
    UWORD8 *pu1_p1_u; /* pointer to the src sample p1 of U */
    UWORD8 *pu1_p1_v; /* pointer to the src sample p1 of U */
    UWORD8 *pu1_p1_temp_u, *pu1_p1_temp_v;
    WORD8 i = 0, edge;

    pu1_p1_u = pu1_src_u - (src_strd << 1);
    pu1_p1_v = pu1_src_v - (src_strd << 1);
    pos_q0 = 0;
    pos_q1 = src_strd;
    pos_p0 = src_strd;
    pos_p1 = 0;

    for(edge = 0; edge < 4; edge++, pu1_src_u += 4, pu1_p1_u += 4,
                    pu1_src_v += 4, pu1_p1_v += 4)
    {
        pu1_src_temp_u = pu1_src_u;
        pu1_p1_temp_u = pu1_p1_u;
        pu1_src_temp_v = pu1_src_v;
        pu1_p1_temp_v = pu1_p1_v;
        for(i = 0; i < 2; ++i, pu1_src_temp_u += 2, pu1_p1_temp_u += 2,
                    pu1_src_temp_v += 2, pu1_p1_temp_v += 2)
        {
            q0_u = pu1_src_temp_u[pos_q0];
            q1_u = pu1_src_temp_u[pos_q1];
            p0_u = pu1_p1_temp_u[pos_p0];
            p1_u = pu1_p1_temp_u[pos_p1];

            q0_v = pu1_src_temp_v[pos_q0];
            q1_v = pu1_src_temp_v[pos_q1];
            p0_v = pu1_p1_temp_v[pos_p0];
            p1_v = pu1_p1_temp_v[pos_p1];

            /* Filter Decision */
            if((ABS(p0_u - q0_u) < alpha) &&
               (ABS(q1_u - q0_u) < beta) &&
               (ABS(p1_u - p0_u) < beta))
            {
                /* p0' */
                pu1_p1_temp_u[pos_p0] = (X2(p1_u) + p0_u + q1_u + 2) >> 2;
                /* q0' */
                pu1_src_temp_u[pos_q0] = (X2(q1_u) + q0_u + p1_u + 2) >> 2;
            }

            /* Filter Decision */
            if((ABS(p0_v - q0_v) < alpha) &&
               (ABS(q1_v - q0_v) < beta) &&
               (ABS(p1_v - p0_v) < beta))
            {
                /* p0' */
                pu1_p1_temp_v[pos_p0] = (X2(p1_v) + p0_v + q1_v + 2) >> 2;
                /* q0' */
                pu1_src_temp_v[pos_q0] = (X2(q1_v) + q0_v + p1_v + 2) >> 2;
            }
        }
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_deblk_luma_vert_bslt4()                            */
/*                                                                           */
/*  Description   : This function performs filtering of a luma block         */
/*                  vertical edge when the boundary strength is less than 4. */
/*                                                                           */
/*  Inputs        : pu1_src       - pointer to the src sample q0             */
/*                  src_strd      - source stride                            */
/*                  alpha         - alpha value for the boundary             */
/*                  beta          - beta value for the boundary              */
/*                  u4_bs         - packed Boundary strength array           */
/*                  pu1_cliptab   - tc0_table                                */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : This operation is described in Sec. 8.7.2.3 under the    */
/*                  title "Filtering process for edges for bS less than 4"   */
/*                  in ITU T Rec H.264.                                      */
/*                                                                           */
/*  Outputs       : None                                                     */
/*                                                                           */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         28 11 2013   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/
void ih264_deblk_luma_vert_bslt4(UWORD8 *pu1_src,
                                 WORD32 src_strd,
                                 WORD32 alpha,
                                 WORD32 beta,
                                 UWORD32 u4_bs,
                                 const UWORD8 *pu1_cliptab)
{
    WORD8 i = 0, edge;
    UWORD8 p2, p1, p0, q0, q1, q2;
    WORD32 pos_p2, pos_p1, pos_p0, pos_q0, pos_q1, pos_q2;
    UWORD8 a_p, a_q; /* threshold variables */
    WORD32 blk_strd = src_strd << 2; /* block_increment = src_strd * 4 */
    UWORD8 *pu1_src_temp;
    WORD8 delta;
    WORD8 tc;
    WORD16 val;
    UWORD8 tc0, u1_bs;

    pos_q0 = 0;
    pos_q1 = 1;
    pos_q2 = 2;
    pos_p0 = -1;
    pos_p1 = -2;
    pos_p2 = -3;

    for(edge = 0; edge < 4; edge++, pu1_src += blk_strd)
    {
        pu1_src_temp = pu1_src;
        /* Filter Decision */
        u1_bs = (UWORD8)((u4_bs >> ((3 - edge) << 3)) & 0x0ff);
        if(!u1_bs)
            continue;
        /* tc0 */
        tc0 = pu1_cliptab[u1_bs];
        for(i = 0; i < 4; ++i, pu1_src_temp += src_strd)
        {
            q0 = pu1_src_temp[pos_q0];
            q1 = pu1_src_temp[pos_q1];
            p0 = pu1_src_temp[pos_p0];
            p1 = pu1_src_temp[pos_p1];

            /* Filter Decision */
            if((ABS(p0 - q0) >= alpha) ||
               (ABS(q1 - q0) >= beta) ||
               (ABS(p1 - p0) >= beta))
                continue;

            q2 = pu1_src_temp[pos_q2];
            p2 = pu1_src_temp[pos_p2];

            a_p = ABS(p2 - p0);
            a_q = ABS(q2 - q0);

            /* tc */
            tc = tc0 + (a_p < beta) + (a_q < beta);

            val = ((((q0 - p0) << 2) + (p1 - q1) + 4) >> 3);
            delta = CLIP3(-tc, tc, val);

            /* p0' */
            val = p0 + delta;
            pu1_src_temp[pos_p0] = CLIP_U8(val);
            /* q0' */
            val = q0 - delta;
            pu1_src_temp[pos_q0] = CLIP_U8(val);

            /* Luma only */
            if(a_p < beta)
            {
                /* p1' */
                val = ((p2 + ((p0 + q0 + 1) >> 1) - (p1 << 1)) >> 1);
                pu1_src_temp[pos_p1] += CLIP3(-tc0, tc0, val);
            }

            if(a_q < beta)
            {
                /* q1' */
                val = ((q2 + ((p0 + q0 + 1) >> 1) - (q1 << 1)) >> 1);
                pu1_src_temp[pos_q1] += CLIP3(-tc0, tc0, val);
            }
        }
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_deblk_chroma_vert_bslt4_bp()                       */
/*                                                                           */
/*  Description   : This function performs filtering of a chroma block       */
/*                  vertical edge when the boundary strength is less than 4. */
/*                                                                           */
/*  Inputs        : pu1_src       - pointer to the src sample q0 of U        */
/*                  src_strd      - source stride                            */
/*                  alpha         - alpha value for the boundary             */
/*                  beta          - beta value for the boundary              */
/*                  u4_bs         - packed Boundary strength array           */
/*                  pu1_cliptab   - tc0_table                                */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : This operation is described in Sec. 8.7.2.3 under the    */
/*                  title "Filtering process for edges for bS less than 4"   */
/*                  in ITU T Rec H.264.                                      */
/*                                                                           */
/*  Outputs       : None                                                     */
/*                                                                           */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         28 11 2013   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/
void ih264_deblk_chroma_vert_bslt4_bp(UWORD8 *pu1_src,
                                      WORD32 src_strd,
                                      WORD32 alpha,
                                      WORD32 beta,
                                      UWORD32 u4_bs,
                                      const UWORD8 *pu1_cliptab)
{
    UWORD8 *pu1_src_u = pu1_src; /* Pointer to the src sample q0 of plane U*/
    UWORD8 *pu1_src_v = pu1_src + 1; /* Pointer to the src sample q0 of plane V*/
    UWORD8 p1_u, p0_u, q0_u, q1_u, p1_v, p0_v, q0_v, q1_v;
    WORD32 blk_strd = src_strd << 1; /* block_increment = src_strd * (4 >> 1)*/
    WORD32 pos_p1, pos_p0, pos_q0, pos_q1;
    UWORD8 *pu1_src_temp_u, *pu1_src_temp_v;
    WORD8 i = 0, edge;
    WORD8 delta;
    WORD8 tc;
    WORD16 val;
    UWORD8 tc0, u1_bs;

    pos_q0 = 0;
    pos_q1 = 2;
    pos_p0 = -2;
    pos_p1 = -4;

    for(edge = 0; edge < 4;
                    edge++, pu1_src_u += blk_strd, pu1_src_v += blk_strd)
    {
        pu1_src_temp_u = pu1_src_u;
        pu1_src_temp_v = pu1_src_v;
        /* Filter Decision */
        u1_bs = (UWORD8)((u4_bs >> ((3 - edge) << 3)) & 0x0ff);
        if(!u1_bs)
            continue;
        /* tc0 */
        tc0 = pu1_cliptab[u1_bs];
        tc = tc0 + 1;
        for(i = 0; i < 2; ++i, pu1_src_temp_u += src_strd, pu1_src_temp_v +=
                        src_strd)
        {
            q0_u = pu1_src_temp_u[pos_q0];
            q1_u = pu1_src_temp_u[pos_q1];
            p0_u = pu1_src_temp_u[pos_p0];
            p1_u = pu1_src_temp_u[pos_p1];

            q0_v = pu1_src_temp_v[pos_q0];
            q1_v = pu1_src_temp_v[pos_q1];
            p0_v = pu1_src_temp_v[pos_p0];
            p1_v = pu1_src_temp_v[pos_p1];

            /* Filter Decision */
            if((ABS(p0_u - q0_u) < alpha) &&
               (ABS(q1_u - q0_u) < beta) &&
               (ABS(p1_u - p0_u) < beta))
            {
                val = ((((q0_u - p0_u) << 2) + (p1_u - q1_u) + 4) >> 3);
                delta = CLIP3(-tc, tc, val);
                /* p0' */
                val = p0_u + delta;
                pu1_src_temp_u[pos_p0] = CLIP_U8(val);
                /* q0' */
                val = q0_u - delta;
                pu1_src_temp_u[pos_q0] = CLIP_U8(val);
            }

            /* Filter Decision */
            if((ABS(p0_v - q0_v) < alpha) &&
               (ABS(q1_v - q0_v) < beta) &&
               (ABS(p1_v - p0_v) < beta))
            {
                val = ((((q0_v - p0_v) << 2) + (p1_v - q1_v) + 4) >> 3);
                delta = CLIP3(-tc, tc, val);
                /* p0' */
                val = p0_v + delta;
                pu1_src_temp_v[pos_p0] = CLIP_U8(val);
                /* q0' */
                val = q0_v - delta;
                pu1_src_temp_v[pos_q0] = CLIP_U8(val);
            }
        }
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_deblk_luma_horz_bslt4()                            */
/*                                                                           */
/*  Description   : This function performs filtering of a luma block         */
/*                  horizontal edge when boundary strength is less than 4.   */
/*                                                                           */
/*  Inputs        : pu1_src       - pointer to the src sample q0             */
/*                  src_strd      - source stride                            */
/*                  alpha         - alpha value for the boundary             */
/*                  beta          - beta value for the boundary              */
/*                  u4_bs         - packed Boundary strength array           */
/*                  pu1_cliptab   - tc0_table                                */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : This operation is described in Sec. 8.7.2.3 under the    */
/*                  title "Filtering process for edges for bS less than 4"   */
/*                  in ITU T Rec H.264.                                      */
/*                                                                           */
/*  Outputs       : None                                                     */
/*                                                                           */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         28 11 2013   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/
void ih264_deblk_luma_horz_bslt4(UWORD8 *pu1_src,
                                 WORD32 src_strd,
                                 WORD32 alpha,
                                 WORD32 beta,
                                 UWORD32 u4_bs,
                                 const UWORD8 *pu1_cliptab)
{
    UWORD8 p2, p1, p0, q0, q1, q2;
    WORD32 pos_p2, pos_p1, pos_p0, pos_q0, pos_q1, pos_q2;
    UWORD8 a_p, a_q; /* Threshold variables */
    UWORD8 *pu1_p2; /* Pointer to the src sample p2 */
    UWORD8 *pu1_p2_temp;
    UWORD8 *pu1_src_temp;
    WORD8 i = 0, edge;
    WORD8 delta;
    WORD8 tc;
    WORD16 val;
    UWORD8 tc0, u1_bs;

    pu1_p2 = pu1_src - (src_strd << 2);
    pos_q0 = 0;
    pos_q1 = src_strd;
    pos_q2 = X2(src_strd);
    pos_p0 = X3(src_strd);
    pos_p1 = X2(src_strd);
    pos_p2 = src_strd;

    for(edge = 0; edge < 4; edge++, pu1_src += 4, pu1_p2 += 4)
    {
        pu1_src_temp = pu1_src;
        pu1_p2_temp = pu1_p2;

        /* Filter Decision */
        u1_bs = (UWORD8)((u4_bs >> ((3 - edge) << 3)) & 0x0ff);
        if(!u1_bs)
            continue;
        /* tc0 */
        tc0 = pu1_cliptab[u1_bs];

        for(i = 0; i < 4; ++i, pu1_src_temp++, pu1_p2_temp++)
        {
            q0 = pu1_src_temp[pos_q0];
            q1 = pu1_src_temp[pos_q1];
            p0 = pu1_p2_temp[pos_p0];
            p1 = pu1_p2_temp[pos_p1];

            /* Filter Decision */
            if((ABS(p0 - q0) >= alpha) ||
               (ABS(q1 - q0) >= beta) ||
               (ABS(p1 - p0) >= beta))
                continue;

            q2 = pu1_src_temp[pos_q2];
            p2 = pu1_p2_temp[pos_p2];

            a_p = ABS(p2 - p0);
            a_q = ABS(q2 - q0);

            /* tc */
            tc = tc0 + (a_p < beta) + (a_q < beta);
            val = ((((q0 - p0) << 2) + (p1 - q1) + 4) >> 3);
            delta = CLIP3(-tc, tc, val);
            /* p0' */
            val = p0 + delta;
            pu1_p2_temp[pos_p0] = CLIP_U8(val);
            /* q0' */
            val = q0 - delta;
            pu1_src_temp[pos_q0] = CLIP_U8(val);

            /* Luma */
            if(a_p < beta)
            {
                /* p1' */
                val = ((p2 + ((p0 + q0 + 1) >> 1) - (p1 << 1)) >> 1);
                pu1_p2_temp[pos_p1] += CLIP3(-tc0, tc0, val);
            }

            if(a_q < beta)
            {
                /* q1' */
                val = ((q2 + ((p0 + q0 + 1) >> 1) - (q1 << 1)) >> 1);
                pu1_src_temp[pos_q1] += CLIP3(-tc0, tc0, val);
            }
        }
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_deblk_chroma_horz_bslt4_bp()                       */
/*                                                                           */
/*  Description   : This function performs filtering of a chroma block       */
/*                  horizontal edge when boundary strength is less than 4.   */
/*                                                                           */
/*  Inputs        : pu1_src       - pointer to the src sample q0 of U        */
/*                  src_strd      - source stride                            */
/*                  alpha         - alpha value for the boundary             */
/*                  beta          - beta value for the boundary              */
/*                  u4_bs         - packed Boundary strength array           */
/*                  pu1_cliptab   - tc0_table                                */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : This operation is described in Sec. 8.7.2.3 under the    */
/*                  title "Filtering process for edges for bS less than 4"   */
/*                  in ITU T Rec H.264.                                      */
/*                                                                           */
/*  Outputs       : None                                                     */
/*                                                                           */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         28 11 2013   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/
void ih264_deblk_chroma_horz_bslt4_bp(UWORD8 *pu1_src,
                                      WORD32 src_strd,
                                      WORD32 alpha,
                                      WORD32 beta,
                                      UWORD32 u4_bs,
                                      const UWORD8 *pu1_cliptab)
{
    UWORD8 *pu1_src_u = pu1_src; /* Pointer to the src sample q0 of plane U*/
    UWORD8 *pu1_src_v = pu1_src + 1; /* Pointer to the src sample q0 of plane V*/
    UWORD8 p1_u, p0_u, q0_u, q1_u, p1_v, p0_v, q0_v, q1_v;
    WORD32 pos_p1, pos_p0, pos_q0, pos_q1;
    UWORD8 *pu1_src_temp_u, *pu1_src_temp_v;
    UWORD8 *pu1_p1_u; /* Pointer to the src sample p1 of plane U*/
    UWORD8 *pu1_p1_v; /* Pointer to the src sample p1 of plane V*/
    UWORD8 *pu1_p1_temp_u, *pu1_p1_temp_v;
    WORD8 i = 0, edge;
    WORD8 delta;
    WORD8 tc;
    WORD16 val;
    UWORD8 u1_bs;
    UWORD8 tc0;

    pu1_p1_u = pu1_src_u - (src_strd << 1);
    pu1_p1_v = pu1_src_v - (src_strd << 1);
    pos_q0 = 0;
    pos_q1 = src_strd;
    pos_p0 = src_strd;
    pos_p1 = 0;

    for(edge = 0; edge < 4; edge++, pu1_src_u += 4, pu1_p1_u += 4,
                    pu1_src_v += 4, pu1_p1_v += 4)
    {
        pu1_src_temp_u = pu1_src_u;
        pu1_p1_temp_u = pu1_p1_u;
        pu1_src_temp_v = pu1_src_v;
        pu1_p1_temp_v = pu1_p1_v;

        /* Filter Decision */
        u1_bs = (UWORD8)((u4_bs >> ((3 - edge) << 3)) & 0x0ff);
        if(!u1_bs)
            continue;
        /* tc0 */
        tc0 = pu1_cliptab[u1_bs];

        for(i = 0; i < 2; ++i, pu1_src_temp_u += 2, pu1_p1_temp_u += 2,
                       pu1_src_temp_v += 2, pu1_p1_temp_v += 2)
        {
            q0_u = pu1_src_temp_u[pos_q0];
            q1_u = pu1_src_temp_u[pos_q1];
            p0_u = pu1_p1_temp_u[pos_p0];
            p1_u = pu1_p1_temp_u[pos_p1];

            q0_v = pu1_src_temp_v[pos_q0];
            q1_v = pu1_src_temp_v[pos_q1];
            p0_v = pu1_p1_temp_v[pos_p0];
            p1_v = pu1_p1_temp_v[pos_p1];

            /* tc */
            tc = tc0 + 1;
            /* Filter Decision */
            if(ABS(p0_u - q0_u) < alpha && ABS(q1_u - q0_u) < beta
                            && ABS(p1_u - p0_u) < beta)
            {
                val = ((((q0_u - p0_u) << 2) + (p1_u - q1_u) + 4) >> 3);
                delta = CLIP3(-tc, tc, val);
                /* p0' */
                val = p0_u + delta;
                pu1_p1_temp_u[pos_p0] = CLIP_U8(val);
                /* q0' */
                val = q0_u - delta;
                pu1_src_temp_u[pos_q0] = CLIP_U8(val);
            }
            /* Filter Decision */
            if(ABS(p0_v - q0_v) < alpha && ABS(q1_v - q0_v) < beta
                            && ABS(p1_v - p0_v) < beta)
            {
                val = ((((q0_v - p0_v) << 2) + (p1_v - q1_v) + 4) >> 3);
                delta = CLIP3(-tc, tc, val);
                /* p0' */
                val = p0_v + delta;
                pu1_p1_temp_v[pos_p0] = CLIP_U8(val);
                /* q0' */
                val = q0_v - delta;
                pu1_src_temp_v[pos_q0] = CLIP_U8(val);
            }
        }
    }
}

/*****************************************************************************/
/* Function Definitions for vertical edge deblocking for double-call         */
/*****************************************************************************/

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_deblk_luma_vert_bs4_mbaff()                        */
/*                                                                           */
/*  Description   : This function performs filtering of a luma block         */
/*                  vertical edge when boundary strength is set to 4.        */
/*                                                                           */
/*  Inputs        : pu1_src       - pointer to the src sample q0             */
/*                  src_strd      - source stride                            */
/*                  alpha         - alpha value for the boundary             */
/*                  beta          - beta value for the boundary              */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : When the function is called twice, this operation is as  */
/*                  described in Sec. 8.7.2.3 under the title "Filtering     */
/*                  process for edges for bS equal to 4" in ITU T Rec H.264. */
/*                                                                           */
/*  Outputs       : None                                                     */
/*                                                                           */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         29 12 2014   Kaushik         Draft                                */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_deblk_luma_vert_bs4_mbaff(UWORD8 *pu1_src,
                                     WORD32 src_strd,
                                     WORD32 alpha,
                                     WORD32 beta)
{
    UWORD8 p3, p2, p1, p0, q0, q1, q2, q3;
    WORD32 pos_p3, pos_p2, pos_p1, pos_p0;
    WORD32 pos_q0, pos_q1, pos_q2, pos_q3;
    UWORD8 a_p, a_q; /* threshold variables */
    WORD32 blk_strd = src_strd << 1; /* block_increment = src_strd * 2 */
    UWORD8 *pu1_src_temp;
    WORD8 i = 0, edge;

    pos_q0 = 0;
    pos_q1 = 1;
    pos_q2 = 2;
    pos_q3 = 3;
    pos_p0 = -1;
    pos_p1 = -2;
    pos_p2 = -3;
    pos_p3 = -4;

    for(edge = 0; edge < 4; edge++, pu1_src += blk_strd)
    {
        pu1_src_temp = pu1_src;
        for(i = 0; i < 2; ++i, pu1_src_temp += src_strd)
        {
            q0 = pu1_src_temp[pos_q0];
            q1 = pu1_src_temp[pos_q1];
            p0 = pu1_src_temp[pos_p0];
            p1 = pu1_src_temp[pos_p1];

            /* Filter Decision */
            if((ABS(p0 - q0) >= alpha) ||
               (ABS(q1 - q0) >= beta) ||
               (ABS(p1 - p0) >= beta))
                continue;

            p2 = pu1_src_temp[pos_p2];
            p3 = pu1_src_temp[pos_p3];
            q2 = pu1_src_temp[pos_q2];
            q3 = pu1_src_temp[pos_q3];

            if(ABS(p0 - q0) < ((alpha >> 2) + 2))
            {
                /* Threshold Variables */
                a_p = (UWORD8)ABS(p2 - p0);
                a_q = (UWORD8)ABS(q2 - q0);

                if(a_p < beta)
                {
                    /* p0', p1', p2' */
                    pu1_src_temp[pos_p0] = ((p2 + X2(p1) + X2(p0) + X2(q0) + q1
                                    + 4) >> 3);
                    pu1_src_temp[pos_p1] = ((p2 + p1 + p0 + q0 + 2) >> 2);
                    pu1_src_temp[pos_p2] =
                                    ((X2(p3) + X3(p2) + p1 + p0 + q0
                                                    + 4) >> 3);
                }
                else
                {
                    /* p0'*/
                    pu1_src_temp[pos_p0] = ((X2(p1) + p0 + q1 + 2) >> 2);
                }

                if(a_q < beta)
                {
                    /* q0', q1', q2' */
                    pu1_src_temp[pos_q0] = (p1 + X2(p0) + X2(q0) + X2(q1) + q2
                                    + 4) >> 3;
                    pu1_src_temp[pos_q1] = (p0 + q0 + q1 + q2 + 2) >> 2;
                    pu1_src_temp[pos_q2] = (X2(q3) + X3(q2) + q1 + q0 + p0 + 4)
                                    >> 3;
                }
                else
                {
                    /* q0'*/
                    pu1_src_temp[pos_q0] = (X2(q1) + q0 + p1 + 2) >> 2;
                }
            }
            else
            {
                /* p0', q0'*/
                pu1_src_temp[pos_p0] = ((X2(p1) + p0 + q1 + 2) >> 2);
                pu1_src_temp[pos_q0] = (X2(q1) + q0 + p1 + 2) >> 2;
            }
        }
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_deblk_chroma_vert_bs4_mbaff_bp()                   */
/*                                                                           */
/*  Description   : This function performs filtering of a chroma block       */
/*                  vertical edge when boundary strength is set to 4.        */
/*                                                                           */
/*  Inputs        : pu1_src       - pointer to the src sample q0 of U        */
/*                  src_strd      - source stride                            */
/*                  alpha         - alpha value for the boundary             */
/*                  beta          - beta value for the boundary              */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : When the function is called twice, this operation is as  */
/*                  described in Sec. 8.7.2.3 under the title "Filtering     */
/*                  process for edges for bS equal to 4" in ITU T Rec H.264. */
/*                                                                           */
/*  Outputs       : None                                                     */
/*                                                                           */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         29 12 2014   Kaushik         Draft                                */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_deblk_chroma_vert_bs4_mbaff_bp(UWORD8 *pu1_src,
                                          WORD32 src_strd,
                                          WORD32 alpha,
                                          WORD32 beta)
{
    UWORD8 *pu1_src_u = pu1_src; /* Pointer to the src sample q0 of U */
    UWORD8 *pu1_src_v = pu1_src + 1; /* Pointer to the src sample q0 of V */
    UWORD8 p1_u, p0_u, q0_u, q1_u, p1_v, p0_v, q0_v, q1_v;
    WORD32 blk_strd = src_strd;
    WORD32 pos_p1, pos_p0, pos_q0, pos_q1;
    UWORD8 *pu1_src_temp_u, *pu1_src_temp_v;
    WORD8 edge;

    pos_q0 = 0;
    pos_q1 = 2;
    pos_p0 = -2;
    pos_p1 = -4;

    for(edge = 0; edge < 4;
                    edge++, pu1_src_u += blk_strd, pu1_src_v += blk_strd)
    {
        pu1_src_temp_u = pu1_src_u;
        pu1_src_temp_v = pu1_src_v;

        q0_u = pu1_src_temp_u[pos_q0];
        q1_u = pu1_src_temp_u[pos_q1];
        p0_u = pu1_src_temp_u[pos_p0];
        p1_u = pu1_src_temp_u[pos_p1];
        q0_v = pu1_src_temp_v[pos_q0];
        q1_v = pu1_src_temp_v[pos_q1];
        p0_v = pu1_src_temp_v[pos_p0];
        p1_v = pu1_src_temp_v[pos_p1];

        /* Filter Decision */
        if((ABS(p0_u - q0_u) < alpha) &&
           (ABS(q1_u - q0_u) < beta) &&
           (ABS(p1_u - p0_u) < beta))
        {
            /* p0' */
            pu1_src_temp_u[pos_p0] = ((X2(p1_u) + p0_u + q1_u + 2) >> 2);
            /* q0' */
            pu1_src_temp_u[pos_q0] = (X2(q1_u) + q0_u + p1_u + 2) >> 2;
        }

        /* Filter Decision */
        if(ABS(p0_v - q0_v) < alpha && ABS(q1_v - q0_v) < beta
                        && ABS(p1_v - p0_v) < beta)
        {
            /* p0' */
            pu1_src_temp_v[pos_p0] = ((X2(p1_v) + p0_v + q1_v + 2) >> 2);
            /* q0' */
            pu1_src_temp_v[pos_q0] = (X2(q1_v) + q0_v + p1_v + 2) >> 2;
        }
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_deblk_luma_vert_bslt4_mbaff()                      */
/*                                                                           */
/*  Description   : This function performs filtering of a luma block         */
/*                  vertical edge when boundary strength is less than 4.     */
/*                                                                           */
/*  Inputs        : pu1_src       - pointer to the src sample q0             */
/*                  src_strd      - source stride                            */
/*                  alpha         - alpha value for the boundary             */
/*                  beta          - beta value for the boundary              */
/*                  u4_bs         - packed Boundary strength array           */
/*                  pu1_cliptab   - tc0_table                                */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : When the function is called twice, this operation is as  */
/*                  described in Sec. 8.7.2.3 under the title "Filtering     */
/*                  process for edges for bS less than 4" in ITU T Rec H.264.*/
/*                                                                           */
/*  Outputs       : None                                                     */
/*                                                                           */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         29 12 2014   Kaushik         Draft                                */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_deblk_luma_vert_bslt4_mbaff(UWORD8 *pu1_src,
                                       WORD32 src_strd,
                                       WORD32 alpha,
                                       WORD32 beta,
                                       UWORD32 u4_bs,
                                       const UWORD8 *pu1_cliptab)
{
    WORD8 i = 0, edge;
    UWORD8 p2, p1, p0, q0, q1, q2;
    WORD32 pos_p2, pos_p1, pos_p0, pos_q0, pos_q1, pos_q2;
    UWORD8 a_p, a_q; /* Threshold variables */
    WORD32 blk_strd = src_strd << 1; /* block_increment = src_strd * 2 */
    UWORD8 *pu1_src_temp;
    WORD8 delta;
    WORD8 tc;
    WORD16 val;
    UWORD8 tc0, u1_bs;

    pos_q0 = 0;
    pos_q1 = 1;
    pos_q2 = 2;
    pos_p0 = -1;
    pos_p1 = -2;
    pos_p2 = -3;

    for(edge = 0; edge < 4; edge++, pu1_src += blk_strd)
    {
        pu1_src_temp = pu1_src;
        /* Filter Decision */
        u1_bs = (UWORD8)((u4_bs >> ((3 - edge) << 3)) & 0x0ff);
        if(!u1_bs)
            continue;
        /* tc0 */
        tc0 = pu1_cliptab[u1_bs];
        for(i = 0; i < 2; ++i, pu1_src_temp += src_strd)
        {
            q0 = pu1_src_temp[pos_q0];
            q1 = pu1_src_temp[pos_q1];
            p0 = pu1_src_temp[pos_p0];
            p1 = pu1_src_temp[pos_p1];

            /* Filter Decision */
            if((ABS(p0 - q0) >= alpha) ||
               (ABS(q1 - q0) >= beta) ||
               (ABS(p1 - p0) >= beta))
                continue;

            q2 = pu1_src_temp[pos_q2];
            p2 = pu1_src_temp[pos_p2];

            a_p = ABS(p2 - p0);
            a_q = ABS(q2 - q0);

            /* tc */
            tc = tc0 + (a_p < beta) + (a_q < beta);

            val = ((((q0 - p0) << 2) + (p1 - q1) + 4) >> 3);
            delta = CLIP3(-tc, tc, val);
            /* p0' */
            val = p0 + delta;
            pu1_src_temp[pos_p0] = CLIP_U8(val);
            /* q0' */
            val = q0 - delta;
            pu1_src_temp[pos_q0] = CLIP_U8(val);

            /* Luma only */
            if(a_p < beta)
            {
                /* p1' */
                val = ((p2 + ((p0 + q0 + 1) >> 1) - (p1 << 1)) >> 1);
                pu1_src_temp[pos_p1] += CLIP3(-tc0, tc0, val);
            }

            if(a_q < beta)
            {
                /* q1' */
                val = ((q2 + ((p0 + q0 + 1) >> 1) - (q1 << 1)) >> 1);
                pu1_src_temp[pos_q1] += CLIP3(-tc0, tc0, val);
            }
        }
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_deblk_chroma_vert_bslt4_mbaff_bp()                 */
/*                                                                           */
/*  Description   : This function performs filtering of a chroma block       */
/*                  vertical edge when boundary strength is less than 4.     */
/*                                                                           */
/*  Inputs        : pu1_src       - pointer to the src sample q0 of U        */
/*                  src_strd      - source stride                            */
/*                  alpha         - alpha value for the boundary             */
/*                  beta          - beta value for the boundary              */
/*                  u4_bs         - packed Boundary strength array           */
/*                  pu1_cliptab   - tc0_table                                */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : When the function is called twice, this operation is as  */
/*                  described in Sec. 8.7.2.3 under the title "Filtering     */
/*                  process for edges for bS less than 4" in ITU T Rec H.264.*/
/*                                                                           */
/*  Outputs       : None                                                     */
/*                                                                           */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         29 12 2014   Kaushik         Draft                                */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_deblk_chroma_vert_bslt4_mbaff_bp(UWORD8 *pu1_src,
                                            WORD32 src_strd,
                                            WORD32 alpha,
                                            WORD32 beta,
                                            UWORD32 u4_bs,
                                            const UWORD8 *pu1_cliptab)
{
    UWORD8 *pu1_src_u = pu1_src; /* Pointer to the src sample q0 of plane U*/
    UWORD8 *pu1_src_v = pu1_src + 1; /* Pointer to the src sample q0 of plane V*/
    UWORD8 p1_u, p0_u, q0_u, q1_u, p1_v, p0_v, q0_v, q1_v;
    WORD32 blk_strd = src_strd;
    WORD32 pos_p1, pos_p0, pos_q0, pos_q1;
    UWORD8 *pu1_src_temp_u, *pu1_src_temp_v;
    WORD8 edge;
    WORD8 delta;
    WORD8 tc;
    WORD16 val;
    UWORD8 tc0, u1_bs;

    pos_q0 = 0;
    pos_q1 = 2;
    pos_p0 = -2;
    pos_p1 = -4;

    for(edge = 0; edge < 4;
                    edge++, pu1_src_u += blk_strd, pu1_src_v += blk_strd)
    {
        pu1_src_temp_u = pu1_src_u;
        pu1_src_temp_v = pu1_src_v;
        /* Filter Decision */
        u1_bs = (UWORD8)((u4_bs >> ((3 - edge) << 3)) & 0x0ff);
        if(!u1_bs)
            continue;
        /* tc0 */
        tc0 = pu1_cliptab[u1_bs];
        tc = tc0 + 1;

        q0_u = pu1_src_temp_u[pos_q0];
        q1_u = pu1_src_temp_u[pos_q1];
        p0_u = pu1_src_temp_u[pos_p0];
        p1_u = pu1_src_temp_u[pos_p1];

        q0_v = pu1_src_temp_v[pos_q0];
        q1_v = pu1_src_temp_v[pos_q1];
        p0_v = pu1_src_temp_v[pos_p0];
        p1_v = pu1_src_temp_v[pos_p1];

        /* Filter Decision */
        if((ABS(p0_u - q0_u) < alpha) &&
           (ABS(q1_u - q0_u) < beta) &&
           (ABS(p1_u - p0_u) < beta))
        {
            val = ((((q0_u - p0_u) << 2) + (p1_u - q1_u) + 4) >> 3);
            delta = CLIP3(-tc, tc, val);
            /* p0' */
            val = p0_u + delta;
            pu1_src_temp_u[pos_p0] = CLIP_U8(val);
            /* q0' */
            val = q0_u - delta;
            pu1_src_temp_u[pos_q0] = CLIP_U8(val);
        }

        /* Filter Decision */
        if((ABS(p0_v - q0_v) < alpha) &&
           (ABS(q1_v - q0_v) < beta) &&
           (ABS(p1_v - p0_v) < beta))
        {
            val = ((((q0_v - p0_v) << 2) + (p1_v - q1_v) + 4) >> 3);
            delta = CLIP3(-tc, tc, val);
            /* p0' */
            val = p0_v + delta;
            pu1_src_temp_v[pos_p0] = CLIP_U8(val);
            /* q0' */
            val = q0_v - delta;
            pu1_src_temp_v[pos_q0] = CLIP_U8(val);
        }
    }
}

/*****************************************************************************/
/* Function Definitions for chroma deblocking in high profile                */
/*****************************************************************************/

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_deblk_chroma_vert_bs4()                            */
/*                                                                           */
/*  Description   : This function performs filtering of a chroma block       */
/*                  vertical edge when the boundary strength is set to 4 in  */
/*                  high profile.                                            */
/*                                                                           */
/*  Inputs        : pu1_src    - pointer to the src sample q0 of U           */
/*                  src_strd   - source stride                               */
/*                  alpha_cb   - alpha value for the boundary in U           */
/*                  beta_cb    - beta value for the boundary in U            */
/*                  alpha_cr   - alpha value for the boundary in V           */
/*                  beta_cr    - beta value for the boundary in V            */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : This operation is described in Sec. 8.7.2.4 under the    */
/*                  title "Filtering process for edges for bS equal to 4" in */
/*                  ITU T Rec H.264 with alpha and beta values different in  */
/*                  U and V.                                                 */
/*                                                                           */
/*  Outputs       : None                                                     */
/*                                                                           */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         29 12 2014   Kaushik         Draft                                */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_deblk_chroma_vert_bs4(UWORD8 *pu1_src,
                                 WORD32 src_strd,
                                 WORD32 alpha_cb,
                                 WORD32 beta_cb,
                                 WORD32 alpha_cr,
                                 WORD32 beta_cr)
{
    UWORD8 *pu1_src_u = pu1_src; /* Pointer to the src sample q0 of U */
    UWORD8 *pu1_src_v = pu1_src + 1; /* Pointer to the src sample q0 of V */
    UWORD8 p1_u, p0_u, q0_u, q1_u, p1_v, p0_v, q0_v, q1_v;
    WORD32 blk_strd = src_strd << 1; /* block_increment = src_strd * 2*/
    WORD32 pos_p1, pos_p0, pos_q0, pos_q1;
    UWORD8 *pu1_src_temp_u, *pu1_src_temp_v;
    WORD8 i = 0, edge;

    pos_q0 = 0;
    pos_q1 = 2;
    pos_p0 = -2;
    pos_p1 = -4;

    for(edge = 0; edge < 4;
                    edge++, pu1_src_u += blk_strd, pu1_src_v += blk_strd)
    {
        pu1_src_temp_u = pu1_src_u;
        pu1_src_temp_v = pu1_src_v;
        for(i = 0; i < 2; ++i, pu1_src_temp_u += src_strd, pu1_src_temp_v +=
                        src_strd)
        {
            q0_u = pu1_src_temp_u[pos_q0];
            q1_u = pu1_src_temp_u[pos_q1];
            p0_u = pu1_src_temp_u[pos_p0];
            p1_u = pu1_src_temp_u[pos_p1];
            q0_v = pu1_src_temp_v[pos_q0];
            q1_v = pu1_src_temp_v[pos_q1];
            p0_v = pu1_src_temp_v[pos_p0];
            p1_v = pu1_src_temp_v[pos_p1];

            /* Filter Decision */
            if((ABS(p0_u - q0_u) < alpha_cb) &&
               (ABS(q1_u - q0_u) < beta_cb) &&
               (ABS(p1_u - p0_u) < beta_cb))
            {
                /* p0' */
                pu1_src_temp_u[pos_p0] = ((X2(p1_u) + p0_u + q1_u + 2) >> 2);
                /* q0' */
                pu1_src_temp_u[pos_q0] = (X2(q1_u) + q0_u + p1_u + 2) >> 2;
            }

            /* Filter Decision */
            if((ABS(p0_v - q0_v) < alpha_cr) &&
               (ABS(q1_v - q0_v) < beta_cr) &&
               (ABS(p1_v - p0_v) < beta_cr))
            {
                /* p0' */
                pu1_src_temp_v[pos_p0] = ((X2(p1_v) + p0_v + q1_v + 2) >> 2);
                /* q0' */
                pu1_src_temp_v[pos_q0] = (X2(q1_v) + q0_v + p1_v + 2) >> 2;
            }
        }
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_deblk_chroma_horz_bs4()                            */
/*                                                                           */
/*  Description   : This function performs filtering of a chroma block       */
/*                  horizontal edge when the boundary strength is set to 4   */
/*                  in high profile.                                         */
/*                                                                           */
/*  Inputs        : pu1_src    - pointer to the src sample q0 of U           */
/*                  src_strd   - source stride                               */
/*                  alpha_cb   - alpha value for the boundary in U           */
/*                  beta_cb    - beta value for the boundary in U            */
/*                  alpha_cr   - alpha value for the boundary in V           */
/*                  beta_cr    - beta value for the boundary in V            */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : This operation is described in Sec. 8.7.2.4 under the    */
/*                  title "Filtering process for edges for bS equal to 4" in */
/*                  ITU T Rec H.264 with alpha and beta values different in  */
/*                  U and V.                                                 */
/*                                                                           */
/*  Outputs       : None                                                     */
/*                                                                           */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         29 12 2014   Kaushik         Draft                                */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_deblk_chroma_horz_bs4(UWORD8 *pu1_src,
                                 WORD32 src_strd,
                                 WORD32 alpha_cb,
                                 WORD32 beta_cb,
                                 WORD32 alpha_cr,
                                 WORD32 beta_cr)
{
    UWORD8 *pu1_src_u = pu1_src; /* Pointer to the src sample q0 of U */
    UWORD8 *pu1_src_v = pu1_src + 1; /* Pointer to the src sample q0 of V */
    UWORD8 p1_u, p0_u, q0_u, q1_u, p1_v, p0_v, q0_v, q1_v;
    WORD32 pos_p1, pos_p0, pos_q0, pos_q1;
    UWORD8 *pu1_src_temp_u, *pu1_src_temp_v;
    UWORD8 *pu1_p1_u; /* Pointer to the src sample p1 of U */
    UWORD8 *pu1_p1_v; /* Pointer to the src sample p1 of U */
    UWORD8 *pu1_p1_temp_u, *pu1_p1_temp_v;
    WORD8 i = 0, edge;

    pu1_p1_u = pu1_src_u - (src_strd << 1);
    pu1_p1_v = pu1_src_v - (src_strd << 1);
    pos_q0 = 0;
    pos_q1 = src_strd;
    pos_p0 = src_strd;
    pos_p1 = 0;

    for(edge = 0; edge < 4; edge++, pu1_src_u += 4, pu1_p1_u += 4, pu1_src_v +=
                    4, pu1_p1_v += 4)
    {
        pu1_src_temp_u = pu1_src_u;
        pu1_p1_temp_u = pu1_p1_u;
        pu1_src_temp_v = pu1_src_v;
        pu1_p1_temp_v = pu1_p1_v;
        for(i = 0; i < 2; ++i, pu1_src_temp_u += 2, pu1_p1_temp_u += 2,
                       pu1_src_temp_v += 2, pu1_p1_temp_v += 2)
        {
            q0_u = pu1_src_temp_u[pos_q0];
            q1_u = pu1_src_temp_u[pos_q1];
            p0_u = pu1_p1_temp_u[pos_p0];
            p1_u = pu1_p1_temp_u[pos_p1];

            q0_v = pu1_src_temp_v[pos_q0];
            q1_v = pu1_src_temp_v[pos_q1];
            p0_v = pu1_p1_temp_v[pos_p0];
            p1_v = pu1_p1_temp_v[pos_p1];

            /* Filter Decision */
            if(ABS(p0_u - q0_u) < alpha_cb && ABS(q1_u - q0_u) < beta_cb
                            && ABS(p1_u - p0_u) < beta_cb)
            {
                /* p0' */
                pu1_p1_temp_u[pos_p0] = (X2(p1_u) + p0_u + q1_u + 2) >> 2;
                /* q0' */
                pu1_src_temp_u[pos_q0] = (X2(q1_u) + q0_u + p1_u + 2) >> 2;
            }

            /* Filter Decision */
            if(ABS(p0_v - q0_v) < alpha_cr && ABS(q1_v - q0_v) < beta_cr
                            && ABS(p1_v - p0_v) < beta_cr)
            {
                /* p0' */
                pu1_p1_temp_v[pos_p0] = (X2(p1_v) + p0_v + q1_v + 2) >> 2;
                /* q0' */
                pu1_src_temp_v[pos_q0] = (X2(q1_v) + q0_v + p1_v + 2) >> 2;
            }
        }
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_deblk_chroma_vert_bslt4()                          */
/*                                                                           */
/*  Description   : This function performs filtering of a chroma block       */
/*                  vertical edge when the boundary strength is less than 4  */
/*                  in high profile.                                         */
/*                                                                           */
/*  Inputs        : pu1_src          - pointer to the src sample q0 of U     */
/*                  src_strd         - source stride                         */
/*                  alpha_cb         - alpha value for the boundary in U     */
/*                  beta_cb          - beta value for the boundary in U      */
/*                  alpha_cr         - alpha value for the boundary in V     */
/*                  beta_cr          - beta value for the boundary in V      */
/*                  u4_bs            - packed Boundary strength array        */
/*                  pu1_cliptab_cb   - tc0_table for U                       */
/*                  pu1_cliptab_cr   - tc0_table for V                       */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : This operation is described in Sec. 8.7.2.3 under the    */
/*                  title "Filtering process for edges for bS less than 4"   */
/*                  in ITU T Rec H.264 with alpha and beta values different  */
/*                  in U and V.                                              */
/*                                                                           */
/*  Outputs       : None                                                     */
/*                                                                           */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         29 12 2014   Kaushik         Draft                                */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_deblk_chroma_vert_bslt4(UWORD8 *pu1_src,
                                   WORD32 src_strd,
                                   WORD32 alpha_cb,
                                   WORD32 beta_cb,
                                   WORD32 alpha_cr,
                                   WORD32 beta_cr,
                                   UWORD32 u4_bs,
                                   const UWORD8 *pu1_cliptab_cb,
                                   const UWORD8 *pu1_cliptab_cr)
{
    UWORD8 *pu1_src_u = pu1_src; /* Pointer to the src sample q0 of plane U*/
    UWORD8 *pu1_src_v = pu1_src + 1; /* Pointer to the src sample q0 of plane V*/
    UWORD8 p1_u, p0_u, q0_u, q1_u, p1_v, p0_v, q0_v, q1_v;
    WORD32 blk_strd = src_strd << 1; /* block_increment = src_strd * 2 */
    WORD32 pos_p1, pos_p0, pos_q0, pos_q1;
    UWORD8 *pu1_src_temp_u, *pu1_src_temp_v;
    WORD8 i = 0, edge;
    WORD8 delta;
    WORD8 tcb, tcr;
    WORD16 val;
    UWORD8 tcb0, tcr0, u1_bs;

    pos_q0 = 0;
    pos_q1 = 2;
    pos_p0 = -2;
    pos_p1 = -4;

    for(edge = 0; edge < 4;
                    edge++, pu1_src_u += blk_strd, pu1_src_v += blk_strd)
    {
        pu1_src_temp_u = pu1_src_u;
        pu1_src_temp_v = pu1_src_v;
        /* Filter Decision */
        u1_bs = (UWORD8)((u4_bs >> ((3 - edge) << 3)) & 0x0ff);
        if(!u1_bs)
            continue;
        /* tc0 */
        tcb0 = pu1_cliptab_cb[u1_bs];
        tcr0 = pu1_cliptab_cr[u1_bs];
        tcb = tcb0 + 1;
        tcr = tcr0 + 1;
        for(i = 0; i < 2; ++i, pu1_src_temp_u += src_strd, pu1_src_temp_v +=
                        src_strd)
        {
            q0_u = pu1_src_temp_u[pos_q0];
            q1_u = pu1_src_temp_u[pos_q1];
            p0_u = pu1_src_temp_u[pos_p0];
            p1_u = pu1_src_temp_u[pos_p1];

            q0_v = pu1_src_temp_v[pos_q0];
            q1_v = pu1_src_temp_v[pos_q1];
            p0_v = pu1_src_temp_v[pos_p0];
            p1_v = pu1_src_temp_v[pos_p1];

            /* Filter Decision */
            if(ABS(p0_u - q0_u) < alpha_cb && ABS(q1_u - q0_u) < beta_cb
                            && ABS(p1_u - p0_u) < beta_cb)
            {
                val = ((((q0_u - p0_u) << 2) + (p1_u - q1_u) + 4) >> 3);
                delta = CLIP3(-tcb, tcb, val);
                /* p0' */
                val = p0_u + delta;
                pu1_src_temp_u[pos_p0] = CLIP_U8(val);
                /* q0' */
                val = q0_u - delta;
                pu1_src_temp_u[pos_q0] = CLIP_U8(val);
            }

            /* Filter Decision */
            if(ABS(p0_v - q0_v) < alpha_cr && ABS(q1_v - q0_v) < beta_cr
                            && ABS(p1_v - p0_v) < beta_cr)
            {
                val = ((((q0_v - p0_v) << 2) + (p1_v - q1_v) + 4) >> 3);
                delta = CLIP3(-tcr, tcr, val);
                /* p0' */
                val = p0_v + delta;
                pu1_src_temp_v[pos_p0] = CLIP_U8(val);
                /* q0' */
                val = q0_v - delta;
                pu1_src_temp_v[pos_q0] = CLIP_U8(val);
            }
        }
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_deblk_chroma_horz_bslt4()                          */
/*                                                                           */
/*  Description   : This function performs filtering of a chroma block       */
/*                  horizontal edge when the boundary strength is less than  */
/*                  4 in high profile.                                       */
/*                                                                           */
/*  Inputs        : pu1_src          - pointer to the src sample q0 of U     */
/*                  src_strd         - source stride                         */
/*                  alpha_cb         - alpha value for the boundary in U     */
/*                  beta_cb          - beta value for the boundary in U      */
/*                  alpha_cr         - alpha value for the boundary in V     */
/*                  beta_cr          - beta value for the boundary in V      */
/*                  u4_bs            - packed Boundary strength array        */
/*                  pu1_cliptab_cb   - tc0_table for U                       */
/*                  pu1_cliptab_cr   - tc0_table for V                       */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : This operation is described in Sec. 8.7.2.3 under the    */
/*                  title "Filtering process for edges for bS less than 4"   */
/*                  in ITU T Rec H.264 with alpha and beta values different  */
/*                  in U and V.                                              */
/*                                                                           */
/*  Outputs       : None                                                     */
/*                                                                           */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         29 12 2014   Kaushik         Draft                                */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_deblk_chroma_horz_bslt4(UWORD8 *pu1_src,
                                   WORD32 src_strd,
                                   WORD32 alpha_cb,
                                   WORD32 beta_cb,
                                   WORD32 alpha_cr,
                                   WORD32 beta_cr,
                                   UWORD32 u4_bs,
                                   const UWORD8 *pu1_cliptab_cb,
                                   const UWORD8 *pu1_cliptab_cr)
{
    UWORD8 *pu1_src_u = pu1_src; /* Pointer to the src sample q0 of plane U*/
    UWORD8 *pu1_src_v = pu1_src + 1; /* Pointer to the src sample q0 of plane V*/
    UWORD8 p1_u, p0_u, q0_u, q1_u, p1_v, p0_v, q0_v, q1_v;
    WORD32 pos_p1, pos_p0, pos_q0, pos_q1;
    UWORD8 *pu1_src_temp_u, *pu1_src_temp_v;
    UWORD8 *pu1_p1_u; /* Pointer to the src sample p1 of plane U*/
    UWORD8 *pu1_p1_v; /* Pointer to the src sample p1 of plane V*/
    UWORD8 *pu1_p1_temp_u, *pu1_p1_temp_v;
    WORD8 i = 0, edge;
    WORD8 delta;
    WORD8 tcb, tcr;
    WORD16 val;
    UWORD8 u1_bs;
    UWORD8 tcb0, tcr0;

    pu1_p1_u = pu1_src_u - (src_strd << 1);
    pu1_p1_v = pu1_src_v - (src_strd << 1);
    pos_q0 = 0;
    pos_q1 = src_strd;
    pos_p0 = src_strd;
    pos_p1 = 0;

    for(edge = 0; edge < 4; edge++, pu1_src_u += 4, pu1_p1_u += 4,
                    pu1_src_v += 4, pu1_p1_v += 4)
    {
        pu1_src_temp_u = pu1_src_u;
        pu1_p1_temp_u = pu1_p1_u;
        pu1_src_temp_v = pu1_src_v;
        pu1_p1_temp_v = pu1_p1_v;

        /* Filter Decision */
        u1_bs = (UWORD8)((u4_bs >> ((3 - edge) << 3)) & 0x0ff);
        if(!u1_bs)
            continue;
        /* tc0 */
        tcb0 = pu1_cliptab_cb[u1_bs];
        tcr0 = pu1_cliptab_cr[u1_bs];

        for(i = 0; i < 2; ++i, pu1_src_temp_u += 2, pu1_p1_temp_u += 2,
                       pu1_src_temp_v += 2, pu1_p1_temp_v += 2)
        {
            q0_u = pu1_src_temp_u[pos_q0];
            q1_u = pu1_src_temp_u[pos_q1];
            p0_u = pu1_p1_temp_u[pos_p0];
            p1_u = pu1_p1_temp_u[pos_p1];

            q0_v = pu1_src_temp_v[pos_q0];
            q1_v = pu1_src_temp_v[pos_q1];
            p0_v = pu1_p1_temp_v[pos_p0];
            p1_v = pu1_p1_temp_v[pos_p1];

            /* tc */
            tcb = tcb0 + 1;
            tcr = tcr0 + 1;
            /* Filter Decision */
            if(ABS(p0_u - q0_u) < alpha_cb && ABS(q1_u - q0_u) < beta_cb
                            && ABS(p1_u - p0_u) < beta_cb)
            {
                val = ((((q0_u - p0_u) << 2) + (p1_u - q1_u) + 4) >> 3);
                delta = CLIP3(-tcb, tcb, val);
                /* p0' */
                val = p0_u + delta;
                pu1_p1_temp_u[pos_p0] = CLIP_U8(val);
                /* q0' */
                val = q0_u - delta;
                pu1_src_temp_u[pos_q0] = CLIP_U8(val);
            }
            /* Filter Decision */
            if(ABS(p0_v - q0_v) < alpha_cr && ABS(q1_v - q0_v) < beta_cr
                            && ABS(p1_v - p0_v) < beta_cr)
            {
                val = ((((q0_v - p0_v) << 2) + (p1_v - q1_v) + 4) >> 3);
                delta = CLIP3(-tcr, tcr, val);
                /* p0' */
                val = p0_v + delta;
                pu1_p1_temp_v[pos_p0] = CLIP_U8(val);
                /* q0' */
                val = q0_v - delta;
                pu1_src_temp_v[pos_q0] = CLIP_U8(val);
            }
        }
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_deblk_chroma_vert_bs4_mbaff()                      */
/*                                                                           */
/*  Description   : This function performs filtering of a chroma block       */
/*                  vertical edge when boundary strength is set to 4 in high */
/*                  profile.                                                 */
/*                                                                           */
/*  Inputs        : pu1_src          - pointer to the src sample q0 of U     */
/*                  src_strd         - source stride                         */
/*                  alpha_cb         - alpha value for the boundary in U     */
/*                  beta_cb          - beta value for the boundary in U      */
/*                  alpha_cr         - alpha value for the boundary in V     */
/*                  beta_cr          - beta value for the boundary in V      */
/*                  u4_bs            - packed Boundary strength array        */
/*                  pu1_cliptab_cb   - tc0_table for U                       */
/*                  pu1_cliptab_cr   - tc0_table for V                       */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : When the function is called twice, this operation is as  */
/*                  described in Sec. 8.7.2.4 under the title "Filtering     */
/*                  process for edges for bS equal to 4" in ITU T Rec H.264  */
/*                  with alpha and beta values different in U and V.         */
/*                                                                           */
/*  Outputs       : None                                                     */
/*                                                                           */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         29 12 2014   Kaushik         Draft                                */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_deblk_chroma_vert_bs4_mbaff(UWORD8 *pu1_src,
                                       WORD32 src_strd,
                                       WORD32 alpha_cb,
                                       WORD32 beta_cb,
                                       WORD32 alpha_cr,
                                       WORD32 beta_cr)
{
    UWORD8 *pu1_src_u = pu1_src; /* Pointer to the src sample q0 of U */
    UWORD8 *pu1_src_v = pu1_src + 1; /* Pointer to the src sample q0 of V */
    UWORD8 p1_u, p0_u, q0_u, q1_u, p1_v, p0_v, q0_v, q1_v;
    WORD32 blk_strd = src_strd;
    WORD32 pos_p1, pos_p0, pos_q0, pos_q1;
    UWORD8 *pu1_src_temp_u, *pu1_src_temp_v;
    WORD8 edge;

    pos_q0 = 0;
    pos_q1 = 2;
    pos_p0 = -2;
    pos_p1 = -4;

    for(edge = 0; edge < 4;
                    edge++, pu1_src_u += blk_strd, pu1_src_v += blk_strd)
    {
        pu1_src_temp_u = pu1_src_u;
        pu1_src_temp_v = pu1_src_v;
        q0_u = pu1_src_temp_u[pos_q0];
        q1_u = pu1_src_temp_u[pos_q1];
        p0_u = pu1_src_temp_u[pos_p0];
        p1_u = pu1_src_temp_u[pos_p1];
        q0_v = pu1_src_temp_v[pos_q0];
        q1_v = pu1_src_temp_v[pos_q1];
        p0_v = pu1_src_temp_v[pos_p0];
        p1_v = pu1_src_temp_v[pos_p1];

        /* Filter Decision */
        if((ABS(p0_u - q0_u) < alpha_cb) &&
           (ABS(q1_u - q0_u) < beta_cb)  &&
           (ABS(p1_u - p0_u) < beta_cb))
        {
            /* p0' */
            pu1_src_temp_u[pos_p0] = ((X2(p1_u) + p0_u + q1_u + 2) >> 2);
            /* q0' */
            pu1_src_temp_u[pos_q0] = (X2(q1_u) + q0_u + p1_u + 2) >> 2;
        }

        /* Filter Decision */
        if((ABS(p0_v - q0_v) < alpha_cr) &&
           (ABS(q1_v - q0_v) < beta_cr) &&
           (ABS(p1_v - p0_v) < beta_cr))
        {
            /* p0' */
            pu1_src_temp_v[pos_p0] = ((X2(p1_v) + p0_v + q1_v + 2) >> 2);
            /* q0' */
            pu1_src_temp_v[pos_q0] = (X2(q1_v) + q0_v + p1_v + 2) >> 2;
        }
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_deblk_chroma_vert_bslt4_mbaff()                    */
/*                                                                           */
/*  Description   : This function performs filtering of a chroma block       */
/*                  vertical edge when boundary strength is less than 4 in   */
/*                  high profile.                                            */
/*                                                                           */
/*  Inputs        : pu1_src          - pointer to the src sample q0 of U     */
/*                  src_strd         - source stride                         */
/*                  alpha_cb         - alpha value for the boundary in U     */
/*                  beta_cb          - beta value for the boundary in U      */
/*                  alpha_cr         - alpha value for the boundary in V     */
/*                  beta_cr          - beta value for the boundary in V      */
/*                  u4_bs            - packed Boundary strength array        */
/*                  pu1_cliptab_cb   - tc0_table for U                       */
/*                  pu1_cliptab_cr   - tc0_table for V                       */
/*                                                                           */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : When the function is called twice, this operation is as  */
/*                  described in Sec. 8.7.2.4 under the title "Filtering     */
/*                  process for edges for bS less than 4" in ITU T Rec H.264 */
/*                  with alpha and beta values different in U and V.         */
/*                                                                           */
/*  Outputs       : None                                                     */
/*                                                                           */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         29 12 2014   Kaushik         Draft                                */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_deblk_chroma_vert_bslt4_mbaff(UWORD8 *pu1_src,
                                         WORD32 src_strd,
                                         WORD32 alpha_cb,
                                         WORD32 beta_cb,
                                         WORD32 alpha_cr,
                                         WORD32 beta_cr,
                                         UWORD32 u4_bs,
                                         const UWORD8 *pu1_cliptab_cb,
                                         const UWORD8 *pu1_cliptab_cr)
{
    UWORD8 *pu1_src_u = pu1_src; /* Pointer to the src sample q0 of plane U*/
    UWORD8 *pu1_src_v = pu1_src + 1; /* Pointer to the src sample q0 of plane V*/
    UWORD8 p1_u, p0_u, q0_u, q1_u, p1_v, p0_v, q0_v, q1_v;
    WORD32 blk_strd = src_strd;
    WORD32 pos_p1, pos_p0, pos_q0, pos_q1;
    UWORD8 *pu1_src_temp_u, *pu1_src_temp_v;
    WORD8 edge;
    WORD8 delta;
    WORD8 tcb, tcr;
    WORD16 val;
    UWORD8 tcb0, tcr0, u1_bs;

    pos_q0 = 0;
    pos_q1 = 2;
    pos_p0 = -2;
    pos_p1 = -4;

    for(edge = 0; edge < 4;
                    edge++, pu1_src_u += blk_strd, pu1_src_v += blk_strd)
    {
        pu1_src_temp_u = pu1_src_u;
        pu1_src_temp_v = pu1_src_v;
        /* Filter Decision */
        u1_bs = (UWORD8)((u4_bs >> ((3 - edge) << 3)) & 0x0ff);
        if(!u1_bs)
            continue;
        /* tc0 */
        tcb0 = pu1_cliptab_cb[u1_bs];
        tcr0 = pu1_cliptab_cr[u1_bs];
        tcb = tcb0 + 1;
        tcr = tcr0 + 1;
        q0_u = pu1_src_temp_u[pos_q0];
        q1_u = pu1_src_temp_u[pos_q1];
        p0_u = pu1_src_temp_u[pos_p0];
        p1_u = pu1_src_temp_u[pos_p1];

        q0_v = pu1_src_temp_v[pos_q0];
        q1_v = pu1_src_temp_v[pos_q1];
        p0_v = pu1_src_temp_v[pos_p0];
        p1_v = pu1_src_temp_v[pos_p1];

        /* Filter Decision */
        if((ABS(p0_u - q0_u) < alpha_cb) &&
           (ABS(q1_u - q0_u) < beta_cb) &&
           (ABS(p1_u - p0_u) < beta_cb))
        {
            val = ((((q0_u - p0_u) << 2) + (p1_u - q1_u) + 4) >> 3);
            delta = CLIP3(-tcb, tcb, val);
            /* p0' */
            val = p0_u + delta;
            pu1_src_temp_u[pos_p0] = CLIP_U8(val);
            /* q0' */
            val = q0_u - delta;
            pu1_src_temp_u[pos_q0] = CLIP_U8(val);
        }

        /* Filter Decision */
        if((ABS(p0_v - q0_v) < alpha_cr) &&
           (ABS(q1_v - q0_v) < beta_cr) &&
           (ABS(p1_v - p0_v) < beta_cr))
        {
            val = ((((q0_v - p0_v) << 2) + (p1_v - q1_v) + 4) >> 3);
            delta = CLIP3(-tcr, tcr, val);
            /* p0' */
            val = p0_v + delta;
            pu1_src_temp_v[pos_p0] = CLIP_U8(val);
            /* q0' */
            val = q0_v - delta;
            pu1_src_temp_v[pos_q0] = CLIP_U8(val);
        }
    }
}
