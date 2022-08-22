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
@ *  ih264_iquant_itrans_recon_a9.s
@ *
@ * @brief
@ *  Contains function definitions for single stage  inverse transform
@ *
@ * @author
@ *  Mohit
@ *  Harinarayanaan
@ *
@ * @par List of Functions:
@ *  - ih264_iquant_itrans_recon_4x4_a9()
@ *  - ih264_iquant_itrans_recon_8x8_a9()
@ *  - ih264_iquant_itrans_recon_chroma_4x4_a9()
@ *
@ * @remarks
@ *  None
@ *
@ *******************************************************************************
@*
@**
@ *******************************************************************************
@ *
@ * @brief
@ *  This function performs inverse quant and Inverse transform type Ci4 for 4*4 block
@ *
@ * @par Description:
@ *  Performs inverse transform Ci4 and adds the residue to get the
@ *  reconstructed block
@ *
@ * @param[in] pi2_src
@ *  Input 4x4 coefficients
@ *
@ * @param[in] pu1_pred
@ *  Prediction 4x4 block
@ *
@ * @param[out] pu1_out
@ *  Output 4x4 block
@ *
@ * @param[in] u4_qp_div_6
@ *     QP
@ *
@ * @param[in] pu2_weigh_mat
@ * Pointer to weight matrix
@ *
@ * @param[in] pred_strd,
@ *  Prediction stride
@ *
@ * @param[in] out_strd
@ *  Output Stride
@ *
@ *@param[in] pi2_tmp
@ * temporary buffer of size 1*16
@ *
@ * @param[in] pu2_iscal_mat
@ * Pointer to the inverse quantization matrix
@ *
@ * @returns  Void
@ *
@ * @remarks
@ *  None
@ *
@ *******************************************************************************
@ *
@void ih264_iquant_itrans_recon_4x4(WORD16 *pi2_src,
@                                   UWORD8 *pu1_pred,
@                                   UWORD8 *pu1_out,
@                                   WORD32 pred_strd,
@                                   WORD32 out_strd,
@                                   const UWORD16 *pu2_iscal_mat,
@                                   const UWORD16 *pu2_weigh_mat,
@                                   UWORD32 u4_qp_div_6,
@                                   WORD32 *pi4_tmp,
@                                   WORD32 iq_start_idx
@                                   WORD16 *pi2_dc_ld_addr)
@**************Variables Vs Registers*****************************************
@r0 => *pi2_src
@r1 => *pu1_pred
@r2 => *pu1_out
@r3 =>  pred_strd
@r4 =>  out_strd
@r5 =>  *pu2_iscal_mat
@r6 =>  *pu2_weigh_mat
@r7 =>  u4_qp_div_6
@r8 =>  iq_start_idx
@r10=>  pi2_dc_ld_addr
.text
.syntax unified
.p2align 2

    .global ih264_iquant_itrans_recon_4x4_a9

ih264_iquant_itrans_recon_4x4_a9:

