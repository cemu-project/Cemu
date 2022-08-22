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
//*  ih264_intra_pred_luma_4x4_av8.s
//*
//* @brief
//*  Contains function definitions for intra 4x4 Luma prediction .
//*
//* @author
//*  Ittiam
//*
//* @par List of Functions:
//*
//*  -ih264_intra_pred_luma_4x4_mode_vert_av8
//*  -ih264_intra_pred_luma_4x4_mode_horz_av8
//*  -ih264_intra_pred_luma_4x4_mode_dc_av8
//*  -ih264_intra_pred_luma_4x4_mode_diag_dl_av8
//*  -ih264_intra_pred_luma_4x4_mode_diag_dr_av8
//*  -ih264_intra_pred_luma_4x4_mode_vert_r_av8
//*  -ih264_intra_pred_luma_4x4_mode_horz_d_av8
//*  -ih264_intra_pred_luma_4x4_mode_vert_l_av8
//*  -ih264_intra_pred_luma_4x4_mode_horz_u_av8
//*
//* @remarks
//*  None
//*
//*******************************************************************************
//*/

///* All the functions here are replicated from ih264_intra_pred_filters.c
//

///**
///**
///**
//

.text
.p2align 2
.include "ih264_neon_macros.s"




///**
//*******************************************************************************
//*
//*ih264_intra_pred_luma_4x4_mode_vert
//*
//* @brief
//*  Perform Intra prediction for  luma_4x4 mode:vertical
//*
//* @par Description:
//* Perform Intra prediction for  luma_4x4 mode:vertical ,described in sec 8.3.1.2.1
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
//* @param[in] ui_neighboravailability
//* availability of neighbouring pixels(Not used in this function)
//*
//* @returns
//*
//* @remarks
//*  None
//*
//*******************************************************************************
//void ih264_intra_pred_luma_4x4_mode_vert(UWORD8 *pu1_src,
//                                        UWORD8 *pu1_dst,
//                                        WORD32 src_strd,
//                                        WORD32 dst_strd,
//                                        WORD32 ui_neighboravailability)

//**************Variables Vs Registers*****************************************
//    x0 => *pu1_src
//    x1 => *pu1_dst
//    w2 =>  src_strd
//    w3 =>  dst_strd
//    w4 =>  ui_neighboravailability

    .global ih264_intra_pred_luma_4x4_mode_vert_av8

ih264_intra_pred_luma_4x4_mode_vert_av8:

    push_v_regs
    sxtw      x3, w3

    add       x0, x0, #5

    ld1       {v0.s}[0], [x0]
    st1       {v0.s}[0], [x1], x3
    st1       {v0.s}[0], [x1], x3
    st1       {v0.s}[0], [x1], x3
    st1       {v0.s}[0], [x1], x3

    pop_v_regs
    ret





///******************************************************************************


///**
//*******************************************************************************
//*
//*ih264_intra_pred_luma_4x4_mode_horz
//*
//* @brief
//*  Perform Intra prediction for  luma_4x4 mode:horizontal
//*
//* @par Description:
//*  Perform Intra prediction for  luma_4x4 mode:horizontal ,described in sec 8.3.1.2.2
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
//* @param[in] ui_neighboravailability
//* availability of neighbouring pixels(Not used in this function)
//*
//* @returns
//*
//* @remarks
//*  None
//*
//*******************************************************************************
//*/
//void ih264_intra_pred_luma_4x4_mode_horz(UWORD8 *pu1_src,
//                                         UWORD8 *pu1_dst,
//                                         WORD32 src_strd,
//                                         WORD32 dst_strd,
//                                         WORD32 ui_neighboravailability)
//**************Variables Vs Registers*****************************************
//    x0 => *pu1_src
//    x1 => *pu1_dst
//    w2 =>  src_strd
//    w3 =>  dst_strd
//    w4 =>  ui_neighboravailability



    .global ih264_intra_pred_luma_4x4_mode_horz_av8

ih264_intra_pred_luma_4x4_mode_horz_av8:

    push_v_regs
    sxtw      x3, w3

    ld1       {v1.s}[0], [x0]
    dup       v0.8b, v1.b[3]
    dup       v2.8b, v1.b[2]
    st1       {v0.s}[0], [x1], x3
    dup       v3.8b, v1.b[1]
    st1       {v2.s}[0], [x1], x3
    dup       v4.8b, v1.b[0]
    st1       {v3.s}[0], [x1], x3
    st1       {v4.s}[0], [x1], x3

    pop_v_regs
    ret







///******************************************************************************


///**
//*******************************************************************************
//*
//*ih264_intra_pred_luma_4x4_mode_dc
//*
//* @brief
//*  Perform Intra prediction for  luma_4x4 mode:DC
//*
//* @par Description:
//*  Perform Intra prediction for  luma_4x4 mode:DC ,described in sec 8.3.1.2.3
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
//* @param[in] ui_neighboravailability
//*  availability of neighbouring pixels
//*
//* @returns
//*
//* @remarks
//*  None
//*
//*******************************************************************************/
//void ih264_intra_pred_luma_4x4_mode_dc(UWORD8 *pu1_src,
//                                       UWORD8 *pu1_dst,
//                                       WORD32 src_strd,
//                                       WORD32 dst_strd,
//                                       WORD32 ui_neighboravailability)

//**************Variables Vs Registers*****************************************
//    x0 => *pu1_src
//    x1 => *pu1_dst
//    w2 =>  src_strd
//    w3 =>  dst_strd
//    w4 =>  ui_neighboravailability



    .global ih264_intra_pred_luma_4x4_mode_dc_av8

ih264_intra_pred_luma_4x4_mode_dc_av8:




    push_v_regs
    stp       x19, x20, [sp, #-16]!
    sxtw      x3, w3

    ands      w5, w4, #0x01
    beq       top_available             //LEFT NOT AVAILABLE

    add       x10, x0, #3
    mov       x2, #-1
    ldrb      w5, [x10], #-1
    ldrb      w6, [x10], #-1
    ldrb      w7, [x10], #-1
    add       w5, w5, w6
    ldrb      w8, [x10], #-1
    add       w5, w5, w7
    ands      w11, w4, #0x04            // CHECKING IF TOP_AVAILABLE  ELSE BRANCHING TO ONLY LEFT AVAILABLE
    add       w5, w5, w8
    beq       left_available
    add       x10, x0, #5
    //    BOTH LEFT AND TOP AVAILABLE
    ldrb      w6, [x10], #1
    ldrb      w7, [x10], #1
    add       w5, w5, w6
    ldrb      w8, [x10], #1
    add       w5, w5, w7
    ldrb      w9, [x10], #1
    add       w5, w5, w8
    add       w5, w5, w9
    add       w5, w5, #4
    lsr       w5, w5, #3
    dup       v0.8b, w5
    st1       {v0.s}[0], [x1], x3
    st1       {v0.s}[0], [x1], x3
    st1       {v0.s}[0], [x1], x3
    st1       {v0.s}[0], [x1], x3
    b         end_func

top_available: // ONLT TOP AVAILABLE
    ands      w11, w4, #0x04            // CHECKING TOP AVAILABILTY  OR ELSE BRANCH TO NONE AVAILABLE
    beq       none_available

    add       x10, x0, #5
    ldrb      w6, [x10], #1
    ldrb      w7, [x10], #1
    ldrb      w8, [x10], #1
    add       w5, w6, w7
    ldrb      w9, [x10], #1
    add       w5, w5, w8
    add       w5, w5, w9
    add       w5, w5, #2
    lsr       w5, w5, #2
    dup       v0.8b, w5
    st1       {v0.s}[0], [x1], x3
    st1       {v0.s}[0], [x1], x3
    st1       {v0.s}[0], [x1], x3
    st1       {v0.s}[0], [x1], x3
    b         end_func

left_available: //ONLY LEFT AVAILABLE
    add       x5, x5, #2
    lsr       x5, x5, #2
    dup       v0.8b, w5
    st1       {v0.s}[0], [x1], x3
    st1       {v0.s}[0], [x1], x3
    st1       {v0.s}[0], [x1], x3
    st1       {v0.s}[0], [x1], x3
    b         end_func

none_available:                         //NONE AVAILABLE
    mov       x5, #128
    dup       v0.8b, w5
    st1       {v0.s}[0], [x1], x3
    st1       {v0.s}[0], [x1], x3
    st1       {v0.s}[0], [x1], x3
    st1       {v0.s}[0], [x1], x3
    b         end_func


end_func:

    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret







///**
//*******************************************************************************
//*
//*ih264_intra_pred_luma_4x4_mode_diag_dl
//*
//* @brief
//*  Perform Intra prediction for  luma_4x4 mode:Diagonal_Down_Left
//*
//* @par Description:
//*  Perform Intra prediction for  luma_4x4 mode:Diagonal_Down_Left ,described in sec 8.3.1.2.4
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
//* @param[in] ui_neighboravailability
//*  availability of neighbouring pixels
//*
//* @returns
//*
//* @remarks
//*  None
//*
//*******************************************************************************/
//void ih264_intra_pred_luma_4x4_mode_diag_dl(UWORD8 *pu1_src,
//                                            UWORD8 *pu1_dst,
//                                            WORD32 src_strd,
//                                              WORD32 dst_strd,
//                                              WORD32 ui_neighboravailability)

//**************Variables Vs Registers*****************************************
//    x0 => *pu1_src
//    x1 => *pu1_dst
//    w2 =>  src_strd
//    w3 =>  dst_strd
//    w4 =>  ui_neighboravailability


    .global ih264_intra_pred_luma_4x4_mode_diag_dl_av8

ih264_intra_pred_luma_4x4_mode_diag_dl_av8:


    push_v_regs
    stp       x19, x20, [sp, #-16]!
    sxtw      x3, w3

    add       x0, x0, #5
    sub       x5, x3, #2
    add       x6, x0, #7
    ld1       {v0.8b}, [x0]
    ext       v1.8b, v0.8b , v0.8b , #1
    ext       v2.8b, v0.8b , v0.8b , #2
    ld1       {v2.b}[6], [x6]
    uaddl     v20.8h, v0.8b, v1.8b
    uaddl     v22.8h, v1.8b, v2.8b
    add       v24.8h, v20.8h , v22.8h
    sqrshrun  v3.8b, v24.8h, #2
    st1       {v3.s}[0], [x1], x3
    ext       v4.8b, v3.8b , v3.8b , #1
    st1       {v4.s}[0], [x1], x3
    st1       {v3.h}[1], [x1], #2
    st1       {v3.h}[2], [x1], x5
    st1       {v4.h}[1], [x1], #2
    st1       {v4.h}[2], [x1]

end_func_diag_dl:

    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret









///**
//*******************************************************************************
//*
//*ih264_intra_pred_luma_4x4_mode_diag_dr
//*
//* @brief
//* Perform Intra prediction for  luma_4x4 mode:Diagonal_Down_Right
//*
//* @par Description:
//*  Perform Intra prediction for  luma_4x4 mode:Diagonal_Down_Right ,described in sec 8.3.1.2.5
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
//* @param[in] ui_neighboravailability
//*  availability of neighbouring pixels
//*
//* @returns
//*
//* @remarks
//*  None
//*
//*******************************************************************************/
//void ih264_intra_pred_luma_4x4_mode_diag_dr(UWORD8 *pu1_src,
//                                            UWORD8 *pu1_dst,
//                                            WORD32 src_strd,
//                                              WORD32 dst_strd,
//                                              WORD32 ui_neighboravailability)

//**************Variables Vs Registers*****************************************
//    x0 => *pu1_src
//    x1 => *pu1_dst
//    w2 =>  src_strd
//    w3 =>  dst_strd
//    w4 =>  ui_neighboravailability


    .global ih264_intra_pred_luma_4x4_mode_diag_dr_av8

ih264_intra_pred_luma_4x4_mode_diag_dr_av8:

    push_v_regs
    stp       x19, x20, [sp, #-16]!
    sxtw      x3, w3


    ld1       {v0.8b}, [x0]
    add       x0, x0, #1
    ld1       {v1.8b}, [x0]
    ext       v2.8b, v1.8b , v1.8b , #1
    uaddl     v20.8h, v0.8b, v1.8b
    uaddl     v22.8h, v1.8b, v2.8b
    add       v24.8h, v20.8h , v22.8h
    sqrshrun  v3.8b, v24.8h, #2

    ext       v4.8b, v3.8b , v3.8b , #1
    sub       x5, x3, #2
    st1       {v4.h}[1], [x1], #2
    st1       {v4.h}[2], [x1], x5
    st1       {v3.h}[1], [x1], #2
    st1       {v3.h}[2], [x1], x5
    st1       {v4.s}[0], [x1], x3
    st1       {v3.s}[0], [x1], x3

end_func_diag_dr:
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret







///**
//*******************************************************************************
//*
//*ih264_intra_pred_luma_4x4_mode_vert_r
//*
//* @brief
//* Perform Intra prediction for  luma_4x4 mode:Vertical_Right
//*
//* @par Description:
//*   Perform Intra prediction for  luma_4x4 mode:Vertical_Right ,described in sec 8.3.1.2.6
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
//* @param[in] ui_neighboravailability
//*  availability of neighbouring pixels
//*
//* @returns
//*
//* @remarks
//*  None
//*
//*******************************************************************************/
//void ih264_intra_pred_luma_4x4_mode_vert_r(UWORD8 *pu1_src,
//                                            UWORD8 *pu1_dst,
//                                            WORD32 src_strd,
//                                              WORD32 dst_strd,
//                                              WORD32 ui_neighboravailability)

//**************Variables Vs Registers*****************************************
//    x0 => *pu1_src
//    x1 => *pu1_dst
//    w2 =>  src_strd
//    w3 =>  dst_strd
//    w4 =>  ui_neighboravailability


    .global ih264_intra_pred_luma_4x4_mode_vert_r_av8

ih264_intra_pred_luma_4x4_mode_vert_r_av8:

    push_v_regs
    stp       x19, x20, [sp, #-16]!
    sxtw      x3, w3


    ld1       {v0.8b}, [x0]
    add       x0, x0, #1
    ld1       {v1.8b}, [x0]
    ext       v2.8b, v1.8b , v1.8b , #1
    uaddl     v20.8h, v0.8b, v1.8b
    uaddl     v22.8h, v1.8b, v2.8b
    add       v24.8h, v20.8h , v22.8h
    sqrshrun  v4.8b, v20.8h, #1
    sqrshrun  v3.8b, v24.8h, #2
    sub       x5, x3, #2
    ext       v5.8b, v3.8b , v3.8b , #3
    st1       {v4.s}[1], [x1], x3
    st1       {v5.s}[0], [x1], x3
    sub       x8, x3, #3
    st1       {v3.b}[2], [x1], #1
    st1       {v4.h}[2], [x1], #2
    st1       {v4.b}[6], [x1], x8
    st1       {v3.b}[1], [x1], #1
    st1       {v5.h}[0], [x1], #2
    st1       {v5.b}[2], [x1]


end_func_vert_r:
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret





///**
//*******************************************************************************
//*
//*ih264_intra_pred_luma_4x4_mode_horz_d
//*
//* @brief
//* Perform Intra prediction for  luma_4x4 mode:Horizontal_Down
//*
//* @par Description:
//*   Perform Intra prediction for  luma_4x4 mode:Horizontal_Down ,described in sec 8.3.1.2.7
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
//* @param[in] ui_neighboravailability
//*  availability of neighbouring pixels
//*
//* @returns
//*
//* @remarks
//*  None
//*
//*******************************************************************************/
//void ih264_intra_pred_luma_4x4_mode_horz_d(UWORD8 *pu1_src,
//                                            UWORD8 *pu1_dst,
//                                            WORD32 src_strd,
//                                              WORD32 dst_strd,
//                                              WORD32 ui_neighboravailability)

//**************Variables Vs Registers*****************************************
//    x0 => *pu1_src
//    x1 => *pu1_dst
//    w2 =>  src_strd
//    w3 =>  dst_strd
//    w4 =>  ui_neighboravailability


    .global ih264_intra_pred_luma_4x4_mode_horz_d_av8

ih264_intra_pred_luma_4x4_mode_horz_d_av8:

    push_v_regs
    stp       x19, x20, [sp, #-16]!
    sxtw      x3, w3

    ld1       {v0.8b}, [x0]
    add       x0, x0, #1
    ld1       {v1.8b}, [x0]
    ext       v2.8b, v1.8b , v0.8b , #1
    uaddl     v20.8h, v0.8b, v1.8b
    uaddl     v22.8h, v1.8b, v2.8b
    add       v24.8h, v20.8h , v22.8h
    sqrshrun  v4.8b, v20.8h, #1
    sqrshrun  v5.8b, v24.8h, #2
    sub       x5, x3, #2
    mov       v6.8b, v5.8b
    trn1      v10.8b, v4.8b, v5.8b
    trn2      v5.8b, v4.8b, v5.8b       //
    mov       v4.8b, v10.8b
    st1       {v5.h}[1], [x1], #2
    st1       {v6.h}[2], [x1], x5
    st1       {v4.h}[1], [x1], #2
    st1       {v5.h}[1], [x1], x5
    st1       {v5.h}[0], [x1], #2
    st1       {v4.h}[1], [x1], x5
    st1       {v4.h}[0], [x1], #2
    st1       {v5.h}[0], [x1], x5

end_func_horz_d:
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret







///**
//*******************************************************************************
//*
//*ih264_intra_pred_luma_4x4_mode_vert_l
//*
//* @brief
//*  Perform Intra prediction for  luma_4x4 mode:Vertical_Left
//*
//* @par Description:
//*   Perform Intra prediction for  luma_4x4 mode:Vertical_Left ,described in sec 8.3.1.2.8
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
//* @param[in] ui_neighboravailability
//*  availability of neighbouring pixels
//*
//* @returns
//*
//* @remarks
//*  None
//*
//*******************************************************************************/
//void ih264_intra_pred_luma_4x4_mode_vert_l(UWORD8 *pu1_src,
//                                            UWORD8 *pu1_dst,
//                                            WORD32 src_strd,
//                                              WORD32 dst_strd,
//                                              WORD32 ui_neighboravailability)

//**************Variables Vs Registers*****************************************
//    x0 => *pu1_src
//    x1 => *pu1_dst
//    w2 =>  src_strd
//    w3 =>  dst_strd
//    w4 =>  ui_neighboravailability


    .global ih264_intra_pred_luma_4x4_mode_vert_l_av8

ih264_intra_pred_luma_4x4_mode_vert_l_av8:

    push_v_regs
    stp       x19, x20, [sp, #-16]!
    sxtw      x3, w3
    add       x0, x0, #4
    ld1       {v0.8b}, [x0]
    add       x0, x0, #1
    ld1       {v1.8b}, [x0]
    ext       v2.8b, v1.8b , v0.8b , #1
    uaddl     v20.8h, v0.8b, v1.8b
    uaddl     v22.8h, v1.8b, v2.8b
    add       v24.8h, v20.8h , v22.8h
    sqrshrun  v4.8b, v20.8h, #1
    sqrshrun  v5.8b, v24.8h, #2
    ext       v6.8b, v4.8b , v4.8b , #1
    ext       v7.8b, v5.8b , v5.8b , #1
    st1       {v6.s}[0], [x1], x3
    ext       v8.8b, v4.8b , v4.8b , #2
    ext       v9.8b, v5.8b , v5.8b , #2
    st1       {v7.s}[0], [x1], x3
    st1       {v8.s}[0], [x1], x3
    st1       {v9.s}[0], [x1], x3

end_func_vert_l:
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret







///**
//*******************************************************************************
//*
//*ih264_intra_pred_luma_4x4_mode_horz_u
//*
//* @brief
//*     Perform Intra prediction for  luma_4x4 mode:Horizontal_Up
//*
//* @par Description:
//*      Perform Intra prediction for  luma_4x4 mode:Horizontal_Up ,described in sec 8.3.1.2.9
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
//* @param[in] ui_neighboravailability
//*  availability of neighbouring pixels
//*
//* @returns
//*
//* @remarks
//*  None
//*
//*******************************************************************************/
//void ih264_intra_pred_luma_4x4_mode_horz_u(UWORD8 *pu1_src,
//                                           UWORD8 *pu1_dst,
//                                           WORD32 src_strd,
//                                             WORD32 dst_strd,
//                                             WORD32 ui_neighboravailability)

//**************Variables Vs Registers*****************************************
//    x0 => *pu1_src
//    x1 => *pu1_dst
//    w2 =>  src_strd
//    w3 =>  dst_strd
//    w4 =>  ui_neighboravailability


    .global ih264_intra_pred_luma_4x4_mode_horz_u_av8

ih264_intra_pred_luma_4x4_mode_horz_u_av8:

    push_v_regs
    sxtw      x3, w3
    stp       x19, x20, [sp, #-16]!
    mov       x10, x0
    ld1       {v0.8b}, [x0]
    ldrb      w9, [x0], #1
    ext       v1.8b, v0.8b , v0.8b , #1
    ld1       {v0.b}[7], [x10]
    ext       v2.8b, v1.8b , v1.8b , #1
    uaddl     v20.8h, v0.8b, v1.8b
    uaddl     v22.8h, v1.8b, v2.8b
    add       v24.8h, v20.8h , v22.8h
    sqrshrun  v4.8b, v20.8h, #1
    sqrshrun  v5.8b, v24.8h, #2
    mov       v6.8b, v4.8b
    ext       v6.8b, v5.8b , v4.8b , #1
    st1       {v4.b}[2], [x1], #1
    st1       {v6.b}[0], [x1], #1
    trn1      v10.8b, v6.8b, v5.8b
    trn2      v5.8b, v6.8b, v5.8b       //
    mov       v6.8b , v10.8b
    sub       x5, x3, #2
    trn1      v10.8b, v4.8b, v6.8b
    trn2      v6.8b, v4.8b, v6.8b       //
    mov       v4.8b , v10.8b
    dup       v7.8b, w9
    st1       {v6.h}[0], [x1], x5
    st1       {v6.h}[0], [x1], #2
    st1       {v5.h}[3], [x1], x5
    st1       {v5.h}[3], [x1], #2
    st1       {v7.h}[3], [x1], x5
    st1       {v7.s}[0], [x1], x3

end_func_horz_u:
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret



