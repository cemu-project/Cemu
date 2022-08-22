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
///**
//******************************************************************************
//* @file
//*  ih264_inter_pred_chroma_av8.s
//*
//* @brief
//*  Contains function definitions for inter prediction  interpolation.
//*
//* @author
//*  Ittaim
//*
//* @par List of Functions:
//*
//*  - ih264_inter_pred_chroma_av8()
//*
//* @remarks
//*  None
//*
//*******************************************************************************
//*/

///* All the functions here are replicated from ih264_inter_pred_filters.c
//

///**
///**
///**
//
///**
//*******************************************************************************
//*
//* @brief
//*    Interprediction chroma filter
//*
//* @par Description:
//*   Applies filtering to chroma samples as mentioned in
//*    sec 8.4.2.2.2 titled "chroma sample interpolation process"
//*
//* @param[in] pu1_src
//*  UWORD8 pointer to the source containing alternate U and V samples
//*
//* @param[out] pu1_dst
//*  UWORD8 pointer to the destination
//*
//* @param[in] src_strd
//*  integer source stride
//*
//* @param[in] dst_strd
//*  integer destination stride
//*
//* @param[in]uc_dx
//*  dx value where the sample is to be produced(refer sec 8.4.2.2.2 )
//*
//* @param[in] uc_dy
//*  dy value where the sample is to be produced(refer sec 8.4.2.2.2 )
//*
//* @param[in] ht
//*  integer height of the array
//*
//* @param[in] wd
//*  integer width of the array
//*
//* @returns
//*
//* @remarks
//*  None
//*
//*******************************************************************************
//*/

//void ih264_inter_pred_chroma(UWORD8 *pu1_src,
//                             UWORD8 *pu1_dst,
//                             WORD32 src_strd,
//                             WORD32 dst_strd,
//                             WORD32 u1_dx,
//                             WORD32 u1_dy,
//                             WORD32 ht,
//                             WORD32 wd)
//**************Variables Vs Registers*****************************************
//    x0 => *pu1_src
//    x1 => *pu1_dst
//    w2 =>  src_strd
//    w3 =>  dst_strd
//    w4 =>  u1_dx
//    w5 =>  u1_dy
//    w6 =>  height
//    w7 =>  width
//
.text
.p2align 2
.include "ih264_neon_macros.s"



    .global ih264_inter_pred_chroma_av8

ih264_inter_pred_chroma_av8:



    // STMFD sp!, {x4-x12, x14}          //store register values to stack
    push_v_regs
    stp       x19, x20, [sp, #-16]!
    sxtw      x2, w2
    sxtw      x3, w3
    sxtw      x4, w4
    sxtw      x5, w5
    sxtw      x6, w6
    sxtw      x7, w7





    sub       x20, x4, #8               //8-u1_dx
    neg       x8, x20
    sub       x20, x5, #8               //8-u1_dy
    neg       x9, x20
    mul       x10, x8, x9               //
    mul       x11, x4, x9               //

    dup       v28.8b, w10
    dup       v29.8b, w11

    mul       x10, x8, x5               //
    mul       x11, x4, x5               //

    dup       v30.8b, w10
    dup       v31.8b, w11

    subs      x12, x7, #2               //if wd=4 branch to loop_4
    beq       loop_2
    subs      x12, x7, #4               //if wd=8 branch to loop_8
    beq       loop_4

loop_8:
    ld1       {v0.8b, v1.8b, v2.8b}, [x0], x2 //// Load row0 ;
    ext       v3.8b, v0.8b , v1.8b , #2
    ld1       {v5.8b, v6.8b, v7.8b}, [x0], x2 //// Load row1;
    umull     v20.8h, v0.8b, v28.8b
    ext       v8.8b, v5.8b , v6.8b , #2
    umlal     v20.8h, v3.8b, v29.8b
    ext       v9.8b, v6.8b , v7.8b , #2
    umlal     v20.8h, v5.8b, v30.8b
    ext       v4.8b, v1.8b , v2.8b , #2
    umlal     v20.8h, v8.8b, v31.8b
    sqrshrun  v26.8b, v20.8h, #6
    umull     v22.8h, v1.8b, v28.8b
    ld1       {v10.8b, v11.8b, v12.8b}, [x0], x2 //// Load row2 ;
    umlal     v22.8h, v4.8b, v29.8b
    ext       v13.8b, v10.8b , v11.8b , #2
    umlal     v22.8h, v6.8b, v30.8b
    ext       v14.8b, v11.8b , v12.8b , #2
    umlal     v22.8h, v9.8b, v31.8b
    sqrshrun  v27.8b, v22.8h, #6
    umull     v24.8h, v5.8b, v28.8b
    st1       { v26.8b, v27.8b}, [x1], x3 ////Store dest row
    umlal     v24.8h, v8.8b, v29.8b
    ld1       {v0.8b, v1.8b, v2.8b}, [x0], x2 //// Load row3 ;
    umlal     v24.8h, v10.8b, v30.8b
    ext       v3.8b, v0.8b , v1.8b , #2
    umlal     v24.8h, v13.8b, v31.8b
    ext       v4.8b, v1.8b , v2.8b , #2
    umull     v16.8h, v6.8b, v28.8b
    sqrshrun  v18.8b, v24.8h, #6
    umlal     v16.8h, v9.8b, v29.8b
    umlal     v16.8h, v11.8b, v30.8b
    umlal     v16.8h, v14.8b, v31.8b
    sqrshrun  v19.8b, v16.8h, #6
    st1       {v18.8b, v19.8b}, [x1], x3 // store row 1
    umull     v20.8h, v10.8b, v28.8b
    umlal     v20.8h, v13.8b, v29.8b
    umlal     v20.8h, v0.8b, v30.8b
    umlal     v20.8h, v3.8b, v31.8b
    sqrshrun  v26.8b, v20.8h, #6
    umull     v24.8h, v11.8b, v28.8b
    ld1       {v5.8b, v6.8b, v7.8b}, [x0], x2 //// Load row4;
    umlal     v24.8h, v14.8b, v29.8b
    ext       v8.8b, v5.8b , v6.8b , #2
    umlal     v24.8h, v1.8b, v30.8b
    ext       v9.8b, v6.8b , v7.8b , #2
    umlal     v24.8h, v4.8b, v31.8b
    umull     v20.8h, v0.8b, v28.8b
    sqrshrun  v27.8b, v24.8h, #6
    umlal     v20.8h, v3.8b, v29.8b
    st1       { v26.8b, v27.8b}, [x1], x3 ////Store dest row2
    umlal     v20.8h, v5.8b, v30.8b
    umlal     v20.8h, v8.8b, v31.8b
    umull     v22.8h, v1.8b, v28.8b
    umlal     v22.8h, v4.8b, v29.8b
    umlal     v22.8h, v6.8b, v30.8b
    sqrshrun  v26.8b, v20.8h, #6
    umlal     v22.8h, v9.8b, v31.8b
    subs      x12, x6, #4
    sqrshrun  v27.8b, v22.8h, #6
    st1       { v26.8b, v27.8b}, [x1], x3 ////Store dest row3

    beq       end_func                  //If ht=4

    ld1       {v10.8b, v11.8b, v12.8b}, [x0], x2 //// Load row5
    ext       v13.8b, v10.8b , v11.8b , #2
    umull     v24.8h, v5.8b, v28.8b
    ext       v14.8b, v11.8b , v12.8b , #2
    ld1       {v0.8b, v1.8b, v2.8b}, [x0], x2 //// Load row6;
    umlal     v24.8h, v8.8b, v29.8b
    umlal     v24.8h, v10.8b, v30.8b
    umlal     v24.8h, v13.8b, v31.8b
    ext       v3.8b, v0.8b , v1.8b , #2
    umull     v16.8h, v6.8b, v28.8b
    sqrshrun  v18.8b, v24.8h, #6
    umlal     v16.8h, v9.8b, v29.8b
    umlal     v16.8h, v11.8b, v30.8b
    umlal     v16.8h, v14.8b, v31.8b
    ext       v4.8b, v1.8b , v2.8b , #2
    sqrshrun  v19.8b, v16.8h, #6
    st1       { v18.8b, v19.8b}, [x1], x3 // store row 4
    umull     v20.8h, v10.8b, v28.8b
    umlal     v20.8h, v13.8b, v29.8b
    umlal     v20.8h, v0.8b, v30.8b
    umlal     v20.8h, v3.8b, v31.8b
    ld1       {v5.8b, v6.8b, v7.8b}, [x0], x2 //// Load row7;
    sqrshrun  v26.8b, v20.8h, #6
    umull     v24.8h, v11.8b, v28.8b
    umlal     v24.8h, v14.8b, v29.8b
    ext       v8.8b, v5.8b , v6.8b , #2
    umlal     v24.8h, v1.8b, v30.8b
    umlal     v24.8h, v4.8b, v31.8b
    ext       v9.8b, v6.8b , v7.8b , #2
    sqrshrun  v27.8b, v24.8h, #6
    st1       {v26.8b, v27.8b}, [x1], x3 ////Store dest row5
    umull     v20.8h, v0.8b, v28.8b
    umlal     v20.8h, v3.8b, v29.8b
    umlal     v20.8h, v5.8b, v30.8b
    umlal     v20.8h, v8.8b, v31.8b
    ld1       {v10.8b, v11.8b, v12.8b}, [x0], x2 //// Load row8 ;
    sqrshrun  v26.8b, v20.8h, #6
    umull     v22.8h, v1.8b, v28.8b
    umlal     v22.8h, v4.8b, v29.8b
    umlal     v22.8h, v6.8b, v30.8b
    ext       v13.8b, v10.8b , v11.8b , #2
    umlal     v22.8h, v9.8b, v31.8b
    ext       v14.8b, v11.8b , v12.8b , #2
    sqrshrun  v27.8b, v22.8h, #6
    st1       { v26.8b, v27.8b}, [x1], x3 ////Store dest row6
    umull     v24.8h, v5.8b, v28.8b
    umlal     v24.8h, v8.8b, v29.8b
    umlal     v24.8h, v10.8b, v30.8b
    umlal     v24.8h, v13.8b, v31.8b
    umull     v16.8h, v6.8b, v28.8b
    sqrshrun  v18.8b, v24.8h, #6
    umlal     v16.8h, v9.8b, v29.8b
    umlal     v16.8h, v11.8b, v30.8b
    umlal     v16.8h, v14.8b, v31.8b
    sqrshrun  v19.8b, v16.8h, #6
    st1       { v18.8b, v19.8b}, [x1], x3 // store row 7
    b         end_func

loop_4:
    ld1       {v0.8b, v1.8b}, [x0], x2  //// Load row0 ;
    ext       v2.8b, v0.8b , v1.8b , #2
    ld1       {v3.8b, v4.8b}, [x0], x2  //// Load row1;
    ext       v5.8b, v3.8b , v4.8b , #2
    umull     v20.8h, v0.8b, v28.8b
    umlal     v20.8h, v2.8b, v29.8b
    umlal     v20.8h, v3.8b, v30.8b
    umlal     v20.8h, v5.8b, v31.8b
    ld1       {v6.8b, v7.8b}, [x0], x2  //// Load row2
    sqrshrun  v26.8b, v20.8h, #6
    ext       v8.8b, v6.8b , v7.8b , #2
    st1       {v26.8b}, [x1], x3        ////Store dest row0
    umull     v22.8h, v3.8b, v28.8b
    umlal     v22.8h, v5.8b, v29.8b
    umlal     v22.8h, v6.8b, v30.8b
    umlal     v22.8h, v8.8b, v31.8b
    subs      x12, x6, #2
    sqrshrun  v27.8b, v22.8h, #6
    st1       {v27.8b}, [x1], x3        ////Store dest row1
    beq       end_func                  //If ht=2

    ld1       {v9.8b, v10.8b}, [x0], x2 //// Load row3;
    ext       v11.8b, v9.8b , v10.8b , #2
    umull     v24.8h, v6.8b, v28.8b
    umlal     v24.8h, v8.8b, v29.8b
    umlal     v24.8h, v9.8b, v30.8b
    umlal     v24.8h, v11.8b, v31.8b
    ld1       {v0.8b, v1.8b}, [x0], x2  //// Load row4 ;
    sqrshrun  v16.8b, v24.8h, #6
    ext       v2.8b, v0.8b , v1.8b , #2
    st1       {v16.8b}, [x1], x3        ////Store dest row2
    umull     v18.8h, v9.8b, v28.8b
    umlal     v18.8h, v11.8b, v29.8b
    umlal     v18.8h, v0.8b, v30.8b
    umlal     v18.8h, v2.8b, v31.8b
    subs      x12, x6, #4
    sqrshrun  v17.8b, v18.8h, #6
    st1       {v17.8b}, [x1], x3        ////Store dest row3
    beq       end_func                  //If ht=4

    ld1       {v3.8b, v4.8b}, [x0], x2  //// Load row5;
    ext       v5.8b, v3.8b , v4.8b , #2
    umull     v20.8h, v0.8b, v28.8b
    umlal     v20.8h, v2.8b, v29.8b
    umlal     v20.8h, v3.8b, v30.8b
    umlal     v20.8h, v5.8b, v31.8b
    ld1       {v6.8b, v7.8b}, [x0], x2  //// Load row6 ;
    sqrshrun  v26.8b, v20.8h, #6
    ext       v8.8b, v6.8b , v7.8b , #2
    st1       {v26.8b}, [x1], x3        ////Store dest row4
    umull     v22.8h, v3.8b, v28.8b
    umlal     v22.8h, v5.8b, v29.8b
    umlal     v22.8h, v6.8b, v30.8b
    umlal     v22.8h, v8.8b, v31.8b
    ld1       {v9.8b, v10.8b}, [x0], x2 //// Load row7;
    sqrshrun  v27.8b, v22.8h, #6
    ext       v11.8b, v9.8b , v10.8b , #2
    st1       {v27.8b}, [x1], x3        ////Store dest row5
    umull     v24.8h, v6.8b, v28.8b
    umlal     v24.8h, v8.8b, v29.8b
    umlal     v24.8h, v9.8b, v30.8b
    umlal     v24.8h, v11.8b, v31.8b
    ld1       {v0.8b, v1.8b}, [x0], x2  //// Load row8;
    sqrshrun  v16.8b, v24.8h, #6
    ext       v2.8b, v0.8b , v1.8b , #2
    st1       {v16.8b}, [x1], x3        ////Store dest row6
    umull     v18.8h, v9.8b, v28.8b
    umlal     v18.8h, v11.8b, v29.8b
    umlal     v18.8h, v0.8b, v30.8b
    umlal     v18.8h, v2.8b, v31.8b
    sqrshrun  v17.8b, v18.8h, #6
    st1       {v17.8b}, [x1], x3        ////Store dest row7
    b         end_func

loop_2:
    ld1       {v0.8b}, [x0], x2         //// Load row0 ;
    ext       v2.8b, v0.8b , v0.8b , #2
    ld1       {v3.8b}, [x0], x2         //// Load row1;
    ext       v5.8b, v3.8b , v3.8b , #2
    umull     v20.8h, v0.8b, v28.8b
    umlal     v20.8h, v2.8b, v29.8b
    umlal     v20.8h, v3.8b, v30.8b
    umlal     v20.8h, v5.8b, v31.8b
    ld1       {v6.8b}, [x0], x2         //// Load row2
    sqrshrun  v26.8b, v20.8h, #6
    ext       v8.8b, v6.8b , v6.8b , #2
    st1       {v26.s}[0], [x1], x3      ////Store dest row0
    umull     v22.8h, v3.8b, v28.8b
    umlal     v22.8h, v5.8b, v29.8b
    umlal     v22.8h, v6.8b, v30.8b
    umlal     v22.8h, v8.8b, v31.8b
    subs      x12, x6, #2
    sqrshrun  v27.8b, v22.8h, #6
    st1       {v27.s}[0], [x1], x3      ////Store dest row1
    beq       end_func                  //If ht=2

    ld1       {v9.8b}, [x0], x2         //// Load row3;
    ext       v11.8b, v9.8b , v9.8b , #2
    umull     v24.8h, v6.8b, v28.8b
    umlal     v24.8h, v8.8b, v29.8b
    umlal     v24.8h, v9.8b, v30.8b
    umlal     v24.8h, v11.8b, v31.8b
    ld1       {v0.8b}, [x0], x2         //// Load row4 ;
    sqrshrun  v16.8b, v24.8h, #6
    ext       v2.8b, v0.8b , v0.8b , #2
    st1       {v16.s}[0], [x1], x3      ////Store dest row2
    umull     v18.8h, v9.8b, v28.8b
    umlal     v18.8h, v11.8b, v29.8b
    umlal     v18.8h, v0.8b, v30.8b
    umlal     v18.8h, v2.8b, v31.8b
    sqrshrun  v17.8b, v18.8h, #6
    st1       {v17.s}[0], [x1], x3      ////Store dest row3


end_func:
    // LDMFD sp!,{x4-x12,PC}         //Restoring registers from stack
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret


