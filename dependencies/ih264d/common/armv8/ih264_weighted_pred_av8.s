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
//*  ih264_weighted_pred_av8.s
//*
//* @brief
//*  Contains function definitions for weighted prediction.
//*
//* @author
//*  Kaushik Senthoor R
//*
//* @par List of Functions:
//*
//*  - ih264_weighted_pred_luma_av8()
//*  - ih264_weighted_pred_chroma_av8()
//*
//* @remarks
//*  None
//*
//*******************************************************************************
//*/
//*******************************************************************************
//* @function
//*  ih264_weighted_pred_luma_av8()
//*
//* @brief
//*  This routine performs the default weighted prediction as described in sec
//* 8.4.2.3.2 titled "Weighted sample prediction process" for luma.
//*
//* @par Description:
//*  This function gets a ht x wd block, calculates the weighted sample, rounds
//* off, adds offset and stores it in the destination block.
//*
//* @param[in] puc_src:
//*  UWORD8 Pointer to the buffer containing the input block.
//*
//* @param[out] puc_dst
//*  UWORD8 pointer to the destination where the output block is stored.
//*
//* @param[in] src_strd
//*  Stride of the input buffer
//*
//* @param[in] dst_strd
//*  Stride of the destination buffer
//*
//* @param[in] log_WD
//*  number of bits to be rounded off
//*
//* @param[in] wt
//*  weight for the weighted prediction
//*
//* @param[in] ofst
//*  offset used after rounding off
//*
//* @param[in] ht
//*  integer height of the array
//*
//* @param[in] wd
//*  integer width of the array
//*
//* @returns
//*  None
//*
//* @remarks
//*  (ht,wd) can be (4,4), (4,8), (8,4), (8,8), (8,16), (16,8) or (16,16).
//*
//*******************************************************************************
//*/
//void ih264_weighted_pred_luma_av8(UWORD8 *puc_src,
//                                  UWORD8 *puc_dst,
//                                  WORD32 src_strd,
//                                  WORD32 dst_strd,
//                                  WORD32 log_WD,
//                                  WORD32 wt,
//                                  WORD32 ofst,
//                                  WORD32 ht,
//                                  WORD32 wd)
//
//**************Variables Vs Registers*****************************************
//    x0      => puc_src
//    x1      => puc_dst
//    w2      => src_strd
//    w3      => dst_strd
//    w4      => log_WD
//    w5      => wt
//    w6      => ofst
//    w7      => ht
//    [sp]    => wd     (w8)
//
.text
.p2align 2
.include "ih264_neon_macros.s"



    .global ih264_weighted_pred_luma_av8

ih264_weighted_pred_luma_av8:

    // STMFD sp!, {x4-x9,x14}                //stack stores the values of the arguments
    push_v_regs
    sxtw      x2, w2
    sxtw      x3, w3
    stp       x19, x20, [sp, #-16]!
    ldr       w8, [sp, #80]             //Load wd
    sxtw      x8, w8

    dup       v2.4h, w5                 //D2 = wt (16-bit)
    neg       w9, w4                    //w9 = -log_WD
    dup       v3.8b, w6                 //D3 = ofst (8-bit)
    cmp       w8, #16                   //check if wd is 16
    dup       v0.8h, w9                 //Q0 = -log_WD (16-bit)
    beq       loop_16                   //branch if wd is 16

    cmp       w8, #8                    //check if wd is 8
    beq       loop_8                    //branch if wd is 8

loop_4:                                 //each iteration processes four rows

    ld1       {v4.s}[0], [x0], x2       //load row 1 in source
    ld1       {v4.s}[1], [x0], x2       //load row 2 in source
    ld1       {v6.s}[0], [x0], x2       //load row 3 in source
    ld1       {v6.s}[1], [x0], x2       //load row 4 in source

    uxtl      v4.8h, v4.8b              //converting rows 1,2 to 16-bit
    uxtl      v6.8h, v6.8b              //converting rows 3,4 to 16-bit

    mul       v4.8h, v4.8h , v2.h[0]    //weight mult. for rows 1,2
    mul       v6.8h, v6.8h , v2.h[0]    //weight mult. for rows 3,4

    subs      w7, w7, #4                //decrement ht by 4
    srshl     v4.8h, v4.8h , v0.8h      //rounds off the weighted samples from rows 1,2
    srshl     v6.8h, v6.8h , v0.8h      //rounds off the weighted samples from rows 3,4

    saddw     v4.8h, v4.8h , v3.8b      //adding offset for rows 1,2
    saddw     v6.8h, v6.8h , v3.8b      //adding offset for rows 3,4

    sqxtun    v4.8b, v4.8h              //saturating rows 1,2 to unsigned 8-bit
    sqxtun    v6.8b, v6.8h              //saturating rows 3,4 to unsigned 8-bit

    st1       {v4.s}[0], [x1], x3       //store row 1 in destination
    st1       {v4.s}[1], [x1], x3       //store row 2 in destination
    st1       {v6.s}[0], [x1], x3       //store row 3 in destination
    st1       {v6.s}[1], [x1], x3       //store row 4 in destination

    bgt       loop_4                    //if greater than 0 repeat the loop again

    b         end_loops

loop_8:                                 //each iteration processes four rows

    ld1       {v4.8b}, [x0], x2         //load row 1 in source
    ld1       {v6.8b}, [x0], x2         //load row 2 in source
    ld1       {v8.8b}, [x0], x2         //load row 3 in source
    uxtl      v4.8h, v4.8b              //converting row 1 to 16-bit
    ld1       {v10.8b}, [x0], x2        //load row 4 in source
    uxtl      v6.8h, v6.8b              //converting row 2 to 16-bit

    uxtl      v8.8h, v8.8b              //converting row 3 to 16-bit
    mul       v4.8h, v4.8h , v2.h[0]    //weight mult. for row 1
    uxtl      v10.8h, v10.8b            //converting row 4 to 16-bit
    mul       v6.8h, v6.8h , v2.h[0]    //weight mult. for row 2
    mul       v8.8h, v8.8h , v2.h[0]    //weight mult. for row 3
    mul       v10.8h, v10.8h , v2.h[0]  //weight mult. for row 4

    srshl     v4.8h, v4.8h , v0.8h      //rounds off the weighted samples from row 1
    srshl     v6.8h, v6.8h , v0.8h      //rounds off the weighted samples from row 2
    srshl     v8.8h, v8.8h , v0.8h      //rounds off the weighted samples from row 3
    saddw     v4.8h, v4.8h , v3.8b      //adding offset for row 1
    srshl     v10.8h, v10.8h , v0.8h    //rounds off the weighted samples from row 4
    saddw     v6.8h, v6.8h , v3.8b      //adding offset for row 2

    saddw     v8.8h, v8.8h , v3.8b      //adding offset for row 3
    sqxtun    v4.8b, v4.8h              //saturating row 1 to unsigned 8-bit
    saddw     v10.8h, v10.8h , v3.8b    //adding offset for row 4
    sqxtun    v6.8b, v6.8h              //saturating row 2 to unsigned 8-bit
    sqxtun    v8.8b, v8.8h              //saturating row 3 to unsigned 8-bit
    sqxtun    v10.8b, v10.8h            //saturating row 4 to unsigned 8-bit

    st1       {v4.8b}, [x1], x3         //store row 1 in destination
    st1       {v6.8b}, [x1], x3         //store row 2 in destination
    subs      w7, w7, #4                //decrement ht by 4
    st1       {v8.8b}, [x1], x3         //store row 3 in destination
    st1       {v10.8b}, [x1], x3        //store row 4 in destination

    bgt       loop_8                    //if greater than 0 repeat the loop again

    b         end_loops

loop_16:                                //each iteration processes two rows

    ld1       {v4.8b, v5.8b}, [x0], x2  //load row 1 in source
    ld1       {v6.8b, v7.8b}, [x0], x2  //load row 2 in source
    uxtl      v12.8h, v4.8b             //converting row 1L to 16-bit
    ld1       {v8.8b, v9.8b}, [x0], x2  //load row 3 in source
    uxtl      v14.8h, v5.8b             //converting row 1H to 16-bit
    ld1       {v10.8b, v11.8b}, [x0], x2 //load row 4 in source
    uxtl      v16.8h, v6.8b             //converting row 2L to 16-bit
    mul       v12.8h, v12.8h , v2.h[0]  //weight mult. for row 1L
    uxtl      v18.8h, v7.8b             //converting row 2H to 16-bit
    mul       v14.8h, v14.8h , v2.h[0]  //weight mult. for row 1H
    uxtl      v20.8h, v8.8b             //converting row 3L to 16-bit
    mul       v16.8h, v16.8h , v2.h[0]  //weight mult. for row 2L
    uxtl      v22.8h, v9.8b             //converting row 3H to 16-bit
    mul       v18.8h, v18.8h , v2.h[0]  //weight mult. for row 2H
    uxtl      v24.8h, v10.8b            //converting row 4L to 16-bit
    mul       v20.8h, v20.8h , v2.h[0]  //weight mult. for row 3L
    uxtl      v26.8h, v11.8b            //converting row 4H to 16-bit
    mul       v22.8h, v22.8h , v2.h[0]  //weight mult. for row 3H
    mul       v24.8h, v24.8h , v2.h[0]  //weight mult. for row 4L
    srshl     v12.8h, v12.8h , v0.8h    //rounds off the weighted samples from row 1L
    mul       v26.8h, v26.8h , v2.h[0]  //weight mult. for row 4H
    srshl     v14.8h, v14.8h , v0.8h    //rounds off the weighted samples from row 1H
    srshl     v16.8h, v16.8h , v0.8h    //rounds off the weighted samples from row 2L
    saddw     v12.8h, v12.8h , v3.8b    //adding offset for row 1L
    srshl     v18.8h, v18.8h , v0.8h    //rounds off the weighted samples from row 2H
    saddw     v14.8h, v14.8h , v3.8b    //adding offset for row 1H
    sqxtun    v4.8b, v12.8h             //saturating row 1L to unsigned 8-bit
    srshl     v20.8h, v20.8h , v0.8h    //rounds off the weighted samples from row 3L
    saddw     v16.8h, v16.8h , v3.8b    //adding offset for row 2L
    sqxtun    v5.8b, v14.8h             //saturating row 1H to unsigned 8-bit
    srshl     v22.8h, v22.8h , v0.8h    //rounds off the weighted samples from row 3H
    saddw     v18.8h, v18.8h , v3.8b    //adding offset for row 2H
    sqxtun    v6.8b, v16.8h             //saturating row 2L to unsigned 8-bit
    srshl     v24.8h, v24.8h , v0.8h    //rounds off the weighted samples from row 4L
    saddw     v20.8h, v20.8h , v3.8b    //adding offset for row 3L
    sqxtun    v7.8b, v18.8h             //saturating row 2H to unsigned 8-bit
    srshl     v26.8h, v26.8h , v0.8h    //rounds off the weighted samples from row 4H
    saddw     v22.8h, v22.8h , v3.8b    //adding offset for row 3H
    sqxtun    v8.8b, v20.8h             //saturating row 3L to unsigned 8-bit
    saddw     v24.8h, v24.8h , v3.8b    //adding offset for row 4L
    sqxtun    v9.8b, v22.8h             //saturating row 3H to unsigned 8-bit
    saddw     v26.8h, v26.8h , v3.8b    //adding offset for row 4H
    sqxtun    v10.8b, v24.8h            //saturating row 4L to unsigned 8-bit
    st1       {v4.8b, v5.8b}, [x1], x3  //store row 1 in destination
    sqxtun    v11.8b, v26.8h            //saturating row 4H to unsigned 8-bit
    st1       {v6.8b, v7.8b}, [x1], x3  //store row 2 in destination
    subs      w7, w7, #4                //decrement ht by 4
    st1       {v8.8b, v9.8b}, [x1], x3  //store row 3 in destination
    st1       {v10.8b, v11.8b}, [x1], x3 //store row 4 in destination

    bgt       loop_16                   //if greater than 0 repeat the loop again

end_loops:

    // LDMFD sp!,{x4-x9,x15}                      //Reload the registers from sp
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret


//*******************************************************************************
//* @function
//*  ih264_weighted_pred_chroma_av8()
//*
//* @brief
//*  This routine performs the default weighted prediction as described in sec
//* 8.4.2.3.2 titled "Weighted sample prediction process" for chroma.
//*
//* @par Description:
//*  This function gets a ht x wd block, calculates the weighted sample, rounds
//* off, adds offset and stores it in the destination block for U and V.
//*
//* @param[in] puc_src:
//*  UWORD8 Pointer to the buffer containing the input block.
//*
//* @param[out] puc_dst
//*  UWORD8 pointer to the destination where the output block is stored.
//*
//* @param[in] src_strd
//*  Stride of the input buffer
//*
//* @param[in] dst_strd
//*  Stride of the destination buffer
//*
//* @param[in] log_WD
//*  number of bits to be rounded off
//*
//* @param[in] wt
//*  weights for the weighted prediction for U and V
//*
//* @param[in] ofst
//*  offsets used after rounding off for U and V
//*
//* @param[in] ht
//*  integer height of the array
//*
//* @param[in] wd
//*  integer width of the array
//*
//* @returns
//*  None
//*
//* @remarks
//*  (ht,wd) can be (2,2), (2,4), (4,2), (4,4), (4,8), (8,4) or (8,8).
//*
//*******************************************************************************
//*/
//void ih264_weighted_pred_chroma_av8(UWORD8 *puc_src,
//                                    UWORD8 *puc_dst,
//                                    WORD32 src_strd,
//                                    WORD32 dst_strd,
//                                    WORD32 log_WD,
//                                    WORD32 wt,
//                                    WORD32 ofst,
//                                    WORD32 ht,
//                                    WORD32 wd)
//
//**************Variables Vs Registers*****************************************
//    x0      => puc_src
//    x1      => puc_dst
//    w2      => src_strd
//    w3      => dst_strd
//    w4      => log_WD
//    w5      => wt
//    w6      => ofst
//    w7      => ht
//    [sp]    => wd     (w8)
//




    .global ih264_weighted_pred_chroma_av8

ih264_weighted_pred_chroma_av8:

    // STMFD sp!, {x4-x9,x14}                //stack stores the values of the arguments
    push_v_regs
    sxtw      x2, w2
    sxtw      x3, w3
    stp       x19, x20, [sp, #-16]!

    ldr       w8, [sp, #80]             //Load wd
    sxtw      x8, w8

    neg       w9, w4                    //w9 = -log_WD
    dup       v2.4s, w5                 //Q1 = {wt_u (16-bit), wt_v (16-bit)}


    dup       v4.4h, w6                 //D4 = {ofst_u (8-bit), ofst_v (8-bit)}
    cmp       w8, #8                    //check if wd is 8
    dup       v0.8h, w9                 //Q0 = -log_WD (16-bit)
    beq       loop_8_uv                 //branch if wd is 8

    cmp       w8, #4                    //check if ws is 4
    beq       loop_4_uv                 //branch if wd is 4

loop_2_uv:                              //each iteration processes two rows

    ld1       {v6.s}[0], [x0], x2       //load row 1 in source
    ld1       {v6.s}[1], [x0], x2       //load row 2 in source
    uxtl      v6.8h, v6.8b              //converting rows 1,2 to 16-bit
    mul       v6.8h, v6.8h , v2.8h      //weight mult. for rows 1,2
    srshl     v6.8h, v6.8h , v0.8h      //rounds off the weighted samples from rows 1,2
    saddw     v6.8h, v6.8h , v4.8b      //adding offset for rows 1,2
    sqxtun    v6.8b, v6.8h              //saturating rows 1,2 to unsigned 8-bit
    subs      w7, w7, #2                //decrement ht by 2
    st1       {v6.s}[0], [x1], x3       //store row 1 in destination
    st1       {v6.s}[1], [x1], x3       //store row 2 in destination
    bgt       loop_2_uv                 //if greater than 0 repeat the loop again
    b         end_loops_uv

loop_4_uv:                              //each iteration processes two rows

    ld1       {v6.8b}, [x0], x2         //load row 1 in source
    ld1       {v8.8b}, [x0], x2         //load row 2 in source
    uxtl      v6.8h, v6.8b              //converting row 1 to 16-bit
    uxtl      v8.8h, v8.8b              //converting row 2 to 16-bit
    mul       v6.8h, v6.8h , v2.8h      //weight mult. for row 1
    mul       v8.8h, v8.8h , v2.8h      //weight mult. for row 2
    subs      w7, w7, #2                //decrement ht by 2
    srshl     v6.8h, v6.8h , v0.8h      //rounds off the weighted samples from row 1
    srshl     v8.8h, v8.8h , v0.8h      //rounds off the weighted samples from row 2
    saddw     v6.8h, v6.8h , v4.8b      //adding offset for row 1
    saddw     v8.8h, v8.8h , v4.8b      //adding offset for row 2
    sqxtun    v6.8b, v6.8h              //saturating row 1 to unsigned 8-bit
    sqxtun    v8.8b, v8.8h              //saturating row 2 to unsigned 8-bit
    st1       {v6.8b}, [x1], x3         //store row 1 in destination
    st1       {v8.8b}, [x1], x3         //store row 2 in destination

    bgt       loop_4_uv                 //if greater than 0 repeat the loop again

    b         end_loops_uv

loop_8_uv:                              //each iteration processes two rows

    ld1       {v6.8b, v7.8b}, [x0], x2  //load row 1 in source
    ld1       {v8.8b, v9.8b}, [x0], x2  //load row 2 in source
    uxtl      v14.8h, v6.8b             //converting row 1L to 16-bit
    ld1       {v10.8b, v11.8b}, [x0], x2 //load row 3 in source
    uxtl      v16.8h, v7.8b             //converting row 1H to 16-bit
    ld1       {v12.8b, v13.8b}, [x0], x2 //load row 4 in source

    mul       v14.8h, v14.8h , v2.8h    //weight mult. for row 1L
    uxtl      v18.8h, v8.8b             //converting row 2L to 16-bit
    mul       v16.8h, v16.8h , v2.8h    //weight mult. for row 1H
    uxtl      v20.8h, v9.8b             //converting row 2H to 16-bit
    mul       v18.8h, v18.8h , v2.8h    //weight mult. for row 2L
    uxtl      v22.8h, v10.8b            //converting row 3L to 16-bit
    mul       v20.8h, v20.8h , v2.8h    //weight mult. for row 2H
    uxtl      v24.8h, v11.8b            //converting row 3H to 16-bit
    mul       v22.8h, v22.8h , v2.8h    //weight mult. for row 3L
    uxtl      v26.8h, v12.8b            //converting row 4L to 16-bit
    mul       v24.8h, v24.8h , v2.8h    //weight mult. for row 3H
    uxtl      v28.8h, v13.8b            //converting row 4H to 16-bit

    mul       v26.8h, v26.8h , v2.8h    //weight mult. for row 4L
    srshl     v14.8h, v14.8h , v0.8h    //rounds off the weighted samples from row 1L
    mul       v28.8h, v28.8h , v2.8h    //weight mult. for row 4H

    srshl     v16.8h, v16.8h , v0.8h    //rounds off the weighted samples from row 1H
    srshl     v18.8h, v18.8h , v0.8h    //rounds off the weighted samples from row 2L
    saddw     v14.8h, v14.8h , v4.8b    //adding offset for row 1L
    srshl     v20.8h, v20.8h , v0.8h    //rounds off the weighted samples from row 2H
    saddw     v16.8h, v16.8h , v4.8b    //adding offset for row 1H
    sqxtun    v6.8b, v14.8h             //saturating row 1L to unsigned 8-bit
    srshl     v22.8h, v22.8h , v0.8h    //rounds off the weighted samples from row 3L
    saddw     v18.8h, v18.8h , v4.8b    //adding offset for row 2L
    sqxtun    v7.8b, v16.8h             //saturating row 1H to unsigned 8-bit
    srshl     v24.8h, v24.8h , v0.8h    //rounds off the weighted samples from row 3H
    saddw     v20.8h, v20.8h , v4.8b    //adding offset for row 2H
    sqxtun    v8.8b, v18.8h             //saturating row 2L to unsigned 8-bit
    srshl     v26.8h, v26.8h , v0.8h    //rounds off the weighted samples from row 4L
    saddw     v22.8h, v22.8h , v4.8b    //adding offset for row 3L
    sqxtun    v9.8b, v20.8h             //saturating row 2H to unsigned 8-bit
    srshl     v28.8h, v28.8h , v0.8h    //rounds off the weighted samples from row 4H
    saddw     v24.8h, v24.8h , v4.8b    //adding offset for row 3H

    sqxtun    v10.8b, v22.8h            //saturating row 3L to unsigned 8-bit
    saddw     v26.8h, v26.8h , v4.8b    //adding offset for row 4L
    sqxtun    v11.8b, v24.8h            //saturating row 3H to unsigned 8-bit
    saddw     v28.8h, v28.8h , v4.8b    //adding offset for row 4H

    sqxtun    v12.8b, v26.8h            //saturating row 4L to unsigned 8-bit
    st1       {v6.8b, v7.8b}, [x1], x3  //store row 1 in destination
    sqxtun    v13.8b, v28.8h            //saturating row 4H to unsigned 8-bit
    st1       {v8.8b, v9.8b}, [x1], x3  //store row 2 in destination
    subs      w7, w7, #4                //decrement ht by 4
    st1       {v10.8b, v11.8b}, [x1], x3 //store row 3 in destination
    st1       {v12.8b, v13.8b}, [x1], x3 //store row 4 in destination

    bgt       loop_8_uv                 //if greater than 0 repeat the loop again

end_loops_uv:

    // LDMFD sp!,{x4-x9,x15}                      //Reload the registers from sp
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret



