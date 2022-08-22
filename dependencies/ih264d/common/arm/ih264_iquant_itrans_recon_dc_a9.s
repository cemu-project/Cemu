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
@ *  ih264_iquant_itrans_recon_dc_a9.s
@ *
@ * @brief
@ *  Contains function definitions for single stage  inverse transform
@ *
@ * @author
@ *  Mohit
@ *
@ * @par List of Functions:
@ *  - ih264_iquant_itrans_recon_4x4_dc_a9()
@ *  - ih264_iquant_itrans_recon_8x8_dc_a9()
@ *  - ih264_iquant_itrans_recon_chroma_4x4_dc_a9()
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
@ *  for dc input pattern only, i.e. only the (0,0) element of the input 4x4 block is
@ *  non-zero. For complete function, refer ih264_iquant_itrans_recon_a9.s
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
@void ih264_iquant_itrans_recon_4x4_dc(WORD16 *pi2_src,
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
@r9 =>  iq_start_idx
@unused =>  pi2_dc_ld_addr

.text
.syntax unified
.p2align 2

    .global ih264_iquant_itrans_recon_4x4_dc_a9

ih264_iquant_itrans_recon_4x4_dc_a9:

@Only one shift is done in horizontal inverse because,
@if u4_qp_div_6 is lesser than 4 then shift value will be neagative and do negative left shift, in this case rnd_factor has value
@if u4_qp_div_6 is greater than 4 then shift value will be positive and do left shift, here rnd_factor is 0

    stmfd         sp!, {r4-r10, r14}    @stack stores the values of the arguments
    ldr           r5, [sp, #36]         @Loads *pu2_iscal_mat
    ldr           r6, [sp, #40]         @Loads *pu2_weigh_mat
    ldrsh         r8, [r0]              @load pi2_src[0], SH for signed halfword load
    ldrh          r6, [r6]              @load pu2_weight_mat[0] , H for unsigned halfword load
    ldrh          r5, [r5]              @load pu2_iscal_mat[0] , H for unsigned halfword load
@=======================DEQUANT FROM HERE===================================
    mul           r6, r6, r5            @pu2_iscal_mat[0]*pu2_weigh_mat[0]
    ldr           r7, [sp, #44]         @Loads u4_qp_div_6
    mul           r6, r6, r8            @pi2_src[0]*pu2_iscal_mat[0]*pu2_weigh_mat[0]
    ldr           r4, [sp, #32]         @Loads out_strd
    ldr           r9, [sp, #52]         @Loads iq_start_idx

    lsl           r6, r6, r7            @(pi2_src[0]*pu2_iscal_mat[0]*pu2_weigh_mat[0])<<u4_qp_div_6
    add           r6, r6, #8            @(pi2_src[0]*pu2_iscal_mat[0]*pu2_weigh_mat[0])<<u4_qp_div_6 + rnd_fact
    asr           r6, r6, #4            @q0 = (pi2_src[0]*pu2_iscal_mat[0]*pu2_weigh_mat[0] + rnd_fact)<<(u4_qp_div_6-4)

    subs          r9, r9, #1            @ if r8 == 1 => intra case , so result of subtraction is zero and Z flag is set
    ldrsheq       r10, [r0]             @ Loads signed halfword pi2_src[0], if r9==1
    moveq         r6, r10               @ Restore dc value in case of intra, i.e. r9 == 1

    add           r6, r6, #32           @i_macro = q0 + 32
    asr           r6, r6, #6            @i_macro >>6 = DC output of 2-stage transform
    vdup.s16      q0, r6                @copy transform output to Q0

    vld1.32       d30[0], [r1], r3      @I row Load pu1_pred buffer

    vld1.32       d30[1], [r1], r3      @II row Load pu1_pred buffer

    vld1.32       d31[0], [r1], r3      @III row Load pu1_pred buf

    vld1.32       d31[1], [r1], r3      @IV row Load pu1_pred buffer
    vaddw.u8      q10, q0, d30

    vaddw.u8      q11, q0, d31

    vqmovun.s16   d0, q10

    vst1.32       d0[0], [r2], r4       @I row store the value
    vqmovun.s16   d1, q11
    vst1.32       d0[1], [r2], r4       @II row store the value
    vst1.32       d1[0], [r2], r4       @III row store the value
    vst1.32       d1[1], [r2]           @IV row store the value

    ldmfd         sp!, {r4-r10, r15}    @Reload the registers from SP




@*
@ *******************************************************************************
@ *
@ * @brief
@ *  This function performs inverse quant and Inverse transform type Ci4 for 8*8 block
@ *  for dc input pattern only, i.e. only the (0,0) element of the input 8x8 block is
@ *  non-zero. For complete function, refer ih264_iquant_itrans_recon_a9.s
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
@void ih264_iquant_itrans_recon_8x8_dc(WORD16 *pi2_src,
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


    .global ih264_iquant_itrans_recon_8x8_dc_a9
ih264_iquant_itrans_recon_8x8_dc_a9:

    stmfd         sp!, {r4-r8, r14}     @stack stores the values of the arguments
    ldr           r5, [sp, #28]         @Loads *pu2_iscal_mat
    ldr           r6, [sp, #32]         @Loads *pu2_weigh_mat
    ldrsh         r8, [r0]              @load pi2_src[0], SH for signed halfword load
    ldrh          r6, [r6]              @load pu2_weight_mat[0] , H for unsigned halfword load
    ldrh          r5, [r5]              @load pu2_iscal_mat[0] , H for unsigned halfword load
@=======================DEQUANT FROM HERE===================================
    mul           r6, r6, r5            @pu2_iscal_mat[0]*pu2_weigh_mat[0]
    ldr           r7, [sp, #36]         @Loads u4_qp_div_6
    mul           r6, r6, r8            @pi2_src[0]*pu2_iscal_mat[0]*pu2_weigh_mat[0]
    ldr           r4, [sp, #24]         @Loads out_strd

    vpush         {d8-d15}
    lsl           r6, r6, r7            @(pi2_src[0]*pu2_iscal_mat[0]*pu2_weigh_mat[0])<<u4_qp_div_6
    add           r6, r6, #32           @(pi2_src[0]*pu2_iscal_mat[0]*pu2_weigh_mat[0])<<u4_qp_div_6 + rnd_fact
    asr           r6, r6, #6            @q0 = (pi2_src[0]*pu2_iscal_mat[0]*pu2_weigh_mat[0] + rnd_fact)<<(u4_qp_div_6-4)
    add           r6, r6, #32           @i_macro = q0 + 32
    asr           r6, r6, #6            @i_macro >>6 = DC output of 2-stage transform
    vdup.s16      q8, r6                @copy transform output to Q0

    vld1.32       d24, [r1], r3         @ Q12 = 0x070605....0x070605....

    vld1.32       d25, [r1], r3         @ Q12 = 0x070605....0x070605....

    vld1.32       d26, [r1], r3         @ Q12 = 0x070605....0x070605....
    vaddw.u8      q0, q8, d24
    vld1.32       d27, [r1], r3         @ Q12 = 0x070605....0x070605....
    vaddw.u8      q1, q8, d25
    vld1.32       d28, [r1], r3         @ Q12 = 0x070605....0x070605....
    vaddw.u8      q2, q8, d26
    vld1.32       d29, [r1], r3         @ Q12 = 0x070605....0x070605....
    vaddw.u8      q3, q8, d27
    vld1.32       d30, [r1], r3         @ Q12 = 0x070605....0x070605....
    vaddw.u8      q4, q8, d28
    vld1.32       d31, [r1], r3         @ Q12 = 0x070605....0x070605....

@ Code Added to pack sign and magnitudes


    vqmovun.s16   d0, q0
    vaddw.u8      q5, q8, d29
    vqmovun.s16   d1, q1
    vaddw.u8      q6, q8, d30
    vqmovun.s16   d2, q2
    vqmovun.s16   d3, q3
    vaddw.u8      q7, q8, d31
    vqmovun.s16   d4, q4
    vqmovun.s16   d5, q5
    vst1.32       d0, [r2], r4          @ Magnitudes of 1st 4x4 block coeffs
    vqmovun.s16   d6, q6
    vst1.32       d1, [r2], r4          @ Magnitudes of 1st 4x4 block coeffs
    vqmovun.s16   d7, q7
    vst1.32       d2, [r2], r4          @ Magnitudes of 1st 4x4 block coeffs
    vst1.32       d3, [r2], r4          @ Magnitudes of 1st 4x4 block coeffs
    vst1.32       d4, [r2], r4          @ Magnitudes of 1st 4x4 block coeffs
    vst1.32       d5, [r2], r4          @ Magnitudes of 1st 4x4 block coeffs
    vst1.32       d6, [r2], r4          @ Magnitudes of 1st 4x4 block coeffs
    vst1.32       d7, [r2], r4          @ Magnitudes of 1st 4x4 block coeffs

    vpop          {d8-d15}
    ldmfd         sp!, {r4-r8, r15}


@ *
@ ********************************************************************************
@ *
@ * @brief This function reconstructs a 4x4 sub block from quantized resiude and
@ * prediction buffer if only dc value is present for residue
@ *
@ * @par Description:
@ *  The quantized residue is first inverse quantized,
@ *  This inverse quantized content is added to the prediction buffer to recon-
@ *  struct the end output
@ *
@ * @param[in] pi2_src
@ *  quantized dc coeffiient
@ *
@ * @param[in] pu1_pred
@ *  prediction 4x4 block in interleaved format
@ *
@ * @param[in] pred_strd,
@ *  Prediction buffer stride in interleaved format
@ *
@ * @param[in] out_strd
@ *  recon buffer Stride
@ *
@ * @returns none
@ *
@ * @remarks none
@ *
@ *******************************************************************************
@ *
@ void ih264_iquant_itrans_recon_chroma_4x4_dc(WORD16 *pi2_src,
@                                             UWORD8 *pu1_pred,
@                                             UWORD8 *pu1_out,
@                                             WORD32 pred_strd,
@                                             WORD32 out_strd,
@                                             const UWORD16 *pu2_iscal_mat,
@                                             const UWORD16 *pu2_weigh_mat,
@                                             UWORD32 u4_qp_div_6,
@                                             WORD16 *pi2_tmp,
@                                             WORD16 *pi2_dc_src)
@ Register Usage
@ r0 : pi2_src
@ r1 : pu1_pred
@ r2 : pu1_out
@ r3 : pred_strd
@ Neon registers d0-d7, d16-d30 are used
@ No need for pushing  arm and neon registers
    .global ih264_iquant_itrans_recon_chroma_4x4_dc_a9
ih264_iquant_itrans_recon_chroma_4x4_dc_a9:

    ldr           r0, [sp, #20]
    vld1.s16      d0, [r0]              @load pi2_dc_src

    ldr           r0, [sp]              @load out_strd

    vld2.s8       {d2, d3}, [r1], r3    @load pred plane 1 => d2 &pred palne 2 => d3
    vld2.s8       {d3, d4}, [r1], r3
    vrshr.s16     d0, d0, #6            @i_macro = ((q0 + 32) >> 6);
    vld2.s8       {d4, d5}, [r1], r3
    vld2.s8       {d5, d6}, [r1], r3

    vdup.s16      q0, d0[0]             @duplicate pi2_sr[0]
    mov           r1, r2                @backup pu1_out

    vtrn.32       d2, d3                @mov the 4 coeffs of current block to d2
    vtrn.32       d4, d5

    vmov.u16      q15, #0x00ff


    vld1.u8       d18, [r2], r0         @load out [8 bit size) -8 coeffs
    vaddw.u8      q1, q0, d2            @Add pred
    vld1.u8       d19, [r2], r0
    vaddw.u8      q2, q0, d4
    vld1.u8       d20, [r2], r0
    vld1.u8       d21, [r2], r0

    vqmovun.s16   d2, q1
    vqmovun.s16   d4, q2

    vmovl.u8      q1, d2
    vmovl.u8      q2, d4

    vbit.u8       q9, q1, q15
    vbit.u8       q10, q2, q15

    vst1.u8       d18, [r1], r0         @store  out
    vst1.u8       d19, [r1], r0
    vst1.u8       d20, [r1], r0
    vst1.u8       d21, [r1], r0

    bx            lr







