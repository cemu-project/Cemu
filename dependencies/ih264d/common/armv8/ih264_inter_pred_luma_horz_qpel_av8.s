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
//*  ih264_inter_pred_luma_horz_qpel_av8.s
//*
//* @brief
//*  Contains function definitions for inter prediction horizontal quarter pel interpolation.
//*
//* @author
//*  Mohit
//*
//* @par List of Functions:
//*
//*  - ih264_inter_pred_luma_horz_qpel_av8()
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
//*******************************************************************************
//*
//* @brief
//*     Quarter pel interprediction luma filter for horizontal input
//*
//* @par Description:
//* Applies a 6 tap horizontal filter .The output is  clipped to 8 bits
//* sec 8.4.2.2.1 titled "Luma sample interpolation process"
//*
//* @param[in] pu1_src
//*  UWORD8 pointer to the source
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
//* @param[in] ht
//*  integer height of the array
//*
//* @param[in] wd
//*  integer width of the array
//*
// @param[in] pu1_tmp: temporary buffer: UNUSED in this function
//*
//* @param[in] dydx: x and y reference offset for qpel calculations.
//* @returns
//*
// @remarks
//*  None
//*
//*******************************************************************************
//*/

//void ih264_inter_pred_luma_horz (
//                            UWORD8 *pu1_src,
//                            UWORD8 *pu1_dst,
//                            WORD32 src_strd,
//                            WORD32 dst_strd,
//                            WORD32 ht,
//                            WORD32 wd,
//                              UWORD8* pu1_tmp,
//                             UWORD32 dydx)

//**************Variables Vs Registers*****************************************
//    x0 => *pu1_src
//    x1 => *pu1_dst
//    w2 =>  src_strd
//    w3 =>  dst_strd
//    w4 =>  ht
//    w5 =>  wd
//    w7 =>  dydx

.text
.p2align 2
.include "ih264_neon_macros.s"




    .global ih264_inter_pred_luma_horz_qpel_av8

ih264_inter_pred_luma_horz_qpel_av8:


    push_v_regs
    stp       x19, x20, [sp, #-16]!
    sxtw      x2, w2
    sxtw      x3, w3
    sxtw      x4, w4
    sxtw      x5, w5


    and       x7, x7, #3                //Finds x-offset
    add       x7, x0, x7, lsr #1        //pu1_src + (x_offset>>1)
    sub       x0, x0, #2                //pu1_src-2
    sub       x14, x4, #16
    movi      v0.16b, #5                //filter coeff
    subs      x12, x5, #8               //if wd=8 branch to loop_8
    movi      v1.16b, #20               //filter coeff

    beq       loop_8

    subs      x12, x5, #4               //if wd=4 branch to loop_4
    beq       loop_4

loop_16:                                //when  wd=16
    //// Processing row0 and row1
    ld1       {v2.8b, v3.8b, v4.8b}, [x0], x2 //// Load row0
    add       x14, x14, #1              //for checking loop
    ext       v31.8b, v2.8b , v3.8b , #5
    ld1       {v5.8b, v6.8b, v7.8b}, [x0], x2 //// Load row1
    ext       v30.8b, v3.8b , v4.8b , #5
    uaddl     v8.8h, v31.8b, v2.8b      //// a0 + a5                             (column1,row0)
    ext       v28.8b, v5.8b , v6.8b , #5
    uaddl     v10.8h, v30.8b, v3.8b     //// a0 + a5                             (column2,row0)
    ext       v27.8b, v6.8b , v7.8b , #5
    uaddl     v14.8h, v28.8b, v5.8b     //// a0 + a5                             (column1,row1)
    ext       v31.8b, v2.8b , v3.8b , #2
    uaddl     v16.8h, v27.8b, v6.8b     //// a0 + a5                             (column2,row1)
    ext       v30.8b, v3.8b , v4.8b , #2
    umlal     v8.8h, v31.8b, v1.8b      //// a0 + a5 + 20a2                         (column1,row0)
    ext       v28.8b, v5.8b , v6.8b , #2
    umlal     v10.8h, v30.8b, v1.8b     //// a0 + a5 + 20a2                         (column2,row0)
    ext       v27.8b, v6.8b , v7.8b , #2
    umlal     v14.8h, v28.8b, v1.8b     //// a0 + a5 + 20a2                         (column1,row1)
    ext       v31.8b, v2.8b , v3.8b , #3
    umlal     v16.8h, v27.8b, v1.8b     //// a0 + a5 + 20a2                         (column2,row1)
    ext       v30.8b, v3.8b , v4.8b , #3
    umlal     v8.8h, v31.8b, v1.8b      //// a0 + a5 + 20a2 + 20a3                  (column1,row0)
    ext       v28.8b, v5.8b , v6.8b , #3
    umlal     v10.8h, v30.8b, v1.8b     //// a0 + a5 + 20a2 + 20a3                  (column2,row0)
    ext       v27.8b, v6.8b , v7.8b , #3
    umlal     v14.8h, v28.8b, v1.8b     //// a0 + a5 + 20a2 + 20a3                  (column1,row1)
    ext       v31.8b, v2.8b , v3.8b , #1
    umlal     v16.8h, v27.8b, v1.8b     //// a0 + a5 + 20a2 + 20a3                  (column2,row1)
    ext       v30.8b, v3.8b , v4.8b , #1
    umlsl     v8.8h, v31.8b, v0.8b      //// a0 + a5 + 20a2 + 20a3 - 5a1           (column1,row0)
    ext       v28.8b, v5.8b , v6.8b , #1
    umlsl     v10.8h, v30.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1           (column2,row0)
    ext       v27.8b, v6.8b , v7.8b , #1
    umlsl     v14.8h, v28.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1           (column1,row1)
    ext       v31.8b, v2.8b , v3.8b , #4
    umlsl     v16.8h, v27.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1           (column2,row1)
    ext       v30.8b, v3.8b , v4.8b , #4
    umlsl     v8.8h, v31.8b, v0.8b      //// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4     (column1,row0)
    ext       v28.8b, v5.8b , v6.8b , #4
    umlsl     v10.8h, v30.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4     (column2,row0)
    ext       v27.8b, v6.8b , v7.8b , #4
    umlsl     v14.8h, v28.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4     (column1,row1)
    ld1       {v2.8b, v3.8b, v4.8b}, [x0], x2 //// Load row2
    umlsl     v16.8h, v27.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4     (column2,row1)

    ld1       {v12.2s, v13.2s}, [x7], x2 //Load value for interpolation            (column1,row0)
    sqrshrun  v20.8b, v8.8h, #5         //// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5    (column1,row0)
    ld1       {v5.8b, v6.8b, v7.8b}, [x0], x2 //// Load row3
    sqrshrun  v21.8b, v10.8h, #5        //// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5    (column2,row0)
    ext       v31.8b, v2.8b , v3.8b , #5
    urhadd    v20.16b, v12.16b , v20.16b //Interpolation step for qpel calculation
    urhadd    v21.16b, v13.16b , v21.16b //Interpolation step for qpel calculation

    sqrshrun  v18.8b, v14.8h, #5        //// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5    (column1,row1)
    st1       {v20.8b, v21.8b}, [x1], x3 ////Store dest row0
    ext       v30.8b, v3.8b , v4.8b , #5
    sqrshrun  v19.8b, v16.8h, #5        //// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5    (column2,row1)



//// Processing row2 and row3
    ld1       {v12.2s, v13.2s}, [x7], x2 //Load value for interpolation            (column1,row1)
    ext       v28.8b, v5.8b , v6.8b , #5
    urhadd    v18.16b, v12.16b , v18.16b //Interpolation step for qpel calculation
    urhadd    v19.16b, v13.16b , v19.16b //Interpolation step for qpel calculation

    uaddl     v8.8h, v31.8b, v2.8b      //// a0 + a5                             (column1,row2)
    st1       {v18.8b, v19.8b}, [x1], x3 ////Store dest row1
    uaddl     v10.8h, v30.8b, v3.8b     //// a0 + a5                             (column2,row2)
    ext       v27.8b, v6.8b , v7.8b , #5
    uaddl     v14.8h, v28.8b, v5.8b     //// a0 + a5                             (column1,row3)
    ext       v31.8b, v2.8b , v3.8b , #2
    uaddl     v16.8h, v27.8b, v6.8b     //// a0 + a5                             (column2,row3)
    ext       v30.8b, v3.8b , v4.8b , #2
    umlal     v8.8h, v31.8b, v1.8b      //// a0 + a5 + 20a2                         (column1,row2)
    ext       v27.8b, v6.8b , v7.8b , #2
    umlal     v10.8h, v30.8b, v1.8b     //// a0 + a5 + 20a2                         (column2,row2)
    ext       v28.8b, v5.8b , v6.8b , #2
    umlal     v14.8h, v28.8b, v1.8b     //// a0 + a5 + 20a2                         (column1,row3)
    ext       v31.8b, v2.8b , v3.8b , #3
    umlal     v16.8h, v27.8b, v1.8b     //// a0 + a5 + 20a2                         (column2,row3)
    ext       v30.8b, v3.8b , v4.8b , #3
    umlal     v8.8h, v31.8b, v1.8b      //// a0 + a5 + 20a2 + 20a3                  (column1,row2)
    ext       v28.8b, v5.8b , v6.8b , #3
    umlal     v10.8h, v30.8b, v1.8b     //// a0 + a5 + 20a2 + 20a3                  (column2,row2)
    ext       v27.8b, v6.8b , v7.8b , #3
    umlal     v14.8h, v28.8b, v1.8b     //// a0 + a5 + 20a2 + 20a3                  (column1,row3)
    ext       v31.8b, v2.8b , v3.8b , #1
    umlal     v16.8h, v27.8b, v1.8b     //// a0 + a5 + 20a2 + 20a3                  (column2,row3)
    ext       v30.8b, v3.8b , v4.8b , #1
    umlsl     v8.8h, v31.8b, v0.8b      //// a0 + a5 + 20a2 + 20a3 - 5a1           (column1,row2)
    ext       v28.8b, v5.8b , v6.8b , #1
    umlsl     v10.8h, v30.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1           (column2,row2)
    ext       v27.8b, v6.8b , v7.8b , #1
    umlsl     v14.8h, v28.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1           (column1,row3)
    ext       v31.8b, v2.8b , v3.8b , #4
    umlsl     v16.8h, v27.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1           (column2,row3)
    ext       v30.8b, v3.8b , v4.8b , #4
    umlsl     v8.8h, v31.8b, v0.8b      //// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4     (column1,row2)
    ext       v28.8b, v5.8b , v6.8b , #4
    umlsl     v10.8h, v30.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4     (column2,row2)
    ext       v27.8b, v6.8b , v7.8b , #4
    umlsl     v14.8h, v28.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4     (column1,row3)
    ld1       {v2.8b, v3.8b, v4.8b}, [x0], x2 //// Load row4
    umlsl     v16.8h, v27.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4     (column2,row3)

    ld1       {v12.2s, v13.2s}, [x7], x2 //Load value for interpolation            (column1,row2)
    sqrshrun  v20.8b, v8.8h, #5         //// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5    (column1,row2)
    ld1       {v5.8b, v6.8b, v7.8b}, [x0], x2 //// Load row5
    sqrshrun  v21.8b, v10.8h, #5        //// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5    (column2,row2)
    ext       v31.8b, v2.8b , v3.8b , #5
    urhadd    v20.16b, v12.16b , v20.16b //Interpolation step for qpel calculation
    urhadd    v21.16b, v13.16b , v21.16b //Interpolation step for qpel calculation

    sqrshrun  v18.8b, v14.8h, #5        //// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5    (column1,row3)
    ext       v30.8b, v3.8b , v4.8b , #5
    st1       {v20.8b, v21.8b}, [x1], x3 ////Store dest row2
    sqrshrun  v19.8b, v16.8h, #5        //// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5    (column2,row3)
    ld1       {v12.2s, v13.2s}, [x7], x2 //Load value for interpolation            (column1,row3)

//// Processing row4 and row5
    ext       v28.8b, v5.8b , v6.8b , #5
    urhadd    v18.16b, v12.16b , v18.16b //Interpolation step for qpel calculation
    urhadd    v19.16b, v13.16b , v19.16b //Interpolation step for qpel calculation

    uaddl     v8.8h, v31.8b, v2.8b      //// a0 + a5                             (column1,row4)
    st1       {v18.8b, v19.8b}, [x1], x3 ////Store dest row3
    uaddl     v10.8h, v30.8b, v3.8b     //// a0 + a5                             (column2,row4)
    ext       v27.8b, v6.8b , v7.8b , #5
    uaddl     v14.8h, v28.8b, v5.8b     //// a0 + a5                             (column1,row5)
    ext       v31.8b, v2.8b , v3.8b , #2
    uaddl     v16.8h, v27.8b, v6.8b     //// a0 + a5                             (column2,row5)
    ext       v30.8b, v3.8b , v4.8b , #2
    umlal     v8.8h, v31.8b, v1.8b      //// a0 + a5 + 20a2                         (column1,row4)
    ext       v27.8b, v6.8b , v7.8b , #2
    umlal     v10.8h, v30.8b, v1.8b     //// a0 + a5 + 20a2                         (column2,row4)
    ext       v28.8b, v5.8b , v6.8b , #2
    umlal     v14.8h, v28.8b, v1.8b     //// a0 + a5 + 20a2                         (column1,row5)
    ext       v31.8b, v2.8b , v3.8b , #3
    umlal     v16.8h, v27.8b, v1.8b     //// a0 + a5 + 20a2                         (column2,row5)
    ext       v30.8b, v3.8b , v4.8b , #3
    umlal     v8.8h, v31.8b, v1.8b      //// a0 + a5 + 20a2 + 20a3                  (column1,row4)
    ext       v28.8b, v5.8b , v6.8b , #3
    umlal     v10.8h, v30.8b, v1.8b     //// a0 + a5 + 20a2 + 20a3                  (column2,row4)
    ext       v27.8b, v6.8b , v7.8b , #3
    umlal     v14.8h, v28.8b, v1.8b     //// a0 + a5 + 20a2 + 20a3                  (column1,row5)
    ext       v31.8b, v2.8b , v3.8b , #1
    umlal     v16.8h, v27.8b, v1.8b     //// a0 + a5 + 20a2 + 20a3                  (column2,row5)
    ext       v30.8b, v3.8b , v4.8b , #1
    umlsl     v8.8h, v31.8b, v0.8b      //// a0 + a5 + 20a2 + 20a3 - 5a1           (column1,row4)
    ext       v28.8b, v5.8b , v6.8b , #1
    umlsl     v10.8h, v30.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1           (column2,row4)
    ext       v27.8b, v6.8b , v7.8b , #1
    umlsl     v14.8h, v28.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1           (column1,row4)
    ext       v31.8b, v2.8b , v3.8b , #4
    umlsl     v16.8h, v27.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1           (column2,row5)
    ext       v30.8b, v3.8b , v4.8b , #4
    umlsl     v8.8h, v31.8b, v0.8b      //// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4     (column1,row4)
    ext       v28.8b, v5.8b , v6.8b , #4
    umlsl     v10.8h, v30.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4     (column2,row4)
    ext       v27.8b, v6.8b , v7.8b , #4
    umlsl     v14.8h, v28.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4     (column1,row5)
    ld1       {v2.8b, v3.8b, v4.8b}, [x0], x2 //// Load row6
    umlsl     v16.8h, v27.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4     (column2,row5)
    ld1       {v12.2s, v13.2s}, [x7], x2 //Load value for interpolation            (column1,row4)
    sqrshrun  v20.8b, v8.8h, #5         //// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5    (column1,row4)
    ld1       {v5.8b, v6.8b, v7.8b}, [x0], x2 //// Load row7
    sqrshrun  v21.8b, v10.8h, #5        //// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5    (column2,row4)
    ext       v31.8b, v2.8b , v3.8b , #5
    urhadd    v20.16b, v12.16b , v20.16b //Interpolation step for qpel calculation
    urhadd    v21.16b, v13.16b , v21.16b //Interpolation step for qpel calculation

    sqrshrun  v18.8b, v14.8h, #5        //// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5    (column1,row5)
    st1       {v20.8b, v21.8b}, [x1], x3 ////Store dest row4
    ext       v30.8b, v3.8b , v4.8b , #5
    sqrshrun  v19.8b, v16.8h, #5        //// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5    (column2,row5)
    ld1       {v12.2s, v13.2s}, [x7], x2 //Load value for interpolation            (column1,row5)


    //// Processing row6 and row7

    ext       v28.8b, v5.8b , v6.8b , #5
    urhadd    v18.16b, v12.16b , v18.16b //Interpolation step for qpel calculation
    urhadd    v19.16b, v13.16b , v19.16b //Interpolation step for qpel calculation

    uaddl     v8.8h, v31.8b, v2.8b      //// a0 + a5                             (column1,row6)
    st1       {v18.8b, v19.8b}, [x1], x3 ////Store dest row5
    uaddl     v10.8h, v30.8b, v3.8b     //// a0 + a5                             (column2,row6)
    ext       v27.8b, v6.8b , v7.8b , #5
    uaddl     v14.8h, v28.8b, v5.8b     //// a0 + a5                             (column1,row7)
    ext       v31.8b, v2.8b , v3.8b , #2
    uaddl     v16.8h, v27.8b, v6.8b     //// a0 + a5                             (column2,row7)
    ext       v30.8b, v3.8b , v4.8b , #2
    umlal     v8.8h, v31.8b, v1.8b      //// a0 + a5 + 20a2                         (column1,row6)
    ext       v27.8b, v6.8b , v7.8b , #2
    umlal     v10.8h, v30.8b, v1.8b     //// a0 + a5 + 20a2                         (column2,row6)
    ext       v28.8b, v5.8b , v6.8b , #2
    umlal     v14.8h, v28.8b, v1.8b     //// a0 + a5 + 20a2                         (column1,row7)
    ext       v31.8b, v2.8b , v3.8b , #3
    umlal     v16.8h, v27.8b, v1.8b     //// a0 + a5 + 20a2                         (column2,row7)
    ext       v30.8b, v3.8b , v4.8b , #3
    umlal     v8.8h, v31.8b, v1.8b      //// a0 + a5 + 20a2 + 20a3                  (column1,row6)
    ext       v28.8b, v5.8b , v6.8b , #3
    umlal     v10.8h, v30.8b, v1.8b     //// a0 + a5 + 20a2 + 20a3                  (column2,row6)
    ext       v27.8b, v6.8b , v7.8b , #3
    umlal     v14.8h, v28.8b, v1.8b     //// a0 + a5 + 20a2 + 20a3                  (column1,row7)
    ext       v31.8b, v2.8b , v3.8b , #1
    umlal     v16.8h, v27.8b, v1.8b     //// a0 + a5 + 20a2 + 20a3                  (column2,row7)
    ext       v30.8b, v3.8b , v4.8b , #1
    umlsl     v8.8h, v31.8b, v0.8b      //// a0 + a5 + 20a2 + 20a3 - 5a1           (column1,row6)
    ext       v28.8b, v5.8b , v6.8b , #1
    umlsl     v10.8h, v30.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1           (column2,row6)
    ext       v27.8b, v6.8b , v7.8b , #1
    umlsl     v14.8h, v28.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1           (column1,row6)
    ext       v31.8b, v2.8b , v3.8b , #4
    umlsl     v16.8h, v27.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1           (column2,row7)
    ext       v30.8b, v3.8b , v4.8b , #4
    umlsl     v8.8h, v31.8b, v0.8b      //// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4     (column1,row6)
    ext       v28.8b, v5.8b , v6.8b , #4
    umlsl     v10.8h, v30.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4     (column2,row6)
    ext       v27.8b, v6.8b , v7.8b , #4
    ld1       {v12.2s, v13.2s}, [x7], x2 //Load value for interpolation            (column1,row6)
    sqrshrun  v20.8b, v8.8h, #5         //// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5    (column1,row6)
    umlsl     v14.8h, v28.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4     (column1,row7)
    sqrshrun  v21.8b, v10.8h, #5        //// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5    (column2,row6)
    umlsl     v16.8h, v27.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4     (column2,row7)
    urhadd    v20.16b, v12.16b , v20.16b //Interpolation step for qpel calculation
    urhadd    v21.16b, v13.16b , v21.16b //Interpolation step for qpel calculation

    ld1       {v12.2s, v13.2s}, [x7], x2 //Load value for interpolation            (column1,row7)
    sqrshrun  v18.8b, v14.8h, #5        //// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5    (column1,row7)
    st1       {v20.8b, v21.8b}, [x1], x3 ////Store dest row6
    sqrshrun  v19.8b, v16.8h, #5        //// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5    (column2,row7)
    urhadd    v18.16b, v12.16b , v18.16b //Interpolation step for qpel calculation
    urhadd    v19.16b, v13.16b , v19.16b //Interpolation step for qpel calculation

    subs      x12, x14, #1              // if height==16  - looping
    st1       {v18.8b, v19.8b}, [x1], x3 ////Store dest row7



    beq       loop_16
    b         end_func

loop_8:
//// Processing row0 and row1

    ld1       {v5.8b, v6.8b}, [x0], x2  //// Load row1
    add       x14, x14, #1              //for checking loop
    ext       v28.8b, v5.8b , v6.8b , #5
    ld1       {v2.8b, v3.8b}, [x0], x2  //// Load row0
    ext       v25.8b, v5.8b , v6.8b , #2
    ext       v31.8b, v2.8b , v3.8b , #5
    ext       v24.8b, v5.8b , v6.8b , #3
    ext       v23.8b, v5.8b , v6.8b , #1
    ext       v22.8b, v5.8b , v6.8b , #4
    uaddl     v14.8h, v28.8b, v5.8b     //// a0 + a5                             (column1,row1)
    ext       v29.8b, v2.8b , v3.8b , #3
    umlal     v14.8h, v25.8b, v1.8b     //// a0 + a5 + 20a2                         (column1,row1)
    umlal     v14.8h, v24.8b, v1.8b     //// a0 + a5 + 20a2 + 20a3                  (column1,row1)
    umlsl     v14.8h, v23.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1           (column1,row1)
    umlsl     v14.8h, v22.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4     (column1,row1)
    ext       v30.8b, v2.8b , v3.8b , #2
    uaddl     v8.8h, v31.8b, v2.8b      //// a0 + a5                             (column1,row0)
    ext       v27.8b, v2.8b , v3.8b , #1
    ext       v26.8b, v2.8b , v3.8b , #4
    ld1       {v2.8b, v3.8b}, [x0], x2  //// Load row2
    umlal     v8.8h, v29.8b, v1.8b      //// a0 + a5 + 20a2 + 20a3                  (column1,row0)
    umlal     v8.8h, v30.8b, v1.8b      //// a0 + a5 + 20a2                         (column1,row0)
    umlsl     v8.8h, v27.8b, v0.8b      //// a0 + a5 + 20a2 + 20a3 - 5a1           (column1,row0)
    umlsl     v8.8h, v26.8b, v0.8b      //// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4     (column1,row0)
    ld1       {v5.8b, v6.8b}, [x0], x2  //// Load row3
    sqrshrun  v18.8b, v14.8h, #5        //// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5    (column1,row0)

    //// Processing row2 and row3
    ext       v28.8b, v5.8b , v6.8b , #5
    ext       v25.8b, v5.8b , v6.8b , #2
    ext       v31.8b, v2.8b , v3.8b , #5
    uaddl     v14.8h, v28.8b, v5.8b     //// a0 + a5                             (column1,row3)
    ld1       {v12.2s}, [x7], x2        //Load value for interpolation            (column1,row0)
    ld1       {v13.2s}, [x7], x2        //Load value for interpolation            (column1,row1)
    ext       v24.8b, v5.8b , v6.8b , #3
    ext       v23.8b, v5.8b , v6.8b , #1
    sqrshrun  v19.8b, v8.8h, #5         //// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5    (column1,row1)
    ext       v22.8b, v5.8b , v6.8b , #4
    ext       v29.8b, v2.8b , v3.8b , #3
    umlal     v14.8h, v25.8b, v1.8b     //// a0 + a5 + 20a2                         (column1,row3)
    umlal     v14.8h, v24.8b, v1.8b     //// a0 + a5 + 20a2 + 20a3                  (column1,row3)
    umlsl     v14.8h, v23.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1           (column1,row3)
    umlsl     v14.8h, v22.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4     (column1,row3)
    urhadd    v18.16b, v12.16b , v18.16b //Interpolation step for qpel calculation
    urhadd    v19.16b, v13.16b , v19.16b //Interpolation step for qpel calculation

    st1       {v18.8b}, [x1], x3        ////Store dest row0
    st1       {v19.8b}, [x1], x3        ////Store dest row1
    uaddl     v8.8h, v31.8b, v2.8b      //// a0 + a5                             (column1,row2)
    ext       v30.8b, v2.8b , v3.8b , #2
    ext       v27.8b, v2.8b , v3.8b , #1
    ext       v26.8b, v2.8b , v3.8b , #4
    ld1       {v2.8b, v3.8b}, [x0], x2  //// Load row4
    umlal     v8.8h, v29.8b, v1.8b      //// a0 + a5 + 20a2 + 20a3                  (column1,row2)
    umlal     v8.8h, v30.8b, v1.8b      //// a0 + a5 + 20a2                         (column1,row2)
    umlsl     v8.8h, v27.8b, v0.8b      //// a0 + a5 + 20a2 + 20a3 - 5a1           (column1,row2)
    umlsl     v8.8h, v26.8b, v0.8b      //// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4     (column1,row2)
    ld1       {v5.8b, v6.8b}, [x0], x2  //// Load row5
    subs      x9, x4, #4
    sqrshrun  v19.8b, v14.8h, #5        //// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5    (column1,row3)
    ld1       {v12.2s}, [x7], x2        //Load value for interpolation            (column1,row2)
    ld1       {v13.2s}, [x7], x2        //Load value for interpolation            (column1,row3)
    ext       v28.8b, v5.8b , v6.8b , #5
    ext       v25.8b, v5.8b , v6.8b , #2
    ext       v31.8b, v2.8b , v3.8b , #5
    uaddl     v14.8h, v28.8b, v5.8b     //// a0 + a5                             (column1,row5)
    ext       v24.8b, v5.8b , v6.8b , #3
    sqrshrun  v18.8b, v8.8h, #5         //// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5    (column1,row2)
    ext       v22.8b, v5.8b , v6.8b , #4
    ext       v29.8b, v2.8b , v3.8b , #3
    urhadd    v18.16b, v12.16b , v18.16b //Interpolation step for qpel calculation
    urhadd    v19.16b, v13.16b , v19.16b //Interpolation step for qpel calculation

    st1       {v18.8b}, [x1], x3        ////Store dest row2
    ext       v30.8b, v2.8b , v3.8b , #2
    uaddl     v8.8h, v31.8b, v2.8b      //// a0 + a5                             (column1,row4)
    st1       {v19.8b}, [x1], x3        ////Store dest row3
    beq       end_func                  // Branch if height==4

//// Processing row4 and row5
    ext       v23.8b, v5.8b , v6.8b , #1
    umlal     v14.8h, v25.8b, v1.8b     //// a0 + a5 + 20a2                         (column1,row5)
    umlal     v14.8h, v24.8b, v1.8b     //// a0 + a5 + 20a2 + 20a3                  (column1,row5)
    umlsl     v14.8h, v23.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1           (column1,row5)
    umlsl     v14.8h, v22.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4     (column1,row5)
    ext       v27.8b, v2.8b , v3.8b , #1
    ext       v26.8b, v2.8b , v3.8b , #4
    ld1       {v2.8b, v3.8b}, [x0], x2  //// Load row6
    umlal     v8.8h, v29.8b, v1.8b      //// a0 + a5 + 20a2 + 20a3                  (column1,row4)
    umlal     v8.8h, v30.8b, v1.8b      //// a0 + a5 + 20a2                         (column1,row4)
    umlsl     v8.8h, v27.8b, v0.8b      //// a0 + a5 + 20a2 + 20a3 - 5a1           (column1,row4)
    umlsl     v8.8h, v26.8b, v0.8b      //// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4     (column1,row4)
    sqrshrun  v19.8b, v14.8h, #5        //// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5    (column1,row5)
    ld1       {v5.8b, v6.8b}, [x0], x2  //// Load row7
    ext       v31.8b, v2.8b , v3.8b , #5
    ext       v28.8b, v5.8b , v6.8b , #5
    ld1       {v12.2s}, [x7], x2        //Load value for interpolation            (column1,row4)
    ld1       {v13.2s}, [x7], x2        //Load value for interpolation            (column1,row5)
    ext       v25.8b, v5.8b , v6.8b , #2
    uaddl     v14.8h, v28.8b, v5.8b     //// a0 + a5                             (column1,row7)
    ext       v24.8b, v5.8b , v6.8b , #3
    ext       v22.8b, v5.8b , v6.8b , #4
    sqrshrun  v18.8b, v8.8h, #5         //// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5    (column1,row4)
    ext       v29.8b, v2.8b , v3.8b , #3
    ext       v30.8b, v2.8b , v3.8b , #2
    urhadd    v18.16b, v12.16b , v18.16b //Interpolation step for qpel calculation
    urhadd    v19.16b, v13.16b , v19.16b //Interpolation step for qpel calculation

    st1       {v18.8b}, [x1], x3        ////Store dest row4
    ext       v27.8b, v2.8b , v3.8b , #1
    uaddl     v8.8h, v31.8b, v2.8b      //// a0 + a5                             (column1,row6)
    ext       v26.8b, v2.8b , v3.8b , #4
    umlal     v8.8h, v29.8b, v1.8b      //// a0 + a5 + 20a2 + 20a3                  (column1,row6)
    umlal     v8.8h, v30.8b, v1.8b      //// a0 + a5 + 20a2                         (column1,row6)
    umlsl     v8.8h, v27.8b, v0.8b      //// a0 + a5 + 20a2 + 20a3 - 5a1           (column1,row6)
    umlsl     v8.8h, v26.8b, v0.8b      //// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4     (column1,row6)
    //// Processing row6 and row7
    st1       {v19.8b}, [x1], x3        ////Store dest row5
    ext       v23.8b, v5.8b , v6.8b , #1
    umlal     v14.8h, v25.8b, v1.8b     //// a0 + a5 + 20a2                         (column1,row7)
    umlal     v14.8h, v24.8b, v1.8b     //// a0 + a5 + 20a2 + 20a3                  (column1,row7)
    umlsl     v14.8h, v23.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1           (column1,row7)
    umlsl     v14.8h, v22.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4     (column1,row7)
    ld1       {v12.2s}, [x7], x2        //Load value for interpolation            (column1,row6)
    ld1       {v13.2s}, [x7], x2        //Load value for interpolation            (column1,row7)
    sqrshrun  v18.8b, v8.8h, #5         //// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5    (column1,row6)
    subs      x12, x14, #1
    sqrshrun  v19.8b, v14.8h, #5        //// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5    (column1,row7)
    urhadd    v18.16b, v12.16b , v18.16b //Interpolation step for qpel calculation
    urhadd    v19.16b, v13.16b , v19.16b //Interpolation step for qpel calculation

    st1       {v18.8b}, [x1], x3        ////Store dest row6
    st1       {v19.8b}, [x1], x3        ////Store dest row7

    beq       loop_8                    //looping if height ==16

    b         end_func

loop_4:
    ld1       {v5.8b, v6.8b}, [x0], x2  //// Load row1
    ext       v28.8b, v5.8b , v6.8b , #5
    ld1       {v2.8b, v3.8b}, [x0], x2  //// Load row0
    ext       v25.8b, v5.8b , v6.8b , #2
    ext       v31.8b, v2.8b , v3.8b , #5
    uaddl     v14.8h, v28.8b, v5.8b     //// a0 + a5                             (column1,row1)
    ext       v24.8b, v5.8b , v6.8b , #3
    ext       v23.8b, v5.8b , v6.8b , #1
    ext       v22.8b, v5.8b , v6.8b , #4
    ext       v29.8b, v2.8b , v3.8b , #3
    umlal     v14.8h, v25.8b, v1.8b     //// a0 + a5 + 20a2                         (column1,row1)
    umlal     v14.8h, v24.8b, v1.8b     //// a0 + a5 + 20a2 + 20a3                  (column1,row1)
    umlsl     v14.8h, v23.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1           (column1,row1)
    umlsl     v14.8h, v22.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4     (column1,row1)
    uaddl     v8.8h, v31.8b, v2.8b      //// a0 + a5                             (column1,row0)
    ext       v30.8b, v2.8b , v3.8b , #2
    ld1       {v12.2s}, [x7], x2        //Load value for interpolation            (column1,row0)
    ld1       {v13.2s}, [x7], x2        //Load value for interpolation            (column1,row1)
    ext       v27.8b, v2.8b , v3.8b , #1
    ext       v26.8b, v2.8b , v3.8b , #4
    ld1       {v2.8b, v3.8b}, [x0], x2  //// Load row2
    umlal     v8.8h, v29.8b, v1.8b      //// a0 + a5 + 20a2 + 20a3                  (column1,row0)
    umlal     v8.8h, v30.8b, v1.8b      //// a0 + a5 + 20a2                         (column1,row0)
    umlsl     v8.8h, v27.8b, v0.8b      //// a0 + a5 + 20a2 + 20a3 - 5a1           (column1,row0)
    umlsl     v8.8h, v26.8b, v0.8b      //// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4     (column1,row0)
    ld1       {v5.8b, v6.8b}, [x0], x2  //// Load row3
    ext       v28.8b, v5.8b , v6.8b , #5
    ext       v25.8b, v5.8b , v6.8b , #2
    sqrshrun  v18.8b, v14.8h, #5        //// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5    (column1,row0)
    ext       v31.8b, v2.8b , v3.8b , #5
    ext       v24.8b, v5.8b , v6.8b , #3

    ext       v23.8b, v5.8b , v6.8b , #1
    ext       v22.8b, v5.8b , v6.8b , #4
    ext       v29.8b, v2.8b , v3.8b , #3
    sqrshrun  v19.8b, v8.8h, #5         //// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5    (column1,row1)
    ext       v30.8b, v2.8b , v3.8b , #2
    ext       v27.8b, v2.8b , v3.8b , #1

    //// Processing row2 and row3
    urhadd    v18.16b, v12.16b , v18.16b //Interpolation step for qpel calculation
    urhadd    v19.16b, v13.16b , v19.16b //Interpolation step for qpel calculation

    st1       {v18.s}[0], [x1], x3      ////Store dest row0
    st1       {v19.s}[0], [x1], x3      ////Store dest row1
    uaddl     v14.8h, v28.8b, v5.8b     //// a0 + a5                             (column1,row3)
    ext       v26.8b, v2.8b , v3.8b , #4
    ld1       {v12.2s}, [x7], x2        //Load value for interpolation            (column1,row2)
    ld1       {v13.2s}, [x7], x2        //Load value for interpolation            (column1,row3)

    umlal     v14.8h, v25.8b, v1.8b     //// a0 + a5 + 20a2                         (column1,row3)
    umlal     v14.8h, v24.8b, v1.8b     //// a0 + a5 + 20a2 + 20a3                  (column1,row3)
    umlsl     v14.8h, v23.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1           (column1,row3)
    umlsl     v14.8h, v22.8b, v0.8b     //// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4     (column1,row3)
    uaddl     v8.8h, v31.8b, v2.8b      //// a0 + a5                             (column1,row2)
    umlal     v8.8h, v29.8b, v1.8b      //// a0 + a5 + 20a2 + 20a3                  (column1,row2)
    umlal     v8.8h, v30.8b, v1.8b      //// a0 + a5 + 20a2                         (column1,row2)
    umlsl     v8.8h, v27.8b, v0.8b      //// a0 + a5 + 20a2 + 20a3 - 5a1           (column1,row2)
    umlsl     v8.8h, v26.8b, v0.8b      //// a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4     (column1,row2)
    sqrshrun  v19.8b, v14.8h, #5        //// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5    (column1,row3)
    sqrshrun  v18.8b, v8.8h, #5         //// (a0 + a5 + 20a2 + 20a3 - 5a1 - 5a4 + 16) >> 5    (column1,row2)
    urhadd    v18.16b, v12.16b , v18.16b //Interpolation step for qpel calculation
    urhadd    v19.16b, v13.16b , v19.16b //Interpolation step for qpel calculation

    st1       {v18.s}[0], [x1], x3      ////Store dest row2
    subs      x4, x4, #8                // Loop if height =8
    st1       {v19.s}[0], [x1], x3      ////Store dest row3

    beq       loop_4

end_func:

    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret



