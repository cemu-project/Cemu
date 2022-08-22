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
//*  ih264_intra_pred_luma_8x8_av8.s
//*
//* @brief
//*  Contains function definitions for intra 8x8 Luma prediction .
//*
//* @author
//*  Ittiam
//*
//* @par List of Functions:
//*
//*  -ih264_intra_pred_luma_8x8_mode_vert_av8
//*  -ih264_intra_pred_luma_8x8_mode_horz_av8
//*  -ih264_intra_pred_luma_8x8_mode_dc_av8
//*  -ih264_intra_pred_luma_8x8_mode_diag_dl_av8
//*  -ih264_intra_pred_luma_8x8_mode_diag_dr_av8
//*  -ih264_intra_pred_luma_8x8_mode_vert_r_av8
//*  -ih264_intra_pred_luma_8x8_mode_horz_d_av8
//*  -ih264_intra_pred_luma_8x8_mode_vert_l_av8
//*  -ih264_intra_pred_luma_8x8_mode_horz_u_av8
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

.text
.p2align 2
.include "ih264_neon_macros.s"

.extern ih264_gai1_intrapred_luma_8x8_horz_u



///**
//*******************************************************************************
//*
//*ih264_intra_pred_luma_8x8_mode_vert
//*
//* @brief
//*   Perform Intra prediction for  luma_8x8 mode:vertical
//*
//* @par Description:
//* Perform Intra prediction for  luma_8x8 mode:vertical ,described in sec 8.3.2.2.2
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
//void ih264_intra_pred_luma_8x8_mode_vert(UWORD8 *pu1_src,
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


    .global ih264_intra_pred_luma_8x8_mode_vert_av8

ih264_intra_pred_luma_8x8_mode_vert_av8:

    // STMFD sp!, {x4-x12, x14}          //store register values to stack
    push_v_regs
    //stp x19, x20,[sp,#-16]!
    sxtw      x3, w3

    add       x0, x0, #9
    ld1       {v0.8b}, [x0]

    st1       {v0.8b}, [x1], x3
    st1       {v0.8b}, [x1], x3
    st1       {v0.8b}, [x1], x3
    st1       {v0.8b}, [x1], x3
    st1       {v0.8b}, [x1], x3
    st1       {v0.8b}, [x1], x3
    st1       {v0.8b}, [x1], x3
    st1       {v0.8b}, [x1], x3

    // LDMFD sp!,{x4-x12,PC}         //Restoring registers from stack
    //ldp x19, x20,[sp],#16
    pop_v_regs
    ret





///******************************************************************************


///**
//*******************************************************************************
//*
//*ih264_intra_pred_luma_8x8_mode_horz
//*
//* @brief
//*  Perform Intra prediction for  luma_8x8 mode:horizontal
//*
//* @par Description:
//*  Perform Intra prediction for  luma_8x8 mode:horizontal ,described in sec 8.3.2.2.2
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
//void ih264_intra_pred_luma_8x8_mode_horz(UWORD8 *pu1_src,
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


    .global ih264_intra_pred_luma_8x8_mode_horz_av8

ih264_intra_pred_luma_8x8_mode_horz_av8:



    // STMFD sp!, {x4-x12, x14}          //store register values to stack
    push_v_regs
    stp       x19, x20, [sp, #-16]!
    sxtw      x3, w3
    add       x0, x0, #7

    ldrb      w5, [x0], #-1
    ldrb      w6, [x0], #-1
    dup       v0.8b, w5
    st1       {v0.8b}, [x1], x3
    ldrb      w7, [x0], #-1
    dup       v1.8b, w6
    st1       {v1.8b}, [x1], x3
    dup       v2.8b, w7
    ldrb      w8, [x0], #-1
    dup       v3.8b, w8
    st1       {v2.8b}, [x1], x3
    ldrb      w5, [x0], #-1
    st1       {v3.8b}, [x1], x3
    dup       v0.8b, w5
    ldrb      w6, [x0], #-1
    st1       {v0.8b}, [x1], x3
    ldrb      w7, [x0], #-1
    dup       v1.8b, w6
    dup       v2.8b, w7
    st1       {v1.8b}, [x1], x3
    ldrb      w8, [x0], #-1
    dup       v3.8b, w8
    st1       {v2.8b}, [x1], x3
    st1       {v3.8b}, [x1], x3

    // LDMFD sp!,{x4-x12,PC}         //Restoring registers from stack
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret







///******************************************************************************


///**
//*******************************************************************************
//*
//*ih264_intra_pred_luma_8x8_mode_dc
//*
//* @brief
//*  Perform Intra prediction for  luma_8x8 mode:DC
//*
//* @par Description:
//*  Perform Intra prediction for  luma_8x8 mode:DC ,described in sec 8.3.2.2.3
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
//void ih264_intra_pred_luma_8x8_mode_dc(UWORD8 *pu1_src,
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


    .global ih264_intra_pred_luma_8x8_mode_dc_av8

ih264_intra_pred_luma_8x8_mode_dc_av8:



    // STMFD sp!, {x4-x12, x14}          //store register values to stack
    push_v_regs
    sxtw      x3, w3
    stp       x19, x20, [sp, #-16]!

    ands      w6, w4, #0x01
    beq       top_available             //LEFT NOT AVAILABLE

    add       x10, x0, #7
    mov       x2, #-1
    ldrb      w5, [x10], -1
    ldrb      w6, [x10], -1
    ldrb      w7, [x10], -1
    add       w5, w5, w6
    ldrb      w8, [x10], -1
    add       w5, w5, w7
    ldrb      w6, [x10], -1
    add       w5, w5, w8
    ldrb      w7, [x10], -1
    add       w5, w5, w6
    ldrb      w8, [x10], -1
    add       w5, w5, w7
    ands      w11, w4, #0x04            // CHECKING IF TOP_AVAILABLE  ELSE BRANCHING TO ONLY LEFT AVAILABLE
    add       w5, w5, w8
    ldrb      w6, [x10], -1
    add       w5, w5, w6
    beq       left_available
    add       x10, x0, #9
    //    BOTH LEFT AND TOP AVAILABLE
    ld1       {v0.8b}, [x10]
    uaddlp    v1.4h, v0.8b
    uaddlp    v3.2s, v1.4h
    uaddlp    v2.1d, v3.2s
    dup       v10.8h, w5
    dup       v8.8h, v2.h[0]
    add       v12.8h, v8.8h , v10.8h
    sqrshrun  v31.8b, v12.8h, #4
    st1       {v31.8b}, [x1], x3
    st1       {v31.8b}, [x1], x3
    st1       {v31.8b}, [x1], x3
    st1       {v31.8b}, [x1], x3
    st1       {v31.8b}, [x1], x3
    st1       {v31.8b}, [x1], x3
    st1       {v31.8b}, [x1], x3
    st1       {v31.8b}, [x1], x3
    b         end_func

top_available: // ONLT TOP AVAILABLE
    ands      w11, w4, #0x04            // CHECKING TOP AVAILABILTY  OR ELSE BRANCH TO NONE AVAILABLE
    beq       none_available

    add       x10, x0, #9
    ld1       {v10.8b}, [x10]
    uaddlp    v14.4h, v10.8b
    uaddlp    v13.2s, v14.4h
    uaddlp    v12.1d, v13.2s
    rshrn     v4.8b, v12.8h, #3
    dup       v31.8b, v4.b[0]
    st1       {v31.8b}, [x1], x3
    st1       {v31.8b}, [x1], x3
    st1       {v31.8b}, [x1], x3
    st1       {v31.8b}, [x1], x3
    st1       {v31.8b}, [x1], x3
    st1       {v31.8b}, [x1], x3
    st1       {v31.8b}, [x1], x3
    st1       {v31.8b}, [x1], x3
    b         end_func


left_available: //ONLY LEFT AVAILABLE
    add       x5, x5, #4
    lsr       x5, x5, #3
    dup       v0.8b, w5
    st1       {v0.8b}, [x1], x3
    st1       {v0.8b}, [x1], x3
    st1       {v0.8b}, [x1], x3
    st1       {v0.8b}, [x1], x3
    st1       {v0.8b}, [x1], x3
    st1       {v0.8b}, [x1], x3
    st1       {v0.8b}, [x1], x3
    st1       {v0.8b}, [x1], x3
    b         end_func

none_available:                         //NONE AVAILABLE
    mov       x9, #128
    dup       v0.8b, w9
    st1       {v0.8b}, [x1], x3
    st1       {v0.8b}, [x1], x3
    st1       {v0.8b}, [x1], x3
    st1       {v0.8b}, [x1], x3
    st1       {v0.8b}, [x1], x3
    st1       {v0.8b}, [x1], x3
    st1       {v0.8b}, [x1], x3
    st1       {v0.8b}, [x1], x3


end_func:

    // LDMFD sp!,{x4-x12,PC}         //Restoring registers from stack
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret






///**
//*******************************************************************************
//*
//*ih264_intra_pred_luma_8x8_mode_diag_dl
//*
//* @brief
//*  Perform Intra prediction for  luma_8x8 mode:Diagonal_Down_Left
//*
//* @par Description:
//*  Perform Intra prediction for  luma_8x8 mode:Diagonal_Down_Left ,described in sec 8.3.2.2.4
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
//void ih264_intra_pred_luma_8x8_mode_diag_dl(UWORD8 *pu1_src,
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

    .global ih264_intra_pred_luma_8x8_mode_diag_dl_av8

ih264_intra_pred_luma_8x8_mode_diag_dl_av8:

    // STMFD sp!, {x4-x12, x14}          //store register values to stack
    push_v_regs
    stp       x19, x20, [sp, #-16]!
    sxtw      x3, w3

    add       x0, x0, #9
    sub       x5, x3, #4
    add       x6, x0, #15
    ld1       { v0.16b}, [x0]
    mov       v1.d[0], v0.d[1]
    ext       v4.16b, v0.16b , v0.16b , #2
    mov       v5.d[0], v4.d[1]
    ext       v2.16b, v0.16b , v0.16b , #1
    mov       v3.d[0], v2.d[1]
    ld1       {v5.b}[6], [x6]
    // q1 = q0 shifted to left once
    // q2 = q1 shifted to left once
    uaddl     v20.8h, v0.8b, v2.8b      //Adding for FILT121
    uaddl     v22.8h, v1.8b, v3.8b
    uaddl     v24.8h, v2.8b, v4.8b
    uaddl     v26.8h, v3.8b, v5.8b
    add       v24.8h, v20.8h , v24.8h
    add       v26.8h, v22.8h , v26.8h

    sqrshrun  v4.8b, v24.8h, #2
    sqrshrun  v5.8b, v26.8h, #2
    mov       v4.d[1], v5.d[0]
    //Q2 has all FILT121 values
    st1       {v4.8b}, [x1], x3
    ext       v18.16b, v4.16b , v4.16b , #1
    ext       v16.16b, v18.16b , v18.16b , #1
    st1       {v18.8b}, [x1], x3
    ext       v14.16b, v16.16b , v16.16b , #1
    st1       {v16.8b}, [x1], x3
    st1       {v14.8b}, [x1], x3
    st1       {v4.s}[1], [x1], #4
    st1       {v5.s}[0], [x1], x5
    st1       {v18.s}[1], [x1], #4
    st1       {v18.s}[2], [x1], x5
    st1       {v16.s}[1], [x1], #4
    st1       {v16.s}[2], [x1], x5
    st1       {v14.s}[1], [x1], #4
    st1       {v14.s}[2], [x1], x5


end_func_diag_dl:
    // LDMFD sp!,{x4-x12,PC}         //Restoring registers from stack
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret




///**
//*******************************************************************************
//*
//*ih264_intra_pred_luma_8x8_mode_diag_dr
//*
//* @brief
//* Perform Intra prediction for  luma_8x8 mode:Diagonal_Down_Right
//*
//* @par Description:
//*  Perform Intra prediction for  luma_8x8 mode:Diagonal_Down_Right ,described in sec 8.3.2.2.5
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
//void ih264_intra_pred_luma_8x8_mode_diag_dr(UWORD8 *pu1_src,
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


    .global ih264_intra_pred_luma_8x8_mode_diag_dr_av8

ih264_intra_pred_luma_8x8_mode_diag_dr_av8:

    // STMFD sp!, {x4-x12, x14}          //store register values to stack
    push_v_regs
    stp       x19, x20, [sp, #-16]!
    sxtw      x3, w3


    ld1       { v0.16b}, [x0]
    mov       v1.d[0], v0.d[1]
    add       x0, x0, #1
    ld1       { v2.16b}, [x0]
    mov       v3.d[0], v2.d[1]
    ext       v4.16b, v2.16b , v2.16b , #1
    mov       v5.d[0], v4.d[1]
    // q1 = q0 shifted to left once
    // q2 = q1 shifted to left once
    uaddl     v20.8h, v0.8b, v2.8b      //Adding for FILT121
    uaddl     v22.8h, v1.8b, v3.8b
    uaddl     v24.8h, v2.8b, v4.8b
    uaddl     v26.8h, v3.8b, v5.8b
    add       v24.8h, v20.8h , v24.8h
    add       v26.8h, v22.8h , v26.8h
    sqrshrun  v4.8b, v24.8h, #2
    sqrshrun  v5.8b, v26.8h, #2
    mov       v4.d[1], v5.d[0]
    //Q2 has all FILT121 values
    sub       x5, x3, #4
    ext       v18.16b, v4.16b , v4.16b , #15
    st1       {v18.d}[1], [x1], x3
    ext       v16.16b, v18.16b , v18.16b , #15
    st1       {v16.d}[1], [x1], x3
    ext       v14.16b, v16.16b , v16.16b , #15
    st1       {v14.d}[1], [x1], x3
    st1       {v4.s}[1], [x1], #4
    st1       {v5.s}[0], [x1], x5
    st1       {v18.s}[1], [x1], #4
    st1       {v18.s}[2], [x1], x5
    st1       {v16.s}[1], [x1], #4
    st1       {v16.s}[2], [x1], x5
    st1       {v14.s}[1], [x1], #4
    st1       {v14.s}[2], [x1], x5
    st1       {v4.8b}, [x1], x3

end_func_diag_dr:
    // LDMFD sp!,{x4-x12,PC}         //Restoring registers from stack
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret




///**
//*******************************************************************************
//*
//*ih264_intra_pred_luma_8x8_mode_vert_r
//*
//* @brief
//* Perform Intra prediction for  luma_8x8 mode:Vertical_Right
//*
//* @par Description:
//*   Perform Intra prediction for  luma_8x8 mode:Vertical_Right ,described in sec 8.3.2.2.6
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
//void ih264_intra_pred_luma_8x8_mode_vert_r(UWORD8 *pu1_src,
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


    .global ih264_intra_pred_luma_8x8_mode_vert_r_av8

ih264_intra_pred_luma_8x8_mode_vert_r_av8:

    // STMFD sp!, {x4-x12, x14}          //store register values to stack
    push_v_regs
    stp       x19, x20, [sp, #-16]!
    sxtw      x3, w3

    ld1       { v0.16b}, [x0]
    mov       v1.d[0], v0.d[1]
    add       x0, x0, #1
    ld1       { v2.16b}, [x0]
    mov       v3.d[0], v2.d[1]
    ext       v4.16b, v2.16b , v2.16b , #1
    mov       v5.d[0], v4.d[1]
    // q1 = q0 shifted to left once
    // q2 = q1 shifted to left once
    uaddl     v20.8h, v0.8b, v2.8b
    uaddl     v22.8h, v1.8b, v3.8b
    uaddl     v24.8h, v2.8b, v4.8b
    uaddl     v26.8h, v3.8b, v5.8b
    add       v24.8h, v20.8h , v24.8h
    add       v26.8h, v22.8h , v26.8h

    sqrshrun  v4.8b, v20.8h, #1
    sqrshrun  v5.8b, v22.8h, #1
    mov       v4.d[1], v5.d[0]
    sqrshrun  v6.8b, v24.8h, #2
    sqrshrun  v7.8b, v26.8h, #2
    mov       v6.d[1], v7.d[0]
    //Q2 has all FILT11 values
    //Q3 has all FILT121 values
    sub       x5, x3, #6
    sub       x6, x3, #4
    st1       {v5.8b}, [x1], x3         // row 0
    ext       v18.16b, v6.16b , v6.16b , #15
    mov       v22.16b , v18.16b
    ext       v16.16b, v4.16b , v4.16b , #1
    st1       {v18.d}[1], [x1], x3      //row 1
    mov       v14.16b , v16.16b
    ext       v20.16b, v4.16b , v4.16b , #15
    uzp1      v17.16b, v16.16b, v18.16b
    uzp2      v18.16b, v16.16b, v18.16b
    mov       v16.16b , v17.16b
    //row 2
    ext       v12.16b, v16.16b , v16.16b , #1
    st1       {v20.d}[1], [x1]
    st1       {v6.b}[6], [x1], x3
    //row 3

    st1       {v12.h}[5], [x1], #2
    st1       {v6.s}[2], [x1], #4
    st1       {v6.h}[6], [x1], x5
    //row 4
    st1       {v18.h}[5], [x1], #2
    st1       {v4.s}[2], [x1], #4
    st1       {v4.h}[6], [x1], x5
    //row 5
    ext       v26.16b, v18.16b , v18.16b , #1
    st1       {v16.h}[5], [x1], #2
    st1       {v22.s}[2], [x1], #4
    st1       {v22.h}[6], [x1], x5
    //row 6
    st1       {v26.h}[4], [x1], #2
    st1       {v26.b}[10], [x1], #1
    st1       {v4.b}[8], [x1], #1
    st1       {v14.s}[2], [x1], x6
    //row 7
    st1       {v12.s}[2], [x1], #4
    st1       {v6.s}[2], [x1], #4

end_func_vert_r:
    // LDMFD sp!,{x4-x12,PC}         //Restoring registers from stack
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret




///**
//*******************************************************************************
//*
//*ih264_intra_pred_luma_8x8_mode_horz_d
//*
//* @brief
//* Perform Intra prediction for  luma_8x8 mode:Horizontal_Down
//*
//* @par Description:
//*   Perform Intra prediction for  luma_8x8 mode:Horizontal_Down ,described in sec 8.3.2.2.7
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
//void ih264_intra_pred_luma_8x8_mode_horz_d(UWORD8 *pu1_src,
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

    .global ih264_intra_pred_luma_8x8_mode_horz_d_av8

ih264_intra_pred_luma_8x8_mode_horz_d_av8:

    // STMFD sp!, {x4-x12, x14}          //store register values to stack
    push_v_regs
    stp       x19, x20, [sp, #-16]!
    sxtw      x3, w3

    ld1       { v0.16b}, [x0]
    mov       v1.d[0], v0.d[1]
    add       x0, x0, #1
    ld1       { v2.16b}, [x0]
    mov       v3.d[0], v2.d[1]
    ext       v4.16b, v2.16b , v2.16b , #1
    mov       v5.d[0], v4.d[1]
    // q1 = q0 shifted to left once
    // q2 = q1 shifted to left once
    uaddl     v20.8h, v0.8b, v2.8b
    uaddl     v22.8h, v1.8b, v3.8b
    uaddl     v24.8h, v2.8b, v4.8b
    uaddl     v26.8h, v3.8b, v5.8b
    add       v24.8h, v20.8h , v24.8h
    add       v26.8h, v22.8h , v26.8h

    sqrshrun  v4.8b, v20.8h, #1
    sqrshrun  v5.8b, v22.8h, #1
    mov       v4.d[1], v5.d[0]
    sqrshrun  v6.8b, v24.8h, #2
    sqrshrun  v7.8b, v26.8h, #2
    mov       v6.d[1], v7.d[0]
    //Q2 has all FILT11 values
    //Q3 has all FILT121 values
    mov       v8.16b, v4.16b
    mov       v10.16b, v6.16b
    sub       x6, x3, #6
    trn1      v9.16b, v8.16b, v10.16b
    trn2      v10.16b, v8.16b, v10.16b  //
    mov       v8.16b, v9.16b
    mov       v12.16b, v8.16b
    mov       v14.16b, v10.16b
    sub       x5, x3, #4
    trn1      v13.8h, v12.8h, v14.8h
    trn2      v14.8h, v12.8h, v14.8h
    mov       v12.16b, v13.16b
    ext       v16.16b, v6.16b , v6.16b , #14
    //ROW 0
    st1       {v16.d}[1], [x1]
    st1       {v10.h}[3], [x1], x3

    //ROW 1
    st1       {v14.s}[1], [x1], #4
    st1       {v6.s}[2], [x1], x5
    //ROW 2
    st1       {v10.h}[2], [x1], #2
    st1       {v14.s}[1], [x1], #4
    st1       {v7.h}[0], [x1], x6
    //ROW 3
    st1       {v12.s}[1], [x1], #4
    st1       {v14.s}[1], [x1], x5
    //ROW 4
    st1       {v14.h}[1], [x1], #2
    st1       {v12.s}[1], [x1], #4
    st1       {v14.h}[2], [x1], x6
    //ROW 5
    st1       {v14.s}[0], [x1], #4
    st1       {v12.s}[1], [x1], x5
    //ROW 6
    st1       {v10.h}[0], [x1], #2
    st1       {v8.h}[1], [x1], #2
    st1       {v14.h}[1], [x1], #2
    st1       {v12.h}[2], [x1], x6
    //ROW 7
    st1       {v12.s}[0], [x1], #4
    st1       {v14.s}[0], [x1], x5

end_func_horz_d:
    // LDMFD sp!,{x4-x12,PC}         //Restoring registers from stack
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret





///**
//*******************************************************************************
//*
//*ih264_intra_pred_luma_8x8_mode_vert_l
//*
//* @brief
//*  Perform Intra prediction for  luma_8x8 mode:Vertical_Left
//*
//* @par Description:
//*   Perform Intra prediction for  luma_8x8 mode:Vertical_Left ,described in sec 8.3.2.2.8
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
//void ih264_intra_pred_luma_8x8_mode_vert_l(UWORD8 *pu1_src,
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


    .global ih264_intra_pred_luma_8x8_mode_vert_l_av8

ih264_intra_pred_luma_8x8_mode_vert_l_av8:

    // STMFD sp!, {x4-x12, x14}         //Restoring registers from stack
    push_v_regs
    stp       x19, x20, [sp, #-16]!
    sxtw      x3, w3
    add       x0, x0, #9
    ld1       { v0.16b}, [x0]
    mov       v1.d[0], v0.d[1]
    add       x0, x0, #1
    ld1       { v2.16b}, [x0]
    mov       v3.d[0], v2.d[1]
    ext       v4.16b, v2.16b , v2.16b , #1
    mov       v5.d[0], v4.d[1]
    uaddl     v20.8h, v0.8b, v2.8b
    uaddl     v22.8h, v1.8b, v3.8b
    uaddl     v24.8h, v2.8b, v4.8b
    uaddl     v26.8h, v3.8b, v5.8b
    add       v24.8h, v20.8h , v24.8h
    add       v26.8h, v22.8h , v26.8h

    sqrshrun  v4.8b, v20.8h, #1
    sqrshrun  v5.8b, v22.8h, #1
    mov       v4.d[1], v5.d[0]
    sqrshrun  v6.8b, v24.8h, #2
    ext       v8.16b, v4.16b , v4.16b , #1
    sqrshrun  v7.8b, v26.8h, #2
    mov       v6.d[1], v7.d[0]
    //Q2 has all FILT11 values
    //Q3 has all FILT121 values

    ext       v10.16b, v6.16b , v6.16b , #1
    //ROW 0,1
    st1       {v4.8b}, [x1], x3
    st1       {v6.8b}, [x1], x3

    ext       v12.16b, v8.16b , v8.16b , #1
    ext       v14.16b, v10.16b , v10.16b , #1
    //ROW 2,3
    st1       {v8.8b}, [x1], x3
    st1       {v10.8b}, [x1], x3

    ext       v16.16b, v12.16b , v12.16b , #1
    ext       v18.16b, v14.16b , v14.16b , #1
    //ROW 4,5
    st1       {v12.8b}, [x1], x3
    st1       {v14.8b}, [x1], x3
    //ROW 6,7
    st1       {v16.8b}, [x1], x3
    st1       {v18.8b}, [x1], x3

end_func_vert_l:
    // LDMFD sp!,{x4-x12,PC}         //Restoring registers from stack
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret





///**
//*******************************************************************************
//*
//*ih264_intra_pred_luma_8x8_mode_horz_u
//*
//* @brief
//*     Perform Intra prediction for  luma_8x8 mode:Horizontal_Up
//*
//* @par Description:
//*      Perform Intra prediction for  luma_8x8 mode:Horizontal_Up ,described in sec 8.3.2.2.9
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
//void ih264_intra_pred_luma_8x8_mode_horz_u(UWORD8 *pu1_src,
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

    .global ih264_intra_pred_luma_8x8_mode_horz_u_av8

ih264_intra_pred_luma_8x8_mode_horz_u_av8:

    // STMFD sp!, {x4-x12, x14}          //store register values to stack
    push_v_regs
    stp       x19, x20, [sp, #-16]!
    sxtw      x3, w3

    ld1       {v0.8b}, [x0]
    ld1       {v1.b}[7], [x0]
    mov       v0.d[1], v1.d[0]
    ext       v2.16b, v0.16b , v0.16b , #1
    mov       v3.d[0], v2.d[1]
    ext       v4.16b, v2.16b , v2.16b , #1
    mov       v5.d[0], v4.d[1]

    adrp      x12, :got:ih264_gai1_intrapred_luma_8x8_horz_u
    ldr       x12, [x12, #:got_lo12:ih264_gai1_intrapred_luma_8x8_horz_u]
    uaddl     v20.8h, v0.8b, v2.8b
    uaddl     v22.8h, v1.8b, v3.8b
    uaddl     v24.8h, v2.8b, v4.8b
    uaddl     v26.8h, v3.8b, v5.8b
    add       v24.8h, v20.8h , v24.8h
    add       v26.8h, v22.8h , v26.8h
    ld1       { v10.16b}, [x12]
    mov       v11.d[0], v10.d[1]
    sqrshrun  v4.8b, v20.8h, #1
    sqrshrun  v5.8b, v22.8h, #1
    mov       v4.d[1], v5.d[0]
    sqrshrun  v6.8b, v24.8h, #2
    sqrshrun  v7.8b, v26.8h, #2
    mov       v6.d[1], v7.d[0]
    //Q2 has all FILT11 values
    //Q3 has all FILT121 values
    mov       v30.16b, v4.16b
    mov       v31.16b, v6.16b
    tbl       v12.8b, {v30.16b, v31.16b}, v10.8b
    dup       v14.16b, v5.b[7]          //
    tbl       v13.8b, {v30.16b, v31.16b}, v11.8b
    mov       v12.d[1], v13.d[0]
    ext       v16.16b, v12.16b , v14.16b , #2
    ext       v18.16b, v16.16b , v14.16b , #2
    st1       {v12.8b}, [x1], x3        //0
    ext       v20.16b, v18.16b , v14.16b , #2
    st1       {v16.8b}, [x1], x3        //1
    st1       {v18.8b}, [x1], x3        //2
    st1       {v20.8b}, [x1], x3        //3
    st1       {v13.8b}, [x1], x3        //4
    st1       {v16.d}[1], [x1], x3      //5
    st1       {v18.d}[1], [x1], x3      //6
    st1       {v20.d}[1], [x1], x3      //7


end_func_horz_u:
    // LDMFD sp!,{x4-x12,PC}         //Restoring registers from stack
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret


