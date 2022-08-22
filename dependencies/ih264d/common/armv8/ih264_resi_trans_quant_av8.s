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
///*****************************************************************************/
///**
//*******************************************************************************
//* @file
//*  ih264_resi_trans_quant_av8.c
//*
//* @brief
//*  contains function definitions for residual and forward trans
//*
//* @author
//*  ittiam
//*
//* @par list of functions:
//*    ih264_resi_trans_quant_4x4_av8
//*    ih264_resi_trans_quant_8x8_av8
//*    ih264_resi_trans_quant_chroma_4x4_av8
//* @remarks
//*  none
//*
//*******************************************************************************
.include "ih264_neon_macros.s"
.text
.p2align 2
//*****************************************************************************
//*
//* function name     : ih264_resi_trans_quant_4x4
//* description       : this function does cf4 of h264
//*
// values returned   : none
//
// register usage    :
// stack usage       : 64 bytes
// cycles            :
// interruptiaility  : interruptable
//
// known limitations
//   \assumptions    :
//
// revision history  :
//         dd mm yyyy    author(s)   changes
//         1 12 2013    100633      first version
//         20 1 2014    100633      changes the api, optimization
//
//*****************************************************************************

    .global ih264_resi_trans_quant_4x4_av8
ih264_resi_trans_quant_4x4_av8:

    push_v_regs
    //x0     :pointer to src buffer
    //x1     :pointer to pred buffer
    //x2     :pointer to dst buffer
    //w3     :source stride
    //w4     :pred stride
    //w5     :scale matirx,
    //x6     :threshold matrix
    //w7     :qbits
    //w8        :round factor
    //x9        :nnz
    //x10       :pointer to store non quantized dc value

    sxtw      x3, w3
    sxtw      x4, w4
    ldr       w8, [sp, #64]             //load round factor
    ldr       x10, [sp, #80]            //load addres for non quant val
    neg       w7, w7                    //negate the qbit value for usiing lsl
    ldr       x9, [sp, #72]

    //------------fucntion loading done----------------;

    ld1       {v30.8b}, [x0], x3        //load first 8 pix src  row 1
    ld1       {v31.8b}, [x1], x4        //load first 8 pix pred row 1
    ld1       {v28.8b}, [x0], x3        //load first 8 pix src  row 2
    ld1       {v29.8b}, [x1], x4        //load first 8 pix pred row 2
    ld1       {v26.8b}, [x0], x3        //load first 8 pix src  row 3
    ld1       {v27.8b}, [x1], x4        //load first 8 pix pred row 3
    ld1       {v24.8b}, [x0]            //load first 8 pix src row 4
    ld1       {v25.8b}, [x1]            //load first 8 pix pred row 4

    usubl     v0.8h, v30.8b, v31.8b     //find residue row 1
    usubl     v2.8h, v28.8b, v29.8b     //find residue row 2
    usubl     v4.8h, v26.8b, v27.8b     //find residue row 3
    usubl     v6.8h, v24.8b, v25.8b     //find residue row 4

    trn1      v1.4h, v0.4h, v2.4h
    trn2      v3.4h, v0.4h, v2.4h       //t12
    trn1      v5.4h, v4.4h, v6.4h
    trn2      v7.4h, v4.4h, v6.4h       //t23

    trn1      v0.2s, v1.2s, v5.2s
    trn2      v4.2s, v1.2s, v5.2s       //t13
    trn1      v2.2s, v3.2s, v7.2s
    trn2      v6.2s, v3.2s, v7.2s       //t14

    add       v8.4h, v0.4h, v6.4h       //x0 = x4+x7
    add       v9.4h, v2.4h, v4.4h       //x1 = x5+x6
    sub       v10.4h, v2.4h, v4.4h      //x2 = x5-x6
    sub       v11.4h, v0.4h, v6.4h      //x3 = x4-x7

    shl       v12.4h, v10.4h, #1        //u_shift(x2,1,shft)
    shl       v13.4h, v11.4h, #1        //u_shift(x3,1,shft)

    add       v14.4h, v8.4h, v9.4h      //x4 = x0 + x1;
    sub       v16.4h, v8.4h, v9.4h      //x6 = x0 - x1;
    add       v15.4h, v13.4h, v10.4h    //x5 = u_shift(x3,1,shft) + x2;
    sub       v17.4h, v11.4h, v12.4h    //x7 = x3 - u_shift(x2,1,shft);

    //taking transpose again so as to make do vert transform
    trn1      v0.4h, v14.4h, v15.4h
    trn2      v1.4h, v14.4h, v15.4h     //t12
    trn1      v2.4h, v16.4h, v17.4h
    trn2      v3.4h, v16.4h, v17.4h     //t23

    trn1      v14.2s, v0.2s, v2.2s
    trn2      v16.2s, v0.2s, v2.2s      //t13
    trn1      v15.2s, v1.2s, v3.2s
    trn2      v17.2s, v1.2s, v3.2s      //t24

    //let us do vertical transform
    //same code as horiz
    add       v18.4h, v14.4h , v17.4h   //x0 = x4+x7
    add       v19.4h, v15.4h , v16.4h   //x1 = x5+x6
    sub       v20.4h, v15.4h , v16.4h   //x2 = x5-x6
    sub       v21.4h, v14.4h , v17.4h   //x3 = x4-x7

    shl       v22.4h, v20.4h, #1        //u_shift(x2,1,shft)
    shl       v23.4h, v21.4h, #1        //u_shift(x3,1,shft)

    dup       v8.4s, w8                 //load rounding value row 1

    add       v24.4h, v18.4h , v19.4h   //x5 = x0 + x1;
    sub       v26.4h, v18.4h , v19.4h   //x7 = x0 - x1;
    add       v25.4h, v23.4h , v20.4h   //x6 = u_shift(x3,1,shft) + x2;
    sub       v27.4h, v21.4h , v22.4h   //x8 = x3 - u_shift(x2,1,shft);

    dup       v23.4s, w8                //load round factor values

    st1       {v24.h}[0], [x10]         //store the dc value to alternate dc sddress
//core tranform is done for 4x8 block 1
    ld1       {v28.4h-v31.4h}, [x5]     //load the scaling values

    abs       v0.4h, v24.4h             //abs val of row 1
    abs       v1.4h, v25.4h             //abs val of row 2
    abs       v2.4h, v26.4h             //abs val of row 3
    abs       v3.4h, v27.4h             //abs val of row 4

    cmgt      v4.4h, v24.4h, #0
    cmgt      v5.4h, v25.4h, #0
    cmgt      v6.4h, v26.4h, #0
    cmgt      v7.4h, v27.4h, #0

    smull     v0.4s, v0.4h, v28.4h      //multiply and add row 1
    smull     v1.4s, v1.4h, v29.4h      //multiply and add row 2
    smull     v2.4s, v2.4h, v30.4h      //multiply and add row 3
    smull     v3.4s, v3.4h, v31.4h      //multiply and add row 4

    add       v20.4s, v0.4s, v23.4s
    add       v21.4s, v1.4s, v23.4s
    add       v22.4s, v2.4s, v23.4s
    add       v23.4s, v3.4s, v23.4s

    dup       v24.4s, w7

    sshl      v20.4s, v20.4s, v24.4s    //shift row 1
    sshl      v21.4s, v21.4s, v24.4s    //shift row 2
    sshl      v22.4s, v22.4s, v24.4s    //shift row 3
    sshl      v23.4s, v23.4s, v24.4s    //shift row 4

    xtn       v20.4h, v20.4s            //narrow row 1
    xtn       v21.4h, v21.4s            //narrow row 2
    xtn       v22.4h, v22.4s            //narrow row 3
    xtn       v23.4h, v23.4s            //narrow row 4

    neg       v24.8h, v20.8h            //get negative
    neg       v25.8h, v21.8h            //get negative
    neg       v26.8h, v22.8h            //get negative
    neg       v27.8h, v23.8h            //get negative

    //compare with zero for computng nnz
    cmeq      v0.4h, v20.4h, #0
    cmeq      v1.4h, v21.4h, #0
    cmeq      v2.4h, v22.4h, #0
    cmeq      v3.4h, v23.4h, #0

    bsl       v4.8b, v20.8b, v24.8b     //restore sign of row 1 and 2
    bsl       v5.8b, v21.8b, v25.8b     //restore sign of row 3 and 4
    bsl       v6.8b, v22.8b, v26.8b     //restore sign of row 1 and 2
    bsl       v7.8b, v23.8b, v27.8b     //restore sign of row 3 and 4

    //narrow the comaprison result
    mov       v0.d[1], v2.d[0]
    mov       v1.d[1], v3.d[0]

    xtn       v0.8b, v0.8h
    xtn       v1.8b, v1.8h

    ushr      v0.8b, v0.8b, #7          //i    reduce comaparison bit to a signle bit row 1 and 2 blk  1 and 2 [ keep the value for later use ]
    ushr      v1.8b, v1.8b, #7          //i    reduce comaparison bit to a signle bit row 1 and 2 blk  1 and 2 [ keep the value for later use ]

    add       v0.8b, v0.8b, v1.8b       //i pair add nnz 1
    addp      v0.8b, v0.8b, v0.8b       //i pair add nnz 1
    addp      v0.8b, v0.8b, v0.8b       //i pair add nnz 1
    addp      v0.8b, v0.8b, v0.8b       //i pair add nnz 1

    st1       {v4.4h-v7.4h}, [x2]       //store blk

    movi      v25.8b, #16               //get max nnz
    sub       v26.8b, v25.8b , v0.8b    //invert current nnz
    st1       {v26.b}[0], [x9]          //write nnz

    pop_v_regs
    ret


//*****************************************************************************
//*
//* function name     : ih264_resi_trans_quant_chroma_4x4
//* description       : this function does residue calculation, forward transform
//*                        and quantization for 4x4 chroma block.
//*
// values returned   : none
//
// register usage    :
// stack usage       : 64 bytes
// cycles            :
// interruptiaility  : interruptable
//
// known limitations
//   \assumptions    :
//
// revision history  :
//         dd mm yyyy    author(s)   changes
//         11 2 2015    100664      first version
//         25 2 2015    100633      first av8 version
//*****************************************************************************

    .global ih264_resi_trans_quant_chroma_4x4_av8
ih264_resi_trans_quant_chroma_4x4_av8:

    push_v_regs
    //x0     :pointer to src buffer
    //x1     :pointer to pred buffer
    //x2     :pointer to dst buffer
    //w3     :source stride
    //w4     :pred stride
    //x5     :scale matirx,
    //x6     :threshold matrix
    //w7     :qbits
    //w8        :round factor
    //x9        :nnz
    //x10       :pointer to store non quantized dc value

    sxtw      x3, w3
    sxtw      x4, w4
    ldr       w8, [sp, #64]             //load round factor
    ldr       x10, [sp, #80]            //load addres for non quant val
    neg       w7, w7                    //negate the qbit value for usiing lsl
    ldr       x9, [sp, #72]
    //------------fucntion loading done----------------;

    ld1       {v30.8b}, [x0], x3        //load first 8 pix src  row 1
    ld1       {v31.8b}, [x1], x4        //load first 8 pix pred row 1
    ld1       {v28.8b}, [x0], x3        //load first 8 pix src  row 2
    ld1       {v29.8b}, [x1], x4        //load first 8 pix pred row 2
    ld1       {v26.8b}, [x0], x3        //load first 8 pix src  row 3
    ld1       {v27.8b}, [x1], x4        //load first 8 pix pred row 3
    ld1       {v24.8b}, [x0]            //load first 8 pix src row 4
    ld1       {v25.8b}, [x1]            //load first 8 pix pred row 4


    //deinterleave the loaded values
    uzp1      v30.8b, v30.8b, v30.8b
    uzp1      v31.8b, v31.8b, v31.8b
    uzp1      v28.8b, v28.8b, v28.8b
    uzp1      v29.8b, v29.8b, v29.8b
    uzp1      v26.8b, v26.8b, v26.8b
    uzp1      v27.8b, v27.8b, v27.8b
    uzp1      v24.8b, v24.8b, v24.8b
    uzp1      v25.8b, v25.8b, v25.8b
    //this deinterleaving is the only differnece betweenchrom and luma fucntions

    usubl     v0.8h, v30.8b, v31.8b     //find residue row 1
    usubl     v2.8h, v28.8b, v29.8b     //find residue row 2
    usubl     v4.8h, v26.8b, v27.8b     //find residue row 3
    usubl     v6.8h, v24.8b, v25.8b     //find residue row 4

    trn1      v1.4h, v0.4h, v2.4h
    trn2      v3.4h, v0.4h, v2.4h       //t12
    trn1      v5.4h, v4.4h, v6.4h
    trn2      v7.4h, v4.4h, v6.4h       //t23

    trn1      v0.2s, v1.2s, v5.2s
    trn2      v4.2s, v1.2s, v5.2s       //t13
    trn1      v2.2s, v3.2s, v7.2s
    trn2      v6.2s, v3.2s, v7.2s       //t14

    add       v8.4h, v0.4h, v6.4h       //x0 = x4+x7
    add       v9.4h, v2.4h, v4.4h       //x1 = x5+x6
    sub       v10.4h, v2.4h, v4.4h      //x2 = x5-x6
    sub       v11.4h, v0.4h, v6.4h      //x3 = x4-x7

    shl       v12.4h, v10.4h, #1        //u_shift(x2,1,shft)
    shl       v13.4h, v11.4h, #1        //u_shift(x3,1,shft)

    add       v14.4h, v8.4h, v9.4h      //x4 = x0 + x1;
    sub       v16.4h, v8.4h, v9.4h      //x6 = x0 - x1;
    add       v15.4h, v13.4h, v10.4h    //x5 = u_shift(x3,1,shft) + x2;
    sub       v17.4h, v11.4h, v12.4h    //x7 = x3 - u_shift(x2,1,shft);

    //taking transpose again so as to make do vert transform
    trn1      v0.4h, v14.4h, v15.4h
    trn2      v1.4h, v14.4h, v15.4h     //t12
    trn1      v2.4h, v16.4h, v17.4h
    trn2      v3.4h, v16.4h, v17.4h     //t23

    trn1      v14.2s, v0.2s, v2.2s
    trn2      v16.2s, v0.2s, v2.2s      //t13
    trn1      v15.2s, v1.2s, v3.2s
    trn2      v17.2s, v1.2s, v3.2s      //t24

    //let us do vertical transform
    //same code as horiz
    add       v18.4h, v14.4h , v17.4h   //x0 = x4+x7
    add       v19.4h, v15.4h , v16.4h   //x1 = x5+x6
    sub       v20.4h, v15.4h , v16.4h   //x2 = x5-x6
    sub       v21.4h, v14.4h , v17.4h   //x3 = x4-x7

    shl       v22.4h, v20.4h, #1        //u_shift(x2,1,shft)
    shl       v23.4h, v21.4h, #1        //u_shift(x3,1,shft)

    dup       v8.4s, w8                 //load rounding value row 1

    add       v24.4h, v18.4h , v19.4h   //x5 = x0 + x1;
    sub       v26.4h, v18.4h , v19.4h   //x7 = x0 - x1;
    add       v25.4h, v23.4h , v20.4h   //x6 = u_shift(x3,1,shft) + x2;
    sub       v27.4h, v21.4h , v22.4h   //x8 = x3 - u_shift(x2,1,shft);

    dup       v23.4s, w8                //load round factor values

    st1       {v24.h}[0], [x10]         //store the dc value to alternate dc sddress
//core tranform is done for 4x8 block 1
    ld1       {v28.4h-v31.4h}, [x5]     //load the scaling values

    abs       v0.4h, v24.4h             //abs val of row 1
    abs       v1.4h, v25.4h             //abs val of row 2
    abs       v2.4h, v26.4h             //abs val of row 3
    abs       v3.4h, v27.4h             //abs val of row 4

    cmgt      v4.4h, v24.4h, #0
    cmgt      v5.4h, v25.4h, #0
    cmgt      v6.4h, v26.4h, #0
    cmgt      v7.4h, v27.4h, #0

    smull     v0.4s, v0.4h, v28.4h      //multiply and add row 1
    smull     v1.4s, v1.4h, v29.4h      //multiply and add row 2
    smull     v2.4s, v2.4h, v30.4h      //multiply and add row 3
    smull     v3.4s, v3.4h, v31.4h      //multiply and add row 4

    add       v20.4s, v0.4s, v23.4s
    add       v21.4s, v1.4s, v23.4s
    add       v22.4s, v2.4s, v23.4s
    add       v23.4s, v3.4s, v23.4s

    dup       v24.4s, w7

    sshl      v20.4s, v20.4s, v24.4s    //shift row 1
    sshl      v21.4s, v21.4s, v24.4s    //shift row 2
    sshl      v22.4s, v22.4s, v24.4s    //shift row 3
    sshl      v23.4s, v23.4s, v24.4s    //shift row 4

    xtn       v20.4h, v20.4s            //narrow row 1
    xtn       v21.4h, v21.4s            //narrow row 2
    xtn       v22.4h, v22.4s            //narrow row 3
    xtn       v23.4h, v23.4s            //narrow row 4

    neg       v24.8h, v20.8h            //get negative
    neg       v25.8h, v21.8h            //get negative
    neg       v26.8h, v22.8h            //get negative
    neg       v27.8h, v23.8h            //get negative

    //compare with zero for computng nnz
    cmeq      v0.4h, v20.4h, #0
    cmeq      v1.4h, v21.4h, #0
    cmeq      v2.4h, v22.4h, #0
    cmeq      v3.4h, v23.4h, #0

    bsl       v4.8b, v20.8b, v24.8b     //restore sign of row 1 and 2
    bsl       v5.8b, v21.8b, v25.8b     //restore sign of row 3 and 4
    bsl       v6.8b, v22.8b, v26.8b     //restore sign of row 1 and 2
    bsl       v7.8b, v23.8b, v27.8b     //restore sign of row 3 and 4

    //narrow the comaprison result
    mov       v0.d[1], v2.d[0]
    mov       v1.d[1], v3.d[0]

    xtn       v0.8b, v0.8h
    xtn       v1.8b, v1.8h

    ushr      v0.8b, v0.8b, #7          //i    reduce comaparison bit to a signle bit row 1 and 2 blk  1 and 2 [ keep the value for later use ]
    ushr      v1.8b, v1.8b, #7          //i    reduce comaparison bit to a signle bit row 1 and 2 blk  1 and 2 [ keep the value for later use ]

    add       v0.8b, v0.8b, v1.8b       //i pair add nnz 1
    addp      v0.8b, v0.8b, v0.8b       //i pair add nnz 1
    addp      v0.8b, v0.8b, v0.8b       //i pair add nnz 1
    addp      v0.8b, v0.8b, v0.8b       //i pair add nnz 1

    st1       {v4.4h-v7.4h}, [x2]       //store blk

    movi      v25.8b, #16               //get max nnz
    sub       v26.8b, v25.8b , v0.8b    //invert current nnz
    st1       {v26.b}[0], [x9]          //write nnz

    pop_v_regs
    ret


//*****************************************************************************
//*
//* function name     : ih264_hadamard_quant_4x4_av8
//* description       : this function does forward hadamard transform and
//*                     quantization for luma dc block
//*
//* arguments         :  x0 :pointer to src buffer
//                       x1 :pointer to dst buffer
//                       x2 :pu2_scale_matrix
//                       x3 :pu2_threshold_matrix
//                       w4 :u4_qbits
//                       w5 :u4_round_factor
//                       x6 :pu1_nnz
// values returned   : none
//
// register usage    :
// stack usage       : 0 bytes
// cycles            : around
// interruptiaility  : interruptable
//
// known limitations
//   \assumptions    :
//
// revision history  :
//         dd mm yyyy    author(s)   changes
//         20 2 2015    100633      first version
//
//*****************************************************************************
//ih264_hadamard_quant_4x4_av8(word16 *pi2_src, word16 *pi2_dst,
//                           const uword16 *pu2_scale_matrix,
//                           const uword16 *pu2_threshold_matrix, uword32 u4_qbits,
//                           uword32 u4_round_factor,uword8  *pu1_nnz
//                           )
    .global ih264_hadamard_quant_4x4_av8
ih264_hadamard_quant_4x4_av8:

//x0 :pointer to src buffer
//x1 :pointer to dst buffer
//x2 :pu2_scale_matrix
//x3 :pu2_threshold_matrix
//w4 :u4_qbits
//w5 :u4_round_factor
//x6 :pu1_nnz

    push_v_regs

    ld4       {v0.4h-v3.4h}, [x0]       //load 4x4 block
    ld1       {v30.h}[0], [x2]          //load pu2_scale_matrix[0]

    saddl     v4.4s, v0.4h, v3.4h       //x0 = x4 + x7;
    saddl     v5.4s, v1.4h, v2.4h       //x1 = x5 + x6;
    ssubl     v6.4s, v1.4h, v2.4h       //x2 = x5 - x6;
    ssubl     v7.4s, v0.4h, v3.4h       //x3 = x4 - x7;

    dup       v30.8h, v30.h[0]          //pu2_scale_matrix[0]

    add       v14.4s, v4.4s, v5.4s      //pi2_dst[0] = x0 + x1;
    add       v15.4s, v7.4s, v6.4s      //pi2_dst[1] = x3 + x2;
    sub       v16.4s, v4.4s, v5.4s      //pi2_dst[2] = x0 - x1;
    sub       v17.4s, v7.4s, v6.4s      //pi2_dst[3] = x3 - x2;

    //transpose 4x4 block
    trn1      v18.4s, v14.4s, v15.4s
    trn2      v19.4s, v14.4s, v15.4s
    trn1      v20.4s, v16.4s, v17.4s
    trn2      v21.4s, v16.4s, v17.4s

    trn1      v14.2d, v18.2d, v20.2d
    trn2      v16.2d, v18.2d, v20.2d
    trn1      v15.2d, v19.2d, v21.2d
    trn2      v17.2d, v19.2d, v21.2d
    //end transpose

    add       v18.4s, v14.4s, v17.4s    //x0 = x4 + x7;
    add       v19.4s, v15.4s, v16.4s    //x1 = x5 + x6;
    sub       v20.4s, v15.4s, v16.4s    //x2 = x5 - x6;
    sub       v21.4s, v14.4s, v17.4s    //x3 = x4 - x7;

    dup       v14.4s, w5                //round factor
    dup       v15.4s, v14.s[0]
    dup       v16.4s, v14.s[0]
    dup       v17.4s, v14.s[0]

    add       v22.4s, v18.4s, v19.4s    //(x0 + x1)
    add       v23.4s, v21.4s, v20.4s    //(x3 + x2)
    sub       v24.4s, v18.4s, v19.4s    //(x0 - x1)
    sub       v25.4s, v21.4s, v20.4s    //(x3 - x2)

    shrn      v0.4h, v22.4s, #1         //i4_value = (x0 + x1) >> 1;
    shrn2     v0.8h, v23.4s, #1         //i4_value = (x3 + x2) >> 1;
    shrn      v1.4h, v24.4s, #1         //i4_value = (x0 - x1) >> 1;
    shrn2     v1.8h, v25.4s, #1         //i4_value = (x3 - x2) >> 1;

    abs       v2.8h, v0.8h
    abs       v3.8h, v1.8h

    cmgt      v4.8h, v0.8h, #0          //get the sign row 1,2
    cmgt      v5.8h, v1.8h, #0

    neg       w4, w4                    //-u4_qbits
    dup       v22.4s, w4                //load  -u4_qbits

    umlal     v14.4s, v2.4h, v30.4h
    umlal2    v15.4s, v2.8h, v30.8h
    umlal     v16.4s, v3.4h, v30.4h
    umlal2    v17.4s, v3.8h, v30.8h

    ushl      v14.4s, v14.4s, v22.4s
    ushl      v15.4s, v15.4s, v22.4s
    ushl      v16.4s, v16.4s, v22.4s
    ushl      v17.4s, v17.4s, v22.4s

    uqxtn     v14.4h, v14.4s
    uqxtn2    v14.8h, v15.4s
    uqxtn     v16.4h, v16.4s
    uqxtn2    v16.8h, v17.4s

    neg       v15.8h, v14.8h
    neg       v17.8h, v16.8h

    bsl       v4.16b, v14.16b, v15.16b
    bsl       v5.16b, v16.16b, v17.16b

    cmeq      v0.8h, v14.8h, #0
    cmeq      v1.8h, v16.8h, #0

    st1       {v4.8h-v5.8h}, [x1]

    movi      v20.8b, #16

    xtn       v2.8b, v0.8h
    xtn       v3.8b, v1.8h

    ushr      v2.8b, v2.8b, #7
    ushr      v3.8b, v3.8b, #7

    add       v2.8b, v2.8b, v3.8b
    addp      v2.8b, v2.8b, v2.8b
    addp      v2.8b, v2.8b, v2.8b
    addp      v2.8b, v2.8b, v2.8b
    sub       v20.8b, v20.8b, v2.8b
    st1       {v20.b}[0], [x6]

    pop_v_regs
    ret


//*****************************************************************************
//*
//* function name     : ih264_hadamard_quant_2x2_uv
//* description       : this function does forward hadamard transform and
//*                     quantization for dc block of chroma for both planes
//*
//* arguments         :  x0 :pointer to src buffer
//                       x1 :pointer to dst buffer
//                       x2 :pu2_scale_matrix
//                       x3 :pu2_threshold_matrix
//                       w4 :u4_qbits
//                       w5 :u4_round_factor
//                       x6 :pu1_nnz
// values returned   : none
//
// register usage    :
// stack usage       : 0 bytes
// cycles            : around
// interruptiaility  : interruptable
//
// known limitations
//   \assumptions    :
//
// revision history  :
//         dd mm yyyy    author(s)   changes
//         20 2 2015    100633      first version
//
//*****************************************************************************
// ih264_hadamard_quant_2x2_uv_av8(word16 *pi2_src, word16 *pi2_dst,
//                             const uword16 *pu2_scale_matrix,
//                             const uword16 *pu2_threshold_matrix, uword32 u4_qbits,
//                             uword32 u4_round_factor,uword8  *pu1_nnz
//                             )

    .global ih264_hadamard_quant_2x2_uv_av8
ih264_hadamard_quant_2x2_uv_av8:

    push_v_regs

    ld2       {v0.4h-v1.4h}, [x0]       //load src

    ld1       {v30.h}[0], [x2]          //load pu2_scale_matrix[0]
    dup       v30.4h, v30.h[0]          //pu2_scale_matrix
    uxtl      v30.4s, v30.4h            //pu2_scale_matrix

    neg       w4, w4
    dup       v24.4s, w4                //u4_qbits

    dup       v25.4s, w5                //round fact
    dup       v26.4s, v25.s[0]

    saddl     v2.4s, v0.4h, v1.4h       //x0 = x4 + x5;, x2 = x6 + x7;
    ssubl     v3.4s, v0.4h, v1.4h       //x1 = x4 - x5;  x3 = x6 - x7;

    trn1      v4.4s, v2.4s, v3.4s
    trn2      v5.4s, v2.4s, v3.4s       //q1 -> x0 x1, q2 -> x2 x3

    add       v0.4s, v4.4s , v5.4s      // (x0 + x2) (x1 + x3)  (y0 + y2); (y1 + y3);
    sub       v1.4s, v4.4s , v5.4s      // (x0 - x2) (x1 - x3)  (y0 - y2); (y1 - y3);

    abs       v2.4s, v0.4s
    abs       v3.4s, v1.4s

    cmgt      v4.4s, v0.4s, #0          //get the sign row 1,2
    cmgt      v5.4s, v1.4s, #0

    uqxtn     v4.4h, v4.4s
    sqxtn2    v4.8h, v5.4s

    mla       v25.4s, v2.4s, v30.4s
    mla       v26.4s, v3.4s, v30.4s

    ushl      v2.4s, v25.4s, v24.4s     //>>qbit
    ushl      v3.4s, v26.4s, v24.4s     //>>qbit

    uqxtn     v2.4h, v2.4s
    uqxtn2    v2.8h, v3.4s

    neg       v5.8h, v2.8h

    bsl       v4.16b, v2.16b, v5.16b    //*sign

    //rearrange such that we get each plane coeffs as continous
    mov       v5.s[0], v4.s[1]
    mov       v4.s[1], v4.s[2]
    mov       v4.s[2], v5.s[0]

    cmeq      v5.8h, v4.8h, #0          //compute nnz
    xtn       v5.8b, v5.8h              //reduce nnz comparison to 1 bit
    ushr      v5.8b, v5.8b, #7          //reduce nnz comparison to 1 bit
    movi      v20.8b, #4                //since we add zeros, we need to subtract from 4 to get nnz
    addp      v5.8b, v5.8b, v5.8b       //sum up nnz
    addp      v5.8b, v5.8b, v5.8b       //sum up nnz

    st1       {v4.8h}, [x1]             //store the block

    st1       {v4.8h}, [x1]             //store the block
    sub       v20.8b, v20.8b, v5.8b     //4- numzeros

    st1       {v20.h}[0], [x6]          //store nnz

    pop_v_regs
    ret



