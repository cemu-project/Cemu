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
// *******************************************************************************
// * @file
// *  ih264_padding_neon.s
// *
// * @brief
// *  Contains function definitions padding
// *
// * @author
// *     Ittiam
// *
// * @par List of Functions:
// *  - ih264_pad_top_av8()
// *  - ih264_pad_left_luma_av8()
// *  - ih264_pad_left_chroma_av8()
// *  - ih264_pad_right_luma_av8()
// *  - ih264_pad_right_chroma_av8()
// *
// * @remarks
// *  None
// *
// *******************************************************************************
//*/

.text
.p2align 2
.include "ih264_neon_macros.s"
///**
//*******************************************************************************
//*
//* @brief pad at the top of a 2d array
//*
//* @par Description:
//*  The top row of a 2d array is replicated for pad_size times at the top
//*
//* @param[in] pu1_src
//*  UWORD8 pointer to the source
//*
//* @param[in] src_strd
//*  integer source stride
//*
//* @param[in] wd
//*  integer width of the array
//*
//* @param[in] pad_size
//*  integer -padding size of the array
//*
//* @returns none
//*
//* @remarks none
//*
//*******************************************************************************
//*/
//void ih264_pad_top(UWORD8 *pu1_src,
//                   WORD32 src_strd,
//                   WORD32 wd,
//                   WORD32 pad_size)
//**************Variables Vs Registers*************************
//    x0 => *pu1_src
//    w1 => src_strd
//    w2 => wd
//    w3 => pad_size

    .global ih264_pad_top_av8

ih264_pad_top_av8:

    // STMFD sp!, {x4-x11,x14}                //stack stores the values of the arguments
    push_v_regs
    sxtw      x1, w1
    stp       x19, x20, [sp, #-16]!

    sub       x5, x0, x1
    neg       x6, x1

loop_neon_memcpy_mul_16:
    // Load 16 bytes
    ld1       {v0.8b, v1.8b}, [x0], #16
    mov       x4, x5
    mov       w7, w3
    add       x5, x5, #16

loop_neon_pad_top:
    st1       {v0.8b, v1.8b}, [x4], x6
    subs      w7, w7, #1
    bne       loop_neon_pad_top

    subs      w2, w2, #16
    bne       loop_neon_memcpy_mul_16

    // LDMFD sp!,{x4-x11,pc}                //Reload the registers from SP
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret




///**
//*******************************************************************************
//*
//* @brief
//*   Padding (luma block) at the left of a 2d array
//*
//* @par Description:
//*   The left column of a 2d array is replicated for pad_size times at the left
//*
//*
//* @param[in] pu1_src
//*  UWORD8 pointer to the source
//*
//* @param[in] src_strd
//*  integer source stride
//*
//* @param[in] ht
//*  integer height of the array
//*
//* @param[in] wd
//*  integer width of the array
//*
//* @param[in] pad_size
//*  integer -padding size of the array
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
//#if PAD_LEFT_LUMA == C
//void ih264_pad_left_luma(UWORD8 *pu1_src,
//                        WORD32 src_strd,
//                        WORD32 ht,
//                        WORD32 pad_size)
//**************Variables Vs Registers*************************
//    x0 => *pu1_src
//    w1 => src_strd
//    w2 => ht
//    w3 => pad_size



    .global ih264_pad_left_luma_av8

ih264_pad_left_luma_av8:

    // STMFD sp!, {x4-x11,x14}                //stack stores the values of the arguments
    push_v_regs
    sxtw      x1, w1
    sxtw      x3, w3
    stp       x19, x20, [sp, #-16]!


    sub       x4, x0, x3
    sub       x6, x1, #16
    subs      x5, x3, #16
    bne       loop_32
loop_16:                                //  /*hard coded for width=16  ,height =8,16*/
    ldrb      w8, [x0]
    add       x0, x0, x1
    ldrb      w9, [x0]
    add       x0, x0, x1
    dup       v0.16b, w8
    ldrb      w10, [x0]
    add       x0, x0, x1
    st1       {v0.16b}, [x4], x1        // 16 bytes store
    dup       v2.16b, w9
    st1       {v2.16b}, [x4], x1        // 16 bytes store
    ldrb      w11, [x0]
    add       x0, x0, x1
    dup       v4.16b, w10
    dup       v6.16b, w11
    st1       {v4.16b}, [x4], x1        // 16 bytes store
    ldrb      w8, [x0]
    add       x0, x0, x1
    st1       {v6.16b}, [x4], x1        // 16 bytes store
    ldrb      w9, [x0]
    add       x0, x0, x1
    dup       v0.16b, w8
    ldrb      w10, [x0]
    add       x0, x0, x1
    st1       {v0.16b}, [x4], x1        // 16 bytes store
    dup       v2.16b, w9
    ldrb      w11, [x0]
    add       x0, x0, x1
    st1       {v2.16b}, [x4], x1        // 16 bytes store
    dup       v4.16b, w10
    dup       v6.16b, w11
    subs      w2, w2, #8
    st1       {v4.16b}, [x4], x1        // 16 bytes store
    st1       {v6.16b}, [x4], x1        // 16 bytes store
    bne       loop_16
    b         end_func

loop_32:                                //  /*hard coded for width=32 ,height =8,16*/
    ldrb      w8, [x0]
    add       x0, x0, x1
    ldrb      w9, [x0]
    add       x0, x0, x1
    dup       v0.16b, w8
    ldrb      w10, [x0]
    add       x0, x0, x1
    st1       {v0.16b}, [x4], #16       // 16 bytes store
    dup       v2.16b, w9
    st1       {v0.16b}, [x4], x6
    st1       {v2.16b}, [x4], #16       // 16 bytes store
    dup       v4.16b, w10
    st1       {v2.16b}, [x4], x6        // 16 bytes store
    ldrb      w11, [x0]
    add       x0, x0, x1
    st1       {v4.16b}, [x4], #16       // 16 bytes store
    dup       v6.16b, w11
    st1       {v4.16b}, [x4], x6        // 16 bytes store
    ldrb      w8, [x0]
    add       x0, x0, x1
    st1       {v6.16b}, [x4], #16       // 16 bytes store
    dup       v0.16b, w8
    ldrb      w9, [x0]
    add       x0, x0, x1
    st1       {v6.16b}, [x4], x6        // 16 bytes store
    ldrb      w10, [x0]
    add       x0, x0, x1
    st1       {v0.16b}, [x4], #16       // 16 bytes store
    dup       v2.16b, w9
    st1       {v0.16b}, [x4], x6        // 16 bytes store
    ldrb      w11, [x0]
    add       x0, x0, x1
    st1       {v2.16b}, [x4], #16       // 16 bytes store
    dup       v4.16b, w10
    st1       {v2.16b}, [x4], x6        // 16 bytes store
    st1       {v4.16b}, [x4], #16       // 16 bytes store
    dup       v6.16b, w11
    st1       {v4.16b}, [x4], x6        // 16 bytes store
    subs      w2, w2, #8
    st1       {v6.16b}, [x4], #16       // 16 bytes store
    st1       {v6.16b}, [x4], x6        // 16 bytes store
    bne       loop_32



end_func:
    // LDMFD sp!,{x4-x11,pc}                //Reload the registers from SP
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret





///**
//*******************************************************************************
//*
//* @brief
//*   Padding (chroma block) at the left of a 2d array
//*
//* @par Description:
//*   The left column of a 2d array is replicated for pad_size times at the left
//*
//*
//* @param[in] pu1_src
//*  UWORD8 pointer to the source
//*
//* @param[in] src_strd
//*  integer source stride
//*
//* @param[in] ht
//*  integer height of the array
//*
//* @param[in] wd
//*  integer width of the array (each colour component)
//*
//* @param[in] pad_size
//*  integer -padding size of the array
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
//#if PAD_LEFT_CHROMA == C
//void ih264_pad_left_chroma(UWORD8 *pu1_src,
//                            WORD32 src_strd,
//                            WORD32 ht,
//                            WORD32 pad_size)
//{
//    x0 => *pu1_src
//    w1 => src_strd
//    w2 => ht
//    w3 => pad_size



    .global ih264_pad_left_chroma_av8

ih264_pad_left_chroma_av8:

    // STMFD sp!, {x4-x11, x14}                //stack stores the values of the arguments
    push_v_regs
    sxtw      x1, w1
    sxtw      x3, w3
    stp       x19, x20, [sp, #-16]!

    sub       x4, x0, x3
    sub       x6, x1, #16


loop_32_l_c:                            //  /*hard coded for width=32  ,height =4,8,12*/
    ldrh      w8, [x0]
    add       x0, x0, x1
    ldrh      w9, [x0]
    add       x0, x0, x1
    dup       v0.8h, w8
    ldrh      w10, [x0]
    add       x0, x0, x1
    st1       {v0.16b}, [x4], #16       // 16 bytes store
    dup       v2.8h, w9
    st1       {v0.16b}, [x4], x6        // 16 bytes store
    ldrh      w11, [x0]
    add       x0, x0, x1
    st1       {v2.16b}, [x4], #16       // 16 bytes store
    dup       v4.8h, w10
    st1       {v2.16b}, [x4], x6        // 16 bytes store
    dup       v6.8h, w11
    st1       {v4.16b}, [x4], #16       // 16 bytes store
    st1       {v4.16b}, [x4], x6        // 16 bytes store
    subs      w2, w2, #4
    st1       {v6.16b}, [x4], #16       // 16 bytes store
    st1       {v6.16b}, [x4], x6        // 16 bytes store


    beq       end_func_l_c              ///* Branching when ht=4*/

    ldrh      w8, [x0]
    add       x0, x0, x1
    ldrh      w9, [x0]
    add       x0, x0, x1
    dup       v0.8h, w8
    ldrh      w10, [x0]
    add       x0, x0, x1
    st1       {v0.16b}, [x4], #16       // 16 bytes store
    dup       v2.8h, w9
    st1       {v0.16b}, [x4], x6
    ldrh      w11, [x0]
    add       x0, x0, x1
    st1       {v2.16b}, [x4], #16       // 16 bytes store
    dup       v4.8h, w10
    st1       {v2.16b}, [x4], x6        // 16 bytes store
    dup       v6.8h, w11
    st1       {v4.16b}, [x4], #16       // 16 bytes store
    st1       {v4.16b}, [x4], x6        // 16 bytes store
    subs      w2, w2, #4
    st1       {v6.16b}, [x4], #16       // 16 bytes store
    st1       {v6.16b}, [x4], x6        // 16 bytes store

    beq       end_func_l_c              ///* Branching when ht=8*/
    bne       loop_32_l_c

    ldrh      w8, [x0]
    add       x0, x0, x1
    ldrh      w9, [x0]
    add       x0, x0, x1
    dup       v0.8h, w8
    ldrh      w10, [x0]
    add       x0, x0, x1
    st1       {v0.16b}, [x4], #16       // 16 bytes store
    dup       v2.8h, w9
    st1       {v0.16b}, [x4], x6
    ldrh      w11, [x0]
    add       x0, x0, x1
    st1       {v2.16b}, [x4], #16       // 16 bytes store
    dup       v4.8h, w10
    st1       {v2.16b}, [x4], x6        // 16 bytes store
    dup       v6.8h, w11
    st1       {v4.16b}, [x4], #16       // 16 bytes store
    st1       {v4.16b}, [x4], x6        // 16 bytes store
    st1       {v6.16b}, [x4], #16       // 16 bytes store
    st1       {v6.16b}, [x4], x6        // 16 bytes store

end_func_l_c:
    // LDMFD sp!,{x4-x11,pc}                //Reload the registers from SP
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret





///**
//*******************************************************************************
//*
//* @brief
//* Padding (luma block) at the right of a 2d array
//*
//* @par Description:
//* The right column of a 2d array is replicated for pad_size times at the right
//*
//*
//* @param[in] pu1_src
//*  UWORD8 pointer to the source
//*
//* @param[in] src_strd
//*  integer source stride
//*
//* @param[in] ht
//*  integer height of the array
//*
//* @param[in] wd
//*  integer width of the array
//*
//* @param[in] pad_size
//*  integer -padding size of the array
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
//#if PAD_RIGHT_LUMA == C
//void ih264_pad_right_luma(UWORD8 *pu1_src,
//                        WORD32 src_strd,
//                        WORD32 ht,
//                        WORD32 pad_size)
//{
//    WORD32 row;
//
//    for(row = 0; row < ht; row++)
//    {
//        memset(pu1_src, *(pu1_src -1), pad_size);
//
//        pu1_src += src_strd;
//    }
//}
//
//    x0 => *pu1_src
//    w1 => src_strd
//    w2 => ht
//    w3 => pad_size



    .global ih264_pad_right_luma_av8

ih264_pad_right_luma_av8:

    // STMFD sp!, {x4-x11, x14}                //stack stores the values of the arguments
    push_v_regs
    sxtw      x1, w1
    sxtw      x3, w3
    stp       x19, x20, [sp, #-16]!

    mov       x4, x0
    sub       x6, x1, #16
    sub       x0, x0, #1
    subs      x5, x3, #16
    bne       loop_32
loop_16_r: //  /*hard coded for width=16  ,height =8,16*/
    ldrb      w8, [x0]
    add       x0, x0, x1
    ldrb      w9, [x0]
    add       x0, x0, x1
    dup       v0.16b, w8
    ldrb      w10, [x0]
    add       x0, x0, x1
    st1       {v0.16b}, [x4], x1        // 16 bytes store
    dup       v2.16b, w9
    st1       {v2.16b}, [x4], x1        // 16 bytes store
    ldrb      w11, [x0]
    add       x0, x0, x1
    dup       v4.16b, w10
    dup       v6.16b, w11
    st1       {v4.16b}, [x4], x1        // 16 bytes store
    ldrb      w8, [x0]
    add       x0, x0, x1
    st1       {v6.16b}, [x4], x1        // 16 bytes store
    ldrb      w9, [x0]
    add       x0, x0, x1
    dup       v0.16b, w8
    ldrb      w10, [x0]
    add       x0, x0, x1
    st1       {v0.16b}, [x4], x1        // 16 bytes store
    dup       v2.16b, w9
    ldrb      w11, [x0]
    add       x0, x0, x1
    st1       {v2.16b}, [x4], x1        // 16 bytes store
    dup       v4.16b, w10
    dup       v6.16b, w11
    subs      w2, w2, #8
    st1       {v4.16b}, [x4], x1        // 16 bytes store
    st1       {v6.16b}, [x4], x1        // 16 bytes store
    bne       loop_16_r
    b         end_func_r

loop_32_r:                              //  /*hard coded for width=32  ,height =8,16*/
    ldrb      w8, [x0]
    add       x0, x0, x1
    ldrb      w9, [x0]
    add       x0, x0, x1
    dup       v0.16b, w8
    ldrb      w10, [x0]
    add       x0, x0, x1
    st1       {v0.16b}, [x4], #16       // 16 bytes store
    dup       v2.16b, w9
    st1       {v0.16b}, [x4], x6
    st1       {v2.16b}, [x4], #16       // 16 bytes store
    dup       v4.16b, w10
    st1       {v2.16b}, [x4], x6        // 16 bytes store
    ldrb      w11, [x0]
    add       x0, x0, x1
    st1       {v4.16b}, [x4], #16       // 16 bytes store
    dup       v6.16b, w11
    st1       {v4.16b}, [x4], x6        // 16 bytes store
    ldrb      w8, [x0]
    add       x0, x0, x1
    st1       {v6.16b}, [x4], #16       // 16 bytes store
    ldrb      w9, [x0]
    add       x0, x0, x1
    dup       v0.16b, w8
    st1       {v6.16b}, [x4], x6        // 16 bytes store
    ldrb      w10, [x0]
    add       x0, x0, x1
    st1       {v0.16b}, [x4], #16       // 16 bytes store
    dup       v2.16b, w9
    st1       {v0.16b}, [x4], x6        // 16 bytes store
    ldrb      w11, [x0]
    add       x0, x0, x1
    st1       {v2.16b}, [x4], #16       // 16 bytes store
    dup       v4.16b, w10
    st1       {v2.16b}, [x4], x6        // 16 bytes store
    st1       {v4.16b}, [x4], #16       // 16 bytes store
    dup       v6.16b, w11
    st1       {v4.16b}, [x4], x6        // 16 bytes store
    subs      w2, w2, #8
    st1       {v6.16b}, [x4], #16       // 16 bytes store
    st1       {v6.16b}, [x4], x6        // 16 bytes store
    bne       loop_32_r



end_func_r:
    // LDMFD sp!,{x4-x11,pc}                //Reload the registers from SP
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret





///**
//*******************************************************************************
//*
//* @brief
//;* Padding (chroma block) at the right of a 2d array
//*
//* @par Description:
//* The right column of a 2d array is replicated for pad_size times at the right
//*
//*
//* @param[in] pu1_src
//;*  UWORD8 pointer to the source
//*
//* @param[in] src_strd
//*  integer source stride
//*
//* @param[in] ht
//;*  integer height of the array
//*
//* @param[in] wd
//*  integer width of the array (each colour component)
//*
//* @param[in] pad_size
//*  integer -padding size of the array
//*
//* @param[in] ht
//;*  integer height of the array
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
//#if PAD_RIGHT_CHROMA == C
//void ih264_pad_right_chroma(UWORD8 *pu1_src,
//                        WORD32 src_strd,
//                        WORD32 ht,
//                        WORD32 pad_size)
//    x0 => *pu1_src
//    w1 => src_strd
//    w2 => ht
//    w3 => pad_size



    .global ih264_pad_right_chroma_av8

ih264_pad_right_chroma_av8:

    // STMFD sp!, {x4-x11, x14}                //stack stores the values of the arguments
    push_v_regs
    sxtw      x1, w1
    sxtw      x3, w3
    stp       x19, x20, [sp, #-16]!

    mov       x4, x0
    sub       x6, x1, #16
    sub       x0, x0, #2
loop_32_r_c: //  /*hard coded for width=32 ,height =8,4*/
    ldrh      w8, [x0]
    add       x0, x0, x1
    ldrh      w9, [x0]
    add       x0, x0, x1
    dup       v0.8h, w8
    ldrh      w10, [x0]
    add       x0, x0, x1
    st1       {v0.16b}, [x4], #16       // 16 bytes store
    dup       v2.8h, w9
    st1       {v0.16b}, [x4], x6
    st1       {v2.16b}, [x4], #16       // 16 bytes store
    dup       v4.8h, w10
    st1       {v2.16b}, [x4], x6        // 16 bytes store
    subs      w2, w2, #4
    ldrh      w11, [x0]
    add       x0, x0, x1
    st1       {v4.16b}, [x4], #16       // 16 bytes store
    dup       v6.8h, w11
    st1       {v4.16b}, [x4], x6        // 16 bytes store
    st1       {v6.16b}, [x4], #16       // 16 bytes store
    st1       {v6.16b}, [x4], x6        // 16 bytes store

    beq       end_func_r_c              ///* Branching when ht=4*/

    ldrh      w8, [x0]
    add       x0, x0, x1
    dup       v0.8h, w8
    ldrh      w9, [x0]
    add       x0, x0, x1
    ldrh      w10, [x0]
    add       x0, x0, x1
    st1       {v0.16b}, [x4], #16       // 16 bytes store
    dup       v2.8h, w9
    st1       {v0.16b}, [x4], x6        // 16 bytes store
    ldrh      w11, [x0]
    add       x0, x0, x1
    st1       {v2.16b}, [x4], #16       // 16 bytes store
    dup       v4.8h, w10
    st1       {v2.16b}, [x4], x6        // 16 bytes store
    st1       {v4.16b}, [x4], #16       // 16 bytes store
    dup       v6.8h, w11
    st1       {v4.16b}, [x4], x6        // 16 bytes store
    subs      w2, w2, #4
    st1       {v6.16b}, [x4], #16       // 16 bytes store
    st1       {v6.16b}, [x4], x6        // 16 bytes store

    beq       end_func_r_c              ///* Branching when ht=8*/
    bne       loop_32_r_c
    ldrh      w8, [x0]
    add       x0, x0, x1
    dup       v0.8h, w8
    ldrh      w9, [x0]
    add       x0, x0, x1
    ldrh      w10, [x0]
    add       x0, x0, x1
    st1       {v0.16b}, [x4], #16       // 16 bytes store
    dup       v2.8h, w9
    st1       {v0.16b}, [x4], x6        // 16 bytes store
    ldrh      w11, [x0]
    add       x0, x0, x1
    st1       {v2.16b}, [x4], #16       // 16 bytes store
    dup       v4.8h, w10
    st1       {v2.16b}, [x4], x6        // 16 bytes store
    st1       {v4.16b}, [x4], #16       // 16 bytes store
    dup       v6.8h, w11
    st1       {v4.16b}, [x4], x6        // 16 bytes store
    st1       {v6.16b}, [x4], #16       // 16 bytes store
    st1       {v6.16b}, [x4], x6        // 16 bytes store

end_func_r_c:
    // LDMFD sp!,{x4-x11,pc}                //Reload the registers from SP
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret






