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
@******************************************************************************
@* @file
@*  ih264_weighted_bi_pred_a9q.s
@*
@* @brief
@*  Contains function definitions for weighted biprediction.
@*
@* @author
@*  Kaushik Senthoor R
@*
@* @par List of Functions:
@*
@*  - ih264_weighted_bi_pred_luma_a9q()
@*  - ih264_weighted_bi_pred_chroma_a9q()
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@*
@*******************************************************************************
@* @function
@*  ih264_weighted_bi_pred_luma_a9q()
@*
@* @brief
@*  This routine performs the weighted biprediction as described in sec
@* 8.4.2.3.2 titled "Weighted sample prediction process" for luma.
@*
@* @par Description:
@*  This function gets two ht x wd blocks, calculates the weighted samples,
@* rounds off, adds offset and stores it in the destination block.
@*
@* @param[in] pu1_src1
@*  UWORD8 Pointer to the buffer containing the input block 1.
@*
@* @param[in] pu1_src2
@*  UWORD8 Pointer to the buffer containing the input block 2.
@*
@* @param[out] pu1_dst
@*  UWORD8 pointer to the destination where the output block is stored.
@*
@* @param[in] src_strd1
@*  Stride of the input buffer 1
@*
@* @param[in] src_strd2
@*  Stride of the input buffer 2
@*
@* @param[in] dst_strd
@*  Stride of the destination buffer
@*
@* @param[in] log_wd
@*  number of bits to be rounded off
@*
@* @param[in] wt1
@*  weight for the weighted prediction
@*
@* @param[in] wt2
@*  weight for the weighted prediction
@*
@* @param[in] ofst1
@*  offset 1 used after rounding off
@*
@* @param[in] ofst2
@*  offset 2 used after rounding off
@*
@* @param[in] ht
@*  integer height of the array
@*
@* @param[in] wd
@*  integer width of the array
@*
@* @returns
@*  None
@*
@* @remarks
@*  (ht,wd) can be (4,4), (4,8), (8,4), (8,8), (8,16), (16,8) or (16,16).
@*
@*******************************************************************************
@*
@void ih264_weighted_bi_pred_luma_a9q(UWORD8 *pu1_src1,
@                                     UWORD8 *pu1_src2,
@                                     UWORD8 *pu1_dst,
@                                     WORD32 src_strd1,
@                                     WORD32 src_strd2,
@                                     WORD32 dst_strd,
@                                     WORD32 log_wd,
@                                     WORD32 wt1,
@                                     WORD32 wt2,
@                                     WORD32 ofst1,
@                                     WORD32 ofst2,
@                                     WORD32 ht,
@                                     WORD32 wd)
@
@**************Variables Vs Registers*****************************************
@   r0      => pu1_src1
@   r1      => pu1_src2
@   r2      => pu1_dst
@   r3      => src_strd1
@   [sp]    => src_strd2 (r4)
@   [sp+4]  => dst_strd  (r5)
@   [sp+8]  => log_wd    (r6)
@   [sp+12] => wt1       (r7)
@   [sp+16] => wt2       (r8)
@   [sp+20] => ofst1     (r9)
@   [sp+24] => ofst2     (r10)
@   [sp+28] => ht        (r11)
@   [sp+32] => wd        (r12)
@
.text
.p2align 2

    .global ih264_weighted_bi_pred_luma_a9q

