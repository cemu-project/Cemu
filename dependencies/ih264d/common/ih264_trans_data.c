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
 *  ih264_trans_data.c
 *
 * @brief
 *  Contains definition of global variables for H264 encoder
 *
 * @author
 *  Ittiam
 *
 * @remarks
 *
 *******************************************************************************
 */

#include "ih264_typedefs.h"
#include "ih264_trans_data.h"

/*****************************************************************************/
/* Extern global definitions                                                 */
/*****************************************************************************/

/*
 * Since we don't have a division operation in neon
 * we will multiply by LCM of 16,6,10 and scale accordingly
 * so care that to get the actual transform you need to divide by LCM
 * LCM = 240
 */

const UWORD16 g_scal_coff_h264_4x4[16] ={
        15,40,40,40,
        40,24,40,24,
        15,40,40,15,
        40,24,40,24};



const UWORD16 g_scal_coff_h264_8x8[16]=
        {
                16,  15,   20,   15,
                15,  14,   19,   14,
                20,  19,   25,   19,
                15,  14,   19,   14
        };
/*
 * The scaling is by an 8x8 matrix, but due its 4x4 symmetry we can use
 * a 4x4 matrix for scaling
 * now since divide is to be avoided, we will compute 1/ values and scale it up
 * to preserve information since our data is max 10 bit +1 sign bit we can shift a maximum of 21 bits up
 * hence multiply the matrix as such
{16.000   15.059   20.227   15.059
15.059   14.173   19.051   14.173
20.227   19.051   25.600   19.051
15.059   14.173   19.051   14.173};
{512,   544,    405,    544,
544,    578,    430,    578,
405,    430,    320,    430,
544,    578,    430,    578};*/


/**
 ******************************************************************************
 * @brief  Scale Table for quantizing 4x4 subblock. To quantize a given 4x4 DCT
 * transformed block, the coefficient at index location (i,j) is scaled by one of
 * the constants in this table and right shift the result by (QP_BITS_h264_4x4 +
 * floor(qp/6)), here qp is the quantization parameter used to quantize the mb.
 *
 * input   : qp%6, index location (i,j)
 * output  : scale constant.
 *
 * @remarks 16 constants for each index position of the subblock and 6 for each
 * qp%6 in the range 0-5 inclusive.
 ******************************************************************************
 */
const UWORD16 gu2_quant_scale_matrix_4x4[96] =
{
     13107,   8066,  13107,   8066,
      8066,   5243,   8066,   5243,
     13107,   8066,  13107,   8066,
      8066,   5243,   8066,   5243,

     11916,   7490,  11916,   7490,
      7490,   4660,   7490,   4660,
     11916,   7490,  11916,   7490,
      7490,   4660,   7490,   4660,

     10082,   6554,  10082,   6554,
      6554,   4194,   6554,   4194,
     10082,   6554,  10082,   6554,
      6554,   4194,   6554,   4194,

      9362,   5825,   9362,   5825,
      5825,   3647,   5825,   3647,
      9362,   5825,   9362,   5825,
      5825,   3647,   5825,   3647,

      8192,   5243,   8192,   5243,
      5243,   3355,   5243,   3355,
      8192,   5243,   8192,   5243,
      5243,   3355,   5243,   3355,

      7282,   4559,   7282,   4559,
      4559,   2893,   4559,   2893,
      7282,   4559,   7282,   4559,
      4559,   2893,   4559,   2893,

};

