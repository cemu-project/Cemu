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
******************************************************************************
* @file ih264_cabac_tables.h
*
* @brief
*  This file contains enumerations, macros and extern declarations of H264
*  cabac tables
*
* @author
*  Ittiam
*
* @remarks
*  none
******************************************************************************
*/

#ifndef IH264_CABAC_TABLES_H_
#define IH264_CABAC_TABLES_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/

/**
******************************************************************************
 *  @brief  maximum range of cabac_init_idc (0-2) + 1 for ISLICE
******************************************************************************
 */
#define NUM_CAB_INIT_IDC_PLUS_ONE 4

/**
******************************************************************************
 *  @brief  max range of qps in H264 (0-51)
******************************************************************************
 */
#define QP_RANGE        52

/**
******************************************************************************
 *  @brief  max range of cabac contexts in H264 (0-459)
******************************************************************************
 */
#define NUM_CABAC_CTXTS 460


/** Macros for Cabac checks */
/** MbType */
/** |x|x|I_PCM|SKIP|
 |S|Inter/Intra|P/B|NON-BD16x16/BD16x16,I16x16/I4x4| */
#define CAB_INTRA         0x00 /* 0000 00xx */
#define CAB_INTER         0x04 /* 0000 01xx */
#define CAB_I4x4          0x00 /* 0000 00x0 */
#define CAB_I16x16        0x01 /* 0000 00x1 */
#define CAB_BD16x16       0x04 /* 0000 0100 */
#define CAB_NON_BD16x16   0x05 /* 0000 0101 */
#define CAB_P             0x07 /* 0000 0111 */
#define CAB_SI4x4         0x08 /* 0000 10x0 */
#define CAB_SI16x16       0x09 /* 0000 10x1 */
#define CAB_SKIP_MASK     0x10 /* 0001 0000 */
#define CAB_SKIP          0x10 /* 0001 0000 */
#define CAB_P_SKIP        0x16 /* 0001 x11x */
#define CAB_B_SKIP        0x14 /* 0001 x100 */
#define CAB_BD16x16_MASK  0x07 /* 0000 0111 */
#define CAB_INTRA_MASK    0x04 /* 0000 0100 */
#define CAB_I_PCM         0x20 /* 001x xxxx */

/**
******************************************************************************
 *  @enum  ctxBlockCat

******************************************************************************
*/
typedef enum
{
    LUMA_DC_CTXCAT   = 0,
    LUMA_AC_CTXCAT   = 1,
    LUMA_4X4_CTXCAT  = 2,
    CHROMA_DC_CTXCAT = 3,
    CHROMA_AC_CTXCAT = 4,
    LUMA_8X8_CTXCAT  = 5,
    NUM_CTX_CAT      = 6
} CTX_BLOCK_CAT;


/**
******************************************************************************
 *  @enum ctxIdxOffset

******************************************************************************
*/
typedef enum
{
    MB_TYPE_SI_SLICE = 0,
    MB_TYPE_I_SLICE = 3,
    MB_SKIP_FLAG_P_SLICE = 11,
    MB_TYPE_P_SLICE = 14,
    SUB_MB_TYPE_P_SLICE = 21,
    MB_SKIP_FLAG_B_SLICE = 24,
    MB_TYPE_B_SLICE = 27,
    SUB_MB_TYPE_B_SLICE = 36,
    MVD_X = 40,
    MVD_Y = 47,
    REF_IDX = 54,
    MB_QP_DELTA = 60,
    INTRA_CHROMA_PRED_MODE = 64,
    PREV_INTRA4X4_PRED_MODE_FLAG = 68,
    REM_INTRA4X4_PRED_MODE = 69,
    MB_FIELD_DECODING_FLAG = 70,
    CBP_LUMA = 73,
    CBP_CHROMA = 77,
    CBF = 85,
    SIGNIFICANT_COEFF_FLAG_FRAME = 105,
    SIGNIFICANT_COEFF_FLAG_FLD = 277,
    LAST_SIGNIFICANT_COEFF_FLAG_FRAME = 166,
    LAST_SIGNIFICANT_COEFF_FLAG_FLD = 338,
    COEFF_ABS_LEVEL_MINUS1 = 227,

    /* High profile related Syntax element CABAC offsets */
    TRANSFORM_SIZE_8X8_FLAG = 399,
    SIGNIFICANT_COEFF_FLAG_8X8_FRAME = 402,
    LAST_SIGNIFICANT_COEFF_FLAG_8X8_FRAME = 417,
    COEFF_ABS_LEVEL_MINUS1_8X8 = 426,
    SIGNIFICANT_COEFF_FLAG_8X8_FIELD = 436,
    LAST_SIGNIFICANT_COEFF_FLAG_8X8_FIELD = 451

} cabac_table_num_t;


/**
******************************************************************************
 *  @enum  ctxIdxOffset

******************************************************************************
*/
typedef enum
{
    SIG_COEFF_CTXT_CAT_0_OFFSET = 0,
    SIG_COEFF_CTXT_CAT_1_OFFSET = 15,
    SIG_COEFF_CTXT_CAT_2_OFFSET = 29,
    SIG_COEFF_CTXT_CAT_3_OFFSET = 44,
    SIG_COEFF_CTXT_CAT_4_OFFSET = 47,
    SIG_COEFF_CTXT_CAT_5_OFFSET = 0,
    COEFF_ABS_LEVEL_CAT_0_OFFSET = 0,
    COEFF_ABS_LEVEL_CAT_1_OFFSET = 10,
    COEFF_ABS_LEVEL_CAT_2_OFFSET = 20,
    COEFF_ABS_LEVEL_CAT_3_OFFSET = 30,
    COEFF_ABS_LEVEL_CAT_4_OFFSET = 39,
    COEFF_ABS_LEVEL_CAT_5_OFFSET = 0
} cabac_blk_cat_offset_t;




/*****************************************************************************/
/* Extern global declarations                                                */
/*****************************************************************************/


/* CABAC Table declaration*/
extern const UWORD32 gau4_ih264_cabac_table[128][4];


/*****************************************************************************/
/* Cabac tables for context initialization depending upon type of Slice,     */
/* cabac init Idc value and Qp.                                              */
/*****************************************************************************/
extern const UWORD8 gau1_ih264_cabac_ctxt_init_table[NUM_CAB_INIT_IDC_PLUS_ONE][QP_RANGE][NUM_CABAC_CTXTS];


#endif /* IH264_CABAC_TABLES_H_ */
