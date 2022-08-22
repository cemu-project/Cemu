@/******************************************************************************
@ *
@ * Copyright (C) 2015 The Android Open Source Project
@ *
@ * Licensed under the Apache License, Version 2.0 (the "License");
@ * you may not use this file except in compliance with the License.
@ * You may obtain a copy of the License at:
@ *
@ * http://www.apache.org/licenses/LICENSE-2.0
@ *
@ * Unless required by applicable law or agreed to in writing, software
@ * distributed under the License is distributed on an "AS IS" BASIS,
@ * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
@ * See the License for the specific language governing permissions and
@ * limitations under the License.
@ *
@ *****************************************************************************
@ * Originally developed and contributed by Ittiam Systems Pvt. Ltd, Bangalore
@*/
@**
@ *******************************************************************************
@ * @file
@ *  ih264_ihadamard_scaling_a9.s
@ *
@ * @brief
@ *  Contains function definitions for inverse hadamard transform on 4x4 DC outputs
@ *  of 16x16 intra-prediction
@ *
@ * @author
@ *  Mohit
@ *
@ * @par List of Functions:
@ *  - ih264_ihadamard_scaling_4x4_a9()
@ *  - ih264_ihadamard_scaling_2x2_uv_a9()
@ *
@ * @remarks
@ *  None
@ *
@ *******************************************************************************
@ *
@ * @brief This function performs a 4x4 inverse hadamard transform on the 4x4 DC coefficients
@ * of a 16x16 intra prediction macroblock, and then performs scaling.
@ * prediction buffer
@ *
@ * @par Description:
@ *  The DC coefficients pass through a 2-stage inverse hadamard transform.
@ *  This inverse transformed content is scaled to based on Qp value.
@ *
@ * @param[in] pi2_src
@ *  input 4x4 block of DC coefficients
@ *
@ * @param[out] pi2_out
@ *  output 4x4 block
@ *
@ * @param[in] pu2_iscal_mat
@ *  pointer to scaling list
@ *
@ * @param[in] pu2_weigh_mat
@ *  pointer to weight matrix
@ *
@ * @param[in] u4_qp_div_6
@ *  Floor (qp/6)
@ *
@ * @param[in] pi4_tmp
@ * temporary buffer of size 1*16
@ *
@ * @returns none
@ *
@ * @remarks none
@ *
@ *******************************************************************************
@ *
@ *
@ *******************************************************************************
@ *
@ void ih264_ihadamard_scaling_4x4(WORD16* pi2_src,
@       WORD16* pi2_out,
@       const UWORD16 *pu2_iscal_mat,
@       const UWORD16 *pu2_weigh_mat,
@       UWORD32 u4_qp_div_6,
@       WORD32* pi4_tmp)
@**************Variables Vs Registers*****************************************
@r0 => *pi2_src
@r1 => *pi2_out
@r2 =>  *pu2_iscal_mat
@r3 =>  *pu2_weigh_mat
@r4 =>  u4_qp_div_6

.text
.p2align 2

    .global ih264_ihadamard_scaling_4x4_a9

ih264_ihadamard_scaling_4x4_a9:

@VLD4.S16 is used because the pointer is incremented by SUB_BLK_WIDTH_4x4
@If the macro value changes need to change the instruction according to it.
@Only one shift is done in horizontal inverse because,
@if u4_qp_div_6 is lesser than 4 then shift value will be neagative and do negative left shift, in this case rnd_factor has value
@if u4_qp_div_6 is greater than 4 then shift value will be positive and do left shift, here rnd_factor is 0

    stmfd         sp!, {r4-r12, r14}    @ stack stores the values of the arguments
    ldr           r4, [sp, #40]         @ Loads u4_qp_div_6
    vdup.s32      q10, r4               @ Populate the u4_qp_div_6 in Q10
    ldrh          r6, [r3]              @ load pu2_weight_mat[0] , H for unsigned halfword load
    ldrh          r7, [r2]              @ load pu2_iscal_mat[0] , H for unsigned halfword load
    mul           r6, r6, r7            @ pu2_iscal_mat[0]*pu2_weigh_mat[0]
    vdup.s32      q9, r6                @ Populate pu2_iscal_mat[0]*pu2_weigh_mat[0] 32-bit in Q9
    vpush         {d8-d15}
@=======================INVERSE HADAMARD TRANSFORM================================

    vld4.s16      {d0, d1, d2, d3}, [r0] @load x4,x5,x6,x7
    vaddl.s16     q12, d0, d3           @x0 = x4 + x7
    vaddl.s16     q13, d1, d2           @x1 = x5 + x6
    vsubl.s16     q14, d1, d2           @x2 = x5 - x6
    vsubl.s16     q15, d0, d3           @x3 = x4 - x7

    vadd.s32      q2, q12, q13          @pi4_tmp_ptr[0] = x0 + x1
    vadd.s32      q3, q15, q14          @pi4_tmp_ptr[1] = x3 + x2
    vsub.s32      q4, q12, q13          @pi4_tmp_ptr[2] = x0 - x1
    vsub.s32      q5, q15, q14          @pi4_tmp_ptr[3] = x3 - x2

    vtrn.32       q2, q3                @Transpose the register for vertical transform
    vtrn.32       q4, q5

    vswp          d5, d8                @Q2 = x4, Q4 = x6
    vswp          d7, d10               @Q3 = x5, Q5 = x7


    vadd.s32      q12, q2, q5           @x0 = x4+x7
    vadd.s32      q13, q3, q4           @x1 = x5+x6
    vsub.s32      q14, q3, q4           @x2 = x5-x6
    vsub.s32      q15, q2, q5           @x3 = x4-x7

    vadd.s32      q0, q12, q13          @pi4_tmp_ptr[0] = x0 + x1
    vadd.s32      q1, q15, q14          @pi4_tmp_ptr[1] = x3 + x2
    vsub.s32      q2, q12, q13          @pi4_tmp_ptr[2] = x0 - x1
    vsub.s32      q3, q15, q14          @pi4_tmp_ptr[3] = x3 - x2


    vmul.s32      q0, q0, q9            @ Q0  = p[i] = (x[i] * trns_coeff[i]) where i = 0..3
    vmul.s32      q1, q1, q9            @ Q1  = p[i] = (x[i] * trns_coeff[i]) where i = 4..7
    vmul.s32      q2, q2, q9            @ Q2  = p[i] = (x[i] * trns_coeff[i]) where i = 8..11
    vmul.s32      q3, q3, q9            @ Q3  = p[i] = (x[i] * trns_coeff[i]) where i = 12..15

    vshl.s32      q0, q0, q10           @ Q0  = q[i] = (p[i] << (qP/6)) where i = 0..3
    vshl.s32      q1, q1, q10           @ Q1  = q[i] = (p[i] << (qP/6)) where i = 4..7
    vshl.s32      q2, q2, q10           @ Q2  = q[i] = (p[i] << (qP/6)) where i = 8..11
    vshl.s32      q3, q3, q10           @ Q3  = q[i] = (p[i] << (qP/6)) where i = 12..15

    vqrshrn.s32   d0, q0, #0x6          @ D0  = c[i] = ((q[i] + 32) >> 4) where i = 0..3
    vqrshrn.s32   d1, q1, #0x6          @ D1  = c[i] = ((q[i] + 32) >> 4) where i = 4..7
    vqrshrn.s32   d2, q2, #0x6          @ D2  = c[i] = ((q[i] + 32) >> 4) where i = 8..11
    vqrshrn.s32   d3, q3, #0x6          @ D3  = c[i] = ((q[i] + 32) >> 4) where i = 12..15

    vst1.s16      {d0, d1, d2, d3}, [r1] @IV row store the value

    vpop          {d8-d15}
    ldmfd         sp!, {r4-r12, r15}    @Reload the registers from SP



@ *******************************************************************************
@ *
@ * @brief This function performs a 2x2 inverse hadamard transform for chroma block
@ *
@ * @par Description:
@ *  The DC coefficients pass through a 2-stage inverse hadamard transform.
@ *  This inverse transformed content is scaled to based on Qp value.
@ *  Both DC blocks of U and v blocks are processesd
@ *
@ * @param[in] pi2_src
@ *  input 1x8 block of ceffs. First 4 are from U and next from V
@ *
@ * @param[out] pi2_out
@ *  output 1x8 block
@ *
@ * @param[in] pu2_iscal_mat
@ *  pointer to scaling list
@ *
@ * @param[in] pu2_weigh_mat
@ *  pointer to weight matrix
@ *
@ * @param[in] u4_qp_div_6
@ *  Floor (qp/6)
@ *
@ * @returns none
@ *
@ * @remarks none
@ *
@ *******************************************************************************
@ *
@ *
@ *******************************************************************************
@ *
@ void ih264_ihadamard_scaling_2x2_uv(WORD16* pi2_src,
@                                  WORD16* pi2_out,
@                                  const UWORD16 *pu2_iscal_mat,
@                                  const UWORD16 *pu2_weigh_mat,
@                                  UWORD32 u4_qp_div_6,

    .global ih264_ihadamard_scaling_2x2_uv_a9
ih264_ihadamard_scaling_2x2_uv_a9:

@Registers used
@   r0 : *pi2_src
@   r1 : *pi2_out
@   r2 : *pu2_iscal_mat
@   r3 : *pu2_weigh_mat

    vld1.u16      d26[0], [r2]
    vld1.u16      d27[0], [r3]
    vmull.u16     q15, d26, d27         @pu2_iscal_mat[0] *  pu2_weigh_mat[0]
    vdup.u32      q15, d30[0]

    vld1.u16      d28[0], [sp]          @load qp/6

    vpush         {d8-d15}

    vmov.u16      d29, #5
    vsubl.u16     q14, d28, d29         @qp\6 - 5
    vdup.s32      q14, d28[0]

    vld2.s16      {d0, d1}, [r0]        @load 8 dc coeffs
                                        @i2_x4,i2_x6,i2_y4,i1_y6 -> d0
                                        @i2_x5,i2_x7,i2_y5,i1_y6 -> d1

    vaddl.s16     q1, d0, d1            @  i4_x0 = i4_x4 + i4_x5;...x2
    vsubl.s16     q2, d0, d1            @  i4_x1 = i4_x4 - i4_x5;...x3

    vtrn.s32      q1, q2                @i4_x0 i4_x1 -> q1

    vadd.s32      q3, q1, q2            @i4_x4 = i4_x0+i4_x2;.. i4_x5
    vsub.s32      q1, q1, q2            @i4_x6 = i4_x0-i4_x2;.. i4_x7

    vmul.s32      q5, q3, q15
    vmul.s32      q6, q1, q15

    vshl.s32      q7, q5, q14
    vshl.s32      q8, q6, q14

    vmovn.s32     d18, q7               @i4_x4 i4_x5 i4_y4 i4_y5
    vmovn.s32     d19, q8               @i4_x6 i4_x7 i4_y6 i4_y7

    vst2.s32      {d18-d19}, [r1]

    vpop          {d8-d15}
    bx            lr


