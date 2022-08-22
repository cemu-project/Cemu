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
/*  File Name         : ih264_weighted_pred.c                                */
/*                                                                           */
/*  Description       : Contains function definitions for weighted           */
/*                      prediction functions                                 */
/*                                                                           */
/*  List of Functions : ih264_default_weighted_pred_luma()                   */
/*                      ih264_default_weighted_pred_chroma()                 */
/*                      ih264_weighted_pred_luma()                           */
/*                      ih264_weighted_pred_chroma()                         */
/*                      ih264_weighted_bipred_luma()                         */
/*                      ih264_weighted_bipred_chroma()                       */
/*                                                                           */
/*  Issues / Problems : None                                                 */
/*                                                                           */
/*  Revision History  :                                                      */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes                              */
/*         07 01 2015   Kaushik         Initial version                      */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

/* User include files */
#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264_weighted_pred.h"

/*****************************************************************************/
/*  Function definitions .                                                   */
/*****************************************************************************/
/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_default_weighted_pred_luma                         */
/*                                                                           */
/*  Description   : This function performs the default weighted prediction   */
/*                  as described in sec 8.4.2.3.1 titled "Default weighted   */
/*                  sample prediction process" for luma. The function gets   */
/*                  two ht x wd blocks, calculates their rounded-average and */
/*                  stores it in the destination block. (ht,wd) can be       */
/*                  (4,4), (8,4), (4,8), (8,8), (16,8), (8,16) or (16,16).   */
/*                                                                           */
/*  Inputs        : puc_src1  - Pointer to source 1                          */
/*                  puc_src2  - Pointer to source 2                          */
/*                  puc_dst   - Pointer to destination                       */
/*                  src_strd1 - stride for source 1                          */
/*                  src_strd1 - stride for source 2                          */
/*                  dst_strd  - stride for destination                       */
/*                  ht        - height of the block                          */
/*                  wd        - width of the block                           */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes                              */
/*         07 01 2015   Kaushik         Initial Version                      */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_default_weighted_pred_luma(UWORD8 *pu1_src1,
                                      UWORD8 *pu1_src2,
                                      UWORD8 *pu1_dst,
                                      WORD32 src_strd1,
                                      WORD32 src_strd2,
                                      WORD32 dst_strd,
                                      WORD32 ht,
                                      WORD32 wd)
{
    WORD32 i, j;

    src_strd1 -= wd;
    src_strd2 -= wd;
    dst_strd -= wd;

    for(i = 0; i < ht; i++)
    {
        for(j = 0; j < wd; j++, pu1_src1++, pu1_src2++, pu1_dst++)
            *pu1_dst = (*pu1_src1 + *pu1_src2 + 1) >> 1;

        pu1_src1 += src_strd1;
        pu1_src2 += src_strd2;
        pu1_dst += dst_strd;
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_default_weighted_pred_chroma                       */
/*                                                                           */
/*  Description   : This function performs the default weighted prediction   */
/*                  as described in sec 8.4.2.3.1 titled "Default weighted   */
/*                  sample prediction process" for chroma. The function gets */
/*                  two ht x wd blocks, calculates their rounded-average and */
/*                  stores it in the destination block. (ht,wd) can be       */
/*                  (2,2), (4,2) , (2,4), (4,4), (8,4), (4,8) or (8,8).      */
/*                                                                           */
/*  Inputs        : puc_src1  - Pointer to source 1                          */
/*                  puc_src2  - Pointer to source 2                          */
/*                  puc_dst   - Pointer to destination                       */
/*                  src_strd1 - stride for source 1                          */
/*                  src_strd1 - stride for source 2                          */
/*                  dst_strd  - stride for destination                       */
/*                  ht        - height of the block                          */
/*                  wd        - width of the block                           */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes                              */
/*         07 01 2015   Kaushik         Initial Version                      */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_default_weighted_pred_chroma(UWORD8 *pu1_src1,
                                        UWORD8 *pu1_src2,
                                        UWORD8 *pu1_dst,
                                        WORD32 src_strd1,
                                        WORD32 src_strd2,
                                        WORD32 dst_strd,
                                        WORD32 ht,
                                        WORD32 wd)
{
    WORD32 i, j;

    wd = wd << 1;

    src_strd1 -= wd;
    src_strd2 -= wd;
    dst_strd -= wd;

    for(i = 0; i < ht; i++)
    {
        for(j = 0; j < wd; j++, pu1_src1++, pu1_src2++, pu1_dst++)
            *pu1_dst = (*pu1_src1 + *pu1_src2 + 1) >> 1;

        pu1_src1 += src_strd1;
        pu1_src2 += src_strd2;
        pu1_dst += dst_strd;
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_weighted_pred_luma                                 */
/*                                                                           */
/*  Description   : This function performs the weighted prediction as        */
/*                  described in sec 8.4.2.3.2 titled "Weighted sample       */
/*                  prediction process" for luma. The function gets one      */
/*                  ht x wd block, weights it, rounds it off, offsets it,    */
/*                  saturates it to unsigned 8-bit and stores it in the      */
/*                  destination block. (ht,wd) can be (4,4), (8,4), (4,8),   */
/*                  (8,8), (16,8), (8,16) or (16,16).                        */
/*                                                                           */
/*  Inputs        : puc_src  - Pointer to source                             */
/*                  puc_dst  - Pointer to destination                        */
/*                  src_strd - stride for source                             */
/*                  dst_strd - stride for destination                        */
/*                  log_wd   - number of bits to be rounded off              */
/*                  wt       - weight value                                  */
/*                  ofst     - offset value                                  */
/*                  ht       - height of the block                           */
/*                  wd       - width of the block                            */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes                              */
/*         07 01 2015   Kaushik         Initial Version                      */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_weighted_pred_luma(UWORD8 *pu1_src,
                              UWORD8 *pu1_dst,
                              WORD32 src_strd,
                              WORD32 dst_strd,
                              WORD32 log_wd,
                              WORD32 wt,
                              WORD32 ofst,
                              WORD32 ht,
                              WORD32 wd)
{
    WORD32 i, j;

    wt = (WORD16)(wt & 0xffff);
    ofst = (WORD8)(ofst & 0xff);

    src_strd -= wd;
    dst_strd -= wd;

    if(log_wd >= 1)
    {
        WORD32 i_ofst = (1 << (log_wd - 1)) + (ofst << log_wd);
        for(i = 0; i < ht; i++)
        {
            for(j = 0; j < wd; j++, pu1_src++, pu1_dst++)
                *pu1_dst = CLIP_U8((wt * (*pu1_src) + i_ofst) >> log_wd);

            pu1_src += src_strd;
            pu1_dst += dst_strd;
        }
    }
    else
    {
        for(i = 0; i < ht; i++)
        {
            for(j = 0; j < wd; j++, pu1_src++, pu1_dst++)
                *pu1_dst = CLIP_U8(wt * (*pu1_src) + ofst);

            pu1_src += src_strd;
            pu1_dst += dst_strd;
        }
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_weighted_pred_chroma                               */
/*                                                                           */
/*  Description   : This function performs the weighted prediction as        */
/*                  described in sec 8.4.2.3.2 titled "Weighted sample       */
/*                  prediction process" for chroma. The function gets one    */
/*                  ht x wd block, weights it, rounds it off, offsets it,    */
/*                  saturates it to unsigned 8-bit and stores it in the      */
/*                  destination block. (ht,wd) can be (2,2), (4,2), (2,4),   */
/*                  (4,4), (8,4), (4,8) or (8,8).                            */
/*                                                                           */
/*  Inputs        : puc_src  - Pointer to source                             */
/*                  puc_dst  - Pointer to destination                        */
/*                  src_strd - stride for source                             */
/*                  dst_strd - stride for destination                        */
/*                  log_wd   - number of bits to be rounded off              */
/*                  wt       - weight values for u and v                     */
/*                  ofst     - offset values for u and v                     */
/*                  ht       - height of the block                           */
/*                  wd       - width of the block                            */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes                              */
/*         07 01 2015   Kaushik         Initial Version                      */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_weighted_pred_chroma(UWORD8 *pu1_src,
                                UWORD8 *pu1_dst,
                                WORD32 src_strd,
                                WORD32 dst_strd,
                                WORD32 log_wd,
                                WORD32 wt,
                                WORD32 ofst,
                                WORD32 ht,
                                WORD32 wd)
{
    WORD32 i, j;
    WORD32 wt_u, wt_v;
    WORD32 ofst_u, ofst_v;

    wt_u = (WORD16)(wt & 0xffff);
    wt_v = (WORD16)(wt >> 16);

    ofst_u = (WORD8)(ofst & 0xff);
    ofst_v = (WORD8)(ofst >> 8);

    src_strd -= wd << 1;
    dst_strd -= wd << 1;

    if(log_wd >= 1)
    {
        ofst_u = (1 << (log_wd - 1)) + (ofst_u << log_wd);
        ofst_v = (1 << (log_wd - 1)) + (ofst_v << log_wd);

        for(i = 0; i < ht; i++)
        {
            for(j = 0; j < wd; j++, pu1_src++, pu1_dst++)
            {
                *pu1_dst = CLIP_U8((wt_u * (*pu1_src) + ofst_u) >> log_wd);
                pu1_src++;
                pu1_dst++;
                *pu1_dst = CLIP_U8((wt_v * (*pu1_src) + ofst_v) >> log_wd);
            }
            pu1_src += src_strd;
            pu1_dst += dst_strd;
        }
    }
    else
    {
        for(i = 0; i < ht; i++)
        {
            for(j = 0; j < wd; j++, pu1_src++, pu1_dst++)
            {
                *pu1_dst = CLIP_U8(wt_u * (*pu1_src) + ofst_u);
                pu1_src++;
                pu1_dst++;
                *pu1_dst = CLIP_U8(wt_v * (*pu1_src) + ofst_v);
            }
            pu1_src += src_strd;
            pu1_dst += dst_strd;
        }
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_weighted_bi_pred_luma                              */
/*                                                                           */
/*  Description   : This function performs the weighted biprediction as      */
/*                  described in sec 8.4.2.3.2 titled "Weighted sample       */
/*                  prediction process" for luma. The function gets two      */
/*                  ht x wd blocks, weights them, adds them, rounds off the  */
/*                  sum, offsets it, saturates it to unsigned 8-bit and      */
/*                  stores it in the destination block. (ht,wd) can be       */
/*                  (4,4), (8,4), (4,8), (8,8), (16,8), (8,16) or (16,16).   */
/*                                                                           */
/*  Inputs        : puc_src1  - Pointer to source 1                          */
/*                  puc_src2  - Pointer to source 2                          */
/*                  puc_dst   - Pointer to destination                       */
/*                  src_strd1 - stride for source 1                          */
/*                  src_strd2 - stride for source 2                          */
/*                  dst_strd2 - stride for destination                       */
/*                  log_wd    - number of bits to be rounded off             */
/*                  wt1       - weight value for source 1                    */
/*                  wt2       - weight value for source 2                    */
/*                  ofst1     - offset value for source 1                    */
/*                  ofst2     - offset value for source 2                    */
/*                  ht        - height of the block                          */
/*                  wd        - width of the block                           */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes                              */
/*         07 01 2015   Kaushik         Initial Version                      */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_weighted_bi_pred_luma(UWORD8 *pu1_src1,
                                 UWORD8 *pu1_src2,
                                 UWORD8 *pu1_dst,
                                 WORD32 src_strd1,
                                 WORD32 src_strd2,
                                 WORD32 dst_strd,
                                 WORD32 log_wd,
                                 WORD32 wt1,
                                 WORD32 wt2,
                                 WORD32 ofst1,
                                 WORD32 ofst2,
                                 WORD32 ht,
                                 WORD32 wd)
{
    WORD32 i, j;
    WORD32 shft, ofst;

    ofst1 = (WORD8)(ofst1 & 0xff);
    ofst2 = (WORD8)(ofst2 & 0xff);
    wt1 = (WORD16)(wt1 & 0xffff);
    wt2 = (WORD16)(wt2 & 0xffff);
    ofst = (ofst1 + ofst2 + 1) >> 1;

    shft = log_wd + 1;
    ofst = (1 << log_wd) + (ofst << shft);

    src_strd1 -= wd;
    src_strd2 -= wd;
    dst_strd -= wd;

    for(i = 0; i < ht; i++)
    {
        for(j = 0; j < wd; j++, pu1_src1++, pu1_src2++, pu1_dst++)
            *pu1_dst = CLIP_U8((wt1 * (*pu1_src1) + wt2 * (*pu1_src2) + ofst) >> shft);

        pu1_src1 += src_strd1;
        pu1_src2 += src_strd2;
        pu1_dst += dst_strd;
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264_weighted_bi_pred_chroma                            */
/*                                                                           */
/*  Description   : This function performs the weighted biprediction as      */
/*                  described in sec 8.4.2.3.2 titled "Weighted sample       */
/*                  prediction process" for chroma. The function gets two    */
/*                  ht x wd blocks, weights them, adds them, rounds off the  */
/*                  sum, offsets it, saturates it to unsigned 8-bit and      */
/*                  stores it in the destination block. (ht,wd) can be       */
/*                  (2,2), (4,2), (2,4), (4,4), (8,4), (4,8) or (8,8).       */
/*                                                                           */
/*  Inputs        : puc_src1  - Pointer to source 1                          */
/*                  puc_src2  - Pointer to source 2                          */
/*                  puc_dst   - Pointer to destination                       */
/*                  src_strd1 - stride for source 1                          */
/*                  src_strd2 - stride for source 2                          */
/*                  dst_strd2 - stride for destination                       */
/*                  log_wd    - number of bits to be rounded off             */
/*                  wt1       - weight values for u and v in source 1        */
/*                  wt2       - weight values for u and v in source 2        */
/*                  ofst1     - offset value for u and v in source 1         */
/*                  ofst2     - offset value for u and v in source 2         */
/*                  ht        - height of the block                          */
/*                  wd        - width of the block                           */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes                              */
/*         07 01 2015   Kaushik         Initial Version                      */
/*                      Senthoor                                             */
/*                                                                           */
/*****************************************************************************/
void ih264_weighted_bi_pred_chroma(UWORD8 *pu1_src1,
                                   UWORD8 *pu1_src2,
                                   UWORD8 *pu1_dst,
                                   WORD32 src_strd1,
                                   WORD32 src_strd2,
                                   WORD32 dst_strd,
                                   WORD32 log_wd,
                                   WORD32 wt1,
                                   WORD32 wt2,
                                   WORD32 ofst1,
                                   WORD32 ofst2,
                                   WORD32 ht,
                                   WORD32 wd)
{
    WORD32 i, j;
    WORD32 wt1_u, wt1_v, wt2_u, wt2_v;
    WORD32 ofst1_u, ofst1_v, ofst2_u, ofst2_v;
    WORD32 ofst_u, ofst_v;
    WORD32 shft;

    ofst1_u = (WORD8)(ofst1 & 0xff);
    ofst1_v = (WORD8)(ofst1 >> 8);
    ofst2_u = (WORD8)(ofst2 & 0xff);
    ofst2_v = (WORD8)(ofst2 >> 8);
    wt1_u = (WORD16)(wt1 & 0xffff);
    wt1_v = (WORD16)(wt1 >> 16);
    wt2_u = (WORD16)(wt2 & 0xffff);
    wt2_v = (WORD16)(wt2 >> 16);
    ofst_u = (ofst1_u + ofst2_u + 1) >> 1;
    ofst_v = (ofst1_v + ofst2_v + 1) >> 1;

    src_strd1 -= wd << 1;
    src_strd2 -= wd << 1;
    dst_strd -= wd << 1;

    shft = log_wd + 1;
    ofst_u = (1 << log_wd) + (ofst_u << shft);
    ofst_v = (1 << log_wd) + (ofst_v << shft);

    for(i = 0; i < ht; i++)
    {
        for(j = 0; j < wd; j++, pu1_src1++, pu1_src2++, pu1_dst++)
        {
            *pu1_dst = CLIP_U8((wt1_u * (*pu1_src1) + wt2_u * (*pu1_src2) + ofst_u) >> shft);
            pu1_src1++;
            pu1_src2++;
            pu1_dst++;
            *pu1_dst = CLIP_U8((wt1_v * (*pu1_src1) + wt2_v * (*pu1_src2) + ofst_v) >> shft);
        }
        pu1_src1 += src_strd1;
        pu1_src2 += src_strd2;
        pu1_dst += dst_strd;
    }
}
