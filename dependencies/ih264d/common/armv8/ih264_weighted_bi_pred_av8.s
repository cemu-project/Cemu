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
//*  ih264_weighted_bi_pred_av8.s
//*
//* @brief
//*  Contains function definitions for weighted biprediction.
//*
//* @author
//*  Kaushik Senthoor R
//*
//* @par List of Functions:
//*
//*  - ih264_weighted_bi_pred_luma_av8()
//*  - ih264_weighted_bi_pred_chroma_av8()
//*
//* @remarks
//*  None
//*
//*******************************************************************************
//*/
//*******************************************************************************
//* @function
//*  ih264_weighted_bi_pred_luma_av8()
//*
//* @brief
//*  This routine performs the default weighted prediction as described in sec
//* 8.4.2.3.2 titled "Weighted sample prediction process" for luma.
//*
//* @par Description:
//*  This function gets two ht x wd blocks, calculates the weighted samples,
//* rounds off, adds offset and stores it in the destination block.
//*
//* @param[in] puc_src1
//*  UWORD8 Pointer to the buffer containing the input block 1.
//*
//* @param[in] puc_src2
//*  UWORD8 Pointer to the buffer containing the input block 2.
//*
//* @param[out] puc_dst
//*  UWORD8 pointer to the destination where the output block is stored.
//*
//* @param[in] src_strd1
//*  Stride of the input buffer 1
//*
//* @param[in] src_strd2
//*  Stride of the input buffer 2
//*
//* @param[in] dst_strd
//*  Stride of the destination buffer
//*
//* @param[in] log_WD
//*  number of bits to be rounded off
//*
//* @param[in] wt1
//*  weight for the weighted prediction
//*
//* @param[in] wt2
//*  weight for the weighted prediction
//*
//* @param[in] ofst1
//*  offset 1 used after rounding off
//*
//* @param[in] ofst2
//*  offset 2 used after rounding off
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
//void ih264_weighted_bi_pred_luma_av8(UWORD8 *puc_src1,
//                                     UWORD8 *puc_src2,
//                                     UWORD8 *puc_dst,
//                                     WORD32 src_strd1,
//                                     WORD32 src_strd2,
//                                     WORD32 dst_strd,
//                                     WORD32 log_WD,
//                                     WORD32 wt1,
//                                     WORD32 wt2,
//                                     WORD16 ofst1,
//                                     WORD16 ofst2,
//                                     WORD32 ht,
//                                     WORD32 wd)
//
//**************Variables Vs Registers*****************************************
//    x0      => puc_src1
//    x1      => puc_src2
//    x2      => puc_dst
//    w3      => src_strd1
//    w4      => src_strd2
//    w5      => dst_strd
//    w6      => log_WD
//    w7      => wt1
//    [sp]    => wt2       (w8)
//    [sp+8]  => ofst1     (w9)
//    [sp+16] => ofst2     (w10)
//    [sp+24] => ht        (w11)
//    [sp+32] => wd        (w12)
//
.text
.p2align 2
.include "ih264_neon_macros.s"



    .global ih264_weighted_bi_pred_luma_av8