@VLD4.S16 is used because the pointer is incremented by SUB_BLK_WIDTH_4x4
@If the macro value changes need to change the instruction according to it.
@Only one shift is done in horizontal inverse because,
@if u4_qp_div_6 is lesser than 4 then shift value will be neagative and do negative left shift, in this case rnd_factor has value
@if u4_qp_div_6 is greater than 4 then shift value will be positive and do left shift, here rnd_factor is 0

    stmfd         sp!, {r4-r12, r14}    @stack stores the values of the arguments
    ldr           r7, [sp, #52]         @Loads u4_qp_div_6
    ldr           r4, [sp, #40]         @Loads out_strd
    vdup.s32      q15, r7               @Populate the u4_qp_div_6 in Q15
    ldr           r5, [sp, #44]         @Loads *pu2_iscal_mat

    ldr           r6, [sp, #48]         @Loads *pu2_weigh_mat

    ldr           r8, [sp, #60]         @Loads iq_start_idx

    ldr           r10, [sp, #64]        @Load alternate dc address

    vpush         {d8-d15}
@=======================DEQUANT FROM HERE===================================

    vld4.s16      {d20, d21, d22, d23}, [r5] @Load pu2_iscal_mat[i], i =0..15
    vld4.s16      {d26, d27, d28, d29}, [r6] @pu2_weigh_mat[i], i =0..15
    vmul.s16      q10, q10, q13         @x[i]=(scale[i] * dequant[i]) where i = 0..7
    vld4.s16      {d16, d17, d18, d19}, [r0] @pi2_src_tmp[i], i =0..15

    vmul.s16      q11, q11, q14         @x[i]=(scale[i] * dequant[i]) where i = 8..15

    subs          r8, r8, #1            @ if r8 == 1 => intra case , so result of subtraction is zero and Z flag is set
    ldrsheq       r9, [r10]             @ Loads signed halfword pi2_dc_ld_addr[0], if r8==1

    vmull.s16     q0, d16, d20          @ Q0  = p[i] = (x[i] * trns_coeff[i]) where i = 0..3
    vmull.s16     q1, d17, d21          @ Q1  = p[i] = (x[i] * trns_coeff[i]) where i = 4..7
    vmull.s16     q2, d18, d22          @ Q2  = p[i] = (x[i] * trns_coeff[i]) where i = 8..11
    vmull.s16     q3, d19, d23          @ Q3  = p[i] = (x[i] * trns_coeff[i]) where i = 12..15

    vshl.s32      q0, q0, q15           @ Q0  = q[i] = (p[i] << (qP/6)) where i = 0..3
    vshl.s32      q1, q1, q15           @ Q1  = q[i] = (p[i] << (qP/6)) where i = 4..7
    vshl.s32      q2, q2, q15           @ Q2  = q[i] = (p[i] << (qP/6)) where i = 8..11
    vshl.s32      q3, q3, q15           @ Q3  = q[i] = (p[i] << (qP/6)) where i = 12..15

    vqrshrn.s32   d0, q0, #0x4          @ D0  = c[i] = ((q[i] + 32) >> 4) where i = 0..3
    vqrshrn.s32   d1, q1, #0x4          @ D1  = c[i] = ((q[i] + 32) >> 4) where i = 4..7
    vqrshrn.s32   d2, q2, #0x4          @ D2  = c[i] = ((q[i] + 32) >> 4) where i = 8..11
    vqrshrn.s32   d3, q3, #0x4          @ D3  = c[i] = ((q[i] + 32) >> 4) where i = 12..15

    vmoveq.16     d0[0], r9             @ Restore dc value in case of intra, i.e. r8 == 1

@========= PROCESS IDCT FROM HERE =======
@Steps for Stage 1:
@------------------
    vld1.32       d30[0], [r1], r3      @I row Load pu1_pred buffer
    vadd.s16      d4, d0, d2            @x0 = q0 + q1;

    vsub.s16      d5, d0, d2            @x1 = q0 - q1;

    vshr.s16      d8, d1, #1            @q0>>1
    vshr.s16      d9, d3, #1            @q1>>1

    vsub.s16      d6, d8, d3            @x2 = (q0 >> 1) -  q1;
    vadd.s16      d7, d1, d9            @x3 = q0+ (q1 >> 1);
    vld1.32       d30[1], [r1], r3      @II row Load pu1_pred buffer

    vswp          d6, d7                @Reverse positions of x2 and x3

    vsub.s16      q6, q2, q3            @x0-x3 and x1-x2 combined
    vadd.s16      q5, q2, q3            @x0 + x3 and x1+x2 combined

    vld1.32       d31[0], [r1], r3      @III row Load pu1_pred buf

    vswp          d12, d13
@Steps for Stage 2:
@------------------
    vtrn.16       d10, d11
    vtrn.16       d12, d13
    vtrn.32       d10, d12
    vtrn.32       d11, d13
    vadd.s16      d14, d10, d12         @x0 = q0 + q1;

    vsub.s16      d15, d10, d12         @x1 = q0 - q1;

    vshr.s16      d18, d11, #1          @q0>>1
    vshr.s16      d19, d13, #1          @q1>>1

    vsub.s16      d16, d18, d13         @x2 = (q0 >> 1) -  q1;
    vadd.s16      d17, d11, d19         @x3 = q0+ (q1 >> 1);

    vld1.32       d31[1], [r1], r3      @IV row Load pu1_pred buffer
    vswp          d16, d17              @Reverse positions of x2 and x3

    vsub.s16      q11, q7, q8           @x0-x3 and x1-x2 combined
    vadd.s16      q10, q7, q8           @x0 + x3 and x1+x2 combined

    vswp          d22, d23

    vrshr.s16     q10, q10, #6          @
    vrshr.s16     q11, q11, #6

    vaddw.u8      q10, q10, d30
    vaddw.u8      q11, q11, d31

    vqmovun.s16   d0, q10
    vqmovun.s16   d1, q11

    vst1.32       d0[0], [r2], r4       @I row store the value
    vst1.32       d0[1], [r2], r4       @II row store the value
    vst1.32       d1[0], [r2], r4       @III row store the value
    vst1.32       d1[1], [r2]           @IV row store the value

    vpop          {d8-d15}
    ldmfd         sp!, {r4-r12, r15}    @Reload the registers from SP


@**
@ *******************************************************************************
@ *
@ * @brief
@ *  This function performs inverse quant and Inverse transform type Ci4 for 4*4 block
@ *
@ * @par Description:
@ *  Performs inverse transform Ci4 and adds the residue to get the
@ *  reconstructed block
@ *
@ * @param[in] pi2_src
@ *  Input 4x4 coefficients
@ *
@ * @param[in] pu1_pred
@ *  Prediction 4x4 block
@ *
@ * @param[out] pu1_out
@ *  Output 4x4 block
@ *
@ * @param[in] u4_qp_div_6
@ *     QP
@ *
@ * @param[in] pu2_weigh_mat
@ * Pointer to weight matrix
@ *
@ * @param[in] pred_strd,
@ *  Prediction stride
@ *
@ * @param[in] out_strd
@ *  Output Stride
@ *
@ *@param[in] pi2_tmp
@ * temporary buffer of size 1*16
@ *
@ * @param[in] pu2_iscal_mat
@ * Pointer to the inverse quantization matrix
@ *
@ * @returns  Void
@ *
@ * @remarks
@ *  None
@ *
@ *******************************************************************************
@ *
@void ih264_iquant_itrans_recon_chroma_4x4(WORD16 *pi2_src,
@                                   UWORD8 *pu1_pred,
@                                   UWORD8 *pu1_out,
@                                   WORD32 pred_strd,
@                                   WORD32 out_strd,
@                                   const UWORD16 *pu2_iscal_mat,
@                                   const UWORD16 *pu2_weigh_mat,
@                                   UWORD32 u4_qp_div_6,
@                                   WORD32 *pi4_tmp
@                                   WORD16 *pi2_dc_src)
@**************Variables Vs Registers*****************************************
@r0 => *pi2_src
@r1 => *pu1_pred
@r2 => *pu1_out
@r3 =>  pred_strd
@r4 =>  out_strd
@r5 =>  *pu2_iscal_mat
@r6 =>  *pu2_weigh_mat
@r7 =>  u4_qp_div_6

    .global ih264_iquant_itrans_recon_chroma_4x4_a9
ih264_iquant_itrans_recon_chroma_4x4_a9:

@VLD4.S16 is used because the pointer is incremented by SUB_BLK_WIDTH_4x4
@If the macro value changes need to change the instruction according to it.
@Only one shift is done in horizontal inverse because,
@if u4_qp_div_6 is lesser than 4 then shift value will be neagative and do negative left shift, in this case rnd_factor has value
@if u4_qp_div_6 is greater than 4 then shift value will be positive and do left shift, here rnd_factor is 0

    stmfd         sp!, {r4-r12, r14}    @stack stores the values of the arguments
    ldr           r7, [sp, #52]         @Loads u4_qp_div_6
    ldr           r4, [sp, #40]         @Loads out_strd
    vdup.s32      q15, r7               @Populate the u4_qp_div_6 in Q15
    ldr           r5, [sp, #44]         @Loads *pu2_iscal_mat
    ldr           r6, [sp, #48]         @Loads *pu2_weigh_mat
    ldr           r8, [sp, #60]         @loads *pi2_dc_src

    vpush         {d8-d15}
@=======================DEQUANT FROM HERE===================================

    vld4.s16      {d20, d21, d22, d23}, [r5] @Load pu2_iscal_mat[i], i =0..15
    vld4.s16      {d26, d27, d28, d29}, [r6] @pu2_weigh_mat[i], i =0..15
    vmul.s16      q10, q10, q13         @x[i]=(scale[i] * dequant[i]) where i = 0..7
    vld4.s16      {d16, d17, d18, d19}, [r0] @pi2_src_tmp[i], i =0..15

    vmul.s16      q11, q11, q14         @x[i]=(scale[i] * dequant[i]) where i = 8..15

    vmull.s16     q0, d16, d20          @ Q0  = p[i] = (x[i] * trns_coeff[i]) where i = 0..3
    vmull.s16     q1, d17, d21          @ Q1  = p[i] = (x[i] * trns_coeff[i]) where i = 4..7
    vmull.s16     q2, d18, d22          @ Q2  = p[i] = (x[i] * trns_coeff[i]) where i = 8..11
    vmull.s16     q3, d19, d23          @ Q3  = p[i] = (x[i] * trns_coeff[i]) where i = 12..15

    vshl.s32      q0, q0, q15           @ Q0  = q[i] = (p[i] << (qP/6)) where i = 0..3
    vshl.s32      q1, q1, q15           @ Q1  = q[i] = (p[i] << (qP/6)) where i = 4..7
    vshl.s32      q2, q2, q15           @ Q2  = q[i] = (p[i] << (qP/6)) where i = 8..11
    vshl.s32      q3, q3, q15           @ Q3  = q[i] = (p[i] << (qP/6)) where i = 12..15

    vqrshrn.s32   d0, q0, #0x4          @ D0  = c[i] = ((q[i] + 32) >> 4) where i = 0..3
    vqrshrn.s32   d1, q1, #0x4          @ D1  = c[i] = ((q[i] + 32) >> 4) where i = 4..7
    vqrshrn.s32   d2, q2, #0x4          @ D2  = c[i] = ((q[i] + 32) >> 4) where i = 8..11
    vqrshrn.s32   d3, q3, #0x4          @ D3  = c[i] = ((q[i] + 32) >> 4) where i = 12..15

    ldrsh         r9, [r8]              @ Loads signed halfword pi2_dc_src[0]
    vmov.16       d0[0], r9             @ Restore dc value since its chroma iq-it

@========= PROCESS IDCT FROM HERE =======
@Steps for Stage 1:
@------------------
    vld2.8        {d28, d29}, [r1], r3  @I row Load pu1_pred buffer
    vadd.s16      d4, d0, d2            @x0 = q0 + q1;

    vsub.s16      d5, d0, d2            @x1 = q0 - q1;

    vshr.s16      d8, d1, #1            @q0>>1
    vshr.s16      d9, d3, #1            @q1>>1

    vsub.s16      d6, d8, d3            @x2 = (q0 >> 1) -  q1;
    vadd.s16      d7, d1, d9            @x3 = q0+ (q1 >> 1);
    vld2.8        {d29, d30}, [r1], r3  @II row Load pu1_pred buffer

    vswp          d6, d7                @Reverse positions of x2 and x3

    vsub.s16      q6, q2, q3            @x0-x3 and x1-x2 combined
    vtrn.32       d28, d29              @ D28 -- row I and II of pu1_pred_buffer
    vadd.s16      q5, q2, q3            @x0 + x3 and x1+x2 combined

    vld2.8        {d29, d30}, [r1], r3  @III row Load pu1_pred buf

    vswp          d12, d13
@Steps for Stage 2:
@------------------
    vtrn.16       d10, d11
    vtrn.16       d12, d13
    vtrn.32       d10, d12
    vtrn.32       d11, d13
    vadd.s16      d14, d10, d12         @x0 = q0 + q1;

    vsub.s16      d15, d10, d12         @x1 = q0 - q1;

    vshr.s16      d18, d11, #1          @q0>>1
    vshr.s16      d19, d13, #1          @q1>>1

    vsub.s16      d16, d18, d13         @x2 = (q0 >> 1) -  q1;
    vadd.s16      d17, d11, d19         @x3 = q0+ (q1 >> 1);

    vld2.8        {d30, d31}, [r1], r3  @IV row Load pu1_pred buffer
    vswp          d16, d17              @Reverse positions of x2 and x3

    vsub.s16      q11, q7, q8           @x0-x3 and x1-x2 combined
    vtrn.32       d29, d30              @ D29 -- row III and IV of pu1_pred_buf
    vadd.s16      q10, q7, q8           @x0 + x3 and x1+x2 combined

    vswp          d22, d23

    vrshr.s16     q10, q10, #6          @
    vrshr.s16     q11, q11, #6

    vaddw.u8      q10, q10, d28
    vaddw.u8      q11, q11, d29

    vld1.u8       d0, [r2], r4          @Loading out buffer 16 coeffs
    vld1.u8       d1, [r2], r4
    vld1.u8       d2, [r2], r4
    vld1.u8       d3, [r2], r4

    sub           r2, r2, r4, lsl #2

    vqmovun.s16   d20, q10              @Getting quantized coeffs
    vqmovun.s16   d22, q11

    vmovl.u8      q10, d20              @Move the coffs into 16 bit
    vmovl.u8      q11, d22              @so that we can use vbit to copy

    vmov.u16      q14, #0x00ff          @Copy lsb from qantized(long)coeffs

    vbit.u8       q0, q10, q14
    vbit.u8       q1, q11, q14

    vst1.u8       d0, [r2], r4
    vst1.u8       d1, [r2], r4
    vst1.u8       d2, [r2], r4
    vst1.u8       d3, [r2]

    vpop          {d8-d15}
    ldmfd         sp!, {r4-r12, r15}    @Reload the registers from SP


@*
@ *******************************************************************************
@ *
@ * @brief
@ *  This function performs inverse quant and Inverse transform type Ci4 for 8*8 block
@ *
@ * @par Description:
@ *  Performs inverse transform Ci8 and adds the residue to get the
@ *  reconstructed block
@ *
@ * @param[in] pi2_src
@ *  Input 4x4 coefficients
@ *
@ * @param[in] pu1_pred
@ *  Prediction 4x4 block
@ *
@ * @param[out] pu1_out
@ *  Output 4x4 block
@ *
@ * @param[in] u4_qp_div_6
@ *     QP
@ *
@ * @param[in] pu2_weigh_mat
@ * Pointer to weight matrix
@ *
@ * @param[in] pred_strd,
@ *  Prediction stride
@ *
@ * @param[in] out_strd
@ *  Output Stride
@ *
@ *@param[in] pi2_tmp
@ * temporary buffer of size 1*64
@ *
@ * @param[in] pu2_iscal_mat
@ * Pointer to the inverse quantization matrix
@ *
@ * @returns  Void
@ *
@ * @remarks
@ *  None
@ *
@ *******************************************************************************
@ *
@void ih264_iquant_itrans_recon_8x8(WORD16 *pi2_src,
@                                   UWORD8 *pu1_pred,
@                                   UWORD8 *pu1_out,
@                                   WORD32 pred_strd,
@                                   WORD32 out_strd,
@                                   const UWORD16 *pu2_iscal_mat,
@                                   const UWORD16 *pu2_weigh_mat,
@                                   UWORD32 u4_qp_div_6,
@                                   WORD32 *pi4_tmp,
@                                   WORD32 iq_start_idx)
@**************Variables Vs Registers*****************************************
@r0 => *pi2_src
@r1 => *pu1_pred
@r2 => *pu1_out
@r3 =>  pred_strd
@r4 =>  out_strd
@r5 =>  *pu2_iscal_mat
@r6 =>  *pu2_weigh_mat
@r7 =>  u4_qp_div_6


    .global ih264_iquant_itrans_recon_8x8_a9
ih264_iquant_itrans_recon_8x8_a9:

    stmfd         sp!, {r4-r12, r14}    @stack stores the values of the arguments
    ldr           r7, [sp, #52]         @Loads u4_qp_div_6
    ldr           r4, [sp, #40]         @Loads out_strd

    ldr           r5, [sp, #44]         @Loads *pu2_iscal_mat
    ldr           r6, [sp, #48]         @Loads *pu2_weigh_mat
    vdup.s32      q15, r7               @Populate the u4_qp_div_6 in Q15
    vpush         {d8-d15}

idct_8x8_begin:

@========= DEQUANT FROM HERE ===========

    vld1.32       {q13}, [r5]!          @ Q13 = dequant values row 0
    vld1.32       {q10}, [r6]!          @ Q10 = scaling factors row 0
    vld1.32       {q14}, [r5]!          @ Q14 = dequant values row 1
    vmul.s16      q10, q10, q13         @ Q10 = x[i] = (scale[i] * dequant[i]) where i = 0..7
    vld1.32       {q11}, [r6]!          @ Q11 = scaling factors row 1
    vld1.32       {q8}, [r0]!           @ Q8  = Source row 0
    vmul.s16      q11, q11, q14         @ Q11 = x[i] = (scale[i] * dequant[i]) where i = 8..15
    vmull.s16     q0, d16, d20          @ Q0  = p[i] = (x[i] * trns_coeff[i]) where i = 0..3
    vld1.32       {q9}, [r0]!           @ Q8  = Source row 1
    vmull.s16     q1, d17, d21          @ Q1  = p[i] = (x[i] * trns_coeff[i]) where i = 4..7
    vmull.s16     q2, d18, d22          @ Q2  = p[i] = (x[i] * trns_coeff[i]) where i = 8..11
    vld1.32       {q13}, [r6]!          @ Scaling factors row 2
    vmull.s16     q3, d19, d23          @ Q3  = p[i] = (x[i] * trns_coeff[i]) where i = 12..15
    vld1.32       {q14}, [r6]!          @ Scaling factors row 3
    vshl.s32      q0, q0, q15           @ Q0  = q[i] = (p[i] << (qP/6)) where i = 0..3
    vld1.32       {q10}, [r5]!          @ Q10 = Dequant values row 2
    vshl.s32      q1, q1, q15           @ Q1  = q[i] = (p[i] << (qP/6)) where i = 4..7
    vld1.32       {q8}, [r0]!           @ Source Row 2
    vshl.s32      q2, q2, q15           @ Q2  = q[i] = (p[i] << (qP/6)) where i = 8..11
    vld1.32       {q11}, [r5]!          @ Q11 = Dequant values row 3
    vshl.s32      q3, q3, q15           @ Q3  = q[i] = (p[i] << (qP/6)) where i = 12..15
    vld1.32       {q9}, [r0]!           @ Source Row 3
    vmul.s16      q10, q10, q13         @ Dequant row2*scale matrix row 2
    vmul.s16      q11, q11, q14         @ Dequant row 3*scale matrix row 3
    vld1.32       {q4}, [r6]!           @ Scaling factors row 4
    vqrshrn.s32   d0, q0, #0x6          @ D0  = c[i] = ((q[i] + 32) >> 6) where i = 0..3
    vqrshrn.s32   d1, q1, #0x6          @ D1  = c[i] = ((q[i] + 32) >> 6) where i = 4..7
    vld1.32       {q5}, [r6]!           @ Scaling factors row 5
    vqrshrn.s32   d2, q2, #0x6          @ D2  = c[i] = ((q[i] + 32) >> 6) where i = 8..11
    vqrshrn.s32   d3, q3, #0x6          @ D3  = c[i] = ((q[i] + 32) >> 6) where i = 12..15
    vld1.32       {q13}, [r5]!          @ Q13 = Dequant values row 4
    vmull.s16     q2, d16, d20          @ p[i] = (x[i] * trns_coeff[i]) where i=16..19
    vmull.s16     q3, d17, d21          @ p[i] = (x[i] * trns_coeff[i]) where i=20..23
    vld1.32       {q12}, [r5]!          @ Q12 = Dequant values row 5
    vmull.s16     q6, d18, d22          @ p[i] = (x[i] * trns_coeff[i]) where i=24..27
    vmull.s16     q7, d19, d23          @ p[i] = (x[i] * trns_coeff[i]) where i=28..31

    vld1.32       {q14}, [r0]!          @ Source row 4
    vmul.s16      q10, q4, q13          @ Dequant row4*scale matrix row 4
    vmul.s16      q11, q5, q12          @ Dequant row5*scale matrix row 5
    vld1.32       {q9}, [r0]!           @ Source row 5
    vshl.s32      q2, q2, q15           @
    vshl.s32      q3, q3, q15           @
    vld1.32       {q13}, [r6]!          @ Scaling factors row 6
    vshl.s32      q6, q6, q15           @
    vshl.s32      q7, q7, q15           @
    vmull.s16     q4, d28, d20          @ i = 32..35
    vqrshrn.s32   d4, q2, #0x6          @ D4  = c[i] = ((q[i] + 32) >> 6) where i = 16..19
    vqrshrn.s32   d5, q3, #0x6          @ D5  = c[i] = ((q[i] + 32) >> 6) where i = 20..23
    vmull.s16     q5, d29, d21          @ i =36..39
    vld1.32       {q10}, [r5]!          @ Dequant values row 6
    vqrshrn.s32   d6, q6, #0x6          @ D6  = c[i] = ((q[i] + 32) >> 6) where i = 24..27
    vqrshrn.s32   d7, q7, #0x6          @ D7  = c[i] = ((q[i] + 32) >> 6) where i = 28..31
    vld1.32       {q14}, [r6]!          @ Scaling factors row 7
    vmull.s16     q6, d18, d22          @
    vld1.32       {q8}, [r0]!           @ Source row 6
    vmull.s16     q7, d19, d23          @
    vld1.32       {q11}, [r5]!          @ Dequant values row 7
    vshl.s32      q4, q4, q15           @
    vld1.32       {q9}, [r0]!           @ Source row 7
    vshl.s32      q5, q5, q15           @

    vshl.s32      q6, q6, q15           @
    vshl.s32      q7, q7, q15           @
    vmul.s16      q10, q10, q13         @ Dequant*scaling row 6
    vmul.s16      q11, q11, q14         @ Dequant*scaling row 7
    vqrshrn.s32   d8, q4, #0x6          @ D8  = c[i] = ((q[i] + 32) >> 6) where i = 32..35
    vqrshrn.s32   d9, q5, #0x6          @ D9  = c[i] = ((q[i] + 32) >> 6) where i = 36..39
    vqrshrn.s32   d10, q6, #0x6         @ D10  = c[i] = ((q[i] + 32) >> 6) where i = 40..43
    vqrshrn.s32   d11, q7, #0x6         @ D11  = c[i] = ((q[i] + 32) >> 6) where i = 44..47
    vmull.s16     q6, d16, d20          @ i= 48..51
    vmull.s16     q7, d17, d21          @ i= 52..55
    vmull.s16     q8, d18, d22          @ i=56..59
    vmull.s16     q9, d19, d23          @ i=60..63
    vshl.s32      q6, q6, q15           @
    vzip.s16      q0, q1                @Transpose
    vshl.s32      q7, q7, q15           @
    vshl.s32      q8, q8, q15           @
    vzip.s16      q2, q3                @
    vshl.s32      q9, q9, q15           @
    vqrshrn.s32   d12, q6, #0x6         @ D12  = c[i] = ((q[i] + 32) >> 6) where i = 48..51
    vzip.s16      q4, q5                @Transpose
    vqrshrn.s32   d13, q7, #0x6         @ D13  = c[i] = ((q[i] + 32) >> 6) where i = 52..55
    vqrshrn.s32   d14, q8, #0x6         @ D14  = c[i] = ((q[i] + 32) >> 6) where i = 56..59
    vzip.s32      q0, q2                @Transpose
    vqrshrn.s32   d15, q9, #0x6         @ D15  = c[i] = ((q[i] + 32) >> 6) where i = 60..63

@========= PROCESS IDCT FROM HERE =======

@Steps for Stage 2:
@------------------

@   TRANSPOSE 8x8 coeffs to actual order

    vzip.s16      q6, q7                @

    vzip.s32      q1, q3                @
    vzip.s32      q4, q6                @
    vzip.s32      q5, q7                @

    vswp          d1, d8                @ Q0/Q1 = Row order x0/x1
    vswp          d3, d10               @ Q2/Q3 = Row order x2/x3
    vswp          d5, d12               @ Q4/Q5 = Row order x4/x5
    vswp          d7, d14               @ Q6/Q7 = Row order x6/x7

    vswp          q1, q4                @
    vshr.s16      q10, q2, #0x1         @
    vswp          q3, q6                @

@Steps for Stage 1:
@------------------

    vadd.s16      q8, q0, q4            @ Q8 = y0
    vsub.s16      q9, q0, q4            @ Q9 = y2

    vsra.s16      q2, q6, #0x1          @ Q2 = y6
    vsub.s16      q6, q10, q6           @ Q6 = y4

    vaddl.s16     q12, d14, d2          @ y3 (0-3) 1+7
    vaddl.s16     q13, d15, d3          @ y3 (4-7) 1+7

    vsubl.s16     q10, d14, d2          @ y5 (0-3) 7-1
    vsubl.s16     q11, d15, d3          @ y5 (4-7) 7-1

    vadd.s16      q0, q8, q2            @ Q0 = z0
    vsub.s16      q4, q8, q2            @ Q4 = z6

    vadd.s16      q8, q9, q6            @ Q8 = z2
    vsub.s16      q2, q9, q6            @ Q2 = z4

    vsubw.s16     q12, q12, d6          @ y3 (0-3) 1+7-3
    vsubw.s16     q13, q13, d7          @ y3 (0-7) 1+7-3

    vshr.s16      q6, q3, #0x1          @

    vaddw.s16     q10, q10, d10         @
    vaddw.s16     q11, q11, d11         @

    vshr.s16      q9, q5, #0x1          @

    vsubw.s16     q12, q12, d12         @
    vsubw.s16     q13, q13, d13         @

    vaddw.s16     q10, q10, d18         @
    vaddw.s16     q11, q11, d19         @

    vqmovn.s32    d12, q12              @
    vaddl.s16     q12, d10, d6          @
    vqmovn.s32    d13, q13              @ Q6 = y3
    vaddl.s16     q13, d11, d7          @
    vqmovn.s32    d18, q10              @
    vsubl.s16     q10, d10, d6          @
    vqmovn.s32    d19, q11              @ Q9 = y5
    vsubl.s16     q11, d11, d7          @

    vshr.s16      q3, q6, #0x2          @

    vsra.s16      q6, q9, #0x2          @ Q6 = z3

    vaddw.s16     q12, q12, d2          @
    vaddw.s16     q13, q13, d3          @

    vshr.s16      q1, #0x1              @

    vsub.s16      q5, q3, q9            @ Q5 = z5

    vsubw.s16     q10, q10, d14         @
    vsubw.s16     q11, q11, d15         @

    vshr.s16      q7, #0x1              @

    vaddw.s16     q12, q12, d2          @
    vaddw.s16     q13, q13, d3          @

    vsubw.s16     q10, q10, d14         @
    vsubw.s16     q11, q11, d15         @


    vqmovn.s32    d14, q12              @
    vadd.s16      q1, q8, q5            @ Q1 = x1
    vqmovn.s32    d15, q13              @ Q7 = y7
    vsub.s16      q3, q8, q5            @ Q3 = x6
    vqmovn.s32    d18, q10              @
    vsub.s16      q5, q2, q6            @ Q5 = x5
    vqmovn.s32    d19, q11              @ Q9 = y1
    vadd.s16      q2, q2, q6            @ Q2 = x2

    vshr.s16      q12, q9, #0x2         @
    vsra.s16      q9, q7, #0x2          @ Q9 = z1

    vsub.s16      q11, q7, q12          @ Q11 = z7

    vadd.s16      q6, q4, q9            @ Q6 = x3
    vsub.s16      q4, q4, q9            @ Q4 = x4

    vsub.s16      q7, q0, q11           @ Q7 = x7
    vadd.s16      q0, q0, q11           @ Q0 = x0

    vswp.s16      q3, q6                @ Q3 = x3, Q6 = x6


@Steps for Stage 2:
@------------------

@   TRANSPOSE 8x8 coeffs to actual order

    vzip.s16      q0, q1                @
    vzip.s16      q2, q3                @
    vzip.s16      q4, q5                @
    vzip.s16      q6, q7                @

    vzip.s32      q0, q2                @
    vzip.s32      q1, q3                @
    vzip.s32      q4, q6                @
    vzip.s32      q5, q7                @

    vswp          d1, d8                @ Q0/Q1 = Row order x0/x1
    vswp          d3, d10               @ Q2/Q3 = Row order x2/x3
    vswp          d5, d12               @ Q4/Q5 = Row order x4/x5
    vswp          d7, d14               @ Q6/Q7 = Row order x6/x7

    vswp          q1, q4                @
    vshr.s16      q10, q2, #0x1         @
    vswp          q3, q6                @

@Steps for Stage 3:
@------------------

@Repeat stage 1 again for vertical transform

    vadd.s16      q8, q0, q4            @ Q8 = y0
    vld1.32       d28, [r1], r3         @ Q12 = 0x070605....0x070605....
    vsub.s16      q9, q0, q4            @ Q9 = y2

    vsra.s16      q2, q6, #0x1          @ Q2 = y6
    vsub.s16      q6, q10, q6           @ Q6 = y4

    vaddl.s16     q12, d14, d2          @
    vld1.32       d29, [r1], r3         @ Q12 = 0x070605....0x070605....
    vaddl.s16     q13, d15, d3          @

    vsubl.s16     q10, d14, d2          @
    vld1.32       d30, [r1], r3         @ Q12 = 0x070605....0x070605....
    vsubl.s16     q11, d15, d3          @

    vadd.s16      q0, q8, q2            @ Q0 = z0
    vld1.32       d31, [r1], r3         @ Q12 = 0x070605....0x070605....
    vsub.s16      q4, q8, q2            @ Q4 = z6

    vadd.s16      q8, q9, q6            @ Q8 = z2
    vsub.s16      q2, q9, q6            @ Q2 = z4

    vsubw.s16     q12, q12, d6          @
    vsubw.s16     q13, q13, d7          @

    vshr.s16      q6, q3, #0x1          @

    vaddw.s16     q10, q10, d10         @
    vaddw.s16     q11, q11, d11         @

    vshr.s16      q9, q5, #0x1          @

    vsubw.s16     q12, q12, d12         @
    vsubw.s16     q13, q13, d13         @

    vaddw.s16     q10, q10, d18         @
    vaddw.s16     q11, q11, d19         @

    vqmovn.s32    d12, q12              @
    vaddl.s16     q12, d10, d6          @
    vqmovn.s32    d13, q13              @ Q6 = y3
    vaddl.s16     q13, d11, d7          @
    vqmovn.s32    d18, q10              @
    vsubl.s16     q10, d10, d6          @
    vqmovn.s32    d19, q11              @ Q9 = y5
    vsubl.s16     q11, d11, d7          @

    vshr.s16      q3, q6, #0x2          @

    vsra.s16      q6, q9, #0x2          @ Q6 = z3

    vaddw.s16     q12, q12, d2          @
    vaddw.s16     q13, q13, d3          @

    vshr.s16      q1, #0x1              @

    vsub.s16      q5, q3, q9            @ Q5 = z5

    vsubw.s16     q10, q10, d14         @
    vsubw.s16     q11, q11, d15         @

    vshr.s16      q7, #0x1              @

    vaddw.s16     q12, q12, d2          @
    vaddw.s16     q13, q13, d3          @

    vsubw.s16     q10, q10, d14         @
    vsubw.s16     q11, q11, d15         @

    vqmovn.s32    d14, q12              @
    vadd.s16      q1, q8, q5            @ Q1 = x1
    vqmovn.s32    d15, q13              @ Q7 = y7
    vsub.s16      q3, q8, q5            @ Q3 = x6
    vqmovn.s32    d18, q10              @
    vsub.s16      q5, q2, q6            @ Q5 = x5
    vqmovn.s32    d19, q11              @ Q9 = y1
    vadd.s16      q2, q2, q6            @ Q2 = x2

    vshr.s16      q12, q9, #0x2         @
    vsra.s16      q9, q7, #0x2          @ Q9 = z1

    vsub.s16      q11, q7, q12          @ Q11 = z7

    vadd.s16      q6, q4, q9            @ Q6 = x3
    vsub.s16      q4, q4, q9            @ Q4 = x4

    vsub.s16      q7, q0, q11           @ Q7 = x7
    vadd.s16      q0, q0, q11           @ Q0 = x0

    vswp.s16      q3, q6                @ Q3 <-> Q6

    vrshr.s16     q1, q1, #6            @
    vld1.32       d16, [r1], r3         @ Q12 = 0x070605....0x070605....
    vrshr.s16     q2, q2, #6            @
    vrshr.s16     q4, q4, #6            @
    vld1.32       d17, [r1], r3         @ Q12 = 0x070605....0x070605....
    vrshr.s16     q5, q5, #6            @
    vrshr.s16     q7, q7, #6            @
    vld1.32       d18, [r1], r3         @ Q12 = 0x070605....0x070605....
    vrshr.s16     q0, q0, #6            @
    vrshr.s16     q3, q3, #6            @
    vld1.32       d19, [r1], r3         @ Q12 = 0x070605....0x070605....
    vrshr.s16     q6, q6, #6            @

@ Code Added to pack sign and magnitudes

    vaddw.u8      q0, q0, d28
    vaddw.u8      q1, q1, d29
    vaddw.u8      q2, q2, d30
    vaddw.u8      q3, q3, d31
    vqmovun.s16   d0, q0
    vaddw.u8      q4, q4, d16
    vqmovun.s16   d1, q1
    vaddw.u8      q5, q5, d17
    vqmovun.s16   d2, q2
    vaddw.u8      q6, q6, d18
    vqmovun.s16   d3, q3
    vaddw.u8      q7, q7, d19

    vqmovun.s16   d4, q4
    vst1.32       d0, [r2], r4          @ Magnitudes of 1st 4x4 block coeffs
    vqmovun.s16   d5, q5
    vst1.32       d1, [r2], r4          @ Magnitudes of 1st 4x4 block coeffs
    vqmovun.s16   d6, q6
    vst1.32       d2, [r2], r4          @ Magnitudes of 1st 4x4 block coeffs
    vqmovun.s16   d7, q7
    vst1.32       d3, [r2], r4          @ Magnitudes of 1st 4x4 block coeffs
    vst1.32       d4, [r2], r4          @ Magnitudes of 1st 4x4 block coeffs

    vst1.32       d5, [r2], r4          @ Magnitudes of 1st 4x4 block coeffs


    vst1.32       d6, [r2], r4          @ Magnitudes of 1st 4x4 block coeffs


    vst1.32       d7, [r2], r4          @ Magnitudes of 1st 4x4 block coeffs

idct_8x8_end:

    vpop          {d8-d15}
    ldmfd         sp!, {r4-r12, r15}

