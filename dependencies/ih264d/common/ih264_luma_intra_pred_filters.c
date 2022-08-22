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
 *  ih264_luma_intra_pred_filters.c
 *
 * @brief
 *  Contains function definitions for intra prediction  filters
 *
 * @author
 *  Ittiam
 *
 * @par List of Functions:
 *  - ih264_intra_pred_luma_4x4_mode_vert
 *  - ih264_intra_pred_luma_4x4_mode_horz
 *  - ih264_intra_pred_luma_4x4_mode_dc
 *  - ih264_intra_pred_luma_4x4_mode_diag_dl
 *  - ih264_intra_pred_luma_4x4_mode_diag_dr
 *  - ih264_intra_pred_luma_4x4_mode_vert_r
 *  - ih264_intra_pred_luma_4x4_mode_horz_d
 *  - ih264_intra_pred_luma_4x4_mode_vert_l
 *  - ih264_intra_pred_luma_4x4_mode_horz_u
 *  - ih264_intra_pred_luma_8x8_mode_ref_filtering
 *  - ih264_intra_pred_luma_8x8_mode_vert
 *  - ih264_intra_pred_luma_8x8_mode_horz
 *  - ih264_intra_pred_luma_8x8_mode_dc
 *  - ih264_intra_pred_luma_8x8_mode_diag_dl
 *  - ih264_intra_pred_luma_8x8_mode_diag_dr
 *  - ih264_intra_pred_luma_8x8_mode_vert_r
 *  - ih264_intra_pred_luma_8x8_mode_horz_d
 *  - ih264_intra_pred_luma_8x8_mode_vert_l
 *  - ih264_intra_pred_luma_8x8_mode_horz_u
 *  - ih264_intra_pred_luma_16x16_mode_vert
 *  - ih264_intra_pred_luma_16x16_mode_horz
 *  - ih264_intra_pred_luma_16x16_mode_dc
 *  - ih264_intra_pred_luma_16x16_mode_plane
 *
 *
 * @remarks
 *  None
 *
 ******************************************************************************
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
const WORD8 ih264_gai1_intrapred_luma_plane_coeffs[] =
{ 0x01, 0x02, 0x03, 0x04,
  0x05, 0x06, 0x07, 0x08,
  0x09, 0x0A, 0x0B, 0x0C,
  0x0D, 0x0E, 0x0F, 0x10, };

const WORD8  ih264_gai1_intrapred_luma_8x8_horz_u[] =
{ 0x06,0x15,0x05,0x14,
  0x04,0x13,0x03,0x12,
  0x02,0x11,0x01,0x10,
  0x00,0x1F,0x0F,0x0F
};

/*******************    LUMA INTRAPREDICTION    *******************/

/*******************    4x4 Modes    *******************/

/**
 *******************************************************************************
 *
 *ih264_intra_pred_luma_4x4_mode_vert
 *
 * @brief
 *  Perform Intra prediction for  luma_4x4 mode:vertical
 *
 * @par Description:
 * Perform Intra prediction for  luma_4x4 mode:vertical ,described in sec 8.3.1.2.1
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
void ih264_intra_pred_luma_4x4_mode_vert(UWORD8 *pu1_src,
                                         UWORD8 *pu1_dst,
                                         WORD32 src_strd,
                                         WORD32 dst_strd,
                                         WORD32 ngbr_avail)
{
    UWORD8 *pu1_top = NULL; /* Pointer to start of top predictors */
    UNUSED(src_strd);
    UNUSED(ngbr_avail);
    pu1_top = pu1_src + BLK_SIZE + 1;

    memcpy(pu1_dst, pu1_top, 4);
    memcpy(pu1_dst + dst_strd, pu1_top, 4);
    memcpy(pu1_dst + 2 * dst_strd, pu1_top, 4);
    memcpy(pu1_dst + 3 * dst_strd, pu1_top, 4);
}

/**
 *******************************************************************************
 *
 *ih264_intra_pred_luma_4x4_mode_horz
 *
 * @brief
 *  Perform Intra prediction for  luma_4x4 mode:horizontal
 *
 * @par Description:
 *  Perform Intra prediction for  luma_4x4 mode:horizontal ,described in sec 8.3.1.2.2
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
void ih264_intra_pred_luma_4x4_mode_horz(UWORD8 *pu1_src,
                                         UWORD8 *pu1_dst,
                                         WORD32 src_strd,
                                         WORD32 dst_strd,
                                         WORD32 ngbr_avail)
{
    UWORD8 *pu1_left = NULL; /* Pointer to start of left predictors */

    UNUSED(src_strd);
    UNUSED(ngbr_avail);
    pu1_left = pu1_src + BLK_SIZE - 1;

    memset(pu1_dst, *pu1_left, 4);
    memset(pu1_dst + dst_strd, *(pu1_left - 1), 4);
    memset(pu1_dst + 2 * dst_strd, *(pu1_left - 2), 4);
    memset(pu1_dst + 3 * dst_strd, *(pu1_left - 3), 4);
}

/**
 *******************************************************************************
 *
 *ih264_intra_pred_luma_4x4_mode_dc
 *
 * @brief
 *  Perform Intra prediction for  luma_4x4 mode:DC
 *
 * @par Description:
 *  Perform Intra prediction for  luma_4x4 mode:DC ,described in sec 8.3.1.2.3
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
 * @param[in] ngbr_avail
 *  availability of neighbouring pixels
 *
 * @returns
 *
 * @remarks
 *  None
 *
 *******************************************************************************/
void ih264_intra_pred_luma_4x4_mode_dc(UWORD8 *pu1_src,
                                       UWORD8 *pu1_dst,
                                       WORD32 src_strd,
                                       WORD32 dst_strd,
                                       WORD32 ngbr_avail)
{
    UWORD8 u1_useleft; /* availability of left predictors (only for DC) */
    UWORD8 u1_usetop; /* availability of top predictors (only for DC) */
    UWORD8 *pu1_left = NULL; /* Pointer to start of left predictors */
    UWORD8 *pu1_top = NULL; /* Pointer to start of top predictors */
    WORD32 val = 0;
    UNUSED(src_strd);
    UNUSED(ngbr_avail);
    u1_useleft = BOOLEAN(ngbr_avail & LEFT_MB_AVAILABLE_MASK);
    u1_usetop = BOOLEAN(ngbr_avail & TOP_MB_AVAILABLE_MASK);
    pu1_top = pu1_src + BLK_SIZE + 1;
    pu1_left = pu1_src + BLK_SIZE - 1;

    if(u1_useleft)
    {
        val += *pu1_left--;
        val += *pu1_left--;
        val += *pu1_left--;
        val += *pu1_left + 2;
    }
    if(u1_usetop)
    {
        val += *pu1_top + *(pu1_top + 1) + *(pu1_top + 2) + *(pu1_top + 3)
                        + 2;
    }
    /* Since 2 is added if either left/top pred is there,
     val still being zero implies both preds are not there */
    val = (val) ? (val >> (1 + u1_useleft + u1_usetop)) : 128;

    /* 4 bytes are copied from src to dst */
    memset(pu1_dst, val, 4);
    memset(pu1_dst + dst_strd, val, 4);
    memset(pu1_dst + 2 * dst_strd, val, 4);
    memset(pu1_dst + 3 * dst_strd, val, 4);
}

/**
 *******************************************************************************
 *
 *ih264_intra_pred_luma_4x4_mode_diag_dl
 *
 * @brief
 *     Perform Intra prediction for  luma_4x4 mode:Diagonal_Down_Left
 *
 * @par Description:
 *    Perform Intra prediction for  luma_4x4 mode:Diagonal_Down_Left ,described in sec 8.3.1.2.4
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
 * @param[in] ngbr_avail
 * availability of neighbouring pixels(Not used in this function)
 *
 * @returns
 *
 * @remarks
 *  None
 *
 *******************************************************************************/
void ih264_intra_pred_luma_4x4_mode_diag_dl(UWORD8 *pu1_src,
                                            UWORD8 *pu1_dst,
                                            WORD32 src_strd,
                                            WORD32 dst_strd,
                                            WORD32 ngbr_avail)
{
    UWORD8 *pu1_top = NULL; /* Pointer to start of top predictors */
    UWORD32 ui4_a, ui4_b, ui4_c, ui4_d, ui4_e, ui4_f, ui4_g, ui4_h;
    UWORD8 predicted_pixels[7];
    UNUSED(src_strd);
    UNUSED(ngbr_avail);
    pu1_top = pu1_src +BLK_SIZE + 1;

    ui4_a = *pu1_top++;
    ui4_b = *pu1_top++;
    ui4_c = *pu1_top++;
    ui4_d = *pu1_top++;
    ui4_e = *pu1_top++;
    ui4_f = *pu1_top++;
    ui4_g = *pu1_top++;
    ui4_h = *pu1_top;

    predicted_pixels[0] = FILT121(ui4_a, ui4_b, ui4_c);
    predicted_pixels[1] = FILT121(ui4_b, ui4_c, ui4_d);
    predicted_pixels[2] = FILT121(ui4_c, ui4_d, ui4_e);
    predicted_pixels[3] = FILT121(ui4_d, ui4_e, ui4_f);
    predicted_pixels[4] = FILT121(ui4_e, ui4_f, ui4_g);
    predicted_pixels[5] = FILT121(ui4_f, ui4_g, ui4_h);
    predicted_pixels[6] = FILT121(ui4_g, ui4_h, ui4_h);

    memcpy(pu1_dst, predicted_pixels, 4);
    memcpy(pu1_dst + dst_strd, predicted_pixels + 1, 4);
    memcpy(pu1_dst + 2 * dst_strd, predicted_pixels + 2, 4);
    memcpy(pu1_dst + 3 * dst_strd, predicted_pixels + 3, 4);
}

