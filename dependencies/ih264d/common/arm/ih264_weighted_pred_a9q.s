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
@*  ih264_weighted_pred_a9q.s
@*
@* @brief
@*  Contains function definitions for weighted prediction.
@*
@* @author
@*  Kaushik Senthoor R
@*
@* @par List of Functions:
@*
@*  - ih264_weighted_pred_luma_a9q()
@*  - ih264_weighted_pred_chroma_a9q()
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@*
@*******************************************************************************
@* @function
@*  ih264_weighted_pred_luma_a9q()
@*
@* @brief
@*  This routine performs the default weighted prediction as described in sec
@* 8.4.2.3.2 titled "Weighted sample prediction process" for luma.
@*
@* @par Description:
@*  This function gets a ht x wd block, calculates the weighted sample, rounds
@* off, adds offset and stores it in the destination block.
@*
@* @param[in] pu1_src:
@*  UWORD8 Pointer to the buffer containing the input block.
@*
@* @param[out] pu1_dst
@*  UWORD8 pointer to the destination where the output block is stored.
@*
@* @param[in] src_strd
@*  Stride of the input buffer
@*
@* @param[in] dst_strd
@*  Stride of the destination buffer
@*
@* @param[in] log_wd
@*  number of bits to be rounded off
@*
@* @param[in] wt
@*  weight for the weighted prediction
@*
@* @param[in] ofst
@*  offset used after rounding off
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
@void ih264_weighted_pred_luma_a9q(UWORD8 *pu1_src,
@                                  UWORD8 *pu1_dst,
@                                  WORD32 src_strd,
@                                  WORD32 dst_strd,
@                                  WORD32 log_wd,
@                                  WORD32 wt,
@                                  WORD32 ofst,
@                                  WORD32 ht,
@                                  WORD32 wd)
@
@**************Variables Vs Registers*****************************************
@   r0      => pu1_src
@   r1      => pu1_dst
@   r2      => src_strd
@   r3      => dst_strd
@   [sp]    => log_wd (r4)
@   [sp+4]  => wt     (r5)
@   [sp+8]  => ofst   (r6)
@   [sp+12] => ht     (r7)
@   [sp+16] => wd     (r8)
@
.text
.p2align 2

    .global ih264_weighted_pred_luma_a9q

