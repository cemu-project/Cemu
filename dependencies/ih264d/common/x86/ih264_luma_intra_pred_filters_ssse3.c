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
 *  ih264_luma_intra_pred_filters_ssse3.c
 *
 * @brief
 *  Contains function definitions for luma intra prediction filters in x86
 *  intrinsics
 *
 * @author
 *  Ittiam
 *
 * @par List of Functions:
 *  - ih264_intra_pred_luma_4x4_mode_vert_ssse3
 *  - ih264_intra_pred_luma_4x4_mode_horz_ssse3
 *  - ih264_intra_pred_luma_4x4_mode_dc_ssse3
 *  - ih264_intra_pred_luma_4x4_mode_diag_dl_ssse3
 *  - ih264_intra_pred_luma_4x4_mode_diag_dr_ssse3
 *  - ih264_intra_pred_luma_4x4_mode_vert_r_ssse3
 *  - ih264_intra_pred_luma_4x4_mode_horz_d_ssse3
 *  - ih264_intra_pred_luma_4x4_mode_vert_l_ssse3
 *  - ih264_intra_pred_luma_4x4_mode_horz_u_ssse3
 *  - ih264_intra_pred_luma_8x8_mode_vert_ssse3
 *  - ih264_intra_pred_luma_8x8_mode_horz_ssse3
 *  - ih264_intra_pred_luma_8x8_mode_dc_ssse3
 *  - ih264_intra_pred_luma_8x8_mode_diag_dl_ssse3
 *  - ih264_intra_pred_luma_8x8_mode_diag_dr_ssse3
 *  - ih264_intra_pred_luma_8x8_mode_vert_r_ssse3
 *  - ih264_intra_pred_luma_8x8_mode_horz_d_ssse3
 *  - ih264_intra_pred_luma_8x8_mode_vert_l_ssse3
 *  - ih264_intra_pred_luma_8x8_mode_horz_u_ssse3
 *  - ih264_intra_pred_luma_16x16_mode_vert_ssse3
 *  - ih264_intra_pred_luma_16x16_mode_horz_ssse3
 *  - ih264_intra_pred_luma_16x16_mode_dc_ssse3
 *  - ih264_intra_pred_luma_16x16_mode_plane_ssse3
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
#include <immintrin.h>

/* User include files */
#include "ih264_defs.h"
#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264_intra_pred_filters.h"



/*******************    LUMA INTRAPREDICTION    *******************/

/*******************    4x4 Modes    *******************/

/**
 *******************************************************************************
 *
 * ih264_intra_pred_luma_4x4_mode_vert_ssse3
 *
 * @brief
 *  Perform Intra prediction for luma_4x4 mode:vertical
 *
 * @par Description:
 *  Perform Intra prediction for luma_4x4 mode:vertical ,described in sec 8.3.1.2.1
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
void ih264_intra_pred_luma_4x4_mode_vert_ssse3(UWORD8 *pu1_src,
                                               UWORD8 *pu1_dst,
                                               WORD32 src_strd,
                                               WORD32 dst_strd,
                                               WORD32 ngbr_avail)
{
    UWORD8 *pu1_top;
    WORD32 dst_strd2, dst_strd3;
    WORD32 i4_top;

    UNUSED(src_strd);
    UNUSED(ngbr_avail);

    pu1_top = pu1_src + BLK_SIZE + 1;

    i4_top = *((WORD32 *)pu1_top);

    dst_strd2 = dst_strd << 1;
    dst_strd3 = dst_strd + dst_strd2;

    *((WORD32 *)(pu1_dst)) = i4_top;
    *((WORD32 *)(pu1_dst + dst_strd)) = i4_top;
    *((WORD32 *)(pu1_dst + dst_strd2)) = i4_top;
    *((WORD32 *)(pu1_dst + dst_strd3)) = i4_top;
}

/**
 *******************************************************************************
 *
 *ih264_intra_pred_luma_4x4_mode_horz_ssse3
 *
 * @brief
 *  Perform Intra prediction for luma_4x4 mode:horizontal
 *
 * @par Description:
 *  Perform Intra prediction for luma_4x4 mode:horizontal ,described in sec 8.3.1.2.2
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
void ih264_intra_pred_luma_4x4_mode_horz_ssse3(UWORD8 *pu1_src,
                                               UWORD8 *pu1_dst,
                                               WORD32 src_strd,
                                               WORD32 dst_strd,
                                               WORD32 ngbr_avail)
{
    UWORD8 *pu1_left = NULL; /* Pointer to start of left predictors */
    WORD32 row1,row2,row3,row4;
    UWORD8 val;
    WORD32 dst_strd2, dst_strd3;

    UNUSED(src_strd);
    UNUSED(ngbr_avail);
    pu1_left = pu1_src + BLK_SIZE - 1;

    val  = *pu1_left;
    row1 = val + (val << 8) + (val << 16) + (val << 24);
    val  = *(pu1_left - 1);
    row2 = val + (val << 8) + (val << 16) + (val << 24);
    val  = *(pu1_left - 2);
    row3 = val + (val << 8) + (val << 16) + (val << 24);
    val  = *(pu1_left - 3);
    row4 = val + (val << 8) + (val << 16) + (val << 24);

    dst_strd2 = dst_strd << 1;
    dst_strd3 = dst_strd + dst_strd2;

    *((WORD32 *)(pu1_dst)) = row1;
    *((WORD32 *)(pu1_dst + dst_strd)) = row2;
    *((WORD32 *)(pu1_dst + dst_strd2)) = row3;
    *((WORD32 *)(pu1_dst + dst_strd3)) = row4;
}

/**
 *******************************************************************************
 *
 * ih264_intra_pred_luma_4x4_mode_dc_ssse3
 *
 * @brief
 *  Perform Intra prediction for luma_4x4 mode:DC
 *
 * @par Description:
 *  Perform Intra prediction for luma_4x4 mode:DC ,described in sec 8.3.1.2.3
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
void ih264_intra_pred_luma_4x4_mode_dc_ssse3(UWORD8 *pu1_src,
                                             UWORD8 *pu1_dst,
                                             WORD32 src_strd,
                                             WORD32 dst_strd,
                                             WORD32 ngbr_avail)
{
    UWORD8 u1_useleft; /* availability of left predictors (only for DC) */
    UWORD8 u1_usetop; /* availability of top predictors (only for DC) */
    UWORD8 *pu1_left = NULL; /* Pointer to start of left predictors */
    UWORD8 *pu1_top = NULL; /* Pointer to start of top predictors */
    WORD32 dst_strd2, dst_strd3;
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

    val = val + (val << 8) + (val << 16) + (val << 24);

    dst_strd2 = dst_strd << 1;
    dst_strd3 = dst_strd + dst_strd2;

    *((WORD32 *)(pu1_dst)) = val;
    *((WORD32 *)(pu1_dst + dst_strd)) = val;
    *((WORD32 *)(pu1_dst + dst_strd2)) = val;
    *((WORD32 *)(pu1_dst + dst_strd3)) = val;
}