/**
 *******************************************************************************
 *
 *ih264_intra_pred_luma_4x4_mode_diag_dr
 *
 * @brief
 *     Perform Intra prediction for  luma_4x4 mode:Diagonal_Down_Right
 *
 * @par Description:
 *    Perform Intra prediction for  luma_4x4 mode:Diagonal_Down_Right ,described in sec 8.3.1.2.5
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
 * @param[in] ngbr_avail
 * availability of neighbouring pixels(Not used in this function)
 *
 * @returns
 *
 * @remarks
 *  None
 *
 *******************************************************************************/
void ih264_intra_pred_luma_4x4_mode_diag_dr(UWORD8 *pu1_src,
                                            UWORD8 *pu1_dst,
                                            WORD32 src_strd,
                                            WORD32 dst_strd,
                                            WORD32 ngbr_avail)
{
    UWORD8 *pu1_top = NULL; /* Pointer to start of top predictors */
    UWORD8 *pu1_left = NULL; /* Pointer to start of left predictors */
    UWORD8 *pu1_topleft = NULL;/* Pointer to top left predictor */
    UWORD32 ui4_a, ui4_b, ui4_c, ui4_d, ui4_i, ui4_j, ui4_k, ui4_l, ui4_m;
    UWORD8 predicted_pixels[7];
    UNUSED(src_strd);
    UNUSED(ngbr_avail);
    pu1_top = pu1_src + BLK_SIZE + 1;
    pu1_left = pu1_src + BLK_SIZE - 1;
    pu1_topleft = pu1_src +BLK_SIZE;

    ui4_a = *pu1_top++;
    ui4_b = *pu1_top++;
    ui4_c = *pu1_top++;
    ui4_d = *pu1_top++;
    ui4_i = *pu1_left--;
    ui4_j = *pu1_left--;
    ui4_k = *pu1_left--;
    ui4_l = *pu1_left;
    ui4_m = *pu1_topleft;

    predicted_pixels[2] = FILT121(ui4_j, ui4_i, ui4_m);
    predicted_pixels[1] = FILT121(ui4_k, ui4_j, ui4_i);
    predicted_pixels[0] = FILT121(ui4_l, ui4_k, ui4_j);
    predicted_pixels[3] = FILT121(ui4_i, ui4_m, ui4_a);
    predicted_pixels[4] = FILT121(ui4_m, ui4_a, ui4_b);
    predicted_pixels[5] = FILT121(ui4_a, ui4_b, ui4_c);
    predicted_pixels[6] = FILT121(ui4_b, ui4_c, ui4_d);

    memcpy(pu1_dst, predicted_pixels + 3, 4);
    memcpy(pu1_dst + dst_strd, predicted_pixels + 2, 4);
    memcpy(pu1_dst + 2 * dst_strd, predicted_pixels + 1, 4);
    memcpy(pu1_dst + 3 * dst_strd, predicted_pixels, 4);
}

/**
 *******************************************************************************
 *
 *ih264_intra_pred_luma_4x4_mode_vert_r
 *
 * @brief
 *     Perform Intra prediction for  luma_4x4 mode:Vertical_Right
 *
 * @par Description:
 *    Perform Intra prediction for  luma_4x4 mode:Vertical_Right ,described in sec 8.3.1.2.6
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
 * @param[in] ngbr_avail
 * availability of neighbouring pixels(Not used in this function)
 *
 * @returns
 *
 * @remarks
 *  None
 *
 *******************************************************************************/
void ih264_intra_pred_luma_4x4_mode_vert_r(UWORD8 *pu1_src,
                                           UWORD8 *pu1_dst,
                                           WORD32 src_strd,
                                           WORD32 dst_strd,
                                           WORD32 ngbr_avail)
{

    UWORD32 ui4_a, ui4_b, ui4_c, ui4_d, ui4_i, ui4_j, ui4_k, ui4_m;
    UWORD8 *pu1_top = NULL; /* Pointer to start of top predictors */
    UWORD8 *pu1_left = NULL; /* Pointer to start of left predictors */
    UWORD8 *pu1_topleft = NULL;/* Pointer to top left predictor */
    UWORD8 predicted_pixels[10];
    UNUSED(src_strd);
    UNUSED(ngbr_avail);
    pu1_top = pu1_src +BLK_SIZE + 1;
    pu1_left = pu1_src + BLK_SIZE - 1;
    pu1_topleft = pu1_src + BLK_SIZE;

    ui4_a = *pu1_top++;
    ui4_b = *pu1_top++;
    ui4_c = *pu1_top++;
    ui4_d = *pu1_top++;
    ui4_i = *pu1_left--;
    ui4_j = *pu1_left--;
    ui4_k = *pu1_left;
    ui4_m = *pu1_topleft;

    predicted_pixels[6] = FILT11(ui4_m, ui4_a);
    predicted_pixels[7] = FILT11(ui4_a, ui4_b);
    predicted_pixels[8] = FILT11(ui4_b, ui4_c);
    predicted_pixels[9] = FILT11(ui4_c, ui4_d);
    predicted_pixels[1] = FILT121(ui4_i, ui4_m, ui4_a);
    predicted_pixels[2] = FILT121(ui4_m, ui4_a, ui4_b);
    predicted_pixels[3] = FILT121(ui4_a, ui4_b, ui4_c);
    predicted_pixels[4] = FILT121(ui4_b, ui4_c, ui4_d);
    predicted_pixels[5] = FILT121(ui4_j, ui4_i, ui4_m);
    predicted_pixels[0] = FILT121(ui4_k, ui4_j, ui4_i);

    memcpy(pu1_dst, predicted_pixels + 6, 4);
    memcpy(pu1_dst + dst_strd, predicted_pixels + 1, 4);
    memcpy(pu1_dst + 2 * dst_strd, predicted_pixels + 5, 4);
    memcpy(pu1_dst + 3 * dst_strd, predicted_pixels, 4);
}

/*
 *******************************************************************************
 *
 *ih264_intra_pred_luma_4x4_mode_horz_d
 *
 * @brief
 *     Perform Intra prediction for  luma_4x4 mode:Horizontal_Down
 *
 * @par Description:
 *    Perform Intra prediction for  luma_4x4 mode:Horizontal_Down ,described in sec 8.3.1.2.7
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
 * @param[in] ngbr_avail
 * availability of neighbouring pixels(Not used in this function)
 *
 * @returns
 *
 * @remarks
 *  None
 *
 *******************************************************************************/
void ih264_intra_pred_luma_4x4_mode_horz_d(UWORD8 *pu1_src,
                                           UWORD8 *pu1_dst,
                                           WORD32 src_strd,
                                           WORD32 dst_strd,
                                           WORD32 ngbr_avail)
{
    UWORD8 *pu1_top = NULL; /* Pointer to start of top predictors */
    UWORD8 *pu1_left = NULL; /* Pointer to start of left predictors */
    UWORD8 *pu1_topleft = NULL;/* Pointer to top left predictor */
    UWORD32 ui4_a, ui4_b, ui4_c, ui4_i, ui4_j, ui4_k, ui4_l, ui4_m;
    UWORD8 predicted_pixels[10];
    UNUSED(src_strd);
    UNUSED(ngbr_avail);
    pu1_top = pu1_src + BLK_SIZE + 1;
    pu1_left = pu1_src + BLK_SIZE - 1;
    pu1_topleft = pu1_src + BLK_SIZE;

    ui4_a = *pu1_top++;
    ui4_b = *pu1_top++;
    ui4_c = *pu1_top++;
    ui4_i = *pu1_left--;
    ui4_j = *pu1_left--;
    ui4_k = *pu1_left--;
    ui4_l = *pu1_left--;
    ui4_m = *pu1_topleft;

    predicted_pixels[6] = FILT11(ui4_i, ui4_m);
    predicted_pixels[7] = FILT121(ui4_i, ui4_m, ui4_a);
    predicted_pixels[8] = FILT121(ui4_m, ui4_a, ui4_b);
    predicted_pixels[9] = FILT121(ui4_a, ui4_b, ui4_c);
    predicted_pixels[1] = FILT121(ui4_l, ui4_k, ui4_j);
    predicted_pixels[2] = FILT11(ui4_k, ui4_j);
    predicted_pixels[3] = FILT121(ui4_k, ui4_j, ui4_i);
    predicted_pixels[4] = FILT11(ui4_j, ui4_i);
    predicted_pixels[5] = FILT121(ui4_j, ui4_i, ui4_m);
    predicted_pixels[0] = FILT11(ui4_l, ui4_k);

    memcpy(pu1_dst, predicted_pixels + 6, 4);
    memcpy(pu1_dst + dst_strd, predicted_pixels + 4, 4);
    memcpy(pu1_dst + 2 * dst_strd, predicted_pixels + 2, 4);
    memcpy(pu1_dst + 3 * dst_strd, predicted_pixels, 4);
}

/**
 *******************************************************************************
 *
 *ih264_intra_pred_luma_4x4_mode_vert_l
 *
 * @brief
 *     Perform Intra prediction for  luma_4x4 mode:Vertical_Left
 *
 * @par Description:
 *    Perform Intra prediction for  luma_4x4 mode:Vertical_Left ,described in sec 8.3.1.2.8
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
 * @param[in] ngbr_avail
 * availability of neighbouring pixels(Not used in this function)
 *
 * @returns
 *
 * @remarks
 *  None
 *
 *******************************************************************************/
void ih264_intra_pred_luma_4x4_mode_vert_l(UWORD8 *pu1_src,
                                           UWORD8 *pu1_dst,
                                           WORD32 src_strd,
                                           WORD32 dst_strd,
                                           WORD32 ngbr_avail)
{
    UWORD8 *pu1_top = NULL; /* Pointer to start of top predictors */
    UWORD32 ui4_a, ui4_b, ui4_c, ui4_d, ui4_e, ui4_f, ui4_g;
    UWORD8 predicted_pixels[10];
    UNUSED(src_strd);
    UNUSED(ngbr_avail);
    pu1_top = pu1_src + BLK_SIZE + 1;

    ui4_a = *pu1_top++;
    ui4_b = *pu1_top++;
    ui4_c = *pu1_top++;
    ui4_d = *pu1_top++;
    ui4_e = *pu1_top++;
    ui4_f = *pu1_top++;
    ui4_g = *pu1_top;

    predicted_pixels[5] = FILT11(ui4_a, ui4_b);
    predicted_pixels[6] = FILT11(ui4_b, ui4_c);
    predicted_pixels[7] = FILT11(ui4_c, ui4_d);
    predicted_pixels[8] = FILT11(ui4_d, ui4_e);
    predicted_pixels[0] = FILT121(ui4_a, ui4_b, ui4_c);
    predicted_pixels[1] = FILT121(ui4_b, ui4_c, ui4_d);
    predicted_pixels[2] = FILT121(ui4_c, ui4_d, ui4_e);
    predicted_pixels[3] = FILT121(ui4_d, ui4_e, ui4_f);
    predicted_pixels[9] = FILT11(ui4_e, ui4_f);
    predicted_pixels[4] = FILT121(ui4_e, ui4_f, ui4_g);

    memcpy(pu1_dst, predicted_pixels + 5, 4);
    memcpy(pu1_dst + dst_strd, predicted_pixels, 4);
    memcpy(pu1_dst + 2 * dst_strd, predicted_pixels + 6, 4);
    memcpy(pu1_dst + 3 * dst_strd, predicted_pixels + 1, 4);
}

/**
 *******************************************************************************
 *
 *ih264_intra_pred_luma_4x4_mode_horz_u
 *
 * @brief
 *     Perform Intra prediction for  luma_4x4 mode:Horizontal_Up
 *
 * @par Description:
 *    Perform Intra prediction for  luma_4x4 mode:Horizontal_Up ,described in sec 8.3.1.2.9
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
 * @param[in] ngbr_avail
 * availability of neighbouring pixels(Not used in this function)
 *
 * @returns
 *
 * @remarks
 *  None
 *
 *******************************************************************************/
void ih264_intra_pred_luma_4x4_mode_horz_u(UWORD8 *pu1_src,
                                           UWORD8 *pu1_dst,
                                           WORD32 src_strd,
                                           WORD32 dst_strd,
                                           WORD32 ngbr_avail)
{
    UWORD8 *pu1_left = NULL; /* Pointer to start of left predictors */
    UWORD32 ui4_i, ui4_j, ui4_k, ui4_l;
    UWORD8 predicted_pixels[10];
    UNUSED(src_strd);
    UNUSED(ngbr_avail);
    pu1_left = pu1_src + BLK_SIZE - 1;

    ui4_i = *pu1_left--;
    ui4_j = *pu1_left--;
    ui4_k = *pu1_left--;
    ui4_l = *pu1_left--;

    predicted_pixels[0] = FILT11(ui4_j, ui4_i);
    predicted_pixels[1] = FILT121(ui4_k, ui4_j, ui4_i);
    predicted_pixels[2] = FILT11(ui4_k, ui4_j);
    predicted_pixels[3] = FILT121(ui4_l, ui4_k, ui4_j);
    predicted_pixels[4] = FILT11(ui4_l, ui4_k);
    predicted_pixels[5] = FILT121(ui4_l, ui4_l, ui4_k);
    predicted_pixels[6] = ui4_l;
    predicted_pixels[7] = ui4_l;
    predicted_pixels[8] = ui4_l;
    predicted_pixels[9] = ui4_l;

    memcpy(pu1_dst, predicted_pixels, 4);
    memcpy(pu1_dst + dst_strd, predicted_pixels + 2, 4);
    memcpy(pu1_dst + 2 * dst_strd, predicted_pixels + 4, 4);
    memcpy(pu1_dst + 3 * dst_strd, predicted_pixels + 6, 4);
}

/*******************    8x8 Modes    *******************/

/**
 *******************************************************************************
 *
 *ih264_intra_pred_luma_8x8_mode_ref_filtering
 *
 * @brief
 *     Reference sample filtering process for Intra_8x8 sample prediction
 *
 * @par Description:
 *    Perform Reference sample filtering process for Intra_8x8 sample prediction ,described in sec 8.3.2.2.1
 *
 * @param[in] pu1_src
 *  UWORD8 pointer to the source
 *
 * @param[out] pu1_dst
 *  UWORD8 pointer to the destination
 *
 * @param[in] src_strd
 *  integer source stride[Not Used]
 *
 * @param[in] dst_strd
 *  integer destination stride[Not Used]
 *
 * @param[in] ngbr_avail
 * availability of neighbouring pixels(Not used in this function)
 *
 * @returns
 *
 * @remarks
 *  None
 *
 *
 *******************************************************************************/
void ih264_intra_pred_luma_8x8_mode_ref_filtering(UWORD8 *pu1_left,
                                                  UWORD8 *pu1_topleft,
                                                  UWORD8 *pu1_top,
                                                  UWORD8 *pu1_dst,
                                                  WORD32 left_strd,
                                                  WORD32 ngbr_avail)
{
    WORD32 top_avail, left_avail, top_left_avail, top_right_avail;

    left_avail = BOOLEAN(ngbr_avail & LEFT_MB_AVAILABLE_MASK);
    top_avail = BOOLEAN(ngbr_avail & TOP_MB_AVAILABLE_MASK);
    top_left_avail = BOOLEAN(ngbr_avail & TOP_LEFT_MB_AVAILABLE_MASK);
    top_right_avail = BOOLEAN(ngbr_avail & TOP_RIGHT_MB_AVAILABLE_MASK);

    if(top_avail)
    {
        WORD32 i;
        UWORD32 u4_xm1;

        if(!top_right_avail)
        {
            memset(pu1_dst + 8 + 1 + 8, pu1_top[7], 8);
            top_right_avail = 1;
        }
        else
        {
            memcpy(pu1_dst + 8 + 1 + 8, pu1_top + 8, 8);
        }

        if(top_left_avail)
        {
            pu1_dst[8 + 1 + 0] = FILT121((*pu1_topleft), pu1_top[0],
                                         pu1_top[1]);

        }
        else
        {
            pu1_dst[8 + 1] = ((3 * pu1_top[0]) + pu1_top[1] + 2) >> 2;
        }

        for(i = 1; i <= 6; i++)
        {
            pu1_dst[8 + 1 + i] = FILT121(pu1_top[i - 1], pu1_top[i],
                                         pu1_top[i + 1]);

        }
        /* First byte of Top Right input is in pu1_dst[8 + 1 + 8]*/
        pu1_dst[8 + 1 + 7] = FILT121(pu1_top[6], pu1_top[7],
                                     pu1_dst[8 + 1 + 8]);

        /* filtered output and source in same buf, to prevent output(x - 1)
         being over written in process */
        u4_xm1 = pu1_top[7];

        for(i = 8; i <= 14; i++)
        {
            UWORD32 u4_x;
            u4_x = (u4_xm1 + (pu1_dst[8 + 1 + i] << 1) + pu1_dst[8 + 1 + i + 1]
                            + 2) >> 2;
            /* assigning u4_xm1 from the un-filtered values for the next iteration */
            u4_xm1 = pu1_dst[8 + 1 + i];
            pu1_dst[8 + 1 + i] = u4_x;
        }

        pu1_dst[8 + 1 + 15] = (u4_xm1 + (3 * pu1_dst[8 + 1 + 15]) + 2) >> 2;

    }

    /* pu1_topleft is overloaded. It is both: */
    /* a. A pointer for the top left pixel */
    /* b. An indicator of availability of top left. */
    /*    If it is null then top left not available */
    if(top_left_avail)
    {
        if((!top_avail) || (!left_avail))
        {
            if(top_avail)
                pu1_dst[8] = (3 * pu1_topleft[0] + pu1_top[0] + 2) >> 2;
            else if(left_avail)
                pu1_dst[8] = (3 * pu1_topleft[0] + pu1_left[0] + 2) >> 2;
        }
        else
        {
            pu1_dst[8] = FILT121(pu1_top[0], (*pu1_topleft), pu1_left[0]);
        }
    }

    if(left_avail)
    {
        UWORD32 idx;
        if(0 != pu1_topleft)
        {
            pu1_dst[7] = FILT121((*pu1_topleft), pu1_left[0],
                                 pu1_left[left_strd]);
        }
        else
        {
            pu1_dst[7] = ((3 * pu1_left[0]) + pu1_left[left_strd] + 2) >> 2;
        }

        for(idx = 1; idx <= 6; idx++)
        {
            pu1_dst[7 - idx] = FILT121(pu1_left[(idx - 1) * left_strd],
                                       pu1_left[idx * left_strd],
                                       pu1_left[(idx + 1) * left_strd]);

        }
        pu1_dst[0] = (pu1_left[6 * left_strd] + 3 * pu1_left[7 * left_strd] + 2)
                        >> 2;

    }
}

/**
 *******************************************************************************
 *
 *ih264_intra_pred_luma_8x8_mode_vert
 *
 * @brief
 *  Perform Intra prediction for  luma_8x8 mode:vertical
 *
 * @par Description:
 *  Perform Intra prediction for  luma_8x8 mode:vertical ,described in sec 8.3.2.2.2
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
void ih264_intra_pred_luma_8x8_mode_vert(UWORD8 *pu1_src,
                                         UWORD8 *pu1_dst,
                                         WORD32 src_strd,
                                         WORD32 dst_strd,
                                         WORD32 ngbr_avail)
{
    UWORD8 *pu1_top = NULL;
    UNUSED(src_strd);
    UNUSED(ngbr_avail);
    pu1_top = pu1_src + BLK8x8SIZE + 1;

    memcpy(pu1_dst, pu1_top, 8);
    memcpy(pu1_dst + dst_strd, pu1_top, 8);
    memcpy(pu1_dst + 2 * dst_strd, pu1_top, 8);
    memcpy(pu1_dst + 3 * dst_strd, pu1_top, 8);
    memcpy(pu1_dst + 4 * dst_strd, pu1_top, 8);
    memcpy(pu1_dst + 5 * dst_strd, pu1_top, 8);
    memcpy(pu1_dst + 6 * dst_strd, pu1_top, 8);
    memcpy(pu1_dst + 7 * dst_strd, pu1_top, 8);
}

/**
 *******************************************************************************
 *
 *ih264_intra_pred_luma_8x8_mode_horz
 *
 * @brief
 *  Perform Intra prediction for  luma_8x8 mode:horizontal
 *
 * @par Description:
 *  Perform Intra prediction for  luma_8x8 mode:horizontal ,described in sec 8.3.2.2.2
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

void ih264_intra_pred_luma_8x8_mode_horz(UWORD8 *pu1_src,
                                         UWORD8 *pu1_dst,
                                         WORD32 src_strd,
                                         WORD32 dst_strd,
                                         WORD32 ngbr_avail)
{
    UWORD8 *pu1_left = pu1_src + BLK8x8SIZE - 1;
    UNUSED(src_strd);
    UNUSED(ngbr_avail);
    memset(pu1_dst, *pu1_left, 8);
    memset(pu1_dst + dst_strd, *(pu1_left - 1), 8);
    memset(pu1_dst + 2 * dst_strd, *(pu1_left - 2), 8);
    memset(pu1_dst + 3 * dst_strd, *(pu1_left - 3), 8);
    memset(pu1_dst + 4 * dst_strd, *(pu1_left - 4), 8);
    memset(pu1_dst + 5 * dst_strd, *(pu1_left - 5), 8);
    memset(pu1_dst + 6 * dst_strd, *(pu1_left - 6), 8);
    memset(pu1_dst + 7 * dst_strd, *(pu1_left - 7), 8);
}

/**
 *******************************************************************************
 *
 *ih264_intra_pred_luma_8x8_mode_dc
 *
 * @brief
 *     Perform Intra prediction for  luma_8x8 mode:DC
 *
 * @par Description:
 *    Perform Intra prediction for  luma_8x8 mode:DC ,described in sec 8.3.2.2.4
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
 * @param[in] ngbr_avail
 *  availability of neighbouring pixels
 *
 * @returns
 *
 * @remarks
 *  None
 *
 *******************************************************************************/
void ih264_intra_pred_luma_8x8_mode_dc(UWORD8 *pu1_src,
                                       UWORD8 *pu1_dst,
                                       WORD32 src_strd,
                                       WORD32 dst_strd,
                                       WORD32 ngbr_avail)
{
    UWORD8 u1_useleft; /* availability of left predictors (only for DC) */
    UWORD8 u1_usetop; /* availability of top predictors (only for DC) */
    UWORD8 *pu1_left = NULL; /* Pointer to start of left predictors */
    UWORD8 *pu1_top = NULL; /* Pointer to start of top predictors */
    WORD32 row;
    WORD32 val = 0;
    UNUSED(src_strd);

    u1_useleft = BOOLEAN(ngbr_avail & LEFT_MB_AVAILABLE_MASK);
    u1_usetop = BOOLEAN(ngbr_avail & TOP_MB_AVAILABLE_MASK);
    pu1_top = pu1_src + BLK8x8SIZE + 1;
    pu1_left = pu1_src + BLK8x8SIZE - 1;

    if(u1_useleft)
    {
        for(row = 0; row < BLK8x8SIZE; row++)
            val += *(pu1_left - row);
        val += 4;
    }
    if(u1_usetop)
    {
        for(row = 0; row < BLK8x8SIZE; row++)
            val += *(pu1_top + row);
        val += 4;
    }

    /* Since 4 is added if either left/top pred is there,
     val still being zero implies both preds are not there */
    val = (val) ? (val >> (2 + u1_useleft + u1_usetop)) : 128;

    memset(pu1_dst, val, 8);
    memset(pu1_dst + dst_strd, val, 8);
    memset(pu1_dst + 2 * dst_strd, val, 8);
    memset(pu1_dst + 3 * dst_strd, val, 8);
    memset(pu1_dst + 4 * dst_strd, val, 8);
    memset(pu1_dst + 5 * dst_strd, val, 8);
    memset(pu1_dst + 6 * dst_strd, val, 8);
    memset(pu1_dst + 7 * dst_strd, val, 8);
}

/**
 *******************************************************************************
 *
 *ih264_intra_pred_luma_8x8_mode_diag_dl
 *
 * @brief
 *     Perform Intra prediction for  luma_8x8 mode:Diagonal_Down_Left
 *
 * @par Description:
 *    Perform Intra prediction for  luma_8x8 mode:Diagonal_Down_Left ,described in sec 8.3.2.2.5
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
 * @param[in] ngbr_avail
 * availability of neighbouring pixels(Not used in this function)
 *
 * @returns
 *
 * @remarks
 *  None
 *
 *******************************************************************************/
void ih264_intra_pred_luma_8x8_mode_diag_dl(UWORD8 *pu1_src,
                                            UWORD8 *pu1_dst,
                                            WORD32 src_strd,
                                            WORD32 dst_strd,
                                            WORD32 ngbr_avail)
{
    UWORD8 *pu1_top = NULL; /* Pointer to start of top predictors */
    UWORD32 ui4_a, ui4_b, ui4_c, ui4_d, ui4_e, ui4_f, ui4_g, ui4_h;
    UWORD32 ui4_i, ui4_j, ui4_k, ui4_l, ui4_m, ui4_n, ui4_o, ui4_p;
    UWORD8 predicted_pixels[15];
    UNUSED(src_strd);
    UNUSED(ngbr_avail);
    pu1_top = pu1_src + BLK8x8SIZE + 1;

    ui4_a = *pu1_top++;
    ui4_b = *pu1_top++;
    ui4_c = *pu1_top++;
    ui4_d = *pu1_top++;
    ui4_e = *pu1_top++;
    ui4_f = *pu1_top++;
    ui4_g = *pu1_top++;
    ui4_h = *pu1_top++;
    ui4_i = *pu1_top++;
    ui4_j = *pu1_top++;
    ui4_k = *pu1_top++;
    ui4_l = *pu1_top++;
    ui4_m = *pu1_top++;
    ui4_n = *pu1_top++;
    ui4_o = *pu1_top++;
    ui4_p = *pu1_top;

    predicted_pixels[0] = FILT121(ui4_a, ui4_b, ui4_c);
    predicted_pixels[1] = FILT121(ui4_b, ui4_c, ui4_d);
    predicted_pixels[2] = FILT121(ui4_c, ui4_d, ui4_e);
    predicted_pixels[3] = FILT121(ui4_d, ui4_e, ui4_f);
    predicted_pixels[4] = FILT121(ui4_e, ui4_f, ui4_g);
    predicted_pixels[5] = FILT121(ui4_f, ui4_g, ui4_h);
    predicted_pixels[6] = FILT121(ui4_g, ui4_h, ui4_i);
    predicted_pixels[7] = FILT121(ui4_h, ui4_i, ui4_j);
    predicted_pixels[8] = FILT121(ui4_i, ui4_j, ui4_k);
    predicted_pixels[9] = FILT121(ui4_j, ui4_k, ui4_l);
    predicted_pixels[10] = FILT121(ui4_k, ui4_l, ui4_m);
    predicted_pixels[11] = FILT121(ui4_l, ui4_m, ui4_n);
    predicted_pixels[12] = FILT121(ui4_m, ui4_n, ui4_o);
    predicted_pixels[13] = FILT121(ui4_n, ui4_o, ui4_p);
    predicted_pixels[14] = FILT121(ui4_o, ui4_p, ui4_p);

    memcpy(pu1_dst, predicted_pixels, 8);
    memcpy(pu1_dst + dst_strd, predicted_pixels + 1, 8);
    memcpy(pu1_dst + 2 * dst_strd, predicted_pixels + 2, 8);
    memcpy(pu1_dst + 3 * dst_strd, predicted_pixels + 3, 8);
    memcpy(pu1_dst + 4 * dst_strd, predicted_pixels + 4, 8);
    memcpy(pu1_dst + 5 * dst_strd, predicted_pixels + 5, 8);
    memcpy(pu1_dst + 6 * dst_strd, predicted_pixels + 6, 8);
    memcpy(pu1_dst + 7 * dst_strd, predicted_pixels + 7, 8);
}

/**
 *******************************************************************************
 *
 *ih264_intra_pred_luma_8x8_mode_diag_dr
 *
 * @brief
 *     Perform Intra prediction for  luma_8x8 mode:Diagonal_Down_Right
 *
 * @par Description:
 *    Perform Intra prediction for  luma_8x8 mode:Diagonal_Down_Right ,described in sec 8.3.2.2.6
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
 * @param[in] ngbr_avail
 * availability of neighbouring pixels(Not used in this function)
 *
 * @returns
 *
 * @remarks
 *  None
 *
 *******************************************************************************/
void ih264_intra_pred_luma_8x8_mode_diag_dr(UWORD8 *pu1_src,
                                            UWORD8 *pu1_dst,
                                            WORD32 src_strd,
                                            WORD32 dst_strd,
                                            WORD32 ngbr_avail)
{
    UWORD8 *pu1_left = NULL; /* Pointer to start of left predictors */
    UWORD8 *pu1_top = NULL; /* Pointer to start of top predictors */
    UWORD8 *pu1_topleft = NULL; /* Pointer to start of top left predictors */
    UWORD32 ui4_a;
    UWORD32 ui4_b, ui4_c, ui4_d, ui4_e, ui4_f, ui4_g, ui4_h, ui4_i;
    UWORD32 ui4_j, ui4_k, ui4_l, ui4_m, ui4_n, ui4_o, ui4_p, ui4_q;
    UWORD8 predicted_pixels[15];
    UNUSED(src_strd);
    UNUSED(ngbr_avail);
    pu1_top = pu1_src + BLK8x8SIZE + 1;
    pu1_left = pu1_src + BLK8x8SIZE - 1;
    pu1_topleft = pu1_src + BLK8x8SIZE;

    ui4_a = *pu1_topleft;
    ui4_b = *pu1_top++;
    ui4_c = *pu1_top++;
    ui4_d = *pu1_top++;
    ui4_e = *pu1_top++;
    ui4_f = *pu1_top++;
    ui4_g = *pu1_top++;
    ui4_h = *pu1_top++;
    ui4_i = *pu1_top;
    ui4_j = *pu1_left--;
    ui4_k = *pu1_left--;
    ui4_l = *pu1_left--;
    ui4_m = *pu1_left--;
    ui4_n = *pu1_left--;
    ui4_o = *pu1_left--;
    ui4_p = *pu1_left--;
    ui4_q = *pu1_left;

    predicted_pixels[6] = FILT121(ui4_a, ui4_j, ui4_k);
    predicted_pixels[5] = FILT121(ui4_j, ui4_k, ui4_l);
    predicted_pixels[4] = FILT121(ui4_k, ui4_l, ui4_m);
    predicted_pixels[3] = FILT121(ui4_l, ui4_m, ui4_n);
    predicted_pixels[2] = FILT121(ui4_m, ui4_n, ui4_o);
    predicted_pixels[1] = FILT121(ui4_n, ui4_o, ui4_p);
    predicted_pixels[0] = FILT121(ui4_o, ui4_p, ui4_q);
    predicted_pixels[7] = FILT121(ui4_b, ui4_a, ui4_j);
    predicted_pixels[8] = FILT121(ui4_a, ui4_b, ui4_c);
    predicted_pixels[9] = FILT121(ui4_b, ui4_c, ui4_d);
    predicted_pixels[10] = FILT121(ui4_c, ui4_d, ui4_e);
    predicted_pixels[11] = FILT121(ui4_d, ui4_e, ui4_f);
    predicted_pixels[12] = FILT121(ui4_e, ui4_f, ui4_g);
    predicted_pixels[13] = FILT121(ui4_f, ui4_g, ui4_h);
    predicted_pixels[14] = FILT121(ui4_g, ui4_h, ui4_i);

    memcpy(pu1_dst, predicted_pixels + 7, 8);
    memcpy(pu1_dst + dst_strd, predicted_pixels + 6, 8);
    memcpy(pu1_dst + 2 * dst_strd, predicted_pixels + 5, 8);
    memcpy(pu1_dst + 3 * dst_strd, predicted_pixels + 4, 8);
    memcpy(pu1_dst + 4 * dst_strd, predicted_pixels + 3, 8);
    memcpy(pu1_dst + 5 * dst_strd, predicted_pixels + 2, 8);
    memcpy(pu1_dst + 6 * dst_strd, predicted_pixels + 1, 8);
    memcpy(pu1_dst + 7 * dst_strd, predicted_pixels, 8);
}

/**
 *******************************************************************************
 *
 *ih264_intra_pred_luma_8x8_mode_vert_r
 *
 * @brief
 *     Perform Intra prediction for  luma_8x8 mode:Vertical_Right
 *
 * @par Description:
 *    Perform Intra prediction for  luma_8x8 mode:Vertical_Right ,described in sec 8.3.2.2.7
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
 * @param[in] ngbr_avail
 * availability of neighbouring pixels(Not used in this function)
 *
 * @returns
 *
 * @remarks
 *  None
 *
 *******************************************************************************/