ih264_weighted_pred_luma_a9q:

    stmfd         sp!, {r4-r9, r14}     @stack stores the values of the arguments
    ldr           r5, [sp, #32]         @Load wt
    ldr           r4, [sp, #28]         @Load log_wd in r4
    ldr           r6, [sp, #36]         @Load ofst
    ldr           r7, [sp, #40]         @Load ht
    ldr           r8, [sp, #44]         @Load wd
    vpush         {d8-d15}

    vdup.16       d2, r5                @D2 = wt (16-bit)
    neg           r9, r4                @r9 = -log_wd
    vdup.8        d3, r6                @D3 = ofst (8-bit)
    cmp           r8, #16               @check if wd is 16
    vdup.16       q0, r9                @Q0 = -log_wd (16-bit)
    beq           loop_16               @branch if wd is 16

    cmp           r8, #8                @check if wd is 8
    beq           loop_8                @branch if wd is 8

loop_4:                                 @each iteration processes four rows

    vld1.32       d4[0], [r0], r2       @load row 1 in source
    vld1.32       d4[1], [r0], r2       @load row 2 in source
    vld1.32       d6[0], [r0], r2       @load row 3 in source
    vld1.32       d6[1], [r0], r2       @load row 4 in source

    vmovl.u8      q2, d4                @converting rows 1,2 to 16-bit
    vmovl.u8      q3, d6                @converting rows 3,4 to 16-bit

    vmul.s16      q2, q2, d2[0]         @weight mult. for rows 1,2
    vmul.s16      q3, q3, d2[0]         @weight mult. for rows 3,4

    subs          r7, r7, #4            @decrement ht by 4
    vrshl.s16     q2, q2, q0            @rounds off the weighted samples from rows 1,2
    vrshl.s16     q3, q3, q0            @rounds off the weighted samples from rows 3,4

    vaddw.s8      q2, q2, d3            @adding offset for rows 1,2
    vaddw.s8      q3, q3, d3            @adding offset for rows 3,4

    vqmovun.s16   d4, q2                @saturating rows 1,2 to unsigned 8-bit
    vqmovun.s16   d6, q3                @saturating rows 3,4 to unsigned 8-bit

    vst1.32       d4[0], [r1], r3       @store row 1 in destination
    vst1.32       d4[1], [r1], r3       @store row 2 in destination
    vst1.32       d6[0], [r1], r3       @store row 3 in destination
    vst1.32       d6[1], [r1], r3       @store row 4 in destination

    bgt           loop_4                @if greater than 0 repeat the loop again

    b             end_loops

loop_8:                                 @each iteration processes four rows

    vld1.8        d4, [r0], r2          @load row 1 in source
    vld1.8        d6, [r0], r2          @load row 2 in source
    vld1.8        d8, [r0], r2          @load row 3 in source
    vmovl.u8      q2, d4                @converting row 1 to 16-bit
    vld1.8        d10, [r0], r2         @load row 4 in source
    vmovl.u8      q3, d6                @converting row 2 to 16-bit

    vmovl.u8      q4, d8                @converting row 3 to 16-bit
    vmul.s16      q2, q2, d2[0]         @weight mult. for row 1
    vmovl.u8      q5, d10               @converting row 4 to 16-bit
    vmul.s16      q3, q3, d2[0]         @weight mult. for row 2
    vmul.s16      q4, q4, d2[0]         @weight mult. for row 3
    vmul.s16      q5, q5, d2[0]         @weight mult. for row 4

    vrshl.s16     q2, q2, q0            @rounds off the weighted samples from row 1
    vrshl.s16     q3, q3, q0            @rounds off the weighted samples from row 2
    vrshl.s16     q4, q4, q0            @rounds off the weighted samples from row 3
    vaddw.s8      q2, q2, d3            @adding offset for row 1
    vrshl.s16     q5, q5, q0            @rounds off the weighted samples from row 4
    vaddw.s8      q3, q3, d3            @adding offset for row 2

    vaddw.s8      q4, q4, d3            @adding offset for row 3
    vqmovun.s16   d4, q2                @saturating row 1 to unsigned 8-bit
    vaddw.s8      q5, q5, d3            @adding offset for row 4
    vqmovun.s16   d6, q3                @saturating row 2 to unsigned 8-bit
    vqmovun.s16   d8, q4                @saturating row 3 to unsigned 8-bit
    vqmovun.s16   d10, q5               @saturating row 4 to unsigned 8-bit

    vst1.8        d4, [r1], r3          @store row 1 in destination
    vst1.8        d6, [r1], r3          @store row 2 in destination
    subs          r7, r7, #4            @decrement ht by 4
    vst1.8        d8, [r1], r3          @store row 3 in destination
    vst1.8        d10, [r1], r3         @store row 4 in destination

    bgt           loop_8                @if greater than 0 repeat the loop again

    b             end_loops

loop_16:                                @each iteration processes two rows

    vld1.8        {q2}, [r0], r2        @load row 1 in source
    vld1.8        {q3}, [r0], r2        @load row 2 in source
    vmovl.u8      q6, d4                @converting row 1L to 16-bit
    vld1.8        {q4}, [r0], r2        @load row 3 in source
    vmovl.u8      q7, d5                @converting row 1H to 16-bit
    vld1.8        {q5}, [r0], r2        @load row 4 in source

    vmovl.u8      q8, d6                @converting row 2L to 16-bit
    vmul.s16      q6, q6, d2[0]         @weight mult. for row 1L
    vmovl.u8      q9, d7                @converting row 2H to 16-bit
    vmul.s16      q7, q7, d2[0]         @weight mult. for row 1H
    vmovl.u8      q10, d8               @converting row 3L to 16-bit
    vmul.s16      q8, q8, d2[0]         @weight mult. for row 2L
    vmovl.u8      q11, d9               @converting row 3H to 16-bit
    vmul.s16      q9, q9, d2[0]         @weight mult. for row 2H
    vmovl.u8      q12, d10              @converting row 4L to 16-bit
    vmul.s16      q10, q10, d2[0]       @weight mult. for row 3L
    vmovl.u8      q13, d11              @converting row 4H to 16-bit
    vmul.s16      q11, q11, d2[0]       @weight mult. for row 3H

    vmul.s16      q12, q12, d2[0]       @weight mult. for row 4L
    vrshl.s16     q6, q6, q0            @rounds off the weighted samples from row 1L
    vmul.s16      q13, q13, d2[0]       @weight mult. for row 4H

    vrshl.s16     q7, q7, q0            @rounds off the weighted samples from row 1H
    vrshl.s16     q8, q8, q0            @rounds off the weighted samples from row 2L
    vaddw.s8      q6, q6, d3            @adding offset for row 1L
    vrshl.s16     q9, q9, q0            @rounds off the weighted samples from row 2H
    vaddw.s8      q7, q7, d3            @adding offset for row 1H
    vqmovun.s16   d4, q6                @saturating row 1L to unsigned 8-bit
    vrshl.s16     q10, q10, q0          @rounds off the weighted samples from row 3L
    vaddw.s8      q8, q8, d3            @adding offset for row 2L
    vqmovun.s16   d5, q7                @saturating row 1H to unsigned 8-bit
    vrshl.s16     q11, q11, q0          @rounds off the weighted samples from row 3H
    vaddw.s8      q9, q9, d3            @adding offset for row 2H
    vqmovun.s16   d6, q8                @saturating row 2L to unsigned 8-bit
    vrshl.s16     q12, q12, q0          @rounds off the weighted samples from row 4L
    vaddw.s8      q10, q10, d3          @adding offset for row 3L
    vqmovun.s16   d7, q9                @saturating row 2H to unsigned 8-bit
    vrshl.s16     q13, q13, q0          @rounds off the weighted samples from row 4H
    vaddw.s8      q11, q11, d3          @adding offset for row 3H

    vqmovun.s16   d8, q10               @saturating row 3L to unsigned 8-bit
    vaddw.s8      q12, q12, d3          @adding offset for row 4L
    vqmovun.s16   d9, q11               @saturating row 3H to unsigned 8-bit
    vaddw.s8      q13, q13, d3          @adding offset for row 4H

    vqmovun.s16   d10, q12              @saturating row 4L to unsigned 8-bit
    vst1.8        {q2}, [r1], r3        @store row 1 in destination
    vqmovun.s16   d11, q13              @saturating row 4H to unsigned 8-bit
    vst1.8        {q3}, [r1], r3        @store row 2 in destination
    subs          r7, r7, #4            @decrement ht by 4
    vst1.8        {q4}, [r1], r3        @store row 3 in destination
    vst1.8        {q5}, [r1], r3        @store row 4 in destination

    bgt           loop_16               @if greater than 0 repeat the loop again

end_loops:

    vpop          {d8-d15}
    ldmfd         sp!, {r4-r9, r15}     @Reload the registers from sp


@*******************************************************************************
@* @function
@*  ih264_weighted_pred_chroma_a9q()
@*
@* @brief
@*  This routine performs the default weighted prediction as described in sec
@* 8.4.2.3.2 titled "Weighted sample prediction process" for chroma.
@*
@* @par Description:
@*  This function gets a ht x wd block, calculates the weighted sample, rounds
@* off, adds offset and stores it in the destination block for U and V.
@*
@* @param[in] pu1_src:
@*  UWORD8 Pointer to the buffer containing the input block.
@*
@* @param[out] pu1_dst
@*  UWORD8 pointer to the destination where the output block is stored.
@*
@* @param[in] src_strd
@*  Stride of the input buffer
@*
@* @param[in] dst_strd
@*  Stride of the destination buffer
@*
@* @param[in] log_wd
@*  number of bits to be rounded off
@*
@* @param[in] wt
@*  weights for the weighted prediction for U and V
@*
@* @param[in] ofst
@*  offsets used after rounding off for U and V
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
@void ih264_weighted_pred_chroma_a9q(UWORD8 *pu1_src,
@                                    UWORD8 *pu1_dst,
@                                    WORD32 src_strd,
@                                    WORD32 dst_strd,
@                                    WORD32 log_wd,
@                                    WORD32 wt,
@                                    WORD32 ofst,
@                                    WORD32 ht,
@                                    WORD32 wd)
@
@**************Variables Vs Registers*****************************************
@   r0      => pu1_src
@   r1      => pu1_dst
@   r2      => src_strd
@   r3      => dst_strd
@   [sp]    => log_wd (r4)
@   [sp+4]  => wt     (r5)
@   [sp+8]  => ofst   (r6)
@   [sp+12] => ht     (r7)
@   [sp+16] => wd     (r8)
@


    .global ih264_weighted_pred_chroma_a9q

ih264_weighted_pred_chroma_a9q:

    stmfd         sp!, {r4-r9, r14}     @stack stores the values of the arguments

    ldr           r4, [sp, #28]         @Load log_wd in r4
    ldr           r5, [sp, #32]         @Load wt = {wt_u (16-bit), wt_v (16-bit)}
    ldr           r6, [sp, #36]         @Load ofst = {ofst_u (8-bit), ofst_v (8-bit)}
    ldr           r8, [sp, #44]         @Load wd

    neg           r9, r4                @r9 = -log_wd
    vdup.32       q1, r5                @Q1 = {wt_u (16-bit), wt_v (16-bit)}
    ldr           r7, [sp, #40]         @Load ht
    vpush         {d8-d15}
    vdup.16       d4, r6                @D4 = {ofst_u (8-bit), ofst_v (8-bit)}
    cmp           r8, #8                @check if wd is 8
    vdup.16       q0, r9                @Q0 = -log_wd (16-bit)
    beq           loop_8_uv             @branch if wd is 8

    cmp           r8, #4                @check if ws is 4
    beq           loop_4_uv             @branch if wd is 4

loop_2_uv:                              @each iteration processes two rows

    vld1.32       d6[0], [r0], r2       @load row 1 in source
    vld1.32       d6[1], [r0], r2       @load row 2 in source

    vmovl.u8      q3, d6                @converting rows 1,2 to 16-bit

    vmul.s16      q3, q3, q1            @weight mult. for rows 1,2

    vrshl.s16     q3, q3, q0            @rounds off the weighted samples from rows 1,2

    vaddw.s8      q3, q3, d4            @adding offset for rows 1,2

    vqmovun.s16   d6, q3                @saturating rows 1,2 to unsigned 8-bit

    subs          r7, r7, #2            @decrement ht by 2
    vst1.32       d6[0], [r1], r3       @store row 1 in destination
    vst1.32       d6[1], [r1], r3       @store row 2 in destination

    bgt           loop_2_uv             @if greater than 0 repeat the loop again

    b             end_loops_uv

loop_4_uv:                              @each iteration processes two rows

    vld1.8        d6, [r0], r2          @load row 1 in source
    vld1.8        d8, [r0], r2          @load row 2 in source

    vmovl.u8      q3, d6                @converting row 1 to 16-bit
    vmovl.u8      q4, d8                @converting row 2 to 16-bit

    vmul.s16      q3, q3, q1            @weight mult. for row 1
    vmul.s16      q4, q4, q1            @weight mult. for row 2

    subs          r7, r7, #2            @decrement ht by 2
    vrshl.s16     q3, q3, q0            @rounds off the weighted samples from row 1
    vrshl.s16     q4, q4, q0            @rounds off the weighted samples from row 2

    vaddw.s8      q3, q3, d4            @adding offset for row 1
    vaddw.s8      q4, q4, d4            @adding offset for row 2

    vqmovun.s16   d6, q3                @saturating row 1 to unsigned 8-bit
    vqmovun.s16   d8, q4                @saturating row 2 to unsigned 8-bit

    vst1.8        d6, [r1], r3          @store row 1 in destination
    vst1.8        d8, [r1], r3          @store row 2 in destination

    bgt           loop_4_uv             @if greater than 0 repeat the loop again

    b             end_loops_uv

loop_8_uv:                              @each iteration processes two rows

    vld1.8        {q3}, [r0], r2        @load row 1 in source
    vld1.8        {q4}, [r0], r2        @load row 2 in source
    vmovl.u8      q7, d6                @converting row 1L to 16-bit
    vld1.8        {q5}, [r0], r2        @load row 3 in source
    vmovl.u8      q8, d7                @converting row 1H to 16-bit
    vld1.8        {q6}, [r0], r2        @load row 4 in source

    vmul.s16      q7, q7, q1            @weight mult. for row 1L
    vmovl.u8      q9, d8                @converting row 2L to 16-bit
    vmul.s16      q8, q8, q1            @weight mult. for row 1H
    vmovl.u8      q10, d9               @converting row 2H to 16-bit
    vmul.s16      q9, q9, q1            @weight mult. for row 2L
    vmovl.u8      q11, d10              @converting row 3L to 16-bit
    vmul.s16      q10, q10, q1          @weight mult. for row 2H
    vmovl.u8      q12, d11              @converting row 3H to 16-bit
    vmul.s16      q11, q11, q1          @weight mult. for row 3L
    vmovl.u8      q13, d12              @converting row 4L to 16-bit
    vmul.s16      q12, q12, q1          @weight mult. for row 3H
    vmovl.u8      q14, d13              @converting row 4H to 16-bit

    vmul.s16      q13, q13, q1          @weight mult. for row 4L
    vrshl.s16     q7, q7, q0            @rounds off the weighted samples from row 1L
    vmul.s16      q14, q14, q1          @weight mult. for row 4H

    vrshl.s16     q8, q8, q0            @rounds off the weighted samples from row 1H
    vrshl.s16     q9, q9, q0            @rounds off the weighted samples from row 2L
    vaddw.s8      q7, q7, d4            @adding offset for row 1L
    vrshl.s16     q10, q10, q0          @rounds off the weighted samples from row 2H
    vaddw.s8      q8, q8, d4            @adding offset for row 1H
    vqmovun.s16   d6, q7                @saturating row 1L to unsigned 8-bit
    vrshl.s16     q11, q11, q0          @rounds off the weighted samples from row 3L
    vaddw.s8      q9, q9, d4            @adding offset for row 2L
    vqmovun.s16   d7, q8                @saturating row 1H to unsigned 8-bit
    vrshl.s16     q12, q12, q0          @rounds off the weighted samples from row 3H
    vaddw.s8      q10, q10, d4          @adding offset for row 2H
    vqmovun.s16   d8, q9                @saturating row 2L to unsigned 8-bit
    vrshl.s16     q13, q13, q0          @rounds off the weighted samples from row 4L
    vaddw.s8      q11, q11, d4          @adding offset for row 3L
    vqmovun.s16   d9, q10               @saturating row 2H to unsigned 8-bit
    vrshl.s16     q14, q14, q0          @rounds off the weighted samples from row 4H
    vaddw.s8      q12, q12, d4          @adding offset for row 3H

    vqmovun.s16   d10, q11              @saturating row 3L to unsigned 8-bit
    vaddw.s8      q13, q13, d4          @adding offset for row 4L
    vqmovun.s16   d11, q12              @saturating row 3H to unsigned 8-bit
    vaddw.s8      q14, q14, d4          @adding offset for row 4H

    vqmovun.s16   d12, q13              @saturating row 4L to unsigned 8-bit
    vst1.8        {q3}, [r1], r3        @store row 1 in destination
    vqmovun.s16   d13, q14              @saturating row 4H to unsigned 8-bit
    vst1.8        {q4}, [r1], r3        @store row 2 in destination
    subs          r7, r7, #4            @decrement ht by 4
    vst1.8        {q5}, [r1], r3        @store row 3 in destination
    vst1.8        {q6}, [r1], r3        @store row 4 in destination

    bgt           loop_8_uv             @if greater than 0 repeat the loop again

end_loops_uv:

    vpop          {d8-d15}
    ldmfd         sp!, {r4-r9, r15}     @Reload the registers from sp


