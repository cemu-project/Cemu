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
@/*****************************************************************************/
@/*                                                                           */
@/*  File Name         : ih264_deblk_chroma_a9.s                              */
@/*                                                                           */
@/*  Description       : Contains function definitions for deblocking luma    */
@/*                      edge. Functions are coded in NEON assembly and can   */
@/*                      be compiled using ARM RVDS.                          */
@/*                                                                           */
@/*  List of Functions : ih264_deblk_chroma_vert_bs4_bp_a9()                  */
@/*                      ih264_deblk_chroma_vert_bslt4_bp_a9()                */
@/*                      ih264_deblk_chroma_horz_bs4_bp_a9()                  */
@/*                      ih264_deblk_chroma_horz_bslt4_bp_a9()                */
@/*                      ih264_deblk_chroma_vert_bs4_mbaff_bp_a9()            */
@/*                      ih264_deblk_chroma_vert_bslt4_mbaff_bp_a9()          */
@/*                      ih264_deblk_chroma_vert_bs4_a9()                     */
@/*                      ih264_deblk_chroma_vert_bslt4_a9()                   */
@/*                      ih264_deblk_chroma_horz_bs4_a9()                     */
@/*                      ih264_deblk_chroma_horz_bslt4_a9()                   */
@/*                      ih264_deblk_chroma_vert_bs4_mbaff_a9()               */
@/*                      ih264_deblk_chroma_vert_bslt4_mbaff_a9()             */
@/*                                                                           */
@/*  Issues / Problems : None                                                 */
@/*                                                                           */
@/*  Revision History  :                                                      */
@/*                                                                           */
@/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
@/*         28 11 2013   Ittiam          Draft                                */
@/*         05 01 2015   Kaushik         Added double-call functions for      */
@/*                      Senthoor        vertical deblocking, and high        */
@/*                                      profile functions.                   */
@/*                                                                           */
@/*****************************************************************************/


.text
.p2align 2

@**
@*******************************************************************************
@*
@* @brief
@*     Performs filtering of a chroma block horizontal edge when the
@*     boundary strength is set to 4
@*
@* @par Description:
@*    This operation is described in  Sec. 8.7.2.4 under the title
@*    "Filtering process for edges for bS equal to 4" in ITU T Rec H.264.
@*
@* @param[in] r0 - pu1_src
@*  Pointer to the src sample q0
@*
@* @param[in] r1 - src_strd
@*  Source stride
@*
@* @param[in] r2 - alpha
@*  Alpha Value for the boundary
@*
@* @param[in] r3 - beta
@*  Beta Value for the boundary
@*
@* @returns
@*  None
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@*

    .global ih264_deblk_chroma_horz_bs4_bp_a9

ih264_deblk_chroma_horz_bs4_bp_a9:

    stmfd         sp!, {r4, lr}         @
    vpush         {d8 - d15}
    sub           r0, r0, r1, lsl #1    @R0 = uc_edgePixel pointing to p1 of chroma
    vld2.8        {d6, d7}, [r0], r1    @D6 = p1u , D7 = p1v
    mov           r4, r0                @Keeping a backup of the pointer p0 of chroma
    vld2.8        {d4, d5}, [r0], r1    @D4 = p0u , D5 = p0v
    vdup.8        q10, r2               @Q10 contains alpha
    vld2.8        {d0, d1}, [r0], r1    @D0 = q0u , D1 = q0v
    vaddl.u8      q4, d6, d0            @
    vaddl.u8      q5, d7, d1            @Q4,Q5 = q0 + p1
    vmov.i8       d31, #2               @
    vld2.8        {d2, d3}, [r0]        @D2 = q1u , D3 = q1v
    vabd.u8       q13, q3, q2           @Q13 = ABS(p1 - p0)
    vmlal.u8      q4, d2, d31           @
    vmlal.u8      q5, d3, d31           @Q5,Q4 = (X2(q1U) + q0U + p1U)
    vabd.u8       q11, q2, q0           @Q11 = ABS(p0 - q0)
    vabd.u8       q12, q1, q0           @Q12 = ABS(q1 - q0)
    vaddl.u8      q7, d4, d2            @
    vaddl.u8      q14, d5, d3           @Q14,Q7 = P0 + Q1
    vdup.8        q8, r3                @Q8 contains beta
    vmlal.u8      q7, d6, d31           @
    vmlal.u8      q14, d7, d31          @Q14,Q7 = (X2(p1U) + p0U + q1U)
    vcge.u8       q9, q11, q10          @Q9 = ( ABS(p0 - q0) >= Alpha )
    vcge.u8       q12, q12, q8          @Q12= ( ABS(q1 - q0) >= Beta )
    vcge.u8       q13, q13, q8          @Q13= ( ABS(p1 - p0) >= Beta )
    vrshrn.u16    d8, q4, #2            @
    vrshrn.u16    d9, q5, #2            @Q4 = (X2(q1U) + q0U + p1U + 2) >> 2
    vorr          q9, q9, q12           @Q9 = ( ABS(p0 - q0) >= Alpha ) | ( ABS(q1 - q0) >= Beta )
    vrshrn.u16    d10, q7, #2           @
    vrshrn.u16    d11, q14, #2          @Q5 = (X2(p1U) + p0U + q1U + 2) >> 2
    vorr          q9, q9, q13           @Q9 = ( ABS(p0 - q0) >= Alpha ) | ( ABS(q1 - q0) >= Beta ) | ( ABS(p1 - p0) >= Beta )
    vbit          q5, q2, q9            @
    vbit          q4, q0, q9            @
    vst2.8        {d10, d11}, [r4], r1  @
    vst2.8        {d8, d9}, [r4]        @
    vpop          {d8 - d15}
    ldmfd         sp!, {r4, pc}         @



@**
@*******************************************************************************
@*
@* @brief
@*     Performs filtering of a chroma block vertical edge when the
@*     boundary strength is set to 4
@*
@* @par Description:
@*    This operation is described in  Sec. 8.7.2.4 under the title
@*    "Filtering process for edges for bS equal to 4" in ITU T Rec H.264.
@*
@* @param[in] r0 - pu1_src
@*  Pointer to the src sample q0
@*
@* @param[in] r1 - src_strd
@*  Source stride
@*
@* @param[in] r2 - alpha
@*  Alpha Value for the boundary
@*
@* @param[in] r3 - beta
@*  Beta Value for the boundary
@*
@* @returns
@*  None
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@*

    .global ih264_deblk_chroma_vert_bs4_bp_a9

ih264_deblk_chroma_vert_bs4_bp_a9:

    stmfd         sp!, {r12, r14}
    vpush         {d8 - d15}
    sub           r0, r0, #4            @point r0 to p1u of row0.
    mov           r12, r0               @keep a back up of r0 for buffer write

    vld4.16       {d0[0], d2[0], d4[0], d6[0]}, [r0], r1
    vld4.16       {d0[1], d2[1], d4[1], d6[1]}, [r0], r1
    vld4.16       {d0[2], d2[2], d4[2], d6[2]}, [r0], r1
    vld4.16       {d0[3], d2[3], d4[3], d6[3]}, [r0], r1

    vld4.16       {d1[0], d3[0], d5[0], d7[0]}, [r0], r1
    vld4.16       {d1[1], d3[1], d5[1], d7[1]}, [r0], r1
    vld4.16       {d1[2], d3[2], d5[2], d7[2]}, [r0], r1
    vld4.16       {d1[3], d3[3], d5[3], d7[3]}, [r0], r1

    vdup.8        q11, r2               @Q4 = alpha
    vdup.8        q12, r3               @Q5 = beta
    vmov.i8       d31, #2

    vabd.u8       q4, q1, q2            @|p0-q0|
    vabd.u8       q5, q3, q2            @|q1-q0|
    vabd.u8       q6, q0, q1            @|p1-p0|
    vaddl.u8      q7, d2, d6
    vaddl.u8      q8, d3, d7            @(p0 + q1)
    vclt.u8       q4, q4, q11           @|p0-q0| < alpha ?
    vclt.u8       q5, q5, q12           @|q1-q0| < beta ?
    vclt.u8       q6, q6, q12           @|p1-p0| < beta ?
    vmlal.u8      q7, d0, d31
    vmlal.u8      q8, d1, d31           @2*p1 + (p0 + q1)
    vaddl.u8      q9, d0, d4
    vaddl.u8      q10, d1, d5           @(p1 + q0)
    vand.u8       q4, q4, q5            @|p0-q0| < alpha && |q1-q0| < beta
    vmlal.u8      q9, d6, d31
    vmlal.u8      q10, d7, d31          @2*q1 + (p1 + q0)

    vrshrn.i16    d14, q7, #2
    vrshrn.i16    d15, q8, #2           @(2*p1 + (p0 + q1) + 2) >> 2
    vand.u8       q4, q4, q6            @|p0-q0| < alpha && |q1-q0| < beta && |p1-p0| < beta
    vrshrn.i16    d18, q9, #2
    vrshrn.i16    d19, q10, #2          @(2*q1 + (p1 + q0) + 2) >> 2

    vbit          q1, q7, q4
    vbit          q2, q9, q4

    vst4.16       {d0[0], d2[0], d4[0], d6[0]}, [r12], r1
    vst4.16       {d0[1], d2[1], d4[1], d6[1]}, [r12], r1
    vst4.16       {d0[2], d2[2], d4[2], d6[2]}, [r12], r1
    vst4.16       {d0[3], d2[3], d4[3], d6[3]}, [r12], r1

    vst4.16       {d1[0], d3[0], d5[0], d7[0]}, [r12], r1
    vst4.16       {d1[1], d3[1], d5[1], d7[1]}, [r12], r1
    vst4.16       {d1[2], d3[2], d5[2], d7[2]}, [r12], r1
    vst4.16       {d1[3], d3[3], d5[3], d7[3]}, [r12], r1
    vpop          {d8 - d15}
    ldmfd         sp!, {r12, pc}



@**
@*******************************************************************************
@*
@* @brief
@*     Performs filtering of a chroma block horizontal edge for cases where the
@*     boundary strength is less than 4
@*
@* @par Description:
@*    This operation is described in  Sec. 8.7.2.4 under the title
@*    "Filtering process for edges for bS equal to 4" in ITU T Rec H.264.
@*
@* @param[in] r0 - pu1_src
@*  Pointer to the src sample q0
@*
@* @param[in] r1 - src_strd
@*  Source stride
@*
@* @param[in] r2 - alpha
@*  Alpha Value for the boundary
@*
@* @param[in] r3 - beta
@*  Beta Value for the boundary
@*
@* @param[in] sp(0) - u4_bs
@*  Packed Boundary strength array
@*
@* @param[in] sp(4) - pu1_cliptab
@*  tc0_table
@*
@* @returns
@*  None
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@*

    .global ih264_deblk_chroma_horz_bslt4_bp_a9

ih264_deblk_chroma_horz_bslt4_bp_a9:

    stmfd         sp!, {r4-r6, lr}      @

    ldrd          r4, r5, [sp, #0x10]   @r4 = u4_bs , r5 = pu1_cliptab
    vpush         {d8 - d15}
    sub           r0, r0, r1, lsl #1    @R0 = uc_edgePixelU pointing to p2 of chroma U
    rev           r4, r4                @
    vmov.32       d12[0], r4            @d12[0] = ui_Bs
    vld1.32       d16[0], [r5]          @D16[0] contains cliptab
    vld2.8        {d6, d7}, [r0], r1    @Q3=p1
    vtbl.8        d14, {d16}, d12       @
    vmovl.u8      q6, d12               @q6 = uc_Bs in each 16 bit scalar
    mov           r6, r0                @Keeping a backup of the pointer to chroma U P0
    vld2.8        {d4, d5}, [r0], r1    @Q2=p0
    vmov.i8       d30, #1               @
    vdup.8        q10, r2               @Q10 contains alpha
    vld2.8        {d0, d1}, [r0], r1    @Q0=q0
    vmovl.u8      q7, d14               @
    vld2.8        {d2, d3}, [r0]        @Q1=q1
    vsubl.u8      q5, d1, d5            @
    vsubl.u8      q4, d0, d4            @Q5,Q4 = (q0 - p0)
    vabd.u8       q13, q3, q2           @Q13 = ABS(p1 - p0)
    vshl.i16      q5, q5, #2            @Q5 = (q0 - p0)<<2
    vabd.u8       q11, q2, q0           @Q11 = ABS(p0 - q0)
    vshl.i16      q4, q4, #2            @Q4 = (q0 - p0)<<2
    vsli.16       q7, q7, #8            @
    vabd.u8       q12, q1, q0           @Q12 = ABS(q1 - q0)
    vcge.u8       q9, q11, q10          @Q9 = ( ABS(p0 - q0) >= Alpha )
    vsubl.u8      q10, d6, d2           @Q10 = (p1 - q1)L
    vsubl.u8      q3, d7, d3            @Q3 = (p1 - q1)H
    vdup.8        q8, r3                @Q8 contains beta
    vadd.i16      q4, q4, q10           @
    vadd.i16      q5, q5, q3            @Q5,Q4 = [ (q0 - p0)<<2 ] + (p1 - q1)
    vcge.u8       q12, q12, q8          @Q12= ( ABS(q1 - q0) >= Beta )
    vcgt.s16      d12, d12, #0          @Q6 = (us_Bs > 0)
    vqrshrn.s16   d8, q4, #3            @
    vqrshrn.s16   d9, q5, #3            @Q4 = i_macro = (((q0 - p0)<<2) + (p1 - q1) + 4)>>3
    vadd.i8       d14, d14, d30         @Q7 = C = C0+1
    vcge.u8       q13, q13, q8          @Q13= ( ABS(p1 - p0) >= Beta )
    vorr          q9, q9, q12           @Q9 = ( ABS(p0 - q0) >= Alpha ) | ( ABS(q1 - q0) >= Beta )
    vabs.s8       q3, q4                @Q4 = ABS (i_macro)
    vmov.i8       d15, d14              @
    vmov.i8       d13, d12              @
    vorr          q9, q9, q13           @Q9 = ( ABS(p0 - q0) >= Alpha ) | ( ABS(q1 - q0) >= Beta ) | ( ABS(p1 - p0) >= Beta )
    vmin.u8       q7, q3, q7            @Q7 = delta = (ABS(i_macro) > C) ? C : ABS(i_macro)
    vbic          q6, q6, q9            @final condition
    vcge.s8       q4, q4, #0            @Q4  = (i_macro >= 0)
    vand          q7, q7, q6            @Making delta zero in places where values shouldn be filterd
    vqadd.u8      q8, q2, q7            @Q8  = p0 + delta
    vqsub.u8      q2, q2, q7            @Q2 = p0 - delta
    vqadd.u8      q9, q0, q7            @Q9 = q0 + delta
    vqsub.u8      q0, q0, q7            @Q0 = q0 - delta
    vbif          q8, q2, q4            @Q8  = (i_macro >= 0 ) ? (p0+delta) : (p0-delta)
    vbif          q0, q9, q4            @Q0  = (i_macro >= 0 ) ? (q0-delta) : (q0+delta)
    vst2.8        {d16, d17}, [r6], r1  @
    vst2.8        {d0, d1}, [r6]        @
    vpop          {d8 - d15}
    ldmfd         sp!, {r4-r6, pc}      @



@**
@*******************************************************************************
@*
@* @brief
@*     Performs filtering of a chroma block vertical edge for cases where the
@*     boundary strength is less than 4
@*
@* @par Description:
@*    This operation is described in  Sec. 8.7.2.4 under the title
@*    "Filtering process for edges for bS equal to 4" in ITU T Rec H.264.
@*
@* @param[in] r0 - pu1_src
@*  Pointer to the src sample q0
@*
@* @param[in] r1 - src_strd
@*  Source stride
@*
@* @param[in] r2 - alpha
@*  Alpha Value for the boundary
@*
@* @param[in] r3 - beta
@*  Beta Value for the boundary
@*
@* @param[in] sp(0) - u4_bs
@*  Packed Boundary strength array
@*
@* @param[in] sp(4) - pu1_cliptab
@*  tc0_table
@*
@* @returns
@*  None
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@*

    .global ih264_deblk_chroma_vert_bslt4_bp_a9

ih264_deblk_chroma_vert_bslt4_bp_a9:

    stmfd         sp!, {r10-r12, r14}

    sub           r0, r0, #4            @point r0 to p1u of row0.
    ldr           r11, [sp, #16]        @r12 = ui_Bs

    ldr           r10, [sp, #20]        @r14 = puc_ClipTab
    mov           r12, r0               @keep a back up of r0 for buffer write
    vpush         {d8 - d15}
    vld4.16       {d0[0], d2[0], d4[0], d6[0]}, [r0], r1
    vld4.16       {d0[1], d2[1], d4[1], d6[1]}, [r0], r1
    vld4.16       {d0[2], d2[2], d4[2], d6[2]}, [r0], r1
    vld4.16       {d0[3], d2[3], d4[3], d6[3]}, [r0], r1

    vld4.16       {d1[0], d3[0], d5[0], d7[0]}, [r0], r1
    vld4.16       {d1[1], d3[1], d5[1], d7[1]}, [r0], r1
    vld4.16       {d1[2], d3[2], d5[2], d7[2]}, [r0], r1
    vld4.16       {d1[3], d3[3], d5[3], d7[3]}, [r0], r1


    vdup.8        q11, r2               @Q4 = alpha
    vabd.u8       q4, q1, q2            @|p0-q0|
    vdup.8        q12, r3               @Q5 = beta
    vabd.u8       q5, q3, q2            @|q1-q0|
    vabd.u8       q6, q0, q1            @|p1-p0|
    vclt.u8       q4, q4, q11           @|p0-q0| < alpha ?
    vsubl.u8      q7, d0, d6
    vclt.u8       q5, q5, q12           @|q1-q0| < beta ?
    vsubl.u8      q8, d1, d7            @(p1 - q1)
    vclt.u8       q6, q6, q12           @|p1-p0| < beta ?
    vsubl.u8      q9, d4, d2
    vand.u8       q4, q4, q5            @|p0-q0| < alpha && |q1-q0| < beta
    vsubl.u8      q10, d5, d3           @(q0 - p0)
    vmov.u16      q14, #4
    vld1.32       {d24[0]}, [r10]       @Load ClipTable
    rev           r11, r11              @Blocking strengths
    vand.u8       q4, q4, q6            @|p0-q0| < alpha && |q1-q0| < beta && |p1-p0| < beta

    vmov.32       d10[0], r11

    vmla.s16      q7, q9, q14
    vmla.s16      q8, q10, q14          @4*(q0 - p0) + (p1 - q1)

    vmovl.u8      q5, d10


    vsli.u16      d10, d10, #8
    vmovl.u16     q5, d10
    vsli.u32      q5, q5, #16
    vtbl.8        d12, {d24}, d10
    vtbl.8        d13, {d24}, d11       @tC0
    vmov.u8       q12, #1
    vadd.u8       q6, q6, q12           @tC0 + 1
    vcge.u8       q5, q5, q12           @u4_bS > 0 ?
    vand.u8       q4, q4, q5            @|p0-q0| < alpha && |q1-q0| < beta && |p1-p0| < beta && u4_bs != 0

    @ Q0 - Q3(inputs),
    @ Q4 (|p0-q0| < alpha && |q1-q0| < beta && |p1-p0| < beta && u4_bs != 0),
    @ Q6 (tC)

    vrshr.s16     q7, q7, #3
    vrshr.s16     q8, q8, #3            @(((q0 - p0) << 2) + (p1 - q1) + 4) >> 3)

    vcgt.s16      q9, q7, #0
    vcgt.s16      q10, q8, #0
    vmovn.i16     d18, q9
    vmovn.i16     d19, q10              @Q9 = sign(delta)
    vabs.s16      q7, q7
    vabs.s16      q8, q8
    vmovn.u16     d14, q7
    vmovn.u16     d15, q8
    vmin.u8       q7, q7, q6            @Q7 = |delta|

    vqadd.u8      q10, q1, q7           @p0+|delta|
    vqadd.u8      q11, q2, q7           @q0+|delta|
    vqsub.u8      q12, q1, q7           @p0-|delta|
    vqsub.u8      q13, q2, q7           @q0-|delta|

    vbit          q12, q10, q9          @p0 + delta
    vbit          q11, q13, q9          @q0 - delta

    vbit          q1, q12, q4
    vbit          q2, q11, q4

    vst4.16       {d0[0], d2[0], d4[0], d6[0]}, [r12], r1
    vst4.16       {d0[1], d2[1], d4[1], d6[1]}, [r12], r1
    vst4.16       {d0[2], d2[2], d4[2], d6[2]}, [r12], r1
    vst4.16       {d0[3], d2[3], d4[3], d6[3]}, [r12], r1

    vst4.16       {d1[0], d3[0], d5[0], d7[0]}, [r12], r1
    vst4.16       {d1[1], d3[1], d5[1], d7[1]}, [r12], r1
    vst4.16       {d1[2], d3[2], d5[2], d7[2]}, [r12], r1
    vst4.16       {d1[3], d3[3], d5[3], d7[3]}, [r12], r1
    vpop          {d8 - d15}
    ldmfd         sp!, {r10-r12, pc}



@**
@*******************************************************************************
@*
@* @brief
@*     Performs filtering of a chroma block vertical edge when the
@*     boundary strength is set to 4 on calling twice
@*
@* @par Description:
@*    This operation is described in  Sec. 8.7.2.4 under the title
@*    "Filtering process for edges for bS equal to 4" in ITU T Rec H.264.
@*
@* @param[in] r0 - pu1_src
@*  Pointer to the src sample q0
@*
@* @param[in] r1 - src_strd
@*  Source stride
@*
@* @param[in] r2 - alpha
@*  Alpha Value for the boundary
@*
@* @param[in] r3 - beta
@*  Beta Value for the boundary
@*
@* @returns
@*  None
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@*

    .global ih264_deblk_chroma_vert_bs4_mbaff_bp_a9

ih264_deblk_chroma_vert_bs4_mbaff_bp_a9:

    stmfd         sp!, {r12, r14}
    vpush         {d8 - d15}
    sub           r0, r0, #4            @point r0 to p1u of row0.
    mov           r12, r0               @keep a back up of r0 for buffer write

    vld4.16       {d0[0], d1[0], d2[0], d3[0]}, [r0], r1
    vld4.16       {d0[1], d1[1], d2[1], d3[1]}, [r0], r1
    vld4.16       {d0[2], d1[2], d2[2], d3[2]}, [r0], r1
    vld4.16       {d0[3], d1[3], d2[3], d3[3]}, [r0], r1

    vdup.8        d11, r2               @D11 = alpha
    vdup.8        d12, r3               @D12 = beta
    vmov.i8       d31, #2

    vabd.u8       d4, d1, d2            @|p0-q0|
    vabd.u8       d5, d3, d2            @|q1-q0|
    vabd.u8       d6, d0, d1            @|p1-p0|
    vaddl.u8      q14, d1, d3           @(p0 + q1)
    vclt.u8       d4, d4, d11           @|p0-q0| < alpha ?
    vclt.u8       d5, d5, d12           @|q1-q0| < beta ?
    vclt.u8       d6, d6, d12           @|p1-p0| < beta ?
    vmlal.u8      q14, d0, d31          @2*p1 + (p0 + q1)
    vaddl.u8      q13, d0, d2           @(p1 + q0)
    vand.u8       d4, d4, d5            @|p0-q0| < alpha && |q1-q0| < beta
    vmlal.u8      q13, d3, d31          @2*q1 + (p1 + q0)

    vrshrn.i16    d7, q14, #2           @(2*p1 + (p0 + q1) + 2) >> 2
    vand.u8       d4, d4, d6            @|p0-q0| < alpha && |q1-q0| < beta && |p1-p0| < beta
    vrshrn.i16    d9, q13, #2           @(2*q1 + (p1 + q0) + 2) >> 2

    vbit          d1, d7, d4
    vbit          d2, d9, d4

    vst4.16       {d0[0], d1[0], d2[0], d3[0]}, [r12], r1
    vst4.16       {d0[1], d1[1], d2[1], d3[1]}, [r12], r1
    vst4.16       {d0[2], d1[2], d2[2], d3[2]}, [r12], r1
    vst4.16       {d0[3], d1[3], d2[3], d3[3]}, [r12], r1
    vpop          {d8 - d15}
    ldmfd         sp!, {r12, pc}



@**
@*******************************************************************************
@*
@* @brief
@*     Performs filtering of a chroma block vertical edge for cases where the
@*     boundary strength is less than 4 on calling twice
@*
@* @par Description:
@*    This operation is described in  Sec. 8.7.2.4 under the title
@*    "Filtering process for edges for bS equal to 4" in ITU T Rec H.264.
@*
@* @param[in] r0 - pu1_src
@*  Pointer to the src sample q0
@*
@* @param[in] r1 - src_strd
@*  Source stride
@*
@* @param[in] r2 - alpha
@*  Alpha Value for the boundary
@*
@* @param[in] r3 - beta
@*  Beta Value for the boundary
@*
@* @param[in] sp(0) - u4_bs
@*  Packed Boundary strength array
@*
@* @param[in] sp(4) - pu1_cliptab
@*  tc0_table
@*
@* @returns
@*  None
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@*

    .global ih264_deblk_chroma_vert_bslt4_mbaff_bp_a9

ih264_deblk_chroma_vert_bslt4_mbaff_bp_a9:

    stmfd         sp!, {r10-r12, r14}

    sub           r0, r0, #4            @point r0 to p1u of row0.
    ldr           r11, [sp, #16]        @r11 = ui_Bs

    ldr           r10, [sp, #20]        @r10 = puc_ClipTab
    mov           r12, r0               @keep a back up of r0 for buffer write
    vpush         {d8 - d15}
    vld4.16       {d0[0], d1[0], d2[0], d3[0]}, [r0], r1
    vld4.16       {d0[1], d1[1], d2[1], d3[1]}, [r0], r1
    vld4.16       {d0[2], d1[2], d2[2], d3[2]}, [r0], r1
    vld4.16       {d0[3], d1[3], d2[3], d3[3]}, [r0], r1

    vdup.8        d11, r2               @D11 = alpha
    vabd.u8       d4, d1, d2            @|p0-q0|
    vdup.8        d12, r3               @D12 = beta
    vabd.u8       d5, d3, d2            @|q1-q0|
    vabd.u8       d6, d0, d1            @|p1-p0|
    vclt.u8       d4, d4, d11           @|p0-q0| < alpha ?
    vclt.u8       d5, d5, d12           @|q1-q0| < beta ?
    vsubl.u8      q14, d0, d3           @(p1 - q1)
    vclt.u8       d6, d6, d12           @|p1-p0| < beta ?
    vand.u8       d4, d4, d5            @|p0-q0| < alpha && |q1-q0| < beta
    vsubl.u8      q12, d2, d1           @(q0 - p0)
    vmov.u16      q10, #4

    vld1.32       {d31[0]}, [r10]       @Load ClipTable
    rev           r11, r11              @Blocking strengths
    vand.u8       d4, d4, d6            @|p0-q0| < alpha && |q1-q0| < beta && |p1-p0| < beta
    vmov.32       d22[0], r11
    vmla.s16      q14, q12, q10         @4*(q0 - p0) + (p1 - q1)
    vmovl.u8      q11, d22
    vsli.u16      d22, d22, #8
    vtbl.8        d6, {d31}, d22        @tC0
    vmov.u8       d12, #1
    vadd.u8       d6, d6, d12           @tC0 + 1
    vcge.u8       d5, d22, d12          @u4_bS > 0 ?
    vand.u8       d4, d4, d5            @|p0-q0| < alpha && |q1-q0| < beta && |p1-p0| < beta && u4_bs != 0

    @ D0 - D3(inputs),
    @ D4 (|p0-q0| < alpha && |q1-q0| < beta && |p1-p0| < beta && u4_bs != 0),
    @ D6 (tC)

    vrshr.s16     q14, q14, #3          @(((q0 - p0) << 2) + (p1 - q1) + 4) >> 3)

    vcgt.s16      q13, q14, #0
    vmovn.i16     d9, q13               @D9 = sign(delta)
    vabs.s16      q14, q14
    vmovn.u16     d7, q14
    vmin.u8       d7, d7, d6            @D7 = |delta|

    vqadd.u8      d10, d1, d7           @p0+|delta|
    vqadd.u8      d11, d2, d7           @q0+|delta|
    vqsub.u8      d12, d1, d7           @p0-|delta|
    vqsub.u8      d13, d2, d7           @q0-|delta|

    vbit          d12, d10, d9          @p0 + delta
    vbit          d11, d13, d9          @q0 - delta

    vbit          d1, d12, d4
    vbit          d2, d11, d4

    vst4.16       {d0[0], d1[0], d2[0], d3[0]}, [r12], r1
    vst4.16       {d0[1], d1[1], d2[1], d3[1]}, [r12], r1
    vst4.16       {d0[2], d1[2], d2[2], d3[2]}, [r12], r1
    vst4.16       {d0[3], d1[3], d2[3], d3[3]}, [r12], r1
    vpop          {d8 - d15}
    ldmfd         sp!, {r10-r12, pc}



@**
@*******************************************************************************
@*
@* @brief
@*     Performs filtering of a chroma block horizontal edge when the
@*     boundary strength is set to 4 in high profile
@*
@* @par Description:
@*    This operation is described in  Sec. 8.7.2.4 under the title
@*    "Filtering process for edges for bS equal to 4" in ITU T Rec H.264.
@*
@* @param[in] r0 - pu1_src
@*  Pointer to the src sample q0
@*
@* @param[in] r1 - src_strd
@*  Source stride
@*
@* @param[in] r2 - alpha_cb
@*  Alpha Value for the boundary in U
@*
@* @param[in] r3 - beta_cb
@*  Beta Value for the boundary in U
@*
@* @param[in] sp(0) - alpha_cr
@*  Alpha Value for the boundary in V
@*
@* @param[in] sp(4) - beta_cr
@*  Beta Value for the boundary in V
@*
@* @returns
@*  None
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@*

    .global ih264_deblk_chroma_horz_bs4_a9

ih264_deblk_chroma_horz_bs4_a9:

    stmfd         sp!, {r4-r6, lr}      @

    ldr           r5, [sp, #16]         @R5 = alpha_cr
    ldr           r6, [sp, #20]         @R6 = beta_cr
    vpush         {d8 - d15}
    sub           r0, r0, r1, lsl #1    @R0 = uc_edgePixel pointing to p1 of chroma
    vld2.8        {d6, d7}, [r0], r1    @D6 = p1u , D7 = p1v
    mov           r4, r0                @Keeping a backup of the pointer p0 of chroma
    vld2.8        {d4, d5}, [r0], r1    @D4 = p0u , D5 = p0v
    vdup.8        d20, r2               @D20 contains alpha_cb
    vdup.8        d21, r5               @D21 contains alpha_cr
    vld2.8        {d0, d1}, [r0], r1    @D0 = q0u , D1 = q0v
    vaddl.u8      q4, d6, d0            @
    vaddl.u8      q5, d7, d1            @Q4,Q5 = q0 + p1
    vmov.i8       d31, #2               @
    vld2.8        {d2, d3}, [r0]        @D2 = q1u , D3 = q1v
    vabd.u8       q13, q3, q2           @Q13 = ABS(p1 - p0)
    vmlal.u8      q4, d2, d31           @
    vmlal.u8      q5, d3, d31           @Q5,Q4 = (X2(q1U) + q0U + p1U)
    vabd.u8       q11, q2, q0           @Q11 = ABS(p0 - q0)
    vabd.u8       q12, q1, q0           @Q12 = ABS(q1 - q0)
    vaddl.u8      q7, d4, d2            @
    vaddl.u8      q14, d5, d3           @Q14,Q7 = P0 + Q1
    vdup.8        d16, r3               @D16 contains beta_cb
    vdup.8        d17, r6               @D17 contains beta_cr
    vmlal.u8      q7, d6, d31           @
    vmlal.u8      q14, d7, d31          @Q14,Q7 = (X2(p1U) + p0U + q1U)
    vcge.u8       q9, q11, q10          @Q9 = ( ABS(p0 - q0) >= Alpha )
    vcge.u8       q12, q12, q8          @Q12= ( ABS(q1 - q0) >= Beta )
    vcge.u8       q13, q13, q8          @Q13= ( ABS(p1 - p0) >= Beta )
    vrshrn.u16    d8, q4, #2            @
    vrshrn.u16    d9, q5, #2            @Q4 = (X2(q1U) + q0U + p1U + 2) >> 2
    vorr          q9, q9, q12           @Q9 = ( ABS(p0 - q0) >= Alpha ) | ( ABS(q1 - q0) >= Beta )
    vrshrn.u16    d10, q7, #2           @
    vrshrn.u16    d11, q14, #2          @Q5 = (X2(p1U) + p0U + q1U + 2) >> 2
    vorr          q9, q9, q13           @Q9 = ( ABS(p0 - q0) >= Alpha ) | ( ABS(q1 - q0) >= Beta ) | ( ABS(p1 - p0) >= Beta )
    vbit          q5, q2, q9            @
    vbit          q4, q0, q9            @
    vst2.8        {d10, d11}, [r4], r1  @
    vst2.8        {d8, d9}, [r4]        @
    vpop          {d8 - d15}
    ldmfd         sp!, {r4-r6, pc}      @



@**
@*******************************************************************************
@*
@* @brief
@*     Performs filtering of a chroma block vertical edge when the
@*     boundary strength is set to 4 in high profile
@*
@* @par Description:
@*    This operation is described in  Sec. 8.7.2.4 under the title
@*    "Filtering process for edges for bS equal to 4" in ITU T Rec H.264.
@*
@* @param[in] r0 - pu1_src
@*  Pointer to the src sample q0
@*
@* @param[in] r1 - src_strd
@*  Source stride
@*
@* @param[in] r2 - alpha_cb
@*  Alpha Value for the boundary in U
@*
@* @param[in] r3 - beta_cb
@*  Beta Value for the boundary in U
@*
@* @param[in] sp(0) - alpha_cr
@*  Alpha Value for the boundary in V
@*
@* @param[in] sp(4) - beta_cr
@*  Beta Value for the boundary in V
@*
@* @returns
@*  None
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@*

    .global ih264_deblk_chroma_vert_bs4_a9

ih264_deblk_chroma_vert_bs4_a9:

    stmfd         sp!, {r4, r5, r12, r14}

    sub           r0, r0, #4            @point r0 to p1u of row0.
    mov           r12, r0               @keep a back up of r0 for buffer write

    ldr           r4, [sp, #16]         @r4 = alpha_cr
    ldr           r5, [sp, #20]         @r5 = beta_cr
    add           r2, r2, r4, lsl #8    @r2 = (alpha_cr,alpha_cb)
    add           r3, r3, r5, lsl #8    @r3 = (beta_cr,beta_cb)
    vpush         {d8 - d15}
    vld4.16       {d0[0], d2[0], d4[0], d6[0]}, [r0], r1
    vld4.16       {d0[1], d2[1], d4[1], d6[1]}, [r0], r1
    vld4.16       {d0[2], d2[2], d4[2], d6[2]}, [r0], r1
    vld4.16       {d0[3], d2[3], d4[3], d6[3]}, [r0], r1

    vld4.16       {d1[0], d3[0], d5[0], d7[0]}, [r0], r1
    vld4.16       {d1[1], d3[1], d5[1], d7[1]}, [r0], r1
    vld4.16       {d1[2], d3[2], d5[2], d7[2]}, [r0], r1
    vld4.16       {d1[3], d3[3], d5[3], d7[3]}, [r0], r1

    vdup.16       q11, r2               @Q11 = alpha
    vdup.16       q12, r3               @Q12 = beta
    vmov.i8       d31, #2

    vabd.u8       q4, q1, q2            @|p0-q0|
    vabd.u8       q5, q3, q2            @|q1-q0|
    vabd.u8       q6, q0, q1            @|p1-p0|
    vaddl.u8      q7, d2, d6
    vaddl.u8      q8, d3, d7            @(p0 + q1)
    vclt.u8       q4, q4, q11           @|p0-q0| < alpha ?
    vclt.u8       q5, q5, q12           @|q1-q0| < beta ?
    vclt.u8       q6, q6, q12           @|p1-p0| < beta ?
    vmlal.u8      q7, d0, d31
    vmlal.u8      q8, d1, d31           @2*p1 + (p0 + q1)
    vaddl.u8      q9, d0, d4
    vaddl.u8      q10, d1, d5           @(p1 + q0)
    vand.u8       q4, q4, q5            @|p0-q0| < alpha && |q1-q0| < beta
    vmlal.u8      q9, d6, d31
    vmlal.u8      q10, d7, d31          @2*q1 + (p1 + q0)

    vrshrn.i16    d14, q7, #2
    vrshrn.i16    d15, q8, #2           @(2*p1 + (p0 + q1) + 2) >> 2
    vand.u8       q4, q4, q6            @|p0-q0| < alpha && |q1-q0| < beta && |p1-p0| < beta
    vrshrn.i16    d18, q9, #2
    vrshrn.i16    d19, q10, #2          @(2*q1 + (p1 + q0) + 2) >> 2

    vbit          q1, q7, q4
    vbit          q2, q9, q4

    vst4.16       {d0[0], d2[0], d4[0], d6[0]}, [r12], r1
    vst4.16       {d0[1], d2[1], d4[1], d6[1]}, [r12], r1
    vst4.16       {d0[2], d2[2], d4[2], d6[2]}, [r12], r1
    vst4.16       {d0[3], d2[3], d4[3], d6[3]}, [r12], r1

    vst4.16       {d1[0], d3[0], d5[0], d7[0]}, [r12], r1
    vst4.16       {d1[1], d3[1], d5[1], d7[1]}, [r12], r1
    vst4.16       {d1[2], d3[2], d5[2], d7[2]}, [r12], r1
    vst4.16       {d1[3], d3[3], d5[3], d7[3]}, [r12], r1
    vpop          {d8 - d15}
    ldmfd         sp!, {r4, r5, r12, pc}



@**
@*******************************************************************************
@*
@* @brief
@*     Performs filtering of a chroma block horizontal edge for cases where the
@*     boundary strength is less than 4 in high profile
@*
@* @par Description:
@*    This operation is described in  Sec. 8.7.2.4 under the title
@*    "Filtering process for edges for bS equal to 4" in ITU T Rec H.264.
@*
@* @param[in] r0 - pu1_src
@*  Pointer to the src sample q0
@*
@* @param[in] r1 - src_strd
@*  Source stride
@*
@* @param[in] r2 - alpha_cb
@*  Alpha Value for the boundary in U
@*
@* @param[in] r3 - beta_cb
@*  Beta Value for the boundary in U
@*
@* @param[in] sp(0) - alpha_cr
@*  Alpha Value for the boundary in V
@*
@* @param[in] sp(4) - beta_cr
@*  Beta Value for the boundary in V
@*
@* @param[in] sp(8) - u4_bs
@*  Packed Boundary strength array
@*
@* @param[in] sp(12) - pu1_cliptab_cb
@*  tc0_table for U
@*
@* @param[in] sp(16) - pu1_cliptab_cr
@*  tc0_table for V
@*
@* @returns
@*  None
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@*

    .global ih264_deblk_chroma_horz_bslt4_a9

ih264_deblk_chroma_horz_bslt4_a9:

    stmfd         sp!, {r4-r9, lr}      @

    ldrd          r4, r5, [sp, #28]     @R4 = alpha_cr , R5 = beta_cr
    ldr           r7, [sp, #36]         @R7 = u4_bs
    ldrd          r8, r9, [sp, #40]     @R8 = pu1_cliptab_cb , R9 = pu1_cliptab_cr
    sub           r0, r0, r1, lsl #1    @R0 = uc_edgePixelU pointing to p1 of chroma U
    vpush         {d8 - d15}
    rev           r7, r7                @
    vmov.32       d12[0], r7            @D12[0] = ui_Bs

    vld1.32       d16[0], [r8]          @D16[0] contains cliptab_cb
    vld1.32       d17[0], [r9]          @D17[0] contains cliptab_cr
    vld2.8        {d6, d7}, [r0], r1    @Q3=p1
    vtbl.8        d14, {d16}, d12       @Retreiving cliptab values for U
    vtbl.8        d28, {d17}, d12       @Retrieving cliptab values for V
    vmovl.u8      q6, d12               @Q6 = uc_Bs in each 16 bit scalar
    mov           r6, r0                @Keeping a backup of the pointer to chroma U P0
    vld2.8        {d4, d5}, [r0], r1    @Q2=p0
    vmov.i8       d30, #1               @
    vdup.8        d20, r2               @D20 contains alpha_cb
    vdup.8        d21, r4               @D21 contains alpha_cr
    vld2.8        {d0, d1}, [r0], r1    @Q0=q0
    vmovl.u8      q7, d14               @
    vmovl.u8      q14, d28              @
    vmov.i16      d15, d28              @D14 has cliptab values for U, D15 for V
    vld2.8        {d2, d3}, [r0]        @Q1=q1
    vsubl.u8      q5, d1, d5            @
    vsubl.u8      q4, d0, d4            @Q5,Q4 = (q0 - p0)
    vabd.u8       q13, q3, q2           @Q13 = ABS(p1 - p0)
    vshl.i16      q5, q5, #2            @Q5 = (q0 - p0)<<2
    vabd.u8       q11, q2, q0           @Q11 = ABS(p0 - q0)
    vshl.i16      q4, q4, #2            @Q4 = (q0 - p0)<<2
    vsli.16       q7, q7, #8            @
    vabd.u8       q12, q1, q0           @Q12 = ABS(q1 - q0)
    vcge.u8       q9, q11, q10          @Q9 = ( ABS(p0 - q0) >= Alpha )
    vsubl.u8      q10, d6, d2           @Q10 = (p1 - q1)L
    vsubl.u8      q3, d7, d3            @Q3 = (p1 - q1)H
    vdup.8        d16, r3               @Q8 contains beta_cb
    vdup.8        d17, r5               @Q8 contains beta_cr
    vadd.i16      q4, q4, q10           @
    vadd.i16      q5, q5, q3            @Q5,Q4 = [ (q0 - p0)<<2 ] + (p1 - q1)
    vcge.u8       q12, q12, q8          @Q12= ( ABS(q1 - q0) >= Beta )
    vcgt.s16      d12, d12, #0          @Q6 = (us_Bs > 0)
    vqrshrn.s16   d8, q4, #3            @
    vqrshrn.s16   d9, q5, #3            @Q4 = i_macro = (((q0 - p0)<<2) + (p1 - q1) + 4)>>3
    vadd.i8       d14, d14, d30         @D14 = C = C0+1 for U
    vcge.u8       q13, q13, q8          @Q13= ( ABS(p1 - p0) >= Beta )
    vorr          q9, q9, q12           @Q9 = ( ABS(p0 - q0) >= Alpha ) | ( ABS(q1 - q0) >= Beta )
    vabs.s8       q3, q4                @Q4 = ABS (i_macro)
    vadd.i8       d15, d15, d30         @D15 = C = C0+1 for V
    vmov.i8       d13, d12              @
    vorr          q9, q9, q13           @Q9 = ( ABS(p0 - q0) >= Alpha ) | ( ABS(q1 - q0) >= Beta ) | ( ABS(p1 - p0) >= Beta )
    vmin.u8       q7, q3, q7            @Q7 = delta = (ABS(i_macro) > C) ? C : ABS(i_macro)
    vbic          q6, q6, q9            @final condition
    vcge.s8       q4, q4, #0            @Q4  = (i_macro >= 0)
    vand          q7, q7, q6            @Making delta zero in places where values shouldn be filterd
    vqadd.u8      q8, q2, q7            @Q8 = p0 + delta
    vqsub.u8      q2, q2, q7            @Q2 = p0 - delta
    vqadd.u8      q9, q0, q7            @Q9 = q0 + delta
    vqsub.u8      q0, q0, q7            @Q0 = q0 - delta
    vbif          q8, q2, q4            @Q8 = (i_macro >= 0 ) ? (p0+delta) : (p0-delta)
    vbif          q0, q9, q4            @Q0 = (i_macro >= 0 ) ? (q0-delta) : (q0+delta)
    vst2.8        {d16, d17}, [r6], r1  @
    vst2.8        {d0, d1}, [r6]        @
    vpop          {d8 - d15}
    ldmfd         sp!, {r4-r9, pc}      @



@**
@*******************************************************************************
@*
@* @brief
@*     Performs filtering of a chroma block vertical edge for cases where the
@*     boundary strength is less than 4 in high profile
@*
@* @par Description:
@*    This operation is described in  Sec. 8.7.2.4 under the title
@*    "Filtering process for edges for bS equal to 4" in ITU T Rec H.264.
@*
@* @param[in] r0 - pu1_src
@*  Pointer to the src sample q0
@*
@* @param[in] r1 - src_strd
@*  Source stride
@*
@* @param[in] r2 - alpha_cb
@*  Alpha Value for the boundary in U
@*
@* @param[in] r3 - beta_cb
@*  Beta Value for the boundary in U
@*
@* @param[in] sp(0) - alpha_cr
@*  Alpha Value for the boundary in V
@*
@* @param[in] sp(4) - beta_cr
@*  Beta Value for the boundary in V
@*
@* @param[in] sp(8) - u4_bs
@*  Packed Boundary strength array
@*
@* @param[in] sp(12) - pu1_cliptab_cb
@*  tc0_table for U
@*
@* @param[in] sp(16) - pu1_cliptab_cr
@*  tc0_table for V
@*
@* @returns
@*  None
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@*

    .global ih264_deblk_chroma_vert_bslt4_a9

ih264_deblk_chroma_vert_bslt4_a9:

    stmfd         sp!, {r4-r7, r10-r12, r14}

    sub           r0, r0, #4            @point r0 to p1u of row0.
    ldrd          r4, r5, [sp, #32]     @R4 = alpha_cr , R5 = beta_cr
    add           r2, r2, r4, lsl #8
    add           r3, r3, r5, lsl #8
    ldr           r6, [sp, #40]         @R6 = u4_bs
    ldrd          r10, r11, [sp, #44]   @R10 = pu1_cliptab_cb , R11 = pu1_cliptab_cr
    vpush         {d8 - d15}
    mov           r12, r0               @keep a back up of R0 for buffer write

    vld4.16       {d0[0], d2[0], d4[0], d6[0]}, [r0], r1
    vld4.16       {d0[1], d2[1], d4[1], d6[1]}, [r0], r1
    vld4.16       {d0[2], d2[2], d4[2], d6[2]}, [r0], r1
    vld4.16       {d0[3], d2[3], d4[3], d6[3]}, [r0], r1

    vld4.16       {d1[0], d3[0], d5[0], d7[0]}, [r0], r1
    vld4.16       {d1[1], d3[1], d5[1], d7[1]}, [r0], r1
    vld4.16       {d1[2], d3[2], d5[2], d7[2]}, [r0], r1
    vld4.16       {d1[3], d3[3], d5[3], d7[3]}, [r0], r1


    vdup.16       q11, r2               @Q11 = alpha
    vabd.u8       q4, q1, q2            @|p0-q0|
    vdup.16       q12, r3               @Q12 = beta
    vabd.u8       q5, q3, q2            @|q1-q0|
    vabd.u8       q6, q0, q1            @|p1-p0|
    vclt.u8       q4, q4, q11           @|p0-q0| < alpha ?
    vsubl.u8      q7, d0, d6
    vclt.u8       q5, q5, q12           @|q1-q0| < beta ?
    vsubl.u8      q8, d1, d7            @(p1 - q1)
    vclt.u8       q6, q6, q12           @|p1-p0| < beta ?
    vsubl.u8      q9, d4, d2
    vand.u8       q4, q4, q5            @|p0-q0| < alpha && |q1-q0| < beta
    vsubl.u8      q10, d5, d3           @(q0 - p0)
    vmov.u16      q14, #4
    vld1.32       {d24[0]}, [r10]       @Load ClipTable for U
    vld1.32       {d25[0]}, [r11]       @Load ClipTable for V
    rev           r6, r6                @Blocking strengths
    vand.u8       q4, q4, q6            @|p0-q0| < alpha && |q1-q0| < beta && |p1-p0| < beta

    vmov.32       d10[0], r6

    vmla.s16      q7, q9, q14
    vmla.s16      q8, q10, q14          @4*(q0 - p0) + (p1 - q1)

    vmovl.u8      q5, d10
    vsli.u16      d10, d10, #8
    vtbl.8        d12, {d24}, d10       @tC0 for U
    vtbl.8        d13, {d25}, d10       @tC0 for V
    vzip.8        d12, d13
    vmovl.u16     q5, d10
    vsli.u32      q5, q5, #16
    vmov.u8       q12, #1
    vadd.u8       q6, q6, q12           @tC0 + 1
    vcge.u8       q5, q5, q12           @u4_bS > 0 ?
    vand.u8       q4, q4, q5            @|p0-q0| < alpha && |q1-q0| < beta && |p1-p0| < beta && u4_bs != 0

    @ Q0 - Q3(inputs),
    @ Q4 (|p0-q0| < alpha && |q1-q0| < beta && |p1-p0| < beta && u4_bs != 0),
    @ Q6 (tC)

    vrshr.s16     q7, q7, #3
    vrshr.s16     q8, q8, #3            @(((q0 - p0) << 2) + (p1 - q1) + 4) >> 3)

    vcgt.s16      q9, q7, #0
    vcgt.s16      q10, q8, #0
    vmovn.i16     d18, q9
    vmovn.i16     d19, q10              @Q9 = sign(delta)
    vabs.s16      q7, q7
    vabs.s16      q8, q8
    vmovn.u16     d14, q7
    vmovn.u16     d15, q8
    vmin.u8       q7, q7, q6            @Q7 = |delta|

    vqadd.u8      q10, q1, q7           @p0+|delta|
    vqadd.u8      q11, q2, q7           @q0+|delta|
    vqsub.u8      q12, q1, q7           @p0-|delta|
    vqsub.u8      q13, q2, q7           @q0-|delta|

    vbit          q12, q10, q9          @p0 + delta
    vbit          q11, q13, q9          @q0 - delta

    vbit          q1, q12, q4
    vbit          q2, q11, q4

    vst4.16       {d0[0], d2[0], d4[0], d6[0]}, [r12], r1
    vst4.16       {d0[1], d2[1], d4[1], d6[1]}, [r12], r1
    vst4.16       {d0[2], d2[2], d4[2], d6[2]}, [r12], r1
    vst4.16       {d0[3], d2[3], d4[3], d6[3]}, [r12], r1

    vst4.16       {d1[0], d3[0], d5[0], d7[0]}, [r12], r1
    vst4.16       {d1[1], d3[1], d5[1], d7[1]}, [r12], r1
    vst4.16       {d1[2], d3[2], d5[2], d7[2]}, [r12], r1
    vst4.16       {d1[3], d3[3], d5[3], d7[3]}, [r12], r1
    vpop          {d8 - d15}
    ldmfd         sp!, {r4-r7, r10-r12, pc}



@**
@*******************************************************************************
@*
@* @brief
@*     Performs filtering of a chroma block vertical edge when the
@*     boundary strength is set to 4 on calling twice in high profile
@*
@* @par Description:
@*    This operation is described in  Sec. 8.7.2.4 under the title
@*    "Filtering process for edges for bS equal to 4" in ITU T Rec H.264.
@*
@* @param[in] r0 - pu1_src
@*  Pointer to the src sample q0
@*
@* @param[in] r1 - src_strd
@*  Source stride
@*
@* @param[in] r2 - alpha_cb
@*  Alpha Value for the boundary in U
@*
@* @param[in] r3 - beta_cb
@*  Beta Value for the boundary in U
@*
@* @param[in] sp(0) - alpha_cr
@*  Alpha Value for the boundary in V
@*
@* @param[in] sp(4) - beta_cr
@*  Beta Value for the boundary in V
@*
@* @returns
@*  None
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@*

    .global ih264_deblk_chroma_vert_bs4_mbaff_a9

ih264_deblk_chroma_vert_bs4_mbaff_a9:

    stmfd         sp!, {r4, r5, r12, r14}

    sub           r0, r0, #4            @point r0 to p1u of row0.
    mov           r12, r0               @keep a back up of r0 for buffer write
    ldrd          r4, r5, [sp, #16]     @R4 = alpha_cr , R5 = beta_cr
    add           r2, r2, r4, lsl #8
    add           r3, r3, r5, lsl #8
    vpush         {d8 - d15}
    vld4.16       {d0[0], d1[0], d2[0], d3[0]}, [r0], r1
    vld4.16       {d0[1], d1[1], d2[1], d3[1]}, [r0], r1
    vld4.16       {d0[2], d1[2], d2[2], d3[2]}, [r0], r1
    vld4.16       {d0[3], d1[3], d2[3], d3[3]}, [r0], r1

    vdup.16       d11, r2               @D11 = alpha
    vdup.16       d12, r3               @D12 = beta
    vmov.i8       d31, #2

    vabd.u8       d4, d1, d2            @|p0-q0|
    vabd.u8       d5, d3, d2            @|q1-q0|
    vabd.u8       d6, d0, d1            @|p1-p0|
    vaddl.u8      q14, d1, d3           @(p0 + q1)
    vclt.u8       d4, d4, d11           @|p0-q0| < alpha ?
    vclt.u8       d5, d5, d12           @|q1-q0| < beta ?
    vclt.u8       d6, d6, d12           @|p1-p0| < beta ?
    vmlal.u8      q14, d0, d31          @2*p1 + (p0 + q1)
    vaddl.u8      q13, d0, d2           @(p1 + q0)
    vand.u8       d4, d4, d5            @|p0-q0| < alpha && |q1-q0| < beta
    vmlal.u8      q13, d3, d31          @2*q1 + (p1 + q0)

    vrshrn.i16    d7, q14, #2           @(2*p1 + (p0 + q1) + 2) >> 2
    vand.u8       d4, d4, d6            @|p0-q0| < alpha && |q1-q0| < beta && |p1-p0| < beta
    vrshrn.i16    d9, q13, #2           @(2*q1 + (p1 + q0) + 2) >> 2

    vbit          d1, d7, d4
    vbit          d2, d9, d4

    vst4.16       {d0[0], d1[0], d2[0], d3[0]}, [r12], r1
    vst4.16       {d0[1], d1[1], d2[1], d3[1]}, [r12], r1
    vst4.16       {d0[2], d1[2], d2[2], d3[2]}, [r12], r1
    vst4.16       {d0[3], d1[3], d2[3], d3[3]}, [r12], r1
    vpop          {d8 - d15}
    ldmfd         sp!, {r4, r5, r12, pc}



@**
@*******************************************************************************
@*
@* @brief
@*     Performs filtering of a chroma block vertical edge for cases where the
@*     boundary strength is less than 4 on calling twice in high profile
@*
@* @par Description:
@*    This operation is described in  Sec. 8.7.2.4 under the title
@*    "Filtering process for edges for bS equal to 4" in ITU T Rec H.264.
@*
@* @param[in] r0 - pu1_src
@*  Pointer to the src sample q0
@*
@* @param[in] r1 - src_strd
@*  Source stride
@*
@* @param[in] r2 - alpha_cb
@*  Alpha Value for the boundary in U
@*
@* @param[in] r3 - beta_cb
@*  Beta Value for the boundary in U
@*
@* @param[in] sp(0) - alpha_cr
@*  Alpha Value for the boundary in V
@*
@* @param[in] sp(4) - beta_cr
@*  Beta Value for the boundary in V
@*
@* @param[in] sp(8) - u4_bs
@*  Packed Boundary strength array
@*
@* @param[in] sp(12) - pu1_cliptab_cb
@*  tc0_table for U
@*
@* @param[in] sp(16) - pu1_cliptab_cr
@*  tc0_table for V
@*
@* @returns
@*  None
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@*

    .global ih264_deblk_chroma_vert_bslt4_mbaff_a9

ih264_deblk_chroma_vert_bslt4_mbaff_a9:

    stmfd         sp!, {r4-r6, r10-r12, r14}

    sub           r0, r0, #4            @point r0 to p1u of row0.
    mov           r12, r0               @keep a back up of r0 for buffer write

    ldrd          r4, r5, [sp, #28]     @R4 = alpha_cr , R5 = beta_cr
    add           r2, r2, r4, lsl #8
    add           r3, r3, r5, lsl #8
    ldr           r6, [sp, #36]         @R6 = u4_bs
    ldrd          r10, r11, [sp, #40]   @R10 = pu1_cliptab_cb , R11 = pu1_cliptab_cr
    vpush         {d8 - d15}
    vld4.16       {d0[0], d1[0], d2[0], d3[0]}, [r0], r1
    vld4.16       {d0[1], d1[1], d2[1], d3[1]}, [r0], r1
    vld4.16       {d0[2], d1[2], d2[2], d3[2]}, [r0], r1
    vld4.16       {d0[3], d1[3], d2[3], d3[3]}, [r0], r1

    vdup.16       d11, r2               @D11 = alpha
    vabd.u8       d4, d1, d2            @|p0-q0|
    vdup.16       d12, r3               @D12 = beta
    vabd.u8       d5, d3, d2            @|q1-q0|
    vabd.u8       d6, d0, d1            @|p1-p0|
    vclt.u8       d4, d4, d11           @|p0-q0| < alpha ?
    vclt.u8       d5, d5, d12           @|q1-q0| < beta ?
    vsubl.u8      q14, d0, d3           @(p1 - q1)
    vclt.u8       d6, d6, d12           @|p1-p0| < beta ?
    vand.u8       d4, d4, d5            @|p0-q0| < alpha && |q1-q0| < beta
    vsubl.u8      q12, d2, d1           @(q0 - p0)
    vmov.u16      q10, #4

    vld1.32       {d31[1]}, [r10]       @Load ClipTable for U
    vld1.32       {d31[0]}, [r11]       @Load ClipTable for V
    rev           r6, r6                @Blocking strengths
    vand.u8       d4, d4, d6            @|p0-q0| < alpha && |q1-q0| < beta && |p1-p0| < beta
    vmov.32       d22[0], r6
    vmla.s16      q14, q12, q10         @4*(q0 - p0) + (p1 - q1)
    vmovl.u8      q11, d22
    vsli.u16      d22, d22, #8
    vmov.u16      d13, #4
    vadd.u8       d22, d22, d13
    vtbl.8        d6, {d31}, d22        @tC0
    vmov.u8       d12, #1
    vsub.u8       d22, d22, d13
    vadd.u8       d6, d6, d12           @tC0 + 1
    vcge.u8       d5, d22, d12          @u4_bS > 0 ?
    vand.u8       d4, d4, d5            @|p0-q0| < alpha && |q1-q0| < beta && |p1-p0| < beta && u4_bs != 0

    @ D0 - D3(inputs),
    @ D4 (|p0-q0| < alpha && |q1-q0| < beta && |p1-p0| < beta && u4_bs != 0),
    @ D6 (tC)

    vrshr.s16     q14, q14, #3          @(((q0 - p0) << 2) + (p1 - q1) + 4) >> 3)

    vcgt.s16      q13, q14, #0
    vmovn.i16     d9, q13               @D9 = sign(delta)
    vabs.s16      q14, q14
    vmovn.u16     d7, q14
    vmin.u8       d7, d7, d6            @D7 = |delta|

    vqadd.u8      d10, d1, d7           @p0+|delta|
    vqadd.u8      d11, d2, d7           @q0+|delta|
    vqsub.u8      d12, d1, d7           @p0-|delta|
    vqsub.u8      d13, d2, d7           @q0-|delta|

    vbit          d12, d10, d9          @p0 + delta
    vbit          d11, d13, d9          @q0 - delta

    vbit          d1, d12, d4
    vbit          d2, d11, d4

    vst4.16       {d0[0], d1[0], d2[0], d3[0]}, [r12], r1
    vst4.16       {d0[1], d1[1], d2[1], d3[1]}, [r12], r1
    vst4.16       {d0[2], d1[2], d2[2], d3[2]}, [r12], r1
    vst4.16       {d0[3], d1[3], d2[3], d3[3]}, [r12], r1
    vpop          {d8 - d15}
    ldmfd         sp!, {r4-r6, r10-r12, pc}