/**
 ******************************************************************************
 * @brief  Round Factor for quantizing subblock. While quantizing a given 4x4 DCT
 * transformed block, the coefficient at index location (i,j) is scaled by one of
 * the constants in the table gu2_forward_quant_scalar_4x4 and then right shift
 * the result by (QP_BITS_h264_4x4 + floor(qp/6)).
 * Before right shifting a round factor is added.
 * The round factor can be any value [a * (1 << (QP_BITS_h264_4x4 + floor(qp/6)))]
 * for 'a' lies in the range 0-0.5.
 * Here qp is the quantization parameter used to quantize the mb.
 *
 * input   : qp/6
 * output  : round factor.
 *
 * @remarks The round factor is constructed by setting a = 1/3
 *
 * round factor constructed by setting a = 1/3
 {
      10922,     21845,     43690,     87381,
      174762,    349525,    699050,   1398101,
      2796202,
 }
 *
 * round factor constructed by setting a = 0.49
 *{
         16056,     32112,     64225,
         128450,    256901,    513802,
         1027604,   2055208,   4110417,
 };

  * round factor constructed by setting a = 0.5
      16384,     32768,     65536,
      131072,    262144,    524288,
     1048576,   2097152,   4194304,

 ******************************************************************************
 */
const UWORD32 gu4_forward_quant_round_factor_4x4[9] =
{
        10922,     21845,     43690,     87381,
        174762,    349525,    699050,   1398101,
        2796202,
};



/**
 ******************************************************************************
 * @brief  Threshold Table. Quantizing the given DCT coefficient is done only if
 * it exceeds the threshold value presented in this table.
 *
 * input   : qp/6, qp%6, index location (i,j)
 * output  : Threshold constant.
 *
 * @remarks 16 constants for each index position of the subblock and 6 for each
 * qp%6 in the range 0-5 inclusive and 9 for each qp/6 in the range 0-51.
 ******************************************************************************
 */
const UWORD16 gu2_forward_quant_threshold_4x4[96] =
{
        426,    693,    426,    693,
        693,   1066,    693,   1066,
        426,    693,    426,    693,
        693,   1066,    693,   1066,

        469,    746,    469,    746,
        746,   1200,    746,   1200,
        469,    746,    469,    746,
        746,   1200,    746,   1200,

        554,    853,    554,    853,
        853,   1333,    853,   1333,
        554,    853,    554,    853,
        853,   1333,    853,   1333,

        597,    960,    597,    960,
        960,   1533,    960,   1533,
        597,    960,    597,    960,
        960,   1533,    960,   1533,

        682,   1066,    682,   1066,
       1066,   1666,   1066,   1666,
        682,   1066,    682,   1066,
       1066,   1666,   1066,   1666,

        767,   1226,    767,   1226,
       1226,   1933,   1226,   1933,
        767,   1226,    767,   1226,
       1226,   1933,   1226,   1933,
};

/**
 ******************************************************************************
 * @brief  Scale Table for quantizing 8x8 subblock. To quantize a given 8x8 DCT
 * transformed block, the coefficient at index location (i,j) is scaled by one of
 * the constants in this table and right shift the result by (QP_BITS_h264_8x8 +
 * floor(qp/6)), here qp is the quantization parameter used to quantize the mb.
 *
 * input   : qp%6, index location (i,j)
 * output  : scale constant.
 *
 * @remarks 64 constants for each index position of the subblock and 6 for each
 * qp%6 in the range 0-5 inclusive.
 ******************************************************************************
 */
const UWORD16 gu2_quant_scale_matrix_8x8 [384] =
{
      13107,  12222,  16777,  12222,  13107,  12222,  16777,  12222,
      12222,  11428,  15481,  11428,  12222,  11428,  15481,  11428,
      16777,  15481,  20972,  15481,  16777,  15481,  20972,  15481,
      12222,  11428,  15481,  11428,  12222,  11428,  15481,  11428,
      13107,  12222,  16777,  12222,  13107,  12222,  16777,  12222,
      12222,  11428,  15481,  11428,  12222,  11428,  15481,  11428,
      16777,  15481,  20972,  15481,  16777,  15481,  20972,  15481,
      12222,  11428,  15481,  11428,  12222,  11428,  15481,  11428,

      11916,  11058,  14980,  11058,  11916,  11058,  14980,  11058,
      11058,  10826,  14290,  10826,  11058,  10826,  14290,  10826,
      14980,  14290,  19174,  14290,  14980,  14290,  19174,  14290,
      11058,  10826,  14290,  10826,  11058,  10826,  14290,  10826,
      11916,  11058,  14980,  11058,  11916,  11058,  14980,  11058,
      11058,  10826,  14290,  10826,  11058,  10826,  14290,  10826,
      14980,  14290,  19174,  14290,  14980,  14290,  19174,  14290,
      11058,  10826,  14290,  10826,  11058,  10826,  14290,  10826,

      10082,   9675,  12710,   9675,  10082,   9675,  12710,   9675,
       9675,   8943,  11985,   8943,   9675,   8943,  11985,   8943,
      12710,  11985,  15978,  11985,  12710,  11985,  15978,  11985,
       9675,   8943,  11985,   8943,   9675,   8943,  11985,   8943,
      10082,   9675,  12710,   9675,  10082,   9675,  12710,   9675,
       9675,   8943,  11985,   8943,   9675,   8943,  11985,   8943,
      12710,  11985,  15978,  11985,  12710,  11985,  15978,  11985,
       9675,   8943,  11985,   8943,   9675,   8943,  11985,   8943,

       9362,   8931,  11984,   8931,   9362,   8931,  11984,   8931,
       8931,   8228,  11259,   8228,   8931,   8228,  11259,   8228,
      11984,  11259,  14913,  11259,  11984,  11259,  14913,  11259,
       8931,   8228,  11259,   8228,   8931,   8228,  11259,   8228,
       9362,   8931,  11984,   8931,   9362,   8931,  11984,   8931,
       8931,   8228,  11259,   8228,   8931,   8228,  11259,   8228,
      11984,  11259,  14913,  11259,  11984,  11259,  14913,  11259,
       8931,   8228,  11259,   8228,   8931,   8228,  11259,   8228,

       8192,   7740,  10486,   7740,   8192,   7740,  10486,   7740,
       7740,   7346,   9777,   7346,   7740,   7346,   9777,   7346,
      10486,   9777,  13159,   9777,  10486,   9777,  13159,   9777,
       7740,   7346,   9777,   7346,   7740,   7346,   9777,   7346,
       8192,   7740,  10486,   7740,   8192,   7740,  10486,   7740,
       7740,   7346,   9777,   7346,   7740,   7346,   9777,   7346,
      10486,   9777,  13159,   9777,  10486,   9777,  13159,   9777,
       7740,   7346,   9777,   7346,   7740,   7346,   9777,   7346,

       7282,   6830,   9118,   6830,   7282,   6830,   9118,   6830,
       6830,   6428,   8640,   6428,   6830,   6428,   8640,   6428,
       9118,   8640,  11570,   8640,   9118,   8640,  11570,   8640,
       6830,   6428,   8640,   6428,   6830,   6428,   8640,   6428,
       7282,   6830,   9118,   6830,   7282,   6830,   9118,   6830,
       6830,   6428,   8640,   6428,   6830,   6428,   8640,   6428,
       9118,   8640,  11570,   8640,   9118,   8640,  11570,   8640,
       6830,   6428,   8640,   6428,   6830,   6428,   8640,   6428,

};


/**
 ******************************************************************************
 * @brief  Specification of QPc as a function of qPi
 *
 * input   : qp luma
 * output  : qp chroma.
 *
 * @remarks Refer Table 8-15 of h264 specification.
 ******************************************************************************
 */
const UWORD8 gu1_qpc_fqpi[52] =
{
     0,     1,     2,     3,     4,     5,     6,     7,
     8,     9,    10,    11,    12,    13,    14,    15,
    16,    17,    18,    19,    20,    21,    22,    23,
    24,    25,    26,    27,    28,    29,    29,    30,
    31,    32,    32,    33,    34,    34,    35,    35,
    36,    36,    37,    37,    37,    38,    38,    38,
    39,    39,    39,    39,
};
