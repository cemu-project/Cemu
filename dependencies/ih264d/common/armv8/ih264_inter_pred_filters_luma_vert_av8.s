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
//*  ih264_inter_pred_luma_vert_av8.s
//*
//* @brief
//*  Contains function definitions for inter prediction  interpolation.
//*
//* @author
//*  Ittiam
//*
//* @par List of Functions:
//*
//*  - ih264_inter_pred_luma_vert_av8()
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
// *******************************************************************************
// *
// * @brief
// *    Interprediction luma filter for vertical input
// *
// * @par Description:
// *   Applies a 6 tap vertcal filter.The output is  clipped to 8 bits
// *    sec 8.4.2.2.1 titled "Luma sample interpolation process"
// *
// * @param[in] pu1_src
// *  UWORD8 pointer to the source
// *
// * @param[out] pu1_dst
// *  UWORD8 pointer to the destination
// *
// * @param[in] src_strd
// *  integer source stride
// *
// * @param[in] dst_strd
// *  integer destination stride
// *
// * @param[in] ht
// *  integer height of the array
// *
// * @param[in] wd
// *  integer width of the array
// *
// * @returns
// *
// * @remarks
// *  None
// *
// *******************************************************************************

//void ih264_inter_pred_luma_vert (
//                            UWORD8 *pu1_src,
//                            UWORD8 *pu1_dst,
//                            WORD32 src_strd,
//                            WORD32 dst_strd,
//                            WORD32 ht,
//                            WORD32 wd   )

//**************Variables Vs Registers*****************************************
//    x0 => *pu1_src
//    x1 => *pu1_dst
//    w2 =>  src_strd
//    w3 =>  dst_strd
//    w4 =>  ht
//    w5 =>  wd

.text
.p2align 2
.include "ih264_neon_macros.s"




    .global ih264_inter_pred_luma_vert_av8

ih264_inter_pred_luma_vert_av8:

    // STMFD sp!, {x4-x12, x14}          //store register values to stack
    push_v_regs
    stp       x19, x20, [sp, #-16]!
    sxtw      x2, w2
    sxtw      x3, w3
    sxtw      x4, w4
    sxtw      x5, w5

    sub       x0, x0, x2, lsl #1        //pu1_src-2*src_strd

    sub       x14, x4, #16
    movi      v22.8h, #20               // Filter coeff 0x14 into Q11

    subs      x12, x5, #8               //if wd=8 branch to loop_8
    movi      v24.8h, #5                // Filter coeff 0x4  into Q12
    beq       loop_8_start

    subs      x12, x5, #4               //if wd=4 branch to loop_4
    beq       loop_4_start


    ld1       {v0.2s, v1.2s}, [x0], x2  // Vector load from src[0_0]
    ld1       {v2.2s, v3.2s}, [x0], x2  // Vector load from src[1_0]
    ld1       {v4.2s, v5.2s}, [x0], x2  // Vector load from src[2_0]
    ld1       {v6.2s, v7.2s}, [x0], x2  // Vector load from src[3_0]
    add       x14, x14, #1              //for checking loop
    ld1       {v8.2s, v9.2s}, [x0], x2  // Vector load from src[4_0]
    uaddl     v12.8h, v4.8b, v6.8b      // temp1 = src[2_0] + src[3_0]
    ld1       {v10.2s, v11.2s}, [x0], x2 // Vector load from src[5_0]

loop_16:                                //when  wd=16

    uaddl     v14.8h, v0.8b, v10.8b     // temp = src[0_0] + src[5_0]
    uaddl     v16.8h, v2.8b, v8.8b      // temp2 = src[1_0] + src[4_0]
    mla       v14.8h, v12.8h, v22.8h    // temp += temp1 * 20
    uaddl     v20.8h, v1.8b, v11.8b     // temp4 = src[0_8] + src[5_8]
    uaddl     v18.8h, v5.8b, v7.8b      // temp3 = src[2_8] + src[3_8]
    mla       v20.8h, v18.8h , v22.8h   // temp4 += temp3 * 20
    ld1       {v0.2s, v1.2s}, [x0], x2
    uaddl     v26.8h, v3.8b, v9.8b      // temp5 = src[1_8] + src[4_8]
    uaddl     v12.8h, v6.8b, v8.8b
    mls       v14.8h, v16.8h , v24.8h   // temp -= temp2 * 5
    uaddl     v16.8h, v2.8b, v0.8b
    uaddl     v18.8h, v4.8b, v10.8b
    mla       v16.8h, v12.8h , v22.8h
    mls       v20.8h, v26.8h , v24.8h   // temp4 -= temp5 * 5
    uaddl     v26.8h, v5.8b, v11.8b
    uaddl     v12.8h, v7.8b, v9.8b
    sqrshrun  v30.8b, v14.8h, #5        // dst[0_0] = CLIP_U8((temp +16) >> 5)
    uaddl     v14.8h, v3.8b, v1.8b
    ld1       {v2.2s, v3.2s}, [x0], x2
    mla       v14.8h, v12.8h , v22.8h
    mls       v16.8h, v18.8h , v24.8h
    sqrshrun  v31.8b, v20.8h, #5        // dst[0_8] = CLIP_U8((temp4 +16) >> 5)
    uaddl     v18.8h, v4.8b, v2.8b
    uaddl     v12.8h, v8.8b, v10.8b

    st1       {v30.2s, v31.2s}, [x1], x3 // Vector store to dst[0_0]
    mla       v18.8h, v12.8h , v22.8h
    uaddl     v20.8h, v6.8b, v0.8b
    mls       v14.8h, v26.8h , v24.8h
    sqrshrun  v30.8b, v16.8h, #5
    uaddl     v12.8h, v9.8b, v11.8b
    uaddl     v16.8h, v5.8b, v3.8b
    uaddl     v26.8h, v7.8b, v1.8b
    mla       v16.8h, v12.8h , v22.8h
    mls       v18.8h, v20.8h , v24.8h
    ld1       {v4.2s, v5.2s}, [x0], x2

    sqrshrun  v31.8b, v14.8h, #5
    uaddl     v12.8h, v10.8b, v0.8b
    uaddl     v14.8h, v6.8b, v4.8b
    uaddl     v20.8h, v8.8b, v2.8b
    mla       v14.8h, v12.8h , v22.8h
    mls       v16.8h, v26.8h , v24.8h
    st1       {v30.2s, v31.2s}, [x1], x3 //store row 1
    sqrshrun  v30.8b, v18.8h, #5
    uaddl     v18.8h, v7.8b, v5.8b
    uaddl     v12.8h, v11.8b, v1.8b
    mla       v18.8h, v12.8h , v22.8h
    uaddl     v26.8h, v9.8b, v3.8b
    mls       v14.8h, v20.8h , v24.8h
    ld1       {v6.2s, v7.2s}, [x0], x2
    sqrshrun  v31.8b, v16.8h, #5
    mls       v18.8h, v26.8h , v24.8h
    uaddl     v12.8h, v0.8b, v2.8b      // temp1 = src[2_0] + src[3_0]
    st1       {v30.2s, v31.2s}, [x1], x3 //store row 2
    uaddl     v16.8h, v10.8b, v4.8b     // temp2 = src[1_0] + src[4_0]
    uaddl     v20.8h, v9.8b, v7.8b      // temp4 = src[0_8] + src[5_8]
    sqrshrun  v30.8b, v14.8h, #5
    uaddl     v26.8h, v5.8b, v11.8b     // temp5 = src[1_8] + src[4_8]
    uaddl     v14.8h, v8.8b, v6.8b      // temp = src[0_0] + src[5_0]
    sqrshrun  v31.8b, v18.8h, #5
    mla       v14.8h, v12.8h , v22.8h   // temp += temp1 * 20
    uaddl     v18.8h, v1.8b, v3.8b      // temp3 = src[2_8] + src[3_8]
    st1       {v30.2s, v31.2s}, [x1], x3 //store row 3
    // 4 rows processed
    mla       v20.8h, v18.8h , v22.8h   // temp4 += temp3 * 20
    ld1       {v8.2s, v9.2s}, [x0], x2
    uaddl     v12.8h, v2.8b, v4.8b
    uaddl     v18.8h, v3.8b, v5.8b
    mls       v14.8h, v16.8h , v24.8h   // temp -= temp2 * 5
    uaddl     v28.8h, v9.8b, v11.8b
    uaddl     v16.8h, v6.8b, v0.8b
    mla       v28.8h, v18.8h , v22.8h   // temp4 += temp3 * 20
    mls       v20.8h, v26.8h , v24.8h   // temp4 -= temp5 * 5
    uaddl     v26.8h, v1.8b, v7.8b
    uaddl     v18.8h, v5.8b, v7.8b
    sqrshrun  v30.8b, v14.8h, #5        // dst[0_0] = CLIP_U8((temp +16) >> 5)
    uaddl     v14.8h, v8.8b, v10.8b

    sqrshrun  v31.8b, v20.8h, #5        // dst[0_8] = CLIP_U8((temp4 +16) >> 5)
    ld1       {v10.2s, v11.2s}, [x0], x2
    mls       v28.8h, v26.8h , v24.8h   // temp4 -= temp5 * 5
    st1       {v30.2s, v31.2s}, [x1], x3 //  store row 4
    mla       v14.8h, v12.8h , v22.8h   // temp += temp1 * 20
    uaddl     v20.8h, v11.8b, v1.8b
    uaddl     v26.8h, v3.8b, v9.8b
    mla       v20.8h, v18.8h , v22.8h   // temp4 += temp3 * 20
    uaddl     v12.8h, v6.8b, v4.8b
    uaddl     v18.8h, v7.8b, v9.8b
    sqrshrun  v31.8b, v28.8h, #5        // dst[0_8] = CLIP_U8((temp4 +16) >> 5)
    mls       v14.8h, v16.8h , v24.8h   // temp -= temp2 * 5
    uaddl     v16.8h, v8.8b, v2.8b
    sqrshrun  v30.8b, v14.8h, #5        // dst[0_0] = CLIP_U8((temp +16) >> 5)
    mls       v20.8h, v26.8h , v24.8h   // temp4 -= temp5 * 5
    uaddl     v14.8h, v10.8b, v0.8b
    st1       {v30.2s, v31.2s}, [x1], x3 //  store row 5
    mla       v14.8h, v12.8h , v22.8h   // temp += temp1 * 20
    ld1       {v0.2s, v1.2s}, [x0], x2
    uaddl     v26.8h, v5.8b, v11.8b
    uaddl     v12.8h, v8.8b, v6.8b
    uaddl     v28.8h, v0.8b, v2.8b
    sqrshrun  v31.8b, v20.8h, #5        // dst[0_8] = CLIP_U8((temp4 +16) >> 5)
    mla       v28.8h, v12.8h , v22.8h   // temp += temp1 * 20
    uaddl     v20.8h, v1.8b, v3.8b
    mls       v14.8h, v16.8h , v24.8h   // temp -= temp2 * 5
    mla       v20.8h, v18.8h , v22.8h   // temp4 += temp3 * 20
    uaddl     v16.8h, v10.8b, v4.8b
    sqrshrun  v30.8b, v14.8h, #5        // dst[0_0] = CLIP_U8((temp +16) >> 5)
    mov       v2.8b, v6.8b
    mov       v3.8b, v7.8b
    mls       v28.8h, v16.8h , v24.8h   // temp -= temp2 * 5
    st1       {v30.2s, v31.2s}, [x1], x3 //  store row 6
    sqrshrun  v30.8b, v28.8h, #5        // dst[0_0] = CLIP_U8((temp +16) >> 5)

    swp       v0.8b, v4.8b
    swp       v1.8b, v5.8b



    mls       v20.8h, v26.8h , v24.8h   // temp4 -= temp5 * 5
    mov       v6.8b, v10.8b
    mov       v7.8b, v11.8b
    subs      x12, x14, #1              // if height==16  - looping

    swp       v4.8b, v8.8b
    swp       v5.8b, v9.8b


    sqrshrun  v31.8b, v20.8h, #5        // dst[0_8] = CLIP_U8((temp4 +16) >> 5)
    st1       {v30.2s, v31.2s}, [x1], x3 //  store row 7
    bne       end_func                  //if height =8  end function
    add       x14, x14, #1              //for checking loop
    ld1       {v10.2s, v11.2s}, [x0], x2
    uaddl     v12.8h, v4.8b, v6.8b      // temp1 = src[2_0] + src[3_0]

    b         loop_16                   // looping if height =16

loop_8_start:
//// Processing row0 and row1

    ld1       {v0.2s}, [x0], x2         // Vector load from src[0_0]
    ld1       {v1.2s}, [x0], x2         // Vector load from src[1_0]
    ld1       {v2.2s}, [x0], x2         // Vector load from src[2_0]
    ld1       {v3.2s}, [x0], x2         // Vector load from src[3_0]
    add       x14, x14, #1              //for checking loop
    ld1       {v4.2s}, [x0], x2         // Vector load from src[4_0]
    ld1       {v5.2s}, [x0], x2         // Vector load from src[5_0]

loop_8:
                                        //for checking loop
    uaddl     v6.8h, v2.8b, v3.8b       // temp1 = src[2_0] + src[3_0]
    uaddl     v8.8h, v0.8b, v5.8b       // temp = src[0_0] + src[5_0]
    uaddl     v10.8h, v1.8b, v4.8b      // temp2 = src[1_0] + src[4_0]
    mla       v8.8h, v6.8h , v22.8h     // temp += temp1 * 20
    ld1       {v6.2s}, [x0], x2
    uaddl     v14.8h, v3.8b, v4.8b
    uaddl     v16.8h, v1.8b, v6.8b
    uaddl     v18.8h, v2.8b, v5.8b
    mls       v8.8h, v10.8h , v24.8h    // temp -= temp2 * 5
    mla       v16.8h, v14.8h , v22.8h
    ld1       {v7.2s}, [x0], x2
    uaddl     v20.8h, v4.8b, v5.8b
    uaddl     v12.8h, v2.8b, v7.8b
    uaddl     v10.8h, v3.8b, v6.8b
    mls       v16.8h, v18.8h , v24.8h
    sqrshrun  v26.8b, v8.8h, #5         // dst[0_0] = CLIP_U8( (temp + 16) >> 5)
    mla       v12.8h, v20.8h , v22.8h
    ld1       {v0.2s}, [x0], x2
    uaddl     v14.8h, v5.8b, v6.8b
    sqrshrun  v27.8b, v16.8h, #5
    uaddl     v20.8h, v3.8b, v0.8b
    mls       v12.8h, v10.8h , v24.8h
    st1       {v26.2s}, [x1], x3        // Vector store to dst[0_0]
    uaddl     v18.8h, v4.8b, v7.8b
    mla       v20.8h, v14.8h , v22.8h
    st1       {v27.2s}, [x1], x3
    sqrshrun  v28.8b, v12.8h, #5
    st1       {v28.2s}, [x1], x3
    mls       v20.8h, v18.8h , v24.8h
    ld1       {v1.2s}, [x0], x2
    sqrshrun  v29.8b, v20.8h, #5
    subs      x9, x4, #4
    st1       {v29.2s}, [x1], x3        //store row 3


    beq       end_func                  // Branch if height==4


    uaddl     v14.8h, v6.8b, v7.8b      // temp1 = src[2_0] + src[3_0]
    uaddl     v16.8h, v0.8b, v5.8b      // temp = src[0_0] + src[5_0]
    uaddl     v18.8h, v1.8b, v4.8b      // temp2 = src[1_0] + src[4_0]
    mla       v18.8h, v14.8h , v22.8h   // temp += temp1 * 20
    ld1       {v2.2s}, [x0], x2
    mls       v18.8h, v16.8h , v24.8h   // temp -= temp2 * 5
    uaddl     v8.8h, v0.8b, v7.8b
    uaddl     v10.8h, v1.8b, v6.8b
    uaddl     v12.8h, v2.8b, v5.8b
    sqrshrun  v26.8b, v18.8h, #5
    mla       v12.8h, v8.8h , v22.8h
    ld1       {v3.2s}, [x0], x2
    mls       v12.8h, v10.8h , v24.8h
    st1       {v26.2s}, [x1], x3
    sqrshrun  v27.8b, v12.8h, #5
    st1       {v27.2s}, [x1], x3
    uaddl     v14.8h, v0.8b, v1.8b      // temp1 = src[2_0] + src[3_0]
    uaddl     v16.8h, v2.8b, v7.8b      // temp = src[0_0] + src[5_0]
    uaddl     v18.8h, v3.8b, v6.8b      // temp2 = src[1_0] + src[4_0]
    mla       v18.8h, v14.8h , v22.8h   // temp += temp1 * 20
    ld1       {v4.2s}, [x0], x2
    mls       v18.8h, v16.8h , v24.8h   // temp -= temp2 * 5
    uaddl     v8.8h, v2.8b, v1.8b
    uaddl     v10.8h, v3.8b, v0.8b
    uaddl     v12.8h, v4.8b, v7.8b
    sqrshrun  v26.8b, v18.8h, #5
    mla       v12.8h, v8.8h , v22.8h
    ld1       {v5.2s}, [x0], x2
    mls       v12.8h, v10.8h , v24.8h
    st1       {v26.2s}, [x1], x3
    sqrshrun  v27.8b, v12.8h, #5
    subs      x12, x14, #1
    st1       {v27.2s}, [x1], x3
    add       x14, x14, #1
    beq       loop_8                    //looping if height ==16

    b         end_func


loop_4_start:
//// Processing row0 and row1


    ld1       {v0.s}[0], [x0], x2       // Vector load from src[0_0]
    ld1       {v1.s}[0], [x0], x2       // Vector load from src[1_0]
    ld1       {v2.s}[0], [x0], x2       // Vector load from src[2_0]
    ld1       {v3.s}[0], [x0], x2       // Vector load from src[3_0]
    ld1       {v4.s}[0], [x0], x2       // Vector load from src[4_0]
    ld1       {v5.s}[0], [x0], x2       // Vector load from src[5_0]

    uaddl     v6.8h, v2.8b, v3.8b       // temp1 = src[2_0] + src[3_0]
    uaddl     v8.8h, v0.8b, v5.8b       // temp = src[0_0] + src[5_0]
    uaddl     v10.8h, v1.8b, v4.8b      // temp2 = src[1_0] + src[4_0]
    mla       v8.8h, v6.8h , v22.8h     // temp += temp1 * 20
    ld1       {v6.2s}, [x0], x2
    uaddl     v14.8h, v3.8b, v4.8b
    uaddl     v16.8h, v1.8b, v6.8b
    uaddl     v18.8h, v2.8b, v5.8b
    mls       v8.8h, v10.8h , v24.8h    // temp -= temp2 * 5
    ld1       {v7.s}[0], [x0], x2
    mla       v16.8h, v14.8h , v22.8h
    uaddl     v20.8h, v4.8b, v5.8b
    uaddl     v12.8h, v2.8b, v7.8b
    uaddl     v10.8h, v3.8b, v6.8b
    mls       v16.8h, v18.8h , v24.8h
    sqrshrun  v26.8b, v8.8h, #5         // dst[0_0] = CLIP_U8( (temp + 16) >> 5)
    mla       v12.8h, v20.8h , v22.8h
    ld1       {v0.s}[0], [x0], x2
    uaddl     v14.8h, v5.8b, v6.8b
    sqrshrun  v27.8b, v16.8h, #5
    uaddl     v20.8h, v3.8b, v0.8b
    mls       v12.8h, v10.8h , v24.8h
    st1       {v26.s}[0], [x1], x3      // Vector store to dst[0_0]
    uaddl     v18.8h, v4.8b, v7.8b
    mla       v20.8h, v14.8h , v22.8h
    st1       {v27.s}[0], [x1], x3
    sqrshrun  v28.8b, v12.8h, #5
    st1       {v28.s}[0], [x1], x3
    mls       v20.8h, v18.8h , v24.8h
    ld1       {v1.s}[0], [x0], x2
    sqrshrun  v29.8b, v20.8h, #5
    st1       {v29.s}[0], [x1], x3      //store row 3

    subs      x9, x4, #4
    beq       end_func                  // Branch if height==4


    uaddl     v14.8h, v6.8b, v7.8b      // temp1 = src[2_0] + src[3_0]
    uaddl     v16.8h, v0.8b, v5.8b      // temp = src[0_0] + src[5_0]
    uaddl     v18.8h, v1.8b, v4.8b      // temp2 = src[1_0] + src[4_0]
    mla       v18.8h, v14.8h , v22.8h   // temp += temp1 * 20
    ld1       {v2.s}[0], [x0], x2
    mls       v18.8h, v16.8h , v24.8h   // temp -= temp2 * 5
    uaddl     v8.8h, v0.8b, v7.8b
    uaddl     v10.8h, v1.8b, v6.8b
    uaddl     v12.8h, v2.8b, v5.8b
    sqrshrun  v26.8b, v18.8h, #5
    mla       v12.8h, v8.8h , v22.8h
    ld1       {v3.s}[0], [x0], x2
    mls       v12.8h, v10.8h , v24.8h
    st1       {v26.s}[0], [x1], x3
    sqrshrun  v27.8b, v12.8h, #5
    st1       {v27.s}[0], [x1], x3
    uaddl     v14.8h, v0.8b, v1.8b      // temp1 = src[2_0] + src[3_0]
    uaddl     v16.8h, v2.8b, v7.8b      // temp = src[0_0] + src[5_0]
    uaddl     v18.8h, v3.8b, v6.8b      // temp2 = src[1_0] + src[4_0]
    mla       v18.8h, v14.8h , v22.8h   // temp += temp1 * 20
    ld1       {v4.s}[0], [x0], x2
    mls       v18.8h, v16.8h , v24.8h   // temp -= temp2 * 5
    uaddl     v8.8h, v2.8b, v1.8b
    uaddl     v10.8h, v3.8b, v0.8b
    uaddl     v12.8h, v4.8b, v7.8b
    sqrshrun  v26.8b, v18.8h, #5
    mla       v12.8h, v8.8h , v22.8h
    ld1       {v5.s}[0], [x0], x2
    mls       v12.8h, v10.8h , v24.8h
    st1       {v26.s}[0], [x1], x3
    sqrshrun  v27.8b, v12.8h, #5
    st1       {v27.s}[0], [x1], x3


end_func:
    // LDMFD sp!,{x4-x12,PC}         //Restoring registers from stack
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret



