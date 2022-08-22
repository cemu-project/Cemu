//******************************************************************************
//*
//* Copyright (C) 2015 The Android Open Source Project
//*
//* Licensed under the Apache License, Version 2.0 (the "License");
//* you may not use this file except in compliance with the License.
//* You may obtain a copy of the License at:
//*
//* http://www.apache.org/licenses/LICENSE-2.0
//*
//* Unless required by applicable law or agreed to in writing, software
//* distributed under the License is distributed on an "AS IS" BASIS,
//* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//* See the License for the specific language governing permissions and
//* limitations under the License.
//*
//*****************************************************************************
//* Originally developed and contributed by Ittiam Systems Pvt. Ltd, Bangalore
//*/
///*****************************************************************************/
///*                                                                           */
///*  File Name         : ih264_deblk_chroma_av8.s                              */
///*                                                                           */
///*  Description       : Contains function definitions for deblocking luma    */
///*                      edge. Functions are coded in NEON assembly and can   */
///*                      be compiled using ARM RVDS.                          */
///*                                                                           */
///*  List of Functions : ih264_deblk_chroma_vert_bs4_av8()              */
///*                      ih264_deblk_chroma_vert_bslt4_av8()            */
///*                      ih264_deblk_chroma_horz_bs4_av8()              */
///*                      ih264_deblk_chroma_horz_bslt4_av8()            */
///*  Issues / Problems : None                                                 */
///*                                                                           */
///*  Revision History  :                                                      */
///*                                                                           */
///*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
///*         28 11 2013   Ittiam          Draft                                */
///*****************************************************************************/


.text
.p2align 2
.include "ih264_neon_macros.s"

///**
//*******************************************************************************
//*
//* @brief
//*     Performs filtering of a chroma block horizontal edge when the
//*     boundary strength is set to 4 in high profile
//*
//* @par Description:
//*       This operation is described in  Sec. 8.7.2.4 under the title
//*       "Filtering process for edges for bS equal to 4" in ITU T Rec H.264.
//*
//* @param[in] x0 - pu1_src
//*  Pointer to the src sample q0
//*
//* @param[in] w1 - src_strd
//*  Source stride
//*
//* @param[in] w2 - alpha_cb
//*  Alpha Value for the boundary in U
//*
//* @param[in] w3 - beta_cb
//*  Beta Value for the boundary in U
//*
//* @param[in] w4 - alpha_cr
//*    Alpha Value for the boundary in V
//*
//* @param[in] w5 - beta_cr
//*    Beta Value for the boundary in V
//*
//* @returns
//*  None
//*
//* @remarks
//*  None
//*
//*******************************************************************************
//*/

    .global ih264_deblk_chroma_horz_bs4_av8

ih264_deblk_chroma_horz_bs4_av8:

    // STMFD sp!,{x4-x6,x14}            //
    push_v_regs
    stp       x19, x20, [sp, #-16]!
    sxtw      x1, w1
    mov       x6, x5
    mov       x5, x4
    sub       x0, x0, x1, lsl #1        //x0 = uc_edgePixel pointing to p1 of chroma
    ld2       {v6.8b, v7.8b}, [x0], x1  //D6 = p1u , D7 = p1v
    mov       x4, x0                    //Keeping a backup of the pointer p0 of chroma
    ld2       {v4.8b, v5.8b}, [x0], x1  //D4 = p0u , D5 = p0v
    dup       v20.8b, w2                //D20 contains alpha_cb
    dup       v21.8b, w5                //D21 contains alpha_cr
    mov       v20.d[1], v21.d[0]
    ld2       {v0.8b, v1.8b}, [x0], x1  //D0 = q0u , D1 = q0v
    uaddl     v8.8h, v6.8b, v0.8b       //
    uaddl     v10.8h, v7.8b, v1.8b      //Q4,Q5 = q0 + p1
    movi      v31.8b, #2                //
    ld2       {v2.8b, v3.8b}, [x0]      //D2 = q1u , D3 = q1v
    mov       v0.d[1], v1.d[0]
    mov       v2.d[1], v3.d[0]
    mov       v4.d[1], v5.d[0]
    mov       v6.d[1], v7.d[0]
    uabd      v26.16b, v6.16b , v4.16b  //Q13 = ABS(p1 - p0)
    umlal     v8.8h, v2.8b, v31.8b      //
    umlal     v10.8h, v3.8b, v31.8b     //Q5,Q4 = (X2(q1U) + q0U + p1U)
    uabd      v22.16b, v4.16b , v0.16b  //Q11 = ABS(p0 - q0)
    uabd      v24.16b, v2.16b , v0.16b  //Q12 = ABS(q1 - q0)
    uaddl     v14.8h, v4.8b, v2.8b      //
    uaddl     v28.8h, v5.8b, v3.8b      //Q14,Q7 = P0 + Q1
    dup       v16.8b, w3                //D16 contains beta_cb
    dup       v17.8b, w6                //D17 contains beta_cr
    mov       v16.d[1], v17.d[0]
    umlal     v14.8h, v6.8b, v31.8b     //
    umlal     v28.8h, v7.8b, v31.8b     //Q14,Q7 = (X2(p1U) + p0U + q1U)
    cmhs      v18.16b, v22.16b, v20.16b
    cmhs      v24.16b, v24.16b, v16.16b
    cmhs      v26.16b, v26.16b, v16.16b
    rshrn     v8.8b, v8.8h, #2          //
    rshrn     v9.8b, v10.8h, #2         //Q4 = (X2(q1U) + q0U + p1U + 2) >> 2
    mov       v8.d[1], v9.d[0]
    orr       v18.16b, v18.16b , v24.16b //Q9 = ( ABS(p0 - q0) >= Alpha ) | ( ABS(q1 - q0) >= Beta )
    rshrn     v10.8b, v14.8h, #2        //
    rshrn     v11.8b, v28.8h, #2        //Q5 = (X2(p1U) + p0U + q1U + 2) >> 2
    mov       v10.d[1], v11.d[0]
    orr       v18.16b, v18.16b , v26.16b //Q9 = ( ABS(p0 - q0) >= Alpha ) | ( ABS(q1 - q0) >= Beta ) | ( ABS(p1 - p0) >= Beta )
    bit       v10.16b, v4.16b , v18.16b //
    bit       v8.16b, v0.16b , v18.16b  //
    mov       v11.d[0], v10.d[1]
    mov       v9.d[0], v8.d[1]
    st2       {v10.8b, v11.8b}, [x4], x1 //
    st2       {v8.8b, v9.8b}, [x4]      //
    // LDMFD sp!,{x4-x6,pc}                //
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret



///**
//*******************************************************************************
//*
//* @brief
//*     Performs filtering of a chroma block vertical edge when the
//*     boundary strength is set to 4 in high profile
//*
//* @par Description:
//*       This operation is described in  Sec. 8.7.2.4 under the title
//*       "Filtering process for edges for bS equal to 4" in ITU T Rec H.264.
//*
//* @param[in] x0 - pu1_src
//*  Pointer to the src sample q0
//*
//* @param[in] w1 - src_strd
//*  Source stride
//*
//* @param[in] w2 - alpha_cb
//*  Alpha Value for the boundary in U
//*
//* @param[in] w3 - beta_cb
//*  Beta Value for the boundary in U
//*
//* @param[in] w4 - alpha_cr
//*    Alpha Value for the boundary in V
//*
//* @param[in] w5 - beta_cr
//*    Beta Value for the boundary in V
//*
//* @returns
//*  None
//*
//* @remarks
//*  None
//*
//*******************************************************************************
//*/

    .global ih264_deblk_chroma_vert_bs4_av8

ih264_deblk_chroma_vert_bs4_av8:

    // STMFD sp!,{x4,x5,x12,x14}
    push_v_regs
    stp       x19, x20, [sp, #-16]!
    sxtw      x1, w1

    sub       x0, x0, #4                //point x0 to p1u of row0.
    mov       x12, x0                   //keep a back up of x0 for buffer write

    add       w2, w2, w4, lsl #8        //w2 = (alpha_cr,alpha_cb)
    add       w3, w3, w5, lsl #8        //w3 = (beta_cr,beta_cb)

    ld4       {v0.h, v1.h, v2.h, v3.h}[0], [x0], x1
    ld4       {v0.h, v1.h, v2.h, v3.h}[1], [x0], x1
    ld4       {v0.h, v1.h, v2.h, v3.h}[2], [x0], x1
    ld4       {v0.h, v1.h, v2.h, v3.h}[3], [x0], x1

    ld4       {v4.h, v5.h, v6.h, v7.h}[0], [x0], x1
    ld4       {v4.h, v5.h, v6.h, v7.h}[1], [x0], x1
    ld4       {v4.h, v5.h, v6.h, v7.h}[2], [x0], x1
    ld4       {v4.h, v5.h, v6.h, v7.h}[3], [x0], x1

    mov       v10.16b, v2.16b
    mov       v2.16b, v1.16b
    mov       v1.16b, v4.16b
    mov       v4.16b, v10.16b
    mov       v10.16b, v6.16b
    mov       v6.16b, v3.16b
    mov       v3.16b, v5.16b
    mov       v5.16b, v10.16b

    dup       v22.8h, w2                //Q11 = alpha
    dup       v24.8h, w3                //Q12 = beta
    movi      v31.8b, #2

    mov       v0.d[1], v1.d[0]
    mov       v2.d[1], v3.d[0]
    mov       v4.d[1], v5.d[0]
    mov       v6.d[1], v7.d[0]

    uabd      v8.16b, v2.16b , v4.16b   //|p0-q0|
    uabd      v10.16b, v6.16b , v4.16b  //|q1-q0|
    uabd      v12.16b, v0.16b , v2.16b  //|p1-p0|
    uaddl     v14.8h, v2.8b, v6.8b
    uaddl     v16.8h, v3.8b, v7.8b      //(p0 + q1)
    cmhi      v8.16b, v22.16b , v8.16b  //|p0-q0| < alpha ?
    cmhi      v10.16b, v24.16b , v10.16b //|q1-q0| < beta ?
    cmhi      v12.16b, v24.16b , v12.16b //|p1-p0| < beta ?
    umlal     v14.8h, v0.8b, v31.8b
    umlal     v16.8h, v1.8b, v31.8b     //2*p1 + (p0 + q1)
    uaddl     v18.8h, v0.8b, v4.8b
    uaddl     v20.8h, v1.8b, v5.8b      //(p1 + q0)
    and       v8.16b, v8.16b , v10.16b  //|p0-q0| < alpha && |q1-q0| < beta
    umlal     v18.8h, v6.8b, v31.8b
    umlal     v20.8h, v7.8b, v31.8b     //2*q1 + (p1 + q0)

    rshrn     v14.8b, v14.8h, #2
    rshrn     v15.8b, v16.8h, #2        //(2*p1 + (p0 + q1) + 2) >> 2
    mov       v14.d[1], v15.d[0]
    and       v8.16b, v8.16b , v12.16b  //|p0-q0| < alpha && |q1-q0| < beta && |p1-p0| < beta
    rshrn     v18.8b, v18.8h, #2
    rshrn     v19.8b, v20.8h, #2        //(2*q1 + (p1 + q0) + 2) >> 2
    mov       v18.d[1], v19.d[0]
    bit       v2.16b, v14.16b , v8.16b
    bit       v4.16b, v18.16b , v8.16b

    mov       v1.d[0], v0.d[1]
    mov       v3.d[0], v2.d[1]
    mov       v5.d[0], v4.d[1]
    mov       v7.d[0], v6.d[1]

    mov       v10.16b, v1.16b
    mov       v1.16b, v2.16b
    mov       v2.16b, v4.16b
    mov       v4.16b, v10.16b
    mov       v10.16b, v3.16b
    mov       v3.16b, v6.16b
    mov       v6.16b, v5.16b
    mov       v5.16b, v10.16b

    st4       {v0.h, v1.h, v2.h, v3.h}[0], [x12], x1
    st4       {v0.h, v1.h, v2.h, v3.h}[1], [x12], x1
    st4       {v0.h, v1.h, v2.h, v3.h}[2], [x12], x1
    st4       {v0.h, v1.h, v2.h, v3.h}[3], [x12], x1

    st4       {v4.h, v5.h, v6.h, v7.h}[0], [x12], x1
    st4       {v4.h, v5.h, v6.h, v7.h}[1], [x12], x1
    st4       {v4.h, v5.h, v6.h, v7.h}[2], [x12], x1
    st4       {v4.h, v5.h, v6.h, v7.h}[3], [x12], x1

    // LDMFD sp!,{x4,x5,x12,pc}
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret



///**
//*******************************************************************************
//*
//* @brief
//*     Performs filtering of a chroma block horizontal edge for cases where the
//*     boundary strength is less than 4 in high profile
//*
//* @par Description:
//*       This operation is described in  Sec. 8.7.2.4 under the title
//*       "Filtering process for edges for bS equal to 4" in ITU T Rec H.264.
//*
//* @param[in] x0 - pu1_src
//*  Pointer to the src sample q0
//*
//* @param[in] w1 - src_strd
//*  Source stride
//*
//* @param[in] w2 - alpha_cb
//*  Alpha Value for the boundary in U
//*
//* @param[in] w3 - beta_cb
//*  Beta Value for the boundary in U
//*
//* @param[in] w4 - alpha_cr
//*    Alpha Value for the boundary in V
//*
//* @param[in] w5 - beta_cr
//*    Beta Value for the boundary in V
//*
//* @param[in] w6 - u4_bs
//*    Packed Boundary strength array
//*
//* @param[in] x7 - pu1_cliptab_cb
//*    tc0_table for U
//*
//* @param[in] sp(0) - pu1_cliptab_cr
//*    tc0_table for V
//*
//* @returns
//*  None
//*
//* @remarks
//*  None
//*
//*******************************************************************************
//*/

    .global ih264_deblk_chroma_horz_bslt4_av8

ih264_deblk_chroma_horz_bslt4_av8:

    // STMFD sp!,{x4-x9,x14}        //
    push_v_regs
    stp       x19, x20, [sp, #-16]!
    sxtw      x1, w1
    ldr       x8, [sp, #80]
    sub       x0, x0, x1, lsl #1        //x0 = uc_edgePixelU pointing to p1 of chroma U
    rev       w6, w6                    //
    mov       v12.s[0], w6              //D12[0] = ui_Bs
    ld1       {v16.s}[0], [x7]          //D16[0] contains cliptab_cb
    ld1       {v17.s}[0], [x8]          //D17[0] contains cliptab_cr
    ld2       {v6.8b, v7.8b}, [x0], x1  //Q3=p1
    tbl       v14.8b, {v16.16b}, v12.8b //Retreiving cliptab values for U
    tbl       v28.8b, {v17.16b}, v12.8b //Retrieving cliptab values for V
    uxtl      v12.8h, v12.8b            //Q6 = uc_Bs in each 16 bit scalar
    mov       x6, x0                    //Keeping a backup of the pointer to chroma U P0
    ld2       {v4.8b, v5.8b}, [x0], x1  //Q2=p0
    movi      v30.8b, #1                //
    dup       v20.8b, w2                //D20 contains alpha_cb
    dup       v21.8b, w4                //D21 contains alpha_cr
    mov       v20.d[1], v21.d[0]
    ld2       {v0.8b, v1.8b}, [x0], x1  //Q0=q0
    uxtl      v14.8h, v14.8b            //
    uxtl      v28.8h, v28.8b            //
    mov       v15.d[0], v28.d[0]        //D14 has cliptab values for U, D15 for V
    mov       v14.d[1], v28.d[0]
    ld2       {v2.8b, v3.8b}, [x0]      //Q1=q1
    usubl     v10.8h, v1.8b, v5.8b      //
    usubl     v8.8h, v0.8b, v4.8b       //Q5,Q4 = (q0 - p0)
    mov       v6.d[1], v7.d[0]
    mov       v4.d[1], v5.d[0]
    uabd      v26.16b, v6.16b , v4.16b  //Q13 = ABS(p1 - p0)
    shl       v10.8h, v10.8h, #2        //Q5 = (q0 - p0)<<2
    mov       v0.d[1], v1.d[0]
    uabd      v22.16b, v4.16b , v0.16b  //Q11 = ABS(p0 - q0)
    shl       v8.8h, v8.8h, #2          //Q4 = (q0 - p0)<<2
    mov       v14.d[1], v15.d[0]
    sli       v14.8h, v14.8h, #8
    mov       v15.d[0], v14.d[1]
    mov       v2.d[1], v3.d[0]
    uabd      v24.16b, v2.16b , v0.16b  //Q12 = ABS(q1 - q0)
    cmhs      v18.16b, v22.16b, v20.16b
    usubl     v20.8h, v6.8b, v2.8b      //Q10 = (p1 - q1)L
    usubl     v6.8h, v7.8b, v3.8b       //Q3 = (p1 - q1)H
    dup       v16.8b, w3                //Q8 contains beta_cb
    dup       v17.8b, w5                //Q8 contains beta_cr
    mov       v16.d[1], v17.d[0]
    add       v8.8h, v8.8h , v20.8h     //
    add       v10.8h, v10.8h , v6.8h    //Q5,Q4 = [ (q0 - p0)<<2 ] + (p1 - q1)
    cmhs      v24.16b, v24.16b, v16.16b
    cmgt      v12.4h, v12.4h, #0
    sqrshrn   v8.8b, v8.8h, #3          //
    sqrshrn   v9.8b, v10.8h, #3         //Q4 = i_macro = (((q0 - p0)<<2) + (p1 - q1) + 4)>>3
    mov       v8.d[1], v9.d[0]
    add       v14.8b, v14.8b , v30.8b   //D14 = C = C0+1 for U
    cmhs      v26.16b, v26.16b, v16.16b
    orr       v18.16b, v18.16b , v24.16b //Q9 = ( ABS(p0 - q0) >= Alpha ) | ( ABS(q1 - q0) >= Beta )
    abs       v6.16b, v8.16b            //Q4 = ABS (i_macro)
    add       v15.8b, v15.8b , v30.8b   //D15 = C = C0+1 for V
    mov       v14.d[1], v15.d[0]
    mov       v13.8b, v12.8b
    mov       v12.d[1], v13.d[0]        //
    orr       v18.16b, v18.16b , v26.16b //Q9 = ( ABS(p0 - q0) >= Alpha ) | ( ABS(q1 - q0) >= Beta ) | ( ABS(p1 - p0) >= Beta )
    umin      v14.16b, v6.16b , v14.16b //Q7 = delta = (ABS(i_macro) > C) ? C : ABS(i_macro)
    bic       v12.16b, v12.16b , v18.16b //final condition
    cmge      v8.16b, v8.16b, #0
    and       v14.16b, v14.16b , v12.16b //Making delta zero in places where values shouldn be filterd
    uqadd     v16.16b, v4.16b , v14.16b //Q8 = p0 + delta
    uqsub     v4.16b, v4.16b , v14.16b  //Q2 = p0 - delta
    uqadd     v18.16b, v0.16b , v14.16b //Q9 = q0 + delta
    uqsub     v0.16b, v0.16b , v14.16b  //Q0 = q0 - delta
    bif       v16.16b, v4.16b , v8.16b  //Q8 = (i_macro >= 0 ) ? (p0+delta) : (p0-delta)
    bif       v0.16b, v18.16b , v8.16b  //Q0 = (i_macro >= 0 ) ? (q0-delta) : (q0+delta)
    mov       v17.d[0], v16.d[1]
    mov       v1.d[0], v0.d[1]
    st2       {v16.8b, v17.8b}, [x6], x1 //
    st2       {v0.8b, v1.8b}, [x6]      //

    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret




///**
//*******************************************************************************
//*
//* @brief
//*     Performs filtering of a chroma block vertical edge for cases where the
//*     boundary strength is less than 4 in high profile
//*
//* @par Description:
//*       This operation is described in  Sec. 8.7.2.4 under the title
//*       "Filtering process for edges for bS equal to 4" in ITU T Rec H.264.
//*
//* @param[in] x0 - pu1_src
//*  Pointer to the src sample q0
//*
//* @param[in] w1 - src_strd
//*  Source stride
//*
//* @param[in] w2 - alpha_cb
//*  Alpha Value for the boundary in U
//*
//* @param[in] w3 - beta_cb
//*  Beta Value for the boundary in U
//*
//* @param[in] w4 - alpha_cr
//*    Alpha Value for the boundary in V
//*
//* @param[in] w5 - beta_cr
//*    Beta Value for the boundary in V
//*
//* @param[in] w6 - u4_bs
//*    Packed Boundary strength array
//*
//* @param[in] x7 - pu1_cliptab_cb
//*    tc0_table for U
//*
//* @param[in] sp(0) - pu1_cliptab_cr
//*    tc0_table for V
//*
//* @returns
//*  None
//*
//* @remarks
//*  None
//*
//*******************************************************************************
//*/

    .global ih264_deblk_chroma_vert_bslt4_av8

ih264_deblk_chroma_vert_bslt4_av8:

    // STMFD sp!,{x4-x7,x10-x12,x14}
    push_v_regs
    stp       x19, x20, [sp, #-16]!
    sxtw      x1, w1
    mov       x10, x7
    ldr       x11, [sp, #80]            //x11 = u4_bs
    sub       x0, x0, #4                //point x0 to p1u of row0.
    add       w2, w2, w4, lsl #8
    add       w3, w3, w5, lsl #8
    mov       x12, x0                   //keep a back up of x0 for buffer write
    ld4       {v0.h, v1.h, v2.h, v3.h}[0], [x0], x1
    ld4       {v0.h, v1.h, v2.h, v3.h}[1], [x0], x1
    ld4       {v0.h, v1.h, v2.h, v3.h}[2], [x0], x1
    ld4       {v0.h, v1.h, v2.h, v3.h}[3], [x0], x1

    ld4       {v4.h, v5.h, v6.h, v7.h}[0], [x0], x1
    ld4       {v4.h, v5.h, v6.h, v7.h}[1], [x0], x1
    ld4       {v4.h, v5.h, v6.h, v7.h}[2], [x0], x1
    ld4       {v4.h, v5.h, v6.h, v7.h}[3], [x0], x1

    mov       v10.16b, v2.16b
    mov       v2.16b, v1.16b
    mov       v1.16b, v4.16b
    mov       v4.16b, v10.16b
    mov       v10.16b, v6.16b
    mov       v6.16b, v3.16b
    mov       v3.16b, v5.16b
    mov       v5.16b, v10.16b
    dup       v22.8h, w2                //Q11 = alpha
    mov       v2.d[1], v3.d[0]
    mov       v4.d[1], v5.d[0]
    uabd      v8.16b, v2.16b , v4.16b   //|p0-q0|
    dup       v24.8h, w3                //Q12 = beta
    mov       v25.d[0], v24.d[1]
    mov       v6.d[1], v7.d[0]
    mov       v0.d[1], v1.d[0]
    uabd      v10.16b, v6.16b , v4.16b  //|q1-q0|
    uabd      v12.16b, v0.16b , v2.16b  //|p1-p0|
    cmhi      v8.16b, v22.16b , v8.16b  //|p0-q0| < alpha ?
    usubl     v14.8h, v0.8b, v6.8b
    cmhi      v10.16b, v24.16b , v10.16b //|q1-q0| < beta ?
    usubl     v16.8h, v1.8b, v7.8b      //(p1 - q1)
    cmhi      v12.16b, v24.16b , v12.16b //|p1-p0| < beta ?
    usubl     v18.8h, v4.8b, v2.8b
    and       v8.16b, v8.16b , v10.16b  //|p0-q0| < alpha && |q1-q0| < beta
    usubl     v20.8h, v5.8b, v3.8b      //(q0 - p0)
    movi      v28.8h, #4
    ld1       {v24.s}[0], [x10]         //Load ClipTable for U
    ld1       {v25.s}[0], [x11]         //Load ClipTable for V
    rev       w6, w6                    //Blocking strengths
    and       v8.16b, v8.16b , v12.16b  //|p0-q0| < alpha && |q1-q0| < beta && |p1-p0| < beta
    mov       v10.s[0], w6
    mla       v14.8h, v18.8h , v28.8h
    mla       v16.8h, v20.8h , v28.8h   //4*(q0 - p0) + (p1 - q1)
    uxtl      v10.8h, v10.8b
    sli       v10.4h, v10.4h, #8
    tbl       v12.8b, {v24.16b}, v10.8b //tC0 for U
    tbl       v13.8b, {v25.16b}, v10.8b //tC0 for V
    zip1      v31.8b, v12.8b, v13.8b
    zip2      v13.8b, v12.8b, v13.8b
    mov       v12.8b, v31.8b
    mov       v12.d[1], v13.d[0]
    uxtl      v10.4s, v10.4h
    sli       v10.4s, v10.4s, #16
    movi      v24.16b, #1
    add       v12.16b, v12.16b , v24.16b //tC0 + 1
    cmhs      v10.16b, v10.16b , v24.16b
    and       v8.16b, v8.16b , v10.16b  //|p0-q0| < alpha && |q1-q0| < beta && |p1-p0| < beta && u4_bs != 0
    // Q0 - Q3(inputs),
    // Q4 (|p0-q0| < alpha && |q1-q0| < beta && |p1-p0| < beta && u4_bs != 0),
    // Q6 (tC)
    srshr     v14.8h, v14.8h, #3
    srshr     v16.8h, v16.8h, #3        //(((q0 - p0) << 2) + (p1 - q1) + 4) >> 3)
    cmgt      v18.8h, v14.8h , #0
    cmgt      v20.8h, v16.8h , #0
    xtn       v18.8b, v18.8h
    xtn       v19.8b, v20.8h            //Q9 = sign(delta)
    mov       v18.d[1], v19.d[0]
    abs       v14.8h, v14.8h
    abs       v16.8h, v16.8h
    xtn       v14.8b, v14.8h
    xtn       v15.8b, v16.8h
    mov       v14.d[1], v15.d[0]
    umin      v14.16b, v14.16b , v12.16b //Q7 = |delta|
    uqadd     v20.16b, v2.16b , v14.16b //p0+|delta|
    uqadd     v22.16b, v4.16b , v14.16b //q0+|delta|
    uqsub     v24.16b, v2.16b , v14.16b //p0-|delta|
    uqsub     v26.16b, v4.16b , v14.16b //q0-|delta|
    bit       v24.16b, v20.16b , v18.16b //p0 + delta
    bit       v22.16b, v26.16b , v18.16b //q0 - delta
    bit       v2.16b, v24.16b , v8.16b
    bit       v4.16b, v22.16b , v8.16b
    mov       v1.d[0], v0.d[1]
    mov       v3.d[0], v2.d[1]
    mov       v5.d[0], v4.d[1]
    mov       v7.d[0], v6.d[1]
    mov       v10.16b, v1.16b
    mov       v1.16b, v2.16b
    mov       v2.16b, v4.16b
    mov       v4.16b, v10.16b
    mov       v10.16b, v3.16b
    mov       v3.16b, v6.16b
    mov       v6.16b, v5.16b
    mov       v5.16b, v10.16b
    st4       {v0.h, v1.h, v2.h, v3.h}[0], [x12], x1
    st4       {v0.h, v1.h, v2.h, v3.h}[1], [x12], x1
    st4       {v0.h, v1.h, v2.h, v3.h}[2], [x12], x1
    st4       {v0.h, v1.h, v2.h, v3.h}[3], [x12], x1

    st4       {v4.h, v5.h, v6.h, v7.h}[0], [x12], x1
    st4       {v4.h, v5.h, v6.h, v7.h}[1], [x12], x1
    st4       {v4.h, v5.h, v6.h, v7.h}[2], [x12], x1
    st4       {v4.h, v5.h, v6.h, v7.h}[3], [x12], x1

    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret


