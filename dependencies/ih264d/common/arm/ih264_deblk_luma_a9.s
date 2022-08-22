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
@/*  File Name         : ih264_deblk_luma_a9.s                                */
@/*                                                                           */
@/*  Description       : Contains function definitions for deblocking luma    */
@/*                      edge. Functions are coded in NEON assembly and can   */
@/*                      be compiled using ARM RVDS.                          */
@/*                                                                           */
@/*  List of Functions : ih264_deblk_luma_vert_bs4_a9()                       */
@/*                      ih264_deblk_luma_vert_bslt4_a9()                     */
@/*                      ih264_deblk_luma_horz_bs4_a9()                       */
@/*                      ih264_deblk_luma_horz_bslt4_a9()                     */
@/*                      ih264_deblk_luma_vert_bs4_mbaff_a9()                 */
@/*                      ih264_deblk_luma_vert_bslt4_mbaff_a9()               */
@/*                                                                           */
@/*  Issues / Problems : None                                                 */
@/*                                                                           */
@/*  Revision History  :                                                      */
@/*                                                                           */
@/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
@/*         28 11 2013   Ittiam          Draft                                */
@/*         05 01 2015   Kaushik         Added double-call functions for      */
@/*                      Senthoor        vertical deblocking.                 */
@/*                                                                           */
@/*****************************************************************************/


.text
.p2align 2

@**
@*******************************************************************************
@*
@* @brief
@*     Performs filtering of a luma block horizontal edge for cases where the
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

    .global ih264_deblk_luma_horz_bslt4_a9

ih264_deblk_luma_horz_bslt4_a9:

    stmfd         sp!, {r4-r7, lr}

    ldrd          r4, r5, [sp, #0x14]   @r4 = ui_Bs , r5 = *puc_ClpTab
    vpush         {d8 - d15}
    sub           r0, r0, r1, lsl #1    @R1 = uc_Horizonpad
    sub           r0, r0, r1            @r0 pointer to p2
    rev           r4, r4                @
    vld1.8        {q5}, [r0], r1        @p2 values are loaded into q5
    vmov.32       d12[0], r4            @d12[0] = ui_Bs
    mov           r6, r0                @keeping backup of pointer to p1
    vld1.8        {q4}, [r0], r1        @p1 values are loaded into q4
    mov           r7, r0                @keeping backup of pointer to p0
    vld1.8        {q3}, [r0], r1        @p0 values are loaded into q3
    vmovl.u8      q6, d12               @q6 = uc_Bs in each 16 bt scalar
    vld1.8        {q0}, [r0], r1        @q0 values are loaded into q0
    vabd.u8       q13, q4, q3           @Q13 = ABS(p1 - p0)
    vld1.8        {q1}, [r0], r1        @q1 values are loaded into q1
    vabd.u8       q11, q3, q0           @Q11 = ABS(p0 - q0)
    vld1.32       d16[0], [r5]          @D16[0] contains cliptab
    vabd.u8       q12, q1, q0           @Q12 = ABS(q1 - q0)
    vld1.8        {q2}, [r0], r1        @q2 values are loaded into q2
    vtbl.8        d14, {d16}, d12       @
    vdup.8        q10, r2               @Q10 contains alpha
    vdup.8        q8, r3                @Q8 contains beta
    vmovl.u16     q6, d12               @
    vmovl.u16     q7, d14               @
    vabd.u8       q14, q5, q3           @Q14 = Ap = ABS(p2 - p0)
    vabd.u8       q15, q2, q0           @Q15 = Aq = ABS(q2 - q0)
    vcgt.s32      q6, q6, #0            @Q6 = (us_Bs > 0)
    vsli.32       q7, q7, #8            @
    vcge.u8       q9, q11, q10          @Q9 = ( ABS(p0 - q0) >= Alpha )
    vcge.u8       q12, q12, q8          @Q12=( ABS(q1 - q0) >= Beta )
    vcge.u8       q13, q13, q8          @Q13=( ABS(p1 - p0) >= Beta )
    vcgt.u8       q10, q8, q14          @Q10=(Ap<Beta)
    vcgt.u8       q11, q8, q15          @Q11=(Aq<Beta)
    vsli.32       q7, q7, #16           @Q7  = C0
    vorr          q9, q9, q12           @Q9 = ( ABS(p0 - q0) >= Alpha ) | ( ABS(q1 - q0) >= Beta )
    vsubl.u8      q15, d1, d7           @
    vsubl.u8      q12, d0, d6           @Q15,Q12 = (q0 - p0)
    vorr          q9, q9, q13           @Q9 = ( ABS(p0 - q0) >= Alpha ) | ( ABS(q1 - q0) >= Beta ) | ( ABS(p1 - p0) >= Beta )
    vsubl.u8      q14, d8, d2           @Q14 = (p1 - q1)L
    vshl.i16      q13, q15, #2          @Q13 = (q0 - p0)<<2
    vshl.i16      q12, q12, #2          @Q12 = (q0 - p0)<<2
    vsubl.u8      q15, d9, d3           @Q15 = (p1 - q1)H
    vbic          q6, q6, q9            @final condition
    vadd.i16      q12, q12, q14         @
    vadd.i16      q13, q13, q15         @Q13,Q12 = [ (q0 - p0)<<2 ] + (p1 - q1)
    vsub.i8       q9, q7, q10           @Q9 = C0 + (Ap < Beta)
    vrhadd.u8     q8, q3, q0            @Q8 = ((p0+q0+1) >> 1)
    vqrshrn.s16   d24, q12, #3          @
    vqrshrn.s16   d25, q13, #3          @Q12 = i_macro = (((q0 - p0)<<2) + (p1 - q1) + 4)>>3
    vsub.i8       q9, q9, q11           @Q9 = C0 + (Ap < Beta) + (Aq < Beta)
    vand.i8       q10, q10, q6          @
    vand.i8       q11, q11, q6          @
    vabs.s8       q13, q12              @Q13 = ABS (i_macro)
    vaddl.u8      q14, d17, d11         @
    vaddl.u8      q5, d16, d10          @Q14,Q5 = p2 + (p0+q0+1)>>1
    vaddl.u8      q15, d17, d5          @
    vmin.u8       q9, q13, q9           @Q9 = delta = (ABS(i_macro) > C) ? C : ABS(i_macro)
    vshll.u8      q13, d9, #1           @
    vaddl.u8      q2, d16, d4           @Q15,Q2 = q2 + (p0+q0+1)>>1
    vshll.u8      q8, d8, #1            @Q13,Q8 = (p1<<1)
    vand          q9, q9, q6            @Making delta zero in places where values shouldn be filterd
    vsub.i16      q14, q14, q13         @Q14,Q5 = [p2 + (p0+q0+1)>>1] - (p1<<1)
    vsub.i16      q5, q5, q8            @
    vshll.u8      q8, d2, #1            @
    vshll.u8      q13, d3, #1           @Q13,Q8 = (q1<<1)
    vqshrn.s16    d29, q14, #1          @
    vqshrn.s16    d28, q5, #1           @Q14 = i_macro_p1
    vsub.i16      q2, q2, q8            @
    vsub.i16      q15, q15, q13         @Q15,Q2  = [q2 + (p0+q0+1)>>1] - (q1<<1)
    vneg.s8       q13, q7               @Q13 = -C0
    vmin.s8       q14, q14, q7          @Q14 = min(C0,i_macro_p1)
    vcge.s8       q12, q12, #0          @Q12 = (i_macro >= 0)
    vqshrn.s16    d31, q15, #1          @
    vqshrn.s16    d30, q2, #1           @Q15 = i_macro_q1
    vmax.s8       q14, q14, q13         @Q14 = max( - C0 , min(C0, i_macro_p1) )
    vqadd.u8      q8, q3, q9            @Q8  = p0 + delta
    vqsub.u8      q3, q3, q9            @Q3 = p0 - delta
    vmin.s8       q15, q15, q7          @Q15 = min(C0,i_macro_q1)
    vand.i8       q14, q10, q14         @condition check Ap<beta
    vqadd.u8      q7, q0, q9            @Q7 = q0 + delta
    vqsub.u8      q0, q0, q9            @Q0   = q0 - delta
    vmax.s8       q15, q15, q13         @Q15 = max( - C0 , min(C0, i_macro_q1) )
    vbif          q8, q3, q12           @Q8  = (i_macro >= 0 ) ? (p0+delta) : (p0-delta)
    vbif          q0, q7, q12           @Q0  = (i_macro >= 0 ) ? (q0-delta) : (q0+delta)
    vadd.i8       q14, q14, q4          @
    vand.i8       q15, q11, q15         @condition check Aq<beta
    vst1.8        {q8}, [r7], r1        @writting back filtered value of p0
    vadd.i8       q15, q15, q1          @
    vst1.8        {q0}, [r7], r1        @writting back filtered value of q0
    vst1.8        {q14}, [r6]           @writting back filtered value of p1
    vst1.8        {q15}, [r7], r1       @writting back filtered value of q1
    vpop          {d8 - d15}
    ldmfd         sp!, {r4-r7, pc}



@**
@*******************************************************************************
@*
@* @brief
@*     Performs filtering of a luma block horizontal edge when the
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

    .global ih264_deblk_luma_horz_bs4_a9

ih264_deblk_luma_horz_bs4_a9:

    @ Back up necessary registers on stack
    stmfd         sp!, {r12, r14}
    vpush         {d8 - d15}
    @ Init
    vdup.8        q0, r2                @duplicate alpha
    sub           r12, r0, r1           @pointer to p0 = q0 - src_strd
    vdup.8        q1, r3                @duplicate beta
    sub           r14, r0, r1, lsl#1    @pointer to p1 = q0 - src_strd*2
    sub           r2, r0, r1, lsl#2     @pointer to p3 = q0 - src_strd*4
    sub           r3, r14, r1           @pointer to p2 = p1 - src_strd

    @ Load Data
    vld1.8        {d4, d5}, [r0], r1    @load q0 to Q2, q0 = q0 + src_strd
    vld1.8        {d6, d7}, [r12]       @load p0 to Q3
    vld1.8        {d8, d9}, [r0], r1    @load q1 to Q4, q0 = q0 + src_strd
    vld1.8        {d10, d11}, [r14]     @load p1 to Q5

    @ Filter Decision
    vabd.u8       q6, q2, q3            @ABS(p0 - q0)
    vabd.u8       q7, q4, q2            @ABS(q1 - q0)
    vabd.u8       q8, q5, q3            @ABS(p1 - p0)
    vcge.u8       q9, q6, q0            @ABS(p0 - q0) >= Alpha
    vcge.u8       q7, q7, q1            @ABS(q1 - q0) >= Beta
    vcge.u8       q8, q8, q1            @ABS(p1 - p0) >= Beta
    vmov.i8       q10, #2
    vorr          q9, q9, q7            @ABS(p0 - q0) >= Alpha || ABS(q1 - q0) >= Beta
    vld1.8        {d14, d15}, [r0], r1  @load q2 to Q7, q0 = q0 + src_strd
    vorr          q9, q9, q8            @ABS(p0 - q0) >= Alpha || ABS(q1 - q0) >= Beta || ABS(p1 - p0) >= Beta
    vsra.u8       q10, q0, #2           @((Alpha >> 2) + 2)
    vabd.u8       q11, q7, q2           @Aq  = ABS(q2 - q0)
    vaddl.u8      q12, d4, d6           @p0+q0 L
    vaddl.u8      q13, d5, d7           @p0+q0 H
    vclt.u8       q11, q11, q1          @Aq < Beta
    vclt.u8       q10, q6, q10          @(ABS(p0 - q0) <((Alpha >>2) + 2))

    @ Deblock Filtering q0', q1', q2'
    vaddw.u8      q14, q12, d8          @p0+q0+q1 L
    vaddw.u8      q15, q13, d9          @p0+q0+q1 H
    vand          q11, q11, q10         @(Aq < Beta && ABS(p0 - q0) <((Alpha >>2) + 2))
    @ q0' if (Aq < Beta && ABS(p0 - q0) <((Alpha >>2) + 2)) TRUE
    vadd.i16      q8, q14, q14          @2*(p0+q0+q1)L
    vadd.i16      q0, q15, q15          @2*(p0+q0+q1)H
    vaddw.u8      q8, q8, d14           @2*(p0+q0+q1)+q2 L
    vaddw.u8      q0, q0, d15           @2*(p0+q0+q1)+q2 H
    vaddw.u8      q8, q8, d10           @2*(p0+q0+q1)+q2 +p1 L
    vaddw.u8      q0, q0, d11           @2*(p0+q0+q1)+q2 +p1 H
    vrshrn.u16    d12, q8, #3           @(2*(p0+q0+q1)+q2 +p1 +4)>> 3 L [q0']
    vrshrn.u16    d13, q0, #3           @(2*(p0+q0+q1)+q2 +p1 +4)>> 3 H [q0']
    @ q0" if (Aq < Beta && ABS(p0 - q0) <((Alpha >>2) + 2)) FALSE
    vaddl.u8      q8, d8, d8            @2*q1 L
    vaddl.u8      q0, d9, d9            @2*q1 H
    vaddw.u8      q8, q8, d4            @2*q1+q0 L
    vaddw.u8      q0, q0, d5            @2*q1+q0 H
    vaddw.u8      q8, q8, d10           @2*q1+q0+p1  L
    vaddw.u8      q0, q0, d11           @2*q1+q0+p1 H
    vrshrn.u16    d16, q8, #2           @(2*q1+q0+p1+2)>>2 L [q0"]
    vrshrn.u16    d17, q0, #2           @(2*q1+q0+p1+2)>>2 H [q0"]
    @ q1'
    vaddw.u8      q14, q14, d14         @p0+q0+q1+q2 L
    vaddw.u8      q15, q15, d15         @p0+q0+q1+q2 H
    vld1.8        {q0}, [r0], r1        @load q3 to Q0, q0 = q0 + src_strd
    vbit          q8, q6, q11           @choosing between q0' and q0" depending on condn
    sub           r0, r0, r1, lsl #2    @pointer to q0
    vbic          q11, q11, q9          @((ABS(p0 - q0) >= Alpha  || ABS(q1 - q0) >= Beta || ABS(p1 - p0) >= Beta))
                                        @ && (Aq < Beta && ABS(p0 - q0) <((Alpha >>2) + 2))
    vrshrn.u16    d12, q14, #2          @(p0+q0+q1+q2+2)>>2 L [q1']
    vrshrn.u16    d13, q15, #2          @(p0+q0+q1+q2+2)>>2 H [q1']
    vbif          q2, q8, q9            @choose q0 or filtered q0
    @ q2'
    vaddl.u8      q8, d14, d0           @q2+q3,L
    vaddl.u8      q0, d15, d1           @q2+q3,H
    vadd.i16      q14, q14, q8          @p0+q0+q1+2*q2+q3 L
    vst1.8        {d4, d5}, [r0], r1    @store q0
    vadd.i16      q15, q15, q0          @p0+q0+q1+2*q2+q3 H
    vadd.i16      q14, q14, q8          @p0+q0+q1+3*q2+2*q3 L
    vadd.i16      q15, q15, q0          @p0+q0+q1+3*q2+2*q3 H
    vrshrn.u16    d0, q14, #3           @(p0+q0+q1+3*q2+2*q3+4)>>3 L [q2']
    vrshrn.u16    d1, q15, #3           @(p0+q0+q1+3*q2+2*q3+4)>>3 H [q2']
    vld1.8        {d30, d31}, [r3]      @load p2 to Q15
    vbif          q6, q4, q11           @choose q1 or filtered value of q1

    vabd.u8       q8, q15, q3           @Ap,ABS(p2 - p0)
    vaddw.u8      q12, q12, d10         @p0+q0+p1 L
    vbif          q0, q7, q11           @choose q2 or filtered q2
    vaddw.u8      q13, q13, d11         @p0+q0+p1 H
    vst1.8        {d12, d13}, [r0], r1  @store q1
    vclt.u8       q8, q8, q1            @Ap < Beta
    vadd.i16      q14, q12, q12         @2*(p0+q0+p1) L
    vadd.i16      q2, q13, q13          @2*(p0+q0+p1) H
    vst1.8        {d0, d1}, [r0], r1    @store q2
    vand          q10, q10, q8          @((Ap < Beta) && (ABS(p0 - q0) <((Alpha >>2) + 2)))
    vaddw.u8      q14, q14, d30         @2*(p0+q0+p1)+p2 l
    vaddw.u8      q2, q2, d31           @2*(p0+q0+p1)+p2 H
    vaddw.u8      q14, q14, d8          @2*(p0+q0+p1)+p2+q1 L
    vaddw.u8      q2, q2, d9            @2*(p0+q0+p1)+p2+q1 H
    vrshrn.u16    d28, q14, #3          @(2*(p0+q0+p1)+p2+q1+4)>>3  L,p0'
    vrshrn.u16    d29, q2, #3           @(2*(p0+q0+p1)+p2+q1+4)>>3  H,p0'
    vmov.i8       d0, #2
    vmov.i16      d1, #2
    vaddl.u8      q1, d6, d8            @p0+q1      L
    vmlal.u8      q1, d10, d0           @2*p1+p0+q1 L
    vaddl.u8      q8, d7, d9            @p0+q1  H
    vmlal.u8      q8, d11, d0           @2*p1+p0+q1 H
    vaddw.u8      q6, q12, d30          @(p0+q0+p1) +p2 L
    vld1.8        {d24, d25}, [r2]      @load p3,Q12
    vaddw.u8      q2, q13, d31          @(p0+q0+p1) +p2 H
    vaddl.u8      q4, d30, d24          @p2+p3 L
    vrshrn.u16    d26, q6, #2           @((p0+q0+p1)+p2 +2)>>2,p1' L
    vrshrn.u16    d2, q1, #2            @(2*p1+p0+q1+2)>>2,p0"L
    vrshrn.u16    d27, q2, #2           @((p0+q0+p1)+p2 +2)>>2,p1' H
    vrshrn.u16    d3, q8, #2            @(2*p1+p0+q1+2)>>2,p0" H
    vaddl.u8      q8, d31, d25          @p2+p3 H
    vmla.u16      q6, q4, d1[0]         @(p0+q0+p1)+3*p2+2*p3 L
    vmla.u16      q2, q8, d1[0]         @(p0+q0+p1)+3*p2+2*p3 H
    vbic          q8, q10, q9           @((ABS(p0 - q0) >= Alpha  || ABS(q1 - q0) >= Beta || ABS(p1 - p0) >= Beta))
                                        @&& (Ap < Beta && ABS(p0 - q0) <((Alpha >>2) + 2))
    vbit          q1, q14, q10          @choosing between po' and p0"
    vrshrn.u16    d12, q6, #3           @((p0+q0+p1)+3*p2+2*p3+4)>>3 L p2'
    vrshrn.u16    d13, q2, #3           @((p0+q0+p1)+3*p2+2*p3+4)>>3 H p2'
    vbif          q3, q1, q9            @choosing between p0 and filtered value of p0
    vbit          q5, q13, q8           @choosing between p1 and p1'
    vbit          q15, q6, q8           @choosing between p2 and p2'
    vst1.8        {d6, d7}, [r12]       @store p0
    vst1.8        {d10, d11}, [r14]     @store p1
    vst1.8        {d30, d31}, [r3]      @store p2
    vpop          {d8 - d15}
    ldmfd         sp!, {r12, pc}



@**
@*******************************************************************************
@*
@* @brief
@*     Performs filtering of a luma block vertical edge for cases where the
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

    .global ih264_deblk_luma_vert_bslt4_a9

ih264_deblk_luma_vert_bslt4_a9:

    stmfd         sp!, {r12, lr}

    sub           r0, r0, #4            @pointer uc_edgePixel-4
    ldr           r12, [sp, #8]         @r12 = ui_Bs
    ldr           r14, [sp, #12]        @r14 = *puc_ClpTab
    vpush         {d8 - d15}
    @loading p3:p2:p1:p0:q0:q1:q2:q3 for every row
    vld1.8        {d0}, [r0], r1        @row1
    vld1.8        d2, [r0], r1          @row2
    vld1.8        d4, [r0], r1          @row3
    rev           r12, r12              @reversing ui_bs
    vld1.8        d6, [r0], r1          @row4
    vmov.32       d18[0], r12           @d12[0] = ui_Bs
    vld1.32       d16[0], [r14]         @D16[0] contains cliptab
    vld1.8        d8, [r0], r1          @row5
    vmovl.u8      q9, d18               @q6 = uc_Bs in each 16 bt scalar
    vld1.8        d10, [r0], r1         @row6
    vld1.8        d12, [r0], r1         @row7
    vtbl.8        d16, {d16}, d18       @puc_ClipTab[uc_Bs]
    vld1.8        d14, [r0], r1         @row8
    vld1.8        d1, [r0], r1          @row9
    vmovl.u16     q8, d16               @
    vld1.8        d3, [r0], r1          @row10
    vld1.8        d5, [r0], r1          @row11
    vld1.8        d7, [r0], r1          @row12
    vsli.32       q8, q8, #8            @
    vld1.8        d9, [r0], r1          @row13
    vld1.8        d11, [r0], r1         @row14
    vld1.8        d13, [r0], r1         @row15
    vsli.32       q8, q8, #16           @Q8  = C0
    vld1.8        d15, [r0], r1         @row16

    @taking two 8x8 transposes
    @2X2 transposes
    vtrn.8        d0, d2                @row1 &2
    vtrn.8        d4, d6                @row3&row4
    vtrn.8        d8, d10               @row5&6
    vtrn.8        d12, d14              @row7 & 8
    vtrn.8        d1, d3                @row9 &10
    vtrn.8        d5, d7                @row11 & 12
    vtrn.8        d9, d11               @row13 &14
    vtrn.8        d13, d15              @row15 & 16
    @4x4 transposes
    vtrn.16       d2, d6                @row2 & row4
    vtrn.16       d10, d14              @row6 & row8
    vtrn.16       d3, d7                @row10 & 12
    vtrn.16       d11, d15              @row14 & row16
    vtrn.32       d6, d14               @row4 & 8
    vtrn.32       d7, d15               @row 12 & 16

    @now Q3 ->p0 and Q7->q3
    vtrn.16       d0, d4                @row1 & 3
    vtrn.16       d8, d12               @row 5 & 7
    vtrn.16       d1, d5                @row9 & row11
    vtrn.16       d9, d13               @row13 & row15
    vtrn.32       d0, d8                @row1 & row5
    vtrn.32       d1, d9                @row9 & 13

    @now Q0->p3 & Q4->q0
    @starting processing as p0 and q0 are now ready
    vtrn.32       d2, d10               @row2 &6
    vrhadd.u8     q10, q3, q4           @((p0 + q0 + 1) >> 1)
    vtrn.32       d3, d11               @row10&row14
    vmov.i8       d19, #2
    @now Q1->p2     & Q5->q1
    vtrn.32       d4, d12               @row3 & 7
    vabd.u8       q11, q3, q4           @ABS(p0 - q0)
    vtrn.32       d5, d13               @row11 & row15
    vaddl.u8      q12, d20, d2          @(p2 + ((p0 + q0 + 1) >> 1) L
    @now            Q2->p1,Q6->q2
    vaddl.u8      q13, d21, d3          @(p2 + ((p0 + q0 + 1) >> 1) H
    vmlsl.u8      q12, d4, d19          @(p2 + ((p0 + q0 + 1) >> 1) - (p1 << 1)) L
    vmlsl.u8      q13, d5, d19          @(p2 + ((p0 + q0 + 1) >> 1) - (p1 << 1)) H
    vdup.8        q14, r2               @alpha
    vcle.u8       q11, q14, q11         @ABS(p0 - q0) >= Alpha(Alpha <=ABS(p0 - q0))
    vdup.i8       q14, r3               @beta
    vabd.u8       q15, q5, q4           @ABS(q1 - q0)
    vqshrn.s16    d24, q12, #1          @((p2 + ((p0 + q0 + 1) >> 1) - (p1 << 1)) >> 1) L
    vqshrn.s16    d25 , q13, #1         @((p2 + ((p0 + q0 + 1) >> 1) - (p1 << 1)) >> 1) H
    vcge.u8       q15, q15, q14         @ABS(q1 - q0) >= Beta
    vabd.u8       q13, q2, q3           @ABS(p1 - p0)
    vmin.s8       q12, q12, q8          @min(deltap1 ,C0)
    vorr          q11, q11, q15         @ABS(q1 - q0) >= Beta ||ABS(p0 - q0) >= Alpha
    vneg.s8       q15, q8               @-C0
    vcge.u8       q13, q13, q14         @ABS(p1 - p0) >= Beta
    vmax.s8       q12, q12, q15         @max(deltap1,-C0)
    vorr          q11, q11, q13         @ABS(p0 - q0) >= Alpha  || ABS(q1 - q0) >= Beta ||  ABS(p1 - p0) >= Beta)
    vmovl.u16     q13, d18              @ui_bs
    vaddl.u8      q9, d20, d12          @q2 + ((p0 + q0 + 1) >> 1) L
    vceq.u32      q13, q13, #0          @ui_bs == 0
    vsubw.u8      q9, q9, d10           @(q2 + ((p0 + q0 + 1) >> 1) - q1) L
    vaddl.u8      q10, d21, d13         @q2 + ((p0 + q0 + 1) >> 1) H
    vsubw.u8      q9, q9, d10           @(q2 + ((p0 + q0 + 1) >> 1) - 2*q1)L
    vsubw.u8      q10, q10, d11         @(q2 + ((p0 + q0 + 1) >> 1) - q1) H
    vorr          q13, q13, q11         @(ABS(p0 - q0) >= Alpha  || ABS(q1 - q0) >= Beta || ABS(p1 - p0) >= Beta)) &&(ui_bs)
    vsubw.u8      q10, q10, d11         @(q2 + ((p0 + q0 + 1) >> 1) - 2*q1) H
    vqshrn.s16    d18, q9, #1           @((q2 + ((p0 + q0 + 1) >> 1) - (q1 << 1)) >> 1) L
    vabd.u8       q11, q1, q3           @Ap = ABS(p2 - p0)
    vqshrn.s16    d19, q10, #1          @((q2 + ((p0 + q0 + 1) >> 1) - (q1 << 1)) >> 1) H
    vabd.u8       q10, q6, q4           @Aq= ABS(q2 - q0)
    vclt.u8       q11, q11, q14         @Ap < Beta
    vmin.s8       q9, q9, q8            @min(delatq1,C0)
    vclt.u8       q10, q10, q14         @Aq <Beta
    vsubl.u8      q14, d8, d6           @(q0 - p0) L
    vmax.s8       q9, q9, q15           @max(deltaq1,-C0)
    vsubl.u8      q15, d9, d7           @(q0 - p0) H
    vshl.s16      q14, q14, #2          @(q0 - p0)<<2 L
    vsub.u8       q8, q8, q11           @C0 + (Ap < Beta)
    vshl.s16      q15, q15, #2          @(q0 - p0) << 2) H
    vaddw.u8      q14, q14, d4          @((q0 - p0) << 2) + (p1  L
    vaddw.u8      q15, q15, d5          @((q0 - p0) << 2) + (p1 H
    vsubw.u8      q14, q14, d10         @((q0 - p0) << 2) + (p1 - q1) L
    vsubw.u8      q15, q15, d11         @((q0 - p0) << 2) + (p1 - q1) H
    vbic          q11, q11, q13         @final condition for p1
    vrshrn.s16    d28, q14, #3          @delta = ((((q0 - p0) << 2) + (p1 - q1) + 4) >> 3); L
    vrshrn.s16    d29, q15, #3          @delta = ((((q0 - p0) << 2) + (p1 - q1) + 4) >> 3) H
    vsub.u8       q8, q8, q10           @C0 + (Ap < Beta) + (Aq < Beta)
    vbic          q10, q10, q13         @final condition for q1
    vabs.s8       q15, q14              @abs(delta)
    vand          q12, q12, q11         @delatp1
    vand          q9, q9, q10           @delta q1
    vmin.u8       q15, q15, q8          @min((abs(delta),C)
    vadd.i8       q2, q2, q12           @p1+deltap1
    vadd.i8       q5, q5, q9            @q1+deltaq1
    vbic          q15, q15, q13         @abs(delta) of pixels to be changed only
    vcge.s8       q14, q14, #0          @sign(delta)
    vqsub.u8      q11, q3, q15          @clip(p0-delta)
    vtrn.8        d0, d2                @row1 &2
    vqadd.u8      q3, q3, q15           @clip(p0+delta)
    vtrn.8        d1, d3                @row9 &10
    vqadd.u8      q12, q4, q15          @clip(q0+delta)
    vtrn.8        d12, d14              @row7 & 8
    vqsub.u8      q4, q4, q15           @clip(q0-delta)
    vtrn.8        d13, d15              @row15 & 16
    vbif          q3, q11, q14          @p0
    vbif          q4, q12, q14          @q0
    vtrn.8        d4, d6                @row3&row4
    vtrn.8        d8, d10               @row5&6
    vtrn.8        d5, d7                @row11 & 12
    vtrn.8        d9, d11               @row13 &14
    vtrn.16       d2, d6                @row2 & row4
    vtrn.16       d10, d14              @row6 & row8
    vtrn.16       d3, d7                @row10 & 12
    vtrn.16       d11, d15              @row14 & row16
    vtrn.32       d6, d14               @row4 & 8
    vtrn.32       d7, d15               @row 12 & 16
    @now Q3 ->p0 and Q7->q3
    vtrn.16       d0, d4                @row1 & 3
    vtrn.16       d8, d12               @row 5 & 7
    vtrn.16       d1, d5                @row9 & row11
    vtrn.16       d9, d13               @row13 & row15
    sub           r0, r0, r1, lsl#4     @restore pointer
    vtrn.32       d0, d8                @row1 & row5
    vtrn.32       d1, d9                @row9 & 13
    vtrn.32       d2, d10               @row2 &6
    vtrn.32       d3, d11               @row10&row14
    vtrn.32       d4, d12               @row3 & 7
    vtrn.32       d5, d13               @row11 & row15
    vst1.8        {d0}, [r0], r1        @row1
    vst1.8        d2, [r0], r1          @row2
    vst1.8        d4, [r0], r1          @row3
    vst1.8        d6, [r0], r1          @row4
    vst1.8        d8, [r0], r1          @row5
    vst1.8        d10, [r0], r1         @row6
    vst1.8        d12, [r0], r1         @row7
    vst1.8        d14, [r0], r1         @row8
    vst1.8        d1, [r0], r1          @row9
    vst1.8        d3, [r0], r1          @row10
    vst1.8        d5, [r0], r1          @row11
    vst1.8        d7, [r0], r1          @row12
    vst1.8        d9, [r0], r1          @row13
    vst1.8        d11, [r0], r1         @row14
    vst1.8        d13, [r0], r1         @row15
    vst1.8        d15, [r0], r1         @row16
    vpop          {d8 - d15}
    ldmfd         sp!, {r12, pc}



@**
@*******************************************************************************
@*
@* @brief
@*     Performs filtering of a luma block vertical edge when the
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

    .global ih264_deblk_luma_vert_bs4_a9

ih264_deblk_luma_vert_bs4_a9:

    stmfd         sp!, {r12, lr}
    vpush         {d8 - d15}
    sub           r0, r0, #4            @pointer uc_edgePixel-4
    @loading p3:p2:p1:p0:q0:q1:q2:q3 for every row
    vld1.8        d0, [r0], r1          @row1
    vld1.8        d2, [r0], r1          @row2
    vld1.8        d4, [r0], r1          @row3
    vld1.8        d6, [r0], r1          @row4
    vld1.8        d8, [r0], r1          @row5
    vld1.8        d10, [r0], r1         @row6
    vld1.8        d12, [r0], r1         @row7
    vld1.8        d14, [r0], r1         @row8
    vld1.8        d1, [r0], r1          @row9
    vld1.8        d3, [r0], r1          @row10
    vld1.8        d5, [r0], r1          @row11
    vld1.8        d7, [r0], r1          @row12
    vld1.8        d9, [r0], r1          @row13
    vld1.8        d11, [r0], r1         @row14
    vld1.8        d13, [r0], r1         @row15
    vld1.8        d15, [r0], r1         @row16
    @taking two 8x8 transposes
    @2X2 transposes
    vtrn.8        d0, d2                @row1 &2
    vtrn.8        d4, d6                @row3&row4
    vtrn.8        d8, d10               @row5&6
    vtrn.8        d12, d14              @row7 & 8
    vtrn.8        d1, d3                @row9 &10
    vtrn.8        d5, d7                @row11 & 12
    vtrn.8        d9, d11               @row13 &14
    vtrn.8        d13, d15              @row15 & 16
    @4x4 transposes
    vtrn.16       d2, d6                @row2 & row4
    vtrn.16       d10, d14              @row6 & row8
    vtrn.16       d3, d7                @row10 & 12
    vtrn.16       d11, d15              @row14 & row16
    vtrn.32       d6, d14               @row4 & 8
    vtrn.32       d7, d15               @row 12 & 16
    @now Q3 ->p0 and Q7->q3
    vtrn.16       d0, d4                @row1 & 3
    vtrn.16       d8, d12               @row 5 & 7
    vtrn.16       d1, d5                @row9 & row11
    vtrn.16       d9, d13               @row13 & row15
    vtrn.32       d0, d8                @row1 & row5
    vtrn.32       d1, d9                @row9 & 13
    @now Q0->p3 & Q4->q0
    @starting processing as p0 and q0 are now ready
    @now Q1->p2 & Q5->q1
    vpush         {q7}                  @saving in stack
    vtrn.32       d4, d12               @row3 & 7
    vmov.i16      q14, #2
    vtrn.32       d5, d13               @row11 & row15
    vaddl.u8      q8, d6, d8            @p0+q0 L
    vtrn.32       d2, d10               @row2 &6
    vaddl.u8      q9, d7, d9            @p0+q0 H
    vtrn.32       d3, d11               @row10&row14
    vaddw.u8      q10, q8, d4           @p0+q0+p1 L
    vaddw.u8      q11, q9, d5           @p0+q0+p1 H
    vaddl.u8      q12, d2, d10          @p2+q1 L
    vaddl.u8      q13, d3, d11          @p2+q1 H
    vmla.u16      q12, q10, q14         @p2 + X2(p1) + X2(p0) + X2(q0) + q1 L
    vmla.u16      q13, q11, q14         @p2 + X2(p1) + X2(p0) + X2(q0) + q1 H
    vmov.i8       q14, #2
    vaddw.u8      q8, q10, d2           @p0+q0+p1+p2 L
    vaddw.u8      q9, q11, d3           @p0+q0+p1+p2 H
    vdup.i8       q15, r2               @duplicate alpha
    vrshrn.u16    d20, q8, #2           @(p2 + p1 + p0 + q0 + 2) >> 2)L p1'
    vrshrn.u16    d21, q9, #2           @(p2 + p1 + p0 + q0 + 2) >> 2)H p1'
    vabd.u8       q11, q3, q4           @ABD(p0-q0)
    vsra.u8       q14, q15, #2          @alpha >>2 +2
    vabd.u8       q15, q1, q3           @Ap = ABD(p2-p0)
    vrshrn.u16    d24, q12, #3          @((p2 + X2(p1) + X2(p0) + X2(q0) + q1 + 4) >> 3) L p0'
    vrshrn.u16    d25, q13, #3          @((p2 + X2(p1) + X2(p0) + X2(q0) + q1 + 4) >> 3) H p0'
    vdup.i8       q13, r3               @beta
    vcgt.u8       q14, q14, q11         @ABS(p0 - q0) <((Alpha >>2) + 2)
    vaddl.u8      q11, d6, d10          @p0+q1 L
    vcgt.u8       q7, q13, q15          @beta>Ap
    vaddl.u8      q15, d7, d11          @p0+q1 H
    vaddw.u8      q11, q11, d4          @p0+q1+p1 L
    vaddw.u8      q15, q15, d5          @p0+q1+p1 H
    vaddw.u8      q11, q11, d4          @p0+q1+2*p1 L
    vaddw.u8      q15, q15, d5          @p0+q1+2*p1 H
    vand          q7, q7, q14           @(Ap < Beta && ABS(p0 - q0) <((Alpha >>2) + 2)
    vrshrn.u16    d22, q11, #2          @((X2(p1) + p0 + q1 + 2) >> 2) L p0"
    vrshrn.u16    d23, q15, #2          @((X2(p1) + p0 + q1 + 2) >> 2) H p0"
    vaddl.u8      q15, d2, d0           @p2+p3 L
    vbif          q12, q11, q7          @p0' or p0 "
    vaddl.u8      q11, d3, d1           @p2+p3 H
    vadd.u16      q15, q15, q15         @2*(p2+p3) L
    vadd.u16      q11, q11, q11         @2*(p2+p3)H
    vadd.u16      q8, q8, q15           @(X2(p3) + X3(p2) + p1 + p0 + q0) L
    vadd.u16      q9, q9, q11           @(X2(p3) + X3(p2) + p1 + p0 + q0) H
    vabd.u8       q15, q6, q4           @Aq = abs(q2-q0)
    vabd.u8       q11, q5, q4           @ABS(Q1-Q0)
    vrshrn.u16    d16, q8, #3           @((X2(p3) + X3(p2) + p1 + p0 + q0 + 4) >> 3); L p2'
    vrshrn.u16    d17, q9, #3           @((X2(p3) + X3(p2) + p1 + p0 + q0 + 4) >> 3); H p2'
    vabd.u8       q9, q2, q3            @ABS(p1-p0)
    vcgt.u8       q15, q13, q15         @Aq < Beta
    vcge.u8       q11, q11, q13         @ABS(q1 - q0) >= Beta
    vcge.u8       q9, q9, q13           @ABS(p1 - p0) >= beta
    vdup.i8       q13, r2               @duplicate alpha
    vand          q15, q15, q14         @(Aq < Beta && ABS(p0 - q0) <((Alpha >>2) + 2))
    vabd.u8       q14, q3, q4           @abs(p0-q0)
    vorr          q11, q11, q9          @ABS(p1 - p0) >= Beta || ABS(q1 - q0) >= Beta
    vaddl.u8      q9, d6, d8            @p0+q0 L
    vcge.u8       q14, q14, q13         @ABS(p0 - q0) >= Alpha
    vaddl.u8      q13, d7, d9           @p0+q0 H
    vaddw.u8      q9, q9, d10           @p0+q0+q1 L
    vorr          q11, q11, q14         @ABS(p1 - p0) >= Beta || ABS(q1 - q0) >= Beta||ABS(p0 - q0) >= Alpha
    vaddw.u8      q13, q13, d11         @p0+q0+q1 H
    vbic          q7, q7, q11           @final condn for p's
    vmov.i8       q14, #2
    vbif          q3, q12, q11          @final p0
    vbit          q1, q8, q7            @final p2
    vbif          q10, q2, q7           @final p1
    vaddl.u8      q12, d8, d4           @q0+p1 L
    vmlal.u8      q12, d10, d28         @X2(q1) + q0 + p1 L
    vaddl.u8      q8, d9, d5            @q0+p1 H
    vmlal.u8      q8, d11, d28          @X2(q1) + q0 + p1 H
    vmov.i16      q14, #2
    vaddl.u8      q7, d4, d12           @p1+q2 L
    vmla.u16      q7, q9, q14           @p1 + X2(p0) + X2(q0) + X2(q1) + q2L
    vaddl.u8      q2, d5, d13           @p1+q2H
    vmla.u16      q2, q13, q14          @p1 + X2(p0) + X2(q0) + X2(q1) + q2H
    vrshrn.u16    d24, q12, #2          @(X2(q1) + q0 + p1 + 2) >> 2; L q0'
    vrshrn.u16    d25, q8, #2           @(X2(q1) + q0 + p1 + 2) >> 2; H q0'
    vaddw.u8      q9, q9, d12           @p0 + q0 + q1 + q2 L
    vaddw.u8      q13, q13, d13         @p0 + q0 + q1 + q2 H
    vrshrn.u16    d16, q7, #3           @(p1 + X2(p0) + X2(q0) + X2(q1) + q2 + 4) >> 3 L qo"
    vpop          {q7}
    vrshrn.u16    d17, q2, #3           @(p1 + X2(p0) + X2(q0) + X2(q1) + q2 + 4) >> 3 H qo"
    vrshrn.u16    d4, q9, #2            @p0 + q0 + q1 + q2 + 2)>>2 L q1'
    vrshrn.u16    d5, q13, #2           @p0 + q0 + q1 + q2 + 2)>>2 H q1'
    vbit          q12, q8, q15          @q0' or q0"
    vbic          q15, q15, q11         @final condn for q's
    vtrn.8        d0, d2                @row1 &2
    vbit          q5, q2, q15           @final q1
    vtrn.8        d1, d3                @row9 &10
    vaddl.u8      q8, d12, d14          @q2+q3 L
    vtrn.8        d20, d6               @row3&row4
    vaddl.u8      q2, d13, d15          @q2+q3 H
    vtrn.8        d21, d7               @row11 & 12
    vmla.u16      q9, q8, q14           @X2(q3) + X3(q2) + q1 + q0 + p0 L
    vtrn.16       d2, d6                @row2 & row4
    vmla.u16      q13, q2, q14          @X2(q3) + X3(q2) + q1 + q0 + p0 H
    vtrn.16       d3, d7                @row10 & 12
    vbif          q4, q12, q11          @final q0
    vtrn.16       d0, d20               @row1 & 3
    vrshrn.u16    d18, q9, #3           @(X2(q3) + X3(q2) + q1 + q0 + p0 + 4) >> 3; L
    vtrn.16       d1, d21               @row9 & row11
    vrshrn.u16    d19, q13, #3          @(X2(q3) + X3(q2) + q1 + q0 + p0 + 4) >> 3; H
    vtrn.8        d8, d10               @row5&6
    vbit          q6, q9, q15           @final q2
    vtrn.8        d9, d11               @row13 &14
    vtrn.8        d12, d14              @row7 & 8
    vtrn.8        d13, d15              @row15 & 16
    vtrn.16       d10, d14              @row6 & row8
    vtrn.16       d11, d15              @row14 & row16
    @now Q3 ->p0 and Q7->q3
    vtrn.16       d8, d12               @row 5 & 7
    vtrn.16       d9, d13               @row13 & row15
    sub           r0, r0, r1, lsl#4     @restore pointer
    vtrn.32       d6, d14               @row4 & 8
    vtrn.32       d7, d15               @row 12 & 16
    vtrn.32       d0, d8                @row1 & row5
    vtrn.32       d1, d9                @row9 & 13
    vtrn.32       d2, d10               @row2 &6
    vtrn.32       d3, d11               @row10&row14
    vtrn.32       d20, d12              @row3 & 7
    vtrn.32       d21, d13              @row11 & row15
    vst1.8        d0, [r0], r1          @row1
    vst1.8        d2, [r0], r1          @row2
    vst1.8        d20, [r0], r1         @row3
    vst1.8        d6, [r0], r1          @row4
    vst1.8        d8, [r0], r1          @row5
    vst1.8        d10, [r0], r1         @row6
    vst1.8        d12, [r0], r1         @row7
    vst1.8        d14, [r0], r1         @row8
    vst1.8        d1, [r0], r1          @row9
    vst1.8        d3, [r0], r1          @row10
    vst1.8        d21, [r0], r1         @row11
    vst1.8        d7, [r0], r1          @row12
    vst1.8        d9, [r0], r1          @row13
    vst1.8        d11, [r0], r1         @row14
    vst1.8        d13, [r0], r1         @row15
    vst1.8        d15, [r0], r1         @row16
    vpop          {d8 - d15}
    ldmfd         sp!, {r12, pc}



@**
@*******************************************************************************
@*
@* @brief
@*     Performs filtering of a luma block vertical edge when the
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

    .global ih264_deblk_luma_vert_bs4_mbaff_a9

ih264_deblk_luma_vert_bs4_mbaff_a9:

    stmfd         sp!, {lr}

    sub           r0, r0, #4            @pointer uc_edgePixel-4
    vpush         {d8 - d15}
    @loading [p3:p2],[p1:p0]:[q0:q1]:[q2:q3] for every row
    vld4.16       {d0[0], d2[0], d4[0], d6[0]}, [r0], r1
    vld4.16       {d0[1], d2[1], d4[1], d6[1]}, [r0], r1
    vld4.16       {d0[2], d2[2], d4[2], d6[2]}, [r0], r1
    vld4.16       {d0[3], d2[3], d4[3], d6[3]}, [r0], r1
    vld4.16       {d1[0], d3[0], d5[0], d7[0]}, [r0], r1
    vld4.16       {d1[1], d3[1], d5[1], d7[1]}, [r0], r1
    vld4.16       {d1[2], d3[2], d5[2], d7[2]}, [r0], r1
    vld4.16       {d1[3], d3[3], d5[3], d7[3]}, [r0], r1

    vuzp.8        d0, d1                @D0->p3, D1->p2
    vuzp.8        d2, d3                @D2->p1, D3->p0
    vuzp.8        d4, d5                @D4->q0, D5->q1
    vuzp.8        d6, d7                @D6->q2, D7->q3

    vmov.i16      q14, #2
    vaddl.u8      q4, d3, d4            @p0+q0
    vaddw.u8      q5, q4, d2            @p0+q0+p1
    vaddl.u8      q6, d1, d5            @p2+q1
    vmla.u16      q6, q5, q14           @p2 + X2(p1) + X2(p0) + X2(q0) + q1

    vmov.i8       d14, #2
    vaddw.u8      q4, q5, d1            @p0+q0+p1+p2
    vdup.i8       d15, r2               @duplicate alpha
    vrshrn.u16    d10, q4, #2           @(p2 + p1 + p0 + q0 + 2) >> 2) p1'
    vabd.u8       d11, d3, d4           @ABD(p0-q0)
    vsra.u8       d14, d15, #2          @alpha >>2 +2
    vabd.u8       d15, d1, d3           @Ap = ABD(p2-p0)
    vrshrn.u16    d12, q6, #3           @((p2 + X2(p1) + X2(p0) + X2(q0) + q1 + 4) >> 3) p0'
    vdup.i8       d13, r3               @beta
    vcgt.u8       d14, d14, d11         @ABS(p0 - q0) <((Alpha >>2) + 2)
    vaddl.u8      q8, d3, d5            @p0+q1
    vcgt.u8       d26, d13, d15         @beta>Ap
    vaddw.u8      q8, q8, d2            @p0+q1+p1
    vaddw.u8      q8, q8, d2            @p0+q1+2*p1
    vand          d26, d26, d14         @(Ap < Beta && ABS(p0 - q0) <((Alpha >>2) + 2)
    vrshrn.u16    d11, q8, #2           @((X2(p1) + p0 + q1 + 2) >> 2) p0"
    vbif          d12, d11, d26         @p0' or p0 "
    vaddl.u8      q9, d1, d0            @p2+p3
    vadd.u16      q9, q9, q9            @2*(p2+p3)
    vadd.u16      q4, q4, q9            @(X2(p3) + X3(p2) + p1 + p0 + q0)
    vabd.u8       d15, d6, d4           @Aq = abs(q2-q0)
    vabd.u8       d11, d5, d4           @ABS(q1-q0)
    vrshrn.u16    d8, q4, #3            @((X2(p3) + X3(p2) + p1 + p0 + q0 + 4) >> 3); p2'
    vabd.u8       d9, d2, d3            @ABS(p1-p0)
    vcgt.u8       d15, d13, d15         @Aq < Beta
    vcge.u8       d11, d11, d13         @ABS(q1 - q0) >= Beta
    vcge.u8       d9, d9, d13           @ABS(p1 - p0) >= beta
    vdup.i8       d13, r2               @duplicate alpha
    vand          d15, d15, d14         @(Aq < Beta && ABS(p0 - q0) <((Alpha >>2) + 2))
    vabd.u8       d14, d3, d4           @abs(p0-q0)
    vorr          d11, d11, d9          @ABS(p1 - p0) >= Beta || ABS(q1 - q0) >= Beta
    vcge.u8       d14, d14, d13         @ABS(p0 - q0) >= Alpha
    vaddl.u8      q10, d3, d4           @p0+q0
    vorr          d11, d11, d14         @ABS(p1 - p0) >= Beta || ABS(q1 - q0) >= Beta||ABS(p0 - q0) >= Alpha
    vaddw.u8      q10, q10, d5          @p0+q0+q1
    vbic          d26, d26, d11         @final condn for p's
    vmov.i8       d14, #2
    vbif          d3, d12, d11          @final p0
    vbit          d1, d8, d26           @final p2
    vbif          d10, d2, d26          @final p1
    vaddl.u8      q6, d4, d2            @q0+p1
    vmlal.u8      q6, d5, d14           @X2(q1) + q0 + p1

    vaddl.u8      q11, d2, d6           @p1+q2
    vmla.u16      q11, q10, q14         @p1 + X2(p0) + X2(q0) + X2(q1) + q2
    vrshrn.u16    d12, q6, #2           @(X2(q1) + q0 + p1 + 2) >> 2; q0'
    vaddw.u8      q10, q10, d6          @p0 + q0 + q1 + q2
    vrshrn.u16    d8, q11, #3           @(p1 + X2(p0) + X2(q0) + X2(q1) + q2 + 4) >> 3 qo"

    vrshrn.u16    d2, q10, #2           @p0 + q0 + q1 + q2 + 2)>>2 q1'
    vbit          d12, d8, d15          @q0' or q0"
    vbic          d15, d15, d11         @final condn for q's
    vbit          d5, d2, d15           @final q1
    vaddl.u8      q12, d6, d7           @q2+q3
    vmla.u16      q10, q12, q14         @X2(q3) + X3(q2) + q1 + q0 + p0
    vbif          d4, d12, d11          @final q0
    vrshrn.u16    d9, q10, #3           @(X2(q3) + X3(q2) + q1 + q0 + p0 + 4) >> 3;
    vbit          d6, d9, d15           @final q2
    vand          d2, d10, d10          @D0->p3, D1->p2, D2->p1, D3->p0, D4->q0, D5->q1, D6->q2, D7->q3

    vzip.8        d0, d1                @D0,D1 -> [p3:p2]
    vzip.8        d2, d3                @D2,D3 -> [p1:p0]
    vzip.8        d4, d5                @D4,D5 -> [q0:q1]
    vzip.8        d6, d7                @D6,D7 -> [q2:q3]

    sub           r0, r0, r1, lsl#3     @restore pointer

    @storing [p3:p2],[p1:p0]:[q0:q1]:[q2:q3] in every row
    vst4.16       {d0[0], d2[0], d4[0], d6[0]}, [r0], r1
    vst4.16       {d0[1], d2[1], d4[1], d6[1]}, [r0], r1
    vst4.16       {d0[2], d2[2], d4[2], d6[2]}, [r0], r1
    vst4.16       {d0[3], d2[3], d4[3], d6[3]}, [r0], r1
    vst4.16       {d1[0], d3[0], d5[0], d7[0]}, [r0], r1
    vst4.16       {d1[1], d3[1], d5[1], d7[1]}, [r0], r1
    vst4.16       {d1[2], d3[2], d5[2], d7[2]}, [r0], r1
    vst4.16       {d1[3], d3[3], d5[3], d7[3]}, [r0], r1
    vpop          {d8 - d15}
    ldmfd         sp!, {pc}



@**
@*******************************************************************************
@*
@* @brief
@*     Performs filtering of a luma block vertical edge for cases where the
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

    .global ih264_deblk_luma_vert_bslt4_mbaff_a9

ih264_deblk_luma_vert_bslt4_mbaff_a9:

    stmfd         sp!, {r12, lr}

    sub           r0, r0, #4            @pointer uc_edgePixel-4
    ldr           r12, [sp, #8]         @r12 = ui_Bs
    ldr           r14, [sp, #12]        @r14 = pu1_ClipTab
    vpush         {d8 - d15}
    @loading [p3:p2],[p1:p0]:[q0:q1]:[q2:q3] for every row
    vld4.16       {d0[0], d2[0], d4[0], d6[0]}, [r0], r1
    vld4.16       {d0[1], d2[1], d4[1], d6[1]}, [r0], r1
    vld4.16       {d0[2], d2[2], d4[2], d6[2]}, [r0], r1
    vld4.16       {d0[3], d2[3], d4[3], d6[3]}, [r0], r1
    vld4.16       {d1[0], d3[0], d5[0], d7[0]}, [r0], r1
    vld4.16       {d1[1], d3[1], d5[1], d7[1]}, [r0], r1
    vld4.16       {d1[2], d3[2], d5[2], d7[2]}, [r0], r1
    vld4.16       {d1[3], d3[3], d5[3], d7[3]}, [r0], r1

    vuzp.8        d0, d1                @D0->p3, D1->p2
    vuzp.8        d2, d3                @D2->p1, D3->p0
    vuzp.8        d4, d5                @D4->q0, D5->q1
    vuzp.8        d6, d7                @D6->q2, D7->q3

    rev           r12, r12              @reversing ui_bs
    vmov.32       d8[0], r12            @D8[0] = ui_Bs
    vld1.32       d9[0], [r14]          @D9[0] contains cliptab
    vmovl.u8      q15, d8               @D30 = ui_Bs in each 16 bt scalar
    vtbl.8        d8, {d9}, d30         @puc_ClipTab[ui_Bs]
    vsli.16       d8, d8, #8            @D8 = C0

    vrhadd.u8     d10, d3, d4           @((p0 + q0 + 1) >> 1)
    vmov.i8       d31, #2
    vabd.u8       d11, d3, d4           @ABS(p0 - q0)
    vaddl.u8      q6, d10, d1           @(p2 + ((p0 + q0 + 1) >> 1)
    vmlsl.u8      q6, d2, d31           @(p2 + ((p0 + q0 + 1) >> 1) - (p1 << 1))
    vdup.8        d14, r2               @alpha
    vcle.u8       d11, d14, d11         @ABS(p0 - q0) >= Alpha(Alpha <=ABS(p0 - q0))
    vdup.i8       d14, r3               @beta
    vabd.u8       d15, d5, d4           @ABS(q1 - q0)
    vqshrn.s16    d12, q6, #1           @((p2 + ((p0 + q0 + 1) >> 1) - (p1 << 1)) >> 1)
    vcge.u8       d15, d15, d14         @ABS(q1 - q0) >= Beta
    vabd.u8       d13, d2, d3           @ABS(p1 - p0)
    vmin.s8       d12, d12, d8          @min(deltap1 ,C0)
    vorr          d11, d11, d15         @ABS(q1 - q0) >= Beta ||ABS(p0 - q0) >= Alpha
    vneg.s8       d15, d8               @-C0
    vcge.u8       d13, d13, d14         @ABS(p1 - p0) >= Beta
    vmax.s8       d12, d12, d15         @max(deltap1,-C0)
    vorr          d11, d11, d13         @ABS(p0 - q0) >= Alpha  || ABS(q1 - q0) >= Beta ||  ABS(p1 - p0) >= Beta)
    vceq.u16      d13, d30, #0          @ui_bs == 0
    vaddl.u8      q14, d10, d6          @q2 + ((p0 + q0 + 1) >> 1)
    vsubw.u8      q14, q14, d5          @q2 + ((p0 + q0 + 1) >> 1) - q1
    vsubw.u8      q14, q14, d5          @q2 + ((p0 + q0 + 1) >> 1) - 2*q1
    vorr          d13, d13, d11         @(ABS(p0 - q0) >= Alpha  || ABS(q1 - q0) >= Beta || ABS(p1 - p0) >= Beta))
                                        @|| (ui_bs == 0)
    vqshrn.s16    d9, q14, #1           @(q2 + ((p0 + q0 + 1) >> 1) - (q1 << 1)) >> 1
    vabd.u8       d11, d1, d3           @Ap = ABS(p2 - p0)
    vabd.u8       d10, d6, d4           @Aq= ABS(q2 - q0)
    vclt.u8       d11, d11, d14         @Ap < Beta
    vmin.s8       d9, d9, d8            @min(deltaq1,C0)
    vclt.u8       d10, d10, d14         @Aq < Beta
    vmax.s8       d9, d9, d15           @max(deltaq1,-C0)
    vsubl.u8      q7, d4, d3            @q0 - p0
    vshl.s16      q7, q7, #2            @(q0 - p0) << 2
    vsub.u8       d8, d8, d11           @C0 + (Ap < Beta)
    vaddw.u8      q7, q7, d2            @((q0 - p0) << 2) + p1
    vsubw.u8      q7, q7, d5            @((q0 - p0) << 2) + (p1 - q1)
    vbic          d11, d11, d13         @final condition for p1
    vrshr.s16     q15, q7, #3           @delta = (((q0 - p0) << 2) + (p1 - q1) + 4) >> 3
    vsub.u8       d8, d8, d10           @C0 + (Ap < Beta) + (Aq < Beta)
    vbic          d10, d10, d13         @final condition for q1
    vabs.s16      q14, q15
    vmovn.i16     d15, q14              @abs(delta)
    vand          d12, d12, d11         @delatp1
    vand          d9, d9, d10           @deltaq1
    vmin.u8       d15, d15, d8          @min((abs(delta),C)
    vadd.i8       d2, d2, d12           @p1+deltap1
    vadd.i8       d5, d5, d9            @q1+deltaq1
    vbic          d15, d15, d13         @abs(delta) of pixels to be changed only
    vcge.s16      q14, q15, #0
    vmovn.i16     d14, q14              @sign(delta)
    vqsub.u8      d11, d3, d15          @clip(p0-delta)
    vqadd.u8      d3, d3, d15           @clip(p0+delta)
    vqadd.u8      d12, d4, d15          @clip(q0+delta)
    vqsub.u8      d4, d4, d15           @clip(q0-delta)
    vbif          d3, d11, d14          @p0
    vbif          d4, d12, d14          @q0

    sub           r0, r0, r1, lsl#3     @restore pointer
                                        @D0->p3, D1->p2, D2->p1, D3->p0, D4->q0, D5->q1, D6->q2, D7->q3
    vzip.8        d0, d1                @D0,D1 -> [p3:p2]
    vzip.8        d2, d3                @D2,D3 -> [p1:p0]
    vzip.8        d4, d5                @D4,D5 -> [q0:q1]
    vzip.8        d6, d7                @D6,D7 -> [q2:q3]

    @storing [p3:p2],[p1:p0]:[q0:q1]:[q2:q3] in every row
    vst4.16       {d0[0], d2[0], d4[0], d6[0]}, [r0], r1
    vst4.16       {d0[1], d2[1], d4[1], d6[1]}, [r0], r1
    vst4.16       {d0[2], d2[2], d4[2], d6[2]}, [r0], r1
    vst4.16       {d0[3], d2[3], d4[3], d6[3]}, [r0], r1
    vst4.16       {d1[0], d3[0], d5[0], d7[0]}, [r0], r1
    vst4.16       {d1[1], d3[1], d5[1], d7[1]}, [r0], r1
    vst4.16       {d1[2], d3[2], d5[2], d7[2]}, [r0], r1
    vst4.16       {d1[3], d3[3], d5[3], d7[3]}, [r0], r1
    vpop          {d8 - d15}
    ldmfd         sp!, {r12, pc}



