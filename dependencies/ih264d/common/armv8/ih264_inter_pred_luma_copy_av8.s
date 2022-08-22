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
///**
//*******************************************************************************
//*
//* @brief
//*     Interprediction luma function for copy
//*
//* @par Description:
//*   Copies the array of width 'wd' and height 'ht' from the  location pointed
//*   by 'src' to the location pointed by 'dst'
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
//void ih264_inter_pred_luma_copy (
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



    .global ih264_inter_pred_luma_copy_av8

ih264_inter_pred_luma_copy_av8:

    push_v_regs
    stp       x19, x20, [sp, #-16]!
    sxtw      x2, w2
    sxtw      x3, w3
    sxtw      x4, w4
    sxtw      x5, w5

    mov       x12, x5
    mov       x7, x4
    cmp       x7, #0                    //checks ht == 0
    ble       end_loops
    tst       x12, #15                  //checks wd for multiples for 4 & 8
    beq       core_loop_wd_16
    tst       x12, #7                   //checks wd for multiples for 4 & 8
    beq       core_loop_wd_8
    sub       x11, x12, #4

outer_loop_wd_4:
    subs      x4, x12, #0               //checks wd == 0
    ble       end_inner_loop_wd_4

inner_loop_wd_4:
    ld1       {v0.s}[0], [x0]           //vld1_lane_u32((uint32_t *)pu1_src_tmp, src_tmp, 0)
    add       x5, x0, x2                //pu1_src_tmp += src_strd
    add       x6, x1, x3                //pu1_dst_tmp += dst_strd
    st1       {v0.s}[0], [x1]           //vst1_lane_u32((uint32_t *)pu1_dst_tmp, src_tmp, 0)
    ld1       {v0.s}[0], [x5], x2       //vld1_lane_u32((uint32_t *)pu1_src_tmp, src_tmp, 0)
    add       x0, x0, #4                //pu1_src += 4
    st1       {v0.s}[0], [x6], x3       //vst1_lane_u32((uint32_t *)pu1_dst_tmp, src_tmp, 0)
    ld1       {v0.s}[0], [x5], x2       //vld1_lane_u32((uint32_t *)pu1_src_tmp, src_tmp, 0)
    subs      x4, x4, #4                //(wd -4)
    st1       {v0.s}[0], [x6], x3       //vst1_lane_u32((uint32_t *)pu1_dst_tmp, src_tmp, 0)
    ld1       {v0.s}[0], [x5], x2       //vld1_lane_u32((uint32_t *)pu1_src_tmp, src_tmp, 0)
    add       x1, x1, #4                //pu1_dst += 4
    st1       {v0.s}[0], [x6], x3       //vst1_lane_u32((uint32_t *)pu1_dst_tmp, src_tmp, 0)

    bgt       inner_loop_wd_4

end_inner_loop_wd_4:
    subs      x7, x7, #4                //ht - 4
    sub       x0, x5, x11               //pu1_src = pu1_src_tmp
    sub       x1, x6, x11               //pu1_dst = pu1_dst_tmp
    bgt       outer_loop_wd_4

end_loops:
    // LDMFD sp!,{x4-x12,x15}                  //Reload the registers from SP
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret


core_loop_wd_8:
    sub       x11, x12, #8

outer_loop_wd_8:
    subs      x4, x12, #0               //checks wd
    ble       end_inner_loop_wd_8

inner_loop_wd_8:
    add       x5, x0, x2                //pu1_src_tmp += src_strd
    ld1       {v0.8b}, [x0], #8         //vld1_u8(pu1_src_tmp)
    add       x6, x1, x3                //pu1_dst_tmp += dst_strd
    st1       {v0.8b}, [x1], #8         //vst1_u8(pu1_dst_tmp, tmp_src)
    ld1       {v1.8b}, [x5], x2         //vld1_u8(pu1_src_tmp)
    st1       {v1.8b}, [x6], x3         //vst1_u8(pu1_dst_tmp, tmp_src)
    subs      x4, x4, #8                //wd - 8(Loop condition)
    ld1       {v2.8b}, [x5], x2         //vld1_u8(pu1_src_tmp)
    st1       {v2.8b}, [x6], x3         //vst1_u8(pu1_dst_tmp, tmp_src)
    ld1       {v3.8b}, [x5], x2         //vld1_u8(pu1_src_tmp)
    st1       {v3.8b}, [x6], x3         //vst1_u8(pu1_dst_tmp, tmp_src)
    bgt       inner_loop_wd_8

end_inner_loop_wd_8:
    subs      x7, x7, #4                //ht -= 4
    sub       x0, x5, x11               //pu1_src = pu1_src_tmp
    sub       x1, x6, x11               //pu1_dst = pu1_dst_tmp
    bgt       outer_loop_wd_8

    // LDMFD sp!,{x4-x12,x15}                  //Reload the registers from SP
    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret

core_loop_wd_16:
    sub       x11, x12, #16

outer_loop_wd_16:
    subs      x4, x12, #0               //checks wd
    ble       end_inner_loop_wd_16

inner_loop_wd_16:
    add       x5, x0, x2                //pu1_src_tmp += src_strd
    ld1       { v0.16b}, [x0], #16      //vld1_u8(pu1_src_tmp)
    add       x6, x1, x3                //pu1_dst_tmp += dst_strd
    st1       { v0.16b}, [x1], #16      //vst1_u8(pu1_dst_tmp, tmp_src)
    ld1       { v2.16b}, [x5], x2       //vld1_u8(pu1_src_tmp)
    st1       { v2.16b}, [x6], x3       //vst1_u8(pu1_dst_tmp, tmp_src)
    subs      x4, x4, #16               //wd - 8(Loop condition)
    ld1       { v4.16b}, [x5], x2       //vld1_u8(pu1_src_tmp)
    st1       { v4.16b}, [x6], x3       //vst1_u8(pu1_dst_tmp, tmp_src)
    ld1       { v6.16b}, [x5], x2       //vld1_u8(pu1_src_tmp)
    st1       { v6.16b}, [x6], x3       //vst1_u8(pu1_dst_tmp, tmp_src)
    bgt       inner_loop_wd_16

end_inner_loop_wd_16:
    subs      x7, x7, #4                //ht -= 4
    sub       x0, x5, x11               //pu1_src = pu1_src_tmp
    sub       x1, x6, x11               //pu1_dst = pu1_dst_tmp
    bgt       outer_loop_wd_16


    ldp       x19, x20, [sp], #16
    pop_v_regs
    ret


// /*
// ********************************************************************************
// *
// * @brief This function copies a 4x4 block to destination
// *
// * @par Description:
// * Copies a 4x4 block to destination, where both src and dst are interleaved
// *
// * @param[in] pi2_src
// *  Source
// *
// * @param[in] pu1_out
// *  Output pointer
// *
// * @param[in] pred_strd,
// *  Prediction buffer stride
// *
// * @param[in] out_strd
// *  output buffer buffer Stride
// *
// * @returns none
// *
// * @remarks none
// * Currently wd and height is not used, ie a 4x4 block is always copied
// *
// *******************************************************************************
// */
// void ih264_interleave_copy(WORD16 *pi2_src,
//                            UWORD8 *pu1_out,
//                            WORD32 pred_strd,
//                            WORD32 out_strd
//                            WORD32 wd
//                            WORD32 ht)
// Register Usage
// x0 : pi2_src
// x1 : pu1_out
// w2 : src_strd
// w3 : out_strd
// Neon registers d0-d7, d16-d30 are used
// No need for pushing  arm and neon registers

    .global ih264_interleave_copy_av8
ih264_interleave_copy_av8:
    push_v_regs
    sxtw      x2, w2
    sxtw      x3, w3
    ld1       {v2.8b}, [x0], x2         //load src plane 1 => d2 &pred palne 2 => d3
    ld1       {v3.8b}, [x0], x2
    mov       v2.d[1], v3.d[0]
    ld1       {v4.8b}, [x0], x2
    ld1       {v5.8b}, [x0], x2
    mov       v4.d[1], v5.d[0]

    mov       x0, x1

    ld1       {v18.8b}, [x1], x3        //load out [8 bit size) -8 coeffs
    ld1       {v19.8b}, [x1], x3
    mov       v18.d[1], v19.d[0]
    movi      v30.8h, #0x00ff
    ld1       {v20.8b}, [x1], x3
    ld1       {v21.8b}, [x1], x3
    mov       v20.d[1], v21.d[0]

    bit       v18.16b, v2.16b , v30.16b
    bit       v20.16b, v4.16b , v30.16b

    st1       {v18.8b}, [x0], x3        //store  out
    st1       {v18.d}[1], [x0], x3
    st1       {v20.8b}, [x0], x3
    st1       {v20.d}[1], [x0], x3

    pop_v_regs
    ret


