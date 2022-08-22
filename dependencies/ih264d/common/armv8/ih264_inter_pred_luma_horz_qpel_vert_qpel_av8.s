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
//*  ih264_inter_pred_luma_horz_qpel_vert_qpel_av8.s
//*
//* @brief
//*  Contains function definitions for inter prediction  interpolation.
//*
//* @author
//*  Mohit
//*
//* @par List of Functions:
//*
//*  - ih264_inter_pred_luma_horz_qpel_vert_qpel_av8()
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
//*******************************************************************************
//*
//* @brief
//*   This function implements two six tap filters. It
//*    applies the six tap filter in the horizontal direction on the
//*    predictor values, then applies the same filter in the
//*    vertical direction on the predictor values. It then averages these
//*      two outputs to obtain quarter pel values in horizontal and vertical direction.
//*    The six tap filtering operation is described in sec 8.4.2.2.1 titled
//*    "Luma sample interpolation process"
//*
//* @par Description:
//*    This function is called to obtain pixels lying at the following
//*    location (1/4,1/4) or (3/4,1/4) or (1/4,3/4) or (3/4,3/4).
//*    The function interpolates the predictors first in the horizontal direction
//*    and then in the vertical direction, and then averages these two
//*      values.
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
//* @param[in] pu1_tmp: temporary buffer
//*
//* @param[in] dydx: x and y reference offset for qpel calculations
//*
//* @returns
//*
//* @remarks
//*  None
//*
//*******************************************************************************
//*/;

//void ih264_inter_pred_luma_horz_qpel_vert_qpel(UWORD8 *pu1_src,
//                                UWORD8 *pu1_dst,
//                                WORD32 src_strd,,
//                                WORD32 dst_strd,
//                                WORD32 ht,
//                                WORD32 wd,
//                                    UWORD8* pu1_tmp,
//                                  UWORD32 dydx)

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



    .global ih264_inter_pred_luma_horz_qpel_vert_qpel_av8

ih264_inter_pred_luma_horz_qpel_vert_qpel_av8:

    push_v_regs
    stp       x19, x20, [sp, #-16]!
    sxtw      x2, w2
    sxtw      x3, w3
    sxtw      x4, w4
    sxtw      x5, w5
    mov       w6, w7
    and       x7, x6, #3
    add       x7, x0, x7, lsr #1        //pu1_pred_vert = pu1_src + (x_offset>>1)

    and       x6, x6, #12               //Finds y-offset
    lsr       x6, x6, #3                //dydx>>3
    mul       x6, x2, x6
    add       x6, x0, x6                //pu1_pred_horz = pu1_src + (y_offset>>1)*src_strd
    sub       x7, x7, x2, lsl #1        //pu1_pred_vert-2*src_strd
    sub       x6, x6, #2                //pu1_pred_horz-2
    movi      v30.8b, #20               // Filter coeff 20
    movi      v31.8b, #5                // Filter coeff 5

    subs      x12, x5, #4               //if wd=4 branch to loop_4
    beq       loop_4_start
    subs      x12, x5, #8               //if wd=8 branch to loop_8
    beq       loop_8_start

    ld1       {v0.2s, v1.2s}, [x7], x2  // Vector load from src[0_0]
    ld1       {v2.2s, v3.2s}, [x7], x2  // Vector load from src[1_0]

    ld1       {v4.2s, v5.2s}, [x7], x2  // Vector load from src[2_0]
    ld1       {v6.2s, v7.2s}, [x7], x2  // Vector load from src[3_0]
    ld1       {v8.2s, v9.2s}, [x7], x2  // Vector load from src[4_0]
    add       x11, x6, #8
loop_16:
    ld1       {v10.2s, v11.2s}, [x7], x2 // Vector load from src[5_0]
    ld1       {v18.2s, v19.2s}, [x6], x2 // horz row0, col 0
    uaddl     v24.8h, v0.8b, v10.8b
    umlal     v24.8h, v4.8b, v30.8b
    umlal     v24.8h, v6.8b, v30.8b
    umlsl     v24.8h, v2.8b, v31.8b
    umlsl     v24.8h, v8.8b, v31.8b
    ext       v23.8b, v18.8b , v19.8b , #5
    ext       v20.8b, v18.8b , v19.8b , #2
    ext       v21.8b, v18.8b , v19.8b , #3
    ext       v22.8b, v18.8b , v19.8b , #4
    ext       v19.8b, v18.8b , v19.8b , #1
    sqrshrun  v26.8b, v24.8h, #5
    uaddl     v28.8h, v18.8b, v23.8b
    umlal     v28.8h, v20.8b, v30.8b
    umlal     v28.8h, v21.8b, v30.8b
    umlsl     v28.8h, v19.8b, v31.8b
    umlsl     v28.8h, v22.8b, v31.8b
    ld1       {v18.2s, v19.2s}, [x11], x2 // horz row 0, col 1
    uaddl     v24.8h, v1.8b, v11.8b
    umlal     v24.8h, v5.8b, v30.8b
    umlal     v24.8h, v7.8b, v30.8b
    umlsl     v24.8h, v3.8b, v31.8b
    umlsl     v24.8h, v9.8b, v31.8b
    sqrshrun  v28.8b, v28.8h, #5
    ext       v23.8b, v18.8b , v19.8b , #5
    ext       v20.8b, v18.8b , v19.8b , #2
    ext       v21.8b, v18.8b , v19.8b , #3
    ext       v22.8b, v18.8b , v19.8b , #4
    ext       v19.8b, v18.8b , v19.8b , #1

    sqrshrun  v27.8b, v24.8h, #5
    ld1       {v12.2s, v13.2s}, [x7], x2 // src[6_0]

    uaddl     v24.8h, v18.8b, v23.8b
    umlal     v24.8h, v20.8b, v30.8b
    umlal     v24.8h, v21.8b, v30.8b
    umlsl     v24.8h, v19.8b, v31.8b
    umlsl     v24.8h, v22.8b, v31.8b

    uaddl     v16.8h, v2.8b, v12.8b
    umlal     v16.8h, v6.8b, v30.8b
    umlal     v16.8h, v8.8b, v30.8b
    umlsl     v16.8h, v4.8b, v31.8b
    umlsl     v16.8h, v10.8b, v31.8b

    sqrshrun  v29.8b, v24.8h, #5
    ld1       {v18.2s, v19.2s}, [x6], x2 // horz row 1, col 0

    uaddl     v24.8h, v3.8b, v13.8b
    umlal     v24.8h, v7.8b, v30.8b
    umlal     v24.8h, v9.8b, v30.8b
    umlsl     v24.8h, v5.8b, v31.8b
    umlsl     v24.8h, v11.8b, v31.8b
    urhadd    v28.16b, v28.16b , v26.16b
    urhadd    v29.16b, v29.16b , v27.16b
    sqrshrun  v26.8b, v16.8h, #5
    ext       v23.8b, v18.8b , v19.8b , #5
    ext       v20.8b, v18.8b , v19.8b , #2
    ext       v21.8b, v18.8b , v19.8b , #3
    ext       v22.8b, v18.8b , v19.8b , #4
    st1       {v28.2s, v29.2s}, [x1], x3 // store row 0
    ext       v19.8b, v18.8b , v19.8b , #1

    sqrshrun  v27.8b, v24.8h, #5

    uaddl     v28.8h, v18.8b, v23.8b
    umlal     v28.8h, v20.8b, v30.8b
    umlal     v28.8h, v21.8b, v30.8b
    umlsl     v28.8h, v19.8b, v31.8b
    umlsl     v28.8h, v22.8b, v31.8b

    ld1       {v18.2s, v19.2s}, [x11], x2 // horz row 1, col 1
    ld1       {v14.2s, v15.2s}, [x7], x2 // src[7_0]

    ext       v23.8b, v18.8b , v19.8b , #5
    ext       v20.8b, v18.8b , v19.8b , #2
    ext       v21.8b, v18.8b , v19.8b , #3
    ext       v22.8b, v18.8b , v19.8b , #4
    ext       v19.8b, v18.8b , v19.8b , #1

    sqrshrun  v28.8b, v28.8h, #5
    uaddl     v24.8h, v18.8b, v23.8b
    umlal     v24.8h, v20.8b, v30.8b
    umlal     v24.8h, v21.8b, v30.8b
    umlsl     v24.8h, v19.8b, v31.8b
    umlsl     v24.8h, v22.8b, v31.8b

    ld1       {v18.2s, v19.2s}, [x6], x2 // horz row 2, col 0
    uaddl     v16.8h, v4.8b, v14.8b
    umlal     v16.8h, v8.8b, v30.8b
    umlal     v16.8h, v10.8b, v30.8b
    umlsl     v16.8h, v6.8b, v31.8b
    umlsl     v16.8h, v12.8b, v31.8b

    sqrshrun  v29.8b, v24.8h, #5
    ext       v23.8b, v18.8b , v19.8b , #5
    ext       v20.8b, v18.8b , v19.8b , #2
    ext       v21.8b, v18.8b , v19.8b , #3
    ext       v22.8b, v18.8b , v19.8b , #4
    ext       v19.8b, v18.8b , v19.8b , #1
    urhadd    v28.16b, v28.16b , v26.16b
    urhadd    v29.16b, v29.16b , v27.16b
    sqrshrun  v26.8b, v16.8h, #5

    uaddl     v24.8h, v5.8b, v15.8b
    umlal     v24.8h, v9.8b, v30.8b
    umlal     v24.8h, v11.8b, v30.8b
    umlsl     v24.8h, v7.8b, v31.8b
    umlsl     v24.8h, v13.8b, v31.8b

    st1       {v28.2s, v29.2s}, [x1], x3 // store row 1

    uaddl     v28.8h, v18.8b, v23.8b
    umlal     v28.8h, v20.8b, v30.8b
    umlal     v28.8h, v21.8b, v30.8b
    umlsl     v28.8h, v19.8b, v31.8b
    umlsl     v28.8h, v22.8b, v31.8b

    ld1       {v18.2s, v19.2s}, [x11], x2 // horz row 2, col 1
    sqrshrun  v27.8b, v24.8h, #5
    ext       v23.8b, v18.8b , v19.8b , #5
    ext       v20.8b, v18.8b , v19.8b , #2
    ext       v21.8b, v18.8b , v19.8b , #3
    ext       v22.8b, v18.8b , v19.8b , #4
    ext       v19.8b, v18.8b , v19.8b , #1

    sqrshrun  v28.8b, v28.8h, #5
    ld1       {v16.2s, v17.2s}, [x7], x2 // src[8_0]
    uaddl     v24.8h, v18.8b, v23.8b
    umlal     v24.8h, v20.8b, v30.8b
    umlal     v24.8h, v21.8b, v30.8b
    umlsl     v24.8h, v19.8b, v31.8b
    umlsl     v24.8h, v22.8b, v31.8b

    ld1       {v18.2s, v19.2s}, [x6], x2 // horz row 3, col 0
    uaddl     v0.8h, v6.8b, v16.8b
    umlal     v0.8h, v10.8b, v30.8b
    umlal     v0.8h, v12.8b, v30.8b
    umlsl     v0.8h, v8.8b, v31.8b
    umlsl     v0.8h, v14.8b, v31.8b

    sqrshrun  v29.8b, v24.8h, #5

    ext       v23.8b, v18.8b , v19.8b , #5
    ext       v20.8b, v18.8b , v19.8b , #2
    ext       v21.8b, v18.8b , v19.8b , #3
    urhadd    v28.16b, v28.16b , v26.16b
    urhadd    v29.16b, v29.16b , v27.16b
    ext       v22.8b, v18.8b , v19.8b , #4
    ext       v19.8b, v18.8b , v19.8b , #1
    sqrshrun  v26.8b, v0.8h, #5
    st1       {v28.2s, v29.2s}, [x1], x3 // store row 2

    uaddl     v24.8h, v18.8b, v23.8b
    umlal     v24.8h, v20.8b, v30.8b
    umlal     v24.8h, v21.8b, v30.8b
    umlsl     v24.8h, v19.8b, v31.8b
    umlsl     v24.8h, v22.8b, v31.8b

    ld1       {v18.2s, v19.2s}, [x11], x2 // horz row 3, col 1

    uaddl     v0.8h, v7.8b, v17.8b
    umlal     v0.8h, v11.8b, v30.8b
    umlal     v0.8h, v13.8b, v30.8b
    umlsl     v0.8h, v9.8b, v31.8b
    umlsl     v0.8h, v15.8b, v31.8b

    sqrshrun  v28.8b, v24.8h, #5

    ext       v23.8b, v18.8b , v19.8b , #5
    ext       v20.8b, v18.8b , v19.8b , #2
    ext       v21.8b, v18.8b , v19.8b , #3
    ext       v22.8b, v18.8b , v19.8b , #4
    ext       v19.8b, v18.8b , v19.8b , #1

    sqrshrun  v27.8b, v0.8h, #5

    uaddl     v24.8h, v18.8b, v23.8b
    umlal     v24.8h, v20.8b, v30.8b
    umlal     v24.8h, v21.8b, v30.8b
    umlsl     v24.8h, v19.8b, v31.8b
    umlsl     v24.8h, v22.8b, v31.8b

    mov       v0.16b, v8.16b
    mov       v1.16b, v9.16b

    mov       v2.16b, v10.16b
    mov       v3.16b, v11.16b

    mov       v4.16b, v12.16b
    mov       v5.16b, v13.16b

    mov       v6.16b, v14.16b
    mov       v7.16b, v15.16b

    mov       v8.16b, v16.16b
    mov       v9.16b, v17.16b

    sqrshrun  v29.8b, v24.8h, #5
    urhadd    v28.16b, v28.16b , v26.16b
    urhadd    v29.16b, v29.16b , v27.16b
    st1       {v28.2s, v29.2s}, [x1], x3 // store row 3

    ld1       {v10.2s, v11.2s}, [x7], x2 // Vector load from src[9_0]
    ld1       {v18.2s, v19.2s}, [x6], x2 // horz row4, col 0
    uaddl     v24.8h, v0.8b, v10.8b
    umlal     v24.8h, v4.8b, v30.8b
    umlal     v24.8h, v6.8b, v30.8b
    umlsl     v24.8h, v2.8b, v31.8b
    umlsl     v24.8h, v8.8b, v31.8b
    ext       v23.8b, v18.8b , v19.8b , #5
    ext       v20.8b, v18.8b , v19.8b , #2
    ext       v21.8b, v18.8b , v19.8b , #3
    ext       v22.8b, v18.8b , v19.8b , #4
    ext       v19.8b, v18.8b , v19.8b , #1
    sqrshrun  v26.8b, v24.8h, #5
    uaddl     v28.8h, v18.8b, v23.8b
    umlal     v28.8h, v20.8b, v30.8b
    umlal     v28.8h, v21.8b, v30.8b
    umlsl     v28.8h, v19.8b, v31.8b
    umlsl     v28.8h, v22.8b, v31.8b
    ld1       {v18.2s, v19.2s}, [x11], x2 // horz row 4, col 1
    uaddl     v24.8h, v1.8b, v11.8b
    umlal     v24.8h, v5.8b, v30.8b
    umlal     v24.8h, v7.8b, v30.8b
    umlsl     v24.8h, v3.8b, v31.8b
    umlsl     v24.8h, v9.8b, v31.8b
    sqrshrun  v28.8b, v28.8h, #5
    ext       v23.8b, v18.8b , v19.8b , #5
    ext       v20.8b, v18.8b , v19.8b , #2
    ext       v21.8b, v18.8b , v19.8b , #3
    ext       v22.8b, v18.8b , v19.8b , #4
    ext       v19.8b, v18.8b , v19.8b , #1

    sqrshrun  v27.8b, v24.8h, #5
    ld1       {v12.2s, v13.2s}, [x7], x2 // src[10_0]
    uaddl     v24.8h, v18.8b, v23.8b
    umlal     v24.8h, v20.8b, v30.8b
    umlal     v24.8h, v21.8b, v30.8b
    umlsl     v24.8h, v19.8b, v31.8b
    umlsl     v24.8h, v22.8b, v31.8b
    uaddl     v16.8h, v2.8b, v12.8b
    umlal     v16.8h, v6.8b, v30.8b
    umlal     v16.8h, v8.8b, v30.8b
    umlsl     v16.8h, v4.8b, v31.8b
    umlsl     v16.8h, v10.8b, v31.8b
    sqrshrun  v29.8b, v24.8h, #5
    ld1       {v18.2s, v19.2s}, [x6], x2 // horz row 5, col 0
    uaddl     v24.8h, v3.8b, v13.8b
    umlal     v24.8h, v7.8b, v30.8b
    umlal     v24.8h, v9.8b, v30.8b
    umlsl     v24.8h, v5.8b, v31.8b
    umlsl     v24.8h, v11.8b, v31.8b
    urhadd    v28.16b, v28.16b , v26.16b
    urhadd    v29.16b, v29.16b , v27.16b
    sqrshrun  v26.8b, v16.8h, #5
    ext       v23.8b, v18.8b , v19.8b , #5
    ext       v20.8b, v18.8b , v19.8b , #2
    ext       v21.8b, v18.8b , v19.8b , #3
    ext       v22.8b, v18.8b , v19.8b , #4
    st1       {v28.2s, v29.2s}, [x1], x3 // store row 4
    ext       v19.8b, v18.8b , v19.8b , #1

    sqrshrun  v27.8b, v24.8h, #5

    uaddl     v28.8h, v18.8b, v23.8b
    umlal     v28.8h, v20.8b, v30.8b
    umlal     v28.8h, v21.8b, v30.8b
    umlsl     v28.8h, v19.8b, v31.8b
    umlsl     v28.8h, v22.8b, v31.8b

    ld1       {v18.2s, v19.2s}, [x11], x2 // horz row 5, col 1
    ld1       {v14.2s, v15.2s}, [x7], x2 // src[11_0]

    ext       v23.8b, v18.8b , v19.8b , #5
    ext       v20.8b, v18.8b , v19.8b , #2
    ext       v21.8b, v18.8b , v19.8b , #3
    ext       v22.8b, v18.8b , v19.8b , #4
    ext       v19.8b, v18.8b , v19.8b , #1

    sqrshrun  v28.8b, v28.8h, #5
    uaddl     v24.8h, v18.8b, v23.8b
    umlal     v24.8h, v20.8b, v30.8b
    umlal     v24.8h, v21.8b, v30.8b
    umlsl     v24.8h, v19.8b, v31.8b
    umlsl     v24.8h, v22.8b, v31.8b

    ld1       {v18.2s, v19.2s}, [x6], x2 // horz row 6, col 0
    uaddl     v16.8h, v4.8b, v14.8b
    umlal     v16.8h, v8.8b, v30.8b
    umlal     v16.8h, v10.8b, v30.8b
    umlsl     v16.8h, v6.8b, v31.8b
    umlsl     v16.8h, v12.8b, v31.8b

    sqrshrun  v29.8b, v24.8h, #5
    ext       v23.8b, v18.8b , v19.8b , #5
    ext       v20.8b, v18.8b , v19.8b , #2
    ext       v21.8b, v18.8b , v19.8b , #3
    ext       v22.8b, v18.8b , v19.8b , #4
    ext       v19.8b, v18.8b , v19.8b , #1
    urhadd    v28.16b, v28.16b , v26.16b
    urhadd    v29.16b, v29.16b , v27.16b
    sqrshrun  v26.8b, v16.8h, #5

    uaddl     v24.8h, v5.8b, v15.8b
    umlal     v24.8h, v9.8b, v30.8b
    umlal     v24.8h, v11.8b, v30.8b
    umlsl     v24.8h, v7.8b, v31.8b
    umlsl     v24.8h, v13.8b, v31.8b

    st1       {v28.2s, v29.2s}, [x1], x3 // store row 5

    uaddl     v28.8h, v18.8b, v23.8b
    umlal     v28.8h, v20.8b, v30.8b
    umlal     v28.8h, v21.8b, v30.8b
    umlsl     v28.8h, v19.8b, v31.8b
    umlsl     v28.8h, v22.8b, v31.8b

    ld1       {v18.2s, v19.2s}, [x11], x2 // horz row 6, col 1
    sqrshrun  v27.8b, v24.8h, #5
    ext       v23.8b, v18.8b , v19.8b , #5
    ext       v20.8b, v18.8b , v19.8b , #2
    ext       v21.8b, v18.8b , v19.8b , #3
    ext       v22.8b, v18.8b , v19.8b , #4
    ext       v19.8b, v18.8b , v19.8b , #1

    sqrshrun  v28.8b, v28.8h, #5
    ld1       {v16.2s, v17.2s}, [x7], x2 // src[12_0]
    uaddl     v24.8h, v18.8b, v23.8b
    umlal     v24.8h, v20.8b, v30.8b
    umlal     v24.8h, v21.8b, v30.8b
    umlsl     v24.8h, v19.8b, v31.8b
    umlsl     v24.8h, v22.8b, v31.8b

    ld1       {v18.2s, v19.2s}, [x6], x2 // horz row 7, col 0
    uaddl     v0.8h, v6.8b, v16.8b
    umlal     v0.8h, v10.8b, v30.8b
    umlal     v0.8h, v12.8b, v30.8b
    umlsl     v0.8h, v8.8b, v31.8b
    umlsl     v0.8h, v14.8b, v31.8b

    sqrshrun  v29.8b, v24.8h, #5

    ext       v23.8b, v18.8b , v19.8b , #5
    ext       v20.8b, v18.8b , v19.8b , #2
    ext       v21.8b, v18.8b , v19.8b , #3
    urhadd    v28.16b, v28.16b , v26.16b
    urhadd    v29.16b, v29.16b , v27.16b
    ext       v22.8b, v18.8b , v19.8b , #4
    ext       v19.8b, v18.8b , v19.8b , #1
    sqrshrun  v26.8b, v0.8h, #5
    st1       {v28.2s, v29.2s}, [x1], x3 // store row 6

    uaddl     v24.8h, v18.8b, v23.8b
    umlal     v24.8h, v20.8b, v30.8b
    umlal     v24.8h, v21.8b, v30.8b
    umlsl     v24.8h, v19.8b, v31.8b
    umlsl     v24.8h, v22.8b, v31.8b

    ld1       {v18.2s, v19.2s}, [x11], x2 // horz row 7, col 1

    uaddl     v0.8h, v7.8b, v17.8b
    umlal     v0.8h, v11.8b, v30.8b
    umlal     v0.8h, v13.8b, v30.8b
    umlsl     v0.8h, v9.8b, v31.8b
    umlsl     v0.8h, v15.8b, v31.8b

    sqrshrun  v28.8b, v24.8h, #5

    ext       v23.8b, v18.8b , v19.8b , #5
    ext       v20.8b, v18.8b , v19.8b , #2
    ext       v21.8b, v18.8b , v19.8b , #3
    ext       v22.8b, v18.8b , v19.8b , #4
    ext       v19.8b, v18.8b , v19.8b , #1

    sqrshrun  v27.8b, v0.8h, #5

    uaddl     v24.8h, v18.8b, v23.8b
    umlal     v24.8h, v20.8b, v30.8b
    umlal     v24.8h, v21.8b, v30.8b
    umlsl     v24.8h, v19.8b, v31.8b
    umlsl     v24.8h, v22.8b, v31.8b

    mov       v0.16b, v8.16b
    mov       v1.16b, v9.16b

    mov       v2.16b, v10.16b
    mov       v3.16b, v11.16b

    mov       v4.16b, v12.16b
    mov       v5.16b, v13.16b

    mov       v6.16b, v14.16b
    mov       v7.16b, v15.16b

    mov       v8.16b, v16.16b
    mov       v9.16b, v17.16b

    sqrshrun  v29.8b, v24.8h, #5
    subs      x4, x4, #8
    urhadd    v28.16b, v28.16b , v26.16b
    urhadd    v29.16b, v29.16b , v27.16b
    st1       {v28.2s, v29.2s}, [x1], x3 // store row 7

    beq       end_func                  // stop looping if ht == 8
    b         loop_16


loop_8_start:
    ld1       {v0.2s}, [x7], x2         // Vector load from src[0_0]
    ld1       {v1.2s}, [x7], x2         // Vector load from src[1_0]
    ld1       {v2.2s}, [x7], x2         // Vector load from src[2_0]
    ld1       {v3.2s}, [x7], x2         // Vector load from src[3_0]
    ld1       {v4.2s}, [x7], x2         // Vector load from src[4_0]

loop_8:
    ld1       {v5.2s}, [x7], x2         // Vector load from src[5_0]
    uaddl     v10.8h, v0.8b, v5.8b
    umlal     v10.8h, v2.8b, v30.8b
    umlal     v10.8h, v3.8b, v30.8b
    umlsl     v10.8h, v1.8b, v31.8b
    umlsl     v10.8h, v4.8b, v31.8b
    ld1       {v12.2s, v13.2s}, [x6], x2 //horz row 0
    ext       v17.8b, v12.8b , v13.8b , #5
    ext       v14.8b, v12.8b , v13.8b , #2
    ext       v15.8b, v12.8b , v13.8b , #3
    ext       v16.8b, v12.8b , v13.8b , #4
    ext       v13.8b, v12.8b , v13.8b , #1
    sqrshrun  v26.8b, v10.8h, #5
    ld1       {v6.2s}, [x7], x2         // src[6_0]
    uaddl     v10.8h, v12.8b, v17.8b
    umlal     v10.8h, v14.8b, v30.8b
    umlal     v10.8h, v15.8b, v30.8b
    umlsl     v10.8h, v13.8b, v31.8b
    umlsl     v10.8h, v16.8b, v31.8b
    ld1       {v12.2s, v13.2s}, [x6], x2 // horz row 1
    uaddl     v18.8h, v1.8b, v6.8b
    umlal     v18.8h, v3.8b, v30.8b
    umlal     v18.8h, v4.8b, v30.8b
    umlsl     v18.8h, v2.8b, v31.8b
    umlsl     v18.8h, v5.8b, v31.8b
    sqrshrun  v28.8b, v10.8h, #5
    ext       v17.8b, v12.8b , v13.8b , #5
    ext       v14.8b, v12.8b , v13.8b , #2
    ext       v15.8b, v12.8b , v13.8b , #3
    ext       v16.8b, v12.8b , v13.8b , #4
    ext       v13.8b, v12.8b , v13.8b , #1
    sqrshrun  v27.8b, v18.8h, #5
    ld1       {v7.2s}, [x7], x2         // src[7_0]
    uaddl     v10.8h, v12.8b, v17.8b
    umlal     v10.8h, v14.8b, v30.8b
    umlal     v10.8h, v15.8b, v30.8b
    umlsl     v10.8h, v13.8b, v31.8b
    umlsl     v10.8h, v16.8b, v31.8b
    ld1       {v12.2s, v13.2s}, [x6], x2 // horz row 2
    uaddl     v18.8h, v2.8b, v7.8b
    umlal     v18.8h, v4.8b, v30.8b
    umlal     v18.8h, v5.8b, v30.8b
    umlsl     v18.8h, v3.8b, v31.8b
    umlsl     v18.8h, v6.8b, v31.8b
    sqrshrun  v29.8b, v10.8h, #5
    ext       v17.8b, v12.8b , v13.8b , #5
    ext       v14.8b, v12.8b , v13.8b , #2
    ext       v15.8b, v12.8b , v13.8b , #3
    ext       v16.8b, v12.8b , v13.8b , #4
    ext       v13.8b, v12.8b , v13.8b , #1
    urhadd    v26.16b, v26.16b , v28.16b
    urhadd    v27.16b, v27.16b , v29.16b
    sqrshrun  v28.8b, v18.8h, #5
    ld1       {v8.2s}, [x7], x2         // src[8_0]
    uaddl     v10.8h, v12.8b, v17.8b
    umlal     v10.8h, v14.8b, v30.8b
    umlal     v10.8h, v15.8b, v30.8b
    umlsl     v10.8h, v13.8b, v31.8b
    umlsl     v10.8h, v16.8b, v31.8b
    ld1       {v12.2s, v13.2s}, [x6], x2 // horz row 3
    uaddl     v18.8h, v3.8b, v8.8b
    umlal     v18.8h, v5.8b, v30.8b
    umlal     v18.8h, v6.8b, v30.8b
    umlsl     v18.8h, v4.8b, v31.8b
    umlsl     v18.8h, v7.8b, v31.8b
    sqrshrun  v24.8b, v10.8h, #5
    ext       v17.8b, v12.8b , v13.8b , #5
    ext       v14.8b, v12.8b , v13.8b , #2
    ext       v15.8b, v12.8b , v13.8b , #3
    ext       v16.8b, v12.8b , v13.8b , #4
    ext       v13.8b, v12.8b , v13.8b , #1
    sqrshrun  v29.8b, v18.8h, #5
    uaddl     v10.8h, v12.8b, v17.8b
    umlal     v10.8h, v14.8b, v30.8b
    umlal     v10.8h, v15.8b, v30.8b
    umlsl     v10.8h, v13.8b, v31.8b
    umlsl     v10.8h, v16.8b, v31.8b
    st1       {v26.2s}, [x1], x3

    mov       v0.16b, v4.16b
    mov       v1.16b, v5.16b

    st1       {v27.2s}, [x1], x3

    mov       v2.16b, v6.16b
    mov       v3.16b, v7.16b

    mov       v4.8b, v8.8b

    sqrshrun  v25.8b, v10.8h, #5
    subs      x9, x4, #4
    urhadd    v24.16b, v24.16b , v28.16b
    urhadd    v25.16b, v25.16b , v29.16b
    st1       {v24.2s}, [x1], x3
    st1       {v25.2s}, [x1], x3
    beq       end_func                  // Branch if height==4

    ld1       {v5.2s}, [x7], x2         // Vector load from src[9_0]
    uaddl     v10.8h, v0.8b, v5.8b
    umlal     v10.8h, v2.8b, v30.8b
    umlal     v10.8h, v3.8b, v30.8b
    umlsl     v10.8h, v1.8b, v31.8b
    umlsl     v10.8h, v4.8b, v31.8b
    ld1       {v12.2s, v13.2s}, [x6], x2 //horz row 4
    ext       v17.8b, v12.8b , v13.8b , #5
    ext       v14.8b, v12.8b , v13.8b , #2
    ext       v15.8b, v12.8b , v13.8b , #3
    ext       v16.8b, v12.8b , v13.8b , #4
    ext       v13.8b, v12.8b , v13.8b , #1
    sqrshrun  v26.8b, v10.8h, #5
    ld1       {v6.2s}, [x7], x2         // src[10_0]
    uaddl     v10.8h, v12.8b, v17.8b
    umlal     v10.8h, v14.8b, v30.8b
    umlal     v10.8h, v15.8b, v30.8b
    umlsl     v10.8h, v13.8b, v31.8b
    umlsl     v10.8h, v16.8b, v31.8b
    ld1       {v12.2s, v13.2s}, [x6], x2 // horz row 5
    uaddl     v18.8h, v1.8b, v6.8b
    umlal     v18.8h, v3.8b, v30.8b
    umlal     v18.8h, v4.8b, v30.8b
    umlsl     v18.8h, v2.8b, v31.8b
    umlsl     v18.8h, v5.8b, v31.8b
    sqrshrun  v28.8b, v10.8h, #5
    ext       v17.8b, v12.8b , v13.8b , #5
    ext       v14.8b, v12.8b , v13.8b , #2
    ext       v15.8b, v12.8b , v13.8b , #3
    ext       v16.8b, v12.8b , v13.8b , #4
    ext       v13.8b, v12.8b , v13.8b , #1
    sqrshrun  v27.8b, v18.8h, #5
    ld1       {v7.2s}, [x7], x2         // src[11_0]
    uaddl     v10.8h, v12.8b, v17.8b
    umlal     v10.8h, v14.8b, v30.8b
    umlal     v10.8h, v15.8b, v30.8b
    umlsl     v10.8h, v13.8b, v31.8b
    umlsl     v10.8h, v16.8b, v31.8b
    ld1       {v12.2s, v13.2s}, [x6], x2 // horz row 6
    uaddl     v18.8h, v2.8b, v7.8b
    umlal     v18.8h, v4.8b, v30.8b
    umlal     v18.8h, v5.8b, v30.8b
    umlsl     v18.8h, v3.8b, v31.8b
    umlsl     v18.8h, v6.8b, v31.8b
    sqrshrun  v29.8b, v10.8h, #5
    ext       v17.8b, v12.8b , v13.8b , #5
    ext       v14.8b, v12.8b , v13.8b , #2
    ext       v15.8b, v12.8b , v13.8b , #3
    ext       v16.8b, v12.8b , v13.8b , #4
    ext       v13.8b, v12.8b , v13.8b , #1
    urhadd    v26.16b, v26.16b , v28.16b
    urhadd    v27.16b, v27.16b , v29.16b
    sqrshrun  v28.8b, v18.8h, #5
    ld1       {v8.2s}, [x7], x2         // src[12_0]
    uaddl     v10.8h, v12.8b, v17.8b
    umlal     v10.8h, v14.8b, v30.8b
    umlal     v10.8h, v15.8b, v30.8b
    umlsl     v10.8h, v13.8b, v31.8b
    umlsl     v10.8h, v16.8b, v31.8b
    ld1       {v12.2s, v13.2s}, [x6], x2 // horz row 7
    uaddl     v18.8h, v3.8b, v8.8b
    umlal     v18.8h, v5.8b, v30.8b
    umlal     v18.8h, v6.8b, v30.8b
    umlsl     v18.8h, v4.8b, v31.8b
    umlsl     v18.8h, v7.8b, v31.8b
    sqrshrun  v24.8b, v10.8h, #5
    ext       v17.8b, v12.8b , v13.8b , #5
    ext       v14.8b, v12.8b , v13.8b , #2
    ext       v15.8b, v12.8b , v13.8b , #3
    ext       v16.8b, v12.8b , v13.8b , #4
    ext       v13.8b, v12.8b , v13.8b , #1
    sqrshrun  v29.8b, v18.8h, #5
    uaddl     v10.8h, v12.8b, v17.8b
    umlal     v10.8h, v14.8b, v30.8b
    umlal     v10.8h, v15.8b, v30.8b
    umlsl     v10.8h, v13.8b, v31.8b
    umlsl     v10.8h, v16.8b, v31.8b
    st1       {v26.2s}, [x1], x3

    mov       v0.16b, v4.16b
    mov       v1.16b, v5.16b
    st1       {v27.2s}, [x1], x3

    mov       v2.16b, v6.16b
    mov       v3.16b, v7.16b

    mov       v4.8b, v8.8b
    mov       v5.8b, v9.8b

    sqrshrun  v25.8b, v10.8h, #5
    subs      x4, x4, #8
    urhadd    v24.16b, v24.16b , v28.16b
    urhadd    v25.16b, v25.16b , v29.16b
    st1       {v24.2s}, [x1], x3
    st1       {v25.2s}, [x1], x3
    bgt       loop_8                    //if height =8  loop
    b         end_func

loop_4_start:
    ld1       {v0.s}[0], [x7], x2       // Vector load from src[0_0]
    ld1       {v1.s}[0], [x7], x2       // Vector load from src[1_0]

    ld1       {v2.s}[0], [x7], x2       // Vector load from src[2_0]
    ld1       {v3.s}[0], [x7], x2       // Vector load from src[3_0]
    ld1       {v4.s}[0], [x7], x2       // Vector load from src[4_0]

    ld1       {v5.s}[0], [x7], x2       // Vector load from src[5_0]
    uaddl     v10.8h, v0.8b, v5.8b
    umlal     v10.8h, v2.8b, v30.8b
    umlal     v10.8h, v3.8b, v30.8b
    umlsl     v10.8h, v1.8b, v31.8b
    umlsl     v10.8h, v4.8b, v31.8b
    ld1       {v12.2s, v13.2s}, [x6], x2 //load for horz filter row 0
    ext       v17.8b, v12.8b , v13.8b , #5
    ext       v14.8b, v12.8b , v13.8b , #2
    ext       v15.8b, v12.8b , v13.8b , #3
    ext       v16.8b, v12.8b , v13.8b , #4
    ext       v13.8b, v12.8b , v13.8b , #1
    sqrshrun  v26.8b, v10.8h, #5
    ld1       {v6.s}[0], [x7], x2       // Vector load from src[6_0]
    uaddl     v10.8h, v12.8b, v17.8b
    umlal     v10.8h, v14.8b, v30.8b
    umlal     v10.8h, v15.8b, v30.8b
    umlsl     v10.8h, v13.8b, v31.8b
    umlsl     v10.8h, v16.8b, v31.8b
    ld1       {v12.2s, v13.2s}, [x6], x2 //horz row 1
    uaddl     v18.8h, v1.8b, v6.8b
    umlal     v18.8h, v3.8b, v30.8b
    umlal     v18.8h, v4.8b, v30.8b
    umlsl     v18.8h, v2.8b, v31.8b
    umlsl     v18.8h, v5.8b, v31.8b
    sqrshrun  v28.8b, v10.8h, #5
    ext       v17.8b, v12.8b , v13.8b , #5
    ext       v14.8b, v12.8b , v13.8b , #2
    ext       v15.8b, v12.8b , v13.8b , #3
    ext       v16.8b, v12.8b , v13.8b , #4
    ext       v13.8b, v12.8b , v13.8b , #1
    sqrshrun  v27.8b, v18.8h, #5
    ld1       {v7.s}[0], [x7], x2       // Vector load from src[7_0]
    uaddl     v10.8h, v12.8b, v17.8b
    umlal     v10.8h, v14.8b, v30.8b
    umlal     v10.8h, v15.8b, v30.8b
    umlsl     v10.8h, v13.8b, v31.8b
    umlsl     v10.8h, v16.8b, v31.8b
    ld1       {v12.2s, v13.2s}, [x6], x2 //horz row 2
    uaddl     v18.8h, v2.8b, v7.8b
    umlal     v18.8h, v4.8b, v30.8b
    umlal     v18.8h, v5.8b, v30.8b
    umlsl     v18.8h, v3.8b, v31.8b
    umlsl     v18.8h, v6.8b, v31.8b
    sqrshrun  v29.8b, v10.8h, #5
    ext       v17.8b, v12.8b , v13.8b , #5
    ext       v14.8b, v12.8b , v13.8b , #2
    ext       v15.8b, v12.8b , v13.8b , #3
    ext       v16.8b, v12.8b , v13.8b , #4
    ext       v13.8b, v12.8b , v13.8b , #1
    urhadd    v26.16b, v26.16b , v28.16b
    urhadd    v27.16b, v27.16b , v29.16b
    sqrshrun  v28.8b, v18.8h, #5
    ld1       {v8.s}[0], [x7], x2       // Vector load from src[8_0]
    uaddl     v10.8h, v12.8b, v17.8b
    umlal     v10.8h, v14.8b, v30.8b
    umlal     v10.8h, v15.8b, v30.8b
    umlsl     v10.8h, v13.8b, v31.8b
    umlsl     v10.8h, v16.8b, v31.8b
    ld1       {v12.2s, v13.2s}, [x6], x2 //horz row 3
    uaddl     v18.8h, v3.8b, v8.8b
    umlal     v18.8h, v5.8b, v30.8b
    umlal     v18.8h, v6.8b, v30.8b
    umlsl     v18.8h, v4.8b, v31.8b
    umlsl     v18.8h, v7.8b, v31.8b
    sqrshrun  v24.8b, v10.8h, #5
    ext       v17.8b, v12.8b , v13.8b , #5
    ext       v14.8b, v12.8b , v13.8b , #2
    ext       v15.8b, v12.8b , v13.8b , #3
    ext       v16.8b, v12.8b , v13.8b , #4
    ext       v13.8b, v12.8b , v13.8b , #1
    sqrshrun  v29.8b, v18.8h, #5
    uaddl     v10.8h, v12.8b, v17.8b
    umlal     v10.8h, v14.8b, v30.8b
    umlal     v10.8h, v15.8b, v30.8b
    umlsl     v10.8h, v13.8b, v31.8b
    umlsl     v10.8h, v16.8b, v31.8b
    st1       {v26.s}[0], [x1], x3

    mov       v0.16b, v4.16b
    mov       v1.16b, v5.16b

    st1       {v27.s}[0], [x1], x3

    mov       v2.16b, v6.16b
    mov       v3.16b, v7.16b
    mov       v4.8b, v8.8b

    sqrshrun  v25.8b, v10.8h, #5
    subs      x4, x4, #4
    urhadd    v24.16b, v24.16b , v28.16b
    urhadd    v25.16b, v25.16b , v29.16b
    st1       {v24.s}[0], [x1], x3
    st1       {v25.s}[0], [x1], x3
    beq       end_func                  // Branch if height==4

    ld1       {v5.s}[0], [x7], x2       // Vector load from src[5_0]
    uaddl     v10.8h, v0.8b, v5.8b
    umlal     v10.8h, v2.8b, v30.8b
    umlal     v10.8h, v3.8b, v30.8b
    umlsl     v10.8h, v1.8b, v31.8b
    umlsl     v10.8h, v4.8b, v31.8b
    ld1       {v12.2s, v13.2s}, [x6], x2 //load for horz filter row 4
    ext       v17.8b, v12.8b , v13.8b , #5
    ext       v14.8b, v12.8b , v13.8b , #2
    ext       v15.8b, v12.8b , v13.8b , #3
    ext       v16.8b, v12.8b , v13.8b , #4
    ext       v13.8b, v12.8b , v13.8b , #1
    sqrshrun  v26.8b, v10.8h, #5
    ld1       {v6.s}[0], [x7], x2
    uaddl     v10.8h, v12.8b, v17.8b
    umlal     v10.8h, v14.8b, v30.8b
    umlal     v10.8h, v15.8b, v30.8b
    umlsl     v10.8h, v13.8b, v31.8b
    umlsl     v10.8h, v16.8b, v31.8b
    ld1       {v12.2s, v13.2s}, [x6], x2 //horz row 5
    uaddl     v18.8h, v1.8b, v6.8b
    umlal     v18.8h, v3.8b, v30.8b
    umlal     v18.8h, v4.8b, v30.8b
    umlsl     v18.8h, v2.8b, v31.8b
    umlsl     v18.8h, v5.8b, v31.8b
    sqrshrun  v28.8b, v10.8h, #5
    ext       v17.8b, v12.8b , v13.8b , #5
    ext       v14.8b, v12.8b , v13.8b , #2
    ext       v15.8b, v12.8b , v13.8b , #3
    ext       v16.8b, v12.8b , v13.8b , #4
    ext       v13.8b, v12.8b , v13.8b , #1
    sqrshrun  v27.8b, v18.8h, #5
    ld1       {v7.s}[0], [x7], x2
    uaddl     v10.8h, v12.8b, v17.8b
    umlal     v10.8h, v14.8b, v30.8b
    umlal     v10.8h, v15.8b, v30.8b
    umlsl     v10.8h, v13.8b, v31.8b
    umlsl     v10.8h, v16.8b, v31.8b
    ld1       {v12.2s, v13.2s}, [x6], x2 //horz row 6
    uaddl     v18.8h, v2.8b, v7.8b
    umlal     v18.8h, v4.8b, v30.8b
    umlal     v18.8h, v5.8b, v30.8b
    umlsl     v18.8h, v3.8b, v31.8b
    umlsl     v18.8h, v6.8b, v31.8b
    sqrshrun  v29.8b, v10.8h, #5
    ext       v17.8b, v12.8b , v13.8b , #5
    ext       v14.8b, v12.8b , v13.8b , #2
    ext       v15.8b, v12.8b , v13.8b , #3
    ext       v16.8b, v12.8b , v13.8b , #4
    ext       v13.8b, v12.8b , v13.8b , #1
    urhadd    v26.16b, v26.16b , v28.16b
    urhadd    v27.16b, v27.16b , v29.16b
    sqrshrun  v28.8b, v18.8h, #5
    ld1       {v8.s}[0], [x7], x2
    uaddl     v10.8h, v12.8b, v17.8b
    umlal     v10.8h, v14.8b, v30.8b
    umlal     v10.8h, v15.8b, v30.8b
    umlsl     v10.8h, v13.8b, v31.8b
    umlsl     v10.8h, v16.8b, v31.8b
    ld1       {v12.2s, v13.2s}, [x6], x2 //horz row 7
    uaddl     v18.8h, v3.8b, v8.8b
    umlal     v18.8h, v5.8b, v30.8b
    umlal     v18.8h, v6.8b, v30.8b
    umlsl     v18.8h, v4.8b, v31.8b
    umlsl     v18.8h, v7.8b, v31.8b
    sqrshrun  v24.8b, v10.8h, #5
    ext       v17.8b, v12.8b , v13.8b , #5
    ext       v14.8b, v12.8b , v13.8b , #2
    ext       v15.8b, v12.8b , v13.8b , #3
    ext       v16.8b, v12.8b , v13.8b , #4
    ext       v13.8b, v12.8b , v13.8b , #1
    sqrshrun  v29.8b, v18.8h, #5
    uaddl     v10.8h, v12.8b, v17.8b
    umlal     v10.8h, v14.8b, v30.8b
    umlal     v10.8h, v15.8b, v30.8b
    umlsl     v10.8h, v13.8b, v31.8b
    umlsl     v10.8h, v16.8b, v31.8b
    st1       {v26.s}[0], [x1], x3
    st1       {v27.s}[0], [x1], x3
    sqrshrun  v25.8b, v10.8h, #5
    urhadd    v24.16b, v24.16b , v28.16b
    urhadd    v25.16b, v25.16b , v29.16b
    st1       {v24.s}[0], [x1], x3
    st1       {v25.s}[0], [x1], x3

end_func:
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret



