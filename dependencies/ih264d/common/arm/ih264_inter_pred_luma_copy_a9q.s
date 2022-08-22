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
@**
@**
@*******************************************************************************
@*
@* @brief
@*     Interprediction luma function for copy
@*
@* @par Description:
@*   Copies the array of width 'wd' and height 'ht' from the  location pointed
@*   by 'src' to the location pointed by 'dst'
@*
@* @param[in] pu1_src
@*  UWORD8 pointer to the source
@*
@* @param[out] pu1_dst
@*  UWORD8 pointer to the destination
@*
@* @param[in] src_strd
@*  integer source stride
@*
@* @param[in] dst_strd
@*  integer destination stride
@*
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
@void ih264_inter_pred_luma_copy (
@                            UWORD8 *pu1_src,
@                            UWORD8 *pu1_dst,
@                            WORD32 src_strd,
@                            WORD32 dst_strd,
@                            WORD32 ht,
@                            WORD32 wd   )

@**************Variables Vs Registers*****************************************
@   r0 => *pu1_src
@   r1 => *pu1_dst
@   r2 =>  src_strd
@   r3 =>  dst_strd
@   r7 =>  ht
@   r12 => wd

.text
.p2align 2

    .global ih264_inter_pred_luma_copy_a9q

ih264_inter_pred_luma_copy_a9q:
    stmfd         sp!, {r4-r12, r14}    @stack stores the values of the arguments
    vstmdb        sp!, {d8-d15}         @push neon registers to stack
    ldr           r12, [sp, #108]       @Loads wd
    ldr           r7, [sp, #104]        @Loads ht
    cmp           r7, #0                @checks ht == 0
    ble           end_loops
    tst           r12, #15              @checks wd for multiples for 4 & 8
    beq           core_loop_wd_16
    tst           r12, #7               @checks wd for multiples for 4 & 8
    beq           core_loop_wd_8
    sub           r11, r12, #4

outer_loop_wd_4:
    subs          r4, r12, #0           @checks wd == 0
    ble           end_inner_loop_wd_4

inner_loop_wd_4:
    vld1.32       {d0[0]}, [r0]         @vld1_lane_u32((uint32_t *)pu1_src_tmp, src_tmp, 0)
    add           r5, r0, r2            @pu1_src_tmp += src_strd
    add           r6, r1, r3            @pu1_dst_tmp += dst_strd
    vst1.32       {d0[0]}, [r1]         @vst1_lane_u32((uint32_t *)pu1_dst_tmp, src_tmp, 0)
    vld1.32       {d0[0]}, [r5], r2     @vld1_lane_u32((uint32_t *)pu1_src_tmp, src_tmp, 0)
    add           r0, r0, #4            @pu1_src += 4
    vst1.32       {d0[0]}, [r6], r3     @vst1_lane_u32((uint32_t *)pu1_dst_tmp, src_tmp, 0)
    vld1.32       {d0[0]}, [r5], r2     @vld1_lane_u32((uint32_t *)pu1_src_tmp, src_tmp, 0)
    subs          r4, r4, #4            @(wd -4)
    vst1.32       {d0[0]}, [r6], r3     @vst1_lane_u32((uint32_t *)pu1_dst_tmp, src_tmp, 0)
    vld1.32       {d0[0]}, [r5], r2     @vld1_lane_u32((uint32_t *)pu1_src_tmp, src_tmp, 0)
    add           r1, r1, #4            @pu1_dst += 4
    vst1.32       {d0[0]}, [r6], r3     @vst1_lane_u32((uint32_t *)pu1_dst_tmp, src_tmp, 0)

    bgt           inner_loop_wd_4

end_inner_loop_wd_4:
    subs          r7, r7, #4            @ht - 4
    sub           r0, r5, r11           @pu1_src = pu1_src_tmp
    sub           r1, r6, r11           @pu1_dst = pu1_dst_tmp
    bgt           outer_loop_wd_4

end_loops:
    vldmia        sp!, {d8-d15}         @ Restore neon registers that were saved
    ldmfd         sp!, {r4-r12, r15}    @Reload the registers from SP



core_loop_wd_8:
    sub           r11, r12, #8

outer_loop_wd_8:
    subs          r4, r12, #0           @checks wd
    ble           end_inner_loop_wd_8

inner_loop_wd_8:
    add           r5, r0, r2            @pu1_src_tmp += src_strd
    vld1.8        {d0}, [r0]!           @vld1_u8(pu1_src_tmp)
    add           r6, r1, r3            @pu1_dst_tmp += dst_strd
    vst1.8        {d0}, [r1]!           @vst1_u8(pu1_dst_tmp, tmp_src)
    vld1.8        {d1}, [r5], r2        @vld1_u8(pu1_src_tmp)
    vst1.8        {d1}, [r6], r3        @vst1_u8(pu1_dst_tmp, tmp_src)
    subs          r4, r4, #8            @wd - 8(Loop condition)
    vld1.8        {d2}, [r5], r2        @vld1_u8(pu1_src_tmp)
    vst1.8        {d2}, [r6], r3        @vst1_u8(pu1_dst_tmp, tmp_src)
    vld1.8        {d3}, [r5], r2        @vld1_u8(pu1_src_tmp)
    vst1.8        {d3}, [r6], r3        @vst1_u8(pu1_dst_tmp, tmp_src)
    bgt           inner_loop_wd_8

end_inner_loop_wd_8:
    subs          r7, r7, #4            @ht -= 4
    sub           r0, r5, r11           @pu1_src = pu1_src_tmp
    sub           r1, r6, r11           @pu1_dst = pu1_dst_tmp
    bgt           outer_loop_wd_8

    vldmia        sp!, {d8-d15}         @ Restore neon registers that were saved
    ldmfd         sp!, {r4-r12, r15}    @Reload the registers from SP

core_loop_wd_16:
    sub           r11, r12, #16

outer_loop_wd_16:
    subs          r4, r12, #0           @checks wd
    ble           end_inner_loop_wd_16

inner_loop_wd_16:
    add           r5, r0, r2            @pu1_src_tmp += src_strd
    vld1.8        {q0}, [r0]!           @vld1_u8(pu1_src_tmp)
    add           r6, r1, r3            @pu1_dst_tmp += dst_strd
    vst1.8        {q0}, [r1]!           @vst1_u8(pu1_dst_tmp, tmp_src)
    vld1.8        {q1}, [r5], r2        @vld1_u8(pu1_src_tmp)
    vst1.8        {q1}, [r6], r3        @vst1_u8(pu1_dst_tmp, tmp_src)
    subs          r4, r4, #16           @wd - 8(Loop condition)
    vld1.8        {q2}, [r5], r2        @vld1_u8(pu1_src_tmp)
    vst1.8        {q2}, [r6], r3        @vst1_u8(pu1_dst_tmp, tmp_src)
    vld1.8        {q3}, [r5], r2        @vld1_u8(pu1_src_tmp)
    vst1.8        {q3}, [r6], r3        @vst1_u8(pu1_dst_tmp, tmp_src)
    bgt           inner_loop_wd_16

end_inner_loop_wd_16:
    subs          r7, r7, #4            @ht -= 4
    sub           r0, r5, r11           @pu1_src = pu1_src_tmp
    sub           r1, r6, r11           @pu1_dst = pu1_dst_tmp
    bgt           outer_loop_wd_16

    vldmia        sp!, {d8-d15}         @ Restore neon registers that were saved
    ldmfd         sp!, {r4-r12, r15}    @Reload the registers from SP


@ *
@ ********************************************************************************
@ *
@ * @brief This function copies a 4x4 block to destination
@ *
@ * @par Description:
@ * Copies a 4x4 block to destination, where both src and dst are interleaved
@ *
@ * @param[in] pi2_src
@ *  Source
@ *
@ * @param[in] pu1_out
@ *  Output pointer
@ *
@ * @param[in] pred_strd,
@ *  Prediction buffer stride
@ *
@ * @param[in] out_strd
@ *  output buffer buffer Stride
@ *
@ * @returns none
@ *
@ * @remarks none
@ * Currently wd and height is not used, ie a 4x4 block is always copied
@ *
@ *******************************************************************************
@ *
@ void ih264_interleave_copy(WORD16 *pi2_src,
@                            UWORD8 *pu1_out,
@                            WORD32 pred_strd,
@                            WORD32 out_strd
@                            WORD32 wd
@                            WORD32 ht)
@ Register Usage
@ r0 : pi2_src
@ r1 : pu1_out
@ r2 : src_strd
@ r3 : out_strd
@ Neon registers d0-d7, d16-d30 are used
@ No need for pushing  arm and neon registers

    .global ih264_interleave_copy_a9
ih264_interleave_copy_a9:

    vld1.u8       d2, [r0], r2          @load src plane 1 => d2 &pred palne 2 => d3
    vld1.u8       d3, [r0], r2
    vld1.u8       d4, [r0], r2
    vld1.u8       d5, [r0], r2

    mov           r0, r1

    vld1.u8       d18, [r1], r3         @load out [8 bit size) -8 coeffs
    vld1.u8       d19, [r1], r3
    vmov.u16      q15, #0x00ff
    vld1.u8       d20, [r1], r3
    vld1.u8       d21, [r1], r3

    vbit.u8       q9, q1, q15
    vbit.u8       q10, q2, q15

    vst1.u8       d18, [r0], r3         @store  out
    vst1.u8       d19, [r0], r3
    vst1.u8       d20, [r0], r3
    vst1.u8       d21, [r0], r3

    bx            lr



