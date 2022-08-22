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
@*  ih264_inter_pred_luma_horz_hpel_vert_hpel_a9q.s
@*
@* @brief
@*  Contains function definitions for inter prediction  interpolation.
@*
@* @author
@*  Mohit
@*
@* @par List of Functions:
@*
@*  - ih264_inter_pred_luma_horz_hpel_vert_hpel_a9q()
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
@*    applies the six tap filter in the vertical direction on the
@*    predictor values, followed by applying the same filter in the
@*    horizontal direction on the output of the first stage. The six tap
@*    filtering operation is described in sec 8.4.2.2.1 titled "Luma sample
@*    interpolation process"
@*
@* @par Description:
@*     This function is called to obtain pixels lying at the following
@*    location (1/2,1/2). The function interpolates
@*    the predictors first in the horizontal direction and then in the
@*    vertical direction to output the (1/2,1/2).
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
@* @param[in] dydx: x and y reference offset for qpel calculations: UNUSED in this function.
@*
@* @returns
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@*;

@void ih264_inter_pred_luma_horz_hpel_vert_hpel(UWORD8 *pu1_src,
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
@   r8 =>  ht
@   r9 =>  wd

.text
.p2align 2

    .global ih264_inter_pred_luma_horz_hpel_vert_hpel_a9q

ih264_inter_pred_luma_horz_hpel_vert_hpel_a9q:

    stmfd         sp!, {r4-r12, r14}    @store register values to stack
    vstmdb        sp!, {d8-d15}         @push neon registers to stack
    ldr           r8, [sp, #104]        @ loads ht
    sub           r0, r0, r2, lsl #1    @pu1_src-2*src_strd
    sub           r0, r0, #2            @pu1_src-2
    ldr           r9, [sp, #108]        @ loads wd

    vmov.s16      d0, #20               @ Filter coeff 20
    vmov.s16      d1, #5                @ Filter coeff 5
    subs          r12, r9, #4           @if wd=4 branch to loop_4
    beq           loop_4
    subs          r12, r9, #8           @if wd=8 branch to loop_8
    beq           loop_8

    mov           r10, #8
    sub           r7, r3, r10
    @when  wd=16

loop_16:
    vld1.u32      {d2, d3, d4}, [r0], r2 @ Vector load from src[0_0]
    vld1.u32      {d5, d6, d7}, [r0], r2 @ Vector load from src[1_0]
    vld1.u32      {d8, d9, d10}, [r0], r2 @ Vector load from src[2_0]
    vld1.u32      {d11, d12, d13}, [r0], r2 @ Vector load from src[3_0]
    vld1.u32      {d14, d15, d16}, [r0], r2 @ Vector load from src[4_0]
    vld1.u32      {d17, d18, d19}, [r0], r2 @ Vector load from src[5_0]

    @ vERTICAL FILTERING FOR ROW 0
    vaddl.u8      q10, d8, d11          @ temp1 = src[2_0] + src[3_0]
    vaddl.u8      q12, d2, d17          @ temp2 = src[0_0] + src[5_0]
    vaddl.u8      q11, d5, d14          @ temp = src[1_0] + src[4_0]
    vaddl.u8      q13, d3, d18          @ temp2 = src[0_0] + src[5_0]
    vmla.u16      q12, q10, d0[0]       @ temp += temp1 * 20
    vmls.s16      q12, q11, d1[0]       @ temp -= temp2 * 5
    vaddl.u8      q10, d6, d15          @ temp = src[1_0] + src[4_0]
    vaddl.u8      q11, d9, d12          @ temp3 = src[2_0] + src[3_0]
    vaddl.u8      q14, d4, d19          @ temp2 = src[0_0] + src[5_0]
    vmla.u16      q13, q11, d0[0]       @ temp4 += temp3 * 20
    vmls.s16      q13, q10, d1[0]       @ temp -= temp2 * 5
    vaddl.u8      q11, d10, d13         @ temp3 = src[2_0] + src[3_0]
    vaddl.u8      q10, d7, d16          @ temp = src[1_0] + src[4_0]
    vmla.u16      q14, q11, d0[0]       @ temp4 += temp3 * 20
    vmls.s16      q14, q10, d1[0]       @ temp -= temp2 * 5
    vext.16       q10, q12, q13, #5     @//extract a[5]                         (column1)

    @Q12,Q13,Q14  HAVE VERTICAL FILTERED VALUES
    @CASCADED FILTERING FOR ROW 0
    vext.16       q11, q12, q13, #2     @//extract a[2]                         (column1)
    vaddl.s16     q1, d20, d24          @// a0 + a5                             (column1)
    vaddl.s16     q15, d21, d25         @// a0 + a5                             (column1)
    vmlal.s16     q1, d22, d0[0]        @// a0 + a5 + 20a2                      (column1)
    vmlal.s16     q15, d23, d0[0]       @// a0 + a5 + 20a2                      (column1)
    vext.16       q11, q12, q13, #1     @//extract a[1]                         (column1)
    vext.16       q10, q12, q13, #3     @//extract a[3]                         (column1)
    vmlsl.s16     q1, d22, d1[0]        @// a0 + a5 + 20a2 + 20a3 - 5a1         (column1)
    vmlsl.s16     q15, d23, d1[0]       @// a0 + a5 + 20a2 + 20a3 - 5a1         (column1)
    vmlal.s16     q1, d20, d0[0]        @// a0 + a5 + 20a2 + 20a3               (column1)
    vmlal.s16     q15, d21, d0[0]       @// a0 + a5 + 20a2 + 20a3               (column1)
    vext.16       q11, q12, q13, #4     @//extract a[4]                         (column1)
    vext.16       q10, q13, q14, #5     @//extract a[5]                         (column2)
    vmlsl.s16     q1, d22, d1[0]        @// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4   (column1)
    vmlsl.s16     q15, d23, d1[0]       @// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4   (column1)
    vqrshrun.s32  d22, q1, #10
    vqrshrun.s32  d23, q15, #10
    vqmovun.s16   d22, q11
    vst1.u8       {d22}, [r1], r10      @//Store dest row0, column 1; (1/2,1/2)
    vext.16       q11, q13, q14, #2     @//extract a[2]                         (column2)
    vaddl.s16     q1, d20, d26          @// a0 + a5                             (column2)
    vaddl.s16     q15, d21, d27         @// a0 + a5                             (column2)
    vmlal.s16     q1, d22, d0[0]        @// a0 + a5 + 20a2                      (column2)
    vmlal.s16     q15, d23, d0[0]       @// a0 + a5 + 20a2                      (column2)
    vext.16       q10, q13, q14, #3     @//extract a[3]                         (column2)
    vext.16       q11, q13, q14, #1     @//extract a[1]                         (column2)
    vmlal.s16     q1, d20, d0[0]        @// a0 + a5 + 20a2 + 20a3               (column2)
    vmlal.s16     q15, d21, d0[0]       @// a0 + a5 + 20a2 + 20a3               (column2)
    vext.16       q10, q13, q14, #4     @//extract a[4]                         (column2)
    vmlsl.s16     q1, d22, d1[0]        @// a0 + a5 + 20a2 + 20a3 - 5a1         (column2)
    vmlsl.s16     q15, d23, d1[0]       @// a0 + a5 + 20a2 + 20a3 - 5a1         (column2)
    vmlsl.s16     q1, d20, d1[0]        @// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4   (column2)
    vmlsl.s16     q15, d21, d1[0]       @// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4   (column2)
    vqrshrun.s32  d20, q1, #10
    vqrshrun.s32  d21, q15, #10
    vld1.u32      {d2, d3, d4}, [r0], r2 @ Vector load from src[6_0]
    vqmovun.s16   d22, q10
    vst1.u8       {d22}, [r1], r7       @//Store dest row0 ,column 2; (1/2,1/2)

    @ vERTICAL FILTERING FOR ROW 1
    vaddl.u8      q10, d11, d14         @ temp1 = src[2_0] + src[3_0]
    vaddl.u8      q12, d5, d2           @ temp2 = src[0_0] + src[5_0]
    vaddl.u8      q11, d8, d17          @ temp = src[1_0] + src[4_0]
    vaddl.u8      q13, d6, d3           @ temp2 = src[0_0] + src[5_0]
    vmla.u16      q12, q10, d0[0]       @ temp += temp1 * 20
    vaddl.u8      q10, d9, d18          @ temp = src[1_0] + src[4_0]
    vmls.s16      q12, q11, d1[0]       @ temp -= temp2 * 5
    vaddl.u8      q11, d12, d15         @ temp3 = src[2_0] + src[3_0]
    vaddl.u8      q14, d7, d4           @ temp2 = src[0_0] + src[5_0]
    vmla.u16      q13, q11, d0[0]       @ temp4 += temp3 * 20
    vaddl.u8      q11, d13, d16         @ temp3 = src[2_0] + src[3_0]
    vmls.s16      q13, q10, d1[0]       @ temp -= temp2 * 5
    vmla.u16      q14, q11, d0[0]       @ temp4 += temp3 * 20
    vaddl.u8      q10, d10, d19         @ temp = src[1_0] + src[4_0]
    vmls.s16      q14, q10, d1[0]       @ temp -= temp2 * 5
    vext.16       q10, q12, q13, #5     @//extract a[5]                         (column1)

    @Q12,Q13,Q14  HAVE VERTICAL FILTERED VALUES
    @CASCADED FILTERING FOR ROW 1
    vext.16       q11, q12, q13, #2     @//extract a[2]                         (column1)
    vaddl.s16     q3, d20, d24          @// a0 + a5                             (column1)
    vaddl.s16     q15, d21, d25         @// a0 + a5                             (column1)
    vmlal.s16     q3, d22, d0[0]        @// a0 + a5 + 20a2                      (column1)
    vmlal.s16     q15, d23, d0[0]       @// a0 + a5 + 20a2                      (column1)
    vext.16       q11, q12, q13, #1     @//extract a[1]                         (column1)
    vext.16       q10, q12, q13, #3     @//extract a[3]                         (column1)
    vmlsl.s16     q3, d22, d1[0]        @// a0 + a5 + 20a2 + 20a3 - 5a1         (column1)
    vmlsl.s16     q15, d23, d1[0]       @// a0 + a5 + 20a2 + 20a3 - 5a1         (column1)
    vmlal.s16     q3, d20, d0[0]        @// a0 + a5 + 20a2 + 20a3               (column1)
    vmlal.s16     q15, d21, d0[0]       @// a0 + a5 + 20a2 + 20a3               (column1)
    vext.16       q11, q12, q13, #4     @//extract a[4]                         (column1)
    vext.16       q10, q13, q14, #5     @//extract a[5]                         (column2)
    vmlsl.s16     q3, d22, d1[0]        @// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4   (column1)
    vmlsl.s16     q15, d23, d1[0]       @// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4   (column1)
    vqrshrun.s32  d22, q3, #10
    vqrshrun.s32  d23, q15, #10
    vqmovun.s16   d22, q11
    vst1.u8       {d22}, [r1], r10      @//Store dest row1, column 1; (1/2,1/2)
    vext.16       q11, q13, q14, #2     @//extract a[2]                         (column2)
    vaddl.s16     q3, d20, d26          @// a0 + a5                             (column2)
    vaddl.s16     q15, d21, d27         @// a0 + a5                             (column2)
    vmlal.s16     q3, d22, d0[0]        @// a0 + a5 + 20a2                      (column2)
    vmlal.s16     q15, d23, d0[0]       @// a0 + a5 + 20a2                      (column2)
    vext.16       q10, q13, q14, #3     @//extract a[3]                         (column2)
    vext.16       q11, q13, q14, #1     @//extract a[1]                         (column2)
    vmlal.s16     q3, d20, d0[0]        @// a0 + a5 + 20a2 + 20a3               (column2)
    vmlal.s16     q15, d21, d0[0]       @// a0 + a5 + 20a2 + 20a3               (column2)
    vext.16       q10, q13, q14, #4     @//extract a[4]                         (column2)
    vmlsl.s16     q3, d22, d1[0]        @// a0 + a5 + 20a2 + 20a3 - 5a1         (column2)
    vmlsl.s16     q15, d23, d1[0]       @// a0 + a5 + 20a2 + 20a3 - 5a1         (column2)
    vmlsl.s16     q3, d20, d1[0]        @// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4   (column2)
    vmlsl.s16     q15, d21, d1[0]       @// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4   (column2)
    vqrshrun.s32  d20, q3, #10
    vqrshrun.s32  d21, q15, #10
    vqmovun.s16   d22, q10
    vst1.u8       {d22}, [r1], r7       @//Store dest row1 ,column 2; (1/2,1/2)

    subs          r8, r8, #2            @ 2 rows processed, decrement by 2
    subne         r0, r0 , r2, lsl #2
    subne         r0, r0, r2
    beq           end_func              @ Branch if height==4

    b             loop_16               @ looping if height = 8 or 16

loop_8:
    vld1.u32      {d2, d3}, [r0], r2    @ Vector load from src[0_0]
    vld1.u32      {d4, d5}, [r0], r2    @ Vector load from src[1_0]
    vld1.u32      {d6, d7}, [r0], r2    @ Vector load from src[2_0]
    vld1.u32      {d8, d9}, [r0], r2    @ Vector load from src[3_0]
    vld1.u32      {d10, d11}, [r0], r2  @ Vector load from src[4_0]
    vld1.u32      {d12, d13}, [r0], r2  @ Vector load from src[5_0]

    @ vERTICAL FILTERING FOR ROW 0
    vaddl.u8      q10, d6, d8           @ temp1 = src[2_0] + src[3_0]
    vaddl.u8      q11, d4, d10          @ temp2 = src[1_0] + src4_0]
    vaddl.u8      q12, d2, d12          @ temp = src[0_0] + src[5_0]
    vaddl.u8      q13, d3, d13          @ temp = src[0_0] + src[5_0]
    vaddl.u8      q14, d7, d9           @ temp1 = src[2_0] + src[3_0]
    vmla.u16      q12, q10, d0[0]       @ temp += temp1 * 20
    vmls.s16      q12, q11, d1[0]       @ temp -= temp2 * 5
    vaddl.u8      q15, d5, d11          @ temp2 = src[1_0] + src4_0]
    vmla.u16      q13, q14, d0[0]       @ temp += temp1 * 20
    vmls.s16      q13, q15, d1[0]       @ temp -= temp2 * 5
    @Q12,Q13 HAVE VERTICAL FILTERED VALUES
    @CASCADED FILTERING FOR ROW 0

    vext.16       q10, q12, q13, #5     @//extract a[5]                         (column1)
    vext.16       q11, q12, q13, #2     @//extract a[2]                         (column1)
    vaddl.s16     q14, d20, d24         @// a0 + a5                             (column1)
    vaddl.s16     q15, d21, d25         @// a0 + a5                             (column1)
    vext.16       q9, q12, q13, #1      @//extract a[1]                         (column1)
    vext.16       q10, q12, q13, #3     @//extract a[3]                         (column1)
    vext.16       q1, q12, q13, #4      @//extract a[4]                         (column1)
    vmlal.s16     q14, d22, d0[0]       @// a0 + a5 + 20a2                      (column1)
    vmlsl.s16     q14, d18, d1[0]       @// a0 + a5 + 20a2 + 20a3 - 5a1         (column1)
    vmlal.s16     q14, d20, d0[0]       @// a0 + a5 + 20a2 + 20a3               (column1)
    vmlsl.s16     q14, d2, d1[0]        @// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4   (column1)
    vld1.u32      {d14, d15}, [r0], r2  @ Vector load from src[6_0]
    vmlal.s16     q15, d23, d0[0]       @// a0 + a5 + 20a2                      (column1)
    vmlal.s16     q15, d21, d0[0]       @// a0 + a5 + 20a2 + 20a3               (column1)
    vmlsl.s16     q15, d19, d1[0]       @// a0 + a5 + 20a2 + 20a3 - 5a1         (column1)
    vmlsl.s16     q15, d3, d1[0]        @// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4   (column1)

    vaddl.u8      q12, d4, d14          @ temp = src[0_0] + src[5_0]
    vaddl.u8      q13, d5, d15          @ temp = src[0_0] + src[5_0]
    vqrshrun.s32  d18, q14, #10
    vaddl.u8      q14, d9, d11          @ temp1 = src[2_0] + src[3_0]
    vaddl.u8      q10, d8, d10          @ temp1 = src[2_0] + src[3_0]
    vaddl.u8      q11, d6, d12          @ temp2 = src[1_0] + src4_0]
    vqrshrun.s32  d19, q15, #10
    vmla.u16      q12, q10, d0[0]       @ temp += temp1 * 20
    vmls.s16      q12, q11, d1[0]       @ temp -= temp2 * 5
    vaddl.u8      q15, d7, d13          @ temp2 = src[1_0] + src4_0]
    vmla.u16      q13, q14, d0[0]       @ temp += temp1 * 20
    vmls.s16      q13, q15, d1[0]       @ temp -= temp2 * 5
    vqmovun.s16   d2, q9
    @ vERTICAL FILTERING FOR ROW 1

    @Q12,Q13 HAVE VERTICAL FILTERED VALUES
    @CASCADED FILTERING FOR ROW 1
    vext.16       q10, q12, q13, #5     @//extract a[5]                         (column1)
    vext.16       q11, q12, q13, #2     @//extract a[2]                         (column1)
    vaddl.s16     q14, d20, d24         @// a0 + a5                             (column1)
    vaddl.s16     q15, d21, d25         @// a0 + a5                             (column1)
    vst1.u8       {d2}, [r1], r3        @//Store dest row0, column 1; (1/2,1/2)
    vext.16       q9, q12, q13, #1      @//extract a[1]                         (column1)
    vext.16       q10, q12, q13, #3     @//extract a[3]                         (column1)
    vext.16       q2, q12, q13, #4      @//extract a[4]                         (column1)
    vmlal.s16     q14, d22, d0[0]       @// a0 + a5 + 20a2                      (column1)
    vmlsl.s16     q14, d18, d1[0]       @// a0 + a5 + 20a2 + 20a3 - 5a1         (column1)
    vmlal.s16     q14, d20, d0[0]       @// a0 + a5 + 20a2 + 20a3               (column1)
    vmlsl.s16     q14, d4, d1[0]        @// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4   (column1)
    vmlal.s16     q15, d23, d0[0]       @// a0 + a5 + 20a2                      (column1)
    vmlal.s16     q15, d21, d0[0]       @// a0 + a5 + 20a2 + 20a3               (column1)
    vmlsl.s16     q15, d19, d1[0]       @// a0 + a5 + 20a2 + 20a3 - 5a1         (column1)
    vmlsl.s16     q15, d5, d1[0]        @// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4   (column1)
    vqrshrun.s32  d18, q14, #10
    vqrshrun.s32  d19, q15, #10
    vqmovun.s16   d3, q9
    vst1.u8       {d3}, [r1], r3        @//Store dest row1, column 1; (1/2,1/2)

    subs          r8, r8, #2            @ 2 rows processed, decrement by 2
    subne         r0, r0 , r2, lsl #2
    subne         r0, r0, r2
    beq           end_func              @ Branch if height==4

    b             loop_8                @looping if height == 8 or 16

loop_4:
    vld1.u32      {d2, d3}, [r0], r2    @ Vector load from src[0_0]
    vld1.u32      {d4, d5}, [r0], r2    @ Vector load from src[1_0]
    vld1.u32      {d6, d7}, [r0], r2    @ Vector load from src[2_0]
    vld1.u32      {d8, d9}, [r0], r2    @ Vector load from src[3_0]
    vld1.u32      {d10, d11}, [r0], r2  @ Vector load from src[4_0]
    vld1.u32      {d12, d13}, [r0], r2  @ Vector load from src[5_0]

    @ vERTICAL FILTERING FOR ROW 0
    vaddl.u8      q10, d6, d8           @ temp1 = src[2_0] + src[3_0]
    vaddl.u8      q11, d4, d10          @ temp2 = src[1_0] + src4_0]
    vaddl.u8      q12, d2, d12          @ temp = src[0_0] + src[5_0]
    vaddl.u8      q13, d3, d13          @ temp = src[0_0] + src[5_0]
    vaddl.u8      q14, d7, d9           @ temp1 = src[2_0] + src[3_0]
    vmla.u16      q12, q10, d0[0]       @ temp += temp1 * 20
    vmls.s16      q12, q11, d1[0]       @ temp -= temp2 * 5
    vaddl.u8      q15, d5, d11          @ temp2 = src[1_0] + src4_0]
    vmla.u16      q13, q14, d0[0]       @ temp += temp1 * 20
    vmls.s16      q13, q15, d1[0]       @ temp -= temp2 * 5
    @Q12,Q13 HAVE VERTICAL FILTERED VALUES
    @CASCADED FILTERING FOR ROW 0

    vext.16       q10, q12, q13, #5     @//extract a[5]                         (column1)
    vext.16       q11, q12, q13, #2     @//extract a[2]                         (column1)
    vaddl.s16     q14, d20, d24         @// a0 + a5                             (column1)
    vaddl.s16     q15, d21, d25         @// a0 + a5                             (column1)

    vext.16       q1, q12, q13, #4      @//extract a[4]                         (column1)
    vext.16       q9, q12, q13, #1      @//extract a[1]                         (column1)
    vext.16       q10, q12, q13, #3     @//extract a[3]                         (column1)

    vmlal.s16     q14, d22, d0[0]       @// a0 + a5 + 20a2                      (column1)
    vmlsl.s16     q14, d18, d1[0]       @// a0 + a5 + 20a2 + 20a3 - 5a1         (column1)
    vmlal.s16     q14, d20, d0[0]       @// a0 + a5 + 20a2 + 20a3               (column1)
    vmlsl.s16     q14, d2, d1[0]        @// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4   (column1)
    vld1.u32      {d14, d15}, [r0], r2  @ Vector load from src[6_0]
    vmlal.s16     q15, d23, d0[0]       @// a0 + a5 + 20a2                      (column1)
    vmlal.s16     q15, d21, d0[0]       @// a0 + a5 + 20a2 + 20a3               (column1)
    vmlsl.s16     q15, d19, d1[0]       @// a0 + a5 + 20a2 + 20a3 - 5a1         (column1)
    vmlsl.s16     q15, d3, d1[0]        @// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4   (column1)
    vaddl.u8      q12, d4, d14          @ temp = src[0_0] + src[5_0]
    vaddl.u8      q13, d5, d15          @ temp = src[0_0] + src[5_0]
    vqrshrun.s32  d18, q14, #10
    vaddl.u8      q14, d9, d11          @ temp1 = src[2_0] + src[3_0]
    vaddl.u8      q11, d6, d12          @ temp2 = src[1_0] + src4_0]
    vaddl.u8      q10, d8, d10          @ temp1 = src[2_0] + src[3_0]
    vqrshrun.s32  d19, q15, #10
    vmla.u16      q12, q10, d0[0]       @ temp += temp1 * 20
    vmls.s16      q12, q11, d1[0]       @ temp -= temp2 * 5
    vaddl.u8      q15, d7, d13          @ temp2 = src[1_0] + src4_0]
    vqmovun.s16   d2, q9
    vmla.u16      q13, q14, d0[0]       @ temp += temp1 * 20
    vmls.s16      q13, q15, d1[0]       @ temp -= temp2 * 5

    @ vERTICAL FILTERING FOR ROW 1

    @Q12,Q13 HAVE VERTICAL FILTERED VALUES
    @CASCADED FILTERING FOR ROW 1
    vext.16       q10, q12, q13, #5     @//extract a[5]                         (column1)
    vext.16       q11, q12, q13, #2     @//extract a[2]                         (column1)
    vst1.u32      {d2[0]}, [r1], r3     @//Store dest row0, column 1; (1/2,1/2)
    vaddl.s16     q14, d20, d24         @// a0 + a5                             (column1)
    vaddl.s16     q15, d21, d25         @// a0 + a5                             (column1)
    vext.16       q9, q12, q13, #1      @//extract a[1]                         (column1)
    vext.16       q10, q12, q13, #3     @//extract a[3]                         (column1)
    vext.16       q2, q12, q13, #4      @//extract a[4]                         (column1)
    vmlal.s16     q14, d22, d0[0]       @// a0 + a5 + 20a2                      (column1)
    vmlsl.s16     q14, d18, d1[0]       @// a0 + a5 + 20a2 + 20a3 - 5a1         (column1)
    vmlal.s16     q14, d20, d0[0]       @// a0 + a5 + 20a2 + 20a3               (column1)
    vmlsl.s16     q14, d4, d1[0]        @// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4   (column1)
    vmlal.s16     q15, d23, d0[0]       @// a0 + a5 + 20a2                      (column1)
    vmlal.s16     q15, d21, d0[0]       @// a0 + a5 + 20a2 + 20a3               (column1)
    vmlsl.s16     q15, d19, d1[0]       @// a0 + a5 + 20a2 + 20a3 - 5a1         (column1)
    vmlsl.s16     q15, d5, d1[0]        @// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4   (column1)
    vqrshrun.s32  d18, q14, #10
    vqrshrun.s32  d19, q15, #10
    vqmovun.s16   d4, q9
    vst1.u32      {d4[0]}, [r1], r3     @//Store dest row1, column 1; (1/2,1/2)

    subs          r8, r8, #2            @ 2 rows processed, decrement by 2
    subne         r0, r0 , r2, lsl #2
    subne         r0, r0, r2
    beq           end_func              @ Branch if height==4

    b             loop_4                @looping if height == 8 or 16

end_func:
    vldmia        sp!, {d8-d15}         @ Restore neon registers that were saved
    ldmfd         sp!, {r4-r12, pc}     @Restoring registers from stack


