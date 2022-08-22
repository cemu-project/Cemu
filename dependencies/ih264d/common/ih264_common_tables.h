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
*  ih264_common_tables.h
*
* @brief
*  Common tables
*
* @author
*  Harish
*
* @par List of Functions:
*
* @remarks
*  None
*
*******************************************************************************
*/

#ifndef _IH264_COMMON_TABLES_H_
#define _IH264_COMMON_TABLES_H_


/*****************************************************************************/
/* Structures                                                                */
/*****************************************************************************/

/**
******************************************************************************
 *  @brief      level tables
******************************************************************************
 */
typedef struct
{
    /* level */
    IH264_LEVEL_T     u4_level_idc;

    /* max macroblock processing rate */
    UWORD32     u4_max_mbps;

    /* max frame size in mbs */
    UWORD32     u4_max_fs;

    /* max dpb size / 768 */
    UWORD32     u4_max_dpb_size;

    /* max bit rate */
    UWORD32     u4_max_br;

    /* max cpb size */
    UWORD32     u4_max_cpb_size;

    /* max vertical MV component range */
    UWORD32     u4_max_mv_y;

}level_tables_t;

/*****************************************************************************/
/* Extern global declarations                                                */
/*****************************************************************************/

/**
 ******************************************************************************
 * @brief  while encoding, basing on the input configuration parameters, the
 * the level of the bitstream is computed basing on the table below.
 * input  : table_idx
 * output : level_idc or cpb size
 * @remarks Table A-1 – level table limits
 ******************************************************************************
 */
extern const level_tables_t gas_ih264_lvl_tbl[16];

extern const WORD32 gai4_ih264_levels[];
extern const WORD32 gai4_ih264_max_luma_pic_size[];
extern const WORD32 gai4_ih264_max_wd_ht[];
extern const WORD32 gai4_ih264_min_wd_ht[];

extern intra_mbtype_info_t gas_ih264_i_mbtype_info[];
extern inter_mbtype_info_t gas_ih264_p_mbtype_info[];
extern inter_mbtype_info_t gas_ih264_b_mbtype_info[];
extern submbtype_info_t gas_ih264_p_submbtype_info[];
extern submbtype_info_t gas_ih264_b_submbtype_info[];


extern const UWORD8 gau1_ih264_inv_scan_prog4x4[];
extern const UWORD8 gau1_ih264_inv_scan_int4x4[];
extern const UWORD8 gau1_ih264_inv_scan_prog8x8_cavlc[64];
extern const UWORD8 gau1_ih264_inv_scan_int8x8_cavlc[64];
extern const UWORD8 gau1_ih264_inv_scan_prog8x8_cabac[64];
extern const UWORD8 gau1_ih264_inv_scan_int8x8_cabac[64];

extern const UWORD8 *const gpau1_ih264_inv_scan8x8[];
extern const UWORD8 *const gpau1_ih264_inv_scan4x4[];

extern const UWORD8 gau1_ih264_8x8_subblk_idx[];

extern const UWORD8 gau1_ih264_chroma_qp[];

extern const UWORD8 gau1_ih264_4x4_ngbr_avbl[16][16];
extern const UWORD8 gau1_ih264_8x8_ngbr_avbl[16][4];


extern const UWORD16 gau2_ih264_default_inter4x4_weight_scale[];
extern const UWORD16 gau2_ih264_default_intra4x4_weight_scale[];
extern const UWORD16 gau2_ih264_default_intra4x4_scaling_list[];
extern const UWORD16 gau2_ih264_default_inter4x4_scaling_list[];
extern const UWORD16 gau2_ih264_default_intra8x8_scaling_list[];
extern const UWORD16 gau2_ih264_default_inter8x8_scaling_list[];
extern const UWORD16 gau2_ih264_default_intra8x8_weight_scale[];
extern const UWORD16 gau2_ih264_default_inter8x8_weight_scale[];
extern const UWORD16 gau2_ih264_flat_4x4_weight_scale[];
extern const UWORD16 gau2_ih264_flat_8x8_weight_scale[];

extern const UWORD16 gau2_ih264_iquant_scale_matrix_4x4 [96];
extern const UWORD16 gau2_ih264_iquant_scale_matrix_8x8 [384];

#endif /*_IH264_COMMON_TABLES_H_*/
