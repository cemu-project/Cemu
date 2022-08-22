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
@*  ih264_inter_pred_luma_horz_qpel_vert_hpel_a9q.s
@*
@* @brief
@*  Contains function definitions for inter prediction  interpolation.
@*
@* @author
@*  Mohit
@*
@* @par List of Functions:
@*
@*  - ih264_inter_pred_luma_horz_qpel_vert_hpel_a9q()
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
@*******************************************************************************
@*
@* @brief
@*   This function implements a two stage cascaded six tap filter. It
@*   applies the six tap filter in the vertical direction on the
@*   predictor values, followed by applying the same filter in the
@*   horizontal direction on the output of the first stage. It then averages
@*   the output of the 1st stage and the final stage to obtain the quarter
@*   pel values.The six tap filtering operation is described in sec 8.4.2.2.1
@*   titled "Luma sample interpolation process".
@*
@* @par Description:
@*    This function is called to obtain pixels lying at the following
@*    location (1/4,1/2) or (3/4,1/2). The function interpolates
@*    the predictors first in the verical direction and then in the
@*    horizontal direction to output the (1/2,1/2). It then averages
@*    the output of the 2nd stage and (1/2,1/2) value to obtain (1/4,1/2)
@*    or (3/4,1/2) depending on the offset.
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
@* @param[in] pu1_tmp: temporary buffer
@*
@* @param[in] dydx: x and y reference offset for qpel calculations
@*
@* @returns
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@*;

@void ih264_inter_pred_luma_horz_qpel_vert_hpel(UWORD8 *pu1_src,
@                                UWORD8 *pu1_dst,
@                                WORD32 src_strd,,
@                                WORD32 dst_strd,
@                                WORD32 ht,
@                                WORD32 wd,
@                                UWORD8* pu1_tmp,
@                                UWORD32 dydx)

@**************Variables Vs Registers*****************************************
@   r0 => *pu1_src
@   r1 => *pu1_dst
@   r2 =>  src_strd
@   r3 =>  dst_strd
@   r4 =>  ht
@   r5 =>  wd
@   r6 =>  dydx
@   r9 => *pu1_tmp

.text
.p2align 2

    .global ih264_inter_pred_luma_horz_qpel_vert_hpel_a9q

ih264_inter_pred_luma_horz_qpel_vert_hpel_a9q:

    stmfd         sp!, {r4-r12, r14}    @store register values to stack
    vstmdb        sp!, {d8-d15}         @push neon registers to stack
    ldr           r4, [sp, #104]        @ loads ht
    sub           r0, r0, r2, lsl #1    @pu1_src-2*src_strd
    sub           r0, r0, #2            @pu1_src-2
    ldr           r5, [sp, #108]        @ loads wd
    ldr           r6, [sp, #116]        @ loads dydx
    and           r6, r6, #2            @ dydx & 0x3 followed by dydx>>1 and dydx<<1
    ldr           r9, [sp, #112]        @pu1_tmp
    add           r7, r9, #4
    add           r6, r7, r6            @ pi16_pred1_temp += (x_offset>>1)

    vmov.u16      q13, #0x14            @ Filter coeff 20 into Q13
    vmov.u16      q12, #0x5             @ Filter coeff 5  into Q12
    mov           r7, #0x20
    mov           r8, #0x30
    subs          r12, r5, #4           @if wd=4 branch to loop_4
    beq           loop_4

    subs          r12, r5, #8           @if wd=8 branch to loop_8
    beq           loop_8

    @when  wd=16
    vmov.u16      q14, #0x14            @ Filter coeff 20 into Q13
    vmov.u16      q15, #0x5             @ Filter coeff 5  into Q12
    add           r14, r2, #0
    sub           r2, r2, #16


loop_16:

    vld1.u32      {q0}, [r0]!           @ Vector load from src[0_0]
    vld1.u32      d12, [r0], r2         @ Vector load from src[0_0]
    vld1.u32      {q1}, [r0]!           @ Vector load from src[1_0]
    vld1.u32      d13, [r0], r2         @ Vector load from src[1_0]
    vld1.u32      {q2}, [r0]!           @ Vector load from src[2_0]
    vld1.u32      d14, [r0], r2         @ Vector load from src[2_0]
    vld1.u32      {q3}, [r0]!           @ Vector load from src[3_0]
    vld1.u32      d15, [r0], r2         @ Vector load from src[3_0]
    vld1.u32      {q4}, [r0]!           @ Vector load from src[4_0]
    vld1.u32      d16, [r0], r2         @ Vector load from src[4_0]

    vld1.u32      {q5}, [r0]!           @ Vector load from src[5_0]
    vld1.u32      d17, [r0], r2         @ Vector load from src[5_0]

    vaddl.u8      q10, d4, d6
    vaddl.u8      q9, d0, d10
    vaddl.u8      q11, d2, d8
    vmla.u16      q9, q10, q14
    vaddl.u8      q12, d5, d7
    vaddl.u8      q10, d1, d11
    vaddl.u8      q13, d3, d9
    vmla.u16      q10, q12, q14
    vaddl.u8      q12, d14, d15
    vmls.u16      q9, q11, q15
    vaddl.u8      q11, d12, d17
    vmls.u16      q10, q13, q15
    vaddl.u8      q13, d13, d16
    vmla.u16      q11, q12, q14
    vmls.u16      q11, q13, q15
    vst1.32       {q9}, [r9]!
    vst1.32       {q10}, [r9]!
    vext.16       q12, q9, q10, #2
    vext.16       q13, q9, q10, #3
    vst1.32       {q11}, [r9]
    vext.16       q11, q9, q10, #5
    vadd.s16      q0, q12, q13
    vext.16       q12, q9, q10, #1
    vext.16       q13, q9, q10, #4
    vadd.s16      q12, q12, q13

    vaddl.s16     q13, d18, d22
    vmlal.s16     q13, d0, d28
    vmlsl.s16     q13, d24, d30

    vaddl.s16     q11, d19, d23
    vmlal.s16     q11, d1, d28
    vmlsl.s16     q11, d25, d30

    vqrshrun.s32  d18, q13, #10
    vqrshrun.s32  d19, q11, #10
    vld1.32       {q11}, [r9]!
    vqmovn.u16    d18, q9

    vext.16       q12, q10, q11, #2
    vext.16       q13, q10, q11, #3
    vext.16       q0, q10, q11, #5
    vst1.32       d18, [r1]
    vadd.s16      q9, q12, q13
    vext.16       q12, q10, q11, #1
    vext.16       q13, q10, q11, #4
    vadd.s16      q12, q12, q13

    vaddl.s16     q13, d0, d20
    vmlal.s16     q13, d18, d28
    vmlsl.s16     q13, d24, d30

    vaddl.s16     q11, d1, d21
    vmlal.s16     q11, d19, d28
    vmlsl.s16     q11, d25, d30

    vqrshrun.s32  d18, q13, #10
    vqrshrun.s32  d19, q11, #10

    vaddl.u8      q12, d7, d9
    vld1.32       {q10}, [r6]!
    vld1.32       {q11}, [r6], r7

    vqmovn.u16    d19, q9

    vld1.32       d18, [r1]
    vqrshrun.s16  d20, q10, #5
    vqrshrun.s16  d21, q11, #5
    vaddl.u8      q11, d4, d10
    vld1.u32      {q0}, [r0]!           @ Vector load from src[6_0]
    vrhadd.u8     q9, q9, q10
    vld1.u32      d12, [r0], r2         @ Vector load from src[6_0]
    vaddl.u8      q10, d6, d8
    vaddl.u8      q13, d5, d11
    vst1.32       {q9}, [r1], r3        @ store row 0

@ROW_2

    vaddl.u8      q9, d2, d0

    vmla.u16      q9, q10, q14

    vaddl.u8      q10, d3, d1

    vmla.u16      q10, q12, q14
    vaddl.u8      q12, d15, d16
    vmls.u16      q9, q11, q15
    vaddl.u8      q11, d13, d12
    vmls.u16      q10, q13, q15
    vaddl.u8      q13, d14, d17
    vmla.u16      q11, q12, q14
    vmls.u16      q11, q13, q15
    vst1.32       {q9}, [r9]!
    vst1.32       {q10}, [r9]!
    vext.16       q12, q9, q10, #2
    vext.16       q13, q9, q10, #3
    vst1.32       {q11}, [r9]
    vext.16       q11, q9, q10, #5
    vadd.s16      q1, q12, q13
    vext.16       q12, q9, q10, #1
    vext.16       q13, q9, q10, #4
    vadd.s16      q12, q12, q13

    vaddl.s16     q13, d18, d22
    vmlal.s16     q13, d2, d28
    vmlsl.s16     q13, d24, d30

    vaddl.s16     q11, d19, d23
    vmlal.s16     q11, d3, d28
    vmlsl.s16     q11, d25, d30

    vqrshrun.s32  d18, q13, #10
    vqrshrun.s32  d19, q11, #10
    vld1.32       {q11}, [r9]!
    vqmovn.u16    d18, q9

    vext.16       q12, q10, q11, #2
    vext.16       q13, q10, q11, #3
    vext.16       q1, q10, q11, #5
    vst1.32       d18, [r1]
    vadd.s16      q9, q12, q13
    vext.16       q12, q10, q11, #1
    vext.16       q13, q10, q11, #4
    vadd.s16      q12, q12, q13

    vaddl.s16     q13, d2, d20
    vmlal.s16     q13, d18, d28
    vmlsl.s16     q13, d24, d30

    vaddl.s16     q11, d3, d21
    vmlal.s16     q11, d19, d28
    vmlsl.s16     q11, d25, d30

    vqrshrun.s32  d18, q13, #10
    vqrshrun.s32  d19, q11, #10
    vaddl.u8      q12, d9, d11
    vld1.32       {q10}, [r6]!
    vld1.32       {q11}, [r6], r7
    vqmovn.u16    d19, q9
    vld1.32       d18, [r1]
    vqrshrun.s16  d20, q10, #5
    vqrshrun.s16  d21, q11, #5

    vrhadd.u8     q9, q9, q10

    vst1.32       {q9}, [r1], r3        @ store row 1

    subs          r4, r4, #2
    subne         r0, r0 , r14, lsl #2
    subne         r0, r0, r14

    beq           end_func              @ Branch if height==4
    b             loop_16               @ Loop if height==8

loop_8:
    vld1.u32      {q0}, [r0], r2        @ Vector load from src[0_0]
    vld1.u32      {q1}, [r0], r2        @ Vector load from src[1_0]
    vld1.u32      {q2}, [r0], r2        @ Vector load from src[2_0]
    vld1.u32      {q3}, [r0], r2        @ Vector load from src[3_0]
    vld1.u32      {q4}, [r0], r2        @ Vector load from src[4_0]

    vld1.u32      {q5}, [r0], r2        @ Vector load from src[5_0]
    vaddl.u8      q7, d4, d6
    vaddl.u8      q6, d0, d10
    vaddl.u8      q8, d2, d8
    vmla.u16      q6, q7, q13
    vaddl.u8      q9, d5, d7
    vaddl.u8      q7, d1, d11
    vaddl.u8      q11, d3, d9
    vmla.u16      q7, q9, q13
    vmls.u16      q6, q8, q12
    vld1.32       {q0}, [r0], r2        @ Vector load from src[6_0]
    vaddl.u8      q8, d6, d8
    vmls.u16      q7, q11, q12
    vaddl.u8      q14, d2, d0
    vst1.32       {q6}, [r9]!           @ store row 0 to temp buffer: col 0
    vext.16       q11, q6, q7, #5
    vaddl.u8      q9, d4, d10
    vmla.u16      q14, q8, q13
    vaddl.s16     q15, d12, d22
    vst1.32       {q7}, [r9], r7        @ store row 0 to temp buffer: col 1
    vaddl.s16     q11, d13, d23
    vext.16       q8, q6, q7, #2
    vmls.u16      q14, q9, q12
    vext.16       q9, q6, q7, #3
    vext.16       q10, q6, q7, #4
    vext.16       q7, q6, q7, #1
    vadd.s16      q8, q8, q9
    vadd.s16      q9, q7, q10
    vaddl.u8      q10, d7, d9
    vmlal.s16     q15, d16, d26
    vmlsl.s16     q15, d18, d24
    vmlal.s16     q11, d17, d26
    vmlsl.s16     q11, d19, d24
    vaddl.u8      q7, d3, d1
    vst1.32       {q14}, [r9]!          @ store row 1 to temp buffer: col 0
    vmla.u16      q7, q10, q13
    vqrshrun.s32  d12, q15, #10
    vaddl.u8      q8, d5, d11
    vqrshrun.s32  d13, q11, #10
    vmls.u16      q7, q8, q12
@   vld1.32     {q1},[r0],r2            ; Vector load from src[7_0]
    vqmovn.u16    d25, q6
    vaddl.u8      q8, d8, d10


    vext.16       q11, q14, q7, #5
    vaddl.u8      q10, d4, d2
    vaddl.s16     q15, d28, d22
    vmla.u16      q10, q8, q13
    vst1.32       {q7}, [r9], r7        @ store row 1 to temp buffer: col 1
    vaddl.s16     q11, d29, d23
    vext.16       q8, q14, q7, #2
    vext.16       q9, q14, q7, #3
    vext.16       q6, q14, q7, #4
    vext.16       q7, q14, q7, #1
    vadd.s16      q8, q8, q9
    vadd.s16      q9, q6, q7
    vld1.32       {q7}, [r6], r8        @ load row 0 from temp buffer
    vmlal.s16     q15, d16, d26
    vmlsl.s16     q15, d18, d24
    vmlal.s16     q11, d17, d26
    vmlsl.s16     q11, d19, d24
    vqrshrun.s16  d14, q7, #0x5
    vld1.32       {q14}, [r6], r8       @ load row 1 from temp buffer
    vaddl.u8      q9, d6, d0
    vqrshrun.s32  d16, q15, #10
    vqrshrun.s16  d15, q14, #0x5
    vqrshrun.s32  d17, q11, #10
    vmov          d12, d25
    vmov          d25, d24

    vqmovn.u16    d13, q8
    vrhadd.u8     q6, q6, q7

    vst1.32       d12, [r1], r3         @ store row 0
    vst1.32       d13, [r1], r3         @ store row 1

    subs          r4, r4, #2
    subne         r0, r0 , r2, lsl #2
    subne         r0, r0, r2

    beq           end_func              @ Branch if height==4
    b             loop_8                @ Loop if height==8

loop_4:
    vld1.u32      {q0}, [r0], r2        @ Vector load from src[0_0]
    vld1.u32      {q1}, [r0], r2        @ Vector load from src[1_0]
    vld1.u32      {q2}, [r0], r2        @ Vector load from src[2_0]
    vld1.u32      {q3}, [r0], r2        @ Vector load from src[3_0]
    vld1.u32      {q4}, [r0], r2        @ Vector load from src[4_0]
    vld1.u32      {q5}, [r0], r2        @ Vector load from src[5_0]

    vaddl.u8      q7, d4, d6            @ temp1 = src[2_0] + src[3_0]
    vaddl.u8      q6, d0, d10           @ temp = src[0_0] + src[5_0]
    vaddl.u8      q8, d2, d8            @ temp2 = src[1_0] + src[4_0]
    vmla.u16      q6, q7, q13           @ temp += temp1 * 20
    vaddl.u8      q9, d5, d7            @ temp1 = src[2_0] + src[3_0]
    vaddl.u8      q7, d1, d11           @ temp = src[0_0] + src[5_0]
    vaddl.u8      q11, d3, d9           @ temp2 = src[1_0] + src[4_0]
    vmla.u16      q7, q9, q13           @ temp += temp1 * 20
    vmls.u16      q6, q8, q12           @ temp -= temp2 * 5
    vld1.32       {q0}, [r0], r2        @ Vector load from src[6_0]
    vaddl.u8      q8, d6, d8
    vmls.u16      q7, q11, q12          @ temp -= temp2 * 5
    @Q6 and Q7 have filtered values
    vaddl.u8      q14, d2, d0
    vst1.32       {q6}, [r9]!           @ store row 0 to temp buffer: col 0
    vext.16       q11, q6, q7, #5
    vaddl.u8      q9, d4, d10
    vmla.u16      q14, q8, q13
    vaddl.s16     q15, d12, d22
    vst1.32       {q7}, [r9], r7        @ store row 0 to temp buffer: col 1
    vaddl.s16     q11, d13, d23
    vext.16       q8, q6, q7, #2
    vmls.u16      q14, q9, q12
    vext.16       q9, q6, q7, #3
    vext.16       q10, q6, q7, #4
    vext.16       q7, q6, q7, #1
    vadd.s16      q8, q8, q9
    vadd.s16      q9, q7, q10
    vaddl.u8      q10, d7, d9
    vmlal.s16     q15, d16, d26
    vmlsl.s16     q15, d18, d24
    vmlal.s16     q11, d17, d26
    vmlsl.s16     q11, d19, d24
    vaddl.u8      q7, d3, d1
    vst1.32       {q14}, [r9]!          @ store row 1 to temp buffer: col 0
    vmla.u16      q7, q10, q13
    vqrshrun.s32  d12, q15, #10
    vaddl.u8      q8, d5, d11
    vqrshrun.s32  d13, q11, #10
    vmls.u16      q7, q8, q12
    vqmovn.u16    d25, q6
    vaddl.u8      q8, d8, d10

    vext.16       q11, q14, q7, #5
    vaddl.u8      q10, d4, d2
    vaddl.s16     q15, d28, d22
    vmla.u16      q10, q8, q13
    vst1.32       {q7}, [r9], r7        @ store row 1 to temp buffer: col 1
    vaddl.s16     q11, d29, d23
    vext.16       q8, q14, q7, #2
    vext.16       q9, q14, q7, #3
    vext.16       q6, q14, q7, #4
    vext.16       q7, q14, q7, #1
    vadd.s16      q8, q8, q9
    vadd.s16      q9, q6, q7
    vld1.32       d14, [r6], r8         @load row 0 from temp buffer
    vmlal.s16     q15, d16, d26
    vmlsl.s16     q15, d18, d24
    vmlal.s16     q11, d17, d26
    vmlsl.s16     q11, d19, d24
    vqrshrun.s16  d14, q7, #0x5
    vld1.32       d28, [r6], r8         @load row 1 from temp buffer
    vaddl.u8      q9, d6, d0
    vqrshrun.s32  d16, q15, #10
    vqrshrun.s16  d15, q14, #0x5
    vqrshrun.s32  d17, q11, #10
    vmov          d12, d25
    vmov          d25, d24

    vqmovn.u16    d13, q8
    vrhadd.u8     q6, q6, q7
    vst1.32       d12[0], [r1], r3      @ store row 0
    vst1.32       d13[0], [r1], r3      @store row 1

    subs          r4, r4, #2
    subne         r0, r0 , r2, lsl #2
    subne         r0, r0, r2

    beq           end_func              @ Branch if height==4
    b             loop_4                @ Loop if height==8

end_func:
    vldmia        sp!, {d8-d15}         @ Restore neon registers that were saved
    ldmfd         sp!, {r4-r12, pc}     @Restoring registers from stack


