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
@*******************************************************************************
@* @file
@*  ih264_resi_trans_quant_a9.s
@*
@* @brief
@*  Contains function definitions for residual and forward trans
@*
@* @author
@*  Ittiam
@*
@* @par List of Functions:
@*  ih264_resi_trans_quant_4x4_a9
@*  ih264_resi_trans_quant_8x8_a9
@*  ih264_resi_trans_quant_chroma_4x4_a9
@*  ih264_hadamard_quant_4x4_a9
@*  ih264_hadamard_quant_2x2_uv_a9
@*
@* @remarks
@*  None
@*
@*******************************************************************************


.text
.p2align 2
@*****************************************************************************
@*
@* Function Name     : ih264_resi_trans_quant_4x4_a9
@* Description       : This function does cf4 of H264
@*
@* Arguments         :  R0 :pointer to src buffer
@                       R1 :pointer to pred buffer
@                       R2 :pointer to dst buffer
@                       R3 :source stride
@                       STACK : pred stride,
@                               dst stride,
@                               pointer to scaling matrix,
@                               pointer to threshold matrix,
@                               qbits,
@                               rounding factor,
@                               pointer to store nnz
@                               pointer to store non quantized dc value
@ Values Returned   : NONE
@
@ Register Usage    :
@ Stack Usage       : 40 bytes
@ Cycles            : Around
@ Interruptiaility  : Interruptable
@
@ Known Limitations
@   \Assumptions    :
@
@ Revision History  :
@         DD MM YYYY    Author(s)   Changes
@         1 12 2013    100633      First version
@         20 1 2014    100633      Changes the API, Optimization
@
@*****************************************************************************

    .global ih264_resi_trans_quant_4x4_a9
ih264_resi_trans_quant_4x4_a9:

    @R0     :pointer to src buffer
    @R1     :pointer to pred buffer
    @R2     :pointer to dst buffer
    @R3     :Source stride
    @STACk  :pred stride
    @       :scale matirx,
    @       :threshold matrix
    @       :qbits
    @       :round factor
    @       :nnz

    push          {r4-r12, lr}          @push all the variables first

    add           r11, sp, #40          @decrement stack pointer,to accomodate two variables
    ldmfd         r11, {r4-r10}         @load the strides into registers

    @R0     :pointer to src buffer
    @R1     :pointer to pred buffer
    @R2     :pointer to dst buffer
    @R3     :Source stride
    @R4     :Pred stride
    @R5     :scale matirx,
    @R6     :threshold matrix
    @R7     :qbits
    @R8     :round factor
    @R9     :nnz

    vpush         {d8-d15}

    mov           r11, #0
    sub           r7, r11, r7           @Negate the qbit value for usiing LSL

    @------------Fucntion Loading done----------------;

    vld1.u8       d30, [r0], r3         @load first 8 pix src row 1

    vld1.u8       d31, [r1], r4         @load first 8 pix pred row 1

    vld1.u8       d28, [r0], r3         @load first 8 pix src row 2

    vld1.u8       d29, [r1], r4         @load first 8 pix pred row 2

    vld1.u8       d26, [r0], r3         @load first 8 pix src row 3

    vld1.u8       d27, [r1], r4         @load first 8 pix pred row 3
    vsubl.u8      q0, d30, d31          @find residue row 1

    vld1.u8       d24, [r0], r3         @load first 8 pix src row 4

    vld1.u8       d25, [r1], r4         @load first 8 pix pred row 4
    vsubl.u8      q1, d28, d29          @find residue row 2

    vsubl.u8      q2, d26, d27          @find residue row 3
    vsubl.u8      q3, d24, d25          @find residue row 4

    vtrn.16       d0, d2                @T12
    vtrn.16       d4, d6                @T23
    vtrn.32       d0, d4                @T13
    vtrn.32       d2, d6                @T14

    vadd.s16      d8 , d0, d6           @x0 = x4+x7
    vadd.s16      d9 , d2, d4           @x1 = x5+x6
    vsub.s16      d10, d2, d4           @x2 = x5-x6
    vsub.s16      d11, d0, d6           @x3 = x4-x7

    vshl.s16      d12, d10, #1          @U_SHIFT(x2,1,shft)
    vshl.s16      d13, d11, #1          @U_SHIFT(x3,1,shft)

    vadd.s16      d14, d8, d9           @x4 = x0 + x1;
    vsub.s16      d16, d8, d9           @x6 = x0 - x1;
    vadd.s16      d15, d13, d10         @x5 = U_SHIFT(x3,1,shft) + x2;
    vsub.s16      d17, d11, d12         @x7 = x3 - U_SHIFT(x2,1,shft);

    @taking transpose again so as to make do vert transform
    vtrn.16       d14, d15              @T12
    vtrn.16       d16, d17              @T23
    vtrn.32       d14, d16              @T13
    vtrn.32       d15, d17              @T24

    @let us do vertical transform
    @same code as horiz
    vadd.s16      d18, d14, d17         @x0 = x4+x7
    vadd.s16      d19, d15, d16         @x1 = x5+x6
    vsub.s16      d20, d15, d16         @x2 = x5-x6
    vsub.s16      d21, d14, d17         @x3 = x4-x7

    vshl.s16      d22, d20, #1          @U_SHIFT(x2,1,shft)
    vshl.s16      d23, d21, #1          @U_SHIFT(x3,1,shft)

    vdup.s32      q4, r8                @Load rounding value row 1

    vadd.s16      d24, d18, d19         @x5 = x0 + x1;
    vsub.s16      d26, d18, d19         @x7 = x0 - x1;
    vadd.s16      d25, d23, d20         @x6 = U_SHIFT(x3,1,shft) + x2;
    vsub.s16      d27, d21, d22         @x8 = x3 - U_SHIFT(x2,1,shft);
    vdup.s32      q10, r7               @Load qbit values

    vst1.s16      d24[0], [r10]         @Store the dc value to alternate dc sddress

@core tranform is done for 4x8 block 1
    vld1.s16      {q14-q15}, [r5]       @load the scaling values

    vabs.s16      q0, q12               @Abs val of row 1 blk 1

    vabs.s16      q1, q13               @Abs val of row 2 blk 1

    vmov.s32      q5, q4                @copy round fact for row 2

    vmov.s32      q6, q4                @copy round fact for row 2
    vclt.s16      q2, q12, #0           @Get the sign of row 1 blk 1

    vmov.s32      q7, q4                @copy round fact for row 2
    vclt.s16      q3, q13, #0           @Get the sign of row 2 blk 1

    vmlal.s16     q4, d0, d28           @Multiply and add row 1
    vmlal.s16     q5, d1, d29           @Multiply and add row 2
    vmlal.s16     q6, d2, d30           @Multiply and add row 3
    vmlal.s16     q7, d3, d31           @Multiply and add row 4

    vshl.s32      q11, q4, q10          @Shift row 1
    vshl.s32      q12, q5, q10          @Shift row 2
    vshl.s32      q13, q6, q10          @Shift row 3
    vshl.s32      q14, q7, q10          @Shift row 4

    vmovn.s32     d30, q11              @Narrow row 1
    vmovn.s32     d31, q12              @Narrow row 2
    vmovn.s32     d0 , q13              @Narrow row 3
    vmovn.s32     d1 , q14              @Narrow row 4

    vneg.s16      q1, q15               @Get negative
    vneg.s16      q4, q0                @Get negative

    vceq.s16      q5, q15, #0           @I  compare with zero row 1 and 2 blk 1
    vceq.s16      q6, q0 , #0           @I  compare with zero row 1 and 2 blk 1

    vbsl.s16      q2, q1, q15           @Restore sign of row 1 and 2
    vbsl.s16      q3, q4, q0            @Restore sign of row 3 and 4


    vmovn.u16     d14, q5               @I  Narrow the comparison for row 1 and 2 blk 1
    vmovn.u16     d15, q6               @I  Narrow the comparison for row 1 and 2 blk 2

    vshr.u8       q8, q7, #7            @I  Reduce comaparison bit to a signle bit row 1 and 2 blk  1 and 2 [ keep the value for later use ]

    vpadd.u8      d18, d16, d17         @I pair add nnz 1
    vpadd.u8      d20, d18, d19         @I Pair add nnz 2
    vpadd.u8      d22, d20, d21         @I Pair add nnz 3
    vpadd.u8      d24, d22, d23         @I Pair add nnz4
    vst1.s16      {q2-q3}, [r2]         @Store blk

    vmov.u8       d25, #16              @I Get max nnz
    vsub.u8       d26, d25, d24         @I invert current nnz

    vst1.u8       d26[0], [r9]          @I  Write nnz

    vpop          {d8-d15}
    pop           {r4-r12, pc}



