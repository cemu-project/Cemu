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
@*  ih264_intra_pred_luma_8x8_a9q.s
@*
@* @brief
@*  Contains function definitions for intra 8x8 Luma prediction .
@*
@* @author
@*  Ittiam
@*
@* @par List of Functions:
@*
@*  -ih264_intra_pred_luma_8x8_mode_ref_filtering_a9q
@*  -ih264_intra_pred_luma_8x8_mode_vert_a9q
@*  -ih264_intra_pred_luma_8x8_mode_horz_a9q
@*  -ih264_intra_pred_luma_8x8_mode_dc_a9q
@*  -ih264_intra_pred_luma_8x8_mode_diag_dl_a9q
@*  -ih264_intra_pred_luma_8x8_mode_diag_dr_a9q
@*  -ih264_intra_pred_luma_8x8_mode_vert_r_a9q
@*  -ih264_intra_pred_luma_8x8_mode_horz_d_a9q
@*  -ih264_intra_pred_luma_8x8_mode_vert_l_a9q
@*  -ih264_intra_pred_luma_8x8_mode_horz_u_a9q
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@*

@* All the functions here are replicated from ih264_intra_pred_filters.c
@

.text
.p2align 2

    .extern ih264_gai1_intrapred_luma_8x8_horz_u
.hidden ih264_gai1_intrapred_luma_8x8_horz_u
scratch_intrapred_addr_8x8:
    .long ih264_gai1_intrapred_luma_8x8_horz_u -  scrlb8x8l2 - 8

@**
@*******************************************************************************
@*
@*ih264_intra_pred_luma_8x8_mode_ref_filtering
@*
@* @brief
@* Reference sample filtering process for Intra_8x8 sample prediction
@*
@* @par Description:
@*  Perform Reference sample filtering process for Intra_8x8 sample prediction ,described in sec 8.3.2.2.1
@*
@* @param[in] pu1_src
@*  UWORD8 pointer to the source
@*
@* @param[out] pu1_dst
@*  UWORD8 pointer to the destination
@*
@* @param[in] src_strd
@*  integer source stride [Not used]
@*
@* @param[in] dst_strd
@*  integer destination stride[Not used]
@*
@* @param[in] ui_neighboravailability
@*  availability of neighbouring pixels[Not used]
@*
@* @returns
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@void ih264_intra_pred_luma_8x8_mode_ref_filtering(UWORD8 *pu1_src,
@                                                 UWORD8 *pu1_dst)

@**************Variables Vs Registers*****************************************
@   r0 => *pu1_src
@   r1 => *pu1_dst


    .global ih264_intra_pred_luma_8x8_mode_ref_filtering_a9q

ih264_intra_pred_luma_8x8_mode_ref_filtering_a9q:

    stmfd         sp!, {r4-r12, r14}    @store register values to stack
    vpush         {d8-d15}

    vld1.u8       {q0}, [r0]!           @
    vld1.u8       {q1}, [r0]
    add           r0, r0, #8            @
    vext.8        q2, q0, q1, #1
    vext.8        q3, q1, q1, #1
    vext.8        q4, q2, q3, #1
    vext.8        q5, q3, q3, #1
    vld1.8        {d10[7]}, [r0]        @ LOADING SRC[24] AGIN TO THE END FOR p'[ 15, -1 ] = ( p[ 14, -1 ] + 3 * p[ 15, -1 ] + 2 ) >> 2
    vaddl.u8      q10, d0, d4
    vaddl.u8      q7, d0, d0            @    SPECIAL CASE FOR p'[ -1 ,7 ] = ( p[ -1, 6 ] + 3 * p[ -1, 7 ] + 2 ) >> 2
    vadd.u16      q7, q10, q7
    vaddl.u8      q11, d1, d5
    vqrshrun.s16  d14, q7, #2
    vaddl.u8      q12, d4, d8
    vaddl.u8      q13, d5, d9
    vst1.8        {d14[0]}, [r1]!
    vadd.u16      q12, q10, q12
    vadd.u16      q13, q11, q13
    vaddl.u8      q9, d2, d6
    vaddl.u8      q8, d6, d10
    vqrshrun.s16  d4, q12, #2
    vqrshrun.s16  d5, q13, #2
    vadd.u16      q6, q8, q9
    vst1.8        {q2}, [r1]!
    vqrshrun.s16  d6, q6, #2
    vst1.8        {d6}, [r1]


end_func_ref_filt:

    vpop          {d8-d15}
    ldmfd         sp!, {r4-r12, pc}     @Restoring registers from stack






@**
@*******************************************************************************
@*
@*ih264_intra_pred_luma_8x8_mode_vert
@*
@* @brief
@*   Perform Intra prediction for  luma_8x8 mode:vertical
@*
@* @par Description:
@* Perform Intra prediction for  luma_8x8 mode:vertical ,described in sec 8.3.2.2.2
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
@void ih264_intra_pred_luma_8x8_mode_vert(UWORD8 *pu1_src,
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


    .global ih264_intra_pred_luma_8x8_mode_vert_a9q

ih264_intra_pred_luma_8x8_mode_vert_a9q:

    stmfd         sp!, {r4-r12, r14}    @store register values to stack

    add           r0, r0, #9
    vld1.8        d0, [r0]

    vst1.8        d0, [r1], r3
    vst1.8        d0, [r1], r3
    vst1.8        d0, [r1], r3
    vst1.8        d0, [r1], r3
    vst1.8        d0, [r1], r3
    vst1.8        d0, [r1], r3
    vst1.8        d0, [r1], r3
    vst1.8        d0, [r1], r3

    ldmfd         sp!, {r4-r12, pc}     @Restoring registers from stack





@******************************************************************************


@**
@*******************************************************************************
@*
@*ih264_intra_pred_luma_8x8_mode_horz
@*
@* @brief
@*  Perform Intra prediction for  luma_8x8 mode:horizontal
@*
@* @par Description:
@*  Perform Intra prediction for  luma_8x8 mode:horizontal ,described in sec 8.3.2.2.2
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
@void ih264_intra_pred_luma_8x8_mode_horz(UWORD8 *pu1_src,
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


    .global ih264_intra_pred_luma_8x8_mode_horz_a9q

ih264_intra_pred_luma_8x8_mode_horz_a9q:

    stmfd         sp!, {r14}            @store register values to stack

    vld1.u8       {d0}, [r0]
    mov           r2, #6

    vdup.u8       d1, d0[7]
    vdup.u8       d2, d0[6]
    vst1.8        {d1}, [r1], r3

loop_8x8_horz:
    vext.8        d0, d0, d0, #6
    vst1.8        {d2}, [r1], r3
    vdup.u8       d1, d0[7]
    subs          r2, #2
    vdup.u8       d2, d0[6]
    vst1.8        {d1}, [r1], r3
    bne           loop_8x8_horz

    vext.8        d0, d0, d0, #6
    vst1.8        {d2}, [r1], r3

    ldmfd         sp!, {pc}             @restoring registers from stack





@******************************************************************************


@**
@*******************************************************************************
@*
@*ih264_intra_pred_luma_8x8_mode_dc
@*
@* @brief
@*  Perform Intra prediction for  luma_8x8 mode:DC
@*
@* @par Description:
@*  Perform Intra prediction for  luma_8x8 mode:DC ,described in sec 8.3.2.2.3
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
@void ih264_intra_pred_luma_8x8_mode_dc(UWORD8 *pu1_src,
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


    .global ih264_intra_pred_luma_8x8_mode_dc_a9q

ih264_intra_pred_luma_8x8_mode_dc_a9q:

    stmfd         sp!, {r4, r14}        @store register values to stack
    ldr           r4, [sp, #8]          @r4 =>  ui_neighboravailability

    ands          r2, r4, #0x01         @CHECKING IF LEFT_AVAILABLE ELSE BRANCHING TO ONLY TOP AVAILABLE
    beq           top_available
    ands          r2, r4, #0x04         @CHECKING IF TOP_AVAILABLE ELSE BRANCHING TO ONLY LEFT AVAILABLE
    beq           left_available

    vld1.u8       {d0}, [r0]            @BOTH LEFT AND TOP AVAILABLE
    add           r0, r0, #9
    vld1.u8       {d1}, [r0]
    vpaddl.u8     q0, q0
    vadd.u16      d0, d0, d1
    vpaddl.u16    d0, d0
    vpaddl.u32    d0, d0
    vqrshrun.s16  d0, q0, #4
    vdup.u8       d0, d0[0]
    b             str_pred

top_available:                          @ONLY TOP AVAILABLE
    ands          r2, r4, #0x04         @CHECKING TOP AVAILABILTY OR ELSE BRANCH TO NONE AVAILABLE
    beq           none_available

    add           r0, r0, #9
    vld1.u8       {d0}, [r0]
    vpaddl.u8     d0, d0
    vpaddl.u16    d0, d0
    vpaddl.u32    d0, d0
    vqrshrun.s16  d0, q0, #3
    vdup.u8       d0, d0[0]
    b             str_pred

left_available:                         @ONLY LEFT AVAILABLE
    vld1.u8       {d0}, [r0]
    vpaddl.u8     d0, d0
    vpaddl.u16    d0, d0
    vpaddl.u32    d0, d0
    vqrshrun.s16  d0, q0, #3
    vdup.u8       d0, d0[0]
    b             str_pred

none_available:                         @NONE AVAILABLE
    vmov.u8       q0, #128

str_pred:
    vst1.8        {d0}, [r1], r3
    vst1.8        {d0}, [r1], r3
    vst1.8        {d0}, [r1], r3
    vst1.8        {d0}, [r1], r3
    vst1.8        {d0}, [r1], r3
    vst1.8        {d0}, [r1], r3
    vst1.8        {d0}, [r1], r3
    vst1.8        {d0}, [r1], r3

    ldmfd         sp!, {r4, pc}         @Restoring registers from stack






@**
@*******************************************************************************
@*
@*ih264_intra_pred_luma_8x8_mode_diag_dl
@*
@* @brief
@*  Perform Intra prediction for  luma_8x8 mode:Diagonal_Down_Left
@*
@* @par Description:
@*  Perform Intra prediction for  luma_8x8 mode:Diagonal_Down_Left ,described in sec 8.3.2.2.4
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
@void ih264_intra_pred_luma_8x8_mode_diag_dl(UWORD8 *pu1_src,
@                                            UWORD8 *pu1_dst,
@                                            WORD32 src_strd,
@                                            WORD32 dst_strd,
@                                            WORD32 ui_neighboravailability)

@**************Variables Vs Registers*****************************************
@   r0 => *pu1_src
@   r1 => *pu1_dst
@   r2 =>  src_strd
@   r3 =>  dst_strd
@   r4 =>  ui_neighboravailability

    .global ih264_intra_pred_luma_8x8_mode_diag_dl_a9q

ih264_intra_pred_luma_8x8_mode_diag_dl_a9q:

    stmfd         sp!, {r4-r12, r14}    @store register values to stack

    add           r0, r0, #9
    sub           r5, r3, #4
    add           r6, r0, #15
    vld1.8        {q0}, [r0]
    vext.8        q2, q0, q0, #2
    vext.8        q1, q0, q0, #1
    vld1.8        {d5[6]}, [r6]
    @ q1 = q0 shifted to left once
    @ q2 = q1 shifted to left once
    vaddl.u8      q10, d0, d2           @Adding for FILT121
    vaddl.u8      q11, d1, d3
    vaddl.u8      q12, d2, d4
    vaddl.u8      q13, d3, d5
    vadd.u16      q12, q10, q12
    vadd.u16      q13, q11, q13

    vqrshrun.s16  d4, q12, #2
    vqrshrun.s16  d5, q13, #2
    @Q2 has all FILT121 values
    vst1.8        {d4}, [r1], r3
    vext.8        q9, q2, q2, #1
    vext.8        q8, q9, q9, #1
    vst1.8        {d18}, [r1], r3
    vext.8        q15, q8, q8, #1
    vst1.8        {d16}, [r1], r3
    vst1.8        {d30}, [r1], r3
    vst1.32       {d4[1]}, [r1]!
    vst1.32       {d5[0]}, [r1], r5
    vst1.32       {d18[1]}, [r1]!
    vst1.32       {d19[0]}, [r1], r5
    vst1.32       {d16[1]}, [r1]!
    vst1.32       {d17[0]}, [r1], r5
    vst1.32       {d30[1]}, [r1]!
    vst1.32       {d31[0]}, [r1], r5


end_func_diag_dl:
    ldmfd         sp!, {r4-r12, pc}     @Restoring registers from stack




@**
@*******************************************************************************
@*
@*ih264_intra_pred_luma_8x8_mode_diag_dr
@*
@* @brief
@* Perform Intra prediction for  luma_8x8 mode:Diagonal_Down_Right
@*
@* @par Description:
@*  Perform Intra prediction for  luma_8x8 mode:Diagonal_Down_Right ,described in sec 8.3.2.2.5
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
@void ih264_intra_pred_luma_8x8_mode_diag_dr(UWORD8 *pu1_src,
@                                            UWORD8 *pu1_dst,
@                                            WORD32 src_strd,
@                                            WORD32 dst_strd,
@                                            WORD32 ui_neighboravailability)

@**************Variables Vs Registers*****************************************
@   r0 => *pu1_src
@   r1 => *pu1_dst
@   r2 =>  src_strd
@   r3 =>  dst_strd
@   r4 =>  ui_neighboravailability


    .global ih264_intra_pred_luma_8x8_mode_diag_dr_a9q

ih264_intra_pred_luma_8x8_mode_diag_dr_a9q:

    stmfd         sp!, {r4-r12, r14}    @store register values to stack


    vld1.u8       {q0}, [r0]
    add           r0, r0, #1
    vld1.u8       {q1}, [r0]
    vext.8        q2, q1, q1, #1
    @ q1 = q0 shifted to left once
    @ q2 = q1 shifted to left once
    vaddl.u8      q10, d0, d2           @Adding for FILT121
    vaddl.u8      q11, d1, d3
    vaddl.u8      q12, d2, d4
    vaddl.u8      q13, d3, d5
    vadd.u16      q12, q10, q12
    vadd.u16      q13, q11, q13
    vqrshrun.s16  d4, q12, #2
    vqrshrun.s16  d5, q13, #2
    @Q2 has all FILT121 values
    sub           r5, r3, #4
    vext.8        q9, q2, q2, #15
    vst1.8        {d19}, [r1], r3
    vext.8        q8, q9, q9, #15
    vst1.8        {d17}, [r1], r3
    vext.8        q15, q8, q8, #15
    vst1.8        {d31}, [r1], r3
    vst1.32       {d4[1]}, [r1]!
    vst1.32       {d5[0]}, [r1], r5
    vst1.32       {d18[1]}, [r1]!
    vst1.32       {d19[0]}, [r1], r5
    vst1.32       {d16[1]}, [r1]!
    vst1.32       {d17[0]}, [r1], r5
    vst1.32       {d30[1]}, [r1]!
    vst1.32       {d31[0]}, [r1], r5
    vst1.8        {d4}, [r1], r3

end_func_diag_dr:
    ldmfd         sp!, {r4-r12, pc}     @Restoring registers from stack




@**
@*******************************************************************************
@*
@*ih264_intra_pred_luma_8x8_mode_vert_r
@*
@* @brief
@* Perform Intra prediction for  luma_8x8 mode:Vertical_Right
@*
@* @par Description:
@*   Perform Intra prediction for  luma_8x8 mode:Vertical_Right ,described in sec 8.3.2.2.6
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
@void ih264_intra_pred_luma_8x8_mode_vert_r(UWORD8 *pu1_src,
@                                            UWORD8 *pu1_dst,
@                                            WORD32 src_strd,
@                                            WORD32 dst_strd,
@                                            WORD32 ui_neighboravailability)

@**************Variables Vs Registers*****************************************
@   r0 => *pu1_src
@   r1 => *pu1_dst
@   r2 =>  src_strd
@   r3 =>  dst_strd
@   r4 =>  ui_neighboravailability


    .global ih264_intra_pred_luma_8x8_mode_vert_r_a9q

ih264_intra_pred_luma_8x8_mode_vert_r_a9q:

    stmfd         sp!, {r4-r12, r14}    @store register values to stack

    vld1.u8       {q0}, [r0]
    add           r0, r0, #1
    vld1.u8       {q1}, [r0]
    vext.8        q2, q1, q1, #1
    @ q1 = q0 shifted to left once
    @ q2 = q1 shifted to left once
    vaddl.u8      q10, d0, d2
    vaddl.u8      q11, d1, d3
    vaddl.u8      q12, d2, d4
    vaddl.u8      q13, d3, d5
    vadd.u16      q12, q10, q12
    vadd.u16      q13, q11, q13

    vqrshrun.s16  d4, q10, #1
    vqrshrun.s16  d5, q11, #1
    vqrshrun.s16  d6, q12, #2
    vqrshrun.s16  d7, q13, #2
    @Q2 has all FILT11 values
    @Q3 has all FILT121 values
    sub           r5, r3, #6
    sub           r6, r3, #4
    vst1.8        {d5}, [r1], r3        @ row 0
    vext.8        q9, q3, q3, #15
    vmov.8        q11, q9
    vext.8        q8, q2, q2, #1
    vst1.8        {d19}, [r1], r3       @row 1

    vmov.8        q15, q8
    vext.8        q10, q2, q2, #15
    vuzp.8        q8, q9
    @row 2
    vext.8        q14, q8, q8, #1
    vst1.8        {d21}, [r1]
    vst1.8        {d6[6]}, [r1], r3
    @row 3

    vst1.16       {d29[1]}, [r1]!
    vst1.32       {d7[0]}, [r1]!
    vst1.16       {d7[2]}, [r1], r5
@row 4
    vst1.16       {d19[1]}, [r1]!
    vst1.32       {d5[0]}, [r1]!
    vst1.16       {d5[2]}, [r1], r5

@row 5
    vext.8        q13, q9, q9, #1
    vst1.16       {d17[1]}, [r1]!
    vst1.32       {d23[0]}, [r1]!
    vst1.16       {d23[2]}, [r1], r5


@row 6
    vst1.16       {d27[0]}, [r1]!
    vst1.8        {d27[2]}, [r1]!
    vst1.8        {d5[0]}, [r1]!
    vst1.32       {d31[0]}, [r1], r6
@row 7
    vst1.32       {d29[0]}, [r1]!
    vst1.32       {d7[0]}, [r1]!



end_func_vert_r:
    ldmfd         sp!, {r4-r12, pc}     @Restoring registers from stack




@**
@*******************************************************************************
@*
@*ih264_intra_pred_luma_8x8_mode_horz_d
@*
@* @brief
@* Perform Intra prediction for  luma_8x8 mode:Horizontal_Down
@*
@* @par Description:
@*   Perform Intra prediction for  luma_8x8 mode:Horizontal_Down ,described in sec 8.3.2.2.7
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
@void ih264_intra_pred_luma_8x8_mode_horz_d(UWORD8 *pu1_src,
@                                            UWORD8 *pu1_dst,
@                                            WORD32 src_strd,
@                                            WORD32 dst_strd,
@                                            WORD32 ui_neighboravailability)

@**************Variables Vs Registers*****************************************
@   r0 => *pu1_src
@   r1 => *pu1_dst
@   r2 =>  src_strd
@   r3 =>  dst_strd
@   r4 =>  ui_neighboravailability

    .global ih264_intra_pred_luma_8x8_mode_horz_d_a9q

ih264_intra_pred_luma_8x8_mode_horz_d_a9q:

    stmfd         sp!, {r4-r12, r14}    @store register values to stack
    vpush         {d8-d15}

    vld1.u8       {q0}, [r0]
    add           r0, r0, #1
    vld1.u8       {q1}, [r0]
    vext.8        q2, q1, q1, #1
    @ q1 = q0 shifted to left once
    @ q2 = q1 shifted to left once
    vaddl.u8      q10, d0, d2
    vaddl.u8      q11, d1, d3
    vaddl.u8      q12, d2, d4
    vaddl.u8      q13, d3, d5
    vadd.u16      q12, q10, q12
    vadd.u16      q13, q11, q13

    vqrshrun.s16  d4, q10, #1
    vqrshrun.s16  d5, q11, #1
    vqrshrun.s16  d6, q12, #2
    vqrshrun.s16  d7, q13, #2
    @Q2 has all FILT11 values
    @Q3 has all FILT121 values
    vmov.8        q4, q2
    vmov.8        q5, q3
    sub           r6, r3, #6
    vtrn.8        q4, q5                @
    vmov.8        q6, q4
    vmov.8        q7, q5
    sub           r5, r3, #4
    vtrn.16       q6, q7
    vext.8        q8, q3, q3, #14
    @ROW 0
    vst1.8        {d17}, [r1]
    vst1.16       {d10[3]}, [r1], r3

    @ROW 1
    vst1.32       {d14[1]}, [r1]!
    vst1.32       {d7[0]}, [r1], r5
    @ROW 2
    vst1.16       {d10[2]}, [r1]!
    vst1.32       {d14[1]}, [r1]!
    vst1.16       {d7[0]}, [r1], r6
    @ROW 3
    vst1.32       {d12[1]}, [r1]!
    vst1.32       {d14[1]}, [r1], r5
    @ROW 4
    vst1.16       {d14[1]}, [r1]!
    vst1.32       {d12[1]}, [r1]!
    vst1.16       {d14[2]}, [r1], r6
    @ROW 5
    vst1.32       {d14[0]}, [r1]!
    vst1.32       {d12[1]}, [r1], r5
    @ROW 6
    vst1.16       {d10[0]}, [r1]!
    vst1.16       {d8[1]}, [r1]!
    vst1.16       {d14[1]}, [r1]!
    vst1.16       {d12[2]}, [r1], r6
    @ROW 7
    vst1.32       {d12[0]}, [r1]!
    vst1.32       {d14[0]}, [r1], r5

end_func_horz_d:
    vpop          {d8-d15}
    ldmfd         sp!, {r4-r12, pc}     @Restoring registers from stack





@**
@*******************************************************************************
@*
@*ih264_intra_pred_luma_8x8_mode_vert_l
@*
@* @brief
@*  Perform Intra prediction for  luma_8x8 mode:Vertical_Left
@*
@* @par Description:
@*   Perform Intra prediction for  luma_8x8 mode:Vertical_Left ,described in sec 8.3.2.2.8
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
@void ih264_intra_pred_luma_8x8_mode_vert_l(UWORD8 *pu1_src,
@                                            UWORD8 *pu1_dst,
@                                            WORD32 src_strd,
@                                            WORD32 dst_strd,
@                                            WORD32 ui_neighboravailability)

@**************Variables Vs Registers*****************************************
@   r0 => *pu1_src
@   r1 => *pu1_dst
@   r2 =>  src_strd
@   r3 =>  dst_strd
@   r4 =>  ui_neighboravailability


    .global ih264_intra_pred_luma_8x8_mode_vert_l_a9q

ih264_intra_pred_luma_8x8_mode_vert_l_a9q:

    stmfd         sp!, {r4-r12, r14}    @Restoring registers from stack
    vpush         {d8-d15}

    add           r0, r0, #9
    vld1.u8       {q0}, [r0]
    add           r0, r0, #1
    vld1.u8       {q1}, [r0]
    vext.8        q2, q1, q1, #1
    vaddl.u8      q10, d0, d2
    vaddl.u8      q11, d1, d3
    vaddl.u8      q12, d2, d4
    vaddl.u8      q13, d3, d5
    vadd.u16      q12, q10, q12
    vadd.u16      q13, q11, q13

    vqrshrun.s16  d4, q10, #1
    vqrshrun.s16  d5, q11, #1
    vqrshrun.s16  d6, q12, #2
    vext.8        q4, q2, q2, #1
    vqrshrun.s16  d7, q13, #2
    @Q2 has all FILT11 values
    @Q3 has all FILT121 values

    vext.8        q5, q3, q3, #1
    @ROW 0,1
    vst1.8        {d4}, [r1], r3
    vst1.8        {d6}, [r1], r3

    vext.8        q6, q4, q4, #1
    vext.8        q7, q5, q5, #1
    @ROW 2,3
    vst1.8        {d8}, [r1], r3
    vst1.8        {d10}, [r1], r3

    vext.8        q8, q6, q6, #1
    vext.8        q9, q7, q7, #1
    @ROW 4,5
    vst1.8        {d12}, [r1], r3
    vst1.8        {d14}, [r1], r3
    @ROW 6,7
    vst1.8        {d16}, [r1], r3
    vst1.8        {d18}, [r1], r3

end_func_vert_l:
    vpop          {d8-d15}
    ldmfd         sp!, {r4-r12, pc}     @Restoring registers from stack





@**
@*******************************************************************************
@*
@*ih264_intra_pred_luma_8x8_mode_horz_u
@*
@* @brief
@*     Perform Intra prediction for  luma_8x8 mode:Horizontal_Up
@*
@* @par Description:
@*      Perform Intra prediction for  luma_8x8 mode:Horizontal_Up ,described in sec 8.3.2.2.9
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
@void ih264_intra_pred_luma_8x8_mode_horz_u(UWORD8 *pu1_src,
@                                           UWORD8 *pu1_dst,
@                                           WORD32 src_strd,
@                                           WORD32 dst_strd,
@                                           WORD32 ui_neighboravailability)

@**************Variables Vs Registers*****************************************
@   r0 => *pu1_src
@   r1 => *pu1_dst
@   r2 =>  src_strd
@   r3 =>  dst_strd
@   r4 =>  ui_neighboravailability

    .global ih264_intra_pred_luma_8x8_mode_horz_u_a9q

ih264_intra_pred_luma_8x8_mode_horz_u_a9q:

    stmfd         sp!, {r4-r12, r14}    @store register values to stack
    vpush         {d8-d15}

    vld1.u8       {q0}, [r0]
    vld1.u8       {d1[7]}, [r0]
    vext.8        q1, q0, q0, #1
    vext.8        q2, q1, q1, #1
    @ LOADING V TABLE
    ldr           r12, scratch_intrapred_addr_8x8
scrlb8x8l2:
    add           r12, r12, pc
    vaddl.u8      q10, d0, d2
    vaddl.u8      q11, d1, d3
    vaddl.u8      q12, d2, d4
    vaddl.u8      q13, d3, d5
    vadd.u16      q12, q10, q12
    vadd.u16      q13, q11, q13
    vld1.u8       {q5}, [r12]
    vqrshrun.s16  d4, q10, #1
    vqrshrun.s16  d5, q11, #1
    vqrshrun.s16  d6, q12, #2
    vqrshrun.s16  d7, q13, #2
    @Q2 has all FILT11 values
    @Q3 has all FILT121 values
    vtbl.u8       d12, {q2, q3}, d10
    vdup.u8       q7, d5[7]             @
    vtbl.u8       d13, {q2, q3}, d11
    vext.8        q8, q6, q7, #2
    vext.8        q9, q8, q7, #2
    vst1.8        {d12}, [r1], r3
    vext.8        q10, q9, q7, #2
    vst1.8        {d16}, [r1], r3
    vst1.8        {d18}, [r1], r3
    vst1.8        {d20}, [r1], r3
    vst1.8        {d13}, [r1], r3
    vst1.8        {d17}, [r1], r3
    vst1.8        {d19}, [r1], r3
    vst1.8        {d21}, [r1], r3


end_func_horz_u:
    vpop          {d8-d15}
    ldmfd         sp!, {r4-r12, pc}     @Restoring registers from stack








