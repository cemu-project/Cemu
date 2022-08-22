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
*  ih264_chroma_intra_pred_filters_ssse3.c
*
* @brief
*  Contains function definitions for chroma intra prediction filters in x86
*  intrinsics
*
* @author
*  Ittiam
*
* @par List of Functions:
*  -ih264_intra_pred_chroma_8x8_mode_horz_ssse3
*  -ih264_intra_pred_chroma_8x8_mode_vert_ssse3
*  -ih264_intra_pred_chroma_8x8_mode_plane_ssse3
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


/*****************************************************************************/
/* Chroma Intra prediction 8x8 filters                                       */
/*****************************************************************************/
/**
*******************************************************************************
*
* ih264_intra_pred_chroma_8x8_mode_horz_ssse3
*
* @brief
*  Perform Intra prediction for chroma_8x8 mode:Horizontal
*
* @par Description:
*  Perform Intra prediction for chroma_8x8 mode:Horizontal ,described in sec 8.3.4.2
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
void ih264_intra_pred_chroma_8x8_mode_horz_ssse3(UWORD8 *pu1_src,
                                                 UWORD8 *pu1_dst,
                                                 WORD32 src_strd,
                                                 WORD32 dst_strd,
                                                 WORD32 ngbr_avail)
{

    UWORD8 *pu1_left; /* Pointer to start of top predictors */
    WORD32 dst_strd2;

    __m128i row1_16x8b, row2_16x8b;

    UNUSED(src_strd);
    UNUSED(ngbr_avail);

    pu1_left = pu1_src + 2 * BLK8x8SIZE - 2;


    dst_strd2 = dst_strd << 1;
    row1_16x8b = _mm_set1_epi16(*((WORD16 *)(pu1_left)));
    row2_16x8b = _mm_set1_epi16(*((WORD16 *)(pu1_left - 2)));
    _mm_storeu_si128((__m128i *)pu1_dst, row1_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), row2_16x8b);

    pu1_dst += dst_strd2;
    row1_16x8b = _mm_set1_epi16(*((WORD16 *)(pu1_left - 4)));
    row2_16x8b = _mm_set1_epi16(*((WORD16 *)(pu1_left - 6)));
    _mm_storeu_si128((__m128i *)pu1_dst, row1_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), row2_16x8b);

    pu1_dst += dst_strd2;
    row1_16x8b = _mm_set1_epi16(*((WORD16 *)(pu1_left - 8)));
    row2_16x8b = _mm_set1_epi16(*((WORD16 *)(pu1_left - 10)));
    _mm_storeu_si128((__m128i *)pu1_dst, row1_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), row2_16x8b);

    pu1_dst += dst_strd2;
    row1_16x8b = _mm_set1_epi16(*((WORD16 *)(pu1_left - 12)));
    row2_16x8b = _mm_set1_epi16(*((WORD16 *)(pu1_left - 14)));
    _mm_storeu_si128((__m128i *)pu1_dst, row1_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), row2_16x8b);
}

/**
*******************************************************************************
*
* ih264_intra_pred_chroma_8x8_mode_vert_ssse3
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
void ih264_intra_pred_chroma_8x8_mode_vert_ssse3(UWORD8 *pu1_src,
                                                 UWORD8 *pu1_dst,
                                                 WORD32 src_strd,
                                                 WORD32 dst_strd,
                                                 WORD32 ngbr_avail)
{
    UWORD8 *pu1_top; /* Pointer to start of top predictors */
    WORD32 dst_strd2;

    __m128i top_16x8b;

    UNUSED(src_strd);
    UNUSED(ngbr_avail);

    pu1_top = pu1_src + 2 * BLK8x8SIZE + 2;

    top_16x8b = _mm_loadu_si128((__m128i *)pu1_top);

    dst_strd2 = dst_strd << 1;
    _mm_storeu_si128((__m128i *)pu1_dst, top_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), top_16x8b);

    pu1_dst += dst_strd2;
    _mm_storeu_si128((__m128i *)pu1_dst, top_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), top_16x8b);

    pu1_dst += dst_strd2;
    _mm_storeu_si128((__m128i *)pu1_dst, top_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), top_16x8b);

    pu1_dst += dst_strd2;
    _mm_storeu_si128((__m128i *)pu1_dst, top_16x8b);
    _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), top_16x8b);
}

/**
*******************************************************************************
*
* ih264_intra_pred_chroma_8x8_mode_plane_ssse3
*
* @brief
*  Perform Intra prediction for chroma_8x8 mode:PLANE
*
* @par Description:
*  Perform Intra prediction for chroma_8x8 mode:PLANE ,described in sec 8.3.4.4
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
void ih264_intra_pred_chroma_8x8_mode_plane_ssse3(UWORD8 *pu1_src,
                                                  UWORD8 *pu1_dst,
                                                  WORD32 src_strd,
                                                  WORD32 dst_strd,
                                                  WORD32 ngbr_avail)
{
    UWORD8 *pu1_left, *pu1_top;
    WORD32 a_u, a_v, b_u, b_v, c_u, c_v;

    __m128i mul_8x16b, shuffle_8x16b;

    UNUSED(src_strd);
    UNUSED(ngbr_avail);

    pu1_top = pu1_src + MB_SIZE + 2;
    pu1_left = pu1_src + MB_SIZE - 2;

    mul_8x16b = _mm_setr_epi16(1, 2, 3, 4, 1, 2, 3, 4);
    shuffle_8x16b = _mm_setr_epi16(0xff00, 0xff02, 0xff04, 0xff06,
                                   0xff01, 0xff03, 0xff05, 0xff07);

    //calculating a, b and c
    {
        WORD32 h_u, h_v, v_u, v_v;

        __m128i h_val1_16x8b, h_val2_16x8b;
        __m128i h_val1_8x16b, h_val2_8x16b, h_val_4x32b;
        __m128i v_val1_16x8b, v_val2_16x8b;
        __m128i v_val1_8x16b, v_val2_8x16b, v_val_4x32b;
        __m128i hv_val_4x32b;

        h_val1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_top + 8));
        h_val2_16x8b = _mm_loadl_epi64((__m128i *)(pu1_top - 2));
        v_val1_16x8b = _mm_loadl_epi64((__m128i *)(pu1_left - 14));
        v_val2_16x8b = _mm_loadl_epi64((__m128i *)(pu1_left - 4));

        // reversing the order
        h_val2_16x8b = _mm_shufflelo_epi16(h_val2_16x8b, 0x1b);
        v_val1_16x8b = _mm_shufflelo_epi16(v_val1_16x8b, 0x1b);

        // separating u and v and 8-bit to 16-bit conversion
        h_val1_8x16b = _mm_shuffle_epi8(h_val1_16x8b, shuffle_8x16b);
        h_val2_8x16b = _mm_shuffle_epi8(h_val2_16x8b, shuffle_8x16b);
        v_val1_8x16b = _mm_shuffle_epi8(v_val1_16x8b, shuffle_8x16b);
        v_val2_8x16b = _mm_shuffle_epi8(v_val2_16x8b, shuffle_8x16b);

        h_val1_8x16b = _mm_sub_epi16(h_val1_8x16b, h_val2_8x16b);
        v_val1_8x16b = _mm_sub_epi16(v_val1_8x16b, v_val2_8x16b);

        h_val_4x32b = _mm_madd_epi16(mul_8x16b, h_val1_8x16b);
        v_val_4x32b = _mm_madd_epi16(mul_8x16b, v_val1_8x16b);

        hv_val_4x32b = _mm_hadd_epi32(h_val_4x32b, v_val_4x32b);

        a_u = (pu1_left[7 * (-2)] + pu1_top[14]) << 4;
        a_v = (pu1_left[7 * (-2) + 1] + pu1_top[15]) << 4;

        h_u = _mm_extract_epi16(hv_val_4x32b, 0);
        h_v = _mm_extract_epi16(hv_val_4x32b, 2);
        v_u = _mm_extract_epi16(hv_val_4x32b, 4);
        v_v = _mm_extract_epi16(hv_val_4x32b, 6);

        h_u = (h_u << 16) >> 15; // sign-extension and multiplication by 2
        h_v = (h_v << 16) >> 15;
        v_u = (v_u << 16) >> 15;
        v_v = (v_v << 16) >> 15;

        b_u = ((h_u << 4) + h_u + 32) >> 6;
        b_v = ((h_v << 4) + h_v + 32) >> 6;
        c_u = ((v_u << 4) + v_u + 32) >> 6;
        c_v = ((v_v << 4) + v_v + 32) >> 6;
    }
    //using a, b and c to compute the fitted plane values
    {
        __m128i const_8x16b, c2_8x16b;
        __m128i res1_l_8x16b, res1_h_8x16b;
        __m128i res2_l_8x16b, res2_h_8x16b;
        __m128i res1_sh_l_8x16b, res1_sh_h_8x16b, res1_16x8b;
        __m128i res2_sh_l_8x16b, res2_sh_h_8x16b, res2_16x8b;

        WORD32 b_u2, b_v2, b_u3, b_v3;
        WORD32 const_u, const_v;
        WORD32 dst_strd2;

        const_u = a_u - (c_u << 1) - c_u + 16;
        const_v = a_v - (c_v << 1) - c_v + 16;

        b_u2 = b_u << 1;
        b_v2 = b_v << 1;
        b_u3 = b_u + b_u2;
        b_v3 = b_v + b_v2;

        const_8x16b = _mm_setr_epi16(const_u, const_v, const_u, const_v, const_u, const_v, const_u, const_v);
        res1_l_8x16b = _mm_setr_epi16(-b_u3, -b_v3, -b_u2, -b_v2, -b_u, -b_v, 0, 0);
        //contains {-b*3, -b*2, -b*1, b*0}
        res1_h_8x16b = _mm_setr_epi16(b_u, b_v, b_u2, b_v2, b_u3, b_v3, b_u << 2, b_v << 2);
        //contains {b*1, b*2, b*3, b*4}
        c2_8x16b = _mm_setr_epi16(c_u, c_v, c_u, c_v, c_u, c_v, c_u, c_v);

        // rows 1, 2
        res1_l_8x16b = _mm_add_epi16(res1_l_8x16b, const_8x16b);
        res1_h_8x16b = _mm_add_epi16(res1_h_8x16b, const_8x16b);
        res2_l_8x16b = _mm_add_epi16(res1_l_8x16b, c2_8x16b);
        res2_h_8x16b = _mm_add_epi16(res1_h_8x16b, c2_8x16b);

        res1_sh_l_8x16b = _mm_srai_epi16(res1_l_8x16b, 5);
        res1_sh_h_8x16b = _mm_srai_epi16(res1_h_8x16b, 5);
        res2_sh_l_8x16b = _mm_srai_epi16(res2_l_8x16b, 5);
        res2_sh_h_8x16b = _mm_srai_epi16(res2_h_8x16b, 5);

        dst_strd2 = dst_strd << 1;
        c2_8x16b = _mm_slli_epi16(c2_8x16b, 1);

        res1_16x8b = _mm_packus_epi16(res1_sh_l_8x16b, res1_sh_h_8x16b);
        res2_16x8b = _mm_packus_epi16(res2_sh_l_8x16b, res2_sh_h_8x16b);

        _mm_storeu_si128((__m128i *)pu1_dst, res1_16x8b);
        _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), res2_16x8b);

        // rows 3, 4
        res1_l_8x16b = _mm_add_epi16(res1_l_8x16b, c2_8x16b);
        res1_h_8x16b = _mm_add_epi16(res1_h_8x16b, c2_8x16b);
        res2_l_8x16b = _mm_add_epi16(res2_l_8x16b, c2_8x16b);
        res2_h_8x16b = _mm_add_epi16(res2_h_8x16b, c2_8x16b);

        res1_sh_l_8x16b = _mm_srai_epi16(res1_l_8x16b, 5);
        res1_sh_h_8x16b = _mm_srai_epi16(res1_h_8x16b, 5);
        res2_sh_l_8x16b = _mm_srai_epi16(res2_l_8x16b, 5);
        res2_sh_h_8x16b = _mm_srai_epi16(res2_h_8x16b, 5);

        pu1_dst += dst_strd2;

        res1_16x8b = _mm_packus_epi16(res1_sh_l_8x16b, res1_sh_h_8x16b);
        res2_16x8b = _mm_packus_epi16(res2_sh_l_8x16b, res2_sh_h_8x16b);

        _mm_storeu_si128((__m128i *)pu1_dst, res1_16x8b);
        _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), res2_16x8b);

        // rows 5, 6
        res1_l_8x16b = _mm_add_epi16(res1_l_8x16b, c2_8x16b);
        res1_h_8x16b = _mm_add_epi16(res1_h_8x16b, c2_8x16b);
        res2_l_8x16b = _mm_add_epi16(res2_l_8x16b, c2_8x16b);
        res2_h_8x16b = _mm_add_epi16(res2_h_8x16b, c2_8x16b);

        res1_sh_l_8x16b = _mm_srai_epi16(res1_l_8x16b, 5);
        res1_sh_h_8x16b = _mm_srai_epi16(res1_h_8x16b, 5);
        res2_sh_l_8x16b = _mm_srai_epi16(res2_l_8x16b, 5);
        res2_sh_h_8x16b = _mm_srai_epi16(res2_h_8x16b, 5);

        pu1_dst += dst_strd2;

        res1_16x8b = _mm_packus_epi16(res1_sh_l_8x16b, res1_sh_h_8x16b);
        res2_16x8b = _mm_packus_epi16(res2_sh_l_8x16b, res2_sh_h_8x16b);

        _mm_storeu_si128((__m128i *)pu1_dst, res1_16x8b);
        _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), res2_16x8b);

        // rows 7, 8
        res1_l_8x16b = _mm_add_epi16(res1_l_8x16b, c2_8x16b);
        res1_h_8x16b = _mm_add_epi16(res1_h_8x16b, c2_8x16b);
        res2_l_8x16b = _mm_add_epi16(res2_l_8x16b, c2_8x16b);
        res2_h_8x16b = _mm_add_epi16(res2_h_8x16b, c2_8x16b);

        res1_sh_l_8x16b = _mm_srai_epi16(res1_l_8x16b, 5);
        res1_sh_h_8x16b = _mm_srai_epi16(res1_h_8x16b, 5);
        res2_sh_l_8x16b = _mm_srai_epi16(res2_l_8x16b, 5);
        res2_sh_h_8x16b = _mm_srai_epi16(res2_h_8x16b, 5);

        pu1_dst += dst_strd2;

        res1_16x8b = _mm_packus_epi16(res1_sh_l_8x16b, res1_sh_h_8x16b);
        res2_16x8b = _mm_packus_epi16(res2_sh_l_8x16b, res2_sh_h_8x16b);

        _mm_storeu_si128((__m128i *)pu1_dst, res1_16x8b);
        _mm_storeu_si128((__m128i *)(pu1_dst + dst_strd), res2_16x8b);

    }
}