ih264_weighted_bi_pred_luma_a9q:

    stmfd         sp!, {r4-r12, r14}    @stack stores the values of the arguments
    ldr           r6, [sp, #48]         @Load log_wd in r6
    ldr           r7, [sp, #52]         @Load wt1 in r7
    ldr           r8, [sp, #56]         @Load wt2 in r8
    ldr           r9, [sp, #60]         @Load ofst1 in r9

    add           r6, r6, #1            @r6  = log_wd + 1
    sxtb          r7, r7                @sign-extend 16-bit wt1 to 32-bit
    ldr           r4, [sp, #40]         @Load src_strd2 in r4
    ldr           r5, [sp, #44]         @Load dst_strd in r5
    sxtb          r9, r9                @sign-extend 8-bit ofst1 to 32-bit
    neg           r10, r6               @r10 = -(log_wd + 1)
    ldr           r11, [sp, #68]        @Load ht in r11
    ldr           r12, [sp, #72]        @Load wd in r12
    vdup.16       q0, r10               @Q0  = -(log_wd + 1) (32-bit)
    add           r9, r9, #1            @r9 = ofst1 + 1

    ldr           r10, [sp, #64]        @Load ofst2 in r10
    sxtb          r8, r8                @sign-extend 16-bit wt2 to 32-bit
    cmp           r12, #16              @check if wd is 16
    vpush         {d8-d15}
    sxtb          r10, r10              @sign-extend 8-bit ofst2 to 32-bit
    add           r9, r9, r10           @r9 = ofst1 + ofst2 + 1
    vmov          d2, r7, r8            @D2 = {wt1(32-bit), wt2(32-bit)}
    asr           r9, r9, #1            @r9 = ofst = (ofst1 + ofst2 + 1) >> 1
    vdup.8        d3, r9                @D3 = ofst (8-bit)
    beq           loop_16               @branch if wd is 16

    cmp           r12, #8               @check if wd is 8
    beq           loop_8                @branch if wd is 8

loop_4:                                 @each iteration processes four rows

    vld1.32       d4[0], [r0], r3       @load row 1 in source 1
    vld1.32       d4[1], [r0], r3       @load row 2 in source 1
    vld1.32       d6[0], [r1], r4       @load row 1 in source 2
    vld1.32       d6[1], [r1], r4       @load row 2 in source 2

    vmovl.u8      q2, d4                @converting rows 1,2 in source 1 to 16-bit
    vld1.32       d8[0], [r0], r3       @load row 3 in source 1
    vld1.32       d8[1], [r0], r3       @load row 4 in source 1
    vmovl.u8      q3, d6                @converting rows 1,2 in source 2 to 16-bit
    vld1.32       d10[0], [r1], r4      @load row 3 in source 2
    vld1.32       d10[1], [r1], r4      @load row 4 in source 2

    vmovl.u8      q4, d8                @converting rows 3,4 in source 1 to 16-bit
    vmovl.u8      q5, d10               @converting rows 3,4 in source 2 to 16-bit

    vmul.s16      q2, q2, d2[0]         @weight 1 mult. for rows 1,2
    vmla.s16      q2, q3, d2[2]         @weight 2 mult. for rows 1,2
    vmul.s16      q4, q4, d2[0]         @weight 1 mult. for rows 3,4
    vmla.s16      q4, q5, d2[2]         @weight 2 mult. for rows 3,4

    subs          r11, r11, #4          @decrement ht by 4
    vrshl.s16     q2, q2, q0            @rounds off the weighted samples from rows 1,2
    vrshl.s16     q4, q4, q0            @rounds off the weighted samples from rows 3,4

    vaddw.s8      q2, q2, d3            @adding offset for rows 1,2
    vaddw.s8      q4, q4, d3            @adding offset for rows 3,4

    vqmovun.s16   d4, q2                @saturating rows 1,2 to unsigned 8-bit
    vqmovun.s16   d8, q4                @saturating rows 3,4 to unsigned 8-bit

    vst1.32       d4[0], [r2], r5       @store row 1 in destination
    vst1.32       d4[1], [r2], r5       @store row 2 in destination
    vst1.32       d8[0], [r2], r5       @store row 3 in destination
    vst1.32       d8[1], [r2], r5       @store row 4 in destination

    bgt           loop_4                @if greater than 0 repeat the loop again

    b             end_loops

loop_8:                                 @each iteration processes four rows

    vld1.8        d4, [r0], r3          @load row 1 in source 1
    vld1.8        d6, [r1], r4          @load row 1 in source 2
    vld1.8        d8, [r0], r3          @load row 2 in source 1
    vld1.8        d10, [r1], r4         @load row 2 in source 2
    vmovl.u8      q2, d4                @converting row 1 in source 1 to 16-bit
    vld1.8        d12, [r0], r3         @load row 3 in source 1
    vld1.8        d14, [r1], r4         @load row 3 in source 2
    vmovl.u8      q3, d6                @converting row 1 in source 2 to 16-bit
    vld1.8        d16, [r0], r3         @load row 4 in source 1
    vld1.8        d18, [r1], r4         @load row 4 in source 2

    vmovl.u8      q4, d8                @converting row 2 in source 1 to 16-bit
    vmovl.u8      q5, d10               @converting row 2 in source 2 to 16-bit

    vmul.s16      q2, q2, d2[0]         @weight 1 mult. for row 1
    vmla.s16      q2, q3, d2[2]         @weight 2 mult. for row 1
    vmovl.u8      q6, d12               @converting row 3 in source 1 to 16-bit
    vmovl.u8      q7, d14               @converting row 3 in source 2 to 16-bit
    vmul.s16      q4, q4, d2[0]         @weight 1 mult. for row 2
    vmla.s16      q4, q5, d2[2]         @weight 2 mult. for row 2
    vmovl.u8      q8, d16               @converting row 4 in source 1 to 16-bit
    vmovl.u8      q9, d18               @converting row 4 in source 2 to 16-bit

    vmul.s16      q6, q6, d2[0]         @weight 1 mult. for row 3
    vmla.s16      q6, q7, d2[2]         @weight 2 mult. for row 3
    vmul.s16      q8, q8, d2[0]         @weight 1 mult. for row 4
    vmla.s16      q8, q9, d2[2]         @weight 2 mult. for row 4

    vrshl.s16     q2, q2, q0            @rounds off the weighted samples from row 1
    vrshl.s16     q4, q4, q0            @rounds off the weighted samples from row 2
    vrshl.s16     q6, q6, q0            @rounds off the weighted samples from row 3
    vaddw.s8      q2, q2, d3            @adding offset for row 1
    vrshl.s16     q8, q8, q0            @rounds off the weighted samples from row 4
    vaddw.s8      q4, q4, d3            @adding offset for row 2

    vaddw.s8      q6, q6, d3            @adding offset for row 3
    vqmovun.s16   d4, q2                @saturating row 1 to unsigned 8-bit
    vaddw.s8      q8, q8, d3            @adding offset for row 4
    vqmovun.s16   d8, q4                @saturating row 2 to unsigned 8-bit

    vqmovun.s16   d12, q6               @saturating row 3 to unsigned 8-bit
    vqmovun.s16   d16, q8               @saturating row 4 to unsigned 8-bit

    vst1.8        d4, [r2], r5          @store row 1 in destination
    vst1.8        d8, [r2], r5          @store row 2 in destination
    subs          r11, r11, #4          @decrement ht by 4
    vst1.8        d12, [r2], r5         @store row 3 in destination
    vst1.8        d16, [r2], r5         @store row 4 in destination

    bgt           loop_8                @if greater than 0 repeat the loop again

    b             end_loops

loop_16:                                @each iteration processes two rows

    vld1.8        {q2}, [r0], r3        @load row 1 in source 1
    vld1.8        {q3}, [r1], r4        @load row 1 in source 2
    vld1.8        {q4}, [r0], r3        @load row 2 in source 1
    vld1.8        {q5}, [r1], r4        @load row 2 in source 2
    vmovl.u8      q10, d4               @converting row 1L in source 1 to 16-bit
    vld1.8        {q6}, [r0], r3        @load row 3 in source 1
    vld1.8        {q7}, [r1], r4        @load row 3 in source 2
    vmovl.u8      q11, d6               @converting row 1L in source 2 to 16-bit
    vld1.8        {q8}, [r0], r3        @load row 4 in source 1
    vld1.8        {q9}, [r1], r4        @load row 4 in source 2

    vmovl.u8      q2, d5                @converting row 1H in source 1 to 16-bit
    vmovl.u8      q3, d7                @converting row 1H in source 2 to 16-bit

    vmul.s16      q10, q10, d2[0]       @weight 1 mult. for row 1L
    vmla.s16      q10, q11, d2[2]       @weight 2 mult. for row 1L
    vmovl.u8      q12, d8               @converting row 2L in source 1 to 16-bit
    vmovl.u8      q13, d10              @converting row 2L in source 2 to 16-bit

    vmul.s16      q2, q2, d2[0]         @weight 1 mult. for row 1H
    vmla.s16      q2, q3, d2[2]         @weight 2 mult. for row 1H
    vmovl.u8      q4, d9                @converting row 2H in source 1 to 16-bit
    vmovl.u8      q5, d11               @converting row 2H in source 2 to 16-bit

    vmul.s16      q12, q12, d2[0]       @weight 1 mult. for row 2L
    vmla.s16      q12, q13, d2[2]       @weight 2 mult. for row 2L
    vmovl.u8      q14, d12              @converting row 3L in source 1 to 16-bit
    vmovl.u8      q15, d14              @converting row 3L in source 2 to 16-bit

    vmul.s16      q4, q4, d2[0]         @weight 1 mult. for row 2H
    vmla.s16      q4, q5, d2[2]         @weight 2 mult. for row 2H
    vmovl.u8      q6, d13               @converting row 3H in source 1 to 16-bit
    vmovl.u8      q7, d15               @converting row 3H in source 2 to 16-bit

    vmul.s16      q14, q14, d2[0]       @weight 1 mult. for row 3L
    vmla.s16      q14, q15, d2[2]       @weight 2 mult. for row 3L
    vmovl.u8      q11, d16              @converting row 4L in source 1 to 16-bit
    vmovl.u8      q3, d18               @converting row 4L in source 2 to 16-bit

    vmul.s16      q6, q6, d2[0]         @weight 1 mult. for row 3H
    vmla.s16      q6, q7, d2[2]         @weight 2 mult. for row 3H
    vmovl.u8      q8, d17               @converting row 4H in source 1 to 16-bit
    vmovl.u8      q9, d19               @converting row 4H in source 2 to 16-bit

    vmul.s16      q11, q11, d2[0]       @weight 1 mult. for row 4L
    vmla.s16      q11, q3, d2[2]        @weight 2 mult. for row 4L
    vrshl.s16     q10, q10, q0          @rounds off the weighted samples from row 1L

    vmul.s16      q8, q8, d2[0]         @weight 1 mult. for row 4H
    vmla.s16      q8, q9, d2[2]         @weight 2 mult. for row 4H
    vrshl.s16     q2, q2, q0            @rounds off the weighted samples from row 1H

    vrshl.s16     q12, q12, q0          @rounds off the weighted samples from row 2L
    vaddw.s8      q10, q10, d3          @adding offset for row 1L
    vrshl.s16     q4, q4, q0            @rounds off the weighted samples from row 2H
    vaddw.s8      q2, q2, d3            @adding offset for row 1H
    vrshl.s16     q14, q14, q0          @rounds off the weighted samples from row 3L
    vaddw.s8      q12, q12, d3          @adding offset for row 2L
    vrshl.s16     q6, q6, q0            @rounds off the weighted samples from row 3H
    vaddw.s8      q4, q4, d3            @adding offset for row 2H
    vrshl.s16     q11, q11, q0          @rounds off the weighted samples from row 4L
    vaddw.s8      q14, q14, d3          @adding offset for row 3L
    vrshl.s16     q8, q8, q0            @rounds off the weighted samples from row 4H
    vaddw.s8      q6, q6, d3            @adding offset for row 3H

    vqmovun.s16   d26, q10              @saturating row 1L to unsigned 8-bit
    vaddw.s8      q11, q11, d3          @adding offset for row 4L
    vqmovun.s16   d27, q2               @saturating row 1H to unsigned 8-bit
    vaddw.s8      q8, q8, d3            @adding offset for row 4H

    vqmovun.s16   d10, q12              @saturating row 2L to unsigned 8-bit
    vqmovun.s16   d11, q4               @saturating row 2H to unsigned 8-bit
    vqmovun.s16   d30, q14              @saturating row 3L to unsigned 8-bit
    vqmovun.s16   d31, q6               @saturating row 3H to unsigned 8-bit
    vst1.8        {q13}, [r2], r5       @store row 1 in destination
    vqmovun.s16   d14, q11              @saturating row 4L to unsigned 8-bit
    vqmovun.s16   d15, q8               @saturating row 4H to unsigned 8-bit

    vst1.8        {q5}, [r2], r5        @store row 2 in destination
    subs          r11, r11, #4          @decrement ht by 4
    vst1.8        {q15}, [r2], r5       @store row 3 in destination
    vst1.8        {q7}, [r2], r5        @store row 4 in destination

    bgt           loop_16               @if greater than 0 repeat the loop again

end_loops:

    vpop          {d8-d15}
    ldmfd         sp!, {r4-r12, r15}    @Reload the registers from sp


@*******************************************************************************
@* @function
@*  ih264_weighted_bi_pred_chroma_a9q()
@*
@* @brief
@*  This routine performs the default weighted prediction as described in sec
@* 8.4.2.3.2 titled "Weighted sample prediction process" for chroma.
@*
@* @par Description:
@*  This function gets two ht x wd blocks, calculates the weighted samples,
@* rounds off, adds offset and stores it in the destination block for U and V.
@*
@* @param[in] pu1_src1
@*  UWORD8 Pointer to the buffer containing the input block 1.
@*
@* @param[in] pu1_src2
@*  UWORD8 Pointer to the buffer containing the input block 2.
@*
@* @param[out] pu1_dst
@*  UWORD8 pointer to the destination where the output block is stored.
@*
@* @param[in] src_strd1
@*  Stride of the input buffer 1
@*
@* @param[in] src_strd2
@*  Stride of the input buffer 2
@*
@* @param[in] dst_strd
@*  Stride of the destination buffer
@*
@* @param[in] log_wd
@*  number of bits to be rounded off
@*
@* @param[in] wt1
@*  weights for the weighted prediction in U and V
@*
@* @param[in] wt2
@*  weights for the weighted prediction in U and V
@*
@* @param[in] ofst1
@*  offset 1 used after rounding off for U an dV
@*
@* @param[in] ofst2
@*  offset 2 used after rounding off for U and V
@*
@* @param[in] ht
@*  integer height of the array
@*
@* @param[in] wd
@*  integer width of the array
@*
@* @returns
@*  None
@*
@* @remarks
@*  (ht,wd) can be (2,2), (2,4), (4,2), (4,4), (4,8), (8,4) or (8,8).
@*
@*******************************************************************************
@*
@void ih264_weighted_bi_pred_chroma_a9q(UWORD8 *pu1_src1,
@                                       UWORD8 *pu1_src2,
@                                       UWORD8 *pu1_dst,
@                                       WORD32 src_strd1,
@                                       WORD32 src_strd2,
@                                       WORD32 dst_strd,
@                                       WORD32 log_wd,
@                                       WORD32 wt1,
@                                       WORD32 wt2,
@                                       WORD32 ofst1,
@                                       WORD32 ofst2,
@                                       WORD32 ht,
@                                       WORD32 wd)
@
@**************Variables Vs Registers*****************************************
@   r0      => pu1_src1
@   r1      => pu1_src2
@   r2      => pu1_dst
@   r3      => src_strd1
@   [sp]    => src_strd2 (r4)
@   [sp+4]  => dst_strd  (r5)
@   [sp+8]  => log_wd    (r6)
@   [sp+12] => wt1       (r7)
@   [sp+16] => wt2       (r8)
@   [sp+20] => ofst1     (r9)
@   [sp+24] => ofst2     (r10)
@   [sp+28] => ht        (r11)
@   [sp+32] => wd        (r12)
@


    .global ih264_weighted_bi_pred_chroma_a9q

ih264_weighted_bi_pred_chroma_a9q:

    stmfd         sp!, {r4-r12, r14}    @stack stores the values of the arguments

    ldr           r6, [sp, #48]         @Load log_wd in r6
    ldr           r7, [sp, #52]         @Load wt1 in r7
    ldr           r8, [sp, #56]         @Load wt2 in r8
    add           r6, r6, #1            @r6  = log_wd + 1
    ldr           r9, [sp, #60]         @Load ofst1 in r9
    ldr           r10, [sp, #64]        @Load ofst2 in r10

    neg           r12, r6               @r12 = -(log_wd + 1)
    ldr           r4, [sp, #40]         @Load src_strd2 in r4
    ldr           r5, [sp, #44]         @Load dst_strd in r5
    vdup.16       q0, r12               @Q0  = -(log_wd + 1) (16-bit)

    ldr           r11, [sp, #68]        @Load ht in r11
    vdup.32       q1, r7                @Q1 = (wt1_u, wt1_v) (32-bit)
    ldr           r12, [sp, #72]        @Load wd in r12
    vdup.32       q2, r8                @Q2 = (wt2_u, wt2_v) (32-bit)
    asr           r7, r9, #8            @r7 = ofst1_v
    asr           r8, r10, #8           @r8 = ofst2_v
    vpush         {d8-d15}
    sxtb          r9, r9                @sign-extend 8-bit ofst1_u to 32-bit
    sxtb          r10, r10              @sign-extend 8-bit ofst2_u to 32-bit
    sxtb          r7, r7                @sign-extend 8-bit ofst1_v to 32-bit
    sxtb          r8, r8                @sign-extend 8-bit ofst2_v to 32-bit

    add           r9, r9, #1            @r9 = ofst1_u + 1
    add           r7, r7, #1            @r7 = ofst1_v + 1
    add           r9, r9, r10           @r9 = ofst1_u + ofst2_u + 1
    add           r7, r7, r8            @r7 = ofst1_v + ofst2_v + 1
    asr           r9, r9, #1            @r9 = ofst_u = (ofst1_u + ofst2_u + 1) >> 1
    asr           r7, r7, #1            @r7 = ofst_v = (ofst1_v + ofst2_v + 1) >> 1
    cmp           r12, #8               @check if wd is 8
    pkhbt         r9, r9, r7, lsl #16   @r9 = {ofst_u(16-bit), ofst_v(16-bit)}
    vdup.32       q3, r9                @Q3 = {ofst_u(16-bit), ofst_v(16-bit)}
    beq           loop_8_uv             @branch if wd is 8

    cmp           r12, #4               @check if wd is 4
    beq           loop_4_uv             @branch if wd is 4

loop_2_uv:                              @each iteration processes two rows

    vld1.32       d8[0], [r0], r3       @load row 1 in source 1
    vld1.32       d8[1], [r0], r3       @load row 2 in source 1
    vld1.32       d10[0], [r1], r4      @load row 1 in source 2
    vld1.32       d10[1], [r1], r4      @load row 2 in source 2

    vmovl.u8      q4, d8                @converting rows 1,2 in source 1 to 16-bit
    vmovl.u8      q5, d10               @converting rows 1,2 in source 2 to 16-bit

    vmul.s16      q4, q4, q1            @weight 1 mult. for rows 1,2
    vmla.s16      q4, q5, q2            @weight 2 mult. for rows 1,2

    vrshl.s16     q4, q4, q0            @rounds off the weighted samples from rows 1,2

    vadd.s16      q4, q4, q3            @adding offset for rows 1,2

    vqmovun.s16   d8, q4                @saturating rows 1,2 to unsigned 8-bit

    vst1.32       d8[0], [r2], r5       @store row 1 in destination
    vst1.32       d8[1], [r2], r5       @store row 2 in destination

    subs          r11, r11, #2          @decrement ht by 2
    bgt           loop_2_uv             @if greater than 0 repeat the loop again

    b             end_loops_uv

loop_4_uv:                              @each iteration processes two rows

    vld1.8        d8, [r0], r3          @load row 1 in source 1
    vld1.8        d10, [r1], r4         @load row 1 in source 2
    vmovl.u8      q4, d8                @converting row 1 in source 1 to 16-bit
    vld1.8        d12, [r0], r3         @load row 2 in source 1
    vmovl.u8      q5, d10               @converting row 1 in source 2 to 16-bit
    vld1.8        d14, [r1], r4         @load row 2 in source 2

    vmovl.u8      q6, d12               @converting row 2 in source 1 to 16-bit
    vmul.s16      q4, q4, q1            @weight 1 mult. for row 1
    vmla.s16      q4, q5, q2            @weight 2 mult. for row 1
    vmovl.u8      q7, d14               @converting row 2 in source 2 to 16-bit

    vmul.s16      q6, q6, q1            @weight 1 mult. for row 2
    vmla.s16      q6, q7, q2            @weight 2 mult. for row 2

    subs          r11, r11, #2          @decrement ht by 2
    vrshl.s16     q4, q4, q0            @rounds off the weighted samples from row 1
    vrshl.s16     q6, q6, q0            @rounds off the weighted samples from row 2
    vadd.s16      q4, q4, q3            @adding offset for row 1
    vadd.s16      q6, q6, q3            @adding offset for row 2

    vqmovun.s16   d8, q4                @saturating row 1 to unsigned 8-bit
    vqmovun.s16   d12, q6               @saturating row 2 to unsigned 8-bit

    vst1.8        d8, [r2], r5          @store row 1 in destination
    vst1.8        d12, [r2], r5         @store row 2 in destination

    bgt           loop_4_uv             @if greater than 0 repeat the loop again

    b             end_loops_uv

loop_8_uv:                              @each iteration processes two rows

    vld1.8        {q4}, [r0], r3        @load row 1 in source 1
    vld1.8        {q5}, [r1], r4        @load row 1 in source 2
    vld1.8        {q6}, [r0], r3        @load row 2 in source 1
    vld1.8        {q7}, [r1], r4        @load row 2 in source 2
    vmovl.u8      q12, d8               @converting row 1L in source 1 to 16-bit
    vld1.8        {q8}, [r0], r3        @load row 3 in source 1
    vld1.8        {q9}, [r1], r4        @load row 3 in source 2
    vmovl.u8      q13, d10              @converting row 1L in source 2 to 16-bit
    vld1.8        {q10}, [r0], r3       @load row 4 in source 1
    vld1.8        {q11}, [r1], r4       @load row 4 in source 2

    vmovl.u8      q4, d9                @converting row 1H in source 1 to 16-bit
    vmovl.u8      q5, d11               @converting row 1H in source 2 to 16-bit

    vmul.s16      q12, q12, q1          @weight 1 mult. for row 1L
    vmla.s16      q12, q13, q2          @weight 2 mult. for row 1L
    vmovl.u8      q14, d12              @converting row 2L in source 1 to 16-bit
    vmovl.u8      q15, d14              @converting row 2L in source 2 to 16-bit

    vmul.s16      q4, q4, q1            @weight 1 mult. for row 1H
    vmla.s16      q4, q5, q2            @weight 2 mult. for row 1H
    vmovl.u8      q6, d13               @converting row 2H in source 1 to 16-bit
    vmovl.u8      q7, d15               @converting row 2H in source 2 to 16-bit

    vmul.s16      q14, q14, q1          @weight 1 mult. for row 2L
    vmla.s16      q14, q15, q2          @weight 2 mult. for row 2L
    vmovl.u8      q13, d16              @converting row 3L in source 1 to 16-bit
    vmovl.u8      q5, d18               @converting row 3L in source 2 to 16-bit

    vmul.s16      q6, q6, q1            @weight 1 mult. for row 2H
    vmla.s16      q6, q7, q2            @weight 2 mult. for row 2H
    vmovl.u8      q8, d17               @converting row 3H in source 1 to 16-bit
    vmovl.u8      q9, d19               @converting row 3H in source 2 to 16-bit

    vmul.s16      q13, q13, q1          @weight 1 mult. for row 3L
    vmla.s16      q13, q5, q2           @weight 2 mult. for row 3L
    vmovl.u8      q15, d20              @converting row 4L in source 1 to 16-bit
    vmovl.u8      q7, d22               @converting row 4L in source 2 to 16-bit

    vmul.s16      q8, q8, q1            @weight 1 mult. for row 3H
    vmla.s16      q8, q9, q2            @weight 2 mult. for row 3H
    vmovl.u8      q10, d21              @converting row 4H in source 1 to 16-bit
    vmovl.u8      q11, d23              @converting row 4H in source 2 to 16-bit

    vmul.s16      q15, q15, q1          @weight 1 mult. for row 4L
    vmla.s16      q15, q7, q2           @weight 2 mult. for row 4L
    vrshl.s16     q12, q12, q0          @rounds off the weighted samples from row 1L

    vmul.s16      q10, q10, q1          @weight 1 mult. for row 4H
    vmla.s16      q10, q11, q2          @weight 2 mult. for row 4H
    vrshl.s16     q4, q4, q0            @rounds off the weighted samples from row 1H

    vrshl.s16     q14, q14, q0          @rounds off the weighted samples from row 2L
    vadd.s16      q12, q12, q3          @adding offset for row 1L
    vrshl.s16     q6, q6, q0            @rounds off the weighted samples from row 2H
    vadd.s16      q4, q4, q3            @adding offset for row 1H
    vrshl.s16     q13, q13, q0          @rounds off the weighted samples from row 3L
    vadd.s16      q14, q14, q3          @adding offset for row 2L
    vrshl.s16     q8, q8, q0            @rounds off the weighted samples from row 3H
    vadd.s16      q6, q6, q3            @adding offset for row 2H
    vrshl.s16     q15, q15, q0          @rounds off the weighted samples from row 4L
    vadd.s16      q13, q13, q3          @adding offset for row 3L
    vrshl.s16     q10, q10, q0          @rounds off the weighted samples from row 4H
    vadd.s16      q8, q8, q3            @adding offset for row 3H

    vqmovun.s16   d10, q12              @saturating row 1L to unsigned 8-bit
    vadd.s16      q15, q15, q3          @adding offset for row 4L
    vqmovun.s16   d11, q4               @saturating row 1H to unsigned 8-bit
    vadd.s16      q10, q10, q3          @adding offset for row 4H

    vqmovun.s16   d18, q14              @saturating row 2L to unsigned 8-bit
    vqmovun.s16   d19, q6               @saturating row 2H to unsigned 8-bit
    vqmovun.s16   d14, q13              @saturating row 3L to unsigned 8-bit
    vqmovun.s16   d15, q8               @saturating row 3H to unsigned 8-bit
    vst1.8        {q5}, [r2], r5        @store row 1 in destination
    vqmovun.s16   d22, q15              @saturating row 4L to unsigned 8-bit
    vqmovun.s16   d23, q10              @saturating row 4H to unsigned 8-bit

    vst1.8        {q9}, [r2], r5        @store row 2 in destination
    subs          r11, r11, #4          @decrement ht by 4
    vst1.8        {q7}, [r2], r5        @store row 3 in destination
    vst1.8        {q11}, [r2], r5       @store row 4 in destination

    bgt           loop_8_uv             @if greater than 0 repeat the loop again

end_loops_uv:

    vpop          {d8-d15}
    ldmfd         sp!, {r4-r12, r15}    @Reload the registers from sp