@*****************************************************************************
@*
@* Function Name     : ih264_resi_trans_quant_chroma_4x4_a9
@* Description       : This function does residue calculation, forward transform
@*                     and quantization for 4x4 chroma block.
@*
@* Arguments         :  R0 :pointer to src buffer
@                       R1 :pointer to pred buffer
@                       R2 :pointer to dst buffer
@                       R3 :source stride
@                       STACK : pred stride,
@                               dst stride,
@                               pointer to scaling matrix,
@                               pointer to threshold matrix,
@                               qbits,
@                               rounding factor,
@                               pointer to store nnz
@                               pointer to store unquantized dc values
@ Values Returned   : NONE
@
@ Register Usage    :
@ Stack Usage       : 40 bytes
@ Cycles            : Around
@ Interruptiaility  : Interruptable
@
@ Known Limitations
@   \Assumptions    :
@
@ Revision History  :
@         DD MM YYYY    Author(s)   Changes
@         11 2 2015    100664      First version
@
@*****************************************************************************

    .global ih264_resi_trans_quant_chroma_4x4_a9
ih264_resi_trans_quant_chroma_4x4_a9:

    @R0     :pointer to src buffer
    @R1     :pointer to pred buffer
    @R2     :pointer to dst buffer
    @R3     :Source stride
    @STACk  :pred stride
    @       :scale matirx,
    @       :threshold matrix
    @       :qbits
    @       :round factor
    @       :nnz
    @       :pu1_dc_alt_addr
    push          {r4-r12, lr}          @push all the variables first

    add           r11, sp, #40          @decrement stack pointer,to accomodate two variables
    ldmfd         r11, {r4-r10}         @load the strides into registers

    @R0     :pointer to src buffer
    @R1     :pointer to pred buffer
    @R2     :pointer to dst buffer
    @R3     :Source stride
    @R4     :Pred stride
    @R5     :scale matirx,
    @R6     :threshold matrix
    @R7     :qbits
    @R8     :round factor
    @R9     :nnz
    vpush         {d8-d15}
    mov           r11, #0
    sub           r7, r11, r7           @Negate the qbit value for usiing LSL

    @------------Fucntion Loading done----------------;

    vld2.u8       {d10, d11}, [r0], r3  @load first 8 pix src row 1

    vld2.u8       {d11, d12}, [r1], r4  @load first 8 pix pred row 1

    vld2.u8       {d28, d29}, [r0], r3  @load first 8 pix src row 2

    vld2.u8       {d29, d30}, [r1], r4  @load first 8 pix pred row 2

    vld2.u8       {d25, d26}, [r0], r3  @load first 8 pix src row 3

    vld2.u8       {d26, d27}, [r1], r4  @load first 8 pix pred row 3
    vsubl.u8      q0, d10, d11          @find residue row 1

    vld2.u8       {d22, d23}, [r0], r3  @load first 8 pix src row 4

    vld2.u8       {d23, d24}, [r1], r4  @load first 8 pix pred row 4
    vsubl.u8      q1, d28, d29          @find residue row 2

    vsubl.u8      q2, d25, d26          @find residue row 3
    vsubl.u8      q3, d22, d23          @find residue row 4

    vtrn.16       d0, d2                @T12
    vtrn.16       d4, d6                @T23
    vtrn.32       d0, d4                @T13
    vtrn.32       d2, d6                @T14

    vadd.s16      d8 , d0, d6           @x0 = x4+x7
    vadd.s16      d9 , d2, d4           @x1 = x5+x6
    vsub.s16      d10, d2, d4           @x2 = x5-x6
    vsub.s16      d11, d0, d6           @x3 = x4-x7

    vshl.s16      d12, d10, #1          @U_SHIFT(x2,1,shft)
    vshl.s16      d13, d11, #1          @U_SHIFT(x3,1,shft)

    vadd.s16      d14, d8, d9           @x4 = x0 + x1;
    vsub.s16      d16, d8, d9           @x6 = x0 - x1;
    vadd.s16      d15, d13, d10         @x5 = U_SHIFT(x3,1,shft) + x2;
    vsub.s16      d17, d11, d12         @x7 = x3 - U_SHIFT(x2,1,shft);

    @taking transpose again so as to make do vert transform
    vtrn.16       d14, d15              @T12
    vtrn.16       d16, d17              @T23
    vtrn.32       d14, d16              @T13
    vtrn.32       d15, d17              @T24

    @let us do vertical transform
    @same code as horiz
    vadd.s16      d18, d14, d17         @x0 = x4+x7
    vadd.s16      d19, d15, d16         @x1 = x5+x6
    vsub.s16      d20, d15, d16         @x2 = x5-x6
    vsub.s16      d21, d14, d17         @x3 = x4-x7

    vshl.s16      d22, d20, #1          @U_SHIFT(x2,1,shft)
    vshl.s16      d23, d21, #1          @U_SHIFT(x3,1,shft)

    vdup.s32      q4, r8                @Load rounding value row 1

    vadd.s16      d24, d18, d19         @x5 = x0 + x1;
    vsub.s16      d26, d18, d19         @x7 = x0 - x1;
    vadd.s16      d25, d23, d20         @x6 = U_SHIFT(x3,1,shft) + x2;
    vsub.s16      d27, d21, d22         @x8 = x3 - U_SHIFT(x2,1,shft);
    vdup.s32      q10, r7               @Load qbit values

    vst1.s16      d24[0], [r10]         @Store Unquantized dc value to dc alte address

@core tranform is done for 4x8 block 1
    vld1.s16      {q14-q15}, [r5]       @load the scaling values

    vabs.s16      q0, q12               @Abs val of row 1 blk 1

    vabs.s16      q1, q13               @Abs val of row 2 blk 1

    vmov.s32      q5, q4                @copy round fact for row 2

    vmov.s32      q6, q4                @copy round fact for row 2
    vclt.s16      q2, q12, #0           @Get the sign of row 1 blk 1

    vmov.s32      q7, q4                @copy round fact for row 2
    vclt.s16      q3, q13, #0           @Get the sign of row 2 blk 1

    vmlal.s16     q4, d0, d28           @Multiply and add row 1
    vmlal.s16     q5, d1, d29           @Multiply and add row 2
    vmlal.s16     q6, d2, d30           @Multiply and add row 3
    vmlal.s16     q7, d3, d31           @Multiply and add row 4

    vshl.s32      q11, q4, q10          @Shift row 1
    vshl.s32      q12, q5, q10          @Shift row 2
    vshl.s32      q13, q6, q10          @Shift row 3
    vshl.s32      q14, q7, q10          @Shift row 4

    vmovn.s32     d30, q11              @Narrow row 1
    vmovn.s32     d31, q12              @Narrow row 2
    vmovn.s32     d0 , q13              @Narrow row 3
    vmovn.s32     d1 , q14              @Narrow row 4

    vneg.s16      q1, q15               @Get negative
    vneg.s16      q4, q0                @Get negative

    vceq.s16      q5, q15, #0           @I  compare with zero row 1 and 2 blk 1
    vceq.s16      q6, q0 , #0           @I  compare with zero row 1 and 2 blk 1

    vbsl.s16      q2, q1, q15           @Restore sign of row 1 and 2
    vbsl.s16      q3, q4, q0            @Restore sign of row 3 and 4

    vmovn.u16     d14, q5               @I  Narrow the comparison for row 1 and 2 blk 1
    vmovn.u16     d15, q6               @I  Narrow the comparison for row 1 and 2 blk 2

    vshr.u8       q8, q7, #7            @I  Reduce comaparison bit to a signle bit row 1 and 2 blk  1 and 2 [ keep the value for later use ]

    vpadd.u8      d18, d16, d17         @I pair add nnz 1
    vpadd.u8      d20, d18, d19         @I Pair add nnz 2
    vpadd.u8      d22, d20, d21         @I Pair add nnz 3
    vpadd.u8      d24, d22, d23         @I Pair add nnz4
    vst1.s16      {q2-q3}, [r2]         @Store blk

    vmov.u8       d25, #16              @I Get max nnz
    vsub.u8       d26, d25, d24         @I invert current nnz

    vst1.u8       d26[0], [r9]          @I  Write nnz

    vpop          {d8-d15}
    pop           {r4-r12, pc}



