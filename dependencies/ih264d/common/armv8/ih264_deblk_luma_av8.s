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
///*  File Name         : ih264_deblk_luma_av8.s                               */
///*                                                                           */
///*  Description       : Contains function definitions for deblocking luma    */
///*                      edge. Functions are coded in NEON assembly and can   */
///*                      be compiled using ARM RVDS.                          */
///*                                                                           */
///*  List of Functions : ih264_deblk_luma_vert_bs4_av8()                      */
///*                      ih264_deblk_luma_vert_bslt4_av8()                    */
///*                      ih264_deblk_luma_horz_bs4_av8()                      */
///*                      ih264_deblk_luma_horz_bslt4_av8()                    */
///*                                                                           */
///*  Issues / Problems : None                                                 */
///*                                                                           */
///*  Revision History  :                                                      */
///*                                                                           */
///*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
///*         28 11 2013   Ittiam          Draft                                */
///*                                                                           */
///*****************************************************************************/


.text
.p2align 2
.include "ih264_neon_macros.s"



///**
//*******************************************************************************
//*
//* @brief
//*     Performs filtering of a luma block horizontal edge for cases where the
//*     boundary strength is less than 4
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
//* @param[in] w2 - alpha
//*  Alpha Value for the boundary
//*
//* @param[in] w3 - beta
//*  Beta Value for the boundary
//*
//* @param[in] w4 - u4_bs
//*    Packed Boundary strength array
//*
//* @param[in] x5 - pu1_cliptab
//*    tc0_table
//*
//* @returns
//*  None
//*
//* @remarks
//*  None
//*
//*******************************************************************************
//*/

    .global ih264_deblk_luma_horz_bslt4_av8

ih264_deblk_luma_horz_bslt4_av8:

    // STMFD sp!,{x4-x7,x14}
    push_v_regs
    sxtw      x1, w1
    stp       x19, x20, [sp, #-16]!

    //LDRD            x4,x5,[SP,#0x14]        //x4 = ui_Bs , x5 = *puc_ClpTab
    sub       x0, x0, x1, lsl #1        //x1 = uc_Horizonpad
    sub       x0, x0, x1                //x0 pointer to p2
    rev       w4, w4                    //
    ld1       {v10.8b, v11.8b}, [x0], x1 //p2 values are loaded into q5
    mov       v12.s[0], w4              //d12[0] = ui_Bs
    mov       x6, x0                    //keeping backup of pointer to p1
    ld1       {v8.8b, v9.8b}, [x0], x1  //p1 values are loaded into q4
    mov       x7, x0                    //keeping backup of pointer to p0
    ld1       {v6.8b, v7.8b}, [x0], x1  //p0 values are loaded into q3
    uxtl      v12.8h, v12.8b            //q6 = uc_Bs in each 16 bt scalar
    ld1       {v0.8b, v1.8b}, [x0], x1  //q0 values are loaded into q0
    mov       v10.d[1], v11.d[0]
    mov       v8.d[1], v9.d[0]
    mov       v6.d[1], v7.d[0]
    uabd      v26.16b, v8.16b, v6.16b
    ld1       {v2.8b, v3.8b}, [x0], x1  //q1 values are loaded into q1
    mov       v0.d[1], v1.d[0]
    mov       v2.d[1], v3.d[0]
    uabd      v22.16b, v6.16b, v0.16b
    ld1       {v16.s}[0], [x5]          //D16[0] contains cliptab
    uabd      v24.16b, v2.16b, v0.16b
    ld1       {v4.8b, v5.8b}, [x0], x1  //q2 values are loaded into q2
    tbl       v14.8b, {v16.16b}, v12.8b //
    mov       v4.d[1], v5.d[0]
    dup       v20.16b, w2               //Q10 contains alpha
    dup       v16.16b, w3               //Q8 contains beta
    uxtl      v12.4s, v12.4h            //
    uxtl      v14.4s, v14.4h            //
    uabd      v28.16b, v10.16b, v6.16b
    uabd      v30.16b, v4.16b, v0.16b
    cmgt      v12.4s, v12.4s, #0
    sli       v14.4s, v14.4s, #8
    cmhs      v18.16b, v22.16b, v20.16b
    cmhs      v24.16b, v24.16b, v16.16b
    cmhs      v26.16b, v26.16b, v16.16b
    cmhi      v20.16b, v16.16b , v28.16b //Q10=(Ap<Beta)
    cmhi      v22.16b, v16.16b , v30.16b //Q11=(Aq<Beta)
    sli       v14.4s, v14.4s, #16
    orr       v18.16b, v18.16b , v24.16b //Q9 = ( ABS(p0 - q0) >= Alpha ) | ( ABS(q1 - q0) >= Beta )
    usubl     v30.8h, v1.8b, v7.8b      //
    usubl     v24.8h, v0.8b, v6.8b      //Q15,Q12 = (q0 - p0)
    orr       v18.16b, v18.16b , v26.16b //Q9 = ( ABS(p0 - q0) >= Alpha ) | ( ABS(q1 - q0) >= Beta ) | ( ABS(p1 - p0) >= Beta )
    usubl     v28.8h, v8.8b, v2.8b      //Q14 = (p1 - q1)L
    shl       v26.8h, v30.8h, #2        //Q13 = (q0 - p0)<<2
    shl       v24.8h, v24.8h, #2        //Q12 = (q0 - p0)<<2
    usubl     v30.8h, v9.8b, v3.8b      //Q15 = (p1 - q1)H
    bic       v12.16b, v12.16b , v18.16b //final condition
    add       v24.8h, v24.8h , v28.8h   //
    add       v26.8h, v26.8h , v30.8h   //Q13,Q12 = [ (q0 - p0)<<2 ] + (p1 - q1)
    sub       v18.16b, v14.16b , v20.16b //Q9 = C0 + (Ap < Beta)
    urhadd    v16.16b, v6.16b , v0.16b  //Q8 = ((p0+q0+1) >> 1)
    mov       v17.d[0], v16.d[1]
    sqrshrn   v24.8b, v24.8h, #3        //
    sqrshrn   v25.8b, v26.8h, #3        //Q12 = i_macro = (((q0 - p0)<<2) + (p1 - q1) + 4)>>3
    mov       v24.d[1], v25.d[0]
    sub       v18.16b, v18.16b , v22.16b //Q9 = C0 + (Ap < Beta) + (Aq < Beta)
    and       v20.16b, v20.16b , v12.16b //
    and       v22.16b, v22.16b , v12.16b //
    abs       v26.16b, v24.16b          //Q13 = ABS (i_macro)
    uaddl     v28.8h, v17.8b, v11.8b    //
    uaddl     v10.8h, v16.8b, v10.8b    //Q14,Q5 = p2 + (p0+q0+1)>>1
    uaddl     v30.8h, v17.8b, v5.8b     //
    umin      v18.16b, v26.16b , v18.16b //Q9 = delta = (ABS(i_macro) > C) ? C : ABS(i_macro)
    ushll     v26.8h, v9.8b, #1         //
    uaddl     v4.8h, v16.8b, v4.8b      //Q15,Q2 = q2 + (p0+q0+1)>>1
    ushll     v16.8h, v8.8b, #1         //Q13,Q8 = (p1<<1)
    and       v18.16b, v18.16b , v12.16b //Making delta zero in places where values shouldn be filterd
    sub       v28.8h, v28.8h , v26.8h   //Q14,Q5 = [p2 + (p0+q0+1)>>1] - (p1<<1)
    sub       v10.8h, v10.8h , v16.8h   //
    ushll     v16.8h, v2.8b, #1         //
    ushll     v26.8h, v3.8b, #1         //Q13,Q8 = (q1<<1)
    sqshrn    v29.8b, v28.8h, #1        //
    sqshrn    v28.8b, v10.8h, #1        //Q14 = i_macro_p1
    mov       v28.d[1], v29.d[0]
    sub       v4.8h, v4.8h , v16.8h     //
    sub       v30.8h, v30.8h , v26.8h   //Q15,Q2  = [q2 + (p0+q0+1)>>1] - (q1<<1)
    neg       v26.16b, v14.16b          //Q13 = -C0
    smin      v28.16b, v28.16b , v14.16b //Q14 = min(C0,i_macro_p1)
    cmge      v24.16b, v24.16b, #0
    sqshrn    v31.8b, v30.8h, #1        //
    sqshrn    v30.8b, v4.8h, #1         //Q15 = i_macro_q1
    mov       v30.d[1], v31.d[0]
    smax      v28.16b, v28.16b , v26.16b //Q14 = max( - C0 , min(C0, i_macro_p1) )
    uqadd     v16.16b, v6.16b , v18.16b //Q8  = p0 + delta
    uqsub     v6.16b, v6.16b , v18.16b  //Q3 = p0 - delta
    smin      v30.16b, v30.16b , v14.16b //Q15 = min(C0,i_macro_q1)
    and       v28.16b, v20.16b , v28.16b //condition check Ap<beta
    uqadd     v14.16b, v0.16b , v18.16b //Q7 = q0 + delta
    uqsub     v0.16b, v0.16b , v18.16b  //Q0   = q0 - delta
    smax      v30.16b, v30.16b , v26.16b //Q15 = max( - C0 , min(C0, i_macro_q1) )
    bif       v16.16b, v6.16b , v24.16b //Q8  = (i_macro >= 0 ) ? (p0+delta) : (p0-delta)
    bif       v0.16b, v14.16b , v24.16b //Q0  = (i_macro >= 0 ) ? (q0-delta) : (q0+delta)
    add       v28.16b, v28.16b , v8.16b //
    and       v30.16b, v22.16b , v30.16b //condition check Aq<beta
    st1       {v16.16b}, [x7], x1       //writting back filtered value of p0
    add       v30.16b, v30.16b , v2.16b //
    st1       {v0.16b}, [x7], x1        //writting back filtered value of q0
    st1       {v28.16b}, [x6]           //writting back filtered value of p1
    st1       {v30.16b}, [x7], x1       //writting back filtered value of q1

    // LDMFD sp!,{x4-x7,pc}
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret



///**
//*******************************************************************************
//*
//* @brief
//*     Performs filtering of a luma block horizontal edge when the
//*     boundary strength is set to 4
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
//* @param[in] w2 - alpha
//*  Alpha Value for the boundary
//*
//* @param[in] w3 - beta
//*  Beta Value for the boundary
//*
//* @returns
//*  None
//*
//* @remarks
//*  None
//*
//*******************************************************************************
//*/

    .global ih264_deblk_luma_horz_bs4_av8

ih264_deblk_luma_horz_bs4_av8:

    // Back up necessary registers on stack
    // STMFD sp!,{x12,x14}
    push_v_regs
    stp       x19, x20, [sp, #-16]!
    sxtw      x1, w1

    // Init
    dup       v0.16b, w2                //duplicate alpha
    sub       x12, x0, x1               //pointer to p0 = q0 - src_strd
    dup       v2.16b, w3                //duplicate beta
    sub       x14, x0, x1, lsl#1        //pointer to p1 = q0 - src_strd*2
    sub       x2, x0, x1, lsl#2         //pointer to p3 = q0 - src_strd*4
    sub       x3, x14, x1               //pointer to p2 = p1 - src_strd

    // Load Data
    ld1       {v4.8b, v5.8b}, [x0], x1  //load q0 to Q2, q0 = q0 + src_strd
    ld1       {v6.8b, v7.8b}, [x12]     //load p0 to Q3
    ld1       {v8.8b, v9.8b}, [x0], x1  //load q1 to Q4, q0 = q0 + src_strd
    ld1       {v10.8b, v11.8b}, [x14]   //load p1 to Q5
    mov       v4.d[1] , v5.d[0]
    mov       v6.d[1] , v7.d[0]
    mov       v8.d[1] , v9.d[0]
    mov       v10.d[1] , v11.d[0]

    // Filter Decision
    uabd      v12.16b  , v4.16b, v6.16b
    uabd      v14.16b  , v8.16b, v4.16b
    uabd      v16.16b  , v10.16b, v6.16b
    cmhs      v18.16b, v12.16b , v0.16b //ABS(p0 - q0) >= Alpha
    cmhs      v14.16b, v14.16b , v2.16b //ABS(q1 - q0) >= Beta
    cmhs      v16.16b, v16.16b , v2.16b //ABS(q1 - q0) >= Beta
    movi      v20.16b, #2
    orr       v18.16b, v18.16b , v14.16b //ABS(p0 - q0) >= Alpha || ABS(q1 - q0) >= Beta
    ld1       {v14.8b, v15.8b}, [x0], x1 //load q2 to Q7, q0 = q0 + src_strd
    mov       v14.d[1] , v15.d[0]
    orr       v18.16b, v18.16b , v16.16b //ABS(p0 - q0) >= Alpha || ABS(q1 - q0) >= Beta || ABS(p1 - p0) >= Beta
    usra      v20.16b, v0.16b, #2       //alpha >>2 +2
    uabd      v22.16b  , v14.16b, v4.16b
    uaddl     v24.8h, v4.8b, v6.8b      //p0+q0 L
    uaddl     v26.8h, v5.8b, v7.8b      //p0+q0 H
    cmhi      v22.16b, v2.16b , v22.16b //Aq < Beta
    cmhi      v20.16b, v20.16b , v12.16b //(ABS(p0 - q0) <((Alpha >>2) + 2))
    // Deblock Filtering q0', q1', q2'
    uaddw     v28.8h, v24.8h , v8.8b    //p0+q0+q1 L
    uaddw     v30.8h, v26.8h , v9.8b    //p0+q0+q1 H
    and       v22.16b, v22.16b , v20.16b //(Aq < Beta && ABS(p0 - q0) <((Alpha >>2) + 2))
    // q0' if (Aq < Beta && ABS(p0 - q0) <((Alpha >>2) + 2)) TRUE
    add       v16.8h, v28.8h , v28.8h   //2*(p0+q0+q1)L
    add       v0.8h, v30.8h , v30.8h    //2*(p0+q0+q1)H
    uaddw     v16.8h, v16.8h , v14.8b   //2*(p0+q0+q1)+q2 L
    uaddw     v0.8h, v0.8h , v15.8b     //2*(p0+q0+q1)+q2 H
    uaddw     v16.8h, v16.8h , v10.8b   //2*(p0+q0+q1)+q2 +p1 L
    uaddw     v0.8h, v0.8h , v11.8b     //2*(p0+q0+q1)+q2 +p1 H
    rshrn     v12.8b, v16.8h, #3        //(2*(p0+q0+q1)+q2 +p1 +4)>> 3 L [q0']
    rshrn     v13.8b, v0.8h, #3         //(2*(p0+q0+q1)+q2 +p1 +4)>> 3 H [q0']
    mov       v12.d[1] , v13.d[0]
    // q0" if (Aq < Beta && ABS(p0 - q0) <((Alpha >>2) + 2)) FALSE
    uaddl     v16.8h, v8.8b, v8.8b      //2*q1 L
    uaddl     v0.8h, v9.8b, v9.8b       //2*q1 H
    uaddw     v16.8h, v16.8h , v4.8b    //2*q1+q0 L
    uaddw     v0.8h, v0.8h , v5.8b      //2*q1+q0 H
    uaddw     v16.8h, v16.8h , v10.8b   //2*q1+q0+p1  L
    uaddw     v0.8h, v0.8h , v11.8b     //2*q1+q0+p1 H
    rshrn     v16.8b, v16.8h, #2        //(2*q1+q0+p1+2)>>2 L [q0"]
    rshrn     v17.8b, v0.8h, #2         //(2*q1+q0+p1+2)>>2 H [q0"]
    mov       v16.d[1] , v17.d[0]
    uaddw     v28.8h, v28.8h , v14.8b   //p0+q0+q1+q2 L
    uaddw     v30.8h, v30.8h , v15.8b   //p0+q0+q1+q2 H
    ld1       {v0.8b, v1.8b}, [x0], x1  //load q3 to Q0, q0 = q0 + src_strd
    mov       v0.d[1] , v1.d[0]
    bit       v16.16b, v12.16b , v22.16b //choosing between q0' and q0" depending on condn
    sub       x0, x0, x1, lsl #2        //pointer to q0
    bic       v22.16b, v22.16b , v18.16b //((ABS(p0 - q0) >= Alpha  || ABS(q1 - q0) >= Beta || ABS(p1 - p0) >= Beta))
                                        // && (Aq < Beta && ABS(p0 - q0) <((Alpha >>2) + 2))
    rshrn     v12.8b, v28.8h, #2        //(p0+q0+q1+q2+2)>>2 L [q1']
    rshrn     v13.8b, v30.8h, #2        //(p0+q0+q1+q2+2)>>2 H [q1']
    mov       v12.d[1] , v13.d[0]
    bif       v4.16b, v16.16b , v18.16b //choose q0 or filtered q0
    mov       v5.d[0] , v4.d[1]
    uaddl     v16.8h, v14.8b, v0.8b     //q2+q3,L
    uaddl     v0.8h, v15.8b, v1.8b      //q2+q3,H
    add       v28.8h, v28.8h , v16.8h   //p0+q0+q1+2*q2+q3 L
    st1       {v4.8b, v5.8b}, [x0], x1  //store q0
    add       v30.8h, v30.8h , v0.8h    //p0+q0+q1+2*q2+q3 H
    add       v28.8h, v28.8h , v16.8h   //p0+q0+q1+3*q2+2*q3 L
    add       v30.8h, v30.8h , v0.8h    //p0+q0+q1+3*q2+2*q3 H
    rshrn     v0.8b, v28.8h, #3         //(p0+q0+q1+3*q2+2*q3+4)>>3 L [q2']
    rshrn     v1.8b, v30.8h, #3         //(p0+q0+q1+3*q2+2*q3+4)>>3 H [q2']
    mov       v0.d[1] , v1.d[0]
    ld1       {v30.8b, v31.8b}, [x3]    //load p2 to Q15
    mov       v30.d[1] , v31.d[0]
    bif       v12.16b, v8.16b , v22.16b //choose q1 or filtered value of q1
    mov       v13.d[0] , v12.d[1]
    uabd      v16.16b  , v30.16b, v6.16b
    uaddw     v24.8h, v24.8h , v10.8b   //p0+q0+p1 L
    bif       v0.16b, v14.16b , v22.16b //choose q2 or filtered q2
    mov       v1.d[0] , v0.d[1]
    uaddw     v26.8h, v26.8h , v11.8b   //p0+q0+p1 H
    st1       {v12.8b, v13.8b}, [x0], x1 //store q1
    cmhi      v16.16b, v2.16b , v16.16b //Ap < Beta
    add       v28.8h, v24.8h , v24.8h   //2*(p0+q0+p1) L
    add       v4.8h, v26.8h , v26.8h    //2*(p0+q0+p1) H
    st1       {v0.8b, v1.8b}, [x0], x1  //store q2
    and       v20.16b, v20.16b , v16.16b //((Ap < Beta) && (ABS(p0 - q0) <((Alpha >>2) + 2)))
    uaddw     v28.8h, v28.8h , v30.8b   //2*(p0+q0+p1)+p2 l
    uaddw     v4.8h, v4.8h , v31.8b     //2*(p0+q0+p1)+p2 H
    uaddw     v28.8h, v28.8h , v8.8b    //2*(p0+q0+p1)+p2+q1 L
    uaddw     v4.8h, v4.8h , v9.8b      //2*(p0+q0+p1)+p2+q1 H
    rshrn     v28.8b, v28.8h, #3        //(2*(p0+q0+p1)+p2+q1+4)>>3  L,p0'
    rshrn     v29.8b, v4.8h, #3         //(2*(p0+q0+p1)+p2+q1+4)>>3  H,p0'
    mov       v28.d[1] , v29.d[0]
    movi      v0.8b, #2
    movi      v1.4h, #2
    uaddl     v2.8h, v6.8b, v8.8b       //p0+q1      L
    umlal     v2.8h, v10.8b, v0.8b      //2*p1+p0+q1 L
    uaddl     v16.8h, v7.8b, v9.8b      //p0+q1  H
    umlal     v16.8h, v11.8b, v0.8b     //2*p1+p0+q1 H
    uaddw     v12.8h, v24.8h , v30.8b   //(p0+q0+p1) +p2 L
    ld1       {v24.8b, v25.8b}, [x2]    //load p3,Q12
    mov       v24.d[1] , v25.d[0]
    uaddw     v4.8h, v26.8h , v31.8b    //(p0+q0+p1) +p2 H
    uaddl     v8.8h, v30.8b, v24.8b     //p2+p3 L
    rshrn     v26.8b, v12.8h, #2        //((p0+q0+p1)+p2 +2)>>2,p1' L
    rshrn     v2.8b, v2.8h, #2          //(2*p1+p0+q1+2)>>2,p0"L
    rshrn     v27.8b, v4.8h, #2         //((p0+q0+p1)+p2 +2)>>2,p1' H
    rshrn     v3.8b, v16.8h, #2         //(2*p1+p0+q1+2)>>2,p0" H
    mov       v26.d[1] , v27.d[0]
    mov       v2.d[1] , v3.d[0]
    uaddl     v16.8h, v31.8b, v25.8b    //p2+p3 H
    mla       v12.8h, v8.8h , v1.h[0]   //(p0+q0+p1)+3*p2+2*p3 L
    mla       v4.8h, v16.8h , v1.h[0]   //(p0+q0+p1)+3*p2+2*p3 H
    bic       v16.16b, v20.16b , v18.16b //((ABS(p0 - q0) >= Alpha  || ABS(q1 - q0) >= Beta || ABS(p1 - p0) >= Beta))
    mov       v17.d[0] , v16.d[1]       //&& (Ap < Beta && ABS(p0 - q0) <((Alpha >>2) + 2))
    bit       v2.16b, v28.16b , v20.16b //choosing between po' and p0"
    mov       v3.d[0] , v2.d[1]
    rshrn     v12.8b, v12.8h, #3        //((p0+q0+p1)+3*p2+2*p3+4)>>3 L p2'
    rshrn     v13.8b, v4.8h, #3         //((p0+q0+p1)+3*p2+2*p3+4)>>3 H p2'
    mov       v12.d[1] , v13.d[0]
    bif       v6.16b, v2.16b , v18.16b  //choosing between p0 and filtered value of p0
    bit       v10.16b, v26.16b , v16.16b //choosing between p1 and p1'
    bit       v30.16b, v12.16b , v16.16b //choosing between p2 and p2'
    st1       {v6.16b}, [x12]           //store p0
    st1       {v10.16b}, [x14]          //store p1
    st1       {v30.16b}, [x3]           //store p2

    // LDMFD sp!,{x12,pc}
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret



///**
//*******************************************************************************
//*
//* @brief
//*     Performs filtering of a luma block vertical edge for cases where the
//*     boundary strength is less than 4
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
//* @param[in] w2 - alpha
//*  Alpha Value for the boundary
//*
//* @param[in] w3 - beta
//*  Beta Value for the boundary
//*
//* @param[in] w4 - u4_bs
//*    Packed Boundary strength array
//*
//* @param[in] x5 - pu1_cliptab
//*    tc0_table
//*
//* @returns
//*  None
//*
//* @remarks
//*  None
//*
//*******************************************************************************
//*/

    .global ih264_deblk_luma_vert_bslt4_av8

ih264_deblk_luma_vert_bslt4_av8:

    // STMFD sp!,{x12,x14}
    push_v_regs
    stp       x19, x20, [sp, #-16]!
    sxtw      x1, w1

    sub       x0, x0, #4                //pointer uc_edgePixel-4
    mov       x12, x4
    mov       x14, x5
    mov       x17, x0
    //loading p3:p2:p1:p0:q0:q1:q2:q3 for every row
    ld1       {v0.8b}, [x0], x1         //row1
    ld1       {v2.8b}, [x0], x1         //row2
    ld1       {v4.8b}, [x0], x1         //row3
    rev       w12, w12                  //reversing ui_bs
    ld1       {v6.8b}, [x0], x1         //row4
    mov       v18.s[0], w12             //d12[0] = ui_Bs
    ld1       {v16.s}[0], [x14]         //D16[0] contains cliptab
    ld1       {v8.8b}, [x0], x1         //row5
    uxtl      v18.8h, v18.8b            //q6 = uc_Bs in each 16 bt scalar
    ld1       {v10.8b}, [x0], x1        //row6
    ld1       {v12.8b}, [x0], x1        //row7
    tbl       v16.8b, {v16.16b}, v18.8b //puc_ClipTab[uc_Bs]
    ld1       {v14.8b}, [x0], x1        //row8
    ld1       {v1.8b}, [x0], x1         //row9
    uxtl      v16.4s, v16.4h            //
    ld1       {v3.8b}, [x0], x1         //row10
    ld1       {v5.8b}, [x0], x1         //row11
    ld1       {v7.8b}, [x0], x1         //row12
    sli       v16.4s, v16.4s, #8        //
    ld1       {v9.8b}, [x0], x1         //row13
    ld1       {v11.8b}, [x0], x1        //row14
    ld1       {v13.8b}, [x0], x1        //row15
    sli       v16.4s, v16.4s, #16
    ld1       {v15.8b}, [x0], x1        //row16


    //taking two 8x8 transposes
    //2X2 transposes
    trn1      v21.8b, v0.8b, v2.8b
    trn2      v2.8b, v0.8b, v2.8b       //row1 &2
    mov       v0.8b, v21.8b
    trn1      v21.8b, v4.8b, v6.8b
    trn2      v6.8b, v4.8b, v6.8b       //row3&row4
    mov       v4.8b, v21.8b
    trn1      v21.8b, v8.8b, v10.8b
    trn2      v10.8b, v8.8b, v10.8b     //row5&6
    mov       v8.8b, v21.8b
    trn1      v21.8b, v12.8b, v14.8b
    trn2      v14.8b, v12.8b, v14.8b    //row7 & 8
    mov       v12.8b, v21.8b
    trn1      v21.8b, v1.8b, v3.8b
    trn2      v3.8b, v1.8b, v3.8b       //row9 &10
    mov       v1.8b, v21.8b
    trn1      v21.8b, v5.8b, v7.8b
    trn2      v7.8b, v5.8b, v7.8b       //row11 & 12
    mov       v5.8b, v21.8b
    trn1      v21.8b, v9.8b, v11.8b
    trn2      v11.8b, v9.8b, v11.8b     //row13 &14
    mov       v9.8b, v21.8b
    trn1      v21.8b, v13.8b, v15.8b
    trn2      v15.8b, v13.8b, v15.8b    //row15 & 16
    mov       v13.8b, v21.8b
    //4x4 transposes
    trn1      v21.4h, v2.4h, v6.4h
    trn2      v6.4h, v2.4h, v6.4h       //row2 & row4
    mov       v2.8b, v21.8b
    trn1      v21.4h, v10.4h, v14.4h
    trn2      v14.4h, v10.4h, v14.4h    //row6 & row8
    mov       v10.8b, v21.8b
    trn1      v21.4h, v3.4h, v7.4h
    trn2      v7.4h, v3.4h, v7.4h       //row10 & 12
    mov       v3.8b, v21.8b
    trn1      v21.4h, v11.4h, v15.4h
    trn2      v15.4h, v11.4h, v15.4h    //row14 & row16
    mov       v11.8b, v21.8b
    trn1      v21.2s, v6.2s, v14.2s
    trn2      v14.2s, v6.2s, v14.2s     //row4 & 8
    mov       v6.8b, v21.8b
    trn1      v21.2s, v7.2s, v15.2s
    trn2      v15.2s, v7.2s, v15.2s     //row 12 & 16
    mov       v7.8b, v21.8b
    //now Q3 ->p0 and Q7->q3
    trn1      v21.4h, v0.4h, v4.4h
    trn2      v4.4h, v0.4h, v4.4h       //row1 & 3
    mov       v0.8b, v21.8b
    trn1      v21.4h, v8.4h, v12.4h
    trn2      v12.4h, v8.4h, v12.4h     //row 5 & 7
    mov       v8.8b, v21.8b
    trn1      v21.4h, v1.4h, v5.4h
    trn2      v5.4h, v1.4h, v5.4h       //row9 & row11
    mov       v1.8b, v21.8b
    trn1      v21.4h, v9.4h, v13.4h
    trn2      v13.4h, v9.4h, v13.4h     //row13 & row15
    mov       v9.8b, v21.8b
    trn1      v21.2s, v0.2s, v8.2s
    trn2      v8.2s, v0.2s, v8.2s       //row1 & row5
    mov       v0.8b, v21.8b
    trn1      v21.2s, v1.2s, v9.2s
    trn2      v9.2s, v1.2s, v9.2s       //row9 & 13
    mov       v1.8b, v21.8b
    //now Q0->p3 & Q4->q0
    //starting processing as p0 and q0 are now ready
    trn1      v21.2s, v2.2s, v10.2s
    trn2      v10.2s, v2.2s, v10.2s     //row2 &6
    mov       v2.8b, v21.8b
    mov       v6.d[1] , v7.d[0]
    mov       v8.d[1] , v9.d[0]
    urhadd    v20.16b, v6.16b , v8.16b  //((p0 + q0 + 1) >> 1)
    mov       v21.d[0], v20.d[1]
    trn1      v31.2s, v3.2s, v11.2s
    trn2      v11.2s, v3.2s, v11.2s     //row10&row14
    mov       v3.8b, v31.8b
    movi      v19.8b, #2
    mov       v18.d[1], v19.d[0]
    //now Q1->p2     & Q5->q1
    trn1      v31.2s, v4.2s, v12.2s
    trn2      v12.2s, v4.2s, v12.2s     //row3 & 7
    mov       v4.8b, v31.8b
    uabd      v22.16b  , v6.16b, v8.16b //ABS(q1 - q0)
    trn1      v31.2s, v5.2s, v13.2s
    trn2      v13.2s, v5.2s, v13.2s     //row11 & row15
    mov       v5.8b, v31.8b
    mov       v0.d[1] , v1.d[0]
    mov       v2.d[1] , v3.d[0]
    mov       v4.d[1] , v5.d[0]
    mov       v10.d[1] , v11.d[0]
    mov       v12.d[1] , v13.d[0]
    mov       v14.d[1] , v15.d[0]
    uaddl     v24.8h, v20.8b, v2.8b     //(p2 + ((p0 + q0 + 1) >> 1) L
    //now            Q2->p1,Q6->q2
    uaddl     v26.8h, v21.8b, v3.8b     //(p2 + ((p0 + q0 + 1) >> 1) H
    umlsl     v24.8h, v4.8b, v19.8b     //(p2 + ((p0 + q0 + 1) >> 1) - (p1 << 1)) L
    umlsl     v26.8h, v5.8b, v19.8b     //(p2 + ((p0 + q0 + 1) >> 1) - (p1 << 1)) H
    dup       v28.16b, w2               //alpha
    cmhs      v22.16b, v22.16b , v28.16b //ABS(p0 - q0) >= Alpha(Alpha <=ABS(p0 - q0))
    dup       v28.16b, w3               //beta
    uabd      v30.16b  , v10.16b, v8.16b //ABS(q1 - q0)
    sqshrn    v24.8b, v24.8h, #1        //((p2 + ((p0 + q0 + 1) >> 1) - (p1 << 1)) >> 1) L
    sqshrn    v25.8b, v26.8h, #1        //((p2 + ((p0 + q0 + 1) >> 1) - (p1 << 1)) >> 1) H
    mov       v24.d[1], v25.d[0]
    cmhs      v30.16b, v30.16b , v28.16b //ABS(p0 - q0) >= Alpha(Alpha <=ABS(p0 - q0))
    uabd      v26.16b  , v4.16b, v6.16b //ABS(q1 - q0)

    smin      v24.16b, v24.16b , v16.16b //min(deltap1 ,C0)
    orr       v22.16b, v22.16b , v30.16b //ABS(q1 - q0) >= Beta ||ABS(p0 - q0) >= Alpha
    neg       v30.16b, v16.16b          //-C0
    cmhs      v26.16b, v26.16b , v28.16b //ABS(p0 - q0) >= Alpha(Alpha <=ABS(p0 - q0))
    smax      v24.16b, v24.16b , v30.16b //max(deltap1,-C0)
    orr       v22.16b, v22.16b , v26.16b //ABS(p0 - q0) >= Alpha  || ABS(q1 - q0) >= Beta ||    ABS(p1 - p0) >= Beta)
    uxtl      v26.4s, v18.4h            //ui_bs
    uaddl     v18.8h, v20.8b, v12.8b    //q2 + ((p0 + q0 + 1) >> 1) L
    cmeq      v26.4s, v26.4s , #0       //ABS(p0 - q0) >= Alpha(Alpha <=ABS(p0 - q0))
    usubw     v18.8h, v18.8h , v10.8b   //(q2 + ((p0 + q0 + 1) >> 1) - q1) L
    uaddl     v20.8h, v21.8b, v13.8b    //q2 + ((p0 + q0 + 1) >> 1) H
    usubw     v18.8h, v18.8h , v10.8b   //(q2 + ((p0 + q0 + 1) >> 1) - 2*q1)L
    usubw     v20.8h, v20.8h , v11.8b   //(q2 + ((p0 + q0 + 1) >> 1) - q1) H
    orr       v26.16b, v26.16b , v22.16b //(ABS(p0 - q0) >= Alpha  || ABS(q1 - q0) >= Beta || ABS(p1 - p0) >= Beta)) &&(ui_bs)
    usubw     v20.8h, v20.8h , v11.8b   //(q2 + ((p0 + q0 + 1) >> 1) - 2*q1) H
    sqshrn    v18.8b, v18.8h, #1        //((q2 + ((p0 + q0 + 1) >> 1) - (q1 << 1)) >> 1) L
    uabd      v22.16b  , v2.16b, v6.16b //ABS(q1 - q0)
    sqshrn    v19.8b, v20.8h, #1        //((q2 + ((p0 + q0 + 1) >> 1) - (q1 << 1)) >> 1) H
    mov       v18.d[1], v19.d[0]
    uabd      v20.16b  , v12.16b, v8.16b //ABS(q1 - q0)
    cmhi      v22.16b, v28.16b , v22.16b //Ap < Beta
    smin      v18.16b, v18.16b , v16.16b //min(delatq1,C0)
    cmhi      v20.16b, v28.16b , v20.16b //Aq <Beta
    usubl     v28.8h, v8.8b, v6.8b      //(q0 - p0) L
    smax      v18.16b, v18.16b , v30.16b //max(deltaq1,-C0)
    usubl     v30.8h, v9.8b, v7.8b      //(q0 - p0) H
    shl       v28.8h, v28.8h, #2        //(q0 - p0)<<2 L
    sub       v16.16b, v16.16b , v22.16b //C0 + (Ap < Beta)
    shl       v30.8h, v30.8h, #2        //(q0 - p0) << 2) H
    uaddw     v28.8h, v28.8h , v4.8b    //((q0 - p0) << 2) + (p1  L
    uaddw     v30.8h, v30.8h , v5.8b    //((q0 - p0) << 2) + (p1 H
    usubw     v28.8h, v28.8h , v10.8b   //((q0 - p0) << 2) + (p1 - q1) L
    usubw     v30.8h, v30.8h , v11.8b   //((q0 - p0) << 2) + (p1 - q1) H
    bic       v22.16b, v22.16b , v26.16b //final condition for p1
    rshrn     v28.8b, v28.8h, #3        //delta = ((((q0 - p0) << 2) + (p1 - q1) + 4) >> 3); L
    rshrn     v29.8b, v30.8h, #3        //delta = ((((q0 - p0) << 2) + (p1 - q1) + 4) >> 3) H
    mov       v28.d[1], v29.d[0]
    sub       v16.16b, v16.16b , v20.16b //C0 + (Ap < Beta) + (Aq < Beta)
    bic       v20.16b, v20.16b , v26.16b //final condition for q1
    abs       v30.16b, v28.16b          //abs(delta)
    and       v24.16b, v24.16b , v22.16b //delatp1
    and       v18.16b, v18.16b , v20.16b //delta q1
    umin      v30.16b, v30.16b , v16.16b //min((abs(delta),C)
    add       v4.16b, v4.16b , v24.16b  //p1+deltap1
    add       v10.16b, v10.16b , v18.16b //q1+deltaq1
    mov       v5.d[0], v4.d[1]
    mov       v11.d[0], v10.d[1]
    bic       v30.16b, v30.16b , v26.16b //abs(delta) of pixels to be changed only
    // VCGE.S8 Q14,    Q14,#0                    //sign(delta)
    cmge      v28.16b, v28.16b , #0
    uqsub     v22.16b, v6.16b , v30.16b //clip(p0-delta)

    trn1      v21.8b, v0.8b, v2.8b
    trn2      v2.8b, v0.8b, v2.8b       //row1 &2
    mov       v0.8b, v21.8b
    uqadd     v6.16b, v6.16b , v30.16b  //clip(p0+delta)

    trn1      v21.8b, v1.8b, v3.8b
    trn2      v3.8b, v1.8b, v3.8b       //row9 &10
    mov       v1.8b, v21.8b
    uqadd     v24.16b, v8.16b , v30.16b //clip(q0+delta)
    trn1      v21.8b, v12.8b, v14.8b
    trn2      v14.8b, v12.8b, v14.8b    //row7 & 8
    mov       v12.8b, v21.8b
    uqsub     v8.16b, v8.16b , v30.16b  //clip(q0-delta)
    trn1      v21.8b, v13.8b, v15.8b
    trn2      v15.8b, v13.8b, v15.8b    //row15 & 16
    mov       v13.8b, v21.8b
    bif       v6.16b, v22.16b , v28.16b //p0
    bif       v8.16b, v24.16b , v28.16b //q0
    mov       v7.d[0], v6.d[1]
    mov       v9.d[0], v8.d[1]
    trn1      v21.8b, v4.8b, v6.8b
    trn2      v6.8b, v4.8b, v6.8b       //row3&row4
    mov       v4.8b, v21.8b
    trn1      v21.8b, v8.8b, v10.8b
    trn2      v10.8b, v8.8b, v10.8b     //row5&6
    mov       v8.8b, v21.8b
    trn1      v21.8b, v5.8b, v7.8b
    trn2      v7.8b, v5.8b, v7.8b       //row11 & 12
    mov       v5.8b, v21.8b
    trn1      v21.8b, v9.8b, v11.8b
    trn2      v11.8b, v9.8b, v11.8b     //row13 &14
    mov       v9.8b, v21.8b
    trn1      v21.4h, v2.4h, v6.4h
    trn2      v6.4h, v2.4h, v6.4h       //row2 & row4
    mov       v2.8b, v21.8b
    trn1      v21.4h, v10.4h, v14.4h
    trn2      v14.4h, v10.4h, v14.4h    //row6 & row8
    mov       v10.8b, v21.8b
    trn1      v21.4h, v3.4h, v7.4h
    trn2      v7.4h, v3.4h, v7.4h       //row10 & 12
    mov       v3.8b, v21.8b
    trn1      v21.4h, v11.4h, v15.4h
    trn2      v15.4h, v11.4h, v15.4h    //row14 & row16
    mov       v11.8b, v21.8b
    trn1      v21.2s, v6.2s, v14.2s
    trn2      v14.2s, v6.2s, v14.2s     //row4 & 8
    mov       v6.8b, v21.8b
    trn1      v21.2s, v7.2s, v15.2s
    trn2      v15.2s, v7.2s, v15.2s     //row 12 & 16
    mov       v7.8b, v21.8b
    //now Q3 ->p0 and Q7->q3
    trn1      v21.4h, v0.4h, v4.4h
    trn2      v4.4h, v0.4h, v4.4h       //row1 & 3
    mov       v0.8b, v21.8b
    trn1      v21.4h, v8.4h, v12.4h
    trn2      v12.4h, v8.4h, v12.4h     //row 5 & 7
    mov       v8.8b, v21.8b
    trn1      v21.4h, v1.4h, v5.4h
    trn2      v5.4h, v1.4h, v5.4h       //row9 & row11
    mov       v1.8b, v21.8b
    trn1      v21.4h, v9.4h, v13.4h
    trn2      v13.4h, v9.4h, v13.4h     //row13 & row15
    mov       v9.8b, v21.8b
    sub       x0, x0, x1, lsl#4         //restore pointer
    trn1      v21.2s, v0.2s, v8.2s
    trn2      v8.2s, v0.2s, v8.2s       //row1 & row5
    mov       v0.8b, v21.8b
    trn1      v21.2s, v1.2s, v9.2s
    trn2      v9.2s, v1.2s, v9.2s       //row9 & 13
    mov       v1.8b, v21.8b
    trn1      v21.2s, v2.2s, v10.2s
    trn2      v10.2s, v2.2s, v10.2s     //row2 &6
    mov       v2.8b, v21.8b
    trn1      v21.2s, v3.2s, v11.2s
    trn2      v11.2s, v3.2s, v11.2s     //row10&row14
    mov       v3.8b, v21.8b
    trn1      v21.2s, v4.2s, v12.2s
    trn2      v12.2s, v4.2s, v12.2s     //row3 & 7
    mov       v4.8b, v21.8b
    trn1      v21.2s, v5.2s, v13.2s
    trn2      v13.2s, v5.2s, v13.2s     //row11 & row15
    mov       v5.8b, v21.8b
    st1       {v0.8b}, [x0], x1         //row1
    st1       {v2.8b}, [x0], x1         //row2
    st1       {v4.8b}, [x0], x1         //row3
    st1       {v6.8b}, [x0], x1         //row4
    st1       {v8.8b}, [x0], x1         //row5
    st1       {v10.8b}, [x0], x1        //row6
    st1       {v12.8b}, [x0], x1        //row7
    st1       {v14.8b}, [x0], x1        //row8
    st1       {v1.8b}, [x0], x1         //row9
    st1       {v3.8b}, [x0], x1         //row10
    st1       {v5.8b}, [x0], x1         //row11
    st1       {v7.8b}, [x0], x1         //row12
    st1       {v9.8b}, [x0], x1         //row13
    st1       {v11.8b}, [x0], x1        //row14
    st1       {v13.8b}, [x0], x1        //row15
    st1       {v15.8b}, [x0], x1        //row16

    // LDMFD sp!,{x12,pc}
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret



///**
//*******************************************************************************
//*
//* @brief
//*     Performs filtering of a luma block vertical edge when the
//*     boundary strength is set to 4
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
//* @param[in] w2 - alpha
//*  Alpha Value for the boundary
//*
//* @param[in] w3 - beta
//*  Beta Value for the boundary
//*
//* @returns
//*  None
//*
//* @remarks
//*  None
//*
//*******************************************************************************
//*/

    .global ih264_deblk_luma_vert_bs4_av8

ih264_deblk_luma_vert_bs4_av8:

    // STMFD sp!,{x12,x14}
    push_v_regs
    stp       x19, x20, [sp, #-16]!

    sub       x0, x0, #4                //pointer uc_edgePixel-4
    mov       x17, x0
    //loading p3:p2:p1:p0:q0:q1:q2:q3 for every row
    ld1       {v0.8b}, [x0], x1         //row1
    ld1       {v2.8b}, [x0], x1         //row2
    ld1       {v4.8b}, [x0], x1         //row3
    ld1       {v6.8b}, [x0], x1         //row4
    ld1       {v8.8b}, [x0], x1         //row5
    ld1       {v10.8b}, [x0], x1        //row6
    ld1       {v12.8b}, [x0], x1        //row7
    ld1       {v14.8b}, [x0], x1        //row8
    ld1       {v1.8b}, [x0], x1         //row9
    ld1       {v3.8b}, [x0], x1         //row10
    ld1       {v5.8b}, [x0], x1         //row11
    ld1       {v7.8b}, [x0], x1         //row12
    ld1       {v9.8b}, [x0], x1         //row13
    ld1       {v11.8b}, [x0], x1        //row14
    ld1       {v13.8b}, [x0], x1        //row15
    ld1       {v15.8b}, [x0], x1        //row16

    //taking two 8x8 transposes
    //2X2 transposes
    trn1      v21.8b, v0.8b, v2.8b
    trn2      v2.8b, v0.8b, v2.8b       //row1 &2
    mov       v0.8b, v21.8b
    trn1      v21.8b, v4.8b, v6.8b
    trn2      v6.8b, v4.8b, v6.8b       //row3&row4
    mov       v4.8b, v21.8b
    trn1      v21.8b, v8.8b, v10.8b
    trn2      v10.8b, v8.8b, v10.8b     //row5&6
    mov       v8.8b, v21.8b
    trn1      v21.8b, v12.8b, v14.8b
    trn2      v14.8b, v12.8b, v14.8b    //row7 & 8
    mov       v12.8b, v21.8b
    trn1      v21.8b, v1.8b, v3.8b
    trn2      v3.8b, v1.8b, v3.8b       //row9 &10
    mov       v1.8b , v21.8b
    trn1      v21.8b, v5.8b, v7.8b
    trn2      v7.8b, v5.8b, v7.8b       //row11 & 12
    mov       v5.8b , v21.8b
    trn1      v21.8b, v9.8b, v11.8b
    trn2      v11.8b, v9.8b, v11.8b     //row13 &14
    mov       v9.8b , v21.8b
    trn1      v21.8b, v13.8b, v15.8b
    trn2      v15.8b, v13.8b, v15.8b    //row15 & 16
    mov       v13.8b , v21.8b
    //4x4 transposes
    trn1      v21.4h, v2.4h, v6.4h
    trn2      v6.4h, v2.4h, v6.4h       //row2 & row4
    mov       v2.8b, v21.8b
    trn1      v21.4h, v10.4h, v14.4h
    trn2      v14.4h, v10.4h, v14.4h    //row6 & row8
    mov       v10.8b , v21.8b
    trn1      v21.4h, v3.4h, v7.4h
    trn2      v7.4h, v3.4h, v7.4h       //row10 & 12
    mov       v3.8b, v21.8b
    trn1      v21.4h, v11.4h, v15.4h
    trn2      v15.4h, v11.4h, v15.4h    //row14 & row16
    mov       v11.8b, v21.8b
    trn1      v21.2s, v6.2s, v14.2s
    trn2      v14.2s, v6.2s, v14.2s     //row4 & 8
    mov       v6.8b, v21.8b
    trn1      v21.2s, v7.2s, v15.2s
    trn2      v15.2s, v7.2s, v15.2s     //row 12 & 16
    mov       v7.8b, v21.8b
    //now Q3 ->p0 and Q7->q3
    trn1      v21.4h, v0.4h, v4.4h
    trn2      v4.4h, v0.4h, v4.4h       //row1 & 3
    mov       v0.8b , v21.8b
    trn1      v21.4h, v8.4h, v12.4h
    trn2      v12.4h, v8.4h, v12.4h     //row 5 & 7
    mov       v8.8b, v21.8b
    trn1      v21.4h, v1.4h, v5.4h
    trn2      v5.4h, v1.4h, v5.4h       //row9 & row11
    mov       v1.8b, v21.8b
    trn1      v21.4h, v9.4h, v13.4h
    trn2      v13.4h, v9.4h, v13.4h     //row13 & row15
    mov       v9.8b , v21.8b
    trn1      v21.2s, v0.2s, v8.2s
    trn2      v8.2s, v0.2s, v8.2s       //row1 & row5
    mov       v0.8b, v21.8b
    trn1      v21.2s, v1.2s, v9.2s
    trn2      v9.2s, v1.2s, v9.2s       //row9 & 13
    mov       v1.8b, v21.8b
    //now Q0->p3 & Q4->q0
    //starting processing as p0 and q0 are now ready
    //now Q1->p2 & Q5->q1
    mov       v31.d[0], v14.d[0]
    mov       v31.d[1], v15.d[0]
    trn1      v21.2s, v4.2s, v12.2s
    trn2      v12.2s, v4.2s, v12.2s     //row3 & 7
    mov       v4.8b, v21.8b
    movi      v28.8h, #2
    trn1      v21.2s, v5.2s, v13.2s
    trn2      v13.2s, v5.2s, v13.2s     //row11 & row15
    mov       v5.8b, v21.8b
    uaddl     v16.8h, v6.8b, v8.8b      //p0+q0 L
    trn1      v21.2s, v2.2s, v10.2s
    trn2      v10.2s, v2.2s, v10.2s     //row2 &6
    mov       v2.8b, v21.8b
    uaddl     v18.8h, v7.8b, v9.8b      //p0+q0 H
    trn1      v21.2s, v3.2s, v11.2s
    trn2      v11.2s, v3.2s, v11.2s     //row10&row14
    mov       v3.8b, v21.8b
    uaddw     v20.8h, v16.8h , v4.8b    //p0+q0+p1 L
    uaddw     v22.8h, v18.8h , v5.8b    //p0+q0+p1 H
    uaddl     v24.8h, v2.8b, v10.8b     //p2+q1 L
    uaddl     v26.8h, v3.8b, v11.8b     //p2+q1 H
    mla       v24.8h, v20.8h , v28.8h   //p2 + X2(p1) + X2(p0) + X2(q0) + q1 L
    mla       v26.8h, v22.8h , v28.8h   //p2 + X2(p1) + X2(p0) + X2(q0) + q1 H
    movi      v28.16b, #2
    uaddw     v16.8h, v20.8h , v2.8b    //p0+q0+p1+p2 L
    uaddw     v18.8h, v22.8h , v3.8b    //p0+q0+p1+p2 H
    dup       v30.16b, w2               //duplicate alpha
    rshrn     v20.8b, v16.8h, #2        //(p2 + p1 + p0 + q0 + 2) >> 2)L p1'
    rshrn     v21.8b, v18.8h, #2        //(p2 + p1 + p0 + q0 + 2) >> 2)H p1'
    mov       v20.d[1] , v21.d[0]
    mov       v0.d[1] , v1.d[0]
    mov       v2.d[1] , v3.d[0]
    mov       v4.d[1] , v5.d[0]
    mov       v6.d[1] , v7.d[0]
    mov       v8.d[1] , v9.d[0]
    mov       v10.d[1] , v11.d[0]
    mov       v12.d[1] , v13.d[0]
    mov       v14.d[1] , v15.d[0]
    uabd      v22.16b  , v6.16b, v8.16b
    usra      v28.16b, v30.16b, #2      //alpha >>2 +2
    uabd      v30.16b  , v2.16b, v6.16b
    rshrn     v24.8b, v24.8h, #3        //((p2 + X2(p1) + X2(p0) + X2(q0) + q1 + 4) >> 3) L p0'
    rshrn     v25.8b, v26.8h, #3        //((p2 + X2(p1) + X2(p0) + X2(q0) + q1 + 4) >> 3) H p0'
    mov       v24.d[1] , v25.d[0]
    dup       v26.16b, w3               //beta
    cmhi      v28.16b, v28.16b , v22.16b //ABS(p0 - q0) <((Alpha >>2) + 2)
    uaddl     v22.8h, v6.8b, v10.8b     //p0+q1 L
    cmhi      v14.16b, v26.16b , v30.16b //beta>Ap
    uaddl     v30.8h, v7.8b, v11.8b     //p0+q1 H
    uaddw     v22.8h, v22.8h , v4.8b    //p0+q1+p1 L
    uaddw     v30.8h, v30.8h , v5.8b    //p0+q1+p1 H
    uaddw     v22.8h, v22.8h , v4.8b    //p0+q1+2*p1 L
    uaddw     v30.8h, v30.8h , v5.8b    //p0+q1+2*p1 H
    and       v14.16b, v14.16b , v28.16b //(Ap < Beta && ABS(p0 - q0) <((Alpha >>2) + 2)
    rshrn     v22.8b, v22.8h, #2        //((X2(p1) + p0 + q1 + 2) >> 2) L p0"
    rshrn     v23.8b, v30.8h, #2        //((X2(p1) + p0 + q1 + 2) >> 2) H p0"
    mov       v22.d[1] , v23.d[0]
    uaddl     v30.8h, v2.8b, v0.8b      //p2+p3 L
    bif       v24.16b, v22.16b , v14.16b //p0' or p0 "
    uaddl     v22.8h, v3.8b, v1.8b      //p2+p3 H
    add       v30.8h, v30.8h , v30.8h   //2*(p2+p3) L
    add       v22.8h, v22.8h , v22.8h   //2*(p2+p3)H
    add       v16.8h, v16.8h , v30.8h   //(X2(p3) + X3(p2) + p1 + p0 + q0) L
    add       v18.8h, v18.8h , v22.8h   //(X2(p3) + X3(p2) + p1 + p0 + q0) H
    uabd      v30.16b  , v12.16b, v8.16b
    uabd      v22.16b  , v10.16b, v8.16b
    rshrn     v16.8b, v16.8h, #3        //((X2(p3) + X3(p2) + p1 + p0 + q0 + 4) >> 3); L p2'
    rshrn     v17.8b, v18.8h, #3        //((X2(p3) + X3(p2) + p1 + p0 + q0 + 4) >> 3); H p2'
    mov       v16.d[1] , v17.d[0]
    uabd      v18.16b  , v4.16b, v6.16b
    cmhi      v30.16b, v26.16b , v30.16b //Aq < Beta
    cmhs      v22.16b, v22.16b, v26.16b
    cmhs      v18.16b, v18.16b, v26.16b
    dup       v26.16b, w2               //duplicate alpha
    and       v30.16b, v30.16b , v28.16b //(Aq < Beta && ABS(p0 - q0) <((Alpha >>2) + 2))
    uabd      v28.16b  , v6.16b, v8.16b
    orr       v22.16b, v22.16b , v18.16b //ABS(p1 - p0) >= Beta || ABS(q1 - q0) >= Beta
    uaddl     v18.8h, v6.8b, v8.8b      //p0+q0 L
    cmhs      v28.16b, v28.16b, v26.16b
    uaddl     v26.8h, v7.8b, v9.8b      //p0+q0 H
    uaddw     v18.8h, v18.8h , v10.8b   //p0+q0+q1 L
    orr       v22.16b, v22.16b , v28.16b //ABS(p1 - p0) >= Beta || ABS(q1 - q0) >= Beta||ABS(p0 - q0) >= Alpha
    uaddw     v26.8h, v26.8h , v11.8b   //p0+q0+q1 H
    bic       v14.16b, v14.16b , v22.16b //final condn for p's
    movi      v28.16b, #2
    bif       v6.16b, v24.16b , v22.16b //final p0
    bit       v2.16b, v16.16b , v14.16b //final p2
    bif       v20.16b, v4.16b , v14.16b //final p1
    mov       v7.d[0] , v6.d[1]
    mov       v3.d[0] , v2.d[1]
    mov       v21.d[0] , v20.d[1]
    uaddl     v24.8h, v8.8b, v4.8b      //q0+p1 L
    umlal     v24.8h, v10.8b, v28.8b    //X2(q1) + q0 + p1 L
    uaddl     v16.8h, v9.8b, v5.8b      //q0+p1 H
    umlal     v16.8h, v11.8b, v28.8b    //X2(q1) + q0 + p1 H
    movi      v28.8h, #2
    uaddl     v14.8h, v4.8b, v12.8b     //p1+q2 L
    mla       v14.8h, v18.8h , v28.8h   //p1 + X2(p0) + X2(q0) + X2(q1) + q2L
    uaddl     v4.8h, v5.8b, v13.8b      //p1+q2H
    mla       v4.8h, v26.8h , v28.8h    //p1 + X2(p0) + X2(q0) + X2(q1) + q2H
    rshrn     v24.8b, v24.8h, #2        //(X2(q1) + q0 + p1 + 2) >> 2; L q0'
    rshrn     v25.8b, v16.8h, #2        //(X2(q1) + q0 + p1 + 2) >> 2; H q0'
    mov       v24.d[1] , v25.d[0]
    uaddw     v18.8h, v18.8h , v12.8b   //p0 + q0 + q1 + q2 L
    uaddw     v26.8h, v26.8h , v13.8b   //p0 + q0 + q1 + q2 H
    rshrn     v16.8b, v14.8h, #3        //(p1 + X2(p0) + X2(q0) + X2(q1) + q2 + 4) >> 3 L qo"
    mov       v14.16b, v31.16b
    rshrn     v17.8b, v4.8h, #3         //(p1 + X2(p0) + X2(q0) + X2(q1) + q2 + 4) >> 3 H qo"
    mov       v16.d[1] , v17.d[0]
    rshrn     v4.8b, v18.8h, #2         //p0 + q0 + q1 + q2 + 2)>>2 L q1'
    rshrn     v5.8b, v26.8h, #2         //p0 + q0 + q1 + q2 + 2)>>2 H q1'
    mov       v4.d[1] , v5.d[0]
    bit       v24.16b, v16.16b , v30.16b //q0' or q0"
    bic       v30.16b, v30.16b , v22.16b //final condn for q's
    trn1      v31.8b, v0.8b, v2.8b
    trn2      v2.8b, v0.8b, v2.8b       //row1 &2
    mov       v0.8b, v31.8b
    bit       v10.16b, v4.16b , v30.16b
    mov       v11.d[0] , v10.d[1]
    mov       v25.d[0] , v24.d[1]
    mov       v31.d[0] , v30.d[1]
    trn1      v31.8b, v1.8b, v3.8b
    trn2      v3.8b, v1.8b, v3.8b       //row9 &10
    mov       v1.8b, v31.8b
    uaddl     v16.8h, v12.8b, v14.8b    //q2+q3 L
    trn1      v31.8b, v20.8b, v6.8b
    trn2      v6.8b, v20.8b, v6.8b      //row3&row4
    mov       v20.8b , v31.8b
    uaddl     v4.8h, v13.8b, v15.8b     //q2+q3 H
    trn1      v31.8b, v21.8b, v7.8b
    trn2      v7.8b, v21.8b, v7.8b      //row11 & 12
    mov       v21.8b , v31.8b
    mla       v18.8h, v16.8h , v28.8h   //X2(q3) + X3(q2) + q1 + q0 + p0 L
    trn1      v31.4h, v2.4h, v6.4h
    trn2      v6.4h, v2.4h, v6.4h       //row2 & row4
    mov       v2.8b, v31.8b
    mla       v26.8h, v4.8h , v28.8h    //X2(q3) + X3(q2) + q1 + q0 + p0 H
    trn1      v31.4h, v3.4h, v7.4h
    trn2      v7.4h, v3.4h, v7.4h       //row10 & 12
    mov       v3.8b , v31.8b
    bif       v8.16b, v24.16b , v22.16b //final q0
    mov       v9.d[0] , v8.d[1]
    trn1      v31.4h, v0.4h, v20.4h
    trn2      v20.4h, v0.4h, v20.4h     //row1 & 3
    mov       v0.8b , v31.8b
    rshrn     v18.8b, v18.8h, #3        //(X2(q3) + X3(q2) + q1 + q0 + p0 + 4) >> 3; L
    trn1      v31.4h, v1.4h, v21.4h
    trn2      v21.4h, v1.4h, v21.4h     //row9 & row11
    mov       v1.8b, v31.8b
    rshrn     v19.8b, v26.8h, #3        //(X2(q3) + X3(q2) + q1 + q0 + p0 + 4) >> 3; H
    mov       v18.d[1] , v19.d[0]
    trn1      v31.8b, v8.8b, v10.8b
    trn2      v10.8b, v8.8b, v10.8b     //row5&6
    mov       v8.8b, v31.8b
    bit       v12.16b, v18.16b , v30.16b //final q2
    mov       v13.d[0] , v12.d[1]
    trn1      v31.8b, v9.8b, v11.8b
    trn2      v11.8b, v9.8b, v11.8b     //row13 &14
    mov       v9.8b, v31.8b
    trn1      v31.8b, v12.8b, v14.8b
    trn2      v14.8b, v12.8b, v14.8b    //row7 & 8
    mov       v12.8b, v31.8b
    trn1      v31.8b, v13.8b, v15.8b
    trn2      v15.8b, v13.8b, v15.8b    //row15 & 16
    mov       v13.8b , v31.8b
    trn1      v31.4h, v10.4h, v14.4h
    trn2      v14.4h, v10.4h, v14.4h    //row6 & row8
    mov       v10.8b, v31.8b
    trn1      v31.4h, v11.4h, v15.4h
    trn2      v15.4h, v11.4h, v15.4h    //row14 & row16
    mov       v11.8b, v31.8b
    //now Q3 ->p0 and Q7->q3
    trn1      v31.4h, v8.4h, v12.4h
    trn2      v12.4h, v8.4h, v12.4h     //row 5 & 7
    mov       v8.8b, v31.8b
    trn1      v31.4h, v9.4h, v13.4h
    trn2      v13.4h, v9.4h, v13.4h     //row13 & row15
    mov       v9.8b, v31.8b
    sub       x0, x0, x1, lsl#4         //restore pointer
    trn1      v31.2s, v6.2s, v14.2s
    trn2      v14.2s, v6.2s, v14.2s     //row4 & 8
    mov       v6.8b , v31.8b
    trn1      v31.2s, v7.2s, v15.2s
    trn2      v15.2s, v7.2s, v15.2s     //row 12 & 16
    mov       v7.8b, v31.8b
    trn1      v31.2s, v0.2s, v8.2s
    trn2      v8.2s, v0.2s, v8.2s       //row1 & row5
    mov       v0.8b , v31.8b
    trn1      v31.2s, v1.2s, v9.2s
    trn2      v9.2s, v1.2s, v9.2s       //row9 & 13
    mov       v1.8b , v31.8b
    trn1      v31.2s, v2.2s, v10.2s
    trn2      v10.2s, v2.2s, v10.2s     //row2 &6
    mov       v2.8b , v31.8b
    trn1      v31.2s, v3.2s, v11.2s
    trn2      v11.2s, v3.2s, v11.2s     //row10&row14
    mov       v3.8b , v31.8b
    trn1      v31.2s, v20.2s, v12.2s
    trn2      v12.2s, v20.2s, v12.2s    //row3 & 7
    mov       v20.8b , v31.8b
    trn1      v31.2s, v21.2s, v13.2s
    trn2      v13.2s, v21.2s, v13.2s    //row11 & row15
    mov       v21.8b, v31.8b
    st1       {v0.8b}, [x0], x1         //row1
    st1       {v2.8b}, [x0], x1         //row2
    st1       {v20.8b}, [x0], x1        //row3
    st1       {v6.8b}, [x0], x1         //row4
    st1       {v8.8b}, [x0], x1         //row5
    st1       {v10.8b}, [x0], x1        //row6
    st1       {v12.8b}, [x0], x1        //row7
    st1       {v14.8b}, [x0], x1        //row8
    st1       {v1.8b}, [x0], x1         //row9
    st1       {v3.8b}, [x0], x1         //row10
    st1       {v21.8b}, [x0], x1        //row11
    st1       {v7.8b}, [x0], x1         //row12
    st1       {v9.8b}, [x0], x1         //row13
    st1       {v11.8b}, [x0], x1        //row14
    st1       {v13.8b}, [x0], x1        //row15
    st1       {v15.8b}, [x0], x1        //row16

    // LDMFD sp!,{x12,pc}
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret


