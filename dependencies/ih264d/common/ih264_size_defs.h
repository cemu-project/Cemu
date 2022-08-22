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
 *  ih264_size_defs.h
 *
 * @brief
 *  Contains declaration of global variables for H264 transform , quant and inverse quant
 *
 * @author
 *  Ittiam
 *
 * @remarks
 *
 ********************************************************************************/

#ifndef IH264_SIZE_DEFS_H_
#define IH264_SIZE_DEFS_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/

/*-----------------------Primary defs--------------------------*/

/*Width of a 4x4 block*/
#define SUB_BLK_WIDTH_4x4                   4

/*Width of an 8x8 block*/
#define SUB_BLK_WIDTH_8x8                   8

/*Number of chroma blocks in a row of coffs*/
#define SUB_BLK_COUNT_CHROMA_4x4_420        2

/*Number of luma blocks in a row of coffs*/
#define SUB_BLK_COUNT_LUMA_4x4              4

/*Numbr of chroma planes*/
#define NUM_CHROMA_PLANES                   2

/*Constant bit shifts*/
#define QP_BITS_h264_4x4                    15
#define QP_BITS_h264_8x8                    16


/*---------------------------Derived defs------------------------*/

/*Number of coefficients ina 4x4 block*/
#define COFF_CNT_SUB_BLK_4x4                SUB_BLK_WIDTH_4x4*SUB_BLK_WIDTH_4x4;

/*Number of luma blocks in a row of coffs*/
#define SUB_BLK_LUMA_4X4_CNT_MB             SUB_BLK_COUNT_LUMA_4x4 * SUB_BLK_COUNT_LUMA_4x4

/*Number of chroma coffs in an MB*/
#define SUB_BLK_CHROMA_4X4_CNT_MB           SUB_BLK_COUNT_CHROMA_4x4_420 * SUB_BLK_COUNT_CHROMA_4x4_420
#define SUB_BLK_CHROMA_4X4_CNT_MB_BIPLANE   SUB_BLK_CHROMA_4X4_CNT_MB*NUM_CHROMA_PLANES

/*Size of trans buff = 4x4 for DC block +  4x4 * coffs for 4x4 ac blocks*/
#define SIZE_TRANS_BUFF                     (SUB_BLK_WIDTH_4x4*SUB_BLK_WIDTH_4x4*+ \
                                            SUB_BLK_WIDTH_4x4*SUB_BLK_WIDTH_4x4*   \
                                            SUB_BLK_COUNT_LUMA_4x4*SUB_BLK_COUNT_LUMA_4x4)

/*memory size = memory size of 4x4 block of resi coff + 4x4 for DC coff block                          */
#define SIZE_TMP_BUFF_ITRANS                ((SUB_BLK_WIDTH_4x4*SUB_BLK_WIDTH_4x4) +\
                                             (SUB_BLK_WIDTH_4x4*SUB_BLK_WIDTH_4x4))

#endif /* IH264_DEFS_H_ */
