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
@*  ih264_inter_pred_luma_vert_qpel_a9q.s
@*
@* @brief
@*  Contains function definitions for inter prediction vertical quarter pel interpolation.
@*
@* @author
@*  Mohit
@*
@* @par List of Functions:
@*
@*  - ih264_inter_pred_luma_vert_qpel_a9q()
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@*

@* All the functions here are replicated from ih264_inter_pred_filters.c
@

@*******************************************************************************
@*
@* @brief
@*     Quarter pel interprediction luma filter for vertical input
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
@* @param[in] pu1_tmp: temporary buffer: UNUSED in this function
@*
@* @param[in] dydx: x and y reference offset for qpel calculations.
@* @returns
@*
@ @remarks
@*  None
@*
@*******************************************************************************
@*

@void ih264_inter_pred_luma_vert (
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

    .global ih264_inter_pred_luma_vert_qpel_a9q

ih264_inter_pred_luma_vert_qpel_a9q:

    stmfd         sp!, {r4-r12, r14}    @store register values to stack
    vstmdb        sp!, {d8-d15}         @push neon registers to stack
    ldr           r5, [sp, #104]        @Loads ht

    ldr           r6, [sp, #108]        @Loads wd
    ldr           r7, [sp, #116]        @Loads dydx
    and           r7, r7, #12           @Finds y-offset
    lsr           r7, r7, #3            @dydx>>3
    mul           r7, r2, r7
    add           r7, r0, r7            @pu1_src + (y_offset>>1)*src_strd
    vmov.u16      q11, #20              @ Filter coeff 0x14 into Q11
    sub           r0, r0, r2, lsl #1    @pu1_src-2*src_strd
    subs          r12, r6, #8           @if wd=8 branch to loop_8
    vmov.u16      q12, #5               @ Filter coeff 0x5  into Q12
    beq           loop_8

    subs          r12, r6, #4           @if wd=4 branch to loop_4
    beq           loop_4

loop_16:                                @when  wd=16

    vld1.u32      {q0}, [r0], r2        @ Vector load from src[0_0]
    vld1.u32      {q1}, [r0], r2        @ Vector load from src[1_0]
    vld1.u32      {q2}, [r0], r2        @ Vector load from src[2_0]
    vld1.u32      {q3}, [r0], r2        @ Vector load from src[3_0]
    vld1.u32      {q4}, [r0], r2        @ Vector load from src[4_0]
    vaddl.u8      q6, d4, d6            @ temp1 = src[2_0] + src[3_0]
    vld1.u32      {q5}, [r0], r2        @ Vector load from src[5_0]
    vaddl.u8      q7, d0, d10           @ temp = src[0_0] + src[5_0]
    vaddl.u8      q8, d2, d8            @ temp2 = src[1_0] + src[4_0]
    vmla.u16      q7, q6, q11           @ temp += temp1 * 20
    vaddl.u8      q10, d1, d11          @ temp4 = src[0_8] + src[5_8]
    vaddl.u8      q9, d5, d7            @ temp3 = src[2_8] + src[3_8]
    vmla.u16      q10, q9, q11          @ temp4 += temp3 * 20
    vld1.u32      {q0}, [r0], r2
    vaddl.u8      q13, d3, d9           @ temp5 = src[1_8] + src[4_8]
    vaddl.u8      q6, d6, d8
    vmls.u16      q7, q8, q12           @ temp -= temp2 * 5
    vaddl.u8      q8, d2, d0
    vaddl.u8      q9, d4, d10
    vmla.u16      q8, q6, q11
    vmls.u16      q10, q13, q12         @ temp4 -= temp5 * 5
    vaddl.u8      q13, d5, d11
    vaddl.u8      q6, d7, d9
    vqrshrun.s16  d30, q7, #5           @ dst[0_0] = CLIP_U8((temp +16) >> 5)
    vaddl.u8      q7, d3, d1
    vld1.u32      {q1}, [r0], r2
    vmla.u16      q7, q6, q11
    vmls.u16      q8, q9, q12
    vqrshrun.s16  d31, q10, #5          @ dst[0_8] = CLIP_U8((temp4 +16) >> 5)
    vld1.u32      {q10}, [r7], r2       @ Load for interpolation row 0
    vrhadd.u8     q15, q10, q15         @ Interpolation to obtain qpel value
    vaddl.u8      q9, d4, d2
    vaddl.u8      q6, d8, d10

    vst1.u32      {q15}, [r1], r3       @ Vector store to dst[0_0]
    vmla.u16      q9, q6, q11
    vaddl.u8      q10, d6, d0
    vmls.u16      q7, q13, q12
    vqrshrun.s16  d30, q8, #5
    vaddl.u8      q6, d9, d11
    vaddl.u8      q8, d5, d3
    vaddl.u8      q13, d7, d1
    vmla.u16      q8, q6, q11
    vmls.u16      q9, q10, q12
    vld1.u32      {q2}, [r0], r2

    vqrshrun.s16  d31, q7, #5
    vld1.u32      {q7}, [r7], r2        @ Load for interpolation row 1
    vaddl.u8      q6, d10, d0
    vrhadd.u8     q15, q7, q15          @ Interpolation to obtain qpel value
    vaddl.u8      q7, d6, d4
    vaddl.u8      q10, d8, d2
    vmla.u16      q7, q6, q11
    vmls.u16      q8, q13, q12
    vst1.u32      {q15}, [r1], r3       @store row 1
    vqrshrun.s16  d30, q9, #5
    vaddl.u8      q9, d7, d5
    vaddl.u8      q6, d11, d1
    vmla.u16      q9, q6, q11
    vaddl.u8      q13, d9, d3
    vmls.u16      q7, q10, q12
    vqrshrun.s16  d31, q8, #5
    vld1.u32      {q8}, [r7], r2        @ Load for interpolation row 2
    vmls.u16      q9, q13, q12
    vrhadd.u8     q15, q8, q15          @ Interpolation to obtain qpel value
    vaddl.u8      q6, d0, d2            @ temp1 = src[2_0] + src[3_0]
    vst1.u32      {q15}, [r1], r3       @store row 2
    vaddl.u8      q8, d10, d4           @ temp2 = src[1_0] + src[4_0]
    vaddl.u8      q10, d9, d7           @ temp4 = src[0_8] + src[5_8]
    vqrshrun.s16  d30, q7, #5
    vaddl.u8      q13, d5, d11          @ temp5 = src[1_8] + src[4_8]
    vaddl.u8      q7, d8, d6            @ temp = src[0_0] + src[5_0]
    vqrshrun.s16  d31, q9, #5
    vld1.u32      {q9}, [r7], r2        @ Load for interpolation row 3
    vmla.u16      q7, q6, q11           @ temp += temp1 * 20
    vrhadd.u8     q15, q9, q15          @ Interpolation to obtain qpel value
    vaddl.u8      q9, d1, d3            @ temp3 = src[2_8] + src[3_8]
    vst1.u32      {q15}, [r1], r3       @store row 3
    subs          r5, r5, #4            @ 4 rows processed, decrement by 4
    subne         r0, r0 , r2, lsl #2
    subne         r0, r0, r2
    beq           end_func              @ Branch if height==4

    b             loop_16               @ looping if height = 8 or 16


loop_8:

    @ Processing row0 and row1
    vld1.u32      d0, [r0], r2          @ Vector load from src[0_0]
    vld1.u32      d1, [r0], r2          @ Vector load from src[1_0]
    vld1.u32      d2, [r0], r2          @ Vector load from src[2_0]
    vld1.u32      d3, [r0], r2          @ Vector load from src[3_0]
    vld1.u32      d4, [r0], r2          @ Vector load from src[4_0]
    vld1.u32      d5, [r0], r2          @ Vector load from src[5_0]

    vaddl.u8      q3, d2, d3            @ temp1 = src[2_0] + src[3_0]
    vaddl.u8      q4, d0, d5            @ temp = src[0_0] + src[5_0]
    vaddl.u8      q5, d1, d4            @ temp2 = src[1_0] + src[4_0]
    vmla.u16      q4, q3, q11           @ temp += temp1 * 20
    vld1.u32      d6, [r0], r2
    vaddl.u8      q7, d3, d4
    vaddl.u8      q8, d1, d6
    vaddl.u8      q9, d2, d5
    vmls.u16      q4, q5, q12           @ temp -= temp2 * 5
    vmla.u16      q8, q7, q11
    vld1.u32      d7, [r0], r2
    vaddl.u8      q10, d4, d5
    vaddl.u8      q6, d2, d7
    vaddl.u8      q5, d3, d6
    vmls.u16      q8, q9, q12
    vqrshrun.s16  d26, q4, #5           @ dst[0_0] = CLIP_U8( (temp + 16) >> 5)
    vmla.u16      q6, q10, q11
    vld1.32       d8, [r7], r2          @Load value for interpolation           (row0)
    vld1.32       d9, [r7], r2          @Load value for interpolation           (row1)
    vld1.u32      d0, [r0], r2
    vaddl.u8      q7, d5, d6
    vqrshrun.s16  d27, q8, #5
    vrhadd.u8     q13, q4, q13          @ Interpolation step for qpel calculation
    vaddl.u8      q10, d3, d0
    vmls.u16      q6, q5, q12
    vst1.u32      d26, [r1], r3         @ Vector store to dst[0_0]
    vaddl.u8      q9, d4, d7
    vmla.u16      q10, q7, q11
    vst1.u32      d27, [r1], r3         @ Vector store to dst[1_0]
    vqrshrun.s16  d28, q6, #5
    vmls.u16      q10, q9, q12
    vld1.32       d12, [r7], r2         @Load value for interpolation           (row2)
    vld1.32       d13, [r7], r2         @Load value for interpolation           (row3)
    vqrshrun.s16  d29, q10, #5
    subs          r9, r5, #4
    vrhadd.u8     q14, q6, q14
    vst1.u32      d28, [r1], r3         @store row 2
    vst1.u32      d29, [r1], r3         @store row 3

    subs          r5, r5, #4            @ 4 rows processed, decrement by 4
    subne         r0, r0 , r2, lsl #2
    subne         r0, r0, r2
    beq           end_func              @ Branch if height==4
    b             loop_8                @looping if height == 8 or 16

loop_4:
@ Processing row0 and row1

    vld1.u32      d0[0], [r0], r2       @ Vector load from src[0_0]
    vld1.u32      d1[0], [r0], r2       @ Vector load from src[1_0]
    vld1.u32      d2[0], [r0], r2       @ Vector load from src[2_0]
    vld1.u32      d3[0], [r0], r2       @ Vector load from src[3_0]
    vld1.u32      d4[0], [r0], r2       @ Vector load from src[4_0]
    vld1.u32      d5[0], [r0], r2       @ Vector load from src[5_0]

    vaddl.u8      q3, d2, d3            @ temp1 = src[2_0] + src[3_0]
    vaddl.u8      q4, d0, d5            @ temp = src[0_0] + src[5_0]
    vaddl.u8      q5, d1, d4            @ temp2 = src[1_0] + src[4_0]
    vmla.u16      q4, q3, q11           @ temp += temp1 * 20
    vld1.u32      d6, [r0], r2
    vaddl.u8      q7, d3, d4
    vaddl.u8      q8, d1, d6
    vaddl.u8      q9, d2, d5
    vmls.u16      q4, q5, q12           @ temp -= temp2 * 5
    vld1.u32      d7[0], [r0], r2
    vmla.u16      q8, q7, q11
    vaddl.u8      q10, d4, d5
    vaddl.u8      q6, d2, d7
    vaddl.u8      q5, d3, d6
    vmls.u16      q8, q9, q12
    vqrshrun.s16  d26, q4, #5           @ dst[0_0] = CLIP_U8( (temp + 16) >> 5)
    vld1.u32      d8[0], [r7], r2       @Load value for interpolation - row 0
    vld1.u32      d9[0], [r7], r2       @Load value for interpolation - row 1
    vmla.u16      q6, q10, q11
    vld1.u32      d0[0], [r0], r2
    vaddl.u8      q7, d5, d6
    vqrshrun.s16  d27, q8, #5
    vaddl.u8      q10, d3, d0
    vrhadd.u8     q13, q13, q4          @Interpolation step for qpel calculation
    vmls.u16      q6, q5, q12
    vst1.u32      d26[0], [r1], r3      @ Vector store to dst[0_0]
    vaddl.u8      q9, d4, d7
    vmla.u16      q10, q7, q11
    vst1.u32      d27[0], [r1], r3      @ store row 1
    vqrshrun.s16  d28, q6, #5
    vld1.u32      d12[0], [r7], r2      @Load value for interpolation - row 2
    vld1.u32      d13[0], [r7], r2      @Load value for interpolation - row 3

    vmls.u16      q10, q9, q12
    vqrshrun.s16  d29, q10, #5
    vrhadd.u8     q14, q6, q14          @Interpolation step for qpel calculation
    vst1.u32      d28[0], [r1], r3      @store row 2
    vst1.u32      d29[0], [r1], r3      @store row 3

    subs          r5, r5, #8
    subeq         r0, r0, r2, lsl #2
    subeq         r0, r0, r2
    beq           loop_4                @ Loop if height==8

end_func:
    vldmia        sp!, {d8-d15}         @ Restore neon registers that were saved
    ldmfd         sp!, {r4-r12, pc}     @Restoring registers from stack


