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
@*  ih264_intra_pred_chroma_a9q.s
@*
@* @brief
@*  Contains function definitions for intra chroma prediction .
@*
@* @author
@*  Ittiam
@*
@* @par List of Functions:
@*
@*  - ih264_intra_pred_chroma_mode_horz_a9q()
@*  - ih264_intra_pred_chroma_8x8_mode_vert_a9q()
@*  - ih264_intra_pred_chroma_mode_dc_a9q()
@*  - ih264_intra_pred_chroma_mode_plane_a9q()
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@*

@* All the functions here are replicated from ih264_chroma_intra_pred_filters.c
@

.text
.p2align 2

    .extern ih264_gai1_intrapred_chroma_plane_coeffs1
.hidden ih264_gai1_intrapred_chroma_plane_coeffs1
    .extern ih264_gai1_intrapred_chroma_plane_coeffs2
.hidden ih264_gai1_intrapred_chroma_plane_coeffs2
scratch_chroma_intrapred_addr1:
    .long ih264_gai1_intrapred_chroma_plane_coeffs1 - scrlblc1 - 8

scratch_intrapred_chroma_plane_addr1:
    .long ih264_gai1_intrapred_chroma_plane_coeffs2 - scrlblc2 - 8
@**
@*******************************************************************************
@*
@*ih264_intra_pred_chroma_8x8_mode_dc
@*
@* @brief
@*     Perform Intra prediction for  chroma_8x8 mode:DC
@*
@* @par Description:
@*    Perform Intra prediction for  chroma_8x8 mode:DC ,described in sec 8.3.4.1
@*
@* @param[in] pu1_src
@*  UWORD8 pointer to the source containing alternate U and V samples
@*
@* @param[out] pu1_dst
@*  UWORD8 pointer to the destination with alternate U and V samples
@*
@* @param[in] src_strd
@*  integer source stride
@*
@* @param[in] dst_strd
@*  integer destination stride
@*
@** @param[in] ui_neighboravailability
@*  availability of neighbouring pixels
@*
@* @returns
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@void ih264_intra_pred_chroma_8x8_mode_dc(UWORD8 *pu1_src,
@                                        UWORD8 *pu1_dst,
@                                        WORD32 src_strd,
@                                        WORD32 dst_strd,
@                                        WORD32 ui_neighboravailability)

@**************Variables Vs Registers*****************************************
@   r0 => *pu1_src
@   r1 => *pu1_dst
@   r2 =>  src_strd
@   r3 =>  dst_strd
@   r4 =>  ui_neighboravailability

    .global ih264_intra_pred_chroma_8x8_mode_dc_a9q

ih264_intra_pred_chroma_8x8_mode_dc_a9q:

    stmfd         sp!, {r4, r14}        @store register values to stack
    ldr           r4, [sp, #8]          @r4 =>  ui_neighboravailability
    vpush         {d8-d15}

    ands          r2, r4, #0x01         @CHECKING IF LEFT_AVAILABLE ELSE BRANCHING TO ONLY TOP AVAILABLE
    beq           top_available
    ands          r2, r4, #0x04         @CHECKING IF TOP_AVAILABLE ELSE BRANCHING TO ONLY LEFT AVAILABLE
    beq           left_available

    vld1.u8       {q0}, [r0]            @BOTH LEFT AND TOP AVAILABLE
    add           r0, r0, #18
    vld1.u8       {q1}, [r0]
    vaddl.u8      q2, d1, d2
    vaddl.u8      q3, d0, d3
    vmovl.u8      q1, d3
    vmovl.u8      q0, d0

    vadd.u16      d12, d4, d5
    vadd.u16      d13, d2, d3
    vadd.u16      d15, d6, d7
    vadd.u16      d14, d0, d1

    vpadd.u32     d12, d12, d15
    vpadd.u32     d14, d13, d14
    vqrshrun.s16  d12, q6, #3
    vqrshrun.s16  d14, q7, #2
    vdup.u16      d8, d12[0]
    vdup.u16      d9, d14[0]
    vdup.u16      d10, d14[1]
    vdup.u16      d11, d12[1]
    b             str_pred

top_available:                          @ONLY TOP AVAILABLE
    ands          r2, r4, #0x04         @CHECKING TOP AVAILABILTY OR ELSE BRANCH TO NONE AVAILABLE
    beq           none_available

    add           r0, r0, #18
    vld1.u8       {q0}, [r0]
    vmovl.u8      q1, d0
    vmovl.u8      q2, d1
    vadd.u16      d0, d2, d3
    vadd.u16      d1, d4, d5
    vpaddl.u32    q0, q0
    vqrshrun.s16  d0, q0, #2
    vdup.u16      d8, d0[0]
    vdup.u16      d9, d0[2]
    vmov          q5, q4
    b             str_pred

left_available:                         @ONLY LEFT AVAILABLE
    vld1.u8       {q0}, [r0]
    vmovl.u8      q1, d0
    vmovl.u8      q2, d1
    vadd.u16      d0, d2, d3
    vadd.u16      d1, d4, d5
    vpaddl.u32    q0, q0
    vqrshrun.s16  d0, q0, #2
    vdup.u16      q5, d0[0]
    vdup.u16      q4, d0[2]
    b             str_pred

none_available:                         @NONE AVAILABLE
    vmov.u8       q4, #128
    vmov.u8       q5, #128

str_pred:
    vst1.8        {q4}, [r1], r3
    vst1.8        {q4}, [r1], r3
    vst1.8        {q4}, [r1], r3
    vst1.8        {q4}, [r1], r3
    vst1.8        {q5}, [r1], r3
    vst1.8        {q5}, [r1], r3
    vst1.8        {q5}, [r1], r3
    vst1.8        {q5}, [r1], r3

    vpop          {d8-d15}
    ldmfd         sp!, {r4, pc}         @Restoring registers from stack



@******************************************************************************


@**
@*******************************************************************************
@*
@*ih264_intra_pred_chroma_8x8_mode_horz
@*
@* @brief
@*  Perform Intra prediction for  chroma_8x8 mode:Horizontal
@*
@* @par Description:
@*   Perform Intra prediction for  chroma_8x8 mode:Horizontal ,described in sec 8.3.4.2
@*
@* @param[in] pu1_src
@* UWORD8 pointer to the source containing alternate U and V samples
@*
@* @param[out] pu1_dst
@*  UWORD8 pointer to the destination with alternate U and V samples
@*
@* @param[in] src_strd
@*  integer source stride
@*
@* @param[in] dst_strd
@*  integer destination stride
@*
@* @param[in] ui_neighboravailability
@* availability of neighbouring pixels(Not used in this function)
@*
@* @returns
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@*
@void ih264_intra_pred_chroma_8x8_mode_horz(UWORD8 *pu1_src,
@                                         UWORD8 *pu1_dst,
@                                         WORD32 src_strd,
@                                         WORD32 dst_strd,
@                                         WORD32 ui_neighboravailability)
@**************Variables Vs Registers*****************************************
@   r0 => *pu1_src
@   r1 => *pu1_dst
@   r2 =>  src_strd
@   r3 =>  dst_strd
@   r4 =>  ui_neighboravailability


    .global ih264_intra_pred_chroma_8x8_mode_horz_a9q

ih264_intra_pred_chroma_8x8_mode_horz_a9q:

    stmfd         sp!, {r14}            @store register values to stack

    vld1.u8       {q0}, [r0]
    mov           r2, #6

    vdup.u16      q1, d1[3]
    vdup.u16      q2, d1[2]
    vst1.8        {q1}, [r1], r3

loop_8x8_horz:
    vext.8        q0, q0, q0, #12
    vst1.8        {q2}, [r1], r3
    vdup.u16      q1, d1[3]
    subs          r2, #2
    vdup.u16      q2, d1[2]
    vst1.8        {q1}, [r1], r3
    bne           loop_8x8_horz

    vext.8        q0, q0, q0, #12
    vst1.8        {q2}, [r1], r3

    ldmfd         sp!, {pc}             @restoring registers from stack




@**
@*******************************************************************************
@*
@*ih264_intra_pred_chroma_8x8_mode_vert
@*
@* @brief
@*   Perform Intra prediction for  chroma_8x8 mode:vertical
@*
@* @par Description:
@*Perform Intra prediction for  chroma_8x8 mode:vertical ,described in sec 8.3.4.3
@*
@* @param[in] pu1_src
@* UWORD8 pointer to the source containing alternate U and V samples
@*
@* @param[out] pu1_dst
@*   UWORD8 pointer to the destination with alternate U and V samples
@*
@* @param[in] src_strd
@*  integer source stride
@*
@* @param[in] dst_strd
@*  integer destination stride
@*
@* @param[in] ui_neighboravailability
@* availability of neighbouring pixels(Not used in this function)
@*
@* @returns
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@void ih264_intra_pred_chroma_8x8_mode_vert(UWORD8 *pu1_src,
@                                        UWORD8 *pu1_dst,
@                                        WORD32 src_strd,
@                                        WORD32 dst_strd,
@                                        WORD32 ui_neighboravailability)

@**************Variables Vs Registers*****************************************
@   r0 => *pu1_src
@   r1 => *pu1_dst
@   r2 =>  src_strd
@   r3 =>  dst_strd
@   r4 =>  ui_neighboravailability


    .global ih264_intra_pred_chroma_8x8_mode_vert_a9q

ih264_intra_pred_chroma_8x8_mode_vert_a9q:

    stmfd         sp!, {r4-r12, r14}    @store register values to stack

    add           r0, r0, #18
    vld1.8        {q0}, [r0]

    vst1.8        {q0}, [r1], r3
    vst1.8        {q0}, [r1], r3
    vst1.8        {q0}, [r1], r3
    vst1.8        {q0}, [r1], r3
    vst1.8        {q0}, [r1], r3
    vst1.8        {q0}, [r1], r3
    vst1.8        {q0}, [r1], r3
    vst1.8        {q0}, [r1], r3

    ldmfd         sp!, {r4-r12, pc}     @Restoring registers from stack




@******************************************************************************


@**
@*******************************************************************************
@*
@*ih264_intra_pred_chroma_8x8_mode_plane
@*
@* @brief
@*   Perform Intra prediction for  chroma_8x8 mode:PLANE
@*
@* @par Description:
@*  Perform Intra prediction for  chroma_8x8 mode:PLANE ,described in sec 8.3.4.4
@*
@* @param[in] pu1_src
@*  UWORD8 pointer to the source containing alternate U and V samples
@*
@* @param[out] pu1_dst
@*  UWORD8 pointer to the destination with alternate U and V samples
@*
@* @param[in] src_strd
@*  integer source stride
@*
@* @param[in] dst_strd
@*  integer destination stride
@*
@* @param[in] ui_neighboravailability
@*  availability of neighbouring pixels
@*
@* @returns
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@void ih264_intra_pred_chroma_8x8_mode_plane(UWORD8 *pu1_src,
@                                        UWORD8 *pu1_dst,
@                                        WORD32 src_strd,
@                                        WORD32 dst_strd,
@                                        WORD32 ui_neighboravailability)

@**************Variables Vs Registers*****************************************
@   r0 => *pu1_src
@   r1 => *pu1_dst
@   r2 =>  src_strd
@   r3 =>  dst_strd
@   r4 =>  ui_neighboravailability

    .global ih264_intra_pred_chroma_8x8_mode_plane_a9q
ih264_intra_pred_chroma_8x8_mode_plane_a9q:

    stmfd         sp!, {r4-r10, r12, lr}
    vpush         {d8-d15}

    vld1.32       d0, [r0]
    add           r10, r0, #10
    vld1.32       d1, [r10]
    add           r10, r10, #6
    vrev64.16     d5, d0
    vld1.32       d2, [r10]!
    add           r10, r10, #2
    vrev64.16     d7, d2
    vld1.32       d3, [r10]
    sub           r5, r3, #8
    ldr           r12, scratch_chroma_intrapred_addr1
scrlblc1:
    add           r12, r12, pc
    vsubl.u8      q5, d5, d1
    vld1.64       {q4}, [r12]           @ Load multiplication factors 1 to 8 into D3
    vsubl.u8      q6, d3, d7
    vmul.s16      q7, q5, q4
    vmul.s16      q8, q6, q4
    vuzp.16       q7, q8

    vpadd.s16     d14, d14
    vpadd.s16     d15, d15
    vpadd.s16     d16, d16
    vpadd.s16     d17, d17
    vpadd.s16     d14, d14
    vpadd.s16     d15, d15
    vpadd.s16     d16, d16
    vpadd.s16     d17, d17

    mov           r6, #34
    vdup.16       q9, r6

    vmull.s16     q11, d14, d18
    vmull.s16     q12, d15, d18
    vmull.s16     q13, d16, d18
    vmull.s16     q14, d17, d18

    vrshrn.s32    d10, q11, #6
    vrshrn.s32    d12, q12, #6
    vrshrn.s32    d13, q13, #6
    vrshrn.s32    d14, q14, #6


    ldrb          r6, [r0], #1
    add           r10, r0, #31
    ldrb          r8, [r0], #1
    ldrb          r7, [r10], #1
    ldrb          r9, [r10], #1

    add           r6, r6, r7
    add           r8, r8, r9
    lsl           r6, r6, #4
    lsl           r8, r8, #4

    vdup.16       q0, r6
    vdup.16       q1, r8
    vdup.16       q2, d12[0]
    vdup.16       q3, d10[0]

    vdup.16       q12, d14[0]
    vdup.16       q13, d13[0]
    vzip.16       q2, q12
    vzip.16       q3, q13
    vzip.16       q0, q1

    ldr           r12, scratch_intrapred_chroma_plane_addr1
scrlblc2:
    add           r12, r12, pc
    vld1.64       {q4}, [r12]
    vmov.16       q5, q4
    vmov          q11, q4
    vzip.16       q4, q5

    vmul.s16      q6, q2, q4
    vmul.s16      q8, q2, q5
    vadd.s16      q6, q0, q6
    vadd.s16      q8, q0, q8


    vdup.16       q10, d22[0]
    vmul.s16      q2, q3, q10
    vdup.16       q15, d22[1]
    vmul.s16      q9, q3, q10
    vmul.s16      q7, q3, q15
    vmul.s16      q4, q3, q15
    vadd.s16      q12, q6, q2
    vadd.s16      q0, q8, q9
    vadd.s16      q1, q6, q7
    vqrshrun.s16  d28, q12, #5
    vadd.s16      q13, q8, q4
    vqrshrun.s16  d29, q0, #5
    vdup.16       q10, d22[2]
    vst1.8        {q14}, [r1], r3
    vqrshrun.s16  d28, q1, #5
    vqrshrun.s16  d29, q13, #5
    vmul.s16      q2, q3, q10
    vmul.s16      q9, q3, q10
    vst1.8        {q14}, [r1], r3
    vadd.s16      q12, q6, q2
    vadd.s16      q0, q8, q9
    vdup.16       q15, d22[3]
    vqrshrun.s16  d28, q12, #5
    vqrshrun.s16  d29, q0, #5
    vmul.s16      q7, q3, q15
    vmul.s16      q4, q3, q15
    vst1.8        {q14}, [r1], r3
    vadd.s16      q1, q6, q7
    vadd.s16      q13, q8, q4
    vdup.16       q10, d23[0]
    vqrshrun.s16  d28, q1, #5
    vqrshrun.s16  d29, q13, #5
    vmul.s16      q2, q3, q10
    vmul.s16      q9, q3, q10
    vst1.8        {q14}, [r1], r3
    vadd.s16      q12, q6, q2
    vadd.s16      q0, q8, q9
    vdup.16       q15, d23[1]
    vqrshrun.s16  d28, q12, #5
    vqrshrun.s16  d29, q0, #5
    vmul.s16      q7, q3, q15
    vmul.s16      q4, q3, q15
    vst1.8        {q14}, [r1], r3
    vadd.s16      q1, q6, q7
    vadd.s16      q13, q8, q4
    vdup.16       q10, d23[2]
    vqrshrun.s16  d28, q1, #5
    vqrshrun.s16  d29, q13, #5
    vmul.s16      q2, q3, q10
    vmul.s16      q9, q3, q10
    vst1.8        {q14}, [r1], r3
    vadd.s16      q12, q6, q2
    vadd.s16      q0, q8, q9
    vdup.16       q15, d23[3]
    vqrshrun.s16  d28, q12, #5
    vqrshrun.s16  d29, q0, #5
    vmul.s16      q7, q3, q15
    vmul.s16      q4, q3, q15
    vst1.8        {q14}, [r1], r3
    vadd.s16      q1, q6, q7
    vadd.s16      q13, q8, q4
    vqrshrun.s16  d28, q1, #5
    vqrshrun.s16  d29, q13, #5
    vst1.8        {q14}, [r1], r3



end_func_plane:

    vpop          {d8-d15}
    ldmfd         sp!, {r4-r10, r12, pc}




