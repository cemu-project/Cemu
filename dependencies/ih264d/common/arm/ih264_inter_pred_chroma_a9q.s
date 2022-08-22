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
@*  ih264_inter_pred_chroma_a9q.s
@*
@* @brief
@*  Contains function definitions for inter prediction  interpolation.
@*
@* @author
@*  Ittaim
@*
@* @par List of Functions:
@*
@*  - ih264_inter_pred_chroma_a9q()
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
@**
@
@**
@*******************************************************************************
@*
@* @brief
@*    Interprediction chroma filter
@*
@* @par Description:
@*   Applies filtering to chroma samples as mentioned in
@*    sec 8.4.2.2.2 titled "chroma sample interpolation process"
@*
@* @param[in] pu1_src
@*  UWORD8 pointer to the source containing alternate U and V samples
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
@* @param[in]uc_dx
@*  dx value where the sample is to be produced(refer sec 8.4.2.2.2 )
@*
@* @param[in] uc_dy
@*  dy value where the sample is to be produced(refer sec 8.4.2.2.2 )
@*
@* @param[in] ht
@*  integer height of the array
@*
@* @param[in] wd
@*  integer width of the array
@*
@* @returns
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@*

@void ih264_inter_pred_chroma(UWORD8 *pu1_src,
@                             UWORD8 *pu1_dst,
@                             WORD32 src_strd,
@                             WORD32 dst_strd,
@                             WORD32 u1_dx,
@                             WORD32 u1_dy,
@                             WORD32 ht,
@                             WORD32 wd)
@**************Variables Vs Registers*****************************************
@   r0 => *pu1_src
@   r1 => *pu1_dst
@   r2 =>  src_strd
@   r3 =>  dst_strd
@   r4 =>  u1_dx
@   r5 =>  u1_dy
@   r6 =>  height
@   r7 => width
@
.text
.p2align 2

    .global ih264_inter_pred_chroma_a9q

ih264_inter_pred_chroma_a9q:

    stmfd         sp!, {r4-r12, r14}    @store register values to stack
    vstmdb        sp!, {d8-d15}         @push neon registers to stack
    ldr           r4, [sp, #104]
    ldr           r5, [sp, #108]
    ldr           r6, [sp, #112]
    ldr           r7, [sp, #116]

    rsb           r8, r4, #8            @8-u1_dx
    rsb           r9, r5, #8            @8-u1_dy
    mul           r10, r8, r9
    mul           r11, r4, r9

    vdup.u8       d28, r10
    vdup.u8       d29, r11

    mul           r10, r8, r5
    mul           r11, r4, r5

    vdup.u8       d30, r10
    vdup.u8       d31, r11

    subs          r12, r7, #2           @if wd=4 branch to loop_4
    beq           loop_2
    subs          r12, r7, #4           @if wd=8 branch to loop_8
    beq           loop_4

loop_8:
    sub           r6, #1
    vld1.8        {d0, d1, d2}, [r0], r2 @ Load row0
    vld1.8        {d5, d6, d7}, [r0], r2 @ Load row1
    vext.8        d3, d0, d1, #2
    vext.8        d8, d5, d6, #2

    vmull.u8      q5, d0, d28
    vmlal.u8      q5, d5, d30
    vmlal.u8      q5, d3, d29
    vmlal.u8      q5, d8, d31
    vext.8        d9, d6, d7, #2
    vext.8        d4, d1, d2, #2

inner_loop_8:
    vmull.u8      q6, d6, d30
    vmlal.u8      q6, d1, d28
    vmlal.u8      q6, d9, d31
    vmlal.u8      q6, d4, d29
    vmov          d0, d5
    vmov          d3, d8

    vqrshrun.s16  d14, q5, #6
    vmov          d1, d6
    vmov          d4, d9

    vld1.8        {d5, d6, d7}, [r0], r2 @ Load row1
    vqrshrun.s16  d15, q6, #6

    vext.8        d8, d5, d6, #2
    subs          r6, #1
    vext.8        d9, d6, d7, #2
    vst1.8        {q7}, [r1], r3        @ Store dest row

    vmull.u8      q5, d0, d28
    vmlal.u8      q5, d5, d30
    vmlal.u8      q5, d3, d29
    vmlal.u8      q5, d8, d31
    bne           inner_loop_8

    vmull.u8      q6, d6, d30
    vmlal.u8      q6, d1, d28
    vmlal.u8      q6, d9, d31
    vmlal.u8      q6, d4, d29

    vqrshrun.s16  d14, q5, #6
    vqrshrun.s16  d15, q6, #6

    vst1.8        {q7}, [r1], r3        @ Store dest row

    b             end_func

loop_4:
    sub           r6, #1
    vld1.8        {d0, d1}, [r0], r2    @ Load row0
    vld1.8        {d2, d3}, [r0], r2    @ Load row1
    vext.8        d1, d0, d1, #2
    vext.8        d3, d2, d3, #2

    vmull.u8      q2, d2, d30
    vmlal.u8      q2, d0, d28
    vmlal.u8      q2, d3, d31
    vmlal.u8      q2, d1, d29

inner_loop_4:
    subs          r6, #1
    vmov          d0, d2
    vmov          d1, d3

    vld1.8        {d2, d3}, [r0], r2    @ Load row1
    vqrshrun.s16  d6, q2, #6

    vext.8        d3, d2, d3, #2
    vst1.8        {d6}, [r1], r3        @ Store dest row

    vmull.u8      q2, d0, d28
    vmlal.u8      q2, d2, d30
    vmlal.u8      q2, d1, d29
    vmlal.u8      q2, d3, d31
    bne           inner_loop_4

    vqrshrun.s16  d6, q2, #6
    vst1.8        {d6}, [r1], r3        @ Store dest row

    b             end_func

loop_2:
    vld1.8        {d0}, [r0], r2        @ Load row0
    vext.8        d1, d0, d0, #2
    vld1.8        {d2}, [r0], r2        @ Load row1
    vext.8        d3, d2, d2, #2
    vmull.u8      q2, d0, d28
    vmlal.u8      q2, d1, d29
    vmlal.u8      q2, d2, d30
    vmlal.u8      q2, d3, d31
    vld1.8        {d6}, [r0]            @ Load row2
    vqrshrun.s16  d4, q2, #6
    vext.8        d7, d6, d6, #2
    vst1.32       d4[0], [r1], r3       @ Store dest row0
    vmull.u8      q4, d2, d28
    vmlal.u8      q4, d3, d29
    vmlal.u8      q4, d6, d30
    vmlal.u8      q4, d7, d31
    subs          r6, #2
    vqrshrun.s16  d8, q4, #6
    vst1.32       d8[0], [r1], r3       @ Store dest row1
    bne           loop_2                @ repeat if ht=2

end_func:
    vldmia        sp!, {d8-d15}         @ Restore neon registers that were saved
    ldmfd         sp!, {r4-r12, pc}     @ Restoring registers from stack

