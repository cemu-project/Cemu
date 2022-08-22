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
@*  ih264_default_weighted_pred_a9q.s
@*
@* @brief
@*  Contains function definitions for default weighted prediction.
@*
@* @author
@*  Kaushik Senthoor R
@*
@* @par List of Functions:
@*
@*  - ih264_default_weighted_pred_luma_a9q()
@*  - ih264_default_weighted_pred_chroma_a9q()
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@*
@*******************************************************************************
@* @function
@*  ih264_default_weighted_pred_luma_a9q()
@*
@* @brief
@*  This routine performs the default weighted prediction as described in sec
@* 8.4.2.3.1 titled "Default weighted sample prediction process" for luma.
@*
@* @par Description:
@*  This function gets two ht x wd blocks, calculates their rounded-average and
@* stores it in the destination block.
@*
@* @param[in] pu1_src1:
@*  UWORD8 Pointer to the buffer containing the first input block.
@*
@* @param[in] pu1_src2:
@*  UWORD8 Pointer to the buffer containing the second input block.
@*
@* @param[out] pu1_dst
@*  UWORD8 pointer to the destination where the output block is stored.
@*
@* @param[in] src_strd1
@*  Stride of the first input buffer
@*
@* @param[in] src_strd2
@*  Stride of the second input buffer
@*
@* @param[in] dst_strd
@*  Stride of the destination buffer
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
@void ih264_default_weighted_pred_luma_a9q(UWORD8 *pu1_src1,
@                                          UWORD8 *pu1_src2,
@                                          UWORD8 *pu1_dst,
@                                          WORD32 src_strd1,
@                                          WORD32 src_strd2,
@                                          WORD32 dst_strd,
@                                          WORD32 ht,
@                                          WORD32 wd)
@
@**************Variables Vs Registers*****************************************
@   r0      => pu1_src1
@   r1      => pu1_src2
@   r2      => pu1_dst
@   r3      => src_strd1
@   [sp]    => src_strd2 (r4)
@   [sp+4]  => dst_strd  (r5)
@   [sp+8]  => ht        (r6)
@   [sp+12] => wd        (r7)
@
.text
.p2align 2

    .global ih264_default_weighted_pred_luma_a9q

ih264_default_weighted_pred_luma_a9q:

    stmfd         sp!, {r4-r7, r14}     @stack stores the values of the arguments
    ldr           r7, [sp, #32]         @Load wd
    ldr           r4, [sp, #20]         @Load src_strd2
    ldr           r5, [sp, #24]         @Load dst_strd
    cmp           r7, #16
    ldr           r6, [sp, #28]         @Load ht
    vpush         {d8-d15}
    beq           loop_16               @branch if wd is 16
    cmp           r7, #8
    beq           loop_8                @branch if wd is 8

loop_4:                                 @each iteration processes four rows

    vld1.32       d0[0], [r0], r3       @load row 1 in source 1
    vld1.32       d0[1], [r0], r3       @load row 2 in source 1
    vld1.32       d2[0], [r1], r4       @load row 1 in source 2
    vld1.32       d2[1], [r1], r4       @load row 2 in source 2

    vld1.32       d1[0], [r0], r3       @load row 3 in source 1
    vld1.32       d1[1], [r0], r3       @load row 4 in source 1
    vrhadd.u8     d0, d0, d2
    vld1.32       d3[0], [r1], r4       @load row 3 in source 2
    vld1.32       d3[1], [r1], r4       @load row 4 in source 2

    subs          r6, r6, #4            @decrement ht by 4
    vst1.32       d0[0], [r2], r5       @load row 1 in destination
    vst1.32       d0[1], [r2], r5       @load row 2 in destination
    vrhadd.u8     d1, d1, d3
    vst1.32       d1[0], [r2], r5       @load row 3 in destination
    vst1.32       d1[1], [r2], r5       @load row 4 in destination

    bgt           loop_4                @if greater than 0 repeat the loop again

    b             end_loops

loop_8:                                 @each iteration processes four rows

    vld1.8        d0, [r0], r3          @load row 1 in source 1
    vld1.8        d4, [r1], r4          @load row 1 in source 2
    vld1.8        d1, [r0], r3          @load row 2 in source 1
    vld1.8        d5, [r1], r4          @load row 2 in source 2
    vld1.8        d2, [r0], r3          @load row 3 in source 1
    vrhadd.u8     q0, q0, q2
    vld1.8        d6, [r1], r4          @load row 3 in source 2
    vld1.8        d3, [r0], r3          @load row 4 in source 1
    vrhadd.u8     d2, d2, d6
    vld1.8        d7, [r1], r4          @load row 4 in source 2

    subs          r6, r6, #4            @decrement ht by 4
    vst1.8        d0, [r2], r5          @load row 1 in destination
    vrhadd.u8     d3, d3, d7
    vst1.8        d1, [r2], r5          @load row 2 in destination
    vst1.8        d2, [r2], r5          @load row 3 in destination
    vst1.8        d3, [r2], r5          @load row 4 in destination

    bgt           loop_8                @if greater than 0 repeat the loop again

    b             end_loops

loop_16:                                @each iteration processes eight rows

    vld1.8        {q0}, [r0], r3        @load row 1 in source 1
    vld1.8        {q8}, [r1], r4        @load row 1 in source 2
    vld1.8        {q1}, [r0], r3        @load row 2 in source 1
    vld1.8        {q9}, [r1], r4        @load row 2 in source 2
    vrhadd.u8     q0, q0, q8
    vld1.8        {q2}, [r0], r3        @load row 3 in source 1
    vld1.8        {q10}, [r1], r4       @load row 3 in source 2
    vrhadd.u8     q1, q1, q9
    vld1.8        {q3}, [r0], r3        @load row 4 in source 1
    vld1.8        {q11}, [r1], r4       @load row 4 in source 2
    vrhadd.u8     q2, q2, q10
    vld1.8        {q4}, [r0], r3        @load row 5 in source 1
    vld1.8        {q12}, [r1], r4       @load row 5 in source 2
    vrhadd.u8     q3, q3, q11
    vld1.8        {q5}, [r0], r3        @load row 6 in source 1
    vld1.8        {q13}, [r1], r4       @load row 6 in source 2
    vrhadd.u8     q4, q4, q12
    vld1.8        {q6}, [r0], r3        @load row 7 in source 1
    vld1.8        {q14}, [r1], r4       @load row 7 in source 2
    vrhadd.u8     q5, q5, q13
    vld1.8        {q7}, [r0], r3        @load row 8 in source 1
    vld1.8        {q15}, [r1], r4       @load row 8 in source 2

    vrhadd.u8     q6, q6, q14
    vst1.8        {q0}, [r2], r5        @load row 1 in destination
    vst1.8        {q1}, [r2], r5        @load row 2 in destination
    vrhadd.u8     q7, q7, q15
    vst1.8        {q2}, [r2], r5        @load row 3 in destination
    vst1.8        {q3}, [r2], r5        @load row 4 in destination
    subs          r6, r6, #8            @decrement ht by 8
    vst1.8        {q4}, [r2], r5        @load row 5 in destination
    vst1.8        {q5}, [r2], r5        @load row 6 in destination
    vst1.8        {q6}, [r2], r5        @load row 7 in destination
    vst1.8        {q7}, [r2], r5        @load row 8 in destination

    bgt           loop_16               @if greater than 0 repeat the loop again

end_loops:

    vpop          {d8-d15}
    ldmfd         sp!, {r4-r7, r15}     @Reload the registers from sp


@*******************************************************************************
@* @function
@*  ih264_default_weighted_pred_chroma_a9q()
@*
@* @brief
@*  This routine performs the default weighted prediction as described in sec
@* 8.4.2.3.1 titled "Default weighted sample prediction process" for chroma.
@*
@* @par Description:
@*  This function gets two ht x wd blocks, calculates their rounded-average and
@* stores it in the destination block for U and V.
@*
@* @param[in] pu1_src1:
@*  UWORD8 Pointer to the buffer containing the first input block.
@*
@* @param[in] pu1_src2:
@*  UWORD8 Pointer to the buffer containing the second input block.
@*
@* @param[out] pu1_dst
@*  UWORD8 pointer to the destination where the output block is stored.
@*
@* @param[in] src_strd1
@*  Stride of the first input buffer
@*
@* @param[in] src_strd2
@*  Stride of the second input buffer
@*
@* @param[in] dst_strd
@*  Stride of the destination buffer
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
@void ih264_default_weighted_pred_chroma_a9q(UWORD8 *pu1_src1,
@                                            UWORD8 *pu1_src2,
@                                            UWORD8 *pu1_dst,
@                                            WORD32 src_strd1,
@                                            WORD32 src_strd2,
@                                            WORD32 dst_strd,
@                                            WORD32 ht,
@                                            WORD32 wd)
@
@**************Variables Vs Registers*****************************************
@   r0      => pu1_src1
@   r1      => pu1_src2
@   r2      => pu1_dst
@   r3      => src_strd1
@   [sp]    => src_strd2 (r4)
@   [sp+4]  => dst_strd  (r5)
@   [sp+8]  => ht        (r6)
@   [sp+12] => wd        (r7)
@


    .global ih264_default_weighted_pred_chroma_a9q

ih264_default_weighted_pred_chroma_a9q:

    stmfd         sp!, {r4-r7, r14}     @stack stores the values of the arguments
    ldr           r7, [sp, #32]         @Load wd
    ldr           r4, [sp, #20]         @Load src_strd2
    ldr           r5, [sp, #24]         @Load dst_strd
    cmp           r7, #8
    ldr           r6, [sp, #28]         @Load ht
    vpush         {d8-d15}
    beq           loop_8_uv             @branch if wd is 8
    cmp           r7, #4
    beq           loop_4_uv             @branch if wd is 4

loop_2_uv:                              @each iteration processes two rows

    vld1.32       d0[0], [r0], r3       @load row 1 in source 1
    vld1.32       d0[1], [r0], r3       @load row 2 in source 1

    vld1.32       d1[0], [r1], r4       @load row 1 in source 2
    vld1.32       d1[1], [r1], r4       @load row 2 in source 2

    vrhadd.u8     d0, d0, d1

    subs          r6, r6, #2            @decrement ht by 2
    vst1.32       d0[0], [r2], r5       @load row 1 in destination
    vst1.32       d0[1], [r2], r5       @load row 2 in destination

    bgt           loop_2_uv             @if greater than 0 repeat the loop again

    b             end_loops_uv

loop_4_uv:                              @each iteration processes two rows

    vld1.8        d0, [r0], r3          @load row 1 in source 1
    vld1.8        d2, [r1], r4          @load row 1 in source 2
    vld1.8        d1, [r0], r3          @load row 2 in source 1
    vrhadd.u8     d0, d0, d2
    vld1.8        d3, [r1], r4          @load row 2 in source 2

    vrhadd.u8     d1, d1, d3
    vst1.8        d0, [r2], r5          @load row 1 in destination
    subs          r6, r6, #2            @decrement ht by 2
    vst1.8        d1, [r2], r5          @load row 2 in destination

    bgt           loop_4_uv             @if greater than 0 repeat the loop again

    b             end_loops_uv

loop_8_uv:                              @each iteration processes four rows

    vld1.8        {q0}, [r0], r3        @load row 1 in source 1
    vld1.8        {q4}, [r1], r4        @load row 1 in source 2
    vld1.8        {q1}, [r0], r3        @load row 2 in source 1
    vrhadd.u8     q0, q0, q4
    vld1.8        {q5}, [r1], r4        @load row 2 in source 2
    vld1.8        {q2}, [r0], r3        @load row 3 in source 1
    vrhadd.u8     q1, q1, q5
    vld1.8        {q6}, [r1], r4        @load row 3 in source 2
    vld1.8        {q3}, [r0], r3        @load row 4 in source 1
    vrhadd.u8     q2, q2, q6
    vld1.8        {q7}, [r1], r4        @load row 4 in source 2

    vst1.8        {q0}, [r2], r5        @load row 1 in destination
    vrhadd.u8     q3, q3, q7
    vst1.8        {q1}, [r2], r5        @load row 2 in destination
    subs          r6, r6, #4            @decrement ht by 4
    vst1.8        {q2}, [r2], r5        @load row 3 in destination
    vst1.8        {q3}, [r2], r5        @load row 4 in destination

    bgt           loop_8_uv             @if greater than 0 repeat the loop again

end_loops_uv:

    vpop          {d8-d15}
    ldmfd         sp!, {r4-r7, r15}     @Reload the registers from sp


