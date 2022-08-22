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
//*  ih264_inter_pred_luma_horz_hpel_vert_qpel_av8.s
//*
//* @brief
//*  Contains function definitions for inter prediction  interpolation.
//*
//* @author
//*  Mohit
//*
//* @par List of Functions:
//*
//*  - ih264_inter_pred_luma_horz_hpel_vert_qpel_av8()
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
//*   This function implements a two stage cascaded six tap filter. It
//*    applies the six tap filter in the horizontal direction on the
//*    predictor values, followed by applying the same filter in the
//*    vertical direction on the output of the first stage. It then averages
//*    the output of the 1st stage and the output of the 2nd stage to obtain
//*    the quarter pel values. The six tap filtering operation is described
//*    in sec 8.4.2.2.1 titled "Luma sample interpolation process".
//*
//* @par Description:
//*     This function is called to obtain pixels lying at the following
//*    location (1/2,1/4) or (1/2,3/4). The function interpolates
//*    the predictors first in the horizontal direction and then in the
//*    vertical direction to output the (1/2,1/2). It then averages
//*      the output of the 2nd stage and (1/2,1/2) value to obtain (1/2,1/4)
//*       or (1/2,3/4) depending on the offset.
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

//void ih264_inter_pred_luma_horz_hpel_vert_qpel(UWORD8 *pu1_src,
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
//    x6 => *pu1_tmp
//    w7 =>  dydx

.text
.p2align 2
.include "ih264_neon_macros.s"



    .global ih264_inter_pred_luma_horz_hpel_vert_qpel_av8

ih264_inter_pred_luma_horz_hpel_vert_qpel_av8:


    // store register values to stack
    push_v_regs
    stp       x19, x20, [sp, #-16]!
    sxtw      x2, w2
    sxtw      x3, w3
    sxtw      x4, w4
    sxtw      x5, w5



    sub       x0, x0, x2, lsl #1        // pu1_src-2*src_strd
    sub       x0, x0, #2                // pu1_src-2

    mov       x9, x6

                                        // by writing to w7 here, we clear the upper half of x7
    lsr       w7, w7, #3                // dydx >> 2 followed by dydx & 0x3 and dydx>>1 to obtain the deciding bit

    add       x7, x7, #2
    mov       x6, #48
    madd      x7, x7, x6, x9

    subs      x12, x5, #4               //if wd=4 branch to loop_4
    beq       loop_4_start

    subs      x12, x5, #8               //if wd=8 branch to loop_8
    beq       loop_8_start

    //when  wd=16
    movi      v22.8h, #20               // Filter coeff 0x14 into Q11
    movi      v24.8h, #5                // Filter coeff 0x5  into Q12
    add       x8, x0, #8
    add       x14, x1, #8
    add       x10, x9, #8
    mov       x12, x4
    add       x11, x7, #8
loop_16_lowhalf_start:
    ld1       {v0.2s, v1.2s}, [x0], x2  // row -2 load for horizontal filter
    ext       v5.8b, v0.8b , v1.8b , #5
    uaddl     v6.8h, v0.8b, v5.8b

    ext       v2.8b, v0.8b , v1.8b , #2
    ext       v3.8b, v0.8b , v1.8b , #3
    uaddl     v8.8h, v2.8b, v3.8b
    ext       v4.8b, v0.8b , v1.8b , #4
    mla       v6.8h, v8.8h , v22.8h
    ext       v1.8b, v0.8b , v1.8b , #1
    uaddl     v8.8h, v1.8b, v4.8b
    ld1       {v0.2s, v1.2s}, [x0], x2  // row -1 load for horizontal filter
    mls       v6.8h, v8.8h , v24.8h
    ext       v5.8b, v0.8b , v1.8b , #5
    uaddl     v8.8h, v0.8b, v5.8b
    ext       v2.8b, v0.8b , v1.8b , #2
    ext       v3.8b, v0.8b , v1.8b , #3
    uaddl     v10.8h, v2.8b, v3.8b

    st1       {v6.4s}, [x9], x6         // store temp buffer 0

    ext       v4.8b, v0.8b , v1.8b , #4
    mla       v8.8h, v10.8h , v22.8h
    ext       v1.8b, v0.8b , v1.8b , #1
    uaddl     v10.8h, v1.8b, v4.8b
    ld1       {v0.2s, v1.2s}, [x0], x2  // row 0 load for horizontal filter
    mls       v8.8h, v10.8h , v24.8h
    ext       v5.8b, v0.8b , v1.8b , #5
    uaddl     v10.8h, v0.8b, v5.8b
    ext       v2.8b, v0.8b , v1.8b , #2
    ext       v3.8b, v0.8b , v1.8b , #3
    uaddl     v12.8h, v2.8b, v3.8b

    st1       {v8.4s}, [x9], x6         // store temp buffer 1

    ext       v4.8b, v0.8b , v1.8b , #4
    mla       v10.8h, v12.8h , v22.8h
    ext       v1.8b, v0.8b , v1.8b , #1
    uaddl     v12.8h, v1.8b, v4.8b
    ld1       {v0.2s, v1.2s}, [x0], x2  // row 1 load for horizontal filter
    mls       v10.8h, v12.8h , v24.8h
    ext       v5.8b, v0.8b , v1.8b , #5
    uaddl     v12.8h, v0.8b, v5.8b
    ext       v2.8b, v0.8b , v1.8b , #2
    ext       v3.8b, v0.8b , v1.8b , #3
    uaddl     v14.8h, v2.8b, v3.8b

    st1       {v10.4s}, [x9], x6        // store temp buffer 2

    ext       v4.8b, v0.8b , v1.8b , #4
    mla       v12.8h, v14.8h , v22.8h
    ext       v1.8b, v0.8b , v1.8b , #1
    uaddl     v14.8h, v1.8b, v4.8b
    ld1       {v0.2s, v1.2s}, [x0], x2  // row 2 load for horizontal filter
    mls       v12.8h, v14.8h , v24.8h
    ext       v5.8b, v0.8b , v1.8b , #5
    uaddl     v14.8h, v0.8b, v5.8b
    ext       v2.8b, v0.8b , v1.8b , #2
    ext       v3.8b, v0.8b , v1.8b , #3
    uaddl     v16.8h, v2.8b, v3.8b

    st1       {v12.4s}, [x9], x6        // store temp buffer 3

    ext       v4.8b, v0.8b , v1.8b , #4
    mla       v14.8h, v16.8h , v22.8h
    ext       v1.8b, v0.8b , v1.8b , #1
    uaddl     v16.8h, v1.8b, v4.8b

    mls       v14.8h, v16.8h , v24.8h
loop_16_lowhalf:

    ld1       {v0.2s, v1.2s}, [x0], x2  // row 3 load for horizontal filter
    ext       v5.8b, v0.8b , v1.8b , #5
    ext       v2.8b, v0.8b , v1.8b , #2
    ext       v3.8b, v0.8b , v1.8b , #3
    uaddl     v16.8h, v0.8b, v5.8b

    st1       {v14.4s}, [x9], x6        // store temp buffer 4

    uaddl     v18.8h, v2.8b, v3.8b
    ext       v4.8b, v0.8b , v1.8b , #4
    mla       v16.8h, v18.8h , v22.8h
    ext       v1.8b, v0.8b , v1.8b , #1
    add       v28.8h, v8.8h , v14.8h
    uaddl     v18.8h, v1.8b, v4.8b
    add       v30.8h, v10.8h , v12.8h
    mls       v16.8h, v18.8h , v24.8h
    ld1       {v0.2s, v1.2s}, [x0], x2  // row 4 load for hoorizontal filter
    ext       v5.8b, v0.8b , v1.8b , #5
    ext       v2.8b, v0.8b , v1.8b , #2
    ext       v3.8b, v0.8b , v1.8b , #3
    uaddl     v20.8h, v0.8b, v5.8b

    st1       {v16.4s}, [x9], x6        // store temp buffer x5

    saddl     v18.4s, v6.4h, v16.4h

    ld1       {v26.4s}, [x7], x6        // load from temp buffer 0

    saddl2    v6.4s, v6.8h, v16.8h

    sqrshrun  v26.8b, v26.8h, #5

    smlal     v18.4s, v30.4h, v22.4h
    smlsl     v18.4s, v28.4h, v24.4h
    smlal2    v6.4s, v30.8h, v22.8h
    smlsl2    v6.4s, v28.8h, v24.8h
    uaddl     v2.8h, v2.8b, v3.8b
    ext       v4.8b, v0.8b , v1.8b , #4
    mla       v20.8h, v2.8h , v22.8h
    sqrshrun  v18.4h, v18.4s, #10
    ext       v1.8b, v0.8b , v1.8b , #1
    sqrshrun  v19.4h, v6.4s, #10
    add       v28.8h, v10.8h , v16.8h
    uaddl     v2.8h, v1.8b, v4.8b
    add       v30.8h, v12.8h , v14.8h
    mls       v20.8h, v2.8h , v24.8h

    uqxtn     v18.8b, v18.8h
    uqxtn     v19.8b, v19.8h
    mov       v18.s[1], v19.s[0]

    ld1       {v0.2s, v1.2s}, [x0], x2  // row 5 load for horizontal filter

    urhadd    v26.8b, v18.8b , v26.8b

    ext       v5.8b, v0.8b , v1.8b , #5
    ext       v2.8b, v0.8b , v1.8b , #2

    st1       {v20.4s}, [x9], x6        // store temp buffer x6

    saddl     v18.4s, v8.4h, v20.4h

    saddl2    v6.4s, v8.8h, v20.8h

    ld1       {v8.4s}, [x7], x6         //load from temp buffer 1


    st1       {v26.2s}, [x1], x3        // store row 0

    smlal     v18.4s, v30.4h, v22.4h
    smlsl     v18.4s, v28.4h, v24.4h
    smlal2    v6.4s, v30.8h, v22.8h
    smlsl2    v6.4s, v28.8h, v24.8h

    sqrshrun  v28.8b, v8.8h, #5
    ext       v3.8b, v0.8b , v1.8b , #3
    uaddl     v8.8h, v0.8b, v5.8b
    uaddl     v2.8h, v2.8b, v3.8b
    sqrshrun  v18.4h, v18.4s, #10
    ext       v4.8b, v0.8b , v1.8b , #4
    sqrshrun  v19.4h, v6.4s, #10
    mla       v8.8h, v2.8h , v22.8h
    ext       v1.8b, v0.8b , v1.8b , #1
    add       v26.8h, v12.8h , v20.8h
    uaddl     v2.8h, v1.8b, v4.8b
    uqxtn     v18.8b, v18.8h
    uqxtn     v19.8b, v19.8h
    mov       v18.s[1], v19.s[0]
    add       v30.8h, v14.8h , v16.8h
    mls       v8.8h, v2.8h , v24.8h
    ld1       {v0.2s, v1.2s}, [x0], x2  // row 6 load for horizontal filter

    urhadd    v28.8b, v28.8b , v18.8b

    ext       v5.8b, v0.8b , v1.8b , #5
    ext       v2.8b, v0.8b , v1.8b , #2
    ext       v3.8b, v0.8b , v1.8b , #3

    st1       {v28.2s}, [x1], x3        // store row 1

    uaddl     v28.8h, v0.8b, v5.8b

    st1       {v8.4s}, [x9], x6         // store temp buffer x7

    saddl     v18.4s, v10.4h, v8.4h
    saddl2    v6.4s, v10.8h, v8.8h

    ld1       {v10.4s}, [x7], x6        // load from temp buffer 2

    smlal     v18.4s, v30.4h, v22.4h
    smlsl     v18.4s, v26.4h, v24.4h

    smlal2    v6.4s, v30.8h, v22.8h
    smlsl2    v6.4s, v26.8h, v24.8h

    sqrshrun  v26.8b, v10.8h, #5

    uaddl     v2.8h, v2.8b, v3.8b
    ext       v4.8b, v0.8b , v1.8b , #4
    mla       v28.8h, v2.8h , v22.8h
    sqrshrun  v18.4h, v18.4s, #10
    ext       v1.8b, v0.8b , v1.8b , #1
    sqrshrun  v19.4h, v6.4s, #10
    add       v10.8h, v14.8h , v8.8h
    uaddl     v2.8h, v1.8b, v4.8b
    add       v30.8h, v16.8h , v20.8h
    mls       v28.8h, v2.8h , v24.8h
    uqxtn     v27.8b, v18.8h
    uqxtn     v19.8b, v19.8h
    mov       v27.s[1], v19.s[0]
    saddl     v18.4s, v12.4h, v28.4h
    saddl2    v6.4s, v12.8h, v28.8h

    urhadd    v26.8b, v26.8b , v27.8b

    smlal     v18.4s, v30.4h, v22.4h
    smlsl     v18.4s, v10.4h, v24.4h
    smlal2    v6.4s, v30.8h, v22.8h
    smlsl2    v6.4s, v10.8h, v24.8h

    st1       {v26.2s}, [x1], x3        // store row 2

    st1       {v28.2s, v29.2s}, [x9]


    sqrshrun  v18.4h, v18.4s, #10

    mov       v10.16b, v20.16b
    mov       v11.16b, v21.16b
    ld1       {v30.4s}, [x7], x6        // load from temp buffer 3

    sqrshrun  v19.4h, v6.4s, #10
    subs      x4, x4, #4

    sqrshrun  v30.8b, v30.8h, #5

    uqxtn     v18.8b, v18.8h
    uqxtn     v19.8b, v19.8h
    mov       v18.s[1], v19.s[0]

    mov       v12.16b, v8.16b
    mov       v13.16b, v9.16b
    mov       v6.16b, v14.16b
    mov       v7.16b, v15.16b

    urhadd    v30.8b, v18.8b , v30.8b

    mov       v8.16b, v16.16b
    mov       v9.16b, v17.16b
    mov       v14.16b, v28.16b
    mov       v15.16b, v29.16b

    st1       {v30.2s}, [x1], x3        // store row 3

    bgt       loop_16_lowhalf           // looping if height =16


loop_16_highhalf_start:
    ld1       {v0.2s, v1.2s}, [x8], x2
    ext       v5.8b, v0.8b , v1.8b , #5
    uaddl     v6.8h, v0.8b, v5.8b
    ext       v2.8b, v0.8b , v1.8b , #2
    ext       v3.8b, v0.8b , v1.8b , #3
    uaddl     v8.8h, v2.8b, v3.8b
    ext       v4.8b, v0.8b , v1.8b , #4
    mla       v6.8h, v8.8h , v22.8h
    ext       v1.8b, v0.8b , v1.8b , #1
    uaddl     v8.8h, v1.8b, v4.8b
    ld1       {v0.2s, v1.2s}, [x8], x2
    mls       v6.8h, v8.8h , v24.8h
    ext       v5.8b, v0.8b , v1.8b , #5
    uaddl     v8.8h, v0.8b, v5.8b
    ext       v2.8b, v0.8b , v1.8b , #2
    ext       v3.8b, v0.8b , v1.8b , #3
    uaddl     v10.8h, v2.8b, v3.8b

    st1       {v6.4s}, [x10], x6

    ext       v4.8b, v0.8b , v1.8b , #4
    mla       v8.8h, v10.8h , v22.8h
    ext       v1.8b, v0.8b , v1.8b , #1
    uaddl     v10.8h, v1.8b, v4.8b
    ld1       {v0.2s, v1.2s}, [x8], x2
    mls       v8.8h, v10.8h , v24.8h
    ext       v5.8b, v0.8b , v1.8b , #5
    uaddl     v10.8h, v0.8b, v5.8b
    ext       v2.8b, v0.8b , v1.8b , #2
    ext       v3.8b, v0.8b , v1.8b , #3
    uaddl     v12.8h, v2.8b, v3.8b

    st1       {v8.4s}, [x10], x6

    ext       v4.8b, v0.8b , v1.8b , #4
    mla       v10.8h, v12.8h , v22.8h
    ext       v1.8b, v0.8b , v1.8b , #1
    uaddl     v12.8h, v1.8b, v4.8b
    ld1       {v0.2s, v1.2s}, [x8], x2
    mls       v10.8h, v12.8h , v24.8h
    ext       v5.8b, v0.8b , v1.8b , #5
    uaddl     v12.8h, v0.8b, v5.8b
    ext       v2.8b, v0.8b , v1.8b , #2
    ext       v3.8b, v0.8b , v1.8b , #3
    uaddl     v14.8h, v2.8b, v3.8b

    st1       {v10.4s}, [x10], x6

    ext       v4.8b, v0.8b , v1.8b , #4
    mla       v12.8h, v14.8h , v22.8h
    ext       v1.8b, v0.8b , v1.8b , #1
    uaddl     v14.8h, v1.8b, v4.8b
    ld1       {v0.2s, v1.2s}, [x8], x2
    mls       v12.8h, v14.8h , v24.8h
    ext       v5.8b, v0.8b , v1.8b , #5
    uaddl     v14.8h, v0.8b, v5.8b
    ext       v2.8b, v0.8b , v1.8b , #2
    ext       v3.8b, v0.8b , v1.8b , #3
    uaddl     v16.8h, v2.8b, v3.8b

    st1       {v12.4s}, [x10], x6

    ext       v4.8b, v0.8b , v1.8b , #4
    mla       v14.8h, v16.8h , v22.8h
    ext       v1.8b, v0.8b , v1.8b , #1
    uaddl     v16.8h, v1.8b, v4.8b

    mls       v14.8h, v16.8h , v24.8h

loop_16_highhalf:

    ld1       {v0.2s, v1.2s}, [x8], x2
    ext       v5.8b, v0.8b , v1.8b , #5
    ext       v2.8b, v0.8b , v1.8b , #2
    ext       v3.8b, v0.8b , v1.8b , #3
    uaddl     v16.8h, v0.8b, v5.8b

    st1       {v14.4s}, [x10], x6

    uaddl     v18.8h, v2.8b, v3.8b
    ext       v4.8b, v0.8b , v1.8b , #4
    mla       v16.8h, v18.8h , v22.8h
    ext       v1.8b, v0.8b , v1.8b , #1
    add       v28.8h, v8.8h , v14.8h
    uaddl     v18.8h, v1.8b, v4.8b
    add       v30.8h, v10.8h , v12.8h
    mls       v16.8h, v18.8h , v24.8h
    ld1       {v0.2s, v1.2s}, [x8], x2
    ext       v5.8b, v0.8b , v1.8b , #5
    ext       v2.8b, v0.8b , v1.8b , #2
    ext       v3.8b, v0.8b , v1.8b , #3
    uaddl     v20.8h, v0.8b, v5.8b

    st1       {v16.4s}, [x10], x6

    saddl     v18.4s, v6.4h, v16.4h

    ld1       {v26.4s}, [x11], x6

    saddl2    v6.4s, v6.8h, v16.8h

    sqrshrun  v26.8b, v26.8h, #5

    smlal     v18.4s, v30.4h, v22.4h
    smlsl     v18.4s, v28.4h, v24.4h
    smlal2    v6.4s, v30.8h, v22.8h
    smlsl2    v6.4s, v28.8h, v24.8h
    uaddl     v2.8h, v2.8b, v3.8b
    ext       v4.8b, v0.8b , v1.8b , #4
    mla       v20.8h, v2.8h , v22.8h
    sqrshrun  v18.4h, v18.4s, #10
    ext       v1.8b, v0.8b , v1.8b , #1
    sqrshrun  v19.4h, v6.4s, #10
    add       v28.8h, v10.8h , v16.8h
    uaddl     v2.8h, v1.8b, v4.8b
    add       v30.8h, v12.8h , v14.8h
    mls       v20.8h, v2.8h , v24.8h
    uqxtn     v18.8b, v18.8h
    uqxtn     v19.8b, v19.8h
    mov       v18.s[1], v19.s[0]
    ld1       {v0.2s, v1.2s}, [x8], x2

    urhadd    v26.8b, v18.8b , v26.8b

    ext       v5.8b, v0.8b , v1.8b , #5
    ext       v2.8b, v0.8b , v1.8b , #2

    st1       {v20.4s}, [x10], x6

    saddl     v18.4s, v8.4h, v20.4h
    saddl2    v6.4s, v8.8h, v20.8h

    ld1       {v8.4s}, [x11], x6


    st1       {v26.2s}, [x14], x3       //store row 0

    smlal     v18.4s, v30.4h, v22.4h
    smlsl     v18.4s, v28.4h, v24.4h
    smlal2    v6.4s, v30.8h, v22.8h
    smlsl2    v6.4s, v28.8h, v24.8h
    sqrshrun  v28.8b, v8.8h, #5
    ext       v3.8b, v0.8b , v1.8b , #3
    uaddl     v8.8h, v0.8b, v5.8b
    uaddl     v2.8h, v2.8b, v3.8b
    sqrshrun  v18.4h, v18.4s, #10
    ext       v4.8b, v0.8b , v1.8b , #4
    sqrshrun  v19.4h, v6.4s, #10
    mla       v8.8h, v2.8h , v22.8h
    ext       v1.8b, v0.8b , v1.8b , #1
    add       v26.8h, v12.8h , v20.8h
    uaddl     v2.8h, v1.8b, v4.8b
    uqxtn     v18.8b, v18.8h
    uqxtn     v19.8b, v19.8h
    mov       v18.s[1], v19.s[0]
    add       v30.8h, v14.8h , v16.8h
    mls       v8.8h, v2.8h , v24.8h
    ld1       {v0.2s, v1.2s}, [x8], x2

    urhadd    v28.8b, v28.8b , v18.8b

    ext       v5.8b, v0.8b , v1.8b , #5
    ext       v2.8b, v0.8b , v1.8b , #2
    ext       v3.8b, v0.8b , v1.8b , #3

    st1       {v28.2s}, [x14], x3       //store row 1

    uaddl     v28.8h, v0.8b, v5.8b

    st1       {v8.4s}, [x10], x6

    saddl     v18.4s, v10.4h, v8.4h
    saddl2    v6.4s, v10.8h, v8.8h

    ld1       {v10.4s}, [x11], x6

    smlal     v18.4s, v30.4h, v22.4h
    smlsl     v18.4s, v26.4h, v24.4h
    smlal2    v6.4s, v30.8h, v22.8h
    smlsl2    v6.4s, v26.8h, v24.8h

    sqrshrun  v26.8b, v10.8h, #5
    uaddl     v2.8h, v2.8b, v3.8b
    ext       v4.8b, v0.8b , v1.8b , #4
    mla       v28.8h, v2.8h , v22.8h
    sqrshrun  v18.4h, v18.4s, #10
    ext       v1.8b, v0.8b , v1.8b , #1
    sqrshrun  v19.4h, v6.4s, #10
    add       v10.8h, v14.8h , v8.8h
    uaddl     v2.8h, v1.8b, v4.8b
    add       v30.8h, v16.8h , v20.8h
    mls       v28.8h, v2.8h , v24.8h
    uqxtn     v27.8b, v18.8h
    uqxtn     v19.8b, v19.8h
    mov       v27.s[1], v19.s[0]


    saddl     v18.4s, v12.4h, v28.4h
    saddl2    v6.4s, v12.8h, v28.8h

    urhadd    v26.8b, v26.8b , v27.8b

    smlal     v18.4s, v30.4h, v22.4h
    smlsl     v18.4s, v10.4h, v24.4h
    smlal2    v6.4s, v30.8h, v22.8h
    smlsl2    v6.4s, v10.8h, v24.8h

    st1       {v26.2s}, [x14], x3       // store row 2

    st1       {v28.4s}, [x10]

    sqrshrun  v18.4h, v18.4s, #10
    mov       v10.16b, v20.16b
    mov       v11.16b, v21.16b
    ld1       {v30.4s}, [x11], x6

    sqrshrun  v19.4h, v6.4s, #10
    subs      x12, x12, #4

    sqrshrun  v30.8b, v30.8h, #5

    uqxtn     v18.8b, v18.8h
    uqxtn     v19.8b, v19.8h
    mov       v18.s[1], v19.s[0]

    mov       v12.16b, v8.16b
    mov       v13.16b, v9.16b
    mov       v6.16b, v14.16b
    mov       v7.16b, v15.16b
    urhadd    v30.8b, v18.8b , v30.8b

    mov       v8.16b, v16.16b
    mov       v9.16b, v17.16b
    mov       v14.16b, v28.16b
    mov       v15.16b, v29.16b
    st1       {v30.2s}, [x14], x3       // store row 3

    bgt       loop_16_highhalf          // looping if height = 8 or 16
    b         end_func

loop_8_start:

    movi      v22.8h, #0x14             // Filter coeff 20 into Q11
    movi      v24.8h, #5                // Filter coeff 5  into Q12
    ld1       {v0.2s, v1.2s}, [x0], x2  // row -2 load for horizontal filter
    ext       v5.8b, v0.8b , v1.8b , #5
    uaddl     v6.8h, v0.8b, v5.8b

    ext       v2.8b, v0.8b , v1.8b , #2
    ext       v3.8b, v0.8b , v1.8b , #3
    uaddl     v8.8h, v2.8b, v3.8b
    ext       v4.8b, v0.8b , v1.8b , #4
    mla       v6.8h, v8.8h , v22.8h
    ext       v1.8b, v0.8b , v1.8b , #1
    uaddl     v8.8h, v1.8b, v4.8b
    ld1       {v0.2s, v1.2s}, [x0], x2  // row -1 load for horizontal filter
    mls       v6.8h, v8.8h , v24.8h
    ext       v5.8b, v0.8b , v1.8b , #5
    uaddl     v8.8h, v0.8b, v5.8b
    ext       v2.8b, v0.8b , v1.8b , #2
    ext       v3.8b, v0.8b , v1.8b , #3
    uaddl     v10.8h, v2.8b, v3.8b

    st1       {v6.4s}, [x9], x6         // store temp buffer 0

    ext       v4.8b, v0.8b , v1.8b , #4
    mla       v8.8h, v10.8h , v22.8h
    ext       v1.8b, v0.8b , v1.8b , #1
    uaddl     v10.8h, v1.8b, v4.8b
    ld1       {v0.2s, v1.2s}, [x0], x2  // row 0 load for horizontal filter
    mls       v8.8h, v10.8h , v24.8h
    ext       v5.8b, v0.8b , v1.8b , #5
    uaddl     v10.8h, v0.8b, v5.8b
    ext       v2.8b, v0.8b , v1.8b , #2
    ext       v3.8b, v0.8b , v1.8b , #3
    uaddl     v12.8h, v2.8b, v3.8b

    st1       {v8.4s}, [x9], x6         // store temp buffer 1

    ext       v4.8b, v0.8b , v1.8b , #4
    mla       v10.8h, v12.8h , v22.8h
    ext       v1.8b, v0.8b , v1.8b , #1
    uaddl     v12.8h, v1.8b, v4.8b
    ld1       {v0.2s, v1.2s}, [x0], x2  // row 1 load for horizontal filter
    mls       v10.8h, v12.8h , v24.8h
    ext       v5.8b, v0.8b , v1.8b , #5
    uaddl     v12.8h, v0.8b, v5.8b
    ext       v2.8b, v0.8b , v1.8b , #2
    ext       v3.8b, v0.8b , v1.8b , #3
    uaddl     v14.8h, v2.8b, v3.8b

    st1       {v10.4s}, [x9], x6        // store temp buffer 2

    ext       v4.8b, v0.8b , v1.8b , #4
    mla       v12.8h, v14.8h , v22.8h
    ext       v1.8b, v0.8b , v1.8b , #1
    uaddl     v14.8h, v1.8b, v4.8b
    ld1       {v0.2s, v1.2s}, [x0], x2  // row 2 load for horizontal filter
    mls       v12.8h, v14.8h , v24.8h
    ext       v5.8b, v0.8b , v1.8b , #5
    uaddl     v14.8h, v0.8b, v5.8b
    ext       v2.8b, v0.8b , v1.8b , #2
    ext       v3.8b, v0.8b , v1.8b , #3
    uaddl     v16.8h, v2.8b, v3.8b

    st1       {v12.4s}, [x9], x6        // store temp buffer 3

    ext       v4.8b, v0.8b , v1.8b , #4
    mla       v14.8h, v16.8h , v22.8h
    ext       v1.8b, v0.8b , v1.8b , #1
    uaddl     v16.8h, v1.8b, v4.8b

    mls       v14.8h, v16.8h , v24.8h
loop_8:

    ld1       {v0.2s, v1.2s}, [x0], x2  // row 3 load for horizontal filter
    ext       v5.8b, v0.8b , v1.8b , #5
    ext       v2.8b, v0.8b , v1.8b , #2
    ext       v3.8b, v0.8b , v1.8b , #3
    uaddl     v16.8h, v0.8b, v5.8b

    st1       {v14.4s}, [x9], x6        // store temp buffer 4

    uaddl     v18.8h, v2.8b, v3.8b
    ext       v4.8b, v0.8b , v1.8b , #4
    mla       v16.8h, v18.8h , v22.8h
    ext       v1.8b, v0.8b , v1.8b , #1
    add       v28.8h, v8.8h , v14.8h
    uaddl     v18.8h, v1.8b, v4.8b
    add       v30.8h, v10.8h , v12.8h
    mls       v16.8h, v18.8h , v24.8h
    ld1       {v0.2s, v1.2s}     , [x0], x2 // row 4 load for hoorizontal filter
    ext       v5.8b, v0.8b , v1.8b , #5
    ext       v2.8b, v0.8b , v1.8b , #2
    ext       v3.8b, v0.8b , v1.8b , #3
    uaddl     v20.8h, v0.8b, v5.8b

    st1       {v16.4s}, [x9], x6        // store temp buffer x5

    saddl     v18.4s, v6.4h, v16.4h

    ld1       {v26.4s}, [x7], x6        // load from temp buffer 0

    saddl2    v6.4s, v6.8h, v16.8h

    sqrshrun  v26.8b, v26.8h, #5

    smlal     v18.4s, v30.4h, v22.4h
    smlsl     v18.4s, v28.4h, v24.4h
    smlal2    v6.4s, v30.8h, v22.8h
    smlsl2    v6.4s, v28.8h, v24.8h
    uaddl     v2.8h, v2.8b, v3.8b
    ext       v4.8b, v0.8b , v1.8b , #4
    mla       v20.8h, v2.8h , v22.8h
    sqrshrun  v18.4h, v18.4s, #10
    ext       v1.8b, v0.8b , v1.8b , #1
    sqrshrun  v19.4h, v6.4s, #10
    add       v28.8h, v10.8h , v16.8h
    uaddl     v2.8h, v1.8b, v4.8b
    add       v30.8h, v12.8h , v14.8h
    mls       v20.8h, v2.8h , v24.8h

    uqxtn     v18.8b, v18.8h
    uqxtn     v19.8b, v19.8h
    mov       v18.s[1], v19.s[0]

    ld1       {v0.2s, v1.2s}, [x0], x2  // row 5 load for horizontal filter

    urhadd    v26.8b, v18.8b , v26.8b

    ext       v5.8b, v0.8b , v1.8b , #5
    ext       v2.8b, v0.8b , v1.8b , #2

    st1       {v20.4s}, [x9], x6        // store temp buffer x6

    saddl     v18.4s, v8.4h, v20.4h

    saddl2    v6.4s, v8.8h, v20.8h

    ld1       {v8.4s}, [x7], x6         //load from temp buffer 1


    st1       {v26.2s}, [x1], x3        // store row 0

    smlal     v18.4s, v30.4h, v22.4h
    smlsl     v18.4s, v28.4h, v24.4h



    smlal2    v6.4s, v30.8h, v22.8h
    smlsl2    v6.4s, v28.8h, v24.8h

    sqrshrun  v28.8b, v8.8h, #5

    ext       v3.8b, v0.8b , v1.8b , #3
    uaddl     v8.8h, v0.8b, v5.8b
    uaddl     v2.8h, v2.8b, v3.8b
    sqrshrun  v18.4h, v18.4s, #10
    ext       v4.8b, v0.8b , v1.8b , #4
    sqrshrun  v19.4h, v6.4s, #10
    mla       v8.8h, v2.8h , v22.8h
    ext       v1.8b, v0.8b , v1.8b , #1
    add       v26.8h, v12.8h , v20.8h
    uaddl     v2.8h, v1.8b, v4.8b


    uqxtn     v18.8b, v18.8h
    uqxtn     v19.8b, v19.8h
    mov       v18.s[1], v19.s[0]

    add       v30.8h, v14.8h , v16.8h
    mls       v8.8h, v2.8h , v24.8h
    ld1       {v0.2s, v1.2s}, [x0], x2  // row 6 load for horizontal filter

    urhadd    v28.8b, v28.8b , v18.8b

    ext       v5.8b, v0.8b , v1.8b , #5
    ext       v2.8b, v0.8b , v1.8b , #2
    ext       v3.8b, v0.8b , v1.8b , #3

    st1       {v28.2s}, [x1], x3        // store row 1

    uaddl     v28.8h, v0.8b, v5.8b

    st1       {v8.4s}, [x9], x6         // store temp buffer x7

    saddl     v18.4s, v10.4h, v8.4h
    saddl2    v6.4s, v10.8h, v8.8h

    ld1       {v10.4s}, [x7], x6        // load from temp buffer 2

    smlal     v18.4s, v30.4h, v22.4h
    smlsl     v18.4s, v26.4h, v24.4h
    smlal2    v6.4s, v30.8h, v22.8h
    smlsl2    v6.4s, v26.8h, v24.8h

    sqrshrun  v26.8b, v10.8h, #5
    uaddl     v2.8h, v2.8b, v3.8b
    ext       v4.8b, v0.8b , v1.8b , #4
    mla       v28.8h, v2.8h , v22.8h
    sqrshrun  v18.4h, v18.4s, #10
    ext       v1.8b, v0.8b , v1.8b , #1
    sqrshrun  v19.4h, v6.4s, #10
    add       v10.8h, v14.8h , v8.8h
    uaddl     v2.8h, v1.8b, v4.8b
    add       v30.8h, v16.8h , v20.8h
    mls       v28.8h, v2.8h , v24.8h

    uqxtn     v27.8b, v18.8h
    uqxtn     v19.8b, v19.8h

    mov       v27.s[1], v19.s[0]

    saddl     v18.4s, v12.4h, v28.4h
    saddl2    v6.4s, v12.8h, v28.8h

    urhadd    v26.8b, v26.8b , v27.8b

    smlal     v18.4s, v30.4h, v22.4h
    smlsl     v18.4s, v10.4h, v24.4h
    smlal2    v6.4s, v30.8h, v22.8h
    smlsl2    v6.4s, v10.8h, v24.8h

    st1       {v26.2s}, [x1], x3        // store row 2

    st1       {v28.2s, v29.2s}, [x9]


    sqrshrun  v18.4h, v18.4s, #10
    mov       v10.16b, v20.16b
    mov       v11.16b, v21.16b
    ld1       {v30.4s}, [x7], x6        // load from temp buffer 3

    sqrshrun  v19.4h, v6.4s, #10
    subs      x4, x4, #4

    sqrshrun  v30.8b, v30.8h, #5


    uqxtn     v18.8b, v18.8h
    uqxtn     v19.8b, v19.8h
    mov       v18.s[1], v19.s[0]


    mov       v12.16b, v8.16b
    mov       v13.16b, v9.16b
    mov       v6.16b, v14.16b
    mov       v7.16b, v15.16b

    urhadd    v30.8b, v18.8b , v30.8b
    mov       v8.16b, v16.16b
    mov       v9.16b, v17.16b
    mov       v14.16b, v28.16b
    mov       v15.16b, v29.16b
    st1       {v30.2s}, [x1], x3        // store row 3

    bgt       loop_8                    //if height =8 or 16  loop
    b         end_func

loop_4_start:
    movi      v22.8h, #20               // Filter coeff 20 into D22
    movi      v23.8h, #5                // Filter coeff 5  into D23

    ld1       {v0.2s, v1.2s}, [x0], x2  //row -2 load
    ext       v5.8b, v0.8b , v1.8b , #5
    uaddl     v6.8h, v0.8b, v5.8b
    ext       v2.8b, v0.8b , v1.8b , #2
    ext       v3.8b, v0.8b , v1.8b , #3
    uaddl     v8.8h, v2.8b, v3.8b
    ext       v4.8b, v0.8b , v1.8b , #4
    mla       v6.4h, v8.4h , v22.4h
    ext       v1.8b, v0.8b , v1.8b , #1
    uaddl     v8.8h, v1.8b, v4.8b
    ld1       {v0.2s, v1.2s}, [x0], x2  // row -1 load
    mls       v6.4h, v8.4h , v23.4h
    ext       v5.8b, v0.8b , v1.8b , #5
    uaddl     v8.8h, v0.8b, v5.8b
    ext       v2.8b, v0.8b , v1.8b , #2
    ext       v3.8b, v0.8b , v1.8b , #3
    uaddl     v10.8h, v2.8b, v3.8b

    st1       {v6.2s}, [x9], x6         // store temp buffer 0

    ext       v4.8b, v0.8b , v1.8b , #4
    mla       v8.4h, v10.4h , v22.4h
    ext       v1.8b, v0.8b , v1.8b , #1
    uaddl     v10.8h, v1.8b, v4.8b
    ld1       {v0.2s, v1.2s}, [x0], x2  // row 0 load
    mls       v8.4h, v10.4h , v23.4h
    ext       v5.8b, v0.8b , v1.8b , #5
    uaddl     v10.8h, v0.8b, v5.8b
    ext       v2.8b, v0.8b , v1.8b , #2
    ext       v3.8b, v0.8b , v1.8b , #3
    uaddl     v12.8h, v2.8b, v3.8b

    st1       {v8.2s}, [x9], x6         // store temp buffer 1

    ext       v4.8b, v0.8b , v1.8b , #4
    mla       v10.4h, v12.4h , v22.4h
    ext       v1.8b, v0.8b , v1.8b , #1
    uaddl     v12.8h, v1.8b, v4.8b
    ld1       {v0.2s, v1.2s}, [x0], x2  // row 1 load
    mls       v10.4h, v12.4h , v23.4h
    ext       v5.8b, v0.8b , v1.8b , #5
    uaddl     v12.8h, v0.8b, v5.8b
    ext       v2.8b, v0.8b , v1.8b , #2
    ext       v3.8b, v0.8b , v1.8b , #3
    uaddl     v14.8h, v2.8b, v3.8b

    st1       {v10.2s}, [x9], x6        // store temp buffer 2

    ext       v4.8b, v0.8b , v1.8b , #4
    mla       v12.4h, v14.4h , v22.4h
    ext       v1.8b, v0.8b , v1.8b , #1
    uaddl     v14.8h, v1.8b, v4.8b
    ld1       {v0.2s, v1.2s}, [x0], x2  // row 2 load
    mls       v12.4h, v14.4h , v23.4h
    ext       v5.8b, v0.8b , v1.8b , #5
    uaddl     v14.8h, v0.8b, v5.8b
    ext       v2.8b, v0.8b , v1.8b , #2
    ext       v3.8b, v0.8b , v1.8b , #3
    uaddl     v16.8h, v2.8b, v3.8b
    ext       v4.8b, v0.8b , v1.8b , #4
    mla       v14.4h, v16.4h , v22.4h
    ext       v1.8b, v0.8b , v1.8b , #1
    uaddl     v16.8h, v1.8b, v4.8b

    st1       {v12.2s}, [x9], x6        // store temp buffer 3

    mls       v14.4h, v16.4h , v23.4h

loop_4:

    ld1       {v0.2s, v1.2s}, [x0], x2  // row 3 load
    ext       v5.8b, v0.8b , v1.8b , #5
    uaddl     v16.8h, v0.8b, v5.8b
    ext       v2.8b, v0.8b , v1.8b , #2
    ext       v3.8b, v0.8b , v1.8b , #3
    uaddl     v18.8h, v2.8b, v3.8b
    st1       {v14.2s}, [x9], x6        // store temp buffer 4
    ext       v4.8b, v0.8b , v1.8b , #4
    mla       v16.4h, v18.4h , v22.4h
    ext       v1.8b, v0.8b , v1.8b , #1
    uaddl     v18.8h, v1.8b, v4.8b
    add       v2.4h, v10.4h , v12.4h
    mls       v16.4h, v18.4h , v23.4h
    add       v3.4h, v8.4h , v14.4h
    ld1       {v18.2s, v19.2s}, [x0], x2 // row 4 load
    ext       v25.8b, v18.8b , v19.8b , #5
    uaddl     v26.8h, v18.8b, v25.8b
    ext       v20.8b, v18.8b , v19.8b , #2

    st1       {v16.2s}, [x9], x6        // store temp buffer 5

    saddl     v0.4s, v6.4h, v16.4h
    smlal     v0.4s, v2.4h, v22.4h
    ext       v21.8b, v18.8b , v19.8b , #3
    uaddl     v28.8h, v20.8b, v21.8b
    ext       v24.8b, v18.8b , v19.8b , #4
    smlsl     v0.4s, v3.4h, v23.4h
    mla       v26.4h, v28.4h , v22.4h
    ext       v19.8b, v18.8b , v19.8b , #1
    uaddl     v28.8h, v19.8b, v24.8b
    add       v2.4h, v12.4h , v14.4h
    mls       v26.4h, v28.4h , v23.4h
    sqrshrun  v0.4h, v0.4s, #0xa
    add       v3.4h, v10.4h , v16.4h
    ld1       {v18.2s, v19.2s}, [x0], x2 // row 5 load
    ext       v25.8b, v18.8b , v19.8b , #5
    uqxtn     v11.8b, v0.8h
    uaddl     v28.8h, v18.8b, v25.8b

    st1       {v26.2s}, [x9], x6        // store temp buffer 6

    //Q3 available here
    ld1       {v6.2s}, [x7], x6         // load from temp buffer 0
    ld1       {v7.2s}, [x7], x6         // load from temp buffer 1

    sqrshrun  v9.8b, v6.8h, #5
    sqrshrun  v7.8b, v7.8h, #5
    mov       v9.s[1], v7.s[0]

    ext       v20.8b, v18.8b , v19.8b , #2

    saddl     v0.4s, v8.4h, v26.4h
    smlal     v0.4s, v2.4h, v22.4h
    ext       v21.8b, v18.8b , v19.8b , #3
    uaddl     v6.8h, v20.8b, v21.8b
    ext       v24.8b, v18.8b , v19.8b , #4
    smlsl     v0.4s, v3.4h, v23.4h
    mla       v28.4h, v6.4h , v22.4h
    ext       v19.8b, v18.8b , v19.8b , #1
    uaddl     v6.8h, v19.8b, v24.8b
    add       v2.4h, v14.4h , v16.4h
    mls       v28.4h, v6.4h , v23.4h
    sqrshrun  v0.4h, v0.4s, #0xa
    add       v3.4h, v12.4h , v26.4h
    ld1       {v18.2s, v19.2s}, [x0], x2 // row 6 load
    ext       v25.8b, v18.8b , v19.8b , #5
    uqxtn     v13.8b, v0.8h

    trn1      v11.2s, v11.2s, v13.2s
    trn2      v13.2s, v11.2s, v13.2s
    saddl     v0.4s, v10.4h, v28.4h
    urhadd    v9.8b, v9.8b , v11.8b

    st1       {v28.2s}, [x9], x6        // store temp buffer 7

    smlal     v0.4s, v2.4h, v22.4h
    uaddl     v30.8h, v18.8b, v25.8b

    st1       {v9.s}[0], [x1], x3       // store row 0

    ext       v20.8b, v18.8b , v19.8b , #2

    st1       {v9.s}[1], [x1], x3       // store row 1

    ext       v21.8b, v18.8b , v19.8b , #3
    smlsl     v0.4s, v3.4h, v23.4h
    uaddl     v8.8h, v20.8b, v21.8b
    ext       v24.8b, v18.8b , v19.8b , #4
    mla       v30.4h, v8.4h , v22.4h
    ext       v19.8b, v18.8b , v19.8b , #1
    uaddl     v8.8h, v19.8b, v24.8b
    sqrshrun  v0.4h, v0.4s, #0xa
    add       v2.4h, v16.4h , v26.4h
    mls       v30.4h, v8.4h , v23.4h
    uqxtn     v4.8b, v0.8h

    add       v3.4h, v14.4h , v28.4h


    saddl     v0.4s, v12.4h, v30.4h

    st1       {v30.2s}, [x9]

    smlal     v0.4s, v2.4h, v22.4h

    ld1       {v8.2s}, [x7], x6         // load from temp buffer 2
    ld1       {v9.2s}, [x7], x6         // load from temp buffer 3
    smlsl     v0.4s, v3.4h, v23.4h
    subs      x4, x4, #4

    sqrshrun  v10.8b, v8.8h, #5
    sqrshrun  v9.8b, v9.8h, #5
    mov       v10.s[1], v9.s[0]

    mov       v12.8b, v28.8b

    sqrshrun  v0.4h, v0.4s, #0xa
    mov       v6.8b, v14.8b
    mov       v8.8b, v16.8b

    uqxtn     v5.8b, v0.8h

    trn1      v4.2s, v4.2s, v5.2s
    trn2      v5.2s, v4.2s, v5.2s
    urhadd    v4.8b, v4.8b , v10.8b
    mov       v10.8b, v26.8b
    mov       v14.8b, v30.8b

    st1       {v4.s}[0], [x1], x3       // store row 2
    st1       {v4.s}[1], [x1], x3       // store row 3

    bgt       loop_4

end_func:
    //Restoring registers from stack
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret



