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
///*******************************************************************************
// * //file
// *  ih264_iquant_itrans_recon_a9.s
// *
// * //brief
// *  Contains function definitions for single stage  inverse transform
// *
// * //author
// *  Parthiban V
// *     Mohit
// *  Harinarayanaan
// *
// * //par List of Functions:
// *  - ih264_iquant_itrans_recon_4x4_av8()
// *     - ih264_iquant_itrans_recon_8x8_av8()
// *     - ih264_iquant_itrans_recon_chroma_4x4_av8()
// *
// * //remarks
// *  None
// *
// *******************************************************************************

.text
.p2align 2
.include "ih264_neon_macros.s"

///*
// *******************************************************************************
// *
// * //brief
// *  This function performs inverse quant and Inverse transform type Ci4 for 4*4 block
// *
// * //par Description:
// *  Performs inverse transform Ci4 and adds the residue to get the
// *  reconstructed block
// *
// * //param[in] pi2_src
// *  Input 4x4 coefficients
// *
// * //param[in] pu1_pred
// *  Prediction 4x4 block
// *
// * //param[out] pu1_out
// *  Output 4x4 block
// *
// * //param[in] u4_qp_div_6
// *     QP
// *
// * //param[in] pu2_weigh_mat
// * Pointer to weight matrix
// *
// * //param[in] pred_strd,
// *  Prediction stride
// *
// * //param[in] out_strd
// *  Output Stride
// *
// *//param[in] pi2_tmp
// * temporary buffer of size 1*16
// *
// * //param[in] pu2_iscal_mat
// * Pointer to the inverse quantization matrix
// *
// * //returns  Void
// *
// * //remarks
// *  None
// *
// *******************************************************************************
// */
//void ih264_iquant_itrans_recon_4x4(WORD16 *pi2_src,
//                                    UWORD8 *pu1_pred,
//                                    UWORD8 *pu1_out,
//                                    WORD32 pred_strd,
//                                    WORD32 out_strd,
//                                    const UWORD16 *pu2_iscal_mat,
//                                    const UWORD16 *pu2_weigh_mat,
//                                    UWORD32 u4_qp_div_6,
//                                    WORD32 *pi4_tmp,
//                                    WORD32 iq_start_idx
//                                    WORD16 *pi2_dc_ld_addr)
//**************Variables Vs Registers*****************************************
//x0 => *pi2_src
//x1 => *pu1_pred
//x2 => *pu1_out
//w3 =>  pred_strd
//w4 =>  out_strd
//x5 => *pu2_iscal_mat
//x6 => *pu2_weigh_mat
//w7 =>  u4_qp_div_6
//   =>  pi4_tmp
//   =>  iq_start_idx
//   =>  pi2_dc_ld_addr
//Only one shift is done in horizontal inverse because,
//if u4_qp_div_6 is lesser than 4 then shift value will be neagative and do negative left shift, in this case rnd_factor has value
//if u4_qp_div_6 is greater than 4 then shift value will be positive and do left shift, here rnd_factor is 0

    .global ih264_iquant_itrans_recon_4x4_av8
ih264_iquant_itrans_recon_4x4_av8:

    push_v_regs
    sxtw      x3, w3
    sxtw      x4, w4

    dup       v30.4s, w7                //Populate the u4_qp_div_6 in Q15

    ldr       w8, [sp, #72]             //Loads iq_start_idx
    sxtw      x8, w8

    ldr       x10, [sp, #80]            //Load alternate dc address

    subs      x8, x8, #1                // if x8 == 1 => intra case , so result of subtraction is zero and z flag is set


//=======================DEQUANT FROM HERE===================================

    ld4       {v20.4h - v23.4h}, [x5]   // load pu2_iscal_mat[i], i =0..15
    ld4       {v26.4h - v29.4h}, [x6]   // pu2_weigh_mat[i], i =0..15
    ld4       {v16.4h - v19.4h}, [x0]   // pi2_src_tmp[i], i =0..15


    mul       v20.4h, v20.4h, v26.4h    // x[i]=(scale[i] * dequant[i]) where i = 0..3
    mul       v21.4h, v21.4h, v27.4h    // x[i]=(scale[i] * dequant[i]) where i = 4..7
    mul       v22.4h, v22.4h, v28.4h    // x[i]=(scale[i] * dequant[i]) where i = 8..11
    mul       v23.4h, v23.4h, v29.4h    // x[i]=(scale[i] * dequant[i]) where i = 12..14

    smull     v0.4s, v16.4h, v20.4h     // q0  = p[i] = (x[i] * trns_coeff[i]) where i = 0..3
    smull     v2.4s, v17.4h, v21.4h     // q1  = p[i] = (x[i] * trns_coeff[i]) where i = 4..7
    smull     v4.4s, v18.4h, v22.4h     // q2  = p[i] = (x[i] * trns_coeff[i]) where i = 8..11
    smull     v6.4s, v19.4h, v23.4h     // q3  = p[i] = (x[i] * trns_coeff[i]) where i = 12..15

    sshl      v0.4s, v0.4s, v30.4s      // q0  = q[i] = (p[i] << (qp/6)) where i = 0..3
    sshl      v2.4s, v2.4s, v30.4s      // q1  = q[i] = (p[i] << (qp/6)) where i = 4..7
    sshl      v4.4s, v4.4s, v30.4s      // q2  = q[i] = (p[i] << (qp/6)) where i = 8..11
    sshl      v6.4s, v6.4s, v30.4s      // q3  = q[i] = (p[i] << (qp/6)) where i = 12..15

    sqrshrn   v0.4h, v0.4s, #0x4        // d0  = c[i] = ((q[i] + 32) >> 4) where i = 0..3
    sqrshrn   v1.4h, v2.4s, #0x4        // d1  = c[i] = ((q[i] + 32) >> 4) where i = 4..7
    sqrshrn   v2.4h, v4.4s, #0x4        // d2  = c[i] = ((q[i] + 32) >> 4) where i = 8..11
    sqrshrn   v3.4h, v6.4s, #0x4        // d3  = c[i] = ((q[i] + 32) >> 4) where i = 12..15

    bne       skip_loading_luma_dc_src
    ld1       {v0.h}[0], [x10]          // loads signed halfword pi2_dc_ld_addr[0], if x8==1
skip_loading_luma_dc_src:

    //========= PROCESS IDCT FROM HERE =======
    //Steps for Stage 1:
    //------------------
    ld1       {v30.s}[0], [x1], x3      // i row load pu1_pred buffer

    sshr      v8.4h, v1.4h, #1          // d1>>1
    sshr      v9.4h, v3.4h, #1          // d3>>1

    add       v4.4h, v0.4h, v2.4h       // x0 = d0 + d2//
    sub       v5.4h, v0.4h, v2.4h       // x1 = d0 - d2//
    sub       v6.4h, v8.4h, v3.4h       // x2 = (d1 >> 1) -  d3//
    add       v7.4h, v1.4h, v9.4h       // x3 =  d1  + (d3 >>  1)//

    ld1       {v30.s}[1], [x1], x3      // ii row load pu1_pred buffer

    add       v10.4h, v4.4h , v7.4h     // x0+x3
    add       v11.4h, v5.4h , v6.4h     // x1+x2
    sub       v12.4h, v5.4h , v6.4h     // x1-x2
    sub       v13.4h, v4.4h , v7.4h

    ld1       {v31.s}[0], [x1], x3      // iii row load pu1_pred buf


    //Steps for Stage 2:
    //transopose
    trn1      v4.4h, v10.4h, v11.4h
    trn2      v5.4h, v10.4h, v11.4h
    trn1      v6.4h, v12.4h, v13.4h
    trn2      v7.4h, v12.4h, v13.4h

    trn1      v10.2s, v4.2s, v6.2s      // 0
    trn1      v11.2s, v5.2s, v7.2s      // 8
    trn2      v12.2s, v4.2s, v6.2s      // 4
    trn2      v13.2s, v5.2s, v7.2s
    //end transpose

    sshr      v18.4h, v11.4h, #1        // q0>>1
    sshr      v19.4h, v13.4h, #1        // q1>>1

    add       v14.4h, v10.4h, v12.4h    // x0 = q0 + q2//
    sub       v15.4h, v10.4h, v12.4h    // x1 = q0 - q2//
    sub       v16.4h, v18.4h, v13.4h    // x2 = (q1 >> 1) -  q3//
    add       v17.4h, v11.4h, v19.4h    // x3 = q1+ (q3 >> 3)//


    ld1       {v31.s}[1], [x1], x3      // iv row load pu1_pred buffer

    add       v20.4h, v14.4h, v17.4h    // x0 + x3
    add       v21.4h, v15.4h, v16.4h    // x1 + x2
    sub       v22.4h, v15.4h, v16.4h    // x1 - x2
    sub       v23.4h, v14.4h, v17.4h    // x0 - x3

    mov       v20.d[1], v21.d[0]
    mov       v22.d[1], v23.d[0]

    srshr     v20.8h, v20.8h, #6
    srshr     v22.8h, v22.8h, #6

    uaddw     v20.8h, v20.8h , v30.8b
    uaddw     v22.8h, v22.8h , v31.8b

    sqxtun    v0.8b, v20.8h
    sqxtun    v1.8b, v22.8h

    st1       {v0.s}[0], [x2], x4       //i row store the value
    st1       {v0.s}[1], [x2], x4       //ii row store the value
    st1       {v1.s}[0], [x2], x4       //iii row store the value
    st1       {v1.s}[1], [x2]           //iv row store the value

    pop_v_regs
    ret


///**
// *******************************************************************************
// *
// * @brief
// *  This function performs inverse quant and Inverse transform type Ci4 for 4*4 block
// *
// * @par Description:
// *  Performs inverse transform Ci4 and adds the residue to get the
// *  reconstructed block
// *
// * @param[in] pi2_src
// *  Input 4x4 coefficients
// *
// * @param[in] pu1_pred
// *  Prediction 4x4 block
// *
// * @param[out] pu1_out
// *  Output 4x4 block
// *
// * @param[in] u4_qp_div_6
// *     QP
// *
// * @param[in] pu2_weigh_mat
// * Pointer to weight matrix
// *
// * @param[in] pred_strd,
// *  Prediction stride
// *
// * @param[in] out_strd
// *  Output Stride
// *
// *@param[in] pi2_tmp
// * temporary buffer of size 1*16
// *
// * @param[in] pu2_iscal_mat
// * Pointer to the inverse quantization matrix
// *
// * @returns  Void
// *
// * @remarks
// *  None
// *
// *******************************************************************************
// */
//void ih264_iquant_itrans_recon_chroma_4x4(WORD16 *pi2_src,
//                                          UWORD8 *pu1_pred,
//                                          UWORD8 *pu1_out,
//                                          WORD32 pred_strd,
//                                          WORD32 out_strd,
//                                          const UWORD16 *pu2_iscal_mat,
//                                          const UWORD16 *pu2_weigh_mat,
//                                          UWORD32 u4_qp_div_6,
//                                          WORD32 *pi4_tmp
//                                          WORD16 *pi2_dc_src)
//**************Variables Vs Registers*****************************************
//x0 => *pi2_src
//x1 => *pu1_pred
//x2 => *pu1_out
//w3 =>  pred_strd
//w4 =>  out_strd
//x5 => *pu2_iscal_mat
//x6 => *pu2_weigh_mat
//w7 =>  u4_qp_div_6
//sp =>  pi4_tmp
//sp#8 => *pi2_dc_src

    .global ih264_iquant_itrans_recon_chroma_4x4_av8
ih264_iquant_itrans_recon_chroma_4x4_av8:

//VLD4.S16 is used because the pointer is incremented by SUB_BLK_WIDTH_4x4
//If the macro value changes need to change the instruction according to it.
//Only one shift is done in horizontal inverse because,
//if u4_qp_div_6 is lesser than 4 then shift value will be neagative and do negative left shift, in this case rnd_factor has value
//if u4_qp_div_6 is greater than 4 then shift value will be positive and do left shift, here rnd_factor is 0

//at the end of the fucntion, we could have moved 64 bits into heigher 64 bits of register and done further processing
//but it seem to give only reduce the number of instruction by 1. [Since a15 we saw add and sub to be very high throughput
//all instructions were taken as equal

    //reduce sp by 64
    push_v_regs
    sxtw      x3, w3
    sxtw      x4, w4

    dup       v30.4s, w7                //Populate the u4_qp_div_6 in Q15

    //was at sp + 8, hence now at sp+64+8 = sp+72
    ldr       x10, [sp, #72]            //Load alternate dc address

//=======================DEQUANT FROM HERE===================================

    ld4       {v20.4h - v23.4h}, [x5]   // load pu2_iscal_mat[i], i =0..15
    ld4       {v26.4h - v29.4h}, [x6]   // pu2_weigh_mat[i], i =0..15
    ld4       {v16.4h - v19.4h}, [x0]   // pi2_src_tmp[i], i =0..15


    mul       v20.4h, v20.4h, v26.4h    // x[i]=(scale[i] * dequant[i]) where i = 0..3
    mul       v21.4h, v21.4h, v27.4h    // x[i]=(scale[i] * dequant[i]) where i = 4..7
    mul       v22.4h, v22.4h, v28.4h    // x[i]=(scale[i] * dequant[i]) where i = 8..11
    mul       v23.4h, v23.4h, v29.4h    // x[i]=(scale[i] * dequant[i]) where i = 12..14

    smull     v0.4s, v16.4h, v20.4h     // q0  = p[i] = (x[i] * trns_coeff[i]) where i = 0..3
    smull     v2.4s, v17.4h, v21.4h     // q1  = p[i] = (x[i] * trns_coeff[i]) where i = 4..7
    smull     v4.4s, v18.4h, v22.4h     // q2  = p[i] = (x[i] * trns_coeff[i]) where i = 8..11
    smull     v6.4s, v19.4h, v23.4h     // q3  = p[i] = (x[i] * trns_coeff[i]) where i = 12..15

    sshl      v0.4s, v0.4s, v30.4s      // q0  = q[i] = (p[i] << (qp/6)) where i = 0..3
    sshl      v2.4s, v2.4s, v30.4s      // q1  = q[i] = (p[i] << (qp/6)) where i = 4..7
    sshl      v4.4s, v4.4s, v30.4s      // q2  = q[i] = (p[i] << (qp/6)) where i = 8..11
    sshl      v6.4s, v6.4s, v30.4s      // q3  = q[i] = (p[i] << (qp/6)) where i = 12..15

    sqrshrn   v0.4h, v0.4s, #0x4        // d0  = c[i] = ((q[i] + 32) >> 4) where i = 0..3
    sqrshrn   v1.4h, v2.4s, #0x4        // d1  = c[i] = ((q[i] + 32) >> 4) where i = 4..7
    sqrshrn   v2.4h, v4.4s, #0x4        // d2  = c[i] = ((q[i] + 32) >> 4) where i = 8..11
    sqrshrn   v3.4h, v6.4s, #0x4        // d3  = c[i] = ((q[i] + 32) >> 4) where i = 12..15

    ld1       {v0.h}[0], [x10]          // loads signed halfword pi2_dc_src[0]

    //========= PROCESS IDCT FROM HERE =======
    //Steps for Stage 1:
    //------------------

    sshr      v8.4h, v1.4h, #1          // d1>>1
    sshr      v9.4h, v3.4h, #1          // d3>>1

    add       v4.4h, v0.4h, v2.4h       // x0 = d0 + d2//
    sub       v5.4h, v0.4h, v2.4h       // x1 = d0 - d2//
    sub       v6.4h, v8.4h, v3.4h       // x2 = (d1 >> 1) -  d3//
    add       v7.4h, v1.4h, v9.4h       // x3 =  d1  + (d3 >>  1)//


    add       v10.4h, v4.4h , v7.4h     // x0+x3
    add       v11.4h, v5.4h , v6.4h     // x1+x2
    sub       v12.4h, v5.4h , v6.4h     // x1-x2
    sub       v13.4h, v4.4h , v7.4h

    ld1       {v26.8b}, [x1], x3        // i row load pu1_pred buffer
    ld1       {v27.8b}, [x1], x3        // ii row load pu1_pred buffer
    ld1       {v28.8b}, [x1], x3        // iii row load pu1_pred buf
    ld1       {v29.8b}, [x1], x3        // iv row load pu1_pred buffer

    //Steps for Stage 2:
    //transopose
    trn1      v4.4h, v10.4h, v11.4h
    trn2      v5.4h, v10.4h, v11.4h
    trn1      v6.4h, v12.4h, v13.4h
    trn2      v7.4h, v12.4h, v13.4h

    trn1      v10.2s, v4.2s, v6.2s      // 0
    trn1      v11.2s, v5.2s, v7.2s      // 8
    trn2      v12.2s, v4.2s, v6.2s      // 4
    trn2      v13.2s, v5.2s, v7.2s
    //end transpose

    sshr      v18.4h, v11.4h, #1        // q0>>1
    sshr      v19.4h, v13.4h, #1        // q1>>1

    add       v14.4h, v10.4h, v12.4h    // x0 = q0 + q2//
    sub       v15.4h, v10.4h, v12.4h    // x1 = q0 - q2//
    sub       v16.4h, v18.4h, v13.4h    // x2 = (q1 >> 1) -  q3//
    add       v17.4h, v11.4h, v19.4h    // x3 = q1+ (q3 >> 3)//

    //Backup the output addr
    mov       x0, x2

    //load outpt buufer for interleaving
    ld1       {v10.8b}, [x2], x4
    ld1       {v11.8b}, [x2], x4
    ld1       {v12.8b}, [x2], x4
    ld1       {v13.8b}, [x2]

    add       v20.4h, v14.4h, v17.4h    // x0 + x3
    add       v21.4h, v15.4h, v16.4h    // x1 + x2
    sub       v22.4h, v15.4h, v16.4h    // x1 - x2
    sub       v23.4h, v14.4h, v17.4h    // x0 - x3

    srshr     v20.4h, v20.4h, #6
    srshr     v21.4h, v21.4h, #6
    srshr     v22.4h, v22.4h, #6
    srshr     v23.4h, v23.4h, #6

    //nop       v30.8b                            //dummy for deinterleaving
    movi      v31.4h, #0x00ff           //mask for interleaving [copy lower 8 bits]

    //Extract u/v plane from interleaved data
    uzp1      v26.8b, v26.8b, v30.8b
    uzp1      v27.8b, v27.8b, v30.8b
    uzp1      v28.8b, v28.8b, v30.8b
    uzp1      v29.8b, v29.8b, v30.8b

    uaddw     v20.8h, v20.8h, v26.8b
    uaddw     v21.8h, v21.8h, v27.8b
    uaddw     v22.8h, v22.8h, v28.8b
    uaddw     v23.8h, v23.8h, v29.8b

    sqxtun    v0.8b, v20.8h
    sqxtun    v1.8b, v21.8h
    sqxtun    v2.8b, v22.8h
    sqxtun    v3.8b, v23.8h

    //long the output so that we have 0 at msb and value at lsb
    uxtl      v6.8h, v0.8b
    uxtl      v7.8h, v1.8b
    uxtl      v8.8h, v2.8b
    uxtl      v9.8h, v3.8b

    //select lsbs from proceesd data and msbs from pu1_out loaded data
    bit       v10.8b, v6.8b, v31.8b
    bit       v11.8b, v7.8b, v31.8b
    bit       v12.8b, v8.8b, v31.8b
    bit       v13.8b, v9.8b, v31.8b

    //store the interleaved result
    st1       {v10.8b}, [x0], x4
    st1       {v11.8b}, [x0], x4
    st1       {v12.8b}, [x0], x4
    st1       {v13.8b}, [x0]

    pop_v_regs
    ret

///*
// *******************************************************************************
// *
// * //brief
// *  This function performs inverse quant and Inverse transform type Ci4 for 8*8 block
// *
// * //par Description:
// *  Performs inverse transform Ci8 and adds the residue to get the
// *  reconstructed block
// *
// * //param[in] pi2_src
// *  Input 4x4 coefficients
// *
// * //param[in] pu1_pred
// *  Prediction 4x4 block
// *
// * //param[out] pu1_out
// *  Output 4x4 block
// *
// * //param[in] u4_qp_div_6
// *     QP
// *
// * //param[in] pu2_weigh_mat
// * Pointer to weight matrix
// *
// * //param[in] pred_strd,
// *  Prediction stride
// *
// * //param[in] out_strd
// *  Output Stride
// *
// *//param[in] pi2_tmp
// * temporary buffer of size 1*64
// *
// * //param[in] pu2_iscal_mat
// * Pointer to the inverse quantization matrix
// *
// * //returns  Void
// *
// * //remarks
// *  None
// *
// *******************************************************************************
// */
//void ih264_iquant_itrans_recon_8x8(WORD16 *pi2_src,
//                                   UWORD8 *pu1_pred,
//                                   UWORD8 *pu1_out,
//                                   WORD32 pred_strd,
//                                   WORD32 out_strd,
//                                   const UWORD16 *pu2_iscal_mat,
//                                   const UWORD16 *pu2_weigh_mat,
//                                   UWORD32 u4_qp_div_6,
//                                   WORD32 *pi4_tmp,
//                                   WORD32 iq_start_idx
//                                   WORD16 *pi2_dc_ld_addr)
//**************Variables Vs Registers*****************************************
//x0       => *pi2_src
//x1       => *pu1_pred
//x2       => *pu1_out
//w3       =>  pred_strd
//w4       =>  out_strd
//x5       =>  *pu2_iscal_mat
//x6       =>  *pu2_weigh_mat
//w7       =>  u4_qp_div_6
//NOT USED =>  pi4_tmp
//NOT USED =>  iq_start_idx
//NOT USED =>  pi2_dc_ld_addr

    .global ih264_iquant_itrans_recon_8x8_av8
ih264_iquant_itrans_recon_8x8_av8:

    push_v_regs
    sxtw      x3, w3
    sxtw      x4, w4

    ld1       {v8.8h -v11.8h}, [x5], #64
    ld1       {v12.8h-v15.8h}, [x5]

    ld1       {v16.8h -v19.8h}, [x6], #64
    ld1       {v20.8h -v23.8h}, [x6]

    mov       x8, #16
    ld1       {v0.8h}, [x0], x8
    ld1       {v1.8h}, [x0], x8
    ld1       {v2.8h}, [x0], x8
    ld1       {v3.8h}, [x0], x8
    ld1       {v4.8h}, [x0], x8
    ld1       {v5.8h}, [x0], x8
    ld1       {v6.8h}, [x0], x8
    ld1       {v7.8h}, [x0]

    mul       v8.8h, v8.8h, v16.8h
    mul       v9.8h, v9.8h, v17.8h
    mul       v10.8h, v10.8h, v18.8h
    mul       v11.8h, v11.8h, v19.8h
    mul       v12.8h, v12.8h, v20.8h
    mul       v13.8h, v13.8h, v21.8h
    mul       v14.8h, v14.8h, v22.8h
    mul       v15.8h, v15.8h, v23.8h

    smull     v16.4s, v0.4h, v8.4h
    smull2    v17.4s, v0.8h, v8.8h
    smull     v18.4s, v1.4h, v9.4h
    smull2    v19.4s, v1.8h, v9.8h
    smull     v20.4s, v2.4h, v10.4h
    smull2    v21.4s, v2.8h, v10.8h
    smull     v22.4s, v3.4h, v11.4h
    smull2    v23.4s, v3.8h, v11.8h
    smull     v24.4s, v4.4h, v12.4h
    smull2    v25.4s, v4.8h, v12.8h
    smull     v26.4s, v5.4h, v13.4h
    smull2    v27.4s, v5.8h, v13.8h
    smull     v28.4s, v6.4h, v14.4h
    smull2    v29.4s, v6.8h, v14.8h
    smull     v30.4s, v7.4h, v15.4h
    smull2    v31.4s, v7.8h, v15.8h

    dup       v0.4s, w7

    sshl      v16.4s, v16.4s, v0.4s
    sshl      v17.4s, v17.4s, v0.4s
    sshl      v18.4s, v18.4s, v0.4s
    sshl      v19.4s, v19.4s, v0.4s
    sshl      v20.4s, v20.4s, v0.4s
    sshl      v21.4s, v21.4s, v0.4s
    sshl      v22.4s, v22.4s, v0.4s
    sshl      v23.4s, v23.4s, v0.4s
    sshl      v24.4s, v24.4s, v0.4s
    sshl      v25.4s, v25.4s, v0.4s
    sshl      v26.4s, v26.4s, v0.4s
    sshl      v27.4s, v27.4s, v0.4s
    sshl      v28.4s, v28.4s, v0.4s
    sshl      v29.4s, v29.4s, v0.4s
    sshl      v30.4s, v30.4s, v0.4s
    sshl      v31.4s, v31.4s, v0.4s

    sqrshrn   v0.4h, v16.4s, #6
    sqrshrn2  v0.8h, v17.4s, #6
    sqrshrn   v1.4h, v18.4s, #6
    sqrshrn2  v1.8h, v19.4s, #6
    sqrshrn   v2.4h, v20.4s, #6
    sqrshrn2  v2.8h, v21.4s, #6
    sqrshrn   v3.4h, v22.4s, #6
    sqrshrn2  v3.8h, v23.4s, #6
    sqrshrn   v4.4h, v24.4s, #6
    sqrshrn2  v4.8h, v25.4s, #6
    sqrshrn   v5.4h, v26.4s, #6
    sqrshrn2  v5.8h, v27.4s, #6
    sqrshrn   v6.4h, v28.4s, #6
    sqrshrn2  v6.8h, v29.4s, #6
    sqrshrn   v7.4h, v30.4s, #6
    sqrshrn2  v7.8h, v31.4s, #6

    //loop counter
    mov       x8, #2
//1x8 transofORM
trans_1x8_1d:

    //transpose 8x8
    trn1      v8.8h, v0.8h, v1.8h
    trn2      v9.8h, v0.8h, v1.8h
    trn1      v10.8h, v2.8h, v3.8h
    trn2      v11.8h, v2.8h, v3.8h
    trn1      v12.8h, v4.8h, v5.8h
    trn2      v13.8h, v4.8h, v5.8h
    trn1      v14.8h, v6.8h, v7.8h
    trn2      v15.8h, v6.8h, v7.8h

    trn1      v0.4s, v8.4s, v10.4s
    trn2      v2.4s, v8.4s, v10.4s
    trn1      v1.4s, v9.4s, v11.4s
    trn2      v3.4s, v9.4s, v11.4s
    trn1      v4.4s, v12.4s, v14.4s
    trn2      v6.4s, v12.4s, v14.4s
    trn1      v5.4s, v13.4s, v15.4s
    trn2      v7.4s, v13.4s, v15.4s

    trn1      v8.2d, v0.2d, v4.2d       //0
    trn2      v12.2d, v0.2d, v4.2d      //1
    trn1      v9.2d, v1.2d, v5.2d       //2
    trn2      v13.2d, v1.2d, v5.2d      //3
    trn1      v10.2d, v2.2d, v6.2d      //4
    trn2      v14.2d, v2.2d, v6.2d      //5
    trn1      v11.2d, v3.2d, v7.2d      //6
    trn2      v15.2d, v3.2d, v7.2d      //7

    // 1 3 5 6 7
    sshr      v16.8h, v9.8h, #1         //(pi2_tmp_ptr[1] >> 1)
    sshr      v17.8h, v10.8h, #1        //(pi2_tmp_ptr[2] >> 1)
    sshr      v18.8h, v11.8h, #1        //(pi2_tmp_ptr[3] >> 1)
    sshr      v19.8h, v13.8h, #1        //(pi2_tmp_ptr[5] >> 1)
    sshr      v20.8h, v14.8h, #1        //(pi2_tmp_ptr[6] >> 1)
    sshr      v21.8h, v15.8h, #1        //(pi2_tmp_ptr[7] >> 1)

    add       v0.8h, v8.8h, v12.8h      // i_y0 = (pi2_tmp_ptr[0] + pi2_tmp_ptr[4] );
    sub       v2.8h, v8.8h, v12.8h      // i_y2 = (pi2_tmp_ptr[0] - pi2_tmp_ptr[4] );

    sub       v4.8h, v17.8h, v14.8h     //i_y4 = ((pi2_tmp_ptr[2] >> 1) - pi2_tmp_ptr[6] );
    add       v6.8h, v10.8h, v20.8h     //i_y6 = (pi2_tmp_ptr[2] + (pi2_tmp_ptr[6] >> 1));

    //-w3 + w5
    ssubl     v22.4s, v13.4h, v11.4h
    ssubl2    v23.4s, v13.8h, v11.8h
    //w3 + w5
    saddl     v24.4s, v13.4h, v11.4h
    saddl2    v25.4s, v13.8h, v11.8h
    //-w1 + w7
    ssubl     v26.4s, v15.4h, v9.4h
    ssubl2    v27.4s, v15.8h, v9.8h
    //w1 + w7
    saddl     v28.4s, v15.4h, v9.4h
    saddl2    v29.4s, v15.8h, v9.8h

    //-w3 + w5 - w7
    ssubw     v22.4s, v22.4s, v15.4h
    ssubw2    v23.4s, v23.4s, v15.8h
    //w3 + w5 + w1
    saddw     v24.4s, v24.4s, v9.4h
    saddw2    v25.4s, v25.4s, v9.8h
    //-w1 + w7 + w5
    saddw     v26.4s, v26.4s, v13.4h
    saddw2    v27.4s, v27.4s, v13.8h
    //w1 + w7 - w3
    ssubw     v28.4s, v28.4s, v11.4h
    ssubw2    v29.4s, v29.4s, v11.8h

    //-w3 + w5 - w7 - (w7 >> 1)
    ssubw     v22.4s, v22.4s, v21.4h
    ssubw2    v23.4s, v23.4s, v21.8h
    //w3 + w5 + w1 + (w1 >> 1)
    saddw     v24.4s, v24.4s, v16.4h
    saddw2    v25.4s, v25.4s, v16.8h
    //-w1 + w7 + w5 + (w5 >> 1)
    saddw     v26.4s, v26.4s, v19.4h
    saddw2    v27.4s, v27.4s, v19.8h
    //w1 + w7 - w3 - (w3 >> 1)
    ssubw     v28.4s, v28.4s, v18.4h
    ssubw2    v29.4s, v29.4s, v18.8h

    xtn       v1.4h, v22.4s
    xtn2      v1.8h, v23.4s
    xtn       v3.4h, v28.4s
    xtn2      v3.8h, v29.4s
    xtn       v5.4h, v26.4s
    xtn2      v5.8h, v27.4s
    xtn       v7.4h, v24.4s
    xtn2      v7.8h, v25.4s

    sshr      v16.8h, v1.8h, #2         //(y1 >> 2)
    sshr      v17.8h, v3.8h, #2         //(y3 >> 2)
    sshr      v18.8h, v5.8h, #2         //(y5 >> 2)
    sshr      v19.8h, v7.8h, #2         //(y7 >> 2)

    add       v8.8h, v0.8h, v6.8h
    add       v9.8h, v1.8h, v19.8h
    add       v10.8h, v2.8h, v4.8h
    add       v11.8h, v3.8h, v18.8h
    sub       v12.8h, v2.8h, v4.8h
    sub       v13.8h, v17.8h, v5.8h
    sub       v14.8h, v0.8h, v6.8h
    sub       v15.8h, v7.8h, v16.8h

    add       v0.8h, v8.8h, v15.8h
    add       v1.8h, v10.8h, v13.8h
    add       v2.8h, v12.8h, v11.8h
    add       v3.8h, v14.8h, v9.8h
    sub       v4.8h, v14.8h, v9.8h
    sub       v5.8h, v12.8h, v11.8h
    sub       v6.8h, v10.8h, v13.8h
    sub       v7.8h, v8.8h, v15.8h

    subs      x8, x8, #1
    bne       trans_1x8_1d

    ld1       {v22.8b}, [x1], x3
    ld1       {v23.8b}, [x1], x3
    ld1       {v24.8b}, [x1], x3
    ld1       {v25.8b}, [x1], x3
    ld1       {v26.8b}, [x1], x3
    ld1       {v27.8b}, [x1], x3
    ld1       {v28.8b}, [x1], x3
    ld1       {v29.8b}, [x1]

    srshr     v0.8h, v0.8h, #6
    srshr     v1.8h, v1.8h, #6
    srshr     v2.8h, v2.8h, #6
    srshr     v3.8h, v3.8h, #6
    srshr     v4.8h, v4.8h, #6
    srshr     v5.8h, v5.8h, #6
    srshr     v6.8h, v6.8h, #6
    srshr     v7.8h, v7.8h, #6

    uaddw     v0.8h, v0.8h, v22.8b
    uaddw     v1.8h, v1.8h, v23.8b
    uaddw     v2.8h, v2.8h, v24.8b
    uaddw     v3.8h, v3.8h, v25.8b
    uaddw     v4.8h, v4.8h, v26.8b
    uaddw     v5.8h, v5.8h, v27.8b
    uaddw     v6.8h, v6.8h, v28.8b
    uaddw     v7.8h, v7.8h, v29.8b

    sqxtun    v0.8b, v0.8h
    sqxtun    v1.8b, v1.8h
    sqxtun    v2.8b, v2.8h
    sqxtun    v3.8b, v3.8h
    sqxtun    v4.8b, v4.8h
    sqxtun    v5.8b, v5.8h
    sqxtun    v6.8b, v6.8h
    sqxtun    v7.8b, v7.8h

    st1       {v0.8b}, [x2], x4
    st1       {v1.8b}, [x2], x4
    st1       {v2.8b}, [x2], x4
    st1       {v3.8b}, [x2], x4
    st1       {v4.8b}, [x2], x4
    st1       {v5.8b}, [x2], x4
    st1       {v6.8b}, [x2], x4
    st1       {v7.8b}, [x2]

    pop_v_regs
    ret