/**
 *******************************************************************************
 *
 * ih264_intra_pred_luma_4x4_mode_diag_dl_ssse3
 *
 * @brief
 *  Perform Intra prediction for luma_4x4 mode:Diagonal_Down_Left
 *
 * @par Description:
 *  Perform Intra prediction for luma_4x4 mode:Diagonal_Down_Left ,described in sec 8.3.1.2.4
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
void ih264_intra_pred_luma_4x4_mode_diag_dl_ssse3(UWORD8 *pu1_src,
                                                  UWORD8 *pu1_dst,
                                                  WORD32 src_strd,
                                                  WORD32 dst_strd,
                                                  WORD32 ngbr_avail)
{
    UWORD8 *pu1_top;
    WORD32 dst_strd2, dst_strd3;

    __m128i top_16x8b, top_8x16b, top_sh_8x16b;
    __m128i res1_8x16b, res2_8x16b, res_16x8b;
    __m128i zero_vector, const_2_8x16b;
    WORD32 row1,row2,row3,row4;

    UNUSED(src_strd);
    UNUSED(ngbr_avail);

    pu1_top = pu1_src + BLK_SIZE + 1;

    top_16x8b = _mm_loadl_epi64((__m128i *)pu1_top);
    zero_vector = _mm_setzero_si128();
    top_8x16b = _mm_unpacklo_epi8(top_16x8b, zero_vector);    //t0 t1 t2 t3 t4 t5 t6 t7

    top_sh_8x16b = _mm_srli_si128(top_8x16b, 2);              //t1 t2 t3 t4 t5 t6 t7 0
    const_2_8x16b = _mm_set1_epi16(2);

    top_sh_8x16b = _mm_shufflehi_epi16(top_sh_8x16b, 0xa4);   //t1 t2 t3 t4 t5 t6 t7 t7
    res1_8x16b = _mm_add_epi16(top_8x16b, top_sh_8x16b);
    res2_8x16b = _mm_srli_si128(res1_8x16b, 2);

    res1_8x16b = _mm_add_epi16(res1_8x16b, const_2_8x16b);
    res1_8x16b = _mm_add_epi16(res2_8x16b, res1_8x16b);
    res1_8x16b = _mm_srai_epi16(res1_8x16b, 2);

    dst_strd2 = dst_strd << 1;
    dst_strd3 = dst_strd + dst_strd2;

    res_16x8b = _mm_packus_epi16(res1_8x16b, res1_8x16b);
    row1 = _mm_cvtsi128_si32(res_16x8b);
    res_16x8b = _mm_srli_si128(res_16x8b, 1);
    row2 = _mm_cvtsi128_si32(res_16x8b);
    res_16x8b = _mm_srli_si128(res_16x8b, 1);
    row3 = _mm_cvtsi128_si32(res_16x8b);
    res_16x8b = _mm_srli_si128(res_16x8b, 1);
    row4 = _mm_cvtsi128_si32(res_16x8b);

    *((WORD32 *)(pu1_dst)) = row1;
    *((WORD32 *)(pu1_dst + dst_strd)) = row2;
    *((WORD32 *)(pu1_dst + dst_strd2)) = row3;
    *((WORD32 *)(pu1_dst + dst_strd3)) = row4;
}

/**
 *******************************************************************************
 *
 * ih264_intra_pred_luma_4x4_mode_diag_dr_ssse3
 *
 * @brief
 *  Perform Intra prediction for luma_4x4 mode:Diagonal_Down_Right
 *
 * @par Description:
 *  Perform Intra prediction for luma_4x4 mode:Diagonal_Down_Right ,described in sec 8.3.1.2.5
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
void ih264_intra_pred_luma_4x4_mode_diag_dr_ssse3(UWORD8 *pu1_src,
                                                  UWORD8 *pu1_dst,
                                                  WORD32 src_strd,
                                                  WORD32 dst_strd,
                                                  WORD32 ngbr_avail)
{
    UWORD8 *pu1_left;
    WORD32 dst_strd2, dst_strd3;

    __m128i top_left_16x8b, top_left_8x16b;
    __m128i top_left_sh_16x8b, top_left_sh_8x16b;
    __m128i res1_8x16b, res2_8x16b;
    __m128i res1_16x8b, res2_16x8b;
    __m128i zero_vector, const_2_8x16b;
    WORD32 row1,row2,row3,row4;

    UNUSED(src_strd);
    UNUSED(ngbr_avail);

    pu1_left = pu1_src + BLK_SIZE - 1;

    top_left_16x8b = _mm_loadu_si128((__m128i *)(pu1_left - 3));             //l3 l2 l1 l0 tl t0 t1 t2...
    zero_vector = _mm_setzero_si128();
    top_left_sh_16x8b = _mm_srli_si128(top_left_16x8b, 1);                   //l2 l1 l0 tl t0 t1 t2 t3...

    top_left_8x16b = _mm_unpacklo_epi8(top_left_16x8b, zero_vector);
    top_left_sh_8x16b = _mm_unpacklo_epi8(top_left_sh_16x8b, zero_vector);

    res1_8x16b = _mm_add_epi16(top_left_8x16b, top_left_sh_8x16b);           //l3+l2 l2+l1 l1+l0 l0+tl tl+t0 t0+t1 t1+t2 t2+t3...
    const_2_8x16b = _mm_set1_epi16(2);
    res2_8x16b = _mm_srli_si128(res1_8x16b, 2);                              //l2+l1 l1+l0 l0+tl tl+t0 t0+t1 t1+t2 t2+t3...

    res1_8x16b = _mm_add_epi16(res1_8x16b, const_2_8x16b);
    res1_8x16b = _mm_add_epi16(res2_8x16b, res1_8x16b);                      //l3+2*l2+l1+2 l2+2*l1+l0+2...
    res1_8x16b = _mm_srai_epi16(res1_8x16b, 2);
    res1_16x8b = _mm_packus_epi16(res1_8x16b, res1_8x16b);

    dst_strd2 = dst_strd << 1;
    dst_strd3 = dst_strd + dst_strd2;

    res2_16x8b = _mm_srli_si128(res1_16x8b, 3);

    row1 = _mm_cvtsi128_si32(res2_16x8b);
    res2_16x8b = _mm_srli_si128(res1_16x8b, 2);
    row2 = _mm_cvtsi128_si32(res2_16x8b);
    res2_16x8b = _mm_srli_si128(res1_16x8b, 1);
    row3 = _mm_cvtsi128_si32(res2_16x8b);
    row4 = _mm_cvtsi128_si32(res1_16x8b);

    *((WORD32 *)(pu1_dst)) = row1;
    *((WORD32 *)(pu1_dst + dst_strd)) = row2;
    *((WORD32 *)(pu1_dst + dst_strd2)) = row3;
    *((WORD32 *)(pu1_dst + dst_strd3)) = row4;
}

/**
 *******************************************************************************
 *
 * ih264_intra_pred_luma_4x4_mode_vert_r_ssse3
 *
 * @brief
 *  Perform Intra prediction for luma_4x4 mode:Vertical_Right
 *
 * @par Description:
 *  Perform Intra prediction for luma_4x4 mode:Vertical_Right ,described in sec 8.3.1.2.6
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
void ih264_intra_pred_luma_4x4_mode_vert_r_ssse3(UWORD8 *pu1_src,
                                                 UWORD8 *pu1_dst,
                                                 WORD32 src_strd,
                                                 WORD32 dst_strd,
                                                 WORD32 ngbr_avail)
{
    UWORD8 *pu1_left;
    WORD32 dst_strd2, dst_strd3;

    __m128i val_16x8b, temp_16x8b;
    __m128i w11_a1_16x8b, w11_a2_16x8b;
    __m128i w121_a1_8x16b, w121_a2_8x16b, w121_sh_8x16b;
    __m128i row1_16x8b, row2_16x8b, row3_16x8b, row4_16x8b;
    __m128i zero_vector, const_2_8x16b;
    WORD32 row1,row2,row3,row4;

    UNUSED(src_strd);
    UNUSED(ngbr_avail);

    pu1_left = pu1_src + BLK_SIZE - 1;

    val_16x8b = _mm_loadl_epi64((__m128i *)(pu1_left - 2));
    zero_vector = _mm_setzero_si128();

    w121_a1_8x16b = _mm_unpacklo_epi8(val_16x8b, zero_vector);        //l2 l1 l0 tl t0 t1 t2 t3
    w11_a1_16x8b = _mm_srli_si128(val_16x8b, 3);
    w121_a2_8x16b = _mm_srli_si128(w121_a1_8x16b, 2);                 //l1 l0 tl t0 t1 t2 t3 0
    w11_a2_16x8b = _mm_srli_si128(val_16x8b, 4);

    w121_a1_8x16b = _mm_add_epi16(w121_a1_8x16b, w121_a2_8x16b);      //l2+l1 l1+l0 l0+tl tl+t0 t0+t1 t1+t2 t2+t3 t3
    row1_16x8b = _mm_avg_epu8(w11_a1_16x8b, w11_a2_16x8b);
    w121_a2_8x16b = _mm_srli_si128(w121_a1_8x16b, 2);                 //l1+l0 l0+tl tl+t0 t0+t1 t1+t2 t2+t3 t3    0

    const_2_8x16b = _mm_set1_epi16(2);
    w121_a1_8x16b = _mm_add_epi16(w121_a1_8x16b, w121_a2_8x16b);      //l2+2*l1+l0 l1+2*l0+tl ...
    w121_a1_8x16b = _mm_add_epi16(w121_a1_8x16b, const_2_8x16b);
    w121_a1_8x16b = _mm_srai_epi16(w121_a1_8x16b, 2);

    w121_sh_8x16b = _mm_shufflelo_epi16(w121_a1_8x16b, 0xe1);
    w121_sh_8x16b = _mm_srli_si128(w121_sh_8x16b, 2);

    row4_16x8b = _mm_packus_epi16(w121_sh_8x16b, w121_sh_8x16b);
    temp_16x8b = _mm_slli_si128(w121_a1_8x16b, 13);
    row2_16x8b = _mm_srli_si128(row4_16x8b, 1);
    row3_16x8b = _mm_alignr_epi8(row1_16x8b, temp_16x8b, 15);

    dst_strd2 = dst_strd << 1;
    dst_strd3 = dst_strd + dst_strd2;

    row1 = _mm_cvtsi128_si32(row1_16x8b);
    row2 = _mm_cvtsi128_si32(row2_16x8b);
    row3 = _mm_cvtsi128_si32(row3_16x8b);
    row4 = _mm_cvtsi128_si32(row4_16x8b);

    *((WORD32 *)(pu1_dst)) = row1;
    *((WORD32 *)(pu1_dst + dst_strd)) = row2;
    *((WORD32 *)(pu1_dst + dst_strd2)) = row3;
    *((WORD32 *)(pu1_dst + dst_strd3)) = row4;
}

/*
 *******************************************************************************
 *
 * ih264_intra_pred_luma_4x4_mode_horz_d_ssse3
 *
 * @brief
 *  Perform Intra prediction for luma_4x4 mode:Horizontal_Down
 *
 * @par Description:
 *  Perform Intra prediction for luma_4x4 mode:Horizontal_Down ,described in sec 8.3.1.2.7
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
void ih264_intra_pred_luma_4x4_mode_horz_d_ssse3(UWORD8 *pu1_src,
                                           UWORD8 *pu1_dst,
                                           WORD32 src_strd,
                                           WORD32 dst_strd,
                                           WORD32 ngbr_avail)
{
    UWORD8 *pu1_left;
    WORD32 dst_strd2, dst_strd3;
    WORD32 val_121_t0t1;

    __m128i val_16x8b, val_sh_16x8b;
    __m128i w11_16x8b;
    __m128i w121_a1_8x16b, w121_a2_8x16b, w121_16x8b;
    __m128i row1_16x8b, row2_16x8b, row3_16x8b, row4_16x8b;

    __m128i zero_vector, const_2_8x16b;
    WORD32 row1,row2,row3,row4;

    UNUSED(src_strd);
    UNUSED(ngbr_avail);

    pu1_left = pu1_src + BLK_SIZE - 1;

    val_16x8b = _mm_loadl_epi64((__m128i *)(pu1_left - 3));
    zero_vector = _mm_setzero_si128();
    val_sh_16x8b = _mm_srli_si128(val_16x8b, 1);
    w11_16x8b = _mm_avg_epu8(val_16x8b, val_sh_16x8b);

    w121_a1_8x16b = _mm_unpacklo_epi8(val_16x8b, zero_vector);        //l3 l2 l1 l0 tl t0 t1 t2
    w121_a2_8x16b = _mm_srli_si128(w121_a1_8x16b, 2);                 //l2 l1 l0 tl t0 t1 t2 0
    w121_a1_8x16b = _mm_add_epi16(w121_a1_8x16b, w121_a2_8x16b);      //l3+l2 l2+l1 l1+l0 l0+tl tl+t0 t0+t1 t1+t2 t2
    w121_a2_8x16b = _mm_srli_si128(w121_a1_8x16b, 2);                 //l2+l1 l1+l0 l0+tl tl+t0 t0+t1 t1+t2 t2    0

    zero_vector = _mm_setzero_si128();
    const_2_8x16b = _mm_set1_epi16(2);

    w121_a1_8x16b = _mm_add_epi16(w121_a1_8x16b, w121_a2_8x16b);      //l3+2*l2+l1 l2+2*l1+l0 l1+2*l0+tl ...
    w121_a1_8x16b = _mm_add_epi16(w121_a1_8x16b, const_2_8x16b);
    w121_a1_8x16b = _mm_srai_epi16(w121_a1_8x16b, 2);

    w121_16x8b = _mm_packus_epi16(w121_a1_8x16b, w121_a1_8x16b);

    row4_16x8b = _mm_unpacklo_epi8(w11_16x8b, w121_16x8b);
    val_121_t0t1 = _mm_extract_epi16(w121_16x8b, 2);
    row4_16x8b = _mm_insert_epi16(row4_16x8b, val_121_t0t1, 4);

    dst_strd2 = dst_strd << 1;
    dst_strd3 = dst_strd + dst_strd2;

    row1_16x8b = _mm_srli_si128(row4_16x8b, 6);
    row2_16x8b = _mm_srli_si128(row4_16x8b, 4);
    row3_16x8b = _mm_srli_si128(row4_16x8b, 2);

    row1 = _mm_cvtsi128_si32(row1_16x8b);
    row2 = _mm_cvtsi128_si32(row2_16x8b);
    row3 = _mm_cvtsi128_si32(row3_16x8b);
    row4 = _mm_cvtsi128_si32(row4_16x8b);

    *((WORD32 *)(pu1_dst)) = row1;
    *((WORD32 *)(pu1_dst + dst_strd)) = row2;
    *((WORD32 *)(pu1_dst + dst_strd2)) = row3;
    *((WORD32 *)(pu1_dst + dst_strd3)) = row4;
}

/**
 *******************************************************************************
 *
 * ih264_intra_pred_luma_4x4_mode_vert_l_ssse3
 *
 * @brief
 *  Perform Intra prediction for luma_4x4 mode:Vertical_Left
 *
 * @par Description:
 *  Perform Intra prediction for luma_4x4 mode:Vertical_Left ,described in sec 8.3.1.2.8
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
void ih264_intra_pred_luma_4x4_mode_vert_l_ssse3(UWORD8 *pu1_src,
                                                 UWORD8 *pu1_dst,
                                                 WORD32 src_strd,
                                                 WORD32 dst_strd,
                                                 WORD32 ngbr_avail)
{
    UWORD8 *pu1_top;
    WORD32 dst_strd2, dst_strd3;

    __m128i val_16x8b, val_sh_16x8b;
    __m128i w121_a1_8x16b, w121_a2_8x16b;
    __m128i row1_16x8b, row2_16x8b, row3_16x8b, row4_16x8b;

    __m128i zero_vector, const_2_8x16b;
    WORD32 row1,row2,row3,row4;

    UNUSED(src_strd);
    UNUSED(ngbr_avail);

    pu1_top = pu1_src +BLK_SIZE + 1;

    val_16x8b = _mm_loadl_epi64((__m128i *)pu1_top);
    zero_vector = _mm_setzero_si128();
    val_sh_16x8b = _mm_srli_si128(val_16x8b, 1);
    row1_16x8b = _mm_avg_epu8(val_16x8b, val_sh_16x8b);

    w121_a1_8x16b = _mm_unpacklo_epi8(val_16x8b, zero_vector);        //t0 t1 t2 t3 t4 t5...
    w121_a2_8x16b = _mm_srli_si128(w121_a1_8x16b, 2);                 //t1 t2 t3 t4 t5 t6...
    w121_a1_8x16b = _mm_add_epi16(w121_a1_8x16b, w121_a2_8x16b);      //t0+t1 t1+t2 t2+t3 t3+t4 t4+t5...
    w121_a2_8x16b = _mm_srli_si128(w121_a1_8x16b, 2);                 //t1+t2 t2+t3 t3+t4 t4+t5 t5+t6...

    zero_vector = _mm_setzero_si128();
    const_2_8x16b = _mm_set1_epi16(2);

    w121_a1_8x16b = _mm_add_epi16(w121_a1_8x16b, w121_a2_8x16b);      //t0+2*t1+t2 t1+2*t2+t3 t2+2*t3+t4...
    w121_a1_8x16b = _mm_add_epi16(w121_a1_8x16b, const_2_8x16b);
    w121_a1_8x16b = _mm_srai_epi16(w121_a1_8x16b, 2);

    row2_16x8b = _mm_packus_epi16(w121_a1_8x16b, w121_a1_8x16b);

    dst_strd2 = dst_strd << 1;
    dst_strd3 = dst_strd + dst_strd2;

    row3_16x8b = _mm_srli_si128(row1_16x8b, 1);
    row4_16x8b = _mm_srli_si128(row2_16x8b, 1);

    row1 = _mm_cvtsi128_si32(row1_16x8b);
    row2 = _mm_cvtsi128_si32(row2_16x8b);
    row3 = _mm_cvtsi128_si32(row3_16x8b);
    row4 = _mm_cvtsi128_si32(row4_16x8b);

    *((WORD32 *)(pu1_dst)) = row1;
    *((WORD32 *)(pu1_dst + dst_strd)) = row2;
    *((WORD32 *)(pu1_dst + dst_strd2)) = row3;
    *((WORD32 *)(pu1_dst + dst_strd3)) = row4;
}

/**
 *******************************************************************************
 *
 * ih264_intra_pred_luma_4x4_mode_horz_u_ssse3
 *
 * @brief
 *  Perform Intra prediction for luma_4x4 mode:Horizontal_Up
 *
 * @par Description:
 *  Perform Intra prediction for luma_4x4 mode:Horizontal_Up ,described in sec 8.3.1.2.9
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
void ih264_intra_pred_luma_4x4_mode_horz_u_ssse3(UWORD8 *pu1_src,
                                                 UWORD8 *pu1_dst,
                                                 WORD32 src_strd,
                                                 WORD32 dst_strd,
                                                 WORD32 ngbr_avail)
{
    UWORD8 *pu1_left;
    WORD32 dst_strd2, dst_strd3;

    __m128i val_16x8b, val_sh_16x8b;
    __m128i w11_16x8b;
    __m128i w121_a1_8x16b, w121_a2_8x16b, w121_16x8b;
    __m128i row1_16x8b, row2_16x8b, row3_16x8b, row4_16x8b;

    __m128i zero_vector, const_2_8x16b, rev_16x8b;
    WORD32 row1,row2,row3,row4;

    UNUSED(src_strd);
    UNUSED(ngbr_avail);

    pu1_left = pu1_src + BLK_SIZE - 1;

    zero_vector = _mm_setzero_si128();
    rev_16x8b = _mm_setr_epi8(3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

    val_16x8b = _mm_loadl_epi64((__m128i *)(pu1_left - 3));           //l3 l2 l1 l0 0  0  0...
    val_16x8b = _mm_shuffle_epi8(val_16x8b, rev_16x8b);                //l0 l1 l2 l3 l3 l3 l3...

    val_sh_16x8b = _mm_srli_si128(val_16x8b, 1);
    w11_16x8b = _mm_avg_epu8(val_16x8b, val_sh_16x8b);

    w121_a1_8x16b = _mm_unpacklo_epi8(val_16x8b, zero_vector);        //l0 l1 l2 l3 l3 l3...
    w121_a2_8x16b = _mm_srli_si128(w121_a1_8x16b, 2);                 //l1 l2 l3 l3 l3 l3...

    w121_a1_8x16b = _mm_add_epi16(w121_a1_8x16b, w121_a2_8x16b);      //l0+t1 l1+l2 l2+l3 2*l3 2*l3...
    w121_a2_8x16b = _mm_srli_si128(w121_a1_8x16b, 2);                 //l1+t2 l2+l3 2*l3  2*l3 2*l3...

    zero_vector = _mm_setzero_si128();
    const_2_8x16b = _mm_set1_epi16(2);

    w121_a1_8x16b = _mm_add_epi16(w121_a1_8x16b, w121_a2_8x16b);      //l0+2*l1+l2 l1+2*l2+l3 l2+3*l3 4*l3 4*l3...
    w121_a1_8x16b = _mm_add_epi16(w121_a1_8x16b, const_2_8x16b);
    w121_a1_8x16b = _mm_srai_epi16(w121_a1_8x16b, 2);

    w121_16x8b = _mm_packus_epi16(w121_a1_8x16b, w121_a1_8x16b);

    dst_strd2 = dst_strd << 1;
    dst_strd3 = dst_strd + dst_strd2;

    row1_16x8b = _mm_unpacklo_epi8(w11_16x8b, w121_16x8b);
    row2_16x8b = _mm_srli_si128(row1_16x8b, 2);
    row3_16x8b = _mm_srli_si128(row1_16x8b, 4);
    row4_16x8b = _mm_srli_si128(row1_16x8b, 6);

    row1 = _mm_cvtsi128_si32(row1_16x8b);
    row2 = _mm_cvtsi128_si32(row2_16x8b);
    row3 = _mm_cvtsi128_si32(row3_16x8b);
    row4 = _mm_cvtsi128_si32(row4_16x8b);

    *((WORD32 *)(pu1_dst)) = row1;
    *((WORD32 *)(pu1_dst + dst_strd)) = row2;
    *((WORD32 *)(pu1_dst + dst_strd2)) = row3;
    *((WORD32 *)(pu1_dst + dst_strd3)) = row4;
}

/*******************    8x8 Modes    *******************/

/**
 *******************************************************************************
 *
 * ih264_intra_pred_luma_8x8_mode_vert_ssse3
 *
 * @brief
 *  Perform Intra prediction for luma_8x8 mode:vertical
 *
 * @par Description:
 *  Perform Intra prediction for luma_8x8 mode:vertical ,described in sec 8.3.2.2.2
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
void ih264_intra_pred_luma_8x8_mode_vert_ssse3(UWORD8 *pu1_src,
                                               UWORD8 *pu1_dst,
                                               WORD32 src_strd,
                                               WORD32 dst_strd,
                                               WORD32 ngbr_avail)
{
    UWORD8 *pu1_top = NULL;
    __m128i top_8x8b;
    UNUSED(src_strd);
    UNUSED(ngbr_avail);
    pu1_top = pu1_src + BLK8x8SIZE + 1;

    top_8x8b = _mm_loadl_epi64((__m128i *)pu1_top);

    _mm_storel_epi64((__m128i *)(pu1_dst + 0 * dst_strd), top_8x8b);
    _mm_storel_epi64((__m128i *)(pu1_dst + 1 * dst_strd), top_8x8b);
    _mm_storel_epi64((__m128i *)(pu1_dst + 2 * dst_strd), top_8x8b);
    _mm_storel_epi64((__m128i *)(pu1_dst + 3 * dst_strd), top_8x8b);
    _mm_storel_epi64((__m128i *)(pu1_dst + 4 * dst_strd), top_8x8b);
    _mm_storel_epi64((__m128i *)(pu1_dst + 5 * dst_strd), top_8x8b);
    _mm_storel_epi64((__m128i *)(pu1_dst + 6 * dst_strd), top_8x8b);
    _mm_storel_epi64((__m128i *)(pu1_dst + 7 * dst_strd), top_8x8b);
}

/**
 *******************************************************************************
 *
 * ih264_intra_pred_luma_8x8_mode_horz_ssse3
 *
 * @brief
 *  Perform Intra prediction for luma_8x8 mode:horizontal
 *
 * @par Description:
 *  Perform Intra prediction for  uma_8x8 mode:horizontal ,described in sec 8.3.2.2.2
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
void ih264_intra_pred_luma_8x8_mode_horz_ssse3(UWORD8 *pu1_src,
                                               UWORD8 *pu1_dst,
                                               WORD32 src_strd,
                                               WORD32 dst_strd,
                                               WORD32 ngbr_avail)
{
    UWORD8 *pu1_left = pu1_src + BLK8x8SIZE - 1;
    __m128i row1_8x8b, row2_8x8b, row3_8x8b, row4_8x8b;
    __m128i row5_8x8b, row6_8x8b, row7_8x8b, row8_8x8b;

    UNUSED(src_strd);
    UNUSED(ngbr_avail);

    row1_8x8b = _mm_set1_epi8(pu1_left[0]);
    row2_8x8b = _mm_set1_epi8(pu1_left[-1]);
    row3_8x8b = _mm_set1_epi8(pu1_left[-2]);
    row4_8x8b = _mm_set1_epi8(pu1_left[-3]);
    row5_8x8b = _mm_set1_epi8(pu1_left[-4]);
    row6_8x8b = _mm_set1_epi8(pu1_left[-5]);
    row7_8x8b = _mm_set1_epi8(pu1_left[-6]);
    row8_8x8b = _mm_set1_epi8(pu1_left[-7]);

    _mm_storel_epi64((__m128i *)(pu1_dst + 0 * dst_strd), row1_8x8b);
    _mm_storel_epi64((__m128i *)(pu1_dst + 1 * dst_strd), row2_8x8b);
    _mm_storel_epi64((__m128i *)(pu1_dst + 2 * dst_strd), row3_8x8b);
    _mm_storel_epi64((__m128i *)(pu1_dst + 3 * dst_strd), row4_8x8b);
    _mm_storel_epi64((__m128i *)(pu1_dst + 4 * dst_strd), row5_8x8b);
    _mm_storel_epi64((__m128i *)(pu1_dst + 5 * dst_strd), row6_8x8b);
    _mm_storel_epi64((__m128i *)(pu1_dst + 6 * dst_strd), row7_8x8b);
    _mm_storel_epi64((__m128i *)(pu1_dst + 7 * dst_strd), row8_8x8b);
}

/**
 *******************************************************************************
 *
 * ih264_intra_pred_luma_8x8_mode_dc_ssse3
 *
 * @brief
 *  Perform Intra prediction for luma_8x8 mode:DC
 *
 * @par Description:
 *  Perform Intra prediction for luma_8x8 mode:DC ,described in sec 8.3.2.2.4
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
void ih264_intra_pred_luma_8x8_mode_dc_ssse3(UWORD8 *pu1_src,
                                             UWORD8 *pu1_dst,
                                             WORD32 src_strd,
                                             WORD32 dst_strd,
                                             WORD32 ngbr_avail)
{
    UWORD8 u1_useleft; /* availability of left predictors (only for DC) */
    UWORD8 u1_usetop; /* availability of top predictors (only for DC) */
    UWORD8 *pu1_left = NULL; /* Pointer to start of left predictors */
    UWORD8 *pu1_top = NULL; /* Pointer to start of top predictors */
    __m128i dc_val_8x8b;
    WORD32 dc_val = 0;
    UNUSED(src_strd);

    u1_useleft = BOOLEAN(ngbr_avail & LEFT_MB_AVAILABLE_MASK);
    u1_usetop = BOOLEAN(ngbr_avail & TOP_MB_AVAILABLE_MASK);
    pu1_top = pu1_src + BLK8x8SIZE + 1;
    pu1_left = pu1_src + BLK8x8SIZE - 1;

    if(u1_useleft || u1_usetop)
    {
        WORD32 shft = 2;
        __m128i val_8x8b, zero_8x8b, sum_8x16b;

        zero_8x8b = _mm_setzero_si128();

        if(u1_useleft)
        {
            val_8x8b = _mm_loadl_epi64((__m128i *)(pu1_left - 7));
            sum_8x16b = _mm_sad_epu8(zero_8x8b, val_8x8b);

            shft++;
            dc_val += 4;
            dc_val += _mm_extract_epi16(sum_8x16b, 0);
        }
        if(u1_usetop)
        {
            val_8x8b = _mm_loadl_epi64((__m128i *)pu1_top);
            sum_8x16b = _mm_sad_epu8(zero_8x8b, val_8x8b);

            shft++;
            dc_val += 4;
            dc_val += _mm_extract_epi16(sum_8x16b, 0);
        }
        dc_val = dc_val >> shft;
    }
    else
        dc_val = 128;

    dc_val_8x8b = _mm_set1_epi8(dc_val);

    _mm_storel_epi64((__m128i *)(pu1_dst + 0 * dst_strd), dc_val_8x8b);
    _mm_storel_epi64((__m128i *)(pu1_dst + 1 * dst_strd), dc_val_8x8b);
    _mm_storel_epi64((__m128i *)(pu1_dst + 2 * dst_strd), dc_val_8x8b);
    _mm_storel_epi64((__m128i *)(pu1_dst + 3 * dst_strd), dc_val_8x8b);
    _mm_storel_epi64((__m128i *)(pu1_dst + 4 * dst_strd), dc_val_8x8b);
    _mm_storel_epi64((__m128i *)(pu1_dst + 5 * dst_strd), dc_val_8x8b);
    _mm_storel_epi64((__m128i *)(pu1_dst + 6 * dst_strd), dc_val_8x8b);
    _mm_storel_epi64((__m128i *)(pu1_dst + 7 * dst_strd), dc_val_8x8b);
}

