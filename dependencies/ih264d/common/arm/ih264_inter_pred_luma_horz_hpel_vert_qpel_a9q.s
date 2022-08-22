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
@*  ih264_inter_pred_luma_horz_hpel_vert_qpel_a9q.s
@*
@* @brief
@*  Contains function definitions for inter prediction  interpolation.
@*
@* @author
@*  Mohit
@*
@* @par List of Functions:
@*
@*  - ih264_inter_pred_luma_horz_hpel_vert_qpel_a9q()
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
@*    applies the six tap filter in the horizontal direction on the
@*    predictor values, followed by applying the same filter in the
@*    vertical direction on the output of the first stage. It then averages
@*    the output of the 1st stage and the output of the 2nd stage to obtain
@*    the quarter pel values. The six tap filtering operation is described
@*    in sec 8.4.2.2.1 titled "Luma sample interpolation process".
@*
@* @par Description:
@*     This function is called to obtain pixels lying at the following
@*    location (1/2,1/4) or (1/2,3/4). The function interpolates
@*    the predictors first in the horizontal direction and then in the
@*    vertical direction to output the (1/2,1/2). It then averages
@*    the output of the 2nd stage and (1/2,1/2) value to obtain (1/2,1/4)
@*    or (1/2,3/4) depending on the offset.
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

@void ih264_inter_pred_luma_horz_hpel_vert_qpel(UWORD8 *pu1_src,
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
@   r7 =>  dydx
@   r9 => *pu1_tmp

.text
.p2align 2

    .global ih264_inter_pred_luma_horz_hpel_vert_qpel_a9q

ih264_inter_pred_luma_horz_hpel_vert_qpel_a9q:

    stmfd         sp!, {r4-r12, r14}    @ store register values to stack
    vstmdb        sp!, {d8-d15}         @push neon registers to stack
    ldr           r4, [sp, #104]        @ loads ht
    sub           r0, r0, r2, lsl #1    @ pu1_src-2*src_strd
    sub           r0, r0, #2            @ pu1_src-2
    ldr           r5, [sp, #108]        @ loads wd
    ldr           r7, [sp, #116]        @ loads dydx
    lsr           r7, r7, #3            @ dydx >> 2 followed by dydx & 0x3 and dydx>>1 to obtain the deciding bit
    ldr           r9, [sp, #112]        @ pu1_tmp
    add           r7, r7, #2
    mov           r6, #48
    mla           r7, r7, r6, r9

    subs          r12, r5, #4           @if wd=4 branch to loop_4
    beq           loop_4_start

    subs          r12, r5, #8           @if wd=8 branch to loop_8
    beq           loop_8_start

    @when  wd=16
    vmov.u16      q11, #20              @ Filter coeff 0x14 into Q11
    vmov.u16      q12, #5               @ Filter coeff 0x5  into Q12
    add           r8, r0, #8
    add           r14, r1, #8
    add           r10, r9, #8
    mov           r12, r4
    add           r11, r7, #8

loop_16_lowhalf_start:
    vld1.32       {q0}, [r0], r2        @ row -2 load for horizontal filter
    vext.8        d5, d0, d1, #5
    vaddl.u8      q3, d0, d5

    vext.8        d2, d0, d1, #2
    vext.8        d3, d0, d1, #3
    vaddl.u8      q4, d2, d3
    vext.8        d4, d0, d1, #4
    vmla.u16      q3, q4, q11
    vext.8        d1, d0, d1, #1
    vaddl.u8      q4, d1, d4
    vld1.32       {q0}, [r0], r2        @ row -1 load for horizontal filter
    vmls.u16      q3, q4, q12
    vext.8        d5, d0, d1, #5
    vaddl.u8      q4, d0, d5
    vext.8        d2, d0, d1, #2
    vext.8        d3, d0, d1, #3
    vaddl.u8      q5, d2, d3

    vst1.32       {q3}, [r9], r6        @ store temp buffer 0

    vext.8        d4, d0, d1, #4
    vmla.u16      q4, q5, q11
    vext.8        d1, d0, d1, #1
    vaddl.u8      q5, d1, d4
    vld1.32       {q0}, [r0], r2        @ row 0 load for horizontal filter
    vmls.u16      q4, q5, q12
    vext.8        d5, d0, d1, #5
    vaddl.u8      q5, d0, d5
    vext.8        d2, d0, d1, #2
    vext.8        d3, d0, d1, #3
    vaddl.u8      q6, d2, d3

    vst1.32       {q4}, [r9], r6        @ store temp buffer 1

    vext.8        d4, d0, d1, #4
    vmla.u16      q5, q6, q11
    vext.8        d1, d0, d1, #1
    vaddl.u8      q6, d1, d4
    vld1.32       {q0}, [r0], r2        @ row 1 load for horizontal filter
    vmls.u16      q5, q6, q12
    vext.8        d5, d0, d1, #5
    vaddl.u8      q6, d0, d5
    vext.8        d2, d0, d1, #2
    vext.8        d3, d0, d1, #3
    vaddl.u8      q7, d2, d3

    vst1.32       {q5}, [r9], r6        @ store temp buffer 2

    vext.8        d4, d0, d1, #4
    vmla.u16      q6, q7, q11
    vext.8        d1, d0, d1, #1
    vaddl.u8      q7, d1, d4
    vld1.32       {q0}, [r0], r2        @ row 2 load for horizontal filter
    vmls.u16      q6, q7, q12
    vext.8        d5, d0, d1, #5
    vaddl.u8      q7, d0, d5
    vext.8        d2, d0, d1, #2
    vext.8        d3, d0, d1, #3
    vaddl.u8      q8, d2, d3

    vst1.32       {q6}, [r9], r6        @ store temp buffer 3

    vext.8        d4, d0, d1, #4
    vmla.u16      q7, q8, q11
    vext.8        d1, d0, d1, #1
    vaddl.u8      q8, d1, d4

    vmls.u16      q7, q8, q12
loop_16_lowhalf:

    vld1.32       {q0}, [r0], r2        @ row 3 load for horizontal filter
    vext.8        d5, d0, d1, #5
    vext.8        d2, d0, d1, #2
    vext.8        d3, d0, d1, #3
    vaddl.u8      q8, d0, d5

    vst1.32       {q7}, [r9], r6        @ store temp buffer 4
    vaddl.u8      q9, d2, d3
    vext.8        d4, d0, d1, #4
    vmla.u16      q8, q9, q11
    vext.8        d1, d0, d1, #1
    vadd.s16      q14, q4, q7
    vaddl.u8      q9, d1, d4
    vadd.s16      q15, q5, q6
    vmls.u16      q8, q9, q12
    vld1.32       {q0}, [r0], r2        @ row 4 load for hoorizontal filter
    vext.8        d5, d0, d1, #5
    vext.8        d2, d0, d1, #2
    vext.8        d3, d0, d1, #3
    vaddl.u8      q10, d0, d5

    vst1.32       {q8}, [r9], r6        @ store temp buffer r5

    vaddl.s16     q9, d6, d16

    vld1.32       {q13}, [r7], r6       @ load from temp buffer 0

    vaddl.s16     q3, d7, d17

    vqrshrun.s16  d26, q13, #5

    vmlal.s16     q9, d30, d22
    vmlsl.s16     q9, d28, d24
    vmlal.s16     q3, d31, d22
    vmlsl.s16     q3, d29, d24
    vaddl.u8      q1, d2, d3
    vext.8        d4, d0, d1, #4
    vmla.u16      q10, q1, q11
    vqrshrun.s32  d18, q9, #10
    vext.8        d1, d0, d1, #1
    vqrshrun.s32  d19, q3, #10
    vadd.s16      q14, q5, q8
    vaddl.u8      q1, d1, d4
    vadd.s16      q15, q6, q7
    vmls.u16      q10, q1, q12
    vqmovn.u16    d18, q9
    vld1.32       {q0}, [r0], r2        @ row 5 load for horizontal filter

    vrhadd.u8     d26, d18, d26

    vext.8        d5, d0, d1, #5
    vext.8        d2, d0, d1, #2

    vst1.32       {q10}, [r9], r6       @ store temp buffer r6

    vaddl.s16     q9, d8, d20

    vaddl.s16     q3, d9, d21

    vld1.32       {q4}, [r7], r6        @load from temp buffer 1


    vst1.32       d26, [r1], r3         @ store row 0

    vmlal.s16     q9, d30, d22
    vmlsl.s16     q9, d28, d24

    vqrshrun.s16  d28, q4, #5

    vmlal.s16     q3, d31, d22
    vmlsl.s16     q3, d29, d24
    vext.8        d3, d0, d1, #3
    vaddl.u8      q4, d0, d5
    vaddl.u8      q1, d2, d3
    vqrshrun.s32  d18, q9, #10
    vext.8        d4, d0, d1, #4
    vqrshrun.s32  d19, q3, #10
    vmla.u16      q4, q1, q11
    vext.8        d1, d0, d1, #1
    vadd.s16      q13, q6, q10
    vaddl.u8      q1, d1, d4
    vqmovn.u16    d18, q9
    vadd.s16      q15, q7, q8
    vmls.u16      q4, q1, q12
    vld1.32       {q0}, [r0], r2        @ row 6 load for horizontal filter

    vrhadd.u8     d28, d28, d18

    vext.8        d5, d0, d1, #5
    vext.8        d2, d0, d1, #2
    vext.8        d3, d0, d1, #3

    vst1.32       d28, [r1], r3         @ store row 1

    vaddl.u8      q14, d0, d5

    vst1.32       {q4}, [r9], r6        @ store temp buffer r7

    vaddl.s16     q9, d10, d8
    vaddl.s16     q3, d11, d9

    vld1.32       {q5}, [r7], r6        @ load from temp buffer 2

    vmlal.s16     q9, d30, d22
    vmlsl.s16     q9, d26, d24
    vmlal.s16     q3, d31, d22

    vqrshrun.s16  d26, q5, #5

    vmlsl.s16     q3, d27, d24
    vaddl.u8      q1, d2, d3
    vext.8        d4, d0, d1, #4
    vmla.u16      q14, q1, q11
    vqrshrun.s32  d18, q9, #10
    vext.8        d1, d0, d1, #1
    vqrshrun.s32  d19, q3, #10
    vadd.s16      q5, q7, q4
    vaddl.u8      q1, d1, d4
    vadd.s16      q15, q8, q10
    vmls.u16      q14, q1, q12
    vqmovn.u16    d27, q9

    vaddl.s16     q9, d12, d28
    vaddl.s16     q3, d13, d29

    vrhadd.u8     d26, d26, d27

    vmlal.s16     q9, d30, d22
    vmlsl.s16     q9, d10, d24
    vmlal.s16     q3, d31, d22
    vmlsl.s16     q3, d11, d24

    vst1.32       d26, [r1], r3         @ store row 2

    vst1.32       {q14}, [r9]


    vqrshrun.s32  d18, q9, #10
    vmov          q5, q10
    vld1.32       {q15}, [r7], r6       @ load from temp buffer 3

    vqrshrun.s32  d19, q3, #10
    subs          r4, r4, #4

    vqrshrun.s16  d30, q15, #5

    vqmovn.u16    d18, q9
    vmov          q6, q4
    vmov          q3, q7
    vrhadd.u8     d30, d18, d30
    vmov          q4, q8
    vmov          q7, q14
    vst1.32       d30, [r1], r3         @ store row 3

    bgt           loop_16_lowhalf       @ looping if height =16


loop_16_highhalf_start:
    vld1.32       {q0}, [r8], r2
    vext.8        d5, d0, d1, #5
    vaddl.u8      q3, d0, d5
    vext.8        d2, d0, d1, #2
    vext.8        d3, d0, d1, #3
    vaddl.u8      q4, d2, d3
    vext.8        d4, d0, d1, #4
    vmla.u16      q3, q4, q11
    vext.8        d1, d0, d1, #1
    vaddl.u8      q4, d1, d4
    vld1.32       {q0}, [r8], r2
    vmls.u16      q3, q4, q12
    vext.8        d5, d0, d1, #5
    vaddl.u8      q4, d0, d5
    vext.8        d2, d0, d1, #2
    vext.8        d3, d0, d1, #3
    vaddl.u8      q5, d2, d3

    vst1.32       {q3}, [r10], r6

    vext.8        d4, d0, d1, #4
    vmla.u16      q4, q5, q11
    vext.8        d1, d0, d1, #1
    vaddl.u8      q5, d1, d4
    vld1.32       {q0}, [r8], r2
    vmls.u16      q4, q5, q12
    vext.8        d5, d0, d1, #5
    vaddl.u8      q5, d0, d5
    vext.8        d2, d0, d1, #2
    vext.8        d3, d0, d1, #3
    vaddl.u8      q6, d2, d3

    vst1.32       {q4}, [r10], r6

    vext.8        d4, d0, d1, #4
    vmla.u16      q5, q6, q11
    vext.8        d1, d0, d1, #1
    vaddl.u8      q6, d1, d4
    vld1.32       {q0}, [r8], r2
    vmls.u16      q5, q6, q12
    vext.8        d5, d0, d1, #5
    vaddl.u8      q6, d0, d5
    vext.8        d2, d0, d1, #2
    vext.8        d3, d0, d1, #3
    vaddl.u8      q7, d2, d3

    vst1.32       {q5}, [r10], r6

    vext.8        d4, d0, d1, #4
    vmla.u16      q6, q7, q11
    vext.8        d1, d0, d1, #1
    vaddl.u8      q7, d1, d4
    vld1.32       {q0}, [r8], r2
    vmls.u16      q6, q7, q12
    vext.8        d5, d0, d1, #5
    vaddl.u8      q7, d0, d5
    vext.8        d2, d0, d1, #2
    vext.8        d3, d0, d1, #3
    vaddl.u8      q8, d2, d3

    vst1.32       {q6}, [r10], r6

    vext.8        d4, d0, d1, #4
    vmla.u16      q7, q8, q11
    vext.8        d1, d0, d1, #1
    vaddl.u8      q8, d1, d4

    vmls.u16      q7, q8, q12

loop_16_highhalf:

    vld1.32       {q0}, [r8], r2
    vext.8        d5, d0, d1, #5
    vext.8        d2, d0, d1, #2
    vext.8        d3, d0, d1, #3
    vaddl.u8      q8, d0, d5

    vst1.32       {q7}, [r10], r6

    vaddl.u8      q9, d2, d3
    vext.8        d4, d0, d1, #4
    vmla.u16      q8, q9, q11
    vext.8        d1, d0, d1, #1
    vadd.s16      q14, q4, q7
    vaddl.u8      q9, d1, d4
    vadd.s16      q15, q5, q6
    vmls.u16      q8, q9, q12
    vld1.32       {q0}, [r8], r2
    vext.8        d5, d0, d1, #5
    vext.8        d2, d0, d1, #2
    vext.8        d3, d0, d1, #3
    vaddl.u8      q10, d0, d5

    vst1.32       {q8}, [r10], r6

    vaddl.s16     q9, d6, d16

    vld1.32       {q13}, [r11], r6

    vaddl.s16     q3, d7, d17

    vqrshrun.s16  d26, q13, #5

    vmlal.s16     q9, d30, d22
    vmlsl.s16     q9, d28, d24
    vmlal.s16     q3, d31, d22
    vmlsl.s16     q3, d29, d24
    vaddl.u8      q1, d2, d3
    vext.8        d4, d0, d1, #4
    vmla.u16      q10, q1, q11
    vqrshrun.s32  d18, q9, #10
    vext.8        d1, d0, d1, #1
    vqrshrun.s32  d19, q3, #10
    vadd.s16      q14, q5, q8
    vaddl.u8      q1, d1, d4
    vadd.s16      q15, q6, q7
    vmls.u16      q10, q1, q12
    vqmovn.u16    d18, q9
    vld1.32       {q0}, [r8], r2

    vrhadd.u8     d26, d18, d26

    vext.8        d5, d0, d1, #5
    vext.8        d2, d0, d1, #2

    vst1.32       {q10}, [r10], r6

    vaddl.s16     q9, d8, d20
    vaddl.s16     q3, d9, d21

    vld1.32       {q4}, [r11], r6


    vst1.32       d26, [r14], r3        @store row 0

    vmlal.s16     q9, d30, d22
    vmlsl.s16     q9, d28, d24

    vqrshrun.s16  d28, q4, #5

    vmlal.s16     q3, d31, d22
    vmlsl.s16     q3, d29, d24
    vext.8        d3, d0, d1, #3
    vaddl.u8      q4, d0, d5
    vaddl.u8      q1, d2, d3
    vqrshrun.s32  d18, q9, #10
    vext.8        d4, d0, d1, #4
    vqrshrun.s32  d19, q3, #10
    vmla.u16      q4, q1, q11
    vext.8        d1, d0, d1, #1
    vadd.s16      q13, q6, q10
    vaddl.u8      q1, d1, d4
    vqmovn.u16    d18, q9
    vadd.s16      q15, q7, q8
    vmls.u16      q4, q1, q12
    vld1.32       {q0}, [r8], r2

    vrhadd.u8     d28, d28, d18

    vext.8        d5, d0, d1, #5
    vext.8        d2, d0, d1, #2
    vext.8        d3, d0, d1, #3

    vst1.32       d28, [r14], r3        @store row 1

    vaddl.u8      q14, d0, d5

    vst1.32       {q4}, [r10], r6

    vaddl.s16     q9, d10, d8
    vaddl.s16     q3, d11, d9

    vld1.32       {q5}, [r11], r6

    vmlal.s16     q9, d30, d22
    vmlsl.s16     q9, d26, d24
    vmlal.s16     q3, d31, d22

    vqrshrun.s16  d26, q5, #5

    vmlsl.s16     q3, d27, d24
    vaddl.u8      q1, d2, d3
    vext.8        d4, d0, d1, #4
    vmla.u16      q14, q1, q11
    vqrshrun.s32  d18, q9, #10
    vext.8        d1, d0, d1, #1
    vqrshrun.s32  d19, q3, #10
    vadd.s16      q5, q7, q4
    vaddl.u8      q1, d1, d4
    vadd.s16      q15, q8, q10
    vmls.u16      q14, q1, q12
    vqmovn.u16    d27, q9


    vaddl.s16     q9, d12, d28
    vaddl.s16     q3, d13, d29

    vrhadd.u8     d26, d26, d27

    vmlal.s16     q9, d30, d22
    vmlsl.s16     q9, d10, d24
    vmlal.s16     q3, d31, d22
    vmlsl.s16     q3, d11, d24

    vst1.32       d26, [r14], r3        @ store row 2

    vst1.32       {q14}, [r10]

    vqrshrun.s32  d18, q9, #10
    vmov          q5, q10
    vld1.32       {q15}, [r11], r6

    vqrshrun.s32  d19, q3, #10
    subs          r12, r12, #4

    vqrshrun.s16  d30, q15, #5

    vqmovn.u16    d18, q9
    vmov          q6, q4
    vmov          q3, q7
    vrhadd.u8     d30, d18, d30
    vmov          q4, q8
    vmov          q7, q14
    vst1.32       d30, [r14], r3        @ store row 3

    bgt           loop_16_highhalf      @ looping if height = 8 or 16
    b             end_func

loop_8_start:

    vmov.u16      q11, #20              @ Filter coeff 20 into Q11
    vmov.u16      q12, #5               @ Filter coeff 5  into Q12
    vld1.32       {q0}, [r0], r2        @ row -2 load for horizontal filter
    vext.8        d5, d0, d1, #5
    vaddl.u8      q3, d0, d5

    vext.8        d2, d0, d1, #2
    vext.8        d3, d0, d1, #3
    vaddl.u8      q4, d2, d3
    vext.8        d4, d0, d1, #4
    vmla.u16      q3, q4, q11
    vext.8        d1, d0, d1, #1
    vaddl.u8      q4, d1, d4
    vld1.32       {q0}, [r0], r2        @ row -1 load for horizontal filter
    vmls.u16      q3, q4, q12
    vext.8        d5, d0, d1, #5
    vaddl.u8      q4, d0, d5
    vext.8        d2, d0, d1, #2
    vext.8        d3, d0, d1, #3
    vaddl.u8      q5, d2, d3

    vst1.32       {q3}, [r9], r6        @ store temp buffer 0

    vext.8        d4, d0, d1, #4
    vmla.u16      q4, q5, q11
    vext.8        d1, d0, d1, #1
    vaddl.u8      q5, d1, d4
    vld1.32       {q0}, [r0], r2        @ row 0 load for horizontal filter
    vmls.u16      q4, q5, q12
    vext.8        d5, d0, d1, #5
    vaddl.u8      q5, d0, d5
    vext.8        d2, d0, d1, #2
    vext.8        d3, d0, d1, #3
    vaddl.u8      q6, d2, d3

    vst1.32       {q4}, [r9], r6        @ store temp buffer 1

    vext.8        d4, d0, d1, #4
    vmla.u16      q5, q6, q11
    vext.8        d1, d0, d1, #1
    vaddl.u8      q6, d1, d4
    vld1.32       {q0}, [r0], r2        @ row 1 load for horizontal filter
    vmls.u16      q5, q6, q12
    vext.8        d5, d0, d1, #5
    vaddl.u8      q6, d0, d5
    vext.8        d2, d0, d1, #2
    vext.8        d3, d0, d1, #3
    vaddl.u8      q7, d2, d3

    vst1.32       {q5}, [r9], r6        @ store temp buffer 2

    vext.8        d4, d0, d1, #4
    vmla.u16      q6, q7, q11
    vext.8        d1, d0, d1, #1
    vaddl.u8      q7, d1, d4
    vld1.32       {q0}, [r0], r2        @ row 2 load for horizontal filter
    vmls.u16      q6, q7, q12
    vext.8        d5, d0, d1, #5
    vaddl.u8      q7, d0, d5
    vext.8        d2, d0, d1, #2
    vext.8        d3, d0, d1, #3
    vaddl.u8      q8, d2, d3

    vst1.32       {q6}, [r9], r6        @ store temp buffer 3

    vext.8        d4, d0, d1, #4
    vmla.u16      q7, q8, q11
    vext.8        d1, d0, d1, #1
    vaddl.u8      q8, d1, d4

    vmls.u16      q7, q8, q12
loop_8:

    vld1.32       {q0}, [r0], r2        @ row 3 load for horizontal filter
    vext.8        d5, d0, d1, #5
    vext.8        d2, d0, d1, #2
    vext.8        d3, d0, d1, #3
    vaddl.u8      q8, d0, d5

    vst1.32       {q7}, [r9], r6        @ store temp buffer 4

    vaddl.u8      q9, d2, d3
    vext.8        d4, d0, d1, #4
    vmla.u16      q8, q9, q11
    vext.8        d1, d0, d1, #1
    vadd.s16      q14, q4, q7
    vaddl.u8      q9, d1, d4
    vadd.s16      q15, q5, q6
    vmls.u16      q8, q9, q12
    vld1.32       {q0}, [r0], r2        @ row 4 load for hoorizontal filter
    vext.8        d5, d0, d1, #5
    vext.8        d2, d0, d1, #2
    vext.8        d3, d0, d1, #3
    vaddl.u8      q10, d0, d5

    vst1.32       {q8}, [r9], r6        @ store temp buffer r5

    vaddl.s16     q9, d6, d16

    vld1.32       {q13}, [r7], r6       @ load from temp buffer 0

    vaddl.s16     q3, d7, d17

    vqrshrun.s16  d26, q13, #5

    vmlal.s16     q9, d30, d22
    vmlsl.s16     q9, d28, d24
    vmlal.s16     q3, d31, d22
    vmlsl.s16     q3, d29, d24
    vaddl.u8      q1, d2, d3
    vext.8        d4, d0, d1, #4
    vmla.u16      q10, q1, q11
    vqrshrun.s32  d18, q9, #10
    vext.8        d1, d0, d1, #1
    vqrshrun.s32  d19, q3, #10
    vadd.s16      q14, q5, q8
    vaddl.u8      q1, d1, d4
    vadd.s16      q15, q6, q7
    vmls.u16      q10, q1, q12
    vqmovn.u16    d18, q9
    vld1.32       {q0}, [r0], r2        @ row 5 load for horizontal filter

    vrhadd.u8     d26, d18, d26

    vext.8        d5, d0, d1, #5
    vext.8        d2, d0, d1, #2

    vst1.32       {q10}, [r9], r6       @ store temp buffer r6

    vaddl.s16     q9, d8, d20

    vaddl.s16     q3, d9, d21

    vld1.32       {q4}, [r7], r6        @load from temp buffer 1


    vst1.32       d26, [r1], r3         @ store row 0

    vmlal.s16     q9, d30, d22
    vmlsl.s16     q9, d28, d24

    vqrshrun.s16  d28, q4, #5

    vmlal.s16     q3, d31, d22
    vmlsl.s16     q3, d29, d24
    vext.8        d3, d0, d1, #3
    vaddl.u8      q4, d0, d5
    vaddl.u8      q1, d2, d3
    vqrshrun.s32  d18, q9, #10
    vext.8        d4, d0, d1, #4
    vqrshrun.s32  d19, q3, #10
    vmla.u16      q4, q1, q11
    vext.8        d1, d0, d1, #1
    vadd.s16      q13, q6, q10
    vaddl.u8      q1, d1, d4
    vqmovn.u16    d18, q9
    vadd.s16      q15, q7, q8
    vmls.u16      q4, q1, q12
    vld1.32       {q0}, [r0], r2        @ row 6 load for horizontal filter

    vrhadd.u8     d28, d28, d18

    vext.8        d5, d0, d1, #5
    vext.8        d2, d0, d1, #2
    vext.8        d3, d0, d1, #3

    vst1.32       d28, [r1], r3         @ store row 1

    vaddl.u8      q14, d0, d5

    vst1.32       {q4}, [r9], r6        @ store temp buffer r7

    vaddl.s16     q9, d10, d8
    vaddl.s16     q3, d11, d9

    vld1.32       {q5}, [r7], r6        @ load from temp buffer 2

    vmlal.s16     q9, d30, d22
    vmlsl.s16     q9, d26, d24
    vmlal.s16     q3, d31, d22

    vqrshrun.s16  d26, q5, #5

    vmlsl.s16     q3, d27, d24
    vaddl.u8      q1, d2, d3
    vext.8        d4, d0, d1, #4
    vmla.u16      q14, q1, q11
    vqrshrun.s32  d18, q9, #10
    vext.8        d1, d0, d1, #1
    vqrshrun.s32  d19, q3, #10
    vadd.s16      q5, q7, q4
    vaddl.u8      q1, d1, d4
    vadd.s16      q15, q8, q10
    vmls.u16      q14, q1, q12
    vqmovn.u16    d27, q9

    vaddl.s16     q9, d12, d28
    vaddl.s16     q3, d13, d29

    vrhadd.u8     d26, d26, d27

    vmlal.s16     q9, d30, d22
    vmlsl.s16     q9, d10, d24
    vmlal.s16     q3, d31, d22
    vmlsl.s16     q3, d11, d24

    vst1.32       d26, [r1], r3         @ store row 2

    vst1.32       {q14}, [r9]


    vqrshrun.s32  d18, q9, #10
    vmov          q5, q10
    vld1.32       {q15}, [r7], r6       @ load from temp buffer 3

    vqrshrun.s32  d19, q3, #10
    subs          r4, r4, #4

    vqrshrun.s16  d30, q15, #5

    vqmovn.u16    d18, q9
    vmov          q6, q4
    vmov          q3, q7
    vrhadd.u8     d30, d18, d30
    vmov          q4, q8
    vmov          q7, q14
    vst1.32       d30, [r1], r3         @ store row 3

    bgt           loop_8                @if height =8 or 16  loop
    b             end_func

loop_4_start:
    vmov.u16      d22, #20              @ Filter coeff 20 into D22
    vmov.u16      d23, #5               @ Filter coeff 5  into D23

    vld1.32       {q0}, [r0], r2        @row -2 load
    vext.8        d5, d0, d1, #5
    vaddl.u8      q3, d0, d5
    vext.8        d2, d0, d1, #2
    vext.8        d3, d0, d1, #3
    vaddl.u8      q4, d2, d3
    vext.8        d4, d0, d1, #4
    vmla.u16      d6, d8, d22
    vext.8        d1, d0, d1, #1
    vaddl.u8      q4, d1, d4
    vld1.32       {q0}, [r0], r2        @ row -1 load
    vmls.u16      d6, d8, d23
    vext.8        d5, d0, d1, #5
    vaddl.u8      q4, d0, d5
    vext.8        d2, d0, d1, #2
    vext.8        d3, d0, d1, #3
    vaddl.u8      q5, d2, d3

    vst1.32       d6, [r9], r6          @ store temp buffer 0

    vext.8        d4, d0, d1, #4
    vmla.u16      d8, d10, d22
    vext.8        d1, d0, d1, #1
    vaddl.u8      q5, d1, d4
    vld1.32       {q0}, [r0], r2        @ row 0 load
    vmls.u16      d8, d10, d23
    vext.8        d5, d0, d1, #5
    vaddl.u8      q5, d0, d5
    vext.8        d2, d0, d1, #2
    vext.8        d3, d0, d1, #3
    vaddl.u8      q6, d2, d3

    vst1.32       d8, [r9], r6          @ store temp buffer 1

    vext.8        d4, d0, d1, #4
    vmla.u16      d10, d12, d22
    vext.8        d1, d0, d1, #1
    vaddl.u8      q6, d1, d4
    vld1.32       {q0}, [r0], r2        @ row 1 load
    vmls.u16      d10, d12, d23
    vext.8        d5, d0, d1, #5
    vaddl.u8      q6, d0, d5
    vext.8        d2, d0, d1, #2
    vext.8        d3, d0, d1, #3
    vaddl.u8      q7, d2, d3

    vst1.32       d10, [r9], r6         @ store temp buffer 2

    vext.8        d4, d0, d1, #4
    vmla.u16      d12, d14, d22
    vext.8        d1, d0, d1, #1
    vaddl.u8      q7, d1, d4
    vld1.32       {q0}, [r0], r2        @ row 2 load
    vmls.u16      d12, d14, d23
    vext.8        d5, d0, d1, #5
    vaddl.u8      q7, d0, d5
    vext.8        d2, d0, d1, #2
    vext.8        d3, d0, d1, #3
    vaddl.u8      q8, d2, d3
    vext.8        d4, d0, d1, #4
    vmla.u16      d14, d16, d22
    vext.8        d1, d0, d1, #1
    vaddl.u8      q8, d1, d4

    vst1.32       d12, [r9], r6         @ store temp buffer 3

    vmls.u16      d14, d16, d23

loop_4:

    vld1.32       {q0}, [r0], r2        @ row 3 load
    vext.8        d5, d0, d1, #5
    vaddl.u8      q8, d0, d5
    vext.8        d2, d0, d1, #2
    vext.8        d3, d0, d1, #3
    vaddl.u8      q9, d2, d3
    vst1.32       d14, [r9], r6         @ store temp buffer 4
    vext.8        d4, d0, d1, #4
    vmla.u16      d16, d18, d22
    vext.8        d1, d0, d1, #1
    vaddl.u8      q9, d1, d4
    vadd.s16      d2, d10, d12
    vmls.u16      d16, d18, d23
    vadd.s16      d3, d8, d14
    vld1.32       {q9}, [r0], r2        @ row 4 load
    vext.8        d25, d18, d19, #5
    vaddl.u8      q13, d18, d25
    vext.8        d20, d18, d19, #2

    vst1.32       d16, [r9], r6         @ store temp buffer 5

    vaddl.s16     q0, d6, d16
    vmlal.s16     q0, d2, d22
    vext.8        d21, d18, d19, #3
    vaddl.u8      q14, d20, d21
    vext.8        d24, d18, d19, #4
    vmlsl.s16     q0, d3, d23
    vmla.u16      d26, d28, d22
    vext.8        d19, d18, d19, #1
    vaddl.u8      q14, d19, d24
    vadd.s16      d2, d12, d14
    vmls.u16      d26, d28, d23
    vqrshrun.s32  d0, q0, #0xa
    vadd.s16      d3, d10, d16
    vld1.32       {q9}, [r0], r2        @ row 5 load
    vext.8        d25, d18, d19, #5
    vqmovn.u16    d11, q0
    vaddl.u8      q14, d18, d25

    vst1.32       d26, [r9], r6         @ store temp buffer 6

    @Q3 available here
    vld1.32       d6, [r7], r6          @ load from temp buffer 0
    vld1.32       d7, [r7], r6          @ load from temp buffer 1
    vqrshrun.s16  d9, q3, #5

    vext.8        d20, d18, d19, #2

    vaddl.s16     q0, d8, d26
    vmlal.s16     q0, d2, d22
    vext.8        d21, d18, d19, #3
    vaddl.u8      q3, d20, d21
    vext.8        d24, d18, d19, #4
    vmlsl.s16     q0, d3, d23
    vmla.u16      d28, d6, d22
    vext.8        d19, d18, d19, #1
    vaddl.u8      q3, d19, d24
    vadd.s16      d2, d14, d16
    vmls.u16      d28, d6, d23
    vqrshrun.s32  d0, q0, #0xa
    vadd.s16      d3, d12, d26
    vld1.32       {q9}, [r0], r2        @ row 6 load
    vext.8        d25, d18, d19, #5
    vqmovn.u16    d13, q0

    vtrn.32       d11, d13
    vaddl.s16     q0, d10, d28
    vrhadd.u8     d9, d9, d11

    vst1.32       d28, [r9], r6         @ store temp buffer 7

    vmlal.s16     q0, d2, d22
    vaddl.u8      q15, d18, d25

    vst1.32       d9[0], [r1], r3       @ store row 0

    vext.8        d20, d18, d19, #2

    vst1.32       d9[1], [r1], r3       @ store row 1

    vext.8        d21, d18, d19, #3
    vmlsl.s16     q0, d3, d23
    vaddl.u8      q4, d20, d21
    vext.8        d24, d18, d19, #4
    vmla.u16      d30, d8, d22
    vext.8        d19, d18, d19, #1
    vaddl.u8      q4, d19, d24
    vqrshrun.s32  d0, q0, #0xa
    vadd.s16      d2, d16, d26
    vmls.u16      d30, d8, d23
    vqmovn.u16    d4, q0

    vadd.s16      d3, d14, d28


    vaddl.s16     q0, d12, d30

    vst1.32       d30, [r9]

    vmlal.s16     q0, d2, d22

    vld1.32       d8, [r7], r6          @ load from temp buffer 2
    vld1.32       d9, [r7], r6          @ load from temp buffer 3
    vmlsl.s16     q0, d3, d23
    subs          r4, r4, #4
    vqrshrun.s16  d10, q4, #5

    vmov          d12, d28

    vqrshrun.s32  d0, q0, #0xa
    vmov          d6, d14
    vmov          d8, d16

    vqmovn.u16    d5, q0

    vtrn.32       d4, d5
    vrhadd.u8     d4, d4, d10
    vmov          d10, d26
    vmov          d14, d30

    vst1.32       d4[0], [r1], r3       @ store row 2
    vst1.32       d4[1], [r1], r3       @ store row 3

    bgt           loop_4

end_func:
    vldmia        sp!, {d8-d15}         @ Restore neon registers that were saved
    ldmfd         sp!, {r4-r12, pc}     @Restoring registers from stack