void ih264_intra_pred_luma_8x8_mode_vert_r(UWORD8 *pu1_src,
                                           UWORD8 *pu1_dst,
                                           WORD32 src_strd,
                                           WORD32 dst_strd,
                                           WORD32 ngbr_avail)
{
    UWORD8 *pu1_left = NULL; /* Pointer to start of left predictors */
    UWORD8 *pu1_top = NULL; /* Pointer to start of top predictors */
    UWORD8 *pu1_topleft = NULL; /* Pointer to start of top left predictors */
    UWORD32 ui4_a;
    UWORD32 ui4_b, ui4_c, ui4_d, ui4_e, ui4_f, ui4_g, ui4_h, ui4_i;
    UWORD32 ui4_j, ui4_k, ui4_l, ui4_m, ui4_n, ui4_o, ui4_p;
    UWORD8 predicted_pixels[22];

    UNUSED(src_strd);
    UNUSED(ngbr_avail);
    pu1_top = pu1_src + BLK8x8SIZE + 1;
    pu1_left = pu1_src + BLK8x8SIZE - 1;
    pu1_topleft = pu1_src + BLK8x8SIZE;

    ui4_a = *pu1_topleft;

    ui4_b = *pu1_top++;
    ui4_c = *pu1_top++;
    ui4_d = *pu1_top++;
    ui4_e = *pu1_top++;
    ui4_f = *pu1_top++;
    ui4_g = *pu1_top++;
    ui4_h = *pu1_top++;
    ui4_i = *pu1_top;
    ui4_j = *pu1_left--;
    ui4_k = *pu1_left--;
    ui4_l = *pu1_left--;
    ui4_m = *pu1_left--;
    ui4_n = *pu1_left--;
    ui4_o = *pu1_left--;
    ui4_p = *pu1_left--;

    predicted_pixels[0] = FILT121(ui4_o, ui4_n, ui4_m);
    predicted_pixels[1] = FILT121(ui4_m, ui4_l, ui4_k);
    predicted_pixels[2] = FILT121(ui4_k, ui4_j, ui4_a);
    predicted_pixels[3] = FILT11(ui4_a, ui4_b);
    predicted_pixels[4] = FILT11(ui4_b, ui4_c);
    predicted_pixels[5] = FILT11(ui4_c, ui4_d);
    predicted_pixels[6] = FILT11(ui4_d, ui4_e);
    predicted_pixels[7] = FILT11(ui4_e, ui4_f);
    predicted_pixels[8] = FILT11(ui4_f, ui4_g);
    predicted_pixels[9] = FILT11(ui4_g, ui4_h);
    predicted_pixels[10] = FILT11(ui4_h, ui4_i);
    predicted_pixels[11] = FILT121(ui4_p, ui4_o, ui4_n);
    predicted_pixels[12] = FILT121(ui4_n, ui4_m, ui4_l);
    predicted_pixels[13] = FILT121(ui4_l, ui4_k, ui4_j);
    predicted_pixels[14] = FILT121(ui4_b, ui4_a, ui4_j);
    predicted_pixels[15] = FILT121(ui4_a, ui4_b, ui4_c);
    predicted_pixels[16] = FILT121(ui4_b, ui4_c, ui4_d);
    predicted_pixels[17] = FILT121(ui4_c, ui4_d, ui4_e);
    predicted_pixels[18] = FILT121(ui4_d, ui4_e, ui4_f);
    predicted_pixels[19] = FILT121(ui4_e, ui4_f, ui4_g);
    predicted_pixels[20] = FILT121(ui4_f, ui4_g, ui4_h);
    predicted_pixels[21] = FILT121(ui4_g, ui4_h, ui4_i);

    memcpy(pu1_dst, predicted_pixels + 3, 8);
    memcpy(pu1_dst + 1 * dst_strd, predicted_pixels + 14, 8);
    memcpy(pu1_dst + 2 * dst_strd, predicted_pixels + 2, 8);
    memcpy(pu1_dst + 3 * dst_strd, predicted_pixels + 13, 8);
    memcpy(pu1_dst + 4 * dst_strd, predicted_pixels + 1, 8);
    memcpy(pu1_dst + 5 * dst_strd, predicted_pixels + 12, 8);
    memcpy(pu1_dst + 6 * dst_strd, predicted_pixels, 8);
    memcpy(pu1_dst + 7 * dst_strd, predicted_pixels + 11, 8);

}

/*
 *******************************************************************************
 *
 *ih264_intra_pred_luma_8x8_mode_horz_d
 *
 * @brief
 *     Perform Intra prediction for  luma_8x8 mode:Horizontal_Down
 *
 * @par Description:
 *    Perform Intra prediction for  luma_8x8 mode:Horizontal_Down ,described in sec 8.3.2.2.8
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
 * @param[in] ngbr_avail
 * availability of neighbouring pixels(Not used in this function)
 *
 * @returns
 *
 * @remarks
 *  None
 *
 *******************************************************************************/

void ih264_intra_pred_luma_8x8_mode_horz_d(UWORD8 *pu1_src,
                                           UWORD8 *pu1_dst,
                                           WORD32 src_strd,
                                           WORD32 dst_strd,
                                           WORD32 ngbr_avail)
{
    UWORD8 *pu1_left = NULL; /* Pointer to start of left predictors */
    UWORD8 *pu1_top = NULL; /* Pointer to start of top predictors */
    UWORD8 *pu1_topleft = NULL; /* Pointer to start of top left predictors */
    UWORD32 ui4_a;
    UWORD32 ui4_b, ui4_c, ui4_d, ui4_e, ui4_f, ui4_g, ui4_h, ui4_i;
    UWORD32 ui4_j, ui4_k, ui4_l, ui4_m, ui4_n, ui4_o, ui4_p;
    UWORD8 predicted_pixels[22];
    UNUSED(src_strd);
    UNUSED(ngbr_avail);
    pu1_top = pu1_src + BLK8x8SIZE + 1;
    pu1_left = pu1_src + BLK8x8SIZE - 1;
    pu1_topleft = pu1_src + BLK8x8SIZE;

    ui4_a = *pu1_topleft;
    ui4_j = *pu1_top++;
    ui4_k = *pu1_top++;
    ui4_l = *pu1_top++;
    ui4_m = *pu1_top++;
    ui4_n = *pu1_top++;
    ui4_o = *pu1_top++;
    ui4_p = *pu1_top++;
    ui4_b = *pu1_left--;
    ui4_c = *pu1_left--;
    ui4_d = *pu1_left--;
    ui4_e = *pu1_left--;
    ui4_f = *pu1_left--;
    ui4_g = *pu1_left--;
    ui4_h = *pu1_left--;
    ui4_i = *pu1_left;

    predicted_pixels[0] = FILT11(ui4_h, ui4_i);
    predicted_pixels[1] = FILT121(ui4_g, ui4_h, ui4_i);
    predicted_pixels[2] = FILT11(ui4_g, ui4_h);
    predicted_pixels[3] = FILT121(ui4_f, ui4_g, ui4_h);
    predicted_pixels[4] = FILT11(ui4_f, ui4_g);
    predicted_pixels[5] = FILT121(ui4_e, ui4_f, ui4_g);
    predicted_pixels[6] = FILT11(ui4_e, ui4_f);
    predicted_pixels[7] = FILT121(ui4_d, ui4_e, ui4_f);
    predicted_pixels[8] = FILT11(ui4_d, ui4_e);
    predicted_pixels[9] = FILT121(ui4_c, ui4_d, ui4_e);
    predicted_pixels[10] = FILT11(ui4_c, ui4_d);
    predicted_pixels[11] = FILT121(ui4_b, ui4_c, ui4_d);
    predicted_pixels[12] = FILT11(ui4_b, ui4_c);
    predicted_pixels[13] = FILT121(ui4_a, ui4_b, ui4_c);
    predicted_pixels[14] = FILT11(ui4_a, ui4_b);
    predicted_pixels[15] = FILT121(ui4_j, ui4_a, ui4_b);
    predicted_pixels[16] = FILT121(ui4_k, ui4_j, ui4_a);
    predicted_pixels[17] = FILT121(ui4_l, ui4_k, ui4_j);
    predicted_pixels[18] = FILT121(ui4_m, ui4_l, ui4_k);
    predicted_pixels[19] = FILT121(ui4_n, ui4_m, ui4_l);
    predicted_pixels[20] = FILT121(ui4_o, ui4_n, ui4_m);
    predicted_pixels[21] = FILT121(ui4_p, ui4_o, ui4_n);

    memcpy(pu1_dst, predicted_pixels + 14, 8);
    memcpy(pu1_dst + dst_strd, predicted_pixels + 12, 8);
    memcpy(pu1_dst + 2 * dst_strd, predicted_pixels + 10, 8);
    memcpy(pu1_dst + 3 * dst_strd, predicted_pixels + 8, 8);
    memcpy(pu1_dst + 4 * dst_strd, predicted_pixels + 6, 8);
    memcpy(pu1_dst + 5 * dst_strd, predicted_pixels + 4, 8);
    memcpy(pu1_dst + 6 * dst_strd, predicted_pixels + 2, 8);
    memcpy(pu1_dst + 7 * dst_strd, predicted_pixels, 8);
}

/**
 *******************************************************************************
 *
 *ih264_intra_pred_luma_8x8_mode_vert_l
 *
 * @brief
 *     Perform Intra prediction for  luma_8x8 mode:Vertical_Left
 *
 * @par Description:
 *    Perform Intra prediction for  luma_8x8 mode:Vertical_Left ,described in sec 8.3.2.2.9
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
 * @param[in] ngbr_avail
 * availability of neighbouring pixels(Not used in this function)
 *
 * @returns
 *
 * @remarks
 *  None
 *
 *******************************************************************************/

void ih264_intra_pred_luma_8x8_mode_vert_l(UWORD8 *pu1_src,
                                           UWORD8 *pu1_dst,
                                           WORD32 src_strd,
                                           WORD32 dst_strd,
                                           WORD32 ngbr_avail)
{
    UWORD8 *pu1_top = NULL; /* Pointer to start of top predictors */
    UWORD32 ui4_a, ui4_b, ui4_c, ui4_d, ui4_e, ui4_f, ui4_g, ui4_h;
    UWORD32 ui4_i, ui4_j, ui4_k, ui4_l, ui4_m;
    UWORD8 predicted_pixels[22];
    UNUSED(src_strd);
    UNUSED(ngbr_avail);
    pu1_top = pu1_src + BLK8x8SIZE + 1;

    ui4_a = *pu1_top++;
    ui4_b = *pu1_top++;
    ui4_c = *pu1_top++;
    ui4_d = *pu1_top++;
    ui4_e = *pu1_top++;
    ui4_f = *pu1_top++;
    ui4_g = *pu1_top++;
    ui4_h = *pu1_top++;
    ui4_i = *pu1_top++;
    ui4_j = *pu1_top++;
    ui4_k = *pu1_top++;
    ui4_l = *pu1_top++;
    ui4_m = *pu1_top++;

    predicted_pixels[0] = FILT11(ui4_a, ui4_b);
    predicted_pixels[1] = FILT11(ui4_b, ui4_c);
    predicted_pixels[2] = FILT11(ui4_c, ui4_d);
    predicted_pixels[3] = FILT11(ui4_d, ui4_e);
    predicted_pixels[4] = FILT11(ui4_e, ui4_f);
    predicted_pixels[5] = FILT11(ui4_f, ui4_g);
    predicted_pixels[6] = FILT11(ui4_g, ui4_h);
    predicted_pixels[7] = FILT11(ui4_h, ui4_i);
    predicted_pixels[8] = FILT11(ui4_i, ui4_j);
    predicted_pixels[9] = FILT11(ui4_j, ui4_k);
    predicted_pixels[10] = FILT11(ui4_k, ui4_l);
    predicted_pixels[11] = FILT121(ui4_a, ui4_b, ui4_c);
    predicted_pixels[12] = FILT121(ui4_b, ui4_c, ui4_d);
    predicted_pixels[13] = FILT121(ui4_c, ui4_d, ui4_e);
    predicted_pixels[14] = FILT121(ui4_d, ui4_e, ui4_f);
    predicted_pixels[15] = FILT121(ui4_e, ui4_f, ui4_g);
    predicted_pixels[16] = FILT121(ui4_f, ui4_g, ui4_h);
    predicted_pixels[17] = FILT121(ui4_g, ui4_h, ui4_i);
    predicted_pixels[18] = FILT121(ui4_h, ui4_i, ui4_j);
    predicted_pixels[19] = FILT121(ui4_i, ui4_j, ui4_k);
    predicted_pixels[20] = FILT121(ui4_j, ui4_k, ui4_l);
    predicted_pixels[21] = FILT121(ui4_k, ui4_l, ui4_m);

    memcpy(pu1_dst, predicted_pixels, 8);
    memcpy(pu1_dst + 2 * dst_strd, predicted_pixels + 1, 8);
    memcpy(pu1_dst + 4 * dst_strd, predicted_pixels + 2, 8);
    memcpy(pu1_dst + 6 * dst_strd, predicted_pixels + 3, 8);
    memcpy(pu1_dst + 1 * dst_strd, predicted_pixels + 11, 8);
    memcpy(pu1_dst + 3 * dst_strd, predicted_pixels + 12, 8);
    memcpy(pu1_dst + 5 * dst_strd, predicted_pixels + 13, 8);
    memcpy(pu1_dst + 7 * dst_strd, predicted_pixels + 14, 8);
}

/**
 *******************************************************************************
 *
 *ih264_intra_pred_luma_8x8_mode_horz_u
 *
 * @brief
 *     Perform Intra prediction for  luma_8x8 mode:Horizontal_Up
 *
 * @par Description:
 *    Perform Intra prediction for  luma_8x8 mode:Horizontal_Up ,described in sec 8.3.2.2.10
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
 * @param[in] ngbr_avail
 * availability of neighbouring pixels(Not used in this function)
 *
 * @returns
 *
 * @remarks
 *  None
 *
 *******************************************************************************/

void ih264_intra_pred_luma_8x8_mode_horz_u(UWORD8 *pu1_src,
                                           UWORD8 *pu1_dst,
                                           WORD32 src_strd,
                                           WORD32 dst_strd,
                                           WORD32 ngbr_avail)

{
    UWORD8 *pu1_left = NULL; /* Pointer to start of left predictors */
    UWORD32 ui4_j, ui4_k, ui4_l, ui4_m, ui4_n, ui4_o, ui4_p, ui4_q;
    UWORD8 predicted_pixels[22];
    UNUSED(src_strd);
    UNUSED(ngbr_avail);
    pu1_left = pu1_src + BLK8x8SIZE - 1;

    ui4_j = *pu1_left--;
    ui4_k = *pu1_left--;
    ui4_l = *pu1_left--;
    ui4_m = *pu1_left--;
    ui4_n = *pu1_left--;
    ui4_o = *pu1_left--;
    ui4_p = *pu1_left--;
    ui4_q = *pu1_left;

    pu1_left = pu1_src + BLK8x8SIZE - 1;

    predicted_pixels[0] = FILT11(ui4_j, ui4_k);
    predicted_pixels[1] = FILT121(ui4_j, ui4_k, ui4_l);
    predicted_pixels[2] = FILT11(ui4_k, ui4_l);
    predicted_pixels[3] = FILT121(ui4_k, ui4_l, ui4_m);
    predicted_pixels[4] = FILT11(ui4_l, ui4_m);
    predicted_pixels[5] = FILT121(ui4_l, ui4_m, ui4_n);
    predicted_pixels[6] = FILT11(ui4_m, ui4_n);
    predicted_pixels[7] = FILT121(ui4_m, ui4_n, ui4_o);
    predicted_pixels[8] = FILT11(ui4_n, ui4_o);
    predicted_pixels[9] = FILT121(ui4_n, ui4_o, ui4_p);
    predicted_pixels[10] = FILT11(ui4_o, ui4_p);
    predicted_pixels[11] = FILT121(ui4_o, ui4_p, ui4_q);
    predicted_pixels[12] = FILT11(ui4_p, ui4_q);
    predicted_pixels[13] = FILT121(ui4_p, ui4_q, ui4_q);
    memset(predicted_pixels+14,ui4_q,8);

    memcpy(pu1_dst, predicted_pixels, 8);
    memcpy(pu1_dst + 1 * dst_strd, predicted_pixels + 2, 8);
    memcpy(pu1_dst + 2 * dst_strd, predicted_pixels + 4, 8);
    memcpy(pu1_dst + 3 * dst_strd, predicted_pixels + 6, 8);
    memcpy(pu1_dst + 4 * dst_strd, predicted_pixels + 8, 8);
    memcpy(pu1_dst + 5 * dst_strd, predicted_pixels + 10, 8);
    memcpy(pu1_dst + 6 * dst_strd, predicted_pixels + 12, 8);
    memcpy(pu1_dst + 7 * dst_strd, predicted_pixels + 14, 8);
}


/*******************    16x16 Modes    *******************/

/**
 *******************************************************************************
 *
 *ih264_intra_pred_luma_16x16_mode_vert
 *
 * @brief
 *  Perform Intra prediction for  luma_16x16 mode:Vertical
 *
 * @par Description:
 *  Perform Intra prediction for  luma_16x16 mode:Vertical, described in sec 8.3.3.1
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
 * @param[in] ngbr_avail
 *  availability of neighbouring pixels (Not used in this function)
 *
 * @returns
 *
 * @remarks
 *  None
 *
 *******************************************************************************/

void ih264_intra_pred_luma_16x16_mode_vert(UWORD8 *pu1_src,
                                           UWORD8 *pu1_dst,
                                           WORD32 src_strd,
                                           WORD32 dst_strd,
                                           WORD32 ngbr_avail)
{
    UWORD8 *pu1_top = NULL; /* Pointer to start of top predictors */
    WORD32 rows; /* loop variables*/
    UNUSED(src_strd);
    UNUSED(ngbr_avail);
    pu1_top = pu1_src + MB_SIZE + 1;

    for(rows = 0; rows < 16; rows += 4, pu1_dst += dst_strd)
    {
        memcpy(pu1_dst, pu1_top, 16);
        pu1_dst += dst_strd;
        memcpy(pu1_dst, pu1_top, 16);
        pu1_dst += dst_strd;
        memcpy(pu1_dst, pu1_top, 16);
        pu1_dst += dst_strd;
        memcpy(pu1_dst, pu1_top, 16);
    }
}

/**
 *******************************************************************************
 *
 *ih264_intra_pred_luma_16x16_mode_horz
 *
 * @brief
 *  Perform Intra prediction for  luma_16x16 mode:Horizontal
 *
 * @par Description:
 *  Perform Intra prediction for  luma_16x16 mode:Horizontal, described in sec 8.3.3.2
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
 * @param[in] ngbr_avail
 * availability of neighbouring pixels(Not used in this function)
 *
 * @returns
 *
 * @remarks
 *  None
 *
 *******************************************************************************/

void ih264_intra_pred_luma_16x16_mode_horz(UWORD8 *pu1_src,
                                           UWORD8 *pu1_dst,
                                           WORD32 src_strd,
                                           WORD32 dst_strd,
                                           WORD32 ngbr_avail)
{
    UWORD8 *pu1_left = NULL; /* Pointer to start of top predictors */
    WORD32 rows;
    UNUSED(src_strd);
    UNUSED(ngbr_avail);
    pu1_left = pu1_src + MB_SIZE - 1;

    for(rows = 0; rows < 16; rows += 4, pu1_dst += dst_strd, pu1_left --)
    {
        memset(pu1_dst, *pu1_left, 16); /* copy the left value to the entire row*/
        pu1_left --;
        pu1_dst += dst_strd;
        memset(pu1_dst, *pu1_left, 16);
        pu1_left --;
        pu1_dst += dst_strd;
        memset(pu1_dst, *pu1_left, 16);
        pu1_left --;
        pu1_dst += dst_strd;
        memset(pu1_dst, *pu1_left, 16);
    }
}

/**
 *******************************************************************************
 *
 *ih264_intra_pred_luma_16x16_mode_dc
 *
 * @brief
 *  Perform Intra prediction for  luma_16x16 mode:DC
 *
 * @par Description:
 *  Perform Intra prediction for  luma_16x16 mode:DC, described in sec 8.3.3.3
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
 ** @param[in] ngbr_avail
 *  availability of neighbouring pixels
 *
 * @returns
 *
 * @remarks
 *  None
 *
 *******************************************************************************/

void ih264_intra_pred_luma_16x16_mode_dc(UWORD8 *pu1_src,
                                         UWORD8 *pu1_dst,
                                         WORD32 src_strd,
                                         WORD32 dst_strd,
                                         WORD32 ngbr_avail)
{
    WORD8 u1_useleft; /* availability of left predictors (only for DC) */
    UWORD8 u1_usetop; /* availability of top predictors (only for DC) */
    UWORD8 *pu1_left = NULL; /* Pointer to start of left predictors */
    UWORD8 *pu1_top = NULL; /* Pointer to start of top predictors */
    WORD32 rows; /* loop variables*/
    WORD32 val = 0;
    UNUSED(src_strd);

    u1_useleft = BOOLEAN(ngbr_avail & LEFT_MB_AVAILABLE_MASK);
    u1_usetop = BOOLEAN(ngbr_avail & TOP_MB_AVAILABLE_MASK);
    pu1_top = pu1_src + MB_SIZE + 1;
    pu1_left = pu1_src + MB_SIZE - 1;
    if(u1_useleft)
    {
        for(rows = 0; rows < 16; rows++)
            val += *(pu1_left - rows);
        val += 8;
    }
    if(u1_usetop)
    {
        for(rows = 0; rows < 16; rows++)
            val += *(pu1_top + rows);
        val += 8;
    }
    /* Since 8 is added if either left/top pred is there,
     val still being zero implies both preds are not there */
    val = (val) ? (val >> (3 + u1_useleft + u1_usetop)) : 128;

    for(rows = 0; rows < 16; rows += 4, pu1_dst += dst_strd)
    {
        memset(pu1_dst, val, 16);
        pu1_dst += dst_strd;
        memset(pu1_dst, val, 16);
        pu1_dst += dst_strd;
        memset(pu1_dst, val, 16);
        pu1_dst += dst_strd;
        memset(pu1_dst, val, 16);
    }
}

/**
 *******************************************************************************
 *
 *ih264_intra_pred_luma_16x16_mode_plane
 *
 * @brief
 *  Perform Intra prediction for  luma_16x16 mode:PLANE
 *
 * @par Description:
 *  Perform Intra prediction for  luma_16x16 mode:PLANE, described in sec 8.3.3.4
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
 * @param[in] ngbr_avail
 * availability of neighbouring pixels(Not used in this function)
 *
 * @returns
 *
 * @remarks
 *  None
 *
 *******************************************************************************/

void ih264_intra_pred_luma_16x16_mode_plane(UWORD8 *pu1_src,
                                            UWORD8 *pu1_dst,
                                            WORD32 src_strd,
                                            WORD32 dst_strd,
                                            WORD32 ngbr_avail)
{
    /*! Written with no multiplications */
    UWORD8 *pu1_left = NULL; /* Pointer to start of left predictors */
    UWORD8 *pu1_top = NULL; /* Pointer to start of top predictors */
    UWORD8 *pu1_topleft = NULL;
    WORD32 a, b, c, tmp;
    UWORD8 *pu1_tmp1, *pu1_tmp2;
    WORD32 shift;
    UNUSED(src_strd);
    UNUSED(ngbr_avail);
    pu1_top = pu1_src + MB_SIZE + 1;
    pu1_left = pu1_src + MB_SIZE - 1;
    pu1_topleft = pu1_src + MB_SIZE;

    {
        a = (*(pu1_top + 15) + *(pu1_left - 15)) << 4;

        /*! Implement Sum(x*(P((x+7),-1) - P((x-7),-1))) x=1...8 */
        pu1_tmp1 = pu1_top + 8;
        pu1_tmp2 = pu1_tmp1 - 2;

        /* Pixel diffs are only 9 bits;
         so sign extension allows shifts to be used even for signed */
        b = ((*pu1_tmp1++) - (*pu1_tmp2--)); /* x=1 */
        b += ((*pu1_tmp1++) - (*pu1_tmp2--)) << 1; /* x=2 */
        tmp = ((*pu1_tmp1++) - (*pu1_tmp2--));
        b += (tmp << 1) + tmp; /* x=3 */
        b += ((*pu1_tmp1++) - (*pu1_tmp2--)) << 2; /* x=4 */

        tmp = ((*pu1_tmp1++) - (*pu1_tmp2--));
        b += (tmp << 2) + tmp; /* x=5 */
        tmp = ((*pu1_tmp1++) - (*pu1_tmp2--));
        b += (tmp << 2) + (tmp << 1); /* x=6 */
        tmp = ((*pu1_tmp1++) - (*pu1_tmp2--));
        b += (tmp << 3) - tmp; /* x=7 */
        b += ((*pu1_tmp1) - (*pu1_topleft)) << 3; /* x=8 */

        b = ((b << 2) + b + 32) >> 6; /*! (5*H + 32)>>6 */

        /*! Implement Sum(y*(P(-1,(y+7)) - P(-1,(y-7)))) y=1...8 */
        pu1_tmp1 = pu1_left - 8;
        pu1_tmp2 = pu1_tmp1 + 2;

        c = ((*pu1_tmp1) - (*pu1_tmp2)); /* y=1 */
        pu1_tmp1--;
        pu1_tmp2++;
        c += ((*pu1_tmp1) - (*pu1_tmp2)) << 1; /* y=2 */
        pu1_tmp1--;
        pu1_tmp2++;
        tmp = ((*pu1_tmp1) - (*pu1_tmp2));
        c += (tmp << 1) + tmp; /* y=3 */
        pu1_tmp1--;
        pu1_tmp2++;
        c += ((*pu1_tmp1) - (*pu1_tmp2)) << 2; /* y=4 */
        pu1_tmp1--;
        pu1_tmp2++;

        tmp = ((*pu1_tmp1) - (*pu1_tmp2));
        c += (tmp << 2) + tmp; /* y=5 */
        pu1_tmp1--;
        pu1_tmp2++;
        tmp = ((*pu1_tmp1) - (*pu1_tmp2));
        c += (tmp << 2) + (tmp << 1); /* y=6 */
        pu1_tmp1--;
        pu1_tmp2++;
        tmp = ((*pu1_tmp1) - (*pu1_tmp2));
        c += (tmp << 3) - tmp; /* y=7 */
        pu1_tmp1--; //pu1_tmp2 ++;
        /* Modified to get (-1,-1) location as *(pu1_top - 1) instead of (pu1_left - ui4_stride) */
        //c += ((*pu1_tmp1) - (*(pu1_top - 1)))<<3;      /* y=8 */
        c += ((*pu1_tmp1) - (*pu1_topleft)) << 3; /* y=8 */

        c = ((c << 2) + c + 32) >> 6; /*! (5*V + 32)>>32 */
        shift = 3;
    }

    /*! Now from the plane parameters a, b, and c,
     compute the fitted plane values over the block */
    {
        WORD32 tmp1, tmpx, tmpx_init, j, i;

        tmpx_init = -(b << shift); /* -8b */
        tmp = a - (c << shift) + 16; /* a-((4or8)*c)+16 */
        for(i = 0; i < 16; i++)
        {
            tmp += c; /*increment every time by c to get c*(y-7or3)*/
            tmpx = tmpx_init; /* Init to -8b */
            for(j = 0; j < 16; j++)
            {
                tmpx += b; /* increment every time by b to get b*(x-7or3) */
                tmp1 = (tmp + tmpx) >> 5;
                *pu1_dst++ = CLIP_U8(tmp1);
            }
            pu1_dst += (dst_strd - 16);
        }
    }
}
