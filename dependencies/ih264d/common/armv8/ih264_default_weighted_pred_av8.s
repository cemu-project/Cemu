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
//*  ih264_default_weighted_pred_av8.s
//*
//* @brief
//*  Contains function definitions for default weighted prediction.
//*
//* @author
//*  Kaushik Senthoor R
//*
//* @par List of Functions:
//*
//*  - ih264_default_weighted_pred_luma_av8()
//*  - ih264_default_weighted_pred_chroma_av8()
//*
//* @remarks
//*  None
//*
//*******************************************************************************
//*/
//*******************************************************************************
//* @function
//*  ih264_default_weighted_pred_luma_av8()
//*
//* @brief
//*  This routine performs the default weighted prediction as described in sec
//* 8.4.2.3.1 titled "Default weighted sample prediction process" for luma.
//*
//* @par Description:
//*  This function gets two ht x wd blocks, calculates their rounded-average and
//* stores it in the destination block.
//*
//* @param[in] puc_src1:
//*  UWORD8 Pointer to the buffer containing the first input block.
//*
//* @param[in] puc_src2:
//*  UWORD8 Pointer to the buffer containing the second input block.
//*
//* @param[out] puc_dst
//*  UWORD8 pointer to the destination where the output block is stored.
//*
//* @param[in] src_strd1
//*  Stride of the first input buffer
//*
//* @param[in] src_strd2
//*  Stride of the second input buffer
//*
//* @param[in] dst_strd
//*  Stride of the destination buffer
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
//void ih264_default_weighted_pred_luma_av8(UWORD8 *puc_src1,
//                                          UWORD8 *puc_src2,
//                                          UWORD8 *puc_dst,
//                                          WORD32 src_strd1,
//                                          WORD32 src_strd2,
//                                          WORD32 dst_strd,
//                                          WORD32 ht,
//                                          WORD32 wd)
//
//**************Variables Vs Registers*****************************************
//    x0      => puc_src1
//    x1      => puc_src2
//    x2      => puc_dst
//    w3      => src_strd1
//    w4      => src_strd2
//    w5      => dst_strd
//    w6      => ht
//    w7      => wd
//
.text
.p2align 2
.include "ih264_neon_macros.s"



    .global ih264_default_weighted_pred_luma_av8

ih264_default_weighted_pred_luma_av8:

    push_v_regs
    stp       x19, x20, [sp, #-16]!
    sxtw      x3, w3
    sxtw      x4, w4
    sxtw      x5, w5
    cmp       w7, #16
    beq       loop_16                   //branch if wd is 16
    cmp       w7, #8
    beq       loop_8                    //branch if wd is 8

loop_4:                                 //each iteration processes four rows

    ld1       {v0.s}[0], [x0], x3       //load row 1 in source 1
    ld1       {v0.s}[1], [x0], x3       //load row 2 in source 1
    ld1       {v2.s}[0], [x1], x4       //load row 1 in source 2
    ld1       {v2.s}[1], [x1], x4       //load row 2 in source 2
    ld1       {v1.s}[0], [x0], x3       //load row 3 in source 1
    ld1       {v1.s}[1], [x0], x3       //load row 4 in source 1
    urhadd    v0.8b, v0.8b , v2.8b
    ld1       {v3.s}[0], [x1], x4       //load row 3 in source 2
    ld1       {v3.s}[1], [x1], x4       //load row 4 in source 2
    subs      w6, w6, #4                //decrement ht by 4
    st1       {v0.s}[0], [x2], x5       //load row 1 in destination
    st1       {v0.s}[1], [x2], x5       //load row 2 in destination
    urhadd    v1.8b, v1.8b , v3.8b
    st1       {v1.s}[0], [x2], x5       //load row 3 in destination
    st1       {v1.s}[1], [x2], x5       //load row 4 in destination
    bgt       loop_4                    //if greater than 0 repeat the loop again
    b         end_loops

loop_8:                                 //each iteration processes four rows

    ld1       {v0.8b}, [x0], x3         //load row 1 in source 1
    ld1       {v4.8b}, [x1], x4         //load row 1 in source 2
    ld1       {v1.8b}, [x0], x3         //load row 2 in source 1
    ld1       {v5.8b}, [x1], x4         //load row 2 in source 2
    ld1       {v2.8b}, [x0], x3         //load row 3 in source 1
    urhadd    v0.16b, v0.16b , v4.16b
    urhadd    v1.16b, v1.16b , v5.16b
    ld1       {v6.8b}, [x1], x4         //load row 3 in source 2
    ld1       {v3.8b}, [x0], x3         //load row 4 in source 1
    urhadd    v2.8b, v2.8b , v6.8b
    ld1       {v7.8b}, [x1], x4         //load row 4 in source 2
    subs      w6, w6, #4                //decrement ht by 4
    st1       {v0.8b}, [x2], x5         //load row 1 in destination
    urhadd    v3.8b, v3.8b , v7.8b
    st1       {v1.8b}, [x2], x5         //load row 2 in destination
    st1       {v2.8b}, [x2], x5         //load row 3 in destination
    st1       {v3.8b}, [x2], x5         //load row 4 in destination
    bgt       loop_8                    //if greater than 0 repeat the loop again
    b         end_loops

loop_16:                                //each iteration processes eight rows

    ld1       {v0.8b, v1.8b}, [x0], x3  //load row 1 in source 1
    ld1       {v16.8b, v17.8b}, [x1], x4 //load row 1 in source 2
    ld1       {v2.8b, v3.8b}, [x0], x3  //load row 2 in source 1
    ld1       {v18.8b, v19.8b}, [x1], x4 //load row 2 in source 2
    urhadd    v0.16b, v0.16b , v16.16b
    urhadd    v1.16b, v1.16b , v17.16b
    ld1       {v4.8b, v5.8b}, [x0], x3  //load row 3 in source 1
    ld1       {v20.8b, v21.8b}, [x1], x4 //load row 3 in source 2
    urhadd    v2.16b, v2.16b , v18.16b
    urhadd    v3.16b, v3.16b , v19.16b
    ld1       {v6.8b, v7.8b}, [x0], x3  //load row 4 in source 1
    ld1       {v22.8b, v23.8b}, [x1], x4 //load row 4 in source 2
    urhadd    v4.16b, v4.16b , v20.16b
    urhadd    v5.16b, v5.16b , v21.16b
    ld1       {v8.8b, v9.8b}, [x0], x3  //load row 5 in source 1
    ld1       {v24.8b, v25.8b}, [x1], x4 //load row 5 in source 2
    urhadd    v6.16b, v6.16b , v22.16b
    urhadd    v7.16b, v7.16b , v23.16b
    ld1       {v10.8b, v11.8b}, [x0], x3 //load row 6 in source 1
    ld1       {v26.8b, v27.8b}, [x1], x4 //load row 6 in source 2
    urhadd    v8.16b, v8.16b , v24.16b
    urhadd    v9.16b, v9.16b , v25.16b
    ld1       {v12.8b, v13.8b}, [x0], x3 //load row 7 in source 1
    ld1       {v28.8b, v29.8b}, [x1], x4 //load row 7 in source 2
    urhadd    v10.16b, v10.16b , v26.16b
    urhadd    v11.16b, v11.16b , v27.16b
    ld1       {v14.8b, v15.8b}, [x0], x3 //load row 8 in source 1
    ld1       {v30.8b, v31.8b}, [x1], x4 //load row 8 in source 2
    urhadd    v12.16b, v12.16b , v28.16b
    urhadd    v13.16b, v13.16b , v29.16b
    st1       {v0.8b, v1.8b}, [x2], x5  //load row 1 in destination
    st1       {v2.8b, v3.8b}, [x2], x5  //load row 2 in destination
    urhadd    v14.16b, v14.16b , v30.16b
    urhadd    v15.16b, v15.16b , v31.16b
    st1       {v4.8b, v5.8b}, [x2], x5  //load row 3 in destination
    st1       {v6.8b, v7.8b}, [x2], x5  //load row 4 in destination
    subs      w6, w6, #8                //decrement ht by 8
    st1       {v8.8b, v9.8b}, [x2], x5  //load row 5 in destination
    st1       {v10.8b, v11.8b}, [x2], x5 //load row 6 in destination
    st1       {v12.8b, v13.8b}, [x2], x5 //load row 7 in destination
    st1       {v14.8b, v15.8b}, [x2], x5 //load row 8 in destination
    bgt       loop_16                   //if greater than 0 repeat the loop again

end_loops:

    // LDMFD sp!,{x4-x7,x15}                      //Reload the registers from sp
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret


//*******************************************************************************
//* @function
//*  ih264_default_weighted_pred_chroma_av8()
//*
//* @brief
//*  This routine performs the default weighted prediction as described in sec
//* 8.4.2.3.1 titled "Default weighted sample prediction process" for chroma.
//*
//* @par Description:
//*  This function gets two ht x wd blocks, calculates their rounded-average and
//* stores it in the destination block for U and V.
//*
//* @param[in] puc_src1:
//*  UWORD8 Pointer to the buffer containing the first input block.
//*
//* @param[in] puc_src2:
//*  UWORD8 Pointer to the buffer containing the second input block.
//*
//* @param[out] puc_dst
//*  UWORD8 pointer to the destination where the output block is stored.
//*
//* @param[in] src_strd1
//*  Stride of the first input buffer
//*
//* @param[in] src_strd2
//*  Stride of the second input buffer
//*
//* @param[in] dst_strd
//*  Stride of the destination buffer
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
//void ih264_default_weighted_pred_chroma_av8(UWORD8 *puc_src1,
//                                            UWORD8 *puc_src2,
//                                            UWORD8 *puc_dst,
//                                            WORD32 src_strd1,
//                                            WORD32 src_strd2,
//                                            WORD32 dst_strd,
//                                            WORD32 ht,
//                                            WORD32 wd)
//
//**************Variables Vs Registers*****************************************
//    x0      => puc_src1
//    x1      => puc_src2
//    x2      => puc_dst
//    w3      => src_strd1
//    w4      => src_strd2
//    w5      => dst_strd
//    w6      => ht
//    w7      => wd
//




    .global ih264_default_weighted_pred_chroma_av8

ih264_default_weighted_pred_chroma_av8:

    push_v_regs
    stp       x19, x20, [sp, #-16]!
    sxtw      x3, w3
    sxtw      x4, w4
    sxtw      x5, w5
    cmp       w7, #8
    beq       loop_8_uv                 //branch if wd is 8
    cmp       w7, #4
    beq       loop_4_uv                 //branch if wd is 4

loop_2_uv:                              //each iteration processes two rows

    ld1       {v0.s}[0], [x0], x3       //load row 1 in source 1
    ld1       {v0.s}[1], [x0], x3       //load row 2 in source 1
    ld1       {v1.s}[0], [x1], x4       //load row 1 in source 2
    ld1       {v1.s}[1], [x1], x4       //load row 2 in source 2
    urhadd    v0.8b, v0.8b , v1.8b
    subs      w6, w6, #2                //decrement ht by 2
    st1       {v0.s}[0], [x2], x5       //load row 1 in destination
    st1       {v0.s}[1], [x2], x5       //load row 2 in destination
    bgt       loop_2_uv                 //if greater than 0 repeat the loop again
    b         end_loops_uv

loop_4_uv:                              //each iteration processes two rows

    ld1       {v0.8b}, [x0], x3         //load row 1 in source 1
    ld1       {v2.8b}, [x1], x4         //load row 1 in source 2
    ld1       {v1.8b}, [x0], x3         //load row 2 in source 1
    urhadd    v0.8b, v0.8b , v2.8b
    ld1       {v3.8b}, [x1], x4         //load row 2 in source 2
    urhadd    v1.8b, v1.8b , v3.8b
    st1       {v0.8b}, [x2], x5         //load row 1 in destination
    subs      w6, w6, #2                //decrement ht by 2
    st1       {v1.8b}, [x2], x5         //load row 2 in destination
    bgt       loop_4_uv                 //if greater than 0 repeat the loop again
    b         end_loops_uv

loop_8_uv:                              //each iteration processes four rows

    ld1       {v0.8b, v1.8b}, [x0], x3  //load row 1 in source 1
    ld1       {v8.8b, v9.8b}, [x1], x4  //load row 1 in source 2
    ld1       {v2.8b, v3.8b}, [x0], x3  //load row 2 in source 1
    urhadd    v0.16b, v0.16b , v8.16b
    urhadd    v1.16b, v1.16b , v9.16b
    ld1       {v10.8b, v11.8b}, [x1], x4 //load row 2 in source 2
    ld1       {v4.8b, v5.8b}, [x0], x3  //load row 3 in source 1
    urhadd    v2.16b, v2.16b , v10.16b
    urhadd    v3.16b, v3.16b , v11.16b
    ld1       {v12.8b, v13.8b}, [x1], x4 //load row 3 in source 2
    ld1       {v6.8b, v7.8b}, [x0], x3  //load row 4 in source 1
    urhadd    v4.16b, v4.16b , v12.16b
    urhadd    v5.16b, v5.16b , v13.16b
    ld1       {v14.8b, v15.8b}, [x1], x4 //load row 4 in source 2
    st1       {v0.8b, v1.8b}, [x2], x5  //load row 1 in destination
    urhadd    v6.16b, v6.16b , v14.16b
    urhadd    v7.16b, v7.16b , v15.16b
    st1       {v2.8b, v3.8b}, [x2], x5  //load row 2 in destination
    subs      w6, w6, #4                //decrement ht by 4
    st1       {v4.8b, v5.8b}, [x2], x5  //load row 3 in destination
    st1       {v6.8b, v7.8b}, [x2], x5  //load row 4 in destination
    bgt       loop_8_uv                 //if greater than 0 repeat the loop again

end_loops_uv:
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret



