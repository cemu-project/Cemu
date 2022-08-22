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
// *  ih264_mem_fns_neon.s
// *
// * @brief
// *  Contains function definitions for memory manipulation
// *
// * @author
// *     Naveen SR
// *
// * @par List of Functions:
// *  - ih264_memcpy_av8()
// *  - ih264_memcpy_mul_8_av8()
// *  - ih264_memset_mul_8_av8()
// *  - ih264_memset_16bit_mul_8_av8()
// *  - ih264_memset_16bit_av8()
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
//* @brief
//*   memcpy of a 1d array
//*
//* @par Description:
//*   Does memcpy of 8bit data from source to destination for 8,16 or 32 number of bytes
//*
//* @param[in] pu1_dst
//*  UWORD8 pointer to the destination
//*
//* @param[in] pu1_src
//*  UWORD8 pointer to the source
//*
//* @param[in] num_bytes
//*  number of bytes to copy
//* @returns
//*
//* @remarks
//*  None
//*
//*******************************************************************************
//*/
//void ih264_memcpy_mul_8(UWORD8 *pu1_dst,
//                      UWORD8 *pu1_src,
//                      UWORD32 num_bytes)
//**************Variables Vs Registers*************************
//    x0 => *pu1_dst
//    x1 => *pu1_src
//    w2 => num_bytes





    .global ih264_memcpy_mul_8_av8

ih264_memcpy_mul_8_av8:

loop_neon_memcpy_mul_8:
    // Memcpy 8 bytes
    ld1       {v0.8b}, [x1], #8
    st1       {v0.8b}, [x0], #8

    subs      w2, w2, #8
    bne       loop_neon_memcpy_mul_8
    ret



//*******************************************************************************
//*/
//void ih264_memcpy(UWORD8 *pu1_dst,
//                  UWORD8 *pu1_src,
//                  UWORD32 num_bytes)
//**************Variables Vs Registers*************************
//    x0 => *pu1_dst
//    x1 => *pu1_src
//    w2 => num_bytes



    .global ih264_memcpy_av8

ih264_memcpy_av8:
    subs      w2, w2, #8
    blt       arm_memcpy
loop_neon_memcpy:
    // Memcpy 8 bytes
    ld1       {v0.8b}, [x1], #8
    st1       {v0.8b}, [x0], #8

    subs      w2, w2, #8
    bge       loop_neon_memcpy
    cmn       w2, #8
    beq       end_func1

arm_memcpy:
    add       w2, w2, #8

loop_arm_memcpy:
    ldrb      w3, [x1], #1
    strb      w3, [x0], #1
    subs      w2, w2, #1
    bne       loop_arm_memcpy
    ret
end_func1:
    ret


//void ih264_memset_mul_8(UWORD8 *pu1_dst,
//                       UWORD8 value,
//                       UWORD32 num_bytes)
//**************Variables Vs Registers*************************
//    x0 => *pu1_dst
//    x1 => value
//    x2 => num_bytes


    .global ih264_memset_mul_8_av8

ih264_memset_mul_8_av8:

// Assumptions: numbytes is either 8, 16 or 32
    dup       v0.8b, w1
loop_memset_mul_8:
    // Memset 8 bytes
    st1       {v0.8b}, [x0], #8

    subs      w2, w2, #8
    bne       loop_memset_mul_8

    ret


//void ih264_memset(UWORD8 *pu1_dst,
//                       UWORD8 value,
//                       UWORD32 num_bytes)
//**************Variables Vs Registers*************************
//    x0 => *pu1_dst
//    w1 => value
//    w2 => num_bytes



    .global ih264_memset_av8

ih264_memset_av8:
    subs      w2, w2, #8
    blt       arm_memset
    dup       v0.8b, w1
loop_neon_memset:
    // Memcpy 8 bytes
    st1       {v0.8b}, [x0], #8

    subs      w2, w2, #8
    bge       loop_neon_memset
    cmn       w2, #8
    beq       end_func2

arm_memset:
    add       w2, w2, #8

loop_arm_memset:
    strb      w1, [x0], #1
    subs      w2, w2, #1
    bne       loop_arm_memset
    ret
end_func2:
    ret





//void ih264_memset_16bit_mul_8(UWORD16 *pu2_dst,
//                                      UWORD16 value,
//                                      UWORD32 num_words)
//**************Variables Vs Registers*************************
//    x0 => *pu2_dst
//    w1 => value
//    w2 => num_words


    .global ih264_memset_16bit_mul_8_av8

ih264_memset_16bit_mul_8_av8:

// Assumptions: num_words is either 8, 16 or 32

    // Memset 8 words
    dup       v0.4h, w1
loop_memset_16bit_mul_8:
    st1       {v0.4h}, [x0], #8
    st1       {v0.4h}, [x0], #8

    subs      w2, w2, #8
    bne       loop_memset_16bit_mul_8

    ret



//void ih264_memset_16bit(UWORD16 *pu2_dst,
//                       UWORD16 value,
//                       UWORD32 num_words)
//**************Variables Vs Registers*************************
//    x0 => *pu2_dst
//    w1 => value
//    w2 => num_words



    .global ih264_memset_16bit_av8

ih264_memset_16bit_av8:
    subs      w2, w2, #8
    blt       arm_memset_16bit
    dup       v0.4h, w1
loop_neon_memset_16bit:
    // Memset 8 words
    st1       {v0.4h}, [x0], #8
    st1       {v0.4h}, [x0], #8

    subs      w2, w2, #8
    bge       loop_neon_memset_16bit
    cmn       w2, #8
    beq       end_func3

arm_memset_16bit:
    add       w2, w2, #8

loop_arm_memset_16bit:
    strh      w1, [x0], #2
    subs      w2, w2, #1
    bne       loop_arm_memset_16bit
    ret

end_func3:
    ret