ih264_weighted_bi_pred_luma_av8:

    // STMFD sp!, {x4-x12,x14}                //stack stores the values of the arguments
    push_v_regs
    sxtw      x3, w3
    sxtw      x4, w4
    sxtw      x5, w5
    stp       x19, x20, [sp, #-16]!
    ldr       w8, [sp, #80]             //Load wt2 in w8
    ldr       w9, [sp, #88]             //Load ofst1 in w9
    add       w6, w6, #1                //w6  = log_WD + 1
    neg       w10, w6                   //w10 = -(log_WD + 1)
    dup       v0.8h, w10                //Q0  = -(log_WD + 1) (32-bit)
    ldr       w10, [sp, #96]            //Load ofst2 in w10
    ldr       w11, [sp, #104]           //Load ht in w11
    ldr       w12, [sp, #112]           //Load wd in w12
    add       w9, w9, #1                //w9 = ofst1 + 1
    add       w9, w9, w10               //w9 = ofst1 + ofst2 + 1
    mov       v2.s[0], w7
    mov       v2.s[1], w8               //D2 = {wt1(32-bit), wt2(32-bit)}
    asr       w9, w9, #1                //w9 = ofst = (ofst1 + ofst2 + 1) >> 1
    dup       v3.8b, w9                 //D3 = ofst (8-bit)
    cmp       w12, #16
    beq       loop_16                   //branch if wd is 16
    cmp       w12, #8                   //check if wd is 8
    beq       loop_8                    //branch if wd is 8

loop_4:                                 //each iteration processes four rows

    ld1       {v4.s}[0], [x0], x3       //load row 1 in source 1
    ld1       {v4.s}[1], [x0], x3       //load row 2 in source 1
    ld1       {v6.s}[0], [x1], x4       //load row 1 in source 2
    ld1       {v6.s}[1], [x1], x4       //load row 2 in source 2
    uxtl      v4.8h, v4.8b              //converting rows 1,2 in source 1 to 16-bit
    ld1       {v8.s}[0], [x0], x3       //load row 3 in source 1
    ld1       {v8.s}[1], [x0], x3       //load row 4 in source 1
    uxtl      v6.8h, v6.8b              //converting rows 1,2 in source 2 to 16-bit
    ld1       {v10.s}[0], [x1], x4      //load row 3 in source 2
    ld1       {v10.s}[1], [x1], x4      //load row 4 in source 2
    uxtl      v8.8h, v8.8b              //converting rows 3,4 in source 1 to 16-bit
    uxtl      v10.8h, v10.8b            //converting rows 3,4 in source 2 to 16-bit
    mul       v4.8h, v4.8h , v2.h[0]    //weight 1 mult. for rows 1,2
    mla       v4.8h, v6.8h , v2.h[2]    //weight 2 mult. for rows 1,2
    mul       v8.8h, v8.8h , v2.h[0]    //weight 1 mult. for rows 3,4
    mla       v8.8h, v10.8h , v2.h[2]   //weight 2 mult. for rows 3,4
    subs      w11, w11, #4              //decrement ht by 4
    srshl     v4.8h, v4.8h , v0.8h      //rounds off the weighted samples from rows 1,2
    srshl     v8.8h, v8.8h , v0.8h      //rounds off the weighted samples from rows 3,4
    saddw     v4.8h, v4.8h , v3.8b      //adding offset for rows 1,2
    saddw     v8.8h, v8.8h , v3.8b      //adding offset for rows 3,4
    sqxtun    v4.8b, v4.8h              //saturating rows 1,2 to unsigned 8-bit
    sqxtun    v8.8b, v8.8h              //saturating rows 3,4 to unsigned 8-bit
    st1       {v4.s}[0], [x2], x5       //store row 1 in destination
    st1       {v4.s}[1], [x2], x5       //store row 2 in destination
    st1       {v8.s}[0], [x2], x5       //store row 3 in destination
    st1       {v8.s}[1], [x2], x5       //store row 4 in destination
    bgt       loop_4                    //if greater than 0 repeat the loop again
    b         end_loops

loop_8:                                 //each iteration processes four rows

    ld1       {v4.8b}, [x0], x3         //load row 1 in source 1
    ld1       {v6.8b}, [x1], x4         //load row 1 in source 2
    ld1       {v8.8b}, [x0], x3         //load row 2 in source 1
    ld1       {v10.8b}, [x1], x4        //load row 2 in source 2
    uxtl      v4.8h, v4.8b              //converting row 1 in source 1 to 16-bit
    ld1       {v12.8b}, [x0], x3        //load row 3 in source 1
    ld1       {v14.8b}, [x1], x4        //load row 3 in source 2
    uxtl      v6.8h, v6.8b              //converting row 1 in source 2 to 16-bit
    ld1       {v16.8b}, [x0], x3        //load row 4 in source 1
    ld1       {v18.8b}, [x1], x4        //load row 4 in source 2
    uxtl      v8.8h, v8.8b              //converting row 2 in source 1 to 16-bit
    uxtl      v10.8h, v10.8b            //converting row 2 in source 2 to 16-bit
    mul       v4.8h, v4.8h , v2.h[0]    //weight 1 mult. for row 1
    mla       v4.8h, v6.8h , v2.h[2]    //weight 2 mult. for row 1
    uxtl      v12.8h, v12.8b            //converting row 3 in source 1 to 16-bit
    uxtl      v14.8h, v14.8b            //converting row 3 in source 2 to 16-bit
    mul       v8.8h, v8.8h , v2.h[0]    //weight 1 mult. for row 2
    mla       v8.8h, v10.8h , v2.h[2]   //weight 2 mult. for row 2
    uxtl      v16.8h, v16.8b            //converting row 4 in source 1 to 16-bit
    uxtl      v18.8h, v18.8b            //converting row 4 in source 2 to 16-bit
    mul       v12.8h, v12.8h , v2.h[0]  //weight 1 mult. for row 3
    mla       v12.8h, v14.8h , v2.h[2]  //weight 2 mult. for row 3
    mul       v16.8h, v16.8h , v2.h[0]  //weight 1 mult. for row 4
    mla       v16.8h, v18.8h , v2.h[2]  //weight 2 mult. for row 4
    srshl     v4.8h, v4.8h , v0.8h      //rounds off the weighted samples from row 1
    srshl     v8.8h, v8.8h , v0.8h      //rounds off the weighted samples from row 2
    srshl     v12.8h, v12.8h , v0.8h    //rounds off the weighted samples from row 3
    saddw     v4.8h, v4.8h , v3.8b      //adding offset for row 1
    srshl     v16.8h, v16.8h , v0.8h    //rounds off the weighted samples from row 4
    saddw     v8.8h, v8.8h , v3.8b      //adding offset for row 2
    saddw     v12.8h, v12.8h , v3.8b    //adding offset for row 3
    sqxtun    v4.8b, v4.8h              //saturating row 1 to unsigned 8-bit
    saddw     v16.8h, v16.8h , v3.8b    //adding offset for row 4
    sqxtun    v8.8b, v8.8h              //saturating row 2 to unsigned 8-bit
    sqxtun    v12.8b, v12.8h            //saturating row 3 to unsigned 8-bit
    sqxtun    v16.8b, v16.8h            //saturating row 4 to unsigned 8-bit
    st1       {v4.8b}, [x2], x5         //store row 1 in destination
    st1       {v8.8b}, [x2], x5         //store row 2 in destination
    subs      w11, w11, #4              //decrement ht by 4
    st1       {v12.8b}, [x2], x5        //store row 3 in destination
    st1       {v16.8b}, [x2], x5        //store row 4 in destination
    bgt       loop_8                    //if greater than 0 repeat the loop again
    b         end_loops

loop_16:                                //each iteration processes two rows

    ld1       {v4.8b, v5.8b}, [x0], x3  //load row 1 in source 1
    ld1       {v6.8b, v7.8b}, [x1], x4  //load row 1 in source 2
    ld1       {v8.8b, v9.8b}, [x0], x3  //load row 2 in source 1
    ld1       {v10.8b, v11.8b}, [x1], x4 //load row 2 in source 2
    uxtl      v20.8h, v4.8b             //converting row 1L in source 1 to 16-bit
    ld1       {v12.8b, v13.8b}, [x0], x3 //load row 3 in source 1
    ld1       {v14.8b, v15.8b}, [x1], x4 //load row 3 in source 2
    uxtl      v22.8h, v6.8b             //converting row 1L in source 2 to 16-bit
    ld1       {v16.8b, v17.8b}, [x0], x3 //load row 4 in source 1
    ld1       {v18.8b, v19.8b}, [x1], x4 //load row 4 in source 2
    uxtl      v4.8h, v5.8b              //converting row 1H in source 1 to 16-bit
    uxtl      v6.8h, v7.8b              //converting row 1H in source 2 to 16-bit
    mul       v20.8h, v20.8h , v2.h[0]  //weight 1 mult. for row 1L
    mla       v20.8h, v22.8h , v2.h[2]  //weight 2 mult. for row 1L
    uxtl      v24.8h, v8.8b             //converting row 2L in source 1 to 16-bit
    uxtl      v26.8h, v10.8b            //converting row 2L in source 2 to 16-bit
    mul       v4.8h, v4.8h , v2.h[0]    //weight 1 mult. for row 1H
    mla       v4.8h, v6.8h , v2.h[2]    //weight 2 mult. for row 1H
    uxtl      v8.8h, v9.8b              //converting row 2H in source 1 to 16-bit
    uxtl      v10.8h, v11.8b            //converting row 2H in source 2 to 16-bit
    mul       v24.8h, v24.8h , v2.h[0]  //weight 1 mult. for row 2L
    mla       v24.8h, v26.8h , v2.h[2]  //weight 2 mult. for row 2L
    uxtl      v28.8h, v12.8b            //converting row 3L in source 1 to 16-bit
    uxtl      v30.8h, v14.8b            //converting row 3L in source 2 to 16-bit
    mul       v8.8h, v8.8h , v2.h[0]    //weight 1 mult. for row 2H
    mla       v8.8h, v10.8h , v2.h[2]   //weight 2 mult. for row 2H
    uxtl      v12.8h, v13.8b            //converting row 3H in source 1 to 16-bit
    uxtl      v14.8h, v15.8b            //converting row 3H in source 2 to 16-bit
    mul       v28.8h, v28.8h , v2.h[0]  //weight 1 mult. for row 3L
    mla       v28.8h, v30.8h , v2.h[2]  //weight 2 mult. for row 3L
    uxtl      v22.8h, v16.8b            //converting row 4L in source 1 to 16-bit
    uxtl      v6.8h, v18.8b             //converting row 4L in source 2 to 16-bit
    mul       v12.8h, v12.8h , v2.h[0]  //weight 1 mult. for row 3H
    mla       v12.8h, v14.8h , v2.h[2]  //weight 2 mult. for row 3H
    uxtl      v16.8h, v17.8b            //converting row 4H in source 1 to 16-bit
    uxtl      v18.8h, v19.8b            //converting row 4H in source 2 to 16-bit
    mul       v22.8h, v22.8h , v2.h[0]  //weight 1 mult. for row 4L
    mla       v22.8h, v6.8h , v2.h[2]   //weight 2 mult. for row 4L
    srshl     v20.8h, v20.8h , v0.8h    //rounds off the weighted samples from row 1L
    mul       v16.8h, v16.8h , v2.h[0]  //weight 1 mult. for row 4H
    mla       v16.8h, v18.8h , v2.h[2]  //weight 2 mult. for row 4H
    srshl     v4.8h, v4.8h , v0.8h      //rounds off the weighted samples from row 1H
    srshl     v24.8h, v24.8h , v0.8h    //rounds off the weighted samples from row 2L
    saddw     v20.8h, v20.8h , v3.8b    //adding offset for row 1L
    srshl     v8.8h, v8.8h , v0.8h      //rounds off the weighted samples from row 2H
    saddw     v4.8h, v4.8h , v3.8b      //adding offset for row 1H
    srshl     v28.8h, v28.8h , v0.8h    //rounds off the weighted samples from row 3L
    saddw     v24.8h, v24.8h , v3.8b    //adding offset for row 2L
    srshl     v12.8h, v12.8h , v0.8h    //rounds off the weighted samples from row 3H
    saddw     v8.8h, v8.8h , v3.8b      //adding offset for row 2H
    srshl     v22.8h, v22.8h , v0.8h    //rounds off the weighted samples from row 4L
    saddw     v28.8h, v28.8h , v3.8b    //adding offset for row 3L
    srshl     v16.8h, v16.8h , v0.8h    //rounds off the weighted samples from row 4H
    saddw     v12.8h, v12.8h , v3.8b    //adding offset for row 3H
    sqxtun    v26.8b, v20.8h            //saturating row 1L to unsigned 8-bit
    saddw     v22.8h, v22.8h , v3.8b    //adding offset for row 4L
    sqxtun    v27.8b, v4.8h             //saturating row 1H to unsigned 8-bit
    saddw     v16.8h, v16.8h , v3.8b    //adding offset for row 4H
    sqxtun    v10.8b, v24.8h            //saturating row 2L to unsigned 8-bit
    sqxtun    v11.8b, v8.8h             //saturating row 2H to unsigned 8-bit
    sqxtun    v30.8b, v28.8h            //saturating row 3L to unsigned 8-bit
    sqxtun    v31.8b, v12.8h            //saturating row 3H to unsigned 8-bit
    st1       {v26.8b, v27.8b}, [x2], x5 //store row 1 in destination
    sqxtun    v14.8b, v22.8h            //saturating row 4L to unsigned 8-bit
    sqxtun    v15.8b, v16.8h            //saturating row 4H to unsigned 8-bit
    st1       {v10.8b, v11.8b}, [x2], x5 //store row 2 in destination
    subs      w11, w11, #4              //decrement ht by 4
    st1       {v30.8b, v31.8b}, [x2], x5 //store row 3 in destination
    st1       {v14.8b, v15.8b}, [x2], x5 //store row 4 in destination
    bgt       loop_16                   //if greater than 0 repeat the loop again

end_loops:

    // LDMFD sp!,{x4-x12,x15}                //Reload the registers from sp
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret


//*******************************************************************************
//* @function
//*  ih264_weighted_bi_pred_chroma_av8()
//*
//* @brief
//*  This routine performs the default weighted prediction as described in sec
//* 8.4.2.3.2 titled "Weighted sample prediction process" for chroma.
//*
//* @par Description:
//*  This function gets two ht x wd blocks, calculates the weighted samples,
//* rounds off, adds offset and stores it in the destination block for U and V.
//*
//* @param[in] puc_src1
//*  UWORD8 Pointer to the buffer containing the input block 1.
//*
//* @param[in] puc_src2
//*  UWORD8 Pointer to the buffer containing the input block 2.
//*
//* @param[out] puc_dst
//*  UWORD8 pointer to the destination where the output block is stored.
//*
//* @param[in] src_strd1
//*  Stride of the input buffer 1
//*
//* @param[in] src_strd2
//*  Stride of the input buffer 2
//*
//* @param[in] dst_strd
//*  Stride of the destination buffer
//*
//* @param[in] log_WD
//*  number of bits to be rounded off
//*
//* @param[in] wt1
//*  weights for the weighted prediction in U and V
//*
//* @param[in] wt2
//*  weights for the weighted prediction in U and V
//*
//* @param[in] ofst1
//*  offset 1 used after rounding off for U an dV
//*
//* @param[in] ofst2
//*  offset 2 used after rounding off for U and V
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
//void ih264_weighted_bi_pred_chroma_av8(UWORD8 *puc_src1,
//                                       UWORD8 *puc_src2,
//                                       UWORD8 *puc_dst,
//                                       WORD32 src_strd1,
//                                       WORD32 src_strd2,
//                                       WORD32 dst_strd,
//                                       WORD32 log_WD,
//                                       WORD32 wt1,
//                                       WORD32 wt2,
//                                       WORD32 ofst1,
//                                       WORD32 ofst2,
//                                       WORD32 ht,
//                                       WORD32 wd)
//
//**************Variables Vs Registers*****************************************
//    x0      => puc_src1
//    x1      => puc_src2
//    x2      => puc_dst
//    w3      => src_strd1
//    w4      => src_strd2
//    w5      => dst_strd
//    w6      => log_WD
//    w7      => wt1
//    [sp]    => wt2       (w8)
//    [sp+8]  => ofst1     (w9)
//    [sp+16] => ofst2     (w10)
//    [sp+24] => ht        (w11)
//    [sp+32] => wd        (w12)
//





    .global ih264_weighted_bi_pred_chroma_av8

ih264_weighted_bi_pred_chroma_av8:

    // STMFD sp!, {x4-x12,x14}                //stack stores the values of the arguments
    push_v_regs
    sxtw      x3, w3
    sxtw      x4, w4
    sxtw      x5, w5
    stp       x19, x20, [sp, #-16]!


    ldr       w8, [sp, #80]             //Load wt2 in w8
    dup       v4.4s, w8                 //Q2 = (wt2_u, wt2_v) (32-bit)
    dup       v2.4s, w7                 //Q1 = (wt1_u, wt1_v) (32-bit)
    add       w6, w6, #1                //w6  = log_WD + 1
    ldr       w9, [sp, #88]             //Load ofst1 in w9
    ldr       w10, [sp, #96]            //Load ofst2 in w10
    neg       w20, w6                   //w20 = -(log_WD + 1)
    dup       v0.8h, w20                //Q0  = -(log_WD + 1) (16-bit)
    ldr       w11, [sp, #104]           //Load ht in x11
    ldr       w12, [sp, #112]           //Load wd in x12
    dup       v20.8h, w9                //0ffset1
    dup       v21.8h, w10               //0ffset2
    srhadd    v6.8b, v20.8b, v21.8b
    sxtl      v6.8h, v6.8b
    cmp       w12, #8                   //check if wd is 8
    beq       loop_8_uv                 //branch if wd is 8
    cmp       w12, #4                   //check if wd is 4
    beq       loop_4_uv                 //branch if wd is 4

loop_2_uv:                              //each iteration processes two rows

    ld1       {v8.s}[0], [x0], x3       //load row 1 in source 1
    ld1       {v8.s}[1], [x0], x3       //load row 2 in source 1
    ld1       {v10.s}[0], [x1], x4      //load row 1 in source 2
    ld1       {v10.s}[1], [x1], x4      //load row 2 in source 2
    uxtl      v8.8h, v8.8b              //converting rows 1,2 in source 1 to 16-bit
    uxtl      v10.8h, v10.8b            //converting rows 1,2 in source 2 to 16-bit
    mul       v8.8h, v8.8h , v2.8h      //weight 1 mult. for rows 1,2
    mla       v8.8h, v10.8h , v4.8h     //weight 2 mult. for rows 1,2
    srshl     v8.8h, v8.8h , v0.8h      //rounds off the weighted samples from rows 1,2
    add       v8.8h, v8.8h , v6.8h      //adding offset for rows 1,2
    sqxtun    v8.8b, v8.8h              //saturating rows 1,2 to unsigned 8-bit/
    st1       {v8.s}[0], [x2], x5       //store row 1 in destination
    st1       {v8.s}[1], [x2], x5       //store row 2 in destination
    subs      w11, w11, #2              //decrement ht by 2
    bgt       loop_2_uv                 //if greater than 0 repeat the loop again
    b         end_loops_uv

loop_4_uv:                              //each iteration processes two rows

    ld1       {v8.8b}, [x0], x3         //load row 1 in source 1
    ld1       {v10.8b}, [x1], x4        //load row 1 in source 2
    uxtl      v8.8h, v8.8b              //converting row 1 in source 1 to 16-bit
    ld1       {v12.8b}, [x0], x3        //load row 2 in source 1
    uxtl      v10.8h, v10.8b            //converting row 1 in source 2 to 16-bit
    ld1       {v14.8b}, [x1], x4        //load row 2 in source 2
    uxtl      v12.8h, v12.8b            //converting row 2 in source 1 to 16-bit
    mul       v8.8h, v8.8h , v2.8h      //weight 1 mult. for row 1
    mla       v8.8h, v10.8h , v4.8h     //weight 2 mult. for row 1
    uxtl      v14.8h, v14.8b            //converting row 2 in source 2 to 16-bit
    mul       v12.8h, v12.8h , v2.8h    //weight 1 mult. for row 2
    mla       v12.8h, v14.8h , v4.8h    //weight 2 mult. for row 2
    subs      w11, w11, #2              //decrement ht by 2
    srshl     v8.8h, v8.8h , v0.8h      //rounds off the weighted samples from row 1
    srshl     v12.8h, v12.8h , v0.8h    //rounds off the weighted samples from row 2
    add       v8.8h, v8.8h , v6.8h      //adding offset for row 1
    add       v12.8h, v12.8h , v6.8h    //adding offset for row 2
    sqxtun    v8.8b, v8.8h              //saturating row 1 to unsigned 8-bit
    sqxtun    v12.8b, v12.8h            //saturating row 2 to unsigned 8-bit
    st1       {v8.8b}, [x2], x5         //store row 1 in destination
    st1       {v12.8b}, [x2], x5        //store row 2 in destination
    bgt       loop_4_uv                 //if greater than 0 repeat the loop again
    b         end_loops_uv

loop_8_uv:                              //each iteration processes two rows

    ld1       {v8.8b, v9.8b}, [x0], x3  //load row 1 in source 1
    ld1       {v10.8b, v11.8b}, [x1], x4 //load row 1 in source 2
    ld1       {v12.8b, v13.8b}, [x0], x3 //load row 2 in source 1
    ld1       {v14.8b, v15.8b}, [x1], x4 //load row 2 in source 2
    uxtl      v24.8h, v8.8b             //converting row 1L in source 1 to 16-bit
    ld1       {v16.8b, v17.8b}, [x0], x3 //load row 3 in source 1
    ld1       {v18.8b, v19.8b}, [x1], x4 //load row 3 in source 2
    uxtl      v26.8h, v10.8b            //converting row 1L in source 2 to 16-bit
    ld1       {v20.8b, v21.8b}, [x0], x3 //load row 4 in source 1
    ld1       {v22.8b, v23.8b}, [x1], x4 //load row 4 in source 2
    uxtl      v8.8h, v9.8b              //converting row 1H in source 1 to 16-bit
    uxtl      v10.8h, v11.8b            //converting row 1H in source 2 to 16-bit
    mul       v24.8h, v24.8h , v2.8h    //weight 1 mult. for row 1L
    mla       v24.8h, v26.8h , v4.8h    //weight 2 mult. for row 1L
    uxtl      v28.8h, v12.8b            //converting row 2L in source 1 to 16-bit
    uxtl      v30.8h, v14.8b            //converting row 2L in source 2 to 16-bit
    mul       v8.8h, v8.8h , v2.8h      //weight 1 mult. for row 1H
    mla       v8.8h, v10.8h , v4.8h     //weight 2 mult. for row 1H
    uxtl      v12.8h, v13.8b            //converting row 2H in source 1 to 16-bit
    uxtl      v14.8h, v15.8b            //converting row 2H in source 2 to 16-bit
    mul       v28.8h, v28.8h , v2.8h    //weight 1 mult. for row 2L
    mla       v28.8h, v30.8h , v4.8h    //weight 2 mult. for row 2L
    uxtl      v26.8h, v16.8b            //converting row 3L in source 1 to 16-bit
    uxtl      v10.8h, v18.8b            //converting row 3L in source 2 to 16-bit
    mul       v12.8h, v12.8h , v2.8h    //weight 1 mult. for row 2H
    mla       v12.8h, v14.8h , v4.8h    //weight 2 mult. for row 2H
    uxtl      v16.8h, v17.8b            //converting row 3H in source 1 to 16-bit
    uxtl      v18.8h, v19.8b            //converting row 3H in source 2 to 16-bit
    mul       v26.8h, v26.8h , v2.8h    //weight 1 mult. for row 3L
    mla       v26.8h, v10.8h , v4.8h    //weight 2 mult. for row 3L
    uxtl      v30.8h, v20.8b            //converting row 4L in source 1 to 16-bit
    uxtl      v14.8h, v22.8b            //converting row 4L in source 2 to 16-bit
    mul       v16.8h, v16.8h , v2.8h    //weight 1 mult. for row 3H
    mla       v16.8h, v18.8h , v4.8h    //weight 2 mult. for row 3H
    uxtl      v20.8h, v21.8b            //converting row 4H in source 1 to 16-bit
    uxtl      v22.8h, v23.8b            //converting row 4H in source 2 to 16-bit
    mul       v30.8h, v30.8h , v2.8h    //weight 1 mult. for row 4L
    mla       v30.8h, v14.8h , v4.8h    //weight 2 mult. for row 4L
    srshl     v24.8h, v24.8h , v0.8h    //rounds off the weighted samples from row 1L
    mul       v20.8h, v20.8h , v2.8h    //weight 1 mult. for row 4H
    mla       v20.8h, v22.8h , v4.8h    //weight 2 mult. for row 4H
    srshl     v8.8h, v8.8h , v0.8h      //rounds off the weighted samples from row 1H
    srshl     v28.8h, v28.8h , v0.8h    //rounds off the weighted samples from row 2L
    add       v24.8h, v24.8h , v6.8h    //adding offset for row 1L
    srshl     v12.8h, v12.8h , v0.8h    //rounds off the weighted samples from row 2H
    add       v8.8h, v8.8h , v6.8h      //adding offset for row 1H
    srshl     v26.8h, v26.8h , v0.8h    //rounds off the weighted samples from row 3L
    add       v28.8h, v28.8h , v6.8h    //adding offset for row 2L
    srshl     v16.8h, v16.8h , v0.8h    //rounds off the weighted samples from row 3H
    add       v12.8h, v12.8h , v6.8h    //adding offset for row 2H
    srshl     v30.8h, v30.8h , v0.8h    //rounds off the weighted samples from row 4L
    add       v26.8h, v26.8h , v6.8h    //adding offset for row 3L
    srshl     v20.8h, v20.8h , v0.8h    //rounds off the weighted samples from row 4H
    add       v16.8h, v16.8h , v6.8h    //adding offset for row 3H
    sqxtun    v10.8b, v24.8h            //saturating row 1L to unsigned 8-bit
    add       v30.8h, v30.8h , v6.8h    //adding offset for row 4L
    sqxtun    v11.8b, v8.8h             //saturating row 1H to unsigned 8-bit
    add       v20.8h, v20.8h , v6.8h    //adding offset for row 4H
    sqxtun    v18.8b, v28.8h            //saturating row 2L to unsigned 8-bit
    sqxtun    v19.8b, v12.8h            //saturating row 2H to unsigned 8-bit
    sqxtun    v14.8b, v26.8h            //saturating row 3L to unsigned 8-bit
    sqxtun    v15.8b, v16.8h            //saturating row 3H to unsigned 8-bit
    st1       {v10.8b, v11.8b}, [x2], x5 //store row 1 in destination
    sqxtun    v22.8b, v30.8h            //saturating row 4L to unsigned 8-bit
    sqxtun    v23.8b, v20.8h            //saturating row 4H to unsigned 8-bit
    st1       {v18.8b, v19.8b}, [x2], x5 //store row 2 in destination
    subs      w11, w11, #4              //decrement ht by 4
    st1       {v14.8b, v15.8b}, [x2], x5 //store row 3 in destination
    st1       {v22.8b, v23.8b}, [x2], x5 //store row 4 in destination
    bgt       loop_8_uv                 //if greater than 0 repeat the loop again

end_loops_uv:

    // LDMFD sp!,{x4-x12,x15}                //Reload the registers from sp
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret



