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
@*  ih264_inter_pred_luma_horz_qpel_a9q.s
@*
@* @brief
@*  Contains function definitions for inter prediction horizontal quarter pel interpolation.
@*
@* @author
@*  Mohit
@*
@* @par List of Functions:
@*
@*  - ih264_inter_pred_luma_horz_qpel_a9q()
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@*

@* All the functions here are replicated from ih264_inter_pred_filters.c
@

@**
@**
@*******************************************************************************
@*
@* @brief
@*     Quarter pel interprediction luma filter for horizontal input
@*
@* @par Description:
@* Applies a 6 tap horizontal filter .The output is  clipped to 8 bits
@* sec 8.4.2.2.1 titled "Luma sample interpolation process"
@*
@* @param[in] pu1_src
@*  UWORD8 pointer to the source
@*
@* @param[out] pu1_dst
@*  UWORD8 pointer to the destination
@*
@* @param[in] src_strd
@*  integer source stride
@*
@* @param[in] dst_strd
@*  integer destination stride
@*
@* @param[in] ht
@*  integer height of the array
@*
@* @param[in] wd
@*  integer width of the array
@*
@ @param[in] pu1_tmp: temporary buffer: UNUSED in this function
@*
@* @param[in] dydx: x and y reference offset for qpel calculations.
@* @returns
@*
@ @remarks
@*  None
@*
@*******************************************************************************
@*

@void ih264_inter_pred_luma_horz (
@                            UWORD8 *pu1_src,
@                            UWORD8 *pu1_dst,
@                            WORD32 src_strd,
@                            WORD32 dst_strd,
@                            WORD32 ht,
@                            WORD32 wd,
@                            UWORD8* pu1_tmp,
@                            UWORD32 dydx)

@**************Variables Vs Registers*****************************************
@   r0 => *pu1_src
@   r1 => *pu1_dst
@   r2 =>  src_strd
@   r3 =>  dst_strd
@   r5 =>  ht
@   r6 =>  wd
@   r7 =>  dydx

.text
.p2align 2


    .global ih264_inter_pred_luma_horz_qpel_a9q

ih264_inter_pred_luma_horz_qpel_a9q:

    stmfd         sp!, {r4-r12, r14}    @store register values to stack
    vstmdb        sp!, {d8-d15}         @push neon registers to stack
    ldr           r5, [sp, #104]        @Loads ht
    ldr           r6, [sp, #108]        @Loads wd
    ldr           r7, [sp, #116]        @Loads dydx
    and           r7, r7, #3            @Finds x-offset
    add           r7, r0, r7, lsr #1    @pu1_src + (x_offset>>1)
    sub           r0, r0, #2            @pu1_src-2
    vmov.i8       d0, #5                @filter coeff
    subs          r12, r6, #8           @if wd=8 branch to loop_8
    vmov.i8       d1, #20               @filter coeff

    beq           loop_8

    subs          r12, r6, #4           @if wd=4 branch to loop_4
    beq           loop_4

loop_16:                                @when  wd=16
    @ Processing row0 and row1
    vld1.8        {d2, d3, d4}, [r0], r2 @// Load row0
    vext.8        d31, d2, d3, #5       @//extract a[5]                         (column1,row0)
    vld1.8        {d5, d6, d7}, [r0], r2 @// Load row1
    vext.8        d30, d3, d4, #5       @//extract a[5]                         (column2,row0)
    vaddl.u8      q4, d31, d2           @// a0 + a5                             (column1,row0)
    vext.8        d28, d5, d6, #5       @//extract a[5]                         (column1,row1)
    vaddl.u8      q5, d30, d3           @// a0 + a5                             (column2,row0)
    vext.8        d27, d6, d7, #5       @//extract a[5]                         (column2,row1)
    vaddl.u8      q7, d28, d5           @// a0 + a5                             (column1,row1)
    vext.8        d31, d2, d3, #2       @//extract a[2]                         (column1,row0)
    vaddl.u8      q8, d27, d6           @// a0 + a5                             (column2,row1)
    vext.8        d30, d3, d4, #2       @//extract a[2]                         (column2,row0)
    vmlal.u8      q4, d31, d1           @// a0 + a5 + 20a2                      (column1,row0)
    vext.8        d28, d5, d6, #2       @//extract a[2]                         (column1,row1)
    vmlal.u8      q5, d30, d1           @// a0 + a5 + 20a2                      (column2,row0)
    vext.8        d27, d6, d7, #2       @//extract a[2]                         (column2,row1)
    vmlal.u8      q7, d28, d1           @// a0 + a5 + 20a2                      (column1,row1)
    vext.8        d31, d2, d3, #3       @//extract a[3]                         (column1,row0)
    vmlal.u8      q8, d27, d1           @// a0 + a5 + 20a2                      (column2,row1)
    vext.8        d30, d3, d4, #3       @//extract a[3]                         (column2,row0)
    vmlal.u8      q4, d31, d1           @// a0 + a5 + 20a2 + 20a3               (column1,row0)
    vext.8        d28, d5, d6, #3       @//extract a[3]                         (column1,row1)
    vmlal.u8      q5, d30, d1           @// a0 + a5 + 20a2 + 20a3               (column2,row0)
    vext.8        d27, d6, d7, #3       @//extract a[3]                         (column2,row1)
    vmlal.u8      q7, d28, d1           @// a0 + a5 + 20a2 + 20a3               (column1,row1)
    vext.8        d31, d2, d3, #1       @//extract a[1]                         (column1,row0)
    vmlal.u8      q8, d27, d1           @// a0 + a5 + 20a2 + 20a3               (column2,row1)
    vext.8        d30, d3, d4, #1       @//extract a[1]                         (column2,row0)
    vmlsl.u8      q4, d31, d0           @// a0 + a5 + 20a2 + 20a3 - 5a1         (column1,row0)
    vext.8        d28, d5, d6, #1       @//extract a[1]                         (column1,row1)
    vmlsl.u8      q5, d30, d0           @// a0 + a5 + 20a2 + 20a3 - 5a1         (column2,row0)
    vext.8        d27, d6, d7, #1       @//extract a[1]                         (column2,row1)
    vmlsl.u8      q7, d28, d0           @// a0 + a5 + 20a2 + 20a3 - 5a1         (column1,row1)
    vext.8        d31, d2, d3, #4       @//extract a[4]                         (column1,row0)
    vmlsl.u8      q8, d27, d0           @// a0 + a5 + 20a2 + 20a3 - 5a1         (column2,row1)
    vext.8        d30, d3, d4, #4       @//extract a[4]                         (column2,row0)
    vmlsl.u8      q4, d31, d0           @// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4   (column1,row0)
    vext.8        d28, d5, d6, #4       @//extract a[4]                         (column1,row1)
    vmlsl.u8      q5, d30, d0           @// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4   (column2,row0)
    vext.8        d27, d6, d7, #4       @//extract a[4]                         (column2,row1)
    vmlsl.u8      q7, d28, d0           @// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4   (column1,row1)
    vmlsl.u8      q8, d27, d0           @// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4   (column2,row1)
    vld1.32       {d12, d13}, [r7], r2  @Load value for interpolation           (column1,row0)
    vqrshrun.s16  d20, q4, #5           @// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5   (column1,row0)
    vqrshrun.s16  d21, q5, #5           @// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5   (column2,row0)
    vext.8        d31, d2, d3, #5       @//extract a[5]                         (column1,row2)
    vrhadd.u8     q10, q6, q10          @Interpolation step for qpel calculation
    vqrshrun.s16  d18, q7, #5           @// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5   (column1,row1)
    vst1.8        {d20, d21}, [r1], r3  @//Store dest row0
    vext.8        d30, d3, d4, #5       @//extract a[5]                         (column2,row2)
    vqrshrun.s16  d19, q8, #5           @// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5   (column2,row1)
    vld1.32       {d12, d13}, [r7], r2  @Load value for interpolation           (column1,row1)
    vrhadd.u8     q9, q6, q9            @Interpolation step for qpel calculation
    vst1.8        {d18, d19}, [r1], r3  @//Store dest row1
    subs          r5, r5, #2            @ 2 rows done, decrement by 2

    beq           end_func
    b             loop_16

loop_8:
@ Processing row0 and row1

    vld1.8        {d5, d6}, [r0], r2    @// Load row1
    vext.8        d28, d5, d6, #5       @//extract a[5]                         (column1,row1)
    vld1.8        {d2, d3}, [r0], r2    @// Load row0
    vext.8        d25, d5, d6, #2       @//extract a[2]                         (column1,row1)
    vext.8        d31, d2, d3, #5       @//extract a[5]                         (column1,row0)
    vext.8        d24, d5, d6, #3       @//extract a[3]                         (column1,row1)
    vext.8        d23, d5, d6, #1       @//extract a[1]                         (column1,row1)
    vext.8        d22, d5, d6, #4       @//extract a[4]                         (column1,row1)
    vaddl.u8      q7, d28, d5           @// a0 + a5                             (column1,row1)
    vext.8        d29, d2, d3, #3       @//extract a[3]                         (column1,row0)
    vmlal.u8      q7, d25, d1           @// a0 + a5 + 20a2                      (column1,row1)
    vmlal.u8      q7, d24, d1           @// a0 + a5 + 20a2 + 20a3               (column1,row1)
    vmlsl.u8      q7, d23, d0           @// a0 + a5 + 20a2 + 20a3 - 5a1         (column1,row1)
    vmlsl.u8      q7, d22, d0           @// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4   (column1,row1)
    vext.8        d30, d2, d3, #2       @//extract a[2]                         (column1,row0)
    vaddl.u8      q4, d31, d2           @// a0 + a5                             (column1,row0)
    vext.8        d27, d2, d3, #1       @//extract a[1]                         (column1,row0)
    vext.8        d26, d2, d3, #4       @//extract a[4]                         (column1,row0)
    vmlal.u8      q4, d29, d1           @// a0 + a5 + 20a2 + 20a3               (column1,row0)
    vmlal.u8      q4, d30, d1           @// a0 + a5 + 20a2                      (column1,row0)
    vmlsl.u8      q4, d27, d0           @// a0 + a5 + 20a2 + 20a3 - 5a1         (column1,row0)
    vmlsl.u8      q4, d26, d0           @// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4   (column1,row0)
    vqrshrun.s16  d18, q7, #5           @// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5   (column1,row0)
    vld1.32       d12, [r7], r2         @Load value for interpolation           (column1,row0)
    vld1.32       d13, [r7], r2         @Load value for interpolation           (column1,row1)
    vqrshrun.s16  d19, q4, #5           @// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5   (column1,row1)
    vrhadd.u8     q9, q6, q9            @Interpolation step for qpel calculation
    vst1.8        {d18}, [r1], r3       @//Store dest row0
    vst1.8        {d19}, [r1], r3       @//Store dest row1
    subs          r5, r5, #2            @ 2 rows done, decrement by 2

    beq           end_func              @ Branch if height==4
    b             loop_8                @looping if height == 8 or 16

loop_4:
    vld1.8        {d5, d6}, [r0], r2    @// Load row1
    vext.8        d28, d5, d6, #5       @//extract a[5]                         (column1,row1)
    vld1.8        {d2, d3}, [r0], r2    @// Load row0
    vext.8        d25, d5, d6, #2       @//extract a[2]                         (column1,row1)
    vext.8        d31, d2, d3, #5       @//extract a[5]                         (column1,row0)
    vaddl.u8      q7, d28, d5           @// a0 + a5                             (column1,row1)
    vext.8        d24, d5, d6, #3       @//extract a[3]                         (column1,row1)
    vext.8        d23, d5, d6, #1       @//extract a[1]                         (column1,row1)
    vext.8        d22, d5, d6, #4       @//extract a[4]                         (column1,row1)
    vext.8        d29, d2, d3, #3       @//extract a[3]                         (column1,row0)
    vmlal.u8      q7, d25, d1           @// a0 + a5 + 20a2                      (column1,row1)
    vmlal.u8      q7, d24, d1           @// a0 + a5 + 20a2 + 20a3               (column1,row1)
    vmlsl.u8      q7, d23, d0           @// a0 + a5 + 20a2 + 20a3 - 5a1         (column1,row1)
    vmlsl.u8      q7, d22, d0           @// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4   (column1,row1)
    vaddl.u8      q4, d31, d2           @// a0 + a5                             (column1,row0)
    vext.8        d30, d2, d3, #2       @//extract a[2]                         (column1,row0)
    vld1.32       d12, [r7], r2         @Load value for interpolation           (column1,row0)
    vld1.32       d13, [r7], r2         @Load value for interpolation           (column1,row1)
    vext.8        d27, d2, d3, #1       @//extract a[1]                         (column1,row0)
    vext.8        d26, d2, d3, #4       @//extract a[4]                         (column1,row0)
    vmlal.u8      q4, d29, d1           @// a0 + a5 + 20a2 + 20a3               (column1,row0)
    vmlal.u8      q4, d30, d1           @// a0 + a5 + 20a2                      (column1,row0)
    vmlsl.u8      q4, d27, d0           @// a0 + a5 + 20a2 + 20a3 - 5a1         (column1,row0)
    vmlsl.u8      q4, d26, d0           @// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4   (column1,row0)
    vqrshrun.s16  d18, q7, #5           @// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5   (column1,row0)
    vqrshrun.s16  d19, q4, #5           @// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5   (column1,row1)
    vrhadd.u8     q9, q6, q9            @Interpolation step for qpel calculation
    vst1.32       d18[0], [r1], r3      @//Store dest row0
    vst1.32       d19[0], [r1], r3      @//Store dest row1

    subs          r5, r5, #2            @ 2 rows done, decrement by 2
    beq           end_func

    b             loop_4

end_func:
    vldmia        sp!, {d8-d15}         @ Restore neon registers that were saved
    ldmfd         sp!, {r4-r12, pc}     @Restoring registers from stack