/**
 *******************************************************************************
 *
 * ih264_intra_pred_luma_8x8_mode_diag_dl_ssse3
 *
 * @brief
 *  Perform Intra prediction for luma_8x8 mode:Diagonal_Down_Left
 *
 * @par Description:
 *  Perform Intra prediction for luma_8x8 mode:Diagonal_Down_Left ,described in sec 8.3.2.2.5
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
void ih264_intra_pred_luma_8x8_mode_diag_dl_ssse3(UWORD8 *pu1_src,
                                                  UWORD8 *pu1_dst,
                                                  WORD32 src_strd,
                                                  WORD32 dst_strd,
                                                  WORD32 ngbr_avail)
{
    UWORD8 *pu1_top = NULL; /* Pointer to start of top predictors */
    __m128i top_16x8;
    __m128i out_15x16;
    __m128i a0_8x16, a1_8x16, a2_8x16;
    __m128i temp1, temp2;
    __m128i res1_8x16, res2_8x16;
    __m128i zero = _mm_setzero_si128();
    __m128i const_val2_8x16 = _mm_set1_epi16(2);

    UNUSED(src_strd);
    UNUSED(ngbr_avail);

    pu1_top = pu1_src + BLK8x8SIZE + 1;

    top_16x8 = _mm_loadu_si128((__m128i *)(pu1_top));

    temp1 = _mm_srli_si128(top_16x8, 1);
    temp2 = _mm_srli_si128(top_16x8, 2);
    a0_8x16 = _mm_unpacklo_epi8(top_16x8, zero);
    a1_8x16 = _mm_unpacklo_epi8(temp1, zero);
    a2_8x16 = _mm_unpacklo_epi8(temp2, zero);

    a0_8x16 = _mm_add_epi16(a0_8x16, a2_8x16);
    a1_8x16 = _mm_add_epi16(a1_8x16, a1_8x16);
    a0_8x16 = _mm_add_epi16(a0_8x16, const_val2_8x16);
    a0_8x16 = _mm_add_epi16(a0_8x16, a1_8x16);
    res1_8x16 = _mm_srai_epi16(a0_8x16, 2);

    temp2 = _mm_srli_si128(top_16x8, 2);
    temp1 = _mm_srli_si128(top_16x8, 1);
    a2_8x16 = _mm_unpackhi_epi8(temp2, zero);
    a0_8x16 = _mm_unpackhi_epi8(top_16x8, zero);
    a2_8x16 = _mm_shufflehi_epi16(a2_8x16, 0x14);
    a1_8x16 = _mm_unpackhi_epi8(temp1, zero);

    a0_8x16 = _mm_add_epi16(a0_8x16, a2_8x16);
    a1_8x16 = _mm_add_epi16(a1_8x16, a1_8x16);
    a0_8x16 = _mm_add_epi16(a0_8x16, const_val2_8x16);
    a0_8x16 = _mm_add_epi16(a0_8x16, a1_8x16);
    res2_8x16 = _mm_srai_epi16(a0_8x16, 2);

    out_15x16 = _mm_packus_epi16(res1_8x16, res2_8x16);

    _mm_storel_epi64((__m128i *)(pu1_dst + 0 * dst_strd), out_15x16);
    out_15x16 = _mm_srli_si128(out_15x16, 1);
    _mm_storel_epi64((__m128i *)(pu1_dst + 1 * dst_strd), out_15x16);
    out_15x16 = _mm_srli_si128(out_15x16, 1);
    _mm_storel_epi64((__m128i *)(pu1_dst + 2 * dst_strd), out_15x16);
    out_15x16 = _mm_srli_si128(out_15x16, 1);
    _mm_storel_epi64((__m128i *)(pu1_dst + 3 * dst_strd), out_15x16);
    out_15x16 = _mm_srli_si128(out_15x16, 1);
    _mm_storel_epi64((__m128i *)(pu1_dst + 4 * dst_strd), out_15x16);
    out_15x16 = _mm_srli_si128(out_15x16, 1);
    _mm_storel_epi64((__m128i *)(pu1_dst + 5 * dst_strd), out_15x16);
    out_15x16 = _mm_srli_si128(out_15x16, 1);
    _mm_storel_epi64((__m128i *)(pu1_dst + 6 * dst_strd), out_15x16);
    out_15x16 = _mm_srli_si128(out_15x16, 1);
    _mm_storel_epi64((__m128i *)(pu1_dst + 7 * dst_strd), out_15x16);
}

/**
 *******************************************************************************
 *
 * ih264_intra_pred_luma_8x8_mode_diag_dr_ssse3
 *
 * @brief
 *  Perform Intra prediction for luma_8x8 mode:Diagonal_Down_Right
 *
 * @par Description:
 *  Perform Intra prediction for luma_8x8 mode:Diagonal_Down_Right ,described in sec 8.3.2.2.6
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
void ih264_intra_pred_luma_8x8_mode_diag_dr_ssse3(UWORD8 *pu1_src,
                                                  UWORD8 *pu1_dst,
                                                  WORD32 src_strd,
                                                  WORD32 dst_strd,
                                                  WORD32 ngbr_avail)
{
    UWORD8 *pu1_left = NULL; /* Pointer to start of left predictors */
    UWORD8 *pu1_top = NULL; /* Pointer to start of top predictors */
    __m128i top_8x8, left_16x8;
    __m128i out_15x16;
    __m128i a0_8x16, a1_8x16, a2_8x16;
    __m128i temp1, temp2;
    __m128i res1_8x16, res2_8x16;
    __m128i zero = _mm_setzero_si128();
    __m128i const_val2_8x16 = _mm_set1_epi16(2);
    __m128i str_8x8;

    UNUSED(src_strd);
    UNUSED(ngbr_avail);

    pu1_left = pu1_src + BLK8x8SIZE - 1;
    pu1_top = pu1_src + BLK8x8SIZE + 1;

    left_16x8 = _mm_loadu_si128((__m128i *)(pu1_left - 7));

    temp1 = _mm_srli_si128(left_16x8, 1);
    temp2 = _mm_srli_si128(left_16x8, 2);
    a0_8x16 = _mm_unpacklo_epi8(left_16x8, zero);
    a1_8x16 = _mm_unpacklo_epi8(temp1, zero);
    a2_8x16 = _mm_unpacklo_epi8(temp2, zero);

    a0_8x16 = _mm_add_epi16(a0_8x16, a2_8x16);
    a1_8x16 = _mm_add_epi16(a1_8x16, a1_8x16);
    a0_8x16 = _mm_add_epi16(a0_8x16, const_val2_8x16);
    a0_8x16 = _mm_add_epi16(a0_8x16, a1_8x16);
    res1_8x16 = _mm_srai_epi16(a0_8x16, 2);

    top_8x8 = _mm_loadu_si128((__m128i *)(pu1_top - 1));

    temp1 = _mm_srli_si128(top_8x8, 1);
    temp2 = _mm_srli_si128(top_8x8, 2);
    a0_8x16 = _mm_unpacklo_epi8(top_8x8, zero);
    a1_8x16 = _mm_unpacklo_epi8(temp1, zero);
    a2_8x16 = _mm_unpacklo_epi8(temp2, zero);

    a0_8x16 = _mm_add_epi16(a0_8x16, a2_8x16);
    a1_8x16 = _mm_add_epi16(a1_8x16, a1_8x16);
    a0_8x16 = _mm_add_epi16(a0_8x16, const_val2_8x16);
    a0_8x16 = _mm_add_epi16(a0_8x16, a1_8x16);
    res2_8x16 = _mm_srai_epi16(a0_8x16, 2);

    out_15x16 = _mm_packus_epi16(res1_8x16, res2_8x16);

    str_8x8 = _mm_srli_si128(out_15x16, 7);
    _mm_storel_epi64((__m128i *)(pu1_dst + 0 * dst_strd), str_8x8);
    str_8x8 = _mm_srli_si128(out_15x16, 6);
    _mm_storel_epi64((__m128i *)(pu1_dst + 1 * dst_strd), str_8x8);
    str_8x8 = _mm_srli_si128(out_15x16, 5);
    _mm_storel_epi64((__m128i *)(pu1_dst + 2 * dst_strd), str_8x8);
    str_8x8 = _mm_srli_si128(out_15x16, 4);
    _mm_storel_epi64((__m128i *)(pu1_dst + 3 * dst_strd), str_8x8);
    str_8x8 = _mm_srli_si128(out_15x16, 3);
    _mm_storel_epi64((__m128i *)(pu1_dst + 4 * dst_strd), str_8x8);
    str_8x8 = _mm_srli_si128(out_15x16, 2);
    _mm_storel_epi64((__m128i *)(pu1_dst + 5 * dst_strd), str_8x8);
    str_8x8 = _mm_srli_si128(out_15x16, 1);
    _mm_storel_epi64((__m128i *)(pu1_dst + 6 * dst_strd), str_8x8);
    _mm_storel_epi64((__m128i *)(pu1_dst + 7 * dst_strd), out_15x16);
}

/**
 *******************************************************************************
 *
 * ih264_intra_pred_luma_8x8_mode_vert_r_ssse3
 *
 * @brief
 *  Perform Intra prediction for luma_8x8 mode:Vertical_Right
 *
 * @par Description:
 *  Perform Intra prediction for luma_8x8 mode:Vertical_Right ,described in sec 8.3.2.2.7
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
void ih264_intra_pred_luma_8x8_mode_vert_r_ssse3(UWORD8 *pu1_src,
                                                 UWORD8 *pu1_dst,
                                                 WORD32 src_strd,
                                                 WORD32 dst_strd,
                                                 WORD32 ngbr_avail)
{
    UWORD8 *pu1_left = NULL; /* Pointer to start of left predictors */
    UWORD8 *pu1_top = NULL; /* Pointer to start of top predictors */
    __m128i top_8x8, left_16x8;
    __m128i out1_16x16, out2_16x16;
    __m128i a0_8x16, a1_8x16, a2_8x16;
    __m128i temp1, temp2;
    __m128i res1_8x16, res2_8x16, res3_8x16;
    __m128i zero = _mm_setzero_si128();
    __m128i const_val2_8x16 = _mm_set1_epi16(2);
    __m128i str_8x8;
    __m128i mask = _mm_set1_epi32(0xFFFF);

    UNUSED(src_strd);
    UNUSED(ngbr_avail);

    pu1_left = pu1_src + BLK8x8SIZE - 1;
    pu1_top = pu1_src + BLK8x8SIZE + 1;

    left_16x8 = _mm_loadu_si128((__m128i *)(pu1_left - 6));

    temp1 = _mm_srli_si128(left_16x8, 1);
    temp2 = _mm_srli_si128(left_16x8, 2);
    a0_8x16 = _mm_unpacklo_epi8(left_16x8, zero);
    a1_8x16 = _mm_unpacklo_epi8(temp1, zero);
    a2_8x16 = _mm_unpacklo_epi8(temp2, zero);

    a0_8x16 = _mm_add_epi16(a0_8x16, a2_8x16);
    a1_8x16 = _mm_add_epi16(a1_8x16, a1_8x16);
    a0_8x16 = _mm_add_epi16(a0_8x16, const_val2_8x16);
    a0_8x16 = _mm_add_epi16(a0_8x16, a1_8x16);
    res1_8x16 = _mm_srai_epi16(a0_8x16, 2);

    top_8x8 = _mm_loadu_si128((__m128i *)(pu1_top - 1));

    temp1 = _mm_srli_si128(top_8x8, 1);
    temp2 = _mm_srli_si128(top_8x8, 2);
    a0_8x16 = _mm_unpacklo_epi8(top_8x8, zero);
    a1_8x16 = _mm_unpacklo_epi8(temp1, zero);
    a2_8x16 = _mm_unpacklo_epi8(temp2, zero);

    res3_8x16 = _mm_avg_epu16(a0_8x16, a1_8x16);

    a0_8x16 = _mm_add_epi16(a0_8x16, a2_8x16);
    a1_8x16 = _mm_add_epi16(a1_8x16, a1_8x16);
    a0_8x16 = _mm_add_epi16(a0_8x16, const_val2_8x16);
    a0_8x16 = _mm_add_epi16(a0_8x16, a1_8x16);
    res2_8x16 = _mm_srai_epi16(a0_8x16, 2);

    str_8x8 = _mm_packus_epi16(res3_8x16, zero);
    _mm_storel_epi64((__m128i *)(pu1_dst + 0 * dst_strd), str_8x8);

    temp1 = _mm_and_si128(res1_8x16, mask);
    temp1 = _mm_packs_epi32(temp1, temp1);
    out1_16x16 = _mm_packus_epi16(temp1, res2_8x16);

    res1_8x16 = _mm_slli_si128(res1_8x16, 2);
    temp1 = _mm_and_si128(res1_8x16, mask);
    temp1 = _mm_packs_epi32(temp1, temp1);
    out2_16x16 = _mm_packus_epi16(temp1, res3_8x16);

    str_8x8 = _mm_srli_si128(out1_16x16, 7);
    _mm_storel_epi64((__m128i *)(pu1_dst + 1 * dst_strd), str_8x8);

    str_8x8 = _mm_srli_si128(out2_16x16, 7);
    _mm_storel_epi64((__m128i *)(pu1_dst + 2 * dst_strd), str_8x8);

    str_8x8 = _mm_srli_si128(out1_16x16, 6);
    _mm_storel_epi64((__m128i *)(pu1_dst + 3 * dst_strd), str_8x8);

    str_8x8 = _mm_srli_si128(out2_16x16, 6);
    _mm_storel_epi64((__m128i *)(pu1_dst + 4 * dst_strd), str_8x8);

    str_8x8 = _mm_srli_si128(out1_16x16, 5);
    _mm_storel_epi64((__m128i *)(pu1_dst + 5 * dst_strd), str_8x8);

    str_8x8 = _mm_srli_si128(out2_16x16, 5);
    _mm_storel_epi64((__m128i *)(pu1_dst + 6 * dst_strd), str_8x8);

    str_8x8 = _mm_srli_si128(out1_16x16, 4);
    _mm_storel_epi64((__m128i *)(pu1_dst + 7 * dst_strd), str_8x8);
}

/*
 *******************************************************************************
 *
 * ih264_intra_pred_luma_8x8_mode_horz_d_ssse3
 *
 * @brief
 *  Perform Intra prediction for luma_8x8 mode:Horizontal_Down
 *
 * @par Description:
 *  Perform Intra prediction for luma_8x8 mode:Horizontal_Down ,described in sec 8.3.2.2.8
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
void ih264_intra_pred_luma_8x8_mode_horz_d_ssse3(UWORD8 *pu1_src,
                                                 UWORD8 *pu1_dst,
                                                 WORD32 src_strd,
                                                 WORD32 dst_strd,
                                                 WORD32 ngbr_avail)
{
    UWORD8 *pu1_left = NULL; /* Pointer to start of left predictors */
    __m128i pels_16x16;
    __m128i temp1, temp2, temp3, temp4;
    __m128i a0_8x16, a1_8x16, a2_8x16;
    __m128i zero = _mm_setzero_si128();
    __m128i const_val2_8x16 = _mm_set1_epi16(2);
    __m128i res1_8x16, res2_8x16;
    __m128i out1_16x16, out2_16x16;
    __m128i str_8x8;
    UNUSED(src_strd);
    UNUSED(ngbr_avail);

    pu1_left = pu1_src + BLK8x8SIZE - 1;

    pels_16x16 = _mm_loadu_si128((__m128i *)(pu1_left - 7));

    temp1 = _mm_srli_si128(pels_16x16, 1);
    temp2 = _mm_srli_si128(pels_16x16, 2);
    a0_8x16 = _mm_unpacklo_epi8(pels_16x16, zero);
    a1_8x16 = _mm_unpacklo_epi8(temp1, zero);
    a2_8x16 = _mm_unpacklo_epi8(temp2, zero);

    res1_8x16 = _mm_avg_epu16(a0_8x16, a1_8x16);

    a0_8x16 = _mm_add_epi16(a0_8x16, a2_8x16);
    a1_8x16 = _mm_add_epi16(a1_8x16, a1_8x16);
    a0_8x16 = _mm_add_epi16(a0_8x16, const_val2_8x16);
    a0_8x16 = _mm_add_epi16(a0_8x16, a1_8x16);
    res2_8x16 = _mm_srai_epi16(a0_8x16, 2);

    temp3 = _mm_unpacklo_epi16(res1_8x16, res2_8x16);
    temp4 = _mm_unpackhi_epi16(res1_8x16, res2_8x16);
    out2_16x16 = _mm_packus_epi16(temp3, temp4);

    a0_8x16 = _mm_unpackhi_epi8(pels_16x16, zero);
    a1_8x16 = _mm_unpackhi_epi8(temp1, zero);
    a2_8x16 = _mm_unpackhi_epi8(temp2, zero);

    a0_8x16 = _mm_add_epi16(a0_8x16, a2_8x16);
    a1_8x16 = _mm_add_epi16(a1_8x16, a1_8x16);
    a0_8x16 = _mm_add_epi16(a0_8x16, const_val2_8x16);
    a0_8x16 = _mm_add_epi16(a0_8x16, a1_8x16);
    res2_8x16 = _mm_srai_epi16(a0_8x16, 2);

    out1_16x16 = _mm_packus_epi16(res2_8x16, zero);
    temp1 = _mm_srli_si128(out2_16x16, 8);
    out1_16x16 = _mm_unpacklo_epi64(temp1, out1_16x16);

    str_8x8 = _mm_srli_si128(out1_16x16, 6);
    _mm_storel_epi64((__m128i *)(pu1_dst + 0 * dst_strd), str_8x8);
    str_8x8 = _mm_srli_si128(out1_16x16, 4);
    _mm_storel_epi64((__m128i *)(pu1_dst + 1 * dst_strd), str_8x8);
    str_8x8 = _mm_srli_si128(out1_16x16, 2);
    _mm_storel_epi64((__m128i *)(pu1_dst + 2 * dst_strd), str_8x8);
    _mm_storel_epi64((__m128i *)(pu1_dst + 3 * dst_strd), out1_16x16);

    str_8x8 = _mm_srli_si128(out2_16x16, 6);
    _mm_storel_epi64((__m128i *)(pu1_dst + 4 * dst_strd), str_8x8);
    str_8x8 = _mm_srli_si128(out2_16x16, 4);
    _mm_storel_epi64((__m128i *)(pu1_dst + 5 * dst_strd), str_8x8);
    str_8x8 = _mm_srli_si128(out2_16x16, 2);
    _mm_storel_epi64((__m128i *)(pu1_dst + 6 * dst_strd), str_8x8);
    _mm_storel_epi64((__m128i *)(pu1_dst + 7 * dst_strd), out2_16x16);
}

/**
 *******************************************************************************
 *
 * ih264_intra_pred_luma_8x8_mode_vert_l_ssse3
 *
 * @brief
 *  Perform Intra prediction for luma_8x8 mode:Vertical_Left
 *
 * @par Description:
 *  Perform Intra prediction for luma_8x8 mode:Vertical_Left ,described in sec 8.3.2.2.9
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

void ih264_intra_pred_luma_8x8_mode_vert_l_ssse3(UWORD8 *pu1_src,
                                                 UWORD8 *pu1_dst,
                                                 WORD32 src_strd,
                                                 WORD32 dst_strd,
                                                 WORD32 ngbr_avail)
{
    UWORD8 *pu1_top = NULL; /* Pointer to start of top predictors */
    __m128i top_16x16;
    __m128i temp1, temp2;
    __m128i a0_8x16, a1_8x16, a2_8x16;
    __m128i zero = _mm_setzero_si128();
    __m128i const_val2_8x16 = _mm_set1_epi16(2);
    __m128i res1_8x16, res2_8x16, res3_8x16, res4_8x16;
    __m128i out1_16x16, out2_16x16;
    UNUSED(src_strd);
    UNUSED(ngbr_avail);
    pu1_top = pu1_src + BLK8x8SIZE + 1;

    top_16x16 = _mm_loadu_si128((__m128i *)(pu1_top));
    temp1 = _mm_srli_si128(top_16x16, 1);
    temp2 = _mm_srli_si128(top_16x16, 2);
    a0_8x16 = _mm_unpacklo_epi8(top_16x16, zero);
    a1_8x16 = _mm_unpacklo_epi8(temp1, zero);
    a2_8x16 = _mm_unpacklo_epi8(temp2, zero);

    res1_8x16 = _mm_avg_epu16(a0_8x16, a1_8x16);

    a0_8x16 = _mm_add_epi16(a0_8x16, a2_8x16);
    a1_8x16 = _mm_add_epi16(a1_8x16, a1_8x16);
    a0_8x16 = _mm_add_epi16(a0_8x16, const_val2_8x16);
    a0_8x16 = _mm_add_epi16(a0_8x16, a1_8x16);
    res2_8x16 = _mm_srai_epi16(a0_8x16, 2);

    a0_8x16 = _mm_unpackhi_epi8(top_16x16, zero);
    a1_8x16 = _mm_unpackhi_epi8(temp1, zero);
    a2_8x16 = _mm_unpackhi_epi8(temp2, zero);

    res3_8x16 = _mm_avg_epu16(a0_8x16, a1_8x16);

    a0_8x16 = _mm_add_epi16(a0_8x16, a2_8x16);
    a1_8x16 = _mm_add_epi16(a1_8x16, a1_8x16);
    a0_8x16 = _mm_add_epi16(a0_8x16, const_val2_8x16);
    a0_8x16 = _mm_add_epi16(a0_8x16, a1_8x16);
    res4_8x16 = _mm_srai_epi16(a0_8x16, 2);

    out1_16x16 = _mm_packus_epi16(res1_8x16, res3_8x16);
    out2_16x16 = _mm_packus_epi16(res2_8x16, res4_8x16);

    _mm_storel_epi64((__m128i *)(pu1_dst + 0 * dst_strd), out1_16x16);
    _mm_storel_epi64((__m128i *)(pu1_dst + 1 * dst_strd), out2_16x16);
    out1_16x16 = _mm_srli_si128(out1_16x16, 1);
    out2_16x16 = _mm_srli_si128(out2_16x16, 1);
    _mm_storel_epi64((__m128i *)(pu1_dst + 2 * dst_strd), out1_16x16);
    _mm_storel_epi64((__m128i *)(pu1_dst + 3 * dst_strd), out2_16x16);
    out1_16x16 = _mm_srli_si128(out1_16x16, 1);
    out2_16x16 = _mm_srli_si128(out2_16x16, 1);
    _mm_storel_epi64((__m128i *)(pu1_dst + 4 * dst_strd), out1_16x16);
    _mm_storel_epi64((__m128i *)(pu1_dst + 5 * dst_strd), out2_16x16);
    out1_16x16 = _mm_srli_si128(out1_16x16, 1);
    out2_16x16 = _mm_srli_si128(out2_16x16, 1);
    _mm_storel_epi64((__m128i *)(pu1_dst + 6 * dst_strd), out1_16x16);
    _mm_storel_epi64((__m128i *)(pu1_dst + 7 * dst_strd), out2_16x16);
}

/**
 *******************************************************************************
 *
 * ih264_intra_pred_luma_8x8_mode_horz_u_ssse3
 *
 * @brief
 *  Perform Intra prediction for luma_8x8 mode:Horizontal_Up
 *
 * @par Description:
 *  Perform Intra prediction for luma_8x8 mode:Horizontal_Up ,described in sec 8.3.2.2.10
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
void ih264_intra_pred_luma_8x8_mode_horz_u_ssse3(UWORD8 *pu1_src,
                                                 UWORD8 *pu1_dst,
                                                 WORD32 src_strd,
                                                 WORD32 dst_strd,
                                                 WORD32 ngbr_avail)
{
    UWORD8 *pu1_left = NULL; /* Pointer to start of left predictors */
    __m128i left_16x16;
    __m128i temp1, temp2;
    __m128i a0_8x16, a1_8x16, a2_8x16;
    __m128i zero = _mm_setzero_si128();
    __m128i const_val2_8x16 = _mm_set1_epi16(2);
    __m128i res1_8x16, res2_8x16;
    __m128i out1_16x16;
    __m128i str_8x8;
    __m128i shuffle_16x16;
    UNUSED(src_strd);
    UNUSED(ngbr_avail);

    pu1_left = pu1_src + BLK8x8SIZE - 1;
    shuffle_16x16 = _mm_set_epi8(0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
                                 0x0F);

    left_16x16 = _mm_loadu_si128((__m128i *)(pu1_left - 7));
    temp1 = _mm_srli_si128(left_16x16, 1);
    a0_8x16 = _mm_unpacklo_epi8(left_16x16, zero);
    a0_8x16 = _mm_slli_si128(a0_8x16, 2);
    a1_8x16 = _mm_unpacklo_epi8(left_16x16, zero);
    a0_8x16 = _mm_shufflelo_epi16(a0_8x16, 0xE5);
    a2_8x16 = _mm_unpacklo_epi8(temp1, zero);

    res1_8x16 = _mm_avg_epu16(a0_8x16, a1_8x16);

    a0_8x16 = _mm_add_epi16(a0_8x16, a2_8x16);
    a1_8x16 = _mm_add_epi16(a1_8x16, a1_8x16);
    a0_8x16 = _mm_add_epi16(a0_8x16, const_val2_8x16);
    a0_8x16 = _mm_add_epi16(a0_8x16, a1_8x16);
    res2_8x16 = _mm_srai_epi16(a0_8x16, 2);

    temp1 = _mm_unpacklo_epi16(res1_8x16, res2_8x16);
    temp2 = _mm_unpackhi_epi16(res1_8x16, res2_8x16);
    out1_16x16 = _mm_packus_epi16(temp1, temp2);
    out1_16x16 = _mm_shuffle_epi8(out1_16x16, shuffle_16x16);

    str_8x8 = _mm_srli_si128(out1_16x16, 1);
    _mm_storel_epi64((__m128i *)(pu1_dst + 0 * dst_strd), str_8x8);
    str_8x8 = _mm_srli_si128(out1_16x16, 3);
    _mm_storel_epi64((__m128i *)(pu1_dst + 1 * dst_strd), str_8x8);
    str_8x8 = _mm_srli_si128(out1_16x16, 5);
    _mm_storel_epi64((__m128i *)(pu1_dst + 2 * dst_strd), str_8x8);
    str_8x8 = _mm_srli_si128(out1_16x16, 7);
    _mm_storel_epi64((__m128i *)(pu1_dst + 3 * dst_strd), str_8x8);
    temp1 = _mm_set1_epi8(pu1_left[-7]);
    str_8x8 = _mm_unpacklo_epi64(str_8x8, temp1);
    str_8x8 = _mm_srli_si128(str_8x8, 2);
    _mm_storel_epi64((__m128i *)(pu1_dst + 4 * dst_strd), str_8x8);
    str_8x8 = _mm_srli_si128(str_8x8, 2);
    _mm_storel_epi64((__m128i *)(pu1_dst + 5 * dst_strd), str_8x8);
    str_8x8 = _mm_srli_si128(str_8x8, 2);
    _mm_storel_epi64((__m128i *)(pu1_dst + 6 * dst_strd), str_8x8);
    str_8x8 = _mm_srli_si128(str_8x8, 2);
    _mm_storel_epi64((__m128i *)(pu1_dst + 7 * dst_strd), str_8x8);

}


/*******************    16x16 Modes    *******************/

/**
 *******************************************************************************
 *
 *ih264_intra_pred_luma_16x16_mode_vert_ssse3
 *
 * @brief
 *  Perform Intra prediction for luma_16x16 mode:Vertical
 *
 * @par Description:
 *  Perform Intra prediction for luma_16x16 mode:Vertical, described in sec 8.3.3.1
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
void ih264_intra_pred_luma_16x16_mode_vert_ssse3(UWORD8 *pu1_src,
                                                 UWORD8 *pu1_dst,
                                                 WORD32 src_strd,
                                                 WORD32 dst_strd,
                                                 WORD32 ngbr_avail)
{
    UWORD8 *pu1_top;
    WORD32 dst_strd2, dst_strd3, dst_strd4;

    __m128i top_16x8b;

    UNUSED(src_strd);
    UNUSED(ngbr_avail);

    pu1_top = pu1_src + MB_SIZE + 1;

    dst_strd2 = dst_strd << 1;
    dst_strd4 = dst_strd << 2;

    top_16x8b = _mm_loadu_si128((__m128i *)pu1_top);

    dst_strd3 = dst_strd + dst_strd2;

    _mm_storeu_si128((__m128i *)pu1_dst, top_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), top_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd2), top_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd3), top_16x8b);
    pu1_dst += dst_strd4;

    _mm_storeu_si128((__m128i *)pu1_dst, top_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), top_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd2), top_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd3), top_16x8b);
    pu1_dst += dst_strd4;

    _mm_storeu_si128((__m128i *)pu1_dst, top_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), top_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd2), top_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd3), top_16x8b);
    pu1_dst += dst_strd4;

    _mm_storeu_si128((__m128i *)pu1_dst, top_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), top_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd2), top_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd3), top_16x8b);
}

/**
 *******************************************************************************
 *
 *ih264_intra_pred_luma_16x16_mode_horz_ssse3
 *
 * @brief
 *  Perform Intra prediction for luma_16x16 mode:Horizontal
 *
 * @par Description:
 *  Perform Intra prediction for luma_16x16 mode:Horizontal, described in sec 8.3.3.2
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
void ih264_intra_pred_luma_16x16_mode_horz_ssse3(UWORD8 *pu1_src,
                                                 UWORD8 *pu1_dst,
                                                 WORD32 src_strd,
                                                 WORD32 dst_strd,
                                                 WORD32 ngbr_avail)
{
    UWORD8 *pu1_left;
    WORD32 dst_strd2, dst_strd3, dst_strd4;

    __m128i row1_16x8b, row2_16x8b, row3_16x8b, row4_16x8b;

    UNUSED(src_strd);
    UNUSED(ngbr_avail);

    pu1_left = pu1_src + MB_SIZE - 1;

    dst_strd4 = dst_strd << 2;

    dst_strd2 = dst_strd << 1;
    dst_strd3 = dst_strd4 - dst_strd;

    row1_16x8b = _mm_set1_epi8(*(pu1_left));
    row2_16x8b = _mm_set1_epi8(*(pu1_left - 1));
    row3_16x8b = _mm_set1_epi8(*(pu1_left - 2));
    row4_16x8b = _mm_set1_epi8(*(pu1_left - 3));

    _mm_storeu_si128((__m128i *)pu1_dst, row1_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), row2_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd2), row3_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd3), row4_16x8b);

    pu1_dst += dst_strd4;
    row1_16x8b = _mm_set1_epi8(*(pu1_left - 4));
    row2_16x8b = _mm_set1_epi8(*(pu1_left - 5));
    row3_16x8b = _mm_set1_epi8(*(pu1_left - 6));
    row4_16x8b = _mm_set1_epi8(*(pu1_left - 7));

    _mm_storeu_si128((__m128i *)pu1_dst, row1_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), row2_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd2), row3_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd3), row4_16x8b);

    pu1_dst += dst_strd4;
    row1_16x8b = _mm_set1_epi8(*(pu1_left - 8));
    row2_16x8b = _mm_set1_epi8(*(pu1_left - 9));
    row3_16x8b = _mm_set1_epi8(*(pu1_left - 10));
    row4_16x8b = _mm_set1_epi8(*(pu1_left - 11));

    _mm_storeu_si128((__m128i *)pu1_dst, row1_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), row2_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd2), row3_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd3), row4_16x8b);

    pu1_dst += dst_strd4;
    row1_16x8b = _mm_set1_epi8(*(pu1_left - 12));
    row2_16x8b = _mm_set1_epi8(*(pu1_left - 13));
    row3_16x8b = _mm_set1_epi8(*(pu1_left - 14));
    row4_16x8b = _mm_set1_epi8(*(pu1_left - 15));

    _mm_storeu_si128((__m128i *)pu1_dst, row1_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), row2_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd2), row3_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd3), row4_16x8b);
}

/**
 *******************************************************************************
 *
 *ih264_intra_pred_luma_16x16_mode_dc_ssse3
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
void ih264_intra_pred_luma_16x16_mode_dc_ssse3(UWORD8 *pu1_src,
                                               UWORD8 *pu1_dst,
                                               WORD32 src_strd,
                                               WORD32 dst_strd,
                                               WORD32 ngbr_avail)
{
    WORD8 u1_useleft, u1_usetop;
    WORD32 dc_val;

    WORD32 dst_strd2, dst_strd3, dst_strd4;

    __m128i dc_val_16x8b;

    UNUSED(src_strd);

    u1_useleft = BOOLEAN(ngbr_avail & LEFT_MB_AVAILABLE_MASK);
    u1_usetop = BOOLEAN(ngbr_avail & TOP_MB_AVAILABLE_MASK);

    if(u1_useleft || u1_usetop)
    {
        WORD32 shft;
        __m128i val_16x8b, zero_16x8b, sum_8x16b;

        dc_val = 0;
        shft = 3;

        zero_16x8b = _mm_setzero_si128();

        if(u1_useleft)
        {
            UWORD8 *pu1_left;

            pu1_left = pu1_src + MB_SIZE - 1;

            val_16x8b = _mm_loadu_si128((__m128i *)(pu1_left - 15));
            sum_8x16b = _mm_sad_epu8(zero_16x8b, val_16x8b);

            shft++;
            dc_val += 8;
            dc_val += _mm_extract_epi16(sum_8x16b, 0);
            dc_val += _mm_extract_epi16(sum_8x16b, 4);
        }
        if(u1_usetop)
        {
            UWORD8 *pu1_top;

            pu1_top = pu1_src + MB_SIZE + 1;

            val_16x8b = _mm_loadu_si128((__m128i *)pu1_top);
            sum_8x16b = _mm_sad_epu8(zero_16x8b, val_16x8b);

            shft++;
            dc_val += 8;
            dc_val += _mm_extract_epi16(sum_8x16b, 0);
            dc_val += _mm_extract_epi16(sum_8x16b, 4);
        }
        dc_val = dc_val >> shft;
    }
    else
        dc_val = 128;

    dc_val_16x8b =  _mm_set1_epi8(dc_val);

    dst_strd2 = dst_strd << 1;
    dst_strd4 = dst_strd << 2;
    dst_strd3 = dst_strd + dst_strd2;

    _mm_storeu_si128((__m128i *)pu1_dst, dc_val_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), dc_val_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd2), dc_val_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd3), dc_val_16x8b);
    pu1_dst += dst_strd4;

    _mm_storeu_si128((__m128i *)pu1_dst, dc_val_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), dc_val_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd2), dc_val_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd3), dc_val_16x8b);
    pu1_dst += dst_strd4;

    _mm_storeu_si128((__m128i *)pu1_dst, dc_val_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), dc_val_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd2), dc_val_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd3), dc_val_16x8b);
    pu1_dst += dst_strd4;

    _mm_storeu_si128((__m128i *)pu1_dst, dc_val_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), dc_val_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd2), dc_val_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd3), dc_val_16x8b);
}

/**
 *******************************************************************************
 *
 *ih264_intra_pred_luma_16x16_mode_plane_ssse3
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
void ih264_intra_pred_luma_16x16_mode_plane_ssse3(UWORD8 *pu1_src,
                                                  UWORD8 *pu1_dst,
                                                  WORD32 src_strd,
                                                  WORD32 dst_strd,
                                                  WORD32 ngbr_avail)
{
    UWORD8 *pu1_left, *pu1_top;
    WORD32 a, b, c;

    __m128i rev_8x16b, mul_8x16b, zero_16x8b;

    UNUSED(src_strd);
    UNUSED(ngbr_avail);

    pu1_top = pu1_src + MB_SIZE + 1;
    pu1_left = pu1_src + MB_SIZE - 1;

    rev_8x16b = _mm_setr_epi16(0x0f0e, 0x0d0c, 0x0b0a, 0x0908, 0x0706, 0x0504, 0x0302, 0x0100);
    //used to reverse the order of 16-bit values in a vector

    mul_8x16b = _mm_setr_epi16(1, 2, 3, 4, 5, 6, 7, 8);
    zero_16x8b = _mm_setzero_si128();

    //calculating a, b and c
    {
        WORD32 h, v;

        __m128i h_val1_16x8b, h_val2_16x8b;
        __m128i h_val1_8x16b, h_val2_8x16b, h_val_4x32b;
        __m128i v_val1_16x8b, v_val2_16x8b;
        __m128i v_val1_8x16b, v_val2_8x16b, v_val_4x32b;
        __m128i hv_val_4x32b;

        a = (pu1_top[15] + pu1_left[-15]) << 4;

        h_val1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_top + 8));
        h_val2_16x8b = _mm_loadl_epi64((__m128i *)(pu1_top - 1));
        v_val1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_left - 15));
        v_val2_16x8b = _mm_loadl_epi64((__m128i *)(pu1_left - 6));

        h_val1_8x16b = _mm_unpacklo_epi8(h_val1_16x8b, zero_16x8b);
        h_val2_8x16b = _mm_unpacklo_epi8(h_val2_16x8b, zero_16x8b);
        v_val1_8x16b = _mm_unpacklo_epi8(v_val1_16x8b, zero_16x8b);
        v_val2_8x16b = _mm_unpacklo_epi8(v_val2_16x8b, zero_16x8b);

        h_val2_8x16b = _mm_shuffle_epi8(h_val2_8x16b, rev_8x16b);
        v_val1_8x16b = _mm_shuffle_epi8(v_val1_8x16b, rev_8x16b);

        h_val1_8x16b = _mm_sub_epi16(h_val1_8x16b, h_val2_8x16b);
        v_val1_8x16b = _mm_sub_epi16(v_val1_8x16b, v_val2_8x16b);

        h_val_4x32b = _mm_madd_epi16(mul_8x16b, h_val1_8x16b);
        v_val_4x32b = _mm_madd_epi16(mul_8x16b, v_val1_8x16b);

        hv_val_4x32b = _mm_hadd_epi32(h_val_4x32b, v_val_4x32b);
        hv_val_4x32b = _mm_hadd_epi32(hv_val_4x32b, hv_val_4x32b);

        h = _mm_extract_epi16(hv_val_4x32b, 0);
        v = _mm_extract_epi16(hv_val_4x32b, 2);
        h = (h << 16) >> 16;
        v = (v << 16) >> 16;

        b = ((h << 2) + h + 32) >> 6;
        c = ((v << 2) + v + 32) >> 6;
    }

    //using a, b and c to compute the fitted plane values
    {
        __m128i const_8x16b, b_8x16b, c_8x16b, c2_8x16b;
        __m128i res1_l_8x16b, res1_h_8x16b;
        __m128i res2_l_8x16b, res2_h_8x16b;
        __m128i res1_sh_l_8x16b, res1_sh_h_8x16b, res1_16x8b;
        __m128i res2_sh_l_8x16b, res2_sh_h_8x16b, res2_16x8b;

        b_8x16b = _mm_set1_epi16(b);
        c_8x16b = _mm_set1_epi16(c);
        c2_8x16b = _mm_set1_epi16(c << 1);
        const_8x16b = _mm_set1_epi16(a - c*7 + 16);

        res1_h_8x16b = _mm_mullo_epi16(mul_8x16b, b_8x16b);
        //contains {b*1, b*2, b*3,... b*8}

        res1_l_8x16b = _mm_shuffle_epi8(res1_h_8x16b, rev_8x16b);
        res1_l_8x16b = _mm_srli_si128(res1_l_8x16b, 2);
        res1_l_8x16b = _mm_sub_epi16(zero_16x8b, res1_l_8x16b);
        //contains {-b*7, -b*6,... -b*1, b*0}

        // rows 1, 2
        res1_h_8x16b = _mm_add_epi16(res1_h_8x16b, const_8x16b);
        res1_l_8x16b = _mm_add_epi16(res1_l_8x16b, const_8x16b);
        res2_h_8x16b = _mm_add_epi16(res1_h_8x16b, c_8x16b);
        res2_l_8x16b = _mm_add_epi16(res1_l_8x16b, c_8x16b);

        res1_sh_h_8x16b = _mm_srai_epi16(res1_h_8x16b, 5);
        res1_sh_l_8x16b = _mm_srai_epi16(res1_l_8x16b, 5);
        res2_sh_h_8x16b = _mm_srai_epi16(res2_h_8x16b, 5);
        res2_sh_l_8x16b = _mm_srai_epi16(res2_l_8x16b, 5);

        res1_16x8b = _mm_packus_epi16(res1_sh_l_8x16b, res1_sh_h_8x16b);
        res2_16x8b = _mm_packus_epi16(res2_sh_l_8x16b, res2_sh_h_8x16b);

        _mm_storeu_si128((__m128i *)pu1_dst, res1_16x8b);
        _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), res2_16x8b);

        // rows 3, 4
        res1_h_8x16b = _mm_add_epi16(res1_h_8x16b, c2_8x16b);
        res1_l_8x16b = _mm_add_epi16(res1_l_8x16b, c2_8x16b);
        res2_h_8x16b = _mm_add_epi16(res2_h_8x16b, c2_8x16b);
        res2_l_8x16b = _mm_add_epi16(res2_l_8x16b, c2_8x16b);

        res1_sh_h_8x16b = _mm_srai_epi16(res1_h_8x16b, 5);
        res1_sh_l_8x16b = _mm_srai_epi16(res1_l_8x16b, 5);
        res2_sh_h_8x16b = _mm_srai_epi16(res2_h_8x16b, 5);
        res2_sh_l_8x16b = _mm_srai_epi16(res2_l_8x16b, 5);

        pu1_dst += dst_strd << 1;

        res1_16x8b = _mm_packus_epi16(res1_sh_l_8x16b, res1_sh_h_8x16b);
        res2_16x8b = _mm_packus_epi16(res2_sh_l_8x16b, res2_sh_h_8x16b);

        _mm_storeu_si128((__m128i *)pu1_dst, res1_16x8b);
        _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), res2_16x8b);

        // rows 5, 6
        res1_h_8x16b = _mm_add_epi16(res1_h_8x16b, c2_8x16b);
        res1_l_8x16b = _mm_add_epi16(res1_l_8x16b, c2_8x16b);
        res2_h_8x16b = _mm_add_epi16(res2_h_8x16b, c2_8x16b);
        res2_l_8x16b = _mm_add_epi16(res2_l_8x16b, c2_8x16b);

        res1_sh_h_8x16b = _mm_srai_epi16(res1_h_8x16b, 5);
        res1_sh_l_8x16b = _mm_srai_epi16(res1_l_8x16b, 5);
        res2_sh_h_8x16b = _mm_srai_epi16(res2_h_8x16b, 5);
        res2_sh_l_8x16b = _mm_srai_epi16(res2_l_8x16b, 5);

        pu1_dst += dst_strd << 1;

        res1_16x8b = _mm_packus_epi16(res1_sh_l_8x16b, res1_sh_h_8x16b);
        res2_16x8b = _mm_packus_epi16(res2_sh_l_8x16b, res2_sh_h_8x16b);

        _mm_storeu_si128((__m128i *)pu1_dst, res1_16x8b);
        _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), res2_16x8b);

        // rows 7, 8
        res1_h_8x16b = _mm_add_epi16(res1_h_8x16b, c2_8x16b);
        res1_l_8x16b = _mm_add_epi16(res1_l_8x16b, c2_8x16b);
        res2_h_8x16b = _mm_add_epi16(res2_h_8x16b, c2_8x16b);
        res2_l_8x16b = _mm_add_epi16(res2_l_8x16b, c2_8x16b);

        res1_sh_h_8x16b = _mm_srai_epi16(res1_h_8x16b, 5);
        res1_sh_l_8x16b = _mm_srai_epi16(res1_l_8x16b, 5);
        res2_sh_h_8x16b = _mm_srai_epi16(res2_h_8x16b, 5);
        res2_sh_l_8x16b = _mm_srai_epi16(res2_l_8x16b, 5);

        pu1_dst += dst_strd << 1;

        res1_16x8b = _mm_packus_epi16(res1_sh_l_8x16b, res1_sh_h_8x16b);
        res2_16x8b = _mm_packus_epi16(res2_sh_l_8x16b, res2_sh_h_8x16b);

        _mm_storeu_si128((__m128i *)pu1_dst, res1_16x8b);
        _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), res2_16x8b);

        // rows 9, 10
        res1_h_8x16b = _mm_add_epi16(res1_h_8x16b, c2_8x16b);
        res1_l_8x16b = _mm_add_epi16(res1_l_8x16b, c2_8x16b);
        res2_h_8x16b = _mm_add_epi16(res2_h_8x16b, c2_8x16b);
        res2_l_8x16b = _mm_add_epi16(res2_l_8x16b, c2_8x16b);

        res1_sh_h_8x16b = _mm_srai_epi16(res1_h_8x16b, 5);
        res1_sh_l_8x16b = _mm_srai_epi16(res1_l_8x16b, 5);
        res2_sh_h_8x16b = _mm_srai_epi16(res2_h_8x16b, 5);
        res2_sh_l_8x16b = _mm_srai_epi16(res2_l_8x16b, 5);

        pu1_dst += dst_strd << 1;

        res1_16x8b = _mm_packus_epi16(res1_sh_l_8x16b, res1_sh_h_8x16b);
        res2_16x8b = _mm_packus_epi16(res2_sh_l_8x16b, res2_sh_h_8x16b);

        _mm_storeu_si128((__m128i *)pu1_dst, res1_16x8b);
        _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), res2_16x8b);

        // rows 11, 12
        res1_h_8x16b = _mm_add_epi16(res1_h_8x16b, c2_8x16b);
        res1_l_8x16b = _mm_add_epi16(res1_l_8x16b, c2_8x16b);
        res2_h_8x16b = _mm_add_epi16(res2_h_8x16b, c2_8x16b);
        res2_l_8x16b = _mm_add_epi16(res2_l_8x16b, c2_8x16b);

        res1_sh_h_8x16b = _mm_srai_epi16(res1_h_8x16b, 5);
        res1_sh_l_8x16b = _mm_srai_epi16(res1_l_8x16b, 5);
        res2_sh_h_8x16b = _mm_srai_epi16(res2_h_8x16b, 5);
        res2_sh_l_8x16b = _mm_srai_epi16(res2_l_8x16b, 5);

        pu1_dst += dst_strd << 1;

        res1_16x8b = _mm_packus_epi16(res1_sh_l_8x16b, res1_sh_h_8x16b);
        res2_16x8b = _mm_packus_epi16(res2_sh_l_8x16b, res2_sh_h_8x16b);

        _mm_storeu_si128((__m128i *)pu1_dst, res1_16x8b);
        _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), res2_16x8b);

        // rows 13, 14
        res1_h_8x16b = _mm_add_epi16(res1_h_8x16b, c2_8x16b);
        res1_l_8x16b = _mm_add_epi16(res1_l_8x16b, c2_8x16b);
        res2_h_8x16b = _mm_add_epi16(res2_h_8x16b, c2_8x16b);
        res2_l_8x16b = _mm_add_epi16(res2_l_8x16b, c2_8x16b);

        res1_sh_h_8x16b = _mm_srai_epi16(res1_h_8x16b, 5);
        res1_sh_l_8x16b = _mm_srai_epi16(res1_l_8x16b, 5);
        res2_sh_h_8x16b = _mm_srai_epi16(res2_h_8x16b, 5);
        res2_sh_l_8x16b = _mm_srai_epi16(res2_l_8x16b, 5);

        pu1_dst += dst_strd << 1;

        res1_16x8b = _mm_packus_epi16(res1_sh_l_8x16b, res1_sh_h_8x16b);
        res2_16x8b = _mm_packus_epi16(res2_sh_l_8x16b, res2_sh_h_8x16b);

        _mm_storeu_si128((__m128i *)pu1_dst, res1_16x8b);
        _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), res2_16x8b);

        // rows 15, 16
        res1_h_8x16b = _mm_add_epi16(res1_h_8x16b, c2_8x16b);
        res1_l_8x16b = _mm_add_epi16(res1_l_8x16b, c2_8x16b);
        res2_h_8x16b = _mm_add_epi16(res2_h_8x16b, c2_8x16b);
        res2_l_8x16b = _mm_add_epi16(res2_l_8x16b, c2_8x16b);

        res1_sh_h_8x16b = _mm_srai_epi16(res1_h_8x16b, 5);
        res1_sh_l_8x16b = _mm_srai_epi16(res1_l_8x16b, 5);
        res2_sh_h_8x16b = _mm_srai_epi16(res2_h_8x16b, 5);
        res2_sh_l_8x16b = _mm_srai_epi16(res2_l_8x16b, 5);

        pu1_dst += dst_strd << 1;

        res1_16x8b = _mm_packus_epi16(res1_sh_l_8x16b, res1_sh_h_8x16b);
        res2_16x8b = _mm_packus_epi16(res2_sh_l_8x16b, res2_sh_h_8x16b);

        _mm_storeu_si128((__m128i *)pu1_dst, res1_16x8b);
        _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), res2_16x8b);
    }
}
