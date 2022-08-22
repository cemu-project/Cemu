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
@*  ih264_intra_pred_luma_16x16_a9q.s
@*
@* @brief
@*  Contains function definitions for intra 16x16 Luma prediction .
@*
@* @author
@*  Ittiam
@*
@* @par List of Functions:
@*
@*  - ih264_intra_pred_luma_16x16_mode_vert_a9q()
@*  - ih264_intra_pred_luma_16x16_mode_horz_a9q()
@*  - ih264_intra_pred_luma_16x16_mode_dc_a9q()
@*  - ih264_intra_pred_luma_16x16_mode_plane_a9q()
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@*

@* All the functions here are replicated from ih264_intra_pred_filters.c
@

@**
@**
@**
@

.text
.p2align 2


    .extern ih264_gai1_intrapred_luma_plane_coeffs
.hidden ih264_gai1_intrapred_luma_plane_coeffs
scratch_intrapred_addr1:
    .long ih264_gai1_intrapred_luma_plane_coeffs - scrlbl1 - 8
@**
@*******************************************************************************
@*
@*ih264_intra_pred_luma_16x16_mode_vert
@*
@* @brief
@*   Perform Intra prediction for  luma_16x16 mode:vertical
@*
@* @par Description:
@* Perform Intra prediction for  luma_16x16 mode:Vertical ,described in sec 8.3.3.1
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
@* @param[in] ui_neighboravailability
@* availability of neighbouring pixels(Not used in this function)
@*
@* @returns
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@void ih264_intra_pred_luma_16x16_mode_vert(UWORD8 *pu1_src,
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


    .global ih264_intra_pred_luma_16x16_mode_vert_a9q

ih264_intra_pred_luma_16x16_mode_vert_a9q:

    stmfd         sp!, {r4-r12, r14}    @store register values to stack

    add           r0, r0, #17
    vld1.8        {q0}, [r0]

    vst1.8        {q0}, [r1], r3
    vst1.8        {q0}, [r1], r3
    vst1.8        {q0}, [r1], r3
    vst1.8        {q0}, [r1], r3
    vst1.8        {q0}, [r1], r3
    vst1.8        {q0}, [r1], r3
    vst1.8        {q0}, [r1], r3
    vst1.8        {q0}, [r1], r3
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
@*ih264_intra_pred_luma_16x16_mode_horz
@*
@* @brief
@*  Perform Intra prediction for  luma_16x16 mode:horizontal
@*
@* @par Description:
@*  Perform Intra prediction for  luma_16x16 mode:horizontal ,described in sec 8.3.3.2
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
@void ih264_intra_pred_luma_16x16_mode_horz(UWORD8 *pu1_src,
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

    .global ih264_intra_pred_luma_16x16_mode_horz_a9q

ih264_intra_pred_luma_16x16_mode_horz_a9q:

    stmfd         sp!, {r14}            @store register values to stack

    vld1.u8       {q0}, [r0]
    mov           r2, #14

    vdup.u8       q1, d1[7]
    vdup.u8       q2, d1[6]
    vst1.8        {q1}, [r1], r3

loop_16x16_horz:
    vext.8        q0, q0, q0, #14
    vst1.8        {q2}, [r1], r3
    vdup.u8       q1, d1[7]
    subs          r2, #2
    vdup.u8       q2, d1[6]
    vst1.8        {q1}, [r1], r3
    bne           loop_16x16_horz

    vext.8        q0, q0, q0, #14
    vst1.8        {q2}, [r1], r3

    ldmfd         sp!, {pc}             @Restoring registers from stack




@******************************************************************************


@**
@*******************************************************************************
@*
@*ih264_intra_pred_luma_16x16_mode_dc
@*
@* @brief
@*  Perform Intra prediction for  luma_16x16 mode:DC
@*
@* @par Description:
@*  Perform Intra prediction for  luma_16x16 mode:DC ,described in sec 8.3.3.3
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
@* @param[in] ui_neighboravailability
@*  availability of neighbouring pixels
@*
@* @returns
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@void ih264_intra_pred_luma_16x16_mode_dc(UWORD8 *pu1_src,
@                                       UWORD8 *pu1_dst,
@                                       WORD32 src_strd,
@                                       WORD32 dst_strd,
@                                       WORD32 ui_neighboravailability)

@**************Variables Vs Registers*****************************************
@   r0 => *pu1_src
@   r1 => *pu1_dst
@   r2 =>  src_strd
@   r3 =>  dst_strd
@   r4 =>  ui_neighboravailability

    .global ih264_intra_pred_luma_16x16_mode_dc_a9q

ih264_intra_pred_luma_16x16_mode_dc_a9q:

    stmfd         sp!, {r4, r14}        @store register values to stack
    ldr           r4, [sp, #8]          @r4 =>  ui_neighboravailability

    ands          r2, r4, #0x01         @CHECKING IF LEFT_AVAILABLE ELSE BRANCHING TO ONLY TOP AVAILABLE
    beq           top_available
    ands          r2, r4, #0x04         @CHECKING IF TOP_AVAILABLE ELSE BRANCHING TO ONLY LEFT AVAILABLE
    beq           left_available

    vld1.u8       {q0}, [r0]            @BOTH LEFT AND TOP AVAILABLE
    add           r0, r0, #17
    vpaddl.u8     q0, q0
    vld1.u8       {q1}, [r0]
    vpaddl.u8     q1, q1
    vadd.u16      q0, q0, q1
    vadd.u16      d0, d0, d1
    vpaddl.u16    d0, d0
    vpaddl.u32    d0, d0
    vqrshrun.s16  d0, q0, #5
    vdup.u8       q0, d0[0]
    b             str_pred

top_available:                          @ONLY TOP AVAILABLE
    ands          r2, r4, #0x04         @CHECKING TOP AVAILABILTY OR ELSE BRANCH TO NONE AVAILABLE
    beq           none_available

    add           r0, r0, #17
    vld1.u8       {q0}, [r0]
    vpaddl.u8     q0, q0
    vadd.u16      d0, d0, d1
    vpaddl.u16    d0, d0
    vpaddl.u32    d0, d0
    vqrshrun.s16  d0, q0, #4
    vdup.u8       q0, d0[0]
    b             str_pred

left_available:                         @ONLY LEFT AVAILABLE
    vld1.u8       {q0}, [r0]
    vpaddl.u8     q0, q0
    vadd.u16      d0, d0, d1
    vpaddl.u16    d0, d0
    vpaddl.u32    d0, d0
    vqrshrun.s16  d0, q0, #4
    vdup.u8       q0, d0[0]
    b             str_pred

none_available:                         @NONE AVAILABLE
    vmov.u8       q0, #128

str_pred:
    vst1.8        {q0}, [r1], r3
    vst1.8        {q0}, [r1], r3
    vst1.8        {q0}, [r1], r3
    vst1.8        {q0}, [r1], r3
    vst1.8        {q0}, [r1], r3
    vst1.8        {q0}, [r1], r3
    vst1.8        {q0}, [r1], r3
    vst1.8        {q0}, [r1], r3
    vst1.8        {q0}, [r1], r3
    vst1.8        {q0}, [r1], r3
    vst1.8        {q0}, [r1], r3
    vst1.8        {q0}, [r1], r3
    vst1.8        {q0}, [r1], r3
    vst1.8        {q0}, [r1], r3
    vst1.8        {q0}, [r1], r3
    vst1.8        {q0}, [r1], r3

    ldmfd         sp!, {r4, pc}         @Restoring registers from stack





@******************************************************************************


@**
@*******************************************************************************
@*
@*ih264_intra_pred_luma_16x16_mode_plane
@*
@* @brief
@*  Perform Intra prediction for  luma_16x16 mode:PLANE
@*
@* @par Description:
@*  Perform Intra prediction for  luma_16x16 mode:PLANE ,described in sec 8.3.3.4
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
@* @param[in] ui_neighboravailability
@*  availability of neighbouring pixels
@*
@* @returns
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@void ih264_intra_pred_luma_16x16_mode_plane(UWORD8 *pu1_src,
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

    .global ih264_intra_pred_luma_16x16_mode_plane_a9q
ih264_intra_pred_luma_16x16_mode_plane_a9q:

    stmfd         sp!, {r4-r10, r12, lr}

    mov           r2, r1
    add           r1, r0, #17
    add           r0, r0, #15

    mov           r8, #9
    sub           r1, r1, #1
    mov           r10, r1               @top_left
    mov           r4, #-1
    vld1.32       d2, [r1], r8
    ldr           r7, scratch_intrapred_addr1
scrlbl1:
    add           r7, r7, pc

    vld1.32       d0, [r1]
    vrev64.8      d2, d2
    vld1.32       {q3}, [r7]
    vsubl.u8      q0, d0, d2
    vmovl.u8      q8, d6
    vmul.s16      q0, q0, q8
    vmovl.u8      q9, d7

    add           r7, r0, r4, lsl #3
    sub           r0, r7, r4, lsl #1
    neg           lr, r4

    vpadd.s16     d0, d0, d1

    ldrb          r8, [r7], r4
    ldrb          r9, [r0], lr

    vpaddl.s16    d0, d0
    sub           r12, r8, r9

    ldrb          r8, [r7], r4

    vpaddl.s32    d0, d0
    ldrb          r9, [r0], lr
    sub           r8, r8, r9
    vshl.s32      d2, d0, #2
    add           r12, r12, r8, lsl #1

    vadd.s32      d0, d0, d2
    ldrb          r8, [r7], r4
    ldrb          r9, [r0], lr
    vrshr.s32     d0, d0, #6            @ i_b = D0[0]
    sub           r8, r8, r9
    ldrb          r5, [r7], r4
    add           r8, r8, r8, lsl #1

    vdup.16       q2, d0[0]
    add           r12, r12, r8
    ldrb          r9, [r0], lr
    vmul.s16      q0, q2, q8
    sub           r5, r5, r9
    vmul.s16      q1, q2, q9
    add           r12, r12, r5, lsl #2

    ldrb          r8, [r7], r4
    ldrb          r9, [r0], lr
    sub           r8, r8, r9
    ldrb          r5, [r7], r4
    add           r8, r8, r8, lsl #2
    ldrb          r6, [r0], lr
    add           r12, r12, r8
    ldrb          r8, [r7], r4
    ldrb          r9, [r0], lr

    sub           r5, r5, r6
    sub           r8, r8, r9
    add           r5, r5, r5, lsl #1
    rsb           r8, r8, r8, lsl #3
    add           r12, r12, r5, lsl #1
    ldrb          r5, [r7], r4
    ldrb          r6, [r10]             @top_left
    add           r12, r12, r8
    sub           r9, r5, r6
    ldrb          r6, [r1, #7]
    add           r12, r12, r9, lsl #3  @ i_c = r12
    add           r8, r5, r6

    add           r12, r12, r12, lsl #2
    lsl           r8, r8, #4            @ i_a = r8

    add           r12, r12, #0x20
    lsr           r12, r12, #6

    vshl.s16      q14, q2, #3
    vdup.16       q3, r12

    vdup.16       q15, r8
    vshl.s16      q13, q3, #3
    vsub.s16      q15, q15, q14
    vsub.s16      q15, q15, q13
    vadd.s16      q14, q15, q3

    mov           r0, #14
    vadd.s16      q13, q14, q0
    vadd.s16      q14, q14, q1
    vqrshrun.s16  d20, q13, #5
    vqrshrun.s16  d21, q14, #5

loop_16x16_plane:

    vadd.s16      q13, q13, q3
    vadd.s16      q14, q14, q3
    vqrshrun.s16  d22, q13, #5
    vst1.32       {q10}, [r2], r3
    vqrshrun.s16  d23, q14, #5

    vadd.s16      q13, q13, q3
    subs          r0, #2
    vadd.s16      q14, q14, q3
    vqrshrun.s16  d20, q13, #5
    vst1.32       {q11}, [r2], r3
    vqrshrun.s16  d21, q14, #5
    bne           loop_16x16_plane

    vadd.s16      q13, q13, q3
    vadd.s16      q14, q14, q3
    vqrshrun.s16  d22, q13, #5
    vst1.32       {q10}, [r2], r3
    vqrshrun.s16  d23, q14, #5
    vst1.32       {q11}, [r2], r3

    ldmfd         sp!, {r4-r10, r12, pc}



