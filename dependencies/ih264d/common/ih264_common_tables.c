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
*  ih264_common_tables.c
*
* @brief
*  Contains common global tables
*
* @author
*  Harish M
*
* @par List of Functions:
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
#include "ih264_defs.h"
#include "ih264_macros.h"
#include "ih264_structs.h"
#include "ih264_common_tables.h"


/*****************************************************************************/
/* Extern global definitions                                                 */
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
const level_tables_t gas_ih264_lvl_tbl[16] =
{
    { IH264_LEVEL_10,    1485,       99,         297,       64,         175,         64  },
    { IH264_LEVEL_1B,    1485,       99,         297,       128,        350,         64  },
    { IH264_LEVEL_11,    3000,       396,        675,       192,        500,         128 },
    { IH264_LEVEL_12,    6000,       396,        1782,      384,        1000,        128 },
    { IH264_LEVEL_13,    11880,      396,        1782,      768,        2000,        128 },
    { IH264_LEVEL_20,    11880,      396,        1782,      2000,       2000,        128 },
    { IH264_LEVEL_21,    19800,      792,        3564,      4000,       4000,        256 },
    { IH264_LEVEL_22,    20250,      1620,       6075,      4000,       4000,        256 },
    { IH264_LEVEL_30,    40500,      1620,       6075,      10000,      10000,       256 },
    { IH264_LEVEL_31,    108000,     3600,       13500,     14000,      14000,       512 },
    { IH264_LEVEL_32,    216000,     5120,       15360,     20000,      20000,       512 },
    { IH264_LEVEL_40,    245760,     8192,       24576,     20000,      25000,       512 },
    { IH264_LEVEL_41,    245760,     8192,       24576,     50000,      62500,       512 },
    { IH264_LEVEL_42,    522240,     8704,       26112,     50000,      62500,       512 },
    { IH264_LEVEL_50,    589824,     22080,      82800,     135000,     135000,      512 },
    { IH264_LEVEL_51,    983040,     36864,      138240,    240000,     240000,      512 },
};


/**
 * Array containing supported levels
 */
const WORD32 gai4_ih264_levels[] =
{
    IH264_LEVEL_10,
    IH264_LEVEL_11,
    IH264_LEVEL_12,
    IH264_LEVEL_13,
    IH264_LEVEL_20,
    IH264_LEVEL_21,
    IH264_LEVEL_22,
    IH264_LEVEL_30,
    IH264_LEVEL_31,
    IH264_LEVEL_32,
    IH264_LEVEL_40,
    IH264_LEVEL_41,
    IH264_LEVEL_42,
    IH264_LEVEL_50,
    IH264_LEVEL_51,
};


/**
 * Array giving size of max luma samples in a picture for a given level
 */
const WORD32 gai4_ih264_max_luma_pic_size[] =
{
    /* Level 1 */
    25344,
    /* Level 1.1 */
    101376,
    /* Level 1.2 */
    101376,
    /* Level 1.3 */
    101376,
    /* Level 2 */
    101376,
    /* Level 2.1 */
    202752,
    /* Level 2.2 */
    414720,
    /* Level 3 */
    414720,
    /* Level 3.1 */
    921600,
    /* Level 3.2 */
    1310720,
    /* Level 4 */
    2097152,
    /* Level 4.1 */
    2097152,
    /* Level 4.2 */
    2228224,
    /* Level 5 */
    5652480,
    /* Level 5.1 */
    9437184
};


/** Max width and height allowed for a given level */
/** This is derived as SQRT(8 * gai4_ih264_max_luma_pic_size[]) */
const WORD32 gai4_ih264_max_wd_ht[] =
{
    /* Level 1 */
    451,
    /* Level 1.1 */
    901,
    /* Level 1.2 */
    901,
    /* Level 1.3 */
    901,
    /* Level 2 */
    901,
    /* Level 2.1 */
    1274,
    /* Level 2.2 */
    1822,
    /* Level 3 */
    1822,
    /* Level 3.1 */
    2716,
    /* Level 3.2 */
    3239,
    /* Level 4 */
    4096,
    /* Level 4.1 */
    4096,
    /* Level 4.2 */
    4223,
    /* Level 5 */
    6725,
    /* Level 5.1 */
    8689
};

/** Min width and height allowed for a given level */
/** This is derived as gai4_ih264_max_luma_pic_size[]/gai4_ih264_max_wd_ht[] */
const WORD32 gai4_ih264_min_wd_ht[] =
{
    /* Level 1 */
    57,
    /* Level 1.1 */
    113,
    /* Level 1.2 */
    113,
    /* Level 1.3 */
    113,
    /* Level 2 */
    113,
    /* Level 2.1 */
    160,
    /* Level 2.2 */
    228,
    /* Level 3 */
    228,
    /* Level 3.1 */
    340,
    /* Level 3.2 */
    405,
    /* Level 4 */
    512,
    /* Level 4.1 */
    512,
    /* Level 4.2 */
    528,
    /* Level 5 */
    841,
    /* Level 5.1 */
    1087

};


/** Table 7-11 Macroblock types for I slices */
intra_mbtype_info_t gas_ih264_i_mbtype_info[] =
{
    /* For first entry, if transform_size_8x8_flag is 1, mode will be MBPART_I8x8 */
    /* This has to be taken care while accessing the table */
    {0, MBPART_I4x4,   VERT_I16x16,     0,  0},
    {0, MBPART_I16x16, VERT_I16x16,     0,  0},
    {0, MBPART_I16x16, HORZ_I16x16,     0,  0},
    {0, MBPART_I16x16, DC_I16x16,       0,  0},
    {0, MBPART_I16x16, PLANE_I16x16,    0,  0},
    {0, MBPART_I16x16, VERT_I16x16,     1,  0},
    {0, MBPART_I16x16, HORZ_I16x16,     1,  0},
    {0, MBPART_I16x16, DC_I16x16,       1,  0},
    {0, MBPART_I16x16, PLANE_I16x16,    1,  0},
    {0, MBPART_I16x16, VERT_I16x16,     2,  0},
    {0, MBPART_I16x16, HORZ_I16x16,     2,  0},
    {0, MBPART_I16x16, DC_I16x16,       2,  0},
    {0, MBPART_I16x16, PLANE_I16x16,    2,  0},
    {0, MBPART_I16x16, VERT_I16x16,     0,  15},
    {0, MBPART_I16x16, HORZ_I16x16,     0,  15},
    {0, MBPART_I16x16, DC_I16x16,       0,  15},
    {0, MBPART_I16x16, PLANE_I16x16,    0,  15},
    {0, MBPART_I16x16, VERT_I16x16,     1,  15},
    {0, MBPART_I16x16, HORZ_I16x16,     1,  15},
    {0, MBPART_I16x16, DC_I16x16,       1,  15},
    {0, MBPART_I16x16, PLANE_I16x16,    1,  15},
    {0, MBPART_I16x16, VERT_I16x16,     2,  15},
    {0, MBPART_I16x16, HORZ_I16x16,     2,  15},
    {0, MBPART_I16x16, DC_I16x16,       2,  15},
    {0, MBPART_I16x16, PLANE_I16x16,    2,  15},
    {0, MBPART_IPCM,   VERT_I16x16,     0,  0}
};

/** Table 7-13 Macroblock types for P slices */
inter_mbtype_info_t gas_ih264_p_mbtype_info[] =
{
    {1, MBPART_L0,  MBPART_NA,  16, 16},
    {2, MBPART_L0,  MBPART_L0,  16, 8},
    {2, MBPART_L0,  MBPART_L0,  8,  16},
    {4, MBPART_NA,  MBPART_NA,  8,  8},
    {4, MBPART_NA,  MBPART_NA,  8,  8},
};

/** Table 7-14 Macroblock types for B slices */
inter_mbtype_info_t gas_ih264_b_mbtype_info[] =
{
    {0, MBPART_DIRECT,  MBPART_NA,  8,  8,  },
    {1, MBPART_L0,      MBPART_NA,  16, 16, },
    {1, MBPART_L1,      MBPART_NA,  16, 16, },
    {1, MBPART_BI,      MBPART_NA,  16, 16, },
    {2, MBPART_L0,      MBPART_L0,  16, 8,  },
    {2, MBPART_L0,      MBPART_L0,  8,  16, },
    {2, MBPART_L1,      MBPART_L1,  16, 8,  },
    {2, MBPART_L1,      MBPART_L1,  8,  16, },
    {2, MBPART_L0,      MBPART_L1,  16, 8,  },
    {2, MBPART_L0,      MBPART_L1,  8,  16, },
    {2, MBPART_L1,      MBPART_L0,  16, 8,  },
    {2, MBPART_L1,      MBPART_L0,  8,  16, },
    {2, MBPART_L0,      MBPART_BI,  16, 8,  },
    {2, MBPART_L0,      MBPART_BI,  8,  16, },
    {2, MBPART_L1,      MBPART_BI,  16, 8,  },
    {2, MBPART_L1,      MBPART_BI,  8,  16, },
    {2, MBPART_BI,      MBPART_L0,  16, 8,  },
    {2, MBPART_BI,      MBPART_L0,  8,  16, },
    {2, MBPART_BI,      MBPART_L1,  16, 8,  },
    {2, MBPART_BI,      MBPART_L1,  8,  16, },
    {2, MBPART_BI,      MBPART_BI,  16, 8,  },
    {2, MBPART_BI,      MBPART_BI,  8,  16, },
    {4, MBPART_NA,      MBPART_NA,  8,  8,  },
};

/** Table 7-17 – Sub-macroblock types in P macroblocks */
submbtype_info_t gas_ih264_p_submbtype_info[] =
{
   {1, MBPART_L0, 8,  8},
   {2, MBPART_L0, 8,  4},
   {2, MBPART_L0, 4,  8},
   {4, MBPART_L0, 4,  4},
};

/** Table 7-18 – Sub-macroblock types in B macroblocks */
submbtype_info_t gas_ih264_b_submbtype_info[] =
{
    {4, MBPART_DIRECT,  4,  4},
    {1, MBPART_L0,      8,  8},
    {1, MBPART_L1,      8,  8},
    {1, MBPART_BI,      8,  8},
    {2, MBPART_L0,      8,  4},
    {2, MBPART_L0,      4,  8},
    {2, MBPART_L1,      8,  4},
    {2, MBPART_L1,      4,  8},
    {2, MBPART_BI,      8,  4},
    {2, MBPART_BI,      4,  8},
    {4, MBPART_L0,      4,  4},
    {4, MBPART_L1,      4,  4},
    {4, MBPART_BI,      4,  4},
};




const UWORD8 gau1_ih264_inv_scan_prog4x4[] =
{
    0,   1,  4,  8,
    5,   2,  3,  6,
    9,  12, 13, 10,
    7,  11, 14, 15
};

const UWORD8 gau1_ih264_inv_scan_int4x4[] =
{
     0, 4,  1,  8,
    12, 5,  9,  13,
     2, 6, 10,  14,
     3, 7, 11,  15
};

/** Inverse scan tables for individual 4x4 blocks of 8x8 transform coeffs of CAVLC */
/* progressive */
const UWORD8 gau1_ih264_inv_scan_prog8x8_cavlc[64] =
{
     0,  9, 17, 18, 12, 40, 27,  7,
    35, 57, 29, 30, 58, 38, 53, 47,
     1,  2, 24, 11, 19, 48, 20, 14,
    42, 50, 22, 37, 59, 31, 60, 55,
     8,  3, 32,  4, 26, 41, 13, 21,
    49, 43, 15, 44, 52, 39, 61, 62,
    16, 10, 25,  5, 33, 34,  6, 28,
    56, 36, 23, 51, 45, 46, 54, 63
};

/* interlace */
const UWORD8 gau1_ih264_inv_scan_int8x8_cavlc[64] =
{
     0,  9,  2, 56, 18, 26, 34, 27,
    35, 28, 36, 29, 45,  7, 54, 39,
     8, 24, 25, 33, 41, 11, 42, 12,
    43, 13, 44, 14, 53, 15, 62, 47,
    16, 32, 40, 10, 49,  4, 50,  5,
    51,  6, 52, 22, 61, 38, 23, 55,
     1, 17, 48,  3, 57, 19, 58, 20,
    59, 21, 60, 37, 30, 46, 31, 63
};



/*Inverse scan tables for individual 8x8 blocks of 8x8 transform coeffs of CABAC */
/* progressive */

const UWORD8 gau1_ih264_inv_scan_prog8x8_cabac[64] =
{
     0,  1,  8, 16,  9, 2,   3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};


/* interlace */

const UWORD8 gau1_ih264_inv_scan_int8x8_cabac[64] =
{
     0,  8, 16,  1,  9, 24, 32, 17,
     2, 25, 40, 48, 56, 33, 10, 3,
    18, 41, 49, 57, 26, 11,  4, 19,
    34, 42, 50, 58, 27, 12,  5, 20,
    35, 43, 51, 59, 28, 13,  6, 21,
    36, 44, 52, 60, 29, 14, 22, 37,
    45, 53, 61, 30,  7, 15, 38, 46,
    54, 62, 23, 31, 39, 47, 55, 63
};


const UWORD8 *const gpau1_ih264_inv_scan8x8[] =
{
     gau1_ih264_inv_scan_prog8x8_cavlc,
     gau1_ih264_inv_scan_int8x8_cavlc,
     gau1_ih264_inv_scan_prog8x8_cabac,
     gau1_ih264_inv_scan_int8x8_cabac
};

const UWORD8 *const gpau1_ih264_inv_scan4x4[] =
{
     gau1_ih264_inv_scan_prog4x4,
     gau1_ih264_inv_scan_int4x4,
};

const UWORD8 gau1_ih264_8x8_subblk_idx[] =
{
      0,    1,   4,  5,
      2,    3,   6,  7,
      8,    9,  12, 13,
     10,   11,  14, 15
};


/* Table 8-15 Chroma QP offset table */
const UWORD8 gau1_ih264_chroma_qp[] =
{
      0,  1,  2,  3,  4,  5,  6,  7,
      8,  9, 10, 11, 12, 13, 14, 15,
     16, 17, 18, 19, 20, 21, 22, 23,
     24, 25, 26, 27, 28, 29, 29, 30,
     31, 32, 32, 33, 34, 34, 35, 35,
     36, 36, 37, 37, 37, 38, 38, 38,
     39, 39, 39, 39
};


/**
******************************************************************************
* @brief  look up table to compute neigbour availability of 4x4 blocks
* input  : subblk idx, mb neighbor availability
* output : sub blk neighbor availability
* @remarks
******************************************************************************
*/
const UWORD8 gau1_ih264_4x4_ngbr_avbl[16][16] =
{
    {  0x0, 0x1, 0xc, 0x7, 0x1, 0x1, 0xf, 0x7, 0xc, 0xf, 0xc, 0x7, 0xf, 0x7, 0xf, 0x7 },
    {  0x1, 0x1, 0xf, 0x7, 0x1, 0x1, 0xf, 0x7, 0xf, 0xf, 0xf, 0x7, 0xf, 0x7, 0xf, 0x7 },
    {  0x2, 0x1, 0xc, 0x7, 0x1, 0x1, 0xf, 0x7, 0xc, 0xf, 0xc, 0x7, 0xf, 0x7, 0xf, 0x7 },
    {  0x3, 0x1, 0xf, 0x7, 0x1, 0x1, 0xf, 0x7, 0xf, 0xf, 0xf, 0x7, 0xf, 0x7, 0xf, 0x7 },

    {  0xc, 0xf, 0xc, 0x7, 0xf, 0x7, 0xf, 0x7, 0xc, 0xf, 0xc, 0x7, 0xf, 0x7, 0xf, 0x7 },
    {  0xd, 0xf, 0xf, 0x7, 0xf, 0x7, 0xf, 0x7, 0xf, 0xf, 0xf, 0x7, 0xf, 0x7, 0xf, 0x7 },
    {  0xe, 0xf, 0xc, 0x7, 0xf, 0x7, 0xf, 0x7, 0xc, 0xf, 0xc, 0x7, 0xf, 0x7, 0xf, 0x7 },
    {  0xf, 0xf, 0xf, 0x7, 0xf, 0x7, 0xf, 0x7, 0xf, 0xf, 0xf, 0x7, 0xf, 0x7, 0xf, 0x7 },

    {  0x0, 0x1, 0xc, 0x7, 0x1, 0x9, 0xf, 0x7, 0xc, 0xf, 0xc, 0x7, 0xf, 0x7, 0xf, 0x7 },
    {  0x1, 0x1, 0xf, 0x7, 0x1, 0x9, 0xf, 0x7, 0xf, 0xf, 0xf, 0x7, 0xf, 0x7, 0xf, 0x7 },
    {  0x2, 0x1, 0xc, 0x7, 0x1, 0x9, 0xf, 0x7, 0xc, 0xf, 0xc, 0x7, 0xf, 0x7, 0xf, 0x7 },
    {  0x3, 0x1, 0xf, 0x7, 0x1, 0x9, 0xf, 0x7, 0xf, 0xf, 0xf, 0x7, 0xf, 0x7, 0xf, 0x7 },

    {  0xc, 0xf, 0xc, 0x7, 0xf, 0xf, 0xf, 0x7, 0xc, 0xf, 0xc, 0x7, 0xf, 0x7, 0xf, 0x7 },
    {  0xd, 0xf, 0xf, 0x7, 0xf, 0xf, 0xf, 0x7, 0xf, 0xf, 0xf, 0x7, 0xf, 0x7, 0xf, 0x7 },
    {  0xe, 0xf, 0xc, 0x7, 0xf, 0xf, 0xf, 0x7, 0xc, 0xf, 0xc, 0x7, 0xf, 0x7, 0xf, 0x7 },
    {  0xf, 0xf, 0xf, 0x7, 0xf, 0xf, 0xf, 0x7, 0xf, 0xf, 0xf, 0x7, 0xf, 0x7, 0xf, 0x7 },
};


/**
******************************************************************************
* @brief  look up table to compute neigbour availability of 8x8 blocks
* input  : subblk idx, mb neighbor availability
* output : sub blk neighbor availability
* @remarks
******************************************************************************
*/
const UWORD8 gau1_ih264_8x8_ngbr_avbl[16][4] =
{
    {  0x0, 0x1, 0xc, 0x7 },
    {  0x1, 0x1, 0xf, 0x7 },
    {  0x2, 0x1, 0xc, 0x7 },
    {  0x3, 0x1, 0xf, 0x7 },

    {  0xc, 0x7, 0xc, 0x7 },
    {  0xd, 0x7, 0xf, 0x7 },
    {  0xe, 0x7, 0xc, 0x7 },
    {  0xf, 0x7, 0xf, 0x7 },

    {  0x0, 0x9, 0xc, 0x7 },
    {  0x1, 0x9, 0xf, 0x7 },
    {  0x2, 0x9, 0xc, 0x7 },
    {  0x3, 0x9, 0xf, 0x7 },

    {  0xc, 0xf, 0xc, 0x7 },
    {  0xd, 0xf, 0xf, 0x7 },
    {  0xe, 0xf, 0xc, 0x7 },
    {  0xf, 0xf, 0xf, 0x7 },
};

/** Table 7-3 Default intra 4x4 scaling list */
const UWORD16 gau2_ih264_default_intra4x4_scaling_list[] =
{
     6, 13, 13, 20,
    20, 20, 28, 28,
    28, 28, 32, 32,
    32, 37, 37, 42
};

/** Table 7-3 Default inter 4x4 scaling list */
const UWORD16 gau2_ih264_default_inter4x4_scaling_list[] =
{
    10, 14, 14, 20,
    20, 20, 24, 24,
    24, 24, 27, 27,
    27, 30, 30, 34
};

/* Inverse scanned output of gau2_ih264_default_intra4x4_scaling_list */
const UWORD16 gau2_ih264_default_intra4x4_weight_scale[] =
{
     6, 13, 20, 28,
    13, 20, 28, 32,
    20, 28, 32, 37,
    28, 32, 37, 42
};

/* Inverse scanned output of gau2_ih264_default_inter4x4_scaling_list */
const UWORD16 gau2_ih264_default_inter4x4_weight_scale[] =
{
     10, 14, 20, 24,
     14, 20, 24, 27,
     20, 24, 27, 30,
     24, 27, 30, 34
};

/** Table 7-4 Default intra 8x8 scaling list */
const UWORD16 gau2_ih264_default_intra8x8_scaling_list[] =
{
     6, 10, 10, 13, 11, 13, 16, 16,
    16, 16, 18, 18, 18, 18, 18, 23,
    23, 23, 23, 23, 23, 25, 25, 25,
    25, 25, 25, 25, 27, 27, 27, 27,
    27, 27, 27, 27, 29, 29, 29, 29,
    29, 29, 29, 31, 31, 31, 31, 31,
    31, 33, 33, 33, 33, 33, 36, 36,
    36, 36, 38, 38, 38, 40, 40, 42
};

/** Table 7-4 Default inter 8x8 scaling list */
const UWORD16 gau2_ih264_default_inter8x8_scaling_list[] =
{
    9,  13, 13, 15, 13, 15, 17, 17,
    17, 17, 19, 19, 19, 19, 19, 21,
    21, 21, 21, 21, 21, 22, 22, 22,
    22, 22, 22, 22, 24, 24, 24, 24,
    24, 24, 24, 24, 25, 25, 25, 25,
    25, 25, 25, 27, 27, 27, 27, 27,
    27, 28, 28, 28, 28, 28, 30, 30,
    30, 30, 32, 32, 32, 33, 33, 35
};

/* Inverse scanned output of gau2_ih264_default_intra8x8_scaling_list */
const UWORD16 gau2_ih264_default_intra8x8_weight_scale[] =
{
     6, 10, 13, 16, 18, 23, 25, 27,
    10, 11, 16, 18, 23, 25, 27, 29,
    13, 16, 18, 23, 25, 27, 29, 31,
    16, 18, 23, 25, 27, 29, 31, 33,
    18, 23, 25, 27, 29, 31, 33, 36,
    23, 25, 27, 29, 31, 33, 36, 38,
    25, 27, 29, 31, 33, 36, 38, 40,
    27, 29, 31, 33, 36, 38, 40, 42
};

/* Inverse scanned output of gau2_ih264_default_inter8x8_scaling_list */
const UWORD16 gau2_ih264_default_inter8x8_weight_scale[] =
{
     9, 13, 15, 17, 19, 21, 22, 24,
    13, 13, 17, 19, 21, 22, 24, 25,
    15, 17, 19, 21, 22, 24, 25, 27,
    17, 19, 21, 22, 24, 25, 27, 28,
    19, 21, 22, 24, 25, 27, 28, 30,
    21, 22, 24, 25, 27, 28, 30, 32,
    22, 24, 25, 27, 28, 30, 32, 33,
    24, 25, 27, 28, 30, 32, 33, 35
};
/* Eq 7-8 Flat scaling matrix for 4x4 */
const UWORD16 gau2_ih264_flat_4x4_weight_scale[] =
{
    16, 16, 16, 16,
    16, 16, 16, 16,
    16, 16, 16, 16,
    16, 16, 16, 16
};

/* Eq 7-9 Flat scaling matrix for 8x8 */
const UWORD16 gau2_ih264_flat_8x8_weight_scale[] =
{
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16
};


/**
 ******************************************************************************
 * @brief  Scale Table for inverse quantizing 4x4 subblock. To inverse quantize
 * a given 4x4 quantized block, the coefficient at index location (i,j) is scaled
 * by one of the constants in this table and right shift the result by abs (4 -
 * floor(qp/6)), here qp is the quantization parameter used to quantize the mb.
 *
 * input   : 16 * qp%6, index location (i,j)
 * output  : scale constant.
 *
 * @remarks 16 constants for each index position of the subblock and 6 for each
 * qp%6 in the range 0-5 inclusive.
 ******************************************************************************
 */

const UWORD16 gau2_ih264_iquant_scale_matrix_4x4[96] =
{
      10,  13,  10,  13,
      13,  16,  13,  16,
      10,  13,  10,  13,
      13,  16,  13,  16,

      11,  14,  11,  14,
      14,  18,  14,  18,
      11,  14,  11,  14,
      14,  18,  14,  18,

      13,  16,  13,  16,
      16,  20,  16,  20,
      13,  16,  13,  16,
      16,  20,  16,  20,

      14,  18,  14,  18,
      18,  23,  18,  23,
      14,  18,  14,  18,
      18,  23,  18,  23,

      16,  20,  16,  20,
      20,  25,  20,  25,
      16,  20,  16,  20,
      20,  25,  20,  25,

      18,  23,  18,  23,
      23,  29,  23,  29,
      18,  23,  18,  23,
      23,  29,  23,  29,

};

/**
 ******************************************************************************
 * @brief  Scale Table for inverse quantizing 8x8 subblock. To inverse quantize
 * a given 8x8 quantized block, the coefficient at index location (i,j) is scaled
 * by one of the constants in this table and right shift the result by abs (4 -
 * floor(qp/6)), here qp is the quantization parameter used to quantize the mb.
 *
 * input   : qp%6, index location (i,j)
 * output  : scale constant.
 *
 * @remarks 64 constants for each index position of the subblock and 6 for each
 * qp%6 in the range 0-5 inclusive.
 ******************************************************************************
 */
const UWORD16 gau2_ih264_iquant_scale_matrix_8x8 [384] =
{
      20,  19,  25,  19,  20,  19,  25,  19,
      19,  18,  24,  18,  19,  18,  24,  18,
      25,  24,  32,  24,  25,  24,  32,  24,
      19,  18,  24,  18,  19,  18,  24,  18,
      20,  19,  25,  19,  20,  19,  25,  19,
      19,  18,  24,  18,  19,  18,  24,  18,
      25,  24,  32,  24,  25,  24,  32,  24,
      19,  18,  24,  18,  19,  18,  24,  18,

      22,  21,  28,  21,  22,  21,  28,  21,
      21,  19,  26,  19,  21,  19,  26,  19,
      28,  26,  35,  26,  28,  26,  35,  26,
      21,  19,  26,  19,  21,  19,  26,  19,
      22,  21,  28,  21,  22,  21,  28,  21,
      21,  19,  26,  19,  21,  19,  26,  19,
      28,  26,  35,  26,  28,  26,  35,  26,
      21,  19,  26,  19,  21,  19,  26,  19,

      26,  24,  33,  24,  26,  24,  33,  24,
      24,  23,  31,  23,  24,  23,  31,  23,
      33,  31,  42,  31,  33,  31,  42,  31,
      24,  23,  31,  23,  24,  23,  31,  23,
      26,  24,  33,  24,  26,  24,  33,  24,
      24,  23,  31,  23,  24,  23,  31,  23,
      33,  31,  42,  31,  33,  31,  42,  31,
      24,  23,  31,  23,  24,  23,  31,  23,

      28,  26,  35,  26,  28,  26,  35,  26,
      26,  25,  33,  25,  26,  25,  33,  25,
      35,  33,  45,  33,  35,  33,  45,  33,
      26,  25,  33,  25,  26,  25,  33,  25,
      28,  26,  35,  26,  28,  26,  35,  26,
      26,  25,  33,  25,  26,  25,  33,  25,
      35,  33,  45,  33,  35,  33,  45,  33,
      26,  25,  33,  25,  26,  25,  33,  25,

      32,  30,  40,  30,  32,  30,  40,  30,
      30,  28,  38,  28,  30,  28,  38,  28,
      40,  38,  51,  38,  40,  38,  51,  38,
      30,  28,  38,  28,  30,  28,  38,  28,
      32,  30,  40,  30,  32,  30,  40,  30,
      30,  28,  38,  28,  30,  28,  38,  28,
      40,  38,  51,  38,  40,  38,  51,  38,
      30,  28,  38,  28,  30,  28,  38,  28,

      36,  34,  46,  34,  36,  34,  46,  34,
      34,  32,  43,  32,  34,  32,  43,  32,
      46,  43,  58,  43,  46,  43,  58,  43,
      34,  32,  43,  32,  34,  32,  43,  32,
      36,  34,  46,  34,  36,  34,  46,  34,
      34,  32,  43,  32,  34,  32,  43,  32,
      46,  43,  58,  43,  46,  43,  58,  43,
      34,  32,  43,  32,  34,  32,  43,  32,

};