@*****************************************************************************
@*
@* Function Name     : ih264_hadamard_quant_4x4_a9
@* Description       : This function does forward hadamard transform and
@*                     quantization for luma dc block
@*
@* Arguments         :  R0 :pointer to src buffer
@                       R1 :pointer to dst buffer
@                       R2 :pu2_scale_matrix
@                       R2 :pu2_threshold_matrix
@                       STACk : u4_qbits
@                               u4_round_factor
@                               pu1_nnz
@ Values Returned   : NONE
@
@ Register Usage    :
@ Stack Usage       : 0 bytes
@ Cycles            : Around
@ Interruptiaility  : Interruptable
@
@ Known Limitations
@   \Assumptions    :
@
@ Revision History  :
@         DD MM YYYY    Author(s)   Changes
@         20 2 2015    100633      First version
@
@*****************************************************************************
@ih264_hadamard_quant_4x4_a9(WORD16 *pi2_src, WORD16 *pi2_dst,
@                           const UWORD16 *pu2_scale_matrix,
@                           const UWORD16 *pu2_threshold_matrix, UWORD32 u4_qbits,
@                           UWORD32 u4_round_factor,UWORD8  *pu1_nnz
@                           )
    .global ih264_hadamard_quant_4x4_a9
ih264_hadamard_quant_4x4_a9:

@Registert usage
@   r0 : src
@   r1 : dst
@   r2 : *pu2_scale_matrix
@   r3 : *pu2_threshold_matrix

    vld4.s16      {d0, d1, d2, d3}, [r0]! @Load 4x4 block
    vpush         {d8-d15}

    vld1.u16      d30[0], [r2]          @load pu2_scale_matrix[0]

    vaddl.s16     q3, d0, d3            @x0 = x4 + x7;
    vaddl.s16     q4, d1, d2            @x1 = x5 + x6;
    vsubl.s16     q5, d1, d2            @x2 = x5 - x6;
    vsubl.s16     q6, d0, d3            @x3 = x4 - x7;

    vdup.u16      d30, d30[0]           @pu2_scale_matrix[0]

    vadd.s32      q7, q3, q4            @pi2_dst[0] = x0 + x1;
    vadd.s32      q8, q6, q5            @pi2_dst[1] = x3 + x2;
    add           r3, sp, #68           @Get address of u4_round_factor
    vsub.s32      q9, q3, q4            @pi2_dst[2] = x0 - x1;
    vsub.s32      q10, q6, q5           @pi2_dst[3] = x3 - x2;

    vtrn.s32      q7, q8                @transpose 4x4 block
    vtrn.s32      q9, q10
    vld1.s32      d0[0], [r3]           @load   u4_round_factor
    vswp          d15, d18
    vswp          d17, d20

    add           r3, sp, #64           @Get address of u4_qbits
    vadd.s32      q11, q7, q10          @x0 = x4 + x7;
    vadd.s32      q12, q8, q9           @x1 = x5 + x6;
    vld1.s32      d31[0], [r3]          @load  u4_qbits
    vsub.s32      q13, q8, q9           @x2 = x5 - x6;
    vsub.s32      q14, q7, q10          @x3 = x4 - x7;

    vdup.s32      q7, d0[0]             @u4_round_factor

    vadd.s32      q0, q11, q12          @(x0 + x1)
    vadd.s32      q1, q14, q13          @(x3 + x2)
    vsub.s32      q2, q11, q12          @(x0 - x1)
    vsub.s32      q3, q14, q13          @(x3 - x2)

    vdup.s32      q11, d31[0]           @u4_round_factor

    vshrn.s32     d0, q0, #1            @i4_value = (x0 + x1) >> 1;
    vshrn.s32     d1, q1, #1            @i4_value = (x3 + x2) >> 1;
    vshrn.s32     d2, q2, #1            @i4_value = (x0 - x1) >> 1;
    vshrn.s32     d3, q3, #1            @i4_value = (x3 - x2) >> 1;

    vabs.s16      q5, q0
    vabs.s16      q6, q1

    vmov.s32      q8, q7                @Get the round fact
    vmov.s32      q9, q7
    vmov.s32      q10, q7

    vclt.s16      q3, q0, #0            @get the sign row 1,2
    vclt.s16      q4, q1, #0

    vneg.s32      q11, q11              @-u4_round_factor

    vmlal.u16     q7, d10, d30
    vmlal.u16     q8, d11, d30
    vmlal.u16     q9, d12, d30
    vmlal.u16     q10, d13, d30

    vshl.u32      q7, q7, q11
    vshl.u32      q8, q8, q11
    vshl.u32      q9, q9, q11
    vshl.u32      q10, q10, q11

    vqmovn.u32    d22, q7
    vqmovn.u32    d23, q8
    vqmovn.u32    d24, q9
    vqmovn.u32    d25, q10

    vneg.s16      q13, q11
    vneg.s16      q14, q12

    vbsl.s16      q3, q13, q11
    vbsl.s16      q4, q14, q12

    vceq.s16      q5, q11, #0
    vceq.s16      q6, q12, #0

    vst1.s16      {q3}, [r1]!

    vshrn.u16     d14, q5, #8
    vshrn.u16     d15, q6, #8

    ldr           r3, [sp, #72]         @Load *pu1_nnz

    vshr.u8       q7, q7, #7

    vst1.s16      {q4}, [r1]!

    vadd.u8       d16, d14, d15
    vmov.u8       d20, #16
    vpadd.u8      d17, d16, d16
    vpadd.u8      d18, d17, d17
    vpadd.u8      d19, d18, d18
    vsub.u8       d20, d20, d19
    vst1.u8       d20[0], [r3]

    vpop          {d8-d15}
    bx            lr




@*****************************************************************************
@*
@* Function Name     : ih264_hadamard_quant_2x2_uv_a9
@* Description       : This function does forward hadamard transform and
@*                     quantization for dc block of chroma for both planes
@*
@* Arguments         :  R0 :pointer to src buffer
@                       R1 :pointer to dst buffer
@                       R2 :pu2_scale_matrix
@                       R2 :pu2_threshold_matrix
@                       STACk : u4_qbits
@                               u4_round_factor
@                               pu1_nnz
@ Values Returned   : NONE
@
@ Register Usage    :
@ Stack Usage       : 0 bytes
@ Cycles            : Around
@ Interruptiaility  : Interruptable
@
@ Known Limitations
@   \Assumptions    :
@
@ Revision History  :
@         DD MM YYYY    Author(s)   Changes
@         20 2 2015    100633      First version
@
@*****************************************************************************
@ ih264_hadamard_quant_2x2_uv_a9(WORD16 *pi2_src, WORD16 *pi2_dst,
@                             const UWORD16 *pu2_scale_matrix,
@                             const UWORD16 *pu2_threshold_matrix, UWORD32 u4_qbits,
@                             UWORD32 u4_round_factor,UWORD8  *pu1_nnz
@                             )

    .global ih264_hadamard_quant_2x2_uv_a9
ih264_hadamard_quant_2x2_uv_a9:

    vpush         {d8-d15}
    vld2.s16      {d0-d1}, [r0]         @load src

    add           r3, sp, #68           @Get address of u4_round_factor

    vaddl.s16     q3, d0, d1            @x0 = x4 + x5;, x2 = x6 + x7;
    vld1.u16      d30[0], [r2]          @load pu2_scale_matrix[0]
    vsubl.s16     q4, d0, d1            @x1 = x4 - x5;  x3 = x6 - x7;

    add           r0, sp, #64           @Get affress of u4_qbits
    vld1.s32      d28[0], [r3]          @load   u4_round_factor
    vtrn.s32      q3, q4                @q1 -> x0 x1, q2 -> x2 x3

    vadd.s32      q0, q3, q4            @ (x0 + x2) (x1 + x3)  (y0 + y2); (y1 + y3);
    vld1.s32      d24[0], [r0]          @load  u4_qbits
    vsub.s32      q1, q3, q4            @ (x0 - x2) (x1 - x3)  (y0 - y2); (y1 - y3);

    vdup.u16      d30, d30[0]           @pu2_scale_matrix

    vabs.s32      q2, q0
    vabs.s32      q3, q1

    vdup.s32      q14, d28[0]           @u4_round_factor

    vmovl.u16     q15, d30              @pu2_scale_matrix

    vclt.s32      q4, q0, #0            @get the sign row 1,2
    vdup.s32      q12, d24[0]           @u4_round_factor
    vclt.s32      q5, q1, #0

    vqmovn.u32    d8, q4
    vqmovn.s32    d9, q5

    vmov.s32      q13, q14              @Get the round fact
    vneg.s32      q12, q12              @-u4_round_factor

    vmla.u32      q13, q2, q15
    vmla.u32      q14, q3, q15

    vshl.u32      q13, q13, q12         @>>qbit
    vshl.u32      q14, q14, q12         @>>qbit

    vqmovn.u32    d10, q13
    vqmovn.u32    d11, q14

    vneg.s16      q6, q5

    vbsl.s16      q4, q6, q5            @*sign

    vtrn.s32      d8, d9

    vceq.s16      q7, q4, #0            @Compute nnz

    vshrn.u16     d14, q7, #8           @reduce nnz comparison to 1 bit

    ldr           r3, [sp, #72]         @Load *pu1_nnz
    vshr.u8       d14, d14, #7          @reduce nnz comparison to 1 bit
    vmov.u8       d20, #4               @Since we add zeros, we need to subtract from 4 to get nnz
    vpadd.u8      d17, d14, d14         @Sum up nnz

    vst1.s16      {q4}, [r1]!           @Store the block

    vpadd.u8      d17, d17, d17         @Sum up nnz
    vsub.u8       d20, d20, d17         @4- numzeros
    vst1.u16      d20[0], [r3]          @store nnz

    vpop          {d8-d15}
    bx            lr





