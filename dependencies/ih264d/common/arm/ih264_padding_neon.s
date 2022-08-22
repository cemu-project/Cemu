@/******************************************************************************
@ *
@ * Copyright (C) 2015 The Android Open Source Project
@ *
@ * Licensed under the Apache License, Version 2.0 (the "License");
@ * you may not use this file except in compliance with the License.
@ * You may obtain a copy of the License at:
@ *
@ * http://www.apache.org/licenses/LICENSE-2.0
@ *
@ * Unless required by applicable law or agreed to in writing, software
@ * distributed under the License is distributed on an "AS IS" BASIS,
@ * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
@ * See the License for the specific language governing permissions and
@ * limitations under the License.
@ *
@ *****************************************************************************
@ * Originally developed and contributed by Ittiam Systems Pvt. Ltd, Bangalore
@*/
@*
@ *******************************************************************************
@ * @file
@ *  ih264_padding_neon.s
@ *
@ * @brief
@ *  Contains function definitions padding
@ *
@ * @author
@ *  Ittiam
@ *
@ * @par List of Functions:
@ *  - ih264_pad_top_a9q()
@ *  - ih264_pad_left_luma_a9q()
@ *  - ih264_pad_left_chroma_a9q()
@ *  - ih264_pad_right_luma_a9q()
@ *  - ih264_pad_right_chroma_a9q()
@ *
@ * @remarks
@ *  None
@ *
@ *******************************************************************************
@*


@**
@*******************************************************************************
@*
@* @brief pad at the top of a 2d array
@*
@* @par Description:
@*  The top row of a 2d array is replicated for pad_size times at the top
@*
@* @param[in] pu1_src
@*  UWORD8 pointer to the source
@*
@* @param[in] src_strd
@*  integer source stride
@*
@* @param[in] wd
@*  integer width of the array
@*
@* @param[in] pad_size
@*  integer -padding size of the array
@*
@* @returns none
@*
@* @remarks none
@*
@*******************************************************************************
@*
@void ih264_pad_top(UWORD8 *pu1_src,
@                   WORD32 src_strd,
@                   WORD32 wd,
@                   WORD32 pad_size)
@**************Variables Vs Registers*************************
@   r0 => *pu1_src
@   r1 => src_strd
@   r2 => wd
@   r3 => pad_size

.text
.p2align 2

    .global ih264_pad_top_a9q

ih264_pad_top_a9q:

    stmfd         sp!, {r4-r11, lr}     @stack stores the values of the arguments

    sub           r5, r0, r1
    neg           r6, r1

loop_neon_memcpy_mul_16:
    @ Load 16 bytes
    vld1.8        {d0, d1}, [r0]!
    mov           r4, r5
    mov           r7, r3
    add           r5, r5, #16

loop_neon_pad_top:
    vst1.8        {d0, d1}, [r4], r6
    subs          r7, r7, #1
    bne           loop_neon_pad_top

    subs          r2, r2, #16
    bne           loop_neon_memcpy_mul_16

    ldmfd         sp!, {r4-r11, pc}     @Reload the registers from SP




@**
@*******************************************************************************
@*
@* @brief
@*   Padding (luma block) at the left of a 2d array
@*
@* @par Description:
@*   The left column of a 2d array is replicated for pad_size times at the left
@*
@*
@* @param[in] pu1_src
@*  UWORD8 pointer to the source
@*
@* @param[in] src_strd
@*  integer source stride
@*
@* @param[in] ht
@*  integer height of the array
@*
@* @param[in] wd
@*  integer width of the array
@*
@* @param[in] pad_size
@*  integer -padding size of the array
@*
@* @param[in] ht
@*  integer height of the array
@*
@* @param[in] wd
@*  integer width of the array
@*
@* @returns
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@*
@#if PAD_LEFT_LUMA == C
@void ih264_pad_left_luma(UWORD8 *pu1_src,
@                        WORD32 src_strd,
@                        WORD32 ht,
@                        WORD32 pad_size)
@**************Variables Vs Registers*************************
@   r0 => *pu1_src
@   r1 => src_strd
@   r2 => ht
@   r3 => pad_size



    .global ih264_pad_left_luma_a9q

ih264_pad_left_luma_a9q:

    stmfd         sp!, {r4-r11, lr}     @stack stores the values of the arguments


    sub           r4, r0, r3
    sub           r6, r1, #16
    subs          r5, r3, #16
    bne           loop_32
loop_16:                                @  /*hard coded for width=16  ,height =8,16*/
    ldrb          r8, [r0], r1
    ldrb          r9, [r0], r1
    vdup.u8       q0, r8
    ldrb          r10, [r0], r1
    vst1.8        {q0}, [r4], r1        @ 16 bytes store
    vdup.u8       q1, r9
    vst1.8        {q1}, [r4], r1        @ 16 bytes store
    ldrb          r11, [r0], r1
    vdup.u8       q2, r10
    vdup.u8       q3, r11
    vst1.8        {q2}, [r4], r1        @ 16 bytes store
    ldrb          r8, [r0], r1
    vst1.8        {q3}, [r4], r1        @ 16 bytes store
    ldrb          r9, [r0], r1
    vdup.u8       q0, r8
    ldrb          r10, [r0], r1
    vst1.8        {q0}, [r4], r1        @ 16 bytes store
    vdup.u8       q1, r9
    ldrb          r11, [r0], r1
    vst1.8        {q1}, [r4], r1        @ 16 bytes store
    vdup.u8       q2, r10
    vdup.u8       q3, r11
    subs          r2, r2, #8
    vst1.8        {q2}, [r4], r1        @ 16 bytes store
    vst1.8        {q3}, [r4], r1        @ 16 bytes store
    bne           loop_16
    b             end_func

loop_32:                                @  /*hard coded for width=32 ,height =8,16*/
    ldrb          r8, [r0], r1
    ldrb          r9, [r0], r1
    vdup.u8       q0, r8
    ldrb          r10, [r0], r1
    vst1.8        {q0}, [r4]!           @ 16 bytes store
    vdup.u8       q1, r9
    vst1.8        {q0}, [r4], r6
    vst1.8        {q1}, [r4]!           @ 16 bytes store
    vdup.u8       q2, r10
    vst1.8        {q1}, [r4], r6        @ 16 bytes store
    ldrb          r11, [r0], r1
    vst1.8        {q2}, [r4]!           @ 16 bytes store
    vdup.u8       q3, r11
    vst1.8        {q2}, [r4], r6        @ 16 bytes store
    ldrb          r8, [r0], r1
    vst1.8        {q3}, [r4]!           @ 16 bytes store
    vdup.u8       q0, r8
    ldrb          r9, [r0], r1
    vst1.8        {q3}, [r4], r6        @ 16 bytes store
    ldrb          r10, [r0], r1
    vst1.8        {q0}, [r4]!           @ 16 bytes store
    vdup.u8       q1, r9
    vst1.8        {q0}, [r4], r6        @ 16 bytes store
    ldrb          r11, [r0], r1
    vst1.8        {q1}, [r4]!           @ 16 bytes store
    vdup.u8       q2, r10
    vst1.8        {q1}, [r4], r6        @ 16 bytes store
    vst1.8        {q2}, [r4]!           @ 16 bytes store
    vdup.u8       q3, r11
    vst1.8        {q2}, [r4], r6        @ 16 bytes store
    subs          r2, r2, #8
    vst1.8        {q3}, [r4]!           @ 16 bytes store
    vst1.8        {q3}, [r4], r6        @ 16 bytes store
    bne           loop_32



end_func:
    ldmfd         sp!, {r4-r11, pc}     @Reload the registers from SP





@**
@*******************************************************************************
@*
@* @brief
@*   Padding (chroma block) at the left of a 2d array
@*
@* @par Description:
@*   The left column of a 2d array is replicated for pad_size times at the left
@*
@*
@* @param[in] pu1_src
@*  UWORD8 pointer to the source
@*
@* @param[in] src_strd
@*  integer source stride
@*
@* @param[in] ht
@*  integer height of the array
@*
@* @param[in] wd
@*  integer width of the array (each colour component)
@*
@* @param[in] pad_size
@*  integer -padding size of the array
@*
@* @param[in] ht
@*  integer height of the array
@*
@* @param[in] wd
@*  integer width of the array
@*
@* @returns
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@*
@#if PAD_LEFT_CHROMA == C
@void ih264_pad_left_chroma(UWORD8 *pu1_src,
@                            WORD32 src_strd,
@                            WORD32 ht,
@                            WORD32 pad_size)
@{
@   r0 => *pu1_src
@   r1 => src_strd
@   r2 => ht
@   r3 => pad_size



    .global ih264_pad_left_chroma_a9q

ih264_pad_left_chroma_a9q:

    stmfd         sp!, {r4-r11, lr}     @stack stores the values of the arguments

    sub           r4, r0, r3
    sub           r6, r1, #16


loop_32_l_c:                            @  /*hard coded for width=32  ,height =4,8,12*/
    ldrh          r8, [r0], r1
    ldrh          r9, [r0], r1
    vdup.u16      q0, r8
    ldrh          r10, [r0], r1
    vst1.8        {q0}, [r4]!           @ 16 bytes store
    vdup.u16      q1, r9
    vst1.8        {q0}, [r4], r6        @ 16 bytes store
    ldrh          r11, [r0], r1
    vst1.8        {q1}, [r4]!           @ 16 bytes store
    vdup.u16      q2, r10
    vst1.8        {q1}, [r4], r6        @ 16 bytes store
    vdup.u16      q3, r11
    vst1.8        {q2}, [r4]!           @ 16 bytes store
    vst1.8        {q2}, [r4], r6        @ 16 bytes store
    subs          r2, r2, #4
    vst1.8        {q3}, [r4]!           @ 16 bytes store
    vst1.8        {q3}, [r4], r6        @ 16 bytes store


    beq           end_func_l_c          @/* Branching when ht=4*/

    ldrh          r8, [r0], r1
    ldrh          r9, [r0], r1
    vdup.u16      q0, r8
    ldrh          r10, [r0], r1
    vst1.8        {q0}, [r4]!           @ 16 bytes store
    vdup.u16      q1, r9
    vst1.8        {q0}, [r4], r6
    ldrh          r11, [r0], r1
    vst1.8        {q1}, [r4]!           @ 16 bytes store
    vdup.u16      q2, r10
    vst1.8        {q1}, [r4], r6        @ 16 bytes store
    vdup.u16      q3, r11
    vst1.8        {q2}, [r4]!           @ 16 bytes store
    vst1.8        {q2}, [r4], r6        @ 16 bytes store
    subs          r2, r2, #4
    vst1.8        {q3}, [r4]!           @ 16 bytes store
    vst1.8        {q3}, [r4], r6        @ 16 bytes store

    beq           end_func_l_c          @/* Branching when ht=8*/
    bne           loop_32_l_c

    ldrh          r8, [r0], r1
    ldrh          r9, [r0], r1
    vdup.u16      q0, r8
    ldrh          r10, [r0], r1
    vst1.8        {q0}, [r4]!           @ 16 bytes store
    vdup.u16      q1, r9
    vst1.8        {q0}, [r4], r6
    ldrh          r11, [r0], r1
    vst1.8        {q1}, [r4]!           @ 16 bytes store
    vdup.u16      q2, r10
    vst1.8        {q1}, [r4], r6        @ 16 bytes store
    vdup.u16      q3, r11
    vst1.8        {q2}, [r4]!           @ 16 bytes store
    vst1.8        {q2}, [r4], r6        @ 16 bytes store
    vst1.8        {q3}, [r4]!           @ 16 bytes store
    vst1.8        {q3}, [r4], r6        @ 16 bytes store

end_func_l_c:
    ldmfd         sp!, {r4-r11, pc}     @Reload the registers from SP





@**
@*******************************************************************************
@*
@* @brief
@* Padding (luma block) at the right of a 2d array
@*
@* @par Description:
@* The right column of a 2d array is replicated for pad_size times at the right
@*
@*
@* @param[in] pu1_src
@*  UWORD8 pointer to the source
@*
@* @param[in] src_strd
@*  integer source stride
@*
@* @param[in] ht
@*  integer height of the array
@*
@* @param[in] wd
@*  integer width of the array
@*
@* @param[in] pad_size
@*  integer -padding size of the array
@*
@* @param[in] ht
@*  integer height of the array
@*
@* @param[in] wd
@*  integer width of the array
@*
@* @returns
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@*
@#if PAD_RIGHT_LUMA == C
@void ih264_pad_right_luma(UWORD8 *pu1_src,
@                        WORD32 src_strd,
@                        WORD32 ht,
@                        WORD32 pad_size)
@{
@    WORD32 row;
@
@    for(row = 0; row < ht; row++)
@    {
@        memset(pu1_src, *(pu1_src -1), pad_size);
@
@        pu1_src += src_strd;
@    }
@}
@
@   r0 => *pu1_src
@   r1 => src_strd
@   r2 => ht
@   r3 => pad_size



    .global ih264_pad_right_luma_a9q

ih264_pad_right_luma_a9q:

    stmfd         sp!, {r4-r11, lr}     @stack stores the values of the arguments

    mov           r4, r0
    sub           r6, r1, #16
    sub           r0, r0, #1
    subs          r5, r3, #16
    bne           loop_32
loop_16_r: @  /*hard coded for width=16  ,height =8,16*/
    ldrb          r8, [r0], r1
    ldrb          r9, [r0], r1
    vdup.u8       q0, r8
    ldrb          r10, [r0], r1
    vst1.8        {q0}, [r4], r1        @ 16 bytes store
    vdup.u8       q1, r9
    vst1.8        {q1}, [r4], r1        @ 16 bytes store
    ldrb          r11, [r0], r1
    vdup.u8       q2, r10
    vdup.u8       q3, r11
    vst1.8        {q2}, [r4], r1        @ 16 bytes store
    ldrb          r8, [r0], r1
    vst1.8        {q3}, [r4], r1        @ 16 bytes store
    ldrb          r9, [r0], r1
    vdup.u8       q0, r8
    ldrb          r10, [r0], r1
    vst1.8        {q0}, [r4], r1        @ 16 bytes store
    vdup.u8       q1, r9
    ldrb          r11, [r0], r1
    vst1.8        {q1}, [r4], r1        @ 16 bytes store
    vdup.u8       q2, r10
    vdup.u8       q3, r11
    subs          r2, r2, #8
    vst1.8        {q2}, [r4], r1        @ 16 bytes store
    vst1.8        {q3}, [r4], r1        @ 16 bytes store
    bne           loop_16_r
    b             end_func_r

loop_32_r:                              @  /*hard coded for width=32  ,height =8,16*/
    ldrb          r8, [r0], r1
    ldrb          r9, [r0], r1
    vdup.u8       q0, r8
    ldrb          r10, [r0], r1
    vst1.8        {q0}, [r4]!           @ 16 bytes store
    vdup.u8       q1, r9
    vst1.8        {q0}, [r4], r6
    vst1.8        {q1}, [r4]!           @ 16 bytes store
    vdup.u8       q2, r10
    vst1.8        {q1}, [r4], r6        @ 16 bytes store
    ldrb          r11, [r0], r1
    vst1.8        {q2}, [r4]!           @ 16 bytes store
    vdup.u8       q3, r11
    vst1.8        {q2}, [r4], r6        @ 16 bytes store
    ldrb          r8, [r0], r1
    vst1.8        {q3}, [r4]!           @ 16 bytes store
    ldrb          r9, [r0], r1
    vdup.u8       q0, r8
    vst1.8        {q3}, [r4], r6        @ 16 bytes store
    ldrb          r10, [r0], r1
    vst1.8        {q0}, [r4]!           @ 16 bytes store
    vdup.u8       q1, r9
    vst1.8        {q0}, [r4], r6        @ 16 bytes store
    ldrb          r11, [r0], r1
    vst1.8        {q1}, [r4]!           @ 16 bytes store
    vdup.u8       q2, r10
    vst1.8        {q1}, [r4], r6        @ 16 bytes store
    vst1.8        {q2}, [r4]!           @ 16 bytes store
    vdup.u8       q3, r11
    vst1.8        {q2}, [r4], r6        @ 16 bytes store
    subs          r2, r2, #8
    vst1.8        {q3}, [r4]!           @ 16 bytes store
    vst1.8        {q3}, [r4], r6        @ 16 bytes store
    bne           loop_32_r



end_func_r:
    ldmfd         sp!, {r4-r11, pc}     @Reload the registers from SP





@**
@*******************************************************************************
@*
@* @brief
@;* Padding (chroma block) at the right of a 2d array
@*
@* @par Description:
@* The right column of a 2d array is replicated for pad_size times at the right
@*
@*
@* @param[in] pu1_src
@;*  UWORD8 pointer to the source
@*
@* @param[in] src_strd
@*  integer source stride
@*
@* @param[in] ht
@;*  integer height of the array
@*
@* @param[in] wd
@*  integer width of the array (each colour component)
@*
@* @param[in] pad_size
@*  integer -padding size of the array
@*
@* @param[in] ht
@;*  integer height of the array
@*
@* @param[in] wd
@*  integer width of the array
@*
@* @returns
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@*
@#if PAD_RIGHT_CHROMA == C
@void ih264_pad_right_chroma(UWORD8 *pu1_src,
@                        WORD32 src_strd,
@                        WORD32 ht,
@                        WORD32 pad_size)
@   r0 => *pu1_src
@   r1 => src_strd
@   r2 => ht
@   r3 => pad_size



    .global ih264_pad_right_chroma_a9q

ih264_pad_right_chroma_a9q:

    stmfd         sp!, {r4-r11, lr}     @stack stores the values of the arguments

    mov           r4, r0
    sub           r6, r1, #16
    sub           r0, r0, #2
loop_32_r_c: @  /*hard coded for width=32 ,height =8,4*/
    ldrh          r8, [r0], r1
    ldrh          r9, [r0], r1
    vdup.u16      q0, r8
    ldrh          r10, [r0], r1
    vst1.8        {q0}, [r4]!           @ 16 bytes store
    vdup.u16      q1, r9
    vst1.8        {q0}, [r4], r6
    vst1.8        {q1}, [r4]!           @ 16 bytes store
    vdup.u16      q2, r10
    vst1.8        {q1}, [r4], r6        @ 16 bytes store
    subs          r2, r2, #4
    ldrh          r11, [r0], r1
    vst1.8        {q2}, [r4]!           @ 16 bytes store
    vdup.u16      q3, r11
    vst1.8        {q2}, [r4], r6        @ 16 bytes store
    vst1.8        {q3}, [r4]!           @ 16 bytes store
    vst1.8        {q3}, [r4], r6        @ 16 bytes store

    beq           end_func_r_c          @/* Branching when ht=4*/

    ldrh          r8, [r0], r1
    vdup.u16      q0, r8
    ldrh          r9, [r0], r1
    ldrh          r10, [r0], r1
    vst1.8        {q0}, [r4]!           @ 16 bytes store
    vdup.u16      q1, r9
    vst1.8        {q0}, [r4], r6        @ 16 bytes store
    ldrh          r11, [r0], r1
    vst1.8        {q1}, [r4]!           @ 16 bytes store
    vdup.u16      q2, r10
    vst1.8        {q1}, [r4], r6        @ 16 bytes store
    vst1.8        {q2}, [r4]!           @ 16 bytes store
    vdup.u16      q3, r11
    vst1.8        {q2}, [r4], r6        @ 16 bytes store
    subs          r2, r2, #4
    vst1.8        {q3}, [r4]!           @ 16 bytes store
    vst1.8        {q3}, [r4], r6        @ 16 bytes store

    beq           end_func_r_c          @/* Branching when ht=8*/
    bne           loop_32_r_c

    ldrh          r8, [r0], r1
    vdup.u16      q0, r8
    ldrh          r9, [r0], r1
    ldrh          r10, [r0], r1
    vst1.8        {q0}, [r4]!           @ 16 bytes store
    vdup.u16      q1, r9
    vst1.8        {q0}, [r4], r6        @ 16 bytes store
    ldrh          r11, [r0], r1
    vst1.8        {q1}, [r4]!           @ 16 bytes store
    vdup.u16      q2, r10
    vst1.8        {q1}, [r4], r6        @ 16 bytes store
    vst1.8        {q2}, [r4]!           @ 16 bytes store
    vdup.u16      q3, r11
    vst1.8        {q2}, [r4], r6        @ 16 bytes store
    vst1.8        {q3}, [r4]!           @ 16 bytes store
    vst1.8        {q3}, [r4], r6        @ 16 bytes store

end_func_r_c:
    ldmfd         sp!, {r4-r11, pc}     @Reload the registers from SP





