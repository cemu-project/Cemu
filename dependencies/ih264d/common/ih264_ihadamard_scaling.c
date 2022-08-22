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
 *  ih264_ihadamard_scaling.c
 *
 * @brief
 *  Contains definition of functions for h264 inverse hadamard 4x4 transform and scaling
 *
 * @author
 *  Mohit
 *
 *  @par List of Functions:
 *  - ih264_ihadamard_scaling_4x4()
 *
 * @remarks
 *
 *******************************************************************************
 */

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

/* User include files */
#include "ih264_typedefs.h"
#include "ih264_defs.h"
#include "ih264_trans_macros.h"
#include "ih264_macros.h"
#include "ih264_trans_data.h"
#include "ih264_size_defs.h"
#include "ih264_structs.h"
#include "ih264_trans_quant_itrans_iquant.h"

/*
 ********************************************************************************
 *
 * @brief This function performs a 4x4 inverse hadamard transform on the 4x4 DC coefficients
 * of a 16x16 intra prediction macroblock, and then performs scaling.
 * prediction buffer
 *
 * @par Description:
 *  The DC coefficients pass through a 2-stage inverse hadamard transform.
 *  This inverse transformed content is scaled to based on Qp value.
 *
 * @param[in] pi2_src
 *  input 4x4 block of DC coefficients
 *
 * @param[out] pi2_out
 *  output 4x4 block
 *
 * @param[in] pu2_iscal_mat
 *  pointer to scaling list
 *
 * @param[in] pu2_weigh_mat
 *  pointer to weight matrix
 *
 * @param[in] u4_qp_div_6
 *  Floor (qp/6)
 *
 * @param[in] pi4_tmp
 * temporary buffer of size 1*16
 *
 * @returns none
 *
 * @remarks none
 *
 *******************************************************************************
 */
void ih264_ihadamard_scaling_4x4(WORD16* pi2_src,
                                 WORD16* pi2_out,
                                 const UWORD16 *pu2_iscal_mat,
                                 const UWORD16 *pu2_weigh_mat,
                                 UWORD32 u4_qp_div_6,
                                 WORD32* pi4_tmp)
{
    WORD32 i;
    WORD32 x0, x1, x2, x3, x4, x5, x6, x7;
    WORD16* pi2_src_ptr, *pi2_out_ptr;
    WORD32* pi4_tmp_ptr;
    WORD32 rnd_fact = (u4_qp_div_6 < 6) ? (1 << (5 - u4_qp_div_6)) : 0;
    pi4_tmp_ptr = pi4_tmp;
    pi2_src_ptr = pi2_src;
    pi2_out_ptr = pi2_out;
    // Horizontal transform
    for(i = 0; i < SUB_BLK_WIDTH_4x4; i++)
    {
        x4 = pi2_src_ptr[0];
        x5 = pi2_src_ptr[1];
        x6 = pi2_src_ptr[2];
        x7 = pi2_src_ptr[3];

        x0 = x4 + x7;
        x1 = x5 + x6;
        x2 = x5 - x6;
        x3 = x4 - x7;

        pi4_tmp_ptr[0] = x0 + x1;
        pi4_tmp_ptr[1] = x2 + x3;
        pi4_tmp_ptr[2] = x0 - x1;
        pi4_tmp_ptr[3] = x3 - x2;

        pi4_tmp_ptr += SUB_BLK_WIDTH_4x4;
        pi2_src_ptr += SUB_BLK_WIDTH_4x4;
    }
    pi4_tmp_ptr = pi4_tmp;
    // Vertical Transform
    for(i = 0; i < SUB_BLK_WIDTH_4x4; i++)
    {
        x4 = pi4_tmp_ptr[0];
        x5 = pi4_tmp_ptr[4];
        x6 = pi4_tmp_ptr[8];
        x7 = pi4_tmp_ptr[12];

        x0 = x4 + x7;
        x1 = x5 + x6;
        x2 = x5 - x6;
        x3 = x4 - x7;

        pi4_tmp_ptr[0] = x0 + x1;
        pi4_tmp_ptr[4] = x2 + x3;
        pi4_tmp_ptr[8] = x0 - x1;
        pi4_tmp_ptr[12] = x3 - x2;

        pi4_tmp_ptr++;
    }
    pi4_tmp_ptr = pi4_tmp;
    //Scaling
    for(i = 0; i < (SUB_BLK_WIDTH_4x4 * SUB_BLK_WIDTH_4x4); i++)
    {
      INV_QUANT(pi4_tmp_ptr[i], pu2_iscal_mat[0], pu2_weigh_mat[0], u4_qp_div_6,
                rnd_fact, 6);
      pi2_out_ptr[i] = pi4_tmp_ptr[i];
    }
}

void ih264_ihadamard_scaling_2x2_uv(WORD16* pi2_src,
                                    WORD16* pi2_out,
                                    const UWORD16 *pu2_iscal_mat,
                                    const UWORD16 *pu2_weigh_mat,
                                    UWORD32 u4_qp_div_6,
                                    WORD32* pi4_tmp)
{
  WORD32 i4_x0,i4_x1,i4_x2,i4_x3,i4_x4,i4_x5,i4_x6,i4_x7;
  WORD32 i4_y0,i4_y1,i4_y2,i4_y3,i4_y4,i4_y5,i4_y6,i4_y7;

  UNUSED(pi4_tmp);

  i4_x4 = pi2_src[0];
  i4_x5 = pi2_src[1];
  i4_x6 = pi2_src[2];
  i4_x7 = pi2_src[3];

  i4_x0 = i4_x4 + i4_x5;
  i4_x1 = i4_x4 - i4_x5;
  i4_x2 = i4_x6 + i4_x7;
  i4_x3 = i4_x6 - i4_x7;

  i4_x4 = i4_x0+i4_x2;
  i4_x5 = i4_x1+i4_x3;
  i4_x6 = i4_x0-i4_x2;
  i4_x7 = i4_x1-i4_x3;

  INV_QUANT(i4_x4,pu2_iscal_mat[0],pu2_weigh_mat[0],u4_qp_div_6,0,5);
  INV_QUANT(i4_x5,pu2_iscal_mat[0],pu2_weigh_mat[0],u4_qp_div_6,0,5);
  INV_QUANT(i4_x6,pu2_iscal_mat[0],pu2_weigh_mat[0],u4_qp_div_6,0,5);
  INV_QUANT(i4_x7,pu2_iscal_mat[0],pu2_weigh_mat[0],u4_qp_div_6,0,5);

  pi2_out[0] = i4_x4;
  pi2_out[1] = i4_x5;
  pi2_out[2] = i4_x6;
  pi2_out[3] = i4_x7;

  i4_y4 = pi2_src[4];
  i4_y5 = pi2_src[5];
  i4_y6 = pi2_src[6];
  i4_y7 = pi2_src[7];

  i4_y0 = i4_y4 + i4_y5;
  i4_y1 = i4_y4 - i4_y5;
  i4_y2 = i4_y6 + i4_y7;
  i4_y3 = i4_y6 - i4_y7;

  i4_y4 = i4_y0+i4_y2;
  i4_y5 = i4_y1+i4_y3;
  i4_y6 = i4_y0-i4_y2;
  i4_y7 = i4_y1-i4_y3;

  INV_QUANT(i4_y4,pu2_iscal_mat[0],pu2_weigh_mat[0],u4_qp_div_6,0,5);
  INV_QUANT(i4_y5,pu2_iscal_mat[0],pu2_weigh_mat[0],u4_qp_div_6,0,5);
  INV_QUANT(i4_y6,pu2_iscal_mat[0],pu2_weigh_mat[0],u4_qp_div_6,0,5);
  INV_QUANT(i4_y7,pu2_iscal_mat[0],pu2_weigh_mat[0],u4_qp_div_6,0,5);

  pi2_out[4] = i4_y4;
  pi2_out[5] = i4_y5;
  pi2_out[6] = i4_y6;
  pi2_out[7] = i4_y7;
}
